#include "micro_rtsp_streamer.h"

#include "rtp_payloads.h"

#include <stddef.h>

#include "esp_random.h"

// https://github.com/txgcwm/Linux-C-Examples/blob/master/h264/h264dec/rtcp.h

// RTP data header (http://www.ietf.org/rfc/rfc3550.txt)
struct rtp_hdr
{
    uint version : 2; // protocol version
    uint p : 1;       // padding flag
    uint x : 1;       // header extension flag
    uint cc : 4;      // CSRC count
    uint m : 1;       // marker bit
    uint pt : 7;      // payload type
    uint seq : 16;    // sequence number
    uint32_t ts;      // timestamp
    uint32_t ssrc;    // synchronization source
    uint32_t csrc[];  // optional CSRC list
} rtp_hdr;

// https://datatracker.ietf.org/doc/html/rfc2435

// The following definition is from RFC1890
#define RTP_PT_JPEG 26

struct jpeghdr
{
    uint322_t tspec : 8; // type-specific field
    uint322_t off : 24;  // fragment byte offset
    uint8_t type;        // id of jpeg decoder params
    uint8_t q;           // quantization factor (or table id)
    uint8_t width;       // frame width in 8 pixel blocks
    uint8_t height;      // frame height in 8 pixel blocks
};

struct jpeghdr_rst
{
    uint16_t dri;
    uint16_t f : 1;
    uint16_t l : 1;
    uint16_t count : 14;
};

struct jpeghdr_qtable
{
    uint8_t mbz;
    uint8_t precision;
    uint16_t length;
};

#define RTP_JPEG_RESTART 0x40

micro_rtsp_streamer::micro_rtsp_streamer()
{
    // Random number
    ssrc_ = esp_random();
    sequenceNumber_ = 0;
}

#define MAX_ESP32_MTU 1440

size_t micro_rtsp_streamer::create_jpg_packet(const uint8_t *jpg, const uint8_t *jpg_end, uint32_t timestamp)
{
    int fragmentLen = MAX_FRAGMENT_SIZE;
    if (fragmentLen + fragmentOffset > jpegLen) // Shrink last fragment if needed
        fragmentLen = jpegLen - fragmentOffset;

    bool isLastFragment = (fragmentOffset + fragmentLen) == jpegLen;

    struct rtp_header header = {
        .version = 2,
        .seq = sequenceNumber_,
        .marker = 1, // TODO = 1 if last fragfment
        .pt = rtp_payload.JPEG,
        .ts = timestamp,
        .ssrc = ssrc_};

    struct jpeghdr jpghdr = {
        .tspec = 0,           // type-specific field
        .off = offset,        // fragment byte offset
        .type = 0,            // id of jpeg decoder params
        .q = 0x5e,            // quantization factor (or table id)
        .width = width >> 3,  // frame width in 8 pixel blocks
        .height = height >> 3 // frame height in 8 pixel blocks
    };

    memset(RtpBuf, 0x00, sizeof(RtpBuf));
    // Prepare the first 4 byte of the packet. This is the Rtp over Rtsp header in case of TCP based transport
    RtpBuf[0] = '$'; // magic number
    RtpBuf[1] = 0;   // number of multiplexed subchannel on RTPS connection - here the RTP channel
    RtpBuf[2] = (RtpPacketSize & 0x0000FF00) >> 8;
    RtpBuf[3] = (RtpPacketSize & 0x000000FF);
    // Prepare the 12 byte RTP header
    RtpBuf[4] = 0x80;                                  // RTP version
    RtpBuf[5] = 0x1a | (isLastFragment ? 0x80 : 0x00); // JPEG payload (26) and marker bit
    RtpBuf[7] = m_SequenceNumber & 0x0FF;              // each packet is counted with a sequence counter
    RtpBuf[6] = m_SequenceNumber >> 8;
    RtpBuf[8] = (m_Timestamp & 0xFF000000) >> 24; // each image gets a timestamp
    RtpBuf[9] = (m_Timestamp & 0x00FF0000) >> 16;
    RtpBuf[10] = (m_Timestamp & 0x0000FF00) >> 8;
    RtpBuf[11] = (m_Timestamp & 0x000000FF);
    RtpBuf[12] = 0x13; // 4 byte SSRC (sychronization source identifier)
    RtpBuf[13] = 0xf9; // we just an arbitrary number here to keep it simple
    RtpBuf[14] = 0x7e;
    RtpBuf[15] = 0x67;

    // Prepare the 8 byte payload JPEG header
    RtpBuf[16] = 0x00;                                // type specific
    RtpBuf[17] = (fragmentOffset & 0x00FF0000) >> 16; // 3 byte fragmentation offset for fragmented images
    RtpBuf[18] = (fragmentOffset & 0x0000FF00) >> 8;
    RtpBuf[19] = (fragmentOffset & 0x000000FF);

    /*    These sampling factors indicate that the chrominance components of
       type 0 video is downsampled horizontally by 2 (often called 4:2:2)
       while the chrominance components of type 1 video are downsampled both
       horizontally and vertically by 2 (often called 4:2:0). */
    RtpBuf[20] = 0x00;         // type (fixme might be wrong for camera data) https://tools.ietf.org/html/rfc2435
    RtpBuf[21] = q;            // quality scale factor was 0x5e
    RtpBuf[22] = m_width / 8;  // width  / 8
    RtpBuf[23] = m_height / 8; // height / 8

    int headerLen = 24; // Inlcuding jpeg header but not qant table header
    if (includeQuantTbl)
    { // we need a quant header - but only in first packet of the frame
        // printf("inserting quanttbl\n");
        RtpBuf[24] = 0; // MBZ
        RtpBuf[25] = 0; // 8 bit precision
        RtpBuf[26] = 0; // MSB of lentgh

        int numQantBytes = 64;         // Two 64 byte tables
        RtpBuf[27] = 2 * numQantBytes; // LSB of length

        headerLen += 4;

        memcpy(RtpBuf + headerLen, quant0tbl, numQantBytes);
        headerLen += numQantBytes;

        memcpy(RtpBuf + headerLen, quant1tbl, numQantBytes);
        headerLen += numQantBytes;
    }
    // printf("Sending timestamp %d, seq %d, fragoff %d, fraglen %d, jpegLen %d\n", m_Timestamp, m_SequenceNumber, fragmentOffset, fragmentLen, jpegLen);

    // append the JPEG scan data to the RTP buffer
    memcpy(RtpBuf + headerLen, jpeg + fragmentOffset, fragmentLen);
    fragmentOffset += fragmentLen;

    m_SequenceNumber++; // prepare the packet counter for the next packet

    IPADDRESS otherip;
    IPPORT otherport;
    socketpeeraddr(m_Client, &otherip, &otherport);
}