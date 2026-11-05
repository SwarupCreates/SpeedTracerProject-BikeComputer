#pragma once
#include "smooth_digits48.h"
// CST3530 protocol follows Waveshare's esp_lcd_touch_cst3530 driver.
// https://github.com/waveshareteam/Waveshare-ESP32-components
TwoWire touchBus(1);
bool touchReady = false, quickOpen = false, ridePaused = false;
uint32_t pauseStart = 0;
int quickDetail = 0;
int quickScroll=0;
int brightnessPercent=100;
static void setBrightnessFromTouch(int x){
  brightnessPercent=constrain(5+(x-52)*95/166,5,100);
  // GPIO5 is the board's active-high backlight; PWM keeps power controls untouched.
  analogWrite(5,(brightnessPercent*255+50)/100);
  uiRevision++;
}
bool welcomeOpen=true, shutdownDialog=false, shutdownPending=false;
uint32_t countdownStart=0;
bool gpsWaiting=false;
bool gpsRadarInspector=false;
int gpsScroll=0;
// Home subpages stay independent of dashboard navigation and touch calibration.
enum HomePage { HOME_MAIN, HOME_APPS, HOME_FILES, HOME_ACTIVITY, HOME_RIDES, HOME_ROUTES };
HomePage homePage=HOME_MAIN;
String browserPath="/", browserError, selectedFile;
String browserNames[4];
bool browserDirs[4];
uint64_t browserSizes[4];
int browserPage=0, browserCount=0;
bool browserMore=false;
uint64_t browserTotal=0,browserUsed=0;

// File Detail page state in MY FILES
bool fileDetailOpen = false;
String fileDetailName = "";
String fileDetailPath = "";
uint64_t fileDetailSize = 0;
String fileDetailDate = "";
bool fileDetailIsFit = false;
bool fileDetailIsGpx = false;
bool fileDeleteConfirm = false;
static String storageSize(uint64_t n){
  if(n>=1073741824ULL)return String((double)n/1073741824.0,2)+" GiB";
  if(n>=1048576)return String((double)n/1048576.0,1)+" MiB";
  if(n>=1024)return String((double)n/1024.0,1)+" KiB";
  return String((unsigned long)n)+" bytes";
}
static String fitBrowserText(String text,int width){
  while(text.length() && textWidth(text,1)>width)text.remove(text.length()-1);
  return text;
}
static String truncateHeading(String text, int maxChars = 16) {
  if (text.length() > maxChars) {
    if (maxChars > 3) {
      String sub = text.substring(0, maxChars - 3);
      sub.trim();
      return sub + "...";
    }
    return text.substring(0, maxChars);
  }
  return text;
}
static void loadBrowser(){
  browserCount=0;browserMore=false;browserError="";selectedFile="";
  browserTotal=browserUsed=0;
  if(!sdOk){browserError="SD card unavailable";return;}
  browserTotal=SD_MMC.totalBytes();browserUsed=SD_MMC.usedBytes();
  if(!browserTotal){browserError="Card filesystem unreadable";return;}
  File dir=SD_MMC.open(browserPath,FILE_READ);
  if(!dir || !dir.isDirectory()){browserError="Folder unavailable";if(dir)dir.close();return;}
  int skip=browserPage*4,index=0;
  while(true){
    File entry=dir.openNextFile();if(!entry)break;
    if(index++<skip){entry.close();continue;}
    if(browserCount==4){browserMore=true;entry.close();break;}
    String name=entry.name();int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);
    browserNames[browserCount]=name;browserDirs[browserCount]=entry.isDirectory();
    browserSizes[browserCount]=entry.size();browserCount++;entry.close();
  }
  dir.close();
}
static void openBrowser(String path){browserPath=path;browserPage=0;homePage=HOME_FILES;fileDetailOpen=false;fileDeleteConfirm=false;loadBrowser();uiRevision++;}

static void openFileDetail(int row){
  if(row<0 || row>=browserCount) return;
  fileDetailOpen = true;
  fileDeleteConfirm = false;
  fileDetailName = browserNames[row];
  fileDetailPath = (browserPath == "/" ? String("") : browserPath) + "/" + browserNames[row];
  fileDetailSize = browserSizes[row];

  String low = fileDetailName;
  low.toLowerCase();
  fileDetailIsFit = low.endsWith(".fit");
  fileDetailIsGpx = low.endsWith(".gpx");

  fileDetailDate = "";
  File f = SD_MMC.open(fileDetailPath, FILE_READ);
  if(f){
    time_t lw = f.getLastWrite();
    if(lw > 100000){
      struct tm *tmInfo = localtime(&lw);
      if(tmInfo && tmInfo->tm_year > 80){
        char buf[32];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
                 tmInfo->tm_year + 1900, tmInfo->tm_mon + 1, tmInfo->tm_mday,
                 tmInfo->tm_hour, tmInfo->tm_min);
        fileDetailDate = String(buf);
      }
    }
    if(fileDetailDate.isEmpty() && fileDetailIsGpx){
      char buf[513];
      int n = f.read((uint8_t*)buf, 512);
      if(n > 0){
        buf[n] = 0;
        char *t1 = strstr(buf, "<time>");
        if(t1){
          char *t2 = strstr(t1, "</time>");
          if(t2){
            *t2 = 0;
            String tStr = String(t1 + 6);
            tStr.replace("T", " ");
            tStr.replace("Z", "");
            if(tStr.length() > 16) tStr = tStr.substring(0, 16);
            fileDetailDate = tStr;
          }
        }
      }
    }
    f.close();
  }
  if(fileDetailDate.isEmpty()){
    fileDetailDate = "Unknown / SD Default";
  }
  uiRevision++;
}

static void drawFileDetail(){
  drawCustomIcon(display,10,35,uiFg(),CustomIcon::GoBackIcon);
  label(34,37,"FILE DETAILS",2);
  display.drawFastHLine(6,58,228,uiCard());

  // Icon & Type Header
  card(6,64,48,46,uiCard());
  if(fileDetailIsFit){
    drawCustomIcon(display,22,79,TEAL,CustomIcon::runIcon);
  } else if(fileDetailIsGpx){
    drawCustomIcon(display,22,79,0x3DF7,CustomIcon::routeMapIcon);
  } else {
    drawCustomIcon(display,22,79,uiFg(),CustomIcon::sdCardIcon);
  }

  label(60,67,fileDetailIsFit?"FIT ACTIVITY":fileDetailIsGpx?"GPX ROUTE":"FILE",2,uiFg());
  label(60,89,fileDetailIsFit?"Recorded Activity Data":fileDetailIsGpx?"GPS Route / Trackpoints":"SD Storage File",1,uiMuted());

  // Full file name card (multi-line wrapped)
  card(6,116,228,52,uiCard());
  label(12,120,"FULL FILE NAME",1,TEAL);
  if(fileDetailName.length()<=34){
    label(12,136,fileDetailName,1,uiFg());
  } else {
    int splitPos = 30;
    for(int i=30; i>=16; i--){
      if(fileDetailName[i]=='_' || fileDetailName[i]==' ' || fileDetailName[i]=='-'){
        splitPos = i+1;
        break;
      }
    }
    String line1 = fileDetailName.substring(0, splitPos);
    String line2 = fileDetailName.substring(splitPos);
    label(12,135,line1,1,uiFg());
    label(12,149,fitBrowserText(line2,216),1,uiFg());
  }

  // Metadata Card: Date and Size
  card(6,174,228,52,uiCard());
  label(12,180,"DATE:",1,uiMuted());
  label(58,180,fileDetailDate,1,uiFg());

  label(12,196,"SIZE:",1,uiMuted());
  label(58,196,storageSize(fileDetailSize)+" ("+String((unsigned long)fileDetailSize)+" B)",1,uiFg());

  label(12,212,"PATH:",1,uiMuted());
  label(58,212,fitBrowserText(fileDetailPath,160),1,uiMuted());

  // Delete Action / Confirmation
  if(!fileDeleteConfirm){
    card(6,234,228,38,0xF800);
    centeredLabel(6,245,228,"DELETE FILE",2,WHITE);
  } else {
    card(6,234,110,38,0x4208);
    centeredLabel(6,245,110,"CANCEL",1,WHITE);

    card(124,234,110,38,0xF800);
    centeredLabel(124,245,110,"CONFIRM",1,WHITE);

    centeredLabel(6,276,228,"Delete permanently from SD?",1,0xFA08);
  }
}

static void requestRideStart();
static void drawGlobalNav();
#include "route_archive.h"
int archiveScroll=0;
const int ARCHIVE_MAX_SCROLL=190;
// Dedicated activity/route library, independent of the general file browser.
String libraryNames[4],libraryPaths[4],librarySelection,libraryMessage;
uint64_t librarySizes[4];
int libraryCount=0,libraryPage=0;
bool libraryMore=false;
uint64_t librarySelectedSize=0;
static bool isLibrary(){return homePage==HOME_ACTIVITY || homePage==HOME_RIDES || homePage==HOME_ROUTES;}
static void loadLibrary(){
  libraryCount=0;libraryMore=false;libraryMessage="";librarySelection="";
  if(!sdOk){libraryMessage="SD card unavailable";return;}
  int found=0;
  auto scan=[&](const char* path){
    File dir=SD_MMC.open(path,FILE_READ);
    if(!dir || !dir.isDirectory()){if(dir)dir.close();return;}
    while(true){
      File f=dir.openNextFile();if(!f)break;
      String n=f.name();int slash=n.lastIndexOf('/');if(slash>=0)n=n.substring(slash+1);
      String low=n;low.toLowerCase();
      bool routes=homePage==HOME_ROUTES;
      bool accept=!f.isDirectory() && (routes ? low.endsWith(".gpx") : low.endsWith(".fit"));
      String full=String(path)+(String(path)=="/"?"":"/")+n;
      if(activityFile && full==activityPath)accept=false; // Never offer a recording still being written.
      if(accept && found++>=libraryPage*4){
        if(libraryCount==4){libraryMore=true;f.close();break;}
        libraryNames[libraryCount]=n;libraryPaths[libraryCount]=full;
        librarySizes[libraryCount++]=f.size();
      }
      f.close();
    }
    dir.close();
  };
  if(homePage==HOME_ROUTES){scan("/routes");if(!libraryMore)scan("/activities");if(!libraryMore)scan("/");}
  else {scan("/activities");if(!libraryMore)scan("/");}
}
static void openLibrary(HomePage page){homePage=page;libraryPage=0;loadLibrary();uiRevision++;}
// Static refresh control only for stored-data apps.
static bool refreshPending=false;
static bool showAppRefresh(){
  return welcomeOpen && !quickOpen && !shutdownDialog && !gpsWaiting && !gpsRadarInspector && !countdownStart
    && ((homePage==HOME_FILES && !fileDetailOpen) || isLibrary());
}
static void drawAppRefresh(){
  if(!showAppRefresh())return;
  display.fillCircle(222,43,11,0x2A5F);
  drawCustomIcon(display,214,35,0xE71C,CustomIcon::refreshIcon);
}
static void serviceAppRefresh(){
  if(!refreshPending)return;
  refreshPending=false;
  if(!showAppRefresh())return;
  if(homePage==HOME_FILES)loadBrowser();
  else if(isLibrary())loadLibrary();
  uiRevision++;
}
static void drawLibrary(){
  bool routes=homePage==HOME_ROUTES;

  if(librarySelection.length()){
    if(routes){
      int slash = librarySelection.lastIndexOf('/');
      String title = (slash >= 0) ? librarySelection.substring(slash + 1) : librarySelection;
      title.replace(".gpx", ""); title.replace(".fit", ""); title.replace(".csv", "");
      title.replace("_", " ");
      String headTitle = truncateHeading(title, 16);
      int topY = 34 - archiveScroll;

      // Header
      if (topY + 18 >= 28 && topY < 290) label(10, topY, headTitle, 2, uiFg());
      if (topY + 36 >= 28 && topY < 290) label(10, topY + 19, "Track guidance  *  No street names", 1, uiMuted());

      // 1. Prominent Large Route Map Preview Card (180px tall!)
      int mapY = topY + 34;
      previewDraw(mapY, 180);

      // 2. Metrics 2x2 Grid (below map)
      int row1Y = mapY + 188;
      if (row1Y + 36 >= 28 && row1Y < 290) {
        card(6, row1Y, 111, 36, uiCard());
        label(14, row1Y + 5, "DISTANCE", 1, uiMuted());
        label(14, row1Y + 20, String(previewKm, 2) + " km", 1, uiFg());

        card(123, row1Y, 111, 36, uiCard());
        label(131, row1Y + 5, "EST. TIME", 1, uiMuted());
        int tMin = (int)round((previewKm / 20.0f) * 60.0f);
        String tStr = (tMin >= 60 ? (String(tMin / 60) + "h " + String(tMin % 60) + "m") : (String(tMin) + " min")) + " @ 20k";
        label(131, row1Y + 20, tStr, 1, uiFg());
      }

      int row2Y = row1Y + 42;
      if (row2Y + 36 >= 28 && row2Y < 290) {
        card(6, row2Y, 111, 36, uiCard());
        label(14, row2Y + 5, "ELEV GAIN", 1, uiMuted());
        label(14, row2Y + 20, previewElevation ? (String(previewGain, 0) + " m up") : "--", 1, uiFg());

        card(123, row2Y, 111, 36, uiCard());
        label(131, row2Y + 5, "AVG GRADIENT", 1, uiMuted());
        float grad = (previewKm > 0.01f && previewElevation) ? ((previewGain / (previewKm * 1000.0f)) * 100.0f) : 0.0f;
        label(131, row2Y + 20, (grad > 0.0f ? ("+" + String(grad, 1) + "%") : String("--")), 1, uiFg());
      }

      // 3. Confirm & Start Route Action Button
      int btnY = row2Y + 42;
      if (btnY + 34 >= 28 && btnY < 290) {
        card(6, btnY, 228, 34, TEAL);
        centeredLabel(6, btnY + 9, 228, "CONFIRM & START ROUTE", 1, WHITE);
      }
      if (libraryMessage.length() && btnY - 14 >= 28 && btnY - 14 < 290) {
        label(10, btnY - 14, fitBrowserText(libraryMessage, 218), 1, 0xF800);
      }

      // Scrollbar on the right edge
      const int routeMaxScroll = 120;
      if (routeMaxScroll > 0) {
        int thumbH = 40;
        int thumbY = 32 + (archiveScroll * (250 - thumbH) / routeMaxScroll);
        display.fillRoundRect(236, thumbY, 3, thumbH, 1, uiMuted());
      }

      statusBar(true, false);
      drawGlobalNav();
      return;
    }

    // Past Activity / Ride History: Full vertically scrollable activity report
    int topY = 34 - archiveScroll;

    // 1. Header (Title & Completed Date)
    int slash = librarySelection.lastIndexOf('/');
    String title = (slash >= 0) ? librarySelection.substring(slash + 1) : librarySelection;
    title.replace(".fit", ""); title.replace(".gpx", ""); title.replace(".csv", "");
    title.replace("_", " ");
    String headTitle = truncateHeading(title, 16);
    if (topY + 16 >= 28 && topY < 290) {
      label(10, topY, headTitle, 2, uiFg());
    }
    if (topY + 34 >= 28 && topY + 18 < 290) {
      label(10, topY + 19, completedText, 1, uiMuted());
    }

    // 2. Metrics 2x2 Grid (y = topY + 36)
    int mY = topY + 36;
    // Card 1: DISTANCE
    if (mY + 40 >= 28 && mY < 290) {
      card(6, mY, 111, 40, uiCard());
      label(14, mY + 4, "DISTANCE", 1, uiMuted());
      label(14, mY + 19, String(previewKm, 2) + " km", 1, uiFg());
    }
    // Card 2: TIME / DURATION
    if (mY + 40 >= 28 && mY < 290) {
      card(123, mY, 111, 40, uiCard());
      label(131, mY + 4, "DURATION", 1, uiMuted());
      uint32_t sec = archiveSummary["seconds"].is<uint32_t>() ? archiveSummary["seconds"].as<uint32_t>() : (archiveSummary["seconds"].is<float>() ? (uint32_t)archiveSummary["seconds"].as<float>() : 0);
      String timeStr;
      if (sec >= 3600) timeStr = String(sec / 3600) + "h " + String((sec % 3600) / 60) + "m";
      else if (sec > 0) timeStr = String(sec / 60) + "m " + String(sec % 60) + "s";
      else timeStr = "--";
      label(131, mY + 19, timeStr, 1, uiFg());
    }

    // Row 2: SPEED & HR (y = mY + 44)
    int mY2 = mY + 44;
    // Card 3: AVG SPEED
    if (mY2 + 40 >= 28 && mY2 < 290) {
      card(6, mY2, 111, 40, uiCard());
      label(14, mY2 + 4, "AVG SPEED", 1, uiMuted());
      uint32_t sec = archiveSummary["seconds"].is<uint32_t>() ? archiveSummary["seconds"].as<uint32_t>() : 0;
      float avgSpd = archiveSummary["avgSpeedKmh"].is<float>() ? archiveSummary["avgSpeedKmh"].as<float>() : ((sec > 0) ? (previewKm / (sec / 3600.0f)) : 0);
      String spdStr = (avgSpd > 0) ? (String(avgSpd, 1) + " km/h") : "--";
      label(14, mY2 + 19, spdStr, 1, uiFg());
    }
    // Card 4: AVG HR
    if (mY2 + 40 >= 28 && mY2 < 290) {
      card(123, mY2, 111, 40, uiCard());
      label(131, mY2 + 4, "AVG HR", 1, uiMuted());
      float avgHr = archiveSummary["avgHeartRate"].is<float>() ? archiveSummary["avgHeartRate"].as<float>() : 0;
      String hrStr = (avgHr > 0) ? (String((int)round(avgHr)) + " bpm") : "--";
      label(131, mY2 + 19, hrStr, 1, uiFg());
    }

    // Row 3: ELEVATION & KCAL (y = mY2 + 44)
    int mY3 = mY2 + 44;
    if (mY3 + 24 >= 28 && mY3 < 290) {
      card(6, mY3, 111, 24, uiCard());
      centeredLabel(6, mY3 + 6, 111, previewElevation ? (String(previewGain, 0) + " m up") : "Elev --", 1, uiMuted());
      card(123, mY3, 111, 24, uiCard());
      float cal = archiveSummary["caloriesEstimate"].is<float>() ? archiveSummary["caloriesEstimate"].as<float>() : 0;
      centeredLabel(123, mY3 + 6, 111, (cal > 0) ? (String((int)cal) + " kcal") : "Kcal --", 1, uiMuted());
    }

    // 3. Route Preview Map Card (y = mY3 + 28, h = 88)
    int mapY = mY3 + 28;
    if (mapY + 88 >= 28 && mapY < 290) {
      previewDraw(mapY, 88);
    }

    // 4. Heart Rate Graph Card (y = mapY + 92, h = 76)
    int hrY = mapY + 92;
    if (hrY + 76 >= 28 && hrY < 290) {
      String sub = archiveHrTop > 0 ? ("MAX " + String((int)archiveHrTop) + " BPM") : "BPM";
      archiveGraph(librarySelection, hrY, 1, "HEART RATE", sub.c_str());
    }

    // 5. Speed Graph Card (y = hrY + 80, h = 76)
    int spdY = hrY + 80;
    if (spdY + 76 >= 28 && spdY < 290) {
      String sub = archiveSpeedTop > 0 ? ("MAX " + String(archiveSpeedTop, 1) + " KM/H") : "KM/H";
      archiveGraph(librarySelection, spdY, 2, "SPEED", sub.c_str());
    }

    // 6. Footer (y = spdY + 80)
    int footY = spdY + 80;
    if (footY + 34 >= 28 && footY < 290) {
      centeredLabel(6, footY + 8, 228, "-- End of Activity --", 1, uiMuted());
    }

    // Right edge scrollbar thumb
    if (ARCHIVE_MAX_SCROLL > 0) {
      int thumbH = 44;
      int thumbY = 32 + (archiveScroll * (250 - thumbH) / ARCHIVE_MAX_SCROLL);
      display.fillRoundRect(237, thumbY, 2, thumbH, 1, uiMuted());
    }

    // Repaint fixed top status bar and bottom nav
    statusBar(true, false);
    drawGlobalNav();
    return;
  }

  drawCustomIcon(display,10,35,uiFg(),routes?CustomIcon::routeMapIcon:homePage==HOME_RIDES?CustomIcon::historyIcon:CustomIcon::runIcon);
  label(34,37,routes?"SAVED ROUTES":homePage==HOME_RIDES?"RIDE HISTORY":"PAST ACTIVITY",2);

  if(!libraryCount){
    card(6,76,228,150,uiCard());
    drawCustomIcon(display,108,105,uiMuted(),routes?CustomIcon::routeMapIcon:homePage==HOME_RIDES?CustomIcon::historyIcon:CustomIcon::runIcon);
    centeredLabel(6,140,228,libraryMessage.length()?libraryMessage:routes?"No saved routes":"No saved activities",1,uiMuted());
    centeredLabel(6,168,228,routes?"Add routes to SD /routes":"Finish and save an activity",1,uiMuted());
  }
  for(int i=0;i<libraryCount;i++){
    int y=68+i*44;card(6,y,228,40,uiCard());
    drawCustomIcon(display,14,y+12,TEAL,routes?CustomIcon::routeMapIcon:homePage==HOME_RIDES?CustomIcon::historyIcon:CustomIcon::runIcon);
    String title=libraryNames[i];title.replace(".fit","");title.replace(".json","");title.replace(".gpx","");
    label(38,y+5,fitBrowserText(title,184),1);
    label(38,y+23,routes?"Route - tap to view":libraryNames[i].startsWith("run-")?"Run - tap to view":"Ride - tap to view",1,uiMuted());
  }
}
static void libraryTap(int x,int y){
  if(librarySelection.length()){
    bool routes=homePage==HOME_ROUTES;
    if(routes){
      int topY = 34 - archiveScroll;
      int mapY = topY + 34;
      int row1Y = mapY + 188;
      int row2Y = row1Y + 42;
      int btnY = row2Y + 42;

      // 1. Zoom + Button Tap
      if (x >= 194 && x <= 234 && y >= mapY + 4 && y < mapY + 32) {
        if (previewZoomLevel < 4) {
          if (previewZoomLevel == 0 && previewLen > 0) {
            previewPanLat = preview[0].lat;
            previewPanLon = preview[0].lon;
            g_mapCache.updatePosition(previewPanLat, previewPanLon);
          }
          previewZoomLevel++;
          uiRevision++;
        }
        return;
      }

      // 2. Zoom - Button Tap
      if (x >= 194 && x <= 234 && y >= mapY + 32 && y < mapY + 60) {
        if (previewZoomLevel > 0) {
          previewZoomLevel--;
          uiRevision++;
        }
        return;
      }

      // 3. Reset to Ride Start Button Tap (when zoomed in)
      if (previewZoomLevel > 0 && x >= 194 && x <= 234 && y >= mapY + 60 && y < mapY + 92) {
        if (previewLen > 0) {
          previewPanLat = preview[0].lat;
          previewPanLon = preview[0].lon;
          g_mapCache.updatePosition(previewPanLat, previewPanLon);
          uiRevision++;
        }
        return;
      }

      if(y >= btnY && y < btnY + 34 && x >= 6 && x <= 234){
        if(rideStarted){libraryMessage="Save current activity first";uiRevision++;return;}
        if(previewLen<2){libraryMessage="GPX missing or invalid";uiRevision++;return;}
        routeLen=previewLen;for(int i=0;i<routeLen;i++)route[i]={float(preview[i].lat),float(preview[i].lon)};
        nav.routeIndex=0;nav.hasDestination=true;nav.destLat=route[routeLen-1].lat;nav.destLon=route[routeLen-1].lon;
        snapActiveRoute();
        currentScreen=SCR_MAP;requestRideStart();
      }
      return;
    }
    return;
  }
  int row=(y-68)/44;
  if(y>=68 && row>=0 && row<libraryCount && y<68+row*44+40){
    librarySelection=libraryPaths[row];
    librarySelectedSize=librarySizes[row];
    libraryMessage="";
    archiveScroll=0;
    previewZoomLevel=0;previewPanLat=0.0;previewPanLon=0.0;
    loadArchive(librarySelection);
    uiRevision++;
  }
}
// Shared tray reserves y=290..319 on every interactive page.
static bool globalHasNext(){
  if(gpsWaiting || gpsRadarInspector) return true;
  if(!welcomeOpen || quickOpen || shutdownDialog || countdownStart)return false;
  if(homePage==HOME_FILES && fileDetailOpen)return false;
  if(isLibrary()){
    if(homePage==HOME_ROUTES && librarySelection.length()) return false;
    return librarySelection.length()?true:libraryMore;
  }
  return homePage==HOME_FILES && browserMore;
}
static void globalNext(){
  if(!globalHasNext())return;
  if(gpsWaiting){
    gpsWaiting = false;
    countdownStart = millis();
    uiRevision++;
    return;
  }
  if(gpsRadarInspector){
    gpsScroll = (gpsScroll == 0) ? 115 : 0;
    uiRevision++;
    return;
  }
  if(isLibrary()){
    if(librarySelection.length()){
      archiveScroll = (archiveScroll < ARCHIVE_MAX_SCROLL / 2) ? ARCHIVE_MAX_SCROLL : 0;
    } else {
      libraryPage++;
      loadLibrary();
    }
  }
  else {browserPage++;loadBrowser();}
  uiRevision++;
}
static bool globalHasBack(){
  if(shutdownDialog || gpsWaiting || gpsRadarInspector || countdownStart || quickOpen) return true;
  if(!welcomeOpen) return true;
  if(homePage != HOME_MAIN) return true;
  return false;
}
static void drawGlobalNav(){
  display.fillRect(0,290,240,30,uiBg());
  if(globalHasBack())drawCustomIcon(display,42,297,uiFg(),CustomIcon::GoBackIcon);
  drawCustomIcon(display,112,297,uiFg(),CustomIcon::HomeIcon);
  if(globalHasNext())drawCustomIcon(display,182,297,uiFg(),CustomIcon::goNextIcon);
}
static void globalHome(){
  if(shutdownPending)return;
  fileDetailOpen=false;fileDeleteConfirm=false;
  shutdownDialog=false;gpsWaiting=false;gpsRadarInspector=false;countdownStart=0;
  quickOpen=false;quickDetail=0;welcomeOpen=true;homePage=HOME_MAIN;
  uiRevision++;
}
static void globalBack(){
  if(shutdownPending)return;
  if(shutdownDialog){shutdownDialog=false;}
  else if(gpsWaiting || countdownStart){gpsWaiting=false;countdownStart=0;}
  else if(gpsRadarInspector){gpsRadarInspector=false;}
  else if(quickOpen){if(quickDetail)quickDetail=0;else quickOpen=false;}
  else if(welcomeOpen){
    if(isLibrary()){
      if(librarySelection.length()){
        librarySelection="";libraryMessage="";archiveScroll=0;
        previewZoomLevel=0;previewPanLat=0.0;previewPanLon=0.0;
      }else if(libraryPage>0){
        libraryPage--;loadLibrary();
      }else homePage=HOME_APPS;
    }
    else if(homePage==HOME_FILES && fileDetailOpen){
      fileDetailOpen=false;
      fileDeleteConfirm=false;
    }
    else if(homePage==HOME_FILES && browserPage>0){browserPage--;loadBrowser();}
    else if(homePage==HOME_FILES && browserPath!="/"){
      int slash=browserPath.lastIndexOf('/');
      browserPath=slash<=0?"/":browserPath.substring(0,slash);
      browserPage=0;loadBrowser();
    }else if(homePage==HOME_FILES)homePage=HOME_APPS;
    else homePage=HOME_MAIN;
  }else if(currentScreen!=SCR_DASH)currentScreen=SCR_DASH;
  else {welcomeOpen=true;homePage=HOME_MAIN;}
  uiRevision++;
}
static void drawHomeSubpage(){
  statusBar();
  if(isLibrary()){drawLibrary();return;}
  if(homePage==HOME_APPS){
    drawCustomIcon(display,10,35,uiFg(),CustomIcon::allAppsIcon);
    label(34,37,"ALL APPS",2);
    const char* names[]={"PAST ACTIVITY","RIDE HISTORY","SAVED ROUTES","SETTINGS","MY FILES","MAP"};
    CustomIcon icons[]={CustomIcon::runIcon,CustomIcon::historyIcon,CustomIcon::routeMapIcon,CustomIcon::systemIcon,CustomIcon::folderIcon,CustomIcon::routeMapIcon};
    for(int i=0;i<6;i++){int x=6+(i%2)*117,y=72+(i/2)*70;
      card(x,y,111,62,uiCard());drawCustomIcon(display,x+47,y+10,uiFg(),icons[i]);
      centeredLabel(x,y+40,111,names[i],1);
    }
  }else{
    if(fileDetailOpen){
      drawFileDetail();
      return;
    }
    drawCustomIcon(display,10,35,uiFg(),browserPath=="/"?CustomIcon::folderIcon:CustomIcon::GoBackIcon);
    String folderName = (browserPath == "/") ? "MY FILES" : browserPath;
    if (folderName.length() > 1 && folderName.startsWith("/")) folderName = folderName.substring(1);
    label(34, 37, truncateHeading(folderName, 16), 2);




    if(!sdOk)label(10,100,"SD card unavailable",1,0xFA08);
    else if(browserError.length())label(10,100,browserError,1,0xFA08);
    else if(!browserCount)label(10,100,"This folder is empty",1,uiMuted());
    for(int i=0;i<browserCount;i++){int y=68+i*44;
      card(6,y,228,40,uiCard());
      String low=browserNames[i];low.toLowerCase();
      CustomIcon icon = browserDirs[i] ? CustomIcon::folderIcon :
                        low.endsWith(".fit") ? CustomIcon::runIcon :
                        low.endsWith(".gpx") ? CustomIcon::routeMapIcon :
                        CustomIcon::sdCardIcon;
      uint16_t iconColor = browserDirs[i] ? uiFg() :
                           low.endsWith(".fit") ? TEAL :
                           low.endsWith(".gpx") ? 0x3DF7 : uiFg();
      drawCustomIcon(display,14,y+12,iconColor,icon);
      label(38,y+5,fitBrowserText(browserNames[i],184));
      label(38,y+23,browserDirs[i]?"Folder - tap to open":storageSize(browserSizes[i])+" - tap to view",1,uiMuted());
    }
    if(selectedFile.length())label(8,275,fitBrowserText(selectedFile,224),1,uiMuted());
  }

}
static unsigned startSatellites(){
  if(!gpsDataReady)return 0;
  if(geprcGps.lastStatus && millis()-geprcGps.lastStatus<3000)return geprcGps.satellitesUsed;
  return gps.satellites.isValid() && gps.satellites.age()<3000 ? gps.satellites.value():0;
}
static bool gpsStartReady(){return gpsDataReady && ride.gpsFixed && startSatellites()>=6;}
static void requestRideStart() {
  if(rideStarted || countdownStart || gpsWaiting)return;
  gpsWaiting=true;gpsRadarInspector=false;gpsScroll=0;quickOpen=false;uiRevision++;
}
static void openGpsInspector() {
  gpsRadarInspector=true;gpsWaiting=false;gpsScroll=0;quickOpen=false;quickDetail=0;uiRevision++;
}
static void enterPowerOff() {
  // Always finish FIT before releasing the battery latch.
  if(rideStarted || activityFile) saveActivity();
  countdownStart=0;gpsWaiting=false;gpsRadarInspector=false;
  display.fillScreen(0); digitalWrite(5,LOW);
  // Caller waited for PWR release: it must not hold the latch on or wake sleep.
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_6,0);
  digitalWrite(7,LOW);
  delay(100);
  // Reached on USB power only; battery power normally disappears above.
  esp_deep_sleep_start();
}
static void pollPowerUi() {
  static bool armed=false, fired=false;
  static uint32_t down=0;
  bool pressed=digitalRead(6)==LOW;
  if(!pressed){armed=true;down=0;fired=false;}
  if(shutdownPending){if(!pressed)enterPowerOff();return;}
  if(armed && pressed && !fired) {
    if(!down) down=millis();
    if(millis()-down>=1500){shutdownDialog=true;countdownStart=0;gpsWaiting=false;gpsRadarInspector=false;fired=true;uiRevision++;}
  }
}

static bool touchCommand(uint32_t c) {
  touchBus.beginTransmission(0x58);
  for (int n = 24; n >= 0; n -= 8) touchBus.write((uint8_t)(c >> n));
  uint8_t error=touchBus.endTransmission();
  if(error) Serial.printf("Touch I2C: command 0x%08lX, error %u\n",(unsigned long)c,error);
  return error == 0;
}

static void initQuickTouch() {
  static bool busStarted = false;
  if (busStarted && !touchReady) {
    touchBus.end();
    busStarted=false;
  }
  if (!busStarted) busStarted = touchBus.begin(1, 3, 100000);
  if (!busStarted) {
    touchReady = false;
    Serial.println("Touch: I2C bus initialization failed");
    return;
  }
  touchBus.setTimeOut(20);
  pinMode(4, INPUT);
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW); delay(10);
  digitalWrite(2, HIGH); delay(10);
  pinMode(4, INPUT);
  uint8_t info[50];
  touchReady = false;
  for (int retry = 0; retry < 5 && !touchReady; retry++) {
    if (touchCommand(0xD0030000) && touchBus.requestFrom((uint8_t)0x58, (uint8_t)50) == 50) {
      for (int i = 0; i < 50; i++) info[i] = touchBus.read();
      touchReady = (info[2] == 0xCA && info[3] == 0xCA);
    }
    delay(10);
  }
  Serial.println(touchReady ? "Touch ready (CST3530)" : "Touch: hardware not ready");
}

static void toggleRide() {
  if (!rideStarted) {
    if(!gpsStartReady()){activityMessage="Waiting for GPS fix";uiRevision++;return;}
    if(!beginActivityFile()){uiRevision++;return;}
    distAccum=0; trackLen=0; ride.distanceKm=0;ride.elapsedSec=0;
    ride.avgSpeedKmh=0;ride.maxSpeedKmh=0;ride.maxPowerW=0;
    hrSum=powerSum=0;hrSamples=powerSamples=0;hrMax=0;lastLat=lastLon=0;
    rideStarted = true;
    rideStartMs = millis();
    ridePaused = false;
  } else if (!ridePaused) {
    fitEvent(4);
    ridePaused = true;
    pauseStart = millis();
  } else {
    fitEvent(0);
    rideStartMs += (millis() - pauseStart);
    ridePaused = false;
  }
  uiRevision++;
}

static void updateRideCountdown() {
  if(gpsWaiting && !shutdownDialog && gpsStartReady()){
    gpsWaiting=false;countdownStart=millis();uiRevision++;
  }
  if(countdownStart && !gpsStartReady()){
    countdownStart=0;gpsWaiting=true;uiRevision++;
  }
  if(countdownStart && !shutdownDialog && millis()-countdownStart>=3000) {
    countdownStart=0; toggleRide();
    if(rideStarted){welcomeOpen=false;currentScreen=nav.hasDestination&&routeLen>1?SCR_MAP:SCR_DASH;}
    uiRevision++;
  }
}
static void drawBigNumber(int rightX, int y, int value, uint16_t fg = 0xFFFF) {
  String s = String(value);
  int totalAdvance = 0;
  for (unsigned int k = 0; k < s.length(); k++) {
    char ch = s[k];
    if (ch >= '0' && ch <= '9') {
      totalAdvance += smoothDigitGlyphs48[ch - '0'].advance;
    }
  }
  int pen = (rightX * 16) - totalAdvance;
  for (unsigned int k = 0; k < s.length(); k++) {
    char ch = s[k];
    if (ch < '0' || ch > '9') continue;
    const auto& g = smoothDigitGlyphs48[ch - '0'];
    int gx = (pen + 8) / 16 + g.x;
    int gy = y + g.y;
    for (int row = 0; row < g.h; row++) {
      for (int col = 0; col < g.w; col++) {
        int i = row * g.w + col;
        uint8_t bits = pgm_read_byte(&smoothDigitBits48[g.offset + i / 2]);
        int a = (bits >> ((i & 1) ? 0 : 4)) & 15;
        if (a) {
          uint16_t bg = display.backgroundPixel(gx + col, gy + row);
          display.drawPixel(gx + col, gy + row, blendText(fg, bg, a));
        }
      }
    }
    pen += g.advance;
  }
}

static void drawGpsRadarScreen() {
  const int cx = 120;
  const int cy = 122 - gpsScroll;
  const uint32_t t = millis();

  // Theme-aware color palette
  uint16_t outerFill, middleFill, radarLineColor, centerDiscColor;
  if (darkTheme) {
    outerFill = 0x0040;        // subtle deep dark green (0, 8, 0)
    middleFill = 0x00A0;       // slightly lighter dark green (0, 20, 0)
    radarLineColor = 0x3620;   // vibrant neon green outline and sweep line (#34C200)
    centerDiscColor = 0x3620;  // solid neon green center disc
  } else {
    outerFill = 0xEFBD;        // soft pastel mint tint (238, 246, 238)
    middleFill = 0xD79A;       // deeper pastel mint (213, 242, 213)
    radarLineColor = 0x24C0;   // rich crisp green outline and sweep line (32, 153, 0)
    centerDiscColor = 0x24C0;  // rich green center disc
  }

  // Outer ring (R=88): fill + boundary outline
  display.fillCircle(cx, cy, 88, outerFill);
  display.drawCircle(cx, cy, 88, radarLineColor);

  // Middle ring (R=56): fill (no inner outline, matches Figma)
  display.fillCircle(cx, cy, 56, middleFill);

  // Single Rotating Radar Sweep Line (radius 26 to 88)
  int deg = (t / 8) % 360;
  float rad = radians((float)deg);
  int x1 = cx + (int)(26.0f * cosf(rad));
  int y1 = cy + (int)(26.0f * sinf(rad));
  int x2 = cx + (int)(88.0f * cosf(rad));
  int y2 = cy + (int)(88.0f * sinf(rad));
  display.drawLine(x1, y1, x2, y2, radarLineColor);

  // Satellite Dots
  struct SatPoint { uint8_t r; uint16_t deg; };
  static const SatPoint satPoints[24] = {
    {42, 60},   {74, 25},   {82, 0},    {46, 160},
    {68, 235},  {76, 120},  {50, 310},  {78, 280},
    {40, 85},   {72, 195},  {62, 345},  {48, 215},
    {70, 30},   {52, 140},  {82, 95},   {44, 275},
    {75, 160},  {60, 45},   {78, 220},  {45, 15},
    {70, 180},  {55, 105},  {80, 320},  {72, 240}
  };
  int sats = startSatellites();
  int numDots = constrain(sats, 0, 24);
  for (int i = 0; i < numDots; i++) {
    float sRad = radians((float)satPoints[i].deg);
    int sx = cx + (int)(satPoints[i].r * cosf(sRad));
    int sy = cy + (int)(satPoints[i].r * sinf(sRad));
    display.fillCircle(sx, sy, 3, radarLineColor);
  }

  // Center Disc: solid vibrant green disc (R=26)
  display.fillCircle(cx, cy, 26, centerDiscColor);
  // Center White Satellite Icon (native 32x32 high resolution)
  drawCustomIcon(display, cx - 16, cy - 16, 0xFFFF, CustomIcon::satelliteIconLarge);

  // Content Below Radar
  int textY = 224 - gpsScroll;
  label(20, textY, "LOCKED", 2, uiFg());
  label(20, textY + 18, "SATELLITES", 2, uiFg());
  drawBigNumber(220, textY - 2, sats, uiFg());
  // Satellite Connection Quality Progress Bar (max 6 sats for stable connection)
  int barY = 266 - gpsScroll;
  if (barY < 282 && barY > 24) {
    const int barX = 20;
    const int barW = 200;
    const int barH = 7;
    const int barR = 3;
    const int maxSats = 6;
    uint16_t trackColor = darkTheme ? 0x31A6 : 0xD6BA;
    uint16_t fillColor;
    if (sats < 3) {
      fillColor = darkTheme ? 0xF800 : 0xD000;      // Red (Poor / Searching)
    } else if (sats < maxSats) {
      fillColor = darkTheme ? 0xFE20 : 0xD540;      // Amber Yellow #FFC21C (Fair / Locking)
    } else {
      fillColor = radarLineColor;                   // Vibrant Green (Stable connection / 6+ sats)
    }
    display.fillRoundRect(barX, barY, barW, barH, barR, trackColor);
    int fillW = constrain(sats * barW / maxSats, 0, barW);
    if (sats > 0 && fillW < barH) fillW = barH;
    if (fillW > 0) {
      display.fillRoundRect(barX, barY, fillW, barH, barR, fillColor);
    }
  }

  // Coordinates (visible when scrolled down)
  int latY = 282 - gpsScroll;
  if (latY < 280 && latY > 24) {
    centeredLabel(0, latY, 240, "LATITUDE", 1, uiMuted());
    bool hasCoord = gps.location.isValid() && gps.location.age() < 3000;
    String latStr = hasCoord ? String(gps.location.lat(), 6) : ((ride.lat != 0.0f) ? String(ride.lat, 6) : "--.------");
    centeredLabel(0, latY + 18, 240, latStr, 2, uiFg());
  }
  int lonY = 340 - gpsScroll;
  if (lonY < 280 && lonY > 24) {
    centeredLabel(0, lonY, 240, "LONGITUDE", 1, uiMuted());
    bool hasCoord = gps.location.isValid() && gps.location.age() < 3000;
    String lonStr = hasCoord ? String(gps.location.lng(), 6) : ((ride.lon != 0.0f) ? String(ride.lon, 6) : "--.------");
    centeredLabel(0, lonY + 18, 240, lonStr, 2, uiFg());
  }

  // Overlay clean top status bar and bottom nav bar
  statusBar(true, false);
  drawGlobalNav();
}
 static bool drawLifecycleScreen() {
  static String previous;
  String state;
  if(shutdownDialog) state=shutdownPending?"release":"shutdown";
  else if(gpsWaiting || gpsRadarInspector) state="gpsRadar:"+String(millis()/40)+":"+String(startSatellites())+":"+String(gpsDataReady)+":"+String(gpsScroll)+":"+String(gpsRadarInspector)+":"+String(darkTheme);
  else if(countdownStart) state="count"+String(3-min(2UL,(millis()-countdownStart)/1000));
  else if(welcomeOpen && !quickOpen) state="welcome:"+String(homePage)+":"+String(startSatellites())+":"+String(gpsDataReady)+":"+String(uiRevision)+":"+String(ride.gpsFixed)+":"+String(sdOk)+":"+String(millis()/500)+":"+String(bleCount())+":"+String(WiFi.status());
  if(quickOpen && !shutdownDialog && !gpsWaiting && !gpsRadarInspector && !countdownStart){previous="";return false;}
  if(state.isEmpty()){previous="";return false;}
  if(state==previous)return true;
  previous=state;
  display.fillScreen(uiBg());
  if(shutdownDialog) {
    label(24,72,"SHUT DOWN?",2,0xFFFF);
    label(16,116,"Current activity will be saved",1,0xFFFF);
    label(16,140,"USB uses deep-sleep standby",1,0x8410);
    if(shutdownPending){label(16,185,"Release power button",1,0xFFFF);return true;}
    card(12,205,102,48,0x4208);label(30,222,"CANCEL",1,0xFFFF);
    card(126,205,102,48,0xF800);label(147,222,"CONFIRM",1,0xFFFF);
  } else if(gpsWaiting || gpsRadarInspector) {
    drawGpsRadarScreen();
  } else if(countdownStart) {
    int n=3-min(2UL,(millis()-countdownStart)/1000);
    label(96,124,String(n),8,0xFFFF);
    label(54,240,"Tap to cancel",1,0x8410);
  } else {
    if(homePage!=HOME_MAIN){drawHomeSubpage();return true;}
    statusBar();
    display.drawBrandLogo((DISP_W-BOOT_LOGO_WIDTH)/2,48,!darkTheme);
    bool ready=gpsStartReady();
    bool countValid=gpsDataReady && ((geprcGps.lastStatus && millis()-geprcGps.lastStatus<3000) || (gps.satellites.isValid() && gps.satellites.age()<3000));
    uint16_t gpsFg=ready?(darkTheme?0x9E73:0x2400):gpsDataReady?(darkTheme?0xFFE0:0x8380):(darkTheme?0xFA6A:0x9800);
    uint16_t gpsBg=ready?(darkTheme?0x0940:0xD753):gpsDataReady?(darkTheme?0x2920:0xFFF1):(darkTheme?0x3000:0xFDE7);
    card(6,140,184,36,gpsBg);drawSatelliteIcon(display,16,150,gpsFg);
    centeredLabel(38,154,144,countValid?String(startSatellites())+" SATELLITES"+(ready?" - READY":" - SEARCHING"):"NO GPS DATA AVAILABLE",1,gpsFg);
    card(196,140,38,36,uiCard());
    drawCustomIcon(display,207,150,uiFg(),runningMode?CustomIcon::bikeIcon:CustomIcon::runIcon);
    card(6,182,228,66,0xF800);
    label(22,195,rideStarted?(ridePaused?"Resume":"Active"):"Start",2,0xFFFF);label(22,219,runningMode?"Run":"Ride",2,0xFFFF);
    drawCustomIcon(display,102,197,0xFFFF,runningMode?CustomIcon::runIconLarge:CustomIcon::bikeIconLarge);
    label(165,209,"GO",2,0xFFFF);
    drawCustomIcon(display,189,197,0xFFFF,CustomIcon::goNextIconLarge);
    card(6,254,111,34,uiCard());drawCustomIcon(display,17,263,uiFg(),CustomIcon::allAppsIcon);
    centeredLabel(36,266,77,"ALL APPS",1);
    card(123,254,111,34,uiCard());drawCustomIcon(display,134,263,uiFg(),CustomIcon::folderIcon);
    centeredLabel(153,266,77,"MY FILES",1);
  }
  return true;
}

static void quickTap(int x, int y) {
  if(showAppRefresh() && x>=196 && y>=28 && y<62){
    refreshPending=true;
    return;
  }
  if(y>=290){
    if(x>=26 && x<78){if(globalHasBack())globalBack();}
    else if(x>=94 && x<146)globalHome();
    else if(x>=164 && x<216)globalNext();
    return;
  }
  if(shutdownDialog) {
    if(!shutdownPending && y>=205 && y<253) {
      if(x<120){shutdownDialog=false;uiRevision++;}
      else {shutdownPending=true;uiRevision++;}
    }
    return;
  }
  if(gpsWaiting || gpsRadarInspector){
    return;
  }
  if(countdownStart){countdownStart=0;uiRevision++;return;}
  if(welcomeOpen && !quickOpen && y>=29) {
    if(isLibrary()){libraryTap(x,y);return;}
    if(homePage==HOME_FILES){
      if(fileDetailOpen){
        if(y>=29 && y<60 && x<60){
          fileDetailOpen=false;
          fileDeleteConfirm=false;
          uiRevision++;
          return;
        }
        if(y>=234 && y<274){
          if(!fileDeleteConfirm){
            fileDeleteConfirm=true;
            uiRevision++;
            return;
          } else {
            if(x<118){
              fileDeleteConfirm=false;
              uiRevision++;
              return;
            } else if(x>=124){
              SD_MMC.remove(fileDetailPath);
              fileDetailOpen=false;
              fileDeleteConfirm=false;
              loadBrowser();
              uiRevision++;
              return;
            }
          }
        }
        return;
      }
      if(browserPath != "/" && y>=29 && y<60 && x<60){
        int slash=browserPath.lastIndexOf('/');
        browserPath=slash<=0?"/":browserPath.substring(0,slash);
        browserPage=0;
        loadBrowser();
        uiRevision++;
        return;
      }
      int row=(y-68)/44;
      if(y>=68 && row>=0 && row<browserCount && y<68+row*44+40){
        if(browserDirs[row])openBrowser((browserPath=="/"?String(""):browserPath)+"/"+browserNames[row]);
        else openFileDetail(row);
      }
      return;
    }
    if(homePage==HOME_APPS){
      int col=x>=123&&x<234?1:x>=6&&x<117?0:-1;
      int row=(y-72)/70;
      if(y<72 || row>2 || col<0 || y>=72+row*70+62)return;
      int tile=row*2+col;
      if(tile==0)openLibrary(HOME_ACTIVITY);
      else if(tile==1)openLibrary(HOME_RIDES);
      else if(tile==2)openLibrary(HOME_ROUTES);
      else if(tile==3){quickOpen=true;quickDetail=0;uiRevision++;}
      else if(tile==4)openBrowser("/");
      else {welcomeOpen=false;currentScreen=SCR_MAP;uiRevision++;}
      return;
    }
    if(y>=140 && y<176 && x>=6 && x<190){
      openGpsInspector();
      return;
    }
    else if(x>=196 && x<234 && y>=140 && y<176){
      if(rideStarted){activityMessage="Save activity before switching mode";}
      else {runningMode=!runningMode;activityMessage=runningMode?"Running - GPS + heart rate":"Cycling mode";}
      uiRevision++;
    }
    else if(x>=6 && x<234 && y>=182 && y<248) {
      if(rideStarted) {
        welcomeOpen=false;
        currentScreen=(nav.hasDestination && routeLen>1)?SCR_MAP:SCR_DASH;
      } else {
        requestRideStart();
      }
      uiRevision++;
    }
    else if(y>=254 && y<288){
      if(x>=6 && x<117){homePage=HOME_APPS;uiRevision++;}
      else if(x>=123 && x<234)openBrowser("/");
    }
    return;
  }
  if (y < 29) {
    quickOpen = !quickOpen;
    quickDetail = 0;
    uiRevision++;
    return;
  }
  if (!quickOpen) {
    if (currentScreen == SCR_MAP && y > 240) {
      mapZoom = constrain((int)mapZoom + (x >= 120 ? 1 : -1), 1, 5);
      uiRevision++;
    }
    return;
  }
  if (quickDetail == 7) {
    if(y>=120 && y<166) {if(!rideStarted)requestRideStart();else toggleRide();}
    else if(y>=175 && y<221 && rideStarted) {saveActivity();ridePaused=false;welcomeOpen=true;quickOpen=false;}
    else if(y>=278) quickDetail=0;
    uiRevision++;return;
  }
  if (quickDetail == 2 && y > 235 && y < 275) {
    bleRequest(1);
    uiRevision++;
    return;
  }
  if (quickDetail) {
    quickDetail = 0;
    uiRevision++;
    return;
  }
  y+=quickScroll;
  if(y>=106 && y<156 && x>=20 && x<234){setBrightnessFromTouch(x);return;}
  if (x>=6 && x<234 && y>=32 && y<101) {
    if (x >= 185) {
      if(!rideStarted) requestRideStart();
      else toggleRide();
      uiRevision++;
      return;
    }
    if (rideStarted) {
      quickOpen = false;
      quickDetail = 0;
      welcomeOpen = false;
      currentScreen = (nav.hasDestination && routeLen > 1) ? SCR_MAP : SCR_DASH;
      uiRevision++;
      return;
    } else {
      quickDetail = 7;
      uiRevision++;
      return;
    }
  }
  // Hit-test the actual card rectangles; gutters do not activate neighbours.
  int col = x>=123 && x<234 ? 1 : (x>=6 && x<117 ? 0 : -1);
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
  }
  uiRevision++;
}

static void pollQuickTouch() {
  static uint32_t poll = 0, lastContact = 0;
  static bool held = false;
  static int candidateX=-1,candidateY=-1;
  static uint32_t candidateMs=0;
  static bool rootGesture=false, sliderGesture=false, moved=false;
  static int startX=0,startY=0,startScroll=0,endX=0,endY=0;
  auto release=[&](){
    if(rootGesture && !sliderGesture && !moved)quickTap(endX,endY);
    rootGesture=false;sliderGesture=false;mapPanGesture=false;held=false;candidateX=-1;
  };
  if (!touchReady || millis() - poll < 25) return;
  poll = millis();
  // Poll the manufacturer data register even between interrupt pulses.
  // Only an explicit release may re-arm a tap; IRQ gaps are not releases.
  uint8_t b[32] = {0};
  bool valid = touchCommand(0xD0070000);
  if (valid) {
    int n = touchBus.requestFrom((uint8_t)0x58, (uint8_t)32);
    valid = (n == 32);
    for (int i = 0; i < n; i++) b[i] = touchBus.read();
  }
  if (valid) valid=touchCommand(0xD00002AB);
  if (!valid) {
    touchReady = false;
    uiRevision++;
    return;
  }
  if (b[2] != 0xFF) return;
  int count = b[3] & 15, keys = b[3] >> 4;
  if (!count) { release(); return; }
  if (count > 5 || (keys + count) * 5 + 4 > 32) return;
  // Ignore multi-finger gestures rather than dispatching an arbitrary point.
  if (count != 1) { held = true; return; }
  uint16_t sum = 0x55;
  for (int i = 4; i < 4 + (keys + count) * 5; i++) sum += b[i];
  if (sum != (uint16_t)(b[0] | (b[1] << 8))) return;
  int i = keys * 5;
  if (!(b[i + 8] >> 4)) { release(); return; }
  int x = b[i + 4] | ((b[i + 7] & 15) << 8);
  int y = b[i + 5] | ((b[i + 7] & 240) << 4);
  // Preserve the calibrated native-axis mapping, then rotate with the panel.
  // Reject invalid points before either transform.
  if(x<0 || x>=DISP_W || y<0 || y>=DISP_H)return;
  x=DISP_W-1-x;
  y=DISP_H-1-y;
  // The panel is now 180 degrees from the previously calibrated orientation.
  x=DISP_W-1-x;
  y=DISP_H-1-y;
  lastContact = millis();
  if(rootGesture){
    endX=x;endY=y;
    if(abs(y-startY)>7 || abs(x-startX)>7)moved=true;
    if(sliderGesture)setBrightnessFromTouch(x);
    else if(moved){
      if(gpsWaiting || gpsRadarInspector){
        int offset=constrain(startScroll+startY-y,0,115);
        if(offset!=gpsScroll){gpsScroll=offset;uiRevision++;}
      } else if(isLibrary() && librarySelection.length()){
        if (mapPanGesture) {
          double co = cos(radians(startPanLat));
          double dXm = (x - startX) / max(0.0001, previewScale);
          double dYm = (y - startY) / max(0.0001, previewScale);
          previewPanLon = startPanLon - (dXm / max(0.0001, co * 111320.0));
          previewPanLat = startPanLat + (dYm / 111320.0);
          g_mapCache.updatePosition(previewPanLat, previewPanLon);
          uiRevision++;
        } else {
          int maxS = (homePage == HOME_ROUTES) ? 120 : ARCHIVE_MAX_SCROLL;
          int offset=constrain(startScroll+startY-y,0,maxS);
          if(offset!=archiveScroll){archiveScroll=offset;uiRevision++;}
        }
      } else {
        int offset=constrain(startScroll+startY-y,0,72);
        if(offset!=quickScroll){quickScroll=offset;uiRevision++;}
      }
    }
    return;
  }
  if (!held && x < 240 && y < 320) {
    // Require two nearby reports before activating a control. A noisy or
    // stale first sample must not navigate to another screen.
    if(candidateX<0 || abs(x-candidateX)>12 || abs(y-candidateY)>12){
      candidateX=x;candidateY=y;candidateMs=millis();return;
    }
    if(millis()-candidateMs<25)return;
    held = true;candidateX=-1;
    if(quickOpen && !quickDetail && y>=29 && y<290){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;startScroll=quickScroll;
      sliderGesture=y+quickScroll>=106 && y+quickScroll<156 && x>=40 && x<228;
      if(sliderGesture)setBrightnessFromTouch(x);
    } else if((gpsWaiting || gpsRadarInspector) && y>=29 && y<290){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;startScroll=gpsScroll;
      sliderGesture=false;
    } else if(isLibrary() && librarySelection.length() && y>=29 && y<290){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;startScroll=archiveScroll;
      sliderGesture=false;
      int mapY = 34 - archiveScroll + 34;
      mapPanGesture = (homePage == HOME_ROUTES && previewZoomLevel > 0 &&
                       x >= 6 && x <= 234 && y >= mapY && y < mapY + 180 &&
                       !(x >= 194 && y < mapY + 95));
      if (mapPanGesture) {
        startPanLat = previewPanLat;
        startPanLon = previewPanLon;
      }
    } else quickTap(x, y);
  }
}

static void drawQuickSettings() {
  statusBar(false);
  // Pull-down chevron handle
  drawCustomIcon(display, 108, 12, 0x8430, CustomIcon::downArrowIcon);

  if (quickDetail) {
    card(6, 32, 228, 254, uiCard());
    const char* titles[] = {"", "WI-FI", "BLUETOOTH", "GPS", "", "SYSTEM", "CONFIGURE", "ACTIVITY"};
    CustomIcon headingIcons[]={CustomIcon::systemIcon,CustomIcon::wifiDisconnectedIcon,CustomIcon::bluetoothDisconnectedIcon,CustomIcon::satelliteIcon,CustomIcon::themeSwitcherIcon,CustomIcon::systemIcon,CustomIcon::configureIcon,runningMode?CustomIcon::runIcon:CustomIcon::bikeIcon};
    drawCustomIcon(display,16,47,uiFg(),headingIcons[quickDetail]);
    label(40,46,truncateHeading(titles[quickDetail], 16),2);
    if (quickDetail == 7) {
      label(16, 76, durationText(), 2);
      label(16, 102, rideStarted ? (ridePaused ? "PAUSED" : "RECORDING") : "STOPPED");
      card(16,120,208,44,0x2564);
      label(32,136,rideStarted?(ridePaused?"RESUME":"PAUSE"):(runningMode?"START RUN":"START RIDE"),1,0xFFFF);
      card(16,175,208,44,0xF800);label(32,191,"STOP & SAVE",1,0xFFFF);
      label(16,232,activityMessage);
      label(16,248,sdOk ? "SD card ready" : "NO SD: RAM ONLY");
      label(16,264,String(activityPoints)+" FIT points saved");
      return;
    }
    if (quickDetail == 1) {
      label(16, 88, "Hotspot: CycloComp");
      label(16, 108, "Password: cycling1234");
      label(16, 128, String(WiFi.softAPgetStationNum()) + " clients connected");
      label(16, 158, "Web: http://192.168.4.1");
    }
    if (quickDetail == 2) {
      label(16, 80, bleReady ? "Bluetooth LE: Ready" : "Hardware not ready");
      label(16, 101, String(bleCount()) + " sensors connected");
      label(16, 126, blePhoneConnected() ? (bleCtsSynced() ? "Phone: Connected (Synced)" : "Phone: Connected") : "Phone: Disconnected");
      label(16, 146, blePhoneConnected() ? "Clock auto-synced via BLE" : "Pair: Settings > Bluetooth");
      label(16, 176, bleHRReady ? "Heart rate: Ready" : "HR: searching...");
      label(16, 196, bleCadReady ? "Cadence: Ready" : "Cadence: searching...");
      label(16, 216, blePowerReady ? "Power: Ready" : "Power: searching...");
      card(16, 240, 208, 30, 0x2564);
      label(40, 248, "TAP TO SCAN (5s)", 1, 0xFFFF);
    }
    if (quickDetail == 3) {
      label(16, 88, ride.gpsFixed ? "POSITION LOCKED" : "SEARCHING FOR FIX");
      label(16, 110, String(gps.satellites.value()) + " satellites reported");
      label(16, 136, "Lat: " + String(ride.lat,6));
      label(16, 154, "Lon: " + String(ride.lon,6));
      label(16, 178, String(ride.speedKmh,1)+" km/h",2);
      label(16, 206, String(ride.distanceKm,3)+" km recorded");
      label(16, 228, gpsDataReady?"GPS data received":"GPS hardware not ready");
      label(16, 248, sdOk?activityMessage:"SD not ready");
    }
    if (quickDetail == 5) {
      const char* names[] = {"Touch CST3530", "GPS u-blox M10", "Compass QMC5883L", "BMP390 Baro", "Buttons Expander", "SD Card FAT32"};
      bool ok[] = {touchReady, gpsDataReady, compass.ready && compass.lastSample && (millis() - compass.lastSample < 2000), bmpOk, buttonsOk, sdOk};
      for (int i = 0; i < 6; i++) {
        label(14, 76 + i * 27, names[i]);
        label(14, 88 + i * 27, ok[i] ? "READY" : "OFFLINE / NOT FOUND", 1, ok[i] ? 0x2564 : 0xFA08);
      }
      label(14, 246, sdStatus, 1, uiMuted());
      label(14, 258, time(nullptr)>=1704067200?"Clock synced (IST)":"Clock: open 192.168.4.1", 1, uiMuted());
    }
    if (quickDetail == 6) {
      label(16, 88, "Connect to CycloComp Wi-Fi");
      label(16, 108, "Password: cycling1234");
      label(16, 138, "Open 192.168.4.1");
      label(16, 158, "in your mobile browser");
    }
    
    return;
  }

  const int offset=quickScroll;
  // Activity mode follows the shared home-page selection.
  card(6, 32-offset, 228, 69, 0xF800); // Vibrant Coral Red
  drawCustomIcon(display,18,44-offset,0xFFFF,runningMode?CustomIcon::runIcon:CustomIcon::bikeIcon);
  label(42,44-offset,runningMode?"OUTDOOR RUNNING":"OUTDOOR CYCLING",1,0xFFFF);
  label(16,68-offset,durationText(),3,0xFFFF);

  // Big White Pause/Play Button on right of banner
  display.fillCircle(206, 66-offset, 18, 0xFFFF);
  if (rideStarted && !ridePaused) {
    display.fillRect(201, 58-offset, 4, 16, 0xF800);
    display.fillRect(208, 58-offset, 4, 16, 0xF800);
  } else {
    display.fillTriangle(202, 58-offset, 202, 74-offset, 215, 66-offset, 0xF800);
  }

  card(6,106-offset,228,50,uiCard());
  drawCustomIcon(display,22,123-offset,uiFg(),CustomIcon::brightnessIcon);
  display.fillRoundRect(52,128-offset,166,6,3,uiMuted());
  int thumb=52+(brightnessPercent-5)*166/95;
  display.fillRoundRect(52,128-offset,max(4,thumb-52),6,3,TEAL);
  display.fillCircle(thumb,131-offset,7,uiFg());

  // 2x3 Grid of Tiles
  // Tile 0: Wi-Fi
  {
    int x = 6, y = 162-offset;
    card(x, y, 111, 62, uiCard());
    drawWifiStateIcon(display, x + 48, y + 12, uiFg(), WiFi.status()==WL_CONNECTED);
    bool wifiOn = (WiFi.getMode() != WIFI_OFF);
    centeredLabel(x, y + 42, 111, wifiOn ? "HOTSPOT ACTIVE" : "NOT CONNECTED", 1, wifiOn ? 0x2564 : 0xFA08);
  }

  // Tile 1: Bluetooth
  {
    int x = 123, y = 162-offset;
    int bCount = bleCount() + (blePhoneConnected() ? 1 : 0);
    bool active = (bCount > 0);
    card(x, y, 111, 62, active ? (darkTheme ? 0x0940 : 0xD753) : uiCard());
    uint16_t fg = active ? (darkTheme ? 0x7FE0 : 0x2400) : uiFg();
    drawBluetoothStateIcon(display, x + 38, y + 12, fg, active);
    label(x + 58, y + 12, String(bCount), 2, fg);
    centeredLabel(x, y + 42, 111, String(bCount) + " CONNECTED", 1, fg);
  }

  // Tile 2: GPS
  {
    int x = 6, y = 228-offset;
    bool locked = ride.gpsFixed;
    card(x, y, 111, 62, locked ? (darkTheme ? 0x0940 : 0xD753) : uiCard());
    uint16_t fg = locked ? (darkTheme ? 0x7FE0 : 0x2400) : uiFg();
    drawSatelliteIcon(display, x + 36, y + 12, fg);
    label(x + 58, y + 12, gps.satellites.isValid() && gps.satellites.age()<3000 ? String(gps.satellites.value()) : "0", 2, fg);
    centeredLabel(x, y + 42, 111, locked ? "SAT LOCKED" : "SEARCHING", 1, fg);
  }

  // Tile 3: Theme Toggle
  {
    int x = 123, y = 228-offset;
    card(x, y, 111, 62, uiCard());
    drawThemeIcon(display, x + 48, y + 12, uiFg());
    centeredLabel(x, y + 42, 111, darkTheme ? "DARK MODE" : "LIGHT MODE", 1, uiFg());
  }

  // Tile 4: System Diagnostics
  {
    int x = 6, y = 294-offset;
    card(x, y, 111, 62, uiCard());
    drawGearIcon(display, x + 48, y + 12, uiFg());
    centeredLabel(x, y + 42, 111, "SYSTEM", 1, uiFg());
  }

  // Tile 5: Configuration
  {
    int x = 123, y = 294-offset;
    card(x, y, 111, 62, uiCard());
    drawWrenchIcon(display, x + 48, y + 12, uiFg());
    centeredLabel(x, y + 42, 111, "CONFIGURE", 1, uiFg());
  }
  // Scrolled content can overlap the header. Repaint only its strip,
  // not the whole page: a full clear here erases every settings tile.
  statusBar(false, false);
  drawCustomIcon(display,108,12,0x8430,CustomIcon::downArrowIcon);
  // Small position indicator; all settings remain reachable above the fixed tray.
  display.fillRoundRect(236,32+quickScroll*174/72,3,80,1,uiMuted());

}
