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
```

---

## Contents

* [Features](#features)
* [Building](#building)
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

Natural next steps: a level editor, variable floor/ceiling heights, more weapons
and monsters, and replacing the procedural art with real sprite sheets.
