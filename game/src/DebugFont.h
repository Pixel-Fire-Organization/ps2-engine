#pragma once

// ---------------------------------------------------------------------------
// DebugFont — a tiny 5x7 bitmap font drawn with the engine's only on-screen 2D
// primitive, game::DrawRect (screen-space colored quads). There is no text
// rendering in the engine, so this game-side font gives readable labels for the
// scene selector (and any future on-screen debug text) with zero renderer work.
//
// Each glyph row is run-length-encoded into horizontal spans, so a string emits
// ~4 rects per glyph row instead of one per lit pixel (keeps well under the 2D
// rect cap). Supported glyphs: A-Z, 0-9, space, and > : - . (others draw blank).
// ---------------------------------------------------------------------------

namespace font
{
    // Pixel size of one 5x7 cell "dot" in screen pixels. A glyph is 5*scale wide
    // and 7*scale tall; glyphs advance 6*scale (5 + 1 gap).
    constexpr int GLYPH_W = 5;
    constexpr int GLYPH_H = 7;
    constexpr int GLYPH_ADVANCE = 6;

    // Draw `text` at screen (x, y), each dot `scale` px, in colour (r,g,b) 0..255.
    // Lower-case is upper-cased. Returns the x position just past the string.
    int DrawText(int x, int y, int scale, const char* text, int r, int g, int b);

    // Pixel width a string would occupy at the given scale (for centering).
    int TextWidth(int scale, const char* text);
} // namespace font
