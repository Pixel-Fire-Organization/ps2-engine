# CLAUDE.md

Claude Code loads this file automatically every session. Keep it short.

## Read first, always

**Read [.github/copilot-instructions.md](.github/copilot-instructions.md) at
the start of every session, before anything else — unconditionally, not just
for non-trivial changes.** It is the single source of truth for build system,
toolchain, submodule rules, memory/resource philosophy, constants standard, and
the "NEVER DO" list. Claude Code does not auto-load it the way it auto-loads
this file, so that read is on you, every session.

## Language-specific rules

Read before editing:
- C → [.github/instructions/c-expert.instructions.md](.github/instructions/c-expert.instructions.md)
- C++ → [.github/instructions/cpp-expert.instructions.md](.github/instructions/cpp-expert.instructions.md)

## Orientation

[README.md](README.md) for project overview; [docs/](docs/) for per-feature docs.

## Keeping instructions in sync

Conventions/build/rules belong in `copilot-instructions.md` — not here. Update
it, and the matching C/C++ instructions file for language-standard changes, in
the same change, every time. This file should almost never need edits.
