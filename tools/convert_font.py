#!/usr/bin/env python3
"""
SpeedTracer LVGL Font Conversion Automation Script
Converts TrueType/OpenType fonts to LVGL 9 C-arrays with automated subsetting.
Requires: Node.js and lv_font_conv (`npm install -g lv_font_conv`)
"""

import sys
import os
import subprocess

DEFAULT_SIZES = [14, 18, 24, 32, 48]
DEFAULT_RANGE = "0x20-0x7E" # Standard ASCII (numbers, symbols, letters)

def convert_font(font_path, size, output_dir="firmware/src/fonts"):
    if not os.path.exists(font_path):
        print(f"Error: Font file not found at {font_path}")
        return False
        
    os.makedirs(output_dir, exist_ok=True)
    base_name = os.path.splitext(os.path.basename(font_path))[0].lower().replace("-", "_")
    output_c = os.path.join(output_dir, f"font_{base_name}_{size}.c")
    
    cmd = [
        "npx", "lv_font_conv",
        "--font", font_path,
        "--range", DEFAULT_RANGE,
        "--size", str(size),
        "--bpp", "4",
        "--format", "lvgl",
        "-o", output_c
    ]
    
    print(f"Generating: {output_c} ...")
    result = subprocess.run(cmd, shell=True)
    return result.returncode == 0

def main():
    if len(sys.argv) < 2:
        print("Usage: python convert_font.py <path_to_font.ttf> [size1 size2 ...]")
        print("Example: python convert_font.py Inter-Bold.ttf 18 24 48")
        sys.exit(1)
        
    font_file = sys.argv[1]
    sizes = [int(s) for s in sys.argv[2:]] if len(sys.argv) > 2 else DEFAULT_SIZES
    
    for s in sizes:
        convert_font(font_file, s)
        
    print("Done! Add the generated files to your firmware project and declare with LV_FONT_DECLARE(font_name).")

if __name__ == "__main__":
    main()
