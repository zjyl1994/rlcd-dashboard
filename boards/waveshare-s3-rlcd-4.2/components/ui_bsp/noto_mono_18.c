/*******************************************************************************
 * Size: 18 px
 * Bpp: 1
 * Opts: --size 18 --bpp 1 --no-compress --no-kerning --format lvgl --font boards/waveshare-s3-rlcd-4.2/components/ui_bsp/NotoSansMonoCJKsc-GBK.ttf -r 0x20-0x7e -o /tmp/noto_mono_18.c --lv-font-name noto_mono_18
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef NOTO_MONO_18
#define NOTO_MONO_18 1
#endif

#if NOTO_MONO_18

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xc3, 0xc0,

    /* U+0022 "\"" */
    0xde, 0xf7, 0xbd, 0x80,

    /* U+0023 "#" */
    0x32, 0x32, 0x22, 0xff, 0x24, 0x24, 0x24, 0x24,
    0xff, 0x44, 0x44, 0x4c, 0x48,

    /* U+0024 "$" */
    0x8, 0x8, 0x3e, 0x62, 0x60, 0x60, 0x70, 0x38,
    0x1e, 0xe, 0x7, 0x3, 0x3, 0x47, 0x3e, 0x8,
    0x8,

    /* U+0025 "%" */
    0x60, 0x91, 0x92, 0x94, 0x94, 0x68, 0x16, 0x19,
    0x29, 0x49, 0x49, 0x89, 0x6,

    /* U+0026 "&" */
    0x38, 0x36, 0x1b, 0xd, 0x86, 0x81, 0x80, 0xc4,
    0xe2, 0xcb, 0x67, 0x31, 0x9c, 0xe7, 0x90,

    /* U+0027 "'" */
    0xff, 0xc0,

    /* U+0028 "(" */
    0x8, 0xcc, 0x46, 0x33, 0x18, 0xc6, 0x31, 0x8c,
    0x31, 0x86, 0x30, 0xc0,

    /* U+0029 ")" */
    0x86, 0x18, 0x43, 0x18, 0x63, 0x18, 0xc6, 0x31,
    0x98, 0xcc, 0x66, 0x0,

    /* U+002A "*" */
    0x10, 0x21, 0xf1, 0xc2, 0x80, 0x0,

    /* U+002B "+" */
    0x18, 0x18, 0x18, 0x18, 0xff, 0x18, 0x18, 0x18,
    0x18,

    /* U+002C "," */
    0x6c, 0xbc,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x2, 0x4, 0x18, 0x20, 0x41, 0x2, 0x4, 0x10,
    0x20, 0xc1, 0x2, 0xc, 0x10, 0x20, 0x81, 0x0,

    /* U+0030 "0" */
    0x38, 0xd9, 0x16, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8d, 0x13, 0x63, 0x80,

    /* U+0031 "1" */
    0x38, 0xf0, 0x60, 0xc1, 0x83, 0x6, 0xc, 0x18,
    0x30, 0x60, 0xcf, 0xe0,

    /* U+0032 "2" */
    0x79, 0x98, 0x18, 0x30, 0x60, 0xc3, 0x6, 0x18,
    0x61, 0x83, 0xf, 0xe0,

    /* U+0033 "3" */
    0x7d, 0x9c, 0x18, 0x30, 0x61, 0x8e, 0x6, 0x6,
    0xe, 0x1c, 0x67, 0x80,

    /* U+0034 "4" */
    0xe, 0xe, 0x1e, 0x16, 0x36, 0x66, 0x46, 0xc6,
    0xff, 0x6, 0x6, 0x6, 0x6,

    /* U+0035 "5" */
    0x7e, 0x81, 0x2, 0x4, 0xf, 0x93, 0x3, 0x6,
    0xc, 0x1e, 0x67, 0x80,

    /* U+0036 "6" */
    0x3c, 0xc9, 0x86, 0xc, 0x1b, 0xbb, 0xe3, 0xc7,
    0x8d, 0x1b, 0x63, 0x80,

    /* U+0037 "7" */
    0xfe, 0xc, 0x30, 0x60, 0x83, 0x6, 0x8, 0x30,
    0x60, 0xc1, 0x83, 0x0,

    /* U+0038 "8" */
    0x3d, 0x9f, 0x1e, 0x3c, 0x6f, 0x8e, 0x26, 0xc7,
    0x8f, 0x1b, 0x33, 0xc0,

    /* U+0039 "9" */
    0x38, 0xdb, 0x16, 0x3c, 0x78, 0xf3, 0xbb, 0x6,
    0xc, 0x32, 0x67, 0x80,

    /* U+003A ":" */
    0xf0, 0x0, 0xf0,

    /* U+003B ";" */
    0xf0, 0x0, 0xf5, 0x80,

    /* U+003C "<" */
    0x7, 0x1e, 0xf0, 0xc0, 0x78, 0xe, 0x3,

    /* U+003D "=" */
    0xff, 0x0, 0x0, 0x0, 0xff,

    /* U+003E ">" */
    0xe0, 0x78, 0xf, 0x3, 0x1e, 0x70, 0xc0,

    /* U+003F "?" */
    0x3d, 0x9c, 0x18, 0x30, 0x61, 0x86, 0x8, 0x30,
    0x60, 0x0, 0x3, 0x6, 0x0,

    /* U+0040 "@" */
    0x1c, 0x32, 0x61, 0x61, 0xc1, 0xc7, 0xcd, 0xd1,
    0xd1, 0xd3, 0xcd, 0xc0, 0x60, 0x60, 0x32, 0x1e,

    /* U+0041 "A" */
    0x1c, 0xa, 0x5, 0x2, 0x83, 0x61, 0xb0, 0x88,
    0x44, 0x7f, 0x31, 0x98, 0xc8, 0x2c, 0x18,

    /* U+0042 "B" */
    0xf9, 0x9f, 0x1e, 0x3c, 0x79, 0xbe, 0x66, 0xc7,
    0x8f, 0x1e, 0x6f, 0x80,

    /* U+0043 "C" */
    0x1e, 0x72, 0x60, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0x60, 0x73, 0x1e,

    /* U+0044 "D" */
    0xf1, 0x9b, 0x36, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x36, 0x6f, 0x0,

    /* U+0045 "E" */
    0xff, 0x83, 0x6, 0xc, 0x18, 0x3f, 0x60, 0xc1,
    0x83, 0x6, 0xf, 0xe0,

    /* U+0046 "F" */
    0xff, 0x83, 0x6, 0xc, 0x18, 0x3f, 0x60, 0xc1,
    0x83, 0x6, 0xc, 0x0,

    /* U+0047 "G" */
    0x1e, 0x33, 0x60, 0xc0, 0xc0, 0xc0, 0xcf, 0xc3,
    0xc3, 0xc3, 0x63, 0x23, 0x1e,

    /* U+0048 "H" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xff, 0xe3, 0xc7,
    0x8f, 0x1e, 0x3c, 0x60,

    /* U+0049 "I" */
    0xfc, 0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3,
    0xc, 0xfc,

    /* U+004A "J" */
    0x6, 0xc, 0x18, 0x30, 0x60, 0xc1, 0x83, 0x6,
    0xc, 0x1e, 0x67, 0xc0,

    /* U+004B "K" */
    0xc6, 0xc6, 0xcc, 0xc8, 0xd8, 0xd8, 0xf8, 0xec,
    0xcc, 0xc4, 0xc6, 0xc2, 0xc3,

    /* U+004C "L" */
    0xc1, 0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xc1,
    0x83, 0x6, 0xf, 0xe0,

    /* U+004D "M" */
    0xc7, 0x8f, 0xbf, 0x7e, 0xfd, 0xfd, 0xeb, 0xd7,
    0x8f, 0x1e, 0x3c, 0x60,

    /* U+004E "N" */
    0xc7, 0xcf, 0x9f, 0x3e, 0x7a, 0xf5, 0xeb, 0xd7,
    0x9f, 0x3e, 0x7c, 0x60,

    /* U+004F "O" */
    0x3c, 0x66, 0x42, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0x42, 0x66, 0x3c,

    /* U+0050 "P" */
    0xf9, 0x9b, 0x1e, 0x3c, 0x78, 0xf3, 0x7c, 0xc1,
    0x83, 0x6, 0xc, 0x0,

    /* U+0051 "Q" */
    0x3c, 0x66, 0x42, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0x66, 0x66, 0x3c, 0x18, 0xc, 0x7,

    /* U+0052 "R" */
    0xf9, 0x9f, 0x1e, 0x3c, 0x79, 0xbe, 0x6c, 0xc9,
    0x9b, 0x36, 0x3c, 0x60,

    /* U+0053 "S" */
    0x3d, 0x8b, 0x6, 0xe, 0xe, 0xf, 0xf, 0xe,
    0xc, 0x1e, 0x67, 0x80,

    /* U+0054 "T" */
    0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18,

    /* U+0055 "U" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x1b, 0x67, 0x80,

    /* U+0056 "V" */
    0x41, 0x30, 0x98, 0xcc, 0x62, 0x31, 0x10, 0xc8,
    0x6c, 0x36, 0xa, 0x5, 0x3, 0x81, 0xc0,

    /* U+0057 "W" */
    0x81, 0xc0, 0xe0, 0x51, 0x2d, 0x96, 0xcb, 0x75,
    0xaa, 0x55, 0x33, 0x99, 0xcc, 0xe6, 0x60,

    /* U+0058 "X" */
    0xc7, 0x8d, 0x93, 0x62, 0x87, 0xe, 0x14, 0x68,
    0xd9, 0x16, 0x3c, 0x60,

    /* U+0059 "Y" */
    0xc3, 0xc3, 0x42, 0x66, 0x26, 0x3c, 0x3c, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18,

    /* U+005A "Z" */
    0xfe, 0xc, 0x30, 0x61, 0x83, 0x4, 0x18, 0x20,
    0xc1, 0x6, 0xf, 0xe0,

    /* U+005B "[" */
    0xfe, 0x31, 0x8c, 0x63, 0x18, 0xc6, 0x31, 0x8c,
    0x63, 0x18, 0xf8,

    /* U+005C "\\" */
    0x81, 0x3, 0x2, 0x4, 0x4, 0x8, 0x10, 0x10,
    0x20, 0x60, 0x40, 0x81, 0x81, 0x2, 0x2, 0x4,

    /* U+005D "]" */
    0xf8, 0xc6, 0x31, 0x8c, 0x63, 0x18, 0xc6, 0x31,
    0x8c, 0x63, 0xf8,

    /* U+005E "^" */
    0x10, 0x70, 0xa1, 0x46, 0xc8, 0x91, 0x63,

    /* U+005F "_" */
    0xfe,

    /* U+0060 "`" */
    0x66, 0x20,

    /* U+0061 "a" */
    0x7c, 0x98, 0x18, 0x33, 0xec, 0xf1, 0xe3, 0xce,
    0xec,

    /* U+0062 "b" */
    0xc1, 0x83, 0x6, 0xd, 0xdd, 0xb1, 0xe3, 0xc7,
    0x8f, 0x1e, 0x3c, 0xdf, 0x0,

    /* U+0063 "c" */
    0x3e, 0xc5, 0x86, 0xc, 0x18, 0x30, 0x70, 0x62,
    0x7c,

    /* U+0064 "d" */
    0x6, 0xc, 0x18, 0x33, 0xec, 0xf1, 0xe3, 0xc7,
    0x8f, 0x1e, 0x36, 0xe7, 0xc0,

    /* U+0065 "e" */
    0x3c, 0x66, 0xc3, 0xc3, 0xff, 0xc0, 0xc0, 0xe0,
    0x62, 0x3e,

    /* U+0066 "f" */
    0x1f, 0x38, 0x30, 0x30, 0xfe, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30,

    /* U+0067 "g" */
    0x7f, 0xc4, 0xc6, 0xc6, 0x46, 0x7c, 0xc0, 0xc0,
    0xc0, 0x3e, 0xc3, 0xc3, 0xc7, 0x7c,

    /* U+0068 "h" */
    0xc1, 0x83, 0x6, 0xd, 0xdc, 0xf1, 0xe3, 0xc7,
    0x8f, 0x1e, 0x3c, 0x78, 0xc0,

    /* U+0069 "i" */
    0xf0, 0xff, 0xff, 0xf0,

    /* U+006A "j" */
    0xc, 0x30, 0x0, 0xc, 0x30, 0xc3, 0xc, 0x30,
    0xc3, 0xc, 0x30, 0xc3, 0x1f, 0xe0,

    /* U+006B "k" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xc6, 0xc4, 0xcc, 0xd8,
    0xd8, 0xe8, 0xec, 0xc4, 0xc6, 0xc2,

    /* U+006C "l" */
    0xc6, 0x31, 0x8c, 0x63, 0x18, 0xc6, 0x31, 0x8c,
    0x3c,

    /* U+006D "m" */
    0xf6, 0xdb, 0xd3, 0xd3, 0xd3, 0xd3, 0xd3, 0xd3,
    0xd3, 0xd3,

    /* U+006E "n" */
    0xdd, 0xcf, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8c,

    /* U+006F "o" */
    0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0x66, 0x3c,

    /* U+0070 "p" */
    0xdd, 0xdb, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0xcd,
    0xf3, 0x6, 0xc, 0x18, 0x0,

    /* U+0071 "q" */
    0x3e, 0xcf, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0x6e,
    0x7c, 0x18, 0x30, 0x60, 0xc0,

    /* U+0072 "r" */
    0xdf, 0xe3, 0x86, 0xc, 0x18, 0x30, 0x60, 0xc1,
    0x80,

    /* U+0073 "s" */
    0x7d, 0x8b, 0x7, 0x7, 0x83, 0x81, 0x83, 0x86,
    0xf8,

    /* U+0074 "t" */
    0x30, 0x60, 0xc7, 0xf3, 0x6, 0xc, 0x18, 0x30,
    0x60, 0xc1, 0xc1, 0xe0,

    /* U+0075 "u" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0xce,
    0xec,

    /* U+0076 "v" */
    0x83, 0x8f, 0x1a, 0x24, 0x4d, 0x8b, 0x14, 0x28,
    0x70,

    /* U+0077 "w" */
    0xc1, 0xe4, 0xf6, 0x6a, 0xa5, 0x52, 0xa9, 0x54,
    0xaa, 0x77, 0x11, 0x80,

    /* U+0078 "x" */
    0xc6, 0x89, 0xb1, 0x43, 0x87, 0xa, 0x36, 0xc5,
    0x8c,

    /* U+0079 "y" */
    0x83, 0x8f, 0x1a, 0x26, 0x4d, 0x8b, 0x14, 0x38,
    0x30, 0x40, 0x83, 0x1c, 0x0,

    /* U+007A "z" */
    0x7e, 0xc, 0x30, 0x41, 0x86, 0xc, 0x30, 0x61,
    0xfc,

    /* U+007B "{" */
    0x1c, 0xc3, 0xc, 0x30, 0xc3, 0xc, 0xc0, 0xc3,
    0xc, 0x30, 0xc3, 0xc, 0x1c,

    /* U+007C "|" */
    0xff, 0xff, 0xf0,

    /* U+007D "}" */
    0xe0, 0xc3, 0xc, 0x30, 0xc3, 0xc, 0xc, 0xc3,
    0xc, 0x30, 0xc3, 0xc, 0xe0,

    /* U+007E "~" */
    0x30, 0x24, 0x81, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 144, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 144, .box_w = 2, .box_h = 13, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 5, .adv_w = 144, .box_w = 5, .box_h = 5, .ofs_x = 2, .ofs_y = 9},
    {.bitmap_index = 9, .adv_w = 144, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 22, .adv_w = 144, .box_w = 8, .box_h = 17, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 39, .adv_w = 144, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 52, .adv_w = 144, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 67, .adv_w = 144, .box_w = 2, .box_h = 5, .ofs_x = 4, .ofs_y = 9},
    {.bitmap_index = 69, .adv_w = 144, .box_w = 5, .box_h = 19, .ofs_x = 3, .ofs_y = -4},
    {.bitmap_index = 81, .adv_w = 144, .box_w = 5, .box_h = 19, .ofs_x = 2, .ofs_y = -4},
    {.bitmap_index = 93, .adv_w = 144, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 99, .adv_w = 144, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 108, .adv_w = 144, .box_w = 3, .box_h = 5, .ofs_x = 3, .ofs_y = -3},
    {.bitmap_index = 110, .adv_w = 144, .box_w = 5, .box_h = 1, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 111, .adv_w = 144, .box_w = 2, .box_h = 2, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 112, .adv_w = 144, .box_w = 7, .box_h = 18, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 128, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 140, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 164, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 144, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 189, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 201, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 225, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 249, .adv_w = 144, .box_w = 2, .box_h = 10, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 252, .adv_w = 144, .box_w = 2, .box_h = 13, .ofs_x = 3, .ofs_y = -3},
    {.bitmap_index = 256, .adv_w = 144, .box_w = 8, .box_h = 7, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 263, .adv_w = 144, .box_w = 8, .box_h = 5, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 268, .adv_w = 144, .box_w = 8, .box_h = 7, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 275, .adv_w = 144, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 288, .adv_w = 144, .box_w = 8, .box_h = 16, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 304, .adv_w = 144, .box_w = 9, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 319, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 331, .adv_w = 144, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 344, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 356, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 368, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 380, .adv_w = 144, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 393, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 405, .adv_w = 144, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 415, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 427, .adv_w = 144, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 440, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 452, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 464, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 476, .adv_w = 144, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 489, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 501, .adv_w = 144, .box_w = 8, .box_h = 16, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 517, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 529, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 541, .adv_w = 144, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 554, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 566, .adv_w = 144, .box_w = 9, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 581, .adv_w = 144, .box_w = 9, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 596, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 608, .adv_w = 144, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 621, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 633, .adv_w = 144, .box_w = 5, .box_h = 17, .ofs_x = 3, .ofs_y = -3},
    {.bitmap_index = 644, .adv_w = 144, .box_w = 7, .box_h = 18, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 660, .adv_w = 144, .box_w = 5, .box_h = 17, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 671, .adv_w = 144, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 678, .adv_w = 144, .box_w = 7, .box_h = 1, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 679, .adv_w = 144, .box_w = 4, .box_h = 3, .ofs_x = 2, .ofs_y = 11},
    {.bitmap_index = 681, .adv_w = 144, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 690, .adv_w = 144, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 703, .adv_w = 144, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 712, .adv_w = 144, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 725, .adv_w = 144, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 735, .adv_w = 144, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 749, .adv_w = 144, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 763, .adv_w = 144, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 776, .adv_w = 144, .box_w = 2, .box_h = 14, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 780, .adv_w = 144, .box_w = 6, .box_h = 18, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 794, .adv_w = 144, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 808, .adv_w = 144, .box_w = 5, .box_h = 14, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 817, .adv_w = 144, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 827, .adv_w = 144, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 836, .adv_w = 144, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 846, .adv_w = 144, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 859, .adv_w = 144, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 872, .adv_w = 144, .box_w = 7, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 881, .adv_w = 144, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 890, .adv_w = 144, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 902, .adv_w = 144, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 911, .adv_w = 144, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 920, .adv_w = 144, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 932, .adv_w = 144, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 941, .adv_w = 144, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 954, .adv_w = 144, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 963, .adv_w = 144, .box_w = 6, .box_h = 17, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 976, .adv_w = 144, .box_w = 1, .box_h = 20, .ofs_x = 4, .ofs_y = -5},
    {.bitmap_index = 979, .adv_w = 144, .box_w = 6, .box_h = 17, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 992, .adv_w = 144, .box_w = 9, .box_h = 3, .ofs_x = 0, .ofs_y = 5}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t noto_mono_18 = {
#else
lv_font_t noto_mono_18 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 20,          /*The maximum line height required by the font*/
    .base_line = 5,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if NOTO_MONO_18*/

