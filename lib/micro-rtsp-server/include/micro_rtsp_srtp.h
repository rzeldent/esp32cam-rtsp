#pragma once

#include <stddef.h>
#include <stdint.h>

// Secure RTP (RFC 3711) for the AES_CM_128_HMAC_SHA1_80 crypto suite:
//  - AES-128 in Counter Mode for encryption (session encryption key),
//  - HMAC-SHA1 truncated to 80 bits for authentication (session auth key),
//  - session keys derived from a 128 bit master key + 112 bit master salt
//    using the AES-CM PRF (RFC 3711 section 4.3).
//
// This class only protects outbound RTP packets (the server is a sender).
// It interoperates with standard SRTP receivers (FFmpeg/libSRTP), which
// derive the same session keys from the master key/salt negotiated through
// the RTSP "a=crypto" (RFC 4568) attribute.
class micro_rtsp_srtp
{
public:
    static constexpr size_t master_key_size = 16;   // 128 bit
    static constexpr size_t master_salt_size = 14;  // 112 bit
    static constexpr size_t auth_tag_size = 10;     // 80 bit HMAC-SHA1 tag
    static constexpr size_t session_key_size = 16;  // AES-128 key
    static constexpr size_t session_salt_size = 14; // 112 bit
    static constexpr size_t session_auth_size = 20; // 160 bit HMAC key

    micro_rtsp_srtp();
    ~micro_rtsp_srtp();

    // Fill the master key and salt with random bytes and derive the session
    // keys. Used by the server when it offers SRTP (RFC 4568).
    void generate_key_salt();

    // Set an explicit master key and salt (e.g. fixed/testing keys) and
    // derive the session keys.
    bool set_key_salt(const uint8_t *key, const uint8_t *salt);

    bool enabled() const;
    size_t tag_size() const;

    const uint8_t *key() const;
    const uint8_t *salt() const;

    // Encrypt the payload of an RTP packet in place and append the
    // authentication tag. "packet" points at the RTP packet (12 byte header
    // + payload); "len" is the RTP packet length on entry and is increased
    // by auth_tag_size on return. The packet buffer must have room for the
    // extra tag bytes.
    void protect_rtp(uint8_t *packet, size_t &len);

private:
    // RFC 3711 section 4.3.3: AES-CM PRF used for key derivation.
    void derive_key(uint8_t label, uint8_t *out, size_t outlen);

    // XOR the AES-CTR keystream (key "key", counter = iv with the block index
    // in bytes 14-15) into "data". Matches the AES-CM keystream of RFC 3711
    // section 4.1.1.
    void encrypt_counter(const uint8_t *key, const uint8_t *iv, uint8_t *data, size_t len);

    // RFC 3711 section 4.1.1 IV: (salt * 2^16) XOR (ssrc * 2^64) XOR (i * 2^16)
    void create_iv(uint8_t iv[16], uint64_t index, uint32_t ssrc);

    uint8_t master_key_[master_key_size];
    uint8_t master_salt_[master_salt_size];
    uint8_t rtp_key_[session_key_size];
    uint8_t rtp_salt_[session_salt_size];
    uint8_t rtp_auth_[session_auth_size];

    bool enabled_;

    // SRTP tracks the rollover counter per SSRC (RFC 3711 section 3.3.1), so
    // a single session can protect several streams (e.g. video + audio) that
    // each have their own sequence number space.
    struct ssrc_state
    {
        uint32_t ssrc;
        uint32_t roc;      // rollover counter
        uint16_t last_seq; // previous RTP sequence number
        bool valid;
    };

    static constexpr size_t max_ssrcs = 4;
    ssrc_state ssrcs_[max_ssrcs];
};
