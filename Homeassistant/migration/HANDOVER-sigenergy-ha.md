# Handover: Sigenergy Home Assistant – rename to the shared display entity names

**For:** an AI agent working on the Home Assistant instance that runs the **Sigenergy** energy package for the Solar Display (ESPHome, Waveshare 7" screen).
**Repo:** https://github.com/ckegels/Solar-Monitor (public). Raw files: `https://raw.githubusercontent.com/ckegels/Solar-Monitor/main/<path>`.

## Why this change

There will be **one display firmware** for SolarEdge and Sigenergy. Both packages provide the **same entity ids**, and the firmware reads only those (`Homeassistant/migration/display-entities.txt`, 244 entities). The SolarEdge names are the standard.

On this instance, **219 entities created by the Sigenergy package get a new entity_id**. Most are `package_sigen_…` → `solar_…`, and "pv" becomes "panel", for example:
- `sensor.package_sigen_pv_to_house_w` → `sensor.solar_panel_to_house_w`
- `sensor.package_sigen_pv_power` → `sensor.solar_panel_production_w`

One sensor is added: `sensor.solar_lifetime_energy`.

**Goal: no history is lost, and nothing on this instance still points at an old id.** That covers dashboards, automations, scripts, helpers, the Energy dashboard and anything else.

## Files you need (from the repo, folder `Homeassistant/migration/`)

| File | What it is |
|---|---|
| `sigenergy-entity-renames.csv` | `old_entity_id,new_entity_id,kind`: the complete list (219 rows). Kinds: `template` (16), `utility_meter` (28), `utility_meter_tariff` (168, the hourly `…_h00`–`…_h23` sensors), `utility_meter_select` (7). |
| `rewrite_sigenergy_package.py` | Rewrites the package file in two phases. Python 3 standard library only. It makes a backup each run and refuses to run a phase twice. |
| `display-entities.txt` | The 244 entities the new firmware reads, for the final check. |

Put the three files together in one folder, e.g. `/config/solar-migration/`. The script looks for the CSV next to itself.

## Rules

- **Do not touch** the ESPHome devices, their API keys or OTA passwords, the ESPHome add-on configs, or the Sigenergy integration. The repo maintainer flashes the displays.
- **Only rename the entities in the CSV.** Other `package_sigen_…` entities stay as they are: the `input_select`/`input_number` helpers, `package_sigen_*_kwh` integration sensors, `package_sigen_battery_soc_hours`, etc.
- **Keep the live `energydevices.yaml`.** The repo version only contains example sensor names.
- **Never change an existing `unique_id`.** Renames happen in the entity registry, which keeps history and long-term statistics.
- **Never hand-edit** `.storage/core.entity_registry` or `.storage/core.restore_state`. Use the UI or the WebSocket API.
- Edit `.storage/*` files (dashboards, energy) **only while HA Core is stopped** (`ha core stop`). Otherwise HA overwrites them. Prefer the UI or WebSocket API.
- **Stop and report to the user if** a check fails, an entity gets a `_2` suffix, a target id already exists, or the live package differs from the repo in a way not described here.
- **Timing:** from step 7 until the displays are flashed, displays on the old firmware show "--". Agree a time with the user first. The maintainer can flash right after step 7, which takes about 5 minutes per display.

## Steps

### 1. Back up
Make a full Home Assistant backup (Settings → System → Backups) and keep it until the user confirms everything works. Also copy the package files (`energy.yaml`, `energydevices.yaml`) to `*.bak-<date>`.

### 2. Compare the live package with the repo
Compare the live package with the **pre-migration** repo version from commit `c3d9054` (2026-10-07):
`https://raw.githubusercontent.com/ckegels/Solar-Monitor/c3d9054/Homeassistant/sigenergy/packages/energy.yaml`

Report every difference to the user before continuing. The script works on the live file, so small local changes are fine, but the user should know about them.

The current `main` version of that file is the **post-migration** state. After step 7, your file should match it, apart from the local changes found here.

### 3. Find every reference (read-only)
Search all of `/config` for each old id from the CSV. Also search for the **utility meter keys**: the old object id without the domain, e.g. `package_sigen_pv_daily`, plus the hourly bases such as `package_sigen_pv_by_hour`. Match whole ids only: an id must not be preceded or followed by `[A-Za-z0-9_]`. Check longer ids first.

Include at least:
- `.storage/lovelace*` (UI dashboards), plus YAML dashboards (`ui-lovelace.yaml`, any `dashboards/` folder)
- `.storage/energy` (Energy dashboard)
- `.storage/core.config_entries` (UI helpers: template, utility_meter, statistics, group, threshold, derivative, …)
- `automations.yaml`, `scripts.yaml`, `scenes.yaml`, `customize*.yaml`, `configuration.yaml` (recorder/history/logbook/influxdb include/exclude), all other packages
- `custom_templates/`, Node-RED flows, AppDaemon apps, `www/` card configs, and anything else that holds entity ids

Skip: the Sigenergy package itself (the script handles it), `.storage/core.entity_registry`, `.storage/core.restore_state`, `home-assistant_v2.db`, logs, backups.

Write the result to `/config/solar-migration/references-before.txt` (file, line, id). This is your to-do list for step 9.

### 4. Snapshot the current values
Save the state of every `old_entity_id` in the CSV to `/config/solar-migration/snapshot.json`, with entity_id, state and last_changed. Use the REST API `GET /api/states` or your HA tools. The utility meter values are needed in step 8.

### 5. Phase A: register the utility meters and kWh sensors

```bash
python3 /config/solar-migration/rewrite_sigenergy_package.py phase-a /config/packages/energy.yaml
```

(Use the real path of the package.) This **only** adds `unique_id`s:
- 35 utility meters get `energy_um_<new key>`
- 7 integration sensors get `energy_int_<name>`

Names stay the same. Then:
1. Developer Tools → YAML → **Check configuration**. It must pass.
2. **Restart** Home Assistant. Utility meters and integration sensors only change on a restart.
3. Verify:
   - **Every** old entity id in the CSV still exists under exactly the same id, with no `_2`.
   - Values continue from the snapshot: daily values the same or higher, `…_kwh` sensors not reset.
   - The meters now appear in Settings → Entities and can be edited, which means they are in the registry.

   If any id changed or got `_2`, stop. Restore `energy.yaml.bak-phase-a`, restart and report.

### 6. (Coordinate with the user) Start of the switch-over
Tell the user that phase B starts now. Their displays will show "--" until they're flashed.

### 7. Phase B: rename

1. **Rename all 219 entities in the entity registry** while HA is running, one call per CSV row. WebSocket:
   ```json
   {"id": 1, "type": "config/entity_registry/update",
    "entity_id": "sensor.package_sigen_house_consumption_w",
    "new_entity_id": "sensor.solar_house_consumption_w"}
   ```
   Or in the UI: Settings → Entities → entity → ⚙ → Entity ID. The recorder moves history and long-term statistics to the new id. If a call fails (for example "entity id already in use"), stop and report. Do not delete anything.
2. Rewrite the package:
   ```bash
   python3 /config/solar-migration/rewrite_sigenergy_package.py phase-b /config/packages/energy.yaml
   ```
   This changes names, utility-meter keys, every reference inside the templates and the hourly-tariff automation's `select.…` targets. It adds the `solar_lifetime_energy` sensor and leaves `unique_id` lines alone. It aborts if an old id is left over.
3. **Check configuration**. It must pass. Then **restart**.

### 8. Verify
- **Display entities.** Every id in `display-entities.txt` exists and is not `unavailable`. Use the Developer Tools → Template loop below with the list pasted in. Hourly sensors for hours that haven't happened yet may be `0`.
  ```jinja
  {% set ids = ['sensor.solar_house_consumption_w', '...'] %}
  {% for e in ids if states(e) in ['unknown', 'unavailable'] or states[e] is none %}{{ e }} = {{ states(e) }}
  {% endfor %}
  ```
- **No leftovers.** No entity from the CSV's `old_entity_id` column exists any more, and none came back as a new entity.
- **History.** Open the history of a few renamed sensors, e.g. `sensor.solar_house_consumption_w` and `sensor.solar_house_consumption_monthly`. Data from before today must be there.
- **Utility meters.** Compare each meter (`utility_meter` and `utility_meter_tariff` rows) with `snapshot.json`.
  - If a meter is **lower** than its snapshot value (it was reset), set it back:
    ```yaml
    action: utility_meter.calibrate
    target:
      entity_id: sensor.solar_house_consumption_monthly
    data:
      value: "123.456"   # snapshot value
    ```
  - For daily and hourly meters, only calibrate if the snapshot is from today and, for hourly ones, from the same hour.
- **Hourly tariffs.** `select.solar_*_by_hour` show the current hour (`H00`–`H23`). The automations "UM - Set hour tariff on start (all)" and "UM - Rotate hour tariffs (all)" have no errors in their traces.
- **New sensor.** `sensor.solar_lifetime_energy` has a kWh value close to `sensor.sigen_plant_total_pv_generation`, converted to kWh.

### 9. Update everything else that used the old ids
Work through `references-before.txt`. Replace each old id with the new one, longest ids first and whole ids only:
- **UI dashboards:** dashboard → ✎ → ⋮ → Raw configuration editor, or WebSocket `lovelace/config` + `lovelace/config/save` (with `url_path` for non-default dashboards). Only edit `.storage/lovelace*` with Core stopped.
- **Energy dashboard:** Settings → Dashboards → Energy, or WebSocket `energy/get_prefs` / `energy/save_prefs`. It normally uses the `package_sigen_*_kwh` sensors or the integration's own sensors, which are not renamed. Check anyway.
- **Automations / scripts / scenes:** edit the YAML (or the UI editor), then reload automations, scripts and scenes.
- **UI helpers** (`core.config_entries`): change them in the helper's options dialog in the UI.
- **Other YAML** (packages, customize, recorder filters, Node-RED, AppDaemon): edit, then reload or restart as needed.

Then search again as in step 3. **No old id may be left**, except in backups and in this migration folder. Save the result as `references-after.txt`.

### 10. Report back
Give the user:
- the diff of the package file (live before → after)
- the step 5 and step 8 checks, and any calibrations you made
- `references-before.txt` and `references-after.txt`, and what you changed in dashboards, automations, etc.
- anything that differed from this handover

The maintainer then flashes the displays with the new firmware.

## Rollback
- **Before step 7:** restore `energy.yaml.bak-phase-a` and restart. Registering the meters does no harm, even if it stays.
- **After step 7:** restore the full backup from step 1. That restores the entity registry and all files. Data recorded since the backup is lost. Or rename back via the registry using the CSV in reverse, restore `energy.yaml.bak-phase-b`, and restart.
