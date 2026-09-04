#!/usr/bin/env python3
"""
cook_assets.py - stage 3 of the build pipeline: cook.

Reads JSON + source file pairs from the asset source directory and bakes each
into an engine-native .ps2a, using the target platform's cook list to decide how.
Output is per platform, because the right encoding is a hardware question - see
docs/PIPELINE.md.

Usage:
    python3 tools/cook_assets.py --platform ps2pal
    python3 tools/cook_assets.py --cooklist <FILE> --src <DIR> --dst <DIR>

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


DEFAULT_COOKLIST = {
    "platform": "<none>",
    "assets": {
        "TEXTURE": {"enabled": True, "format": "source"},
        "MODEL": {"enabled": True},
        "SOUND": {"enabled": False},
        "FONT": {"enabled": False},
    },
}


def load_cooklist(path):
    """Read a platform cook list. Missing file means cook everything as authored,
    which is what a platform that has not declared a policy should get."""
    if not path or not os.path.isfile(path):
        return DEFAULT_COOKLIST
    with open(path, "r", encoding="utf-8-sig") as fh:
        return json.load(fh)


def cooklist_for_platform(project_root, platform):
    """Map a CMake platform name onto the directory holding its cook list.
    Regional variants share their base platform's list."""
    name = (platform or "").lower()
    if name.startswith("ps2"):
        return os.path.join(project_root, "engine", "platform", "ps2", "cooklist.json")
    if name.startswith("vita"):
        return os.path.join(project_root, "engine", "platform", "vita", "cooklist.json")
    if name:
        return os.path.join(project_root, "engine", "platform", name, "cooklist.json")
    return None


def pack_asset(json_path, src_dir, dst_dir, cooklist=None):
    with open(json_path, "r", encoding="utf-8-sig") as f:
        meta = json.load(f)

    asset_type_str = meta.get("type", "").upper()
    source_name = meta.get("source", "")
    deps = meta.get("deps", [])

    if asset_type_str not in ps2a.TYPE_MAP:
        print(f"  ERROR: unknown type '{asset_type_str}' in {json_path}")
        return False

    policy = (cooklist or DEFAULT_COOKLIST).get("assets", {}).get(asset_type_str, {})
    if not policy.get("enabled", False):
        print(f"  SKIP:  {asset_type_str} is not cooked on this platform: {json_path}")
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

        # The cook list overrides the authored format unless it defers to it.
        # This is what lets one source tree cook palettised for the console and
        # directly-uploadable for the desktop.
        want = str(policy.get("format", "source")).lower()
        if want != "source":
            tex_fmt = want

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
    dst_dir = None
    cooklist_path = None
    platform = None

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--src" and i + 1 < len(args):
            src_dir = args[i + 1]; i += 2
        elif args[i] == "--dst" and i + 1 < len(args):
            dst_dir = args[i + 1]; i += 2
        elif args[i] == "--platform" and i + 1 < len(args):
            platform = args[i + 1]; i += 2
        elif args[i] == "--cooklist" and i + 1 < len(args):
            cooklist_path = args[i + 1]; i += 2
        else:
            i += 1

    # Cooked output is keyed by platform: the encodings differ, so one shared
    # directory would mean each platform overwriting the other's work.
    if dst_dir is None:
        if not platform:
            print("cook_assets: --platform or --dst is required")
            return 1
        dst_dir = os.path.join(project_root, "dist", "cooked", platform.lower(), "rassets")

    if cooklist_path is None:
        cooklist_path = cooklist_for_platform(project_root, platform)

    cooklist = load_cooklist(cooklist_path)
    listed = cooklist.get("platform", "<default>")

    if not os.path.isdir(src_dir):
        print("Source directory not found: " + src_dir)
        return 0

    os.makedirs(dst_dir, exist_ok=True)
    json_files = sorted(f for f in os.listdir(src_dir) if f.lower().endswith(".json"))
    if not json_files:
        print("No .json asset descriptors found in " + src_dir)
        return 0

    print(f"Cooking {len(json_files)} asset(s) for '{platform or listed}' [cook list: {listed}]")
    print(f"  {src_dir} -> {dst_dir}")
    errors = 0
    for jf in json_files:
        if pack_asset(os.path.join(src_dir, jf), src_dir, dst_dir, cooklist) is False:
            errors += 1

    print("")
    print(f"Finished ({errors} error(s)).")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
