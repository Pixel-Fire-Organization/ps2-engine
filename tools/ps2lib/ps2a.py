"""The .ps2a asset container: a fixed 2080-byte AssetFileHeader + payload.

The runtime resource manager (engine/src/EngineResource.cpp) reads this header to
find the asset type and its dependency keys. Byte-compatible with the historical
pack_assets output.
"""

import struct

MAGIC = 0x50533241  # "PS2A" little-endian
MAX_DEPS = 8
MAX_PATH_LEN = 256
EXT_LEN = 16  # matches AssetFileHeader.ext[16]

TYPE_MAP = {"TEXTURE": 0, "MODEL": 1, "SOUND": 2, "FONT": 3}

# 4 + 4 + 1 + 3 + 16 + (8 * 256) + 4 = 2080 bytes
HEADER_SIZE = 4 + 4 + 1 + 3 + EXT_LEN + (MAX_DEPS * MAX_PATH_LEN) + 4


def pack_dep_string(dep_str):
    encoded = dep_str.encode("utf-8")[:MAX_PATH_LEN - 1]
    return encoded + b"\x00" * (MAX_PATH_LEN - len(encoded))


def write_ps2a(type_id, payload, dep_keys, ext_str):
    """Build a .ps2a blob: header + payload. `dep_keys` are the full canonical
    dependency keys (e.g. "RASSETS/BOX.PS2A") in dependency order (max 8)."""
    dep_count = min(len(dep_keys), MAX_DEPS)

    ext_bytes = ext_str.encode("utf-8")[:EXT_LEN - 1]
    ext_field = ext_bytes + b"\x00" * (EXT_LEN - len(ext_bytes))

    header = struct.pack("<II", MAGIC, type_id)
    header += struct.pack("B", dep_count)
    header += b"\x00" * 3
    header += ext_field
    for i in range(MAX_DEPS):
        if i < dep_count:
            header += pack_dep_string(dep_keys[i])
        else:
            header += b"\x00" * MAX_PATH_LEN
    header += struct.pack("<I", len(payload))
    assert len(header) == HEADER_SIZE, f"Header size mismatch: {len(header)} != {HEADER_SIZE}"

    return header + payload
