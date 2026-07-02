#!/usr/bin/env python3
"""
pack_assets.py — PS2 Engine Asset Packer

Reads JSON + source file pairs from app/cd_files/ASSETS/ and compiles them into
binary .ps2a files in app/cd_files/rassets/.

Usage:
    python3 scripts/pack_assets.py [--src <ASSETS_DIR>] [--dst <RASSETS_DIR>]

JSON schema (e.g. player_tex.json):
    { "type": "TEXTURE", "source": "player_tex.png", "deps": [] }
    { "type": "MODEL",   "source": "prop.obj",       "deps": ["prop_tex"] }

Binary .ps2a layout:
    [AssetFileHeader]  (fixed-size, 2080 bytes)
    [payload]          (TIM2 for textures, baked BKM2 blob for models)

Payload encodings (raylib was removed; runtime decodes are trivial):
  TEXTURE -> TIM2 (PS2-native). Pillow decodes the source to RGBA8888 and it is
             re-wrapped as a single-picture, non-paletted 32-bit (A8B8G8R8) TIM2.
             Runtime just parses the header and DMAs the pixels to GS VRAM.
  MODEL   -> BKM2 (baked). An .obj is parsed into separated, UNINDEXED
             vertex/normal/uv arrays (exactly what ps2gl needs and the GIFTAG
             builder consumes). The first dependency (if any) is the diffuse texture.
  FONT / SOUND -> unsupported (dropped with raylib); skipped with a warning.
"""

import json
import os
import struct
import sys

MAGIC = 0x50533241  # "PS2A" little-endian
MAX_DEPS = 8
MAX_PATH_LEN = 256
EXT_LEN = 16  # matches AssetFileHeader.ext[16]

TYPE_MAP = {"TEXTURE": 0, "MODEL": 1, "SOUND": 2, "FONT": 3}

# Header size: 4 + 4 + 1 + 3 + 16 + (8 * 256) + 4 = 2080 bytes
HEADER_SIZE = 4 + 4 + 1 + 3 + EXT_LEN + (MAX_DEPS * MAX_PATH_LEN) + 4

BAKED_MODEL_MAGIC = 0x324D4B42  # "BKM2"
BAKED_MODEL_VERSION = 1


# ---------------------------------------------------------------------------
# TIM2 texture encoder (single picture, 32-bit A8B8G8R8, no CLUT)
# ---------------------------------------------------------------------------
def _encode_tim2_rgba32(width, height, pixels_rgba):
    """pixels_rgba: bytes of length w*h*4 in R,G,B,A order (matches A8B8G8R8 in
    little-endian memory, and GL_RGBA/GL_UNSIGNED_BYTE for the ps2gl path)."""
    image_size = len(pixels_rgba)

    # Picture header (0x30 bytes). GsTex0/GsTex1/GsRegs/GsTexClut are left zero —
    # the renderer computes TBP0/TBW from the allocated GS address at upload time.
    pic = struct.pack("<III", 0x30 + image_size, 0, image_size)  # totalSize, clutSize, imageSize
    pic += struct.pack("<HH", 0x30, 0)                            # headerSize, clutColors
    pic += struct.pack("<BBBB", 0, 1, 0, 0x03)                    # pictFormat, mipmapCount, clutType, imageType(A8B8G8R8)
    pic += struct.pack("<HH", width, height)                      # imageWidth, imageHeight
    pic += struct.pack("<QQ", 0, 0)                               # GsTex0, GsTex1
    pic += struct.pack("<II", 0, 0)                               # GsRegs, GsTexClut
    assert len(pic) == 0x30, len(pic)

    # File header (16 bytes): magic, formatVersion=4, formatId=0, pictureCount=1, pad[8]
    fh = b"TIM2" + struct.pack("<BBH", 0x04, 0x00, 1) + (b"\x00" * 8)
    assert len(fh) == 16, len(fh)

    return fh + pic + pixels_rgba


def _convert_texture_to_tim2(source_path):
    """Decode any Pillow-supported image and re-encode as 32-bit TIM2.
    Returns (tim2_bytes, ".tm2") or raises ImportError if Pillow is absent."""
    from PIL import Image
    img = Image.open(source_path).convert("RGBA")
    w, h = img.size
    return _encode_tim2_rgba32(w, h, img.tobytes()), ".tm2"


# ---------------------------------------------------------------------------
# Baked model encoder (.obj -> BKM2 separated unindexed arrays)
# ---------------------------------------------------------------------------
def _align16(n):
    return (n + 15) & ~15


def _bake_obj_model(source_path, has_texture):
    """Parse a triangulated-or-convex .obj into separated unindexed float arrays."""
    positions, normals, uvs = [], [], []
    out_v, out_n, out_t = [], [], []

    def resolve(tok, count):
        i = int(tok)
        return i - 1 if i > 0 else count + i  # 1-based, or negative from end

    with open(source_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            tag = p[0]
            if tag == "v":
                positions.append((float(p[1]), float(p[2]), float(p[3])))
            elif tag == "vn":
                normals.append((float(p[1]), float(p[2]), float(p[3])))
            elif tag == "vt":
                uvs.append((float(p[1]), float(p[2]) if len(p) > 2 else 0.0))
            elif tag == "f":
                face = []
                for v in p[1:]:
                    s = v.split("/")
                    vi = resolve(s[0], len(positions))
                    ti = resolve(s[1], len(uvs)) if len(s) > 1 and s[1] else -1
                    ni = resolve(s[2], len(normals)) if len(s) > 2 and s[2] else -1
                    face.append((vi, ti, ni))
                # Fan-triangulate.
                for k in range(1, len(face) - 1):
                    for (vi, ti, ni) in (face[0], face[k], face[k + 1]):
                        out_v.append(positions[vi])
                        out_n.append(normals[ni] if 0 <= ni < len(normals) else (0.0, 0.0, 0.0))
                        uv = uvs[ti] if 0 <= ti < len(uvs) else (0.0, 0.0)
                        out_t.append((uv[0], 1.0 - uv[1]))  # flip V for GS texel origin

    count = len(out_v)
    if count == 0 or count % 3 != 0:
        raise ValueError(f"{source_path}: no triangles parsed ({count} verts)")

    vbytes = b"".join(struct.pack("<fff", *p) for p in out_v)
    nbytes = b"".join(struct.pack("<fff", *n) for n in out_n)
    tbytes = b"".join(struct.pack("<ff", *t) for t in out_t)

    mat_count = 1 if has_texture else 0
    header_size, mesh_size, mat_size = 16, 24, 8 * mat_count
    verts_off = _align16(header_size + mesh_size + mat_size)
    norms_off = _align16(verts_off + len(vbytes))
    uvs_off = _align16(norms_off + len(nbytes))
    total = uvs_off + len(tbytes)

    buf = bytearray(total)
    struct.pack_into("<IIII", buf, 0, BAKED_MODEL_MAGIC, BAKED_MODEL_VERSION, 1, mat_count)
    struct.pack_into("<IIIIII", buf, 16, count, 0, verts_off, norms_off, uvs_off, 0)
    if mat_count:
        struct.pack_into("<II", buf, 40, 0, 0)  # diffuseTexRef = dependency 0
    buf[verts_off:verts_off + len(vbytes)] = vbytes
    buf[norms_off:norms_off + len(nbytes)] = nbytes
    buf[uvs_off:uvs_off + len(tbytes)] = tbytes
    return bytes(buf), ".bkm"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def pack_dep_string(dep_str):
    encoded = dep_str.encode("utf-8")[:MAX_PATH_LEN - 1]
    return encoded + b"\x00" * (MAX_PATH_LEN - len(encoded))


def pack_asset(json_path, src_dir, dst_dir):
    with open(json_path, "r", encoding="utf-8-sig") as f:
        meta = json.load(f)

    asset_type_str = meta.get("type", "").upper()
    source_name = meta.get("source", "")
    deps = meta.get("deps", [])

    if asset_type_str not in TYPE_MAP:
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

    if len(deps) > MAX_DEPS:
        print(f"  WARNING: truncating deps to {MAX_DEPS} for {json_path}")
        deps = deps[:MAX_DEPS]

    # --- Build payload ---
    if asset_type_str == "TEXTURE":
        try:
            payload, ext_str = _convert_texture_to_tim2(source_path)
            print(f"  CONV:  {source_name} -> TIM2 ({len(payload)} bytes)")
        except ImportError:
            print(f"  ERROR: Pillow required to bake TIM2 textures; cannot pack {source_name}")
            return False
    elif asset_type_str == "MODEL":
        if not source_name.lower().endswith(".obj"):
            print(f"  ERROR: MODEL baking supports .obj only (got '{source_name}')")
            return False
        try:
            payload, ext_str = _bake_obj_model(source_path, has_texture=len(deps) > 0)
            print(f"  BAKE:  {source_name} -> BKM2 ({len(payload)} bytes)")
        except Exception as e:  # noqa: BLE001 — surface any parse failure
            print(f"  ERROR: model bake failed for {source_name}: {e}")
            return False
    else:
        return False

    # --- Build header ---
    type_id = TYPE_MAP[asset_type_str]
    dep_count = len(deps)

    ext_bytes = ext_str.encode("utf-8")[:EXT_LEN - 1]
    ext_field = ext_bytes + b"\x00" * (EXT_LEN - len(ext_bytes))

    header = struct.pack("<II", MAGIC, type_id)
    header += struct.pack("B", dep_count)
    header += b"\x00" * 3
    header += ext_field
    for i in range(MAX_DEPS):
        if i < dep_count:
            header += pack_dep_string(f"RASSETS/{deps[i].upper()}.PS2A")
        else:
            header += b"\x00" * MAX_PATH_LEN
    header += struct.pack("<I", len(payload))
    assert len(header) == HEADER_SIZE, f"Header size mismatch: {len(header)} != {HEADER_SIZE}"

    base_name = os.path.splitext(os.path.basename(json_path))[0]
    out_path = os.path.join(dst_dir, f"{base_name}.PS2A")
    with open(out_path, "wb") as fh:
        fh.write(header)
        fh.write(payload)

    print(f"  OK:   {base_name}.PS2A ({len(payload)} bytes payload, {dep_count} deps)")
    return True


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)

    src_dir = os.path.join(project_root, "app", "cd_files", "ASSETS")
    dst_dir = os.path.join(project_root, "app", "cd_files", "rassets")

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
