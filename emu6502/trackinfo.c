// Commodore 1541 disk drive sectors
// Source: https://ist.uwaterloo.ca/~schepers/formats/D64.TXT
//
// Field 1: The track number (count starts from 1)
// Field 2: How many sectors are in this track
// Field 3: Offset from the beginning of the disk, in sectors
// Field 4: Offset, in bytes, of the start byte of this track in a D64 file
//
// A sector is 256 bytes.

#include "em.h"

trackinfo_t TRACK_INFO[] = {

    { 0, 0, 0, 0}, // There is no track 0.

    {  1, 21,   0, 0x00000 },
    {  2, 21,  21, 0x01500 },
    {  3, 21,  42, 0x02A00 },
    {  4, 21,  63, 0x03F00 },
    {  5, 21,  84, 0x05400 },
    {  6, 21, 105, 0x06900 },
    {  7, 21, 126, 0x07E00 },
    {  8, 21, 147, 0x09300 },
    {  9, 21, 168, 0x0A800 },
    { 10, 21, 189, 0x0BD00 },

    { 11, 21, 210, 0x0D200 },
    { 12, 21, 231, 0x0E700 },
    { 13, 21, 252, 0x0FC00 },
    { 14, 21, 273, 0x11100 },
    { 15, 21, 294, 0x12600 },
    { 16, 21, 315, 0x13B00 },
    { 17, 21, 336, 0x15000 },
    { 18, 19, 357, 0x16500 },
    { 19, 19, 376, 0x17800 },
    { 20, 19, 395, 0x18B00 },

    { 21, 19, 414, 0x19E00 },
    { 22, 19, 433, 0x1B100 },
    { 23, 19, 452, 0x1C400 },
    { 24, 19, 471, 0x1D700 },
    { 25, 18, 490, 0x1EA00 },
    { 26, 18, 508, 0x1FC00 },
    { 27, 18, 526, 0x20E00 },
    { 28, 18, 544, 0x22000 },
    { 29, 18, 562, 0x23200 },
    { 30, 18, 580, 0x24400 },

    { 31, 17, 598, 0x25600 },
    { 32, 17, 615, 0x26700 },
    { 33, 17, 632, 0x27800 },
    { 34, 17, 649, 0x28900 },
    { 35, 17, 666, 0x29A00 },
    { 36, 17, 683, 0x2AB00 },
    { 37, 17, 700, 0x2BC00 },
    { 38, 17, 717, 0x2CD00 },
    { 39, 17, 734, 0x2DE00 },
    { 40, 17, 751, 0x2EF00 },

};

