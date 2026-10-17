# Branch Strategy

Zephyrus is split by game into two long-lived branches: they share the same
history (up to `e929ae0`), and evolve independently from there, neither
containing the other's game code.

| Branch | Game | Status |
|---|---|---|
| `csgo` | CS:GO (legacy linux64) | **Main development branch** (default branch) |
| `acr` | ACR | **Frozen**, archive only, no further development |

- Each branch only contains the bindings, features, overlay and assets of its
  own game; the shared layer (hook, RmlUi, config, logging, tests) is maintained
  separately on both branches.
- The code baseline of `acr` is the last pure-ACR commit `e929ae0`; the freeze
  note and this branch-policy update sit on top of it, and the tag `acr-final`
  points at the archived tip. It does not receive shared-layer changes made
  during the CS:GO era; when ACR development resumes in the future, the
  shared-layer changes from `csgo` must be ported over first.
- New features, fixes and dependency upgrades always go to `csgo`. Do not start
  new work on `acr`.
- Build and test commands for the CS:GO branch are in `AGENTS.md`; the script
  index is in `scripts/README.md`.

## Remote

`origin` uses SSH: `git@github.com:OtonashiHorobu/zephyrus.git`. The remote
default branch is `csgo`; `acr` and the tag `acr-final` are kept as archives,
and the old `main` has been deleted.
