import os, struct

def pack_tiles(tile_files, out_pak):
    tile_dict = {}
    for tf in tile_files:
        basename = os.path.basename(tf)
        parts = basename.replace('.bin', '').split('_')
        z, tx, ty = int(parts[0]), int(parts[1]), int(parts[2])
        with open(tf, 'rb') as f:
            tile_dict[(tx, ty)] = f.read()

    tile_count = len(tile_dict)
    index_entry_size = 12
    header_size = 8
    data_start = header_size + (tile_count * index_entry_size)

    header = struct.pack("<4sI", b"PAK1", tile_count)
    index_bytes = bytearray()
    data_bytes = bytearray()

    current_offset = data_start
    for (tx, ty), tbytes in tile_dict.items():
        tlen = len(tbytes)
        index_bytes.extend(struct.pack("<HHII", tx, ty, current_offset, tlen))
        data_bytes.extend(tbytes)
        current_offset += tlen

    with open(out_pak, "wb") as f:
        f.write(header)
        f.write(index_bytes)
        f.write(data_bytes)

    print(f"Packed {tile_count} tiles into {out_pak} ({os.path.getsize(out_pak)} bytes)")

if __name__ == '__main__':
    tile_dir = "maps/tiles"
    # 27 tiles covering route from (12214, 7134) to (12216, 7142)
    selected = []
    for x in (12214, 12215, 12216):
        for y in range(7134, 7143):
            fn = os.path.join(tile_dir, f"14_{x}_{y}.bin")
            if os.path.exists(fn):
                selected.append(fn)

    pack_tiles(selected, "maps/kolkata_test_z14.pak")
