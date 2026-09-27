# E33 Boss Music Swapper

Runtime boss-music override for **Clair Obscur: Expedition 33**, as a self-loading C++
mod with a draggable in-game ImGui overlay. No mod loader required.

![The overlay](docs/images/overlay.png)

> **Status: pre-alpha.** Not distributable yet — see the note under
> Installation. What it cannot do yet is
> swap a track: the combat hook and the audio call (M0/M2) need the game running
> and are not written. Everything around them — config, catalogue, decision
> logic, overlay — is built and tested.

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

No UE4SS needed — this mod loads itself. Grab `Repertoire33.zip` from
[Releases](../../releases), or from the artifacts of the latest
[CI run](../../actions).

1. In Steam: right-click the game → Manage → **Browse local files**, then open
   `Sandfall\Binaries\Win64\`. That folder holds the game executable.
2. Extract the whole zip **into that folder**. You end up with:

   ```
   Win64\
   ├── dinput8.dll          <- next to the executable, on purpose
   └── Repertoire33\
       ├── data\
       ├── assets\
       └── config.json
   ```

3. Start the game and press **F9**. Nothing changes until you create an override in the menu.

`dinput8.dll` is a proxy: Windows loads it instead of the system copy, and every call
is passed straight through. The two mods in this pair use different proxy names
(dinput8.dll here), so they can sit in the same folder. If you would rather not
replace a system DLL name, any DLL injector loads the same file unchanged.

To uninstall, delete `dinput8.dll` and the `Repertoire33` folder.

## Building

Every dependency is public, so the DLL builds with no account, token or private
checkout — that was not true of the UE4SS route, see
[docs/BUILD-BLOCKER.md](docs/BUILD-BLOCKER.md):

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # Windows/MSVC
cmake --build build
cmake --build build --target package                 # the installable zip
```

Before pushing Windows code from a Mac or Linux box, syntax-check it without
waiting on CI:

```sh
brew install mingw-w64       # or: apt install g++-mingw-w64-x86-64
./tools/check-windows.sh
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

## Interface

The palette comes from the game's own material library — obsidian, black
marble, gold — with gold used only as rule, border and highlight, mitred
corners, letterspaced capitals and diamond fleurons. Text is EB Garamond, a
French old-style shipped under the OFL: the game's own face is third-party and
cannot be redistributed in a mod. Swap `assets/fonts/EBGaramond.ttf` for your
own if you prefer; the overlay falls back to ImGui's default if it is missing.

The screenshot above is generated, not hand-taken:

```sh
xmake run harness --shot shot.bmp --frames 40 --demo
```

## License

MIT — see [LICENSE](LICENSE). Bundled font under the SIL OFL 1.1, see
[assets/fonts/](assets/fonts/).
