"""TIM2 texture encoding (PS2-native). rgba32 / pal8, optional box-filtered mips.

Byte-compatible with the historical pack_assets encoders (golden-tested via
BOX.PS2A). `encode_pal8_cutout` is a level-pipeline addition: it reserves CLUT
index 0 as fully transparent for billboard-impostor cutouts.
"""

import struct

TIM2_IMGTYPE_RGBA16 = 0x01
TIM2_IMGTYPE_RGBA32 = 0x03
TIM2_IMGTYPE_IDTEX8 = 0x05


def mip_level_count(w, h, requested):
    """Total mip levels (>=1) for `requested` extra levels, clamped so the
    smallest level stays >= 8x8."""
    levels = 1
    while levels <= requested:
        if (w >> levels) < 8 or (h >> levels) < 8:
            break
        levels += 1
    return levels


def assemble_tim2(width, height, level_payloads, image_type, clut_bytes):
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


def encode_rgba32(img, mip_levels):
    """RGBA32 (A8B8G8R8) TIM2, box-filtered mip chain."""
    from PIL import Image
    w, h = img.size
    levels = mip_level_count(w, h, mip_levels)
    payloads = []
    for l in range(levels):
        lw, lh = max(1, w >> l), max(1, h >> l)
        lvl = img if l == 0 else img.resize((lw, lh), Image.BOX)
        payloads.append(lvl.tobytes())
    return assemble_tim2(w, h, payloads, TIM2_IMGTYPE_RGBA32, None)


def encode_pal8(img, mip_levels):
    """8-bit indexed (IDTEX8) TIM2 with a linear 256-entry A8B8G8R8 CLUT. Level 0
    is quantized; lower mips are box-filtered then remapped to the same palette
    so one CLUT serves the whole chain. Alpha is not preserved (set opaque)."""
    from PIL import Image
    w, h = img.size
    rgb = img.convert("RGB")
    pal_img = rgb.quantize(colors=256, method=Image.Quantize.FASTOCTREE)

    levels = mip_level_count(w, h, mip_levels)
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
    return assemble_tim2(w, h, payloads, TIM2_IMGTYPE_IDTEX8, bytes(clut))


def encode_pal8_cutout(img):
    """IDTEX8 TIM2 whose CLUT index 0 is fully transparent (alpha 0). Used for
    billboard-impostor atlases: fully-transparent source pixels map to index 0,
    opaque pixels are quantized into indices 1..255. No mips (impostors are drawn
    at roughly one screen size). CLUT alpha is 0x80 (GS 1.0) for opaque entries."""
    from PIL import Image
    w, h = img.size
    rgba = img.convert("RGBA")
    px = rgba.load()

    opaque_rgb = rgba.convert("RGB")
    # Quantize the RGB into 255 colours (indices become 0..254); shift to 1..255
    # so index 0 is free for transparency.
    pal_img = opaque_rgb.quantize(colors=255, method=Image.Quantize.FASTOCTREE)
    pal_idx = pal_img.load()

    indices = bytearray(w * h)
    for y in range(h):
        for x in range(w):
            if px[x, y][3] < 128:
                indices[y * w + x] = 0  # transparent
            else:
                indices[y * w + x] = pal_idx[x, y] + 1

    palette = pal_img.getpalette() or []
    clut = bytearray()
    clut += struct.pack("<BBBB", 0, 0, 0, 0)  # index 0 = transparent
    for i in range(255):
        r = palette[i * 3 + 0] if i * 3 + 2 < len(palette) else 0
        g = palette[i * 3 + 1] if i * 3 + 2 < len(palette) else 0
        b = palette[i * 3 + 2] if i * 3 + 2 < len(palette) else 0
        clut += struct.pack("<BBBB", r, g, b, 0x80)
    return assemble_tim2(w, h, [bytes(indices)], TIM2_IMGTYPE_IDTEX8, bytes(clut))


def convert_texture_to_tim2(source_path, fmt="rgba32", mip_levels=0):
    """Decode any Pillow-supported image and re-encode as TIM2. `fmt` is
    'rgba32' (default) or 'pal8'; `mip_levels` is the number of extra mip levels.
    Returns (tim2_bytes, ".tm2") or raises ImportError if Pillow is absent."""
    from PIL import Image
    img = Image.open(source_path).convert("RGBA")
    if fmt == "pal8":
        return encode_pal8(img, mip_levels), ".tm2"
    return encode_rgba32(img, mip_levels), ".tm2"
