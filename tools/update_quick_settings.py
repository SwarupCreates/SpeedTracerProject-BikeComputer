import os

def update_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Update lifecycle state if not already updated
    if 'state="locked:"+pinInput' in content:
        content = content.replace(
            'if(screenLocked) state="locked:"+pinInput+":"+String(pinError)+":"+String(millis()/500);',
            'if(screenLocked) state="locked:"+String((int)lockScreenMode)+":"+pinInput+":"+String(pinError)+":"+String(millis()/500);'
        )

    # 2. Update quickTap tiles
    old_quicktap_tiles = """  int col = x>=123 && x<234 ? 1 : (x>=6 && x<117 ? 0 : -1);
  int row = y>=162 && y<224 ? 0 : (y>=228 && y<290 ? 1 :
            (y>=294 && y<356 ? 2 : -1));
  if(col<0 || row<0)return;
  int tile=row*2+col;
  if (tile == 3) {
    setDarkTheme(!darkTheme);
  } else if (tile == 2) {
    openGpsInspector();
  } else {
    quickDetail = tile + 1;
  }"""

    new_quicktap_tiles = """  int col = x>=123 && x<234 ? 1 : (x>=6 && x<117 ? 0 : -1);
  int row = y>=162 && y<224 ? 0 : (y>=228 && y<290 ? 1 :
            (y>=294 && y<356 ? 2 : (y>=360 && y<422 ? 3 : -1)));
  if(col<0 || row<0)return;
  int tile=row*2+col;
  if (tile == 3) {
    setDarkTheme(!darkTheme);
  } else if (tile == 2) {
    openGpsInspector();
  } else if (tile == 6) {
    quickOpen = false;
    quickDetail = 0;
    screenLocked = true;
    lockScreenMode = LOCK_HERO;
    uiRevision++;
    return;
  } else if (tile == 7) {
    quickOpen = false;
    quickDetail = 0;
    shutdownDialog = true;
    shutdownPending = false;
    uiRevision++;
    return;
  } else {
    quickDetail = tile + 1;
  }"""
    assert old_quicktap_tiles in content, f"old_quicktap_tiles not found in {path}"
    content = content.replace(old_quicktap_tiles, new_quicktap_tiles)

    # 3. Update release in pollQuickTouch
    old_release = """  auto release=[&](){
    if(rootGesture && !sliderGesture && !moved)quickTap(endX,endY);
    rootGesture=false;sliderGesture=false;mapPanGesture=false;held=false;candidateX=-1;
  };"""

    new_release = """  auto release=[&](){
    if (screenLocked && lockScreenMode == LOCK_HERO) {
      if (moved && (startY - endY >= 20)) {
        if (screenLockEnabled) {
          lockScreenMode = LOCK_KEYPAD;
          pinInput = "";
          pinError = false;
        } else {
          screenLocked = false;
        }
        uiRevision++;
      } else if (!moved) {
        quickTap(endX, endY);
      }
      rootGesture=false; sliderGesture=false; mapPanGesture=false; held=false; candidateX=-1;
      return;
    }
    if(rootGesture && !sliderGesture && !moved)quickTap(endX,endY);
    rootGesture=false;sliderGesture=false;mapPanGesture=false;held=false;candidateX=-1;
  };"""
    assert old_release in content, f"old_release not found in {path}"
    content = content.replace(old_release, new_release)

    # 4. Timeout check after poll = millis();
    old_poll = """  if (!touchReady || millis() - poll < 25) return;
  poll = millis();"""

    new_poll = """  if (!touchReady || millis() - poll < 25) return;
  poll = millis();

  // Auto Screen Timeout (inhibited when an activity is actively ongoing)
  if (screenTimeoutSec > 0 && !rideStarted && !screenBlanked) {
    if (lastTouchActivityMs == 0) lastTouchActivityMs = millis();
    if (millis() - lastTouchActivityMs >= (uint32_t)screenTimeoutSec * 1000UL) {
      screenBlanked = true;
      analogWrite(5, 0); // Turn off LCD backlight
      screenLocked = true;
      lockScreenMode = LOCK_HERO;
      uiRevision++;
    }
  }"""
    assert old_poll in content, f"old_poll not found in {path}"
    content = content.replace(old_poll, new_poll)

    # 5. Touch activity and wake check after coordinate transform
    old_touch_contact = """  // The panel is now 180 degrees from the previously calibrated orientation.
  x=DISP_W-1-x;
  y=DISP_H-1-y;
  lastContact = millis();"""

    new_touch_contact = """  // The panel is now 180 degrees from the previously calibrated orientation.
  x=DISP_W-1-x;
  y=DISP_H-1-y;
  lastContact = millis();
  lastTouchActivityMs = millis();
  if (screenBlanked) {
    screenBlanked = false;
    analogWrite(5, (brightnessPercent * 255 + 50) / 100);
    screenLocked = true;
    lockScreenMode = LOCK_HERO;
    uiRevision++;
    return;
  }"""
    assert old_touch_contact in content, f"old_touch_contact not found in {path}"
    content = content.replace(old_touch_contact, new_touch_contact)

    # 6. Scroll range 72 -> 140
    old_scroll = """      } else {
        int offset=constrain(startScroll+startY-y,0,72);
        if(offset!=quickScroll){quickScroll=offset;uiRevision++;}
      }"""

    new_scroll = """      } else {
        int offset=constrain(startScroll+startY-y,0,140);
        if(offset!=quickScroll){quickScroll=offset;uiRevision++;}
      }"""
    assert old_scroll in content, f"old_scroll not found in {path}"
    content = content.replace(old_scroll, new_scroll)

    # 7. Touch begin on LOCK_HERO
    old_touch_begin = """    if(quickOpen && !quickDetail && y>=29 && y<290){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;startScroll=quickScroll;"""

    new_touch_begin = """    if (screenLocked && lockScreenMode == LOCK_HERO) {
      rootGesture = true;
      moved = false;
      startX = endX = x;
      startY = endY = y;
      sliderGesture = false;
    } else if(quickOpen && !quickDetail && y>=29 && y<290){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;startScroll=quickScroll;"""
    assert old_touch_begin in content, f"old_touch_begin not found in {path}"
    content = content.replace(old_touch_begin, new_touch_begin)

    # 8. Tile 0 Wi-Fi icon -> CustomIcon::hotSpotIcon
    old_tile0 = """  // 2x3 Grid of Tiles
  // Tile 0: Wi-Fi
  {
    int x = 6, y = 162-offset;
    card(x, y, 111, 62, uiCard());
    drawWifiStateIcon(display, x + 48, y + 12, uiFg(), WiFi.status()==WL_CONNECTED);"""

    new_tile0 = """  // 2x4 Grid of Tiles
  // Tile 0: Wi-Fi
  {
    int x = 6, y = 162-offset;
    card(x, y, 111, 62, uiCard());
    drawCustomIcon(display, x + 48, y + 12, uiFg(), CustomIcon::hotSpotIcon);"""
    assert old_tile0 in content, f"old_tile0 not found in {path}"
    content = content.replace(old_tile0, new_tile0)

    # 9. Add Tile 6 & Tile 7 after Tile 5
    old_tile5 = """  // Tile 5: Configuration
  {
    int x = 123, y = 294-offset;
    card(x, y, 111, 62, uiCard());
    drawWrenchIcon(display, x + 48, y + 12, uiFg());
    centeredLabel(x, y + 42, 111, "CONFIGURE", 1, uiFg());
  }"""

    new_tile5_6_7 = """  // Tile 5: Configuration
  {
    int x = 123, y = 294-offset;
    card(x, y, 111, 62, uiCard());
    drawWrenchIcon(display, x + 48, y + 12, uiFg());
    centeredLabel(x, y + 42, 111, "CONFIGURE", 1, uiFg());
  }

  // Tile 6: Lock Screen
  {
    int x = 6, y = 360-offset;
    card(x, y, 111, 62, uiCard());
    drawCustomIcon(display, x + 48, y + 12, uiFg(), CustomIcon::lockIcon);
    centeredLabel(x, y + 42, 111, "LOCK SCREEN", 1, uiFg());
  }

  // Tile 7: Power Off
  {
    int x = 123, y = 360-offset;
    card(x, y, 111, 62, uiCard());
    drawCustomIcon(display, x + 48, y + 12, 0xF800, CustomIcon::powerIcon);
    centeredLabel(x, y + 42, 111, "POWER OFF", 1, 0xF800);
  }"""
    assert old_tile5 in content, f"old_tile5 not found in {path}"
    content = content.replace(old_tile5, new_tile5_6_7)

    # 10. Update scroll indicator
    old_indicator = "display.fillRoundRect(236,32+quickScroll*174/72,3,80,1,uiMuted());"
    new_indicator = "display.fillRoundRect(236,32+quickScroll*174/140,3,40,1,uiMuted());"
    assert old_indicator in content, f"old_indicator not found in {path}"
    content = content.replace(old_indicator, new_indicator)

    with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(content)
    print(f"Successfully updated {path}")

update_file('firmware/src/quick_settings.h')
update_file('schematikIO-Export/src/quick_settings.h')
