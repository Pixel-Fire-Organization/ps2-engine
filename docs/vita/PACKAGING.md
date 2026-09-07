# Vita Packaging

The Vita deliverable is a `.vpk` — a container holding the signed executable, the
title metadata, the store-front presentation, the asset archive and the compiled
worlds. One is produced per variant, into `dist/vita/` and `dist/vitatv/`.

Unlike the PS2 disc, a Vita package carries **per-title metadata**: a title
identifier, a display name, an icon, a store-front layout and optionally a trophy
set. None of that is a build setting, so none of it lives in CMake. It is declared
in `game/platform/vita/package.json`, validated against
`game/platform/package.schema.json`, and consumed by `tools/vita_package.py`.

---

## Why the config lives under `game/`

The cook list under `engine/platform/<name>/` answers a hardware question — how
this machine wants textures baked. A title identifier and an icon answer a
question about **the game**, and would be wrong to inherit from the engine. A
second game built on this engine ships its own identity while sharing the cook
list unchanged.

This is a new convention, and it is deliberately narrow: `game/platform/<name>/`
holds title metadata for one platform, nothing else.

---

## Why the toolchain macros are not used

The VitaSDK ships `vita_create_self` and `vita_create_vpk`, and they are the
documented path. This project calls the underlying tools directly instead, for one
concrete reason:

> Both macros accumulate their arguments into **cache** variables and append on
> every call. A configure that builds two variants calls each macro twice, and the
> second call inherits the first variant title identifier and its entire file
> list.

Both also attach their targets to `ALL`, while every executable here is
`EXCLUDE_FROM_ALL` and driven through the `dist` target.

So the platform fragment invokes `vita-elf-create`, `vita-make-fself`,
`vita-mksfoex` and `vita-pack-vpk` itself, exactly as the PS2 fragment invokes
`mkisofs` itself. Each invocation gets its own argument list and nothing is shared
between variants. This costs roughly twenty lines and buys a two-variant build
that is correct.

---

## `package.json`

| Field | Meaning |
|---|---|
| `title.id` | Nine characters, `XXXXYYYYY`. Four author characters, five digits. Also names the writable data directory |
| `title.name` | The display name under the bubble |
| `title.version` | `##.##` |
| `self.safe` | Restricts the executable to the ordinary user API. Clear it only if the title genuinely needs more |
| `self.compress` | Compress the executable inside the package |
| `self.extended_memory` | Request the larger main-memory allowance. Off by default |
| `sfo.category` | `gd` for an application |
| `sfo.parental_level` | 0–11 |
| `sfo.extra` | Raw metadata key-values, passed through unchanged |
| `livearea.*` | Paths to the presentation images, and the layout style |
| `trophies.*` | See [Trophies](#trophies) below |
| `files[]` | Extra `{src, dst}` pairs staged into the package |
| `variants.vitatv` | A deep-merge override applied only to the set-top build |

`variants` exists so the set-top build can carry its own display name and metadata
without duplicating the whole file. Anything not overridden is inherited.

---

## LiveArea assets

These sizes are **exact**. The console rejects the install with error
`0x8010113D` when one is wrong, and that error names nothing — which is why
validation runs first and fails with the file and the expected size instead.

| File | Size | Colour | Alpha |
|---|---|---|---|
| `sce_sys/icon0.png` | 128 x 128 | 8-bit indexed | **Not allowed** |
| `sce_sys/pic0.png` | 960 x 544 | 256-colour indexed | Not allowed |
| `sce_sys/livearea/contents/bg0.png` | 840 x 500 | 8-bit indexed | Not allowed |
| `sce_sys/livearea/contents/startup.png` | 280 x 158 | 8-bit indexed | **Allowed** |
| `sce_sys/livearea/contents/template.xml` | | | Layout |

`pic0.png` is optional. The others are required when a store-front is declared.

`startup.png` is the only layer that may be transparent. Supplying an RGB image
where an indexed one is required is the most common failure, and it is silent
until install.

`template.xml` is generated from `livearea.style` when the config does not supply
one. Two styles are supported: `a1` centres the startup image, `psmobile`
right-aligns it. A hand-written template is used verbatim.

---

## Trophies

**Trophies on unsigned software require a plugin the player installs**, because
the console verifies two signatures this project cannot produce: one over the
title communication identifier, and one over the trophy pack. The `NoTrpDrm`
plugin disables both checks for the running title.

Consequences, all of which belong in front of anyone enabling this:

- A player without the plugin sees a title that runs normally and awards nothing.
- Unlocks are recorded **locally** and are visible in the console trophy
  application. They never reach the online service and never appear on a profile.
- The communication identifier is declared, not allocated. It must match the one
  baked into the trophy pack.

Two ways to supply the pack, and both are first-class:

- **`trophies.trp`** — a path to a pack built elsewhere. Staged as-is.
- **`trophies.list`** — trophy definitions, from which the build generates the
  configuration files, the pack, and a header of trophy identifiers for the game
  to use so no trophy number is ever written by hand.

The generated header lands in the build tree, the same way the entity definitions
are generated. Runtime behaviour is [ACHIEVEMENT.md](../subsystems/ACHIEVEMENT.md);
the container layout is [TROPHY_PACK.md](../formats/TROPHY_PACK.md).

**A pack is generated automatically** when `trophies.enabled` is set: the build
writes the configuration files, the container and the identifier header, and
stages the container at `sce_sys/trophy/TROPHY.TRP`. `VITA_TRP_MAGIC` overrides
the container identifier and is not normally needed; see
[../formats/TROPHY_PACK.md](../formats/TROPHY_PACK.md).

**A trophy set is registered when the title is installed, not when it runs.**
There is no call to register one: the module exports fifteen functions and none
of them registers a set, which both the firmware NID database and an independent
reimplementation agree on. The system takes delivery of the pack out of the
package.

The console says so when it has not. Errors `0x80551610` and `0x80551611` are
"not registered" and "already registered"; a code in that group coming back from
reading trophy state means the console recognises the identifier and holds no set
behind it. Every later call is refused against nothing. The remedy is to delete
the title and install it again **with the trophy plugin already active**, because
registration happens once, at install, and an unsigned title needs the plugin
present at that moment.

**A title that ships no trophy data declares none.** The count the engine
reports is what was packaged, so it does not drop to zero merely because a
console refused, and a packaging mistake stays distinguishable from a missing
plugin. Where trophies are packaged but the console refuses them, the engine
shows the player a dismissible notice at boot; see
[../subsystems/DEBUG.md](../subsystems/DEBUG.md).

---

## Package layout

```
eboot.bin                                  the executable
sce_sys/param.sfo                          title metadata
sce_sys/icon0.png                          bubble icon
sce_sys/pic0.png                           optional
sce_sys/livearea/contents/bg0.png
sce_sys/livearea/contents/startup.png
sce_sys/livearea/contents/template.xml
sce_sys/trophy/TROPHY.TRP                  when trophies are enabled
RASSETS.PS2R                               the cooked asset archive
LEVELS/                                    compiled worlds
```

The archive and the worlds are inside the package rather than beside it, because
the application mount is the only read-only root the title has and a distribution
must be installable as one file.

---

## Build order

Packaging is gated on validation, so a bad config cannot reach a container — the
same rule the cooked-asset pipeline follows.

1. **Validate** the config, every referenced file, and every image size.
2. **Emit** the generated pieces: the layout template if absent, the trophy
   configuration and pack, the trophy identifier header, and the argument lists
   the fragment needs.
3. **Convert** the linked executable to the console executable format.
4. **Generate** the title metadata from the config.
5. **Pack** everything, plus the asset archive and worlds, into the `.vpk`.

Steps 3 to 5 run once per variant with independent argument lists.

---

## Adding a file to the package

Add a `{src, dst}` pair to `files[]` in the config. Do not add it to the CMake
fragment — the fragment deliberately names no content, so that what ships is
described in one readable place.
