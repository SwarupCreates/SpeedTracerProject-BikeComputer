#pragma once
struct ArchivePoint {double lat,lon;float ele;};
ArchivePoint preview[MAX_ROUTE];int previewLen=0;
float previewKm=0,previewGain=0;bool previewElevation=false;
String previewPath,completedText;JsonDocument archiveSummary;
static bool xmlNumber(String tag,const char* key,double &out){
 String k=String(key)+"=";int p=tag.indexOf(k);if(p<0)return false;p+=k.length();while(p<(int)tag.length()&&tag[p]==' ')p++;
 char q=tag[p++];if(q!='\''&&q!='"')return false;int e=tag.indexOf(q,p);if(e<0)return false;
 String v=tag.substring(p,e);char* end;out=strtod(v.c_str(),&end);return end!=v.c_str()&&!*end&&isfinite(out);
}
// Two streaming passes: measure all points, then bounded evenly spaced preview.
// The full original GPX stays on SD. Never silently truncate a long route.
static bool readGPX(String path){
 previewLen=0;previewKm=previewGain=0;previewElevation=false;previewPath=path;
 uint32_t count=0;double lastA=0,lastB=0;float lastE=NAN;
 for(int pass=0;pass<2;pass++){
 File f=SD_MMC.open(path,FILE_READ);if(!f)return false;String tag;uint32_t index=0;bool point=false;double a=0,b=0;float ele=NAN;
 uint32_t stride=max(1UL,(unsigned long)((count+MAX_ROUTE-2)/(MAX_ROUTE-1)));
 while(f.available()){
 char c=f.read();if(c!='<')continue;tag="";
 while(f.available()){c=f.read();if(c=='>')break;if(tag.length()<384)tag+=c;}
 if(tag.startsWith("trkpt ")||tag.startsWith("rtept ")){point=xmlNumber(tag,"lat",a)&&xmlNumber(tag,"lon",b)&&fabs(a)<=90&&fabs(b)<=180;ele=NAN;}
 else if(point&&tag=="ele"){String v=f.readStringUntil('<');ele=v.toFloat();/* closing tag consumed below */}
 else if(point&&(tag=="/trkpt"||tag=="/rtept")){
 if(!pass){if(index)previewKm+=haversineKm(lastA,lastB,a,b);if(isfinite(ele)){previewElevation=true;if(isfinite(lastE)&&ele>lastE)previewGain+=ele-lastE;}lastA=a;lastB=b;lastE=ele;}
 else if((index%stride==0||index+1==count)&&previewLen<MAX_ROUTE)preview[previewLen++]={a,b,ele};
 index++;point=false;
 }
 if((index&127)==0)yield();
 }f.close();if(!pass)count=index;
 }return previewLen>=2;
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
 label(12,y,title,1,uiMuted());File f=SD_MMC.open(companion(path,".csv"),FILE_READ);
 if(!f){label(12,y+16,"No recorded samples",1,uiMuted());return;}
 float values[100];for(auto &v:values)v=NAN;uint32_t duration=archiveSummary["seconds"]|0;float top=0;
 f.readStringUntil('\n');while(f.available()){
 String line=f.readStringUntil('\n');int a=line.indexOf(','),b=line.indexOf(',',a+1);if(a<0||b<0)continue;
 String v=col==1?line.substring(a+1,b):line.substring(b+1);int bucket=constrain((int)(line.substring(0,a).toInt()*99/max(1UL,(unsigned long)duration)),0,99);
 values[bucket]=v.length()?v.toFloat():NAN;if(isfinite(values[bucket]))top=max(top,values[bucket]);
 }f.close();int prev=-1;for(int i=0;i<100;i++){if(!isfinite(values[i])){prev=-1;continue;}int yy=y+46-int(28*values[i]/max(1.0f,top));if(prev>=0)display.drawLine(12+(i-1)*2,prev,12+i*2,yy,TEAL);prev=yy;}
 if(top==0)label(12,y+16,"No nonzero samples",1,uiMuted());
}
