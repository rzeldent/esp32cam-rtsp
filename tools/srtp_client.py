#!/usr/bin/env python3
"""
SRTP test client for the ESP32-CAM micro-rtsp-server.

Negotiates SRTP (RFC 3711) by offering an "a=crypto" attribute (RFC 4568) in
the RTSP SETUP request. The server responds with its own master key/salt, then
decrypts the incoming RTP and reassembles/saves the JPEG frames.

The decryption replicates the server's implementation EXACTLY, see
  lib/micro-rtsp-server/src/micro_rtsp_srtp.cpp
Notably its key derivation is NON-standard: the label is XORed into salt byte 7
(not the RFC 3711 position), so a stock libsrtp/FFmpeg receiver would fail.
This client mirrors the firmware's bytes, which is what actually verifies SRTP.

Usage:
    pip install cryptography
    python tools/srtp_client.py --host 192.168.1.149 [--port 554]
        [--rtp-port 50000] [--frames 5] [--time 10] [--out frames]
"""

import argparse
import base64
import hashlib
import hmac as hmac_mod
import os
import re
import socket
import struct
import sys
import time

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
except ImportError:
    sys.exit("Missing dependency: pip install cryptography")


# ---------------------------------------------------------------------------
# SRTP (mirrors micro_rtsp_srtp.cpp exactly)
# ---------------------------------------------------------------------------

def aes_ecb_encrypt(key, block):
    enc = Cipher(algorithms.AES(key), modes.ECB()).encryptor()
    return enc.update(block) + enc.finalize()


def aes_cm_keystream(key, iv, n_bytes):
    """RFC 3711 AES-CM keystream: E(k,IV)||E(k,IV+1)||... with the 16-bit
    block counter held in bytes 14-15 (matches server encrypt_counter())."""
    out = bytearray()
    counter = bytearray(iv)
    block = 0
    while len(out) < n_bytes:
        counter[14] = (block >> 8) & 0xFF
        counter[15] = block & 0xFF
        out += aes_ecb_encrypt(key, bytes(counter))
        block += 1
    return bytes(out[:n_bytes])


def derive_key(master_key, master_salt, label, n_bytes):
    """Session key derivation matching server derive_key(): salt in bytes
    0..13, label XORed into byte 7, then AES-CM keystream."""
    iv = bytearray(16)
    iv[0:14] = master_salt
    iv[7] ^= label
    return aes_cm_keystream(master_key, iv, n_bytes)


def create_iv(rtp_salt, index, ssrc):
    """RFC 3711 4.1.1 IV: iv[4..7]=ssrc, iv[8..13]=index(48b), then XOR the
    session salt over bytes 0..13 (bytes 14-15 are the AES-CM block counter)."""
    iv = bytearray(16)
    iv[4] = (ssrc >> 24) & 0xFF
    iv[5] = (ssrc >> 16) & 0xFF
    iv[6] = (ssrc >> 8) & 0xFF
    iv[7] = ssrc & 0xFF
    iv[8] = (index >> 40) & 0xFF
    iv[9] = (index >> 32) & 0xFF
    iv[10] = (index >> 24) & 0xFF
    iv[11] = (index >> 16) & 0xFF
    iv[12] = (index >> 8) & 0xFF
    iv[13] = index & 0xFF
    for i in range(14):
        iv[i] ^= rtp_salt[i]
    return bytes(iv)


class SrtpReceiver:
    """Decrypts and authenticates outbound SRTP packets from the server."""

    def __init__(self, master_key, master_salt):
        self.k_e = derive_key(master_key, master_salt, 0x00, 16)
        self.k_a = derive_key(master_key, master_salt, 0x01, 20)
        self.k_s = derive_key(master_key, master_salt, 0x02, 14)
        self.roc = 0
        self.last_seq = None

    def unprotect(self, packet):
        if len(packet) < 12 + 10:
            raise ValueError("packet too short")
        rtp = packet[:12]          # RTP header stays clear
        body = packet[12:-10]      # encrypted payload
        tag = packet[-10:]         # 80-bit HMAC-SHA1 tag

        seq = struct.unpack(">H", rtp[2:4])[0]
        ssrc = struct.unpack(">I", rtp[8:12])[0]

        if self.last_seq is not None and seq < self.last_seq:
            self.roc += 1
        self.last_seq = seq
        index = (self.roc << 16) | seq

        # Decrypt the payload (AES-CM keystream XOR)
        iv = create_iv(self.k_s, index, ssrc)
        plain = bytes(a ^ b for a, b in zip(body, aes_cm_keystream(self.k_e, iv, len(body))))

        # The server authenticates the RTP header || ENCRYPTED payload || ROC
        mac = hmac_mod.new(self.k_a, rtp + body + struct.pack(">I", self.roc),
                           hashlib.sha1).digest()[:10]
        if not hmac_mod.compare_digest(mac, tag):
            raise ValueError("SRTP authentication tag mismatch")
        return rtp + plain


# ---------------------------------------------------------------------------
# RTSP client helpers
# ---------------------------------------------------------------------------

def recv_response(sock):
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            break
        data += chunk
    head, _, body = data.partition(b"\r\n\r\n")
    lines = head.split(b"\r\n")
    status = lines[0].decode(errors="replace")
    headers = {}
    for line in lines[1:]:
        if b":" in line:
            k, v = line.split(b":", 1)
            headers[k.strip().decode().lower()] = v.strip().decode()
    cl = int(headers.get("content-length", "0"))
    while len(body) < cl:
        chunk = sock.recv(cl - len(body))
        if not chunk:
            break
        body += chunk
    return status, headers, body


def rtsp_request(sock, request):
    sock.sendall(request)
    return recv_response(sock)


def build_request(method, url, cseq, extra_headers=b"", body=b""):
    headers = (f"{method} {url} RTSP/1.0\r\n"
               f"CSeq: {cseq}\r\n"
               f"User-Agent: python-srtp-test\r\n").encode()
    if body:
        headers += b"Content-Type: application/sdp\r\n"
        headers += f"Content-Length: {len(body)}\r\n".encode()
    headers += extra_headers
    return headers + b"\r\n" + body


# ---------------------------------------------------------------------------
# JPEG reassembly (RFC 2435)
# ---------------------------------------------------------------------------

class JpegAssembler:
    """Collects RTP/JPEG fragments into frames, keyed by RTP timestamp."""

    def __init__(self):
        self.buf = {}   # timestamp -> (data, last_marker)

    def add(self, packet, marker):
        if len(packet) < 12 + 8:
            return None
        ts = struct.unpack(">I", packet[4:8])[0]
        payload = packet[12:]  # 8-byte RFC 2435 header (+ tables on first frag) + scan data
        frag_offset = (payload[1] << 16) | (payload[2] << 8) | payload[3]

        cur = self.buf.setdefault(ts, bytearray())
        # The server sends contiguous fragments; place this one at its offset.
        if len(cur) < frag_offset:
            cur += b"\x00" * (frag_offset - len(cur))
        cur[frag_offset:frag_offset + len(payload)] = payload

        if marker:
            frame = bytes(cur)
            del self.buf[ts]
            return frame
        return None


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="SRTP test client for micro-rtsp-server")
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=554)
    ap.add_argument("--rtp-port", type=int, default=50000, help="local client RTP port")
    ap.add_argument("--frames", type=int, default=0, help="stop after N frames (0=by time)")
    ap.add_argument("--time", type=float, default=10.0, help="receive for this many seconds")
    ap.add_argument("--out", default="frames", help="output directory for raw frames")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    base = f"rtsp://{args.host}:{args.port}/mjpeg/1"
    sock = socket.create_connection((args.host, args.port), timeout=5)
    sock.settimeout(10)

    # --- OPTIONS ----------------------------------------------------------
    status, _, _ = rtsp_request(sock, build_request("OPTIONS", base, 1))
    assert status.startswith("RTSP/1.0 200"), status

    # --- DESCRIBE ----------------------------------------------------------
    status, _, sdp = rtsp_request(sock, build_request("DESCRIBE", base, 2, b"Accept: application/sdp\r\n"))
    assert status.startswith("RTSP/1.0 200"), status
    print("--- SDP ---\n" + sdp.decode())

    # --- SETUP with a=crypto offer ----------------------------------------
    client_key = os.urandom(16)
    client_salt = os.urandom(14)
    inline = base64.b64encode(client_key + client_salt).decode()
    crypto_body = f"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:{inline}\r\n".encode()
    transport = f"Transport: RTP/AVP;unicast;client_port={args.rtp_port}-{args.rtp_port + 1}\r\n"
    status, headers, body = rtsp_request(sock, build_request("SETUP", base + "/track1", 3, transport.encode(), crypto_body))
    if not status.startswith("RTSP/1.0 200"):
        print("SETUP failed:", status, body.decode(errors="replace"))
        sys.exit(1)

    m = re.search(rb"inline:([A-Za-z0-9+/=]+)", body)
    if not m:
        print("!! Server did not advertise an a=crypto attribute -> SRTP not negotiated.")
        sys.exit(1)

    server_key_salt = base64.b64decode(m.group(1))
    server_key, server_salt = server_key_salt[:16], server_key_salt[16:30]
    print("--- SETUP response ---\n" + body.decode())
    print(f"server master key : {server_key.hex()}")
    print(f"server master salt: {server_salt.hex()}")

    session = headers["session"]

    # --- local RTP socket ---------------------------------------------------
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.bind(("0.0.0.0", args.rtp_port))
    udp.settimeout(5)

    # --- PLAY ----------------------------------------------------------------
    play_headers = f"Session: {session}\r\nRange: npt=0.000-\r\n".encode()
    status, _, _ = rtsp_request(sock, build_request("PLAY", base + "/", 4, play_headers))
    assert status.startswith("RTSP/1.0 200"), status

    # --- receive loop --------------------------------------------------------
    print(f"Receiving SRTP on UDP {args.rtp_port} ...")
    rx = SrtpReceiver(server_key, server_salt)
    jpeg = JpegAssembler()

    rx_count = 0
    frame_count = 0
    tag_failures = 0
    start = time.time()
    deadline = start + args.time

    while True:
        if args.frames and frame_count >= args.frames:
            break
        if args.frames == 0 and time.time() >= deadline:
            break
        try:
            pkt, _ = udp.recvfrom(2048)
        except socket.timeout:
            print("UDP receive timed out")
            break

        try:
            rtp = rx.unprotect(pkt)
        except ValueError as e:
            tag_failures += 1
            print(f"  ! {e}")
            if tag_failures > 3:
                print("Too many auth failures - is SRTP actually active?")
                break
            continue

        rx_count += 1
        marker = bool(rtp[1] & 0x80)
        frame = jpeg.add(rtp, marker)
        if frame is not None:
            frame_count += 1
            path = os.path.join(args.out, f"frame_{frame_count:04d}.bin")
            with open(path, "wb") as f:
                f.write(frame)
            print(f"  frame {frame_count}: {len(frame)} bytes -> {path}")

    print(f"\nReceived {rx_count} SRTP packets, {frame_count} frames, "
          f"{tag_failures} auth failures")
    ok = rx_count > 0 and frame_count > 0 and tag_failures == 0
    print("SRTP: OK - packets decrypted and authenticated" if ok
          else "SRTP: CHECK LOG - something is off")


if __name__ == "__main__":
    main()
