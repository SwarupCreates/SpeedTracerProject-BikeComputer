import sys

def update_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Normalize to LF
    content_lf = content.replace('\r\n', '\n')

    old_render = (
        '    // Row 2: Slide-to-Start Slider (224x54, R=27, Y=190..244)\n'
        '    display.fillRoundRect(8, 190, 224, 54, 27, uiCard());\n'
        '    const char* sliderText = rideStarted ? "Swipe to open" : "Swipe to start";\n'
        '    int textW = textWidth(sliderText, 0);\n'
        '    int iconW = 12;\n'
        '    int gap = 6;\n'
        '    int totalTrackW = textW + gap + iconW;\n'
        '    int trackStartX = 86 + (146 - totalTrackW) / 2;\n'
        '    label(trackStartX, 190 + 21, sliderText, 0, uiMuted());\n'
        '    drawCustomIcon(display, trackStartX + textW + gap, 190 + 21, uiMuted(), CustomIcon::goNextIcon, iconW, iconW);\n'
        '    int pillX = 8 + homeSliderOffset;\n'
        '    display.fillRoundRect(pillX, 190, 78, 54, 27, 0xF800);\n'
        '    drawCustomIcon(display, pillX + (78 - 28) / 2, 190 + 13, 0xFFFF, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 28, 28);'
    )

    new_render = (
        '    // Row 2: Slide-to-Start Slider (224x54, R=27, Y=190..244)\n'
        '    display.fillRoundRect(8, 190, 224, 54, 27, uiCard());\n'
        '    const char* sliderText = rideStarted ? "Swipe to open" : "Swipe to start";\n'
        '    int textW = textWidth(sliderText, 0);\n'
        '    int trackStartX = 116 + (116 - textW) / 2;\n'
        '    label(trackStartX, 190 + 21, sliderText, 0, uiMuted());\n'
        '    int pillX = 8 + homeSliderOffset;\n'
        '    display.fillRoundRect(pillX, 190, 108, 54, 27, 0xF800);\n'
        '    drawCustomIcon(display, pillX + 27, 190 + 13, 0xFFFF, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 28, 28);\n'
        '    drawCustomIcon(display, pillX + 67, 190 + 20, 0xFFFF, CustomIcon::goNextIcon, 14, 14);'
    )

    old_release = (
        '    if (homeSliderDragging) {\n'
        '      if (homeSliderOffset >= 110) {'
    )

    new_release = (
        '    if (homeSliderDragging) {\n'
        '      if (homeSliderOffset >= 85) {'
    )

    old_move = (
        '    else if(homeSliderDragging){\n'
        '      homeSliderOffset = constrain(x - homeSliderStartX, 0, 146);\n'
        '      uiRevision++;\n'
        '    }'
    )

    new_move = (
        '    else if(homeSliderDragging){\n'
        '      homeSliderOffset = constrain(x - homeSliderStartX, 0, 116);\n'
        '      uiRevision++;\n'
        '    }'
    )

    old_start = '      if (y >= 186 && y < 248 && x >= 4 && x <= 8 + homeSliderOffset + 88) {'
    new_start = '      if (y >= 186 && y < 248 && x >= 4 && x <= 8 + homeSliderOffset + 116) {'

    assert old_render in content_lf, f'old_render not found in {path}'
    content_lf = content_lf.replace(old_render, new_render)

    assert old_release in content_lf, f'old_release not found in {path}'
    content_lf = content_lf.replace(old_release, new_release)

    assert old_move in content_lf, f'old_move not found in {path}'
    content_lf = content_lf.replace(old_move, new_move)

    assert old_start in content_lf, f'old_start not found in {path}'
    content_lf = content_lf.replace(old_start, new_start)

    # Convert back to CRLF
    content_crlf = content_lf.replace('\n', '\r\n')
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(content_crlf)
    print(f'Successfully updated {path}')

if __name__ == '__main__':
    update_file('firmware/src/quick_settings.h')
    update_file('schematikIO-Export/src/quick_settings.h')
