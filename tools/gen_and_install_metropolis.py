import re

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
    assert len(glyphs) == 95, f'Expected 95 glyphs, got {len(glyphs)}'
    
    return hex_tokens, glyphs

sizes_caps = [(10, 7), (12, 9), (18, 13), (28, 20)]

header_code = '''#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════════
//  METROPOLIS-MEDIUM GLOBAL FONT DATA
//  Source: /font/Metropolis-Medium.ttf (SIL Open Font License)
//  Auto-generated 4-BPP anti-aliased font tables for SpeedTracer
// ═══════════════════════════════════════════════════════════════════════════════

'''

for sz, cap in sizes_caps:
    tokens, glyphs = parse_font_c(f'scratch/metro_{sz}.c')
    
    header_code += f'// --- Metropolis-Medium Size {sz}px (Cap: {cap}px) ---\n'
    header_code += f'static const uint8_t smoothBits{sz}[] PROGMEM = {{\n'
    for i in range(0, len(tokens), 16):
        row = ','.join(tokens[i:i+16])
        header_code += f'  {row},\n'
    header_code += '};\n\n'
    
    header_code += f'static const SmoothGlyph smoothGlyphs{sz}[] PROGMEM = {{\n'
    for i, g in enumerate(glyphs):
        header_code += f'  {{{g[0]},{g[1]},{g[2]},{g[3]},{g[4]},{g[5]}}},'
        if (i + 1) % 4 == 0 or i == len(glyphs) - 1:
            header_code += '\n'
    header_code += '};\n\n'
    header_code += f'static const SmoothFont smoothFont{sz} = {{ smoothBits{sz}, smoothGlyphs{sz}, {cap} }};\n\n'

for dest in ['firmware/src/metropolis_fonts.h', 'schematikIO-Export/src/metropolis_fonts.h']:
    with open(dest, 'w', encoding='utf-8') as f:
        f.write(header_code)
    print(f'Successfully wrote {dest}')
