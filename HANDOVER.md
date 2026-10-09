# Handover – Solar Display

State of the project and everything learned while moving it from ESPHome 2025.10.0 to current ESPHome (October 2026).
Read this before changing the firmware.

---

## 1. What the project is

A tabletop display for solar/battery/grid/home power, built on a **Waveshare ESP32-S3-Touch-LCD-7B** (7", 1024×600, GT911 touch, CH32V003 I/O expander, 8 MB octal PSRAM, 16 MB flash).
The firmware is ESPHome + LVGL; all data comes from Home Assistant.

| Path | What it is |
|---|---|
| `Sigenergy/solar-display.yaml` | Display firmware for Sigenergy systems (~10.8k lines) |
| `Solaredge/solar-display.yaml` | Display firmware for SolarEdge systems (~10.8k lines) |
| `*/secrets.yaml` | Example secrets (placeholders + a **public** example API key) |
| `*/includes/ap_mode_helper.h` | Helper for the on-device "AP-only setup mode" |
| `*/includes/themes.h` | Colour themes: palette table and the runtime theme switch (see §3 Themes) |
| `*/includes/edge_swipe.h` | Edge-swipe gesture detection for switching pages (see §3 Edge swipe) |
| `*/includes/lvgl_helpers.h` | Performance helpers: skip unchanged label texts, internal-RAM draw buffer, draw-buffer log line (see §3 Performance) |
| `*/fonts`, `*/icons` | Fonts and PNG icons used by the firmware (duplicated per folder) |
| `Homeassistant/sigenergy/packages/` | HA packages (`energy.yaml`, `energydevices.yaml`) for Sigenergy |
| `Homeassistant/solaredge/2 inverters/packages/` | HA packages for SolarEdge with I1 + I2 |
| `Homeassistant/solaredge/single inverter/packages/` | HA packages for SolarEdge with I1 only |
| `Solaredge/energy.yaml` | Exact duplicate of the 2-inverter HA `energy.yaml` (left over; can be removed) |
| `3Dfiles/Solar-Case.3mf` | Case model |
| `tools/sync_firmwares.py` | Regenerates the Sigenergy firmware from the SolarEdge one and copies `includes/` (`--check` to verify) |

**There is one screen, two firmwares, and they are identical except for the Home Assistant entities they read** (SolarEdge reads `sensor.solar_*`, Sigenergy `sensor.package_sigen_*`, each created by its own HA package). Make firmware changes in `Solaredge/solar-display.yaml` only, then regenerate the Sigenergy file:

```bash
python3 tools/sync_firmwares.py           # rewrite Sigenergy/solar-display.yaml
python3 tools/sync_firmwares.py --check   # verify (exit 1 if out of sync)
```

New Home Assistant sensors need their Sigenergy entity added to `SIGENERGY_NEW` in the script first. The commented-out `ch422g` lines are left over from a config for the older 800×480 Waveshare 7" board; that board is not supported.

---

## 2. Why it was stuck on ESPHome 2025.10.0

The 7B board's backlight, display reset and touch reset go through a CH32V003 I/O expander. ESPHome had no driver for it, so the YAML pulled one from an open pull request:

```yaml
external_components:
- source: github://pr#10071
  components: [waveshare_io_ch32v003]
```

That only worked with ESPHome versions from around that time. **PR #10071 was merged on 2026-06-24 and ships in ESPHome 2026.7.0+**, with the same config keys, so the block can simply be removed.

---

## 3. Everything that was changed (October 2026)

### Build / platform
- Removed the `external_components` block (built-in now).
- Removed `CONFIG_ESPTOOLPY_FLASHSIZE_8MB: y`. It conflicts with `flash_size: 16MB`; the old PlatformIO build silently overrode it, the new native ESP-IDF build fails with *"Partitions tables occupies 16.0MB of flash … does not fit in configured flash size 8MB"*.
- Removed `esphome: platformio_options:`; the native ESP-IDF toolchain ignores it (and `build_flags` there is removed in 2026.12).

### LVGL 8 → 9 (ESPHome switched in 2026.4/2026.5; now LVGL 9.5.0)
Only two compile errors in ~10k lines of lambdas:
- `id(img).get_lv_img_dsc()` → `get_lv_image_dsc()` (11 places, home-usage device icons)
- `lv_point_t` → `lv_point_precise_t`, casts `(lv_coord_t)` → `(lv_value_precise_t)` for `lv_line_set_points` (home-usage power-flow lines)

Everything else (`lv_obj_clear_flag`, `lv_coord_t`, etc.) still works through LVGL's v8 compatibility macros.

### Screen glitching (new with ESPHome 2026)
Symptom: occasional glitching around text and elements that was not there on 2025.10.0.
Cause: the `rpi_dpi_rgb` driver itself did not change. What changed is the build config: ESPHome 2026 sets `FREERTOS_PLACE_FUNCTIONS_INTO_FLASH` and `HEAP_PLACE_FUNCTION_INTO_FLASH` (more code fetched through the flash cache), ESP-IDF went from 5.4.2 to 5.5.5, and none of Espressif's recommended RGB-panel settings were on. The RGB panel streams from a PSRAM framebuffer through bounce buffers, and PSRAM shares the cache/bus with flash, so the LCD DMA gets starved.
Fix, added to `esp32: framework: sdkconfig_options` (confirmed fixed on the real display):

```yaml
CONFIG_ESP32S3_DATA_CACHE_LINE_64B: y
CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB: y
CONFIG_LCD_RGB_ISR_IRAM_SAFE: y
CONFIG_LCD_RGB_RESTART_IN_VSYNC: y
CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH: n
CONFIG_HEAP_PLACE_FUNCTION_INTO_FLASH: n
```

Side effects: internal RAM use went from 44% to 54% (fine). LVGL's worst "took a long time" warning dropped from 804 ms to 371 ms, and the `interval` warning from 270 ms to 143 ms.
To verify the options actually landed, check `<folder>/.esphome/build/solar-display/sdkconfig.solar-display`.

### Layout
- **Scrollbar on the 4-box home page** (`home_page_2`, container `hp2_row`): LVGL 9's default padding made the 288 px cards overflow the 325 px row. Added `scrollable: false` + `scrollbar_mode: 'off'` to `hp2_row`, `col_solar`, `col_battery`, `col_home`, `suggestion_box`, `card_batt_in`, `card_batt_out`, `card_pv_prod`, `card_pv_dist`, `card_import`, `card_export`.
  Do **not** do this to `settings_scroll`; the settings page scrolls on purpose (`lv_obj_scroll_by`).
  About 100 other containers/buttons still have no `scrollable` setting. They looked fine, but if a scrollbar shows up elsewhere, this is the fix.
- **Loading popup**: the spinner moved up 10 px (`y: 8`) and "Loading…" down 6 px (`y: 6`) so there is ~19 px between them (they overlapped by 4 px).
- **Gap between the 4 boxes and the summary card** (exported today / remaining battery / self use today): `hp2_summary_card` moved from `y: 400` to `y: 410`, making the gap 16 px, the same as `pad_column` between the cards. Layout math: `hp2_row` y=88, h=325, cards h=288, vertically centred → cards end at ~y=394. Summary card is 119 px tall → ends at ~529; the tips label (`hp2_lbl_suggestion`, `my_font_medium` 38 px, `bottom_mid` y=-10) starts ~545. A tip that wraps to two lines would grow upward towards the card.
- **Power Flow page dots off-centre** (`solar_info_page`): the animation lambda builds the dot paths from hard-coded node centres (`sx/sy`, `hx/hy`, `bx/by`, `gx/gy`, hub `cx/cy`) that were 14 px right and 12 px below the real icon centres, so every path ran visibly right of the solar and battery icons. They are now the node images' `x`/`y` + 50 (the icons are 100×100): solar (498, 62), home (831, 288), battery (498, 488), grid (165, 288), hub (498, 288). The dots (10×10) are placed at `x − 5, y − 5` so they sit centred on the path (was `− 4`). The flow labels use the same hub, so they moved with the paths. Confirmed on the owner's display.

### Deprecations cleared and touch start-up fixed
- `image:` block converted to the new format (`- platform: file` per image, 33 images).
- `zoom:` → `scale:` on all 43 images (same values).
- Display driver `rpi_dpi_rgb` → `mipi_rgb` with `model: RPI` (generic model, no init sequence) and the original pins/timings. `mipi_rgb` sets up the panel exactly like `rpi_dpi_rgb` (PSRAM framebuffer, 10-line bounce buffer, 1 FB). There is no built-in 7B model; `WAVESHARE-5-1024X600` uses the same pins but different timings and a CH422G reset pin. `setup_priority: 800` keeps the old start-up order (`rpi_dpi_rgb` is HARDWARE priority, `mipi_rgb` uses the default).
- **Touch randomly failing to start** (`touchscreen is marked FAILED: Calibration error`, touch dead until the next reboot). Measured with repeated software restarts: `rpi_dpi_rgb` 3/8 failed, `mipi_rgb` 6/8 failed, so the bug was already there and the new driver only made it more likely.
  Cause: ESPHome's GT911 `setup()` pulses reset (EXIO1 via the CH32V003), holds INT low, then 56 ms later switches INT to input and **immediately** talks I2C. The GT911 datasheet wants ≥50 ms *after* releasing INT, so the chip is sometimes not ready yet.
  Fix: removed `reset_pin` from the `gt911` touchscreen. The chip is already running at 0x5D at boot (seen in the I2C scan before any reset), and without the reset ESPHome talks to it directly. Result: **0/10 failures** on `mipi_rgb`.
  The reboot test scripts used for this (press the "Restart Device" button through the API, wait, then grep `esphome logs` for `touchscreen is marked FAILED`) were throwaway tools and are not in the repo.

### Home screen additions
- Summary card (`hp2_summary_card`), bottom row: **Self-use today** (left) and **Total use today** (right).
  - Total use today = `house_consumption_daily` (everything the house used today).
  - Self-use today = `house_consumption_daily − imported_power_daily`, clamped at 0: solar used directly plus solar stored in the battery and used later. (Before, the "Self-use today" label actually showed total use.)
  - The bottom row's `pad_right` went from 500 to 75 (same as the top row) to make room.
- **Breathing boot logo**: ESPHome's LVGL `animations:` (added after 2025.10) fades `boot_logo` between 40% and 100% `image_opa` (3 s, `round_trip` + `ease_in_out`, looping). `finish_boot` stops it with `lvgl.animation.stop: boot_logo_breathe` so it costs nothing afterwards. Scaling cannot be animated on images, and a big glow is too slow to redraw on this chip, so it is opacity only.

### Performance (scrolling, page loads, stalls)
All numbers measured on the SolarEdge display with ESPHome 2026.9.1. "Frame" = one LVGL refresh while the settings list scrolls (883×475 px redrawn per frame).

| Step | Scroll frame | Notes |
|---|---|---|
| Start (after the 2026 migration) | **166 ms** (~6 fps) | 81 ms of it was copying to the screen |
| `logger: INFO` (SolarEdge was DEBUG), `compiler_optimization: PERF`, no default button shadows | — | not measured separately |
| No byte swap (little-endian `data_pins`) | 133 ms | copy 81 → 49 ms |
| 20-line draw buffer in internal RAM + larger LVGL caches | **~80 ms** (~12–13 fps) | page loads also equal or faster on every page |

What each change does and why:
- **`logger: level: INFO`** (SolarEdge had `logger: null` = DEBUG): DEBUG logged every one of ~250 HA sensor updates (~1,400 lines per 45 s).
- **`esp32: framework: advanced: compiler_optimization: PERF`** (-O2 instead of -Os). Setting `CONFIG_COMPILER_OPTIMIZATION_PERF` in `sdkconfig_options` does **not** work; ESPHome overrides it.
- **`lvgl: theme: button: shadow_width: 0`**: LVGL's default theme gives every button a grey drop shadow; shadows are among the slowest things to draw.
- **No per-frame byte swap**: with `data_pins: {red, green, blue}` the `mipi_rgb` driver always swaps the two pixel bytes in its pin mapping and reports big-endian, so ESPHome sets `LV_COLOR_16_SWAP 1` and LVGL 9 byte-swaps **every pixel of every flushed frame** in PSRAM. Fix: `byte_order: little_endian` and a flat 16-pin list in natural order `[14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40]` (B3–B7, G2–G7, R3–R7). Images are unaffected (ESPHome stores RGB565 images little-endian by default). Colours confirmed correct on the display; a wrong pin order would show up as wrong colours, so check them after touching this list.
- **Internal-RAM draw buffer** (`lvx_use_internal_draw_buffer(20)` in `on_boot`, priority −150): ESPHome always gives LVGL a buffer of at least 1/8 screen (here: full screen) in PSRAM, which is slow to draw into. The helper allocates a 20-line strip (40 KB) in internal RAM and calls `lv_display_set_buffers()`; LVGL then draws in strips. 30 s after boot it logs e.g. `Draw buffer: 40960 bytes in internal RAM; internal RAM free 51696 bytes (largest block 36864, lowest 37232)`. If there is not enough internal RAM it silently keeps the PSRAM buffer.
  Strip height trade-off (page loads in ms: settings / graph / info / menu / home):

  | Buffer | Scroll frame | Page loads | Internal RAM free |
  |---|---|---|---|
  | PSRAM full screen | 133 ms | 172 / 178 / 177 / 150–166 / 293 | ~80 KB |
  | 16 lines | 87 ms | 110 / 192 / 195 / 122 / 250 | ~47 KB |
  | **20 lines** | **81 ms** | **103 / 179 / 179 / 111 / 232** | **~38–52 KB** |
  | 24 lines | 78 ms | 101 / 160 / 160 / 108 / 223 | ~30 KB, dipped to 22 KB with a 6 KB largest block: **too tight** |
- **LVGL caches** (`esphome: build_flags:`): `-DLV_DRAW_SW_CIRCLE_CACHE_SIZE=16` (default 4; this UI uses ~10 corner radii, so rounded corners were recomputed for every strip) and `-DLV_DRAW_SW_SHADOW_CACHE_SIZE=32` (default 0; the home cards use 6 px shadows with 19 px radius). ESPHome removes `LV_*` names passed as build flags from its generated `lv_conf.h`, so this is the supported way to change LVGL options.
- **750 ms `interval` cost 92 ms → ~2 ms** (it blocked the screen for 92 ms every 750 ms, on every page):
  - A block (daily-totals alignment, SoC widgets, a settings pill) had been pasted **inside the 24-bar value-label loop** of the hourly graph, so it ran up to 24 times per tick. Each `lv_obj_align_to()` forces LVGL to recalculate a whole page layout. Removed; the one line that was only there (`batt_soc_arc` alignment) moved to the SoC section above.
  - The hourly graph (`graph_page`) and the solar-info alignment (`solar_info_page`) now update every tick only while that page is shown, and every ~30 s otherwise. A full graph refresh still takes ~85 ms.
  - Settings pills (`display_page`) and the old home page's suggestion box are restyled only when their state/message changes.
- **Loading overlay without dimming**: the 61 navigation handlers show `loading_overlay`, switch page, wait 250 ms and hide it. It used to be a full-screen 40% black layer, so showing and hiding it made LVGL redraw the whole screen twice more per page change (~0.6–0.7 s per change). Now `loading_overlay` is a transparent 280×170 container the size of the spinner card, so only that area is redrawn: a page change is one full redraw plus the 250 ms spinner (~0.3 s). The handlers did not need to change.
- **`lv_label_set_text()` skips identical text** (`lvgl_helpers.h` redefines it as a macro for the YAML lambdas): LVGL re-measures and redraws a label even when the new text is the same, and many labels are refreshed several times a second. Pass `nullptr` as text to force a refresh.

Tried and **rejected**:
- **Two LVGL draw threads** (`-DLV_USE_OS=LV_OS_FREERTOS -DLV_DRAW_SW_DRAW_UNIT_CNT=2`): only 7% faster, and both cores hammering PSRAM starved the screen's DMA → the **picture jumped around**. Do not enable.
- **Lower pixel clock** (21 MHz, ~23 Hz refresh instead of ~33 Hz): 13% faster with the PSRAM buffer, but caused visible **flicker**, and with the internal buffer 30 MHz is just as fast. Keep `pclk_frequency: 30MHz`.
- **120 MHz PSRAM**: not tried. Octal 120 MHz is an experimental ESP-IDF feature and forces flash to 120 MHz; if it fails the display crashes before Wi-Fi comes up, and the case is sealed.

How it was measured (throwaway code, not in the repo, easy to recreate): a header registering LVGL display events (`LV_EVENT_RENDER_START`/`RENDER_READY` for frame time, `FLUSH_START`/`FLUSH_FINISH` for copy time), ESPHome's `runtime_stats:` component (per-component loop time, found the 92 ms interval), and an `api: actions:` entry that opens the settings page and calls `lv_obj_scroll_by(id(settings_scroll), 0, ±170, LV_ANIM_ON)`, triggered from a PC with `aioesphomeapi`.

### Themes
- **Settings → Themes** shows 8 tiles (Graphite = the original look and the default, Midnight, Nord, Dracula, Forest, Ember, OLED, Plum). Each tile previews its own colours. Tapping one switches the whole UI instantly (~22 ms, measured).
- The theme is a template `select` (`theme_select`, "Theme" in Home Assistant, entity category *config*): it can be changed from HA or automations, is saved in flash (`restore_value`) and written immediately (`global_preferences->sync()`), and is re-applied in `on_boot` before the first frame is drawn. Tested: set from the HA API → restart → restored.
- How it works (`includes/themes.h`): the YAML stays written in theme 0's colours. Each colour of `THEMES[0]` is a *role* (page, card, button, borders, text shades, line, accent, control…). `lvx_theme::apply()` walks every LVGL screen plus the top layer (via `lvgl_private.h` → `lv_display_t::screens`) and replaces every local style colour that matches a role of the current theme with the same role of the new theme. The tile container `theme_grid` is skipped so previews keep their own colours.
- Lambdas that set role colours at runtime wrap them in `lvx_theme::color(0x……)` (pills, icon-picker borders, flow lines, suggestion box, inactive graph buttons, white text on normal backgrounds). Caches of "last applied style" compare `lvx_theme::epoch()` so they re-apply after a switch.
- Not themed on purpose: solar/grid/battery data colours, warning colours, graph dataset colours, black/white text on coloured buttons, and the PNG icons (which is also why there is no light theme yet: the white icons would disappear).
- The brightness slider's indicator/knob are now written explicitly as LVGL's default blue `0x2196F3` (role CONTROL), so the slider follows the theme without changing the original look.
- **Adding a theme:** add a row to `THEMES` in `includes/themes.h` (both firmwares), a tile to `theme_page` and the name to `theme_select`'s options. Rules: no colour twice within one theme, and no theme colour equal to a data/warning colour; otherwise the switch cannot tell roles apart.

### Edge swipe between pages
- Swiping in from the right edge shows the next page, from the left edge the previous one. Order (same as the menu, wraps around): Home (`home_page` or `home_page_2`, whichever `change_homepage` selects) → `solar_info_page` → `solar_page` → `battery_page` → `grid_page` → `graph_page` → `home_usage_page`. Menu, settings, setup pages, the interval page and the boot page ignore swipes.
- `includes/edge_swipe.h` is the gesture logic only (no ESPHome/LVGL types): a touch must start within `EDGE_PX` (60 px) of the left/right edge and move `MIN_TRAVEL_PX` (110 px) inwards; it is dropped as soon as it moves more than 40 px vertically and more than 0.6 px vertically per px inwards (a vertical drag stays a scroll). Fires once per touch.
- The touchscreen's `on_update` feeds it `x_org/y_org` (touch start) and `x/y`, finds the current page in the list, and on a swipe calls `lv_indev_wait_release()` on LVGL's pointer input. LVGL then sends PRESS_LOST instead of CLICKED, so a swipe that starts on a button does not press it; taps at the edge still work. The page change is the `edge_swipe_show_page` script: the same spinner + `show_page` as the navigation buttons. `on_release` resets the detector.
- `on_update` runs before `on_touch` in ESPHome, so the touch that wakes a dark screen (backlight off) never switches pages.
- Interval mode: a swipe restarts the countdown, so the page you swiped to stays for a full interval.
- None of the swipe pages can scroll sideways (all scroll extents ≤ 0, measured on the display), so an edge drag cannot move a page before the swipe fires. If a page ever gets content past its edges, check this again.
- Tested: 18 gesture cases on the PC (g++ against the header); on the display, swipes replayed through the real `on_update`/`on_release` triggers via a temporary API action (all 7 pages forward and back incl. wrap-around; no page change for mid-screen swipes, vertical edge drags, short moves, starts 70 px from the edge, screen off, boot page). Real finger swipes on the display were logged as recognised during the test. Not tested automatically: that a swipe starting on a button does not click it (needs a real finger).

### Firmware parity and energy packages
- The firmwares had drifted. Sigenergy was missing "Remaining Battery" (home summary card) and "Autarkie" (grid page), scrolled the settings list without animation, had WiFi preset buttons hard-coded to network names, ran the home-usage flow animation at 100 ms (now 300 ms like SolarEdge), and read two SolarEdge entities by mistake (`binary_sensor.solaredge_i1_grid_status` for off-grid detection and `sensor.solar_battery_time_remaining`). SolarEdge showed a stray `\` in "Disconnected. Retrying…". All fixed; the Sigenergy file is now generated from the SolarEdge file (see §1). Verified with `esphome config`: zero differences besides `entity_id`.
- Private WiFi network names were removed from comments/labels (they remain in old git history).
- **What each package must provide** = every `entity_id` in its firmware. Checked for all three packages; integration entities (`solaredge_i1_grid_status`, `solaredge_b1_state_of_energy`, `sigen_plant_total_pv_generation`) come from the inverter integrations. The hourly graph sensors (`…_by_hour_h00`…`_h23`) come from `utility_meter` **tariffs** (`H00`–`H23`) plus the `automation:` that rotates the tariff every hour; they were always there.
- Added to the **Sigenergy** package: `binary_sensor.package_sigen_grid_connected` (from the integration's "Grid Connection Status": 0 = on grid, 1/2 = off grid), `sensor.package_sigen_battery_time_remaining` (from "Available Max Discharging Capacity" if enabled — it is disabled by default — otherwise SoC × "Rated Energy Capacity" minus "Discharge Cut-Off SOC"; divided by battery discharge power) and `sensor.package_sigen_autarkie_ratio` (1 − imported/house consumption today).
- Added to both **SolarEdge** packages: `sensor.solar_lifetime_energy` (kWh) = "AC Energy" of `solaredge_i1` (+ `i2` when present), unit-aware. The display used `sensor.solarhouse_lifetime_energy`, which only exists with the SolarEdge cloud integration and a site named "solarhouse". "Solar Lifetime Production" (MWh) now uses it too; it read `sensor.solaredge_ac_energy_kwh`, which SolarEdge Modbus Multi does not create. Note: the inverters' AC energy counter can differ slightly from the cloud's lifetime PV figure.
- **Single-inverter fix:** "Solar Panel To House W" copied a two-inverter branch with `i2_ac_power` replaced by `i1_ac_power`, which counted I1's own consumption twice: while the battery charged from the grid it read about −6 kW, pulling house consumption and the daily totals down. It is now 0 in that state; the two-inverter package clamps the same branches at 0.
- The new templates were render-tested with stand-in values (grid status values, both time-remaining paths, autarky clamps, Wh/kWh/MWh, missing I2, night-time grid charging).
- `Solaredge/energy.yaml` is kept identical to the 2-inverter package (still a leftover copy).
- **Nothing visible changes for existing Sigenergy users.** Checked against the original version: the display exposes the same 16 entities plus the new "Theme" select; in the Sigenergy package the `sensor`, `utility_meter` (incl. tariff selects), `automation`, `input_select` and `input_number` sections are byte-for-byte unchanged, and the `template` section only gained the 3 sensors above (no existing line removed or edited). Updating only the firmware without the package just shows "--" for Remaining Battery / Autarkie and leaves off-grid detection inactive.

### README
ESPHome version requirement, an upgrade guide for people with modified YAMLs, OTA update instructions, a warning to generate your own API key, and the single-inverter SolarEdge package.

---

## 4. Test status

| Firmware | Compiles on 2026.9.1 | Tested on hardware |
|---|---|---|
| SolarEdge | yes | **yes**: boots, Wi-Fi, HA API, PSRAM, touch, layout; glitching and scrollbar confirmed fixed by the owner; `mipi_rgb` + no touch reset: 10/10 software restarts with working touch; performance changes confirmed by the owner (stable picture, correct colours, no flicker, much smoother) |
| Sigenergy | yes | **no**: identical to SolarEdge apart from HA entities, but nobody has run it on a display yet; the new Sigenergy package sensors are untested on a real Sigenergy system |

Build sizes: RAM ~56%, flash ~48% of the 8 MB app partition. Internal RAM free at runtime: ~50 KB (lowest seen ~37 KB).

---

## 5. How to build and update

```bash
python -m pip install --upgrade esphome        # 2026.7.0 or newer
cd Solaredge                                    # or Sigenergy
esphome compile solar-display.yaml
esphome upload solar-display.yaml --device <display-ip>   # OTA over Wi-Fi
esphome logs solar-display.yaml --device <display-ip>     # live logs
```

- First build downloads ESP-IDF (~minutes); later builds are fast. A 16-core PC is fine; a Raspberry Pi is not.
- OTA upload of ~3.7 MB takes ~17 s.
- `esphome logs` connects through the API, so it doubles as a test that `api_encryption_key` in `secrets.yaml` matches the device. Run it **before** uploading.
- Safe mode is active (comes with OTA): after 10 failed boots the device starts a minimal Wi-Fi + OTA system for 5 minutes. A boot counts as good after 60 s.
- Wi-Fi credentials entered on the device are kept across OTA updates; there's also a fallback hotspot.
- ESPHome **2025.10.0 cannot be installed on Python 3.14** (fails building a dependency). Use Python ≤ 3.13 if you ever need to build the old `main` for comparison.

---

## 6. Gotchas

- **`secrets.yaml` is tracked in git** even though `.gitignore` lists it (it was committed as a template). Putting real keys in it means they show up in `git status`. On the maintainer's PC this is handled with:
  ```bash
  git update-index --skip-worktree Sigenergy/secrets.yaml Solaredge/secrets.yaml
  ```
  (undo with `--no-skip-worktree`). Cleaner long-term fix: rename to `secrets.example.yaml` and untrack `secrets.yaml`.
- **The example `api_encryption_key` is public.** Devices flashed with it unchanged all share the same key. Changing it on an existing device means updating the key in Home Assistant too.
- **`ota_password` vs encryption:** ESPHome 2026 warns that an OTA password wastes flash/RAM and recommends `ota: encryption:` instead. The plaintext OTA fallback is removed in **2027.3.0**, so switch before then (and update every display once with the new config).
- `Solaredge/energy.yaml` duplicates the HA package and can confuse people; the HA files live in `Homeassistant/`.
- **Internal RAM is the scarce resource**, not PSRAM. The draw buffer takes 40 KB of it; keep ~35 KB free for Wi-Fi, the API and OTA. Don't raise the strip height above 20 lines without checking the `Draw buffer:` log line.
- `lv_label_set_text()` in the YAML lambdas is a macro from `lvgl_helpers.h` that skips identical text (see §3 Performance).
- **Firmware changes go into `Solaredge/solar-display.yaml`** (and `Solaredge/includes/`), then run `python3 tools/sync_firmwares.py`; it also copies the headers to `Sigenergy/includes/`. Never hand-edit the Sigenergy firmware (except to add the Sigenergy entity of a new sensor).
- **New UI colours:** write them in theme 0's colours. If a lambda sets a UI (role) colour at runtime, wrap it in `lvx_theme::color(...)`; data/warning colours stay plain `lv_color_hex(...)`.
- **Adding a main page:** add it to the `pages[]` list in the touchscreen's `on_update` (edge swipe) as well as to the menu.
- **Moving a node icon on the Power Flow page** (`node_solar`, `node_home`, `node_battery`, `node_grid`): also update the node centres at the top of the `solar_info_page` animation lambda (image `x`/`y` + 50), or the dots no longer line up with the icons.
- Keep `loading_overlay` small and transparent. Making it full-screen again (e.g. to dim the page) brings back two extra full-screen redraws on every page change.

---

## 7. Open TODOs (none of these currently break anything)

| Item | Deadline / reason |
|---|---|
| Move OTA from `password` to `encryption` | plaintext OTA fallback removed in **2027.3.0** |
| "Show Tips/Suggestions" switch name: remove the `/` | becomes an error in **2027.7.0** |
| Confirm touch also starts reliably after a real power cut (only software restarts were tested) | the GT911 fix relies on the chip coming up by itself at power-on |
| Test the Sigenergy firmware on real hardware | only compile-tested |
| Load the updated packages in a real Home Assistant (SolarEdge on the owner's system; Sigenergy needs a Sigenergy owner) | new/changed templates were only render-tested offline |
| Optional: shorten the 250 ms spinner delay in the navigation handlers (e.g. to 100 ms) | it is now most of a page change; the owner kept 250 ms for now |
| Graph page: replace the 24 `lv_obj_align_to()` calls for the value labels with computed positions | a graph refresh still takes ~85 ms every 750 ms while the graph page is open |
| Remove `Solaredge/energy.yaml` duplicate, `ch422g` leftovers, `old_icon_sun_100.png` | cleanup |
| From the README: configurable suggestion texts/thresholds, interval page graphs | features |

---

## 8. Where we left off (2026-10-07)

**Repo state**
- Everything is committed and pushed to GitHub `main` (setting-row centring + brightness slider, colour themes, firmware parity + energy packages, this handover).
- The owner's display (SolarEdge firmware) runs the latest build. The owner still has to copy the updated `Homeassistant/solaredge/2 inverters/packages/energy.yaml` into Home Assistant; until then the display's lifetime value shows "--" (it now reads `sensor.solar_lifetime_energy`).

**Agreed roadmap** (discussed, not started)
1. **One firmware for all inverter brands.** Define a fixed list of Home Assistant sensor names the display reads (a "display contract"); every brand package provides exactly those names (Sigenergy gets extra sensors with the standard names next to its existing ones, so nothing breaks). Then there is a single `solar-display.yaml`, and a new brand only needs a new HA package. Do **not** select the brand on the device: ESPHome compiles HA entity names into the firmware, so that would mean subscribing to every brand's ~250 sensors and using scarce internal RAM. Device-side setup options (e.g. "Battery installed", optional brand name/logo) can be selects/switches like Theme.
2. **SolarEdge 1 or 2 inverters in one package:** treat I2 as optional (missing entities already read as 0 with `float(0)`; fix the `is_state(..., 'unknown')` guards), keep the "Panel To House" clamps, and detect grid charging from I1's DC power going negative instead of "AC output <= 0" (more reliable with one hybrid inverter).
3. **Systems without a battery:** package treats a missing B1 as 0 (today "Solar Panel Production W" is forced to 0 when B1 is unknown); firmware gets a "Battery installed" setting that hides the battery card on the home page (the flex row re-centres the other three), the battery node/flows on the info page, the battery page and its menu button, the battery graph datasets, the "Remaining Battery" line and the battery-related tips. Testable on the owner's own display by switching the setting off.

**Open questions for the owner**
- Which other inverter brands should the display contract cover?
- Private WiFi network names were removed from the files but are still in old git history; leave as is (low risk) or rewrite history (needs a force-push)?

