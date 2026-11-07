with open('firmware/src/firmware.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Top Row
old_top = """  // Top Row: AVG (Left) and MAX (Right)
  label(18, 38, "AVG", 1, mutedCol);
  label(18, 48, (ride.elapsedSec ? (runningMode ? paceText(ride.avgSpeedKmh) : String(ride.avgSpeedKmh, 1)) : String("--")), 2, fgCol);
  label(222 - textWidth("MAX", 1), 38, "MAX", 1, mutedCol);
  String maxSp = (ride.elapsedSec ? (runningMode ? paceText(ride.maxSpeedKmh) : String(ride.maxSpeedKmh, 1)) : String("--"));
  label(222 - textWidth(maxSp, 2), 48, maxSp, 2, fgCol);"""

new_top = """  // Top Row: AVG (Left) and MAX (Right)
  label(18, 35, "AVG", 1, mutedCol);
  label(18, 52, (ride.elapsedSec ? (runningMode ? paceText(ride.avgSpeedKmh) : String(ride.avgSpeedKmh, 1)) : String("--")), 2, fgCol);
  label(222 - textWidth("MAX", 1), 35, "MAX", 1, mutedCol);
  String maxSp = (ride.elapsedSec ? (runningMode ? paceText(ride.maxSpeedKmh) : String(ride.maxSpeedKmh, 1)) : String("--"));
  label(222 - textWidth(maxSp, 2), 52, maxSp, 2, fgCol);"""

# 2. Heart Rate Row
old_hr = """  // Heart Rate Row (Frame 27: y = 201..245)
  label(18, 208, "AVG", 1, mutedCol);
  label(18, 222, (hrSamples ? String(hrSum / hrSamples) : String("--")), 2, fgCol);"""

new_hr = """  // Heart Rate Row (Frame 27: y = 201..245)
  label(18, 205, "AVG", 1, mutedCol);
  label(18, 223, (hrSamples ? String(hrSum / hrSamples) : String("--")), 2, fgCol);"""

old_hr_max = """  label(222 - textWidth("MAX", 1), 208, "MAX", 1, mutedCol);
  String maxHrStr = (hrSamples ? String(hrMax) : String("--"));
  label(222 - textWidth(maxHrStr, 2), 222, maxHrStr, 2, fgCol);"""

new_hr_max = """  label(222 - textWidth("MAX", 1), 205, "MAX", 1, mutedCol);
  String maxHrStr = (hrSamples ? String(hrMax) : String("--"));
  label(222 - textWidth(maxHrStr, 2), 223, maxHrStr, 2, fgCol);"""

# 3. Slider
old_slider = """  // Row 4 (y = 230, h = 54): REVERSE SLIDER "Swipe to end"
  display.fillRoundRect(8, 230, 224, 54, 27, cardBg);
  label(32, 253, "Swipe to end", 1, mutedCol);

  int pillX = 124 + controlSliderOffset;
  display.fillRoundRect(pillX, 230, 108, 54, 27, 0xE800);
  drawCustomIconFlippedH(display, pillX + 18, 249, 0xFFFF, CustomIcon::goNextIcon, 16, 16);
  drawCustomIcon(display, pillX + 54, 242, 0xFFFF, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 30, 30);"""

new_slider = """  // Row 4 (y = 230, h = 54): REVERSE SLIDER "Swipe to end" (Exact mirror of Home Page slider)
  display.fillRoundRect(8, 230, 224, 54, 27, cardBg);
  int textW = textWidth("Swipe to end", 0);
  int trackStartX = 8 + (116 - textW) / 2;
  label(trackStartX, 230 + 21, "Swipe to end", 0, mutedCol);

  int pillX = 124 + controlSliderOffset;
  display.fillRoundRect(pillX, 230, 108, 54, 27, 0xF800);
  drawCustomIconFlippedH(display, pillX + 27, 230 + 20, 0xFFFF, CustomIcon::goNextIcon, 14, 14);
  drawCustomIcon(display, pillX + 53, 230 + 13, 0xFFFF, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 28, 28);"""

def replace_all(content):
    for o, n in [(old_top, new_top), (old_hr, new_hr), (old_hr_max, new_hr_max), (old_slider, new_slider)]:
        if o in content:
            content = content.replace(o, n)
        else:
            o_crlf = o.replace('\n', '\r\n')
            n_crlf = n.replace('\n', '\r\n')
            if o_crlf in content:
                content = content.replace(o_crlf, n_crlf)
            else:
                print("Warning: not found:\n", o[:40])
    return content

for path in ['firmware/src/firmware.cpp', 'schematikIO-Export/src/firmware.cpp']:
    with open(path, 'r', encoding='utf-8') as f:
        c = f.read()
    c = replace_all(c)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(c)
    print("Updated", path)
