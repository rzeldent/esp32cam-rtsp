#include <string.h>
#include <esp32-hal-log.h>
#include <esp_random.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>

#include "micro_rtsp_srtp.h"
#include "micro_rtsp_structs.h"

micro_rtsp_srtp::micro_rtsp_srtp()
    : enabled_(false)
{
    memset(master_key_, 0, sizeof(master_key_));
    memset(master_salt_, 0, sizeof(master_salt_));
    memset(rtp_key_, 0, sizeof(rtp_key_));
    memset(rtp_salt_, 0, sizeof(rtp_salt_));
    memset(rtp_auth_, 0, sizeof(rtp_auth_));
    memset(ssrcs_, 0, sizeof(ssrcs_));
}

micro_rtsp_srtp::~micro_rtsp_srtp()
{
}

void micro_rtsp_srtp::generate_key_salt()
{
    for (size_t i = 0; i < master_key_size; i += sizeof(uint32_t))
    {
        uint32_t v = esp_random();
        memcpy(master_key_ + i, &v, sizeof(uint32_t));
    }

    for (size_t i = 0; i < master_salt_size; i += sizeof(uint32_t))
    {
        uint32_t v = esp_random();
        memcpy(master_salt_ + i, &v, sizeof(uint32_t));
    }

    derive_key(0x00, rtp_key_, sizeof(rtp_key_));
    derive_key(0x01, rtp_auth_, sizeof(rtp_auth_));
    derive_key(0x02, rtp_salt_, sizeof(rtp_salt_));
    memset(ssrcs_, 0, sizeof(ssrcs_));
    enabled_ = true;
}

bool micro_rtsp_srtp::set_key_salt(const uint8_t *key, const uint8_t *salt)
{
    if (key == nullptr || salt == nullptr)
        return false;

    memcpy(master_key_, key, master_key_size);
    memcpy(master_salt_, salt, master_salt_size);

    derive_key(0x00, rtp_key_, sizeof(rtp_key_));
    derive_key(0x01, rtp_auth_, sizeof(rtp_auth_));
    derive_key(0x02, rtp_salt_, sizeof(rtp_salt_));
    memset(ssrcs_, 0, sizeof(ssrcs_));
    enabled_ = true;
    return true;
}

// RFC 3711 section 4.3.3: session key = AES-CM PRF(master_key, x) where x = master_salt XOR (label || 0...0) and the counter block is (x * 2^16),
// i.e. a 16 byte block holding the (right aligned) master salt with the label in byte 7 and two trailing zero bytes. The keystream block counter is written into the two trailing bytes (14-15).
void micro_rtsp_srtp::derive_key(uint8_t label, uint8_t *out, size_t outlen)
{
    uint8_t input[16] = {0};
    memcpy(input, master_salt_, master_salt_size);
    input[7] ^= label;

    memset(out, 0, outlen);
    encrypt_counter(master_key_, input, out, outlen);
}

// AES-CM keystream (RFC 3711 section 4.1.1): E(k, IV) || E(k, IV+1) || ...
// where the block counter is kept in the two least significant bytes (14-15).
void micro_rtsp_srtp::encrypt_counter(const uint8_t *key, const uint8_t *iv, uint8_t *data, size_t len)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);

    uint8_t counter[16];
    memcpy(counter, iv, 16);

    size_t pos = 0;
    uint16_t block = 0;
    while (pos < len)
    {
        counter[14] = (uint8_t)(block >> 8);
        counter[15] = (uint8_t)(block & 0xff);

        uint8_t keystream[16];
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, counter, keystream);

        for (size_t j = 0; j < 16 && pos < len; j++, pos++)
            data[pos] ^= keystream[j];

        block++;
    }

    mbedtls_aes_free(&aes);
}

// RFC 3711 section 4.1.1: IV = (salt * 2^16) XOR (ssrc * 2^64) XOR (i * 2^16).
// Byte layout (matches FFmpeg/libSRTP): iv[0..13] = salt XOR (ssrc || index)
// spread as follows:
//   iv[0..3]   = salt[0..3]
//   iv[4..7]   = salt[4..7]   ^ ssrc
//   iv[8..13]  = salt[8..13]  ^ index (48 bit)
//   iv[14..15] = 0
void micro_rtsp_srtp::create_iv(uint8_t iv[16], uint64_t index, uint32_t ssrc)
{
    memset(iv, 0, 16);

    iv[4] = (uint8_t)(ssrc >> 24);
    iv[5] = (uint8_t)(ssrc >> 16);
    iv[6] = (uint8_t)(ssrc >> 8);
    iv[7] = (uint8_t)(ssrc & 0xff);

    // 48 bit packet index at bytes 8..13
    iv[8] = (uint8_t)(index >> 40);
    iv[9] = (uint8_t)(index >> 32);
    iv[10] = (uint8_t)(index >> 24);
    iv[11] = (uint8_t)(index >> 16);
    iv[12] = (uint8_t)(index >> 8);
    iv[13] = (uint8_t)(index & 0xff);

    for (int i = 0; i < 14; i++)
        iv[i] ^= rtp_salt_[i];
}

void micro_rtsp_srtp::protect_rtp(uint8_t *packet, size_t &len)
{
    if (!enabled_ || len < rtp_hdr_size)
        return;

    const uint16_t seq = (uint16_t)(((uint16_t)packet[2] << 8) | packet[3]);
    const uint32_t ssrc = ((uint32_t)packet[8] << 24) | ((uint32_t)packet[9] << 16) | ((uint32_t)packet[10] << 8) | packet[11];

    // Find (or allocate) the per-SSRC rollover counter state
    ssrc_state *state = nullptr;
    for (size_t i = 0; i < max_ssrcs; i++)
    {
        if (ssrcs_[i].valid && ssrcs_[i].ssrc == ssrc)
        {
            state = &ssrcs_[i];
            break;
        }
    }

    if (state == nullptr)
    {
        for (size_t i = 0; i < max_ssrcs; i++)
        {
            if (!ssrcs_[i].valid)
            {
                state = &ssrcs_[i];
                state->ssrc = ssrc;
                state->roc = 0;
                state->last_seq = 0;
                state->valid = true;
                break;
            }
        }

        if (state == nullptr)
            state = &ssrcs_[0]; // fallback, should not happen
    }

    // Update the rollover counter when the sequence number wraps (RFC 3711 3.3.1)
    if (seq < state->last_seq)
        state->roc++;

    state->last_seq = seq;

    const uint64_t index = ((uint64_t)state->roc << 16) | seq;

    // Build the IV and encrypt the RTP payload (after the 12 byte header)
    uint8_t iv[16];
    create_iv(iv, index, ssrc);
    encrypt_counter(rtp_key_, iv, packet + rtp_hdr_size, len - rtp_hdr_size);

    // HMAC-SHA1 over the whole RTP packet plus the ROC (RFC 3711 section 4.2), truncated to the 80 bit authentication tag.
    uint8_t hmac[20];
    mbedtls_md_context_t md;
    mbedtls_md_init(&md);
    mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
    mbedtls_md_hmac_starts(&md, rtp_auth_, sizeof(rtp_auth_));
    mbedtls_md_hmac_update(&md, packet, len);
    uint8_t roc_buf[4] = {(uint8_t)(state->roc >> 24), (uint8_t)(state->roc >> 16), (uint8_t)(state->roc >> 8), (uint8_t)(state->roc & 0xff)};
    mbedtls_md_hmac_update(&md, roc_buf, sizeof(roc_buf));
    mbedtls_md_hmac_finish(&md, hmac);
    mbedtls_md_free(&md);

    memcpy(packet + len, hmac, auth_tag_size);
    len += auth_tag_size;
}
