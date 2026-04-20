# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`AGENTS.md` is the authoritative agent guide for this repo and contains detailed workflow rules. Read it before non-trivial work. This file summarizes the parts that matter most day-to-day.

## Build commands

Always drive builds from the repo root — it copies `version.txt` into each subproject and orchestrates the four sub-builds in the right order.

```sh
./build.sh pico_w debug         # iterating
./build.sh pico_w release       # release UF2 only
./build.sh pico_w release final # also produces -full.uf2, upgrade.bin, SIDECARTVERSION
./build.sh pico_w minsizerel    # MinSizeRel build (Upgrader is always MinSizeRel for release/minsizerel)
```

Artifacts land in `dist/` as `rp-booster-<version>[-build_type][-full].uf2`.

First-time setup:

```sh
git submodule init
git submodule update --init --recursive
```

Toolchain: CMake, Python 3, `arm-none-eabi-*`. The `term/atarist/` build also requires the `stcmd` tooling.

Flash for quick validation: `picotool load -xv dist/rp-booster-<version>.uf2`.

There is no test suite — "done" means a clean root build plus on-device verification (see AGENTS.md §"What 'done' looks like").

## Build pipeline (important coupling)

`build.sh` runs four sub-builds, in order, and the first three feed headers into the fourth:

1. **`upgrader/`** → builds `upgrader.bin`, runs `firmware.py` to emit `upgrader_firmware.h`, copies it into `booster/src/include/upgrader_firmware.h`.
2. **`term/atarist/`** → builds the Atari ST terminal firmware via `stcmd`, pads to 2 KB, runs `firmware.py` to emit `term_firmware.h`, copies it into `booster/src/include/term_firmware.h`.
3. **`booster/`** → main firmware, depends on the two generated headers above.
4. **`placeholder/`** → minimal app used when no microfirmware is installed.

Then `build_uf2.py` combines `placeholder.uf2` + `booster.uf2` into the shipping UF2; with a third arg, `merge_uf2.py` also produces the full image and `upgrade.bin`.

**Implication:** if you change `upgrader/` or `term/atarist/`, you must rebuild from the root (or rerun those sub-builds) so Booster picks up refreshed headers. Don't hand-edit the generated headers in `booster/src/include/`.

The sub-build scripts (`booster/build.sh`, `placeholder/build.sh`, `upgrader/build.sh`) **`cd ..` and run `git submodule update` plus `git checkout tags/...`** to pin SDK versions (currently pico-sdk 2.2.0, pico-extras sdk-2.2.0, fatfs-sdk v3.6.2). Treat any root build as potentially mutating the working tree — warn the user and confirm how to protect in-progress work before invoking it.

`build.sh` also `rm -rf build` at the start of each run.

## Architecture

The device is a Raspberry Pi Pico W on a SidecarTridge cartridge plugged into an Atari ST. Four firmware components share one 2 MB flash:

```
0x10000000  MICROFIRMWARE APP (1152K)   — active microfirmware (replaceable, OTA)
0x10120000  BOOSTER CORE     (768K)     — manager firmware, web UI, OTA, SD
0x101E0000  CONFIG FLASH     (120K)     — 30 × 4K per-app config sectors
0x101FE000  GLOBAL LOOKUP    (4K)       — UUID → config sector mapping
0x101FF000  GLOBAL SETTINGS  (4K)       — device-wide config
```

The placeholder app occupies the microfirmware slot when nothing is installed and just hands control back to Booster. Booster runs in three modes — Factory (initial WiFi setup via captive portal), Manager (online: web UI + OTA catalog), and offline Manager (WiFi unreachable: Atari terminal still launches already-downloaded apps).

### Booster source layout (`booster/src/`)

- `main.c`, `mngr.c`, `fabric.c` — top-level mode controllers (manager / factory / fabric).
- `mngr_httpd.c`, `fabric_httpd.c`, `fsdata_srv.c`, `fs/` — embedded HTTP server. The web assets in `fs/` are minified and compiled into `fsdata_srv.c` via `external/makefsdata`; flash usage is reported by `report_flash_usage.cmake` and the build fails if Booster overruns its slot.
- `appmngr.c`, `sdcard.c`, `hw_config.c` — microfirmware install/launch and FatFs over SD.
- `network.c`, `dhcpserver/`, `dnsserver/`, `httpc/` — lwIP plumbing, captive portal, OTA fetcher.
- `romemul.c` + `romemul.pio` — PIO-based ROM emulation that talks to the Atari ST cartridge port.
- `term.c`, `display_*.c`, `u8g2/` — Atari ST terminal protocol (`tprotocol.h`) and rendering.
- `gconfig.c`, `settings/` — global + per-app settings on top of the `rp-settings` library.
- `select.c`, `reset.c`, `blink.c` — SELECT-button (long-press → factory reset), reset paths, status LED.
- `memmap_booster.ld` — linker script encoding the memory map above; do not change without coordinated updates everywhere.
- `include/` — public headers, including the **generated** `upgrader_firmware.h` and `term_firmware.h`.

Generated headers in `include/` are build artifacts — never edit by hand.

## Constraints to respect

- **Real-time path is hot.** Cartridge protocol parsing happens in PIO/DMA/IRQ context. Keep handlers tiny: capture state, set flags, defer work to the main loop.
- **Static buffers, bounded sizes.** Avoid dynamic allocation in hot paths. Watch RP2040 stack usage.
- **Memory map is load-bearing.** Booster, placeholder, config sectors, lookup table, and global settings all assume the layout in `memmap_booster.ld`. Changing offsets means coordinated changes in the linker script, the build's flash-usage report, OTA, and the upgrader.
- **Flash budget is tight.** Release builds use `MinSizeRel` for the upgrader and the build aborts if Booster exceeds its slot. When adding code or web assets, keep an eye on the size report.
- **Style:** follow `.clang-format` and `.clang-tidy` for any C/C++ you touch. Don't introduce new clang-tidy warnings.

## Non-destructive workflow (from AGENTS.md)

The user often edits files while an agent is running. Treat the working tree as the source of truth.

**Never run without explicit user approval:** `git restore`, `git checkout -- <path>`, `git reset` (any form), `git clean`, or `git submodule update` / submodule SHA changes. Note again that the sub-build scripts do submodule updates and tag checkouts internally — flag this before invoking `./build.sh`.
