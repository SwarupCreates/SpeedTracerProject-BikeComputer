import os
import subprocess
import re

FONTS = [
    ("Metro", "font/Metropolis-Medium.ttf", "Metropolis Medium"),
    ("GoogleSans", "font/GoogleSans-Medium.ttf", "Google Sans Medium"),
    ("Michroma", "font/Michroma-Regular.ttf", "Michroma Regular"),
    ("SpaceGrotesk", "font/SpaceGrotesk-SemiBold.ttf", "Space Grotesk SemiBold")
]

SIZES_CAPS = [(10, 7), (12, 9), (18, 13), (28, 20)]

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
//  SPEEDTRACER MULTI-FONT SYSTEM (4 ANTI-ALIASED FONTS x 4 SIZES)
//  Auto-generated 4-BPP font tables for live theme switching
// ═══════════════════════════════════════════════════════════════════════════════

enum SystemFontId {
  FONT_METROPOLIS = 0,
  FONT_GOOGLE_SANS = 1,
  FONT_MICHROMA = 2,
  FONT_SPACE_GROTESK = 3
};

static const char* const SYSTEM_FONT_NAMES[] = {
  "Metropolis",
  "Google Sans",
  "Michroma",
  "Space Grotesk"
};

'''

os.makedirs("scratch/fonts_gen", exist_ok=True)

for prefix, font_path, name in FONTS:
    print(f"Processing font: {name} ({font_path})...")
    header_code += f"// ═══════════════════════════════════════════════════════════════════════════════\n"
    header_code += f"//  {name}\n"
    header_code += f"// ═══════════════════════════════════════════════════════════════════════════════\n\n"
    
    for sz, cap in SIZES_CAPS:
        out_c = f"scratch/fonts_gen/{prefix.lower()}_{sz}.c"
        if not os.path.exists(out_c):
            cmd = f'npx lv_font_conv --font "{font_path}" --range 0x20-0x7E --size {sz} --bpp 4 --format lvgl -o "{out_c}"'
            res = subprocess.run(cmd, shell=True, capture_output=True, text=True)
            if res.returncode != 0:
                print(f"Error converting {font_path} size {sz}: {res.stderr}")
                exit(1)
        
        tokens, glyphs = parse_font_c(out_c)
        
        header_code += f'// --- {name} {sz}px (Cap: {cap}px) ---\n'
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
for sz in [10, 12, 18, 28]:
    header_code += f'static const SmoothFont& smoothFont{sz} = smoothFont_Metro_{sz};\n'
header_code += '\n'

with open('scratch/system_fonts.h', 'w', encoding='utf-8') as f:
    f.write(header_code)

print("Generated scratch/system_fonts.h successfully!")
