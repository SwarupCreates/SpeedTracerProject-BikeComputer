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
  float    elevationGainM = 0;
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
  float maxSpeedRide    = 60.0f;
  float maxSpeedRun     = 20.0f;
  int   countdownSec    = 3;
  int   targetCalories  = 1200;
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

enum Screen { SCR_DASH, SCR_METRICS, SCR_MAP, SCR_CONTROLS, SCR_COUNT };
bool darkTheme=false;
bool appsListView=false;
bool runningMode=false;
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
 int logoW=BOOT_LOGO_WIDTH,logoH=BOOT_LOGO_HEIGHT;
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
     if(x>=0 && x<logoW && y>=0 && y<logoH){
        int srcX=(logoW==BOOT_LOGO_WIDTH)?x:(x*BOOT_LOGO_WIDTH/logoW);
        int srcY=(logoH==BOOT_LOGO_HEIGHT)?y:(y*BOOT_LOGO_HEIGHT/logoH);
        return pgm_read_word(&(logoLight?HOME_LOGO_LIGHT_DATA:BOOT_LOGO_DATA)[srcY*BOOT_LOGO_WIDTH+srcX]);
      }
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
 void drawBrandLogo(int x,int y,bool light=false,int targetW=0,int targetH=0){
   logoLight=light;
   logoX=x;logoY=y;logoOverlay=true;
   logoW=(targetW>0)?targetW:BOOT_LOGO_WIDTH;
   logoH=(targetH>0)?targetH:BOOT_LOGO_HEIGHT;
   const uint16_t* data=light?HOME_LOGO_LIGHT_DATA:BOOT_LOGO_DATA;
   if(buffer){
     for(int r=0;r<logoH;r++){
       int py=y+r;
       if(py<0 || py>=DISP_H)continue;
       int srcY=(logoH==BOOT_LOGO_HEIGHT)?r:(r*BOOT_LOGO_HEIGHT/logoH);
       for(int c=0;c<logoW;c++){
         int px=x+c;
         if(px<0 || px>=DISP_W)continue;
         int srcX=(logoW==BOOT_LOGO_WIDTH)?c:(c*BOOT_LOGO_WIDTH/logoW);
         buffer[py*DISP_W+px]=pgm_read_word(&data[srcY*BOOT_LOGO_WIDTH+srcX]);
       }
     }
   }
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
    // Compare tiles with 32-bit unrolled word reads, merge adjacent changed tiles into one 16-row transfer.
    static uint16_t strip[DISP_W*16];
    for(int y=0;y<DISP_H;y+=16){
      bool dirty[DISP_W/16];
      for(int x=0;x<DISP_W;x+=16){
        uint32_t hash=2166136261UL;
        if(buffer){
          for(int r=0;r<16;r++){
            const uint32_t* p32=(const uint32_t*)(buffer + (y+r)*DISP_W + x);
            hash ^= p32[0]; hash *= 16777619UL;
            hash ^= p32[1]; hash *= 16777619UL;
            hash ^= p32[2]; hash *= 16777619UL;
            hash ^= p32[3]; hash *= 16777619UL;
            hash ^= p32[4]; hash *= 16777619UL;
            hash ^= p32[5]; hash *= 16777619UL;
            hash ^= p32[6]; hash *= 16777619UL;
            hash ^= p32[7]; hash *= 16777619UL;
          }
        } else {
          for(int r=0;r<16;r++)for(int c=0;c<16;c++)
            hash=(hash^pixelAt((y+r)*DISP_W+x+c))*16777619UL;
        }
        int index=(y/16)*(DISP_W/16)+x/16;
        dirty[x/16]=first || shown[index]!=hash;
        shown[index]=hash;
      }
      for(int t=0;t<DISP_W/16;){
        if(!dirty[t]){++t;continue;}
        int begin=t;while(t<DISP_W/16 && dirty[t])++t;
        int x=begin*16,w=(t-begin)*16;
        if(buffer){
          for(int r=0;r<16;r++){
            memcpy(&strip[r*w], buffer + (y+r)*DISP_W + x, w * sizeof(uint16_t));
          }
        } else {
          for(int r=0;r<16;r++)for(int c=0;c<w;c++)strip[r*w+c]=pixelAt((y+r)*DISP_W+x+c);
        }
        panel.drawRGBBitmap(x,y,strip,w,16);
      }
    }
    first=false;
  }
} display;

#include "icons.h"
#include "map_tile_cache.h"
MapTileCache g_mapCache;
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

static void setAppsListView(bool value){
  if(appsListView==value)return;
  appsListView=value;
  Preferences p;
  if(p.begin("cyclo",false)){
    if(p.putBool("appsList",value)!=1)Serial.println("Apps layout save failed");
    p.end();
  }else Serial.println("Apps layout storage unavailable");
}


uint8_t activeFontId = FONT_METROPOLIS;
static void setActiveFont(uint8_t id) {
  if (id > FONT_TEKTUR) id = FONT_METROPOLIS;
  if (activeFontId == id) return;
  activeFontId = id;
  Preferences fontPrefs;
  if (fontPrefs.begin("cyclo", false)) {
    fontPrefs.putUChar("fontId", id);
    fontPrefs.end();
  }
  uiRevision++;
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

static void snapActiveRoute() {
  if (routeLen < 2) return;
  for (int i = 0; i < routeLen; i++) {
    double lat = route[i].lat;
    double lon = route[i].lon;
    if (g_mapCache.snapPointToRoad(lat, lon, 25.0f)) {
      route[i].lat = (float)lat;
      route[i].lon = (float)lon;
    }
  }
}

#define MAX_TRACK 8192
TrackPoint* track = nullptr;
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
float           distAnchorLat = 0.0f, distAnchorLon = 0.0f;
bool            autoPauseEnabled = true;
bool            autoPaused = false;
uint32_t        autoPauseStartMs = 0;
uint32_t        nearZeroSpeedStartMs = 0;
uint32_t        autoResumeMovementStartMs = 0;
uint32_t        autoPauseOverrideGraceMs = 0;
float           autoPauseAnchorLat = 0.0f, autoPauseAnchorLon = 0.0f;
int             controlSliderOffset = 0;
bool            controlSliderDragging = false;
int             controlSliderStartX = 0;

// ─── Forward declarations ────────────────────────────────────────────────────
static float haversineKm(float la1, float lo1, float la2, float lo2);
static int   bearingTo(float la1, float lo1, float la2, float lo2);

static void  updateBtn(Btn &b);

static void  drawArcFill(int cx, int cy, int r, int startDeg, int spanDeg, int thicc);
static void  drawProgressBar(int x, int y, int w, int h, float frac);
static void  drawDividerH(int y, int x0 = 0, int x1 = DISP_W - 1);
static void  drawDashScreen();
static void  drawMetricsScreen();
static void  drawMapScreen();
static void  drawActivityControlsScreen();
static void  drawAutoPauseScreen();
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
static uint16_t uiCard() { return darkTheme ? 0x1082 : 0xDEFB; }
static uint16_t uiFg()   { return darkTheme ? 0xFFFF : 0x18C3; }
static uint16_t uiMuted(){ return 0x8410; }
static const uint16_t TEAL = 0x3DF7, ORANGE = 0xFA60, RED = 0xF800, GREEN = 0x07E0;

// Native-resolution, 4-bit coverage glyphs, blended over the actual card background.
// Use one shared baseline and measured advances; no enlarged 5x7 bitmap text.
static const SmoothFont& textFont(int size) {
  switch (activeFontId) {
    case FONT_GOOGLE_SANS:
      return size == 0 ? smoothFont_GoogleSans_12 : size <= 1 ? smoothFont_GoogleSans_10 : size == 2 ? smoothFont_GoogleSans_18 : size == 3 ? smoothFont_GoogleSans_28 : smoothFont_GoogleSans_36;
    case FONT_GALANO:
      return size == 0 ? smoothFont_Galano_12 : size <= 1 ? smoothFont_Galano_10 : size == 2 ? smoothFont_Galano_18 : size == 3 ? smoothFont_Galano_28 : smoothFont_Galano_36;
    case FONT_GALANO_ITALIC:
      return size == 0 ? smoothFont_GalanoItalic_12 : size <= 1 ? smoothFont_GalanoItalic_10 : size == 2 ? smoothFont_GalanoItalic_18 : size == 3 ? smoothFont_GalanoItalic_28 : smoothFont_GalanoItalic_36;
    case FONT_SPACE_GROTESK:
      return size == 0 ? smoothFont_SpaceGrotesk_12 : size <= 1 ? smoothFont_SpaceGrotesk_10 : size == 2 ? smoothFont_SpaceGrotesk_18 : size == 3 ? smoothFont_SpaceGrotesk_28 : smoothFont_SpaceGrotesk_36;
    case FONT_TEKTUR:
      return size == 0 ? smoothFont_Tektur_12 : size <= 1 ? smoothFont_Tektur_10 : size == 2 ? smoothFont_Tektur_18 : size == 3 ? smoothFont_Tektur_28 : smoothFont_Tektur_36;
    case FONT_METROPOLIS:
    default:
      return size == 0 ? smoothFont_Metro_12 : size <= 1 ? smoothFont_Metro_10 : size == 2 ? smoothFont_Metro_18 : size == 3 ? smoothFont_Metro_28 : smoothFont_Metro_36;
  }
}
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

// Live Battery State (Frame 27 Parity with Level & Charging Animations)
static int batteryPercent = 76;
static bool batteryCharging = false;
static bool batteryAlert = false;

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
  label(12, 10, clock, 0, uiFg());
  if (meridiem.length()) {
    int clockW = textWidth(clock, 0);
    label(12 + clockW + 2, 10, meridiem, 0, uiFg());
  }

  // Quick Settings uses a chevron instead of the four centre indicators.
  if (showStatusIcons) {
  // Centre the visible 16px icons as a group, not a fixed left anchor.
  const bool recording = activityRecording();
  const int iconSize = 16, gap = 6;
  const int count = recording ? 4 : 3;
  int x = (DISP_W - (count * iconSize + (count - 1) * gap)) / 2;
  const bool blinkOn = (millis() / 500) % 2;
  const uint16_t unavailable = blinkOn ? RED : uiBg();
  // Only a deliberately disabled radio is grey. Missing hardware, no fix,
  // or no sensor connection must not look like a manually disabled feature.
  const wifi_mode_t mode = WiFi.getMode();
  const bool wifiOff = mode == WIFI_OFF;
  const bool apReady = (mode == WIFI_AP || mode == WIFI_AP_STA) &&
                       uint32_t(WiFi.softAPIP()) != 0;
  const bool wifiAvailable = WiFi.status() == WL_CONNECTED || apReady;
  if (recording) {
    const uint16_t actCol = darkTheme ? 0x3CE0 : 0x05E0;
    const CustomIcon actIcon = runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon;
    drawCustomIcon(display, x, 5, blinkOn ? actCol : uiBg(), actIcon, 16, 16);
    x += iconSize + gap;
  }
  drawSatelliteIcon(display, x, 5, ride.gpsFixed ? uiFg() : unavailable);
  x += iconSize + gap;
  drawBluetoothStateIcon(display, x, 5, bleCount() > 0 ? uiFg() : unavailable, bleCount() > 0);
  x += iconSize + gap;
  drawWifiStateIcon(display, x, 5, wifiOff ? uiMuted() : (wifiAvailable ? uiFg() : unavailable), wifiAvailable);

  }

  // Battery Status (Frame 27 Parity with Animated & Level-Switched States)
  String batteryText;
  uint16_t batCol;
  if (batteryCharging) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0x3CE0; // Charging green
  } else if (batteryAlert || batteryPercent < 10) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0xF800; // Red alert
  } else if (batteryPercent < 25) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0xF800; // Red low
  } else if (batteryPercent < 50) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0xFA60; // Amber
  } else {
    batteryText = String(batteryPercent) + "%";
    batCol = uiFg(); // Crisp white in dark theme
  }

  int batX = 208;
  int batY = 8;
  label(batX - 4 - textWidth(batteryText, 0), 10, batteryText, 0, batCol);
  drawBatteryIcon(display, batX, batY, batteryPercent, batteryCharging, batteryAlert);
}

// One binary FIT activity per SD file; finalized on Save.
File activityFile;
String activityPath, activityMessage = "Ready - tap Start";
uint32_t activityPoints = 0;
#include "activity_fit.h"
#include "activity_archive.h"
static String paceText(float kmh){
  if(kmh < 0.5f) return "--:--";
  unsigned seconds = lroundf(3600.0f / kmh);
  if(seconds > 3599) return "59:59";
  unsigned mm = seconds / 60;
  unsigned ss = seconds % 60;
  return String(mm) + ":" + (ss < 10 ? "0" : "") + String(ss);
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
static float topPeaks[4] = {0, 0, 0, 0};
static int peakCount = 0;
static float speedHist[3] = {0, 0, 0};
static int histCount = 0;

static void resetMaxSpeedTracker() {
  for (int i = 0; i < 4; i++) topPeaks[i] = 0.0f;
  for (int i = 0; i < 3; i++) speedHist[i] = 0.0f;
  peakCount = 0;
  histCount = 0;
  ride.maxSpeedKmh = 0.0f;
}

extern bool ridePaused;
// ═══════════════════════════════════════════════════════════════════════════════
//  INTELLIGENT HEADING FUSION (Compass & GPS Course Over Ground)
// ═══════════════════════════════════════════════════════════════════════════════
static float s_fusedHeading = 0.0f;
static bool  s_fusedHeadingInit = false;
static uint32_t s_gpsMovingStartMs = 0;
static bool  s_gpsHeadingLocked = false;
static float s_lastStableMovingHeading = 0.0f;

static void resetFusedHeading() {
  s_gpsHeadingLocked = false;
  s_gpsMovingStartMs = 0;
}

static void updateFusedHeading() {
  bool compassValid = (compass.ready && isfinite(compass.heading) && (millis() - compass.lastSample < 1500));
  bool gpsValid = (ride.gpsFixed && gps.course.isValid() && gps.course.age() < 2500);

  // High-speed straight patch threshold: >= 8 km/h running, >= 12 km/h cycling
  float highSpeedThreshold = runningMode ? 8.0f : 12.0f;
  bool highSpeedStraight = (gpsValid && ride.speedKmh >= highSpeedThreshold);

  float targetHeading = s_fusedHeading;
  float filterAlpha = 0.22f;

  if (compassValid) {
    // Prioritize clean compass data for live heading
    targetHeading = compass.heading;
    filterAlpha = 0.25f;

    if (highSpeedStraight) {
      // Blend GPS course on higher-speed straight patches to lock true geographic course
      float gpsCourse = (float)gps.course.deg();
      float diff = gpsCourse - compass.heading;
      while (diff < -180.0f) diff += 360.0f;
      while (diff > 180.0f)  diff -= 360.0f;

      if (fabsf(diff) < 45.0f) {
        targetHeading = fmodf(compass.heading + diff * 0.75f + 360.0f, 360.0f);
        filterAlpha = 0.30f;
      }
    }
  } else if (gpsValid && ride.speedKmh >= 2.0f) {
    // Fallback to GPS course if compass unavailable
    targetHeading = (float)gps.course.deg();
    filterAlpha = 0.25f;
  }

  if (!s_fusedHeadingInit) {
    s_fusedHeading = targetHeading;
    s_fusedHeadingInit = true;
  } else {
    float diff = targetHeading - s_fusedHeading;
    while (diff < -180.0f) diff += 360.0f;
    while (diff > 180.0f)  diff -= 360.0f;
    s_fusedHeading = fmodf(s_fusedHeading + diff * filterAlpha + 360.0f, 360.0f);
  }

  ride.heading = (int)lroundf(s_fusedHeading) % 360;
}

static void updateMaxSpeed(float currentSpeed) {
  if (!rideStarted || ridePaused) return;

  float minMovingSpeed = runningMode ? 2.5f : 2.0f;
  float gaugeMax = runningMode ? cfg.maxSpeedRun : cfg.maxSpeedRide;

  // Reject speeds below moving threshold (idle) or above plausible physical maximum
  if (currentSpeed < minMovingSpeed || currentSpeed > gaugeMax * 1.3f) {
    histCount = 0; // Break sustained chain when in idle/deadzone
    return;
  }

  // Physical acceleration limit: human motion cannot jump more than 10 km/h in a single second from standstill
  if (histCount > 0 && abs(currentSpeed - speedHist[0]) > 10.0f) {
    return; // Discard erratic single-second GPS jump
  }

  // Shift history
  speedHist[2] = speedHist[1];
  speedHist[1] = speedHist[0];
  speedHist[0] = currentSpeed;
  if (histCount < 3) histCount++;

  // Must have at least 3 consecutive seconds of actual movement
  if (histCount < 3) return;

  // 3-second sustained speed
  float sustained = (speedHist[0] + speedHist[1] + speedHist[2]) / 3.0f;

  // Track top 4 sustained peaks
  if (peakCount == 0) {
    topPeaks[0] = sustained;
    peakCount = 1;
  } else {
    if (peakCount < 4) {
      topPeaks[peakCount++] = sustained;
      // Sort descending
      for (int i = 0; i < peakCount - 1; i++) {
        for (int j = i + 1; j < peakCount; j++) {
          if (topPeaks[j] > topPeaks[i]) {
            float tmp = topPeaks[i]; topPeaks[i] = topPeaks[j]; topPeaks[j] = tmp;
          }
        }
      }
    } else if (sustained > topPeaks[3]) {
      topPeaks[3] = sustained;
      // Sort descending
      for (int i = 2; i >= 0; i--) {
        if (topPeaks[i+1] > topPeaks[i]) {
          float tmp = topPeaks[i]; topPeaks[i] = topPeaks[i+1]; topPeaks[i+1] = tmp;
        }
      }
    }
  }

  // Calculate mean average of top maxes
  if (peakCount >= 3) {
    float sum = 0.0f;
    for (int i = 0; i < peakCount; i++) sum += topPeaks[i];
    ride.maxSpeedKmh = sum / (float)peakCount;
  } else {
    ride.maxSpeedKmh = topPeaks[0];
  }
}

static void saveActivity() {
  if(activityFile){bool ok=fitFinish();archiveFinish(ok);activityMessage=ok?"Saved "+String(activityPoints)+" FIT points":"FIT save failed";}
  else activityMessage="No open activity to save";
  rideStarted=false;
  autoPaused=false;
  autoPauseStartMs = 0;
  nearZeroSpeedStartMs = 0;
  autoResumeMovementStartMs = 0;
  autoPauseAnchorLat = autoPauseAnchorLon = 0.0f;
  controlSliderOffset=0;
  controlSliderDragging=false;
  resetMaxSpeedTracker();
  resetFusedHeading();
  trackLen = 0;
  distAccum = 0;
  ride.distanceKm = 0;
  ride.elevationGainM = 0;
  ride.elapsedSec = 0;
  uiRevision++;
}
static bool activityEndConfirm = false;
#include "quick_settings.h"
static bool activityRecording(){return rideStarted && !ridePaused && !autoPaused;}

static void heartCard(int y, int h) {
  int hrVal = bleHRReady ? bleHR : 0;
  int zone = calcHrZone(hrVal);
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];

  uint16_t cardBg = darkTheme ? zc.bgDark : zc.bgLight;
  uint16_t hrFg   = darkTheme ? zc.textDark : zc.textLight;
  uint16_t hrSub  = darkTheme ? zc.subDark : zc.subLight;

  card(6, y, 228, h, cardBg);

  // AVG
  label(16, y + 6, "AVG", 1, hrSub);
  label(16, y + 16, (hrSamples?String(hrSum/hrSamples):String("--")), 2, hrFg);

  // Current HR & Zone
  label(42, y + 16, bleHRReady ? "ZONE " + String(zone) : "NO DATA", 1, hrFg);
  label(84, y + 8, (bleHRReady?String(hrVal):String("--")), 3, hrFg);
  label(144, y + 16, "BPM", 1, hrFg);
  drawHeartIcon(display, 168, y + 15, hrFg);

  // MAX
  label(192, y + 6, "MAX", 1, hrSub);
  label(192, y + 16, (hrSamples?String(hrMax):String("--")), 2, hrFg);

  // 5-Zone Bar with Active Indicator
  if (bleHRReady) drawHRZoneBar(display, 16, y + 46, 208, zone);
}

static void drawDashScreen() {
  statusBar();

  // Floating values layout (Frame 27 - AMOLED black background)
  uint16_t fgCol = uiFg();
  uint16_t mutedCol = uiMuted();

  // Top Row: AVG (Left) and MAX (Right)
  label(18, 35, "AVG", 1, mutedCol);
  label(18, 52, (ride.elapsedSec ? (runningMode ? paceText(ride.avgSpeedKmh) : String(ride.avgSpeedKmh, 1)) : String("--")), 2, fgCol);
  label(222 - textWidth("MAX", 1), 35, "MAX", 1, mutedCol);
  String maxSp = (ride.elapsedSec ? (runningMode ? paceText(ride.maxSpeedKmh) : String(ride.maxSpeedKmh, 1)) : String("--"));
  label(222 - textWidth(maxSp, 2), 52, maxSp, 2, fgCol);

  // Center Circular Arc Gauge with Rounded Caps (cx = 120, cy = 103, R = 61.5, W = 6)
  const int cx = 120, cy = 103;
  const float arcR = 61.5f;
  uint16_t arcBg = darkTheme ? 0x2945 : 0xCE79;

  // Track Background: round caps at 135 deg and 405 deg
  int x_start = cx + (int)lroundf(arcR * cosf(radians(135.0f)));
  int y_start = cy + (int)lroundf(arcR * sinf(radians(135.0f)));
  int x_end   = cx + (int)lroundf(arcR * cosf(radians(405.0f)));
  int y_end   = cy + (int)lroundf(arcR * sinf(radians(405.0f)));
  display.fillCircle(x_start, y_start, 3, arcBg);
  display.fillCircle(x_end, y_end, 3, arcBg);

  // High-res continuous 0.25 deg stroke
  for (float d = 135.0f; d <= 405.0f; d += 0.25f) {
    float a = radians(d);
    float ca = cosf(a), sa = sinf(a);
    for (int r = 59; r <= 64; r++) {
      display.drawPixel(cx + (int)lroundf(r * ca), cy + (int)lroundf(r * sa), arcBg);
    }
  }

  // Active Progress Arc with Rounded Caps
  float spVal = (ride.speedKmh >= 0.8f && activityRecording()) ? ride.speedKmh : 0.0f;
  float gaugeMax = runningMode ? cfg.maxSpeedRun : cfg.maxSpeedRide;
  int fillSpan = (int)(270.0f * constrain(spVal / gaugeMax, 0.0f, 1.0f));
  if (fillSpan > 0) {
    // Round cap at start
    display.fillCircle(x_start, y_start, 3, TEAL);

    // Continuous high-res arc body
    for (float d = 135.0f; d <= 135.0f + (float)fillSpan; d += 0.25f) {
      float a = radians(d);
      float ca = cosf(a), sa = sinf(a);
      for (int r = 59; r <= 64; r++) {
        display.drawPixel(cx + (int)lroundf(r * ca), cy + (int)lroundf(r * sa), TEAL);
      }
    }

    // Smooth rounded cap at leading tip
    float tipA = radians(135.0f + (float)fillSpan);
    int x_tip = cx + (int)lroundf(arcR * cosf(tipA));
    int y_tip = cy + (int)lroundf(arcR * sinf(tipA));
    display.fillCircle(x_tip, y_tip, 3, TEAL);
  }

  // Bike / Run Icon inside Arc (Frame 27: 30x30 square icon at x=105, lowered to y=62)
  drawCustomIcon(display, 105, 62, TEAL, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 30, 30);

  // Large Speed Digits & Unit inside Arc (36px font, lowered to y=96)
  String sp = speedAvailable() ? (runningMode ? paceText(ride.speedKmh) : String(ride.speedKmh, 1)) : (runningMode ? String("0:00") : String("0.0"));
  centeredLabel(40, 96, 160, sp, 4, fgCol);
  centeredLabel(40, 132, 160, (ride.gpsFixed || (!runningMode && (bleSpeedReady || (sensors.speedPhysical && millis() - externalSensorMs < 5000)))) ? (runningMode ? "MIN/KM" : "KM/H") : "NO DATA", 1, mutedCol);

  // Complications Row (Frame 27: 3 columns at y = 156..184)
  // Col 1: Distance
  String distStr = (rideStarted || activityPoints ? String(ride.distanceKm, 2) : String("--"));
  centeredLabel(6, 156, 72, distStr, 2, TEAL);
  centeredLabel(6, 175, 72, "DIST(KM)", 0, TEAL);

  // Col 2: Total Time
  centeredLabel(80, 156, 80, durationText(), 2, TEAL);
  centeredLabel(80, 175, 80, "TIME", 0, TEAL);

  // Col 3: Gradient
  String gradStr = (ride.gpsFixed && trackLen > 1 ? String(ride.gradePercent, 0) + "%" : String("--"));
  centeredLabel(162, 156, 72, gradStr, 2, TEAL);
  centeredLabel(162, 175, 72, "GRAD", 0, TEAL);

  // Heart Rate Row (Frame 27: y = 201..245)
  label(18, 205, "AVG", 1, mutedCol);
  label(18, 223, (hrSamples ? String(hrSum / hrSamples) : String("--")), 2, fgCol);

  // Center HR Pill (w = 116, h = 44, R = 22)
  int hrVal = bleHRReady ? bleHR : 0;
  int zone = (bleHRReady && hrVal > 0) ? calcHrZone(hrVal) : 1;
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrPillBg, hrPillFg;
  if (bleHRReady && hrVal > 0) {
    hrPillBg = darkTheme ? zc.bgDark : zc.bgLight;
    hrPillFg = darkTheme ? zc.textDark : zc.textLight;
  } else {
    hrPillBg = darkTheme ? 0x18C3 : 0xEF5D;
    hrPillFg = mutedCol;
  }
  display.fillRoundRect(62, 201, 116, 44, 22, hrPillBg);
  String hrStr = (bleHRReady && hrVal > 0) ? String(hrVal) : "--";
  int hrTextW = textWidth(hrStr, 4);
  int hrBlockW = 30 + 6 + hrTextW;
  int hrStartX = 62 + (116 - hrBlockW) / 2;
  drawCustomIcon(display, hrStartX, 208, hrPillFg, CustomIcon::heartRateIcon, 30, 30);
  label(hrStartX + 36, 209, hrStr, 4, hrPillFg);

  label(222 - textWidth("MAX", 1), 205, "MAX", 1, mutedCol);
  String maxHrStr = (hrSamples ? String(hrMax) : String("--"));
  label(222 - textWidth(maxHrStr, 2), 223, maxHrStr, 2, fgCol);

  // Zone text and pointer (neutral muted grey if no HR reading)
  uint16_t zoneCol = (bleHRReady && hrVal > 0) ? (darkTheme ? zc.textDark : zc.textLight) : mutedCol;
  int segCenters[5] = {35, 78, 120, 162, 205};
  int ptrX = segCenters[constrain(zone - 1, 0, 4)];
  display.fillTriangle(ptrX - 4, 267, ptrX + 4, 267, ptrX, 272, zoneCol);

  // Indicator text sits directly on top of the down arrow marker
  String zoneStr = "ZONE " + String(zone);
  int zw = textWidth(zoneStr, 1);
  int zx = constrain(ptrX - zw / 2, 6, 234 - zw);
  label(zx, 255, zoneStr, 1, zoneCol);

  // 5-Segment Colored Bar at y = 278..282
  display.fillRoundRect(17, 278, 38, 4, 2, 0x4208); // Zone 1: Grey
  display.fillRoundRect(59, 278, 39, 4, 2, 0x0353); // Zone 2: Blue
  display.fillRoundRect(101, 278, 39, 4, 2, 0x7FE0); // Zone 3: Green
  display.fillRoundRect(143, 278, 39, 4, 2, 0xFA60); // Zone 4: Amber
  display.fillRoundRect(186, 278, 38, 4, 2, 0xF800); // Zone 5: Red
}

static void drawMetricsScreen() {
  statusBar();
  uint16_t fgCol = uiFg();
  uint16_t mutedCol = uiMuted();
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // Row 1 (y = 38, h = 54): Two 108x54 pills
  uint16_t spdBg = darkTheme ? 0x09A5 : 0xCFF7;
  uint16_t spdFg = darkTheme ? 0x475F : 0x0395;
  display.fillRoundRect(8, 38, 108, 54, 27, spdBg);
  String sp = speedAvailable() ? (runningMode ? paceText(ride.speedKmh) : String(ride.speedKmh, 1)) : (runningMode ? String("0:00") : String("0.0"));
  centeredLabel(8, 55, 108, sp, 3, spdFg);

  int hrVal = bleHRReady ? bleHR : 0;
  int zone = (bleHRReady && hrVal > 0) ? calcHrZone(hrVal) : 1;
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrPillBg, hrPillFg;
  if (bleHRReady && hrVal > 0) {
    hrPillBg = darkTheme ? zc.bgDark : zc.bgLight;
    hrPillFg = darkTheme ? zc.textDark : zc.textLight;
  } else {
    hrPillBg = darkTheme ? 0x18C3 : 0xEF5D;
    hrPillFg = mutedCol;
  }
  display.fillRoundRect(124, 38, 108, 54, 27, hrPillBg);
  String hrStr = (bleHRReady && hrVal > 0) ? String(hrVal) : "--";
  int hrTextW = textWidth(hrStr, 3);
  int hrBlockW = 30 + 6 + hrTextW;
  int hrStartX = 124 + (108 - hrBlockW) / 2;
  drawCustomIcon(display, hrStartX, 50, hrPillFg, CustomIcon::heartRateIcon, 30, 30);
  label(hrStartX + 36, 55, hrStr, 3, hrPillFg);

  // Row 2 (y = 102, h = 54): Calories Card (224x54) with Smooth High-Res Dial & Rounded Caps
  display.fillRoundRect(8, 102, 224, 54, 27, cardBg);
  const int calCx = 36, calCy = 129;
  const float calR = 19.5f;
  uint16_t ringBg = darkTheme ? 0x31A6 : 0xD69A;

  // Solid, continuous 360 deg track (0.5 deg steps, eliminating all segmentation)
  for (float d = 0.0f; d < 360.0f; d += 0.5f) {
    float a = radians(d);
    float ca = cosf(a), sa = sinf(a);
    for (int r = 18; r <= 21; r++) display.drawPixel(calCx + (int)lroundf(r * ca), calCy + (int)lroundf(r * sa), ringBg);
  }

  int tgtCal = max(100, cfg.targetCalories);
  float calRatio = constrain(calorieEstimate / (float)tgtCal, 0.0f, 1.0f);
  int calSweep = (int)(360.0f * calRatio);
  if (calSweep > 0) {
    // Round cap at top (-90 deg)
    display.fillCircle(calCx, calCy - 20, 2, ORANGE);

    // Continuous progress arc
    for (float d = -90.0f; d <= -90.0f + (float)calSweep; d += 0.5f) {
      float a = radians(d);
      float ca = cosf(a), sa = sinf(a);
      for (int r = 18; r <= 21; r++) display.drawPixel(calCx + (int)lroundf(r * ca), calCy + (int)lroundf(r * sa), ORANGE);
    }

    // Round cap at leading tip
    float tipA = radians(-90.0f + (float)calSweep);
    int tx = calCx + (int)lroundf(calR * cosf(tipA));
    int ty = calCy + (int)lroundf(calR * sinf(tipA));
    display.fillCircle(tx, ty, 2, ORANGE);
  }
  drawCustomIcon(display, 24, 117, ORANGE, CustomIcon::caloriesIcon, 24, 24);
  String calStr = String((int)calorieEstimate) + "/" + String(tgtCal);
  int calW = textWidth(calStr, 3);
  int calX = 8 + (224 - calW) / 2;
  label(calX, 119, calStr, 3, fgCol);
  label(220 - textWidth("KCAL", 1), 125, "KCAL", 1, mutedCol);

  // Row 3 (y = 166, h = 54): Two 108x54 pills
  display.fillRoundRect(8, 166, 108, 54, 27, cardBg);
  String distStr = (rideStarted || activityPoints ? String(ride.distanceKm, 2) : String("0.00"));
  centeredLabel(8, 183, 108, distStr, 3, fgCol);

  display.fillRoundRect(124, 166, 108, 54, 27, cardBg);
  String pwrStr = runningMode ? paceText(ride.speedKmh) : (blePowerReady ? String(ride.powerW) : "0");
  int pwrTextW = textWidth(pwrStr, 3);
  int pwrBlockW = 30 + 6 + pwrTextW;
  int pwrStartX = 124 + (108 - pwrBlockW) / 2;
  drawCustomIcon(display, pwrStartX, 178, 0xFEE0, CustomIcon::powerIcon, 30, 30);
  label(pwrStartX + 36, 183, pwrStr, 3, fgCol);

  // Row 4 (y = 230, h = 54): 224x54 pill with Bike icon + Duration
  display.fillRoundRect(8, 230, 224, 54, 27, cardBg);
  String durStr = durationText();
  int durTextW = textWidth(durStr, 3);
  int durBlockW = 30 + 10 + durTextW;
  int durStartX = 8 + (224 - durBlockW) / 2;
  drawCustomIcon(display, durStartX, 242, TEAL, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 30, 30);
  label(durStartX + 40, 247, durStr, 3, fgCol);
}

static const float zoomRadiusKm[] = {0, 4.0f, 1.5f, 0.5f, 0.2f, 0.08f};
static void drawMapScreen() {
  statusBar();

  // Outer Map Viewport Card (Frame 30)
  const int cardX = 8, cardY = 30, cardW = 224, cardH = 190, cardR = 20;
  const uint16_t terrainBg = darkTheme ? 0x18E3 : 0xF79D;
  const uint16_t mapBorder = darkTheme ? 0x2945 : 0xD69A;
  display.fillRoundRect(cardX, cardY, cardW, cardH, cardR, terrainBg);
  display.drawRoundRect(cardX, cardY, cardW, cardH, cardR, mapBorder);

  // Clipping bounds inside the card
  const int clipX1 = cardX + 2;
  const int clipY1 = cardY + 2;
  const int clipX2 = cardX + cardW - 3;
  const int clipY2 = cardY + cardH - 3;

  // Center pin location on display
  const int cardCenterX = 120, cardCenterY = 125;

  double centerLat = (ride.gpsFixed && isfinite(ride.lat) && ride.lat != 0.0) ? ride.lat : (routeLen > 0 ? route[0].lat : 22.594637);
  double centerLon = (ride.gpsFixed && isfinite(ride.lon) && ride.lon != 0.0) ? ride.lon : (routeLen > 0 ? route[0].lon : 88.425853);

  // Dynamic LRU 3x3 sliding window in PSRAM: load neighbouring chunks, automatically unload out-of-range chunks
  g_mapCache.updatePosition(centerLat, centerLon);

  // Fixed Max Zoom Level 4 for maximum performance, zero lag, and crisp street detail
  const float scale = 0.85f;

  int centerTX, centerTY;
  float cfx, cfy;
  MapTileCache::latLonToTile(centerLat, centerLon, centerTX, centerTY, cfx, cfy);
  int centerU = (int)lround(cfx * 4096.0f);
  int centerV = (int)lround(cfy * 4096.0f);

  double latRad = centerLat * (PI / 180.0);
  float metersPerUnit = (float)((40075016.686 * cos(latRad) / 16384.0) / 4096.0);
  float pxPerUnit = metersPerUnit * scale;

  // Course-Up (default): counter-rotate map by -s_fusedHeading so travel direction is straight UP.
  // North-Up: rotHeading = 0 (map locked to North at top).
  float rotHeading = activityMapNorthUp ? 0.0f : s_fusedHeading;
  float rad = radians(rotHeading);
  float cosA = cosf(rad);
  float sinA = sinf(rad);
  float cosP = cosA * pxPerUnit;
  float sinP = sinA * pxPerUnit;

  // Rotation-invariant radial bound for culling
  int maxDistU = (int)(160.0f / pxPerUnit) + 120;

  auto projectTilePoint = [&](int du, int dv, int &outX, int &outY) {
    outX = cardCenterX + (int)lroundf(du * cosP + dv * sinP);
    outY = cardCenterY + (int)lroundf(-du * sinP + dv * cosP);
  };

  static int s_mapPolyX[256];
  static int s_mapPolyY[256];

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
      if ((p0X > maxDistU && pEndX > maxDistU) || (p0X < -maxDistU && pEndX < -maxDistU) ||
          (p0Y > maxDistU && pEndY > maxDistU) || (p0Y < -maxDistU && pEndY < -maxDistU)) continue;

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
        projectTilePoint(tileDeltaX + pts[off + k].u, tileDeltaY + pts[off + k].v, s_mapPolyX[k], s_mapPolyY[k]);
      }
      if (ft == FEAT_PARK || isClosed) {
        uint16_t fCol = (ft == FEAT_PARK) ? parkFill : waterFill;
        uint16_t bCol = (ft == FEAT_PARK) ? parkBorder : waterBorder;
        fillPolygon(s_mapPolyX, s_mapPolyY, cnt, fCol, clipX1, clipY1, clipX2, clipY2);
        for (uint8_t k = 1; k < cnt; k++) {
          clipAndDrawLine(s_mapPolyX[k - 1], s_mapPolyY[k - 1], s_mapPolyX[k], s_mapPolyY[k], bCol, 1);
        }
      } else {
        for (uint8_t k = 1; k < cnt; k++) {
          clipAndDrawLine(s_mapPolyX[k - 1], s_mapPolyY[k - 1], s_mapPolyX[k], s_mapPolyY[k], waterFill, 3);
        }
      }
    }
  }

  // PASS 2: Road Casings (seamless junction outlines)
  const int minorCasingW = 5, minorFillW = 3;
  const int majorCasingW = 7, majorFillW = 5;

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
      if ((p0X > maxDistU && pEndX > maxDistU) || (p0X < -maxDistU && pEndX < -maxDistU) ||
          (p0Y > maxDistU && pEndY > maxDistU) || (p0Y < -maxDistU && pEndY < -maxDistU)) continue;

      bool isMajor = (ft == FEAT_MOTORWAY || ft == FEAT_PRIMARY || ft == FEAT_SECONDARY);
      int w = isMajor ? majorCasingW : minorCasingW;

      int sx0, sy0;
      projectTilePoint(p0X, p0Y, sx0, sy0);
      for (uint8_t k = 1; k < cnt; k++) {
        if (abs(pts[off + k].u - pts[off + k - 1].u) > 500 || abs(pts[off + k].v - pts[off + k - 1].v) > 500) {
          projectTilePoint(tileDeltaX + pts[off + k].u, tileDeltaY + pts[off + k].v, sx0, sy0);
          continue;
        }
        int pkX = tileDeltaX + pts[off + k].u;
        int pkY = tileDeltaY + pts[off + k].v;
        int sx1, sy1;
        projectTilePoint(pkX, pkY, sx1, sy1);
        clipAndDrawLine(sx0, sy0, sx1, sy1, roadCasing, w);
        sx0 = sx1;
        sy0 = sy1;
      }
    }
  }

  // PASS 3: Road Inner Fills
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
      if ((p0X > maxDistU && pEndX > maxDistU) || (p0X < -maxDistU && pEndX < -maxDistU) ||
          (p0Y > maxDistU && pEndY > maxDistU) || (p0Y < -maxDistU && pEndY < -maxDistU)) continue;

      bool isMajor = (ft == FEAT_MOTORWAY || ft == FEAT_PRIMARY || ft == FEAT_SECONDARY);
      int w = isMajor ? majorFillW : minorFillW;

      int sx0, sy0;
      projectTilePoint(p0X, p0Y, sx0, sy0);
      for (uint8_t k = 1; k < cnt; k++) {
        if (abs(pts[off + k].u - pts[off + k - 1].u) > 500 || abs(pts[off + k].v - pts[off + k - 1].v) > 500) {
          projectTilePoint(tileDeltaX + pts[off + k].u, tileDeltaY + pts[off + k].v, sx0, sy0);
          continue;
        }
        int pkX = tileDeltaX + pts[off + k].u;
        int pkY = tileDeltaY + pts[off + k].v;
        int sx1, sy1;
        projectTilePoint(pkX, pkY, sx1, sy1);
        clipAndDrawLine(sx0, sy0, sx1, sy1, roadFill, w);
        sx0 = sx1;
        sy0 = sy1;
      }
    }
  }

  // Fast Lat/Lon to pixel projection helper (with dynamic map counter-rotation)
  auto projectLatLon = [&](double pLat, double pLon, int &outX, int &outY) {
    int pTX, pTY;
    float pfx, pfy;
    MapTileCache::latLonToTile(pLat, pLon, pTX, pTY, pfx, pfy);
    int pU = (int)lround(pfx * 4096.0f);
    int pV = (int)lround(pfy * 4096.0f);
    int dX = (pTX - centerTX) * 4096 + (pU - centerU);
    int dY = (pTY - centerTY) * 4096 + (pV - centerV);
    projectTilePoint(dX, dY, outX, outY);
  };

  const uint16_t stravaOrange = 0xFA40; // Vibrant Strava Orange (#FC5200)
  bool isNavigating = (nav.hasDestination || routeLen > 1);

  if (isNavigating && routeLen > 1) {
    int sx0, sy0;
    projectLatLon(route[0].lat, route[0].lon, sx0, sy0);
    for (int i = 1; i < routeLen; i++) {
      int sx1, sy1;
      projectLatLon(route[i].lat, route[i].lon, sx1, sy1);
      clipAndDrawLine(sx0, sy0, sx1, sy1, stravaOrange, 3);
      sx0 = sx1;
      sy0 = sy1;
    }
    int sx, sy, ex, ey;
    projectLatLon(route[0].lat, route[0].lon, sx, sy);
    projectLatLon(route[routeLen - 1].lat, route[routeLen - 1].lon, ex, ey);
    if (sx >= clipX1 - 10 && sx <= clipX2 + 10 && sy >= clipY1 - 10 && sy <= clipY2 + 10) drawStartNodeIcon(display, sx - 10, sy - 10);
    if (ex >= clipX1 - 10 && ex <= clipX2 + 10 && ey >= clipY1 - 10 && ey <= clipY2 + 10) drawFinishNodeIcon(display, ex - 10, ey - 10);
  } else {
    // If NOT navigating: livestream recorded breadcrumbs behind rider in Strava Orange (#FC5200)
    if (trackLen > 0 && track) {
      int tx0, ty0;
      projectLatLon(track[0].lat, track[0].lon, tx0, ty0);
      for (int i = 1; i < trackLen; i++) {
        int tx1, ty1;
        projectLatLon(track[i].lat, track[i].lon, tx1, ty1);
        if (abs(tx1 - tx0) >= 2 || abs(ty1 - ty0) >= 2 || i == trackLen - 1) {
          clipAndDrawLine(tx0, ty0, tx1, ty1, stravaOrange, 3);
          tx0 = tx1;
          ty0 = ty1;
        }
      }
      if (ride.gpsFixed && isfinite(ride.lat) && ride.lat != 0.0) {
        int cx1, cy1;
        projectLatLon(ride.lat, ride.lon, cx1, cy1);
        clipAndDrawLine(tx0, ty0, cx1, cy1, stravaOrange, 3);
      }
    }
  }

  // Center Navigation Marker (Frame 8 & SVG: 36x36 Blue Circle with Crisp White Heading Chevron)
  // In Course-Up mode (default): marker always renders straight UP (0.0f), while map below counter-rotates.
  // In North-Up mode: map is locked to North (rotHeading = 0.0f), and marker rotates to s_fusedHeading.
  float markerHeading = activityMapNorthUp ? s_fusedHeading : 0.0f;
  drawMapHeadingMarker(display, cardCenterX, cardCenterY, markerHeading);

  // Top-Right Floating Compass Coin (Frame 8)
  // In Course-Up mode: needle points to real Earth North (360 - s_fusedHeading).
  // In North-Up mode: compass needle locks straight UP to North (0.0f).
  float compassH = activityMapNorthUp ? 0.0f : fmodf(360.0f - s_fusedHeading + 360.0f, 360.0f);
  drawCompassCoin(display, 202, 54, compassH);

  // Erase any vector bleeding at the 4 rounded card corners (R=20)
  for (int dy = 0; dy < 20; dy++) {
    for (int dx = 0; dx < 20; dx++) {
      if ((19 - dx) * (19 - dx) + (19 - dy) * (19 - dy) > 400) {
        display.drawPixel(cardX + dx, cardY + dy, uiBg());
        display.drawPixel(cardX + cardW - 1 - dx, cardY + dy, uiBg());
        display.drawPixel(cardX + dx, cardY + cardH - 1 - dy, uiBg());
        display.drawPixel(cardX + cardW - 1 - dx, cardY + cardH - 1 - dy, uiBg());
      }
    }
  }

  // Re-stroke map border around viewport card
  display.drawRoundRect(cardX, cardY, cardW, cardH, cardR, mapBorder);

  // Bottom Split HUD (Frame 30: SPEED & DISTANCE dual pills at y = 230..284)
  // Left Pill: SPEED (Dark cyan background, cyan text)
  uint16_t spdBg = darkTheme ? 0x09A5 : 0xCFF7;
  uint16_t spdFg = darkTheme ? 0x475F : 0x0395;
  display.fillRoundRect(8, 230, 108, 54, 27, spdBg);
  float curSpeed = speedAvailable() ? ride.speedKmh : 0.0f;
  if (curSpeed < 0.1f) curSpeed = 0.0f;
  String sp = runningMode ? paceText(curSpeed) : String(curSpeed, 1);
  centeredLabel(8, 247, 108, sp, 3, spdFg);

  // Right Pill: DISTANCE (Dark card background, white text)
  display.fillRoundRect(124, 230, 108, 54, 27, darkTheme ? 0x18C3 : 0xEF5D);
  String distStr = (rideStarted || activityPoints ? String(ride.distanceKm, 2) : String("0.00"));
  centeredLabel(124, 247, 108, distStr, 3, uiFg());
}

static void drawEndConfirmScreen() {
  statusBar();
  uint16_t fgCol = uiFg();
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // Prominent Red Stop / Warning Badge Disc
  display.fillCircle(120, 64, 24, 0xF800);
  display.fillRect(118, 50, 4, 18, 0xFFFF);
  display.fillCircle(120, 74, 2, 0xFFFF);

  // Large Bold Title
  centeredLabel(8, 98, 224, runningMode ? "END RUN?" : "END RIDE?", 3, fgCol);

  // Activity Summary Metrics Subtitle
  String summaryStr = durationText() + "  |  " + (rideStarted || activityPoints ? String(ride.distanceKm, 2) + " KM" : "0.00 KM");
  centeredLabel(8, 130, 224, summaryStr, 1, TEAL);

  // Big Capsule Button 1: Save & Finish (Solid Red, 224x54, R=27)
  display.fillRoundRect(8, 166, 224, 54, 27, 0xF800);
  centeredLabel(8, 184, 224, "Save & Finish", 2, 0xFFFF);

  // Big Capsule Button 2: Resume Activity (Card Bg, 224x54, R=27)
  display.fillRoundRect(8, 230, 224, 54, 27, cardBg);
  centeredLabel(8, 248, 224, "Resume Activity", 2, fgCol);
}

static void drawActivityControlsScreen() {
  if (activityEndConfirm) {
    drawEndConfirmScreen();
    return;
  }
  statusBar();
  uint16_t fgCol = uiFg();
  uint16_t mutedCol = uiMuted();
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // Row 1 (y = 38, h = 54): 224x54 pill with Bike icon + Duration
  display.fillRoundRect(8, 38, 224, 54, 27, cardBg);
  String durStr = durationText();
  int durTextW = textWidth(durStr, 3);
  int durBlockW = 30 + 10 + durTextW;
  int durStartX = 8 + (224 - durBlockW) / 2;
  drawCustomIcon(display, durStartX, 50, TEAL, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 30, 30);
  label(durStartX + 40, 55, durStr, 3, fgCol);

  // Row 2 (y = 102, h = 54): Auto Pause Activity toggle
  display.fillRoundRect(8, 102, 224, 54, 27, cardBg);
  label(24, 125, "Auto Pause Activity", 1, fgCol);
  uint16_t swBg = autoPauseEnabled ? (darkTheme ? 0x3CE0 : 0x2CE0) : (darkTheme ? 0x31A6 : 0xD69A);
  display.fillRoundRect(168, 114, 48, 30, 15, swBg);
  display.fillCircle(autoPauseEnabled ? 201 : 183, 129, 11, 0xFFFF);

  // Row 3 (y = 166, h = 54): Pause Ride warm amber card
  uint16_t amberCardBg = darkTheme ? 0x4220 : 0xFFE0;
  uint16_t amberText = darkTheme ? 0xFEE0 : 0x8C00;
  display.fillRoundRect(8, 166, 224, 54, 27, amberCardBg);
  label(24, 189, ridePaused ? "Resume Ride" : "Pause Ride", 1, amberText);
  display.fillCircle(198, 193, 17, 0xFFFF);
  if (!ridePaused) {
    display.fillRect(193, 186, 3, 14, 0xF800);
    display.fillRect(199, 186, 3, 14, 0xF800);
  } else {
    display.fillTriangle(195, 186, 195, 200, 204, 193, 0x05E0);
  }

  // Row 4 (y = 230, h = 54): REVERSE SLIDER "Swipe to end" (Exact mirror of Home Page slider)
  display.fillRoundRect(8, 230, 224, 54, 27, cardBg);
  int textW = textWidth("Swipe to end", 0);
  int trackStartX = 8 + (116 - textW) / 2;
  label(trackStartX, 230 + 21, "Swipe to end", 0, mutedCol);

  int pillX = 124 + controlSliderOffset;
  display.fillRoundRect(pillX, 230, 108, 54, 27, 0xF800);
  drawCustomIconFlippedH(display, pillX + 27, 230 + 20, 0xFFFF, CustomIcon::goNextIcon, 14, 14);
  drawCustomIcon(display, pillX + 53, 230 + 13, 0xFFFF, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 28, 28);


}

static void drawAutoPauseScreen() {
  statusBar();
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // Upper Card: 224x182, R = 24 (y = 34..216)
  display.fillRoundRect(8, 34, 224, 182, 24, cardBg);

  // Big White Circle Disc: center (120, 106), r = 30
  display.fillCircle(120, 106, 30, 0xFFFF);
  display.fillRect(111, 94, 6, 24, 0xF800);
  display.fillRect(123, 94, 6, 24, 0xF800);

  // Centered label "Auto paused"
  centeredLabel(8, 160, 224, "Auto paused", 3, uiFg());

  // Lower Green Pill: 224x54, R = 27 (y = 228..282)
  uint16_t greenBg = darkTheme ? 0x3CE0 : 0x2CE0;
  display.fillRoundRect(8, 228, 224, 54, 27, greenBg);
  String ovrStr = "Override";
  int ovrTextW = textWidth(ovrStr, 3);
  int ovrBlockW = 30 + 10 + ovrTextW;
  int ovrStartX = 8 + (224 - ovrBlockW) / 2;
  drawCustomIcon(display, ovrStartX, 240, 0xFFFF, runningMode ? CustomIcon::runIconLarge : CustomIcon::bikeIconLarge, 30, 30);
  label(ovrStartX + 40, 245, ovrStr, 3, 0xFFFF);
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
  JsonDocument doc;
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
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "Bad JSON"); return; }
  if(!doc.is<JsonArray>()||doc.size()>MAX_ROUTE){server.send(400,"text/plain","Route must be an array with at most 512 points");return;}
  for(JsonObject pt:doc.as<JsonArray>())if(!pt["lat"].is<double>()||!pt["lon"].is<double>()||fabs(pt["lat"].as<double>())>90||fabs(pt["lon"].as<double>())>180){server.send(400,"text/plain","Invalid point");return;}
  routeLen = 0;
  for (JsonObject pt : doc.as<JsonArray>()) {
    if (routeLen >= MAX_ROUTE) break;
    route[routeLen++] = {pt["lat"].as<float>(), pt["lon"].as<float>()};
  }
  snapActiveRoute();
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
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "Bad JSON"); return; }
  if(!doc["ageYears"].isNull())cfg.ageYears=doc["ageYears"];
  if(!doc["restingHR"].isNull())cfg.restingHR=doc["restingHR"];
  if(!doc["maximumHR"].isNull())cfg.maximumHR=doc["maximumHR"];
  if(!doc["calorieSex"].isNull())cfg.calorieSex=constrain(doc["calorieSex"].as<int>(),0,2);
  if(!doc["mass"].isNull())cfg.massConfirmed=true;
  if (!doc["gearRatio"].isNull())       cfg.gearRatio        = doc["gearRatio"];
  if (!doc["wheelCircumM"].isNull())    cfg.wheelCircumM     = doc["wheelCircumM"];
  if (!doc["mass"].isNull())            cfg.riderMassKg      = doc["mass"];
  if (!doc["maxSpeedKmh"].isNull())     cfg.maxSpeedKmh      = doc["maxSpeedKmh"];
  if (!doc["maxPowerW"].isNull())       cfg.maxPowerW        = doc["maxPowerW"];
  if (!doc["maxCadenceRpm"].isNull())   cfg.maxCadRpm        = doc["maxCadenceRpm"];
  if (!doc["vo2maxManual"].isNull())    cfg.vo2maxManual     = doc["vo2maxManual"];
  if (!doc["targetDistanceKm"].isNull()) cfg.targetDistanceKm = doc["targetDistanceKm"];
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
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "Bad JSON"); return; }
  if (!doc["cadence"].isNull())    sensors.cadencePhysical = doc["cadence"].as<bool>();
  if (!doc["speed"].isNull())      sensors.speedPhysical   = doc["speed"].as<bool>();
  if (!doc["cadenceRpm"].isNull()) sensors.physCadenceRpm  = doc["cadenceRpm"];
  if (!doc["speedKmh"].isNull())   sensors.physSpeedKmh    = doc["speedKmh"];
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
  pinMode(6, INPUT_PULLUP);
  if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    uint32_t t = millis();
    while(digitalRead(6) == LOW && millis() - t < 1500) delay(10);
    if(digitalRead(6) != LOW) {
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_6, 0);
      esp_deep_sleep_start();
    }
  }
  setenv("TZ", "IST-5:30", 1);
  tzset();
  Serial.begin(115200);

  // 1. Maintain battery power latch & display backlight pin
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  pinMode(6, INPUT_PULLUP);
  pinMode(5, OUTPUT); digitalWrite(5, LOW);

  // 2. Initialize PSRAM FIRST so 8MB external RAM is active for framebuffer
  psramInit();
  if (psramFound()) {
    track = (TrackPoint*)ps_malloc(MAX_TRACK * sizeof(TrackPoint));
    if (track) memset(track, 0, MAX_TRACK * sizeof(TrackPoint));
  } else {
    track = (TrackPoint*)malloc(1024 * sizeof(TrackPoint));
  }

  if(prefs.begin("cyclo", true)){
    darkTheme = prefs.getBool("darkTheme", false);
    appsListView = prefs.getBool("appsList", false);
    autoPauseEnabled = prefs.getBool("autoPause", true);
    activeFontId = prefs.getUChar("fontId", FONT_METROPOLIS);
    if (activeFontId > FONT_TEKTUR) activeFontId = FONT_METROPOLIS;
    prefs.end();
  }

  // Restore clock and load security settings
  restoreSystemTime();
  initSecuritySettings();

  // 3. Initialize Display & Turn ON Backlight IMMEDIATELY
  lcdSPI.begin(40, -1, 45, 42);
  display.init(240, 320);
  display.setRotation(0);
  display.invertDisplay(true);
  digitalWrite(5, HIGH); // Screen turns ON here!
  drawSplash();
  display.refresh();

  // 4. Sensors & I/O
  initQuickTouch();
  externalBusReady = Wire.begin(COMPASS_SDA, COMPASS_SCL);
  Wire.setTimeOut(30);
  buttonsOk = externalPresent(0x20) && buttons.begin_I2C(0x20, &Wire);
  if(buttonsOk) for(int i = 0; i < 4; i++) buttons.pinMode(i, INPUT_PULLUP);
  bmpOk = beginBarometer();
  if(externalPresent(0x0D)) compass.begin();
  gpsSerial.setRxBufferSize(2048);
  gpsSerial.begin(115200, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  server.on("/compass", HTTP_POST, [](){
    JsonDocument d; if(deserializeJson(d, server.arg("plain"))){ server.send(400, "text/plain", "Invalid JSON"); return; }
    String action = d["action"] | "";
    if(action == "start") compass.start();
    else if(action == "finish"){ if(!compass.finish()){ server.send(400, "text/plain", "Insufficient rotation: rotate entire device level through full circles"); return; } }
    else if(action == "alignment"){ float y = d["yawOffset"] | 0.f, dec = d["declination"] | 0.f; if(!isfinite(y)||!isfinite(dec)||fabs(y)>360||fabs(dec)>90){ server.send(400, "text/plain", "Invalid angle"); return; } compass.yawOffset = y; compass.declination = dec; compass.save(); }
    else { server.send(400, "text/plain", "Use action start, finish or alignment"); return; }
    server.send(200, "text/plain", "OK");
  });

  // 5. Safe MicroSD Initialization (never call end() if begin() failed)
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(50);
  const uint32_t sdClocksKHz[] = {20000, 4000, 400};
  if (SD_MMC.setPins(SD_SCK_PIN, SD_MOSI_PIN, SD_MISO_PIN)) {
    for (uint32_t khz : sdClocksKHz) {
      Serial.printf("SDMMC: mounting 1-bit at %lu kHz\n", (unsigned long)khz);
      if (SD_MMC.begin("/sd", true, false, khz, 5)) {
        if (SD_MMC.cardType() != CARD_NONE) {
          sdOk = true;
          break;
        }
        SD_MMC.end();
      }
      sdOk = false;
      delay(50);
    }
  }
  if (sdOk) {
    sdStatus = "Mounted: " + String((unsigned long)(SD_MMC.cardSize() / (1024 * 1024))) + " MiB";
    Serial.printf("SDMMC: filesystem %llu MiB, used %llu MiB\n",
      SD_MMC.totalBytes() / (1024 * 1024), SD_MMC.usedBytes() / (1024 * 1024));
    SD_MMC.mkdir("/maps");
    g_mapCache.begin();
    if (SD_MMC.exists("/route.json")) {
      File f = SD_MMC.open("/route.json", "r");
      if (f) {
        JsonDocument doc;
        if (!deserializeJson(doc, f)) {
          routeLen = 0;
          for (JsonObject pt : doc.as<JsonArray>()) {
            if (routeLen >= MAX_ROUTE) break;
            route[routeLen++] = {pt["lat"].as<float>(), pt["lon"].as<float>()};
          }
          snapActiveRoute();
          if (routeLen > 0) {
            nav.hasDestination = true;
            nav.destLat = route[routeLen-1].lat;
            nav.destLon = route[routeLen-1].lon;
          }
        }
        f.close();
      }
    }
  } else {
    sdStatus = "Mount failed: check seating / FAT32";
  }
  Serial.printf("SDMMC: %s\n", sdStatus.c_str());

  // 6. Load config from NVS
  prefs.begin("cyclo", false);
  cfg.ageYears         = prefs.getFloat("age", 0);
  cfg.restingHR        = prefs.getFloat("restHR", 0);
  cfg.maximumHR        = prefs.getFloat("maxHR", 0);
  cfg.calorieSex       = prefs.getInt("calSex", 0);
  cfg.massConfirmed    = prefs.getBool("massSet", false);
  cfg.gearRatio        = prefs.getFloat("gearRatio", cfg.gearRatio);
  cfg.wheelCircumM     = prefs.getFloat("wheelCircum", cfg.wheelCircumM);
  cfg.riderMassKg      = prefs.getFloat("riderMass", cfg.riderMassKg);
  cfg.maxSpeedRide     = prefs.getFloat("maxSpdRide", 60.0f);
  cfg.maxSpeedRun      = prefs.getFloat("maxSpdRun", 20.0f);
    cfg.countdownSec     = prefs.getInt("cntdownSec", 3);
    if (cfg.countdownSec != 3 && cfg.countdownSec != 5 && cfg.countdownSec != 10) cfg.countdownSec = 3;
    homeHeroClock        = prefs.getBool("heroClock", false);
    activityMapNorthUp   = prefs.getBool("mapNorthUp", false);
    cfg.targetCalories   = prefs.getInt("tgtCalories", 1200);
    if (cfg.targetCalories < 100 || cfg.targetCalories > 10000) cfg.targetCalories = 1200;
  cfg.maxSpeedKmh      = runningMode ? cfg.maxSpeedRun : cfg.maxSpeedRide;
  cfg.maxPowerW        = prefs.getFloat("maxPower", cfg.maxPowerW);
  cfg.maxCadRpm        = prefs.getFloat("maxCad", cfg.maxCadRpm);
  cfg.vo2maxManual     = prefs.getFloat("vo2max", cfg.vo2maxManual);
  cfg.targetDistanceKm = prefs.getFloat("targetDist", cfg.targetDistanceKm);
  prefs.end();

  // 7. Start Wi-Fi AP & Web Server
  WiFi.mode(WIFI_AP);
  if(!WiFi.softAP("CycloComp", "cycling1234")) Serial.println("Wi-Fi unavailable: AP initialization failed");

  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/time",   HTTP_POST, [](){
    JsonDocument d;
    if(deserializeJson(d, server.arg("plain")) || !d["epoch"].is<int64_t>()){ server.send(400, "text/plain", "Invalid time"); return; }
    int64_t epoch = d["epoch"].as<int64_t>();
    if(epoch < 1704067200LL || epoch > 4102444799LL){ server.send(400, "text/plain", "Time out of range"); return; }
    if(!(gps.date.isValid() && gps.time.isValid() && gps.date.age() < 3000 && gps.time.age() < 3000)){
      timeval tv = {}; tv.tv_sec = (time_t)epoch; settimeofday(&tv, nullptr);
    }
    uiRevision++; server.send(200, "text/plain", "Clock synced: Kolkata IST (UTC+05:30)");
  });
  server.on("/ui",     HTTP_POST, [](){
    JsonDocument d; if(deserializeJson(d, server.arg("plain"))){ server.send(400, "text/plain", "Invalid JSON"); return; }
    if(d["screen"].is<int>()){ int n = d["screen"]; if(n >= 0 && n < SCR_COUNT) currentScreen = (Screen)n; }
    if(d["quick"].is<bool>()){ quickOpen = d["quick"]; quickDetail = 0; }
    if(d["dark"].is<bool>()) setDarkTheme(d["dark"].as<bool>());
    uiRevision++; server.send(200, "text/plain", "Display updated");
  });
  server.on("/status", HTTP_GET,  handleStatus);
  server.on("/nav",    HTTP_POST, handleNavSet);
  server.on("/upload", HTTP_POST, handleUploadRoute);
  server.on("/config", HTTP_POST, handleConfig);
  server.on("/sensor", HTTP_POST, handleSensor);
  server.on("/export", HTTP_GET,  handleExportGPX);
  server.on("/log",    HTTP_GET,  [](){
    File f = SD_MMC.open("/ride.csv", "r"); if(!f){ server.send(404, "text/plain", "No SD log"); return; }
    server.sendHeader("Content-Disposition", "attachment; filename=ride.csv"); server.streamFile(f, "text/csv"); f.close();
  });
  server.begin();

  // 8. Start Bluetooth LE AFTER Wi-Fi & Display are up and running!
  bleBegin();
  bleHttp();
  antSensorsBegin();
}

static uint32_t lastDispMs  = 0;
static uint32_t lastTrackMs = 0;


static File uploadFile;

static void handleSerialTransfer() {
  if (!Serial.available()) return;
  if (Serial.peek() != 'C' && Serial.peek() != 'S' && Serial.peek() != 'T') {
    while (Serial.available() && Serial.peek() != 'C' && Serial.peek() != 'S' && Serial.peek() != 'T') Serial.read();
    if (!Serial.available()) return;
  }
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.startsWith("CMD:SYNC_TIME ") || line.startsWith("SYNC_TIME:") || line.startsWith("TIME:")) {
    int idx = line.indexOf(' ');
    if (idx < 0) idx = line.indexOf(':');
    time_t epoch = (time_t)strtoul(line.substring(idx + 1).c_str(), nullptr, 10);
    if (epoch > 1704067200) {
      struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
      settimeofday(&tv, nullptr);
      persistSystemTime(); // Instantly persist to RTC fast memory & NVS preferences
      struct tm local;
      localtime_r(&epoch, &local);
      Serial.printf("OK:TIME_SYNC %ld %02d:%02d:%02d\n",
                    (long)epoch, local.tm_hour, local.tm_min, local.tm_sec);
      uiRevision++;
    } else {
      Serial.println("ERR:INVALID_EPOCH");
    }
    return;
  }
  if (line == "CMD:PING") {
    Serial.println("PONG");
    return;
  }
  if (line.startsWith("CMD:BAT ") || line.startsWith("BAT:")) {
    String arg = line.startsWith("CMD:BAT ") ? line.substring(8) : line.substring(4);
    arg.trim();
    if (arg == "CHG" || arg == "CHARGE") {
      batteryCharging = true;
      batteryAlert = false;
      Serial.println("OK:BAT_CHARGING");
    } else if (arg == "ALERT") {
      batteryCharging = false;
      batteryAlert = true;
      Serial.println("OK:BAT_ALERT");
    } else {
      int val = arg.toInt();
      batteryPercent = constrain(val, 0, 100);
      batteryCharging = false;
      batteryAlert = (batteryPercent < 10);
      Serial.printf("OK:BAT_SET %d\n", batteryPercent);
    }
    uiRevision++;
    return;
  }
  if (line == "CMD:SYS_INFO") {
    Serial.printf("RESET_REASON:%d\n", (int)esp_reset_reason());
    Serial.printf("GPS_READY:%d\n", (int)gpsDataReady);
    Serial.printf("GPS_SATS:%u\n", startSatellites());
    Serial.printf("GEPRC_SATS:%u\n", (unsigned)geprcGps.satellitesUsed);
    Serial.printf("GEPRC_PVT_AGE_MS:%lu\n", (unsigned long)(geprcGps.lastPvt ? (millis() - geprcGps.lastPvt) : 999999));
    Serial.printf("GEPRC_MSG_AGE_MS:%lu\n", (unsigned long)(geprcGps.lastMessage ? (millis() - geprcGps.lastMessage) : 999999));
    Serial.printf("TINYGPS_PASSED:%lu\n", (unsigned long)gps.passedChecksum());
    Serial.printf("TINYGPS_FAILED:%lu\n", (unsigned long)gps.failedChecksum());
    Serial.printf("COMPASS_READY:%d\n", (int)compass.ready);
    Serial.printf("COMPASS_HEADING:%.1f\n", compass.heading);
    Serial.printf("COMPASS_SAMPLE_AGE_MS:%lu\n", (unsigned long)(compass.lastSample ? (millis() - compass.lastSample) : 999999));
    Serial.printf("COMPASS_RAW:%d,%d,%d\n", compass.raw[0], compass.raw[1], compass.raw[2]);
    Serial.printf("BUTTONS_OK:%d\n", (int)buttonsOk);
    Serial.printf("CURRENT_SCREEN:%d\n", (int)currentScreen);
    Serial.printf("FREE_HEAP:%lu\n", (unsigned long)ESP.getFreeHeap());
    Serial.printf("FREE_PSRAM:%lu\n", (unsigned long)ESP.getFreePsram());
    return;
  }
  if (line == "CMD:I2C_SCAN") {
    Serial.println("I2C_SCAN_START");
    Serial.printf("Scanning Wire (SDA:%d, SCL:%d)...\n", COMPASS_SDA, COMPASS_SCL);
    int wireCount = 0;
    for (uint8_t a = 1; a < 127; a++) {
      Wire.beginTransmission(a);
      uint8_t err = Wire.endTransmission();
      if (err == 0) {
        Serial.printf("  Wire FOUND: 0x%02X\n", a);
        wireCount++;
      }
    }
    Serial.printf("Wire found %d devices\n", wireCount);

    Serial.printf("Scanning touchBus (SDA:%d, SCL:%d)...\n", 1, 3);
    int touchCount = 0;
    for (uint8_t a = 1; a < 127; a++) {
      touchBus.beginTransmission(a);
      uint8_t err = touchBus.endTransmission();
      if (err == 0) {
        Serial.printf("  touchBus FOUND: 0x%02X\n", a);
        touchCount++;
      }
    }
    Serial.printf("touchBus found %d devices\n", touchCount);
    // Probe 0x7E registers:
    Wire.beginTransmission(0x7E);
    Wire.write(0x00);
    uint8_t err7e = Wire.endTransmission();
    Serial.printf("0x7E write reg 0 err: %u\n", err7e);
    if (err7e == 0 && Wire.requestFrom(uint8_t(0x7E), uint8_t(1)) == 1) {
      Serial.printf("0x7E reg 0x00: 0x%02X\n", Wire.read());
    }

    Serial.printf("bmpOk: %d\n", (int)bmpOk);
    Serial.println("I2C_SCAN_END");
    return;
  }
  if (line == "CMD:COMPASS_DEBUG") {
    Serial.printf("COMPASS_READY:%d\n", (int)compass.ready);
    Serial.printf("COMPASS_CALIBRATED:%d\n", (int)compass.calibrated);
    Serial.printf("COMPASS_HEADING:%.1f\n", compass.heading);
    Serial.printf("COMPASS_RAW:%d,%d,%d\n", compass.raw[0], compass.raw[1], compass.raw[2]);
    uint32_t sampleAge = compass.lastSample ? (millis() - compass.lastSample) : 999999;
    Serial.printf("COMPASS_SAMPLE_AGE_MS:%lu\n", (unsigned long)sampleAge);
    bool sysCheckOk = compass.ready && compass.lastSample && (sampleAge < 2000);
    Serial.printf("SYS_CHECK_OK:%d\n", (int)sysCheckOk);
    Wire.beginTransmission(0x0D);
    uint8_t probeErr = Wire.endTransmission();
    Serial.printf("I2C_PROBE_0x0D_ERR:%u\n", probeErr);
    Wire.beginTransmission(0x0D);
    Wire.write(0x06);
    Wire.endTransmission();
    if (Wire.requestFrom(uint8_t(0x0D), uint8_t(1)) == 1) {
      uint8_t st = Wire.read();
      Serial.printf("REG_0x06_STATUS:0x%02X (DRDY:%d OVL:%d DOR:%d)\n", st, st&1, (st>>1)&1, (st>>2)&1);
    } else {
      Serial.println("REG_0x06_READ_FAILED");
    }
    return;
  }
  if (line == "CMD:GPS_LOG" || line.startsWith("CMD:GPS_LOG")) {
    Serial.println("START_GPS_LOG");
    uint32_t t0 = millis();
    while (millis() - t0 < 6000) {
      while (gpsSerial.available()) {
        uint8_t c = (uint8_t)gpsSerial.read();
        Serial.write(c);
        geprcGps.feed(c, gps);
      }
      if (Serial.available()) {
        while (Serial.available()) Serial.read();
        break;
      }
      delay(1);
    }
    Serial.println("\nEND_GPS_LOG");
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
  static uint32_t s_lastTimePersist = 0;
  if (millis() - s_lastTimePersist >= 30000UL) {
    s_lastTimePersist = millis();
    persistSystemTime();
  }

  handleSerialTransfer();
  static uint32_t s_lastTimeReq = 0;
  if (time(nullptr) < 1704067200 && millis() - s_lastTimeReq >= 2000UL) {
    s_lastTimeReq = millis();
    if (Serial) {
      Serial.println("REQ:TIME_SYNC");
    }
  }
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
  bool compassScreenActive = (welcomeOpen || quickOpen) && (homePage == HOME_COMPASS);
  compass.update(compassScreenActive);

  if (gps.location.isValid() && gps.location.isUpdated()) {
    float nLat = (float)gps.location.lat(), nLon = (float)gps.location.lng();

    // Check GPS fix quality: accept fixes with PDOP/HDOP < 6.0f, or valid 3D fixes when DOP is unavailable
    bool goodFixQuality = true;
    if (geprcGps.lastPvt && (millis() - geprcGps.lastPvt < 3000)) {
      goodFixQuality = (geprcGps.pdop < 6.0f);
    } else if (gps.hdop.isValid() && (gps.hdop.age() < 5000)) {
      goodFixQuality = (gps.hdop.hdop() < 6.0f);
    }

    if (rideStarted && !ridePaused && !autoPaused) {
      static uint32_t lastDistCommitMs = 0;

      // Anchor holds its position until distance exceeds displacement threshold,
      // preventing fractional sub-metre noise while seamlessly capturing walking steps (~0.5 to 1.5 m/s).
      if (distAnchorLat == 0.0f || distAnchorLon == 0.0f) {
        distAnchorLat = nLat;
        distAnchorLon = nLon;
        lastDistCommitMs = millis();
        lastAltM = ride.altM;
      } else {
        float delta = haversineKm(distAnchorLat, distAnchorLon, nLat, nLon);
        uint32_t dtMs = millis() - lastDistCommitMs;
        float dtSec = dtMs > 0 ? (dtMs / 1000.0f) : 1.0f;
        float impliedSpeedKmh = (dtSec > 0.05f) ? ((delta / dtSec) * 3600.0f) : 0.0f;

        // Current speed check (GPS or BLE sensor)
        float curSpd = ride.speedKmh > 0.0f ? ride.speedKmh : (gps.speed.isValid() && gps.speed.age() < 3000 ? (float)gps.speed.kmph() : 0.0f);
        if (!runningMode && bleSpeedReady) curSpd = max(curSpd, bleSpeed);

        // Movement conditions:
        // Accumulate once displaced at least 1.0 meter (0.0010 km) from anchor,
        // capturing walking steps (~0.5 to 1.5 m/s) and runs/rides cleanly.
        bool hasMoved = (delta >= 0.0010f && delta < 0.200f && impliedSpeedKmh < 130.0f);
        float minMotionSpd = runningMode ? 0.3f : 0.6f;
        bool isMotion = (curSpd >= minMotionSpd) || (impliedSpeedKmh >= minMotionSpd);

        if (goodFixQuality && hasMoved && isMotion) {
          distAccum += delta;
          ride.distanceKm = distAccum;
          distAnchorLat = nLat;
          distAnchorLon = nLon;
          lastDistCommitMs = millis();

          // Grade: alt change / horizontal distance (only over >= 5m to avoid division noise)
          if (delta >= 0.005f) {
            float altDelta = ride.altM - lastAltM;
            float rawGrade = (altDelta / (delta * 1000.0f)) * 100.0f;
            ride.gradePercent = constrain((0.65f * ride.gradePercent) + (0.35f * rawGrade), -30.0f, 30.0f);
            lastAltM = ride.altM;
          }

          // Advance route waypoint
          if (routeLen > 0 && nav.routeIndex < routeLen - 1) {
            float wd = haversineKm(nLat, nLon, route[nav.routeIndex+1].lat, route[nav.routeIndex+1].lon);
            if (wd < 0.025f) nav.routeIndex++;
            int next = min(nav.routeIndex + 1, routeLen - 1);
            nav.remainDistKm = haversineKm(nLat, nLon, route[next].lat, route[next].lon);
            for (int k = next; k < routeLen - 1; k++) nav.remainDistKm += haversineKm(route[k].lat, route[k].lon, route[k+1].lat, route[k+1].lon);
            nav.bearingDeg = bearingTo(nLat, nLon, route[next].lat, route[next].lon);
          }
        } else if (impliedSpeedKmh > 130.0f || delta >= 0.200f) {
          // Reject multipath teleport jump: reset anchor to current point without accumulating
          distAnchorLat = nLat;
          distAnchorLon = nLon;
          lastDistCommitMs = millis();
        } else if (dtMs > 3000) {
          // Keep time anchor fresh so dtSec does not explode while stationary
          lastDistCommitMs = millis();
        }
      }
    }
    lastLat = ride.lat = nLat;
    lastLon = ride.lon = nLon;
    ride.gpsFixed = gps.location.age() < 3000;

    // Update nav
    if (nav.hasDestination) {
      nav.remainDistKm = haversineKm(nLat, nLon, nav.destLat, nav.destLon);
      nav.bearingDeg   = bearingTo(nLat, nLon, nav.destLat, nav.destLon);
    }

    // Track breadcrumb every 3s while moving
    if (rideStarted && !ridePaused && !autoPaused && millis() - lastTrackMs > 3000) {
      lastTrackMs = millis();
      if(activityFile){fitPoint(nLat,nLon);archivePoint(nLat,nLon);}
      if (track) {
        if (trackLen >= MAX_TRACK) {
          // Decimate older half: discard every 2nd point to compress and make room
          int half = MAX_TRACK / 2;
          int newHalf = half / 2;
          for (int i = 1; i < newHalf; i++) {
            track[i] = track[i * 2];
          }
          int remaining = MAX_TRACK - half;
          memmove(&track[newHalf], &track[half], remaining * sizeof(TrackPoint));
          trackLen = newHalf + remaining;
        }
        track[trackLen++] = {nLat, nLon, ride.altM, ride.elapsedSec};
      }
    }
  } else if (!gps.location.isValid()) {
    ride.gpsFixed = false;
  }

  ride.gpsFixed = gps.location.isValid() && gps.location.age() < 3000;
  float rawGpsSpeed = (ride.gpsFixed && gps.speed.isValid() && gps.speed.age() < 3000) ? (float)gps.speed.kmph() : 0.0f;
  
  // GPS noise deadzones:
  // In outdoor ride / walk: stationary jitter up to ~0.6 km/h (running) or 1.2 km/h (cycling)
  float speedDeadzone = runningMode ? 0.6f : 1.2f;
  if (rawGpsSpeed < speedDeadzone) {
    rawGpsSpeed = 0.0f;
  }

  // Low-pass EMA filter on GPS speed to reject sudden 1-sample noise spikes
  static float filteredGpsSpeed = 0.0f;
  if (rawGpsSpeed <= 0.0f) {
    filteredGpsSpeed = 0.0f;
  } else {
    if (filteredGpsSpeed <= 0.0f) {
      filteredGpsSpeed = rawGpsSpeed;
    } else {
      filteredGpsSpeed = (0.65f * filteredGpsSpeed) + (0.35f * rawGpsSpeed);
    }
  }
  ride.speedKmh = filteredGpsSpeed;

  // Auto-Pause & Auto-Resume Detection (3-tier filter with speed hysteresis, temporal & spatial confirmation)
  if (rideStarted && !ridePaused && autoPauseEnabled) {
    float pauseSpeedThreshold = runningMode ? 0.4f : 1.0f;
    float resumeSpeedThreshold = runningMode ? 0.7f : 1.4f;
    const float highSpeedThreshold = runningMode ? 4.0f : 6.0f;
    float curSpd = speedAvailable() ? ride.speedKmh : 0.0f;

    if (!autoPaused) {
      if (curSpd < pauseSpeedThreshold && millis() > autoPauseOverrideGraceMs) {
        if (nearZeroSpeedStartMs == 0) {
          nearZeroSpeedStartMs = millis();
        } else if (millis() - nearZeroSpeedStartMs >= (runningMode ? 5000 : 4000)) { // 5s buffer in run/walk
          autoPaused = true;
          autoPauseStartMs = millis();
          nearZeroSpeedStartMs = 0;
          autoResumeMovementStartMs = 0;
          autoPauseAnchorLat = (ride.gpsFixed && gps.location.isValid()) ? (float)gps.location.lat() : 0.0f;
          autoPauseAnchorLon = (ride.gpsFixed && gps.location.isValid()) ? (float)gps.location.lng() : 0.0f;
          fitEvent(4); // Pause FIT event
          uiRevision++;
        }
      } else {
        nearZeroSpeedStartMs = 0;
      }
    } else {
      // Auto-resume check:
      // Require HDOP to be reasonable to prevent indoor multipath false triggers
      bool hdopAcceptable = (!gps.hdop.isValid() || gps.hdop.hdop() <= 5.5f);
      bool movingNow = (curSpd >= resumeSpeedThreshold) && hdopAcceptable;

      if (!movingNow) {
        autoResumeMovementStartMs = 0;
      } else {
        if (autoResumeMovementStartMs == 0) {
          autoResumeMovementStartMs = millis();
        } else {
          uint32_t moveDuration = millis() - autoResumeMovementStartMs;

          // Check physical displacement from the pause location (eliminates stationary chair drift)
          float dispMeters = 0.0f;
          if (autoPauseAnchorLat != 0.0f && autoPauseAnchorLon != 0.0f && ride.gpsFixed && gps.location.isValid()) {
            dispMeters = haversineKm(autoPauseAnchorLat, autoPauseAnchorLon, (float)gps.location.lat(), (float)gps.location.lng()) * 1000.0f;
          }
          bool displacementOk = (dispMeters >= 3.5f) || (autoPauseAnchorLat == 0.0f);

          // Fast resume if moving fast (>= highSpeedThreshold for 800ms), or standard resume (>= 1.2s sustained with > 3.5m displacement)
          bool highSpeedConfirm = (curSpd >= highSpeedThreshold && moveDuration >= 800);
          bool normalConfirm = (moveDuration >= 1000 && displacementOk);
          bool walkingDisplaceConfirm = (dispMeters >= 2.5f);
          if (walkingDisplaceConfirm) normalConfirm = true;

          if (highSpeedConfirm || normalConfirm) {
            autoPaused = false;
            autoResumeMovementStartMs = 0;
            nearZeroSpeedStartMs = 0;
            if (autoPauseStartMs > 0) {
              rideStartMs += (millis() - autoPauseStartMs);
              autoPauseStartMs = 0;
            }
            distAnchorLat = distAnchorLon = 0.0f;
            autoPauseAnchorLat = autoPauseAnchorLon = 0.0f;
            fitEvent(0); // Resume FIT event
            uiRevision++;
          }
        }
      }
    }
  } else if (!autoPauseEnabled && autoPaused) {
    autoPaused = false;
    if (autoPauseStartMs > 0) {
      rideStartMs += (millis() - autoPauseStartMs);
      autoPauseStartMs = 0;
    }
    nearZeroSpeedStartMs = 0;
    autoResumeMovementStartMs = 0;
    distAnchorLat = distAnchorLon = 0.0f;
    autoPauseAnchorLat = autoPauseAnchorLon = 0.0f;
    uiRevision++;
  }
  static float filteredGpsAlt = 0.0f;
  static float baroSeaLevelPressure = 101325.0f; // Pa, auto-calibrated by GPS

  // Barometer reading + GPS pressure calibration
  if(bmpOk && millis()-sensorMs>500){
    sensorMs=millis();
    if(bmp.performReading()&&isfinite(bmp.pressure)&&bmp.pressure>=30000&&bmp.pressure<=125000){
      // When GPS fix is solid, continuously calibrate sea-level reference pressure:
      if (ride.gpsFixed && gps.altitude.isValid() && gps.altitude.age() < 2500 && startSatellites() >= 6) {
        float gpsAlt = (float)gps.altitude.meters();
        if (gpsAlt > -200.0f && gpsAlt < 9000.0f) {
          float pTarget = bmp.pressure / powf(1.0f - (gpsAlt / 44330.0f), 5.255f);
          if (pTarget >= 90000.0f && pTarget <= 110000.0f) {
            baroSeaLevelPressure = (0.998f * baroSeaLevelPressure) + (0.002f * pTarget);
          }
        }
      }
      float baroAlt = 44330.0f * (1.0f - powf(bmp.pressure / baroSeaLevelPressure, 0.1903f));
      ride.altM = baroAlt;
    } else {
      bmpOk=false;
      uiRevision++;
    }
  }

  // Periodic sensor probe (every 5 seconds)
  static uint32_t retryMs=0;
  if(millis()-retryMs>5000){
    retryMs=millis();
    if(!bmpOk)bmpOk=beginBarometer();
    bool present=externalPresent(0x20);
    if(!present)buttonsOk=false;
    else if(!buttonsOk){buttonsOk=externalPresent(0x20) && buttons.begin_I2C(0x20,&Wire);if(buttonsOk)for(int i=0;i<4;i++)buttons.pinMode(i,INPUT_PULLUP);}
    if(compass.ready && compass.lastSample && millis()-compass.lastSample > 3000) compass.ready=false;
    else if(!compass.ready && externalPresent(0x0D)) compass.begin();
    if(!touchReady)initQuickTouch();
  }

  // Filtered GPS altitude fallback when barometer is offline or in running mode
  if(!bmpOk || runningMode) {
    if (ride.gpsFixed && gps.altitude.isValid() && gps.altitude.age() < 3000) {
      float rawAlt = (float)gps.altitude.meters();
      if (filteredGpsAlt == 0.0f || !isfinite(filteredGpsAlt)) {
        filteredGpsAlt = rawAlt;
      } else {
        // Low-pass exponential moving average to reject GPS vertical noise jitter
        filteredGpsAlt = (0.80f * filteredGpsAlt) + (0.20f * rawAlt);
      }
      ride.altM = filteredGpsAlt;
    }
  }

  // 2.5m climb hysteresis filter on ride.altM to eliminate noise accumulation
  if (rideStarted && !ridePaused && !autoPaused && isfinite(ride.altM) && ride.altM > -100.0f) {
    static float s_elevMin = 0.0f, s_elevMax = 0.0f;
    static bool s_elevInit = false;
    if (!s_elevInit) {
      s_elevMin = s_elevMax = ride.altM;
      s_elevInit = true;
    } else {
      if (ride.altM < s_elevMin) {
        s_elevMin = ride.altM;
        s_elevMax = ride.altM;
      } else if (ride.altM > s_elevMax) {
        if (ride.altM - s_elevMin >= 2.5f) {
          ride.elevationGainM += (ride.altM - s_elevMin);
          s_elevMin = ride.altM;
          s_elevMax = ride.altM;
        } else {
          s_elevMax = ride.altM;
        }
      } else if (s_elevMax - ride.altM >= 2.0f) {
        s_elevMin = ride.altM;
        s_elevMax = ride.altM;
      }
    }
  }

  // Zero out slope jitter when stationary or moving under 2.0 km/h
  if (ride.speedKmh < 2.0f) {
    ride.gradePercent = 0.0f;
  }
  if(!ride.gpsFixed) {ride.lat=0;ride.lon=0;ride.gradePercent=0;nav.remainDistKm=0;lastLat=0;lastLon=0;distAnchorLat=0;distAnchorLon=0;}

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
    if(rideStarted && !ridePaused && !autoPaused){
      updateMaxSpeed(ride.speedKmh);
      if(samplesFile){samplesFile.print(ride.elapsedSec);samplesFile.print(',');if(bleHRReady)samplesFile.print(bleHR);samplesFile.print(',');if(speedAvailable())samplesFile.print(ride.speedKmh,2);samplesFile.println();}
      if(bleHRReady&&cfg.massConfirmed&&cfg.ageYears>=18&&cfg.ageYears<=80&&cfg.calorieSex&&dt<=2000){
        float kj=cfg.calorieSex==1 ? -55.0969f+0.6309f*bleHR+0.1988f*cfg.riderMassKg+0.2017f*cfg.ageYears : -20.4022f+0.4472f*bleHR-0.1263f*cfg.riderMassKg+0.074f*cfg.ageYears;
        calorieEstimate+=max(0.0f,kj/4.184f)*dt/60000.0f;calorieSeconds+=dt/1000;
      }
    }
    if(rideStarted && !ridePaused && !autoPaused && bleHRReady){hrSum+=bleHR;hrSamples++;hrMax=max(hrMax,(int)bleHR);}
    if(rideStarted && !runningMode && !ridePaused && !autoPaused && blePowerReady){powerSum+=blePower;powerSamples++;}
  }

  // Ride timer
  if (rideStarted && !ridePaused && !autoPaused) {
    ride.elapsedSec = (millis() - rideStartMs) / 1000;
    if (ride.elapsedSec > 0)
      ride.avgSpeedKmh = (distAccum / (float)ride.elapsedSec) * 3600.0f;
  }
  if (ride.powerW   > ride.maxPowerW)   ride.maxPowerW   = ride.powerW;

  // Intelligent Heading Fusion (Filtered Compass -> Locked GPS Course Over Ground)
  updateFusedHeading();

  // ── Buttons ───────────────────────────────────────────────────────────────

  updateBtn(rmtUp);  updateBtn(rmtDn);  updateBtn(rmtLt);  updateBtn(rmtRt);

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
  }

  // SEL (body) → switch screen

  // Remote LEFT → prev screen, Remote RIGHT → next screen
  if (buttonsOk) {
    if (rmtLt.pressed) { rmtLt.pressed = false; currentScreen = (Screen)(((int)currentScreen - 1 + SCR_COUNT) % SCR_COUNT); }
    if (rmtRt.pressed) { rmtRt.pressed = false; currentScreen = (Screen)(((int)currentScreen + 1) % SCR_COUNT); }
  } else {
    rmtLt.pressed = rmtRt.pressed = false;
  }

  // Remote UP/DN → zoom (map) or future use on dash
  if (rmtUp.pressed) {
    rmtUp.pressed = false;
    if (currentScreen == SCR_MAP) mapZoom = min((int)mapZoom+1, 5);
    else if (quickOpen && isLibrary() && librarySelection.length()) {
      archiveScroll = max(0, archiveScroll - 40);
      uiRevision++;
    }
  }
  if (rmtDn.pressed) {
    rmtDn.pressed = false;
    if (currentScreen == SCR_MAP) mapZoom = max((int)mapZoom-1, 1);
    else if (quickOpen && isLibrary() && librarySelection.length()) {
      int maxS = (homePage == HOME_ROUTES) ? 120 : ARCHIVE_MAX_SCROLL;
      archiveScroll = min(maxS, archiveScroll + 40);
      uiRevision++;
    }
  }

  // Body UP/DN → zoom on map too (mirrored for one-handed use)

  updateRideCountdown();

  // Web server
  server.handleClient();
  serviceAppRefresh();
  pollQuickTouch();

  // Target 60 FPS display refresh (16ms period, zero-allocation scalar comparison)
  if (millis() - lastDispMs >= 16) {
    lastDispMs = millis();
    struct UIState {
      uint32_t radarTick;
      uint32_t blinkTick;
      uint32_t uiRev;
      uint32_t elapsedMin;
      float speedKmh;
      float distanceKm;
      float lat, lon, altM;
      int cadence, power, heading;
      uint8_t screen, zoom, theme, ble, wifiMode, wifiStatus, gpsFixed, gpsReady, sats;
      int routeL, trackL;
      float destLat, destLon;
      uint8_t aPaused;
      int8_t ctrlSlider;
    };
    int cHeading = (compass.ready && isfinite(compass.heading)) ? (int)roundf(compass.heading) : ride.heading;
    UIState curState = {
      (uint32_t)((gpsWaiting || gpsRadarInspector || countdownStart || countdownGoStart || ((welcomeOpen || quickOpen) && homePage == HOME_COMPASS) || autoPaused) ? (millis() / 16) : 0),
      (uint32_t)(millis() / 500),
      uiRevision,
      ride.elapsedSec / 60,
      ride.speedKmh,
      ride.distanceKm,
      ride.lat, ride.lon, ride.altM,
      ride.cadenceRpm, ride.powerW, cHeading,
      (uint8_t)(welcomeOpen ? (int)homePage : (int)currentScreen), (uint8_t)mapZoom, (uint8_t)darkTheme, (uint8_t)bleCount(),
      (uint8_t)WiFi.getMode(), (uint8_t)WiFi.status(), (uint8_t)ride.gpsFixed, (uint8_t)gpsDataReady, (uint8_t)startSatellites(),
      routeLen, trackLen,
      nav.destLat, nav.destLon,
      (uint8_t)autoPaused,
      (int8_t)controlSliderOffset
    };
    static UIState prevState = {};
    if (memcmp(&curState, &prevState, sizeof(UIState)) == 0) return;
    prevState = curState;
    if(drawLifecycleScreen()){drawGlobalNav();drawAppRefresh();display.refresh();return;}
    if(quickOpen){drawQuickSettings();drawGlobalNav();drawAppRefresh();display.refresh();return;}
    if(autoPaused && rideStarted){
      drawAutoPauseScreen();
      drawGlobalNav();
      drawAppRefresh();
      display.refresh();
      return;
    }
    switch (currentScreen) {
      case SCR_DASH:     drawDashScreen();             break;
      case SCR_METRICS:  drawMetricsScreen();          break;
      case SCR_MAP:      drawMapScreen();              break;
      case SCR_CONTROLS: drawActivityControlsScreen(); break;
      default: break;
    }
    drawGlobalNav();
    drawAppRefresh();
    display.refresh();
  }
}