#!/usr/bin/env python3
"""Convert GNU Unifont `.hex` data into an LVGL v9 bitmap font.

GNU Unifont stores glyphs row-by-row:
- Halfwidth glyphs: 8x16, 16 bytes total, one byte per row
- Fullwidth glyphs: 16x16, 32 bytes total, two bytes per row

This already matches LVGL's expected MSB-first 1bpp row layout, so the
bitmap bytes can be copied directly once the glyph metadata is correct.
"""

import sys


GLYPH_HEIGHT = 16
HALFWIDTH_BYTES = 16
FULLWIDTH_BYTES = 32


def parse_unifont_hex(filepath):
    """Parse a Unifont `.hex` file into `{codepoint: (width, bitmap_bytes)}`."""
    glyphs = {}

    with open(filepath, "r", encoding="utf-8") as font_file:
        for raw_line in font_file:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue

            parts = line.split(":", 1)
            if len(parts) != 2:
                continue

            try:
                codepoint = int(parts[0], 16)
            except ValueError:
                continue

            hexdata = parts[1]
            byte_count = len(hexdata) // 2

            if byte_count == HALFWIDTH_BYTES:
                width = 8
            elif byte_count == FULLWIDTH_BYTES:
                width = 16
            else:
                continue

            glyphs[codepoint] = (width, bytes.fromhex(hexdata))

    return glyphs


def unifont_to_lvgl_bitmap(width, data):
    """Return validated row-major bitmap data for LVGL."""
    expected_len = ((width + 7) // 8) * GLYPH_HEIGHT
    if len(data) != expected_len:
        raise ValueError(
            f"Unexpected glyph size for width {width}: {len(data)} != {expected_len}"
        )
    return data


def collect_codepoints(glyphs):
    """Select the Unicode ranges needed by the dashboard UI."""
    included = set()

    for codepoint in range(0x20, 0x7F):
        if codepoint in glyphs:
            included.add(codepoint)

    for codepoint in range(0xA0, 0x100):
        if codepoint in glyphs:
            included.add(codepoint)

    included_ranges = [
        (0x2000, 0x206F),
        (0x2100, 0x214F),
        (0x2150, 0x218F),
        (0x2190, 0x21FF),
        (0x2200, 0x22FF),
        (0x2300, 0x23FF),
        (0x2500, 0x257F),
        (0x2580, 0x259F),
        (0x25A0, 0x25FF),
        (0x2600, 0x26FF),
        (0x3000, 0x303F),
        (0x3040, 0x309F),
        (0x30A0, 0x30FF),
        (0x4E00, 0x9FFF),
        (0xFE30, 0xFE4F),
        (0xFF00, 0xFFEF),
    ]

    for start, end in included_ranges:
        for codepoint in range(start, end + 1):
            if codepoint in glyphs:
                included.add(codepoint)

    return sorted(included)


def generate_lvgl_font_c(glyphs, output_path, font_name="unifont_16"):
    """Generate an LVGL v9-compatible C font file."""
    sorted_cps = collect_codepoints(glyphs)
    if not sorted_cps:
        raise ValueError("No glyphs selected for output")

    glyph_dscs = [
        {
            "bitmap_index": 0,
            "adv_w": 0,
            "box_w": 0,
            "box_h": 0,
            "ofs_x": 0,
            "ofs_y": 0,
        }
    ]
    all_bitmap = bytearray()

    for codepoint in sorted_cps:
        width, data = glyphs[codepoint]
        bitmap = unifont_to_lvgl_bitmap(width, data)
        bitmap_index = len(all_bitmap)
        all_bitmap.extend(bitmap)

        glyph_dscs.append(
            {
                "bitmap_index": bitmap_index,
                "adv_w": width * 16,
                "box_w": width,
                "box_h": GLYPH_HEIGHT,
                "ofs_x": 0,
                "ofs_y": 0,
            }
        )

    range_start = sorted_cps[0]
    range_length = sorted_cps[-1] - range_start + 1

    lines = [
        '#include "lvgl.h"',
        "",
        f"/* Unifont 16x16 bitmap font, {len(sorted_cps)} glyphs */",
        "/* Generated from GNU Unifont .hex file */",
        "",
        f"static const uint8_t {font_name}_bitmap[] = {{",
    ]

    for index in range(0, len(all_bitmap), 16):
        chunk = all_bitmap[index:index + 16]
        suffix = "," if index + 16 < len(all_bitmap) else ""
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + suffix)

    lines.extend(
        [
            "};",
            "",
            f"static const uint16_t {font_name}_rcp_list[] = {{",
        ]
    )

    for codepoint in sorted_cps:
        lines.append(f"    {codepoint - range_start}, /* U+{codepoint:04X} */")

    lines.extend(
        [
            "};",
            "",
            f"static const lv_font_fmt_txt_cmap_t {font_name}_cmaps[] = {{",
            "    {",
            f"        .range_start = 0x{range_start:04X},",
            f"        .range_length = {range_length},",
            "        .glyph_id_start = 1,",
            f"        .unicode_list = {font_name}_rcp_list,",
            "        .glyph_id_ofs_list = NULL,",
            f"        .list_length = {len(sorted_cps)},",
            "        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY,",
            "    },",
            "};",
            "",
            f"static const lv_font_fmt_txt_glyph_dsc_t {font_name}_glyph_dsc[] = {{",
        ]
    )

    for glyph in glyph_dscs:
        lines.append(
            "    "
            f"{{.bitmap_index = {glyph['bitmap_index']}, .adv_w = {glyph['adv_w']}, "
            f".box_w = {glyph['box_w']}, .box_h = {glyph['box_h']}, "
            f".ofs_x = {glyph['ofs_x']}, .ofs_y = {glyph['ofs_y']}}},"
        )

    lines.extend(
        [
            "};",
            "",
            f"static const lv_font_fmt_txt_dsc_t {font_name}_dsc = {{",
            f"    .glyph_bitmap = {font_name}_bitmap,",
            f"    .glyph_dsc = {font_name}_glyph_dsc,",
            f"    .cmaps = {font_name}_cmaps,",
            "    .kern_dsc = NULL,",
            "    .kern_scale = 0,",
            "    .cmap_num = 1,",
            "    .bpp = 1,",
            "    .kern_classes = 0,",
            "    .bitmap_format = LV_FONT_FMT_TXT_PLAIN,",
            "    .stride = 0,",
            "};",
            "",
            f"const lv_font_t {font_name} = {{",
            "    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,",
            "    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,",
            f"    .line_height = {GLYPH_HEIGHT},",
            "    .base_line = 0,",
            "    .subpx = LV_FONT_SUBPX_NONE,",
            "    .underline_position = 0,",
            "    .underline_thickness = 0,",
            f"    .dsc = &{font_name}_dsc,",
            "    .fallback = NULL,",
            "};",
            "",
        ]
    )

    with open(output_path, "w", encoding="utf-8") as output_file:
        output_file.write("\n".join(lines))

    print(
        f"Generated {output_path} with {len(sorted_cps)} glyphs, "
        f"{len(all_bitmap)} bytes bitmap"
    )
    return len(sorted_cps)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <unifont.hex> [output.c]")
        sys.exit(1)

    hex_path = sys.argv[1]
    output = sys.argv[2] if len(sys.argv) > 2 else "unifont_16.c"

    print(f"Parsing {hex_path}...")
    glyphs = parse_unifont_hex(hex_path)
    print(f"Found {len(glyphs)} glyphs")

    generate_lvgl_font_c(glyphs, output)
