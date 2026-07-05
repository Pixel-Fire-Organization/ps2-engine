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
BAKED_MODEL_VERSION = 2

BAKED_TOPOLOGY_LIST = 0
BAKED_TOPOLOGY_STRIP = 1

# Above this verts/triangle ratio the strip is worse than just paying for the
# extra draw-call overhead, so bake an (unstripped) triangle list instead.
STRIP_MAX_VERTS_PER_TRI = 2.5


# ---------------------------------------------------------------------------
# TIM2 texture encoder (single picture; RGBA32 / RGBA16 / PAL8, optional mips)
# ---------------------------------------------------------------------------
# imageType codes (low byte of the picture header).
TIM2_IMGTYPE_RGBA16 = 0x01
TIM2_IMGTYPE_RGBA32 = 0x03
TIM2_IMGTYPE_IDTEX8 = 0x05


def _mip_level_count(w, h, requested):
    """Total mip levels (>=1) for `requested` extra levels, clamped so the
    smallest level stays >= 8x8."""
    levels = 1
    while levels <= requested:
        if (w >> levels) < 8 or (h >> levels) < 8:
            break
        levels += 1
    return levels


def _assemble_tim2(width, height, level_payloads, image_type, clut_bytes):
    """Assemble a single-picture TIM2 from per-level pixel payloads (largest
    first, each padded to 16 bytes) and an optional linear CLUT."""
    image = bytearray()
    for lp in level_payloads:
        image += lp
        image += b"\x00" * ((-len(image)) % 16)  # pad each level to 16 bytes
    image_size = len(image)
    clut_size = len(clut_bytes) if clut_bytes else 0
    clut_colors = (clut_size // 4) if clut_bytes else 0
    mip_count = len(level_payloads)
    clut_type = 0x03 if clut_bytes else 0  # A8B8G8R8 CLUT, CSM1, stored linear

    total_size = 0x30 + image_size + clut_size
    pic = struct.pack("<III", total_size, clut_size, image_size)  # totalSize, clutSize, imageSize
    pic += struct.pack("<HH", 0x30, clut_colors)                  # headerSize, clutColors
    pic += struct.pack("<BBBB", 0, mip_count, clut_type, image_type)  # pictFormat, mipmapCount, clutType, imageType
    pic += struct.pack("<HH", width, height)                      # imageWidth, imageHeight
    pic += struct.pack("<QQ", 0, 0)                               # GsTex0, GsTex1
    pic += struct.pack("<II", 0, 0)                               # GsRegs, GsTexClut
    assert len(pic) == 0x30, len(pic)

    # File header (16 bytes): magic, formatVersion=4, formatId=0, pictureCount=1, pad[8]
    fh = b"TIM2" + struct.pack("<BBH", 0x04, 0x00, 1) + (b"\x00" * 8)
    assert len(fh) == 16, len(fh)

    return fh + pic + bytes(image) + (clut_bytes if clut_bytes else b"")


def _encode_rgba32(img, mip_levels):
    """RGBA32 (A8B8G8R8) TIM2, box-filtered mip chain."""
    from PIL import Image
    w, h = img.size
    levels = _mip_level_count(w, h, mip_levels)
    payloads = []
    for l in range(levels):
        lw, lh = max(1, w >> l), max(1, h >> l)
        lvl = img if l == 0 else img.resize((lw, lh), Image.BOX)
        payloads.append(lvl.tobytes())
    return _assemble_tim2(w, h, payloads, TIM2_IMGTYPE_RGBA32, None)


def _encode_pal8(img, mip_levels):
    """8-bit indexed (IDTEX8) TIM2 with a linear 256-entry A8B8G8R8 CLUT. Level 0
    is quantized; lower mips are box-filtered then remapped to the same palette
    so one CLUT serves the whole chain. Alpha is not preserved (set opaque)."""
    from PIL import Image
    w, h = img.size
    rgb = img.convert("RGB")
    pal_img = rgb.quantize(colors=256, method=Image.Quantize.FASTOCTREE)

    levels = _mip_level_count(w, h, mip_levels)
    payloads = []
    for l in range(levels):
        lw, lh = max(1, w >> l), max(1, h >> l)
        idx_img = pal_img if l == 0 else rgb.resize((lw, lh), Image.BOX).quantize(palette=pal_img, dither=Image.Dither.NONE)
        payloads.append(idx_img.tobytes())

    # CLUT: palette RGB triples -> linear A8B8G8R8 (alpha 0x80 == GS 1.0), 256 entries.
    palette = pal_img.getpalette() or []
    clut = bytearray()
    for i in range(256):
        r = palette[i * 3 + 0] if i * 3 + 2 < len(palette) else 0
        g = palette[i * 3 + 1] if i * 3 + 2 < len(palette) else 0
        b = palette[i * 3 + 2] if i * 3 + 2 < len(palette) else 0
        clut += struct.pack("<BBBB", r, g, b, 0x80)
    return _assemble_tim2(w, h, payloads, TIM2_IMGTYPE_IDTEX8, bytes(clut))


def _convert_texture_to_tim2(source_path, fmt="rgba32", mip_levels=0):
    """Decode any Pillow-supported image and re-encode as TIM2. `fmt` is
    'rgba32' (default) or 'pal8'; `mip_levels` is the number of extra mip levels.
    Returns (tim2_bytes, ".tm2") or raises ImportError if Pillow is absent."""
    from PIL import Image
    img = Image.open(source_path).convert("RGBA")
    if fmt == "pal8":
        return _encode_pal8(img, mip_levels), ".tm2"
    return _encode_rgba32(img, mip_levels), ".tm2"


# ---------------------------------------------------------------------------
# Baked model encoder (.obj -> BKM2 v2: vec4 positions, strip-or-list + bounds)
# ---------------------------------------------------------------------------
def _align16(n):
    return (n + 15) & ~15


def _bounding_sphere(positions):
    """AABB-midpoint center + exact max-distance radius over (x,y,z) tuples."""
    if not positions:
        return (0.0, 0.0, 0.0), 0.0
    xs = [p[0] for p in positions]
    ys = [p[1] for p in positions]
    zs = [p[2] for p in positions]
    cx = (min(xs) + max(xs)) * 0.5
    cy = (min(ys) + max(ys)) * 0.5
    cz = (min(zs) + max(zs)) * 0.5
    r2 = 0.0
    for (x, y, z) in positions:
        d2 = (x - cx) ** 2 + (y - cy) ** 2 + (z - cz) ** 2
        if d2 > r2:
            r2 = d2
    return (cx, cy, cz), r2 ** 0.5


def _dedup_corners(out_v, out_n, out_t):
    """Collapse identical (pos, normal, uv) triangle corners to unique vertices.
    Returns (unique_v, unique_n, unique_t, triangles) with triangles as index
    triples into the unique arrays."""
    unique = {}
    uv, un, ut = [], [], []
    indices = []
    for i in range(len(out_v)):
        key = (out_v[i], out_n[i], out_t[i])
        idx = unique.get(key)
        if idx is None:
            idx = len(uv)
            unique[key] = idx
            uv.append(out_v[i])
            un.append(out_n[i])
            ut.append(out_t[i])
        indices.append(idx)
    triangles = [(indices[k], indices[k + 1], indices[k + 2]) for k in range(0, len(indices), 3)]
    return uv, un, ut, triangles


def _stripify(triangles):
    """Greedy triangle-strip builder. Returns a list of strips (lists of vertex
    indices) whose GL_TRIANGLE_STRIP decode reproduces `triangles` with winding."""
    from collections import defaultdict
    edge_tris = defaultdict(list)
    for idx, (a, b, c) in enumerate(triangles):
        edge_tris[frozenset((a, b))].append(idx)
        edge_tris[frozenset((b, c))].append(idx)
        edge_tris[frozenset((c, a))].append(idx)

    used = [False] * len(triangles)

    def third(tri, p, q):
        for v in triangles[tri]:
            if v != p and v != q:
                return v
        return None

    def cyclic_match(tri, target):
        a, b, c = triangles[tri]
        return target in ((a, b, c), (b, c, a), (c, a, b))

    def neighbor(p, q, exclude):
        for ti in edge_tris[frozenset((p, q))]:
            if not used[ti] and ti != exclude:
                return ti
        return None

    strips = []
    for start in range(len(triangles)):
        if used[start]:
            continue
        a, b, c = triangles[start]
        strip = [a, b, c]
        used[start] = True
        prev = start
        while True:
            i = len(strip) - 2  # strip index of the triangle about to be added
            p, q = strip[-2], strip[-1]
            nb = neighbor(p, q, prev)
            if nb is None:
                break
            r = third(nb, p, q)
            if r is None:
                break
            required = (p, q, r) if (i % 2 == 0) else (q, p, r)
            if not cyclic_match(nb, required):
                break
            strip.append(r)
            used[nb] = True
            prev = nb
        strips.append(strip)
    return strips


def _stitch_strips(strips):
    """Concatenate strips into one using degenerate triangles, keeping each
    sub-strip's first triangle on an even index (GL winding rule)."""
    combined = list(strips[0])
    for s in strips[1:]:
        if not s:
            continue
        combined.append(combined[-1])  # duplicate current tail (degenerate)
        combined.append(s[0])          # duplicate next head (degenerate)
        if len(combined) % 2 != 0:
            combined.append(s[0])      # extra degenerate fixes winding parity
        combined.extend(s)
    return combined


def _canon_tri(t):
    """Rotate a triangle so its smallest index is first (winding preserved)."""
    a, b, c = t
    if a <= b and a <= c:
        return (a, b, c)
    if b <= a and b <= c:
        return (b, c, a)
    return (c, a, b)


def _decode_strip(strip):
    """Decode a GL_TRIANGLE_STRIP index list to triangles, dropping degenerates."""
    tris = []
    for i in range(len(strip) - 2):
        t = (strip[i], strip[i + 1], strip[i + 2]) if (i % 2 == 0) else (strip[i + 1], strip[i], strip[i + 2])
        if t[0] == t[1] or t[1] == t[2] or t[0] == t[2]:
            continue
        tris.append(t)
    return tris


def _verify_strip(strip, triangles):
    """True if the stitched strip decodes back to exactly the source triangles."""
    from collections import Counter
    want = Counter(_canon_tri(t) for t in triangles)
    got = Counter(_canon_tri(t) for t in _decode_strip(strip))
    return want == got


def _parse_obj(source_path):
    """Parse a .obj into per-corner (pos, normal, uv) arrays, fan-triangulated."""
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
    return out_v, out_n, out_t


def _bake_obj_model(source_path, has_texture):
    """Parse a .obj and bake it as BKM2 v2: a degenerate-stitched triangle strip
    when stripification is a win, else an unindexed triangle list. Positions are
    written as vec4 (x, y, z, 1) — a 16-byte stride for VU0 / VIF / glVertexPointer."""
    out_v, out_n, out_t = _parse_obj(source_path)
    count = len(out_v)
    if count == 0 or count % 3 != 0:
        raise ValueError(f"{source_path}: no triangles parsed ({count} verts)")
    tri_count = count // 3

    # Try to stripify: dedup corners, build strips, stitch, verify.
    uv, un, ut, triangles = _dedup_corners(out_v, out_n, out_t)
    topology = BAKED_TOPOLOGY_LIST
    emit_v, emit_n, emit_t = out_v, out_n, out_t  # list fallback (per-corner)

    try:
        strips = _stripify(triangles)
        combined = _stitch_strips(strips)
        ratio = len(combined) / float(tri_count)
        if ratio <= STRIP_MAX_VERTS_PER_TRI and _verify_strip(combined, triangles):
            topology = BAKED_TOPOLOGY_STRIP
            emit_v = [uv[i] for i in combined]
            emit_n = [un[i] for i in combined]
            emit_t = [ut[i] for i in combined]
            print(f"    strip: {tri_count} tris -> {len(combined)} strip verts "
                  f"({ratio:.2f} v/tri, {len(strips)} runs)")
        else:
            print(f"    list:  {tri_count} tris (strip {ratio:.2f} v/tri rejected)")
    except Exception as e:  # noqa: BLE001 — any strip failure falls back to a list
        print(f"    list:  {tri_count} tris (stripify failed: {e})")

    vert_count = len(emit_v)
    (cx, cy, cz), radius = _bounding_sphere(emit_v)

    # Positions as vec4 (x, y, z, 1); normals vec3; uvs vec2.
    vbytes = b"".join(struct.pack("<ffff", p[0], p[1], p[2], 1.0) for p in emit_v)
    nbytes = b"".join(struct.pack("<fff", *n) for n in emit_n)
    tbytes = b"".join(struct.pack("<ff", *t) for t in emit_t)

    mat_count = 1 if has_texture else 0
    header_size, mesh_size, mat_size = 16, 48, 8 * mat_count
    verts_off = _align16(header_size + mesh_size + mat_size)
    norms_off = _align16(verts_off + len(vbytes))
    uvs_off = _align16(norms_off + len(nbytes))
    total = uvs_off + len(tbytes)

    buf = bytearray(total)
    struct.pack_into("<IIII", buf, 0, BAKED_MODEL_MAGIC, BAKED_MODEL_VERSION, 1, mat_count)
    # BakedMeshEntry v2 (48 bytes): counts/offsets, topology, bounds, reserved[2].
    struct.pack_into("<IIIII", buf, 16, vert_count, 0, verts_off, norms_off, uvs_off)
    struct.pack_into("<I", buf, 36, topology)
    struct.pack_into("<ffff", buf, 40, cx, cy, cz, radius)
    struct.pack_into("<II", buf, 56, 0, 0)  # reserved[2]
    if mat_count:
        struct.pack_into("<II", buf, 64, 0, 0)  # diffuseTexRef = dependency 0
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
        tex_fmt = str(meta.get("format", "rgba32")).lower()
        mip_levels = int(meta.get("mipmaps", 0))
        if tex_fmt not in ("rgba32", "pal8"):
            print(f"  WARNING: unknown texture format '{tex_fmt}', using rgba32 for {source_name}")
            tex_fmt = "rgba32"
        try:
            payload, ext_str = _convert_texture_to_tim2(source_path, tex_fmt, mip_levels)
            print(f"  CONV:  {source_name} -> TIM2 {tex_fmt} mips={mip_levels} ({len(payload)} bytes)")
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
