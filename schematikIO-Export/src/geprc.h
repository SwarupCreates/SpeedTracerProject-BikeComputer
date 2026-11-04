#pragma once
// GEPRC M1025Q: UBX NAV-PVT plus NMEA fallback; QST QMC5883L registers.
// References: GEPRC M1025 product page; u-blox M10 SPG 5.21 interface;
// QST QMC5883L Rev B sections 7 and 9.
class GeprcGps {
  uint8_t state=0, cls=0,id=0,a=0,b=0,ca=0,p[92]{};
  uint16_t len=0,pos=0;
  uint32_t byteMs=0;
  int32_t i32(int n){uint32_t v=uint32_t(p[n])|uint32_t(p[n+1])<<8|uint32_t(p[n+2])<<16|uint32_t(p[n+3])<<24;return (int32_t)v;}
  void sentence(TinyGPSPlus &gps,const char* s){uint8_t ck=0;gps.encode('$');for(const char*c=s;*c;c++){ck^=*c;gps.encode(*c);}char tail[8];snprintf(tail,sizeof(tail),"*%02X\r\n",ck);for(char*c=tail;*c;c++)gps.encode(*c);}
  void publish(TinyGPSPlus &gps){
    if(len!=92 || cls!=1 || id!=7 || !(p[21]&1) || p[20]<2 || p[20]>4)return;
    double lat=i32(28)*1e-7,lon=i32(24)*1e-7;
    double al=fabs(lat),ao=fabs(lon);
    double la=int(al)*100+(al-int(al))*60,lo=int(ao)*100+(ao-int(ao))*60;
    char s[180],tm[16]="",dt[16]="";
    if((p[11]&3)==3){snprintf(tm,sizeof(tm),"%02u%02u%02u",p[8],p[9],p[10]);snprintf(dt,sizeof(dt),"%02u%02u%02u",p[7],p[6],(p[4]+256*p[5])%100);}
    snprintf(s,sizeof(s),"GPRMC,%s,A,%010.5f,%c,%011.5f,%c,%.3f,%.3f,%s,,,A",tm,la,lat<0?'S':'N',lo,lon<0?'W':'E',fmax(0.0,i32(60)/514.444),i32(64)*1e-5,dt);sentence(gps,s);
    // NAV-PVT provides pDOP, not hDOP: use a separate pDOP gate below.
    snprintf(s,sizeof(s),"GPGGA,%s,%010.5f,%c,%011.5f,%c,1,%02u,,%.3f,M,0,M,,",tm,la,lat<0?'S':'N',lo,lon<0?'W':'E',p[23],i32(36)*0.001);sentence(gps,s);
    pdop=(p[76]+256*p[77])*0.01f;lastPvt=millis();
  }
public:
  float pdop=99;uint32_t lastPvt=0,lastMessage=0,lastStatus=0;uint8_t satellitesUsed=0;
  void feed(uint8_t c,TinyGPSPlus &gps){
    if(state && millis()-byteMs>200)state=0;byteMs=millis();
    if(state==0){if(c==0xB5)state=1;else gps.encode(c);return;}
    if(state==1){state=c==0x62?2:0;a=b=0;return;}
    if(state==2){cls=c;state=3;}else if(state==3){id=c;state=4;}
    else if(state==4){len=c;state=5;}else if(state==5){len|=uint16_t(c)<<8;pos=0;if(len>2048){state=0;return;}state=len?6:7;}
    else if(state==6){if(pos<sizeof(p))p[pos]=c;if(++pos==len)state=7;}
    else if(state==7){ca=c;state=8;return;}
    else if(state==8){if(ca==a && c==b){lastMessage=millis();if(cls==1 && id==7 && len==92){satellitesUsed=p[23];lastStatus=millis();}publish(gps);}state=0;return;}
    a+=c;b+=a;
  }
  void poll(HardwareSerial &port){static uint32_t ms=0;if(millis()-ms<1000 || millis()-lastPvt<1000)return;ms=millis();const uint8_t request[]={0xB5,0x62,1,7,0,0,8,0x19};port.write(request,sizeof(request));}
};

class GeprcCompass {
  bool write(uint8_t r,uint8_t v){Wire.beginTransmission(0x0D);Wire.write(r);Wire.write(v);return Wire.endTransmission()==0;}
  bool read(uint8_t r,uint8_t *p,uint8_t n){Wire.beginTransmission(0x0D);Wire.write(r);if(Wire.endTransmission(false))return false;if(Wire.requestFrom(uint8_t(0x0D),n)!=n)return false;for(int i=0;i<n;i++)p[i]=Wire.read();return true;}
  float offset[2]={0,0},scale[2]={1,1};int16_t mn[2],mx[2];uint32_t sampleMs=0;
public:
  bool ready=false,calibrated=false,calibrating=false;uint32_t lastSample=0;float heading=0,yawOffset=0,declination=0;int16_t raw[3]{};
  void begin(){Preferences p;p.begin("qmc",false);calibrated=p.getBool("valid",false);for(int i=0;i<2;i++){String k="o"+String(i);offset[i]=p.getFloat(k.c_str(),0);k="s"+String(i);scale[i]=p.getFloat(k.c_str(),1);}yawOffset=p.getFloat("yaw",0);declination=p.getFloat("decl",0);p.end();ready=write(0x0B,1)&&write(0x09,0x11);}
  void start(){calibrating=true;calibrated=false;for(int i=0;i<2;i++){mn[i]=32767;mx[i]=-32768;}}
  bool finish(){for(int i=0;i<2;i++)if(int(mx[i])-mn[i]<200)return false;float radius[2];for(int i=0;i<2;i++){offset[i]=(float(mx[i])+mn[i])/2;radius[i]=(float(mx[i])-mn[i])/2;}for(int i=0;i<2;i++)scale[i]=(radius[0]+radius[1])/(2*radius[i]);calibrating=false;calibrated=true;save();return true;}
  void save(){Preferences p;p.begin("qmc",false);p.putBool("valid",calibrated);for(int i=0;i<2;i++){String k="o"+String(i);p.putFloat(k.c_str(),offset[i]);k="s"+String(i);p.putFloat(k.c_str(),scale[i]);}p.putFloat("yaw",yawOffset);p.putFloat("decl",declination);p.end();}
  void update(){if(!ready || millis()-sampleMs<100)return;sampleMs=millis();uint8_t st,v[6];if(!read(6,&st,1)||!(st&1))return;if(!read(0,v,6)||(st&2))return;for(int i=0;i<3;i++)raw[i]=int16_t(v[i*2]|uint16_t(v[i*2+1])<<8);lastSample=millis();if(calibrating)for(int i=0;i<2;i++){mn[i]=min(mn[i],raw[i]);mx[i]=max(mx[i],raw[i]);}
    // GEPRC specifies CW0 flip: x remains forward, y and z are inverted.
    // Assumes module arrow points toward the bicycle front, with antenna upward.
    float x=(raw[0]-offset[0])*scale[0],y=-(raw[1]-offset[1])*scale[1];
    heading=fmodf(atan2f(y,x)*180/PI+yawOffset+declination+720,360);
  }
  bool usable(){return ready&&calibrated&&!calibrating&&millis()-lastSample<1000;}
};
