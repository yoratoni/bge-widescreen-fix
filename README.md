<p align="center">
  <br />
  <a href="https://yoratoni.com" target="_blank"><img width="100px" src="https://yoratoni.com/favicon.ico" /></a>
  <h2 align="center">@yoratoni/bge-widescreen-fix</h2>
  <p align="center">A DLL to make Beyond Good & Evil 20th anniversary edition compatible with widescreen displays.</p>
</p>

## Features
Out of the box, the 20th Anniversary Edition renders in 16:9 with black bars on any wider screen, this fix makes it
use the whole screen:
- **Full-width 3D** with **Hor+**: the vertical field of view stays the original one, you see more on the sides
  (water, scenery and culling included).
- **HUD, menus, text, fades and loading screens** stay at their original 16:9 layout, centered on the screen.
- **No black bars** during gameplay, any resolution wider than 16:9 (21:9, 32:9, ...) is computed automatically.
- Screens that aren't wider than 16:9 (16:9, 16:10, Steam Deck) are left untouched.

Tested at 3440x1440 (21:9), other ultrawide resolutions use the same maths but haven't been tested yet.

## Installation
1. Download the latest `BGEWidescreenFix-x.y.z.zip` from the [Releases](https://github.com/yoratoni/bge-widescreen-fix/releases) page.
2. Open the game folder, the one containing `bge.exe` (on Steam: right-click the game, **Manage**, **Browse local files**).
3. Extract the ZIP **directly** into that folder (the files must sit next to `bge.exe`, not in a subfolder).
4. Launch the game normally.

The ZIP contains:
| File                   | Role                                                                                                         |
|------------------------|--------------------------------------------------------------------------------------------------------------|
| `BGEWidescreenFix.asi` | The fix itself                                                                                               |
| `BGEWidescreenFix.ini` | Its configuration (optional, every setting has a default)                                                    |
| `dinput8.dll`          | [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader), loads the fix when the game starts |
| `licenses/`            | The licenses of the fix and of every component it includes (Ultimate ASI Loader, SafetyHook, Zydis, Zycore)  |

There's no in-game setting to change, the fix adapts to the size of the game window (your monitor's resolution in
fullscreen).

Some antivirus software flags ASI loaders, because loading a DLL from the game folder in place of a system one is
also a technique malware uses. Ultimate ASI Loader is open source and is the standard loader for PC game fixes.

## Linux and Steam Deck
On Linux (Proton), Wine loads its own `dinput8.dll` instead of the one in the game folder, so the fix isn't loaded.
Add this to the game's launch options on Steam (right-click the game, **Properties**, **Launch Options**):
```text
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

On a Steam Deck, the fix stays inactive: its screen is 16:10, which isn't wider than 16:9.

## Configuration
Every setting lives in `BGEWidescreenFix.ini`, next to the game executable, a missing key keeps its default value:
| Section     | Key                   | Default | Description                                                                        |
|-------------|-----------------------|---------|------------------------------------------------------------------------------------|
| `[General]` | `Enabled`             | `1`     | Master switch, `0` loads the fix but patches nothing                               |
| `[Fixes]`   | `ForceRenderSize`     | `1`     | Keeps the render target at the window size instead of the game's 16:9 size         |
| `[Fixes]`   | `FitInside`           | `1`     | Makes every view fit inside the screen, needed by the 2D layers                    |
| `[Fixes]`   | `HorPlus`             | `1`     | Keeps the 16:9 vertical field of view and widens the horizontal one                |
| `[Fixes]`   | `TextFix`             | `1`     | Corrects the size of the text                                                      |
| `[Fixes]`   | `CenterFittedOffsets` | `1`     | Centers the 16:9 area the HUD, menus, text, fades and loading screens are drawn in |
| `[Debug]`   | `LogFittedViews`      | `0`     | Logs every view the game fits to the screen, for development only                  |

## Uninstallation
Delete `BGEWidescreenFix.asi`, `BGEWidescreenFix.ini` and `BGEWidescreenFix.log` from the game folder. Delete
`dinput8.dll` too, unless another mod in that folder also relies on Ultimate ASI Loader.

## Troubleshooting
The fix writes `BGEWidescreenFix.log` next to the game executable every time the game starts:
- **No log file**: the fix wasn't loaded, check that the files are next to `bge.exe`, and on Linux that the launch
  option above is set.
- **`At least one signature wasn't found, nothing was patched`**: the game was most likely updated and the fix needs
  to be updated as well, the game runs unmodified in the meantime. Please open an issue with your log.
- **`Ready`** at the end of the log, but something looks wrong: please open an issue with your log, your resolution
  and a screenshot.

## Known issues
- **2D-only screens** (loading screens, fades, menus) are centered in 16:9, whatever is behind them stays visible on
  the sides. Some menus (e.g. the map) also park their inactive button states just outside the 16:9 area, which are
  now visible on the sides.
- **The pause menu** artwork is a 16:9 asset, so it doesn't cover the sides.
- **Cutscene videos** are pre-rendered in 16:9 and stay pillarboxed.
- **The item next to Jade's portrait** (and its text) is drawn slightly too big, this is a bug of the original game
  (it also happens on consoles), not of this fix.

## Building from source
Requirements (the same as the template this project is based on):
- [LLVM](https://github.com/llvm/llvm-project/releases) (Clang >= 20), the project builds in C++23.
- [CMake](https://cmake.org/download/) >= 3.21 and [Ninja](https://ninja-build.org/).
- [vcpkg](https://learn.microsoft.com/vcpkg/get_started/get-started), with the `VCPKG_ROOT` environment variable
  pointing to its installation directory.
- The Visual Studio Build Tools ("Desktop development with C++" workload), Clang targets the MSVC ABI.

```bash
# Debug build (output in "build/")
cmake --preset default
cmake --build --preset default

# Release build (output in "build-release/")
cmake --preset release
cmake --build --preset release

# Run the tests
ctest --test-dir build --output-on-failure
```

The version is defined once, in the `project()` call of `CMakeLists.txt`. Pushing a `vX.Y.Z` tag matching it builds,
tests and publishes the release ZIP through the GitHub workflow.

The reverse engineering behind the fix (engine structures, hooks, experiments) is documented in
[docs/RESEARCH.md](docs/RESEARCH.md).

## License
This fix is released under the [PolyForm Strict License 1.0.0](LICENSE): you're free to download, install and use
it for any non-commercial purpose. Redistributing it (including reuploads to other sites), distributing modified
versions, reusing parts of it in another mod or using it commercially requires my permission, ask at
yoratoni.dev@gmail.com.

It bundles [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) by ThirteenAG (MIT) and is built
with [SafetyHook](https://github.com/cursey/safetyhook) (Boost 1.0) and [Zydis](https://github.com/zyantific/zydis) /
Zycore (MIT), these components stay under their own licenses, all included in the release ZIP.
