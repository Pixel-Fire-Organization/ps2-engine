#!/usr/bin/env python3
"""
pack_assets.py — PS2 Engine Asset Packer

Reads JSON + source file pairs from app/cd_files/RAYLIB/ and compiles them
into binary .ps2a files in app/cd_files/rassets/.

Usage:
    python3 scripts/pack_assets.py [--src <RAYLIB_DIR>] [--dst <RASSETS_DIR>]

JSON schema (e.g. player_tex.json):
    {
        "type": "TEXTURE",
        "source": "player_tex.png",
        "deps": []
    }

Binary .ps2a layout:
    [AssetFileHeader]  (fixed-size)
    [raw source bytes] (variable-size)

AssetFileHeader (C struct, packed):
    uint32_t magic          — 0x50533241 ("PS2A")
    uint32_t type           — 0=TEXTURE, 1=MODEL, 2=SOUND, 3=FONT
    uint8_t  depCount
    uint8_t  reserved[3]
    char     deps[8][256]   — null-terminated dependency asset paths
    uint32_t dataSize       — byte count of the raw payload
"""

import json
import os
import struct
import sys

MAGIC = 0x50533241  # "PS2A" little-endian
MAX_DEPS = 8
MAX_PATH_LEN = 256

TYPE_MAP = {
    "TEXTURE": 0,
    "MODEL": 1,
    "SOUND": 2,
    "FONT": 3,
}

# Header size: 4 + 4 + 1 + 3 + (8 * 256) + 4 = 2064 bytes
HEADER_SIZE = 4 + 4 + 1 + 3 + (MAX_DEPS * MAX_PATH_LEN) + 4


def pack_dep_string(dep_str):
    """Encode a dependency path as a fixed-width 256-byte null-terminated field."""
    encoded = dep_str.encode("utf-8")[:MAX_PATH_LEN - 1]
    return encoded + b"\x00" * (MAX_PATH_LEN - len(encoded))


def pack_asset(json_path, src_dir, dst_dir):
    """Pack a single JSON + source pair into a .ps2a binary."""
    with open(json_path, "r") as f:
        meta = json.load(f)

    asset_type_str = meta.get("type", "").upper()
    source_name = meta.get("source", "")
    deps = meta.get("deps", [])

    if asset_type_str not in TYPE_MAP:
        print(f"  ERROR: unknown type '{asset_type_str}' in {json_path}")
        return False

    if not source_name:
        print(f"  ERROR: missing 'source' in {json_path}")
        return False

    source_path = os.path.join(src_dir, source_name)
    if not os.path.isfile(source_path):
        print(f"  ERROR: source file not found: {source_path}")
        return False

    if len(deps) > MAX_DEPS:
        print(f"  WARNING: truncating deps to {MAX_DEPS} for {json_path}")
        deps = deps[:MAX_DEPS]

    # Read the raw source data
    with open(source_path, "rb") as f:
        payload = f.read()

    # Build the header
    type_id = TYPE_MAP[asset_type_str]
    dep_count = len(deps)

    header = struct.pack("<II", MAGIC, type_id)
    header += struct.pack("B", dep_count)
    header += b"\x00" * 3  # reserved

    for i in range(MAX_DEPS):
        if i < dep_count:
            # Dependency paths point to rassets/<dep_name>.ps2a on disc
            dep_path = f"cdrom0:\\RASSETS\\{deps[i].upper()}.PS2A;1"
            header += pack_dep_string(dep_path)
        else:
            header += b"\x00" * MAX_PATH_LEN

    header += struct.pack("<I", len(payload))

    assert len(header) == HEADER_SIZE, f"Header size mismatch: {len(header)} != {HEADER_SIZE}"

    # Write the .ps2a file
    base_name = os.path.splitext(os.path.basename(json_path))[0]
    out_path = os.path.join(dst_dir, f"{base_name}.ps2a")

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(payload)

    print(f"  OK: {base_name}.ps2a ({len(payload)} bytes payload, {dep_count} deps)")
    return True


def main():
    # Default paths relative to project root
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)

    src_dir = os.path.join(project_root, "app", "cd_files", "RAYLIB")
    dst_dir = os.path.join(project_root, "app", "cd_files", "rassets")

    # Allow overrides via command-line
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--src" and i + 1 < len(args):
            src_dir = args[i + 1]
            i += 2
        elif args[i] == "--dst" and i + 1 < len(args):
            dst_dir = args[i + 1]
            i += 2
        else:
            i += 1

    if not os.path.isdir(src_dir):
        print(f"Source directory not found: {src_dir}")
        print("Nothing to pack.")
        return 0

    os.makedirs(dst_dir, exist_ok=True)

    json_files = sorted(
        f for f in os.listdir(src_dir) if f.endswith(".json")
    )

    if not json_files:
        print(f"No .json asset descriptors found in {src_dir}")
        print("Nothing to pack.")
        return 0

    print(f"Packing {len(json_files)} asset(s) from {src_dir} -> {dst_dir}")
    errors = 0
    for jf in json_files:
        json_path = os.path.join(src_dir, jf)
        if not pack_asset(json_path, src_dir, dst_dir):
            errors += 1

    if errors > 0:
        print(f"\nFinished with {errors} error(s).")
        return 1

    print(f"\nAll {len(json_files)} asset(s) packed successfully.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

