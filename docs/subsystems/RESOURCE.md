# Subsystem — Resource

## Purpose

Own the lifetime of everything loaded from an asset file: textures, models, and
their dependencies. Callers hold small integer handles rather than pointers, so
the subsystem can move, evict and reload the thing behind a handle without
invalidating anything the game is holding.

The cooked asset container format is specified in
[formats/ASSET_FORMAT.md](../formats/ASSET_FORMAT.md).

## Contract

**Handles, not pointers.** A load returns a handle immediately, before the data
exists. The handle is valid from that moment; the *data* is not. Callers ask
whether a handle is ready and fetch the underlying object only then. A fetch on a
handle that is not ready yields nothing rather than a partially-decoded object.

**A fixed handle table.** Capacity is a platform constant. There is no growth
path: an engine that silently doubles its table hides a content problem until it
fails on the smallest target.

**Asynchronous by default.** Every type streams through [IO](IO.md) and decodes
from memory. Nothing blocks the frame that requested it.

**Declared dependencies load first.** An asset names its dependencies in its own
header, and they are loaded before it is reported ready. A model is never
ready before the textures it references. This is what makes a single load request
sufficient for a whole object graph.

**Reference counting with pinning and LRU.** A dependency's reference count rises
with each dependent. When the table is full, the least recently used entry that
is unreferenced and unpinned is evicted. Pinning marks an entry as never
evictable, for content the game cannot tolerate losing mid-frame — HUD fonts,
persistent UI. Eviction never touches a referenced entry, so a live model cannot
lose its texture.

**Type may be declared or inferred.** A caller that knows the type states it; one
that does not — a level's required-resource list, which may hold anything — lets
the header decide. Both paths converge before decoding.

**Texture budget is accounted in bytes, and the platform defines the cost.** What
a texture occupies is hardware-specific: one platform rounds to page granularity
in a fixed video memory region, another simply consumes heap. The subsystem asks
the platform what a given texture costs and compares the total against a
platform-supplied ceiling. Byte accounting is what makes that comparison mean the
same thing everywhere.

**The table can be enumerated.** Every live slot, its key, type, state,
reference count, pinned flag and footprint are readable. A budget figure says
how much is spent; only the table says on what, which is the difference between
knowing a load was refused and knowing what to release.

## Depends on

- [IO](IO.md) — all asset reads.
- [Memory](MEMORY.md) — decoded payload storage and load contexts.
- [Renderer](RENDERER.md) — texture upload; a decoded texture is not usable until
  the active backend has accepted it.
- **Platform** — texture footprint accounting and the budget ceiling.

## Depended on by

- [Level](LEVEL.md) — level required-resource lists.
- [Sector](SECTOR.md) — sector materials.
- Game code, through the public game API.

## Lifecycle

Started after IO and Archive, since it needs both to resolve and read. It must be
updated once per frame to advance the frame counter that drives LRU recency;
without that update, eviction cannot distinguish recent from stale and degrades
to arbitrary choice. Shutdown force-unloads everything, pinned entries included.

## When not loaded

No asset can be loaded. A game in this configuration must generate its content
procedurally. Level and Sector both depend on Resource and cannot be requested
without it.

## Failure modes

- **Table full with nothing evictable** — load fails and returns an invalid
  handle, with a log naming what is pinned and what is referenced. The engine
  does not evict a referenced entry to make room; that would trade a clear
  failure for a corrupt frame.
- **Budget exceeded** — reported as an actionable error naming the asset and the
  overage. There is no silent downscaling: an engine that quietly halves a
  texture makes the eventual overflow harder to attribute than the error would
  have been.
- **Missing dependency** — the dependent fails to become ready and logs which
  dependency was missing and what referenced it.
- **Bad magic or unsupported version** — the load is refused rather than
  reinterpreted.
- **Unsupported type** — sound and font are not implemented on any platform; a
  request logs an error and fails immediately rather than returning a handle that
  will never become ready.

## Limits

- Handle table capacity, and the texture budget, are fixed per platform.
- Dependency count per asset is fixed by the asset format.
- Models decode from a baked, unindexed representation; there is no runtime mesh
  optimisation or index generation.
- Sound and font are unimplemented across the engine, not merely on one platform.
- Eviction is LRU over frames, not over bytes: evicting one large unused entry is
  not preferred to evicting several small ones.
