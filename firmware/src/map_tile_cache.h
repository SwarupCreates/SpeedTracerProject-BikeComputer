#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <math.h>
#include <vector>

#define MAP_ZOOM_LEVEL 14

// Feature Types matching compiler
enum MapFeatureType : uint8_t {
  FEAT_NONE = 0,
  FEAT_MOTORWAY = 1,     // Motorways / Trunks (corridor)
  FEAT_PRIMARY = 2,      // Primary roads
  FEAT_SECONDARY = 3,    // Secondary / Tertiary roads
  FEAT_RESIDENTIAL = 4,  // Residential / Minor roads
  FEAT_CYCLEWAY = 5,     // Dedicated bike lanes
  FEAT_WATER = 6,        // Rivers / Canals / Water bodies
  FEAT_PARK = 7          // Parks / Green spaces
};

struct __attribute__((packed)) MapTileHeader {
  char     magic[4];       // 'M','A','P','1'
  uint16_t tileX;          // Slippy tile X (Zoom 14)
  uint16_t tileY;          // Slippy tile Y (Zoom 14)
  uint8_t  zoom;           // Zoom level (14)
  uint8_t  flags;          // Reserved
  uint16_t featureCount;   // Number of feature records
  uint16_t totalPoints;    // Total point coordinates in tile
  uint16_t reserved;
};

struct __attribute__((packed)) MapFeatureRecord {
  uint8_t  type;           // MapFeatureType
  uint8_t  pointCount;     // Points in this polyline (2..255)
  uint16_t pointOffset;    // Offset into points buffer
};

struct __attribute__((packed)) MapPoint12 {
  uint16_t u;  // 0..4095: X relative to tile west edge
  uint16_t v;  // 0..4095: Y relative to tile north edge
};

struct __attribute__((packed)) PakIndexEntry {
  uint16_t tileX;
  uint16_t tileY;
  uint32_t offset;
  uint32_t length;
};

// Cached tile in PSRAM
struct CachedTile {
  bool loaded = false;
  uint16_t tileX = 0;
  uint16_t tileY = 0;
  MapTileHeader header;
  MapFeatureRecord* features = nullptr; // Allocated in PSRAM
  MapPoint12* points = nullptr;         // Allocated in PSRAM

  void freeBuffers() {
    if (features) { free(features); features = nullptr; }
    if (points) { free(points); points = nullptr; }
    loaded = false;
  }
};

class MapTileCache {
public:
  static const int CACHE_SIZE = 9; // 3x3 sliding window
  CachedTile tiles[CACHE_SIZE];
  
  bool pakLoaded = false;
  String pakPath = "";
  File pakFile;
  PakIndexEntry* pakIndex = nullptr; // in PSRAM
  uint32_t pakTileCount = 0;

  int currentCenterTileX = -1;
  int currentCenterTileY = -1;

  void begin() {
    // Check for PAK archives in order of preference
    const char* candidates[] = {
      "/maps/kolkata_z14.pak",
      "/maps/kolkata_test_z14.pak",
      "/maps/wb_z14.pak",
      "/maps/eastern_z14.pak"
    };

    for (const char* path : candidates) {
      if (SD_MMC.exists(path)) {
        if (openPak(path)) {
          Serial.printf("[MAP] Loaded PAK archive %s with %lu tiles\n", path, (unsigned long)pakTileCount);
          return;
        }
      }
    }
    Serial.println("[MAP] No PAK archive found, will attempt loose tiles in /maps/tiles/");
  }

  bool openPak(const char* path) {
    if (pakLoaded) closePak();
    pakFile = SD_MMC.open(path, FILE_READ);
    if (!pakFile) return false;

    char magic[4];
    if (pakFile.read((uint8_t*)magic, 4) != 4 || memcmp(magic, "PAK1", 4) != 0) {
      pakFile.close();
      return false;
    }

    if (pakFile.read((uint8_t*)&pakTileCount, 4) != 4 || pakTileCount == 0 || pakTileCount > 50000) {
      pakFile.close();
      return false;
    }

    size_t indexSize = pakTileCount * sizeof(PakIndexEntry);
    pakIndex = (PakIndexEntry*)ps_malloc(indexSize);
    if (!pakIndex) {
      pakIndex = (PakIndexEntry*)malloc(indexSize);
    }
    if (!pakIndex) {
      pakFile.close();
      return false;
    }

    if (pakFile.read((uint8_t*)pakIndex, indexSize) != indexSize) {
      free(pakIndex);
      pakIndex = nullptr;
      pakFile.close();
      return false;
    }

    pakPath = path;
    pakLoaded = true;
    return true;
  }

  void closePak() {
    if (pakFile) pakFile.close();
    if (pakIndex) { free(pakIndex); pakIndex = nullptr; }
    pakLoaded = false;
    pakTileCount = 0;
  }

  // Convert lat/lon to Zoom 14 tile coordinates
  static void latLonToTile(double lat, double lon, int &tileX, int &tileY, float &fracX, float &fracY) {
    double latRad = lat * (PI / 180.0);
    double n = 16384.0; // 2^14
    double x = (lon + 180.0) / 360.0 * n;
    double y = (1.0 - asinh(tan(latRad)) / PI) / 2.0 * n;
    tileX = (int)floor(x);
    tileY = (int)floor(y);
    fracX = (float)(x - floor(x));
    fracY = (float)(y - floor(y));
  }

  // Find index in PAK
  int findInPak(uint16_t tx, uint16_t ty) {
    if (!pakLoaded || !pakIndex) return -1;
    for (uint32_t i = 0; i < pakTileCount; i++) {
      if (pakIndex[i].tileX == tx && pakIndex[i].tileY == ty) {
        return (int)i;
      }
    }
    return -1;
  }

  // Load a single tile into destination CachedTile
  bool loadTile(uint16_t tx, uint16_t ty, CachedTile &dst) {
    dst.freeBuffers();

    // 1. Try PAK archive
    if (pakLoaded) {
      int idx = findInPak(tx, ty);
      if (idx >= 0) {
        if (!pakFile.seek(pakIndex[idx].offset)) return false;
        if (pakFile.read((uint8_t*)&dst.header, sizeof(MapTileHeader)) != sizeof(MapTileHeader)) return false;
        if (memcmp(dst.header.magic, "MAP1", 4) != 0) return false;

        size_t featBytes = dst.header.featureCount * sizeof(MapFeatureRecord);
        size_t ptBytes = dst.header.totalPoints * sizeof(MapPoint12);

        if (featBytes > 0) {
          dst.features = (MapFeatureRecord*)ps_malloc(featBytes);
          if (!dst.features) dst.features = (MapFeatureRecord*)malloc(featBytes);
          if (dst.features) {
            pakFile.read((uint8_t*)dst.features, featBytes);
          }
        }
        if (ptBytes > 0) {
          dst.points = (MapPoint12*)ps_malloc(ptBytes);
          if (!dst.points) dst.points = (MapPoint12*)malloc(ptBytes);
          if (dst.points) {
            pakFile.read((uint8_t*)dst.points, ptBytes);
          }
        }

        dst.tileX = tx;
        dst.tileY = ty;
        dst.loaded = true;
        return true;
      }
    }

    // 2. Try loose tile file
    char path[64];
    snprintf(path, sizeof(path), "/maps/tiles/%d_%d_%d.bin", MAP_ZOOM_LEVEL, tx, ty);
    if (!SD_MMC.exists(path)) return false;

    File f = SD_MMC.open(path, FILE_READ);
    if (!f) return false;

    if (f.read((uint8_t*)&dst.header, sizeof(MapTileHeader)) != sizeof(MapTileHeader)) {
      f.close();
      return false;
    }
    if (memcmp(dst.header.magic, "MAP1", 4) != 0) {
      f.close();
      return false;
    }

    size_t featBytes = dst.header.featureCount * sizeof(MapFeatureRecord);
    size_t ptBytes = dst.header.totalPoints * sizeof(MapPoint12);

    if (featBytes > 0) {
      dst.features = (MapFeatureRecord*)ps_malloc(featBytes);
      if (!dst.features) dst.features = (MapFeatureRecord*)malloc(featBytes);
      if (dst.features) f.read((uint8_t*)dst.features, featBytes);
    }
    if (ptBytes > 0) {
      dst.points = (MapPoint12*)ps_malloc(ptBytes);
      if (!dst.points) dst.points = (MapPoint12*)malloc(ptBytes);
      if (dst.points) f.read((uint8_t*)dst.points, ptBytes);
    }

    f.close();
    dst.tileX = tx;
    dst.tileY = ty;
    dst.loaded = true;
    return true;
  }

  // Update 3x3 sliding window around lat/lon
  void updatePosition(double lat, double lon) {
    if (lat == 0.0 && lon == 0.0) return;
    int tx, ty;
    float fx, fy;
    latLonToTile(lat, lon, tx, ty, fx, fy);

    if (tx == currentCenterTileX && ty == currentCenterTileY) return;

    currentCenterTileX = tx;
    currentCenterTileY = ty;

    // Target 3x3 tiles
    uint16_t targetX[9], targetY[9];
    int tIdx = 0;
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        targetX[tIdx] = (uint16_t)(tx + dx);
        targetY[tIdx] = (uint16_t)(ty + dy);
        tIdx++;
      }
    }

    // Keep existing tiles that match targets
    bool targetMatched[9] = {false};
    bool slotPreserved[CACHE_SIZE] = {false};

    for (int s = 0; s < CACHE_SIZE; s++) {
      if (tiles[s].loaded) {
        bool needed = false;
        for (int t = 0; t < 9; t++) {
          if (!targetMatched[t] && tiles[s].tileX == targetX[t] && tiles[s].tileY == targetY[t]) {
            targetMatched[t] = true;
            slotPreserved[s] = true;
            needed = true;
            break;
          }
        }
        if (!needed) {
          tiles[s].freeBuffers();
        }
      }
    }

    // Load missing target tiles into available slots
    for (int t = 0; t < 9; t++) {
      if (!targetMatched[t]) {
        for (int s = 0; s < CACHE_SIZE; s++) {
          if (!slotPreserved[s] && !tiles[s].loaded) {
            loadTile(targetX[t], targetY[t], tiles[s]);
            slotPreserved[s] = true;
            break;
          }
        }
      }
    }
  }

  // Snapping algorithm: project point P(lat, lon) onto nearest road centerline segment
  bool snapPointToRoad(double &ptLat, double &ptLon, float maxSnapMeters = 25.0f) {
    if (currentCenterTileX < 0) updatePosition(ptLat, ptLon);

    int ptx, pty;
    float pfx, pfy;
    latLonToTile(ptLat, ptLon, ptx, pty, pfx, pfy);

    float pu = (float)ptx * 4096.0f + pfx * 4096.0f;
    float pv = (float)pty * 4096.0f + pfy * 4096.0f;

    double latRad = ptLat * (PI / 180.0);
    float metersPerUnit = (float)((40075016.686 * cos(latRad) / 16384.0) / 4096.0);
    float maxDistUnits = maxSnapMeters / metersPerUnit;
    float bestDistSq = maxDistUnits * maxDistUnits;
    float bestProjU = pu, bestProjV = pv;
    bool found = false;

    for (int s = 0; s < CACHE_SIZE; s++) {
      if (!tiles[s].loaded || !tiles[s].features || !tiles[s].points) continue;
      float baseU = (float)tiles[s].tileX * 4096.0f;
      float baseV = (float)tiles[s].tileY * 4096.0f;

      for (uint16_t f = 0; f < tiles[s].header.featureCount; f++) {
        uint8_t ftype = tiles[s].features[f].type;
        // Only snap to road types (exclude water and parks)
        if (ftype != FEAT_MOTORWAY && ftype != FEAT_PRIMARY &&
            ftype != FEAT_SECONDARY && ftype != FEAT_RESIDENTIAL &&
            ftype != FEAT_CYCLEWAY) continue;

        uint8_t cnt = tiles[s].features[f].pointCount;
        uint16_t off = tiles[s].features[f].pointOffset;

        for (uint8_t i = 1; i < cnt; i++) {
          float ax = baseU + (float)tiles[s].points[off + i - 1].u;
          float ay = baseV + (float)tiles[s].points[off + i - 1].v;
          float bx = baseU + (float)tiles[s].points[off + i].u;
          float by = baseV + (float)tiles[s].points[off + i].v;

          float abx = bx - ax;
          float aby = by - ay;
          float segLenSq = abx * abx + aby * aby;
          if (segLenSq < 1.0f) continue;

          float t = ((pu - ax) * abx + (pv - ay) * aby) / segLenSq;
          if (t < 0.0f) t = 0.0f;
          else if (t > 1.0f) t = 1.0f;

          float prx = ax + t * abx;
          float pry = ay + t * aby;
          float dx = pu - prx;
          float dy = pv - pry;
          float dSq = dx * dx + dy * dy;

          if (dSq < bestDistSq) {
            bestDistSq = dSq;
            bestProjU = prx;
            bestProjV = pry;
            found = true;
          }
        }
      }
    }

    if (found) {
      // Convert projected continuous tile coordinate back to lat/lon
      double n = 16384.0;
      double x = (double)bestProjU / 4096.0;
      double y = (double)bestProjV / 4096.0;
      ptLon = (x / n) * 360.0 - 180.0;
      double lr = atan(sinh(PI * (1.0 - 2.0 * (y / n))));
      ptLat = lr * (180.0 / PI);
      return true;
    }
    return false;
  }
};

extern MapTileCache g_mapCache;
