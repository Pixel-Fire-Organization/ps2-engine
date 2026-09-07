# Subsystem — Archive

## Purpose

Serve asset bytes out of a small number of large container files instead of many
loose files on disc. The container's on-disc layout is specified separately in
[formats/ARCHIVE_FORMAT.md](../formats/ARCHIVE_FORMAT.md); this document covers
the runtime read side only.

The motivation is mechanical, not organisational. On optical media a loose-file
layout costs a directory traversal and a long seek per asset. Packing assets in
access order into one container means one seek and one read per asset, with the
drive head barely moving between them, and a level switch becomes closing one
container and opening another rather than re-walking a directory tree.

## Contract

**Mounting.** A container is mounted from a device path and stays mounted until
dropped. Mounting reads the header, the entry table and the string table into
memory and keeps the file open for streaming; payload bytes are never preloaded.
Mounting is blocking and belongs on a load screen, not in a frame.

**Resolution.** Any form of an asset path — device path, baked dependency path,
canonical key — resolves to the same entry, because both the runtime and the
packing tool agree on one canonicalisation and one name hash. Hash matches are
confirmed by string comparison, so a collision cannot return the wrong asset.

**Shadowing.** When several containers are mounted, the most recently mounted
wins. This is what lets a level ship an asset that overrides one of the same name
in the boot container, with no indirection at the call site.

**Reads are span-bounded.** A read is expressed relative to a resolved entry and
is bounds-checked against it, so a corrupt offset cannot read another asset's
bytes or past the end of the container. Reads take the IO file-access lock and
are therefore safe to issue from the main thread while the IO worker is active.

**Duplication is accepted.** The same asset may exist in several containers.
Storage is cheap on the target media; a shared-asset index would add a lookup
layer and a failure mode to save space that is not scarce.

**A mount can be enumerated.** What is mounted, where it was mounted from, how
many entries it holds and what those entries are, are all readable without
reading a payload. Diagnostics need to show an archive that resolution alone
cannot describe: a lookup that misses says only that a key was not found, never
which keys the archive actually contains -- which is the question being asked
whenever a lookup misses.

## Depends on

- [IO](IO.md) — the file-access lock, and the platform file primitives reached
  through it.
- [Memory](MEMORY.md) — entry and string tables are platform allocations held for
  the mount's lifetime, released through the same contract that provided them.

## Depended on by

- [IO](IO.md) — for path-to-span resolution. See the note in the IO spec about
  this deliberate inversion.
- [Resource](RESOURCE.md) and [Level](LEVEL.md) — indirectly; neither addresses
  archives directly, which is the point.

## Lifecycle

Started after IO and before Resource. Startup allocates no containers; the engine
then mounts the boot container if one exists. A missing boot container is **not**
an error — assets fall back to loose files on disc, which is how an unpacked
development tree runs. Shutdown unmounts everything, closing files and releasing
tables.

## When not loaded

Every asset resolves as a loose file. This is a supported configuration: it is
how content is iterated during development, where repacking a container per
texture change would be intolerable. It is slower on disc-based hardware and is
not how a build ships.

## Failure modes

- **Missing container** — mount returns failure and the caller continues; for the
  boot container this is expected and logged as information, not an error.
- **Bad magic or unsupported version** — mount is refused and logged. The engine
  does not attempt partial recovery: a container it cannot parse may be
  truncated, and reading it would produce corruption attributed to the wrong
  asset.
- **No free mount slot** — mount fails; the slot count is a platform constant and
  exhausting it is a content-structure error.
- **Out-of-range span read** — refused and logged rather than clamped.

## Limits

- Mount slots are fixed per platform.
- Containers are read-only at runtime; producing one is a build-pipeline stage,
  described in [PIPELINE.md](../PIPELINE.md).
- Entry tables live in memory for the whole mount, so container size is bounded
  by table size, not payload size.
- There is no compression. Payloads are stored as cooked, so a read is a copy
  rather than a decode.
