def update_nav(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    old_nav = "static void drawGlobalNav(){\n  display.fillRect(0,290,240,30,uiBg());"
    new_nav = "static void drawGlobalNav(){\n  if(screenLocked) return;\n  display.fillRect(0,290,240,30,uiBg());"
    if old_nav not in content:
        old_nav = "static void drawGlobalNav(){\r\n  display.fillRect(0,290,240,30,uiBg());"
        new_nav = "static void drawGlobalNav(){\r\n  if(screenLocked) return;\r\n  display.fillRect(0,290,240,30,uiBg());"
    assert old_nav in content, f"old_nav not found in {path}"
    content = content.replace(old_nav, new_nav, 1)
    with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(content)
    print(f"Updated drawGlobalNav in {path}")

def update_btn(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    old_btn = "  updateBtn(rmtUp);  updateBtn(rmtDn);  updateBtn(rmtLt);  updateBtn(rmtRt);"
    new_btn = """  updateBtn(rmtUp);  updateBtn(rmtDn);  updateBtn(rmtLt);  updateBtn(rmtRt);

  if (rmtLt.pressed || rmtRt.pressed || rmtUp.pressed || rmtDn.pressed) {
    lastTouchActivityMs = millis();
    if (screenBlanked) {
      screenBlanked = false;
      analogWrite(5, (brightnessPercent * 255 + 50) / 100);
      screenLocked = true;
      lockScreenMode = LOCK_HERO;
      uiRevision++;
      rmtLt.pressed = rmtRt.pressed = rmtUp.pressed = rmtDn.pressed = false;
      return;
    }
  }"""
    assert old_btn in content, f"old_btn not found in {path}"
    content = content.replace(old_btn, new_btn, 1)
    with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(content)
    print(f"Updated button wake in {path}")

update_nav('firmware/src/quick_settings.h')
update_nav('schematikIO-Export/src/quick_settings.h')
update_btn('firmware/src/firmware.cpp')
update_btn('schematikIO-Export/src/firmware.cpp')
