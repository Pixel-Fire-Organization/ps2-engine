#!/usr/bin/env python3
"""
pack_assets.py — PS2 Engine Asset Packer

Reads JSON + source file pairs from app/cd_files/RAYLIB/ and compiles them
into binary .ps2a files in app/cd_files/rassets/.

Usage:
    python3 scripts/pack_assets.py [--src <RAYLIB_DIR>] [--dst <RASSETS_DIR>] [--skip-convert]

      --skip-convert   Embed textures raw without QOI transcoding (not recommended for PS2)

JSON schema (e.g. player_tex.json):
    {
        "type": "TEXTURE",
        "source": "player_tex.jpg",
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
    char     ext[16]        — payload extension e.g. ".qoi", ".png"
    char     deps[8][256]   — null-terminated dependency asset paths
    uint32_t dataSize       — byte count of the raw payload

Note: TEXTURE payloads are always transcoded to QOI regardless of source
format. QOI uses a trivially simple decoder (no IDCT, no entropy coding)
that is reliable on the PS2 R5900 MIPS processor.
"""

import json
import os
import struct
import sys

MAGIC = 0x50533241  # "PS2A" little-endian
MAX_DEPS = 8
MAX_PATH_LEN = 256
EXT_LEN = 16  # matches AssetFileHeader.ext[16]

TYPE_MAP = {
    "TEXTURE": 0,
    "MODEL": 1,
    "SOUND": 2,
    "FONT": 3,
}

# Header size: 4 + 4 + 1 + 3 + 16 + (8 * 256) + 4 = 2080 bytes
HEADER_SIZE = 4 + 4 + 1 + 3 + EXT_LEN + (MAX_DEPS * MAX_PATH_LEN) + 4

# ---------------------------------------------------------------------------
# QOI encoder (pure Python, no external deps)
# Spec: https://qoiformat.org/
# ---------------------------------------------------------------------------
_QOI_MAGIC = b"qoif"
_QOI_END   = b"\x00\x00\x00\x00\x00\x00\x00\x01"

def _encode_qoi(width, height, channels, pixels):
    """
    Encode raw pixel bytes to QOI format.
      channels : 3 (RGB) or 4 (RGBA)
      pixels   : bytes of length width*height*channels (row-major, top-to-bottom)
    Returns bytes of the complete QOI file.
    """
    assert channels in (3, 4), f"channels must be 3 or 4, got {channels}"
    assert len(pixels) == width * height * channels, \
        f"pixel buffer size mismatch: {len(pixels)} != {width*height*channels}"

    out = bytearray()
    # Header
    out += _QOI_MAGIC
    out += struct.pack(">II", width, height)
    out += struct.pack("BB", channels, 0)   # colorspace = sRGB

    # Running 64-entry index table, all zero-initialised as (0,0,0,0)
    index = [[0, 0, 0, 0] for _ in range(64)]
    prev  = [0, 0, 0, 255]   # previous pixel (r,g,b,a)
    run   = 0
    n     = width * height

    for i in range(n):
        base = i * channels
        if channels == 4:
            px = [pixels[base], pixels[base+1], pixels[base+2], pixels[base+3]]
        else:
            px = [pixels[base], pixels[base+1], pixels[base+2], 255]

        is_last = (i == n - 1)

        if px == prev:
            run += 1
            if run == 62 or is_last:
                out.append(0xC0 | (run - 1))   # QOI_OP_RUN
                run = 0
        else:
            # Flush pending run
            if run > 0:
                out.append(0xC0 | (run - 1))
                run = 0

            h = (px[0]*3 + px[1]*5 + px[2]*7 + px[3]*11) % 64

            if index[h] == px:
                out.append(h & 0x3F)           # QOI_OP_INDEX
            else:
                index[h] = px[:]

                if px[3] != prev[3]:
                    # Alpha changed — QOI_OP_RGBA
                    out += bytes([0xFF, px[0], px[1], px[2], px[3]])
                else:
                    # Compute signed channel deltas
                    def sdelta(a, b):
                        d = (a - b) & 0xFF
                        return d if d < 128 else d - 256

                    dr = sdelta(px[0], prev[0])
                    dg = sdelta(px[1], prev[1])
                    db = sdelta(px[2], prev[2])

                    if -2 <= dr <= 1 and -2 <= dg <= 1 and -2 <= db <= 1:
                        # QOI_OP_DIFF
                        out.append(0x40 | ((dr+2)<<4) | ((dg+2)<<2) | (db+2))
                    else:
                        dr_dg = dr - dg
                        db_dg = db - dg
                        if (-32 <= dg <= 31 and -8 <= dr_dg <= 7 and -8 <= db_dg <= 7):
                            # QOI_OP_LUMA
                            out.append(0x80 | (dg + 32))
                            out.append(((dr_dg + 8) << 4) | (db_dg + 8))
                        else:
                            # QOI_OP_RGB
                            out += bytes([0xFE, px[0], px[1], px[2]])

        prev = px[:]

    out += _QOI_END
    return bytes(out)


def _convert_texture_to_qoi(source_path):
    """
    Decode any image format supported by Pillow and re-encode as QOI.
    Returns (qoi_bytes, ".qoi") or raises ImportError if Pillow is absent.
    """
    from PIL import Image
    img = Image.open(source_path).convert("RGBA")
    w, h = img.size
    pixels = img.tobytes()           # RGBA, row-major
    return _encode_qoi(w, h, 4, pixels), ".qoi"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def pack_dep_string(dep_str):
    """Encode a dependency path as a fixed-width 256-byte null-terminated field."""
    encoded = dep_str.encode("utf-8")[:MAX_PATH_LEN - 1]
    return encoded + b"\x00" * (MAX_PATH_LEN - len(encoded))


def pack_asset(json_path, src_dir, dst_dir, skip_convert=False):
    """Pack a single JSON + source pair into a .ps2a binary."""
    with open(json_path, "r", encoding="utf-8-sig") as f:
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
        # Warn and skip — descriptor may be added before the art is ready.
        print(f"  SKIP:  source file not found: {source_path}")
        print(f"         Place '{source_name}' in {src_dir} to pack this asset.")
        return True  # not a hard error; don't block the build

    if len(deps) > MAX_DEPS:
        print(f"  WARNING: truncating deps to {MAX_DEPS} for {json_path}")
        deps = deps[:MAX_DEPS]

    # --- Build payload ---
    # TEXTURE assets are always transcoded to QOI for reliable PS2 decode.
    # stb_image's JPEG decoder (used by raylib) can fail on progressive JPEGs
    # on the PS2 R5900 MIPS; qoi_decode is a trivially simple loop with no
    # integer-overflow-sensitive arithmetic.
    if asset_type_str == "TEXTURE":
        if skip_convert:
            print(f"  SKIP CONVERT: {source_name} (raw embed)")
            with open(source_path, "rb") as fh:
                payload = fh.read()
            _, raw_ext = os.path.splitext(source_name)
            ext_str = raw_ext.lower()
        else:
            try:
                payload, ext_str = _convert_texture_to_qoi(source_path)
                print(f"  CONV:  {source_name} -> QOI ({len(payload)} bytes)")
            except ImportError:
                print(f"  WARN:  Pillow not available; embedding {source_name} raw (may fail on PS2)")
                with open(source_path, "rb") as fh:
                    payload = fh.read()
                _, raw_ext = os.path.splitext(source_name)
                ext_str = raw_ext.lower()
    else:
        with open(source_path, "rb") as fh:
            payload = fh.read()
        _, raw_ext = os.path.splitext(source_name)
        ext_str = raw_ext.lower()

    # Build the header
    type_id   = TYPE_MAP[asset_type_str]
    dep_count = len(deps)

    ext_bytes = ext_str.encode("utf-8")[:EXT_LEN - 1]
    ext_field = ext_bytes + b"\x00" * (EXT_LEN - len(ext_bytes))

    header  = struct.pack("<II", MAGIC, type_id)
    header += struct.pack("B", dep_count)
    header += b"\x00" * 3  # reserved
    header += ext_field

    for i in range(MAX_DEPS):
        if i < dep_count:
            dep_path = f"cdrom0:\\RASSETS\\{deps[i].upper()}.PS2A;1"
            header += pack_dep_string(dep_path)
        else:
            header += b"\x00" * MAX_PATH_LEN

    header += struct.pack("<I", len(payload))

    assert len(header) == HEADER_SIZE, f"Header size mismatch: {len(header)} != {HEADER_SIZE}"

    # Write the .ps2a file
    base_name = os.path.splitext(os.path.basename(json_path))[0]
    out_path  = os.path.join(dst_dir, f"{base_name}.ps2a")

    with open(out_path, "wb") as fh:
        fh.write(header)
        fh.write(payload)

    print(f"  OK:   {base_name}.ps2a ({len(payload)} bytes payload, {dep_count} deps)")
    return True


def main():
    # Default paths relative to project root
    script_dir   = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)

    src_dir = os.path.join(project_root, "app", "cd_files", "RAYLIB")
    dst_dir = os.path.join(project_root, "app", "cd_files", "rassets")

    # Allow overrides via command-line
    args = sys.argv[1:]
    i = 0
    skip_convert = False
    while i < len(args):
        if args[i] == "--src" and i + 1 < len(args):
            src_dir = args[i + 1]
            i += 2
        elif args[i] == "--dst" and i + 1 < len(args):
            dst_dir = args[i + 1]
            i += 2
        elif args[i] == "--skip-convert":
            skip_convert = True
            i += 1
        else:
            i += 1

    if not os.path.isdir(src_dir):
        print(f"Source directory not found: {src_dir}")
        print("Nothing to pack.")
        return 0

    os.makedirs(dst_dir, exist_ok=True)

    json_files = sorted(
        f for f in os.listdir(src_dir) if f.lower().endswith(".json")
    )

    if not json_files:
        print(f"No .json asset descriptors found in {src_dir}")
        print("Nothing to pack.")
        return 0

    print(f"Packing {len(json_files)} asset(s) from {src_dir} -> {dst_dir}")
    errors = 0
    for jf in json_files:
        json_path = os.path.join(src_dir, jf)
        result = pack_asset(json_path, src_dir, dst_dir, skip_convert)
        if result is False:
            errors += 1

    if errors > 0:
        print(f"\nFinished with {errors} error(s).")
        return 1

    print(f"\nAll {len(json_files)} descriptor(s) processed (errors: {errors}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())

