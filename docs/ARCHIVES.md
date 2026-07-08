# Game Archives (.PS2R)

The engine reads assets from **flat single-file archives** instead of loose files
scattered at the disc root. One archive = one header + a table of contents (TOC)
+ a string table + the raw payloads, each payload aligned to a 2048-byte DVD
sector. No folders, no per-file headers on disc.

Why:

- **Fast loads** — one TOC lookup gives `{offset, size}`; the load is one seek +
  one read. No directory walk.
- **Locality** — payloads are packed in access order, so the DVD sled barely
  moves; sector alignment means a read never straddles an extra sector.
- **Fast level switch** — changing levels drops one open file descriptor and
  opens another (`Engine_Archive_Unmount` + `Engine_Archive_Mount`). No re-parse
  of a directory tree.

Resource duplication across archives is acceptable — a 4.7GB DVD has room, and
PS2 RAM (32MB) caps asset resolution anyway.

## On-disc format

Little-endian. Defined in [engine/include/EngineArchive.h](../engine/include/EngineArchive.h),
constants in [engine/include/Constants.ARCH.h](../engine/include/Constants.ARCH.h).

```
+-----------------------------+  offset 0
| ArchiveFileHeader (32 B)    |  magic "PS2R", version, entryCount,
|                             |  stringsOffset, stringsSize, dataOffset
+-----------------------------+  offset 32
| ArchiveTocEntry[entryCount] |  { nameHash, nameOffset, offset, size } x16 B
+-----------------------------+  stringsOffset
| string table                |  NUL-terminated canonical keys
+-----------------------------+  dataOffset (2048-aligned)
| payload 0                   |  each payload starts on a 2048-byte sector
| (pad to next sector)        |
| payload 1                   |
| ...                         |
+-----------------------------+
```

Each `ArchiveTocEntry` stores an FNV-1a-32 hash of the canonical key (fast
reject) plus a string-table offset; a hash hit is always confirmed with `strcmp`
so a collision can never silently resolve to the wrong asset.

## Canonical asset key

Both the packer and the runtime derive the same **canonical key** for every
asset, so a lookup succeeds regardless of how the path was written:

| Input path                       | Canonical key       |
|----------------------------------|---------------------|
| `cdrom0:/RASSETS/BOX.PS2A;1`      | `RASSETS/BOX.PS2A`  |
| `RASSETS\BOX.PS2A` (baked dep)    | `RASSETS/BOX.PS2A`  |
| `mass0:/LEVELS/CITY.PS2R`         | `LEVELS/CITY.PS2R`  |

Rule: strip the device token (up to the first `:`) and any `;N` version suffix,
convert `\` to `/`, upper-case, drop leading `/`. Implemented once as
`Engine_Path_Canonical` (engine/src/EngineCore.cpp) and mirrored by
`canonical_key` / `fnv1a32` in tools/pack_archive.py. Canonicalising the resource
key also fixes a latent duplicate-slot bug where the same asset loaded by device
path and by dependency string used to occupy two resource slots.

## Runtime

`Engine_Init` mounts the boot archive `RASSETS.PS2R` (slot 0) if present; a level
archive mounts into slot 1 and takes lookup priority (a level asset shadows a
boot asset of the same key). The archive is the single resolution point in the
**IO seam**:

- `Engine_IO_ReadAsync` worker: `Engine_Archive_Find(path)` hit → seek + read the
  span; miss → loose-file `fopen` fallback (host: dev builds and the transition).
- `Internal_PeekAssetType` (resource header peek): archive first, then a loose
  fallback now taken under the file-access semaphore.

No caller outside this seam changed. If no archive is mounted (or an asset is not
in it), everything falls back to loose files, so a build with only loose
`rassets/` still runs.

## Producing archives

`tools/pack_archive.py` (importable: `write_archive`, `read_toc`, `read_payload`):

```
python3 tools/pack_archive.py --dir game/cd_files/rassets --prefix RASSETS \
    --dst build/<preset>/iso_root/RASSETS.PS2R
```

The `generate-iso` target packs `cd_files/rassets/*.PS2A` into `RASSETS.PS2R` at
the disc root on every build. During the transition the loose `rassets/` tree is
still copied to the ISO as a fallback; it is dropped once streaming is archive-only.

Tests: `tools/tests/test_pack_archive.py` (round-trip, sector alignment, TOC
lookup, canonical-key / FNV parity) run in the Tools CI workflow.
