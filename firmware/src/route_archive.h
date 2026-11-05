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

// Fast stream-buffered GPX parser (4096-byte blocks, zero heap thrashing, ~50-80ms total)
static bool readGPX(String path){
  previewLen=0;previewKm=0;previewGain=0;previewElevation=false;previewPath=path;
  uint32_t count=0;
  const int CHUNK_SIZE=4096;
  static char buf[CHUNK_SIZE+256];

  for(int pass=0;pass<2;pass++){
    File f=SD_MMC.open(path,FILE_READ);if(!f)return false;
    uint32_t index=0;
    uint32_t stride=1;
    if(pass==1){stride=max(1UL,(unsigned long)((count+MAX_ROUTE-2)/(MAX_ROUTE-1)));}
    double lastA=0,lastB=0;float lastE=NAN;
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
            if(eleP){
              ele=strtof(eleP+5,nullptr);
            }

            if(!pass){
              if(index){
                previewKm+=haversineKm(lastA,lastB,a,b);
                if(isfinite(ele)){
                  previewElevation=true;
                  if(isfinite(lastE)&&ele>lastE)previewGain+=(ele-lastE);
                }
              }
              lastA=a;lastB=b;lastE=ele;
            }else{
              if((index%stride==0 || index+1==count) && previewLen<MAX_ROUTE){
                preview[previewLen++]={a,b,ele};
              }
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
    }
  }

  return previewLen>=2;
}

static void loadArchiveCsv(String csvPath){
  archiveCsvLoaded=false;
  archiveHrTop=0;
  archiveSpeedTop=0;
  for(int i=0;i<100;i++){archiveHrValues[i]=NAN;archiveSpeedValues[i]=NAN;}

  File f=SD_MMC.open(csvPath,FILE_READ);
  if(!f)return;

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
  String summary=companion(path,".summary.json");File f=SD_MMC.open(summary,FILE_READ);
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
  card(6,y,228,h,uiCard());if(previewLen<2){label(16,y+12,"GPX route unavailable",1,uiMuted());return;}
  double minA=preview[0].lat,maxA=minA,minB=preview[0].lon,maxB=minB;
  for(int i=1;i<previewLen;i++){minA=min(minA,preview[i].lat);maxA=max(maxA,preview[i].lat);minB=min(minB,preview[i].lon);maxB=max(maxB,preview[i].lon);}
  double co=cos(radians((minA+maxA)/2));double scale=min(204.0/max(0.000001,(maxB-minB)*co),(h-16.0)/max(0.000001,maxA-minA));
  auto x=[&](int i){return 120+int((preview[i].lon-(minB+maxB)/2)*co*scale);};auto z=[&](int i){return y+h/2-int((preview[i].lat-(minA+maxA)/2)*scale);};
  for(int i=1;i<previewLen;i++)display.drawLine(x(i-1),z(i-1),x(i),z(i),TEAL);
}

static void archiveGraph(String path,int y,int col,const char* title){
  label(12,y,title,1,uiMuted());
  float *vals=(col==1?archiveHrValues:archiveSpeedValues);
  float top=(col==1?archiveHrTop:archiveSpeedTop);

  if(!archiveCsvLoaded || top<=0){
    label(12,y+16,"No recorded samples",1,uiMuted());
    return;
  }

  int prev=-1;
  for(int i=0;i<100;i++){
    if(!isfinite(vals[i])){prev=-1;continue;}
    int yy=y+46-int(28*vals[i]/max(1.0f,top));
    if(prev>=0)display.drawLine(12+(i-1)*2,prev,12+i*2,yy,TEAL);
    prev=yy;
  }
}
