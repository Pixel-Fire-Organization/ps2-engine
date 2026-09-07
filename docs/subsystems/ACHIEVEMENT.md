# Subsystem — Achievement

## Purpose

Let a game say "the player has done this" once, in its own vocabulary, and have
that recorded wherever the platform records such things — without the game
knowing whether the platform has any such concept. Exactly one platform in this
engine does; every other must behave correctly anyway, and the game code must not
change between them.

## Contract

**Achievements are identified by enumerated value, never by name or index.**
Identifiers are generated from the same declaration the packaging step reads, so
the set the game can name and the set the platform knows are the same set by
construction. A game cannot unlock an achievement that was never declared, and a
declared achievement cannot be missing from the package.

**Unlocking is idempotent and one-way.** Unlocking an already-unlocked
achievement succeeds and changes nothing. There is no lock operation: the engine
does not take a player's achievement away, and a subsystem that could would
invite exactly that.

**Unlocking is a request, not a guarantee, and it reports.** It returns whether
the platform accepted it. A caller that ignores the result is not wrong — most
games should — but the subsystem never fails silently: a refusal is logged with
the reason.

**A refusal is logged once, not once per call.** The common refusal is
structural: the platform has no achievement support, or the player is missing a
prerequisite. That condition holds for the whole session and a game may unlock on
every frame that a condition is met, so repeating the message would drown the
log. The first refusal explains what to do; later ones are silent.

**Query is always safe.** Asking whether an achievement is unlocked answers
false rather than failing when there is no platform support, when the subsystem
is not loaded, or when the identifier is out of range.

**The platform is asked, not cached.** Unlock state lives with the platform,
which is what a player sees in the system interface. The subsystem holds no
authoritative copy, so state cannot drift from what the console reports.

**What a title declares and what a platform will record are separate facts.**
The count comes from what was packaged and does not drop to zero when recording
is unavailable. Collapsing the two into one number makes a title that declares
no achievements indistinguishable from a platform that refused the ones it has,
and those need opposite fixes -- one is a packaging mistake, the other is the
console. A refusal therefore also carries a reason, phrased for whoever can act
on it.

**Nothing is attempted when nothing was packaged.** A title that shipped no
achievement data does not start the platform service to discover that; it is
known before the engine runs. Starting it anyway costs a module load on every
boot and reports a console problem for what is a build configuration.

**A refusal is not automatically the player's problem.** The platform reports a
reason, and the reason distinguishes a console declining to record from a call
the service considered malformed. Presenting every failure as "install the
plugin" sent a real invalid-argument fault back to the player as an
installation problem, which no amount of reinstalling could resolve. A code the
service returns is quoted rather than interpreted.

## Depends on

- **Platform** — the achievement contract, which a platform supplies only if it
  has one. A platform without achievements offers nothing at all rather than an
  implementation that always fails; the difference is what lets the subsystem
  distinguish "not supported here" from "supported and refused".

Nothing else. In particular it does **not** depend on IO: the achievement store
is the platform's, reached through the platform, not a file this engine reads.

## Depended on by

- Game code, through the public game API.

Nothing in the engine depends on it, which is what makes it safely optional.

## Lifecycle

Started after the platform, on request. Start binds to the title achievement set
declared at packaging time; if the platform has no achievement support, start
succeeds and the subsystem runs in its unsupported state — that is not a failure,
because a game asking for achievements on a platform that has none is running
correctly.

There is no per-frame work. Shutdown releases whatever the platform opened.

## When not loaded

Every unlock returns false and every query returns false. The engine runs
normally, and so does the game: a title that unlocks achievements as it goes
plays identically with the subsystem absent, minus the recording. This is the
configuration a headless or automated host wants, and it is also exactly what
every platform without achievement support looks like — so the unsupported path
is exercised on every platform, every run, rather than being a rarely-taken
branch.

## Failure modes

- **Platform has no achievement support** — start succeeds, the subsystem reports
  itself unsupported, all unlocks return false. Logged once, naming the platform.
- **Player prerequisite missing** — where a platform needs software the player
  installs, its absence is discovered at start. Same treatment: start succeeds,
  unlocks return false, one log line saying what is missing and what to install.
  This is not an error condition; it is the normal state of an ordinary console.
- **Unknown identifier** — refused and logged, every time rather than once,
  because unlike the structural refusals this is a programming error and each
  occurrence is a distinct bug.
- **Declared set and packaged set disagree** — caught at build time by packaging
  validation, not at runtime. By the time the title runs, the two agree.
- **Platform refuses an individual unlock** — returned as false and logged with
  whatever reason the platform gave.

## Limits

- **No progress, no tiers, no counters.** An achievement is unlocked or it is
  not. Platforms that support incremental progress are not exposed through this.
- **No enumeration and no metadata.** A game cannot ask for names, descriptions
  or icons to build its own achievement screen. The platform owns presentation.
- **No synchronisation with any online service.** What that means in practice is
  a platform property and is documented per platform.
- **No unlock notification.** Whether the player sees something is the platform's
  business, and it is not reported back.
- **The set is fixed at build time.** There is no runtime registration.

## Off-the-shelf evaluation

None was considered, and the reason is worth recording. An achievement library
would exist to abstract across storefronts. This engine has exactly one platform
with achievements and no plans for a second storefront; the abstraction that
matters is already the platform contract. Every candidate in this space also
carries a network client and its own allocation, both of which are ruled out by
the engine's fixed budgets and its rule that anything reaching the OS goes behind
the platform contract.

The subsystem is therefore about fifty lines over a platform-supplied interface —
which is the correct size for what it does.
