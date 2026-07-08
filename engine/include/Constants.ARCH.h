#pragma once

// Game archive (.PS2R): a single flat container — a fixed header, a TOC of
// per-file {offset,size} entries, and a string table, followed by the raw
// payloads. No folders, no per-file headers on disc.
//
// Why: fast loads (one seek + one read per asset via the TOC), disc locality
// (assets are packed in access order so the DVD sled barely moves), and fast
// level switch (drop one open file descriptor, open another — no re-parse of a
// directory tree). Resource duplication across archives is fine on a 4.7GB DVD.

#define ARCH_FILE_MAGIC 0x52325350u // "PS2R" little-endian
#define ARCH_FILE_VERSION 1u

// Mounted archive slots. Slot 0 is the always-resident boot archive (RASSETS);
// slot 1 is the current level archive, dropped/switched on level change. Later
// (higher-index) mounts take lookup priority so a level asset shadows a boot one.
#define ARCH_MAX_MOUNTED 2

// Payloads are aligned to a DVD sector so a read never straddles an extra sector
// and every seek target lands on a sector boundary (drive locality).
#define ARCH_SECTOR_ALIGN 2048u

#define ARCH_FILE_EXT ".PS2R"

// Boot archive base name, mounted at Engine_Init against the active device token.
#define ARCH_BOOT_ARCHIVE_NAME "RASSETS.PS2R"
