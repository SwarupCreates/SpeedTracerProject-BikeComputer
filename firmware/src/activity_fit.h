
#pragma once
// Compact FIT protocol 1.0 writer. Little-endian definitions, standard global
// messages, FIT epoch (1989-12-31 UTC), invalid sentinels, header and file CRC.
// Reference: developer.garmin.com/fit/articles/fit-protocol/fit_protocol.html
static uint16_t fitCRC(uint16_t c,uint8_t b){
  c^=b;for(int i=0;i<8;i++)c=(c&1)?(c>>1)^0xA001:c>>1;return c;
}
static uint32_t fitStart=0, fitWallStart=0;
static uint32_t fitNow(){time_t t=time(nullptr);return t>=1704067200 ? uint32_t(t-631065600):0xFFFFFFFF;}
static void fitU(uint32_t v,int n){for(int i=0;i<n;i++)activityFile.write(uint8_t(v>>(i*8)));}
struct FitField{uint8_t number,size,type;};
static void fitDef(uint8_t local,uint16_t global,std::initializer_list<FitField> fields){
 fitU(0x40|local,1);fitU(0,1);fitU(0,1);fitU(global,2);fitU(fields.size(),1);
 for(auto f:fields){fitU(f.number,1);fitU(f.size,1);fitU(f.type,1);}
}
static void fitEvent(uint8_t type){if(!activityFile)return;fitU(4,1);fitU(fitNow(),4);fitU(0,1);fitU(type,1);}
static void fitBegin(){
 uint8_t header[14]={14,0x10,0xD0,0x07,0,0,0,0,'.','F','I','T',0,0};
 activityFile.write(header,14);fitStart=fitNow();fitWallStart=millis();
 fitDef(0,0,{{0,1,0},{1,2,0x84},{2,2,0x84},{4,4,0x86}});
 fitU(0,1);fitU(4,1);fitU(255,2);fitU(0,2);fitU(fitStart,4);
 fitDef(1,20,{{253,4,0x86},{0,4,0x85},{1,4,0x85},{2,2,0x84},{3,1,2},{4,1,2},{5,4,0x86},{6,2,0x84},{7,2,0x84}});
 fitDef(4,21,{{253,4,0x86},{0,1,0},{1,1,0}});fitEvent(0);
 activityFile.flush();
}
static void fitPoint(double lat,double lon){
 if(!activityFile || fitNow()==0xFFFFFFFF)return;
 fitU(1,1);fitU(fitNow(),4);
 fitU((uint32_t)(int32_t)llround(lat*2147483648.0/180),4);
 fitU((uint32_t)(int32_t)llround(lon*2147483648.0/180),4);
 bool alt=gps.altitude.isValid()&&gps.altitude.age()<3000;
 float elevation=runningMode?gps.altitude.meters():ride.altM;
 fitU((!runningMode && bmpOk)||alt ? (uint16_t)constrain(lround((elevation+500)*5),0L,65534L):0xFFFF,2);
 fitU(bleHRReady?bleHR:255,1);fitU(!runningMode && (bleCadReady || (sensors.cadencePhysical && millis()-externalSensorMs<5000))?ride.cadenceRpm:255,1);
 fitU(lround(ride.distanceKm*100000),4);
 bool speed=gps.speed.isValid()&&gps.speed.age()<3000;
 fitU(speed?lround((runningMode?gps.speed.kmph():bleSpeedReady?bleSpeed:ride.speedKmh)/3.6*1000):0xFFFF,2);
 fitU(!runningMode && blePowerReady?blePower:0xFFFF,2);
 activityFile.flush();
 if(activityFile.getWriteError()){activityMessage="SD WRITE ERROR";activityFile.close();}
 else activityPoints++;
}
static bool fitFinish(){
 fitEvent(9);uint32_t end=fitNow();
 // Lap and session carry elapsed wall time, active timer time and distance.
 for(int k=0;k<2;k++){
  if(k)fitDef(2,18,{{253,4,0x86},{2,4,0x86},{7,4,0x86},{8,4,0x86},{9,4,0x86},{5,1,0},{25,2,0x84},{26,2,0x84}});
  else fitDef(2,19,{{253,4,0x86},{2,4,0x86},{7,4,0x86},{8,4,0x86},{9,4,0x86}});
  fitU(2,1);fitU(end,4);fitU(fitStart,4);fitU(millis()-fitWallStart,4);
  fitU(ride.elapsedSec*1000,4);fitU(lround(ride.distanceKm*100000),4);
  if(k){fitU(runningMode?1:2,1);fitU(0,2);fitU(1,2);}
 }
 fitDef(3,34,{{253,4,0x86},{0,4,0x86},{1,2,0x84},{2,1,0},{3,1,0},{4,1,0}});
 fitU(3,1);fitU(end,4);fitU(ride.elapsedSec*1000,4);fitU(1,2);fitU(0,1);fitU(26,1);fitU(1,1);
 uint32_t size=activityFile.position()-14;activityFile.flush();
 uint8_t h[14]={14,0x10,0xD0,0x07,0,0,0,0,'.','F','I','T',0,0};
 for(int i=0;i<4;i++)h[4+i]=size>>(8*i);
 uint16_t c=0;for(int i=0;i<12;i++)c=fitCRC(c,h[i]);h[12]=c;h[13]=c>>8;
 activityFile.seek(0);activityFile.write(h,14);activityFile.flush();activityFile.close();
 File f=SD_MMC.open(activityPath,FILE_READ);if(!f)return false;
 c=0;while(f.available())c=fitCRC(c,f.read());f.close();
 activityFile=SD_MMC.open(activityPath,FILE_APPEND);if(!activityFile)return false;
 fitU(c,2);activityFile.flush();bool ok=!activityFile.getWriteError();activityFile.close();return ok;
}
