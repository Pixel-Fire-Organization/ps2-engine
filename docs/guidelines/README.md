# Guidelines

Process documents. Read the relevant one **before** starting, not while
reviewing.

| Adding | Read |
|---|---|
| An engine subsystem — memory, io, archive, resource, level, sector, input, debug, renderer | [NEW_SYSTEM.md](NEW_SYSTEM.md) |
| A build target — a console, a desktop OS, a variant of either | [NEW_PLATFORM.md](NEW_PLATFORM.md) |

Both follow the same four steps, for the same reason:

1. **Spec first.** Write the contract before the code. It is cheaper to move a
   dependency line than a dependency.
2. **Decompose the spec** into components and tasks, each traceable to a line of
   the spec. Anything with no spec line behind it is scope creep or a gap.
3. **Evaluate off-the-shelf components**, and record the decision — including
   the decision to write your own. This engine's constraints rule most libraries
   out; say which one applied.
4. **Implement per platform first, then engine-wide.** Anything built against a
   single platform encodes that platform's assumptions into its contract, and
   they are expensive to remove later.

Each guideline ends with a definition of done. Work it — the checklists exist
because every item on them has been missed at least once.
