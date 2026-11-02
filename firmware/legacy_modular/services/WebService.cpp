#include "services/WebService.hpp"
#include "Logger.hpp"

static const char pageHTML[] PROGMEM = R"HTML(<!doctype html><meta name="viewport" content="width=device-width"><title>CycloComp</title><style>body{font:16px system-ui;background:#101c28;color:white;max-width:600px;margin:30px auto;padding:16px}input,button,textarea{padding:12px;margin:5px;max-width:90%}pre{white-space:pre-wrap}</style><h1>CycloComp V2</h1><p>Prototype: coordinate guidance, not road routing. Estimates are not measured fitness data.</p><h2>Destination</h2><input id="coord" placeholder="latitude, longitude"><button onclick="nav()">Navigate</button><h2>Configuration JSON</h2><textarea id="conf">{"gearRatio":2.94,"wheelCircumM":2.096,"mass":75,"targetDistanceKm":50,"vo2maxManual":0}</textarea><button onclick="post('/config',JSON.parse(conf.value))">Save</button><h2>Route JSON</h2><input type="file" id="route"><button onclick="upload()">Load route</button><p>Array of {"lat":number,"lon":number}, max 512 points.</p><a href="/export">Download current GPX preview (max 1024 points)</a><p><a href="/log">Download full SD ride CSV</a></p><pre id="out"></pre><script>async function post(p,d){let r=await fetch(p,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});out.textContent=await r.text()}function nav(){let a=coord.value.split(',').map(Number);if(a.length===2&&a.every(Number.isFinite))post('/nav',{lat:a[0],lon:a[1]})}async function upload(){if(route.files[0])post('/upload',JSON.parse(await route.files[0].text()))}setInterval(async()=>{try{out.textContent=JSON.stringify(await(await fetch('/status')).json(),null,2)}catch(e){}},2000)</script>)HTML";

WebService::WebService(RideData& ride, Config& cfg, SensorFlags& sensors,
                       NavState& nav, StorageService& storage, GPSService& gpsService,
                       RoutePoint* route, int& routeLen,
                       TrackPoint* track, int& trackLen)
    : ride_(ride), cfg_(cfg), sensors_(sensors), nav_(nav), storage_(storage),
      gpsService_(gpsService), route_(route), routeLen_(routeLen),
      track_(track), trackLen_(trackLen), server_(80) {}

WebService::~WebService() = default;

void WebService::init() {
    Logger::info("Web", "Starting WiFi SoftAP 'CycloComp'");
    WiFi.softAP("CycloComp", "cycling1234");

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/nav", HTTP_POST, [this]() { handleNavSet(); });
    server_.on("/upload", HTTP_POST, [this]() { handleUploadRoute(); });
    server_.on("/config", HTTP_POST, [this]() { handleConfig(); });
    server_.on("/sensor", HTTP_POST, [this]() { handleSensor(); });
    server_.on("/export", HTTP_GET, [this]() { handleExportGPX(); });
    server_.on("/log", HTTP_GET, [this]() { handleLogDownload(); });
    server_.on("/compass", HTTP_POST, [this]() { handleCompass(); });

    server_.begin();
    Logger::info("Web", "HTTP WebServer listening on port 80");
}

void WebService::update() {
    server_.handleClient();
}

void WebService::handleRoot() {
    server_.send_P(200, "text/html", pageHTML);
}

void WebService::handleStatus() {
    StaticJsonDocument<512> doc;
    doc["speedKmh"]        = ride_.speedKmh;
    doc["distKm"]          = ride_.distanceKm;
    doc["cadence"]         = ride_.cadenceRpm;
    doc["power"]           = ride_.powerW;
    doc["vo2maxManual"]    = cfg_.vo2maxManual;

    auto& comp = gpsService_.getCompass();
    doc["compassReady"]       = comp.ready;
    doc["compassCalibrated"]  = comp.calibrated;
    doc["compassCalibrating"] = comp.calibrating;
    doc["headingSource"]      = comp.usable() ? "magnetic-level-only" : "GPS course";
    doc["compassX"]           = comp.raw[0];
    doc["compassY"]           = comp.raw[1];
    doc["compassZ"]           = comp.raw[2];
    doc["touchReady"]         = false;
    doc["blePairingReady"]    = false;
    doc["barometerReady"]     = gpsService_.isBmpOk();

    doc["heading"]         = ride_.heading;
    doc["lat"]             = ride_.lat;
    doc["lon"]             = ride_.lon;
    doc["altM"]            = ride_.altM;
    doc["gradePercent"]    = ride_.gradePercent;
    doc["elapsed"]         = ride_.elapsedSec;
    doc["avgSpeedKmh"]     = ride_.avgSpeedKmh;
    doc["maxSpeedKmh"]     = ride_.maxSpeedKmh;
    doc["maxPowerW"]       = ride_.maxPowerW;
    doc["gpsFixed"]        = ride_.gpsFixed;
    doc["sdOk"]            = storage_.isOk();
    doc["cadenceHW"]       = sensors_.cadencePhysical;
    doc["speedHW"]         = sensors_.speedPhysical;
    doc["targetDistKm"]    = cfg_.targetDistanceKm;

    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
}

void WebService::handleNavSet() {
    if (!server_.hasArg("plain")) {
        server_.send(400, "text/plain", "No body");
        return;
    }
    StaticJsonDocument<64> doc;
    if (deserializeJson(doc, server_.arg("plain"))) {
        server_.send(400, "text/plain", "Bad JSON");
        return;
    }
    if (!doc["lat"].is<double>() || !doc["lon"].is<double>() ||
        fabs(doc["lat"].as<double>()) > 90.0 || fabs(doc["lon"].as<double>()) > 180.0) {
        server_.send(400, "text/plain", "Latitude -90..90, longitude -180..180 required");
        return;
    }

    nav_.destLat        = doc["lat"] | 0.0f;
    nav_.destLon        = doc["lon"] | 0.0f;
    nav_.hasDestination = true;
    nav_.remainDistKm   = GPSService::haversineKm(ride_.lat, ride_.lon, nav_.destLat, nav_.destLon);
    nav_.bearingDeg     = GPSService::bearingTo(ride_.lat, ride_.lon, nav_.destLat, nav_.destLon);
    server_.send(200, "text/plain", "Destination set");
}

void WebService::handleUploadRoute() {
    if (!server_.hasArg("plain")) {
        server_.send(400, "text/plain", "No body");
        return;
    }
    DynamicJsonDocument doc(16384);
    if (deserializeJson(doc, server_.arg("plain"))) {
        server_.send(400, "text/plain", "Bad JSON");
        return;
    }
    if (!doc.is<JsonArray>() || doc.size() > MAX_ROUTE_POINTS) {
        server_.send(400, "text/plain", "Route must be an array with at most 512 points");
        return;
    }
    for (JsonObject pt : doc.as<JsonArray>()) {
        if (!pt["lat"].is<double>() || !pt["lon"].is<double>() ||
            fabs(pt["lat"].as<double>()) > 90.0 || fabs(pt["lon"].as<double>()) > 180.0) {
            server_.send(400, "text/plain", "Invalid point");
            return;
        }
    }

    routeLen_ = 0;
    for (JsonObject pt : doc.as<JsonArray>()) {
        if (routeLen_ >= MAX_ROUTE_POINTS) break;
        route_[routeLen_++] = { pt["lat"].as<float>(), pt["lon"].as<float>() };
    }
    if (routeLen_ > 0) {
        nav_.hasDestination = true;
        nav_.destLat = route_[routeLen_ - 1].lat;
        nav_.destLon = route_[routeLen_ - 1].lon;
    }
    nav_.routeIndex = 0;
    storage_.saveRoute(doc);
    server_.send(200, "text/plain", "Route: " + String(routeLen_) + " pts");
}

void WebService::handleConfig() {
    if (!server_.hasArg("plain")) {
        server_.send(400, "text/plain", "No body");
        return;
    }
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, server_.arg("plain"))) {
        server_.send(400, "text/plain", "Bad JSON");
        return;
    }

    if (doc.containsKey("gearRatio"))        cfg_.gearRatio        = doc["gearRatio"];
    if (doc.containsKey("wheelCircumM"))     cfg_.wheelCircumM     = doc["wheelCircumM"];
    if (doc.containsKey("mass"))             cfg_.riderMassKg      = doc["mass"];
    if (doc.containsKey("maxSpeedKmh"))      cfg_.maxSpeedKmh      = doc["maxSpeedKmh"];
    if (doc.containsKey("maxPowerW"))        cfg_.maxPowerW        = doc["maxPowerW"];
    if (doc.containsKey("maxCadenceRpm"))    cfg_.maxCadRpm        = doc["maxCadenceRpm"];
    if (doc.containsKey("vo2maxManual"))     cfg_.vo2maxManual     = doc["vo2maxManual"];
    if (doc.containsKey("targetDistanceKm"))  cfg_.targetDistanceKm = doc["targetDistanceKm"];

    cfg_.gearRatio        = constrain(cfg_.gearRatio, 0.1f, 10.0f);
    cfg_.wheelCircumM     = constrain(cfg_.wheelCircumM, 0.5f, 3.0f);
    cfg_.riderMassKg      = constrain(cfg_.riderMassKg, 20.0f, 250.0f);
    cfg_.maxSpeedKmh      = constrain(cfg_.maxSpeedKmh, 10.0f, 150.0f);
    cfg_.maxPowerW        = constrain(cfg_.maxPowerW, 50.0f, 2500.0f);

    prefs_.begin("cyclo", false);
    prefs_.putFloat("gearRatio",   cfg_.gearRatio);
    prefs_.putFloat("wheelCircum", cfg_.wheelCircumM);
    prefs_.putFloat("riderMass",   cfg_.riderMassKg);
    prefs_.putFloat("maxSpeed",    cfg_.maxSpeedKmh);
    prefs_.putFloat("maxPower",    cfg_.maxPowerW);
    prefs_.putFloat("maxCad",      cfg_.maxCadRpm);
    prefs_.putFloat("vo2max",      cfg_.vo2maxManual);
    prefs_.putFloat("targetDist",  cfg_.targetDistanceKm);
    prefs_.end();

    server_.send(200, "text/plain", "Config saved");
}

void WebService::handleSensor() {
    if (!server_.hasArg("plain")) {
        server_.send(400, "text/plain", "No body");
        return;
    }
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, server_.arg("plain"))) {
        server_.send(400, "text/plain", "Bad JSON");
        return;
    }
    if (doc.containsKey("cadence"))    sensors_.cadencePhysical = doc["cadence"].as<bool>();
    if (doc.containsKey("speed"))      sensors_.speedPhysical   = doc["speed"].as<bool>();
    if (doc.containsKey("cadenceRpm")) sensors_.physCadenceRpm  = doc["cadenceRpm"];
    if (doc.containsKey("speedKmh"))   sensors_.physSpeedKmh    = doc["speedKmh"];

    server_.send(200, "text/plain", "External bridge readings updated (not BLE pairing)");
}

void WebService::handleExportGPX() {
    String gpx = "<?xml version=\"1.0\"?>\n<gpx version=\"1.1\" creator=\"CycloComp\">\n<trk><trkseg>\n";
    for (int i = 0; i < trackLen_; i++) {
        gpx += "<trkpt lat=\"";
        gpx += String(track_[i].lat, 6);
        gpx += "\" lon=\"";
        gpx += String(track_[i].lon, 6);
        gpx += "\"><ele>";
        gpx += String(track_[i].alt, 1);
        gpx += "</ele></trkpt>\n";
    }
    gpx += "</trkseg></trk>\n</gpx>";
    server_.send(200, "application/gpx+xml", gpx);
}

void WebService::handleLogDownload() {
    File f = storage_.openFile("/ride.csv", FILE_READ);
    if (!f) {
        server_.send(404, "text/plain", "No SD log");
        return;
    }
    server_.sendHeader("Content-Disposition", "attachment; filename=ride.csv");
    server_.streamFile(f, "text/csv");
    f.close();
}

void WebService::handleCompass() {
    StaticJsonDocument<128> d;
    if (deserializeJson(d, server_.arg("plain"))) {
        server_.send(400, "text/plain", "Invalid JSON");
        return;
    }
    auto& comp = gpsService_.getCompass();
    String action = d["action"] | "";
    if (action == "start") {
        comp.start();
    } else if (action == "finish") {
        if (!comp.finish()) {
            server_.send(400, "text/plain", "Insufficient rotation: rotate entire device level through full circles");
            return;
        }
    } else if (action == "alignment") {
        float y = d["yawOffset"] | 0.0f;
        float dec = d["declination"] | 0.0f;
        if (!isfinite(y) || !isfinite(dec) || fabs(y) > 360.0f || fabs(dec) > 90.0f) {
            server_.send(400, "text/plain", "Invalid angle");
            return;
        }
        comp.yawOffset = y;
        comp.declination = dec;
        comp.save();
    } else {
        server_.send(400, "text/plain", "Use action start, finish or alignment");
        return;
    }
    server_.send(200, "text/plain", "OK");
}
