# scripts

| Script | Purpose |
|---|---|
| `conan-export.sh` | Export the three local Conan recipes (`boost-ext-di`, `cmrc`, `zydis`) into the local cache — once per machine |
| `csgo-start.sh` | Launch legacy linux64 CS:GO with `libzephyrus-csgo.so` preloaded: asserts `-insecure`, forces X11, picks the newest Debug/Release build and the discrete GPU |
| `csgo-cmd.sh` | Send one or more commands to the running instance's command channel (`$ZEPHYRUS_CSGO_CMD`, default `$XDG_RUNTIME_DIR/zephyrus-cmd`) |

All ACR launchers (`acr-start.sh`, `acr-do.sh`, `acr-inject.sh`,
`acr-menu-click.sh`, `acr-dbg-run.sh`) were removed together with the ACR code;
the archived versions live on the `acr` branch.
