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

> **This engine does not write this format.** The container is packed by TRPWork,
> vendored as a submodule, which owns the offsets, the payload alignment and the
> digest. The build supplies an entry table naming the payloads and the payloads
> themselves. A pre-built pack remains a first-class alternative. This spec exists
> so what TRPWork produces can be checked, not so it can be reimplemented.

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
| 0x04 | `version` | 4 | Format version; **`2`** for PS3 and Vita. `3` is PS4 |
| 0x08 | `fileSize` | 8 | Total size of the container, 64-bit |
| 0x10 | `entryCount` | 4 | Number of entries |
| 0x14 | `entrySize` | 4 | Size of one entry, `0x40`, which is also where the table starts |
| 0x18 | `devFlag` | 4 | Non-zero marks a development pack |
| 0x1C | `sha1` | 20 | Digest over the container, this field zeroed |
| 0x30 | padding | 16 | Zero, to `0x40` |

### Entry

| Offset | Field | Width | Meaning |
|---|---|---|---|
| 0x00 | `name` | 0x20 | NUL-padded file name, no directory component |
| 0x20 | `offset` | 8 | Payload start, from the beginning of the container |
| 0x28 | `size` | 8 | Payload length in bytes |
| 0x30 | `type` | 8 | Zero in observed packs |
| 0x38 | reserved | 8 | Zero |

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

The set icon is named `ICON0.PNG` inside the container whatever it is called on
disc, and is the set's own icon rather than one of the per-trophy ones. A set
that does not declare one is given the store-front icon, because the file is
required and an approximate icon is better than an absent one.

### The configuration files

Both are XML. `TROPCONF.SFM` is the set itself and `TROP.SFM` is the same set
with the display strings filled in; a reader may cross-check them, so **each
carries the communication identifier and the set version** rather than leaving
either to the other.

**Trophy identifiers are written unpadded** — `0`, not `000`. The console
matches them against the integer identifier it is asked to unlock, and a padded
identifier names a trophy that is not in the set. The failure this produces is
misleading: the set parses, a context opens against the communication
identifier, and every read and unlock afterwards reports that no set is
registered. The per-trophy icon *file names* are padded to three digits and are
a separate convention — `TROP000.PNG` carries trophy `0`.

`TROPCONF.SFM` also declares a parental level. It is the trophy set's own, taken
from the same declaration the package parameters use so the two cannot disagree.

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

## Payload alignment

Payloads start on **sixteen-byte boundaries**, padded with zeros. This is not
visible from the layout above — it shows up only when a pack is repacked and the
offsets move — and it is one of the reasons the container is TRPWork's job. A
contiguous pack parses correctly and may still be wrong on hardware.

## Verified by round-trip

A generated pack has been round-tripped through TRPWork: extracted to its five
payloads and an entry table, repacked, and compared. Both the original and the
repack carry a self-consistent digest, and the only difference is the alignment
above. That exercises magic, version, count, table offset, entry names, offsets
and sizes against an independent implementation — one that refuses a bad magic
and a wrong version rather than printing them.

## Contents this build does not produce

A pack produced here holds the two configuration files, the set icon and one
icon per trophy. Packs taken off a retail title are reported to also carry
**`TRPPARAM.INI`**, and group icons where a set uses groups.

Whether the console *requires* `TRPPARAM.INI` is unknown, and no trustworthy
description of it was found, so it is not written rather than guessed at. If a
console refuses a pack this build produced, this is the first thing to suspect,
and the way to settle it is to extract a genuine pack with the vendored tool and
look at what is in it.

Two fields a retail configuration carries are also absent, for the same reason.
A set with a platinum links each contributing trophy to it, and a configuration
carries the communication signature that authenticates the set. Neither is
written, because the linkage semantics are unverified and no signature can
honestly be produced for an identifier that was declared rather than allocated.

## Still unverified

**No genuine retail pack has been round-tripped**, only one this build produced.
Reading a real `TROPHY.TRP` and reproducing it byte-for-byte would confirm the
remaining unknowns: the meaning of the zero fields at `0x30` and `0x38` in each
entry, and the `devFlag`.

**Nothing here has run on hardware.** Acceptance by the console trophy service is
unproven, and needs the player-installed plugin described in
[../vita/PACKAGING.md](../vita/PACKAGING.md) regardless.
