BG&E Widescreen Fix
===================

Makes Beyond Good & Evil - 20th Anniversary Edition use the whole screen on
displays wider than 16:9 (21:9, 32:9, ...), with Hor+ field of view and the HUD,
menus and text kept at their original 16:9 layout, centered on the screen.

https://github.com/yoratoni/bge-widescreen-fix

Installation
------------
1. Open the game folder, the one containing "bge.exe" (on Steam: right-click
   the game, Manage, Browse local files).
2. Copy every file of this ZIP into that folder, next to "bge.exe" (not in a
   subfolder). The "licenses" folder isn't needed by the game.
3. Launch the game normally.

There's no in-game setting to change, the fix adapts to the size of the game
window (your monitor's resolution in fullscreen).

Some antivirus software flags "dinput8.dll" (Ultimate ASI Loader), because
loading a DLL from the game folder in place of a system one is also a technique
malware uses. It's open source and is the standard loader for PC game fixes.

Linux and Steam Deck
--------------------
On Linux (Proton), add this to the game's launch options on Steam
(right-click the game, Properties, Launch Options):

    WINEDLLOVERRIDES="dinput8=n,b" %command%

On a Steam Deck, the fix stays inactive: its screen is 16:10, which isn't wider
than 16:9.

Files
-----
BGEWidescreenFix.asi   The fix itself
BGEWidescreenFix.ini   Its configuration (optional, every setting has a default)
dinput8.dll            Ultimate ASI Loader, loads the fix when the game starts
README.txt             This file, not needed by the game
licenses/              The licenses of the fix and of every component it includes

Configuration
-------------
Every setting lives in "BGEWidescreenFix.ini" and is described inside it, set a
setting to 0 to disable it.

Uninstallation
--------------
Delete "BGEWidescreenFix.asi", "BGEWidescreenFix.ini", "BGEWidescreenFix.log",
this "README.txt" and the "licenses" folder from the game folder. Delete
"dinput8.dll" too, unless another mod in that folder also relies on Ultimate
ASI Loader.

Troubleshooting
---------------
The fix writes "BGEWidescreenFix.log" next to "bge.exe" every time the game
starts:
- No log file: the fix wasn't loaded, check that the files are next to
  "bge.exe", and on Linux that the launch option above is set.
- "At least one signature wasn't found, nothing was patched": the game was
  most likely updated and the fix needs to be updated as well, the game runs
  unmodified in the meantime.
- "Ready" at the end of the log, but something looks wrong: open an issue on
  GitHub with your log, your resolution and a screenshot.

License
-------
Copyright (c) 2026 Adrien Bibollet, released under the PolyForm Strict License
1.0.0 (see "licenses/BGEWidescreenFix.txt"): free to download, install and use
for any non-commercial purpose. Redistributing it, distributing modified
versions, reusing parts of it in another mod or using it commercially requires
my permission, contact yoratoni.dev@gmail.com.
