# FPS Shooter

A first-person shooter in C++17 with SDL2, in the spirit of Wolfenstein 3D and
Doom: grid-based levels, a software raycasting renderer, hitscan and projectile
weapons, explosive barrels, keycards, and a Doom-style status bar with a face
that reacts to your health.

It has no art, sound or level files. Everything you see and hear is either
generated in code at startup or hand-written as ASCII in the source, so the
whole game is one `git clone` and one build away — and every piece of it can be
replaced with your own art later (see [Custom art](#custom-art)).

```
    build: cmake -B build && cmake --build build -j
    run:   ./build/fps_shooter

    no toolchain? ./scripts/package-linux.sh   -> a portable Linux build
```

---

## Contents

* [Features](#features)
* [Building](#building)
* [Playing without building (Linux)](#playing-without-building-linux)
* [Memory footprint](#memory-footprint)
* [Controls](#controls)
* [Game modes](#game-modes)
* [Enemies, weapons and pickups](#enemies-weapons-and-pickups)
* [Custom art](#custom-art)
* [Authoring levels](#authoring-levels)
* [Project layout](#project-layout)
* [Developer tools](#developer-tools)
* [Tuning knobs](#tuning-knobs)
* [Known limitations](#known-limitations)

---

## Features

**Renderer** — Wolfenstein/Doom style software raycaster running into an
ARGB framebuffer that is scaled up to the window.

* Textured walls via DDA raycasting, with per-column depth buffer
* Perspective-correct textured floors *and* ceilings (rows cast back into the grid)
* Vertical look (y-shearing pitch), square pixels derived from the FOV
* Billboard sprites, alpha blended, depth-clipped, sorted back to front
* Sliding doors rendered as real geometry that recedes into the ceiling
* Distance fog, per-axis wall darkening, muzzle sparks, blood, explosion puffs
* Internal resolution is independent of the window (320x200 up to 960x600)

**Gameplay**

* Three-weapon-plus arsenal: pistol, shotgun, chaingun, rocket launcher, plasma rifle
* Hitscan with per-pellet spread, plus rocket and plasma projectiles with splash damage
* Three enemy types with distinct AI: guards (hitscan), imps and brutes (fireballs)
* Enemy AI on a BFS flow field, so they navigate corridors and push doors open
* Explosive barrels with chain reactions
* Red keycards, locked doors, sealed exit doors that open once the sector is clear
* Health, armour (absorbs a third of damage), ammo caps, weapon pickups
* Melee, pain, death and animation states; corpses and floor decals persist

**Shell**

* Title screen with a live, slowly orbiting camera behind the menu
* Pause menu, how-to-play screen, level intro banners, death and victory screens
* Endless arena mode with escalating waves, score, and a persisted best score
* Procedurally synthesised sound effects — no audio files, and the game still
  runs if no audio device exists

---

## Building

Requirements: a C++17 compiler, CMake 3.16+, and SDL2 development headers.
zlib is optional (it only enables PNG textures; BMP and PPM work without it).

### Linux

```bash
sudo apt install build-essential cmake libsdl2-dev zlib1g-dev   # Debian/Ubuntu
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/fps_shooter
```

Fedora/Arch equivalents: `SDL2-devel cmake gcc-c++ zlib-devel`.

### macOS

```bash
brew install sdl2 cmake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/fps_shooter
```

### Windows

With MSYS2 / MinGW-w64:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/fps_shooter.exe
```

With Visual Studio, use the `cmake -B build` flow from a developer prompt with
SDL2 installed where CMake can find it, then open `build/fps_shooter.sln`.

CMake copies the `assets/` folder next to the executable after every build, so
the game finds custom art whether you run it from the build directory or the
source tree.

---

## Playing without building (Linux)

`scripts/package-linux.sh` produces a relocatable bundle, so you can play it on
a machine with no compiler and no SDL2 installed:

```bash
./scripts/package-linux.sh          # writes dist/fps-shooter-linux-x86_64.tar.gz
```

The tarball unpacks to:

```
fps-shooter/
  fps-shooter          <- the executable; run this
  lib/                 <- the SDL2 it was linked against
  assets/textures/     <- drop-in art overrides
  fps-shooter.desktop  <- install into your application menu if you want
  README.txt
```

```bash
tar -xzf fps-shooter-linux-x86_64.tar.gz
./fps-shooter/fps-shooter
```

How it stays self-contained:

* the C++ runtime is linked in statically (`-static-libstdc++ -static-libgcc`)
* the executable carries an `$ORIGIN/lib` rpath, so it finds the bundled SDL2 by
  itself — no wrapper script and no `LD_LIBRARY_PATH`
* only `libc` and `zlib` are taken from the host, and those ship with every
  mainstream distribution

Keep the binary inside its folder: it deliberately looks for `lib/` relative to
itself. The packaging script re-runs the game from the finished bundle and fails
if it did not load the bundled SDL2, so a green run means the tarball really is
relocatable.

---

## Memory footprint

The art is stored the way the 1990s shooters stored it: **one 8-bit palette
index per pixel instead of four bytes of ARGB**, plus a 256-entry colour table
per texture and a separate 1-byte coverage plane for sprites that need alpha.
That trades RAM for one extra array lookup per sampled pixel — exactly the
right way round for a software renderer, which is already CPU-bound and often
runs on machines with tight memory budgets.

The same trick is used everywhere a texture is read: walls, floors, ceilings,
sprites and the HUD all go `palette[index[x, y]]` in their inner loops.

Colours are chosen per texture by median cut rather than a fixed palette, so
the quantiser spends its 256 slots on the colours the art actually uses. Hand
painted textures with few flat colours (most of this art, and most pixel art)
are stored losslessly.

`--mem` prints the current breakdown:

```
$ ./build/fps_shooter --mem --quiet

=== MEMORY ===
  art slots / frames    : 72 / 106 (80 sprite frames carry alpha)
  texels                : 491776 (0.49 million)

  textures
    palette indices     (1 B/texel) :   491776 B
    coverage planes     (sprites)   :   385280 B
    colour tables       (<=256 each):    29668 B
    authoring buffers   (retained)  :        0 B
    subtotal                        :   906724 B (0.86 MiB)
  synthesised audio     (17 effects)  :   390270 B (0.37 MiB)
  frame buffer          (480x300)     :   576000 B (0.55 MiB)
  -------------------------------------------------------------
  total                        :  1872994 B (1.79 MiB)

  the same art as 32-bit RGBA  :  1967104 B (1.88 MiB)
  saving from palettising     : 54% less texture RAM
```

Two details worth knowing if you work on `src/image.h`:

* Textures are written in a wide RGBA buffer while they are being generated or
decoded, then `Texture::finalize()` quantises them and **releases** the buffer.
  `Assets::build()` quantises every slot in one pass once generation is done,
  because the generators blend, outline and dither by reading pixels back.
* A non-zero "authoring buffers (retained)" line means something forgot to call
  `finalize()`, and those textures will render as garbage.

---

## Controls

| Action | Keys |
| --- | --- |
| Move | `W` `S`, strafe `A` `D` |
| Look | mouse, or `←` `→` to turn and `↑` `↓` to aim |
| Run | `Left Shift` |
| Fire | left mouse button, or `Left Ctrl` |
| Use / open door | `Space` or `E` |
| Select weapon | `1` `2` `3` `4` `5`, or mouse wheel |
| Pause | `Esc` |
| Fullscreen | `F11` |
| Render scale | `F2` (lower) / `F3` (higher) |

Menus are navigated with `W`/`S` or the arrow keys and confirmed with `Enter`,
`Space` or a mouse click.

---

## Game modes

### Campaign

Three hand-authored levels of increasing size and hostility:

1. **Docking Bay** — a compact tech base. Guards, one imp, a keycard and a
   sealed exit. The shotgun is in the room you start in.
2. **Refinery** — tighter corridors, imps, and a keycard vault holding the exit.
3. **The Pit** — a wide arena ring with brutes, every weapon available, and a
   gated exit.

Each level requires clearing every hostile to unseal the exit door, then walking
up to it. Red keycards open the locked doors; the HUD shows a key icon once you
have one. The status bar tracks how many hostiles are still alive.

### Arena

Endless waves on a freshly generated map. Each wave spawns `2 + wave` hostiles
(guards, then imps from wave 2, then brutes from wave 4) away from you, along
with supply drops. A new weapon unlocks every couple of waves. Score comes from
kills plus a wave-clear bonus, and the best score is written to
`arena_best.txt`.

---

## Enemies, weapons and pickups

| Enemy | Health | Speed | Attack | Notes |
| --- | --- | --- | --- | --- |
| Guard | 24 | 1.55 | hitscan (4–9) + melee | Fast to kill, dangerous in groups. Shots often miss a moving target. |
| Imp | 60 | 1.95 | fireball (6–12) + melee | Throws dodgable projectiles. |
| Brute | 150 | 1.25 | fireball (14–24) + melee | Slow, tanky, hits hard. |

| Weapon | Ammo | Damage | Notes |
| --- | --- | --- | --- |
| Pistol | bullets | 9–15 | Fast, always available |
| Shotgun | shells | 7 × 5–10 | Close range shredder |
| Chaingun | bullets | 8–14 | Full auto, small spread |
| Rocket launcher | rockets | 70–100 splash, 2.7 radius | Hurts you too — mind the walls |
| Plasma rifle | cells | 14–22 projectile | Fast projectiles, no splash |

Ammo caps: 200 bullets, 60 shells, 40 rockets, 200 cells. Health caps at 100,
armour at 100 and absorbs a third of every hit. Health and ammo pickups are
ignored when you are already full, so you never waste them.

---

## Custom art

The game generates all of its art procedurally, so this is entirely optional.
To replace any of it:

```bash
./fps_shooter --dump-textures    # writes every built-in texture into assets/textures/ as PNG
```

Then open, repaint and save any of those files under the same name — the game
picks them up on the next launch. Files are optional and independent, so you can
replace one wall texture or the entire sprite set. PNG, BMP and PPM are all
accepted at any size; use the alpha channel for sprite transparency.

See **[assets/textures/README.md](assets/textures/README.md)** for the full slot
list (`wall_brick`, `zombie_walk_2`, `pickup_shotgun`, `viewmodel_plasma_fire`,
and so on) and the sizing conventions for billboards and weapon view models.

---

## Authoring levels

Levels are ASCII maps in `src/levels.cpp`. The legend is documented at the top
of `src/level.cpp`:

```
Walls   '#' brick   '%' stone   'M' metal    'N' blue    'W' wood   'G' tech
        'C' concrete 'X' blood  'R' crate    'U' steel   'F' flag   'S' exit sign
Doors   'D' metal   'd' wood    'L' locked (needs the red key)      'Z' level exit
Floors  ',' stone   '.' cobble  '_' grate    '=' tech    ':' dirt   ';' blood
Ceils   '~' stone   '^' rust    '"' dark     '`' tech
Spawns  'P' player  'z' guard   'i' imp      'B' brute   'o' barrel 'l' lamp
        't' pillar  'g' gore    'K' key      'h' stim     'H' medkit 'v'/'V' armour
        'm' bullets 'n' shells  'k' rockets  'e' cells
        'w' shotgun 'q' chaingun 'y' launcher 'j' plasma
```

Rows can be ragged without breaking anything (short rows are padded with wall
and the border is sealed automatically — doors placed in the border survive, which
is how each level's exit is wired into the bottom wall).

After editing, validate the maps before playing:

```bash
cmake --build build --target level_check -j && ./build/level_check
```

It reports row alignment, flood-fills from the player start, and flags anything
unreachable (spawns, the key, the exit) plus whether the exit is properly gated
behind the keycard door.

---

## Project layout

```
CMakeLists.txt          build script (SDL2 + optional zlib)
assets/textures/        empty by design; drop custom art here
src/
  core.h                math, colour and random helpers
  framebuffer.h/.cpp    ARGB8888 render target with drawing primitives
  font.h/.cpp           5x7 bitmap font (HUD, menus and signs in textures)
  image.h/.cpp          texture storage + PNG/BMP/PPM decode and PNG/PPM encode
  assets.h/.cpp         procedural retro art for every slot + disk overrides
  level.h/.cpp          grid map, doors, collision, raycasts, BFS flow field
  levels.h/.cpp         the three campaign maps as ASCII
  render.h/.cpp         the raycaster: floors, ceilings, walls, sprites
  entities.h/.cpp       player, weapons, enemy AI, projectiles, pickups, world
  hud.h/.cpp            status bar, view model, menus, screen effects
  audio.h/.cpp          waveform synthesis + software mixer
  game.h/.cpp           window, fixed-step loop, state machine, modes
  main.cpp              CLI parsing and the headless self test
tools/
  render_check.cpp      renders known scenes as terminal art (renderer unit test)
  level_check.cpp       validates campaign geometry and reachability
```

The simulation runs at a fixed 60 Hz with an accumulator, so physics and AI
behave identically regardless of frame rate, and rendering is decoupled at
whatever rate the machine can manage.

---

## Developer tools

```bash
# Renderer smoke test: prints scenes as ASCII luminance art so geometry can be
# checked from a terminal, including a closed vs. opening door and sprite depth.
cmake --build build --target render_check -j && ./build/render_check

# Map validation: alignment, reachability, keycard gating.
cmake --build build --target level_check -j && ./build/level_check

# End-to-end headless test: 30+ assertions covering combat, explosions, armour,
# pickups, door timing, raycasts, line of sight, then a scripted playthrough
# with terminal-art frames. Exits non-zero on failure.
./build/fps_shooter --selftest --frames 600

# Same, but in arena mode.
./build/fps_shooter --selftest --arena --frames 600

# Save a rendered frame as a PNG (works headless).
./build/fps_shooter --selftest --frames 60 --shot frame.png

# RAM breakdown: art, synthesised audio and frame buffers, next to what the
# textures would have cost as 32-bit RGBA. Also flags leaked authoring buffers.
./build/fps_shooter --mem

# Build a relocatable Linux bundle (see Playing without building).
./scripts/package-linux.sh
```

`./fps_shooter --help` lists all options, including `--level N` and `--arena`
to skip the menu, `--render WxH` and `--size WxH` for resolution, and `--fov`,
`--sens` and `--mute`.

---

## Tuning knobs

| What | Where |
| --- | --- |
| Weapon damage, spread, fire rate, recoil | `kWeapons[]` in `src/entities.cpp` |
| Enemy health, speed, damage, ranges, sight | `kEnemies[]` in `src/entities.cpp` |
| Player speed and run multiplier | `World::updatePlayer` in `src/entities.cpp` |
| Fog distance and ambient light | `Game::init` → `renderSettings_` |
| FOV, mouse sensitivity, render scale | `GameConfig` in `src/game.h` |
| Movement speed for head bob | `updatePlayer` (`bobPhase`) |
| Arena wave sizes and rewards | `Game::startArenaWave` in `src/game.cpp` |
| Level layouts | `src/levels.cpp` |
| Palette size per texture (256 = max quality, 16 = chunkiest) | `Texture::finalize` in `src/image.cpp` |

---

## Known limitations

* Walls are full height — no stairs, lifts or rooms-over-rooms (that is the
  Wolfenstein model; Doom's variable floor heights would need a different
  renderer).
* One texture per cell face; no independent upper/lower wall sections.
* Lighting is distance haze only, no dynamic lights or per-sector light levels.
* Enemy navigation uses a shared flow field, which is robust in open maps but
  can funnel a crowd through the same doorway.
* The arena's best score lives in `arena_best.txt` in the working directory;
  delete it to reset.
* Textures are palettised to 256 colours each. Art with smooth photographic
  gradients can band slightly; a photograph used as a wall texture is the worst
  case. Increase the palette size in `Texture::finalize` if you would rather
  spend the RAM.
* The synthesised sound effects are held in RAM as 16-bit samples (~0.4 MiB).
  They could be generated on the fly in the audio callback instead — pure CPU,
  no clip table — which is the obvious next RAM saving if you want one.

Natural next steps: a level editor, variable floor/ceiling heights, more weapons
and monsters, and replacing the procedural art with real sprite sheets.
