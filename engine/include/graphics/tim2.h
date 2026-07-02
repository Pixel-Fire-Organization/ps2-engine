#pragma once
#include <cstddef>
#include <cstdint>

#include "Types.h"

// ---------------------------------------------------------------------------
// TIM2 (.tm2) — PlayStation 2 native texture container.
//
// Only what pack_assets.py bakes is supported: a single-picture, non-paletted
// image in 32-bit A8B8G8R8 (imageType 0x03 → PixelFormat::RGBA32) or 16-bit
// A1B5G5R5 (imageType 0x01 → PixelFormat::RGBA16). CLUT / paletted / mipmapped
// / multi-picture TIM2 files are rejected.
//
// The parser is zero-copy: `pixels` points INTO the supplied blob, which must
// remain valid for as long as the pixels are used (i.e. until UploadTexture
// has copied them to GS VRAM).
// ---------------------------------------------------------------------------

struct Tim2Image
{
    const void* pixels; // pointer into the blob at the image payload
    int width;
    int height;
    PixelFormat format;
    uint32_t imageSize; // bytes of pixel payload
};

// Parse a TIM2 blob resident in main RAM. Returns true on success.
bool Tim2_Parse(const void* data, size_t size, Tim2Image* out);
