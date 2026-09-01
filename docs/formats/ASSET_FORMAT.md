# Format — Cooked asset (`.ps2a`)

One cooked asset: a fixed-size header followed by a payload. Produced by the cook
stage of the build pipeline, read by the resource subsystem.

Little-endian. The header size is fixed and is part of the contract — it is
computed from the maximum dependency count and the maximum path length, both of
which are format constants and **identical on every platform**. A platform that
chose its own path length would produce assets no other platform could parse.

Runtime behaviour is in [subsystems/RESOURCE.md](../subsystems/RESOURCE.md);
production is in [PIPELINE.md](../PIPELINE.md).

## Layout

```
+-----------------------------+  offset 0
| header (fixed size)         |
+-----------------------------+
| payload                     |  encoding depends on type
+-----------------------------+
```

| Field | Width | Meaning |
|---|---|---|
| `magic` | 4 | `"PS2A"` little-endian |
| `type` | 4 | asset category |
| `depCount` | 1 | number of declared dependencies |
| `reserved` | 3 | padding, zero |
| `ext` | 16 | source file extension, for diagnostics only |
| `deps` | maxDeps x maxPath | dependency keys, each NUL-terminated |
| `dataSize` | 4 | payload size in bytes |

## Asset types

| Type | Payload | Status |
|---|---|---|
| Texture | Platform texture encoding chosen by that platform cook list | Supported |
| Model | Baked geometry: separated, unindexed vertex arrays | Supported |
| Sound | — | **Not implemented on any platform** |
| Font | — | **Not implemented on any platform** |

Sound and font are enumerated but unimplemented engine-wide. A request for either
fails immediately rather than returning a handle that never becomes ready.

## Dependencies

An asset names the assets it needs. They are loaded first and reference-counted,
so an asset is never reported ready before everything it references is. This is
what makes one load request sufficient for a whole object graph, and it is why a
model does not need its textures listed by the caller.

Dependency paths are stored as canonical keys — see
[ARCHIVE_FORMAT.md](ARCHIVE_FORMAT.md) — so a baked dependency and a device path
resolve to the same asset.

## Platform-varying payloads

**The header is identical across platforms; the payload is not.** Texture
encoding is chosen per platform by that platform cook list, because the right
encoding is a hardware question: a platform with a small dedicated video memory
region wants a palettised format, while a platform with an ordinary GPU wants a
directly-uploadable one and no runtime expansion.

Consequently cooked assets and the archives built from them **differ between
platforms by design**, and comparing them across platforms proves nothing. A
per-platform comparison against a previous build of the same platform is the
meaningful check.

## Constraints

- Dependency count and path length are fixed by the format; exceeding either is a
  cook-time error.
- The header is a fixed size regardless of how many dependencies are used, so
  every asset pays for the maximum. This is deliberate: a fixed header means the
  payload offset is known without parsing.
- No checksum. A truncated payload is detected by size mismatch, not by content
  verification.
- `ext` is diagnostic only. Nothing dispatches on it; `type` is authoritative.
