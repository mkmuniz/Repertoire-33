# E33 Boss Music Swapper

Runtime boss-music override for **Clair Obscur: Expedition 33**, as a UE4SS C++ mod
with a draggable in-game ImGui overlay.

> **Status: pre-alpha (M0).** Not installable yet.

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
2. Start the game. **Nothing changes yet** — the config starts empty.
3. Press **F9** to open the menu.
4. Pick an encounter on the left, a track on the right. Done.

Tested game version: _TBD_.

## Building

```sh
xmake f --ue4ss=C:/path/to/RE-UE4SS -m release
xmake
```

Windows/MSVC only — the artifact is a DLL loaded by UE4SS.

## Roadmap

| Milestone | Scope |
|---|---|
| M0 | Log the encounter id when combat starts |
| M1 | `bosses.json` / `tracks.json` catalogue |
| M2 | Hardcoded swap to a **native** game track |
| M3 | JSON config, empty by default, hot reload |
| M4 | ImGui overlay |
| M5 | Verbose logging, presets, release |
| M6 | External audio via Wwise `.wem` (optional) |

Native tracks are preferred: the E33 soundtrack was written to soften during
recovery windows and intensify at the climax of a fight. A flat external loop
loses that.

**The soundtrack is never distributed with this mod.** You supply your own files.

## License

MIT — see [LICENSE](LICENSE).
