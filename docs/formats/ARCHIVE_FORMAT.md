# Format — Game archive (`.PS2R`)

A flat single-file container: one header, a table of contents, a string table,
and the raw payloads. No folders, no per-file headers on disc.

Little-endian on every platform. This layout is a contract between the runtime
reader and the packaging stage of the build pipeline; both sides must change
together. The runtime behaviour built on it is specified in
[subsystems/ARCHIVE.md](../subsystems/ARCHIVE.md); how archives are produced is
in [PIPELINE.md](../PIPELINE.md).

## Layout

```
+-----------------------------+  offset 0
| header (32 B)               |  magic "PS2R", version, entryCount,
|                             |  stringsOffset, stringsSize, dataOffset
+-----------------------------+  offset 32
| TOC entry x entryCount      |  { nameHash, nameOffset, offset, size }, 16 B each
+-----------------------------+  stringsOffset
| string table                |  NUL-terminated canonical keys
+-----------------------------+  dataOffset (sector-aligned)
| payload 0                   |  each payload starts on a sector boundary
| (pad to next sector)        |
| payload 1                   |
| ...                         |
+-----------------------------+
```

| Field | Width | Meaning |
|---|---|---|
| `magic` | 4 | `"PS2R"` little-endian |
| `version` | 2 | format version; a reader refuses anything it does not know |
| `flags` | 2 | reserved, zero |
| `entryCount` | 4 | number of TOC entries |
| `stringsOffset` / `stringsSize` | 4 + 4 | string table location and extent |
| `dataOffset` | 4 | first payload byte, sector-aligned |

Each TOC entry holds a 32-bit FNV-1a hash of the canonical key for fast
rejection, plus an offset into the string table. **A hash hit is always confirmed
by string comparison**, so a collision cannot resolve to the wrong asset.

## Sector alignment

Payloads are aligned to a 2048-byte optical sector. A read therefore never
straddles an extra sector and every seek target lands on a sector boundary. The
cost is padding between payloads; the benefit is that on disc-based hardware the
drive does the minimum work per asset. The alignment value is part of the format
because the packer and the reader must agree on it.

## Canonical asset key

The packer and the runtime derive the same key for every asset, so a lookup
succeeds regardless of how the path was written:

| Input path | Canonical key |
|---|---|
| `cdrom0:/RASSETS/BOX.PS2A;1` | `RASSETS/BOX.PS2A` |
| `RASSETS\BOX.PS2A` (baked dependency) | `RASSETS/BOX.PS2A` |
| `mass0:/LEVELS/CITY.PS2R` | `LEVELS/CITY.PS2R` |

The rule: strip the device token up to the first colon, strip any `;N` version
suffix, convert backslashes to forward slashes, upper-case, and drop a leading
slash.

This must be implemented identically in the engine and in the packaging tool.
Canonicalisation is also what prevents the same asset occupying two resource
slots when it is loaded once by device path and once by baked dependency string.

## Constraints

- Payload order is chosen by the packer. Packing in access order is what makes
  locality pay off; the format does not enforce it.
- No compression. Payloads are stored exactly as cooked, so a read is a copy.
- No per-payload checksum. Corruption is detected at the asset level, by the
  format in [ASSET_FORMAT.md](ASSET_FORMAT.md), not here.
- Archives are read-only at runtime. Nothing appends to a mounted archive.
- The whole TOC and string table are read into memory on mount, so a very large
  entry count costs resident memory even when few assets are read.
