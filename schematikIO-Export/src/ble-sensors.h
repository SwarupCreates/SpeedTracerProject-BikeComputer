#pragma once
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <time.h>
#include <sys/time.h>

// Standard Bluetooth SIG Heart Rate, CSC and Cycling Power measurement profiles.
// Plus Apple Current Time Service (CTS, 0x1805) for automatic clock synchronization.
// All radio operations run off the UI task. Shared state is copied under a mutex.
struct BlePeer { char address[24]={0},name[32]={0}; uint8_t type=0; int profiles=0; };
struct BleSlot {
 BlePeer peer; NimBLEClient* client=nullptr; bool connected=false;
 char status[48]="Hardware not ready";
 uint32_t hrMs=0,cadMs=0,speedMs=0,powerMs=0;
 int hr=0,cad=0,power=0; float speed=0;
 uint32_t wheel=0; uint16_t wheelTime=0,crank=0,crankTime=0;
 bool wheelBase=false,crankBase=false;
};

static SemaphoreHandle_t bleMutex;
static BleSlot bleSlots[3];
static BlePeer bleFound[16]; static int bleFoundCount=0;
static bool bleReady=false,bleBusy=false;
static int bleCommand=0,bleArg=0; // 1 scan, 2 connect discovered index, 3 forget slot
static float bleCircum=2.096f;
static bool bleFresh(uint32_t ms){return ms && millis()-ms<10000;}

// Phone connection & Current Time Service (CTS) state
static bool phoneConnected=false;
static bool ctsSynced=false;
static uint32_t ctsLastSyncMs=0;
static bool ctsSyncPending=false;
static int ctsSyncAttempts=0;
static uint16_t phoneConnHandle=0xFFFF;
static NimBLEClient* phoneClient=nullptr;
static NimBLEServer* bleServer=nullptr;

static bool blePhoneConnected(){return phoneConnected;}
static bool bleCtsSynced(){return ctsSynced;}

static int bleCount(){int n=0;if(!bleMutex)return 0;xSemaphoreTake(bleMutex,portMAX_DELAY);for(auto &s:bleSlots)n+=s.connected;xSemaphoreGive(bleMutex);return n;}
static bool bleRequest(int cmd,int arg=0){if(!bleReady)return false;xSemaphoreTake(bleMutex,portMAX_DELAY);bool ok=!bleBusy&&!bleCommand;if(ok){bleCommand=cmd;bleArg=arg;}xSemaphoreGive(bleMutex);return ok;}
static uint16_t bu16(const uint8_t*p){return p[0]|(uint16_t(p[1])<<8);}
static uint32_t bu32(const uint8_t*p){return bu16(p)|(uint32_t(bu16(p+2))<<16);}

static bool parseCtsTime(const uint8_t* p, size_t len, time_t& outEpoch) {
  if (len < 7) return false;
  uint16_t year = p[0] | ((uint16_t)p[1] << 8);
  uint8_t month = p[2];
  uint8_t day = p[3];
  uint8_t hour = p[4];
  uint8_t min = p[5];
  uint8_t sec = p[6];

  if (year < 2024 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 59) {
    return false;
  }

  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = min;
  t.tm_sec = sec;
  t.tm_isdst = -1;

  time_t epoch = mktime(&t);
  if (epoch >= 1704067200LL) {
    outEpoch = epoch;
    return true;
  }
  return false;
}

static void ctsNotifyCallback(NimBLERemoteCharacteristic*, uint8_t* p, size_t len, bool) {
  time_t epoch = 0;
  if (parseCtsTime(p, len, epoch)) {
    timeval tv = {}; tv.tv_sec = epoch; settimeofday(&tv, nullptr);
    ctsSynced = true; ctsLastSyncMs = millis();
    uiRevision++;
    Serial.printf("CTS: Clock updated via notification: epoch %lld\n", (long long)epoch);
  }
}

class CtsCharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo&) override {
    std::string val = pChar->getValue();
    time_t epoch = 0;
    if (parseCtsTime((const uint8_t*)val.data(), val.length(), epoch)) {
      timeval tv = {}; tv.tv_sec = epoch; settimeofday(&tv, nullptr);
      ctsSynced = true; ctsLastSyncMs = millis();
      uiRevision++;
      Serial.printf("CTS: Clock set by client write: epoch %lld\n", (long long)epoch);
    }
  }
};
static CtsCharCallbacks ctsCharCallbacks;

class BleServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    Serial.printf("BLE Server: Phone connected: %s\n", connInfo.getAddress().toString().c_str());
    xSemaphoreTake(bleMutex, portMAX_DELAY);
    phoneConnected = true;
    phoneConnHandle = connInfo.getConnHandle();
    phoneClient = pServer->getClient(phoneConnHandle);
    ctsSyncPending = true;
    ctsSyncAttempts = 0;
    xSemaphoreGive(bleMutex);
    uiRevision++;
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo& connInfo, int reason) override {
    Serial.printf("BLE Server: Phone disconnected: %s (reason %d)\n", connInfo.getAddress().toString().c_str(), reason);
    xSemaphoreTake(bleMutex, portMAX_DELAY);
    phoneConnected = false;
    phoneClient = nullptr;
    ctsSyncPending = false;
    xSemaphoreGive(bleMutex);
    uiRevision++;
  }
  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    Serial.printf("BLE Server: Auth complete: %s\n", connInfo.getAddress().toString().c_str());
    xSemaphoreTake(bleMutex, portMAX_DELAY);
    phoneConnHandle = connInfo.getConnHandle();
    phoneClient = bleServer ? bleServer->getClient(phoneConnHandle) : nullptr;
    ctsSyncPending = true;
    ctsSyncAttempts = 0;
    xSemaphoreGive(bleMutex);
  }
};
static BleServerCallbacks bleServerCallbacks;

static void bleRotation(BleSlot&s,const uint8_t*p,bool wheel,int ticks){
 uint32_t rev=wheel?bu32(p):bu16(p);uint16_t t=bu16(p+(wheel?4:2));
 bool &base=wheel?s.wheelBase:s.crankBase;
 uint32_t delta=wheel?uint32_t(rev-s.wheel):uint16_t(rev-s.crank);
 uint16_t dt=t-(wheel?s.wheelTime:s.crankTime);
 if(base){
  if(wheel){float v=dt?delta*bleCircum*ticks*3.6f/dt:0;if(v<=150){s.speed=v;s.speedMs=millis();}}
  else {float rpm=dt?delta*60.0f*ticks/dt:0;if(rpm<=250){s.cad=lroundf(rpm);s.cadMs=millis();}}
 }
 if(wheel){s.wheel=rev;s.wheelTime=t;}else{s.crank=rev;s.crankTime=t;}base=true;
}

static void bleNotify(NimBLERemoteCharacteristic*c,uint8_t*p,size_t n,bool){
 xSemaphoreTake(bleMutex,portMAX_DELAY);
 for(auto&s:bleSlots)if(s.client==c->getClient()){
  if(c->getUUID()==NimBLEUUID((uint16_t)0x2A37)){
   if(n>=2 && (!(p[0]&1)||n>=3)){int hr=(p[0]&1)?bu16(p+1):p[1];if(hr>0&&hr<=250){s.hr=hr;s.hrMs=millis();}}
  }else if(c->getUUID()==NimBLEUUID((uint16_t)0x2A5B)){
   if(n<1)break;size_t need=1+((p[0]&1)?6:0)+((p[0]&2)?4:0);if(n<need)break;
   int off=1;if(p[0]&1){bleRotation(s,p+off,true,1024);off+=6;}if(p[0]&2)bleRotation(s,p+off,false,1024);
  }else if(c->getUUID()==NimBLEUUID((uint16_t)0x2A63)){
   if(n<4)break;uint16_t f=bu16(p);size_t off=4+((f&1)?1:0)+((f&4)?2:0);
   size_t need=off+((f&16)?6:0)+((f&32)?4:0);if(n<need)break;
   int power=(int16_t)bu16(p+2);if(power>=-2000&&power<=5000){s.power=power;s.powerMs=millis();}
   if(f&16){bleRotation(s,p+off,true,2048);off+=6;}if(f&32)bleRotation(s,p+off,false,1024);
  }break;
 }
 xSemaphoreGive(bleMutex);
}

static void bleSave(){Preferences p;p.begin("ble-sensors",false);for(int i=0;i<3;i++){String k=String(i);p.putString((k+"a").c_str(),bleSlots[i].peer.address);p.putUChar((k+"t").c_str(),bleSlots[i].peer.type);p.putString((k+"n").c_str(),bleSlots[i].peer.name);}p.end();}

static void bleConnect(int i){
 BleSlot&s=bleSlots[i];if(!s.peer.address[0])return;
 xSemaphoreTake(bleMutex,portMAX_DELAY);snprintf(s.status,sizeof(s.status),"Connecting...");s.wheelBase=s.crankBase=false;xSemaphoreGive(bleMutex);
 if(!s.client){s.client=NimBLEDevice::createClient();if(s.client)s.client->setConnectTimeout(4000);}
 bool ok=s.client&&s.client->connect(NimBLEAddress(std::string(s.peer.address),s.peer.type));int subscribed=0;
 if(ok){const uint16_t svc[]={0x180D,0x1816,0x1818}, chr[]={0x2A37,0x2A5B,0x2A63};
  for(int j=0;j<3;j++){auto service=s.client->getService(NimBLEUUID(svc[j]));if(!service)continue;auto c=service->getCharacteristic(NimBLEUUID(chr[j]));if(c&&c->canNotify()&&c->subscribe(true,bleNotify))subscribed++;}
 }
 if(ok&&!subscribed)s.client->disconnect();
 xSemaphoreTake(bleMutex,portMAX_DELAY);s.connected=ok&&subscribed;snprintf(s.status,sizeof(s.status),"%s",s.connected?"Connected; awaiting data":ok?"Unsupported / subscribe failed":"Hardware not ready; retrying");xSemaphoreGive(bleMutex);
}

static void bleWorker(void*){
 uint32_t retry=0;
 for(;;){int cmd,arg;xSemaphoreTake(bleMutex,portMAX_DELAY);cmd=bleCommand;arg=bleArg;bleCommand=0;bleBusy=cmd!=0;xSemaphoreGive(bleMutex);
  if(cmd==1){auto scan=NimBLEDevice::getScan();scan->setActiveScan(true);scan->setMaxResults(40);auto results=scan->getResults(5000);
   BlePeer found[16];int count=0;for(int i=0;i<results.getCount()&&count<16;i++){auto d=results.getDevice(i);int profiles=0;const uint16_t ids[]={0x180D,0x1816,0x1818};for(int j=0;j<3;j++)if(d->isAdvertisingService(NimBLEUUID(ids[j])))profiles|=1<<j;if(!profiles)continue;
    auto &p=found[count++];snprintf(p.address,sizeof(p.address),"%s",d->getAddress().toString().c_str());snprintf(p.name,sizeof(p.name),"%s",d->getName().empty()?"Cycling sensor":d->getName().c_str());p.type=d->getAddress().getType();p.profiles=profiles;
   }xSemaphoreTake(bleMutex,portMAX_DELAY);bleFoundCount=count;for(int i=0;i<count;i++)bleFound[i]=found[i];xSemaphoreGive(bleMutex);scan->clearResults();
  }else if(cmd==2){int slot=-1;xSemaphoreTake(bleMutex,portMAX_DELAY);if(arg>=0&&arg<bleFoundCount){for(int i=0;i<3;i++)if(!strcmp(bleSlots[i].peer.address,bleFound[arg].address)){slot=i;break;}if(slot<0)for(int i=0;i<3;i++)if(!bleSlots[i].peer.address[0]){slot=i;break;}if(slot>=0)bleSlots[slot].peer=bleFound[arg];}xSemaphoreGive(bleMutex);if(slot>=0){bleSave();if(!bleSlots[slot].connected)bleConnect(slot);}
  }else if(cmd==3&&arg>=0&&arg<3){auto c=bleSlots[arg].client;if(c)c->disconnect();xSemaphoreTake(bleMutex,portMAX_DELAY);bleSlots[arg]=BleSlot();bleSlots[arg].client=c;xSemaphoreGive(bleMutex);bleSave();}
  
  // CTS Time Sync Handshake with connected phone
  if(ctsSyncPending && phoneConnected && phoneClient){
    bool success=false;
    if(phoneClient->isConnected()){
      phoneClient->secureConnection();
      auto pCtsService=phoneClient->getService(NimBLEUUID((uint16_t)0x1805));
      if(pCtsService){
        auto pCtsChar=pCtsService->getCharacteristic(NimBLEUUID((uint16_t)0x2A2B));
        if(pCtsChar && pCtsChar->canRead()){
          std::string val=pCtsChar->readValue();
          time_t epoch=0;
          if(parseCtsTime((const uint8_t*)val.data(),val.length(),epoch)){
            timeval tv={};tv.tv_sec=epoch;settimeofday(&tv,nullptr);
            ctsSynced=true;ctsLastSyncMs=millis();ctsSyncPending=false;success=true;uiRevision++;
            Serial.printf("CTS: Successfully synced time from phone (epoch %lld)!\n",(long long)epoch);
            if(pCtsChar->canNotify())pCtsChar->subscribe(true,ctsNotifyCallback);
          }
        }
      }
    }
    if(!success){
      ctsSyncAttempts++;
      if(ctsSyncAttempts>=8){
        ctsSyncPending=false;
        Serial.println("CTS: Sync attempts exhausted; will retry on next reconnect or auth");
      }else{
        vTaskDelay(pdMS_TO_TICKS(1500));
      }
    }
  }

  // Clear stale data immediately on link loss, and reconnect saved sensors periodically.
  for(auto&s:bleSlots)if(s.connected&&(!s.client||!s.client->isConnected())){xSemaphoreTake(bleMutex,portMAX_DELAY);s.connected=false;s.hrMs=s.cadMs=s.speedMs=s.powerMs=0;snprintf(s.status,sizeof(s.status),"Disconnected; retrying");xSemaphoreGive(bleMutex);}
  if(millis()-retry>15000){xSemaphoreTake(bleMutex,portMAX_DELAY);bleBusy=true;xSemaphoreGive(bleMutex);retry=millis();for(int i=0;i<3;i++)if(bleSlots[i].peer.address[0]&&!bleSlots[i].connected)bleConnect(i);}
  xSemaphoreTake(bleMutex,portMAX_DELAY);bleBusy=false;xSemaphoreGive(bleMutex);vTaskDelay(pdMS_TO_TICKS(100));
 }
}

static void bleBegin(){
 bleMutex=xSemaphoreCreateMutex();if(!bleMutex)return;
 if(!NimBLEDevice::init("SpeedTracer")){Serial.println("Bluetooth unavailable: initialization failed");return;}

 // Enable Security & Bonding for Apple iOS CTS
 NimBLEDevice::setSecurityAuth(true, false, true); // bonding, mitm, sc
 NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT); // Just Works

 // Start Server for incoming phone connections
 bleServer=NimBLEDevice::createServer();
 if(bleServer){
   bleServer->setCallbacks(&bleServerCallbacks);
   bleServer->advertiseOnDisconnect(true);

   // Device Information Service (0x180A)
   auto pDis=bleServer->createService(NimBLEUUID((uint16_t)0x180A));
   if(pDis){
     auto pMfr=pDis->createCharacteristic(NimBLEUUID((uint16_t)0x2A29),NIMBLE_PROPERTY::READ);
     if(pMfr)pMfr->setValue("SpeedTracer");
     auto pModel=pDis->createCharacteristic(NimBLEUUID((uint16_t)0x2A24),NIMBLE_PROPERTY::READ);
     if(pModel)pModel->setValue("V2");
     pDis->start();
   }

   // Current Time Service (0x1805) - Local server for Android / Web writes
   auto pCts=bleServer->createService(NimBLEUUID((uint16_t)0x1805));
   if(pCts){
     auto pTimeChar=pCts->createCharacteristic(
       NimBLEUUID((uint16_t)0x2A2B),
       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
     );
     if(pTimeChar)pTimeChar->setCallbacks(&ctsCharCallbacks);
     pCts->start();
   }
 }

 // Configure Advertising for iPhone discovery
 NimBLEAdvertising* pAdv=NimBLEDevice::getAdvertising();
 if(pAdv){
   pAdv->setName("SpeedTracer");
   pAdv->addServiceUUID(NimBLEUUID((uint16_t)0x1805));
   pAdv->setAppearance(0x0440); // 0x0440 = Generic Cycling Computer
   pAdv->start();
   Serial.println("BLE: Advertising as 'SpeedTracer' with CTS");
 }

 Preferences p;
 if(p.begin("ble-sensors",false)) {
   for(int i=0;i<3;i++) {
     String k=String(i), addressKey=k+"a", nameKey=k+"n";
     // Missing saved sensors are normal on a new device, not NVS errors.
     if(p.isKey(addressKey.c_str()))
       snprintf(bleSlots[i].peer.address,24,"%s",p.getString(addressKey.c_str(),"").c_str());
     if(p.isKey(nameKey.c_str()))
       snprintf(bleSlots[i].peer.name,32,"%s",p.getString(nameKey.c_str(),"").c_str());
     bleSlots[i].peer.type=p.getUChar((k+"t").c_str(),0);
   }
   p.end();
 } else Serial.println("Bluetooth: saved-sensor storage unavailable");

 bleReady=xTaskCreate(bleWorker,"ble-sensors",8192,nullptr,1,nullptr)==pdPASS;
 if(!bleReady){NimBLEDevice::deinit(true);Serial.println("Bluetooth unavailable: worker allocation failed");}
 else Serial.println("Bluetooth ready: controller, host, CTS server and sensor worker initialized");
}

static int bleHR=0;static bool bleHRReady=false,bleCadReady=false,bleSpeedReady=false,blePowerReady=false;
static float bleSpeed=0;static int bleCad=0,blePower=0;
static void bleReadings(){bleHRReady=bleCadReady=bleSpeedReady=blePowerReady=false;if(!bleMutex)return;xSemaphoreTake(bleMutex,portMAX_DELAY);bleCircum=cfg.wheelCircumM;
 for(auto&s:bleSlots)if(s.connected){if(bleFresh(s.hrMs)){bleHR=s.hr;bleHRReady=true;}if(bleFresh(s.cadMs)){bleCad=s.cad;bleCadReady=true;}if(bleFresh(s.speedMs)){bleSpeed=s.speed;bleSpeedReady=true;}if(bleFresh(s.powerMs)){blePower=s.power;blePowerReady=true;}}
 xSemaphoreGive(bleMutex);
}

static void bleHttp(){
 server.on("/ble",HTTP_GET,[](){JsonDocument d;d["ready"]=bleReady;d["phoneConnected"]=phoneConnected;d["ctsSynced"]=ctsSynced;if(bleMutex){xSemaphoreTake(bleMutex,portMAX_DELAY);d["busy"]=bleBusy||bleCommand;for(int i=0;i<bleFoundCount;i++){auto o=d["found"].add<JsonObject>();o["index"]=i;o["name"]=bleFound[i].name;o["address"]=bleFound[i].address;o["profiles"]=bleFound[i].profiles;}for(int i=0;i<3;i++){auto&s=bleSlots[i];auto o=d["saved"].add<JsonObject>();o["slot"]=i;o["name"]=s.peer.name;o["address"]=s.peer.address;o["connected"]=s.connected;o["status"]=s.status;o["heartRateReady"]=s.connected&&bleFresh(s.hrMs);o["cadenceReady"]=s.connected&&bleFresh(s.cadMs);o["speedReady"]=s.connected&&bleFresh(s.speedMs);o["powerReady"]=s.connected&&bleFresh(s.powerMs);}xSemaphoreGive(bleMutex);}String text;serializeJson(d,text);server.send(200,"application/json",text);});
 server.on("/ble",HTTP_POST,[](){JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"text/plain","Invalid JSON");return;}String a=d["action"]|"";int cmd=a=="scan"?1:a=="connect"?2:a=="forget"?3:0;int index=d["index"]|-1;
 if(!cmd||(cmd==2&&(index<0||index>=bleFoundCount))||(cmd==3&&(index<0||index>=3))){server.send(400,"text/plain","Invalid action/index");return;}if(cmd==2){bool room=false;for(auto&s:bleSlots)if(!s.peer.address[0]||!strcmp(s.peer.address,bleFound[index].address))room=true;if(!room){server.send(409,"text/plain","Three saved sensors already; forget one first");return;}}
 bool ok=bleRequest(cmd,index);server.send(ok?202:409,"text/plain",ok?"Request queued":"Bluetooth busy or not ready; retry shortly");});
}
