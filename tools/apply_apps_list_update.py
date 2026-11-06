import sys

def update_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    content_lf = content.replace('\r\n', '\n')

    old_block = (
        '    if (appsListView) {\n'
        '      // LIST VIEW: 1 app per row, button with label beside in row, column layout\n'
        '      const int rowH = 48;\n'
        '      const int rowPitch = 56;\n'
        '\n'
        '      for (int i = 0; i < kNumApps; i++) {\n'
        '        int cardY = 66 + i * rowPitch - appsScroll;\n'
        '        if (cardY + rowH < 56 || cardY > 290) continue;\n'
        '\n'
        '        display.fillRoundRect(6, cardY, 228, rowH, 12, uiCard());\n'
        '        if (!darkTheme) display.drawRoundRect(6, cardY, 228, rowH, 12, 0xD69A);\n'
        '\n'
        '        // Circular colored app button on the left (R = 17, diameter 34)\n'
        '        int btnCx = 28;\n'
        '        int btnCy = cardY + rowH / 2;\n'
        '        int btnR = 17;\n'
        '        display.fillCircle(btnCx, btnCy, btnR, kApps[i].color);\n'
        '        drawCustomIcon(display, btnCx - 12, btnCy - 12, 0xFFFF, kApps[i].icon, 24, 24);\n'
        '\n'
        '        // App label beside button in row\n'
        '        label(54, cardY + 16, kApps[i].name, 1, uiFg());\n'
        '\n'
        '        // goNext chevron on the right\n'
        '        drawCustomIcon(display, 212, cardY + 16, uiMuted(), CustomIcon::goNextIcon, 16, 16);\n'
        '      }\n'
        '    } else {'
    )

    new_block = (
        '    if (appsListView) {\n'
        '      // LIST VIEW: 1 app per row, large button with label beside, clean minimal background\n'
        '      const int rowH = 56;\n'
        '      const int rowPitch = 64;\n'
        '      const int gutterX = 20;\n'
        '      const int btnR = 24;\n'
        '      const int btnCx = gutterX + btnR; // 44\n'
        '\n'
        '      for (int i = 0; i < kNumApps; i++) {\n'
        '        int cardY = 66 + i * rowPitch - appsScroll;\n'
        '        if (cardY + rowH < 56 || cardY > 290) continue;\n'
        '\n'
        '        // Circular colored app button on the left (R = 24, diameter 48)\n'
        '        int btnCy = cardY + rowH / 2;\n'
        '        display.fillCircle(btnCx, btnCy, btnR, kApps[i].color);\n'
        '        drawCustomIcon(display, btnCx - 16, btnCy - 16, 0xFFFF, kApps[i].icon, 32, 32);\n'
        '\n'
        '        // App label beside button in row (size 2 = 18pt font, more than double size 1)\n'
        '        label(btnCx + btnR + 14, cardY + 20, kApps[i].name, 2, uiFg());\n'
        '      }\n'
        '    } else {'
    )

    old_touch = (
        '      if (appsListView) {\n'
        '        const int rowH = 48;\n'
        '        const int rowPitch = 56;\n'
        '        for (int i = 0; i < 7; i++) {\n'
        '          int cardY = 66 + i * rowPitch - appsScroll;\n'
        '          if (x >= 6 && x <= 234 && y >= cardY && y <= cardY + rowH) {\n'
        '            hitApp = i;\n'
        '            break;\n'
        '          }\n'
        '        }\n'
        '      } else {'
    )

    new_touch = (
        '      if (appsListView) {\n'
        '        const int rowH = 56;\n'
        '        const int rowPitch = 64;\n'
        '        for (int i = 0; i < 7; i++) {\n'
        '          int cardY = 66 + i * rowPitch - appsScroll;\n'
        '          if (x >= 10 && x <= 235 && y >= cardY && y <= cardY + rowH) {\n'
        '            hitApp = i;\n'
        '            break;\n'
        '          }\n'
        '        }\n'
        '      } else {'
    )

    old_scroll = '        int maxS = appsListView ? 180 : MAX_APPS_SCROLL;'
    new_scroll = '        int maxS = appsListView ? 220 : MAX_APPS_SCROLL;'

    assert old_block in content_lf, f'old_block not found in {path}'
    content_lf = content_lf.replace(old_block, new_block)

    assert old_touch in content_lf, f'old_touch not found in {path}'
    content_lf = content_lf.replace(old_touch, new_touch)

    assert old_scroll in content_lf, f'old_scroll not found in {path}'
    content_lf = content_lf.replace(old_scroll, new_scroll)

    content_crlf = content_lf.replace('\n', '\r\n')
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(content_crlf)
    print(f'Successfully updated {path}')

if __name__ == '__main__':
    update_file('firmware/src/quick_settings.h')
    update_file('schematikIO-Export/src/quick_settings.h')
