# SpeedTracer LVGL Custom Fonts & SVG Icons Pipeline 🎨

This guide explains how to convert custom fonts (`.ttf` / `.woff`) and SVG vector icons exported from **Figma** into high-performance, memory-efficient C-arrays for LVGL on the ESP32-S3.

---

## 1. Custom Typography Pipeline (Figma ➔ LVGL)

### Prerequisites
Install the official LVGL font converter CLI:
```bash
npm install -g lv_font_conv
```

### Step 1: Export Font from Figma
Choose your favorite typography (e.g., **Inter-Bold**, **Montserrat-Bold**, or **DIN Condensed**).

### Step 2: Convert with Subsetting
To keep flash footprint small (~15 KB instead of megabytes), subset only the characters the bike computer needs:

```bash
# Convert 48pt Hero Speed Numbers (0-9, decimal point, colons, units):
npx lv_font_conv \
  --font Inter-Bold.ttf \
  --range 0x20-0x7E \
  --size 48 \
  --bpp 4 \
  --format lvgl \
  -o firmware/src/fonts/font_inter_bold_48.c

# Convert 18pt Card Headings:
npx lv_font_conv \
  --font Inter-SemiBold.ttf \
  --range 0x20-0x7E \
  --size 18 \
  --bpp 4 \
  --format lvgl \
  -o firmware/src/fonts/font_inter_semi_18.c
```

### Step 3: Use in Code
Declare the font in your UI file:
```c
LV_FONT_DECLARE(font_inter_bold_48);

lv_obj_t *speed_label = lv_label_create(speed_card);
lv_obj_set_style_text_font(speed_label, &font_inter_bold_48, 0);
lv_label_set_text(speed_label, "28.2");
```

---

## 2. Custom SVG Icons Pipeline (Figma ➔ LVGL)

### Method A: Single Icon Font via IcoMoon (Recommended)
1. Export all your icons from Figma as `.svg` (Bike, Heart, Satellite, Bluetooth, Flame, Lightning, Wrench, Gear).
2. Go to [IcoMoon.io](https://icomoon.io/app/) and drag-and-drop your SVG files.
3. Click **Generate Font** and download `speedtracer_icons.ttf`.
4. Convert using `lv_font_conv`:
   ```bash
   npx lv_font_conv \
     --font speedtracer_icons.ttf \
     --range 0xE000-0xE010 \
     --size 20 \
     --bpp 4 \
     --format lvgl \
     -o firmware/src/fonts/font_cycling_icons.c
   ```
5. In code, icons behave just like text glyphs! Changing `lv_obj_set_style_text_color()` instantly changes icon colors for Light/Dark themes and connection statuses.

---

### Method B: Runtime Vector Graphics via ThorVG (LVGL 9)
With `LV_USE_VECTOR_GRAPHIC 1` and `LV_USE_THORVG_INTERNAL 1` enabled in `firmware/include/lv_conf.h`, LVGL 9 can render raw SVG data directly stored in flash or loaded from the microSD card:

```c
// Loading and drawing an SVG icon directly in LVGL 9
lv_obj_t *svg_icon = lv_image_create(parent);
lv_image_set_src(svg_icon, "S:/assets/icons/bike.svg"); // from SD or flash
```
