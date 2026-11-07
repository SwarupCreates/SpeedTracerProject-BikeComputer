#pragma once
// Streaming companions keep FIT untouched and preserve missing samples as gaps.
File gpxFile, samplesFile;
float calorieEstimate=0;
uint32_t calorieSeconds=0;
static String companion(String path,const char* suffix){int p=path.lastIndexOf('.');return (p<0?path:path.substring(0,p))+suffix;}
static void archiveBegin(){
 calorieEstimate=0;calorieSeconds=0;
 gpxFile=SD_MMC.open(companion(activityPath,".gpx"),FILE_WRITE);
 if(gpxFile)gpxFile.print("<?xml version=\"1.0\"?><gpx version=\"1.1\" creator=\"CycloComp\" xmlns=\"http://www.topografix.com/GPX/1/1\"><trk><trkseg>\n");
 samplesFile=SD_MMC.open(companion(activityPath,".csv"),FILE_WRITE);
 if(samplesFile)samplesFile.println("seconds,heart_rate_bpm,speed_kmh");
}
static void archivePoint(double lat,double lon){
 if(!gpxFile)return;
 gpxFile.printf("<trkpt lat=\"%.7f\" lon=\"%.7f\">",lat,lon);
 if(isfinite(ride.altM))gpxFile.printf("<ele>%.2f</ele>",ride.altM);
 time_t t=time(nullptr);struct tm utc;gmtime_r(&t,&utc);char b[32];strftime(b,sizeof b,"%Y-%m-%dT%H:%M:%SZ",&utc);
 gpxFile.printf("<time>%s</time></trkpt>\n",b);gpxFile.flush();
}
static void archiveFinish(bool fitOK){
 if(gpxFile){gpxFile.println("</trkseg></trk></gpx>");gpxFile.flush();gpxFile.close();}
 if(samplesFile){samplesFile.flush();samplesFile.close();}
 if(!fitOK)return;
 JsonDocument d;d["completedEpoch"]=(uint32_t)time(nullptr);d["distanceKm"]=ride.distanceKm;d["seconds"]=ride.elapsedSec;d["elevationGainM"]=ride.elevationGainM;d["elevationGainM"]=ride.elevationGainM;
 if(calorieSeconds)d["caloriesEstimate"]=calorieEstimate;
 if(ride.vo2est>0)d["vo2Estimate"]=ride.vo2est;
 d["calorieCoverageSeconds"]=calorieSeconds;
 File f=SD_MMC.open(companion(activityPath,".summary.json"),FILE_WRITE);if(f){serializeJson(d,f);f.close();}
}
static bool speedAvailable(){return (ride.gpsFixed&&gps.speed.isValid()&&gps.speed.age()<3000)||(!runningMode&&(bleSpeedReady||(sensors.speedPhysical&&millis()-externalSensorMs<5000)));}
