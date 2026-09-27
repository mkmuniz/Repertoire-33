# E33 Boss Music Swapper

Runtime boss-music override for **Clair Obscur: Expedition 33**, as a UE4SS C++ mod
with a draggable in-game ImGui overlay.

> **Status: pre-alpha.** Not installable yet. The config, catalogue, decision
> logic and UI are built and tested; the combat hook and the audio swap (M0/M2)
> need the game running and are not done.

## Why this instead of a `.pak` replacement

Existing music mods replace files inside the `.pak`, so the swap is permanent and
global, and the author has to ship several variants of the same mod. This one does
it at runtime:

- `config.json` ships **empty** — the game stays 100% vanilla until you say otherwise;
- every override is created from the in-game menu;
- no entry for an encounter means the hook does nothing and the original plays;
- "revert to original" per row;
- one global toggle disables every override at once, without uninstalling;
- one download, no variants.

Because the config is plain JSON, a full alternative soundtrack can be shared as a
single file instead of a new mod.

```json
{
  "enabled": true,
  "overrides": {
    "Boss_Simon": "Track_Renoir",
    "Boss_Sirene": "Track_UneVieATaimer"
  }
}
```

## Installation

**Requires** UE4SS installed in `Expedition 33\Sandfall\Binaries\Win64\`.

1. Extract the `BossMusicSwapper` folder to
   `Expedition 33\Sandfall\Binaries\Win64\ue4ss\Mods\BossMusicSwapper\`
2. Add a `BossMusicSwapper : 1` line to `ue4ss\Mods\mods.txt`. The bundled
   `enabled.txt` also works, but it bypasses mods.txt and gives up load ordering.
3. Start the game. **Nothing changes yet** — the config starts empty.
4. Press **F9** to open the menu.
5. Pick an encounter on the left, a track on the right. Done.

Tested game version: _TBD_.

## Building

The mod DLL needs Windows, MSVC and a UE4SS checkout:

```sh
xmake f --ue4ss=C:/path/to/RE-UE4SS -m release
xmake build BossMusicSwapper
```

Everything else builds anywhere, and is how the project is developed off-Windows:

```sh
xmake f -y                      # fetches nlohmann_json, doctest, imgui, glfw
xmake build tests && xmake run tests      # logic suite, no game needed
xmake build harness && xmake run harness  # the overlay in a native window
```

The harness draws the same overlay the mod draws in-game, with a combat
simulator standing in for the hook and a recording backend standing in for the
game's audio. Sample fixtures live in `harness/sample/` and are deliberately
separate from `data/` — their ids are invented.

See [docs/DEV-MACOS.md](docs/DEV-MACOS.md) for the full split of what runs where.

## Roadmap

| Milestone | State |
|---|---|
| M0 — log the encounter id when combat starts | needs the game; `simulate()` stands in for it |
| M1 — `bosses.json` / `tracks.json` catalogue | loader, search and "seen in game" done; real ids need the game |
| M2 — swap to a **native** game track | decision logic done; the audio call needs the game |
| M3 — JSON config, empty by default, hot reload | done |
| M4 — ImGui overlay | done (in the harness; in-game ImGui registration unverified) |
| M5 — verbose logging, presets, release | logging and presets done |
| M6 — external audio via Wwise `.wem` | not started (optional) |

Native tracks are preferred: the E33 soundtrack was written to soften during
recovery windows and intensify at the climax of a fight. A flat external loop
loses that.

**The soundtrack is never distributed with this mod.** You supply your own files.

## License

MIT — see [LICENSE](LICENSE).
