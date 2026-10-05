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
| `*/fonts`, `*/icons` | Fonts and PNG icons used by the firmware (duplicated per folder) |
| `Homeassistant/sigenergy/packages/` | HA packages (`energy.yaml`, `energydevices.yaml`) for Sigenergy |
| `Homeassistant/solaredge/2 inverters/packages/` | HA packages for SolarEdge with I1 + I2 |
| `Homeassistant/solaredge/single inverter/packages/` | HA packages for SolarEdge with I1 only |
| `Solaredge/energy.yaml` | Exact duplicate of the 2-inverter HA `energy.yaml` (left over; can be removed) |
| `3Dfiles/Solar-Case.3mf` | Case model |

**There is one screen, two firmwares.** Both YAML files target the same 7B display; they differ only in which inverter entities they read. The two files are ~95% identical, so **every firmware fix has to be applied to both files**. Check with:

```bash
diff <(git diff main -- Sigenergy/solar-display.yaml | grep '^[-+][^-+]') \
     <(git diff main -- Solaredge/solar-display.yaml | grep '^[-+][^-+]')
```

(empty output = both files got the same changes). The commented-out `ch422g` lines are left over from a config for the older 800×480 Waveshare 7" board; that board is not supported.

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

## 3. Everything that was changed (branch `esphome-2026-update`, merged to `main`)

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
- **Gap between the 4 boxes and the summary card** (exported today / remaining battery / self use today): `hp2_summary_card` moved from `y: 400` to `y: 410`, making the gap 16 px, the same as `pad_column` between the cards. Layout math: `hp2_row` y=88, h=325, cards h=288, vertically centred → cards end at ~y=394. Summary card is 119 px tall → ends at ~529; the tips label (`hp2_lbl_suggestion`, `my_font_medium` 38 px, `bottom_mid` y=-10) starts ~545. A tip that wraps to two lines would grow upward towards the card.

### Deprecations cleared and touch start-up fixed
- `image:` block converted to the new format (`- platform: file` per image, 33 images).
- `zoom:` → `scale:` on all 43 images (same values).
- Display driver `rpi_dpi_rgb` → `mipi_rgb` with `model: RPI` (generic model, no init sequence) and the original pins/timings. `mipi_rgb` sets up the panel exactly like `rpi_dpi_rgb` (PSRAM framebuffer, 10-line bounce buffer, 1 FB). There is no built-in 7B model; `WAVESHARE-5-1024X600` uses the same pins but different timings and a CH422G reset pin. `setup_priority: 800` keeps the old start-up order (`rpi_dpi_rgb` is HARDWARE priority, `mipi_rgb` uses the default).
- **Touch randomly failing to start** (`touchscreen is marked FAILED: Calibration error`, touch dead until the next reboot). Measured with repeated software restarts: `rpi_dpi_rgb` 3/8 failed, `mipi_rgb` 6/8 failed, so the bug was already there and the new driver only made it more likely.
  Cause: ESPHome's GT911 `setup()` pulses reset (EXIO1 via the CH32V003), holds INT low, then 56 ms later switches INT to input and **immediately** talks I2C. The GT911 datasheet wants ≥50 ms *after* releasing INT, so the chip is sometimes not ready yet.
  Fix: removed `reset_pin` from the `gt911` touchscreen. The chip is already running at 0x5D at boot (seen in the I2C scan before any reset), and without the reset ESPHome talks to it directly. Result: **0/10 failures** on `mipi_rgb`.
  The reboot test scripts used for this (press the "Restart Device" button through the API, wait, then grep `esphome logs` for `touchscreen is marked FAILED`) were throwaway tools and are not in the repo.

### README
ESPHome version requirement, an upgrade guide for people with modified YAMLs, OTA update instructions, a warning to generate your own API key, and the single-inverter SolarEdge package.

---

## 4. Test status

| Firmware | Compiles on 2026.9.1 | Tested on hardware |
|---|---|---|
| SolarEdge | yes | **yes**: boots, Wi-Fi, HA API, PSRAM, touch, layout; glitching and scrollbar confirmed fixed by the owner; `mipi_rgb` + no touch reset: 10/10 software restarts with working touch |
| Sigenergy | yes | **no**: identical changes, but nobody has run it on a display yet |

Build sizes: RAM ~54%, flash ~45% of the 8 MB app partition.

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

---

## 7. Open TODOs (none of these currently break anything)

| Item | Deadline / reason |
|---|---|
| Move OTA from `password` to `encryption` | plaintext OTA fallback removed in **2027.3.0** |
| "Show Tips/Suggestions" switch name: remove the `/` | becomes an error in **2027.7.0** |
| Confirm touch also starts reliably after a real power cut (only software restarts were tested) | the GT911 fix relies on the chip coming up by itself at power-on |
| Test the Sigenergy firmware on real hardware | only compile-tested |
| Share common YAML between the two firmwares (ESPHome `packages:`) | removes the "fix it twice" problem |
| Remove `Solaredge/energy.yaml` duplicate, `ch422g` leftovers, `old_icon_sun_100.png` | cleanup |
| From the README: configurable suggestion texts/thresholds, interval page graphs | features |
