import os
import shutil
import filecmp

def update_files_app():
    with open('firmware/src/quick_settings.h', 'r', encoding='utf-8') as f:
        qs = f.read()

    # Search for the drawFilesApp block
    start_marker = "    statusBar();\n    bool isRoot = (browserPath == \"/\");\n    uint16_t folderCardBg = darkTheme ? 0x3221 : 0xFEE7;\n    uint16_t folderAmber = darkTheme ? 0xFDE0 : 0xB480;"
    
    assert start_marker in qs, "start_marker not found in quick_settings.h"

    old_block = """    statusBar();
    bool isRoot = (browserPath == "/");
    uint16_t folderCardBg = darkTheme ? 0x3221 : 0xFEE7;
    uint16_t folderAmber = darkTheme ? 0xFDE0 : 0xB480;

    if(!sdOk){
      display.fillRoundRect(8, 38, 224, 54, 27, uiCard());
      centeredLabel(8, 56, 224, "SD card unavailable", 1, 0xFA08);
    }
    else if(browserError.length()){
      display.fillRoundRect(8, 38, 224, 54, 27, uiCard());
      centeredLabel(8, 56, 224, browserError, 1, 0xFA08);
    }
    else if(isRoot){
      // Root Directory: Frame 33 style - up to 4 folder pills (224x54, R=27)
      if(!browserCount){
        display.fillRoundRect(8, 38, 224, 54, 27, uiCard());
        centeredLabel(8, 56, 224, "No folders found", 1, uiMuted());
      }
      for(int i = 0; i < browserCount; i++){
        int y = 38 + i * 64;
        display.fillRoundRect(8, y, 224, 54, 27, folderCardBg);
        // Large Folder Icon (amber)
        drawCustomIcon(display, 22, y + 13, folderAmber, CustomIcon::folderIconLarge, 30, 28);
        // Folder Name (amber, font size 2)
        label(62, y + 19, browserNames[i], 2, folderAmber);
        // Dynamic file count or size on right (amber, font size 3)
        if(browserDirs[i]){
          String dirPath = String("/") + browserNames[i];
          int c = countFilesInDir(dirPath);
          String cStr = String(c);
          int cw = textWidth(cStr, 3);
          label(216 - cw, y + 15, cStr, 3, folderAmber);
        } else {
          String sz = storageSize(browserSizes[i]);
          int sw = textWidth(sz, 2);
          label(216 - sw, y + 19, sz, 2, folderAmber);
        }
      }
    } else {
      // Subdirectory: Frame 34 style
      // Row 1: Folder Header Pill (tapping navigates back)
      int slash = browserPath.lastIndexOf('/');
      String curFolder = (slash >= 0) ? browserPath.substring(slash + 1) : browserPath;
      if(curFolder.isEmpty()) curFolder = "files";
      display.fillRoundRect(8, 38, 224, 54, 27, folderCardBg);
      drawCustomIcon(display, 22, 38 + 13, folderAmber, CustomIcon::folderIconLarge, 30, 28);
      label(62, 38 + 19, curFolder, 2, folderAmber);

      // Rows 2, 3, 4: File Pills (224x54, R=27, dark cards)
      if(!browserCount){
        display.fillRoundRect(8, 102, 224, 54, 27, uiCard());
        centeredLabel(8, 120, 224, "This folder is empty", 1, uiMuted());
      }
      for(int i = 0; i < browserCount; i++){
        int y = 102 + i * 64;
        display.fillRoundRect(8, y, 224, 54, 27, uiCard());

        String low = browserNames[i];
        low.toLowerCase();
        CustomIcon icon;
        if(browserDirs[i]){
          icon = CustomIcon::folderIconLarge;
        } else if(low.endsWith(".fit")){
          if(low.indexOf("run") >= 0) icon = CustomIcon::runIconLarge;
          else icon = CustomIcon::bikeIconLarge;
        } else if(low.endsWith(".gpx")){
          icon = CustomIcon::routeMapIconLarge;
        } else if(low.endsWith(".bin")){
          icon = CustomIcon::mapAppIconLarge;
        } else {
          icon = CustomIcon::sdCardIcon;
        }

        // Left Icon in clean white/uiFg
        drawCustomIcon(display, 22, y + 13, uiFg(), icon, 28, 28);

        // Center: Formatted File Display Name
        String dispName = browserNames[i];
        dispName.replace(".fit", "");
        dispName.replace(".gpx", "");
        dispName.replace(".bin", "");
        dispName.replace("_", " ");
        if(dispName.length() > 14) dispName = truncateHeading(dispName, 14);
        label(62, y + 19, dispName, 2, uiFg());

        // Right: File Size in uiMuted() (e.g. "50KB")
        String szStr = browserDirs[i] ? "DIR" : storageSize(browserSizes[i]);
        int sw = textWidth(szStr, 1);
        label(220 - sw, y + 21, szStr, 1, uiMuted());
      }
    }"""

    new_block = """    statusBar();
    bool isRoot = (browserPath == "/");
    // Themed identically to the compass button on the home page (0x2900 in dark, 0xEEF7 in light)
    uint16_t folderCardBg = darkTheme ? 0x2900 : 0xEEF7;
    uint16_t folderAmber  = darkTheme ? 0xFE64 : 0xAC63;

    if(!sdOk){
      display.fillRoundRect(8, 38, 224, 54, 27, uiCard());
      centeredLabel(8, 56, 224, "SD card unavailable", 1, 0xFA08);
    }
    else if(browserError.length()){
      display.fillRoundRect(8, 38, 224, 54, 27, uiCard());
      centeredLabel(8, 56, 224, browserError, 1, 0xFA08);
    }
    else if(isRoot){
      // Root Directory: Frame 33 style - up to 4 folder pills (224x54, R=27)
      if(!browserCount){
        display.fillRoundRect(8, 38, 224, 54, 27, folderCardBg);
        centeredLabel(8, 56, 224, "No folders found", 1, folderAmber);
      }
      for(int i = 0; i < browserCount; i++){
        int y = 38 + i * 64;
        display.fillRoundRect(8, y, 224, 54, 27, folderCardBg);
        // Large Folder Icon (amber, 30x30 square centered at y+27)
        drawCustomIcon(display, 22, y + 12, folderAmber, CustomIcon::folderIconLarge, 30, 30);
        // Folder Name (amber, font size 2) - truncated post 16 characters
        String folderName = browserNames[i];
        if(folderName.length() > 16) folderName = truncateHeading(folderName, 16);
        label(62, y + 20, folderName, 2, folderAmber);
        // Dynamic file count or size on right (amber, font size 3 centered at y+27)
        if(browserDirs[i]){
          String dirPath = String("/") + browserNames[i];
          int c = countFilesInDir(dirPath);
          String cStr = String(c);
          int cw = textWidth(cStr, 3);
          label(216 - cw, y + 17, cStr, 3, folderAmber);
        } else {
          String sz = storageSize(browserSizes[i]);
          int sw = textWidth(sz, 2);
          label(216 - sw, y + 20, sz, 2, folderAmber);
        }
      }
    } else {
      // Subdirectory: Frame 34 style
      // Row 1: Folder Header Pill (tapping navigates back)
      int slash = browserPath.lastIndexOf('/');
      String curFolder = (slash >= 0) ? browserPath.substring(slash + 1) : browserPath;
      if(curFolder.isEmpty()) curFolder = "files";
      if(curFolder.length() > 16) curFolder = truncateHeading(curFolder, 16);
      display.fillRoundRect(8, 38, 224, 54, 27, folderCardBg);
      drawCustomIcon(display, 22, 38 + 12, folderAmber, CustomIcon::folderIconLarge, 30, 30);
      label(62, 38 + 20, curFolder, 2, folderAmber);

      // Rows 2, 3, 4: File / Subfolder Pills (224x54, R=27)
      if(!browserCount){
        display.fillRoundRect(8, 102, 224, 54, 27, uiCard());
        centeredLabel(8, 120, 224, "This folder is empty", 1, uiMuted());
      }
      for(int i = 0; i < browserCount; i++){
        int y = 102 + i * 64;
        bool isDir = browserDirs[i];
        display.fillRoundRect(8, y, 224, 54, 27, isDir ? folderCardBg : uiCard());

        String low = browserNames[i];
        low.toLowerCase();
        CustomIcon icon;
        uint16_t iconCol = isDir ? folderAmber : uiFg();
        uint16_t textCol = isDir ? folderAmber : uiFg();
        if(isDir){
          icon = CustomIcon::folderIconLarge;
        } else if(low.endsWith(".fit")){
          if(low.indexOf("run") >= 0) icon = CustomIcon::runIconLarge;
          else icon = CustomIcon::bikeIconLarge;
        } else if(low.endsWith(".gpx")){
          icon = CustomIcon::routeMapIconLarge;
        } else if(low.endsWith(".bin")){
          icon = CustomIcon::mapAppIconLarge;
        } else {
          icon = CustomIcon::sdCardIcon;
        }

        // Left Icon (folderAmber for folder, uiFg for files, 30x30 square centered at y+27)
        drawCustomIcon(display, 22, y + 12, iconCol, icon, 30, 30);

        // Center: Formatted File / Folder Display Name (truncated post 16 chars for folders, 14 chars for files)
        String dispName = browserNames[i];
        if(isDir){
          if(dispName.length() > 16) dispName = truncateHeading(dispName, 16);
        } else {
          dispName.replace(".fit", "");
          dispName.replace(".gpx", "");
          dispName.replace(".bin", "");
          dispName.replace("_", " ");
          if(dispName.length() > 14) dispName = truncateHeading(dispName, 14);
        }
        label(62, y + 20, dispName, 2, textCol);

        // Right: File Count (if folder) or File Size in uiMuted() (e.g. "50KB")
        if(isDir){
          String dirPath = (browserPath == "/" ? String("") : browserPath) + "/" + browserNames[i];
          int c = countFilesInDir(dirPath);
          String cStr = String(c);
          int cw = textWidth(cStr, 3);
          label(216 - cw, y + 17, cStr, 3, folderAmber);
        } else {
          String szStr = storageSize(browserSizes[i]);
          int sw = textWidth(szStr, 1);
          label(220 - sw, y + 23, szStr, 1, uiMuted());
        }
      }
    }"""

    assert old_block in qs, "old_block not found in quick_settings.h"
    qs = qs.replace(old_block, new_block, 1)

    with open('firmware/src/quick_settings.h', 'w', encoding='utf-8') as f:
        f.write(qs)
    print("Updated firmware/src/quick_settings.h successfully!")

    # Mirror to schematikIO-Export
    shutil.copy2('firmware/src/quick_settings.h', 'schematikIO-Export/src/quick_settings.h')
    print("Mirrored to schematikIO-Export/src/quick_settings.h, identical:", filecmp.cmp('firmware/src/quick_settings.h', 'schematikIO-Export/src/quick_settings.h'))

if __name__ == '__main__':
    update_files_app()
