"""
SpeedTracer Offline Vector Tile Compiler
Extracts road networks and water features from OSM PBF and compiles
compact binary Slippy tiles (Zoom 14) for ESP32-S3.
"""

import os
import sys
import math
import struct
import argparse
from collections import defaultdict
import osmium

ZOOM_LEVEL = 14

# Feature Types matching firmware enum
FEAT_NONE = 0
FEAT_MOTORWAY = 1     # Motorway / Trunk
FEAT_PRIMARY = 2      # Primary
FEAT_SECONDARY = 3    # Secondary / Tertiary
FEAT_RESIDENTIAL = 4  # Residential / Unclassified
FEAT_CYCLEWAY = 5     # Cycleway
FEAT_WATER = 6        # Rivers, Canals, Water bodies
FEAT_PARK = 7         # Parks, green spaces

HIGHWAY_MAP = {
    "motorway": FEAT_MOTORWAY,
    "motorway_link": FEAT_MOTORWAY,
    "trunk": FEAT_MOTORWAY,
    "trunk_link": FEAT_MOTORWAY,
    "primary": FEAT_PRIMARY,
    "primary_link": FEAT_PRIMARY,
    "secondary": FEAT_SECONDARY,
    "secondary_link": FEAT_SECONDARY,
    "tertiary": FEAT_SECONDARY,
    "tertiary_link": FEAT_SECONDARY,
    "residential": FEAT_RESIDENTIAL,
    "living_street": FEAT_RESIDENTIAL,
    "unclassified": FEAT_RESIDENTIAL,
    "cycleway": FEAT_CYCLEWAY,
}

WATERWAY_MAP = {
    "river": FEAT_WATER,
    "canal": FEAT_WATER,
    "stream": FEAT_WATER,
    "drain": FEAT_WATER,
}

def lat_lon_to_tile_frac(lat, lon, zoom=ZOOM_LEVEL):
    """Returns (tile_x, tile_y, frac_x, frac_y) where frac is 0.0..1.0 within tile."""
    lat_rad = math.radians(lat)
    n = 2.0 ** zoom
    x = (lon + 180.0) / 360.0 * n
    y = (1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n
    tx = int(math.floor(x))
    ty = int(math.floor(y))
    fx = x - tx
    fy = y - ty
    return tx, ty, fx, fy

class OsmFeatureExtractor(osmium.SimpleHandler):
    def __init__(self, bbox=None):
        super().__init__()
        self.bbox = bbox  # (min_lat, min_lon, max_lat, max_lon) or None
        # Dict mapping (tile_x, tile_y) -> list of features
        # Each feature is (feat_type, [(u0, v0), (u1, v1), ...])
        self.tiles = defaultdict(list)
        self.total_ways_processed = 0
        self.total_features_added = 0

    def way(self, w):
        tags = w.tags
        feat_type = None

        if "highway" in tags:
            h = tags["highway"]
            if h in HIGHWAY_MAP:
                feat_type = HIGHWAY_MAP[h]
        elif "waterway" in tags:
            ww = tags["waterway"]
            if ww in WATERWAY_MAP:
                feat_type = FEAT_WATER
        elif "natural" in tags and tags["natural"] == "water":
            feat_type = FEAT_WATER
        elif "leisure" in tags and tags["leisure"] in ("park", "nature_reserve", "garden"):
            feat_type = FEAT_PARK

        if feat_type is None:
            return

        self.total_ways_processed += 1

        # Extract coordinates
        coords = []
        for n in w.nodes:
            if not n.location.valid():
                continue
            lat = n.location.lat
            lon = n.location.lon
            if self.bbox:
                # Filter bounding box with slight margin
                min_lat, min_lon, max_lat, max_lon = self.bbox
                if lat < min_lat or lat > max_lat or lon < min_lon or lon > max_lon:
                    continue
            coords.append((lat, lon))

        if len(coords) < 2:
            return

        # Segment coordinates across tiles
        # Convert each point to continuous tile coordinates:
        # X = tx + fx, Y = ty + fy
        tile_segments = defaultdict(list)
        current_tile = None
        current_subpoly = []

        for lat, lon in coords:
            tx, ty, fx, fy = lat_lon_to_tile_frac(lat, lon, ZOOM_LEVEL)
            # 12-bit integer coord within tile (0..4095)
            u = max(0, min(4095, int(fx * 4096.0)))
            v = max(0, min(4095, int(fy * 4096.0)))
            tkey = (tx, ty)

            if current_tile is None:
                current_tile = tkey
                current_subpoly = [(u, v)]
            elif current_tile == tkey:
                current_subpoly.append((u, v))
            else:
                # Crossed tile boundary: save segment in current tile
                if len(current_subpoly) >= 2:
                    tile_segments[current_tile].append(current_subpoly)
                elif len(current_subpoly) == 1:
                    # Single point on boundary - still include for continuity
                    tile_segments[current_tile].append([current_subpoly[0], (u, v)])
                current_tile = tkey
                current_subpoly = [(u, v)]

        if current_tile and len(current_subpoly) >= 2:
            tile_segments[current_tile].append(current_subpoly)

        # Add segments into self.tiles
        for tkey, polylines in tile_segments.items():
            for poly in polylines:
                if len(poly) >= 2:
                    # Simplify or cap point count to 255 per segment
                    chunks = [poly[i:i + 250] for i in range(0, len(poly), 249)]
                    for ch in chunks:
                        if len(ch) >= 2:
                            self.tiles[tkey].append((feat_type, ch))
                            self.total_features_added += 1

def serialize_tile(tile_x, tile_y, features):
    """
    Serializes features into SpeedTracer binary tile format (.bin).
    Returns bytes.
    """
    # Header format:
    # 4s (magic: MAP1)
    # H (tileX)
    # H (tileY)
    # B (zoom)
    # B (flags)
    # H (featureCount)
    # H (totalPoints)
    # H (reserved)
    feature_count = len(features)
    total_points = sum(len(pts) for _, pts in features)

    # Feature record format:
    # B (type)
    # B (pointCount)
    # H (pointOffset)

    # Point record format:
    # H (u: 0..4095)
    # H (v: 0..4095)

    feat_records = bytearray()
    point_records = bytearray()
    point_offset = 0

    for feat_type, pts in features:
        pcount = len(pts)
        feat_records.extend(struct.pack("<BBH", feat_type, pcount, point_offset))
        for u, v in pts:
            point_records.extend(struct.pack("<HH", u, v))
        point_offset += pcount

    header = struct.pack(
        "<4sHHBBHHH",
        b"MAP1",
        tile_x,
        tile_y,
        ZOOM_LEVEL,
        0, # flags
        feature_count,
        total_points,
        0  # reserved
    )

    return bytes(header + feat_records + point_records)

def create_pak_archive(tile_data_dict, pak_path):
    """
    Packs multiple tiles into a single indexed .pak archive.
    Header: 'PAK1', tile_count (uint32)
    Index: [tile_x (H), tile_y (H), offset (I), length (I)] * tile_count
    """
    tile_count = len(tile_data_dict)
    index_entry_size = 12 # 2 + 2 + 4 + 4
    header_size = 8       # 4s + I
    data_start = header_size + (tile_count * index_entry_size)

    header = struct.pack("<4sI", b"PAK1", tile_count)
    index_bytes = bytearray()
    data_bytes = bytearray()

    current_offset = data_start
    for (tx, ty), tbytes in tile_data_dict.items():
        tlen = len(tbytes)
        index_bytes.extend(struct.pack("<HHII", tx, ty, current_offset, tlen))
        data_bytes.extend(tbytes)
        current_offset += tlen

    with open(pak_path, "wb") as f:
        f.write(header)
        f.write(index_bytes)
        f.write(data_bytes)

    print(f"Packed {tile_count} tiles into {pak_path} ({os.path.getsize(pak_path)} bytes)")

def main():
    parser = argparse.ArgumentParser(description="SpeedTracer Offline Map Compiler")
    parser.add_argument("osm_pbf", help="Path to input .osm.pbf file")
    parser.add_argument("--out-dir", default="maps/tiles", help="Output directory for individual tiles")
    parser.add_argument("--pak", default="maps/wb_z14.pak", help="Output path for packed archive")
    parser.add_argument("--bbox", nargs=4, type=float, metavar=('MIN_LAT', 'MIN_LON', 'MAX_LAT', 'MAX_LON'),
                        help="Optional bounding box: min_lat min_lon max_lat max_lon")
    args = parser.parse_args()

    print(f"Starting vector tile compilation from {args.osm_pbf}...")
    if args.bbox:
        print(f"Bounding box filter: {args.bbox}")

    handler = OsmFeatureExtractor(bbox=args.bbox)
    # osmium NodeLocationsForWays provides locations for way nodes
    print("Reading OSM PBF ways and nodes...")
    handler.apply_file(args.osm_pbf, locations=True)

    print(f"Ways processed: {handler.total_ways_processed}")
    print(f"Total features extracted: {handler.total_features_added}")
    print(f"Unique tiles generated: {len(handler.tiles)}")

    os.makedirs(args.out_dir, exist_ok=True)
    serialized_tiles = {}

    for (tx, ty), features in handler.tiles.items():
        tbytes = serialize_tile(tx, ty, features)
        serialized_tiles[(tx, ty)] = tbytes

        # Also save individual tile file
        tile_file = os.path.join(args.out_dir, f"{ZOOM_LEVEL}_{tx}_{ty}.bin")
        with open(tile_file, "wb") as f:
            f.write(tbytes)

    # Save PAK archive
    if args.pak:
        os.makedirs(os.path.dirname(args.pak) or ".", exist_ok=True)
        create_pak_archive(serialized_tiles, args.pak)

    print("Map compilation complete!")

if __name__ == "__main__":
    main()
