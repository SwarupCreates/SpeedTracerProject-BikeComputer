import subprocess
import re
import os

cmd = [
    "npx", "lv_font_conv",
    "--font", "font/GoogleSans-Medium.ttf",
    "-r", "0x30-0x3a",
    "--size", "56",
    "--bpp", "4",
    "--format", "lvgl",
    "--no-compress",
    "--no-prefilter",
    "-o", "clock56_temp.c"
]

subprocess.run(cmd, shell=True, check=True)

with open("clock56_temp.c", "r", encoding="utf-8") as f:
    code = f.read()

# Extract glyph_bitmap array
bm_match = re.search(r"static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap\[\] = \{(.*?)\};", code, re.DOTALL)
if not bm_match:
    raise ValueError("Could not find glyph_bitmap in generated code")

bitmap_raw = bm_match.group(1).strip()
# Clean up hex numbers
hex_bytes = re.findall(r"0x[0-9a-fA-F]+", bitmap_raw)
print(f"Extracted {len(hex_bytes)} bitmap bytes")

# Extract glyph_dsc array
dsc_match = re.search(r"static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc\[\] = \{(.*?)\};", code, re.DOTALL)
if not dsc_match:
    raise ValueError("Could not find glyph_dsc in generated code")

dsc_entries = re.findall(r"\{\.bitmap_index\s*=\s*(\d+),\s*\.adv_w\s*=\s*(\d+),\s*\.box_w\s*=\s*(\d+),\s*\.box_h\s*=\s*(\d+),\s*\.ofs_x\s*=\s*(-?\d+),\s*\.ofs_y\s*=\s*(-?\d+)\}", dsc_match.group(1))
print(f"Extracted {len(dsc_entries)} glyph entries")
# Entry 0 is dummy, entries 1..10 are '0'..'9', entry 11 is ':'
# Format into SmoothClockGlyph: offset, advance, w, h, x, y
glyphs = []
for entry in dsc_entries[1:]:
    bm_idx, adv, w, h, ofs_x, ofs_y = entry
    glyphs.append({
        "offset": int(bm_idx),
        "advance": int(adv),
        "w": int(w),
        "h": int(h),
        "x": int(ofs_x),
        "y": int(ofs_y)
    })

# Format header content
header = """#pragma once
#include <Arduino.h>

// Google Sans Medium 56px Digits (0-9) and Colon (:) - Raw 4-BPP Antialiased Glyphs
struct SmoothClockGlyph {
  uint16_t offset;
  uint16_t advance;
  uint8_t w, h;
  int8_t x, y;
};

static const SmoothClockGlyph smoothClockGlyphs56[11] PROGMEM = {
"""

for i, g in enumerate(glyphs):
    char_label = chr(0x30 + i) if i < 10 else "':'"
    header += f"  {{{g['offset']}, {g['advance']}, {g['w']}, {g['h']}, {g['x']}, {g['y']}}}, // {char_label}\n"

header += """};

static const uint8_t smoothClockBits56[] PROGMEM = {
"""

for i in range(0, len(hex_bytes), 16):
    line = "  " + ", ".join(hex_bytes[i:i+16]) + ",\n"
    header += line

header += """};
"""

for dest in ["firmware/src/smooth_clock56.h", "schematikIO-Export/src/smooth_clock56.h"]:
    with open(dest, "w", encoding="utf-8") as f:
        f.write(header)
    print(f"Wrote {dest}")

if os.path.exists("clock56_temp.c"):
    os.remove("clock56_temp.c")
print("Done!")
