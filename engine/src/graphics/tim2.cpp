#include "../include/graphics/tim2.h"

#include <cstring>

#include "EngineDebug.h"

namespace
{
// Alignment-safe little-endian reads (the blob comes straight off disc and its
// multi-byte fields are not guaranteed to sit on aligned addresses).
uint16_t ReadU16(const uint8_t* p)
{
    uint16_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

uint32_t ReadU32(const uint8_t* p)
{
    uint32_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

// File header layout (16 bytes):
//   [0..3]  magic "TIM2"   [4] formatVersion   [5] formatId
//   [6..7]  pictureCount   [8..15] padding
constexpr uint32_t TIM2_MAGIC = 0x324D4954u; // 'T','I','M','2' little-endian
constexpr size_t TIM2_FILE_HEADER_SIZE = 16;

// Picture header layout (48 bytes / 0x30) — see tim2.h. We only read what we
// need: sizes, dimensions and the image color type.
constexpr size_t TIM2_PIC_HEADER_SIZE = 0x30;

// TIM2 imageType values we accept.
constexpr uint8_t TIM2_IMGTYPE_RGBA16 = 0x01; // A1B5G5R5
constexpr uint8_t TIM2_IMGTYPE_RGBA32 = 0x03; // A8B8G8R8
} // namespace

bool Tim2_Parse(const void* data, size_t size, Tim2Image* out)
{
    if (!data || !out || size < TIM2_FILE_HEADER_SIZE)
    {
        Engine_LogError("TIM2: null/short blob (%zu bytes).", size);
        return false;
    }

    const uint8_t* base = static_cast<const uint8_t*>(data);

    if (ReadU32(base) != TIM2_MAGIC)
    {
        Engine_LogError("TIM2: bad magic.");
        return false;
    }

    const uint8_t formatId = base[5];
    const uint16_t pictureCount = ReadU16(base + 6);
    if (pictureCount == 0)
    {
        Engine_LogError("TIM2: zero pictures.");
        return false;
    }

    // pack_assets.py always writes formatId 0 (16-byte alignment → picture header
    // immediately after the 16-byte file header). formatId 1 aligns to 128.
    const size_t picBase = (formatId == 0) ? TIM2_FILE_HEADER_SIZE : 128;
    if (size < picBase + TIM2_PIC_HEADER_SIZE)
    {
        Engine_LogError("TIM2: truncated picture header.");
        return false;
    }

    const uint8_t* pic = base + picBase;
    const uint32_t clutSize = ReadU32(pic + 4);
    const uint32_t imageSize = ReadU32(pic + 8);
    const uint16_t headerSize = ReadU16(pic + 12);
    const uint8_t mipmapCount = pic[17];
    const uint8_t imageType = pic[19];
    const uint16_t imageWidth = ReadU16(pic + 20);
    const uint16_t imageHeight = ReadU16(pic + 22);

    if (clutSize != 0)
    {
        Engine_LogError("TIM2: paletted images are not supported (clutSize=%u).", clutSize);
        return false;
    }
    if (mipmapCount > 1)
    {
        Engine_LogError("TIM2: mipmaps are not supported (mipmapCount=%u).", mipmapCount);
        return false;
    }

    PixelFormat fmt;
    switch (imageType)
    {
    case TIM2_IMGTYPE_RGBA32:
        fmt = PixelFormat::RGBA32;
        break;
    case TIM2_IMGTYPE_RGBA16:
        fmt = PixelFormat::RGBA16;
        break;
    default:
        Engine_LogError("TIM2: unsupported imageType 0x%02X.", imageType);
        return false;
    }

    // Image data starts headerSize bytes into the picture block.
    const size_t imgOffset = picBase + headerSize;
    if (headerSize < TIM2_PIC_HEADER_SIZE || imgOffset + imageSize > size)
    {
        Engine_LogError("TIM2: image payload out of bounds (hdr=%u, size=%u).", headerSize, imageSize);
        return false;
    }

    out->pixels = base + imgOffset;
    out->width = imageWidth;
    out->height = imageHeight;
    out->format = fmt;
    out->imageSize = imageSize;
    return true;
}
