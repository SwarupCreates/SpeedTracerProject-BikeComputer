#pragma once
struct ArchivePoint {double lat,lon;float ele;};
ArchivePoint preview[MAX_ROUTE];int previewLen=0;
float previewKm=0,previewGain=0;bool previewElevation=false;
String previewPath,completedText;JsonDocument archiveSummary;

// Route preview interactive zoom and procedural map state
static int previewZoomLevel = 0; // 0 = Overview (instant, no basemap), 1 = Street 1x, 2 = Street 2x
static double previewPanLat = 0.0;
static double previewPanLon = 0.0;
static double previewScale = 1.0;
static bool mapPanGesture = false;
static double startPanLat = 0.0;
static double startPanLon = 0.0;

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

static void previewDraw(int y, int h) {
  if (y + h <= 28 || y >= 290) return;
  const int cardX = 6, cardW = 228;
  const uint16_t terrainBg = 0xEF7D; // Soft off-white
  display.fillRoundRect(cardX, y, cardW, h, 8, terrainBg);
  display.drawRoundRect(cardX, y, cardW, h, 8, darkTheme ? 0x2945 : 0xCE79);

  if (previewLen < 2) {
    label(14, y + 6, "ROUTE PREVIEW", 1, 0x4208);
    centeredLabel(cardX, y + h / 2, cardW, "GPX route unavailable", 1, uiMuted());
    return;
  }

  // Clip boundaries inside map card
  const int clipX1 = cardX + 2;
  const int clipY1 = y + 2;
  const int clipX2 = cardX + cardW - 3;
  const int clipY2 = y + h - 3;

  double centerLat = 0.0;
  double centerLon = 0.0;
  double scale = 1.0;

  if (previewZoomLevel == 0) {
    // --- MODE 0: FULL ROUTE OVERVIEW (Instantaneous, 0 basemap disk reads, never freezes) ---
    double minA = preview[0].lat, maxA = minA, minB = preview[0].lon, maxB = minB;
    for (int i = 1; i < previewLen; i++) {
      minA = min(minA, preview[i].lat);
      maxA = max(maxA, preview[i].lat);
      minB = min(minB, preview[i].lon);
      maxB = max(maxB, preview[i].lon);
    }
    centerLat = (minA + maxA) / 2.0;
    centerLon = (minB + maxB) / 2.0;
    double co = cos(radians(centerLat));
    double spanLonM = max(0.000001, (maxB - minB) * co * 111320.0);
    double spanLatM = max(0.000001, (maxA - minA) * 111320.0);
    scale = min((cardW - 30.0) / (spanLonM * 1.15), (h - 30.0) / (spanLatM * 1.15));
    previewScale = scale;
  } else {
    // --- MODE 1 & 2: ZOOMED-IN PROCEDURAL STREET INSPECTION (PSRAM 3x3 tiles only) ---
    if (previewPanLat == 0.0 && previewPanLon == 0.0) {
      previewPanLat = preview[0].lat;
      previewPanLon = preview[0].lon;
    }
    centerLat = previewPanLat;
    centerLon = previewPanLon;
    scale = (previewZoomLevel == 1) ? 0.85 : 1.6; // 0.85 px/m (~260m across) or 1.6 px/m (~140m across)
    previewScale = scale;

    // Align PSRAM sliding window around current pan position
    g_mapCache.updatePosition(centerLat, centerLon);
  }

  double co = cos(radians(centerLat));
  int cardCenterX = cardX + cardW / 2;
  int cardCenterY = y + h / 2;

  auto projectCoord = [&](double pLat, double pLon, int &outX, int &outY) {
    double dxM = (pLon - centerLon) * co * 111320.0;
    double dyM = (pLat - centerLat) * 111320.0;
    outX = cardCenterX + (int)lround(dxM * scale);
    outY = cardCenterY - (int)lround(dyM * scale);
  };

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
        } else if (width == 2) {
          display.drawLine(x0, y0, x1, y1, color);
          display.drawLine(x0 + 1, y0, x1 + 1, y1, color);
        } else {
          for (int w = -(width / 2); w <= width / 2; w++) {
            display.drawLine(x0 + w, y0, x1 + w, y1, color);
            display.drawLine(x0, y0 + w, x1, y1 + w, color);
          }
        }
        return;
      } else if (c0 & c1) {
        return;
      } else {
        int x, y;
        uint8_t cOut = c0 ? c0 : c1;
        if (cOut & 8) {
          x = x0 + (int)((int64_t)(x1 - x0) * (clipY2 - y0) / (y1 - y0));
          y = clipY2;
        } else if (cOut & 4) {
          x = x0 + (int)((int64_t)(x1 - x0) * (clipY1 - y0) / (y1 - y0));
          y = clipY1;
        } else if (cOut & 2) {
          x = y0 + (int)((int64_t)(y1 - y0) * (clipX2 - x0) / (x1 - x0));
          x = clipX2;
        } else {
          y = y0 + (int)((int64_t)(y1 - y0) * (clipX1 - x0) / (x1 - x0));
          x = clipX1;
        }
        if (cOut == c0) { x0 = x; y0 = y; c0 = getCode(x0, y0); }
        else { x1 = x; y1 = y; c1 = getCode(x1, y1); }
      }
    }
  };

  // --- DRAW BASEMAP ONLY WHEN ZOOMED IN (Zero overhead in overview!) ---
  if (previewZoomLevel > 0) {
    const uint16_t waterColor = 0x9E5F; // #90CAF9
    const uint16_t minorRoadColor = 0xD6BA; // #D6D6D6
    const uint16_t majorBorder = 0xCE79;
    const uint16_t majorCorridor = 0xFFFF;
    const double n = 16384.0;

    int centerTX, centerTY;
    float cfx, cfy;
    MapTileCache::latLonToTile(centerLat, centerLon, centerTX, centerTY, cfx, cfy);

    // Only render tiles from the 9-tile PSRAM sliding window that touch our view
    for (int s = 0; s < MapTileCache::CACHE_SIZE; s++) {
      if (!g_mapCache.tiles[s].loaded || !g_mapCache.tiles[s].features || !g_mapCache.tiles[s].points) continue;
      int tx = g_mapCache.tiles[s].tileX;
      int ty = g_mapCache.tiles[s].tileY;
      // Only process immediate neighboring tiles (1-2 tiles max)
      if (abs(tx - centerTX) > 1 || abs(ty - centerTY) > 1) continue;

      MapFeatureRecord* feats = g_mapCache.tiles[s].features;
      MapPoint12* pts = g_mapCache.tiles[s].points;
      uint16_t featCount = g_mapCache.tiles[s].header.featureCount;

      // Pass A: Water bodies
      for (uint16_t f = 0; f < featCount; f++) {
        if (feats[f].type == FEAT_WATER) {
          uint8_t cnt = feats[f].pointCount;
          uint16_t off = feats[f].pointOffset;
          for (uint8_t k = 1; k < cnt; k++) {
            double x0T = (double)tx + (double)pts[off + k - 1].u / 4096.0;
            double y0T = (double)ty + (double)pts[off + k - 1].v / 4096.0;
            double x1T = (double)tx + (double)pts[off + k].u / 4096.0;
            double y1T = (double)ty + (double)pts[off + k].v / 4096.0;

            double lon0 = (x0T / n) * 360.0 - 180.0;
            double lat0 = atan(sinh(PI * (1.0 - 2.0 * (y0T / n)))) * (180.0 / PI);
            double lon1 = (x1T / n) * 360.0 - 180.0;
            double lat1 = atan(sinh(PI * (1.0 - 2.0 * (y1T / n)))) * (180.0 / PI);

            int sx0, sy0, sx1, sy1;
            projectCoord(lat0, lon0, sx0, sy0);
            projectCoord(lat1, lon1, sx1, sy1);
            clipAndDrawLine(sx0, sy0, sx1, sy1, waterColor, 3);
          }
        }
      }

      // Pass B: Residential / minor roads
      for (uint16_t f = 0; f < featCount; f++) {
        uint8_t ft = feats[f].type;
        if (ft == FEAT_RESIDENTIAL || ft == FEAT_CYCLEWAY) {
          uint8_t cnt = feats[f].pointCount;
          uint16_t off = feats[f].pointOffset;
          for (uint8_t k = 1; k < cnt; k++) {
            double x0T = (double)tx + (double)pts[off + k - 1].u / 4096.0;
            double y0T = (double)ty + (double)pts[off + k - 1].v / 4096.0;
            double x1T = (double)tx + (double)pts[off + k].u / 4096.0;
            double y1T = (double)ty + (double)pts[off + k].v / 4096.0;

            double lon0 = (x0T / n) * 360.0 - 180.0;
            double lat0 = atan(sinh(PI * (1.0 - 2.0 * (y0T / n)))) * (180.0 / PI);
            double lon1 = (x1T / n) * 360.0 - 180.0;
            double lat1 = atan(sinh(PI * (1.0 - 2.0 * (y1T / n)))) * (180.0 / PI);

            int sx0, sy0, sx1, sy1;
            projectCoord(lat0, lon0, sx0, sy0);
            projectCoord(lat1, lon1, sx1, sy1);
            clipAndDrawLine(sx0, sy0, sx1, sy1, minorRoadColor, 2);
          }
        }
      }

      // Pass C: Major / Primary corridors (white with gray border)
      for (uint16_t f = 0; f < featCount; f++) {
        uint8_t ft = feats[f].type;
        if (ft == FEAT_MOTORWAY || ft == FEAT_PRIMARY || ft == FEAT_SECONDARY) {
          uint8_t cnt = feats[f].pointCount;
          uint16_t off = feats[f].pointOffset;
          for (uint8_t k = 1; k < cnt; k++) {
            double x0T = (double)tx + (double)pts[off + k - 1].u / 4096.0;
            double y0T = (double)ty + (double)pts[off + k - 1].v / 4096.0;
            double x1T = (double)tx + (double)pts[off + k].u / 4096.0;
            double y1T = (double)ty + (double)pts[off + k].v / 4096.0;

            double lon0 = (x0T / n) * 360.0 - 180.0;
            double lat0 = atan(sinh(PI * (1.0 - 2.0 * (y0T / n)))) * (180.0 / PI);
            double lon1 = (x1T / n) * 360.0 - 180.0;
            double lat1 = atan(sinh(PI * (1.0 - 2.0 * (y1T / n)))) * (180.0 / PI);

            int sx0, sy0, sx1, sy1;
            projectCoord(lat0, lon0, sx0, sy0);
            projectCoord(lat1, lon1, sx1, sy1);
            clipAndDrawLine(sx0, sy0, sx1, sy1, majorBorder, 4);
            clipAndDrawLine(sx0, sy0, sx1, sy1, majorCorridor, 2);
          }
        }
      }
    }
  }

  // --- DRAW GPX ROUTE VECTOR (Vibrant Orange, 3px or 4px) ---
  const uint16_t routeOrange = 0xFD00; // #FF7A00
  int routeW = (previewZoomLevel > 0) ? 4 : 3;
  for (int i = 1; i < previewLen; i++) {
    int sx0, sy0, sx1, sy1;
    projectCoord(preview[i - 1].lat, preview[i - 1].lon, sx0, sy0);
    projectCoord(preview[i].lat, preview[i].lon, sx1, sy1);
    clipAndDrawLine(sx0, sy0, sx1, sy1, routeOrange, routeW);
  }

  // Start (Green) & End (Red) Badges
  int sx, sy, ex, ey;
  projectCoord(preview[0].lat, preview[0].lon, sx, sy);
  projectCoord(preview[previewLen - 1].lat, preview[previewLen - 1].lon, ex, ey);
  if (sy >= clipY1 && sy <= clipY2 && sx >= clipX1 && sx <= clipX2) {
    display.fillCircle(sx, sy, 5, 0x07E0);
    display.drawCircle(sx, sy, 5, 0xFFFF);
    display.drawCircle(sx, sy, 6, 0x0000);
  }
  if (ey >= clipY1 && ey <= clipY2 && ex >= clipX1 && ex <= clipX2) {
    display.fillCircle(ex, ey, 5, 0xF800);
    display.drawCircle(ex, ey, 5, 0xFFFF);
    display.drawCircle(ex, ey, 6, 0x0000);
  }

  // --- HEADER & HINTS ON CARD ---
  if (previewZoomLevel == 0) {
    label(14, y + 6, "ROUTE PREVIEW", 1, 0x4208);
    label(14, y + 18, "Overview", 1, uiMuted());
    label(14, y + h - 14, "Tap + to inspect turns", 1, uiMuted());
  } else {
    label(14, y + 6, "STREET DETAIL", 1, 0x4208);
    label(14, y + 18, (previewZoomLevel == 1 ? "1x Turn View" : "2x Close View"), 1, 0x02E0);
    label(14, y + h - 14, "Drag to pan along route", 1, uiMuted());
  }

  // --- ZOOM BUTTONS (+ / - / Reset) at top-right corner ---
  int btnX = cardX + cardW - 32; // 202
  int btnW = 26, btnH = 24;

  // 1. Plus Button (+)
  int btnPlusY = y + 6;
  display.fillRoundRect(btnX, btnPlusY, btnW, btnH, 4, (previewZoomLevel < 2) ? 0xFFFF : 0xE71C);
  display.drawRoundRect(btnX, btnPlusY, btnW, btnH, 4, 0xCE79);
  uint16_t plusCol = (previewZoomLevel < 2) ? 0x0000 : 0x8410;
  display.drawFastHLine(btnX + 7, btnPlusY + 11, 12, plusCol);
  display.drawFastHLine(btnX + 7, btnPlusY + 12, 12, plusCol);
  display.drawFastVLine(btnX + 12, btnPlusY + 6, 12, plusCol);
  display.drawFastVLine(btnX + 13, btnPlusY + 6, 12, plusCol);

  // 2. Minus Button (-)
  int btnMinusY = btnPlusY + 28;
  display.fillRoundRect(btnX, btnMinusY, btnW, btnH, 4, (previewZoomLevel > 0) ? 0xFFFF : 0xE71C);
  display.drawRoundRect(btnX, btnMinusY, btnW, btnH, 4, 0xCE79);
  uint16_t minusCol = (previewZoomLevel > 0) ? 0x0000 : 0x8410;
  display.drawFastHLine(btnX + 7, btnMinusY + 11, 12, minusCol);
  display.drawFastHLine(btnX + 7, btnMinusY + 12, 12, minusCol);

  // 3. Reset to Start Button (only shown when zoomed in)
  if (previewZoomLevel > 0) {
    int btnResetY = btnMinusY + 28;
    display.fillRoundRect(btnX, btnResetY, btnW, btnH, 4, 0xFFFF);
    display.drawRoundRect(btnX, btnResetY, btnW, btnH, 4, 0xCE79);
    display.fillCircle(btnX + 13, btnResetY + 12, 4, 0x07E0);
    display.drawCircle(btnX + 13, btnResetY + 12, 4, 0x0000);
  }
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
