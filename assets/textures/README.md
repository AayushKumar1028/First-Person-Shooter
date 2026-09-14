# Custom textures & sprites

Empty folder on purpose — the game generates all of its art procedurally, so it
runs with nothing in here. Drop a file in with one of the names below and it
replaces that slot on the next launch.

## Rules

* **Formats:** `.png` (8-bit, non-interlaced), `.bmp` (8/24/32-bit uncompressed)
  or `.ppm` (`P6`/`P3`).
* **Size:** any size. Power-of-two sizes (32, 64, 128, 256) tile slightly
  faster and are recommended.
* **Transparency:** use the alpha channel. Sprites need transparent
  backgrounds; walls/floors are opaque.
* **Files are optional and independent** — override one slot or all of them.
* Search order: `assets/textures/`, then the copy next to the executable,
  then the source tree.
* The console prints the list of slots it loaded when the game starts.

## Getting a starting point

```bash
./fps_shooter --dump-textures
```

This writes every built-in texture into `assets/textures/` as a real PNG you can
open, paint over and save back under the same name. Delete the ones you don't
want to change.

## Wall, floor and ceiling slots

| File | Contents |
| --- | --- |
| `wall_brick.png` | red brick |
| `wall_stone.png` | grey stone blocks |
| `wall_metal.png` | riveted metal panels |
| `wall_blue.png` | blue brick |
| `wall_wood.png` | wooden planks |
| `wall_green.png` | dark tech panel with green traces |
| `wall_concrete.png` | cracked concrete |
| `wall_blood.png` | bloodied concrete |
| `wall_crate.png` | supply crate |
| `wall_support.png` | steel I-beams |
| `wall_flag.png` | banner |
| `wall_exit_sign.png` | concrete with an EXIT plate |
| `door_metal.png` | sliding metal door |
| `door_wood.png` | sliding wooden door |
| `door_locked.png` | red keycard door |
| `door_exit.png` | level exit door |
| `floor_stone.png`, `floor_cobble.png`, `floor_metal.png`, `floor_tech.png`, `floor_dirt.png`, `floor_blood.png` | floor tiles |
| `ceil_stone.png`, `ceil_rust.png`, `ceil_dark.png`, `ceil_tech.png` | ceiling tiles |

Wall textures are drawn with `v = 0` at the **top** of the wall.

## Enemy sprites

Each animation is a numbered sequence starting at `_0`:

| Slot | Frames |
| --- | --- |
| `zombie_idle_0/1.png` | 2 |
| `zombie_walk_0..3.png` | 4 |
| `zombie_attack_0/1.png` | 2 (frame 1 shows the muzzle flash) |
| `zombie_pain_0.png` | 1 |
| `zombie_death_0..4.png` | 5 |

The same pattern applies to `imp_*` and `brute_*` (`imp_idle_0`, `imp_walk_2`, …).

Sprites are billboards that sit on the floor: draw the **feet at the bottom**
of the canvas and leave the rest transparent. A canvas of 64x64 drawn with the
body filling the full height matches the built-in proportions.

| Slot | Frames | Notes |
| --- | --- | --- |
| `fireball_0..2.png` | 3 | drawn centred, follows its own height |
| `rocket_0.png` | 1 | centred |
| `explosion_0..5.png` | 6 | centred, grows then fades |
| `particle_0.png` | 1 | soft dot, tinted per particle |

## Pickups, props and gore

`pickup_health_small`, `pickup_health_large`, `pickup_armor_small`,
`pickup_armor_large`, `pickup_ammo_bullets`, `pickup_ammo_shells`,
`pickup_ammo_rockets`, `pickup_ammo_cells`, `pickup_shotgun`,
`pickup_chaingun`, `pickup_launcher`, `pickup_plasma`, `decor_lamp`,
`decor_tech_pillar`, `decor_gore` — all single frame (`_0` is not needed for
single-frame slots; `decor_lamp.png` is correct).

## First-person weapon view models

Canvas is 128x80, drawn scaled to the bottom centre of the screen. Keep the
barrel pointing "up" into the screen.

`viewmodel_pistol`, `viewmodel_pistol_fire`, `viewmodel_shotgun`,
`viewmodel_shotgun_fire`, `viewmodel_chaingun`, `viewmodel_chaingun_fire`,
`viewmodel_launcher`, `viewmodel_launcher_fire`, `viewmodel_plasma`,
`viewmodel_plasma_fire`

The `_fire` variant is shown for a few frames when the weapon goes off, so give
it a muzzle flash.
