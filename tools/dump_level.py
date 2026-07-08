#!/usr/bin/env python3
"""
dump_level.py — inspect a compiled level archive (.PS2R containing a .ps2l).

Read-only. Prints the chunk table, grid occupancy, materials, entities, and
per-sector mesh stats; optionally renders a top-down occupancy PNG.

Usage:
  python3 tools/dump_level.py build/levels/TEST.PS2R [--png occ.png]
"""

import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2lib import levelfmt
import pack_archive


def dump(archive_path, png=None):
    toc = pack_archive.read_toc(archive_path)
    by_key = {e["key"]: e for e in toc["entries"]}
    core_key = next((k for k in by_key if k.endswith(".PS2L")), None)
    if not core_key:
        print("no .ps2l core in archive")
        return 1

    core = pack_archive.read_payload(archive_path, by_key[core_key])
    lv = levelfmt.parse_ps2l(core)
    print(f"archive: {archive_path}  ({toc['entry_count']} entries)")
    print(f".ps2l:   version {lv['version']}, {lv['chunk_count']} chunks, {lv['total_size']} bytes")
    for c in lv["chunks"]:
        print(f"  chunk {c['name']}  offset={c['offset']} size={c['size']}")

    info_chunk = next((c for c in lv["chunks"] if c["name"] == "INFO"), None)
    info = levelfmt.parse_info(core, info_chunk)
    print(f"\ninfo: name={info['name']} grid={info['cells_x']}x{info['cells_z']} "
          f"cell={info['cell_size']} origin=({info['origin_x']},{info['origin_z']}) "
          f"materials={info['material_count']} entities={info['entity_count']}")

    matl = next((c for c in lv["chunks"] if c["name"] == "MATL"), None)
    if matl:
        print("materials:")
        for k in levelfmt.parse_materials(core, matl):
            print(f"  {k}")

    grid = next((c for c in lv["chunks"] if c["name"] == "SGRD"), None)
    cells = levelfmt.parse_grid(core, grid, info["cells_x"], info["cells_z"])
    occupied = sum(1 for c in cells if c["sector_bytes"] > 0)
    print(f"\ngrid: {occupied}/{len(cells)} cells have geometry")

    total_verts = 0
    for e in toc["entries"]:
        if e["key"].endswith(".SEC"):
            sec = levelfmt.parse_sector(pack_archive.read_payload(archive_path, e))
            verts = sum(m["vert_count"] for m in sec["meshes"])
            total_verts += verts
            print(f"  {e['key']}: {sec['mesh_count']} meshes, {verts} verts, {e['size']} bytes")
    print(f"total sector verts: {total_verts}")

    farf = next((c for c in lv["chunks"] if c["name"] == "FARF"), None)
    if farf:
        cluster_count, atlas_count, m0, m1, m2, m3, azimuths, ground = struct.unpack_from(
            "<IIIIIIII", core, farf["offset"])
        print(f"\nfar field: {cluster_count} clusters, {atlas_count} atlas(es), {azimuths} azimuths")

    if png:
        _render(png, info, cells)
    return 0


def _render(path, info, cells):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("Pillow not available; skipping --png")
        return
    cx, cz = info["cells_x"], info["cells_z"]
    scale = 24
    img = Image.new("RGB", (cx * scale + 1, cz * scale + 1), (30, 30, 30))
    d = ImageDraw.Draw(img)
    for z in range(cz):
        for x in range(cx):
            cell = cells[z * cx + x]
            fill = (70, 120, 70) if cell["sector_bytes"] > 0 else (50, 50, 50)
            d.rectangle([x * scale, z * scale, x * scale + scale, z * scale + scale],
                        fill=fill, outline=(90, 90, 90))
    img.save(path)
    print(f"occupancy render -> {path}")


def main(argv=None):
    ap = argparse.ArgumentParser(description="Inspect a compiled level archive")
    ap.add_argument("archive", help=".PS2R level archive")
    ap.add_argument("--png", help="write a top-down occupancy image")
    args = ap.parse_args(argv)
    return dump(args.archive, args.png)


if __name__ == "__main__":
    sys.exit(main())
