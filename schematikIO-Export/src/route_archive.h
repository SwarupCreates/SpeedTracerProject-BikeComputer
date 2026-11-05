#pragma once
struct ArchivePoint {double lat,lon;float ele;};
ArchivePoint preview[MAX_ROUTE];int previewLen=0;
float previewKm=0,previewGain=0;bool previewElevation=false;
String previewPath,completedText;JsonDocument archiveSummary;

// Graph cache for HR and Speed so we never re-read SD CSV on every screen redraw:
static float archiveHrValues[100];
static float archiveSpeedValues[100];
static float archiveHrTop = 0;
static float archiveSpeedTop = 0;
static bool archiveCsvLoaded = false;

static uint32_t parseIsoEpoch(const char* s){
  if(!s)return 0;
  while(*s==' '||*s=='\t')s++;
  if(strlen(s)<19)return 0;
  int y=(s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+(s[3]-'0');
  int m=(s[5]-'0')*10+(s[6]-'0');
  int d=(s[8]-'0')*10+(s[9]-'0');
  int h=(s[11]-'0')*10+(s[12]-'0');
  int mn=(s[14]-'0')*10+(s[15]-'0');
  int sc=(s[17]-'0')*10+(s[18]-'0');
  struct tm tm{};
  tm.tm_year=y-1900;
  tm.tm_mon=m-1;
  tm.tm_mday=d;
  tm.tm_hour=h;
  tm.tm_min=mn;
  tm.tm_sec=sc;
  return (uint32_t)mktime(&tm);
}

// Fast stream-buffered GPX parser (4096-byte blocks, zero heap thrashing, ~50-80ms total)
static bool readGPX(String path){
  previewLen=0;previewKm=0;previewGain=0;previewElevation=false;previewPath=path;
  uint32_t count=0;
  uint32_t t0Sec=0,tEndSec=0;
  uint32_t gpxHrSum=0,gpxHrCount=0;float gpxHrMax=0;
  double gpxSpeedSum=0;uint32_t gpxSpeedCount=0;float gpxSpeedMax=0;
  uint32_t totalDuration=0;
  const int CHUNK_SIZE=4096;
  static char buf[CHUNK_SIZE+256];

  for(int pass=0;pass<2;pass++){
    File f=SD_MMC.open(path,FILE_READ);if(!f)return false;
    uint32_t index=0;
    uint32_t stride=1;
    if(pass==1){stride=max(1UL,(unsigned long)((count+MAX_ROUTE-2)/(MAX_ROUTE-1)));}
    double lastA=0,lastB=0;float lastE=NAN;uint32_t lastTSec=0;
    int carry=0;

    while(true){
      int toRead=CHUNK_SIZE-carry;
      int n=f.read((uint8_t*)(buf+carry),toRead);
      if(n<=0 && carry==0)break;
      int totalInBuf=carry+(n>0?n:0);
      buf[totalInBuf]='\0';

      int pos=0;
      while(pos<totalInBuf){
        char *p1=strstr(buf+pos,"<trkpt");
        char *p2=strstr(buf+pos,"<rtept");
        char *pt=nullptr;
        if(p1&&p2)pt=(p1<p2?p1:p2);
        else if(p1)pt=p1;
        else if(p2)pt=p2;

        if(!pt)break;

        char *pEnd=strstr(pt,"</trkpt>");
        int endTagLen=8;
        if(!pEnd){
          pEnd=strstr(pt,"</rtept>");
          if(pEnd)endTagLen=8;
        }
        if(!pEnd){
          char *pClose=strstr(pt,"/>");
          char *nextP1=strstr(pt+6,"<trkpt");
          char *nextP2=strstr(pt+6,"<rtept");
          char *nextPt=nextP1?(nextP2&&nextP2<nextP1?nextP2:nextP1):nextP2;
          if(pClose && (!nextPt || pClose<nextPt)){
            pEnd=pClose;
            endTagLen=2;
          }
        }

        if(!pEnd){
          pos=pt-buf;
          break;
        }

        char saved=*(pEnd+endTagLen);
        *(pEnd+endTagLen)='\0';

        char *latP=strstr(pt,"lat=\"");
        if(!latP)latP=strstr(pt,"lat='");
        char *lonP=strstr(pt,"lon=\"");
        if(!lonP)lonP=strstr(pt,"lon='");

        if(latP&&lonP){
          double a=strtod(latP+5,nullptr);
          double b=strtod(lonP+5,nullptr);
          if(fabs(a)<=90 && fabs(b)<=180){
            float ele=NAN;
            char *eleP=strstr(pt,"<ele>");
            if(eleP) ele=strtof(eleP+5,nullptr);

            uint32_t tSec=0;
            char *timeP=strstr(pt,"<time>");
            if(timeP) tSec=parseIsoEpoch(timeP+6);

            int hr=0;
            char *hrP=strstr(pt,":hr>");
            if(!hrP) hrP=strstr(pt,"<hr>");
            if(hrP){char *v=strchr(hrP,'>');if(v)hr=atoi(v+1);}

            if(!pass){
              if(index){
                double d=haversineKm(lastA,lastB,a,b);
                previewKm+=d;
                if(isfinite(ele)){
                  previewElevation=true;
                  if(isfinite(lastE)&&ele>lastE) previewGain+=(ele-lastE);
                }
                if(lastTSec>0 && tSec>lastTSec){
                  uint32_t dt=tSec-lastTSec;
                  if(dt>0 && dt<=120){
                    float spd=(d/dt)*3600.0f;
                    if(spd>=0 && spd<120.0f){
                      gpxSpeedSum+=spd; gpxSpeedCount++;
                      if(spd>gpxSpeedMax) gpxSpeedMax=spd;
                    }
                  }
                }
              }
              if(t0Sec==0 && tSec>0) t0Sec=tSec;
              if(tSec>0) tEndSec=tSec;
              if(hr>30 && hr<240){
                gpxHrSum+=hr; gpxHrCount++;
                if((float)hr>gpxHrMax) gpxHrMax=(float)hr;
              }
              lastA=a;lastB=b;lastE=ele;lastTSec=tSec;
            }else{
              if((index%stride==0 || index+1==count) && previewLen<MAX_ROUTE){
                preview[previewLen++]={a,b,ele};
              }
              if(gpxHrCount>0 || gpxSpeedCount>0){
                int bucket=0;
                if(totalDuration>0 && tSec>=t0Sec){
                  bucket=constrain((int)((tSec-t0Sec)*99/totalDuration),0,99);
                } else {
                  bucket=constrain((int)(index*99/max(1UL,(unsigned long)(count-1))),0,99);
                }
                if(hr>30 && hr<240){
                  archiveHrValues[bucket]=(float)hr;
                }
                if(lastTSec>0 && tSec>lastTSec){
                  uint32_t dt=tSec-lastTSec;
                  if(dt>0 && dt<=120){
                    float spd=(haversineKm(lastA,lastB,a,b)/dt)*3600.0f;
                    if(spd>=0 && spd<120.0f){
                      archiveSpeedValues[bucket]=spd;
                    }
                  }
                }
              }
              lastA=a;lastB=b;lastTSec=tSec;
            }
            index++;
          }
        }

        *(pEnd+endTagLen)=saved;
        pos=(pEnd+endTagLen)-buf;
      }

      if(pos<totalInBuf && n>0){
        carry=totalInBuf-pos;
        if(carry>256)carry=256;
        memmove(buf,buf+totalInBuf-carry,carry);
      }else{
        carry=0;
      }

      if((index&127)==0)yield();
      if(n<=0)break;
    }

    f.close();
    if(!pass){
      count=index;
      if(count<2)return false;
      totalDuration=(tEndSec>=t0Sec && t0Sec>0)?(tEndSec-t0Sec):0;
      if(gpxHrCount>0 || gpxSpeedCount>0){
        archiveHrTop=gpxHrMax;
        archiveSpeedTop=gpxSpeedMax;
        for(int i=0;i<100;i++){archiveHrValues[i]=NAN;archiveSpeedValues[i]=NAN;}
      }
    }
  }

  if(gpxHrCount>0 || gpxSpeedCount>0){
    archiveCsvLoaded=true;
    if(!archiveSummary["seconds"].is<uint32_t>() || archiveSummary["seconds"].as<uint32_t>()==0){
      archiveSummary["seconds"]=totalDuration;
      archiveSummary["distanceKm"]=previewKm;
      archiveSummary["elevationGainM"]=previewGain;
      if(gpxHrCount>0){
        float avgHr=(float)gpxHrSum/gpxHrCount;
        archiveSummary["avgHeartRate"]=avgHr;
        archiveSummary["maxHeartRate"]=gpxHrMax;
        int cal=(int)(((-55.0969f+0.6309f*avgHr+0.1988f*75.0f+0.2017f*30.0f)/4.184f)*(totalDuration/60.0f));
        if(cal<50 && totalDuration>300) cal=(int)(previewKm*32.0f);
        archiveSummary["caloriesEstimate"]=cal;
      }
      if(gpxSpeedCount>0){
        float avgSpd=(totalDuration>0)?(previewKm/(totalDuration/3600.0f)):((float)gpxSpeedSum/gpxSpeedCount);
        archiveSummary["avgSpeedKmh"]=avgSpd;
        archiveSummary["maxSpeedKmh"]=gpxSpeedMax;
      }
      if(tEndSec>0){
        archiveSummary["completedEpoch"]=tEndSec;
        time_t tt=(time_t)tEndSec; struct tm local; localtime_r(&tt,&local);
        char b[40]; strftime(b,sizeof b,"%d %b %Y %I:%M %p",&local);
        completedText=b;
      }
    }
  }

  return previewLen>=2;
}

static void loadArchiveCsv(String csvPath){
  File f=SD_MMC.open(csvPath,FILE_READ);
  if(!f)return; // Preserve GPX-loaded HR/Speed data if CSV does not exist!

  archiveCsvLoaded=false;
  archiveHrTop=0;
  archiveSpeedTop=0;
  for(int i=0;i<100;i++){archiveHrValues[i]=NAN;archiveSpeedValues[i]=NAN;}

  uint32_t duration=archiveSummary["seconds"]|0;
  const int CHUNK_SIZE=2048;
  static char buf[CHUNK_SIZE+128];
  int carry=0;
  bool headerSkipped=false;

  while(true){
    int toRead=CHUNK_SIZE-carry;
    int n=f.read((uint8_t*)(buf+carry),toRead);
    if(n<=0 && carry==0)break;
    int totalInBuf=carry+(n>0?n:0);
    buf[totalInBuf]='\0';

    int pos=0;
    while(pos<totalInBuf){
      char *eol=strchr(buf+pos,'\n');
      if(!eol)break;

      *eol='\0';
      char *line=buf+pos;
      pos=(eol-buf)+1;

      if(!headerSkipped){
        headerSkipped=true;
        continue;
      }

      char *c1=strchr(line,',');
      if(!c1)continue;
      char *c2=strchr(c1+1,',');
      if(!c2)continue;

      long sec=strtol(line,nullptr,10);
      int bucket=constrain((int)(sec*99/max(1UL,(unsigned long)duration)),0,99);

      if(c1+1<c2){
        float hr=strtof(c1+1,nullptr);
        if(hr>0){
          archiveHrValues[bucket]=hr;
          if(hr>archiveHrTop)archiveHrTop=hr;
        }
      }
      if(*(c2+1)!='\0'){
        float spd=strtof(c2+1,nullptr);
        if(spd>=0){
          archiveSpeedValues[bucket]=spd;
          if(spd>archiveSpeedTop)archiveSpeedTop=spd;
        }
      }
    }

    if(pos<totalInBuf && n>0){
      carry=totalInBuf-pos;
      if(carry>120)carry=120;
      memmove(buf,buf+totalInBuf-carry,carry);
    }else{
      carry=0;
    }

    if(n<=0)break;
  }

  f.close();
  archiveCsvLoaded=true;
}

static void loadArchive(String path){
  archiveSummary.clear();completedText="Completion unavailable";
  String summary=companion(path,".summary.json");
  if(!SD_MMC.exists(summary)){
    int slash=summary.lastIndexOf('/');
    String fn=slash>=0?summary.substring(slash+1):summary;
    if(SD_MMC.exists("/activities/"+fn)) summary="/activities/"+fn;
    else if(SD_MMC.exists("/routes/"+fn)) summary="/routes/"+fn;
    else if(SD_MMC.exists("/"+fn)) summary="/"+fn;
  }
  File f=SD_MMC.open(summary,FILE_READ);
  if(f){deserializeJson(archiveSummary,f);f.close();time_t t=archiveSummary["completedEpoch"]|0;if(t){struct tm local;localtime_r(&t,&local);char b[40];strftime(b,sizeof b,"%d %b %Y %I:%M %p",&local);completedText=b;}}
  
  String gpxP=companion(path,".gpx");
  if(!SD_MMC.exists(gpxP)){
    int slash=gpxP.lastIndexOf('/');
    String fn=slash>=0?gpxP.substring(slash+1):gpxP;
    if(SD_MMC.exists("/routes/"+fn)) gpxP="/routes/"+fn;
    else if(SD_MMC.exists("/activities/"+fn)) gpxP="/activities/"+fn;
    else if(SD_MMC.exists("/"+fn)) gpxP="/"+fn;
  }
  readGPX(gpxP);

  String csvP=companion(path,".csv");
  if(!SD_MMC.exists(csvP)){
    int slash=csvP.lastIndexOf('/');
    String fn=slash>=0?csvP.substring(slash+1):csvP;
    if(SD_MMC.exists("/activities/"+fn)) csvP="/activities/"+fn;
    else if(SD_MMC.exists("/routes/"+fn)) csvP="/routes/"+fn;
    else if(SD_MMC.exists("/"+fn)) csvP="/"+fn;
  }
  loadArchiveCsv(csvP);

  if(completedText=="Completion unavailable" && previewLen>=2){
    completedText=String(previewKm,2)+" km completed";
  }
}

static void previewDraw(int y,int h){
  if(y+h<=28 || y>=290) return;
  card(6,y,228,h,uiCard());
  label(14,y+6,"ROUTE PREVIEW",1,uiFg());
  if(previewLen<2){centeredLabel(6,y+h/2,228,"GPX route unavailable",1,uiMuted());return;}
  double minA=preview[0].lat,maxA=minA,minB=preview[0].lon,maxB=minB;
  for(int i=1;i<previewLen;i++){minA=min(minA,preview[i].lat);maxA=max(maxA,preview[i].lat);minB=min(minB,preview[i].lon);maxB=max(maxB,preview[i].lon);}
  double co=cos(radians((minA+maxA)/2));double scale=min(204.0/max(0.000001,(maxB-minB)*co),(h-24.0)/max(0.000001,maxA-minA));
  auto x=[&](int i){return 120+int((preview[i].lon-(minB+maxB)/2)*co*scale);};auto z=[&](int i){return y+8+h/2-int((preview[i].lat-(minA+maxA)/2)*scale);};
  for(int i=1;i<previewLen;i++){
    int yA=z(i-1), yB=z(i);
    if(yA>=29 && yA<290 && yB>=29 && yB<290){
      display.drawLine(x(i-1),yA,x(i),yB,TEAL);
    }
  }
  int sx=x(0), sy=z(0);
  int ex=x(previewLen-1), ey=z(previewLen-1);
  if(sy>=29 && sy<290) display.fillCircle(sx,sy,3,0x07E0);
  if(ey>=29 && ey<290) display.fillCircle(ex,ey,3,0xF800);
}

static void archiveGraph(String path,int y,int col,const char* title,const char* subtitle=nullptr){
  if(y+76<=28 || y>=290) return;
  card(6,y,228,76,uiCard());
  label(14,y+6,title,1,uiFg());
  if(subtitle) label(120,y+6,subtitle,1,uiMuted());
  float *vals=(col==1?archiveHrValues:archiveSpeedValues);
  float top=(col==1?archiveHrTop:archiveSpeedTop);

  if(!archiveCsvLoaded || top<=0){
    centeredLabel(6,y+34,228,col==1?"No heart rate data":"No speed data",1,uiMuted());
    return;
  }

  uint16_t colr=(col==1?(darkTheme?0xFA6A:0xD000):(darkTheme?0x07FF:0x0419));
  int prev=-1;
  for(int i=0;i<100;i++){
    if(!isfinite(vals[i])){prev=-1;continue;}
    int yy=y+66-int(38*vals[i]/max(1.0f,top));
    if(prev>=29 && prev<290 && yy>=29 && yy<290){
      display.drawLine(14+(i-1)*2,prev,14+i*2,yy,colr);
    }
    prev=yy;
  }
}
