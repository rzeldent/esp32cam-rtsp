#include "jpg_section.h"

uint16_t jpg_section_t::data_length() const
{
    return (length_msb << 8) + length_lsb - sizeof(jpg_section_t::length_msb)- sizeof(jpg_section_t::length_lsb);
}

uint16_t jpg_section_t::section_length() const
{
    return flag == SOI || flag == EOI ? sizeof(jpg_section_t::framing) + sizeof(jpg_section_t::flag) : sizeof(jpg_section_t::framing) + sizeof(jpg_section_t::flag) + (length_msb << 8) + length_lsb;
}

bool jpg_section_t::is_valid_flag(const jpg_section_flag flag)
{
    return flag >= SOF0 && flag <= COM;
}

// from: https://www.disktuna.com/list-of-jpeg-markers/
const char *jpg_section_t::flag_name(const jpg_section_flag flag)
{
    switch (flag)
    {
    case DATA:
        return "DATA"; // DATA
    case SOF0:
        return "SOF0"; // Start of Frame 0	Baseline DCT
    case SOF1:
        return "SOF1"; // Start of Frame 1	Extended Sequential DCT
    case SOF2:
        return "SOF2"; // Start of Frame 2	Progressive DCT
    case SOF3:
        return "SOF3"; // Start of Frame 3	Lossless (sequential)
    case DHT:
        return "DHT"; // Define Huffman Table
    case SOF5:
        return "SOF5"; // Start of Frame 5	Differential sequential DCT
    case SOF6:
        return "SOF6"; // Start of Frame 6	Differential progressive DCT
    case SOF7:
        return "SOF7"; // Start of Frame 7	Differential lossless (sequential)
    case JPG:
        return "JPG"; // JPEG Extensions
    case SOF9:
        return "SOF9"; // Start of Frame 9	Extended sequential DCT, Arithmetic coding
    case SOF10:
        return "SOF10"; // 	Start of Frame 10	Progressive DCT, Arithmetic coding
    case SOF11:
        return "SOF11"; // 	Start of Frame 11	Lossless (sequential), Arithmetic coding
    case DAC:
        return "DAC"; // Define Arithmetic Coding
    case SOF13:
        return "SOF13"; // 	Start of Frame 13	Differential sequential DCT, Arithmetic coding
    case SOF14:
        return "SOF14"; // 	Start of Frame 14	Differential progressive DCT, Arithmetic coding
    case SOF15:
        return "SOF15"; // 	Start of Frame 15	Differential lossless (sequential), Arithmetic coding
    case RST0:
        return "RST0"; // Restart Marker 0
    case RST1:
        return "RST1"; // Restart Marker 1
    case RST2:
        return "RST2"; // Restart Marker 2
    case RST3:
        return "RST3"; // Restart Marker 3
    case RST4:
        return "RST4"; // Restart Marker 4
    case RST5:
        return "RST5"; // Restart Marker 5
    case RST6:
        return "RST6"; // Restart Marker 6
    case RST7:
        return "RST7"; // Restart Marker 7
    case SOI:
        return "SOI"; // Start of Image
    case EOI:
        return "EOI"; // End of Image
    case SOS:
        return "SOS"; // Start of Scan
    case DQT:
        return "DQT"; // Define Quantization Table
    case DNL:
        return "DNL"; // Define Number of Lines	(Not common)
    case DRI:
        return "DRI"; // Define Restart Interval
    case DHP:
        return "DHP"; // Define Hierarchical Progression	(Not common)
    case EXP:
        return "EXP"; // Expand Reference Component	(Not common)
    case APP0:
        return "APP0"; // Application Segment 0	JFIF – JFIF JPEG image, AVI1 – Motion JPEG (MJPG)
    case APP1:
        return "APP1"; // Application Segment 1	EXIF Metadata, TIFF IFD format, JPEG Thumbnail (160×120) Adobe XMP
    case APP2:
        return "APP2"; // Application Segment 2	ICC color profile, FlashPix
    case APP3:
        return "APP3"; // Application Segment 3	(Not common) JPS Tag for Stereoscopic JPEG images
    case APP4:
        return "APP4"; // Application Segment 4	(Not common)
    case APP5:
        return "APP5"; // Application Segment 5	(Not common)
    case APP6:
        return "APP6"; // Application Segment 6	(Not common) NITF Lossles profile
    case APP7:
        return "APP7"; // Application Segment 7	(Not common)
    case APP8:
        return "APP8"; // Application Segment 8	(Not common)
    case APP9:
        return "APP9"; // Application Segment 9	(Not common)
    case APP10:
        return "APP10"; // 	Application Segment 10 PhoTags	(Not common) ActiveObject (multimedia messages / captions)
    case APP11:
        return "APP11"; // 	Application Segment 11	(Not common) HELIOS JPEG Resources (OPI Postscript)
    case APP12:
        return "APP12"; // 	Application Segment 12	Picture Info (older digicams), Photoshop Save for Web: Ducky
    case APP13:
        return "APP13"; // 	Application Segment 13	Photoshop Save As: IRB, 8BIM, IPTC
    case APP14:
        return "APP14"; // 	Application Segment 14	(Not common)
    case APP15:
        return "APP15"; // 	Application Segment 15	(Not common)
    case JPG0:
        return "JPG0"; // JPEG Extension 0
    case JPG1:
        return "JPG1"; // JPEG Extension 1
    case JPG2:
        return "JPG2"; // JPEG Extension 2
    case JPG3:
        return "JPG3"; // JPEG Extension 3
    case JPG4:
        return "JPG4"; // JPEG Extension 4
    case JPG5:
        return "JPG5"; // JPEG Extension 5
    case JPG6:
        return "JPG6"; // JPEG Extension 6
    case JPG7:
        return "JPG7"; // 	SOF48	JPEG Extension 7 JPEG-LS	Lossless JPEG
    case JPG8:
        return "JPG8"; // LSE	JPEG Extension 8 JPEG-LS Extension	Lossless JPEG Extension Parameters
    case JPG9:
        return "JPG9"; // JPEG Extension 9	(Not common)
    case JPG10:
        return "JPG10"; // 	JPEG Extension 10	(Not common)
    case JPG11:
        return "JPG11"; // 	JPEG Extension 11	(Not common)
    case JPG12:
        return "JPG12"; // 	JPEG Extension 12	(Not common)
    case JPG13:
        return "JPG13"; // 	JPEG Extension 13	(Not common)
    case COM:
        return "COM"; // Comment
    }

    return "Unknown";
}
