#include "services/GPSService.hpp"
#include "Logger.hpp"
#include <cmath>

GPSService::GPSService(RideData& ride, Config& cfg, SensorFlags& sensors,
                       NavState& nav, StorageService& storage,
                       RoutePoint* route, int& routeLen,
                       TrackPoint* track, int& trackLen)
    : ride_(ride), cfg_(cfg), sensors_(sensors), nav_(nav), storage_(storage),
      route_(route), routeLen_(routeLen), track_(track), trackLen_(trackLen),
      gpsSerial_(config::GPS_UART_NUM) {}

GPSService::~GPSService() = default;

void GPSService::init(TwoWire* wire) {
    Logger::info("GPS", "Initializing GEPRC M1025Q GPS & Sensors");

    if (wire != nullptr) {
        bmpOk_ = bmp_.begin_I2C(config::BMP390_ADDR, wire);
        if (bmpOk_) {
            Logger::info("GPS", "BMP390 initialized at 0x77");
        } else {
            Logger::error("GPS", "BMP390 not detected at 0x77");
        }
        compass_.begin();
    }

    gpsSerial_.setRxBufferSize(2048);
    gpsSerial_.begin(config::GPS_UART_BAUD, SERIAL_8N1, config::GPS_RX_PIN, config::GPS_TX_PIN);
    Logger::info("GPS", "GPS UART started on RX:44, TX:43 @ 115200");
}

float GPSService::haversineKm(float la1, float lo1, float la2, float lo2) {
    const float R = 6371.0f;
    float dLat = radians(la2 - la1);
    float dLon = radians(lo2 - lo1);
    float a = sinf(dLat / 2.0f) * sinf(dLat / 2.0f) +
              cosf(radians(la1)) * cosf(radians(la2)) * sinf(dLon / 2.0f) * sinf(dLon / 2.0f);
    return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

int GPSService::bearingTo(float la1, float lo1, float la2, float lo2) {
    float y = sinf(radians(lo2 - lo1)) * cosf(radians(la2));
    float x = cosf(radians(la1)) * sinf(radians(la2)) -
              sinf(radians(la1)) * cosf(radians(la2)) * cosf(radians(lo2 - lo1));
    return (static_cast<int>(degrees(atan2f(y, x))) + 360) % 360;
}

int GPSService::calcCadence(float speedKmh) const {
    if (speedKmh < 0.5f) return 0;
    return static_cast<int>((speedKmh / 3.6f / cfg_.wheelCircumM) * 60.0f / cfg_.gearRatio);
}

int GPSService::calcVirtualPower(float speedKmh, float gradePercent) const {
    if (speedKmh < 0.5f) return 0;
    float v  = speedKmh / 3.6f;
    float m  = cfg_.riderMassKg + cfg_.bikeMassKg;
    float g  = 9.81f;
    float gr = gradePercent / 100.0f;
    return max(0, static_cast<int>((m * g * gr + 0.004f * m * g + 0.5f * 1.2f * 0.35f * v * v) * v));
}

void GPSService::update() {
    // Feed incoming GPS stream (UBX NAV-PVT + NMEA)
    while (gpsSerial_.available()) {
        geprcGps_.feed(gpsSerial_.read(), gps_);
    }
    geprcGps_.poll(gpsSerial_);
    compass_.update();

    uint32_t now = millis();

    if (gps_.location.isValid() && gps_.location.isUpdated()) {
        float nLat = gps_.location.lat();
        float nLon = gps_.location.lng();

        if (lastLat_ != 0.0f) {
            float delta = haversineKm(lastLat_, lastLon_, nLat, nLon);
            bool dopOk = ((geprcGps_.lastPvt && (now - geprcGps_.lastPvt < 3000))
                          ? (geprcGps_.pdop < 3.0f)
                          : (gps_.hdop.isValid() && gps_.hdop.hdop() < 3.0f));

            if (delta > 0.001f && delta < 0.1f && gps_.speed.kmph() > 2.0f && dopOk) {
                distAccum_ += delta;
                ride_.distanceKm = distAccum_;

                // Grade %: alt change / horizontal distance
                float altDelta = ride_.altM - lastAltM_;
                ride_.gradePercent = constrain((altDelta / (delta * 1000.0f)) * 100.0f, -30.0f, 30.0f);
                lastAltM_ = ride_.altM;

                // Advance route waypoint if within 25m
                if (routeLen_ > 0 && nav_.routeIndex < routeLen_ - 1) {
                    float wd = haversineKm(nLat, nLon, route_[nav_.routeIndex + 1].lat, route_[nav_.routeIndex + 1].lon);
                    if (wd < 0.025f) {
                        nav_.routeIndex++;
                    }
                }
            }
        }
        lastLat_ = ride_.lat = nLat;
        lastLon_ = ride_.lon = nLon;
        ride_.gpsFixed = (gps_.location.age() < 3000);

        // Update navigation guidance
        if (nav_.hasDestination) {
            nav_.remainDistKm = haversineKm(nLat, nLon, nav_.destLat, nav_.destLon);
            nav_.bearingDeg   = bearingTo(nLat, nLon, nav_.destLat, nav_.destLon);
        }

        // Track breadcrumb every 3s while moving
        if ((now - lastTrackMs_ > 3000) && ride_.speedKmh > 0.5f) {
            lastTrackMs_ = now;
            char utc[30] = "";
            if (gps_.date.isValid() && gps_.time.isValid()) {
                snprintf(utc, sizeof(utc), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                         gps_.date.year(), gps_.date.month(), gps_.date.day(),
                         gps_.time.hour(), gps_.time.minute(), gps_.time.second());
            }
            storage_.logBreadcrumb(utc, nLat, nLon, ride_.altM, ride_.speedKmh, ride_.cadenceRpm, ride_.powerW);

            if (trackLen_ < MAX_TRACK_POINTS) {
                track_[trackLen_++] = { nLat, nLon, ride_.altM, ride_.elapsedSec };
            }
        }
    } else if (!gps_.location.isValid()) {
        ride_.gpsFixed = false;
    }

    ride_.gpsFixed = gps_.location.isValid() && (gps_.location.age() < 3000);
    ride_.speedKmh = (ride_.gpsFixed && gps_.speed.isValid()) ? gps_.speed.kmph() : 0.0f;

    // Barometer altitude reading every 500ms
    if (bmpOk_ && (now - sensorMs_ > 500)) {
        sensorMs_ = now;
        if (bmp_.performReading()) {
            ride_.altM = bmp_.readAltitude(1013.25f);
        }
    }
    if (!bmpOk_ && gps_.altitude.isValid()) {
        ride_.altM = gps_.altitude.meters();
    }

    // Physical speed override
    if (sensors_.speedPhysical && (now - externalSensorMs_ < 5000)) {
        ride_.speedKmh = sensors_.physSpeedKmh;
    }

    // Cadence source
    ride_.cadenceRpm = (sensors_.cadencePhysical && (now - externalSensorMs_ < 5000))
                       ? sensors_.physCadenceRpm
                       : calcCadence(ride_.speedKmh);

    // Power and VO2
    ride_.powerW = calcVirtualPower(ride_.speedKmh, ride_.gradePercent);
    ride_.vo2est = cfg_.vo2maxManual;

    // Ride timer
    if (ride_.speedKmh > 0.5f && !rideStarted_) {
        rideStarted_ = true;
        rideStartMs_ = now;
    }
    if (rideStarted_) {
        ride_.elapsedSec = (now - rideStartMs_) / 1000;
        if (ride_.elapsedSec > 0) {
            ride_.avgSpeedKmh = (distAccum_ / static_cast<float>(ride_.elapsedSec)) * 3600.0f;
        }
    }
    if (ride_.speedKmh > ride_.maxSpeedKmh) ride_.maxSpeedKmh = ride_.speedKmh;
    if (ride_.powerW > ride_.maxPowerW) ride_.maxPowerW = ride_.powerW;

    // Heading calculation
    if (ride_.gpsFixed && ride_.speedKmh > 3.0f && gps_.course.isValid()) {
        ride_.heading = gps_.course.deg();
    }
    if (compass_.usable()) {
        ride_.heading = lroundf(compass_.heading) % 360;
    }
}
