import os
import re

FONTS = [
    ("Metro", "font/Metropolis-Medium.ttf", "Metropolis Medium", [(10, 7), (12, 9), (18, 13), (28, 20), (36, 26)]),
    ("GoogleSans", "font/GoogleSans-Medium.ttf", "Google Sans Medium", [(10, 7), (12, 9), (18, 13), (28, 20), (36, 26)]),
    ("Galano", "font/Galano Grotesque Semi Bold.otf", "Galano Grotesque SemiBold", [(10, 7), (12, 9), (18, 13), (28, 20), (36, 26)]),
    ("GalanoItalic", "font/Galano Grotesque Semi Bold Italic.otf", "Galano Grotesque Italic", [(10, 7), (12, 9), (18, 13), (28, 20), (36, 26)]),
    ("SpaceGrotesk", "font/SpaceGrotesk-SemiBold.ttf", "Space Grotesk SemiBold", [(10, 7), (12, 9), (18, 13), (28, 20), (36, 26)]),
    ("Tektur", "font/Tektur-Medium.ttf", "Tektur Medium", [(10, 8), (12, 9), (18, 13), (28, 20), (36, 26)])
]

def parse_font_c(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        text = f.read()

    bmp_match = re.search(r'glyph_bitmap\[\]\s*=\s*\{(.*?)\};', text, re.DOTALL)
    raw_bmp = bmp_match.group(1)
    raw_bmp = re.sub(r'/\*.*?\*/', '', raw_bmp)
    hex_tokens = re.findall(r'0x[0-9a-fA-F]+', raw_bmp)

    dsc_match = re.search(r'glyph_dsc\[\]\s*=\s*\{(.*?)\};', text, re.DOTALL)
    raw_dsc = dsc_match.group(1)
    entries = re.findall(r'\{\.bitmap_index\s*=\s*(\d+),\s*\.adv_w\s*=\s*(\d+),\s*\.box_w\s*=\s*(\d+),\s*\.box_h\s*=\s*(\d+),\s*\.ofs_x\s*=\s*(-?\d+),\s*\.ofs_y\s*=\s*(-?\d+)\}', raw_dsc)

    glyphs = entries[1:96]
    assert len(glyphs) == 95, f'Expected 95 glyphs, got {len(glyphs)} in {filepath}'

    return hex_tokens, glyphs

header_code = '''#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════════
//  SPEEDTRACER MULTI-FONT SYSTEM (6 ANTI-ALIASED FONTS x 4 SIZES)
//  Auto-generated 4-BPP raw uncompressed font tables for live theme switching
// ═══════════════════════════════════════════════════════════════════════════════

enum SystemFontId {
  FONT_METROPOLIS = 0,
  FONT_GOOGLE_SANS = 1,
  FONT_GALANO = 2,
  FONT_GALANO_ITALIC = 3,
  FONT_SPACE_GROTESK = 4,
  FONT_TEKTUR = 5
};

static const char* const SYSTEM_FONT_NAMES[] = {
  "Metropolis",
  "Google Sans",
  "Galano Grotesque",
  "Galano Italic",
  "Space Grotesk",
  "Tektur"
};

'''

total_bytes = 0

for prefix, font_path, name, sizes_caps in FONTS:
    header_code += f"// ═══════════════════════════════════════════════════════════════════════════════\n"
    header_code += f"//  {name}\n"
    header_code += f"// ═══════════════════════════════════════════════════════════════════════════════\n\n"

    for sz, cap in sizes_caps:
        out_c = f"scratch/fonts_uncompressed/{prefix.lower()}_{sz}.c"
        tokens, glyphs = parse_font_c(out_c)
        total_bytes += len(tokens)

        header_code += f'// --- {name} {sz}px (Cap: {cap}px, {len(tokens)} bytes) ---\n'
        header_code += f'static const uint8_t smoothBits_{prefix}_{sz}[] PROGMEM = {{\n'
        for i in range(0, len(tokens), 16):
            row = ','.join(tokens[i:i+16])
            header_code += f'  {row},\n'
        header_code += '};\n\n'

        header_code += f'static const SmoothGlyph smoothGlyphs_{prefix}_{sz}[] PROGMEM = {{\n'
        for i, g in enumerate(glyphs):
            header_code += f'  {{{g[0]},{g[1]},{g[2]},{g[3]},{g[4]},{g[5]}}},'
            if (i + 1) % 4 == 0 or i == len(glyphs) - 1:
                header_code += '\n'
        header_code += '};\n\n'
        header_code += f'static const SmoothFont smoothFont_{prefix}_{sz} = {{ smoothBits_{prefix}_{sz}, smoothGlyphs_{prefix}_{sz}, {cap} }};\n\n'

# Backward-compatibility alias for Metropolis
for sz in [10, 12, 18, 28, 36]:
    header_code += f'static const SmoothFont& smoothFont{sz} = smoothFont_Metro_{sz};\n'
header_code += '\n'

print(f"Total bitmap bytes across all 24 fonts: {total_bytes} bytes (~{total_bytes // 1024} KB)")

with open('firmware/src/system_fonts.h', 'w', encoding='utf-8') as f:
    f.write(header_code)

with open('schematikIO-Export/src/system_fonts.h', 'w', encoding='utf-8') as f:
    f.write(header_code)

print("Generated firmware/src/system_fonts.h and schematikIO-Export/src/system_fonts.h successfully!")
