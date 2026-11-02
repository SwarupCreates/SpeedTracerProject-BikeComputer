#include "services/StorageService.hpp"
#include "Logger.hpp"

StorageService::StorageService()
    : sdSPI_(HSPI) {}

StorageService::~StorageService() = default;

bool StorageService::init() {
    Logger::info("Storage", "Initializing FAT32 MicroSD card via HSPI");
    sdSPI_.begin(config::SD_SCK_PIN, config::SD_MISO_PIN, config::SD_MOSI_PIN, config::SD_CS_PIN);
    sdOk_ = SD.begin(config::SD_CS_PIN, sdSPI_);
    if (sdOk_) {
        Logger::info("Storage", "MicroSD mounted successfully");
        SD.mkdir("/maps");
    } else {
        Logger::error("Storage", "MicroSD mount failed");
    }
    return sdOk_;
}

bool StorageService::loadRoute(RoutePoint* route, int& routeLen, int maxRoute, NavState& nav) {
    if (!sdOk_ || !SD.exists("/route.json")) return false;

    File f = SD.open("/route.json", "r");
    if (!f) return false;

    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Logger::error("Storage", "Failed to parse /route.json");
        return false;
    }

    routeLen = 0;
    for (JsonObject pt : doc.as<JsonArray>()) {
        if (routeLen >= maxRoute) break;
        route[routeLen++] = { pt["lat"].as<float>(), pt["lon"].as<float>() };
    }

    if (routeLen > 0) {
        nav.hasDestination = true;
        nav.destLat = route[routeLen - 1].lat;
        nav.destLon = route[routeLen - 1].lon;
    }
    Logger::info("Storage", "Loaded route points from SD card");
    return true;
}

bool StorageService::saveRoute(const DynamicJsonDocument& doc) {
    if (!sdOk_) return false;
    File f = SD.open("/route.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

void StorageService::logBreadcrumb(const char* utc, float lat, float lon, float alt, float speedKmh, int cadenceRpm, int powerW) {
    if (!sdOk_) return;
    File f = SD.open("/ride.csv", FILE_APPEND);
    if (f) {
        if (!f.size()) {
            f.println("utc,lat,lon,alt_m,speed_kmh,cadence_est_rpm,power_est_w");
        }
        f.printf("%s,%.7f,%.7f,%.1f,%.2f,%d,%d\n",
                 utc, lat, lon, alt, speedKmh, cadenceRpm, powerW);
        f.close();
    }
}

File StorageService::openFile(const char* path, const char* mode) {
    if (!sdOk_) return File();
    return SD.open(path, mode);
}

bool StorageService::exists(const char* path) {
    if (!sdOk_) return false;
    return SD.exists(path);
}
