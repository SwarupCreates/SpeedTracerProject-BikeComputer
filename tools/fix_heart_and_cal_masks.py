import os
from PIL import Image

def generate_masks():
    # 1. Heart Rate Mask (24x24) from high-res Figma Frame 27
    heart_tight = Image.open('tools/heart_tight.png')
    w, h = heart_tight.size
    min_x, max_x, min_y, max_y = 999, -1, 999, -1
    for y in range(h):
        for x in range(w):
            if heart_tight.getpixel((x, y))[1] > 50:
                if x < min_x: min_x = x
                if x > max_x: max_x = x
                if y < min_y: min_y = y
                if y > max_y: max_y = y

    heart_exact = heart_tight.crop((min_x, min_y, max_x + 1, max_y + 1))
    heart_scaled = heart_exact.resize((24, 22), Image.Resampling.LANCZOS)
    canvas_hr = Image.new('L', (24, 24), 0)
    for y in range(22):
        for x in range(24):
            g = heart_scaled.getpixel((x, y))[1]
            val = int(max(0, min(255, (g - 35) * 255 / (160 - 35))))
            canvas_hr.putpixel((x, y + 1), val)

    mask_hr = [canvas_hr.getpixel((x, y)) for y in range(24) for x in range(24)]

    # 2. Calories Mask (24x24) from 1:1 Figma Frame 1
    f1 = Image.open('tools/figma_frame_1.png')
    crop_cal = f1.crop((162, 113, 186, 137))
    bg_r, bg_g, bg_b = (23, 23, 23)
    vr, vg, vb = 255 - bg_r, 89 - bg_g, 0 - bg_b
    vlen = (vr*vr + vg*vg + vb*vb) ** 0.5

    mask_cal = []
    for y in range(24):
        for x in range(24):
            r, g, b = crop_cal.getpixel((x, y))[:3]
            pr, pg, pb = r - bg_r, g - bg_g, b - bg_b
            proj = (pr*vr + pg*vg + pb*vb) / vlen
            alpha = int(round(proj * 255.0 / vlen))
            mask_cal.append(max(0, min(255, alpha)))

    return mask_hr, mask_cal

def format_mask(name, mask):
    lines = [f"// {name}: 24x24 (576 bytes)", f"static const uint8_t {name}[576] = {{"]
    for y in range(24):
        row = mask[y*24 : (y+1)*24]
        row_str = "    " + ", ".join(f"{v:3d}" for v in row) + ","
        lines.append(row_str)
    lines.append("};")
    return "\n".join(lines)

def update_file(filepath, hr_code, cal_code):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Replace heartRateIconMask
    import re
    hr_pattern = re.compile(r'// heartRateIcon:.*?\nstatic const uint8_t heartRateIconMask\[\d+\] = \{.*?\};', re.DOTALL)
    if not hr_pattern.search(content):
        raise RuntimeError(f"Could not find heartRateIconMask in {filepath}")
    content = hr_pattern.sub(hr_code, content)

    # Replace caloriesIconMask
    cal_pattern = re.compile(r'// caloriesIcon:.*?\nstatic const uint8_t caloriesIconMask\[\d+\] = \{.*?\};', re.DOTALL)
    if not cal_pattern.search(content):
        raise RuntimeError(f"Could not find caloriesIconMask in {filepath}")
    content = cal_pattern.sub(cal_code, content)

    # Replace customWidths and customHeights
    # index 3 (calories) and index 6 (heart rate)
    # customWidths[] = {16,16,16,32,16,16,36,32,...} -> {16,16,16,24,16,16,24,32,...}
    content = content.replace(
        "static const uint8_t customWidths[] = {16,16,16,32,16,16,36,32,",
        "static const uint8_t customWidths[] = {16,16,16,24,16,16,24,32,"
    )
    content = content.replace(
        "static const uint8_t customHeights[] = {16,16,16,32,16,16,36,32,",
        "static const uint8_t customHeights[] = {16,16,16,24,16,16,24,32,"
    )

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Updated {filepath} successfully.")

def main():
    mask_hr, mask_cal = generate_masks()
    hr_code = format_mask("heartRateIconMask", mask_hr)
    cal_code = format_mask("caloriesIconMask", mask_cal)

    targets = [
        "firmware/src/custom_icon_masks.h",
        "schematikIO-Export/src/custom_icon_masks.h"
    ]

    for t in targets:
        update_file(t, hr_code, cal_code)

    print("\nAll target files updated!")

if __name__ == "__main__":
    main()
