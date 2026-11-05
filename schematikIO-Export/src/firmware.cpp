// UART RX44/TX43 and shared I2C SDA11/SCL10 verified against Waveshare connector table.
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_BMP3XX.h>
#include <Adafruit_GFX.h>
#include <TinyGPSPlus.h>

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "geprc.h"
#include "boot_logo.h"
#include "smooth_fonts.h"
#include <esp_sleep.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

// Waveshare ESP32-S3-Touch-LCD-2.8 V2 with CST3530 touch.
#define GPS_RX_PIN 44
#define GPS_TX_PIN 43
#define COMPASS_SDA 11
#define COMPASS_SCL 10
#define SD_SCK_PIN 14
#define SD_MOSI_PIN 17
#define SD_MISO_PIN 16
#define SD_CS_PIN 21
#define RMT_UP 0
#define RMT_DN 1
#define RMT_LT 2
#define RMT_RT 3
#define DISP_W 240
#define DISP_H 320
#define BLACK ST77XX_BLACK
#define WHITE ST77XX_WHITE
// ─── Data structures ─────────────────────────────────────────────────────────
struct RideData {
  float    speedKmh     = 0;
  float    distanceKm   = 0;
  float    altM         = 0;
  float    gradePercent = 0;
  float    lat          = 0, lon = 0;
  int      cadenceRpm   = 0;
  int      powerW       = 0;
  float    vo2est       = 0;
  int      heading      = 0;
  uint32_t elapsedSec   = 0;
  float    avgSpeedKmh  = 0;
  float    maxSpeedKmh  = 0;
  int      maxPowerW    = 0;
  bool     gpsFixed     = false;
};

struct Config {
  float gearRatio       = 50.0f / 17.0f;
  float wheelCircumM    = 2.096f;
  float riderMassKg     = 75.0f;
  float maxSpeedKmh     = 60.0f;
  float maxPowerW       = 400.0f;
  float maxCadRpm       = 120.0f;
  float vo2maxManual    = 0.0f;
  float ageYears=0, restingHR=0, maximumHR=0;
  int calorieSex=0; // 0 unset, 1 male equation, 2 female equation
  bool massConfirmed=false;
  float targetDistanceKm= 0.0f;   // 0 = no target set
};

struct SensorFlags {
  bool  cadencePhysical = false;
  bool  speedPhysical   = false;
  int   physCadenceRpm  = 0;
  float physSpeedKmh    = 0;
};

struct NavState {
  float destLat         = 0, destLon = 0;
  bool  hasDestination  = false;
  float remainDistKm    = 0;
  int   bearingDeg      = 0;
  int   routeIndex      = 0;
};

struct Btn {
  uint8_t  pin;
  bool     last    = HIGH;
  uint32_t lastMs  = 0;
  bool     pressed = false;
};

struct RoutePoint { float lat, lon; };
struct TrackPoint  { float lat, lon, alt; uint32_t sec; };

enum Screen { SCR_DASH, SCR_MAP, SCR_METRICS, SCR_COUNT };
bool darkTheme=false;
uint32_t uiRevision=0;

// ─── Globals ─────────────────────────────────────────────────────────────────

// Forward declarations
static void drawArcOutline(int cx, int cy, int r, int startDeg, int endDeg);
static const char* cardinalStr(int hdg);

SPIClass lcdSPI(FSPI);
// Compose off-screen, then transfer only changed 16x16 tiles. Never clear
// the physical LCD between frames (the old direct clear caused visible wipes).
class CycleDisplay : public GFXcanvas16 {
 Adafruit_ST7789 panel;
 uint32_t shown[300]={0}; // Tile fingerprints: 2.4 KB, not a second 154 KB framebuffer.
 bool first=true;
 bool logoOverlay=false;
 bool logoLight=false;
 int logoX=0,logoY=0;
 uint8_t *compact=nullptr;
 // Ten neutral shades preserve card contrast and antialiased text even without PSRAM.
 static uint16_t palette(int i){
   static const uint16_t colors[16]={0x0000,0x18C3,0x3186,0x528A,0x738E,0x9492,0xB5B6,0xD6BA,0xEF7D,0xFFFF,0xF800,0x3DF7,0x2564,0xFA60,0x7FE0,0x0940};
   return colors[i];
 }
 static uint8_t pack(uint16_t c){
   int best=0,dist=100000;
   for(int i=0;i<16;i++){
     uint16_t v=palette(i);int r=int(c>>11)-int(v>>11),g=int((c>>5)&63)-int((v>>5)&63),b=int(c&31)-int(v&31);
     int d=2*r*r+g*g+2*b*b;if(d<dist){dist=d;best=i;}
   }return best;
 }
 void putCompact(int i,uint8_t c){uint8_t &v=compact[i/2];if(i&1)v=(v&0xF0)|c;else v=(v&15)|(c<<4);}
 uint16_t pixelAt(int i)const{
   // Read the embedded RGB565 artwork directly, never through the 16-colour palette.
   if(logoOverlay){
     int x=i%DISP_W-logoX;
     int y=i/DISP_W-logoY;
     if(x>=0 && x<BOOT_LOGO_WIDTH && y>=0 && y<BOOT_LOGO_HEIGHT)
       return pgm_read_word(&(logoLight?HOME_LOGO_LIGHT_DATA:BOOT_LOGO_DATA)[y*BOOT_LOGO_WIDTH+x]);
   }
   return buffer?buffer[i]:palette((compact[i/2]>>((i&1)?0:4))&15);
 }

public:
 CycleDisplay(): GFXcanvas16(DISP_W,DISP_H,false),panel(&lcdSPI,42,41,39) {}
 ~CycleDisplay(){heap_caps_free(buffer);free(compact);}
 void init(int w,int h){
   // Allocate after PSRAM initialization, never in a global constructor.
   // Keep scarce internal SRAM available to Wi-Fi/BLE and DMA.
   buffer=(uint16_t*)heap_caps_calloc(w*h,sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
   if(!buffer){
     // Some build targets do not enable the board's external RAM. Use an indexed colour buffer
     // rather than taking another 154 KB from the radio's internal heap.
     compact=(uint8_t*)calloc((w*h+1)/2,1);
     Serial.println("Display: PSRAM unavailable; using indexed low-memory mode (38400 bytes)");
     if(!compact){Serial.println("Display allocation failed");while(true)delay(1000);}
   }
   panel.init(w,h);
   // Rotate the physical panel only; keep the canvas and tile coordinates unchanged.
   // Portrait orientation with the USB-C connector below the screen.
   panel.setRotation(2);
 }
 void drawPixel(int16_t x,int16_t y,uint16_t color) override {
   if(buffer){GFXcanvas16::drawPixel(x,y,color);return;}
   if(compact && x>=0 && y>=0 && x<DISP_W && y<DISP_H)putCompact(y*DISP_W+x,pack(color));
 }
 void fillScreen(uint16_t color) override {
   logoOverlay=false;
   if(buffer)GFXcanvas16::fillScreen(color);
   else if(compact)memset(compact,pack(color)*17,DISP_W*DISP_H/2);
 }
 void drawFastHLine(int16_t x,int16_t y,int16_t w,uint16_t c) override {
   if(buffer){GFXcanvas16::drawFastHLine(x,y,w,c);return;}
   if(!compact || y<0 || y>=DISP_H)return;
   int a=max(0,int(x)),b=min(DISP_W,int(x)+w);
   uint8_t v=pack(c);for(int col=a;col<b;col++)putCompact(y*DISP_W+col,v);
 }
 void drawFastVLine(int16_t x,int16_t y,int16_t h,uint16_t c) override {
   if(buffer){GFXcanvas16::drawFastVLine(x,y,h,c);return;}
   if(!compact || x<0 || x>=DISP_W)return;
   uint8_t v=pack(c);for(int r=max(0,int(y));r<min(DISP_H,int(y)+h);r++)putCompact(r*DISP_W+x,v);
 }
 uint16_t backgroundPixel(int x,int y)const{return (x>=0&&x<DISP_W&&y>=0&&y<DISP_H)?pixelAt(y*DISP_W+x):0;}
 void drawBrandLogo(int x,int y,bool light=false){
   logoLight=light;
   logoX=x;logoY=y;logoOverlay=true;
 }
 void showBrandLogo(){
   fillScreen(0x0000);
   drawBrandLogo((DISP_W-BOOT_LOGO_WIDTH)/2,(DISP_H-BOOT_LOGO_HEIGHT)/2);
   refresh();
   logoOverlay=false;
   first=true; // The next normal frame must replace the logo, including black tiles.
 }
 void invertDisplay(bool v){panel.invertDisplay(v);}
 void refresh(){
   if(!buffer && !compact)return;
   // Compare tiles, merge adjacent changed tiles into one 16-row transfer.
   // Reuse fixed scratch storage; do not allocate during a refresh.
   static uint16_t strip[DISP_W*16];
   for(int y=0;y<DISP_H;y+=16){
     bool dirty[DISP_W/16];
     for(int x=0;x<DISP_W;x+=16){
       uint32_t hash=2166136261UL;
       for(int r=0;r<16;r++)for(int c=0;c<16;c++)
         hash=(hash^pixelAt((y+r)*DISP_W+x+c))*16777619UL;
       int index=(y/16)*(DISP_W/16)+x/16;
       dirty[x/16]=first || shown[index]!=hash;
       shown[index]=hash;
     }
     for(int t=0;t<DISP_W/16;){
       if(!dirty[t]){++t;continue;}
       int begin=t;while(t<DISP_W/16 && dirty[t])++t;
       int x=begin*16,w=(t-begin)*16;
       for(int r=0;r<16;r++)for(int c=0;c<w;c++)strip[r*w+c]=pixelAt((y+r)*DISP_W+x+c);
       panel.drawRGBBitmap(x,y,strip,w,16);
     }
   }
   first=false;
 }
} display;

#include "icons.h"
Adafruit_MCP23X17 buttons;
Adafruit_BMP3XX bmp;
bool buttonsOk=false, bmpOk=false, externalBusReady=false;
static bool externalPresent(uint8_t address){
 if(!externalBusReady)return false;
 Wire.beginTransmission(address);
 uint8_t error=Wire.endTransmission();
 if(error && error!=2){
   Serial.printf("External I2C: address 0x%02X, error %u; restarting bus\n",address,error);
   Wire.end();
   externalBusReady=Wire.begin(COMPASS_SDA,COMPASS_SCL,100000);
   Wire.setTimeOut(30);
 }
 return error==0;
}
static bool beginBarometer(){
 if(externalPresent(0x77))return bmp.begin_I2C(0x77,&Wire);
 if(externalPresent(0x76))return bmp.begin_I2C(0x76,&Wire);
 return false;
}
uint32_t sensorMs=0, gpsByteMs=0;
bool gpsDataReady=false;

TinyGPSPlus       gps;
GeprcGps geprcGps;
GeprcCompass compass;
HardwareSerial    gpsSerial(1);

WebServer         server(80);
Preferences       prefs;
// Save only actual changes, not every rendered frame.
static void setDarkTheme(bool value){
  if(darkTheme==value)return;
  darkTheme=value;
  Preferences themePrefs;
  if(themePrefs.begin("cyclo",false)){
    if(themePrefs.putBool("darkTheme",value)!=1)Serial.println("Theme save failed");
    themePrefs.end();
  }else Serial.println("Theme storage unavailable");
}
// Built-in socket uses the native one-bit SDMMC host, not SPI.

RideData    ride;
Config      cfg;
SensorFlags sensors;
uint32_t externalSensorMs=0;
NavState    nav;

#define MAX_ROUTE 512
RoutePoint route[MAX_ROUTE];
int        routeLen = 0;

#define MAX_TRACK 1024
TrackPoint track[MAX_TRACK];
int        trackLen = 0;

Btn rmtUp{RMT_UP}, rmtDn{RMT_DN}, rmtLt{RMT_LT}, rmtRt{RMT_RT};

bool    sdOk          = false;
String sdStatus = "Not initialized";
Screen  currentScreen = SCR_DASH;
uint8_t mapZoom       = 2;    // 1–5

// Ride accumulators
static uint64_t hrSum=0, powerSum=0;
static uint32_t hrSamples=0, powerSamples=0, statsMs=0;
static int hrMax=0;
static uint32_t rideStartMs  = 0;
static bool     rideStarted  = false;
static float    lastLat      = 0, lastLon = 0, lastAltM = 0;
static float    distAccum    = 0;

// ─── Forward declarations ────────────────────────────────────────────────────
static float haversineKm(float la1, float lo1, float la2, float lo2);
static int   bearingTo(float la1, float lo1, float la2, float lo2);

static void  updateBtn(Btn &b);

static void  drawArcFill(int cx, int cy, int r, int startDeg, int spanDeg, int thicc);
static void  drawProgressBar(int x, int y, int w, int h, float frac);
static void  drawDividerH(int y, int x0 = 0, int x1 = DISP_W - 1);
static void  drawDashScreen();
static void  drawMapScreen();
static void  drawSplash();
static void  handleRoot();
static void  handleStatus();
static void  handleNavSet();
static void  handleUploadRoute();
static void  handleConfig();
static void  handleSensor();
static void  handleExportGPX();
static bool activityRecording();

// ═══════════════════════════════════════════════════════════════════════════════
//  Maths
// ═══════════════════════════════════════════════════════════════════════════════
static float haversineKm(float la1, float lo1, float la2, float lo2) {
  const float R = 6371.0f;
  float dLat = radians(la2 - la1);
  float dLon = radians(lo2 - lo1);
  float a = sinf(dLat/2)*sinf(dLat/2)
           + cosf(radians(la1))*cosf(radians(la2))*sinf(dLon/2)*sinf(dLon/2);
  return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

static int bearingTo(float la1, float lo1, float la2, float lo2) {
  float y = sinf(radians(lo2-lo1)) * cosf(radians(la2));
  float x = cosf(radians(la1))*sinf(radians(la2))
            - sinf(radians(la1))*cosf(radians(la2))*cosf(radians(lo2-lo1));
  return (int)(degrees(atan2f(y, x)) + 360) % 360;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Button debounce
// ═══════════════════════════════════════════════════════════════════════════════
static void updateBtn(Btn &b) {
  bool cur = buttonsOk ? buttons.digitalRead(b.pin) : HIGH;
  if (cur != b.last && (millis() - b.lastMs) > 40) {
    b.lastMs = millis(); b.last = cur;
    if (cur == LOW) b.pressed = true;
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  VCOM ISR  — 2 Hz, prevents Sharp Memory LCD burn-in
// ═══════════════════════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════════════════
//  Drawing primitives
// ═══════════════════════════════════════════════════════════════════════════════

// Arc: startDeg=0 at top, clockwise. spanDeg = how far to fill (0-360).
// thicc = stroke width in pixels.
static void drawArcFill(int cx, int cy, int r, int startDeg, int spanDeg, int thicc) {
  spanDeg = constrain(spanDeg, 0, 360);
  for (int t = startDeg; t <= startDeg + spanDeg; t++) {
    float rad = radians(t - 90);
    for (int d = 0; d < thicc; d++) {
      int x = cx + (int)((r - d) * cosf(rad));
      int y = cy + (int)((r - d) * sinf(rad));
      if (x >= 0 && x < DISP_W && y >= 0 && y < DISP_H)
        display.drawPixel(x, y, BLACK);
    }
  }
}

// Thin outline arc (background ring)
static void drawArcOutline(int cx, int cy, int r, int startDeg, int endDeg) {
  for (int t = startDeg; t <= endDeg; t++) {
    float rad = radians(t - 90);
    int x = cx + (int)(r * cosf(rad));
    int y = cy + (int)(r * sinf(rad));
    if (x >= 0 && x < DISP_W && y >= 0 && y < DISP_H)
      display.drawPixel(x, y, BLACK);
  }
}

// Rounded-corner progress bar
static void drawProgressBar(int x, int y, int w, int h, float frac) {
  frac = constrain(frac, 0.0f, 1.0f);
  display.drawRect(x, y, w, h, BLACK);
  int fill = (int)(frac * (w - 4));
  if (fill > 0) display.fillRect(x + 2, y + 2, fill, h - 4, BLACK);
}

static void drawDividerH(int y, int x0, int x1) {
  display.drawLine(x0, y, x1, y, BLACK);
}

// Cardinal direction from bearing
static const char* cardinalStr(int hdg) {
  static const char* c[] = {"N","NE","E","SE","S","SW","W","NW"};
  return c[((hdg + 22) % 360) / 45];
}

// ═══════════════════════════════════════════════════════════════════════════════
//  DASHBOARD & SCREENS (Figma-Matched Design)
// ═══════════════════════════════════════════════════════════════════════════════
static uint16_t uiBg()   { return darkTheme ? 0x0000 : 0xFFFF; }
static uint16_t uiCard() { return darkTheme ? 0x18C3 : 0xDEFB; }
static uint16_t uiFg()   { return darkTheme ? 0xFFFF : 0x18C3; }
static uint16_t uiMuted(){ return 0x8410; }
static const uint16_t TEAL = 0x3DF7, ORANGE = 0xFA60;

// Native-resolution, 4-bit coverage glyphs, blended over the actual card background.
// Use one shared baseline and measured advances; no enlarged 5x7 bitmap text.
static const SmoothFont& textFont(int size){return size==0?smoothFont12:size<=1?smoothFont10:size==2?smoothFont18:smoothFont28;}
static int textWidth(const String& v,int size){const auto& f=textFont(size);int w=0;for(unsigned char c:v){if(c<32||c>126)c='?';w+=f.glyphs[c-32].advance;}return (w+8)/16;}
static uint16_t blendText(uint16_t fg,uint16_t bg,int a){
 int r=((fg>>11)*a+(bg>>11)*(15-a)+7)/15;
 int g=(((fg>>5)&63)*a+((bg>>5)&63)*(15-a)+7)/15;
 int b=((fg&31)*a+(bg&31)*(15-a)+7)/15;
 return (r<<11)|(g<<5)|b;
}
static void label(int x,int y,const String& v,int size=1,uint16_t color=0){
 const auto& f=textFont(size);uint16_t fg=color?color:uiFg();int pen=x*16;
 for(unsigned char c:v){if(c<32||c>126)c='?';const auto& g=f.glyphs[c-32];
   int gx=(pen+8)/16+g.x,gy=y+f.cap-g.h-g.y;
   for(int row=0;row<g.h;row++)for(int col=0;col<g.w;col++){
     int i=row*g.w+col;uint8_t bits=f.bits[g.offset+i/2];int a=(bits>>((i&1)?0:4))&15;
     if(a)display.drawPixel(gx+col,gy+row,blendText(fg,display.backgroundPixel(gx+col,gy+row),a));
   }pen+=g.advance;
 }
}
static void centeredLabel(int x,int y,int width,const String& v,int size=1,uint16_t color=0){
 label(x+(width-textWidth(v,size))/2,y,v,size,color);
}

static void card(int x, int y, int w, int h, uint16_t color) {
  display.fillRoundRect(x, y, w, h, 8, color);
}

static String durationText() {
  char t[16];
  snprintf(t, sizeof(t), "%02lu:%02lu:%02lu",
           (unsigned long)ride.elapsedSec / 3600,
           (unsigned long)(ride.elapsedSec / 60) % 60,
           (unsigned long)ride.elapsedSec % 60);
  return t;
}

#include "ble_sensors.h"

static void statusBar(bool showStatusIcons = true, bool clearPage = true) {
  if (clearPage) display.fillScreen(uiBg());
  else display.fillRect(0, 0, DISP_W, 29, uiBg());
  // Kolkata: UTC+05:30 all year. Keep UTC internally for activity files.
  String clock = "--:--";
  String meridiem;
  time_t now = time(nullptr);
  if (now >= 1704067200) {
    struct tm local;
    localtime_r(&now, &local);
    char t[16];
    snprintf(t, sizeof(t), "%d:%02d", local.tm_hour % 12 == 0 ? 12 : local.tm_hour % 12, local.tm_min);
    clock = t;
    meridiem = local.tm_hour < 12 ? "AM" : "PM";
  }
  String clockText = meridiem.length() ? (clock + " " + meridiem) : clock;
  label(12, 10, clockText, 0, uiFg());

  // Quick Settings uses a chevron instead of the four centre indicators.
  if (showStatusIcons) {
  // Centre the visible 16px icons as a group, not a fixed left anchor.
  const bool recording = activityRecording();
  const int iconSize = 16, gap = 6;
  const int count = recording ? 4 : 3;
  int x = (DISP_W - (count * iconSize + (count - 1) * gap)) / 2;
  const bool blinkOn = (millis() / 500) % 2;
  const uint16_t unavailable = blinkOn ? 0xF800 : uiBg();
  // Only a deliberately disabled radio is grey. Missing hardware, no fix,
  // or no sensor connection must not look like a manually disabled feature.
  const wifi_mode_t mode = WiFi.getMode();
  const bool wifiOff = mode == WIFI_OFF;
  const bool apReady = (mode == WIFI_AP || mode == WIFI_AP_STA) &&
                       uint32_t(WiFi.softAPIP()) != 0;
  const bool wifiAvailable = WiFi.status() == WL_CONNECTED || apReady;
  if (recording) {
    drawBikeIcon(display, x, 5, blinkOn ? 0xF800 : uiBg());
    x += iconSize + gap;
  }
  drawSatelliteIcon(display, x, 5, ride.gpsFixed ? uiFg() : unavailable);
  x += iconSize + gap;
  drawBluetoothStateIcon(display, x, 5, bleCount() > 0 ? uiFg() : unavailable, bleCount() > 0);
  x += iconSize + gap;
  drawWifiStateIcon(display, x, 5, wifiOff ? uiMuted() : (wifiAvailable ? uiFg() : unavailable), wifiAvailable);

  }

  // Battery Status
  const String batteryText = "N/A";
  label(206 - 4 - textWidth(batteryText, 1), 9, batteryText, 1, uiMuted());
  drawBatteryIcon(display, 206, 7, 0, uiFg());
  display.drawLine(206, 6, 228, 19, uiMuted());
  display.drawLine(206, 19, 228, 6, uiMuted()); // Status unavailable, not proof of disconnection
}

// One binary FIT activity per SD file; finalized on Save.
bool runningMode=false;
File activityFile;
String activityPath, activityMessage = "Ready - tap Start";
uint32_t activityPoints = 0;
#include "activity_fit.h"
#include "activity_archive.h"
static String paceText(float kmh){
  if(kmh<=0.1f)return "--:--";
  unsigned seconds=lroundf(3600.0f/kmh);
  return String(seconds/60)+":"+(seconds%60<10?"0":"")+String(seconds%60);
}
static bool beginActivityFile() {
  activityPoints=0;
  if(!sdOk){activityMessage="SD required to save activity";return false;}
  time_t now=time(nullptr);
  if(now<1704067200){activityMessage="Waiting for valid GPS / clock time";return false;}
  struct tm local;localtime_r(&now,&local);char stamp[32];
  strftime(stamp,sizeof(stamp),"%Y-%m-%d-%H-%M-%S",&local);
  SD_MMC.mkdir("/activities");
  activityPath=String("/activities/")+(runningMode?"run-":"ride-")+stamp+".fit";
  if(SD_MMC.exists(activityPath)){activityMessage="Start time already exists; retry";return false;}
  activityFile=SD_MMC.open(activityPath,FILE_WRITE);
  if(!activityFile){activityMessage="SD open failed";return false;}
  fitBegin();archiveBegin();activityMessage="Recording FIT to SD";return true;
}
static void saveActivity() {
  if(activityFile){bool ok=fitFinish();archiveFinish(ok);activityMessage=ok?"Saved "+String(activityPoints)+" FIT points":"FIT save failed";}
  else activityMessage="No open activity to save";
  rideStarted=false;
  uiRevision++;
}
#include "quick_settings.h"
static bool activityRecording(){return rideStarted && !ridePaused;}

static void heartCard(int y, int h) {
  int hrVal = bleHRReady ? bleHR : 0;
  int zone = calcHrZone(hrVal);
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];

  uint16_t cardBg = darkTheme ? zc.bgDark : zc.bgLight;
  uint16_t hrFg   = darkTheme ? zc.textDark : zc.textLight;
  uint16_t hrSub  = darkTheme ? zc.subDark : zc.subLight;

  card(6, y, 228, h, cardBg);

  // AVG
  label(16, y + 8, "AVG", 1, hrSub);
  label(16, y + 18, (hrSamples?String(hrSum/hrSamples):String("--")), 2, hrFg);

  // Current HR & Zone
  label(42, y + 18, bleHRReady ? "ZONE " + String(zone) : "NO DATA", 1, hrFg);
  label(84, y + 10, (bleHRReady?String(hrVal):String("--")), 3, hrFg);
  label(144, y + 18, "BPM", 1, hrFg);
  drawHeartIcon(display, 168, y + 17, hrFg);

  // MAX
  label(192, y + 8, "MAX", 1, hrSub);
  label(192, y + 18, (hrSamples?String(hrMax):String("--")), 2, hrFg);

  // 5-Zone Bar with Active Indicator
  if (bleHRReady) drawHRZoneBar(display, 16, y + 54, 208, zone);
}

static void drawDashScreen() {
  statusBar();
  heartCard(29, 68);

  // Hero Speed Card
  int y = 101, h = 171;
  card(6, y, 228, h, uiCard());
  label(16, y + 8, "AVG", 1, uiMuted());
  label(16, y + 18, (ride.elapsedSec?(runningMode?paceText(ride.avgSpeedKmh):String(ride.avgSpeedKmh,1)):String("--")), 2, uiFg());
  label(192, y + 8, "MAX", 1, uiMuted());
  label(180, y + 18, (ride.elapsedSec?(runningMode?paceText(ride.maxSpeedKmh):String(ride.maxSpeedKmh,1)):String("--")), 2, uiFg());

  // Center Arc Gauge (270 degrees sweep)
  int cx = 120, cy = y + 78;
  uint16_t arcBg = darkTheme ? 0x2945 : 0xCE79;
  for (int d = 135; d <= 405; d++) {
    float a = radians((float)d);
    for (int r = 50; r <= 56; r++) {
      display.drawPixel(cx + (int)(r * cosf(a)), cy + (int)(r * sinf(a)), arcBg);
    }
  }

  // Active Progress Arc
  float spVal = ride.speedKmh;
  int fillSpan = (int)(270.0f * constrain(spVal / cfg.maxSpeedKmh, 0.0f, 1.0f));
  for (int d = 135; d <= 135 + fillSpan; d++) {
    float a = radians((float)d);
    for (int r = 50; r <= 56; r++) {
      display.drawPixel(cx + (int)(r * cosf(a)), cy + (int)(r * sinf(a)), TEAL);
    }
  }

  // Bike Icon & Speed Readout inside Arc
  if(runningMode)drawCustomIcon(display,111,cy-36,TEAL,CustomIcon::runIcon);
  else drawBikeIcon(display, 111, cy - 36, TEAL);
  String sp = speedAvailable()?(runningMode?paceText(ride.speedKmh):String(ride.speedKmh,1)):String("--");
  centeredLabel(45, cy - 12, 150, sp, 3, uiFg());
  label(96, cy + 18, (ride.gpsFixed || (!runningMode && (bleSpeedReady || (sensors.speedPhysical && millis()-externalSensorMs<5000)))) ? (runningMode?"MIN/KM":"KM/H") : "NO DATA", 1, uiMuted());

  // Bottom Complications Row
  label(16, y + 128, (rideStarted||activityPoints?String(ride.distanceKm,2):String("--")), 2, uiFg());
  label(16, y + 148, "DISTANCE (KM)", 1, uiMuted());
  label(78, y + 128, durationText(), 2, uiFg());
  label(86, y + 148, "TOTAL TIME", 1, uiMuted());
  label(184, y + 128, (ride.gpsFixed&&trackLen>1?String(ride.gradePercent,0)+"%":String("--")), 2, uiFg());
  label(180, y + 148, "GRADIENT", 1, uiMuted());

  // Compact footer leaves the global tray unobstructed.
  card(6,274,228,14,uiCard());
  label(12,276,(calorieSeconds?"KCAL EST: "+String(calorieEstimate,0):String("KCAL EST: --")),1,uiMuted());
  label(144,276,"SAT: "+(gps.satellites.isValid() && gps.satellites.age()<3000 ? String(gps.satellites.value()):String("--")),1,uiMuted());
}

static void drawMetricsScreen() {
  statusBar();
  // Row 1: Ambient Temp & Heart Rate
  int y = 29;
  card(6, y, 111, 57, uiCard());
  drawTempCloudIcon(display, 18, y + 12, uiFg());
  String tempStr = bmpOk && sensorMs && isfinite(bmp.temperature) ? String(bmp.temperature, 0) + " C" : "--";
  label(46, y + 12, tempStr, 2, uiFg());
  label(18, y + 38, bmpOk && sensorMs ? "SENSOR TEMP" : "TEMP: NO DATA", 1, uiMuted());

  // Dynamic Heart Rate Card matching active zone
  int hrVal = bleHRReady ? bleHR : 0;
  int zone = calcHrZone(hrVal);
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrCardBg = darkTheme ? zc.bgDark : zc.bgLight;
  uint16_t hrCol    = darkTheme ? zc.textDark : zc.textLight;

  card(123, y, 111, 57, hrCardBg);
  drawHeartIcon(display, 134, y + 14, hrCol);
  label(154, y + 12, bleHRReady ? String(bleHR) : "--", 2, hrCol);
  label(194, y + 17, "BPM", 1, hrCol);
  label(148, y + 38, bleHRReady ? "ZONE " + String(zone) : "NO DATA", 1, hrCol);

  // Same card layout, running uses GPS pace instead of cycling power.
  y = 90;
  card(6, y, 228, 57, darkTheme ? 0x2920 : 0xFFF1);
  uint16_t pwrFg = darkTheme ? 0xFFE0 : 0x9400;
  if(runningMode){
    label(16,y+10,"GPS PACE",1,pwrFg);
    label(110,y+10,paceText(ride.speedKmh),2,pwrFg);
    label(16,y+38,"MIN/KM",1,pwrFg);
  }else{
  label(16, y + 8, "AVG", 1, 0x9CE0);
  label(16, y + 18, (powerSamples?String(powerSum/powerSamples):String("--")), 2, pwrFg);
  drawLightningIcon(display, 82, y + 13, pwrFg);
  label(100, y + 12, (blePowerReady?String(ride.powerW)+"W":String("--")), 2, pwrFg);
  label(82, y + 38, blePowerReady ? "POWER" : "NO DATA", 1, pwrFg);
  label(192, y + 8, "MAX", 1, 0x9CE0);
  label(180, y + 18, (powerSamples?String(ride.maxPowerW):String("--")), 2, pwrFg);

  }
  // Row 3 & 4 (2x2 Grid)
  y = 151;
  card(6, y, 111, 57, uiCard());
  display.fillCircle(24, y + 20, 10, ORANGE);
  drawFlameIcon(display, 18, y + 13, 0xFFFF);
  label(42, y + 12, (calorieSeconds?String(calorieEstimate,0):String("--")), 2, uiFg());
  label(16, y + 38, (calorieSeconds?"KCAL EST: "+String(calorieEstimate,0):String("KCAL EST: --")), 1, uiMuted());

  card(123, y, 111, 57, uiCard());
  label(135, y + 12, (speedAvailable()?String(ride.speedKmh,1):String("--")), 2, uiFg());
  label(185, y + 17, "KM/H", 1, uiMuted());
  label(155, y + 38, "SPEED", 1, uiMuted());

  y = 212;
  card(6, y, 111, 57, uiCard());
  label(14, y + 12, durationText(), 2, uiFg());
  label(26, y + 38, "TOTAL TIME", 1, uiMuted());

  card(123, y, 111, 57, uiCard());
  drawCustomIcon(display, 134, y + 12, uiFg(), CustomIcon::routeMapIcon);
  label(150, y + 12, nav.hasDestination && ride.gpsFixed ? String(nav.remainDistKm, 1) + "km" : "--", 2, uiFg());
  label(142, y + 38, "DIRECTIONS", 1, uiMuted());

  label(8,280,"VO2 EST: "+(ride.vo2est>0?String(ride.vo2est,1):String("--")),1,uiMuted());
  label(8, 271, String(bleHRReady ? "HR: BLE" : "HR: NO DATA") + " | " + String(runningMode?"RUN / GPS":blePowerReady ? "PWR: BLE" : "PWR: NO DATA"), 1, uiMuted());
}

static const float zoomRadiusKm[] = {0, 4.0f, 1.5f, 0.5f, 0.2f, 0.08f};
static void drawMapScreen() {
  statusBar();
  card(6, 29, 228, 215, darkTheme ? 0x18C3 : 0xF7DE);

  label(16, 34, "GPS trail - no basemap", 1, uiMuted());
  double lat = ride.gpsFixed ? ride.lat : (routeLen ? route[0].lat : 0);
  double lon = ride.gpsFixed ? ride.lon : (routeLen ? route[0].lon : 0);
  float scale = 90 / zoomRadiusKm[mapZoom];
  auto px = [&](float x) { return (int)(120 + (x - lon) * 111 * cosf(radians(lat)) * scale); };
  auto py = [&](float y) { return (int)(137 - (y - lat) * 111 * scale); };
  auto inside = [](int x, int y) { return x >= 9 && x <= 230 && y >= 48 && y <= 240; };

  // Planned Route in thick Orange
  for (int i = 1; i < routeLen; i++) {
    int x = px(route[i-1].lon), y = py(route[i-1].lat);
    int u = px(route[i].lon), v = py(route[i].lat);
    if (inside(x, y) && inside(u, v)) {
      for (int w = -1; w <= 1; w++) display.drawLine(x + w, y, u + w, v, ORANGE);
    }
  }

  // Live Breadcrumbs in Teal
  for (int i = 1; i < trackLen; i++) {
    int x = px(track[i-1].lon), y = py(track[i-1].lat);
    int u = px(track[i].lon), v = py(track[i].lat);
    if (inside(x, y) && inside(u, v)) display.drawLine(x, y, u, v, TEAL);
  }

  // Pinned User Location at (120, 137): Blue disc with white navigation triangle
  if (ride.gpsFixed) {
  display.fillCircle(120, 137, 13, 0x2A5F);
  display.drawCircle(120, 137, 13, 0xFFFF);
  float h = radians((float)ride.heading);
  display.fillTriangle(120 + 9 * sinf(h), 137 - 9 * cosf(h),
                       120 - 6 * cosf(h) - 4 * sinf(h), 137 - 6 * sinf(h) + 4 * cosf(h),
                       120 + 6 * cosf(h) - 4 * sinf(h), 137 + 6 * sinf(h) + 4 * cosf(h), 0xFFFF);

  }
  // Top-Right Compass Coin Badge
  drawCompassCoin(display, 205, 52, (float)ride.heading);

  // Bottom Card: Speed & Directions
  card(6, 248, 228, 40, uiCard());
  label(16, 262, (speedAvailable()?String(ride.speedKmh,1):String("--")), 3, uiFg());
  label(78, 270, "KM/H", 1, uiMuted());
  

  display.drawLine(120, 256, 120, 284, 0xCE79);

  label(128,251,routeLen>1?"TRACK":"DIRECT",1,uiMuted());
  label(154, 262, nav.hasDestination && ride.gpsFixed ? String(nav.remainDistKm, 1) + "km" : "--", 3, uiFg());
  
}

static void drawSplash() {
 display.showBrandLogo();
 delay(1800);
}

static const char page[] PROGMEM=R"HTML(<!doctype html><meta name="viewport" content="width=device-width"><title>CycloComp</title><style>body{font:16px system-ui;background:#101c28;color:white;max-width:600px;margin:30px auto;padding:16px}input,button,textarea{padding:12px;margin:5px;max-width:90%}pre{white-space:pre-wrap}</style><h1>CycloComp V2</h1><p>Prototype: coordinate guidance, not road routing. Estimates are not measured fitness data.</p><h2>Clock (India)</h2><p>After each power-up, open this page to set the clock from this phone/computer, or wait for real GPS time. The clock does not guess the time.</p><button onclick="syncClock()">Sync time now</button><p id="clockMsg">Clock not yet synced</p><h2>Display</h2><button onclick="post('/ui',{quick:true})">Quick settings</button><button onclick="post('/ui',{quick:false})">Close settings</button><button onclick="post('/ui',{screen:0})">Dashboard</button><button onclick="post('/ui',{screen:2})">Metrics</button><button onclick="post('/ui',{screen:1})">Map</button><button onclick="post('/ui',{dark:false})">Light</button><button onclick="post('/ui',{dark:true})">Dark</button><h2>Bluetooth sensors</h2><p>Wake your sensors. Scan, then select your own sensor. Supports standard BLE heart rate, speed/cadence and cycling power; up to three saved sensors. ANT+ and proprietary profiles are not supported.</p><button onclick="bleAction('scan')">Scan for sensors</button><div id="bleList"></div><p id="bleMsg"></p><h2>Destination</h2><input id="coord" placeholder="latitude, longitude"><button onclick="nav()">Navigate</button><h2>Calculated fitness estimates</h2><p>VO2 estimate = 15.3 x maximumHR / restingHR (adult population estimate). Enter your measured restingHR and known maximumHR. Calories use the HR-based Keytel equation: enter ageYears, mass in kg and calorieSex (1 male equation, 2 female equation). No profile means unavailable; no HR means no calorie accumulation. Estimates are not medical measurements.</p><h2>Configuration JSON</h2><textarea id="conf">{"gearRatio":2.94,"wheelCircumM":2.096,"mass":75,"targetDistanceKm":50,"vo2maxManual":0}</textarea><button onclick="post('/config',JSON.parse(conf.value))">Save</button><h2>Route JSON</h2><input type="file" id="route"><button onclick="upload()">Load route</button><p>Array of {"lat":number,"lon":number}, max 512 points.</p><a href="/export">Download current GPX preview (max 1024 points)</a><p><a href="/log">Download full SD ride CSV</a></p><pre id="out"></pre><script>async function syncClock(){try{let r=await fetch('/time',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({epoch:Math.floor(Date.now()/1000)})});clockMsg.textContent=await r.text()}catch(e){clockMsg.textContent='Sync failed. Check CycloComp Wi-Fi connection.'}}syncClock();setInterval(syncClock,60000);async function bleAction(action,index=-1){let r=await fetch('/ble',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action,index})});bleMsg.textContent=await r.text()}async function bleRefresh(){try{let d=await(await fetch('/ble')).json();bleList.replaceChildren();let title=document.createElement('p');title.textContent=d.ready?(d.busy?'Bluetooth busy...':'Bluetooth ready'):'Hardware not ready';bleList.append(title);for(let x of (d.found||[])){let b=document.createElement('button');b.textContent='Connect '+x.name+' '+x.address;b.onclick=()=>bleAction('connect',x.index);bleList.append(b)}for(let x of (d.saved||[])){if(!x.address)continue;let p=document.createElement('p');p.textContent=x.name+': '+(x.connected?'Connected':x.status);let b=document.createElement('button');b.textContent='Forget';b.onclick=()=>bleAction('forget',x.slot);p.append(b);bleList.append(p)}}catch(e){}}setInterval(bleRefresh,2500);bleRefresh();async function post(p,d){let r=await fetch(p,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});out.textContent=await r.text()}function nav(){let a=coord.value.split(',').map(Number);if(a.length===2&&a.every(Number.isFinite))post('/nav',{lat:a[0],lon:a[1]})}async function upload(){if(route.files[0])post('/upload',JSON.parse(await route.files[0].text()))}setInterval(async()=>{try{out.textContent=JSON.stringify(await(await fetch('/status')).json(),null,2)}catch(e){}},2000)</script>)HTML";
static void handleRoot(){server.send_P(200,"text/html",page);}

static void handleStatus() {
  JsonDocument doc;
  doc["batteryStatus"]="unavailable";
  doc["batteryPercent"]=nullptr; doc["batteryConnected"]=nullptr; doc["batteryCharging"]=nullptr;
  doc["missingReadingPolicy"]="0 means no data when readiness is false";
  doc["activityType"]=runningMode?"running":"cycling";
  doc["paceMinKm"]=runningMode && ride.speedKmh>0.1?60.0f/ride.speedKmh:0;
  doc["speedKmh"]        = ride.speedKmh;
  doc["distKm"]          = ride.distanceKm;
  doc["cadence"]         = ride.cadenceRpm;
  doc["power"]           = ride.powerW;
  doc["vo2maxManual"] = cfg.vo2maxManual;
  if(ride.vo2est>0)doc["vo2maxEstimate"]=ride.vo2est;else doc["vo2maxEstimate"]=nullptr;
  if(calorieSeconds)doc["caloriesEstimate"]=calorieEstimate;else doc["caloriesEstimate"]=nullptr;
  doc["calorieCoverageSeconds"]=calorieSeconds;
  doc["compassReady"]=compass.ready; doc["compassCalibrated"]=compass.calibrated; doc["compassCalibrating"]=compass.calibrating; doc["headingSource"]=compass.usable()?"magnetic-level-only":"GPS course"; doc["compassX"]=compass.raw[0]; doc["compassY"]=compass.raw[1]; doc["compassZ"]=compass.raw[2]; doc["touchReady"]=touchReady; doc["blePairingReady"]=bleReady; doc["bleConnected"]=bleCount(); doc["heartRate"]=bleHRReady?bleHR:0; doc["heartRateReady"]=bleHRReady; doc["powerMeasured"]=!runningMode && blePowerReady; doc["cadenceMeasured"]=!runningMode && bleCadReady; doc["speedMeasured"]=runningMode?ride.gpsFixed:bleSpeedReady; doc["barometerReady"]=bmpOk; doc["gpsDataReady"]=gpsDataReady; doc["buttonsReady"]=buttonsOk; doc["imuReady"]=false;
  doc["heading"]         = ride.heading;
  doc["lat"]             = ride.lat;
  doc["lon"]             = ride.lon;
  doc["altM"]            = ride.altM;
  doc["gradePercent"]    = ride.gradePercent;
  doc["elapsed"]         = ride.elapsedSec;
  doc["avgSpeedKmh"]     = ride.avgSpeedKmh;
  doc["maxSpeedKmh"]     = ride.maxSpeedKmh;
  doc["maxPowerW"]       = ride.maxPowerW;
  doc["gpsFixed"]        = ride.gpsFixed;
  doc["sdOk"]            = sdOk;
  doc["sdStatus"]        = sdStatus;
  doc["cadenceHW"]       = sensors.cadencePhysical;
  doc["speedHW"]         = sensors.speedPhysical;
  doc["targetDistKm"]    = cfg.targetDistanceKm;
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleNavSet() {
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "No body"); return; }
  StaticJsonDocument<64> doc;
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "Bad JSON"); return; }
  if(!doc["lat"].is<double>()||!doc["lon"].is<double>()||fabs(doc["lat"].as<double>())>90||fabs(doc["lon"].as<double>())>180){server.send(400,"text/plain","Latitude -90..90, longitude -180..180 required");return;}
  nav.destLat        = doc["lat"] | 0.0f;
  nav.destLon        = doc["lon"] | 0.0f;
  nav.hasDestination = true;
  nav.remainDistKm   = haversineKm(ride.lat, ride.lon, nav.destLat, nav.destLon);
  nav.bearingDeg     = bearingTo(ride.lat, ride.lon, nav.destLat, nav.destLon);
  server.send(200, "text/plain", "Destination set");
}

static void handleUploadRoute() {
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "No body"); return; }
  DynamicJsonDocument doc(16384);
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "Bad JSON"); return; }
  if(!doc.is<JsonArray>()||doc.size()>MAX_ROUTE){server.send(400,"text/plain","Route must be an array with at most 512 points");return;}
  for(JsonObject pt:doc.as<JsonArray>())if(!pt["lat"].is<double>()||!pt["lon"].is<double>()||fabs(pt["lat"].as<double>())>90||fabs(pt["lon"].as<double>())>180){server.send(400,"text/plain","Invalid point");return;}
  routeLen = 0;
  for (JsonObject pt : doc.as<JsonArray>()) {
    if (routeLen >= MAX_ROUTE) break;
    route[routeLen++] = {pt["lat"].as<float>(), pt["lon"].as<float>()};
  }
  if (routeLen > 0) {
    nav.hasDestination = true;
    nav.destLat = route[routeLen-1].lat;
    nav.destLon = route[routeLen-1].lon;
  }
  nav.routeIndex = 0;
  if (sdOk) {
    File f = SD_MMC.open("/route.json", "w");
    if (f) { serializeJson(doc, f); f.close(); }
  }
  server.send(200, "text/plain", "Route: " + String(routeLen) + " pts");
}

static void handleConfig() {
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "No body"); return; }
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "Bad JSON"); return; }
  if(doc.containsKey("ageYears"))cfg.ageYears=doc["ageYears"];
  if(doc.containsKey("restingHR"))cfg.restingHR=doc["restingHR"];
  if(doc.containsKey("maximumHR"))cfg.maximumHR=doc["maximumHR"];
  if(doc.containsKey("calorieSex"))cfg.calorieSex=constrain(doc["calorieSex"].as<int>(),0,2);
  if(doc.containsKey("mass"))cfg.massConfirmed=true;
  if (doc.containsKey("gearRatio"))       cfg.gearRatio        = doc["gearRatio"];
  if (doc.containsKey("wheelCircumM"))    cfg.wheelCircumM     = doc["wheelCircumM"];
  if (doc.containsKey("mass"))            cfg.riderMassKg      = doc["mass"];
  if (doc.containsKey("maxSpeedKmh"))     cfg.maxSpeedKmh      = doc["maxSpeedKmh"];
  if (doc.containsKey("maxPowerW"))       cfg.maxPowerW        = doc["maxPowerW"];
  if (doc.containsKey("maxCadenceRpm"))   cfg.maxCadRpm        = doc["maxCadenceRpm"];
  if (doc.containsKey("vo2maxManual"))    cfg.vo2maxManual     = doc["vo2maxManual"];
  if (doc.containsKey("targetDistanceKm")) cfg.targetDistanceKm = doc["targetDistanceKm"];
  cfg.gearRatio=constrain(cfg.gearRatio,0.1f,10.0f);cfg.wheelCircumM=constrain(cfg.wheelCircumM,0.5f,3.0f);
  cfg.riderMassKg=constrain(cfg.riderMassKg,20.0f,250.0f);cfg.maxSpeedKmh=constrain(cfg.maxSpeedKmh,10.0f,150.0f);cfg.maxPowerW=constrain(cfg.maxPowerW,50.0f,2500.0f);
  prefs.begin("cyclo", false);
  prefs.putFloat("age",cfg.ageYears);prefs.putFloat("restHR",cfg.restingHR);prefs.putFloat("maxHR",cfg.maximumHR);prefs.putInt("calSex",cfg.calorieSex);prefs.putBool("massSet",cfg.massConfirmed);
  prefs.putFloat("gearRatio",  cfg.gearRatio);
  prefs.putFloat("wheelCircum",cfg.wheelCircumM);
  prefs.putFloat("riderMass",  cfg.riderMassKg);
  prefs.putFloat("maxSpeed",   cfg.maxSpeedKmh);
  prefs.putFloat("maxPower",   cfg.maxPowerW);
  prefs.putFloat("maxCad",     cfg.maxCadRpm);
  prefs.putFloat("vo2max",     cfg.vo2maxManual);
  prefs.putFloat("targetDist", cfg.targetDistanceKm);
  prefs.end();
  server.send(200, "text/plain", "Config saved");
}

static void handleSensor() {
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "No body"); return; }
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "Bad JSON"); return; }
  if (doc.containsKey("cadence"))    sensors.cadencePhysical = doc["cadence"].as<bool>();
  if (doc.containsKey("speed"))      sensors.speedPhysical   = doc["speed"].as<bool>();
  if (doc.containsKey("cadenceRpm")) sensors.physCadenceRpm  = doc["cadenceRpm"];
  if (doc.containsKey("speedKmh"))   sensors.physSpeedKmh    = doc["speedKmh"];
  externalSensorMs=millis();
  server.send(200, "text/plain", "External bridge readings updated (not BLE pairing)");
}

static void handleExportGPX() {
  String gpx = "<?xml version=\"1.0\"?>\n<gpx version=\"1.1\" creator=\"CycloComp\">\n<trk><trkseg>\n";
  for (int i = 0; i < trackLen; i++) {
    gpx += "<trkpt lat=\""; gpx += String(track[i].lat, 6);
    gpx += "\" lon=\"";     gpx += String(track[i].lon, 6);
    gpx += "\"><ele>";       gpx += String(track[i].alt, 1);
    gpx += "</ele></trkpt>\n";

  }
  gpx += "</trkseg></trk>\n</gpx>";
  server.send(200, "application/gpx+xml", gpx);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  setup()
// ═══════════════════════════════════════════════════════════════════════════════
void setup() {
  // USB cannot be electrically switched off. A held key wakes USB deep sleep.
  // Ignore short presses, and require release before arming shutdown again.
  pinMode(6, INPUT);
  if(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_EXT0) {
    uint32_t t=millis();
    while(digitalRead(6)==LOW && millis()-t<1500) delay(10);
    if(digitalRead(6)!=LOW) {
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_6,0);
      esp_deep_sleep_start();
    }
  }
  setenv("TZ", "IST-5:30", 1); // POSIX sign is reversed: Kolkata is east of UTC.
  tzset();
  Serial.begin(115200);
  // Allow native USB to reconnect so startup storage errors remain visible.
  uint32_t usbWait = millis();
  while (!Serial && millis()-usbWait < 1500) delay(10);
  if(prefs.begin("cyclo",true)){
    darkTheme=prefs.getBool("darkTheme",false);
    prefs.end();
  }

  // SYS_EN active high maintains battery power, before peripheral startup.
  pinMode(7, OUTPUT); digitalWrite(7,HIGH);
  pinMode(6, INPUT);
  pinMode(5, OUTPUT);digitalWrite(5,LOW);
  // Reserve controller and radio task memory before optional display allocations.
  bleBegin();
  if(!WiFi.softAP("CycloComp", "cycling1234"))Serial.println("Wi-Fi unavailable: AP initialization failed");
  psramInit();
  lcdSPI.begin(40,-1,45,42);
  display.init(240,320); display.setRotation(0);display.invertDisplay(true);
  digitalWrite(5,HIGH);drawSplash();display.refresh();
  initQuickTouch();
  externalBusReady=Wire.begin(COMPASS_SDA,COMPASS_SCL);
  Wire.setTimeOut(30);
  buttonsOk=externalPresent(0x20) && buttons.begin_I2C(0x20,&Wire);
  if(buttonsOk)for(int i=0;i<4;i++)buttons.pinMode(i,INPUT_PULLUP);
  bmpOk=beginBarometer();
  if(externalPresent(0x0D))compass.begin();
  gpsSerial.setRxBufferSize(2048);
  gpsSerial.begin(115200,SERIAL_8N1,GPS_RX_PIN,GPS_TX_PIN);
  server.on("/compass", HTTP_POST, [](){
    JsonDocument d; if(deserializeJson(d,server.arg("plain"))){server.send(400,"text/plain","Invalid JSON");return;}
    String action=d["action"] | "";
    if(action=="start")compass.start();
    else if(action=="finish"){if(!compass.finish()){server.send(400,"text/plain","Insufficient rotation: rotate entire device level through full circles");return;}}
    else if(action=="alignment"){float y=d["yawOffset"] | 0.f, dec=d["declination"] | 0.f;if(!isfinite(y)||!isfinite(dec)||fabs(y)>360||fabs(dec)>90){server.send(400,"text/plain","Invalid angle");return;}compass.yawOffset=y;compass.declination=dec;compass.save();}
    else {server.send(400,"text/plain","Use action start, finish or alignment");return;}
    server.send(200,"text/plain","OK");
  });

  // Native SDMMC 1-bit: CLK14, CMD17, D0=16. D3=21 stays high.
  // Never format on mount failure: preserve the user's existing files.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(100);
  const uint32_t sdClocksKHz[] = {20000, 4000, 400};
  if (SD_MMC.setPins(SD_SCK_PIN, SD_MOSI_PIN, SD_MISO_PIN)) {
    for (uint32_t khz : sdClocksKHz) {
      Serial.printf("SDMMC: mounting 1-bit at %lu kHz\n", (unsigned long)khz);
      if (SD_MMC.begin("/sd", false, false, khz, 5)) {
        if (SD_MMC.cardType() != CARD_NONE) {
          sdOk = true;
          break;
        }
        SD_MMC.end(); // Safe: only called if begin() succeeded
      }
      sdOk = false;
      delay(100);
    }
  }
  if (sdOk) {
    sdStatus = "Mounted: " + String((unsigned long)(SD_MMC.cardSize() / (1024 * 1024))) + " MiB";
    Serial.printf("SDMMC: filesystem %llu MiB, used %llu MiB\n",
      SD_MMC.totalBytes() / (1024 * 1024), SD_MMC.usedBytes() / (1024 * 1024));
  } else {
    sdStatus = "Mount failed: check seating / FAT32";
  }
  Serial.printf("SDMMC: %s\n", sdStatus.c_str());
  if (sdOk) {
    SD_MMC.mkdir("/maps");
    if (SD_MMC.exists("/route.json")) {
      File f = SD_MMC.open("/route.json", "r");
      if (f) {
        DynamicJsonDocument doc(16384);
        if (!deserializeJson(doc, f)) {
          routeLen = 0;
          for (JsonObject pt : doc.as<JsonArray>()) {
            if (routeLen >= MAX_ROUTE) break;
            route[routeLen++] = {pt["lat"].as<float>(), pt["lon"].as<float>()};
          }
          if (routeLen > 0) {
            nav.hasDestination = true;
            nav.destLat = route[routeLen-1].lat;
            nav.destLon = route[routeLen-1].lon;
          }
        }
        f.close();
      }
    }
  }

  // Load config from NVS
  prefs.begin("cyclo", false);
  cfg.ageYears=prefs.getFloat("age",0);cfg.restingHR=prefs.getFloat("restHR",0);cfg.maximumHR=prefs.getFloat("maxHR",0);cfg.calorieSex=prefs.getInt("calSex",0);cfg.massConfirmed=prefs.getBool("massSet",false);
  cfg.gearRatio         = prefs.getFloat("gearRatio",  cfg.gearRatio);
  cfg.wheelCircumM      = prefs.getFloat("wheelCircum", cfg.wheelCircumM);
  cfg.riderMassKg       = prefs.getFloat("riderMass",  cfg.riderMassKg);
  cfg.maxSpeedKmh       = prefs.getFloat("maxSpeed",   cfg.maxSpeedKmh);
  cfg.maxPowerW         = prefs.getFloat("maxPower",   cfg.maxPowerW);
  cfg.maxCadRpm         = prefs.getFloat("maxCad",     cfg.maxCadRpm);
  cfg.vo2maxManual      = prefs.getFloat("vo2max",     cfg.vo2maxManual);
  cfg.targetDistanceKm  = prefs.getFloat("targetDist", cfg.targetDistanceKm);
  prefs.end();

  // WiFi AP + web server
  server.on("/",       HTTP_GET,  handleRoot);
  // A phone/computer supplies its real UTC epoch; never use build time as a clock.
  server.on("/time", HTTP_POST, [](){
    JsonDocument d;
    if(deserializeJson(d,server.arg("plain")) || !d["epoch"].is<int64_t>()){
      server.send(400,"text/plain","Invalid time");return;
    }
    int64_t epoch=d["epoch"].as<int64_t>();
    if(epoch<1704067200LL || epoch>4102444799LL){server.send(400,"text/plain","Time out of range");return;}
    // Prefer fresh GPS UTC over a companion device's potentially incorrect clock.
    if(!(gps.date.isValid() && gps.time.isValid() && gps.date.age()<3000 && gps.time.age()<3000)){
      timeval tv={};tv.tv_sec=(time_t)epoch;settimeofday(&tv,nullptr);
    }
    uiRevision++;server.send(200,"text/plain","Clock synced: Kolkata IST (UTC+05:30)");
  });
  server.on("/ui",HTTP_POST,[](){
    JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"text/plain","Invalid JSON");return;}
    if(d["screen"].is<int>()){int n=d["screen"];if(n<0||n>=SCR_COUNT){server.send(400,"text/plain","Invalid screen");return;}currentScreen=(Screen)n;}
    if(d["quick"].is<bool>()){quickOpen=d["quick"];quickDetail=0;}
    if(d["dark"].is<bool>())setDarkTheme(d["dark"].as<bool>());
    uiRevision++;server.send(200,"text/plain","Display updated");
  });
  server.on("/status", HTTP_GET,  handleStatus);
  server.on("/nav",    HTTP_POST, handleNavSet);
  server.on("/upload", HTTP_POST, handleUploadRoute);
  server.on("/config", HTTP_POST, handleConfig);
  server.on("/sensor", HTTP_POST, handleSensor);
  server.on("/export", HTTP_GET,  handleExportGPX);
  server.on("/log",HTTP_GET,[](){
    File f=SD_MMC.open("/ride.csv","r"); if(!f){server.send(404,"text/plain","No SD log");return;}
    server.sendHeader("Content-Disposition","attachment; filename=ride.csv");server.streamFile(f,"text/csv");f.close();
  });
  bleHttp();
  server.begin();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  loop()
// ═══════════════════════════════════════════════════════════════════════════════
static uint32_t lastDispMs  = 0;
static uint32_t lastTrackMs = 0;

static File uploadFile;

static void handleSerialTransfer() {
  if (!Serial.available()) return;
  if (Serial.peek() != 'C') {
    while (Serial.available() && Serial.peek() != 'C') Serial.read();
    if (!Serial.available()) return;
  }
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line == "CMD:PING") {
    Serial.println("PONG");
    return;
  }
  if (line.startsWith("CMD:LIST_FILES ")) {
    String dir = line.substring(15);
    File d = SD_MMC.open(dir);
    if (!d || !d.isDirectory()) {
      Serial.println("ERR:NOT_DIR");
      return;
    }
    Serial.println("START_LIST");
    File entry = d.openNextFile();
    while (entry) {
      Serial.printf("%s %u\n", entry.name(), (unsigned int)entry.size());
      entry = d.openNextFile();
    }
    Serial.println("END_LIST");
    d.close();
    return;
  }
  if (line.startsWith("CMD:DELETE_FILE ")) {
    String path = line.substring(16);
    if (SD_MMC.remove(path)) {
      Serial.println("OK:DELETED");
      uiRevision++;
    } else {
      Serial.println("ERR:DELETE_FAILED");
    }
    return;
  }
  if (line.startsWith("CMD:FILE_OPEN ")) {
    String path = line.substring(14);
    if (uploadFile) { uploadFile.flush(); uploadFile.close(); }
    int p = path.lastIndexOf('/');
    if (p > 0) {
      String dir = path.substring(0, p);
      SD_MMC.mkdir(dir);
    }
    SD_MMC.remove(path);
    uploadFile = SD_MMC.open(path, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("ERR:OPEN_FAILED");
    } else {
      Serial.println("OK:OPEN");
    }
    return;
  }
    if (line.startsWith("CMD:HEX ")) {
    if (!uploadFile) {
      Serial.println("ERR:NO_FILE");
      return;
    }
    const char *hex = line.c_str() + 8;
    int hexLen = strlen(hex);
    uint8_t outBuf[256];
    int outLen = 0;
    auto hexVal = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    for (int i = 0; i + 1 < hexLen && outLen < (int)sizeof(outBuf); i += 2) {
      int h1 = hexVal(hex[i]);
      int h2 = hexVal(hex[i + 1]);
      if (h1 >= 0 && h2 >= 0) {
        outBuf[outLen++] = (uint8_t)((h1 << 4) | h2);
      }
    }
    if (outLen > 0) {
      uploadFile.write(outBuf, outLen);
      Serial.println("K");
    } else {
      Serial.println("ERR:HEX");
    }
    return;
  }
  if (line.startsWith("CMD:CHUNK ")) {
    int length = line.substring(10).toInt();
    if (!uploadFile) {
      Serial.println("ERR:NO_FILE");
      return;
    }
    Serial.println("READY");
    int readBytes = 0;
    uint8_t buf[1024];
    uint32_t startMs = millis();
    while (readBytes < length && millis() - startMs < 3000) {
      int avail = Serial.available();
      if (avail > 0) {
        int toRead = min((uint32_t)avail, (uint32_t)(length - readBytes));
        toRead = min((uint32_t)toRead, (uint32_t)sizeof(buf));
        int n = Serial.readBytes((char*)buf, toRead);
        if (n > 0) {
          uploadFile.write(buf, n);
          readBytes += n;
          startMs = millis();
        }
      } else {
        delay(1);
      }
    }
    if (readBytes == length) {
      Serial.println("K");
    } else {
      Serial.printf("ERR:CHUNK %d/%d\n", readBytes, length);
    }
    return;
  }
  if (line == "CMD:FILE_CLOSE") {
    if (uploadFile) {
      uploadFile.flush();
      uploadFile.close();
    }
    uiRevision++;
    Serial.println("OK:CLOSED");
    return;
  }
}

void loop() {
  handleSerialTransfer();
  delay(1); // Yield between UI/input passes; never wait a frame in touch polling.
  bleReadings();
  pollPowerUi();
  // GPS feed
  while (gpsSerial.available()){gpsByteMs=millis();geprcGps.feed(gpsSerial.read(),gps);}
  static uint32_t lastNmeaCount=0, validGpsMs=0;
  if(gps.passedChecksum()!=lastNmeaCount){lastNmeaCount=gps.passedChecksum();validGpsMs=millis();}
  gpsDataReady=(validGpsMs && millis()-validGpsMs<3000) ||
    (geprcGps.lastMessage && millis()-geprcGps.lastMessage<3000);
  // Seed the system clock from fresh GPS UTC, then let it keep ticking through signal loss.
  static uint32_t clockSyncMs=0;
  if(gps.date.isValid() && gps.time.isValid() && gps.date.age()<2000 && gps.time.age()<2000 &&
     (clockSyncMs==0 || millis()-clockSyncMs>=60000) && gps.date.year()>=2024){
    struct tm utc={};
    utc.tm_year=gps.date.year()-1900;utc.tm_mon=gps.date.month()-1;utc.tm_mday=gps.date.day();
    utc.tm_hour=gps.time.hour();utc.tm_min=gps.time.minute();utc.tm_sec=gps.time.second();
    // mktime interprets Kolkata local time: add its offset to recover the UTC epoch.
    time_t epoch=mktime(&utc)+19800;
    timeval tv={};tv.tv_sec=epoch;settimeofday(&tv,nullptr);clockSyncMs=millis();
  }
  geprcGps.poll(gpsSerial);
  compass.update();

  if (gps.location.isValid() && gps.location.isUpdated()) {
    float nLat = gps.location.lat(), nLon = gps.location.lng();

    if (lastLat != 0.0f) {
      float delta = haversineKm(lastLat, lastLon, nLat, nLon);
      if (rideStarted && !ridePaused && delta > 0.001f && delta<0.1f && gps.speed.kmph()>2 && ((geprcGps.lastPvt && millis()-geprcGps.lastPvt<3000)?geprcGps.pdop<3:(gps.hdop.isValid() && gps.hdop.hdop()<3))) {   // ignore sub-metre jitter
        distAccum += delta;
        ride.distanceKm = distAccum;
        // Grade: alt change / horizontal distance
        float altDelta = ride.altM - lastAltM;
        ride.gradePercent = constrain((altDelta / (delta * 1000.0f)) * 100.0f, -30.0f, 30.0f);
        lastAltM = ride.altM;
        // Advance route waypoint
        if (routeLen > 0 && nav.routeIndex < routeLen - 1) {
          float wd = haversineKm(nLat, nLon, route[nav.routeIndex+1].lat, route[nav.routeIndex+1].lon);
          if (wd < 0.025f) nav.routeIndex++;
          // Remaining distance follows the selected polyline, not a straight line.
          int next=min(nav.routeIndex+1,routeLen-1);
          nav.remainDistKm=haversineKm(nLat,nLon,route[next].lat,route[next].lon);
          for(int k=next;k<routeLen-1;k++)nav.remainDistKm+=haversineKm(route[k].lat,route[k].lon,route[k+1].lat,route[k+1].lon);
          nav.bearingDeg=bearingTo(nLat,nLon,route[next].lat,route[next].lon);
        }
      }
    }
    lastLat = ride.lat = nLat;
    lastLon = ride.lon = nLon;
    ride.gpsFixed = gps.location.age()<3000;

    // Update nav
    if (nav.hasDestination) {
      nav.remainDistKm = haversineKm(nLat, nLon, nav.destLat, nav.destLon);
      nav.bearingDeg   = bearingTo(nLat, nLon, nav.destLat, nav.destLon);
    }

    // Track breadcrumb every 3s while moving
    if (rideStarted && !ridePaused && millis() - lastTrackMs > 3000) {
      lastTrackMs = millis();
      if(activityFile){fitPoint(nLat,nLon);archivePoint(nLat,nLon);}
      if (trackLen < MAX_TRACK)
        track[trackLen++] = {nLat, nLon, ride.altM, ride.elapsedSec};
    }
  } else if (!gps.location.isValid()) {
    ride.gpsFixed = false;
  }

  ride.gpsFixed=gps.location.isValid()&&gps.location.age()<3000;
  ride.speedKmh=ride.gpsFixed && gps.speed.isValid()&&gps.speed.age()<3000?gps.speed.kmph():0;
  if(bmpOk && millis()-sensorMs>500){sensorMs=millis();
    if(bmp.performReading()&&isfinite(bmp.pressure)&&bmp.pressure>=30000&&bmp.pressure<=125000)
      ride.altM=44330.0f*(1.0f-powf(bmp.pressure/101325.0f,0.1903f));
    else {bmpOk=false;uiRevision++;}
  }
  static uint32_t retryMs=0;
  if(millis()-retryMs>5000){retryMs=millis();
    if(!bmpOk)bmpOk=beginBarometer();
    bool present=externalPresent(0x20);
    if(!present)buttonsOk=false;
    else if(!buttonsOk){buttonsOk=externalPresent(0x20) && buttons.begin_I2C(0x20,&Wire);if(buttonsOk)for(int i=0;i<4;i++)buttons.pinMode(i,INPUT_PULLUP);}
    if(!externalPresent(0x0D))compass.ready=false;
    else if(!compass.ready)compass.begin();
    if(!touchReady)initQuickTouch();
  }
  if(!bmpOk || runningMode) ride.altM = ride.gpsFixed && gps.altitude.isValid() && gps.altitude.age()<3000 ? gps.altitude.meters() : 0;
  if(!ride.gpsFixed) {ride.lat=0;ride.lon=0;ride.gradePercent=0;nav.remainDistKm=0;lastLat=0;lastLon=0;}

  // Speed source
  if (!runningMode && sensors.speedPhysical && millis()-externalSensorMs<5000)
    ride.speedKmh = sensors.physSpeedKmh;

  if(!runningMode && bleSpeedReady)ride.speedKmh=bleSpeed;

  // Cadence source
  ride.cadenceRpm = (sensors.cadencePhysical && millis()-externalSensorMs<5000)
                    ? sensors.physCadenceRpm
                    : 0;

  if(bleCadReady)ride.cadenceRpm=bleCad;
  if(runningMode)ride.cadenceRpm=0;

  // Power + VO2
  ride.powerW = !runningMode && blePowerReady?blePower:0;
  ride.vo2est = cfg.maximumHR>cfg.restingHR && cfg.restingHR>=30 ? 15.3f*cfg.maximumHR/cfg.restingHR : 0; // HR-ratio population estimate, not a live measurement.

  // Sample only fresh, measured data, at one-second intervals.
  if(millis()-statsMs>=1000){uint32_t dt=millis()-statsMs;statsMs=millis();
    if(rideStarted&&!ridePaused){
      if(samplesFile){samplesFile.print(ride.elapsedSec);samplesFile.print(',');if(bleHRReady)samplesFile.print(bleHR);samplesFile.print(',');if(speedAvailable())samplesFile.print(ride.speedKmh,2);samplesFile.println();}
      if(bleHRReady&&cfg.massConfirmed&&cfg.ageYears>=18&&cfg.ageYears<=80&&cfg.calorieSex&&dt<=2000){
        float kj=cfg.calorieSex==1 ? -55.0969f+0.6309f*bleHR+0.1988f*cfg.riderMassKg+0.2017f*cfg.ageYears : -20.4022f+0.4472f*bleHR-0.1263f*cfg.riderMassKg+0.074f*cfg.ageYears;
        calorieEstimate+=max(0.0f,kj/4.184f)*dt/60000.0f;calorieSeconds+=dt/1000;
      }
    }
    if(rideStarted && !ridePaused && bleHRReady){hrSum+=bleHR;hrSamples++;hrMax=max(hrMax,(int)bleHR);}
    if(rideStarted && !runningMode && !ridePaused && blePowerReady){powerSum+=blePower;powerSamples++;}
  }

  // Ride timer
  if (rideStarted && !ridePaused) {
    ride.elapsedSec = (millis() - rideStartMs) / 1000;
    if (ride.elapsedSec > 0)
      ride.avgSpeedKmh = (distAccum / (float)ride.elapsedSec) * 3600.0f;
  }
  if (ride.speedKmh > ride.maxSpeedKmh) ride.maxSpeedKmh = ride.speedKmh;
  if (ride.powerW   > ride.maxPowerW)   ride.maxPowerW   = ride.powerW;

  // Compass
  // Calibrated compass heading is level-only; IMU tilt compensation remains pending.
  if(ride.gpsFixed && ride.speedKmh>3 && gps.course.isValid())ride.heading=gps.course.deg();
  if(compass.usable())ride.heading=lroundf(compass.heading)%360;

  // ── Buttons ───────────────────────────────────────────────────────────────

  updateBtn(rmtUp);  updateBtn(rmtDn);  updateBtn(rmtLt);  updateBtn(rmtRt);

  // SEL (body) → switch screen

  // Remote LEFT → dash, Remote RIGHT → map
  if (rmtLt.pressed) { rmtLt.pressed = false; currentScreen = SCR_DASH; }
  if (rmtRt.pressed) { rmtRt.pressed = false; currentScreen = (Screen)(((int)currentScreen+1)%SCR_COUNT);  }

  // Remote UP/DN → zoom (map) or future use on dash
  if (rmtUp.pressed) { rmtUp.pressed = false; if (currentScreen == SCR_MAP) mapZoom = min((int)mapZoom+1, 5); }
  if (rmtDn.pressed) { rmtDn.pressed = false; if (currentScreen == SCR_MAP) mapZoom = max((int)mapZoom-1, 1); }

  // Body UP/DN → zoom on map too (mirrored for one-handed use)

  updateRideCountdown();

  // Web server
  server.handleClient();
  serviceAppRefresh();
  pollQuickTouch();

  // Check changed content at up to 40 Hz; static content is not transferred.
  if (millis() - lastDispMs >= 25) {
    lastDispMs = millis();
    String state=String((gpsWaiting||gpsRadarInspector)?(millis()/40):0)+":"+String(gpsDataReady)+":"+String(uiRevision)+":"+String(darkTheme)+":"+String(millis()/500)+":"+String(bleCount())+":"+String(WiFi.getMode())+":"+String(WiFi.status())+":"+String(currentScreen)+":"+String(mapZoom)+":"+String(ride.speedKmh,1)+":"+String(ride.distanceKm,2)+":"+String(ride.elapsedSec/60)+":"+String(ride.lat,5)+":"+String(ride.lon,5)+":"+String(ride.altM,1)+":"+String(ride.cadenceRpm)+":"+String(ride.powerW)+":"+String(ride.gpsFixed)+":"+String(routeLen)+":"+String(trackLen)+":"+String(nav.destLat,5)+":"+String(nav.destLon,5)+":"+String(cfg.vo2maxManual)+":"+String(cfg.targetDistanceKm);
    static String previous; if(state==previous)return;previous=state;
    if(drawLifecycleScreen()){drawGlobalNav();drawAppRefresh();display.refresh();return;}
    if(quickOpen){drawQuickSettings();drawGlobalNav();drawAppRefresh();display.refresh();return;}
    switch (currentScreen) {
      case SCR_DASH: drawDashScreen(); break;
      case SCR_MAP:  drawMapScreen();  break;
      case SCR_METRICS: drawMetricsScreen(); break;
      default: break;
    }
    drawGlobalNav();
    drawAppRefresh();
    display.refresh();
  }
}