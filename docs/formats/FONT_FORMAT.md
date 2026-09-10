# Format — Cooked font (`.ps2a`, font payload)

A font's metrics: how large its glyphs are, where each one sits in its atlas,
and how far the pen moves after drawing it. **The atlas itself is not in this
payload.** It is an ordinary cooked texture, named as this asset's dependency.

The enclosing container is [ASSET_FORMAT.md](ASSET_FORMAT.md); the atlas payload
is [TIM2_TEXTURE.md](TIM2_TEXTURE.md); the consumer is
[subsystems/UI.md](../subsystems/UI.md).

Little-endian throughout.

## Why the atlas is a dependency rather than part of the payload

An atlas is a texture, and the pipeline already knows how to bake, budget,
validate, upload, reference-count and release a texture. Embedding the pixels
here would mean a second implementation of every one of those, differing subtly
from the first.

Declaring it as a dependency reuses all of it: dependencies are loaded before
the asset that names them and are reference-counted, so a font is never reported
ready before its atlas is; the atlas is charged against the platform's texture
budget and refused with the ordinary actionable message if it does not fit; and
the existing validation and inspection tools understand it without change.

It also puts the platform variation where the pipeline already expects it.
**This payload is identical on every platform** — the first cooked payload that
is — because glyph metrics are not a hardware question. Everything that varies
by hardware varies in the atlas.

## Layout

```
+-----------------------------+
| font header (24 bytes)      |
+-----------------------------+
| glyph table                 |  glyphCount entries, 12 bytes each
+-----------------------------+
```

### Font header

| Offset | Field | Width | Meaning |
|---|---|---|---|
| 0 | `magic` | 4 | `"PSFN"` |
| 4 | `version` | 2 | layout version; a mismatch is refused, never migrated |
| 6 | `flags` | 2 | bit 0 set when every advance is equal |
| 8 | `atlasWidth` | 2 | texels; must match the atlas dependency |
| 10 | `atlasHeight` | 2 | texels; must match the atlas dependency |
| 12 | `lineHeight` | 2 | pixels from one baseline to the next |
| 14 | `baseline` | 2 | pixels from the top of the line box to the baseline |
| 16 | `spaceAdvance` | 2 | pixels the pen moves for a space |
| 18 | `firstCode` | 2 | codepoint of the first glyph in the table |
| 20 | `glyphCount` | 2 | entries following |
| 22 | `missingIndex` | 2 | glyph drawn for a codepoint outside the table |

### Glyph entry

| Offset | Field | Width | Meaning |
|---|---|---|---|
| 0 | `u` | 2 | atlas column of the glyph's left edge, in texels |
| 2 | `v` | 2 | atlas row of the glyph's top edge, in texels |
| 4 | `w` | 1 | glyph width in texels |
| 5 | `h` | 1 | glyph height in texels |
| 6 | `bearingX` | 1, signed | pixels from the pen to the glyph's left edge |
| 7 | `bearingY` | 1, signed | pixels from the top of the line box to the glyph's top edge |
| 8 | `advance` | 1 | pixels the pen moves after this glyph |
| 9 | `reserved` | 3 | zero |

Twelve bytes rather than ten so that entries stay four-byte aligned. The
constrained platform loads the two-byte fields directly out of the mapped
payload, and an unaligned load there is a fault, not a slow path.

## The codepoint range is dense

Glyphs cover a contiguous run of codepoints starting at `firstCode`, and a
codepoint is looked up by subtraction. There is no sparse table and no search.

This is a deliberate trade: the interface's own contract limits coverage to
printable ASCII, so a dense table is small, and a lookup that is one subtraction
and one bounds check costs nothing per glyph on the slowest platform. A sparse
table would buy coverage the interface does not offer, at the price of a search
per glyph in the hottest loop the interface has.

A codepoint outside the range draws the glyph at `missingIndex` — visibly wrong
rather than invisibly absent, so missing coverage is found while authoring
rather than reported by a player.

## The atlas

The atlas dependency carries **coverage only**: how much of each texel the glyph
covers, and nothing about its colour. Colour comes from the drawing call, which
is what lets one font serve every colour role and every theme.

**The atlas also reserves one fully-opaque texel**, and the header records where
it is. Solid fills are drawn from that texel rather than untextured, so a screen
of panels and text is a single run of one texture instead of alternating between
textured and untextured state once per row. On the platform that binds textures
rather than batching them, that is the difference between one bind per frame and
one per row.

How that coverage is encoded is a platform question, answered by the platform's
cook list:

- Where video memory is scarce, an **indexed** atlas whose colour table is a
  ramp from transparent to opaque white. One byte per texel, and the ramp gives
  antialiased edges for free.
- Elsewhere, a **32-bit** atlas that is white throughout with coverage in the
  alpha channel.

Both are the same image. The distinction is where the coverage is stored, not
what it means.

**The atlas must be power-of-two on every platform**, not only on the one whose
hardware requires it. A font cooked for a permissive platform would otherwise be
silently unusable on the strict one, and the cook stage is where that should be
found.

**The atlas must be sampled without interpolation**, which it requests through
the filter field of its own payload. A pixel font magnified to an integer scale
is exact; the same font linearly filtered is blurred at every scale.

## Constraints

- `atlasWidth` and `atlasHeight` must equal the dependency's actual dimensions.
  They are duplicated here so that metrics can be validated without decoding an
  image, and a disagreement is a cook-time error.
- Every glyph must lie entirely within the atlas.
- An advance of zero is refused. It is almost always an authoring mistake, and
  it produces a line of glyphs stacked on one another.
- **No kerning table.** Advances are per glyph.
- **No colour, no outline, no shadow, no signed-distance encoding.** A shadow is
  drawn by the caller as a second, offset draw.
- **No checksum**, consistent with every other cooked payload: a truncated file
  is caught by size mismatch. The metrics are parsed rather than mapped straight
  into live state, so parsing catches malformed content as a side effect. The
  theme payload, which is *not* parsed, carries a checksum for exactly that
  reason — see [THEME_FORMAT.md](THEME_FORMAT.md).
