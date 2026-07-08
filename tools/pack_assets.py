#!/usr/bin/env python3
"""
pack_assets.py — PS2 Engine Asset Packer

Reads JSON + source file pairs from game/cd_files/ASSETS/ and compiles them into
binary .ps2a files in game/cd_files/rassets/.

Usage:
    python3 tools/pack_assets.py [--src <ASSETS_DIR>] [--dst <RASSETS_DIR>]

JSON schema (e.g. player_tex.json):
    { "type": "TEXTURE", "source": "player_tex.png", "deps": [] }
    { "type": "MODEL",   "source": "prop.obj",       "deps": ["prop_tex"] }

Binary .ps2a layout:
    [AssetFileHeader]  (fixed-size, 2080 bytes)
    [payload]          (TIM2 for textures, baked BKM2 blob for models)

The TIM2 / BKM2 encoders and the .ps2a header writer live in tools/ps2lib so the
level compiler (tools/compile_level.py) bakes geometry and textures through the
exact same code path.
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2lib import mesh, ps2a, tim2


def pack_asset(json_path, src_dir, dst_dir):
    with open(json_path, "r", encoding="utf-8-sig") as f:
        meta = json.load(f)

    asset_type_str = meta.get("type", "").upper()
    source_name = meta.get("source", "")
    deps = meta.get("deps", [])

    if asset_type_str not in ps2a.TYPE_MAP:
        print(f"  ERROR: unknown type '{asset_type_str}' in {json_path}")
        return False
    if asset_type_str in ("SOUND", "FONT"):
        print(f"  SKIP:  {asset_type_str} is unsupported (dropped with raylib): {json_path}")
        return True
    if not source_name:
        print(f"  ERROR: missing 'source' in {json_path}")
        return False

    source_path = os.path.join(src_dir, source_name)
    if not os.path.isfile(source_path):
        print(f"  SKIP:  source file not found: {source_path}")
        return True

    if len(deps) > ps2a.MAX_DEPS:
        print(f"  WARNING: truncating deps to {ps2a.MAX_DEPS} for {json_path}")
        deps = deps[:ps2a.MAX_DEPS]

    # --- Build payload ---
    if asset_type_str == "TEXTURE":
        tex_fmt = str(meta.get("format", "rgba32")).lower()
        mip_levels = int(meta.get("mipmaps", 0))
        if tex_fmt not in ("rgba32", "pal8"):
            print(f"  WARNING: unknown texture format '{tex_fmt}', using rgba32 for {source_name}")
            tex_fmt = "rgba32"
        try:
            payload, ext_str = tim2.convert_texture_to_tim2(source_path, tex_fmt, mip_levels)
            print(f"  CONV:  {source_name} -> TIM2 {tex_fmt} mips={mip_levels} ({len(payload)} bytes)")
        except ImportError:
            print(f"  ERROR: Pillow required to bake TIM2 textures; cannot pack {source_name}")
            return False
    elif asset_type_str == "MODEL":
        if not source_name.lower().endswith(".obj"):
            print(f"  ERROR: MODEL baking supports .obj only (got '{source_name}')")
            return False
        try:
            payload, ext_str = mesh.bake_obj_model(source_path, has_texture=len(deps) > 0)
            print(f"  BAKE:  {source_name} -> BKM2 ({len(payload)} bytes)")
        except Exception as e:  # noqa: BLE001 — surface any parse failure
            print(f"  ERROR: model bake failed for {source_name}: {e}")
            return False
    else:
        return False

    # --- Build header + write ---
    type_id = ps2a.TYPE_MAP[asset_type_str]
    dep_keys = [f"RASSETS/{d.upper()}.PS2A" for d in deps]
    blob = ps2a.write_ps2a(type_id, payload, dep_keys, ext_str)

    base_name = os.path.splitext(os.path.basename(json_path))[0]
    out_path = os.path.join(dst_dir, f"{base_name}.PS2A")
    with open(out_path, "wb") as fh:
        fh.write(blob)

    print(f"  OK:   {base_name}.PS2A ({len(payload)} bytes payload, {len(dep_keys)} deps)")
    return True


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)

    src_dir = os.path.join(project_root, "game", "cd_files", "ASSETS")
    dst_dir = os.path.join(project_root, "game", "cd_files", "rassets")

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--src" and i + 1 < len(args):
            src_dir = args[i + 1]; i += 2
        elif args[i] == "--dst" and i + 1 < len(args):
            dst_dir = args[i + 1]; i += 2
        else:
            i += 1

    if not os.path.isdir(src_dir):
        print(f"Source directory not found: {src_dir}\nNothing to pack.")
        return 0

    os.makedirs(dst_dir, exist_ok=True)
    json_files = sorted(f for f in os.listdir(src_dir) if f.lower().endswith(".json"))
    if not json_files:
        print(f"No .json asset descriptors found in {src_dir}\nNothing to pack.")
        return 0

    print(f"Packing {len(json_files)} asset(s) from {src_dir} -> {dst_dir}")
    errors = 0
    for jf in json_files:
        if pack_asset(os.path.join(src_dir, jf), src_dir, dst_dir) is False:
            errors += 1

    print(f"\nFinished ({errors} error(s)).")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
