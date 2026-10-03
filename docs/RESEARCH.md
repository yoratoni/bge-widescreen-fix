# Research notes: BG&E 20th Anniversary Edition, ultra-wide
Everything learned while reverse engineering `bge.exe` for the widescreen fix: what the game does,
where, how it was confirmed, and what is still open. Written so the work can be resumed from scratch.

All addresses are **RVAs** (offsets from the image base) for the build below. The exe is ASLR-relocated,
in Cheat Engine an RVA `0xAAB9CC` is written `"bge.exe"+AAB9CC`, in static tools it's `0x140AAB9CC`
(default image base `0x140000000`). The mod itself never uses addresses, only the signatures in
`src/core/constants.hpp`.

## Table of contents
1. [The build](#the-build)
2. [Protection and injection](#protection-and-injection)
3. [Settings and saves](#settings-and-saves)
4. [Engine structures](#engine-structures)
5. [Functions](#functions)
6. [Globals](#globals)
7. [How the 16:9 lock works](#how-the-169-lock-works)
8. [Experiment log (Cheat Engine)](#experiment-log-cheat-engine)
9. [How the mod maps to the findings](#how-the-mod-maps-to-the-findings)
10. [Open issues](#open-issues)
11. [Method and tooling](#method-and-tooling)

## The build
| Property        | Value                                                                                                                      |
|-----------------|----------------------------------------------------------------------------------------------------------------------------|
| Executable      | `bge.exe`, 377,683,992 bytes, x64                                                                                          |
| Build date      | PE timestamp `1722621509` (2024-08-02), build string `"Aug  2 2024"`                                                       |
| Build path      | `F:\Jenkins_Workspace\BGE_BUILD\MS25_Support_Patch2\...`                                                                   |
| Engine          | Jade (original BG&E engine, ported to C++), module paths `Src\Libraries\ENGine`, `GraphicDK`, `BIGfiles`, `INOut`, `SouND` |
| Renderer        | D3D12, through Ubisoft's "Fusion 12.6.0" (GearCore 8.1.0), the backend is called "Nori"                                    |
| Other libraries | Wwise 2021.1 (exported from the exe), Bink 2, Ubisoft Services SDK (OpenSSL, curl), FreeType                               |
| Image base      | `0x140000000`, DllCharacteristics `0x8160` (dynamic base + high entropy VA, i.e. ASLR)                                     |

Game data lives in `Data/` (`BigDir.jdproj`, `Paks/*.pak`, `Movies/*.bk2`), most game logic is Jade AI scripts
compiled to C ("AI2C"), so a large part of the code section is generated script code.

## Protection and injection
**Denuvo Anti-Tamper**, identified by the section layout:
| Section                          | Characteristics        | Content                                   |
|----------------------------------|------------------------|-------------------------------------------|
| `.didata` (RVA `0x1000`, ~21 MB) | code, read + execute   | The real game code, plain and unencrypted |
| `.rodata` (RVA `0x1545000`)      | read-only data         | Constants, strings, float literal pools   |
| `.ecode` (RVA `0x192D000`)       | read + write data      | Most game globals                         |
| `.tls$` (~330 MB, entropy 6.7)   | read + write + execute | Denuvo's virtual machine                  |

Section names are scrambled, the entry point sits in a 97-byte stub, and the exe imports
`NtGetContextThread`, `NtSuspendThread` and `AddVectoredExceptionHandler`. None of the functions used by the fix
are virtualized, they are all plain code in `.didata`. Patching them in memory has caused no issue so far,
the exe on disk must never be modified.

There is **no anti-cheat** (no EasyAntiCheat/BattlEye files, imports or strings, the only "cheat" strings
are the game's own debug cheat menu scripts). Ubisoft Connect offline mode is recommended while testing.

**Injection**: the exe imports `dinput8.dll`, `winmm.dll` and `xinput1_4.dll`, so Ultimate ASI Loader works
as `dinput8.dll` or `winmm.dll` in the game folder.

## Settings and saves
`%USERPROFILE%\Saved Games\Beyond Good & Evil - 20th Anniversary Edition\`:
- `BGE.player.ini`: `[Display]` (`MonitorName`, `WindowWidth_PC`, `WindowHeight_PC`, `RefreshRate`,
  `RefreshRate_4K_PC`, `PresentationInterval`), `[Nori]` (FXAA, SSAO, anisotropy, shadows), `[Performance]`, `[Time]`
- `global.sav` (options, binary, contains `i_RenderingMode` = remaster/original), `slot0-4.sav` (+ `.bak`),
  the game rewrites `global.sav` and autosaves into slots, **back the folder up before testing**

Keys the exe reads that the player ini doesn't contain by default (all under `[Display]`): `ScreenRatio`
(default 3), `RenderWidth`, `RenderHeight`, `CPUFrameLimit`. A base `BGE.ini` is read first, then `BGE.player.ini`.

The in-game resolution list comes from the monitor's display modes and does **not** offer 3440x1440.

## Engine structures
### Display data (`GDI_tdst_DisplayData`)
Allocated by `GDI_fnpst_CreateDisplayData` (0x598 bytes). **There are several instances**, the global
`GDI_gpst_CurDD` (pointer at RVA `0x19AFE18`) is switched between them at runtime (writers found at
`+99A888`, `+9B3470`, `+99ABFA`, `+999561`), so a Cheat Engine pointer entry built on it lands on a different
instance from one moment to the next.

| Offset   | Type     | Meaning                                                                                                  |
|----------|----------|----------------------------------------------------------------------------------------------------------|
| `+0x1B0` | `uint32` | Screen format flags, initialized to `8`, bit 0 = fit inside, bit 1 = fit outside, bit 2 = reference is Y |
| `+0x1B4` | `float`  | Pixel Y/X ratio (`1.0`)                                                                                  |
| `+0x1B8` | `float`  | Custom screen Y/X ratio (`1.0`), used when the ratio constant isn't 1..3                                 |
| `+0x1CC` | `int32`  | Screen ratio constant (`[Display] ScreenRatio`, default `3` = 16:9), index into the ratio table          |
| `+0x1D0` | camera   | The embedded camera, **the 3D world camera** (see below), so `+0x1F4`/`+0x1F8` = its width/height        |

### Camera (`CAM_tdst_Camera`)
Offsets relative to the camera (add `0x1D0` for the one embedded in the display data):
| Offset           | Type     | Meaning                                                        |
|------------------|----------|----------------------------------------------------------------|
| `+0x00`          | `uint32` | Flags (bit 0 = apply the screen ratio)                         |
| `+0x04`          | `float`  | Near plane (`1.0`)                                             |
| `+0x08`          | `float`  | Far plane (`50000.0`)                                          |
| `+0x0C`          | `float`  | Horizontal field of view in radians (`π/2` default)            |
| `+0x10`          | `float`  | Y/X ratio of the camera (`1.0`)                                |
| `+0x14`, `+0x18` | `float`  | Projection factors X / Y (`1 / tan(FOV / 2)` at init)          |
| `+0x1C`, `+0x20` | `float`  | Viewport center X / Y                                          |
| `+0x24`, `+0x28` | `float`  | Viewport width / height, **in the virtual screen** (see below) |
| `+0x2C`..`+0x38` | `float`  | Viewport position / size as fractions                          |
| `+0x3C`, `+0x40` | `int32`  | Fitted viewport offsets X / Y, written by the viewport fit     |

### Ratio table
RVA `0x16E5950` (read-only), 4 floats indexed by the screen ratio constant: `{ 1.0, 1.0, 0.75, 0.5625 }`
(index 3 = 9/16 = 16:9). Read at three sites, `+A1D195`, `+A2F2D6` and `+AAB9CC`.

Changing the table itself fixes the 3D view but breaks every 2D layer, because the HUD and menus read the same entry.

## Functions
| RVA        | Name (inferred)                 | Notes                                                                                                                                                                                                                                                                                                              |
|------------|---------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `0xA00500` | `GDI_fnpst_CreateDisplayData`   | Reads `ScreenRatio`, writes the flags (`mov [rdi+1B0], 8` at `+A00873`) and initializes the camera                                                                                                                                                                                                                 |
| `0xAAB980` | Viewport fit                    | `rbx` = display data, `r8` = camera being fitted. Loads the ratio into `xmm6` (`+AAB9CC`), fits a rect of ratio `PixelYoverX * cam.YoverX * ratio` into the camera's width/height according to the flags, writes `+0x3C`/`+0x40`, then calls the matrix builder. The D3D viewport itself is always the full screen |
| `0xA97FE0` | Projection matrix builder       | `(out, fov, ratio, near, far, ..., refY)`, `t = tanf(fov / 2)` (call to `0x79D270`), then `m00 = 1/t`, `m11 = -1/(t * ratio)` (or `m00 = ratio/t`, `m11 = -1/t` when refY)                                                                                                                                         |
| `0xA1D140` | Per-view camera setup           | Same fitting logic plus projection factors, used by the HUD's own 3D views (character portraits, item disk)                                                                                                                                                                                                        |
| `0xA2F250` | Text projection setup           | Computes the display camera's factors (`+0x1E4`/`+0x1E8`) from its width/height and the ratio table (`mulss xmm2, [rcx+rax*4]` at `+A2F2D6`), part of the 2D/text ("STRing") renderer                                                                                                                              |
| `0xA2F6C2` | String positioning              | Converts string positions to pixels: `y * 4/3 - 1/6` when ratio is 16:9 (4:3 authored layouts), then `(pos + 0.03) / 1.06 * size` (safe frame). Stores the origin at `+A2F7A3` into int16 globals                                                                                                                  |
| `0xA2F108` | 2D coordinate conversion helper | Three modes (normalized to pixels, pixels to normalized, safe frame), same constants                                                                                                                                                                                                                               |
| `0x98DA70` | Window/display init             | Runs once (single caller `+98D279`), **clamps the render size to 16:9** (see below)                                                                                                                                                                                                                                |
| `0xA83EE0` | Display ini reader              | Reads `WindowWidth/Height`, `RenderWidth/Height`, calls the render size setter at `+A84610`                                                                                                                                                                                                                        |
| `0xA02B30` | Render size setter              | `void(int width, int height)`, writes the render size globals, raises the "size changed" byte, also writes the window size when the global at `+19A0274` is `1`                                                                                                                                                    |
| `0xAA0D70` | Present path                    | Compares render and window aspect ratios (threshold `1e-5`)                                                                                                                                                                                                                                                        |

The float literal pool (sorted) starts around RVA `0x173C980`, `16/9` is at `0x173CA2C` and is used by
~28 AI2C script functions (HUD layout in script space, `x * 16 / 9`).

## Globals
| RVA                      | Type    | Meaning                                          |
|--------------------------|---------|--------------------------------------------------|
| `0x19597FC`, `0x1959800` | `int32` | Render width / height (the render target)        |
| `0x1959804`, `0x1959808` | `int32` | Window width / height (the actual output)        |
| `0x21373C1`              | `uint8` | "Render size changed" flag, raised by the setter |
| `0x19AFE18`              | pointer | `GDI_gpst_CurDD`, the current display data       |
| `0x19B0020`, `0x19B0638` | `int16` | Current string origin X                          |
| `0x19AFE04`, `0x19AFDE4` | `int16` | Current string origin Y                          |

Other writers of the render size: `+98D3EF` (from the monitor's mode list), `+9B9857` and `+A849F7` (setter clones),
`+14AC943` (resolution change, with a `SetWindowPos`), `+14AD854` (800x600 fallback).

## How the 16:9 lock works
There are three separate layers, each one had to be handled:
1. **Render target clamp**: at startup, `0x98DA70` computes `ratio = height / width` and fits the render size to
   16:9 (constant `0.5625` at RVA `0x173C6B0`, shared with one other function):
   if wider, `width = height / 0.5625` (3440 becomes 2560), if taller, `height = width * 0.5625`.
   The present step then pillarboxes that 16:9 target inside the window. The branch is the `jae` at `+98DF97`
   (`73 23`, `EB 23` disables the clamp), pattern `0F 2F F0 73 23 0F 2F E6 76 0A F3 0F 59 CB`, unique.
2. **Virtual screen**: every camera's width/height (and so every viewport fit) is expressed in a virtual screen equal
   to the **startup** render size (2560x1440 here), changing the render size later doesn't update it. With the
   output ratio, a 2560-wide virtual screen fits a 2560x1071 viewport letterboxed at y = 184, which is exactly the
   offset the text layer ended up with.
3. **Ratio and FOV**: the ratio table entry (`0.5625`) drives both the viewport fit and the projection. Jade keeps the
   **horizontal** FOV fixed, so a wider ratio alone gives Vert- (top and bottom cropped), which pushed the
   camera-attached HUD (portraits, health bars) off the top of the screen.

## Experiment log (Cheat Engine)
The table is `cheat-engine/bge-widescreen-table.CT`, the hooks below are its script entries
(`A`, `B`, `C`, `H`, `T`, ...):
| #  | Test                                                                                         | Result                                                                                | Conclusion                                                               |
|----|----------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------|--------------------------------------------------------------------------|
| 1  | Ratio table entry `0.5625` to `1440/3440`                                                    | 3D fills the width, black bars remain, HUD and menus destroyed                        | The table is shared by 3D and 2D                                         |
| 2  | Render width set live to 3440 + "size changed" flag                                          | Bars gone                                                                             | Found the startup render clamp                                           |
| 3  | "Find out what accesses" on the table entry                                                  | Read every frame by `+AAB9CC`, `+A2F2D6`, `+A1D195`                                   | Three consumers, all live                                                |
| 4  | Hooks A (viewport fit), B (text projection), C (per-view camera) applying the ratio per site | A + B + C: 3D correct, HUD too high                                                   | Per-site overrides work                                                  |
| 5  | B+ / C+ (Hor+ attempts in the wrong functions)                                               | Text scaled up, C+ no effect                                                          | C is the HUD's per-view camera, B is the text renderer                   |
| 6  | Flags `8` to `9` (fit inside)                                                                | HUD at the right height, shifted left                                                 | Fit inside is right for the 2D views, the x offset isn't applied to them |
| 7  | "Accesses" on `+AAB9E5` (`[r8+10]`)                                                          | 3 views: `DD+0x1D0` (heap) + 2 stack cameras                                          | One long-lived camera, two built per frame                               |
| 8  | A2 (ratio for `DD+0x1D0` only) / A3 (all except it)                                          | A2: 3D perfect, A3: menu buttons right                                                | `DD+0x1D0` is the world camera, the stack cameras are menus              |
| 9  | H: `t *= 0.5625 / ratio` right after `tanf` in the matrix builder, world camera only         | 3D zooms out, portraits and health bars back in place                                 | Hor+ done right, the portraits are camera-space 3D objects               |
| 10 | LOG hook at `+AABB2C`                                                                        | All views: x = 0, y = 184, h = 1071                                                   | Fitting happens in a 2560x1440 virtual screen                            |
| 11 | Freezing `DD+0x1F4` to 3440                                                                  | Flicker                                                                               | Several display data instances                                           |
| 12 | Main menu text: B / B+ / B2                                                                  | B: 1.34x taller, B+: 1.34x bigger both ways, B2: right size                           | Text width ~ `1/FactorX`, text height ~ `FactorY/FactorX`                |
| 13 | T: text origin `+= (440, 184)`                                                               | Text exactly on its banners (difference overlay)                                      | `dx = (W - virtualW) / 2`, `dy = (virtualH - virtualW * H / W) / 2`      |
| 14 | Pause menu with/without H                                                                    | Without H the menu art zooms in and gets cut, with H it keeps its size with side bars | Keep Hor+ on, the bars are the limit of a 16:9 asset                     |

**Final recipe** (3440x1440, game started at 2560x1440): render 3440x1440 + size changed flag, flags = 9,
A4 (ratio for every view + flag the world camera), H (Hor+ for the world camera), B2 (text size), T (text origin).

## How the mod maps to the findings
| Mod piece                                                                                                        | Finding                                                                        | CE equivalent                |
|------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------|------------------------------|
| `SCREEN_FORMAT_FLAGS_SIGNATURE`: immediate `8` to `9` at `+A00879`                                               | Fit inside for every display data                                              | Shift+F9 (one instance only) |
| `RENDER_SIZE_SETTER_SIGNATURE`: the call at `+A84610`, its target `0xA02B30`, its first operand the size globals | Render target = window size, through the game's own setter                     | F9 + F11                     |
| `VIEWPORT_FIT_SIGNATURE`: mid hook at `+AAB9DB` (after the ratio load)                                           | `xmm6 = H / W` when `rdx == 3`, flag the world camera when `r8 == rbx + 0x1D0` | A4                           |
| `PROJECTION_MATRIX_SIGNATURE`: mid hook at `+A98009` (after `tanf`)                                              | `xmm0 *= 0.5625 / (H / W)` for the flagged world camera                        | H                            |
| `TEXT_PROJECTION_SIGNATURE`: mid hook at `+A2F2D6`                                                               | `xmm1 *= 0.5625 / (H / W)`, `xmm2 *= (H / W) / 0.5625`                         | B2                           |
| `TEXT_ORIGIN_SIGNATURE`: mid hook at `+A2F7A3`                                                                   | `xmm0 += dx`, `xmm2 += dy`, virtual size from `xmm4` / `xmm3`                  | T                            |

The startup clamp (`+98DF97`) is **not** patched yet, v1 reproduces the CE recipe exactly (16:9 virtual screen).

**First in-game run (v0.1.0, 2026-10-03)**: every signature found at the expected address, all hooks installed,
the log reported `Render size 1920x1080 -> 3440x1440`. The game started at **1920x1080** (not the 2560x1440 of the
player ini), so the virtual screen was 1920x1080, and the text was still aligned: the text offset formula holds for a
virtual screen of a different height than the output, as long as it is 16:9. 3D view, Hor+, portraits, main menu,
pause menu and gameplay text all correct.

## Open issues
- **Loading / black screens** are drawn 16:9 in the bottom-left corner of the 3440x1440 output (the game is visible
  behind). They match the **startup** render size (1920x1080 in the first in-game run), they run on their own thread
  (`"Loading Screen Thread"` string) and most likely keep the size captured at startup. Patching the startup clamp
  would likely fix it, but it also changes the virtual screen and therefore the text offsets.
- **Map menu**: a hover effect is drawn over the side bars, i.e. outside the 16:9 area the rest of the menu uses.
- **Pause menu side bars** with Hor+: accepted, the menu art is a 16:9 asset.
- **Cutscenes**: not tested, Bink movies are pre-rendered 16:9 (pillarboxing expected and fine), in-engine cutscenes unknown.
- **Supported setups**: only outputs wider than 16:9 are touched, 16:10 and narrower keep the stock behaviour.
  The text offsets were validated with 16:9 virtual screens (2560x1440 in Cheat Engine, 1920x1080 in game),
  a game started at a non-16:9 resolution is untested.
- **Where the startup 1920x1080 comes from**: not identified yet, the player ini says 2560x1440, the most likely
  source is the in-game resolution option (applied through the monitor mode list writer at `+98D3EF`).
- **Game updates**: every signature must match exactly once, otherwise the mod patches nothing and logs which one failed.

## Method and tooling
- Static analysis: Python with `pefile` (PE headers, sections, imports), `capstone` (disassembly) and `numpy`
  (brute-force scan for RIP-relative references: for every position, `position + 4 + disp32 == target`, then
  verified by disassembling the longest instruction ending there). Function boundaries come from the `.pdata`
  exception table. Strings, ini keys and float constants were the entry points (e.g. `ScreenRatio` led to the
  display data init, `+0x1CC` led to the ratio table).
- Runtime confirmation: Cheat Engine, "Find out what accesses/writes", auto-assembler hooks with hotkeys, and
  screenshot difference overlays to measure offsets.
- Signatures: RIP-relative displacements and call targets are wildcarded, each pattern was checked to match exactly
  once in the exe before being used.
