# Format — Trophy pack (`TROPHY.TRP`)

A flat single-file container: one header, a table of entries, and the raw
payloads. It carries the trophy configuration and the trophy icons that the
console trophy service reads.

**Big-endian**, unlike every other format in this project — it is a Sony console
format inherited from the PlayStation 3, not one this engine designed. Reading it
with the host byte order produces plausible-looking nonsense, so byte order is
the first thing to check when a pack will not parse.

This layout is a contract between `tools/vita_package.py` and the console trophy
service. Runtime behaviour built on it is in
[subsystems/ACHIEVEMENT.md](../subsystems/ACHIEVEMENT.md); how packs are produced
and installed is in [vita/PACKAGING.md](../vita/PACKAGING.md).

> **This engine does not have to write this format.** The packaging config
> accepts a pre-built pack as a first-class alternative, and that path involves no
> knowledge of the layout below. The generator exists as a convenience; the spec
> exists so it can be checked.

## Layout

```
+-----------------------------+  offset 0x00
| header (0x40 B)             |  magic, version, file size, entry count,
|                             |  entry size, dev flag, SHA-1
+-----------------------------+  offset 0x40
| entry x entryCount          |  { name[0x24], offset, size, reserved }, 0x40 B each
+-----------------------------+  first entry offset
| payload 0                   |  in entry order
| payload 1                   |
| ...                         |
+-----------------------------+
```

### Header

| Offset | Field | Width | Meaning |
|---|---|---|---|
| 0x00 | `magic` | 4 | Container identifier, `0xDCA24D00` |
| 0x04 | `version` | 4 | Format version; `3` for the packs this targets |
| 0x08 | `fileSize` | 8 | Total size of the container, 64-bit |
| 0x10 | `entryCount` | 4 | Number of entries |
| 0x14 | `entrySize` | 4 | Size of one entry: `0x40` |
| 0x18 | `devFlag` | 4 | Non-zero marks a development pack |
| 0x1C | `sha1` | 20 | Digest over the container, this field zeroed |
| 0x30 | padding | 16 | Zero, to `0x40` |

### Entry

| Offset | Field | Width | Meaning |
|---|---|---|---|
| 0x00 | `name` | 0x24 | NUL-padded file name, no directory component |
| 0x24 | `offset` | 4 | Payload start, from the beginning of the container |
| 0x28 | reserved | 4 | Zero — the high half of a 64-bit offset |
| 0x2C | `size` | 4 | Payload length in bytes |
| 0x30 | reserved | 16 | Zero |

`entrySize` is `0x40`, and the header is also `0x40`, so the entry table begins
immediately after the header. **These two numbers being equal is a coincidence,
not a rule** — one is the size of the header and the other is the size of one
entry. Third-party tools conflate them and work by luck; a reader should take the
table as starting at `0x40` and stride by `entrySize`.

## Contents

| Name | Required | Meaning |
|---|---|---|
| `TROPCONF.SFM` | Yes | The trophy set: identifiers, grades, hidden flags |
| `TROP.SFM` | Yes | Localised names and descriptions |
| `ICON0.PNG` | Yes | The trophy-set icon |
| `TROP000.PNG` … | One per trophy | Per-trophy icons, numbered to match identifiers |

Trophy identifiers are dense and start at zero. A gap is a packaging error, not a
runtime condition — nothing at runtime can recover from an icon that is not
there.

## The magic value

`0xDCA24D00`, big-endian like the rest of the header. This is settled, and the
generator writes it without being told:

- A reader that **rejects** anything else — TRPWork reads the first four bytes
  big-endian and raises `Bad TRP Magic` on a mismatch. A reader that merely
  printed the field, as the extractors this document was first written against
  did, establishes nothing; one that refuses on a mismatch does.
- The PS3 and PS4 developer wikis document the same value for the same container,
  which is consistent with this being a format inherited rather than designed.

The value is a fact about the format, not a component: nothing third-party is
vendored or linked to obtain it, and the generator here remains the project's
own. Overriding it is possible for a reader that wants something else, but there
is no known reason to.

## Still unverified

**The exact meaning of the reserved fields**, which are consistently zero in
observed packs but are not documented as reserved anywhere authoritative.

**No genuine pack has been round-tripped.** Reading a real `TROPHY.TRP`,
regenerating it byte-for-byte and comparing would confirm every offset and size
here at once — the digest can only match if the whole layout is right — and that
has not been done. A generated pack has been checked for internal consistency
(header, entry table, digest) but not against hardware, so acceptance by the
console trophy service is still unproven.
