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
  brightnessPercent=constrain(5+(x-46)*95/160,5,100);
  // GPIO5 is the board's active-high backlight; PWM keeps power controls untouched.
  analogWrite(5,(brightnessPercent*255+50)/100);
  uiRevision++;
}
bool welcomeOpen=true, shutdownDialog=false, shutdownPending=false;
uint32_t countdownStart=0;
uint32_t countdownGoStart=0;
uint32_t gpsWaitStartMs=0;
bool gpsWaiting=false;
bool gpsRadarInspector=false;
int gpsScroll=0;
int settingsScroll=0;
// Home subpages stay independent of dashboard navigation and touch calibration.
enum HomePage { HOME_MAIN, HOME_APPS, HOME_FILES, HOME_ACTIVITY, HOME_RIDES, HOME_ROUTES, HOME_SETTINGS, HOME_ACTIVITY_SETTINGS, HOME_COMPASS, HOME_MAPS, HOME_MAP_SEARCH };
HomePage homePage=HOME_MAIN;
bool homeHeroClock = false;
bool activityMapNorthUp = false;
int appsScroll = 0;
const int MAX_APPS_SCROLL = 200;
static int appsPage = 0;
static double mapViewerLat = 22.594637;
static double mapViewerLon = 88.425853;
static int mapViewerZoom = 2; // 1..4
static String mapSearchLat = "22.5946";
static String mapSearchLon = "88.4258";
static int mapSearchField = 0; // 0 for LAT, 1 for LON
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
static void drawLoadingScreen(const char* title = "Loading assets...", const char* subtitle = "Please wait!") {
  statusBar(true, true);
  display.fillRect(0, 28, 240, 292, uiBg());

  const int cardX = 20, cardY = 95, cardW = 200, cardH = 130, cardR = 12;
  display.fillRoundRect(cardX, cardY, cardW, cardH, cardR, uiCard());
  display.drawRoundRect(cardX, cardY, cardW, cardH, cardR, darkTheme ? 0x2945 : 0xD6BA);

  const int cx = 120, cy = 132, r = 16;
  display.drawCircle(cx, cy, r, darkTheme ? 0x2945 : 0xCE79);
  display.drawCircle(cx, cy, r - 1, darkTheme ? 0x2945 : 0xCE79);
  for (int a = 30; a <= 210; a += 15) {
    float rad = radians((float)a);
    int px = cx + (int)lround(cosf(rad) * (float)r);
    int py = cy + (int)lround(sinf(rad) * (float)r);
    int px1 = cx + (int)lround(cosf(rad) * (float)(r - 1));
    int py1 = cy + (int)lround(sinf(rad) * (float)(r - 1));
    display.drawPixel(px, py, TEAL);
    display.drawPixel(px1, py1, TEAL);
  }
  display.fillCircle(cx, cy, 3, TEAL);

  centeredLabel(cardX, cardY + 68, cardW, title, 1, uiFg());
  centeredLabel(cardX, cardY + 90, cardW, subtitle, 1, uiMuted());

  display.refresh();
}
static void drawScreenHeader(const char* title, CustomIcon icon, bool showBack = true) {
  if (showBack) {
    drawCustomIcon(display, 8, 33, uiFg(), CustomIcon::GoBackIcon, 20, 20);
  }
  int titleW = textWidth(title, 0);
  int iconW = 16;
  int gap = 6;
  int totalW = iconW + gap + titleW;
  int startX = (DISP_W - totalW) / 2;
  int iconY = 35;
  int textY = 37;
  drawCustomIcon(display, startX, iconY, uiFg(), icon, 16, 16);
  label(startX + iconW + gap, textY, title, 0, uiFg());
}

#include "settings_app.h"

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
static void openBrowser(String path){drawLoadingScreen("Loading files...", "Please wait!");browserPath=path;browserPage=0;homePage=HOME_FILES;fileDetailOpen=false;fileDeleteConfirm=false;loadBrowser();uiRevision++;}

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
  drawScreenHeader("FILE DETAILS", fileDetailIsFit ? CustomIcon::runIcon : (fileDetailIsGpx ? CustomIcon::routeMapIcon : CustomIcon::sdCardIcon), true);
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
static HomePage prevActivityPage = HOME_APPS;
static String prevActivitySelection = "";
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
static void openLibrary(HomePage page){drawLoadingScreen("Loading assets...", "Please wait!");homePage=page;libraryPage=0;loadLibrary();uiRevision++;}
// App refresh button removed per user request
static bool refreshPending=false;
static bool showAppRefresh(){ return false; }
static void drawAppRefresh(){}
static void serviceAppRefresh(){}
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
      if (topY + 36 >= 28 && topY < 290) {
        const char* zoomSub = (previewZoomLevel == 0) ? "Overview" :
                              (previewZoomLevel == 1) ? "1x District (~1.4km)" :
                              (previewZoomLevel == 2) ? "2x Area (~800m)" :
                              (previewZoomLevel == 3) ? "3x Neighborhood (~450m)" : "4x Street Detail (Max)";
        label(10, topY + 19, zoomSub, 1, previewZoomLevel == 0 ? uiMuted() : TEAL);
      }

      // 1. Prominent Large Route Map Preview Card (180px tall!)
      int mapY = topY + 34;
      previewDraw(mapY, 180, true);

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
      previewDraw(mapY, 88, false);
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

  drawScreenHeader(routes ? "SAVED ROUTES" : "MY ACTIVITIES", routes ? CustomIcon::routeMapIcon : CustomIcon::historyIcon, true);

  if(!libraryCount){
    card(6,76,228,150,uiCard());
    drawCustomIcon(display,108,105,uiMuted(),routes?CustomIcon::routeMapIcon:CustomIcon::historyIcon);
    centeredLabel(6,140,228,libraryMessage.length()?libraryMessage:routes?"No saved routes":"No saved activities",1,uiMuted());
    centeredLabel(6,168,228,routes?"Add routes to SD /routes":"Finish and save an activity",1,uiMuted());
  }
  for(int i=0;i<libraryCount;i++){
    int y=68+i*44;card(6,y,228,40,uiCard());
    drawCustomIcon(display,14,y+12,TEAL,routes?CustomIcon::routeMapIcon:CustomIcon::historyIcon);
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

      // 3. Snap to Ride Start Button Tap
      if (x >= 194 && x <= 234 && y >= mapY + 60 && y < mapY + 88) {
        if (previewLen > 0) {
          if (previewZoomLevel == 0) previewZoomLevel = 3;
          previewPanLat = preview[0].lat;
          previewPanLon = preview[0].lon;
          g_mapCache.updatePosition(previewPanLat, previewPanLon);
          uiRevision++;
        }
        return;
      }

      // 4. Snap to Ride End Button Tap
      if (x >= 194 && x <= 234 && y >= mapY + 88 && y < mapY + 116) {
        if (previewLen > 0) {
          if (previewZoomLevel == 0) previewZoomLevel = 3;
          previewPanLat = preview[previewLen - 1].lat;
          previewPanLon = preview[previewLen - 1].lon;
          g_mapCache.updatePosition(previewPanLat, previewPanLon);
          uiRevision++;
        }
        return;
      }

      if(y >= btnY && y < btnY + 34 && x >= 6 && x <= 234){
        if(rideStarted){libraryMessage="Save current activity first";uiRevision++;return;}
        if(previewLen<2){libraryMessage="GPX missing or invalid";uiRevision++;return;}
        drawLoadingScreen("Starting Route...", "Please wait!");
        routeLen=previewLen;for(int i=0;i<routeLen;i++)route[i]={float(preview[i].lat),float(preview[i].lon)};
        nav.routeIndex=0;nav.hasDestination=true;nav.destLat=route[routeLen-1].lat;nav.destLon=route[routeLen-1].lon;
        snapActiveRoute();
        currentScreen=SCR_MAP;requestRideStart();
      }
      return;
    } else {
      // Past Activity / Ride History Details
      int topY = 34 - archiveScroll;
      int mY3 = topY + 36 + 44 + 44;
      int mapY = mY3 + 28;
      if (x >= 6 && x <= 234 && y >= mapY && y < mapY + 88) {
        String gpxPath = companion(librarySelection, ".gpx");
        if (!SD_MMC.exists(gpxPath)) {
          int slash = gpxPath.lastIndexOf('/');
          String fn = (slash >= 0) ? gpxPath.substring(slash + 1) : gpxPath;
          if (SD_MMC.exists("/activities/" + fn)) gpxPath = "/activities/" + fn;
          else if (SD_MMC.exists("/routes/" + fn)) gpxPath = "/routes/" + fn;
          else if (SD_MMC.exists("/" + fn)) gpxPath = "/" + fn;
        }
        if (SD_MMC.exists(gpxPath) || previewLen >= 2) {
          drawLoadingScreen("Loading Route...", "Please wait!");
          prevActivityPage = homePage;
          prevActivitySelection = librarySelection;
          homePage = HOME_ROUTES;
          librarySelection = gpxPath;
          archiveScroll = 0;
          previewZoomLevel = 0;
          previewPanLat = 0.0;
          previewPanLon = 0.0;
          loadArchive(librarySelection);
          uiRevision++;
          return;
        }
      }
      return;
    }
    return;
  }
  int row=(y-68)/44;
  if(y>=68 && row>=0 && row<libraryCount && y<68+row*44+40){
    drawLoadingScreen("Loading assets...", "Please wait!");
    prevActivitySelection = "";
    prevActivityPage = HOME_APPS;
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
  if(!welcomeOpen) return true;
  if(quickOpen || shutdownDialog || countdownStart || countdownGoStart)return false;
  if(homePage==HOME_FILES && fileDetailOpen)return false;
  if(homePage==HOME_APPS) return false;
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
    gpsScroll = (gpsScroll == 0) ? 128 : 0;
    uiRevision++;
    return;
  }
  if(!welcomeOpen){
    currentScreen = (Screen)(((int)currentScreen + 1) % SCR_COUNT);
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
  if(screenLocked) return false;
  if(shutdownDialog || gpsWaiting || gpsRadarInspector || countdownStart || countdownGoStart || quickOpen) return true;
  if(!welcomeOpen) return true;
  if(homePage != HOME_MAIN) return true;
  return false;
}
static void drawGlobalNav(){
  if(screenLocked) return;
  display.fillRect(0,290,240,30,uiBg());
  if(globalHasBack())drawCustomIcon(display,42,297,uiFg(),CustomIcon::GoBackIcon);
  drawCustomIcon(display,112,297,uiFg(),CustomIcon::HomeIcon);
  if(globalHasNext())drawCustomIcon(display,182,297,uiFg(),CustomIcon::goNextIcon);
}
static void globalHome(){
  if(shutdownPending)return;
  activityEndConfirm=false;
  fileDetailOpen=false;fileDeleteConfirm=false;
  shutdownDialog=false;gpsWaiting=false;gpsRadarInspector=false;countdownStart=0;countdownGoStart=0;
  quickOpen=false;quickDetail=0;welcomeOpen=true;homePage=HOME_MAIN;appsPage=0;appsScroll=0;
  settingsPage=SET_MAIN;settingsScroll=0;settingChangePin=false;settingFormatConfirm=false;pinInput="";
  uiRevision++;
}
static void globalBack(){
  if(shutdownPending)return;
  if(shutdownDialog){shutdownDialog=false;}
  else if(gpsWaiting || countdownStart || countdownGoStart){gpsWaiting=false;countdownStart=0;countdownGoStart=0;}
  else if(gpsRadarInspector){gpsRadarInspector=false;}
  else if(quickOpen){if(quickDetail)quickDetail=0;else quickOpen=false;}
  else if(welcomeOpen){
    if(homePage == HOME_ACTIVITY_SETTINGS) {
      homePage = HOME_APPS;
    }
    else if(homePage == HOME_COMPASS || homePage == HOME_MAPS) {
      homePage = HOME_APPS;
    }
    else if(homePage == HOME_MAP_SEARCH) {
      homePage = HOME_MAPS;
    }
    else if(homePage == HOME_APPS) {
      homePage = HOME_MAIN;
      appsScroll = 0;
    }
    else if(isLibrary()){
      if(librarySelection.length()){
        if(prevActivitySelection.length()){
          drawLoadingScreen("Loading assets...", "Please wait!");
          homePage = prevActivityPage;
          librarySelection = prevActivitySelection;
          prevActivitySelection = "";
          prevActivityPage = HOME_APPS;
          archiveScroll = 0;
          previewZoomLevel = 0; previewPanLat = 0.0; previewPanLon = 0.0;
          loadArchive(librarySelection);
        } else {
          librarySelection="";libraryMessage="";archiveScroll=0;
          previewZoomLevel=0;previewPanLat=0.0;previewPanLon=0.0;
        }
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
  } else if(!welcomeOpen) {
    if(activityEndConfirm) {
      activityEndConfirm = false;
    } else {
      currentScreen = (Screen)(((int)currentScreen - 1 + SCR_COUNT) % SCR_COUNT);
    }
  }
  uiRevision++;
}

static void saveMapNorthUpPref() {
  Preferences p;
  if (p.begin("cyclo", false)) {
    p.putBool("mapNorthUp", activityMapNorthUp);
    p.end();
  }
}

static void saveHomeHeroPref() {
  Preferences p;
  if (p.begin("cyclo", false)) {
    p.putBool("heroClock", homeHeroClock);
    p.end();
  }
}

static void saveActivityConfig() {
  cfg.maxSpeedKmh = runningMode ? cfg.maxSpeedRun : cfg.maxSpeedRide;
  Preferences p;
  if (p.begin("cyclo", false)) {
    p.putFloat("maxSpdRide", cfg.maxSpeedRide);
    p.putFloat("maxSpdRun", cfg.maxSpeedRun);
    p.putInt("cntdownSec", cfg.countdownSec);
    p.end();
  }
}

static void drawActivitySettingsApp() {
  drawScreenHeader("ACTIVITY SETTINGS", CustomIcon::configureIcon, true);

  // Tile 1: Ride Max Speed (Gauge limit)
  int y = 62;
  card(6, y, 228, 42, uiCard());
  drawBikeIcon(display, 16, y + 10, TEAL);
  label(44, y + 6, "RIDE MAX SPEED", 1, uiFg());
  label(44, y + 22, String((int)cfg.maxSpeedRide) + " KM/H (Gauge)", 1, uiMuted());
  display.fillRoundRect(152, y + 7, 30, 28, 6, darkTheme ? 0x2124 : 0xCE79);
  display.drawRoundRect(152, y + 7, 30, 28, 6, darkTheme ? 0x2945 : 0xB596);
  centeredLabel(152, y + 13, 30, "-", 2, uiFg());
  display.fillRoundRect(190, y + 7, 30, 28, 6, darkTheme ? 0x2124 : 0xCE79);
  display.drawRoundRect(190, y + 7, 30, 28, 6, darkTheme ? 0x2945 : 0xB596);
  centeredLabel(190, y + 13, 30, "+", 2, uiFg());

  // Tile 2: Run Max Speed (Gauge limit)
  y = 108;
  card(6, y, 228, 42, uiCard());
  drawCustomIcon(display, 16, y + 10, ORANGE, CustomIcon::runIcon);
  label(44, y + 6, "RUN MAX SPEED", 1, uiFg());
  label(44, y + 22, String((int)cfg.maxSpeedRun) + " KM/H (Gauge)", 1, uiMuted());
  display.fillRoundRect(152, y + 7, 30, 28, 6, darkTheme ? 0x2124 : 0xCE79);
  display.drawRoundRect(152, y + 7, 30, 28, 6, darkTheme ? 0x2945 : 0xB596);
  centeredLabel(152, y + 13, 30, "-", 2, uiFg());
  display.fillRoundRect(190, y + 7, 30, 28, 6, darkTheme ? 0x2124 : 0xCE79);
  display.drawRoundRect(190, y + 7, 30, 28, 6, darkTheme ? 0x2945 : 0xB596);
  centeredLabel(190, y + 13, 30, "+", 2, uiFg());

  // Tile 3: Sport Mode Toggle
  y = 154;
  card(6, y, 228, 42, uiCard());
  drawCustomIcon(display, 16, y + 10, uiFg(), runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon);
  label(44, y + 6, "ACTIVE SPORT", 1, uiFg());
  label(44, y + 22, runningMode ? "Running Mode" : "Cycling Mode", 1, uiMuted());
  display.fillRoundRect(152, y + 7, 68, 28, 14, runningMode ? ORANGE : TEAL);
  centeredLabel(152, y + 14, 68, runningMode ? "RUN" : "RIDE", 1, 0xFFFF);

  // Tile 4: Countdown Timer (3s / 5s / 10s)
  y = 200;
  card(6, y, 228, 42, uiCard());
  drawCustomIcon(display, 16, y + 10, 0xFA40, CustomIcon::routeMapIcon);
  label(44, y + 6, "START COUNTDOWN", 1, uiFg());
  label(44, y + 22, "Timer before activity starts", 1, uiMuted());
  display.fillRoundRect(152, y + 7, 68, 28, 14, 0xFA40);
  centeredLabel(152, y + 14, 68, String(cfg.countdownSec) + " SEC", 1, 0xFFFF);

  // Tile 5: Speed Deadzone info
  y = 246;
  card(6, y, 228, 38, uiCard());
  drawSatelliteIcon(display, 16, y + 10, uiFg());
  label(44, y + 6, "GPS SPEED DEADZONE", 1, uiFg());
  label(44, y + 20, runningMode ? "Run: < 1.8 km/h clamped to 0" : "Ride: < 1.2 km/h clamped to 0", 1, uiMuted());
}

static void tapActivitySettings(int x, int y) {
  if (y < 56 && x < 48) {
    homePage = HOME_APPS;
    uiRevision++;
    return;
  }

  // Tile 1: Ride Max Speed (y=68..120)
  if (y >= 68 && y < 120) {
    if (x >= 140 && x < 184) {
      cfg.maxSpeedRide = max(20.0f, cfg.maxSpeedRide - 5.0f);
      saveActivityConfig();
      uiRevision++;
    } else if (x >= 184 && x < 232) {
      cfg.maxSpeedRide = min(120.0f, cfg.maxSpeedRide + 5.0f);
      saveActivityConfig();
      uiRevision++;
    }
    return;
  }

  // Tile 2: Run Max Speed (y=126..178)
  if (y >= 126 && y < 178) {
    if (x >= 140 && x < 184) {
      cfg.maxSpeedRun = max(10.0f, cfg.maxSpeedRun - 2.0f);
      saveActivityConfig();
      uiRevision++;
    } else if (x >= 184 && x < 232) {
      cfg.maxSpeedRun = min(40.0f, cfg.maxSpeedRun + 2.0f);
      saveActivityConfig();
      uiRevision++;
    }
    return;
  }

  // Tile 3: Active Sport Toggle (y=154..196)
  if (y >= 154 && y < 196) {
    runningMode = !runningMode;
    saveActivityConfig();
    uiRevision++;
    return;
  }

  // Tile 4: Countdown Timer (y=200..242)
  if (y >= 200 && y < 242) {
    if (cfg.countdownSec == 3) cfg.countdownSec = 5;
    else if (cfg.countdownSec == 5) cfg.countdownSec = 10;
    else cfg.countdownSec = 3;
    saveActivityConfig();
    uiRevision++;
    return;
  }
}

static const char* getCardinalDirection(float deg) {
  deg = fmodf(deg + 360.0f, 360.0f);
  if (deg >= 337.5f || deg < 22.5f) return "N";
  if (deg < 67.5f) return "NE";
  if (deg < 112.5f) return "E";
  if (deg < 157.5f) return "SE";
  if (deg < 202.5f) return "S";
  if (deg < 247.5f) return "SW";
  if (deg < 292.5f) return "W";
  return "NW";
}

static void drawCompassApp() {
  display.fillRect(0, 28, 240, 262, uiBg());

  const int cx = 120, cy = 135, r = 78;

  // Outer circular dial
  display.drawCircle(cx, cy, r, uiFg());
  display.drawCircle(cx, cy, r - 1, uiFg());

  // Cardinal letters: N in Red, others in White
  label(cx - 5, cy - r + 8, "N", 2, 0xF800);
  label(cx - 5, cy + r - 22, "S", 2, uiFg());
  label(cx + r - 20, cy - 7, "E", 2, uiFg());
  label(cx - r + 8, cy - 7, "W", 2, uiFg());

  // Current heading
  float h = (compass.ready && isfinite(compass.heading)) ? compass.heading : (ride.gpsFixed ? (float)ride.heading : 0.0f);
  float rad = radians(h);
  float dx = sinf(rad), dy = -cosf(rad);
  float px = -dy, py = dx;

  float L = 58.0f;
  float hw = 6.0f;

  int ntx = lroundf((float)cx + L * dx);
  int nty = lroundf((float)cy + L * dy);
  int stx = lroundf((float)cx - L * dx);
  int sty = lroundf((float)cy - L * dy);
  int s1x = lroundf((float)cx + hw * px);
  int s1y = lroundf((float)cy + hw * py);
  int s2x = lroundf((float)cx - hw * px);
  int s2y = lroundf((float)cy - hw * py);

  // Red North half
  display.fillTriangle(ntx, nty, s1x, s1y, s2x, s2y, 0xF800);
  // White South half
  display.fillTriangle(stx, sty, s1x, s1y, s2x, s2y, uiFg());

  // Center pivot (dark hole with white rim)
  display.fillCircle(cx, cy, 6, uiBg());
  display.drawCircle(cx, cy, 6, uiFg());
  display.drawCircle(cx, cy, 7, uiFg());

  // Large heading readout at bottom (Frame 16: "20°N")
  int deg = (int)lroundf(fmodf(h + 360.0f, 360.0f)) % 360;
  const char* card = getCardinalDirection(deg);
  String degStr = String(deg);
  int wNum = textWidth(degStr, 3);
  int wCard = textWidth(card, 3);
  int degGap = 12;
  int totalW = wNum + degGap + wCard;
  int startX = (DISP_W - totalW) / 2;
  int yText = 246;

  label(startX, yText, degStr, 3, uiFg());
  display.drawCircle(startX + wNum + 5, yText + 5, 3, uiFg());
  display.drawCircle(startX + wNum + 5, yText + 5, 2, uiFg());
  label(startX + wNum + degGap, yText, card, 3, uiFg());
}

static void drawMapViewerApp() {
  uint16_t terrainBg = darkTheme ? 0x18E3 : 0xF79D;
  display.fillRect(0, 28, 240, 262, terrainBg);

  const int clipX1 = 0, clipY1 = 28, clipX2 = 239, clipY2 = 289;
  const int cardCenterX = 120, cardCenterY = 158;

  // 4 Preset Zoom Levels:
  // Level 1: 0.15 (District, ~1.5 km across)
  // Level 2: 0.27 (Neighborhood, ~900 m across)
  // Level 3: 0.48 (Street View, ~500 m across)
  // Level 4: 0.85 (Max Zoom, ~280 m across)
  float scale = (mapViewerZoom == 1) ? 0.15f : (mapViewerZoom == 2) ? 0.27f : (mapViewerZoom == 3) ? 0.48f : 0.85f;

  double centerLat = mapViewerLat;
  double centerLon = mapViewerLon;

  // Align PSRAM sliding window around current pin position
  g_mapCache.updatePosition(centerLat, centerLon);

  int centerTX, centerTY;
  float cfx, cfy;
  MapTileCache::latLonToTile(centerLat, centerLon, centerTX, centerTY, cfx, cfy);
  int centerU = (int)lround(cfx * 4096.0f);
  int centerV = (int)lround(cfy * 4096.0f);

  double latRad = centerLat * (PI / 180.0);
  float metersPerUnit = (float)((40075016.686 * cos(latRad) / 16384.0) / 4096.0);
  float pxPerUnit = metersPerUnit * scale;

  int maxDxU = (int)(130.0f / pxPerUnit) + 120;
  int maxDyU = (int)(140.0f / pxPerUnit) + 120;

  static int s_viewerPolyX[256];
  static int s_viewerPolyY[256];

  const uint16_t parkFill    = darkTheme ? 0x1A44 : 0xC673;
  const uint16_t parkBorder  = darkTheme ? 0x1222 : 0xAE30;
  const uint16_t waterFill   = darkTheme ? 0x1A36 : 0x9E5F;
  const uint16_t waterBorder = darkTheme ? 0x1171 : 0x7CD8;
  const uint16_t roadCasing  = darkTheme ? 0x3186 : 0xD69A;
  const uint16_t roadFill    = darkTheme ? 0x52AA : 0xFFFF;

  auto clipAndDrawLine = [&](int x0, int y0, int x1, int y1, uint16_t color, int width) {
    if ((x0 < clipX1 && x1 < clipX1) || (x0 > clipX2 && x1 > clipX2) ||
        (y0 < clipY1 && y1 < clipY1) || (y0 > clipY2 && y1 > clipY2)) return;

    auto getCode = [&](int x, int y) -> uint8_t {
      uint8_t code = 0;
      if (x < clipX1) code |= 1;
      else if (x > clipX2) code |= 2;
      if (y < clipY1) code |= 4;
      else if (y > clipY2) code |= 8;
      return code;
    };
    uint8_t c0 = getCode(x0, y0), c1 = getCode(x1, y1);
    while (true) {
      if (!(c0 | c1)) {
        if (width <= 1) {
          display.drawLine(x0, y0, x1, y1, color);
        } else {
          int dx = abs(x1 - x0);
          int dy = abs(y1 - y0);
          int h = width / 2;
          if (dx >= dy) {
            for (int k = -h; k <= width - 1 - h; k++) {
              display.drawLine(x0, y0 + k, x1, y1 + k, color);
            }
          } else {
            for (int k = -h; k <= width - 1 - h; k++) {
              display.drawLine(x0 + k, y0, x1 + k, y1, color);
            }
          }
          if (width >= 3) {
            display.fillCircle(x0, y0, h, color);
            display.fillCircle(x1, y1, h, color);
          }
        }
        return;
      } else if (c0 & c1) {
        return;
      } else {
        int x, y;
        uint8_t cOut = c0 ? c0 : c1;
        if (cOut & 8) {
          x = (y1 != y0) ? (x0 + (int)((int64_t)(x1 - x0) * (clipY2 - y0) / (y1 - y0))) : x0;
          y = clipY2;
        } else if (cOut & 4) {
          x = (y1 != y0) ? (x0 + (int)((int64_t)(x1 - x0) * (clipY1 - y0) / (y1 - y0))) : x0;
          y = clipY1;
        } else if (cOut & 2) {
          y = (x1 != x0) ? (y0 + (int)((int64_t)(y1 - y0) * (clipX2 - x0) / (x1 - x0))) : y0;
          x = clipX2;
        } else {
          y = (x1 != x0) ? (y0 + (int)((int64_t)(y1 - y0) * (clipX1 - x0) / (x1 - x0))) : y0;
          x = clipX1;
        }
        if (cOut == c0) { x0 = x; y0 = y; c0 = getCode(x0, y0); }
        else { x1 = x; y1 = y; c1 = getCode(x1, y1); }
      }
    }
  };

  // PASS 1: FILLED POLYGONS (Parks & Water)
  for (int s = 0; s < MapTileCache::CACHE_SIZE; s++) {
    if (!g_mapCache.tiles[s].loaded || !g_mapCache.tiles[s].features || !g_mapCache.tiles[s].points) continue;
    int tx = g_mapCache.tiles[s].tileX;
    int ty = g_mapCache.tiles[s].tileY;
    if (abs(tx - centerTX) > 1 || abs(ty - centerTY) > 1) continue;

    int tileDeltaX = (tx - centerTX) * 4096 - centerU;
    int tileDeltaY = (ty - centerTY) * 4096 - centerV;

    MapFeatureRecord* feats = g_mapCache.tiles[s].features;
    MapPoint12* pts = g_mapCache.tiles[s].points;
    uint16_t featCount = g_mapCache.tiles[s].header.featureCount;

    for (uint16_t f = 0; f < featCount; f++) {
      uint8_t ft = feats[f].type;
      if (ft != FEAT_WATER && ft != FEAT_PARK) continue;
      uint8_t cnt = feats[f].pointCount;
      uint16_t off = feats[f].pointOffset;
      if (cnt < 2 || cnt > 255) continue;

      int p0X = tileDeltaX + pts[off].u;
      int p0Y = tileDeltaY + pts[off].v;
      int pEndX = tileDeltaX + pts[off + cnt - 1].u;
      int pEndY = tileDeltaY + pts[off + cnt - 1].v;
      if ((p0X > maxDxU && pEndX > maxDxU) || (p0X < -maxDxU && pEndX < -maxDxU) ||
          (p0Y > maxDyU && pEndY > maxDyU) || (p0Y < -maxDyU && pEndY < -maxDyU)) continue;

      bool hasJump = false;
      for (uint8_t k = 1; k < cnt; k++) {
        if (abs(pts[off + k].u - pts[off + k - 1].u) > 500 || abs(pts[off + k].v - pts[off + k - 1].v) > 500) {
          hasJump = true; break;
        }
      }
      if (hasJump) continue;

      bool isClosed = (cnt >= 3 && abs(pts[off].u - pts[off + cnt - 1].u) <= 35 &&
                                   abs(pts[off].v - pts[off + cnt - 1].v) <= 35);
      for (uint8_t k = 0; k < cnt; k++) {
        s_viewerPolyX[k] = cardCenterX + (int)((tileDeltaX + pts[off + k].u) * pxPerUnit);
        s_viewerPolyY[k] = cardCenterY + (int)((tileDeltaY + pts[off + k].v) * pxPerUnit);
      }
      if (ft == FEAT_PARK || isClosed) {
        uint16_t fCol = (ft == FEAT_PARK) ? parkFill : waterFill;
        uint16_t bCol = (ft == FEAT_PARK) ? parkBorder : waterBorder;
        fillPolygon(s_viewerPolyX, s_viewerPolyY, cnt, fCol, clipX1, clipY1, clipX2, clipY2);
        for (uint8_t k = 1; k < cnt; k++) {
          clipAndDrawLine(s_viewerPolyX[k - 1], s_viewerPolyY[k - 1], s_viewerPolyX[k], s_viewerPolyY[k], bCol, 1);
        }
      } else {
        for (uint8_t k = 1; k < cnt; k++) {
          clipAndDrawLine(s_viewerPolyX[k - 1], s_viewerPolyY[k - 1], s_viewerPolyX[k], s_viewerPolyY[k], waterFill, 3);
        }
      }
    }
  }

  // PASS 2: ROADS
  int minorCasingW = (mapViewerZoom == 4) ? 5 : (mapViewerZoom == 3) ? 4 : (mapViewerZoom == 2) ? 3 : 2;
  int minorFillW   = (mapViewerZoom == 4) ? 3 : (mapViewerZoom == 3) ? 2 : (mapViewerZoom == 2) ? 1 : 1;
  int majorCasingW = (mapViewerZoom == 4) ? 7 : (mapViewerZoom == 3) ? 5 : (mapViewerZoom == 2) ? 4 : 3;
  int majorFillW   = (mapViewerZoom == 4) ? 5 : (mapViewerZoom == 3) ? 3 : (mapViewerZoom == 2) ? 2 : 1;

  // 2A. Road Casings
  for (int s = 0; s < MapTileCache::CACHE_SIZE; s++) {
    if (!g_mapCache.tiles[s].loaded || !g_mapCache.tiles[s].features || !g_mapCache.tiles[s].points) continue;
    int tx = g_mapCache.tiles[s].tileX;
    int ty = g_mapCache.tiles[s].tileY;
    if (abs(tx - centerTX) > 1 || abs(ty - centerTY) > 1) continue;

    int tileDeltaX = (tx - centerTX) * 4096 - centerU;
    int tileDeltaY = (ty - centerTY) * 4096 - centerV;

    MapFeatureRecord* feats = g_mapCache.tiles[s].features;
    MapPoint12* pts = g_mapCache.tiles[s].points;
    uint16_t featCount = g_mapCache.tiles[s].header.featureCount;

    for (uint16_t f = 0; f < featCount; f++) {
      uint8_t ft = feats[f].type;
      if (ft != FEAT_MOTORWAY && ft != FEAT_PRIMARY && ft != FEAT_SECONDARY &&
          ft != FEAT_RESIDENTIAL && ft != FEAT_CYCLEWAY) continue;
      uint8_t cnt = feats[f].pointCount;
      uint16_t off = feats[f].pointOffset;
      if (cnt < 2) continue;

      int p0X = tileDeltaX + pts[off].u;
      int p0Y = tileDeltaY + pts[off].v;
      int pEndX = tileDeltaX + pts[off + cnt - 1].u;
      int pEndY = tileDeltaY + pts[off + cnt - 1].v;
      if ((p0X > maxDxU && pEndX > maxDxU) || (p0X < -maxDxU && pEndX < -maxDxU) ||
          (p0Y > maxDyU && pEndY > maxDyU) || (p0Y < -maxDyU && pEndY < -maxDyU)) continue;

      bool isMajor = (ft == FEAT_MOTORWAY || ft == FEAT_PRIMARY || ft == FEAT_SECONDARY);
      int w = isMajor ? majorCasingW : minorCasingW;

      int sx0 = cardCenterX + (int)(p0X * pxPerUnit);
      int sy0 = cardCenterY + (int)(p0Y * pxPerUnit);
      for (uint8_t k = 1; k < cnt; k++) {
        if (abs(pts[off + k].u - pts[off + k - 1].u) > 500 || abs(pts[off + k].v - pts[off + k - 1].v) > 500) {
          sx0 = cardCenterX + (int)((tileDeltaX + pts[off + k].u) * pxPerUnit);
          sy0 = cardCenterY + (int)((tileDeltaY + pts[off + k].v) * pxPerUnit);
          continue;
        }
        int pkX = tileDeltaX + pts[off + k].u;
        int pkY = tileDeltaY + pts[off + k].v;
        int sx1 = cardCenterX + (int)(pkX * pxPerUnit);
        int sy1 = cardCenterY + (int)(pkY * pxPerUnit);
        clipAndDrawLine(sx0, sy0, sx1, sy1, roadCasing, w);
        sx0 = sx1;
        sy0 = sy1;
      }
    }
  }

  // 2B. Road Inner Fills
  for (int s = 0; s < MapTileCache::CACHE_SIZE; s++) {
    if (!g_mapCache.tiles[s].loaded || !g_mapCache.tiles[s].features || !g_mapCache.tiles[s].points) continue;
    int tx = g_mapCache.tiles[s].tileX;
    int ty = g_mapCache.tiles[s].tileY;
    if (abs(tx - centerTX) > 1 || abs(ty - centerTY) > 1) continue;

    int tileDeltaX = (tx - centerTX) * 4096 - centerU;
    int tileDeltaY = (ty - centerTY) * 4096 - centerV;

    MapFeatureRecord* feats = g_mapCache.tiles[s].features;
    MapPoint12* pts = g_mapCache.tiles[s].points;
    uint16_t featCount = g_mapCache.tiles[s].header.featureCount;

    for (uint16_t f = 0; f < featCount; f++) {
      uint8_t ft = feats[f].type;
      if (ft != FEAT_MOTORWAY && ft != FEAT_PRIMARY && ft != FEAT_SECONDARY &&
          ft != FEAT_RESIDENTIAL && ft != FEAT_CYCLEWAY) continue;
      uint8_t cnt = feats[f].pointCount;
      uint16_t off = feats[f].pointOffset;
      if (cnt < 2) continue;

      int p0X = tileDeltaX + pts[off].u;
      int p0Y = tileDeltaY + pts[off].v;
      int pEndX = tileDeltaX + pts[off + cnt - 1].u;
      int pEndY = tileDeltaY + pts[off + cnt - 1].v;
      if ((p0X > maxDxU && pEndX > maxDxU) || (p0X < -maxDxU && pEndX < -maxDxU) ||
          (p0Y > maxDyU && pEndY > maxDyU) || (p0Y < -maxDyU && pEndY < -maxDyU)) continue;

      bool isMajor = (ft == FEAT_MOTORWAY || ft == FEAT_PRIMARY || ft == FEAT_SECONDARY);
      int w = isMajor ? majorFillW : minorFillW;

      int sx0 = cardCenterX + (int)(p0X * pxPerUnit);
      int sy0 = cardCenterY + (int)(p0Y * pxPerUnit);
      for (uint8_t k = 1; k < cnt; k++) {
        if (abs(pts[off + k].u - pts[off + k - 1].u) > 500 || abs(pts[off + k].v - pts[off + k - 1].v) > 500) {
          sx0 = cardCenterX + (int)((tileDeltaX + pts[off + k].u) * pxPerUnit);
          sy0 = cardCenterY + (int)((tileDeltaY + pts[off + k].v) * pxPerUnit);
          continue;
        }
        int pkX = tileDeltaX + pts[off + k].u;
        int pkY = tileDeltaY + pts[off + k].v;
        int sx1 = cardCenterX + (int)(pkX * pxPerUnit);
        int sy1 = cardCenterY + (int)(pkY * pxPerUnit);
        clipAndDrawLine(sx0, sy0, sx1, sy1, roadFill, w);
        sx0 = sx1;
        sy0 = sy1;
      }
    }
  }

  // Dropped Pin at center
  drawCustomIcon(display, cardCenterX - 8, cardCenterY - 16, 0x03FF, CustomIcon::locationPinIcon);
  display.fillCircle(cardCenterX, cardCenterY, 3, 0x03FF);
  display.drawCircle(cardCenterX, cardCenterY, 3, 0xFFFF);

  // Floating Search Button (top-left)
  display.fillRoundRect(10, 36, 68, 30, 8, uiCard());
  display.drawRoundRect(10, 36, 68, 30, 8, darkTheme ? 0x39E7 : 0xC618);
  drawCustomIcon(display, 16, 43, uiFg(), CustomIcon::locationPinIcon);
  label(36, 44, "FIND", 1, uiFg());

  // Floating Zoom In [+]
  display.fillRoundRect(196, 36, 34, 30, 8, uiCard());
  display.drawRoundRect(196, 36, 34, 30, 8, darkTheme ? 0x39E7 : 0xC618);
  label(208, 43, "+", 2, mapViewerZoom < 4 ? uiFg() : uiMuted());

  // Floating Zoom Out [-]
  display.fillRoundRect(196, 72, 34, 30, 8, uiCard());
  display.drawRoundRect(196, 72, 34, 30, 8, darkTheme ? 0x39E7 : 0xC618);
  label(210, 78, "-", 2, mapViewerZoom > 1 ? uiFg() : uiMuted());

  // Floating GPS Recenter
  display.fillRoundRect(196, 108, 34, 30, 8, uiCard());
  display.drawRoundRect(196, 108, 34, 30, 8, darkTheme ? 0x39E7 : 0xC618);
  drawCustomIcon(display, 205, 115, ride.gpsFixed ? TEAL : uiMuted(), CustomIcon::satelliteIcon);

  // Zoom badge (bottom-left)
  display.fillRoundRect(10, 262, 40, 22, 11, uiCard());
  display.drawRoundRect(10, 262, 40, 22, 11, darkTheme ? 0x39E7 : 0xC618);
  centeredLabel(10, 267, 40, String(mapViewerZoom) + "/4", 0, uiMuted());

  // Dropped Pin coords pill (bottom-center)
  int pillW = 150, pillH = 22;
  int pillX = (DISP_W - pillW) / 2 + 10, pillY = 262;
  display.fillRoundRect(pillX, pillY, pillW, pillH, 11, uiCard());
  display.drawRoundRect(pillX, pillY, pillW, pillH, 11, darkTheme ? 0x39E7 : 0xC618);
  String coordStr = String(mapViewerLat, 4) + ", " + String(mapViewerLon, 4);
  centeredLabel(pillX, pillY + 5, pillW, coordStr, 0, uiFg());
}

static void drawMapSearchApp() {
  drawScreenHeader("SEARCH COORD", CustomIcon::locationPinIcon, true);

  // Input boxes
  display.fillRoundRect(12, 68, 216, 26, 6, uiCard());
  display.drawRoundRect(12, 68, 216, 26, 6, mapSearchField == 0 ? TEAL : (darkTheme ? 0x2945 : 0xD6BA));
  label(18, 74, "LAT: " + mapSearchLat + (mapSearchField == 0 ? "_" : ""), 1, uiFg());

  display.fillRoundRect(12, 98, 216, 26, 6, uiCard());
  display.drawRoundRect(12, 98, 216, 26, 6, mapSearchField == 1 ? TEAL : (darkTheme ? 0x2945 : 0xD6BA));
  label(18, 104, "LON: " + mapSearchLon + (mapSearchField == 1 ? "_" : ""), 1, uiFg());

  // Keypad: 4 rows x 3 columns
  const char* keys[4][3] = {
    {"1", "2", "3"},
    {"4", "5", "6"},
    {"7", "8", "9"},
    {".", "0", "DEL"}
  };
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      int kx = 14 + c * 72;
      int ky = 130 + r * 30;
      display.fillRoundRect(kx, ky, 68, 26, 6, uiCard());
      display.drawRoundRect(kx, ky, 68, 26, 6, darkTheme ? 0x2945 : 0xD6BA);
      centeredLabel(kx, ky + 6, 68, keys[r][c], 1, (r == 3 && c == 2) ? 0xFA08 : uiFg());
    }
  }

  // Action Buttons row at y = 254
  // CLEAR
  display.fillRoundRect(14, 254, 68, 28, 6, uiCard());
  display.drawRoundRect(14, 254, 68, 28, 6, darkTheme ? 0x2945 : 0xD6BA);
  centeredLabel(14, 262, 68, "CLEAR", 1, uiMuted());

  // MY LOC
  display.fillRoundRect(86, 254, 68, 28, 6, uiCard());
  display.drawRoundRect(86, 254, 68, 28, 6, darkTheme ? 0x2945 : 0xD6BA);
  centeredLabel(86, 262, 68, "MY LOC", 1, uiFg());

  // SEARCH
  display.fillRoundRect(158, 254, 68, 28, 6, TEAL);
  display.drawRoundRect(158, 254, 68, 28, 6, TEAL);
  centeredLabel(158, 262, 68, "SEARCH", 1, 0xFFFF);
}

static void drawHomeSubpage(){

  statusBar();
  if(isLibrary()){drawLibrary();return;}
  if(homePage==HOME_APPS){
    struct AppItem {
      const char* name;
      CustomIcon icon;
      uint16_t color;
    };
    static const AppItem kApps[] = {
      {"Maps", CustomIcon::mapAppIcon, 0x33FB},           // Bright Blue (#3578DD)
      {"Compass", CustomIcon::compassIcon, 0xE800},        // Bright Red (#EE0000)
      {"My Activities", CustomIcon::runIcon, 0xFA61},      // Bright Orange (#F4510C)
      {"Past Routes", CustomIcon::routeMapIcon, 0x5444},   // Leaf Green (#558920)
      {"Settings", CustomIcon::systemIcon, 0x79F7},        // Purple (#7C3AED)
      {"Activity Settings", CustomIcon::configureIcon, 0xDBC0}, // Amber (#D97706)
      {"My Files", CustomIcon::folderIcon, 0x0CA1}         // Teal (#0D9488)
    };
    const int kNumApps = 7;
    const int R = 36;
    const int rowPitch = 108;

    for (int i = 0; i < kNumApps; i++) {
      int col = i % 2;
      int row = i / 2;
      int cx = (col == 0) ? 68 : 172;
      int cy = 100 + row * rowPitch - appsScroll;

      // Cull items completely offscreen
      if (cy + R + 22 < 56 || cy - R > 290) continue;

      display.fillCircle(cx, cy, R, kApps[i].color);
      drawCustomIcon(display, cx - 17, cy - 17, 0xFFFF, kApps[i].icon, 34, 34);
      centeredLabel(cx - 58, cy + R + 7, 116, kApps[i].name, 0, uiFg());
    }

    // Scrollbar indicator
    const int maxS = MAX_APPS_SCROLL;
    if (maxS > 0) {
      int trackH = 290 - 58 - 36;
      int thumbY = 58 + (appsScroll * trackH) / maxS;
      display.fillRoundRect(236, thumbY, 3, 36, 1, uiMuted());
    }

    // Re-draw fixed header and status bar to cleanly clip top scroll
    display.fillRect(0, 0, DISP_W, 56, uiBg());
    statusBar(true, false);
    drawScreenHeader("All Apps", CustomIcon::allAppsIcon, false);

    // Re-draw fixed bottom nav bar to cleanly clip bottom scroll
    display.fillRect(0, 290, DISP_W, 30, uiBg());
    drawGlobalNav();
  }else if(homePage==HOME_SETTINGS){
    drawSettingsApp();
  }else if(homePage==HOME_ACTIVITY_SETTINGS){
    drawActivitySettingsApp();
  }else if(homePage==HOME_COMPASS){
    drawCompassApp();
  }else if(homePage==HOME_MAPS){
    drawMapViewerApp();
  }else if(homePage==HOME_MAP_SEARCH){
    drawMapSearchApp();
  }else{
    if(fileDetailOpen){
      drawFileDetail();
      return;
    }
    String folderName = (browserPath == "/") ? "MY FILES" : browserPath;
    if (folderName.length() > 1 && folderName.startsWith("/")) folderName = folderName.substring(1);
    drawScreenHeader(truncateHeading(folderName, 16).c_str(), CustomIcon::folderIcon, browserPath != "/");




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
  gpsWaiting=true;gpsWaitStartMs=millis();gpsRadarInspector=false;gpsScroll=0;quickOpen=false;uiRevision++;
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
    resetMaxSpeedTracker();
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
  // 1. GPS waiting: MUST show GPS status for at least 2000 ms, then check gpsStartReady()
  if (gpsWaiting && !shutdownDialog) {
    if (millis() - gpsWaitStartMs >= 2000 && gpsStartReady()) {
      gpsWaiting = false;
      countdownStart = millis();
      countdownGoStart = 0;
      uiRevision++;
    }
  }

  // If GPS lost during countdown before GO, revert to GPS status
  if (countdownStart && !countdownGoStart && !gpsStartReady()) {
    countdownStart = 0;
    gpsWaiting = true;
    gpsWaitStartMs = millis();
    uiRevision++;
  }

  // 2. Countdown phase: run for cfg.countdownSec seconds
  uint32_t totalCountdownMs = (uint32_t)cfg.countdownSec * 1000UL;
  if (countdownStart && !countdownGoStart && !shutdownDialog) {
    if (millis() - countdownStart >= totalCountdownMs) {
      // Countdown finished -> Show big "GO" screen!
      countdownGoStart = millis();
      uiRevision++;
    }
  }

  // 3. "GO" screen phase: display for 700ms before launching activity panes
  if (countdownGoStart && !shutdownDialog) {
    if (millis() - countdownGoStart >= 700) {
      countdownStart = 0;
      countdownGoStart = 0;
      toggleRide();
      if (rideStarted) {
        welcomeOpen = false;
        currentScreen = (nav.hasDestination && routeLen > 1) ? SCR_MAP : SCR_DASH;
      }
      uiRevision++;
    }
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
  const int cy = 135 - gpsScroll;
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

  // Outer ring (R=78): unified size & position with Compass and Countdown dials
  display.fillCircle(cx, cy, 78, outerFill);
  display.drawCircle(cx, cy, 78, radarLineColor);

  // Middle ring (R=50): fill (no inner outline, matches Figma)
  display.fillCircle(cx, cy, 50, middleFill);

  // Single Rotating Radar Sweep Line (radius 22 to 78)
  int deg = (t / 8) % 360;
  float rad = radians((float)deg);
  int x1 = cx + (int)(22.0f * cosf(rad));
  int y1 = cy + (int)(22.0f * sinf(rad));
  int x2 = cx + (int)(78.0f * cosf(rad));
  int y2 = cy + (int)(78.0f * sinf(rad));
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

  // Center Disc: solid vibrant green disc (R=22)
  display.fillCircle(cx, cy, 22, centerDiscColor);
  // Center White Satellite Icon
  drawCustomIcon(display, cx - 14, cy - 14, 0xFFFF, CustomIcon::satelliteIconLarge, 28, 28);

  // Content Below Radar: Lowered sat count row for balanced progress bar gap
  int textY = 217 - gpsScroll;
  label(20, textY, "LOCKED", 2, uiFg());
  label(20, textY + 23, "SATELLITES", 2, uiFg());
  drawBigNumber(220, textY + 1, sats, uiFg());

  // Satellite Connection Quality Progress Bar (max 6 sats for stable connection)
  int barY = 270 - gpsScroll;
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

  // Coordinates: Increased space between progressbar and lat/lon block below
  int latY = 312 - gpsScroll;
  if (latY < 280 && latY > 24) {
    centeredLabel(0, latY, 240, "LATITUDE", 1, uiMuted());
    bool hasCoord = gps.location.isValid() && gps.location.age() < 3000;
    String latStr = hasCoord ? String(gps.location.lat(), 6) : ((ride.lat != 0.0f) ? String(ride.lat, 6) : "--.------");
    centeredLabel(0, latY + 18, 240, latStr, 2, uiFg());
  }
  int lonY = 368 - gpsScroll;
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
 static void drawCenteredBigNumber(int cx, int cy, int value, uint16_t fg = 0xFFFF) {
  String s = String(value);
  int totalAdvance = 0;
  for (unsigned int k = 0; k < s.length(); k++) {
    char ch = s[k];
    if (ch >= '0' && ch <= '9') {
      totalAdvance += smoothDigitGlyphs48[ch - '0'].advance;
    }
  }
  int totalW = (totalAdvance + 8) / 16;
  int startX = cx - totalW / 2;
  int pen = startX * 16;
  int startY = cy - 17; // vertical center around cy for 34px glyph height
  for (unsigned int k = 0; k < s.length(); k++) {
    char ch = s[k];
    if (ch < '0' || ch > '9') continue;
    const auto& g = smoothDigitGlyphs48[ch - '0'];
    int gx = (pen + 8) / 16 + g.x;
    int gy = startY + g.y;
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

static void drawColoredArc(int cx, int cy, int r, float startDeg, float spanDeg, int thickness, uint16_t color) {
  if (spanDeg <= 0) return;
  if (spanDeg > 360.0f) spanDeg = 360.0f;
  float step = 0.5f;
  int steps = (int)(spanDeg / step);
  for (int i = 0; i <= steps; i++) {
    float deg = startDeg + (float)i * step;
    float rad = radians(deg);
    float c = cosf(rad);
    float s = sinf(rad);
    for (int t = 0; t < thickness; t++) {
      int px = cx + (int)lroundf((r - t) * c);
      int py = cy + (int)lroundf((r - t) * s);
      if (px >= 0 && px < DISP_W && py >= 0 && py < DISP_H) {
        display.drawPixel(px, py, color);
      }
    }
  }
}

static void drawCountdownScreen() {
  statusBar(true, false);

  const int cx = 120, cy = 135, r = 78, thicc = 5;
  const uint16_t orangeCol = 0xFA40; // Vibrant Strava Orange (#FC5200)
  const uint16_t trackBg   = darkTheme ? 0x2104 : 0xCE79; // Dark/muted base ring

  // 1. Base Ring (Full circle, thickness 5px)
  for (int t = 0; t < thicc; t++) {
    display.drawCircle(cx, cy, r - t, trackBg);
  }

  if (countdownGoStart) {
    // "GO" PHASE: Full circle in orange, big GO text!
    for (int t = 0; t < thicc; t++) {
      display.drawCircle(cx, cy, r - t, orangeCol);
    }
    centeredLabel(0, cy - 10, DISP_W, "GO", 3, 0xFFFF);

    int iconW = 28, iconH = runningMode ? 28 : 24, gap = 8;
    int textW = textWidth("GO", 3);
    int totalW = iconW + gap + textW;
    int startX = (DISP_W - totalW) / 2;
    int yBottom = 238;

    if (runningMode) drawCustomIcon(display, startX, yBottom - 2, 0xFFFF, CustomIcon::runIcon, 28, 28);
    else drawCustomIcon(display, startX, yBottom, 0xFFFF, CustomIcon::bikeIcon, 28, 24);
    label(startX + iconW + gap, yBottom + 1, "GO", 3, uiFg());

  } else {
    // COUNTDOWN PHASE
    uint32_t totalMs = (uint32_t)cfg.countdownSec * 1000UL;
    uint32_t elapsedMs = millis() - countdownStart;
    if (elapsedMs > totalMs) elapsedMs = totalMs;

    int remainingSec = (int)ceil((float)(totalMs - elapsedMs) / 1000.0f);
    if (remainingSec < 1) remainingSec = 1;

    float frac = (float)elapsedMs / (float)totalMs;
    float spanDeg = frac * 360.0f;

    // Active animated orange progress arc starting at 12 o'clock (-90 deg)
    drawColoredArc(cx, cy, r, -90.0f, spanDeg, thicc, orangeCol);

    // Big central number (Montserrat 48px anti-aliased digits)
    drawCenteredBigNumber(cx, cy, remainingSec, uiFg());

    // Below dial (Frame 18): White activity icon + "Ready" text
    int iconW = 28, iconH = runningMode ? 28 : 24, gap = 8;
    int textW = textWidth("Ready", 3);
    int totalW = iconW + gap + textW;
    int startX = (DISP_W - totalW) / 2;
    int yBottom = 238;

    if (runningMode) drawCustomIcon(display, startX, yBottom - 2, 0xFFFF, CustomIcon::runIcon, 28, 28);
    else drawCustomIcon(display, startX, yBottom, 0xFFFF, CustomIcon::bikeIcon, 28, 24);
    label(startX + iconW + gap, yBottom + 1, "Ready", 3, uiFg());
  }

  drawGlobalNav();
}

static void drawHomeClockWidget() {
  uint16_t fg = uiFg();
  time_t now = time(nullptr);
  struct tm local;
  bool timeValid = (now >= 1704067200);
  if (timeValid) {
    localtime_r(&now, &local);
  } else {
    memset(&local, 0, sizeof(local));
    uint32_t sec = millis() / 1000UL;
    local.tm_hour = (sec / 3600UL) % 24;
    local.tm_min = (sec / 60UL) % 60;
    local.tm_sec = sec % 60;
    local.tm_mday = 1;
    local.tm_mon = 0;
    local.tm_year = 124;
  }

  const char* const days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int h12 = local.tm_hour % 12;
  if (h12 == 0) h12 = 12;

  String dateStr;
  if (timeValid) {
    dateStr = String(days[local.tm_wday]) + " " + String(local.tm_mday) + " " + String(months[local.tm_mon]) + ", " + String(1900 + local.tm_year);
  } else {
    dateStr = "Waiting for time sync";
  }

  // Large 56px smooth digital clock centered at Y = 44
  drawSmoothClock56(44, h12, local.tm_min, fg);

  // Date subtitle centered at Y = 96 (size 0 = 12px smooth anti-aliased font)
  centeredLabel(0, 96, DISP_W, dateStr, 0, fg);
}

bool drawLifecycleScreen() {
  static String previous;
  String state;
  if(screenLocked) {
    if (lockScreenMode == LOCK_HERO) {
      time_t now = time(nullptr);
      struct tm l;
      localtime_r(&now, &l);
      state = "lock_hero:" + String(l.tm_hour) + ":" + String(l.tm_min) + ":" + String(uiRevision);
    } else {
      state = "lock_keypad:" + pinInput + ":" + String(pinError) + ":" + String(uiRevision);
    }
  }
  else if(shutdownDialog) state=shutdownPending?"release":"shutdown";
  else if(gpsWaiting || gpsRadarInspector) state="gpsRadar:"+String(millis()/40)+":"+String(startSatellites())+":"+String(gpsDataReady)+":"+String(gpsScroll)+":"+String(gpsRadarInspector)+":"+String(darkTheme);
  else if(countdownGoStart) state="countGo:"+String((millis()-countdownGoStart)/40);
  else if(countdownStart) state="count:"+String((millis()-countdownStart)/40);
  else if(welcomeOpen && !quickOpen) state="welcome:"+String(homePage)+":"+String(homeHeroClock)+":"+String(startSatellites())+":"+String(gpsDataReady)+":"+String(uiRevision)+":"+String(ride.gpsFixed)+":"+String(sdOk)+":"+String(millis()/500)+":"+String(bleCount())+":"+String(WiFi.status());
  if(quickOpen && !shutdownDialog && !gpsWaiting && !gpsRadarInspector && !countdownStart && !countdownGoStart){previous="";return false;}
  if(state.isEmpty()){previous="";return false;}
  if(state==previous)return true;
  previous=state;
  if(screenLocked){
    drawLockScreen();
    return true;
  }
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
  } else if(countdownStart || countdownGoStart) {
    drawCountdownScreen();
  } else {
    if(homePage!=HOME_MAIN){drawHomeSubpage();return true;}
    statusBar();
    if (homeHeroClock) {
      drawHomeClockWidget();
    } else {
      display.drawBrandLogo((DISP_W-BOOT_LOGO_WIDTH)/2,48,!darkTheme);
    }
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
  if (screenLocked) {
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
  if(gpsWaiting){
    gpsWaiting = false;
    uiRevision++;
    return;
  }
  if(gpsRadarInspector){
    return;
  }
  if(countdownStart || countdownGoStart){
    countdownStart = 0;
    countdownGoStart = 0;
    uiRevision++;
    return;
  }
  if(welcomeOpen && !quickOpen && y>=29) {
    if(isLibrary()){libraryTap(x,y);return;}
    if(homePage==HOME_SETTINGS){tapSettings(x,y);return;}
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
    if(homePage==HOME_ACTIVITY_SETTINGS){
      tapActivitySettings(x, y);
      return;
    }
    if(homePage==HOME_COMPASS){
      return;
    }
    if(homePage==HOME_MAPS){
      // SEARCH button (x=10..78, y=36..66)
      if (x >= 8 && x <= 85 && y >= 34 && y <= 68) {
        mapSearchField = 0;
        homePage = HOME_MAP_SEARCH;
        uiRevision++;
        return;
      }
      // Zoom in [+] (x=196..230, y=36..66)
      if (x >= 190 && x <= 236 && y >= 34 && y <= 68) {
        if (mapViewerZoom < 4) { mapViewerZoom++; uiRevision++; }
        return;
      }
      // Zoom out [-] (x=196..230, y=72..102)
      if (x >= 190 && x <= 236 && y >= 70 && y <= 104) {
        if (mapViewerZoom > 1) { mapViewerZoom--; uiRevision++; }
        return;
      }
      // GPS Recenter (x=196..230, y=108..138)
      if (x >= 190 && x <= 236 && y >= 106 && y <= 142) {
        if (ride.gpsFixed && isfinite(ride.lat) && isfinite(ride.lon) && ride.lat != 0.0) {
          mapViewerLat = ride.lat;
          mapViewerLon = ride.lon;
          g_mapCache.updatePosition(mapViewerLat, mapViewerLon);
          uiRevision++;
        }
        return;
      }
      return;
    }
    if(homePage==HOME_MAP_SEARCH){
      // LAT input box (y=68..94)
      if (x >= 12 && x <= 228 && y >= 68 && y <= 94) {
        mapSearchField = 0;
        uiRevision++;
        return;
      }
      // LON input box (y=98..124)
      if (x >= 12 && x <= 228 && y >= 98 && y <= 124) {
        mapSearchField = 1;
        uiRevision++;
        return;
      }
      // Keypad area (y=130..250)
      if (x >= 14 && x <= 230 && y >= 130 && y <= 250) {
        int kr = (y - 130) / 30;
        int kc = (x - 14) / 72;
        if (kr >= 0 && kr < 4 && kc >= 0 && kc < 3) {
          const char* keys[4][3] = {
            {"1", "2", "3"},
            {"4", "5", "6"},
            {"7", "8", "9"},
            {".", "0", "DEL"}
          };
          String& target = (mapSearchField == 0) ? mapSearchLat : mapSearchLon;
          if (kr == 3 && kc == 2) {
            if (target.length() > 0) target.remove(target.length() - 1);
          } else {
            if (target.length() < 12) target += keys[kr][kc];
          }
          uiRevision++;
          return;
        }
      }
      // Action row (y=254..284)
      if (y >= 252 && y <= 286) {
        // CLEAR (x=14..82)
        if (x >= 12 && x <= 84) {
          if (mapSearchField == 0) mapSearchLat = "";
          else mapSearchLon = "";
          uiRevision++;
          return;
        }
        // MY LOC (x=86..154)
        if (x >= 86 && x <= 156) {
          if (ride.gpsFixed && isfinite(ride.lat) && isfinite(ride.lon) && ride.lat != 0.0) {
            mapSearchLat = String(ride.lat, 5);
            mapSearchLon = String(ride.lon, 5);
          } else {
            mapSearchLat = "22.5946";
            mapSearchLon = "88.4258";
          }
          uiRevision++;
          return;
        }
        // SEARCH (x=158..228)
        if (x >= 158 && x <= 230) {
          double lat = mapSearchLat.toDouble();
          double lon = mapSearchLon.toDouble();
          if (isfinite(lat) && isfinite(lon) && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 && (lat != 0.0 || lon != 0.0)) {
            mapViewerLat = lat;
            mapViewerLon = lon;
            g_mapCache.updatePosition(mapViewerLat, mapViewerLon);
            homePage = HOME_MAPS;
            uiRevision++;
          }
          return;
        }
      }
      return;
    }
    if(homePage==HOME_APPS){
      if(y < 56 || y >= 290) return;
      for(int i = 0; i < 7; i++){
        int col = i % 2;
        int row = i / 2;
        int cx = (col == 0) ? 68 : 172;
        int cy = 100 + row * 108 - appsScroll;
        int dx = x - cx;
        int dy = y - cy;
        bool inCircle = (dx * dx + dy * dy <= 42 * 42);
        bool inLabel = (abs(dx) <= 56 && (y >= cy + 32 && y <= cy + 62));
        if (inCircle || inLabel) {
          if(i == 0){ // Maps
            if(ride.gpsFixed && isfinite(ride.lat) && isfinite(ride.lon) && ride.lat != 0.0){
              mapViewerLat = ride.lat;
              mapViewerLon = ride.lon;
            }
            g_mapCache.updatePosition(mapViewerLat, mapViewerLon);
            homePage = HOME_MAPS;
            uiRevision++;
          }
          else if(i == 1){ // Compass
            homePage = HOME_COMPASS;
            uiRevision++;
          }
          else if(i == 2){ // My Activities
            openLibrary(HOME_ACTIVITY);
          }
          else if(i == 3){ // Past Routes
            openLibrary(HOME_ROUTES);
          }
          else if(i == 4){ // Settings
            homePage = HOME_SETTINGS;
            settingsPage = SET_MAIN;
            settingsScroll = 0;
            uiRevision++;
          }
          else if(i == 5){ // Activity Settings
            homePage = HOME_ACTIVITY_SETTINGS;
            uiRevision++;
          }
          else if(i == 6){ // My Files
            openBrowser("/");
          }
          return;
        }
      }
      return;
    }
    if (y >= 28 && y < 140) {
      homeHeroClock = !homeHeroClock;
      saveHomeHeroPref();
      uiRevision++;
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
      if(x>=6 && x<117){homePage=HOME_APPS;appsScroll=0;uiRevision++;}
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
    if (currentScreen == SCR_CONTROLS) {
      if (activityEndConfirm) {
        if (y >= 150 && y <= 210) {
          if (x >= 24 && x <= 118) {
            activityEndConfirm = false;
            saveActivity();
            ridePaused = false;
            welcomeOpen = true;
            homePage = HOME_MAIN;
            currentScreen = SCR_DASH;
            uiRevision++;
            return;
          } else if (x >= 122 && x <= 216) {
            activityEndConfirm = false;
            uiRevision++;
            return;
          }
        }
        activityEndConfirm = false;
        uiRevision++;
        return;
      }

      if (y >= 106 && y < 165 && x >= 6 && x < 234) {
        if (!rideStarted) requestRideStart();
        else toggleRide();
        uiRevision++;
        return;
      }

      if (y >= 170 && y < 228 && x >= 6 && x < 234) {
        if (rideStarted) {
          activityEndConfirm = true;
          uiRevision++;
        }
        return;
      }

      if (y >= 234 && y < 285 && x >= 6 && x < 234) {
        runningMode = !runningMode;
        saveActivityConfig();
        uiRevision++;
        return;
      }
      return;
    }
    if (currentScreen == SCR_MAP) {
      // Tap on Top-Right Compass Coin (cx = 202, cy = 54) to toggle North-Up vs Course-Up
      if (x >= 180 && x <= 226 && y >= 34 && y <= 76) {
        activityMapNorthUp = !activityMapNorthUp;
        saveMapNorthUpPref();
        uiRevision++;
        return;
      }
      if (y > 240) {
        mapZoom = constrain((int)mapZoom + (x >= 120 ? 1 : -1), 1, 5);
        uiRevision++;
        return;
      }
      return;
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
  if(y>=107 && y<155 && x>=20 && x<234){setBrightnessFromTouch(x);return;}
  if (x>=6 && x<234 && y>=32 && y<101) {
    if (x >= 174) {
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
  int row = y>=161 && y<223 ? 0 : (y>=229 && y<291 ? 1 :
            (y>=297 && y<359 ? 2 : (y>=365 && y<427 ? 3 : -1)));
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
  auto release = [&]() {
    if (screenLocked) {
      if (rootGesture && lockScreenMode == LOCK_HERO) {
        // Swipe up: moved upward by >= 25 px
        if (startY - endY >= 25) {
          if (screenLockEnabled) {
            lockScreenMode = LOCK_KEYPAD;
            pinInput = "";
            pinError = false;
          } else {
            screenLocked = false;
          }
          uiRevision++;
        }
        // Tap on circular lock button (cx = 120, cy = 246, radius = 36)
        else {
          int dx = endX - 120, dy = endY - 246;
          if (dx * dx + dy * dy <= 36 * 36) {
            if (screenLockEnabled) {
              lockScreenMode = LOCK_KEYPAD;
              pinInput = "";
              pinError = false;
            } else {
              screenLocked = false;
            }
            uiRevision++;
          }
        }
      }
      rootGesture = false; sliderGesture = false; mapPanGesture = false; held = false; candidateX = -1;
      return;
    }
    if(rootGesture && !sliderGesture) {
      if (welcomeOpen && !quickOpen && homePage == HOME_MAIN) {
        int dx = endX - startX;
        int dy = endY - startY;
        if (startY < 140) {
          if (abs(dx) >= 20 && abs(dx) > abs(dy)) {
            homeHeroClock = !homeHeroClock;
            saveHomeHeroPref();
            uiRevision++;
          } else if (!moved) {
            homeHeroClock = !homeHeroClock;
            saveHomeHeroPref();
            uiRevision++;
          }
        } else {
          if (abs(dx) >= 35 && abs(dx) > abs(dy) * 1.5) {
            homeHeroClock = !homeHeroClock;
            saveHomeHeroPref();
            uiRevision++;
          } else if (!moved || (abs(dx) < 20 && abs(dy) < 20)) {
            quickTap(startX, startY);
          }
        }
      } else if (!moved) {
        quickTap(endX, endY);
      }
    }
    rootGesture=false;sliderGesture=false;mapPanGesture=false;held=false;candidateX=-1;
  };
  if (!touchReady || millis() - poll < 25) return;
  poll = millis();

  // Auto-reset PIN error dots after 1.5 seconds
  if (screenLocked && lockScreenMode == LOCK_KEYPAD && pinError) {
    if (millis() - pinErrorMs >= 1500) {
      pinError = false;
      uiRevision++;
    }
  }

  // Auto Screen Timeout (inhibited when an activity is actively ongoing)
  if (screenTimeoutSec > 0 && !rideStarted && !screenBlanked) {
    if (lastTouchActivityMs == 0) lastTouchActivityMs = millis();
    if (millis() - lastTouchActivityMs >= (uint32_t)screenTimeoutSec * 1000UL) {
      screenBlanked = true;
      analogWrite(5, 0); // Turn off LCD backlight
      if (screenLockEnabled) {
        screenLocked = true;
        lockScreenMode = LOCK_HERO;
      } else {
        screenLocked = false;
      }
      uiRevision++;
    }
  }
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
  lastTouchActivityMs = millis();
  if (screenBlanked) {
    screenBlanked = false;
    analogWrite(5, (brightnessPercent * 255 + 50) / 100);
    if (screenLockEnabled) {
      screenLocked = true;
      lockScreenMode = LOCK_HERO;
    } else {
      screenLocked = false;
    }
    uiRevision++;
    return;
  }
  if(rootGesture){
    endX=x;endY=y;
    if(abs(y-startY)>7 || abs(x-startX)>7)moved=true;
    if(sliderGesture)setBrightnessFromTouch(x);
    else if(moved){
      if(gpsWaiting || gpsRadarInspector){
        int offset=constrain(startScroll+startY-y,0,128);
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
      } else if(welcomeOpen && !quickOpen && homePage == HOME_MAPS){
        if (mapPanGesture) {
          float scale = (mapViewerZoom == 1) ? 0.15f : (mapViewerZoom == 2) ? 0.27f : (mapViewerZoom == 3) ? 0.48f : 0.85f;
          double co = cos(radians(startPanLat));
          double dXm = (x - startX) / max(0.0001f, scale);
          double dYm = (y - startY) / max(0.0001f, scale);
          mapViewerLon = startPanLon - (dXm / max(0.0001, co * 111320.0));
          mapViewerLat = startPanLat + (dYm / 111320.0);
          g_mapCache.updatePosition(mapViewerLat, mapViewerLon);
          uiRevision++;
        }
      } else if(welcomeOpen && !quickOpen && homePage == HOME_APPS){
        int offset=constrain(startScroll+startY-y,0,MAX_APPS_SCROLL);
        if(offset!=appsScroll){appsScroll=offset;uiRevision++;}
      } else if(welcomeOpen && !quickOpen && homePage == HOME_SETTINGS && settingsPage == SET_MAIN){
        int offset=constrain(startScroll+startY-y,0,88);
        if(offset!=settingsScroll){settingsScroll=offset;uiRevision++;}
      } else if (quickOpen) {
        int offset=constrain(startScroll+startY-y,0,140);
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
    if (screenLocked) {
      if (lockScreenMode == LOCK_KEYPAD) {
        rootGesture = false;
        tapLockScreen(x, y);
        return;
      }
      rootGesture = true;
      moved = false;
      startX = endX = x;
      startY = endY = y;
      sliderGesture = false;
      return;
    } else if(quickOpen && !quickDetail && y>=29 && y<290){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;startScroll=quickScroll;
      sliderGesture=y+quickScroll>=107 && y+quickScroll<155 && x>=36 && x<225;
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
    } else if(welcomeOpen && !quickOpen && homePage == HOME_MAPS && y >= 28 && y < 290){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;
      sliderGesture=false;
      bool inButtons = (x >= 8 && x <= 85 && y >= 34 && y <= 68) || (x >= 190 && y >= 34 && y <= 145);
      mapPanGesture = !inButtons;
      if (mapPanGesture) {
        startPanLat = mapViewerLat;
        startPanLon = mapViewerLon;
      }
    } else if(welcomeOpen && !quickOpen && homePage == HOME_APPS && y >= 28 && y < 290){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;startScroll=appsScroll;
      sliderGesture=false;
    } else if(welcomeOpen && !quickOpen && homePage == HOME_SETTINGS && settingsPage == SET_MAIN && y >= 54 && y < 286){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;startScroll=settingsScroll;
      sliderGesture=false;
    } else if(welcomeOpen && !quickOpen && homePage == HOME_MAIN && y >= 28 && y < 290){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;
      sliderGesture=false;
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
    drawCustomIcon(display,14,43,uiFg(),headingIcons[quickDetail],20,20);
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
      label(16, 110, String(startSatellites()) + " satellites reported");
      label(16, 136, "Lat: " + String(ride.lat,6));
      label(16, 154, "Lon: " + String(ride.lon,6));
      label(16, 178, String(ride.speedKmh,1)+" km/h",2);
      label(16, 206, String(ride.distanceKm,3)+" km recorded");
      label(16, 228, gpsDataReady?"GPS data received":"GPS hardware not ready");
      label(16, 248, sdOk?activityMessage:"SD not ready");
    }
    if (quickDetail == 5) {
      const char* names[] = {"Touch CST3530", "GPS u-blox M10", "Compass QMC5883L", "BMP390 Baro", "Buttons Expander", "SD Card FAT32"};
      bool ok[] = {touchReady, gpsDataReady, compass.ready && (compass.lastSample == 0 || (millis() - compass.lastSample < 3000)), bmpOk, buttonsOk, sdOk};
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
  drawCustomIcon(display, 16, 40-offset, 0xFFFF, runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon, 20, 20);
  label(42, 46-offset, runningMode ? "OUTDOOR RUNNING" : "OUTDOOR CYCLING", 1, 0xFFFF);
  label(16, 68-offset, durationText(), 3, 0xFFFF);

  // Big White Pause/Play Button on right of banner (shifted inward towards left)
  display.fillCircle(196, 66-offset, 18, 0xFFFF);
  if (rideStarted && !ridePaused) {
    display.fillRect(191, 58-offset, 4, 16, 0xF800);
    display.fillRect(198, 58-offset, 4, 16, 0xF800);
  } else {
    display.fillTriangle(192, 58-offset, 192, 74-offset, 205, 66-offset, 0xF800);
  }

  // Brightness slider: shifted towards brightness icon to left, larger right gutter for visual centering
  card(6, 107-offset, 228, 48, uiCard());
  drawCustomIcon(display, 18, 122-offset, uiFg(), CustomIcon::brightnessIcon, 18, 18);
  display.fillRoundRect(46, 128-offset, 160, 6, 3, uiMuted());
  int thumb = 46 + (brightnessPercent - 5) * 160 / 95;
  display.fillRoundRect(46, 128-offset, max(4, thumb - 46), 6, 3, TEAL);
  display.fillCircle(thumb, 131-offset, 7, uiFg());

  // 2x4 Grid of Tiles with consistent 6px horizontal & vertical gutters
  // Tile 0: Wi-Fi
  {
    int x = 6, y = 161-offset;
    card(x, y, 111, 62, uiCard());
    drawCustomIcon(display, x + 45, y + 14, uiFg(), CustomIcon::hotSpotIcon, 20, 20);
    bool wifiOn = (WiFi.getMode() != WIFI_OFF);
    centeredLabel(x, y + 42, 111, wifiOn ? "HOTSPOT ACTIVE" : "NOT CONNECTED", 1, wifiOn ? 0x2564 : 0xFA08);
  }

  // Tile 1: Bluetooth
  {
    int x = 123, y = 161-offset;
    int bCount = bleCount() + (blePhoneConnected() ? 1 : 0);
    bool active = (bCount > 0);
    card(x, y, 111, 62, active ? (darkTheme ? 0x0940 : 0xD753) : uiCard());
    uint16_t fg = active ? (darkTheme ? 0x7FE0 : 0x2400) : uiFg();
    drawBluetoothStateIcon(display, x + 36, y + 14, fg, active, 20, 20);
    label(x + 62, y + 16, String(bCount), 2, fg);
    centeredLabel(x, y + 42, 111, String(bCount) + " CONNECTED", 1, fg);
  }

  // Tile 2: GPS
  {
    int x = 6, y = 229-offset;
    bool locked = ride.gpsFixed;
    card(x, y, 111, 62, locked ? (darkTheme ? 0x0940 : 0xD753) : uiCard());
    uint16_t fg = locked ? (darkTheme ? 0x7FE0 : 0x2400) : uiFg();
    drawSatelliteIcon(display, x + 36, y + 14, fg, 20, 20);
    label(x + 62, y + 16, String(startSatellites()), 2, fg);
    centeredLabel(x, y + 42, 111, locked ? "SAT LOCKED" : "SEARCHING", 1, fg);
  }

  // Tile 3: Theme Toggle
  {
    int x = 123, y = 229-offset;
    card(x, y, 111, 62, uiCard());
    drawCustomIcon(display, x + 45, y + 14, uiFg(), CustomIcon::themeSwitcherIcon, 20, 20);
    centeredLabel(x, y + 42, 111, darkTheme ? "DARK MODE" : "LIGHT MODE", 1, uiFg());
  }

  // Tile 4: System Diagnostics
  {
    int x = 6, y = 297-offset;
    card(x, y, 111, 62, uiCard());
    drawCustomIcon(display, x + 45, y + 14, uiFg(), CustomIcon::systemIcon, 20, 20);
    centeredLabel(x, y + 42, 111, "SYSTEM", 1, uiFg());
  }

  // Tile 5: Configuration
  {
    int x = 123, y = 297-offset;
    card(x, y, 111, 62, uiCard());
    drawCustomIcon(display, x + 45, y + 14, uiFg(), CustomIcon::configureIcon, 20, 20);
    centeredLabel(x, y + 42, 111, "CONFIGURE", 1, uiFg());
  }

  // Tile 6: Lock Screen
  {
    int x = 6, y = 365-offset;
    card(x, y, 111, 62, uiCard());
    drawCustomIcon(display, x + 45, y + 14, uiFg(), CustomIcon::lockIcon, 20, 20);
    centeredLabel(x, y + 42, 111, "LOCK SCREEN", 1, uiFg());
  }

  // Tile 7: Power Off
  {
    int x = 123, y = 365-offset;
    card(x, y, 111, 62, uiCard());
    drawCustomIcon(display, x + 45, y + 14, 0xF800, CustomIcon::powerIcon, 20, 20);
    centeredLabel(x, y + 42, 111, "POWER OFF", 1, 0xF800);
  }
  // Scrolled content can overlap the header. Repaint only its strip,
  // not the whole page: a full clear here erases every settings tile.
  statusBar(false, false);
  drawCustomIcon(display,108,12,0x8430,CustomIcon::downArrowIcon);
  // Small position indicator; all settings remain reachable above the fixed tray.
  display.fillRoundRect(236,32+quickScroll*174/140,3,40,1,uiMuted());

}
