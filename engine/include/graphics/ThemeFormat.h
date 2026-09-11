#pragma once

#include <cstddef>
#include <cstdint>

// --- PSTH on-disc format (written by tools/ps2lib/theme.py) ---------------
// FORMAT CONSTANTS - mirrored by the cooker, identical on every platform.
#define THEME_MAGIC 0x48545350 /* "PSTH" in little-endian */
#define THEME_VERSION 2
#define THEME_HEADER_SIZE 16

// The style block is copied into live engine state rather than parsed, so its
// size is part of the contract and the build asserts the compiled struct
// against this value. See docs/formats/THEME_FORMAT.md.
//
// Version 2 added UiColor::TextDisabled and three metrics (menuBarHeight,
// caretWidth, iconSpacing), spending the format's last two reserved slots plus
// four bytes of colour growth. A version-1 file is refused, never migrated.
#define THEME_STYLE_BYTES 108

// A font reference: which text role, and the asset key that serves it.
#define THEME_KEY_MAX 48
#define THEME_FONT_REF_SIZE (1 + THEME_KEY_MAX)
#define THEME_MAX_FONT_REFS 8

/// Which text role a theme's font reference serves.
enum class UiFontRole : uint8_t
{
    Body = 0,
    Header,
    Value,

    Count
};
