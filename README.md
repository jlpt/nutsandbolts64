# Nuts & Bolts 64

**Banjo-Kazooie (N64) with Banjo-Kazooie: Nuts & Bolts-style vehicle building.**

Press **L** anywhere in the world to open *Mumbo's Garage*. Snap parts together on a
3D grid (blocks, wheels, engines, fuel, seats, propellers, jets, wings, floaters,
balloons, springs, egg cannons and more), then hit **Start → Test Drive** and Banjo
hops in. Vehicles are real physics objects: they drive over Banjo-Kazooie's own
terrain, fly, float, crash and lose parts.

This repository contains **only original code, tools and textures**. It builds on
the [Banjo-Kazooie decompilation](https://github.com/n64decomp/banjo-kazooie)
(included as a submodule). No ROM data, ripped assets or Nuts & Bolts textures are included:
you supply your own Banjo-Kazooie ROM at build time.

---

## Features

- **Garage editor**: 15 × 8 × 15 build grid, a camera-relative cursor, layers,
  part rotation, ten paint colours, a ghost preview (green = fits, red = blocked),
  and a live weight/power/fuel readout.
- **36 parts in 6 categories** (see the table below), including multi-cell parts like
  big wheels, wings and girders.
- **6 preset vehicles**: Trolley, Kazooie Kart, Glider, Boat, Balloon Bus and
  Monster Truck. There are also 3 save slots, which last until you power off.
- **Physics**:
  - raycast wheel suspension and tyre grip against the game's own collision
  - steering, impulse-based body collisions and engine power vs. weight
  - fuel consumption
  - aerodynamic lift with stable flight
  - buoyancy for floaters and balloons that hover
  - shock-spring jumps and jet boost
- **Damage**: hard crashes knock HP off parts, and broken parts fly off as debris.
- **Banjo-Kazooie integration**:
  - parts unlock as you collect **Jiggies**, or you can switch the garage to sandbox mode
  - egg cannons fire Kazooie's real eggs, which use your egg supply and hit enemies
  - Banjo rides in the seat and still collects notes and items as you drive past
  - game sound effects, and the pause menu is blocked while you build or drive
- **N64-native**: every part model is generated procedurally at boot into F3DEX display
  lists. Textures are 32×32 RGBA16 (2 KB each).

## Controls

| On foot | |
|---|---|
| **L** | Open Mumbo's Garage (when standing on solid ground) / get into your parked vehicle |
| **D-Down** (near the vehicle) | Edit the parked vehicle |

| Garage | |
|---|---|
| Stick | Move the cursor (relative to the camera) |
| C-Up / C-Down | Cursor up / down one layer |
| C-Left / C-Right | Orbit the camera |
| **A** | Place part |
| **B** | Remove part (hold for 1.5 s to clear everything) |
| **Z** | Rotate part 90° |
| L / R | Previous / next part |
| D-Left / D-Right | Previous / next category |
| D-Up / D-Down | Paint colour |
| **Start** | Garage menu: Test Drive, Load (slots and presets), Save, Sandbox, Clear, Leave |

| Driving | |
|---|---|
| **A** / **B** | Gas / brake, then reverse |
| Stick | Steer. In the air: pitch and roll (with wings: bank and turn) |
| **Z** | Jet boost |
| **R** | Fire egg cannons / honk the horn |
| C-Up | Shock spring jump |
| C-Left / C-Right / C-Down | Camera orbit / zoom |
| D-Up | Flip upright (also automatic after a few seconds) |
| D-Down | Back to the garage to edit the vehicle where it stands |
| **L** | Get out |

## Parts

| Category | Parts |
|---|---|
| Body | Wood Block, Metal Block, Wood Wedge, Metal Wedge, Wood Slab, Long Plank, Metal Girder, Glass Block, Honeycomb Block, Jiggy Block, Rubber Bumper |
| Wheels | Small Wheel, Big Wheel, Monster Wheel, Ski, Tank Tread |
| Power | Small Engine, Big Engine, Fuel Can, Fuel Tank, Seat |
| Flight | Propeller, Jet Booster, Wing, Tail Fin, Floater, Balloon |
| Gadgets | Shock Spring, Egg Cannon, Horn, Headlight |
| Decor | Mumbo Skull, Music Note, Checker Flag, Exhaust Pipe, Front Grille |

**Building rules**
- A vehicle needs a **Seat**. Parts must touch an existing part face to face.
- Parts that end up disconnected from the seat are left behind when you drive off.
- Engines drive the wheels and need fuel. Propellers and jets push in the direction
  they face, so rotate them with **Z**.
- Wings give lift once you're moving fast.
- Floaters keep you on water. Balloons lift you, and B vents them.

Parts unlock at 0–10 Jiggies. Turn on **PARTS: ALL (SANDBOX)** in the garage menu to use everything straight away,
or build with `NB_UNLOCK_ALL=1`.

## Requirements

- A **Banjo-Kazooie USA v1.0** ROM (`.z64`, `.n64` or `.v64`; sha1 of the `.z64` is
  `1fe1632098865f639e22c11b9a81ee8f29c75d7a`). USA Rev 1 / v1.1 is **not** supported,
  because the decomp only builds v1.0.
- An **Expansion Pak** (8 MB RAM). The mod is loaded into the extra 4 MB, which leaves the
  original game's memory untouched. Most emulators enable it by default. Without it the
  game runs unmodded.

## Building (Linux / WSL)

```sh
git clone --recursive https://github.com/jlpt/nutsandbolts64.git
cd nutsandbolts64
./tools/setup.sh --deps        # apt packages, submodules, hook patch, Python venv
# also install Rust (https://rustup.rs) for the decomp's ROM tools, and: pip install pillow
make BASEROM=/path/to/Banjo-Kazooie_USA.z64
```

The output is `build/nutsandbolts64.z64`. Build options:

| Option | Effect |
|---|---|
| `NB_UNLOCK_ALL=1` | All parts unlocked from the start |
| `NB_DEBUG=1` | On-screen physics readout |
| `TEST_MAP=0x27` | Boot straight into a level (testing only; `0x27` = Freezeezy Peak) |

`make test` runs the host-side physics test suite (no ROM needed).

## Textures

The part textures in `assets/textures/` are original and drawn procedurally by
`tools/gen_textures.py` (`make textures` regenerates them). Ripped Nuts & Bolts
textures are copyrighted, so none are used here.

You can swap in your own art. Put a PNG of **any resolution** in `assets/textures/`
under an existing name (for example `wood.png`) and rebuild. `tools/png2n64.py`
resizes it to 32×32 with Lanczos filtering and converts it to RGBA16 (RGBA5551) for
the N64's 4 KB texture memory. Paintable materials (wood, metal, seat, canvas,
fuel, balloon…) are tinted at runtime, so keep those textures light and neutral.

## How it works

```
decomp/                     Banjo-Kazooie decompilation (submodule, CC0)
patches/                    small hook patch applied to the decomp
mod/                        the mod (C, built with modern mips GCC)
  src/nb_main.c               hook entry points, on-foot/garage/drive modes, cameras
  src/nb_garage.c             build mode
  src/nb_vehicle.c            blueprint compiler + physics
  src/nb_mesh.c               procedural part meshes -> F3DEX display lists
  src/nb_render.c             per-frame rendering + HUD
  src/nb_parts.c              part catalogue and preset vehicles
assets/textures/            original 32x32 textures (generated)
tools/                      ROM prep, symbol export, texture tools, ROM packer
tests/physics_sim.c         physics test suite against a synthetic world
```

1. `patches/0001-nutsandbolts64-hooks.patch` adds a loader to `core1_init`. If an
   Expansion Pak is present, the loader DMAs the mod image from ROM offset `0x01000000`
   into `0x80400000`. The patch also adds five hook calls:
   - per-frame update
   - 3D draw (right after Banjo)
   - HUD draw
   - pause blocking
   - map unload

   The rest of the game is unchanged. Anti-tamper checks are compiled out with the
   decomp's `ANTI_TAMPER=0 ANTI_PIRACY=0` switches.
2. The mod links against the decomp's ELF. `tools/gen_syms.py` exports every core1/core2
   symbol, so the mod calls the game's own functions directly: collision raycasts, the
   viewport, the print buffer, sfx, items and the egg projectile.
3. The mod draws into its own display list and matrix buffers in Expansion Pak RAM. The
   game's display list only gets a single `gSPDisplayList` call.
4. `tools/mkrom.py` appends the mod image after the rebuilt 16 MB ROM. The IPL3 checksum
   is unaffected.

## Testing

- `make test` runs `tests/physics_sim.c`, which compiles the real vehicle code with
  stubbed game functions. It drives every preset through scripted scenarios: flat ground,
  a 22° ramp, a cliff wall, a lake, drops and roll-overs. It checks they behave (they
  climb, stop, float, fly, land and self-right).
- `tools/m64p-script-input/` is a scripted-input plugin for mupen64plus. It was used to
  play the ROM headless and capture screenshots during development.

## Known limitations

- 36 parts, not Nuts & Bolts' 1,600+ variants. The catalogue is a table in
  `mod/src/nb_parts.c`, and models are a `case` in `mod/src/nb_mesh.c`, so it's easy to extend.
- Blueprint save slots live in RAM only: Banjo-Kazooie's EEPROM is full with its own saves.
- Your vehicle has no collision with Banjo or enemies (it collides with the world).
- Cutscenes, the Jiggy dance and taking damage pop Banjo out of the vehicle so the game can
  run its own logic.
