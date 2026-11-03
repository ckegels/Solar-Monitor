# Handover – Solar Display

State of the project and everything learned since moving it from ESPHome 2025.10.0 to current ESPHome (October–November 2026, now on ESPHome 2026.10.0).
Read this before changing the firmware.

---

## 1. What the project is

A tabletop display for solar/battery/grid/home power, built on a **Waveshare ESP32-S3-Touch-LCD-7B** (7", 1024×600, GT911 touch, CH32V003 I/O expander, 8 MB octal PSRAM, 16 MB flash).
The firmware is ESPHome + LVGL; all data comes from Home Assistant.

| Path | What it is |
|---|---|
| `Firmware/solar-display.yaml` | The display firmware, the same for every inverter brand (~13k lines) |
| `Firmware/secrets.example.yaml` | Template for `Firmware/secrets.yaml` (API key, OTA password, Wi-Fi). The real `secrets.yaml` is git-ignored |
| `Firmware/includes/ap_mode_helper.h` | Helper for the on-device "AP-only setup mode" |
| `Firmware/includes/themes.h` | Colour themes: palette table and the runtime theme switch (see §3 Themes) |
| `Firmware/includes/edge_swipe.h` | Edge-swipe gesture detection for switching pages (see §3 Edge swipe) |
| `Firmware/includes/hu_sites.h` | `HuSiteSet`: the other site's Home Usage settings as one 161-byte flash block (see §3 Two Home Assistant sites) |
| `Firmware/includes/lvgl_helpers.h` | Performance helpers: skip unchanged label texts, internal-RAM draw buffer, draw-buffer log line (see §3 Performance) |
| `Firmware/fonts`, `Firmware/icons` | Fonts and PNG icons used by the firmware |
| `Homeassistant/sigenergy/packages/` | HA packages (`energy.yaml`, `energydevices.yaml`) for Sigenergy |
| `Homeassistant/solaredge/2 inverters/packages/` | HA packages for SolarEdge with I1 + I2 |
| `Homeassistant/solaredge/single inverter/packages/` | HA packages for SolarEdge with I1 only |
| `Homeassistant/migration/` | Display entity list, Sigenergy rename list + script, handovers for the HA instances (see §3 One firmware) |
| `3Dfiles/Solar-Case.3mf` | Case model |

**There is one firmware.** It reads a fixed set of 244 Home Assistant entities (`Homeassistant/migration/display-entities.txt`), and every brand package provides exactly those entity ids. A new brand only needs a new package. When the firmware starts reading a new entity, add it to **every** package and to `display-entities.txt`.

The commented-out `ch422g` lines are left over from a config for the older 800×480 Waveshare 7" board; that board is not supported.

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

## 3. Everything that was changed (October–November 2026)

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
- **Internal-RAM draw buffer** (`lvx_use_internal_draw_buffer(16)` in `on_boot`, priority −150; 20 lines until 2026-11-01, reduced for internal RAM, see §6): ESPHome always gives LVGL a buffer of at least 1/8 screen (here: full screen) in PSRAM, which is slow to draw into. The helper allocates a 16-line strip (32 KB) in internal RAM and calls `lv_display_set_buffers()`; LVGL then draws in strips. 30 s after boot it logs e.g. `Draw buffer: 40960 bytes in internal RAM; internal RAM free 51696 bytes (largest block 36864, lowest 37232)`. If there is not enough internal RAM it silently keeps the PSRAM buffer.
  Strip height trade-off (page loads in ms: settings / graph / info / menu / home):

  | Buffer | Scroll frame | Page loads | Internal RAM free |
  |---|---|---|---|
  | PSRAM full screen | 133 ms | 172 / 178 / 177 / 150–166 / 293 | ~80 KB |
  | **16 lines (current)** | **87 ms** | 110 / 192 / 195 / 122 / 250 | ~47 KB |
  | 20 lines (until 2026-11-01) | 81 ms | 103 / 179 / 179 / 111 / 232 | ~38–52 KB |
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
- **House consumption fix (both SolarEdge packages):** "Solar House Consumption W" is now the AC balance `i1_ac + i2_ac − m1` (clamped at 0) instead of panel + battery + grid → house. Measured against the main panel meter while the battery discharged: 447.7 W vs 441.5 W, where the old sum gave 423.1 W. Two causes: "Solar Battery To House W" also multiplied by `solar_battery_effectiveness` (≈ 0.93), but B1 DC power is already measured at the inverter's DC input, so the battery's losses were counted twice (≈ 7 % of battery discharge missing from house kWh); and the sum mixed three template sensors that update one after another, which could briefly show a wrong total. Battery To House W no longer uses battery effectiveness. The sensor itself stays (its average sensor uses it).
- **Brought back from the live HA install (2026-10-31)** into both SolarEdge packages: unique_ids for all `integration` sensors (`energy_int_<name>`), all `utility_meter`s (`energy_um_<key>`) and "Solar Battery SOC Rolling", so they can be managed in the UI; keep these IDs exactly as they are or HA creates `…_2` duplicates. Also the night rule: "Solar Panel Production W" is 0 while `sun.sun` is below the horizon, because B1 lags I1 by one Modbus poll and a load step at night showed up as fake PV for ~34 s. "Solar Panel To House W" got the same rule (it has the same lag in the other direction). The live install in turn was missing the 10-06 changes (Panel To House clamps, "Solar Lifetime Energy").
- The new templates were render-tested with stand-in values (grid status values, both time-remaining paths, autarky clamps, Wh/kWh/MWh, missing I2, night-time grid charging).
- **Nothing visible changes for existing Sigenergy users.** Checked against the original version: the display exposes the same 16 entities plus the new "Theme" select; in the Sigenergy package the `sensor`, `utility_meter` (incl. tariff selects), `automation`, `input_select` and `input_number` sections are byte-for-byte unchanged, and the `template` section only gained the 3 sensors above (no existing line removed or edited). Updating only the firmware without the package just shows "--" for Remaining Battery / Autarkie and leaves off-grid detection inactive.

### One firmware, shared entity names (2026-10-31)
- `Solaredge/` and `Sigenergy/` are replaced by **`Firmware/`**. `tools/sync_firmwares.py` and the leftover `Solaredge/energy.yaml` are gone. The firmware is the former SolarEdge one, with two entities changed: `binary_sensor.solar_grid_connected` (was `binary_sensor.solaredge_i1_grid_status`) and `sensor.solar_battery_soc` (was `sensor.solaredge_b1_state_of_energy`; firmware id `battery_soc`).
- **The SolarEdge names are the standard.** Both SolarEdge packages gained only `Solar Grid Connected` and `Solar Battery SoC`, which wrap the two integration entities. Nothing was renamed.
- **The Sigenergy package was migrated** with `Homeassistant/migration/rewrite_sigenergy_package.py`:
  - **phase A:** unique_ids `energy_um_<new key>` / `energy_int_<name>` for the 35 utility meters and 7 integration sensors
  - **phase B:** the 219 renames in `sigenergy-entity-renames.csv` (`package_sigen_…` → `solar_…`, "pv" → "panel"), plus a new `solar_lifetime_energy` (kWh, from `sensor.sigen_plant_total_pv_generation`)
  - Template unique_ids were kept. The `package_sigen_*_kwh` integration sensors, `package_sigen_battery_soc_hours` and the input helpers keep their names.
- **Live installs:** SolarEdge only needs the current package (nothing renamed; its one-off migration note was removed 2026-11-03, it is in the git history). Sigenergy follows `Homeassistant/migration/HANDOVER-sigenergy-ha.md`. The Sigenergy one renames the entities in the registry, so history and statistics move with them, and updates dashboards etc. **Order:** packages first, then flash the displays right after (displays on the old firmware read the old names).
- Checked: each of the three packages defines all 244 display entities; the firmware compiles.
- **Secrets:** `secrets.yaml` is untracked and ignored (root `.gitignore`); `Firmware/secrets.example.yaml` is the template, without the previously committed key. The example `energydevices.yaml` files now use neutral sensor names (they held the owner's device and place names).

### Two Home Assistant sites (2026-10-31, optional since 2026-11-01)
- **Off by default.** `dual_site_mode` (global, restored: 1 = on, 0 = off, −1 = not decided yet) is set on **Settings → Two Home Assistant sites** (`dual_site_page`: explanation + on/off button) or with the *Two Home Assistant Sites* switch (`dual_site_set`). The first boot with this firmware turns it on only for a display that is already on site 2, so it keeps working. `dual_site` holds it for this boot and the UI. Off: no site card in Settings, no site buttons on the boot screen, and turning it off while on site 2 switches back to site 1 (restart). The owner's three displays have it on; the repo default stays off.
- **Choosing a site:** the boot screen (two buttons at the top) and **Settings → Home Assistant** (top card) have a button per site. The home page (`home_page_2`) has `hp2_btn_site` next to the logo (2026-11-03, only with two sites on): it shows the current site's name (`hp2_lbl_site`, set in `site_update_label`) and a tap runs `site_switch` for the other site, without asking. The choice is the global `site_index` (`restore_value`, initial `${default_site}`). Tapping the other site runs `site_switch`: it sets the index, waits 500 ms so the globals component stores it, calls `global_preferences->sync()`, then reboots.
- **Switching from Home Assistant:** the template select `site_ha_select` ("Home Assistant Site"). At boot its options are set to the two names (static buffers, `traits.set_options`) and its state to the active site. `set_action` runs `site_switch`. Home Assistant reads the options when it connects, so a rename shows up there after the next restart. Safe because the Home Assistant you switch away from just sees a closed port and keeps retrying.
- **Site names** are the template texts `site_1_name_text` / `site_2_name_text` ("Site 1 Name", "Site 2 Name" in HA, `restore_value`, initial from the `site_1_name` / `site_2_name` substitutions, default "Home" / "Site 2"; disabled by default in HA, like the *Home Assistant Site* select). They're edited on `site_names_page` (✎ on the Settings card) with the same floating keyboard + Done button as the subpanel editor. `site_update_label` refreshes all four buttons (labels and active colour); it runs at boot, on a theme change and on a name change.
- Site 2 uses **port 6055** (`api_port_site_2`) and the key in the `api_key_site_2` substitution. Its repo default is `""` = the site 1 key (only the port differs). The owner's displays are built with `-s api_key_site_2 <api_encryption_key_site_2 from secrets.yaml>` (see §5); **without it a display on site 2 offers the site 1 key and the Sigenergy HA asks for re-authentication.** `on_boot` priority 225 (before the API opens its socket at `AFTER_WIFI` = 200) calls `set_port()`. Priority 150 (after the API setup, before any client can connect) calls `set_noise_psk()` with the key decoded into a static buffer. `site2_active` holds the boot-time decision.
- **Why the port too:** on a wrong key, Home Assistant's ESPHome integration calls `_start_reauth_and_disconnect()`: it stops reconnecting and waits for re-authentication. On "connection refused" it keeps retrying with backoff (max 60 s, immediately on an mDNS announcement). A discovered port is only adopted if the probe with the entry's own key succeeds, so with different keys the inactive Home Assistant never overwrites its port.
- **Key and port belong to the site, the same on every display:** site 1 = SolarEdge HA (key A, 6053), site 2 = Sigenergy HA (key B, 6055). `secrets.yaml` is identical everywhere. Per display only `device_name`, `friendly_name` and `default_site` differ (see §5). Each Home Assistant uses the same port and key for all displays.
- **Home Usage settings are per site** (names, icons, routing, subpanels on/off): the `hu_*` globals hold the active site's set, `hu_alt_set` (`std::array<uint8_t, 161>`, layout `HuSiteSet` in `includes/hu_sites.h`) the other one. `on_boot` priority 225 swaps them when `hu_set_site` differs from the site of this boot; on the first boot with this firmware both sites start with the current set. The *Home Usage …* entities follow the active site.
- **Boot screen after a switch:** `site_switch` sets `site_switch_boot`; the next boot copies it to `boot_after_switch` and the boot screen says that Home Assistant can take up to a minute.
- The boot screen shows the site. The API-key reveal on the info page shows the active site's key. With the wrong site and no connection, the boot screen's "Menu" button (after 30 s) still leads to Settings.
- `esphome logs` from the PC uses `api_encryption_key` on 6053, so it only works while site 1 is active. For a display on site 2 (.92) use `aioesphomeapi` on port 6055 with the site 2 key. OTA uses the OTA password and works on both.
- **ESPHome Builder with the repo link works.** Tested: a config with `packages: {url: https://github.com/ckegels/Solar-Monitor, files: [Firmware/solar-display.yaml]}` plus local `substitutions` and `secrets.yaml` validates and compiles. `includes/`, fonts and icons are taken from the cloned repo. Each Builder needs the same `secrets.yaml`, or a flash from there changes the keys.

### Screen sleep, wake and brightness (2026-11-01)
- **Three modes** on the Display page's *Screen Sleep* card (cycles Always → Motion → Touch): *Motion* = HMMD presence sensor (GPIO6) + touch, *Touch* = no sensor fitted, the timeout runs from the last touch, *Always* = never sleeps. In HA: *Display Auto Sleep* (off = Always) and *Presence Sensor* (off = Touch). `presence_sensor_default` (substitution, `"ON"`) is the initial value for a display without the sensor.
- `screen_last_activity_ms` (was `ld1125_last_presence_ms`) is updated by touch (press, release), presence and every wake; the 1 s sleep check compares it with *Display Timeout (s)*. Turning auto sleep on restarts the count.
- **Touch mode pulls GPIO6 down** (`presence_pin_apply`, also run at boot priority −150 because the binary sensor's own pin setup would undo it): an open input floated and flickered *HMMD Presence* in HA.
- The touch that wakes a dark screen calls `lv_indev_wait_release()`, so it does not also press the button under the finger.
- **Screen = `backlight_switch`** (*Screen* in HA). The `lcd_backlight` light is `internal` now: its on/off did not match the screen. Every wake turns the light back on if it was off (otherwise the PWM sits at `min_power` 3 %).
- **Brightness** is the template number *Screen Brightness* (`screen_brightness`, 5–100 %, restored): the Display page slider sets it through `make_call()`, and `on_boot` priority 500 applies it (was a fixed 80 %).
- **Backlight watchdog** (30 s interval): the CH32V003 holds the backlight enable (EXIO2) and PWM, and ESPHome only writes them on a change, so after a chip reset the screen stayed dark/dim. The interval reads input register 0x04, restores EXIO2 if it differs from the switch, and rewrites the PWM level.
- `display_timeout_seconds` changes on the display use `make_call()` (saved) instead of `publish_state()` (lost at restart).

### Boot screen status (2026-11-01)
- `boot_status_update` runs every second until Home Assistant connects and on Wi-Fi/API events: *Connecting to Wi-Fi…* / *Waiting for Home Assistant (site)… N s*, plus `lbl_boot_info` with SSID, IP and API port, the setup hotspot name when there is no Wi-Fi after 20 s, and a hint after 30 s (90 s after a site switch) to check that the display is added in that Home Assistant. Labels are only set on a change.
- The boot page is `scrollable: false` + `scrollbar_mode: 'off'`: it scrolled sideways at boot (scrollbar at the bottom, everything ~10 cm to the left) once the site buttons were visible. The overflowing widget was not found; with scrolling off it cannot shift.

### Network and diagnostics (2026-11-01)
- `wifi: power_save_mode: none`: with the ESP32 default (light) the radio slept between beacons: ~90 % ping loss, slow commands, HA briefly "unavailable". The display is mains powered.
- `debug:` component + `uptime`: *Uptime*, *Reset Reason*, *Free Internal RAM*, *Largest Free RAM Block*, *Lowest Free Internal RAM* in HA. "other watchdogs" as reset reason means a hang, not a requested restart ("Reboot request from …").
- *Page Rotation* switch and *Page Rotation Interval (s)* number control the Menu → Interval rotation from HA.

### Settings in Home Assistant (2026-11-01)
- Every on-display setting except Wi-Fi has an HA entity, and every setting has `entity_category: config` (HA "Configuration" section); the raw HMMD pin and the debug/Wi-Fi sensors are diagnostic. The full list is in the README (*Settings in Home Assistant*).
- New: *Rotate …* switches (one bit each of `interval_pages_mask`, via `interval_page_set`, which refuses to clear the last page), *Home Usage Subpanel 1/2* switches (`hu_sp*_enabled`), *Home Usage … Icon* and *… Panel* selects (`hu_*_icon_idx`, `hu_dev*_route`).
- Two patterns: **switches** poll a lambda (template switches dedupe, so nothing is sent unless it changed). **Selects** do not dedupe, so they are not polled: `set_action` writes the global and runs `hu_publish_settings`, which publishes all Home Usage names/icons/routes from the globals. It also runs at boot (after the per-site swap) and after every on-display Home Usage edit (Save buttons, subpanel route buttons). A new on-display Home Usage control must call it too.
- `prefs_sync` (1 s, then `global_preferences->sync()`) saves a change made from HA right away instead of after `flash_write_interval`.
- Renamed: *homepage toggle* → *Original Home Page*, *Show Tips/Suggestions* → *Show Tips* (the `/` TODO). The restore state is keyed by name, so both start from their default once. Removed the never-updated *LD1125H Distance* sensor; *Screen Backlight* is internal (*Screen* + *Screen Brightness* replace it).
- All lambda-driven template switches have `restore_mode: DISABLED` (see §6).
- Tested on 192.168.10.13 over the API: categories, rotation page on/off incl. last-page guard, icon and panel selects set and restored, settings kept across restarts.

### README
ESPHome version requirement, an upgrade guide for people with modified YAMLs, OTA update instructions, a warning to generate your own API key, the single-inverter SolarEdge package, two Home Assistant sites, screen sleep modes, and the list of HA entities (*Settings in Home Assistant*).

---

## 4. Test status

One firmware, compiled on ESPHome 2026.10.0. All three of the owner's displays run the 2026-11-01 build:

| Display | Site | Tested |
|---|---|---|
| 192.168.10.13 `solar-display` (test display, owner's network) | 1 | boots, Wi-Fi, HA API, touch, layout; earlier: glitching/scrollbar fixed, 10/10 restarts with working touch, performance confirmed. 2026-11-01: all HA settings set/restored over the API, settings survive restarts, site switch both ways, boot screen no longer shifts (confirmed by the owner) |
| 192.168.2.164 `solar-display-dev7b` (SolarEdge site, VPN) | 1 | flashed, two sites on, survives a restart, name kept |
| 192.168.1.92 `solar-display-bb59` (Sigenergy site, VPN) | 2 (6055) | flashed, two sites on (by itself), survives a restart, name kept |

Build sizes: RAM ~58 %, flash ~49 % of the 8 MB app partition. Internal RAM at runtime with the 16-line draw buffer: ~50 KB free, lowest ~36 KB on .13 after boot (was 23 KB with 20 lines; .92 had dipped to 14.9 KB).

---

## 5. How to build and update

```bash
python -m pip install --upgrade esphome        # 2026.7.0 or newer
cd Firmware
esphome compile solar-display.yaml
esphome upload solar-display.yaml --device <display-ip>   # OTA over Wi-Fi
esphome logs solar-display.yaml --device <display-ip>     # live logs
```

- **The owner's displays** (ESPHome venv at `~/esphome-venv`): always pass the site 2 key, plus the per-display names. Compile first; `upload` does not build.
  ```bash
  K2=$(python -I -c "import yaml;print(yaml.safe_load(open('secrets.yaml'))['api_encryption_key_site_2'])")
  # .13:  esphome -s api_key_site_2 "$K2" compile|upload solar-display.yaml --device 192.168.10.13
  # .164: esphome -s device_name solar-display-dev7b -s friendly_name solar-display-dev7b -s default_site 0 -s api_key_site_2 "$K2" …  --device 192.168.2.164
  # .92:  esphome -s device_name solar-display-bb59 -s friendly_name "Solar Display BB59" -s default_site 1 -s api_key_site_2 "$K2" …  --device 192.168.1.92
  ```
  Test on .13 first; the other two are behind a site-to-site VPN (~100 ms) in sealed cases.
- First build downloads ESP-IDF (~minutes); later builds are fast. A 16-core PC is fine; a Raspberry Pi is not.
- OTA upload of ~3.9 MB takes ~20 s (longer over the VPN).
- `esphome logs` connects through the API, so it doubles as a test that `api_encryption_key` in `secrets.yaml` matches the device. Run it **before** uploading.
- Safe mode is active (comes with OTA): after 10 failed boots the device starts a minimal Wi-Fi + OTA system for 5 minutes. A boot counts as good after 60 s.
- Wi-Fi credentials entered on the device are kept across OTA updates; there's also a fallback hotspot.
- ESPHome **2025.10.0 cannot be installed on Python 3.14** (fails building a dependency). Use Python ≤ 3.13 if you ever need to build the old `main` for comparison.

---

## 6. Gotchas

- **`secrets.yaml` is never committed.** Copy `Firmware/secrets.example.yaml` to `Firmware/secrets.yaml`; the root `.gitignore` ignores every `secrets.yaml`. Until 2026-10-31 the secrets files were tracked, and the API key committed then (2026-02-07) is still in the public git history. Devices that use that key should get a new one; changing it on an existing device means updating the key in Home Assistant too.
- **`ota_password` vs encryption:** ESPHome 2026 warns that an OTA password wastes flash/RAM and recommends `ota: encryption:` instead. The plaintext OTA fallback is removed in **2027.3.0**, so switch before then (and update every display once with the new config).
- **Internal RAM is the scarce resource**, not PSRAM. The draw buffer takes 32 KB of it (16 lines); keep ~25–35 KB free for Wi-Fi, the API and OTA. On 2026-11-01 it went from 20 to 16 lines: with 20, the *Lowest Free Internal RAM* sensor read 23 KB on .13 and 14.9 KB on .92. Check that sensor before raising it again.
- `lv_label_set_text()` in the YAML lambdas is a macro from `lvgl_helpers.h` that skips identical text (see §3 Performance).
- **The firmware may only read entities from `display-entities.txt`.** Reading an integration entity directly (e.g. `solaredge_…`, `sigen_plant_…`) ties the firmware to one brand again. Wrap it in a package sensor with a shared name instead.
- **New UI colours:** write them in theme 0's colours. If a lambda sets a UI (role) colour at runtime, wrap it in `lvx_theme::color(...)`; data/warning colours stay plain `lv_color_hex(...)`.
- **Adding a main page:** add it to the `pages[]` list in the touchscreen's `on_update` (edge swipe) as well as to the menu.
- **Moving a node icon on the Power Flow page** (`node_solar`, `node_home`, `node_battery`, `node_grid`): also update the node centres at the top of the `solar_info_page` animation lambda (image `x`/`y` + 50), or the dots no longer line up with the icons.
- **Template switches that mirror a setting** (`lambda:` + `turn_on_action`/`turn_off_action`) need `restore_mode: DISABLED`. Without it the switch runs its `turn_off_action` once at every boot: that silently turned *Two Home Assistant Sites* off on every restart, and cleared the rotation pages and Home Usage subpanels (fixed 2026-11-01).
- The boot page is `scrollable: false`: it scrolled sideways at boot (scrollbar at the bottom, everything shifted left); the overflowing widget was not found.
- Keep `loading_overlay` small and transparent. Making it full-screen again (e.g. to dim the page) brings back two extra full-screen redraws on every page change.

---

## 7. Open TODOs (none of these currently break anything)

| Item | Deadline / reason |
|---|---|
| Move OTA from `password` to `encryption` | plaintext OTA fallback removed in **2027.3.0** |
| Confirm touch also starts reliably after a real power cut (only software restarts were tested) | the GT911 fix relies on the chip coming up by itself at power-on |
| Confirm the HA migrations (`Homeassistant/migration/HANDOVER-*.md`) ran on both Home Assistants | all displays now run the one firmware and read the new names |
| .13: choose the rotation pages again (only *Graph* is left; the old switch bug cleared the rest) | owner will do it later |
| .164: site names are *SolarEdge* / *Sigenergy* (from `secrets.yaml`), .13 uses *EW14* / *BB59*; align if wanted | cosmetic |
| Check *Lowest Free Internal RAM* on .92 and .164 after a few hours | first runtime with the 16-line draw buffer there |
| Find what made the boot page wider than the screen | scrolling is off, so it no longer shows |
| Rotation page selection (`interval_pages_mask`) and the theme are not per site | only Home Usage is |
| Optional: shorten the 250 ms spinner delay in the navigation handlers (e.g. to 100 ms) | it is now most of a page change; the owner kept 250 ms for now |
| Graph page: replace the 24 `lv_obj_align_to()` calls for the value labels with computed positions | a graph refresh still takes ~85 ms every 750 ms while the graph page is open |
| Remove `ch422g` leftovers | cleanup (unused fonts/icons removed 2026-11-03) |
| From the README: configurable suggestion texts/thresholds, interval page graphs | features |

---

## 8. Where we left off (2026-11-03)

**Repo state**
- 2026-11-03: site button on the home page (`hp2_btn_site`), README rewritten (how it works: one firmware, only the Home Assistant package differs per inverter; repository map), unused fonts/icons and the finished SolarEdge migration note removed. Flashed on .13 only (compiled 17:57); .164 and .92 still run the 2026-11-01 build.
- Committed and pushed to GitHub `main`: optional two-site mode, per-site Home Usage settings, screen sleep modes, boot status screen, all settings in HA, the switch restore fix, the 16-line draw buffer, docs.
- All three displays run that build with two sites on (see §4).

**Done from the 2026-10-07 roadmap:** 1. one firmware for all brands (2026-10-31, §3 One firmware).

**Still open from that roadmap** (discussed, not started)
- **SolarEdge 1 or 2 inverters in one package:** treat I2 as optional (missing entities already read as 0 with `float(0)`; fix the `is_state(..., 'unknown')` guards), keep the "Panel To House" clamps, and detect grid charging from I1's DC power going negative instead of "AC output <= 0" (more reliable with one hybrid inverter).
- **Systems without a battery:** package treats a missing B1 as 0 (today "Solar Panel Production W" is forced to 0 when B1 is unknown); firmware gets a "Battery installed" setting that hides the battery card on the home page (the flex row re-centres the other three), the battery node/flows on the info page, the battery page and its menu button, the battery graph datasets, the "Remaining Battery" line and the battery-related tips. Testable on the owner's own display by switching the setting off.

**Open questions for the owner**
- Which other inverter brands should the display contract cover?
- Private WiFi network names were removed from the files but are still in old git history; leave as is (low risk) or rewrite history (needs a force-push)?
