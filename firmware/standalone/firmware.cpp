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
#include <SD.h>
#include <math.h>
#include "geprc.h"

// Waveshare ESP32-S3-Touch-LCD-2.8 V2 with CST3530 touch.
#define GPS_RX_PIN 44
#define GPS_TX_PIN 43
#define COMPASS_SDA 11
#define COMPASS_SCL 10
#define SD_SCK_PIN 14
#define SD_MOSI_PIN 17
#define SD_MISO_PIN 16
#define SD_CS_PIN 21
#define BTN_UP 0
#define BTN_DN 1
#define BTN_SEL 2
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
  float bikeMassKg      = 9.0f;
  float maxSpeedKmh     = 60.0f;
  float maxPowerW       = 400.0f;
  float maxCadRpm       = 120.0f;
  float vo2maxManual    = 0.0f;
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
class CycleDisplay : public Adafruit_ST7789 {
public:
 CycleDisplay(): Adafruit_ST7789(&lcdSPI,42,41,39) {}
 void clearDisplay(){fillScreen(WHITE);}
 void refresh(){}
} display;
Adafruit_MCP23X17 buttons;
Adafruit_BMP3XX bmp;
bool buttonsOk=false, bmpOk=false;
uint32_t sensorMs=0, gpsByteMs=0;
bool gpsDataReady=false;

TinyGPSPlus       gps;
GeprcGps geprcGps;
GeprcCompass compass;
HardwareSerial    gpsSerial(1);


WebServer         server(80);
Preferences       prefs;
SPIClass          sdSPI(HSPI);

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

Btn btnUp{BTN_UP}, btnDn{BTN_DN}, btnSel{BTN_SEL};
Btn rmtUp{RMT_UP}, rmtDn{RMT_DN}, rmtLt{RMT_LT}, rmtRt{RMT_RT};



bool    sdOk          = false;
Screen  currentScreen = SCR_DASH;
uint8_t mapZoom       = 2;    // 1–5

// Ride accumulators
static uint32_t rideStartMs  = 0;
static bool     rideStarted  = false;
static float    lastLat      = 0, lastLon = 0, lastAltM = 0;
static float    distAccum    = 0;

// ─── Forward declarations ────────────────────────────────────────────────────
static float haversineKm(float la1, float lo1, float la2, float lo2);
static int   bearingTo(float la1, float lo1, float la2, float lo2);
static int   calcCadence(float speedKmh);
static int   calcVirtualPower(float speedKmh, float gradePercent);

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

static int calcCadence(float speedKmh) {
  if (speedKmh < 0.5f) return 0;
  return (int)((speedKmh / 3.6f / cfg.wheelCircumM) * 60.0f / cfg.gearRatio);
}

static int calcVirtualPower(float speedKmh, float gradePercent) {
  if (speedKmh < 0.5f) return 0;
  float v  = speedKmh / 3.6f;
  float m  = cfg.riderMassKg + cfg.bikeMassKg;
  float g  = 9.81f;
  float gr = gradePercent / 100.0f;
  return max(0, (int)(( m*g*gr + 0.004f*m*g + 0.5f*1.2f*0.35f*v*v ) * v));
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

static void drawDividerH(int y, int x0 = 0, int x1 = DISP_W - 1) {
  display.drawLine(x0, y, x1, y, BLACK);
}

// Cardinal direction from bearing
static const char* cardinalStr(int hdg) {
  static const char* c[] = {"N","NE","E","SE","S","SW","W","NW"};
  return c[((hdg + 22) % 360) / 45];
}

// ═══════════════════════════════════════════════════════════════════════════════
//  DASHBOARD SCREEN
// ═══════════════════════════════════════════════════════════════════════════════
// Figma-inspired layout scaled to 240x320. No synthetic sensor values.
static uint16_t uiBg(){return darkTheme?0x0000:0xFFFF;}
static uint16_t uiCard(){return darkTheme?0x18C3:0xEF7D;}
static uint16_t uiFg(){return darkTheme?0xFFFF:0x3186;}
static const uint16_t TEAL=0x4C30, ORANGE=0xFC20;
static void label(int x,int y,const String &v,int size=1,uint16_t color=0){
 display.setFont(nullptr);display.setTextSize(size);display.setTextColor(color?color:uiFg());display.setCursor(x,y);display.print(v);
}
static void card(int x,int y,int w,int h,uint16_t color){display.fillRoundRect(x,y,w,h,7,color);}
static String durationText(){char t[16];snprintf(t,sizeof(t),"%02lu:%02lu:%02lu",(unsigned long)ride.elapsedSec/3600,(unsigned long)(ride.elapsedSec/60)%60,(unsigned long)ride.elapsedSec%60);return t;}
static void bikeIcon(int x,int y,uint16_t color){
 display.drawCircle(x,y+4,3,color);display.drawCircle(x+12,y+4,3,color);
 display.drawLine(x,y+4,x+5,y-3,color);display.drawLine(x+5,y-3,x+12,y+4,color);display.drawLine(x,y+4,x+8,y+4,color);display.drawLine(x+8,y+4,x+5,y-3,color);
}
#include "ble_sensors.h"
static void statusBar(){
 display.fillScreen(uiBg());
 String clock="--:--";
 if(gps.time.isValid()&&gps.time.age()<3000){char t[8];snprintf(t,sizeof(t),"%02d:%02d",gps.time.hour(),gps.time.minute());clock=t;}
 label(7,9,clock);label(7,19,"UTC");
 bikeIcon(73,12,rideStarted&&((millis()/500)%2)?ST77XX_RED:uiFg());
 // Live GPS, BLE sensor connection and Wi-Fi status.
 uint16_t lock=ride.gpsFixed?0x5D47:0x8410;
 display.drawRect(98,10,5,5,lock);display.drawRect(92,5,5,5,lock);display.drawRect(104,16,5,5,lock);display.drawLine(96,9,105,18,lock);display.drawCircle(101,13,9,lock);
 display.drawLine(118,5,118,21,(bleCount()?TEAL:0x8410));display.drawLine(118,5,124,10,(bleCount()?TEAL:0x8410));display.drawLine(124,10,113,18,(bleCount()?TEAL:0x8410));display.drawLine(113,8,124,16,(bleCount()?TEAL:0x8410));display.drawLine(124,16,118,21,(bleCount()?TEAL:0x8410));
 bool wifi=WiFi.getMode()!=WIFI_OFF;
 for(int r=3;r<=8;r+=3)for(int d=220;d<=320;d+=8){float a=radians((float)d);display.drawPixel(139+r*cos(a),17+r*sin(a),wifi?TEAL:0x8410);}
 display.fillCircle(139,17,1,wifi?TEAL:0x8410);
 label(178,9,"--%");display.drawRect(204,8,22,10,uiFg());display.fillRect(226,11,2,4,uiFg());
}
#include "quick_settings.h"
static void heartCard(int y,int h){
 card(6,y,228,h,darkTheme?0x0940:0xD753);
 label(16,y+9,"HEART RATE",1,darkTheme?0xAEEA:0x3240);
 label(77,y+22,bleHRReady?String(bleHR):"--",3,darkTheme?0xAEEA:0x3240);label(126,y+37,"BPM");
 label(16,y+h-15,bleHRReady?"LIVE BLUETOOTH SENSOR":"Hardware not ready");
}
static void drawDashScreen(){
 statusBar();heartCard(29,68);card(6,103,228,169,uiCard());
 label(15,111,"AVG");label(15,123,String(ride.avgSpeedKmh,1));label(191,111,"MAX");label(191,123,String(ride.maxSpeedKmh,1));
 for(int d=135;d<=405;d++)for(int r=52;r<=57;r++){
 float a=radians((float)d);uint16_t c=(d-135)<270*constrain(ride.speedKmh/cfg.maxSpeedKmh,0.0f,1.0f)?TEAL:(darkTheme?0x4208:0xCE79);
 display.drawPixel(120+r*cos(a),185+r*sin(a),c);}
 bikeIcon(114,159,TEAL);String sp=ride.gpsFixed||bleSpeedReady||(sensors.speedPhysical&&millis()-externalSensorMs<5000)?String(ride.speedKmh,1):"--";
 label(120-sp.length()*9,180,sp,3);label(108,208,"KM/H");
 label(17,241,String(ride.distanceKm,2));label(87,241,durationText());label(194,241,String(ride.gradePercent,0)+"%");
 label(17,256,"DIST KM");label(88,256,"TOTAL TIME");label(186,256,"GRADE");
 card(6,278,228,36,uiCard());label(15,285,"CALORIES --");label(147,285,"SATELLITES");
 label(147,300,gps.satellites.isValid()?String(gps.satellites.value()):"--");label(15,300,"No health source");
}
static void metricCard(int x,int y,const String &value,const String &caption,uint16_t color){
 card(x,y,111,57,color);label(x+9,y+14,value,2);label(x+9,y+43,caption);
}
static void drawMetricsScreen(){
 statusBar();metricCard(6,29,"-- C","AMBIENT TEMP",uiCard());metricCard(123,29,bleHRReady?String(bleHR)+" BPM":"-- BPM","HEART RATE",darkTheme?0x0940:0xD753);
 card(6,92,228,61,darkTheme?0x2920:0xFFF1);label(67,103,String(ride.powerW)+" W",2);label(67,134,blePowerReady?"POWER SENSOR":"POWER ESTIMATE");
 metricCard(6,159,"--","KCAL",uiCard());metricCard(123,159,String(ride.speedKmh,1),"SPEED KM/H",uiCard());
 metricCard(6,222,String(ride.cadenceRpm),bleCadReady?"CADENCE SENSOR":"CADENCE EST*",uiCard());metricCard(123,222,nav.hasDestination?String(nav.remainDistKm,1)+"km":"--","DIRECT DIST",uiCard());
 label(8,287,durationText()+"  ALT "+String(ride.altM,0)+"m");label(8,302,"*External if fresh; else fixed gear");
}
static const float zoomRadiusKm[] = {0,4.0f,1.5f,0.5f,0.2f,0.08f};
static void drawMapScreen(){
 statusBar();card(6,29,228,215,uiCard());
 double lat=ride.gpsFixed?ride.lat:(routeLen?route[0].lat:0),lon=ride.gpsFixed?ride.lon:(routeLen?route[0].lon:0);
 float scale=90/zoomRadiusKm[mapZoom];
 auto px=[&](float x){return (int)(120+(x-lon)*111*cos(radians(lat))*scale);};
 auto py=[&](float y){return (int)(137-(y-lat)*111*scale);};
 // Skip offscreen segments instead of clamping them into false edge roads.
 auto inside=[](int x,int y){return x>=9&&x<=230&&y>=48&&y<=240;};
 for(int i=1;i<routeLen;i++){int x=px(route[i-1].lon),y=py(route[i-1].lat),u=px(route[i].lon),v=py(route[i].lat);if(inside(x,y)&&inside(u,v))for(int w=-1;w<=1;w++)display.drawLine(x+w,y,u+w,v,ORANGE);}
 for(int i=1;i<trackLen;i++){int x=px(track[i-1].lon),y=py(track[i-1].lat),u=px(track[i].lon),v=py(track[i].lat);if(inside(x,y)&&inside(u,v))display.drawLine(x,y,u,v,TEAL);}
 label(14,36,"ROUTE / TRAIL   N ^");
 if(!routeLen)label(19,213,"Upload route via web page");
 if(ride.gpsFixed){display.fillCircle(120,137,15,ST77XX_BLUE);float h=radians((float)ride.heading);display.fillTriangle(120+11*sin(h),137-11*cos(h),120-7*cos(h)-5*sin(h),137-7*sin(h)+5*cos(h),120+7*cos(h)-5*sin(h),137+7*sin(h)+5*cos(h),WHITE);}else label(66,132,"WAITING GPS");
 metricCard(6,250,String(ride.speedKmh,1),"SPEED KM/H",uiCard());
 metricCard(123,250,nav.hasDestination?String(nav.remainDistKm,1)+"km":"--","DIRECT DIST",uiCard());
 label(8,311,"No road tiles / turn cues yet");
}

static void drawSplash() {
 display.clearDisplay();display.setTextColor(BLACK);display.setTextSize(2);
 display.setCursor(12,50);display.print("CycloComp V2");
 display.setTextSize(1);display.setCursor(12,90);display.print("CycloComp / cycling1234");
 display.setCursor(12,110);display.print("http://192.168.4.1");
}

static const char page[] PROGMEM=R"HTML(<!doctype html><meta name="viewport" content="width=device-width"><title>CycloComp</title><style>body{font:16px system-ui;background:#101c28;color:white;max-width:600px;margin:30px auto;padding:16px}input,button,textarea{padding:12px;margin:5px;max-width:90%}pre{white-space:pre-wrap}</style><h1>CycloComp V2</h1><p>Prototype: coordinate guidance, not road routing. Estimates are not measured fitness data.</p><h2>Display</h2><button onclick="post('/ui',{quick:true})">Quick settings</button><button onclick="post('/ui',{quick:false})">Close settings</button><button onclick="post('/ui',{screen:0})">Dashboard</button><button onclick="post('/ui',{screen:2})">Metrics</button><button onclick="post('/ui',{screen:1})">Map</button><button onclick="post('/ui',{dark:false})">Light</button><button onclick="post('/ui',{dark:true})">Dark</button><h2>Bluetooth sensors</h2><p>Wake your sensors. Scan, then select your own sensor. Supports standard BLE heart rate, speed/cadence and cycling power; up to three saved sensors. ANT+ and proprietary profiles are not supported.</p><button onclick="bleAction('scan')">Scan for sensors</button><div id="bleList"></div><p id="bleMsg"></p><h2>Destination</h2><input id="coord" placeholder="latitude, longitude"><button onclick="nav()">Navigate</button><h2>Configuration JSON</h2><textarea id="conf">{"gearRatio":2.94,"wheelCircumM":2.096,"mass":75,"targetDistanceKm":50,"vo2maxManual":0}</textarea><button onclick="post('/config',JSON.parse(conf.value))">Save</button><h2>Route JSON</h2><input type="file" id="route"><button onclick="upload()">Load route</button><p>Array of {"lat":number,"lon":number}, max 512 points.</p><a href="/export">Download current GPX preview (max 1024 points)</a><p><a href="/log">Download full SD ride CSV</a></p><pre id="out"></pre><script>async function bleAction(action,index=-1){let r=await fetch('/ble',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action,index})});bleMsg.textContent=await r.text()}async function bleRefresh(){try{let d=await(await fetch('/ble')).json();bleList.replaceChildren();let title=document.createElement('p');title.textContent=d.ready?(d.busy?'Bluetooth busy...':'Bluetooth ready'):'Hardware not ready';bleList.append(title);for(let x of (d.found||[])){let b=document.createElement('button');b.textContent='Connect '+x.name+' '+x.address;b.onclick=()=>bleAction('connect',x.index);bleList.append(b)}for(let x of (d.saved||[])){if(!x.address)continue;let p=document.createElement('p');p.textContent=x.name+': '+(x.connected?'Connected':x.status);let b=document.createElement('button');b.textContent='Forget';b.onclick=()=>bleAction('forget',x.slot);p.append(b);bleList.append(p)}}catch(e){}}setInterval(bleRefresh,2500);bleRefresh();async function post(p,d){let r=await fetch(p,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});out.textContent=await r.text()}function nav(){let a=coord.value.split(',').map(Number);if(a.length===2&&a.every(Number.isFinite))post('/nav',{lat:a[0],lon:a[1]})}async function upload(){if(route.files[0])post('/upload',JSON.parse(await route.files[0].text()))}setInterval(async()=>{try{out.textContent=JSON.stringify(await(await fetch('/status')).json(),null,2)}catch(e){}},2000)</script>)HTML";
static void handleRoot(){server.send_P(200,"text/html",page);}

static void handleStatus() {
  StaticJsonDocument<512> doc;
  doc["speedKmh"]        = ride.speedKmh;
  doc["distKm"]          = ride.distanceKm;
  doc["cadence"]         = ride.cadenceRpm;
  doc["power"]           = ride.powerW;
  doc["vo2maxManual"] = cfg.vo2maxManual;
  doc["compassReady"]=compass.ready; doc["compassCalibrated"]=compass.calibrated; doc["compassCalibrating"]=compass.calibrating; doc["headingSource"]=compass.usable()?"magnetic-level-only":"GPS course"; doc["compassX"]=compass.raw[0]; doc["compassY"]=compass.raw[1]; doc["compassZ"]=compass.raw[2]; doc["touchReady"]=touchReady; doc["blePairingReady"]=bleReady; doc["bleConnected"]=bleCount(); doc["heartRate"]=bleHRReady?bleHR:0; doc["heartRateReady"]=bleHRReady; doc["powerMeasured"]=blePowerReady; doc["cadenceMeasured"]=bleCadReady; doc["speedMeasured"]=bleSpeedReady; doc["barometerReady"]=bmpOk; doc["gpsDataReady"]=gpsDataReady; doc["buttonsReady"]=buttonsOk; doc["imuReady"]=false;
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
    File f = SD.open("/route.json", "w");
    if (f) { serializeJson(doc, f); f.close(); }
  }
  server.send(200, "text/plain", "Route: " + String(routeLen) + " pts");
}

static void handleConfig() {
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "No body"); return; }
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "Bad JSON"); return; }
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
  Serial.begin(115200);

  // SYS_EN active high maintains battery power, before peripheral startup.
  pinMode(7, OUTPUT); digitalWrite(7,HIGH);
  pinMode(6, INPUT);
  pinMode(5, OUTPUT);digitalWrite(5,LOW);
  lcdSPI.begin(40,-1,45,42);
  display.init(240,320); display.setRotation(0);display.invertDisplay(true);
  digitalWrite(5,HIGH);drawSplash();
  initQuickTouch();
  Wire.begin(COMPASS_SDA,COMPASS_SCL);
  Wire.setTimeOut(30);
  buttonsOk=buttons.begin_I2C(0x20,&Wire);
  if(buttonsOk)for(int i=0;i<4;i++)buttons.pinMode(i,INPUT_PULLUP);
  bmpOk=bmp.begin_I2C(0x77,&Wire)||bmp.begin_I2C(0x76,&Wire);
  compass.begin();
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

  // SD card HSPI
  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  sdOk = SD.begin(SD_CS_PIN, sdSPI);
  if (sdOk) {
    SD.mkdir("/maps");
    if (SD.exists("/route.json")) {
      File f = SD.open("/route.json", "r");
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
  prefs.begin("cyclo", true);
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
  WiFi.softAP("CycloComp", "cycling1234");
  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/ui",HTTP_POST,[](){
    JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"text/plain","Invalid JSON");return;}
    if(d["screen"].is<int>()){int n=d["screen"];if(n<0||n>=SCR_COUNT){server.send(400,"text/plain","Invalid screen");return;}currentScreen=(Screen)n;}
    if(d["quick"].is<bool>()){quickOpen=d["quick"];quickDetail=0;}
    if(d["dark"].is<bool>())darkTheme=d["dark"];
    uiRevision++;server.send(200,"text/plain","Display updated");
  });
  server.on("/status", HTTP_GET,  handleStatus);
  server.on("/nav",    HTTP_POST, handleNavSet);
  server.on("/upload", HTTP_POST, handleUploadRoute);
  server.on("/config", HTTP_POST, handleConfig);
  server.on("/sensor", HTTP_POST, handleSensor);
  server.on("/export", HTTP_GET,  handleExportGPX);
  server.on("/log",HTTP_GET,[](){
    File f=SD.open("/ride.csv","r"); if(!f){server.send(404,"text/plain","No SD log");return;}
    server.sendHeader("Content-Disposition","attachment; filename=ride.csv");server.streamFile(f,"text/csv");f.close();
  });
  bleBegin();bleHttp();
  server.begin();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  loop()
// ═══════════════════════════════════════════════════════════════════════════════
static uint32_t lastDispMs  = 0;
static uint32_t lastTrackMs = 0;

void loop() {
  bleReadings();
  static uint32_t held=0;
  if(digitalRead(6)==LOW){if(!held)held=millis();if(millis()-held>3000){digitalWrite(5,LOW);digitalWrite(7,LOW);delay(100);}}else held=0;
  // GPS feed
  while (gpsSerial.available()){gpsByteMs=millis();geprcGps.feed(gpsSerial.read(),gps);}
  gpsDataReady=gpsByteMs&&millis()-gpsByteMs<3000;
  geprcGps.poll(gpsSerial);
  compass.update();

  if (gps.location.isValid() && gps.location.isUpdated()) {
    float nLat = gps.location.lat(), nLon = gps.location.lng();

    if (lastLat != 0.0f) {
      float delta = haversineKm(lastLat, lastLon, nLat, nLon);
      if (!ridePaused && delta > 0.001f && delta<0.1f && gps.speed.kmph()>2 && ((geprcGps.lastPvt && millis()-geprcGps.lastPvt<3000)?geprcGps.pdop<3:(gps.hdop.isValid() && gps.hdop.hdop()<3))) {   // ignore sub-metre jitter
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
    if (!ridePaused && millis() - lastTrackMs > 3000 && ride.speedKmh > 0.5f) {
      lastTrackMs = millis();
      if(sdOk){File f=SD.open("/ride.csv",FILE_APPEND);if(f){
        if(!f.size())f.println("utc,lat,lon,alt_m,speed_kmh,cadence_rpm,power_w,speed_source,cadence_source,power_source");
        char utc[30]="";
        if(gps.date.isValid()&&gps.time.isValid())snprintf(utc,sizeof(utc),"%04d-%02d-%02dT%02d:%02d:%02dZ",gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second());
        f.printf("%s,%.7f,%.7f,%.1f,%.2f,%d,%d,%s,%s,%s\n",utc,nLat,nLon,ride.altM,ride.speedKmh,ride.cadenceRpm,ride.powerW,bleSpeedReady?"BLE":"GPS_or_bridge",bleCadReady?"BLE":(sensors.cadencePhysical&&millis()-externalSensorMs<5000)?"bridge":"estimate",blePowerReady?"BLE":"estimate");f.close();}}

      if (trackLen < MAX_TRACK)
        track[trackLen++] = {nLat, nLon, ride.altM, ride.elapsedSec};
    }
  } else if (!gps.location.isValid()) {
    ride.gpsFixed = false;
  }

  ride.gpsFixed=gps.location.isValid()&&gps.location.age()<3000;
  ride.speedKmh=ride.gpsFixed && gps.speed.isValid()?gps.speed.kmph():0;
  if(bmpOk && millis()-sensorMs>500){sensorMs=millis();
    if(bmp.performReading()&&isfinite(bmp.pressure)&&bmp.pressure>=30000&&bmp.pressure<=125000)
      ride.altM=44330.0f*(1.0f-powf(bmp.pressure/101325.0f,0.1903f));
    else {bmpOk=false;uiRevision++;}
  }
  static uint32_t retryMs=0;
  if(millis()-retryMs>5000){retryMs=millis();
    if(!bmpOk)bmpOk=bmp.begin_I2C(0x77,&Wire)||bmp.begin_I2C(0x76,&Wire);
    Wire.beginTransmission(0x20);bool present=Wire.endTransmission()==0;
    if(!present)buttonsOk=false;
    else if(!buttonsOk){buttonsOk=buttons.begin_I2C(0x20,&Wire);if(buttonsOk)for(int i=0;i<4;i++)buttons.pinMode(i,INPUT_PULLUP);}
    Wire.beginTransmission(0x0D);if(Wire.endTransmission()!=0)compass.ready=false;
    else if(!compass.ready)compass.begin();
    if(!touchReady)initQuickTouch();
    uiRevision++;
  }
  if(!bmpOk && gps.altitude.isValid())ride.altM=gps.altitude.meters();

  // Speed source
  if (sensors.speedPhysical && millis()-externalSensorMs<5000)
    ride.speedKmh = sensors.physSpeedKmh;

  if(bleSpeedReady)ride.speedKmh=bleSpeed;

  // Cadence source
  ride.cadenceRpm = (sensors.cadencePhysical && millis()-externalSensorMs<5000)
                    ? sensors.physCadenceRpm
                    : calcCadence(ride.speedKmh);

  if(bleCadReady)ride.cadenceRpm=bleCad;

  // Power + VO2
  ride.powerW = blePowerReady?blePower:calcVirtualPower(ride.speedKmh, ride.gradePercent);
  ride.vo2est = cfg.vo2maxManual; // Never derive VO2max from instantaneous power.

  // Ride timer
  if (ride.speedKmh > 0.5f && !rideStarted) {
    rideStarted = true; rideStartMs = millis();
  }
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

  // Web server
  server.handleClient();
  pollQuickTouch();

  // Redraw at 5 fps
  if (millis() - lastDispMs > 200) {
    lastDispMs = millis();
    String state=String(uiRevision)+":"+String(darkTheme)+":"+String(rideStarted?millis()/500:millis()/1000)+":"+String(currentScreen)+":"+String(mapZoom)+":"+String(ride.speedKmh,1)+":"+String(ride.distanceKm,2)+":"+String(ride.elapsedSec/60)+":"+String(ride.lat,5)+":"+String(ride.lon,5)+":"+String(ride.altM,1)+":"+String(ride.cadenceRpm)+":"+String(ride.powerW)+":"+String(ride.gpsFixed)+":"+String(routeLen)+":"+String(trackLen)+":"+String(nav.destLat,5)+":"+String(nav.destLon,5)+":"+String(cfg.vo2maxManual)+":"+String(cfg.targetDistanceKm);
    static String previous; if(state==previous)return;previous=state;
    if(quickOpen){drawQuickSettings();return;}
    switch (currentScreen) {
      case SCR_DASH: drawDashScreen(); break;
      case SCR_MAP:  drawMapScreen();  break;
      case SCR_METRICS: drawMetricsScreen(); break;
      default: break;
    }
  }
}
