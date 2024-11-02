#pragma once

#include <stdint.h>

// https://www.ietf.org/rfc/rfc2326#section-10.12
typedef struct __attribute__((packed))
{
    char magic = '$'; // Magic encapsulation ASCII dollar sign (24 hexadecimal)
    uint8_t channel;  // Channel identifier
    uint16_t length;  // Network order
} rtp_over_tcp_hdr_t;

// RTP data header - http://www.ietf.org/rfc/rfc3550.txt
typedef struct __attribute__((packed))
{
    uint16_t version : 2;   // protocol version
    uint16_t padding : 1;   // padding flag
    uint16_t extension : 1; // header extension flag
    uint16_t cc : 4;        // CSRC count
    uint16_t marker : 1;    // marker bit
    uint16_t pt : 7;        // payload type
    uint16_t seq : 16;      // sequence number
    uint32_t ts;            // timestamp
    uint32_t ssrc;          // synchronization source
} rtp_hdr_t;

// https://datatracker.ietf.org/doc/html/rfc2435
typedef struct __attribute__((packed))
{
    uint32_t tspec : 8; // type-specific field
    uint32_t off : 24;  // fragment byte offset
    uint8_t type;       // id of jpeg decoder params
    uint8_t q;          // Q values 0-127 indicate the quantization tables. JPEG types 0 and 1 (and their corresponding types 64 and 65)
    uint8_t width;      // frame width in 8 pixel blocks
    uint8_t height;     // frame height in 8 pixel blocks
} jpeg_hdr_t;

typedef struct __attribute__((packed))
{
    uint16_t dri;
    uint16_t f : 1;
    uint16_t l : 1;
    uint16_t count : 14;
} jpeg_hdr_rst_t;

typedef struct __attribute__((packed))
{
    uint8_t mbz;
    uint8_t precision;
    uint16_t length;
} jpeg_hdr_qtable_t;
