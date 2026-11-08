#pragma once
#include "smooth_digits48.h"
// CST3530 protocol follows Waveshare's esp_lcd_touch_cst3530 driver.
// https://github.com/waveshareteam/Waveshare-ESP32-components
TwoWire touchBus(1);
bool touchReady = false, quickOpen = false, ridePaused = false;
extern bool autoPauseEnabled, autoPaused;
extern uint32_t autoPauseStartMs, nearZeroSpeedStartMs, autoPauseOverrideGraceMs, autoResumeMovementStartMs;
extern float autoPauseAnchorLat, autoPauseAnchorLon;
extern int controlSliderOffset;
extern bool controlSliderDragging;
extern int controlSliderStartX;
uint32_t pauseStart = 0;
int quickDetail = 0;
int quickScroll=0;
int brightnessPercent=100;
static void setBrightnessFromTouch(int x){
  brightnessPercent=constrain(5+(x-56)*95/146,5,100);
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
enum HomePage { HOME_MAIN, HOME_APPS, HOME_FILES, HOME_ACTIVITY, HOME_RIDES, HOME_ROUTES, HOME_SETTINGS, HOME_ACTIVITY_SETTINGS, HOME_COMPASS, HOME_MAPS, HOME_MAP_SEARCH, HOME_SENSORS };
HomePage homePage=HOME_MAIN;
bool homeHeroClock = false;
static int homeSliderOffset = 0;
static bool homeSliderDragging = false;
static int homeSliderStartX = 0;
bool activityMapNorthUp = false;
extern bool appsListView;
extern float distAnchorLat, distAnchorLon;
int appsScroll = 0;
const int MAX_APPS_SCROLL = 200;
static inline int getAppsMaxScroll() {
  return appsListView ? 268 : 200;
}
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
static void getAppLoadingMeta(HomePage page, CustomIcon &icon, uint16_t &color) {
  switch (page) {
    case HOME_MAPS:
    case HOME_MAP_SEARCH:
      icon = CustomIcon::mapAppIconLarge; color = 0x33FB; break;
    case HOME_COMPASS:
      icon = CustomIcon::compassIconLarge; color = 0xE800; break;
    case HOME_SENSORS:
      icon = CustomIcon::heartRateIcon; color = 0x051F; break;
    case HOME_ACTIVITY:
    case HOME_RIDES:
      icon = CustomIcon::activitiesIconLarge; color = 0xFA61; break;
    case HOME_ROUTES:
      icon = CustomIcon::routeMapIconLarge; color = 0x5444; break;
    case HOME_SETTINGS:
      icon = CustomIcon::systemIconLarge; color = 0x79F7; break;
    case HOME_ACTIVITY_SETTINGS:
      icon = CustomIcon::configureIconLarge; color = 0xDBC0; break;
    case HOME_FILES:
      icon = CustomIcon::folderIconLarge; color = 0x0CA1; break;
    default:
      icon = CustomIcon::activitiesIconLarge; color = 0xFA61; break;
  }
}

static void drawLoadingScreen(const char* title = "Loading asset ...", const char* subtitle = "Please wait!", HomePage targetPage = HOME_MAIN) {
  statusBar(true, true);
  display.fillRect(0, 28, 240, 292, uiBg());

  CustomIcon appIcon;
  uint16_t appColor;
  HomePage p = (targetPage != HOME_MAIN) ? targetPage : homePage;
  getAppLoadingMeta(p, appIcon, appColor);

  const int cx = 120, cy = 135;
  const int circleR = 34;

  // Circular container of that app color in the screen center
  display.fillCircle(cx, cy, circleR, appColor);

  // White app icon in the center of the circular container
  drawCustomIcon(display, cx - 18, cy - 18, 0xFFFF, appIcon, 36, 36);

  // Display specific app loading title (font size 2 = 18pt smooth font)
  centeredLabel(10, 186, 220, (title && strlen(title) > 0) ? title : "Loading ...", 2, uiFg());
  if (subtitle && strlen(subtitle) > 0) {
    centeredLabel(10, 212, 220, subtitle, 1, uiMuted());
  }

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
  if(n>=1073741824ULL)return String((double)n/1073741824.0,1)+"GB";
  if(n>=1048576)return String((double)n/1048576.0,1)+"MB";
  if(n>=1024)return String((unsigned long)(n/1024))+"KB";
  return String((unsigned long)n)+"B";
}

static int countFilesInDir(const String& path) {
  if (!sdOk) return 0;
  File dir = SD_MMC.open(path, FILE_READ);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return 0; }
  int count = 0;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      count++;
    }
    entry.close();
  }
  dir.close();
  return count;
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
  bool isRoot = (browserPath == "/");
  int perPage = isRoot ? 4 : 3;
  int skip = browserPage * perPage;
  int index = 0;
  while(true){
    File entry=dir.openNextFile();if(!entry)break;
    if(index++<skip){entry.close();continue;}
    if(browserCount==perPage){browserMore=true;entry.close();break;}
    String name=entry.name();int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);
    browserNames[browserCount]=name;browserDirs[browserCount]=entry.isDirectory();
    browserSizes[browserCount]=entry.size();browserCount++;entry.close();
  }
  dir.close();
}
static void openBrowser(String path){homePage=HOME_FILES;drawLoadingScreen("Loading Files...", "Please wait!", HOME_FILES);browserPath=path;browserPage=0;fileDetailOpen=false;fileDeleteConfirm=false;loadBrowser();uiRevision++;}

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
  statusBar();

  // Icon & Type Header
  display.fillRoundRect(8, 36, 224, 46, 14, uiCard());
  if(fileDetailIsFit){
    drawCustomIcon(display, 20, 45, TEAL, CustomIcon::runIconLarge, 28, 28);
  } else if(fileDetailIsGpx){
    drawCustomIcon(display, 20, 45, 0x3DF7, CustomIcon::routeMapIconLarge, 28, 28);
  } else {
    drawCustomIcon(display, 20, 45, uiFg(), CustomIcon::sdCardIcon, 28, 28);
  }

  label(58, 42, fileDetailIsFit ? "FIT ACTIVITY" : fileDetailIsGpx ? "GPX ROUTE" : "SD FILE", 2, uiFg());
  label(58, 62, fileDetailIsFit ? "Recorded Activity Data" : fileDetailIsGpx ? "GPS Route Trackpoints" : "Storage File", 0, uiMuted());

  // Full file name card (multi-line wrapped)
  display.fillRoundRect(8, 88, 224, 50, 14, uiCard());
  label(16, 94, "FILE NAME", 0, TEAL);
  if(fileDetailName.length() <= 32){
    label(16, 112, fileDetailName, 1, uiFg());
  } else {
    int splitPos = 28;
    for(int i = 28; i >= 16; i--){
      if(fileDetailName[i] == '_' || fileDetailName[i] == ' ' || fileDetailName[i] == '-'){
        splitPos = i + 1;
        break;
      }
    }
    String line1 = fileDetailName.substring(0, splitPos);
    String line2 = fileDetailName.substring(splitPos);
    label(16, 108, line1, 1, uiFg());
    label(16, 122, fitBrowserText(line2, 208), 1, uiFg());
  }

  // Metadata Card: Date and Size
  display.fillRoundRect(8, 144, 224, 56, 14, uiCard());
  label(16, 150, "DATE:", 0, uiMuted());
  label(58, 150, fileDetailDate, 0, uiFg());

  label(16, 166, "SIZE:", 0, uiMuted());
  label(58, 166, storageSize(fileDetailSize) + " (" + String((unsigned long)fileDetailSize) + " B)", 0, uiFg());

  label(16, 182, "PATH:", 0, uiMuted());
  label(58, 182, fitBrowserText(fileDetailPath, 160), 0, uiMuted());

  // Delete Action / Confirmation
  if(!fileDeleteConfirm){
    display.fillRoundRect(8, 206, 224, 40, 20, 0xF800);
    centeredLabel(8, 218, 224, "DELETE FILE", 2, WHITE);
  } else {
    display.fillRoundRect(8, 206, 108, 40, 20, 0x4208);
    centeredLabel(8, 218, 108, "CANCEL", 1, WHITE);

    display.fillRoundRect(124, 206, 108, 40, 20, 0xF800);
    centeredLabel(124, 218, 108, "CONFIRM", 1, WHITE);

    centeredLabel(8, 252, 224, "Delete permanently from SD?", 0, 0xFA08);
  }
}

static void requestRideStart();
static void drawGlobalNav();
#include "route_archive.h"
int archiveScroll=0;
const int ARCHIVE_MAX_SCROLL=280;
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
static void openLibrary(HomePage page){homePage=page;const char* t=(page==HOME_ROUTES)?"Loading Routes...":"Loading Activities...";drawLoadingScreen(t, "Please wait!", page);libraryPage=0;loadLibrary();uiRevision++;}
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
        float showGain = (archiveSummary["elevationGainM"].is<float>() && archiveSummary["elevationGainM"].as<float>() > 0.0f)
                     ? archiveSummary["elevationGainM"].as<float>()
                     : previewGain;
        float grad = (previewKm > 0.01f && previewElevation) ? ((showGain / (previewKm * 1000.0f)) * 100.0f) : 0.0f;
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

// Scrollbar removed per user design request

      statusBar(true, false);
      drawGlobalNav();
      return;
    }

    // Past Activity / Ride History: Full vertically scrollable activity report
    int topY = 70 - archiveScroll;

    // Determine Activity Title & sport type
    int slash = librarySelection.lastIndexOf('/');
    String fn = (slash >= 0) ? librarySelection.substring(slash + 1) : librarySelection;
    fn.replace(".fit", ""); fn.replace(".gpx", ""); fn.replace(".csv", ""); fn.replace(".json", "");
    bool isRun = runningMode || fn.indexOf("run") >= 0;
    String titleStr;
    if (archiveSummary["title"].is<const char*>() && strlen(archiveSummary["title"].as<const char*>()) > 0) {
      titleStr = String(archiveSummary["title"].as<const char*>());
    } else if (fn.startsWith("ride-") || fn.startsWith("run-") || fn.startsWith("activity-") || fn.startsWith("activity_") || fn.length() == 0) {
      uint32_t ep = archiveSummary["completedEpoch"] | 0;
      String tod = "Evening";
      if (ep > 0) {
        time_t t = (time_t)ep; struct tm l; localtime_r(&t, &l);
        int h = l.tm_hour;
        if (h >= 5 && h < 12) tod = "Morning";
        else if (h >= 12 && h < 17) tod = "Afternoon";
        else if (h >= 17 && h < 21) tod = "Evening";
        else tod = "Night";
      }
      titleStr = tod + (isRun ? " Run" : " Ride");
    } else {
      fn.replace("_", " ");
      titleStr = truncateHeading(fn, 16);
    }

    uint32_t sec = archiveSummary["seconds"].is<uint32_t>() ? archiveSummary["seconds"].as<uint32_t>() : (archiveSummary["seconds"].is<float>() ? (uint32_t)archiveSummary["seconds"].as<float>() : 0);

    // --- ROW 1: Square Map (Left) + Stacked Distance & Duration (Right) ---
    // 1A. Square Map Card (108x104)
    drawActivitySquareMap(8, topY, 108, 104);

    // 1B. Distance Card (108x48)
    if (topY + 48 >= 68 && topY < 290) {
      display.fillRoundRect(124, topY, 108, 48, 24, uiCard());
      if (!darkTheme) display.drawRoundRect(124, topY, 108, 48, 24, 0xD69A);
      float showDist = (archiveSummary["distanceKm"].is<float>() && archiveSummary["distanceKm"].as<float>() > 0.001f)
                     ? archiveSummary["distanceKm"].as<float>()
                     : previewKm;
      String distStr = (showDist > 0.001f) ? (String(showDist, 2) + "KM") : "--KM";
      centeredLabel(124, topY + 16, 108, distStr, 2, uiFg());
    }

    // 1C. Duration Card (108x48)
    int durY = topY + 56;
    if (durY + 48 >= 68 && durY < 290) {
      display.fillRoundRect(124, durY, 108, 48, 24, uiCard());
      if (!darkTheme) display.drawRoundRect(124, durY, 108, 48, 24, 0xD69A);
      String timeStr;
      if (sec >= 3600) timeStr = String(sec / 3600) + "hr " + String((sec % 3600) / 60) + "m";
      else if (sec > 0) timeStr = String(sec / 60) + "m " + String(sec % 60) + "s";
      else timeStr = "--";
      centeredLabel(124, durY + 16, 108, timeStr, 2, uiFg());
    }

    // --- ROW 2: Speed (Left) & Heart Rate (Right) ---
    int row2Y = topY + 112;
    // 2A. Speed Card (108x68) - Exact uniform 24x24 icon size
    if (row2Y + 68 >= 68 && row2Y < 290) {
      display.fillRoundRect(8, row2Y, 108, 68, 10, uiCard());
      if (!darkTheme) display.drawRoundRect(8, row2Y, 108, 68, 10, 0xD69A);
      const uint16_t speedCol = darkTheme ? 0x475F : 0x03D4; // High-contrast deep cyan in light theme, neon cyan in dark theme
      drawCustomIcon(display, 8 + (108 - 24) / 2, row2Y + 9, speedCol, CustomIcon::speedometerIcon, 24, 24);
      float avgSpd = archiveSummary["avgSpeedKmh"].is<float>() ? archiveSummary["avgSpeedKmh"].as<float>() : ((sec > 0) ? (previewKm / (sec / 3600.0f)) : 0);
      String spdStr = (avgSpd > 0) ? (String(avgSpd, 1) + "KMPH") : "--KMPH";
      centeredLabel(8, row2Y + 44, 108, spdStr, 2, uiFg());
    }

    // 2B. Heart Rate Card (108x68) - Exact uniform 24x24 icon size
    if (row2Y + 68 >= 68 && row2Y < 290) {
      display.fillRoundRect(124, row2Y, 108, 68, 10, uiCard());
      if (!darkTheme) display.drawRoundRect(124, row2Y, 108, 68, 10, 0xD69A);
      float avgHr = archiveSummary["avgHeartRate"].is<float>() ? archiveSummary["avgHeartRate"].as<float>() : 0;
      int hrVal = (int)round(avgHr);
      if (hrVal > 0) {
        int zone = 1;
        if (hrVal < 120) zone = 1;
        else if (hrVal < 140) zone = 2;
        else if (hrVal < 160) zone = 3;
        else if (hrVal < 180) zone = 4;
        else zone = 5;
        // Red heart icon (uniform 24x24) + Zone number (size 2), center aligned together
        String zoneStr = String(zone);
        int zw = textWidth(zoneStr, 2);
        int totalTopW = 24 + 6 + zw;
        int hrStartX = 124 + (108 - totalTopW) / 2;
        drawCustomIcon(display, hrStartX, row2Y + 9, 0xF800, CustomIcon::heartRateIcon, 24, 24);
        label(hrStartX + 24 + 6, row2Y + 12, zoneStr, 2, uiFg());
        centeredLabel(124, row2Y + 44, 108, String(hrVal) + " BPM", 2, uiFg());
      } else {
        // Neutral fallback when no heart rate reading exists (stop showing zone 3)
        drawCustomIcon(display, 124 + (108 - 24) / 2, row2Y + 9, uiMuted(), CustomIcon::heartRateIcon, 24, 24);
        centeredLabel(124, row2Y + 44, 108, "-- BPM", 2, uiMuted());
      }
    }

    // --- ROW 3: Elevation Gain (Left) & Calories (Right) ---
    int row3Y = topY + 188;
    // 3A. Elevation Gain Card (108x68) - Exact uniform 24x24 icon size
    if (row3Y + 68 >= 68 && row3Y < 290) {
      display.fillRoundRect(8, row3Y, 108, 68, 10, uiCard());
      if (!darkTheme) display.drawRoundRect(8, row3Y, 108, 68, 10, 0xD69A);
      drawCustomIcon(display, 8 + (108 - 24) / 2, row3Y + 9, 0xFDC0, CustomIcon::elevationIcon, 24, 24);
      String elevStr = previewElevation ? (String((int)previewGain) + "M") : "--M";
      centeredLabel(8, row3Y + 44, 108, elevStr, 2, uiFg());
    }

    // 3B. Calories Card (108x68) - Exact uniform 24x24 icon size
    if (row3Y + 68 >= 68 && row3Y < 290) {
      display.fillRoundRect(124, row3Y, 108, 68, 10, uiCard());
      if (!darkTheme) display.drawRoundRect(124, row3Y, 108, 68, 10, 0xD69A);
      drawCustomIcon(display, 124 + (108 - 24) / 2, row3Y + 9, 0xFA80, CustomIcon::caloriesIcon, 24, 24);
      float cal = archiveSummary["caloriesEstimate"].is<float>() ? archiveSummary["caloriesEstimate"].as<float>() : 0;
      String calStr = (cal > 0) ? (String((int)cal) + " KCAL") : "-- KCAL";
      centeredLabel(124, row3Y + 44, 108, calStr, 2, uiFg());
    }

    // --- ROW 4: Heart Rate Graph Card (224x82) ---
    int hrY = topY + 264;
    drawActivityGraph(8, hrY, 224, 82, 1);

    // --- ROW 5: Speed Graph Card (224x82) ---
    int spdY = topY + 354;
    drawActivityGraph(8, spdY, 224, 82, 2);

    // --- ROW 6: Sync to Strava Button (224x48) ---
    int btnY = topY + 444;
    drawStravaSyncButton(8, btnY, 224, 48);

    // Fixed Top Header & Status Bar (repainted so scrolled cards never overlap)
    display.fillRect(0, 0, 240, 68, uiBg());
    statusBar(true, false);
    // Square 32x32 icon, center aligned with the 2-line title & date text beside it
    drawCustomIcon(display, 14, 32, uiFg(), isRun ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 32, 32);
    label(52, 32, titleStr, 2, uiFg());
    label(52, 49, completedText, 0, uiMuted());

    // Fixed Bottom Navigation Bar
    display.fillRect(0, 290, 240, 30, uiBg());
    drawGlobalNav();

// Scrollbar removed per user design request
    return;
  }

  statusBar();

  if(!libraryCount){
    display.fillRoundRect(8, 38, 224, 54, 27, uiCard());
    centeredLabel(8, 56, 224, libraryMessage.length() ? libraryMessage : (routes ? "No saved routes" : "No saved activities"), 1, uiMuted());
  }
  for(int i = 0; i < libraryCount; i++){
    int y = 38 + i * 64;
    display.fillRoundRect(8, y, 224, 54, 27, uiCard());
    bool isRun = libraryNames[i].startsWith("run-") || libraryNames[i].indexOf("run") >= 0;
    CustomIcon icon = routes ? CustomIcon::routeMapIconLarge : (isRun ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge);
    drawCustomIcon(display, 22, y + 13, uiFg(), icon, 28, 28);

    String title = libraryNames[i];
    title.replace(".fit", ""); title.replace(".json", ""); title.replace(".gpx", ""); title.replace("_", " ");
    if(title.length() > 14) title = truncateHeading(title, 14);
    label(62, y + 19, title, 2, uiFg());

    String szStr = storageSize(librarySizes[i]);
    int sw = textWidth(szStr, 1);
    label(220 - sw, y + 21, szStr, 1, uiMuted());
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
        drawLoadingScreen("Starting Route...", "Please wait!", HOME_ROUTES);
        routeLen=previewLen;for(int i=0;i<routeLen;i++)route[i]={float(preview[i].lat),float(preview[i].lon)};
        nav.routeIndex=0;nav.hasDestination=true;nav.destLat=route[routeLen-1].lat;nav.destLon=route[routeLen-1].lon;
        snapActiveRoute();
        currentScreen=SCR_MAP;requestRideStart();
      }
      return;
    } else {
      // Past Activity / Ride History Details
      int topY = 70 - archiveScroll;
      int mapY = topY;
      if (x >= 8 && x <= 116 && y >= mapY && y < mapY + 104) {
        String gpxPath = companion(librarySelection, ".gpx");
        if (!SD_MMC.exists(gpxPath)) {
          int slash = gpxPath.lastIndexOf('/');
          String fn = (slash >= 0) ? gpxPath.substring(slash + 1) : gpxPath;
          if (SD_MMC.exists("/activities/" + fn)) gpxPath = "/activities/" + fn;
          else if (SD_MMC.exists("/routes/" + fn)) gpxPath = "/routes/" + fn;
          else if (SD_MMC.exists("/" + fn)) gpxPath = "/" + fn;
        }
        if (SD_MMC.exists(gpxPath) || previewLen >= 2) {
          drawLoadingScreen("Loading Route...", "Please wait!", HOME_ROUTES);
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
  int row = (y - 38) / 64;
  if(y >= 38 && row >= 0 && row < libraryCount && (y - 38) % 64 < 54 && x >= 8 && x <= 232){
    bool routes = (homePage == HOME_ROUTES);
    drawLoadingScreen(routes ? "Loading Route..." : "Loading Activity...", "Please wait!", homePage);
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
static bool isHomeScreen() {
  return (welcomeOpen && homePage == HOME_MAIN && !quickOpen && !shutdownDialog && !gpsWaiting && !gpsRadarInspector && !countdownStart && !countdownGoStart && !quickDetail);
}

static bool globalHasBack(){
  if(screenLocked) return false;
  if(shutdownDialog || gpsWaiting || gpsRadarInspector || countdownStart || countdownGoStart || quickOpen) return true;
  if(!welcomeOpen) return true;
  if(homePage != HOME_MAIN) return true;
  return false;
}
static void drawGlobalNav(){
  if(screenLocked || isHomeScreen()) return;
  display.fillRect(0,290,240,30,uiBg());
  if(globalHasBack())drawCustomIcon(display,42,297,uiFg(),CustomIcon::GoBackIcon);
  drawCustomIcon(display,112,297,uiFg(),CustomIcon::HomeIcon);
  if(globalHasNext())drawCustomIcon(display,182,297,uiFg(),CustomIcon::goNextIcon);
}
static int sensorsScroll = 0;
static int sensorsGuideSubpage = 0;
static uint32_t sensorsScanStartMs = 0;

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
    else if(homePage == HOME_SENSORS) {
      if (sensorsGuideSubpage > 0) {
        sensorsGuideSubpage = 0;
      } else {
        homePage = HOME_APPS;
      }
      uiRevision++;
    }
    else if(homePage == HOME_MAP_SEARCH) {
      homePage = HOME_MAPS;
    }
    else if(homePage == HOME_APPS) {
      homePage = HOME_MAIN;
      appsScroll = 0;
    }
    else if(homePage == HOME_SETTINGS) {
      if (settingsPage != SET_MAIN) {
        if (settingChangePin) { settingChangePin = false; pinInput = ""; secMessage = ""; }
        else if (settingFormatConfirm) { settingFormatConfirm = false; }
        else if (settingsPage == SET_FONT) { settingsPage = SET_THEME; }
        else if (settingsPage == SET_THEME) { settingsPage = SET_MAIN; settingsScroll = 0; }
        else { settingsPage = SET_MAIN; settingsScroll = 0; }
      } else {
        homePage = HOME_APPS;
        settingsScroll = 0;
      }
    }
    else if(isLibrary()){
      if(librarySelection.length()){
        if(prevActivitySelection.length()){
          drawLoadingScreen("Loading Activity...", "Please wait!", prevActivityPage);
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
    p.putInt("tgtCalories", cfg.targetCalories);
    p.end();
  }
}

static void drawActivitySettingsApp() {
  statusBar();

  // Tile 1: Ride Max Speed (Gauge limit)
  int y = 34;
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
  y = 82;
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
  y = 130;
  card(6, y, 228, 42, uiCard());
  drawCustomIcon(display, 16, y + 10, uiFg(), runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon);
  label(44, y + 6, "ACTIVE SPORT", 1, uiFg());
  label(44, y + 22, runningMode ? "Running Mode" : "Cycling Mode", 1, uiMuted());
  display.fillRoundRect(152, y + 7, 68, 28, 14, runningMode ? ORANGE : TEAL);
  centeredLabel(152, y + 14, 68, runningMode ? "RUN" : "RIDE", 1, 0xFFFF);

  // Tile 4: Countdown Timer (3s / 5s / 10s)
  y = 178;
  card(6, y, 228, 42, uiCard());
  drawCustomIcon(display, 16, y + 10, 0xFA40, CustomIcon::routeMapIcon);
  label(44, y + 6, "START COUNTDOWN", 1, uiFg());
  label(44, y + 22, "Timer before activity starts", 1, uiMuted());
  display.fillRoundRect(152, y + 7, 68, 28, 14, 0xFA40);
  centeredLabel(152, y + 14, 68, String(cfg.countdownSec) + " SEC", 1, 0xFFFF);

  // Tile 5: Target Calories
  y = 226;
  card(6, y, 228, 42, uiCard());
  drawCustomIcon(display, 16, y + 9, ORANGE, CustomIcon::caloriesIcon, 24, 24);
  label(46, y + 6, "TARGET CALORIES", 1, uiFg());
  label(46, y + 22, String(cfg.targetCalories) + " KCAL (Target)", 1, uiMuted());
  display.fillRoundRect(152, y + 7, 30, 28, 6, darkTheme ? 0x2124 : 0xCE79);
  display.drawRoundRect(152, y + 7, 30, 28, 6, darkTheme ? 0x2945 : 0xB596);
  centeredLabel(152, y + 13, 30, "-", 2, uiFg());
  display.fillRoundRect(190, y + 7, 30, 28, 6, darkTheme ? 0x2124 : 0xCE79);
  display.drawRoundRect(190, y + 7, 30, 28, 6, darkTheme ? 0x2945 : 0xB596);
  centeredLabel(190, y + 13, 30, "+", 2, uiFg());
}

static void tapActivitySettings(int x, int y) {
  // Tile 1: Ride Max Speed (y=34..78)
  if (y >= 34 && y < 78) {
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

  // Tile 2: Run Max Speed (y=82..126)
  if (y >= 82 && y < 126) {
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

  // Tile 3: Active Sport Toggle (y=130..174)
  if (y >= 130 && y < 174) {
    runningMode = !runningMode;
    saveActivityConfig();
    uiRevision++;
    return;
  }

  // Tile 4: Start Countdown Toggle (y=178..222)
  if (y >= 178 && y < 222) {
    if (cfg.countdownSec == 3) cfg.countdownSec = 5;
    else if (cfg.countdownSec == 5) cfg.countdownSec = 10;
    else cfg.countdownSec = 3;
    saveActivityConfig();
    uiRevision++;
    return;
  }

  // Tile 5: Target Calories (y=226..270)
  if (y >= 226 && y < 270) {
    if (x >= 140 && x < 184) {
      cfg.targetCalories = max(200, cfg.targetCalories - 100);
      saveActivityConfig();
      uiRevision++;
    } else if (x >= 184 && x < 232) {
      cfg.targetCalories = min(5000, cfg.targetCalories + 100);
      saveActivityConfig();
      uiRevision++;
    }
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

  const int cx = 120, cy = 133;
  const int rOuter = 89;
  const int rDial = 69;

  // Outer circular dial boundary (Frame 35)
  display.drawCircle(cx, cy, rOuter, uiFg());

  // Fixed solid red up-headed needle pointing to 12 o'clock
  // Vertex 1: Tip at (cx, 80)
  // Vertex 2: Base left at (cx - 10, 111)
  // Vertex 3: Base right at (cx + 10, 111)
  display.fillTriangle(cx, 80, cx - 10, 111, cx + 10, 111, 0xF800);

  // Reticle / Hub at center
  // 1. Center disc (radius 14): dark gray #292929 in dark mode, light gray #D6D6D6 in light mode
  uint16_t hubDiscColor = darkTheme ? 0x2945 : 0xD69A;
  display.fillCircle(cx, cy, 14, hubDiscColor);

  // 2. Inner ring outline (radius 23)
  display.drawCircle(cx, cy, 23, uiFg());

  // 3. Crosshair lines extending across the inner ring
  display.drawFastHLine(cx - 23, cy, 47, uiFg());
  display.drawFastVLine(cx, cy - 23, 47, uiFg());

  // Current heading with smooth low-pass interpolation
  static float s_smoothCompassH = 0.0f;
  static bool s_compassInit = false;
  static uint32_t s_lastCompassFrameMs = 0;
  uint32_t nowCompassMs = millis();
  float dtCompass = (s_lastCompassFrameMs > 0) ? constrain((nowCompassMs - s_lastCompassFrameMs) * 0.001f, 0.001f, 0.05f) : 0.016f;
  s_lastCompassFrameMs = nowCompassMs;

  float rawH = (compass.ready && isfinite(compass.heading)) ? compass.heading : (ride.gpsFixed ? (float)ride.heading : 0.0f);
  if (!s_compassInit) {
    s_smoothCompassH = rawH;
    s_compassInit = true;
  } else {
    float diff = rawH - s_smoothCompassH;
    while (diff < -180.0f) diff += 360.0f;
    while (diff > 180.0f) diff -= 360.0f;
    if (fabsf(diff) > 0.35f) {
      float alpha = 1.0f - expf(-dtCompass * 12.0f);
      s_smoothCompassH += diff * alpha;
    }
    while (s_smoothCompassH < 0.0f) s_smoothCompassH += 360.0f;
    while (s_smoothCompassH >= 360.0f) s_smoothCompassH -= 360.0f;
  }
  float h = s_smoothCompassH;

  // Rotating Dial with 12 Steppings (4 Cardinals + 2 Intermediate dots between each)
  // Stepping angle d in degrees: 0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330
  // When device faces heading h, direction d is at angle (d - h) from top (12 o'clock)
  uint16_t dotColor = darkTheme ? 0x632C : 0xA514;
  for (int i = 0; i < 12; i++) {
    float d = i * 30.0f;
    float rad = radians(d - h);
    int px = cx + (int)lroundf(rDial * sinf(rad));
    int py = cy - (int)lroundf(rDial * cosf(rad));

    if (i == 0) {
      // North: vibrant red
      int w = textWidth("N", 2);
      label(px - w / 2, py - 6, "N", 2, 0xF800);
    } else if (i == 3) {
      // East: uiFg
      int w = textWidth("E", 2);
      label(px - w / 2, py - 6, "E", 2, uiFg());
    } else if (i == 6) {
      // South: uiFg
      int w = textWidth("S", 2);
      label(px - w / 2, py - 6, "S", 2, uiFg());
    } else if (i == 9) {
      // West: uiFg
      int w = textWidth("W", 2);
      label(px - w / 2, py - 6, "W", 2, uiFg());
    } else {
      // 8 Intermediate stepping dots
      display.fillCircle(px, py, 2, dotColor);
    }
  }

  // Large heading readout at bottom (Frame 35: e.g. "346°N")
  int deg = (int)lroundf(fmodf(h + 360.0f, 360.0f)) % 360;
  const char* card = getCardinalDirection(deg);
  String degStr = String(deg);
  int wNum = textWidth(degStr, 4);
  int wCard = textWidth(card, 4);
  const int degSymbolWidth = 14;
  int totalW = wNum + degSymbolWidth + wCard;
  int startX = (DISP_W - totalW) / 2;
  int yText = 241;

  // Numerical heading
  label(startX, yText, degStr, 4, uiFg());

  // Degree symbol: clean 2-pixel hollow circle at the top-right of the number
  int degCx = startX + wNum + 6;
  int degCy = yText + 6;
  display.drawCircle(degCx, degCy, 3, uiFg());
  display.drawCircle(degCx, degCy, 4, uiFg());

  // Cardinal direction text
  label(startX + wNum + degSymbolWidth, yText, card, 4, uiFg());

  // Redraw global navigation bar
  drawGlobalNav();
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
        x0 = constrain(x0, clipX1, clipX2);
        x1 = constrain(x1, clipX1, clipX2);
        y0 = constrain(y0, clipY1, clipY2);
        y1 = constrain(y1, clipY1, clipY2);
        if (width <= 1) {
          display.drawLine(x0, y0, x1, y1, color);
        } else {
          int dx = abs(x1 - x0);
          int dy = abs(y1 - y0);
          int h = width / 2;
          if (dx >= dy) {
            for (int k = -h; k <= width - 1 - h; k++) {
              int ly0 = constrain(y0 + k, clipY1, clipY2);
              int ly1 = constrain(y1 + k, clipY1, clipY2);
              display.drawLine(x0, ly0, x1, ly1, color);
            }
          } else {
            for (int k = -h; k <= width - 1 - h; k++) {
              int lx0 = constrain(x0 + k, clipX1, clipX2);
              int lx1 = constrain(x1 + k, clipX1, clipX2);
              display.drawLine(lx0, y0, lx1, y1, color);
            }
          }
          if (width >= 3) {
            if (x0 >= clipX1 + h && x0 <= clipX2 - h && y0 >= clipY1 + h && y0 <= clipY2 - h) {
              display.fillCircle(x0, y0, h, color);
            }
            if (x1 >= clipX1 + h && x1 <= clipX2 - h && y1 >= clipY1 + h && y1 <= clipY2 - h) {
              display.fillCircle(x1, y1, h, color);
            }
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
  statusBar();

  // Input boxes
  display.fillRoundRect(12, 34, 216, 26, 6, uiCard());
  display.drawRoundRect(12, 34, 216, 26, 6, mapSearchField == 0 ? TEAL : (darkTheme ? 0x2945 : 0xD6BA));
  label(18, 40, "LAT: " + mapSearchLat + (mapSearchField == 0 ? "_" : ""), 1, uiFg());

  display.fillRoundRect(12, 64, 216, 26, 6, uiCard());
  display.drawRoundRect(12, 64, 216, 26, 6, mapSearchField == 1 ? TEAL : (darkTheme ? 0x2945 : 0xD6BA));
  label(18, 70, "LON: " + mapSearchLon + (mapSearchField == 1 ? "_" : ""), 1, uiFg());

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
      int ky = 96 + r * 32;
      display.fillRoundRect(kx, ky, 68, 28, 6, uiCard());
      display.drawRoundRect(kx, ky, 68, 28, 6, darkTheme ? 0x2945 : 0xD6BA);
      centeredLabel(kx, ky + 7, 68, keys[r][c], 1, (r == 3 && c == 2) ? 0xFA08 : uiFg());
    }
  }

  // Action Buttons row at y = 232
  // CLEAR
  display.fillRoundRect(14, 232, 68, 28, 6, uiCard());
  display.drawRoundRect(14, 232, 68, 28, 6, darkTheme ? 0x2945 : 0xD6BA);
  centeredLabel(14, 240, 68, "CLEAR", 1, uiMuted());

  // MY LOC
  display.fillRoundRect(86, 232, 68, 28, 6, uiCard());
  display.drawRoundRect(86, 232, 68, 28, 6, darkTheme ? 0x2945 : 0xD6BA);
  centeredLabel(86, 240, 68, "MY LOC", 1, uiFg());

  // SEARCH
  display.fillRoundRect(158, 232, 68, 28, 6, TEAL);
  display.drawRoundRect(158, 232, 68, 28, 6, TEAL);
  centeredLabel(158, 240, 68, "SEARCH", 1, 0xFFFF);

  display.fillRect(0, 286, 240, 34, uiBg());
  drawGlobalNav();
}

static void drawSensorsApp() {
  statusBar();

  if (sensorsGuideSubpage > 0) {
    // -------------------------------------------------------------
    // SENSOR SYNC & SETUP WALKTHROUGH GUIDE
    // -------------------------------------------------------------
    // Header Bar with Back Button
    display.fillRoundRect(6, 30, 80, 26, 13, uiCard());
    drawCustomIcon(display, 12, 35, uiFg(), CustomIcon::GoBackIcon, 16, 16);
    label(30, 37, "BACK", 1, uiFg());

    const char* title = "";
    uint16_t headerCol = uiFg();
    if (sensorsGuideSubpage == 1) { title = "MAGENE H303 HRM"; headerCol = 0xF800; }
    else if (sensorsGuideSubpage == 2) { title = "SPEED SENSOR"; headerCol = TEAL; }
    else if (sensorsGuideSubpage == 3) { title = "CADENCE SENSOR"; headerCol = 0xFA60; }
    else if (sensorsGuideSubpage == 4) { title = "POWER METER"; headerCol = 0xFE20; }

    centeredLabel(88, 36, 146, title, 1, headerCol);

    // Step-by-Step Instructions Container Card
    display.fillRoundRect(6, 62, 228, 180, 10, uiCard());

    struct StepItem { const char* num; const char* line1; const char* line2; };
    StepItem steps[4];

    if (sensorsGuideSubpage == 1) { // HRM
      steps[0] = {"1", "Moisten rubber electrode pads", "on strap with water or gel"};
      steps[1] = {"2", "Snap H303 sensor onto strap", "Ensure Magene logo is upright"};
      steps[2] = {"3", "Wear strap snugly around chest", "Position directly below pectorals"};
      steps[3] = {"4", "Sensor turns on from skin contact", "Broadcasts ANT+ (2457MHz) & BLE"};
    } else if (sensorsGuideSubpage == 2) { // Speed
      steps[0] = {"1", "Select SPEED MODE (Green LED)", "Reinstall CR2032 battery to switch"};
      steps[1] = {"2", "Attach curved rubber pad", "Strap securely to front or rear hub"};
      steps[2] = {"3", "Spin wheel 2-3 rotations", "Wakes internal sensor from sleep"};
      steps[3] = {"4", "Speed calculated from wheel hub", "Zero GPS multipath or drift errors"};
    } else if (sensorsGuideSubpage == 3) { // Cadence
      steps[0] = {"1", "Select CADENCE MODE (Red LED)", "Reinstall CR2032 battery to switch"};
      steps[1] = {"2", "Attach flat silicone pad", "Strap to inside face of left crank"};
      steps[2] = {"3", "Rotate crank arm 2 full turns", "Accelerometer wakes from standby"};
      steps[3] = {"4", "Live pedaling RPM broadcasts", "Streams to display & FIT records"};
    } else { // Power
      steps[0] = {"1", "Rotate crank arms 3-4 times", "Powers up strain-gauge radio"};
      steps[1] = {"2", "Keep cranks vertical & unweighted", "For zero-offset torque balance"};
      steps[2] = {"3", "ANT+ Channel 1 binds automatically", "Standard 4.00 Hz TDMA period"};
      steps[3] = {"4", "Live power (Watts) & balance", "Saved to FIT & activity summary"};
    }

    for (int i = 0; i < 4; i++) {
      int sy = 70 + i * 42;
      display.fillCircle(22, sy + 14, 10, headerCol);
      centeredLabel(12, sy + 10, 20, steps[i].num, 1, 0xFFFF);
      label(40, sy + 6, steps[i].line1, 1, uiFg());
      label(40, sy + 20, steps[i].line2, 1, uiMuted());
    }

    // Bottom Action Button: [ START PAIRING NOW ]
    display.fillRoundRect(16, 248, 208, 34, 17, (millis() - sensorsScanStartMs < 5000) ? 0x07E0 : 0x051F);
    centeredLabel(16, 258, 208, (millis() - sensorsScanStartMs < 5000) ? "SEARCHING SENSORS..." : "START PAIRING NOW", 1, 0xFFFF);

    drawGlobalNav();
    return;
  }

  // -------------------------------------------------------------
  // MAIN SENSORS DASHBOARD
  // -------------------------------------------------------------
  // Header Status Bar: ANT+ & BLE Radio Indicators + Scan Button
  int hy = 30;
  card(6, hy, 228, 34, uiCard());
  // ANT+ Indicator
  display.fillCircle(18, hy + 17, 4, antReady ? 0x07E0 : 0xF800);
  label(26, hy + 12, "ANT+ (CORE 0)", 1, uiFg());
  // BLE Indicator
  display.fillCircle(118, hy + 17, 4, 0x051F);
  label(126, hy + 12, "BLE READY", 1, uiFg());

  // Pair Action Pill
  bool scanning = (millis() - sensorsScanStartMs < 5000);
  uint16_t pairBg = scanning ? 0x07E0 : 0xFA60;
  display.fillRoundRect(172, hy + 4, 56, 26, 13, pairBg);
  centeredLabel(172, hy + 10, 56, scanning ? "SCAN" : "PAIR", 1, 0xFFFF);

  // Card 1: Heart Rate Monitor (Magene H303)
  int y1 = 68 - sensorsScroll;
  if (y1 >= 24 && y1 < 290) {
    card(6, y1, 228, 48, uiCard());
    drawCustomIcon(display, 14, y1 + 12, 0xF800, CustomIcon::heartRateIcon, 24, 24);
    label(44, y1 + 9, "HEART RATE (HRM)", 1, uiFg());
    bool hrActive = (bleHRReady || (antHRMs && millis() - antHRMs < 4000 && antHR > 0));
    int curHr = bleHRReady ? bleHR : antHR;
    if (hrActive) {
      String sub = antHrmDevId ? ("ANT+ #" + String(antHrmDevId) + " (" + String(antHrmRssi) + "dBm)") : "BLE HRM Connected";
      label(44, y1 + 27, sub, 1, 0x07E0);
      String val = String(curHr) + " BPM";
      int vw = textWidth(val, 2);
      label(226 - vw, y1 + 14, val, 2, 0x07E0);
    } else {
      label(44, y1 + 27, "Magene H303 • Tap for guide", 1, uiMuted());
      display.fillRoundRect(168, y1 + 12, 58, 24, 12, darkTheme ? 0x2124 : 0xCE79);
      centeredLabel(168, y1 + 18, 58, "GUIDE", 1, uiFg());
    }
  }

  // Card 2: Speed Sensor (Magene S314 / CAD70)
  int y2 = 122 - sensorsScroll;
  if (y2 >= 24 && y2 < 290) {
    card(6, y2, 228, 48, uiCard());
    drawCustomIcon(display, 14, y2 + 12, TEAL, CustomIcon::speedometerIcon, 24, 24);
    label(44, y2 + 9, "SPEED SENSOR", 1, uiFg());
    bool spdActive = (bleSpeedReady || (antSpeedMs && millis() - antSpeedMs < 4000));
    float curSpd = bleSpeedReady ? bleSpeed : antSpeed;
    if (spdActive) {
      String sub = antSpdDevId ? ("ANT+ #" + String(antSpdDevId) + " • 2096mm") : "Wheel Hub • 2096mm";
      label(44, y2 + 27, sub, 1, 0x07E0);
      String val = String(curSpd, 1) + " KM/H";
      int vw = textWidth(val, 2);
      label(226 - vw, y2 + 14, val, 2, TEAL);
    } else {
      label(44, y2 + 27, "Wheel Hub • Green LED Mode", 1, uiMuted());
      display.fillRoundRect(168, y2 + 12, 58, 24, 12, darkTheme ? 0x2124 : 0xCE79);
      centeredLabel(168, y2 + 18, 58, "GUIDE", 1, uiFg());
    }
  }

  // Card 3: Cadence Sensor (Magene S314 / CAD70)
  int y3 = 176 - sensorsScroll;
  if (y3 >= 24 && y3 < 290) {
    card(6, y3, 228, 48, uiCard());
    drawCustomIcon(display, 14, y3 + 12, 0xFA60, CustomIcon::bikeIcon, 24, 24);
    label(44, y3 + 9, "CADENCE SENSOR", 1, uiFg());
    bool cadActive = (bleCadReady || (antCadMs && millis() - antCadMs < 4000));
    int curCad = bleCadReady ? bleCad : antCad;
    if (cadActive) {
      String sub = antCadDevId ? ("ANT+ #" + String(antCadDevId)) : "Crank Arm Connected";
      label(44, y3 + 27, sub, 1, 0x07E0);
      String val = String(curCad) + " RPM";
      int vw = textWidth(val, 2);
      label(226 - vw, y3 + 14, val, 2, 0xFA60);
    } else {
      label(44, y3 + 27, "Crank Arm • Red LED Mode", 1, uiMuted());
      display.fillRoundRect(168, y3 + 12, 58, 24, 12, darkTheme ? 0x2124 : 0xCE79);
      centeredLabel(168, y3 + 18, 58, "GUIDE", 1, uiFg());
    }
  }

  // Card 4: Power Meter
  int y4 = 230 - sensorsScroll;
  if (y4 >= 24 && y4 < 290) {
    card(6, y4, 228, 48, uiCard());
    drawCustomIcon(display, 14, y4 + 12, 0xFE20, CustomIcon::powerIcon, 24, 24);
    label(44, y4 + 9, "POWER METER", 1, uiFg());
    bool pwrActive = (blePowerReady || (antPowerMs && millis() - antPowerMs < 4000));
    int curPwr = blePowerReady ? blePower : antPower;
    if (pwrActive) {
      String sub = antPowerDevId ? ("ANT+ #" + String(antPowerDevId)) : "Power Meter Connected";
      label(44, y4 + 27, sub, 1, 0x07E0);
      String val = String(curPwr) + " W";
      int vw = textWidth(val, 2);
      label(226 - vw, y4 + 14, val, 2, 0xFE20);
    } else {
      label(44, y4 + 27, "ANT+ Ch 1 • Tap for guide", 1, uiMuted());
      display.fillRoundRect(168, y4 + 12, 58, 24, 12, darkTheme ? 0x2124 : 0xCE79);
      centeredLabel(168, y4 + 18, 58, "GUIDE", 1, uiFg());
    }
  }

  drawGlobalNav();
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
      {"Maps", CustomIcon::mapAppIconLarge, 0x33FB},           // Bright Blue (#3578DD)
      {"Compass", CustomIcon::compassIconLarge, 0xE800},        // Bright Red (#EE0000)
      {"Sensors", CustomIcon::heartRateIcon, 0x051F},          // Neon Cyan / Royal (#00A2FF)
      {"My Activities", CustomIcon::activitiesIconLarge, 0xFA61}, // Bright Orange (#F4510C)
      {"Past Routes", CustomIcon::routeMapIconLarge, 0x5444},   // Leaf Green (#558920)
      {"Settings", CustomIcon::systemIconLarge, 0x79F7},        // Purple (#7C3AED)
      {"Activity Settings", CustomIcon::configureIconLarge, 0xDBC0}, // Amber (#D97706)
      {"My Files", CustomIcon::folderIconLarge, 0x0CA1}         // Teal (#0D9488)
    };
    const int kNumApps = 8;
    if (appsListView) {
      // LIST VIEW: 1 app per row, large button with label beside, clean minimal background
      const int rowH = 56;
      const int rowPitch = 64;
      const int gutterX = 20;
      const int btnR = 24;
      const int btnCx = gutterX + btnR; // 44

      for (int i = 0; i < kNumApps; i++) {
        int cardY = 34 + i * rowPitch - appsScroll;
        if (cardY + rowH < 26 || cardY > 290) continue;

        // Circular colored app button on the left (R = 24, diameter 48)
        int btnCy = cardY + rowH / 2;
        display.fillCircle(btnCx, btnCy, btnR, kApps[i].color);
        drawCustomIcon(display, btnCx - 16, btnCy - 16, 0xFFFF, kApps[i].icon, 32, 32);

        // App label beside button in row (size 2 = 18pt font, more than double size 1)
        label(btnCx + btnR + 14, cardY + 20, kApps[i].name, 2, uiFg());
      }
    } else {
      // GRID VIEW: 2-column icon grid
      const int R = 38;
      const int rowPitch = 114;

      for (int i = 0; i < kNumApps; i++) {
        int col = i % 2;
        int row = i / 2;
        int cx = (col == 0) ? 68 : 172;
        int cy = 68 + row * rowPitch - appsScroll;

        // Cull items completely offscreen
        if (cy + R + 24 < 26 || cy - R > 290) continue;

        display.fillCircle(cx, cy, R, kApps[i].color);
        drawCustomIcon(display, cx - 20, cy - 20, 0xFFFF, kApps[i].icon);
        centeredLabel(cx - 58, cy + R + 7, 116, kApps[i].name, 0, uiFg());
      }
    }

    // Re-draw status bar to cleanly clip top scroll
    display.fillRect(0, 0, DISP_W, 26, uiBg());
    statusBar(true, false);

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
  }else if(homePage==HOME_SENSORS){
    drawSensorsApp();
  }else{
    if(fileDetailOpen){
      drawFileDetail();
      return;
    }
    statusBar();
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
    }
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
    distAccum=0; trackLen=0; ride.distanceKm=0;ride.elevationGainM=0;ride.elapsedSec=0;
    ride.avgSpeedKmh=0;ride.maxSpeedKmh=0;ride.maxPowerW=0;
    resetMaxSpeedTracker();
    hrSum=powerSum=0;hrSamples=powerSamples=0;hrMax=0;lastLat=lastLon=0;distAnchorLat=distAnchorLon=0;
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
    distAnchorLat = distAnchorLon = 0;
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
  int w = textWidth(s, 4);
  label(rightX - w, y, s, 4, fg);
}

static void drawGpsRadarScreen() {
  const int cx = 120;
  const int cy = 120 - gpsScroll;
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

  // Outer ring (R=88): increased dial size shifted upward to comfortably fit all satellite dots
  display.fillCircle(cx, cy, 88, outerFill);
  display.drawCircle(cx, cy, 88, radarLineColor);

  // Middle ring (R=55): proportional concentric division
  display.fillCircle(cx, cy, 55, middleFill);

  // Single Rotating Radar Sweep Line (radius 22 to 88)
  int deg = (t / 8) % 360;
  float rad = radians((float)deg);
  int x1 = cx + (int)(22.0f * cosf(rad));
  int y1 = cy + (int)(22.0f * sinf(rad));
  int x2 = cx + (int)(88.0f * cosf(rad));
  int y2 = cy + (int)(88.0f * sinf(rad));
  display.drawLine(x1, y1, x2, y2, radarLineColor);

  // Satellite Dots (calibrated radii 36..76 to stay comfortably within the R=88 boundary)
  struct SatPoint { uint8_t r; uint16_t deg; };
  static const SatPoint satPoints[24] = {
    {38, 60},   {66, 25},   {76, 0},    {42, 160},
    {62, 235},  {72, 120},  {46, 310},  {74, 280},
    {36, 85},   {68, 195},  {58, 345},  {44, 215},
    {65, 30},   {48, 140},  {75, 95},   {40, 275},
    {70, 160},  {54, 45},   {74, 220},  {42, 15},
    {65, 180},  {50, 105},  {76, 320},  {68, 240}
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
  drawBigNumber(220, textY + 5, sats, uiFg());

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
    static float s_animBarW = 0.0f;
    static uint32_t s_lastRadarMs = 0;
    uint32_t nowRadarMs = millis();
    float dtBar = (s_lastRadarMs > 0) ? constrain((nowRadarMs - s_lastRadarMs) * 0.001f, 0.001f, 0.05f) : 0.016f;
    s_lastRadarMs = nowRadarMs;

    float targetW = (sats <= 0) ? 0.0f : constrain((float)sats * (float)barW / (float)maxSats, (float)barH, (float)barW);
    float alphaBar = 1.0f - expf(-dtBar * 10.0f);
    s_animBarW += (targetW - s_animBarW) * alphaBar;
    if (fabsf(targetW - s_animBarW) < 0.2f) s_animBarW = targetW;

    int fillW = (int)lroundf(s_animBarW);
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
  int w = textWidth(s, 4);
  const auto& f = textFont(4);
  int startY = cy - f.cap / 2;
  label(cx - w / 2, startY, s, 4, fg);
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

  const int cx = 120, cy = 120, r = 88, thicc = 5;
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

    int iconSize = 28, gap = 8;
    int textW = textWidth("GO", 3);
    int totalW = iconSize + gap + textW;
    int startX = (DISP_W - totalW) / 2;
    int yBottom = 238;

    drawCustomIcon(display, startX, yBottom - 2, 0xFFFF, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, iconSize, iconSize);
    label(startX + iconSize + gap, yBottom + 1, "GO", 3, uiFg());

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
    int iconSize = 28, gap = 8;
    int textW = textWidth("Ready", 3);
    int totalW = iconSize + gap + textW;
    int startX = (DISP_W - totalW) / 2;
    int yBottom = 238;

    drawCustomIcon(display, startX, yBottom - 2, 0xFFFF, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, iconSize, iconSize);
    label(startX + iconSize + gap, yBottom + 1, "Ready", 3, uiFg());
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
    local.tm_mday = 6;
    local.tm_mon = 10;
    local.tm_year = 126;
  }

  const char* const days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int h12 = local.tm_hour % 12;
  if (h12 == 0) h12 = 12;

  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", local.tm_hour, local.tm_min);

  String dateStr;
  if (timeValid) {
    dateStr = String(days[local.tm_wday]) + " " + String(local.tm_mday) + " " + String(months[local.tm_mon]) + ", " + String(1900 + local.tm_year);
  } else {
    dateStr = "Friday 6 Nov, 2026";
  }

  // Large 56px smooth digits lowered slightly to Y = 46 (height 44px)
  drawSmoothClock56(46, local.tm_hour, local.tm_min, fg);
  centeredLabel(0, 98, DISP_W, dateStr, 0, fg);
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
  else if(gpsWaiting || gpsRadarInspector) state="gpsRadar:"+String(millis()/16)+":"+String(startSatellites())+":"+String(gpsDataReady)+":"+String(gpsScroll)+":"+String(gpsRadarInspector)+":"+String(darkTheme);
  else if(countdownGoStart) state="countGo:"+String((millis()-countdownGoStart)/40);
  else if(countdownStart) state="count:"+String((millis()-countdownStart)/40);
  else if((welcomeOpen || quickOpen) && homePage == HOME_COMPASS) state="compass:"+String(millis()/16)+":"+String((int)(compass.heading*10))+":"+String(darkTheme);
  else if((welcomeOpen || quickOpen) && homePage == HOME_SENSORS) state="sensors:"+String(millis()/250)+":"+String(antHR)+":"+String(antCad)+":"+String((int)(antSpeed*10))+":"+String(antPower)+":"+String(bleCount())+":"+String(sensorsGuideSubpage)+":"+String(darkTheme);
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
      display.drawBrandLogo((DISP_W - 137) / 2, 48, !darkTheme, 137, 56);
    }

    // Row 1: Two Clean Pill Cards (108x54, R=27, Y=126..180)
    bool ready = gpsStartReady();
    int sats = startSatellites();
    bool countValid = gpsDataReady && ((geprcGps.lastStatus && millis() - geprcGps.lastStatus < 3000) || (gps.satellites.isValid() && gps.satellites.age() < 3000));
    uint16_t gpsBg = darkTheme ? (ready ? 0x0900 : (gpsDataReady ? 0x2100 : 0x1082)) : (ready ? 0xD753 : 0xFDE7);
    uint16_t gpsFg = darkTheme ? (ready ? 0x7FE0 : (gpsDataReady ? 0xFD20 : 0x8410)) : (ready ? 0x2400 : 0x9800);

    // Row 1 Left: GPS Status Pill
    display.fillRoundRect(8, 126, 108, 54, 27, gpsBg);
    String satStr = (countValid && sats > 0) ? String(sats) : (gpsDataReady ? "0" : "--");
    int satNumW = textWidth(satStr, 3);
    int iconW = 28;
    int gap = 8;
    int totalGpsW = iconW + gap + satNumW;
    int gpsStartX = 8 + (108 - totalGpsW) / 2;
    drawCustomIcon(display, gpsStartX, 126 + 13, gpsFg, CustomIcon::satelliteIconLarge, iconW, iconW);
    label(gpsStartX + iconW + gap, 126 + 17, satStr, 3, gpsFg);

    // Row 1 Right: Sport Switcher Pill (108x54, R=27)
    display.fillRoundRect(124, 126, 108, 54, 27, uiCard());
    int switchStartX = 124 + (108 - (24 + 12 + 24)) / 2;
    uint16_t sportAltColor = darkTheme ? 0x475F : 0x034E;
    drawCustomIcon(display, switchStartX, 126 + 15, sportAltColor, runningMode ? CustomIcon::bikeIcon : CustomIcon::runIcon, 24, 24);
    drawCustomIcon(display, switchStartX + 24 + 12, 126 + 15, uiMuted(), CustomIcon::switchIcon, 24, 24);

    // Row 2: Slide-to-Start Slider (224x54, R=27, Y=190..244)
    display.fillRoundRect(8, 190, 224, 54, 27, uiCard());
    const char* sliderText = rideStarted ? "Swipe to open" : "Swipe to start";
    int textW = textWidth(sliderText, 0);
    int trackStartX = 116 + (116 - textW) / 2;
    label(trackStartX, 190 + 21, sliderText, 0, uiMuted());
    int pillX = 8 + homeSliderOffset;
    display.fillRoundRect(pillX, 190, 108, 54, 27, 0xF800);
    drawCustomIcon(display, pillX + 27, 190 + 13, 0xFFFF, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 28, 28);
    drawCustomIcon(display, pillX + 67, 190 + 20, 0xFFFF, CustomIcon::goNextIcon, 14, 14);

    // Row 3: Two Clean Pill Cards (108x54, R=27, Y=254..308)
    // Row 3 Left: Compass App Pill (warm amber in dark theme, brown on transparent brown in light theme)
    uint16_t compassBg = darkTheme ? 0x2900 : 0xEEF7;
    uint16_t compassFg = darkTheme ? 0xFE64 : 0xAC63;
    display.fillRoundRect(8, 254, 108, 54, 27, compassBg);
    drawCustomIcon(display, 8 + (108 - 28) / 2, 254 + 13, compassFg, CustomIcon::compassIconLarge, 28, 28);

    // Row 3 Right: All Apps Launcher Pill (3x3 white dot matrix)
    display.fillRoundRect(124, 254, 108, 54, 27, uiCard());
    int dotCx = 124 + 54;
    int dotCy = 254 + 27;
    for (int dy = -7; dy <= 7; dy += 7) {
      for (int dx = -7; dx <= 7; dx += 7) {
        display.fillCircle(dotCx + dx, dotCy + dy, 2, uiFg());
      }
    }
  }
  return true;
}

static void quickTap(int x, int y) {
  if (screenLocked) {
    return;
  }
  if(y>=290 && !isHomeScreen()){
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
        if(y>=204 && y<250){
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
      if(browserPath != "/"){
        // Tapping Row 1 (Folder Header Pill) goes back to root
        if(y >= 38 && y < 92 && x >= 8 && x <= 232){
          int slash = browserPath.lastIndexOf('/');
          browserPath = slash <= 0 ? "/" : browserPath.substring(0, slash);
          browserPage = 0;
          loadBrowser();
          uiRevision++;
          return;
        }
        // File rows (Rows 2, 3, 4 at y=102, 166, 230)
        int row = (y - 102) / 64;
        if(y >= 102 && row >= 0 && row < browserCount && (y - 102) % 64 < 54 && x >= 8 && x <= 232){
          if(browserDirs[row]) openBrowser((browserPath == "/" ? String("") : browserPath) + "/" + browserNames[row]);
          else openFileDetail(row);
          return;
        }
        return;
      } else {
        // Root folder rows (Rows 1, 2, 3, 4 at y=38, 102, 166, 230)
        int row = (y - 38) / 64;
        if(y >= 38 && row >= 0 && row < browserCount && (y - 38) % 64 < 54 && x >= 8 && x <= 232){
          if(browserDirs[row]) openBrowser((browserPath == "/" ? String("") : browserPath) + "/" + browserNames[row]);
          else openFileDetail(row);
          return;
        }
        return;
      }
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
      // LAT input box (y=34..60)
      if (x >= 12 && x <= 228 && y >= 34 && y <= 60) {
        mapSearchField = 0;
        uiRevision++;
        return;
      }
      // LON input box (y=64..90)
      if (x >= 12 && x <= 228 && y >= 64 && y <= 90) {
        mapSearchField = 1;
        uiRevision++;
        return;
      }
      // Keypad area (y=96..224)
      if (x >= 14 && x <= 230 && y >= 96 && y <= 224) {
        int kr = (y - 96) / 32;
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
      // Action row (y=230..264)
      if (y >= 230 && y <= 264) {
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
    if (homePage == HOME_SENSORS) {
      if (y < 28 || y >= 290) return;
      if (sensorsGuideSubpage > 0) {
        if (x <= 95 && y <= 58) {
          sensorsGuideSubpage = 0;
          uiRevision++;
          return;
        }
        if (y >= 245 && y <= 288) {
          antSearchAll();
          bleRequest(1);
          sensorsScanStartMs = millis();
          sensorsGuideSubpage = 0;
          uiRevision++;
          return;
        }
        return;
      }
      if (x >= 155 && y >= 30 && y <= 66) {
        antSearchAll();
        bleRequest(1);
        sensorsScanStartMs = millis();
        uiRevision++;
        return;
      }
      int relY = y + sensorsScroll;
      if (relY >= 66 && relY < 118) {
        sensorsGuideSubpage = 1;
        uiRevision++;
        return;
      }
      if (relY >= 120 && relY < 172) {
        sensorsGuideSubpage = 2;
        uiRevision++;
        return;
      }
      if (relY >= 174 && relY < 226) {
        sensorsGuideSubpage = 3;
        uiRevision++;
        return;
      }
      if (relY >= 228 && relY < 285) {
        sensorsGuideSubpage = 4;
        uiRevision++;
        return;
      }
      return;
    }
    if(homePage==HOME_APPS){
      if(y < 26 || y >= 290) return;
      int hitApp = -1;
      if (appsListView) {
        const int rowH = 56;
        const int rowPitch = 64;
        for (int i = 0; i < 8; i++) {
          int cardY = 34 + i * rowPitch - appsScroll;
          if (x >= 10 && x <= 235 && y >= cardY && y <= cardY + rowH) {
            hitApp = i;
            break;
          }
        }
      } else {
        for(int i = 0; i < 8; i++){
          int col = i % 2;
          int row = i / 2;
          int cx = (col == 0) ? 68 : 172;
          int cy = 68 + row * 114 - appsScroll;
          int dx = x - cx;
          int dy = y - cy;
          bool inCircle = (dx * dx + dy * dy <= 44 * 44);
          bool inLabel = (abs(dx) <= 58 && (y >= cy + 34 && y <= cy + 64));
          if (inCircle || inLabel) {
            hitApp = i;
            break;
          }
        }
      }
      if (hitApp >= 0) {
        if(hitApp == 0){ // Maps
          if(ride.gpsFixed && isfinite(ride.lat) && isfinite(ride.lon) && ride.lat != 0.0){
            mapViewerLat = ride.lat;
            mapViewerLon = ride.lon;
          }
          homePage = HOME_MAPS;
          drawLoadingScreen("Loading Maps...", "Please wait!", HOME_MAPS);
          g_mapCache.updatePosition(mapViewerLat, mapViewerLon);
          uiRevision++;
        }
        else if(hitApp == 1){ // Compass
          homePage = HOME_COMPASS;
          drawLoadingScreen("Loading Compass...", "Please wait!", HOME_COMPASS);
          uiRevision++;
        }
        else if(hitApp == 2){ // Sensors
          homePage = HOME_SENSORS;
          drawLoadingScreen("Loading Sensors...", "Please wait!", HOME_SENSORS);
          sensorsScroll = 0;
          sensorsGuideSubpage = 0;
          uiRevision++;
        }
        else if(hitApp == 3){ // My Activities
          openLibrary(HOME_ACTIVITY);
        }
        else if(hitApp == 4){ // Past Routes
          openLibrary(HOME_ROUTES);
        }
        else if(hitApp == 5){ // Settings
          homePage = HOME_SETTINGS;
          drawLoadingScreen("Loading Settings...", "Please wait!", HOME_SETTINGS);
          settingsPage = SET_MAIN;
          settingsScroll = 0;
          uiRevision++;
        }
        else if(hitApp == 6){ // Activity Settings
          homePage = HOME_ACTIVITY_SETTINGS;
          drawLoadingScreen("Loading Settings...", "Please wait!", HOME_ACTIVITY_SETTINGS);
          uiRevision++;
        }
        else if(hitApp == 7){ // My Files
          openBrowser("/");
        }
        return;
      }
    }
    if (y >= 28 && y < 98) {
      homeHeroClock = !homeHeroClock;
      saveHomeHeroPref();
      uiRevision++;
      return;
    }
    if (y >= 29 && y < 124) {
      homeHeroClock = !homeHeroClock;
      saveHomeHeroPref();
      uiRevision++;
      return;
    }
    if (y >= 124 && y < 184) {
      if (x < 120) {
        openGpsInspector();
      } else {
        if (rideStarted) {
          activityMessage = "Save activity before switching mode";
        } else {
          runningMode = !runningMode;
          activityMessage = runningMode ? "Running mode" : "Cycling mode";
        }
        uiRevision++;
      }
      return;
    }
    if (y >= 250 && y < 320) {
      if (x < 120) {
        homePage = HOME_COMPASS;
        drawLoadingScreen("Loading Compass...", "Please wait!", HOME_COMPASS);
        uiRevision++;
      } else {
        homePage = HOME_APPS;
        appsScroll = 0;
        uiRevision++;
      }
      return;
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
    if (autoPaused && rideStarted) {
      // Tap on Override pill (y = 220..286)
      if (y >= 220 && y <= 286 && x >= 8 && x <= 232) {
        autoPaused = false;
        if (autoPauseStartMs > 0) {
          rideStartMs += (millis() - autoPauseStartMs);
          autoPauseStartMs = 0;
        }
        autoPauseOverrideGraceMs = millis() + 15000;
        nearZeroSpeedStartMs = 0;
        autoResumeMovementStartMs = 0;
        autoPauseAnchorLat = autoPauseAnchorLon = 0.0f;
        distAnchorLat = distAnchorLon = 0.0f;
        fitEvent(0);
        uiRevision++;
        return;
      }
      return;
    }
    if (currentScreen == SCR_CONTROLS) {
      if (activityEndConfirm) {
        // Big Capsule Button 1: Save & Finish (y = 166..220)
        if (y >= 160 && y <= 224 && x >= 8 && x <= 232) {
          activityEndConfirm = false;
          saveActivity();
          ridePaused = false;
          welcomeOpen = true;
          homePage = HOME_MAIN;
          currentScreen = SCR_DASH;
          uiRevision++;
          return;
        }
        // Big Capsule Button 2: Resume Activity (y = 230..284)
        if (y >= 226 && y <= 288 && x >= 8 && x <= 232) {
          activityEndConfirm = false;
          uiRevision++;
          return;
        }
        // Tapping anywhere else also cancels confirmation safely
        activityEndConfirm = false;
        uiRevision++;
        return;
      }

      // Row 2: Auto Pause Activity toggle (y = 102..156)
      if (y >= 102 && y < 156 && x >= 8 && x < 232) {
        autoPauseEnabled = !autoPauseEnabled;
        Preferences p;
        if (p.begin("cyclo", false)) {
          p.putBool("autoPause", autoPauseEnabled);
          p.end();
        }
        if (!autoPauseEnabled && autoPaused) {
          autoPaused = false;
          if (autoPauseStartMs > 0) {
            rideStartMs += (millis() - autoPauseStartMs);
            autoPauseStartMs = 0;
          }
          nearZeroSpeedStartMs = 0;
          autoResumeMovementStartMs = 0;
          autoPauseAnchorLat = autoPauseAnchorLon = 0.0f;
        }
        uiRevision++;
        return;
      }

      // Row 3: Pause / Resume Ride (y = 166..220)
      if (y >= 166 && y < 220 && x >= 8 && x < 232) {
        if (!rideStarted) requestRideStart();
        else toggleRide();
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
  if(y>=106 && y<160 && x>=8 && x<232){setBrightnessFromTouch(x);return;}
  if (x>=8 && x<232 && y>=30 && y<98) {
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
  // Hit-test the capsule pill rectangles; gutters do not activate neighbours.
  int col = (x>=124 && x<232) ? 1 : ((x>=8 && x<116) ? 0 : -1);
  int row = (y>=168 && y<222) ? 0 :
            ((y>=232 && y<286) ? 1 :
            ((y>=296 && y<350) ? 2 :
            ((y>=360 && y<414) ? 3 : -1)));
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

// ─── 60 FPS Kinetic Scrolling Engine ───────────────────────────────────────
static float scrollVelocity = 0.0f;
static bool touchScrolling = false;
static int lastTouchY = 0;
static uint32_t lastTouchYMs = 0;

static inline bool isScrollingActive() {
  return (fabsf(scrollVelocity) > 0.2f) || touchScrolling;
}

static void serviceKineticScroll() {
  if (fabsf(scrollVelocity) > 0.25f && !touchScrolling) {
    if (quickOpen && !quickDetail) {
      int next = constrain((int)roundf(quickScroll + scrollVelocity), 0, 140);
      if (next == quickScroll || next == 0 || next == 140) scrollVelocity = 0.0f;
      quickScroll = next;
    } else if (isLibrary() && librarySelection.length()) {
      int maxS = (homePage == HOME_ROUTES) ? 120 : ARCHIVE_MAX_SCROLL;
      int next = constrain((int)roundf(archiveScroll + scrollVelocity), 0, maxS);
      if (next == archiveScroll || next == 0 || next == maxS) scrollVelocity = 0.0f;
      archiveScroll = next;
    } else if (welcomeOpen && !quickOpen && homePage == HOME_APPS) {
      int maxS = getAppsMaxScroll();
      int next = constrain((int)roundf(appsScroll + scrollVelocity), 0, maxS);
      if (next == appsScroll || next == 0 || next == maxS) scrollVelocity = 0.0f;
      appsScroll = next;
    } else if (welcomeOpen && !quickOpen && homePage == HOME_SETTINGS && settingsPage == SET_MAIN) {
      int next = constrain((int)roundf(settingsScroll + scrollVelocity), 0, 140);
      if (next == settingsScroll || next == 0 || next == 140) scrollVelocity = 0.0f;
      settingsScroll = next;
    } else if (gpsWaiting || gpsRadarInspector) {
      int next = constrain((int)roundf(gpsScroll + scrollVelocity), 0, 128);
      if (next == gpsScroll || next == 0 || next == 128) scrollVelocity = 0.0f;
      gpsScroll = next;
    } else {
      scrollVelocity = 0.0f;
    }
    scrollVelocity *= 0.90f; // Silky smooth deceleration curve
    if (fabsf(scrollVelocity) <= 0.25f) scrollVelocity = 0.0f;
    uiRevision++;
  }
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
    if (homeSliderDragging) {
      if (homeSliderOffset >= 85) {
        if (rideStarted) {
          welcomeOpen = false;
          currentScreen = (nav.hasDestination && routeLen > 1) ? SCR_MAP : SCR_DASH;
        } else {
          requestRideStart();
        }
      }
      homeSliderOffset = 0;
      homeSliderDragging = false;
      uiRevision++;
      rootGesture = false; sliderGesture = false; mapPanGesture = false; held = false; candidateX = -1;
      return;
    }
    if (controlSliderDragging) {
      if (controlSliderOffset <= -85) {
        activityEndConfirm = true;
      }
      controlSliderOffset = 0;
      controlSliderDragging = false;
      uiRevision++;
      rootGesture = false; sliderGesture = false; mapPanGesture = false; held = false; candidateX = -1;
      return;
    }
    if(rootGesture && !sliderGesture) {
      if (welcomeOpen && !quickOpen && homePage == HOME_MAIN) {
        int dx = endX - startX;
        int dy = endY - startY;
        if (startY < 124) {
          if (abs(dx) >= 20 && abs(dx) > abs(dy)) {
            homeHeroClock = !homeHeroClock;
            saveHomeHeroPref();
            uiRevision++;
          } else if (!moved) {
            homeHeroClock = !homeHeroClock;
            saveHomeHeroPref();
            uiRevision++;
          }
        } else if (!moved || (abs(dx) < 20 && abs(dy) < 20)) {
          quickTap(startX, startY);
        }
      } else if (!moved) {
        quickTap(endX, endY);
      }
    }
    touchScrolling = false;
    if (moved && !sliderGesture && !mapPanGesture) {
      if (fabsf(scrollVelocity) >= 0.8f) {
        scrollVelocity = constrain(scrollVelocity, -28.0f, 28.0f);
      } else {
        scrollVelocity = 0.0f;
      }
    } else {
      scrollVelocity = 0.0f;
    }
    rootGesture=false;sliderGesture=false;mapPanGesture=false;held=false;candidateX=-1;
  };
  if (!touchReady || millis() - poll < 8) return;
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
    if(abs(y-startY)>5 || abs(x-startX)>5){
      moved=true;
      touchScrolling=true;
    }
    if(sliderGesture)setBrightnessFromTouch(x);
    else if(homeSliderDragging){
      homeSliderOffset = constrain(x - homeSliderStartX, 0, 116);
      uiRevision++;
    } else if(controlSliderDragging){
      controlSliderOffset = constrain(x - controlSliderStartX, -116, 0);
      uiRevision++;
    } else if(moved){
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
          uint32_t now = millis();
          if (lastTouchYMs > 0 && now > lastTouchYMs) {
            uint32_t dt = now - lastTouchYMs;
            if (dt <= 60) {
              float v = (float)(lastTouchY - y) / (float)dt * 16.0f;
              scrollVelocity = 0.45f * scrollVelocity + 0.55f * v;
            }
          }
          lastTouchY = y;
          lastTouchYMs = now;
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
        uint32_t now = millis();
        if (lastTouchYMs > 0 && now > lastTouchYMs) {
          uint32_t dt = now - lastTouchYMs;
          if (dt <= 60) {
            float v = (float)(lastTouchY - y) / (float)dt * 16.0f;
            scrollVelocity = 0.45f * scrollVelocity + 0.55f * v;
          }
        }
        lastTouchY = y;
        lastTouchYMs = now;
        int maxS = getAppsMaxScroll();
        int offset=constrain(startScroll+startY-y,0,maxS);
        if(offset!=appsScroll){appsScroll=offset;uiRevision++;}
      } else if(welcomeOpen && !quickOpen && homePage == HOME_SETTINGS && settingsPage == SET_MAIN){
        uint32_t now = millis();
        if (lastTouchYMs > 0 && now > lastTouchYMs) {
          uint32_t dt = now - lastTouchYMs;
          if (dt <= 60) {
            float v = (float)(lastTouchY - y) / (float)dt * 16.0f;
            scrollVelocity = 0.45f * scrollVelocity + 0.55f * v;
          }
        }
        lastTouchY = y;
        lastTouchYMs = now;
        int offset=constrain(startScroll+startY-y,0,140);
        if(offset!=settingsScroll){settingsScroll=offset;uiRevision++;}
      } else if (quickOpen) {
        uint32_t now = millis();
        if (lastTouchYMs > 0 && now > lastTouchYMs) {
          uint32_t dt = now - lastTouchYMs;
          if (dt <= 60) {
            float v = (float)(lastTouchY - y) / (float)dt * 16.0f;
            scrollVelocity = 0.45f * scrollVelocity + 0.55f * v;
          }
        }
        lastTouchY = y;
        lastTouchYMs = now;
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
    if(millis()-candidateMs<10)return;
    held = true;candidateX=-1;
    if (fabsf(scrollVelocity) > 0.8f) {
      scrollVelocity = 0.0f; // Finger catch halts running glide
      moved = true;          // Prevent accidental item tap
    }
    lastTouchY = y;
    lastTouchYMs = millis();
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
      sliderGesture=y+quickScroll>=106 && y+quickScroll<160 && x>=36 && x<225;
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
    } else if(welcomeOpen && !quickOpen && homePage == HOME_MAIN && y >= 28 && y < 320){
      rootGesture=true;moved=false;startX=endX=x;startY=endY=y;
      sliderGesture=false;
      if (y >= 186 && y < 248 && x >= 4 && x <= 8 + homeSliderOffset + 116) {
        homeSliderDragging = true;
        homeSliderStartX = x;
        homeSliderOffset = 0;
      }
    } else if(!welcomeOpen && !quickOpen && currentScreen == SCR_CONTROLS && y >= 226 && y < 288 && x >= 110) {
      rootGesture = true; moved = false; startX = endX = x; startY = endY = y;
      sliderGesture = false;
      controlSliderDragging = true;
      controlSliderStartX = x;
      controlSliderOffset = 0;
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
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // 1. Live Activity Banner Tile (Top, rounded card)
  display.fillRoundRect(8, 30-offset, 224, 68, 20, 0xF800); // Vibrant Coral Red
  drawCustomIcon(display, 18, 38-offset, 0xFFFF, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 24, 24);
  label(48, 44-offset, runningMode ? "OUTDOOR RUNNING" : "OUTDOOR CYCLING", 1, 0xFFFF);
  label(18, 68-offset, durationText(), 3, 0xFFFF);

  // Play/Pause circular white button on right of banner
  display.fillCircle(196, 64-offset, 18, 0xFFFF);
  if (rideStarted && !ridePaused) {
    display.fillRect(191, 56-offset, 4, 16, 0xF800);
    display.fillRect(198, 56-offset, 4, 16, 0xF800);
  } else {
    display.fillTriangle(192, 56-offset, 192, 72-offset, 205, 64-offset, 0xF800);
  }

  // 2. Brightness Slider Capsule (224x54, R=27, matching homepage/activity pills)
  int brightY = 106 - offset;
  display.fillRoundRect(8, brightY, 224, 54, 27, cardBg);
  if (!darkTheme) display.drawRoundRect(8, brightY, 224, 54, 27, 0xD69A);
  drawCustomIcon(display, 22, brightY + 15, uiFg(), CustomIcon::brightnessIcon, 24, 24);
  display.fillRoundRect(56, brightY + 24, 146, 6, 3, darkTheme ? 0x31A6 : 0xD69A);
  int thumb = 56 + (brightnessPercent - 5) * 146 / 95;
  display.fillRoundRect(56, brightY + 24, max(4, thumb - 56), 6, 3, TEAL);
  display.fillCircle(thumb, brightY + 27, 8, uiFg());

  // 3. 2x4 Grid of Symmetrical Capsule Tiles (108x54, R=27, 8px col gap, 10px row gap)
  // Showing ONLY icons and numericals (no text labels!)
  
  // Row 0: Wi-Fi & Bluetooth (y = 168 - offset)
  int r0Y = 168 - offset;
  // Tile 0: Wi-Fi Hotspot
  {
    int x = 8, y = r0Y;
    bool wifiOn = (WiFi.getMode() != WIFI_OFF);
    uint16_t bg = wifiOn ? (darkTheme ? 0x09A5 : 0xCFF7) : cardBg;
    uint16_t fg = wifiOn ? (darkTheme ? 0x475F : 0x0395) : uiFg();
    display.fillRoundRect(x, y, 108, 54, 27, bg);
    if (!darkTheme) display.drawRoundRect(x, y, 108, 54, 27, 0xD69A);
    drawCustomIcon(display, x + (108 - 28) / 2, y + (54 - 28) / 2, fg, CustomIcon::hotSpotIcon, 28, 28);
    if (wifiOn) display.fillCircle(x + 108 - 20, y + 16, 3, 0x07E0);
  }

  // Tile 1: Bluetooth (Icon + Connected Count)
  {
    int x = 124, y = r0Y;
    int bCount = bleCount() + (blePhoneConnected() ? 1 : 0);
    bool active = (bCount > 0);
    uint16_t bg = active ? (darkTheme ? 0x0940 : 0xD753) : cardBg;
    uint16_t fg = active ? (darkTheme ? 0x7FE0 : 0x2400) : uiFg();
    display.fillRoundRect(x, y, 108, 54, 27, bg);
    if (!darkTheme) display.drawRoundRect(x, y, 108, 54, 27, 0xD69A);
    String bStr = String(bCount);
    int numW = textWidth(bStr, 3);
    int blockW = 26 + 10 + numW;
    int startX = x + (108 - blockW) / 2;
    drawBluetoothStateIcon(display, startX, y + (54 - 26) / 2, fg, active, 26, 26);
    label(startX + 36, y + 18, bStr, 3, fg);
  }

  // Row 1: GPS & Theme Toggle (y = 232 - offset)
  int r1Y = 232 - offset;
  // Tile 2: GPS (Icon + Satellite Count)
  {
    int x = 8, y = r1Y;
    bool locked = ride.gpsFixed;
    int sats = startSatellites();
    uint16_t bg = locked ? (darkTheme ? 0x0940 : 0xD753) : cardBg;
    uint16_t fg = locked ? (darkTheme ? 0x7FE0 : 0x2400) : uiFg();
    display.fillRoundRect(x, y, 108, 54, 27, bg);
    if (!darkTheme) display.drawRoundRect(x, y, 108, 54, 27, 0xD69A);
    String satStr = String(sats);
    int numW = textWidth(satStr, 3);
    int blockW = 26 + 10 + numW;
    int startX = x + (108 - blockW) / 2;
    drawSatelliteIcon(display, startX, y + (54 - 26) / 2, fg, 26, 26);
    label(startX + 36, y + 18, satStr, 3, fg);
  }

  // Tile 3: Theme Toggle (Icon Only)
  {
    int x = 124, y = r1Y;
    display.fillRoundRect(x, y, 108, 54, 27, cardBg);
    if (!darkTheme) display.drawRoundRect(x, y, 108, 54, 27, 0xD69A);
    drawCustomIcon(display, x + (108 - 28) / 2, y + (54 - 28) / 2, uiFg(), CustomIcon::themeSwitcherIcon, 28, 28);
  }

  // Row 2: System Diagnostics & Configuration (y = 296 - offset)
  int r2Y = 296 - offset;
  // Tile 4: System Diagnostics (Icon Only)
  {
    int x = 8, y = r2Y;
    display.fillRoundRect(x, y, 108, 54, 27, cardBg);
    if (!darkTheme) display.drawRoundRect(x, y, 108, 54, 27, 0xD69A);
    drawCustomIcon(display, x + (108 - 28) / 2, y + (54 - 28) / 2, uiFg(), CustomIcon::systemIcon, 28, 28);
  }

  // Tile 5: Configuration (Icon Only)
  {
    int x = 124, y = r2Y;
    display.fillRoundRect(x, y, 108, 54, 27, cardBg);
    if (!darkTheme) display.drawRoundRect(x, y, 108, 54, 27, 0xD69A);
    drawCustomIcon(display, x + (108 - 28) / 2, y + (54 - 28) / 2, uiFg(), CustomIcon::configureIcon, 28, 28);
  }

  // Row 3: Lock Screen & Power Off (y = 360 - offset)
  int r3Y = 360 - offset;
  // Tile 6: Lock Screen (Icon Only)
  {
    int x = 8, y = r3Y;
    display.fillRoundRect(x, y, 108, 54, 27, cardBg);
    if (!darkTheme) display.drawRoundRect(x, y, 108, 54, 27, 0xD69A);
    drawCustomIcon(display, x + (108 - 28) / 2, y + (54 - 28) / 2, uiFg(), CustomIcon::lockIcon, 28, 28);
  }

  // Tile 7: Power Off (Icon Only, Red Accent)
  {
    int x = 124, y = r3Y;
    uint16_t pwrBg = darkTheme ? 0x3800 : 0xFDD0;
    display.fillRoundRect(x, y, 108, 54, 27, pwrBg);
    if (!darkTheme) display.drawRoundRect(x, y, 108, 54, 27, 0xF800);
    drawCustomIcon(display, x + (108 - 28) / 2, y + (54 - 28) / 2, 0xF800, CustomIcon::powerIcon, 28, 28);
  }
  // Scrolled content can overlap the header. Repaint only its strip,
  // not the whole page: a full clear here erases every settings tile.
  statusBar(false, false);
  drawCustomIcon(display,108,12,0x8430,CustomIcon::downArrowIcon);
  // Scrollbar removed per user design request

}
