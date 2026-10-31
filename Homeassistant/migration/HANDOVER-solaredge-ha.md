# Handover: SolarEdge Home Assistant – shared display entity names

**For:** an AI agent working on the Home Assistant instance that runs the **SolarEdge** energy package for the Solar Display (ESPHome, Waveshare 7" screen).
**Repo:** https://github.com/ckegels/Solar-Monitor (public). Raw files: `https://raw.githubusercontent.com/ckegels/Solar-Monitor/main/<path>`.

## Why this change

There will be **one display firmware** for SolarEdge and Sigenergy. The two Home Assistant packages provide the **same entity ids**, and the firmware reads only those (full list: `Homeassistant/migration/display-entities.txt`, 244 entities).

The SolarEdge package already provides all of them except two. Until now the firmware read those two directly from the SolarEdge Modbus Multi integration. **Nothing is renamed on this instance.** Dashboards, automations and history are not affected.

## What not to touch

- The ESPHome devices, their API keys or OTA passwords, and the ESPHome add-on configs. The repo maintainer flashes the displays.
- Any SolarEdge integration settings.
- Existing `unique_id`s in the package.
- `energydevices.yaml`: keep the live one. The repo version only contains example sensor names.

## Steps

### 1. Back up
Make a Home Assistant backup (Settings → System → Backups), or at least copy `packages/energy.yaml` (the SolarEdge package, wherever it lives under `/config`) to `energy.yaml.bak-<date>`.

### 2. Check that the live package matches the repo
Compare the live package with `Homeassistant/solaredge/2 inverters/packages/energy.yaml` from the repo. Use `single inverter/` instead if this site has only one inverter, i.e. no `sensor.solaredge_i2_*` entities.

The repo version contains the two shared sensors from step 3 (`Solar Grid Connected`, `Solar Battery SoC`). That difference is always expected. If the live file was already updated from the repo on 2026-10-31, it is the only one. Otherwise, if the live file is the version from 2026-10-31 15:00, these differences are expected too:
- `Solar Panel To House W`: the repo clamps each branch at 0 (`[ … , 0] | max`) and returns 0 while `sun.sun` is `below_horizon`.
- `Solar Lifetime Production`: the repo reads `sensor.solar_lifetime_energy`; live reads `sensor.solaredge_ac_energy_kwh`.
- The repo has an extra template block, `Solar Lifetime Energy` (`sensor.solar_lifetime_energy`).
- One blank line in `Solar Battery To House W`.

If there are **other** differences, stop and report them to the user with the diff. Do not overwrite local changes.

### 3. Take the repo version and add the two shared sensors
Replace the live package with the repo version. Then append these two blocks at the **end of the `template:` list**, directly before the top-level `sensor:` line. If the repo version already contains them, skip this.

```yaml
  # --- Used by the display: shared names for grid status and battery SoC (same ids as the Sigenergy package) ---
  - binary_sensor:
      - name: "Solar Grid Connected"
        unique_id: solar_grid_connected
        device_class: connectivity
        state: "{{ is_state('binary_sensor.solaredge_i1_grid_status', 'on') }}"
        availability: "{{ states('binary_sensor.solaredge_i1_grid_status') in ['on', 'off'] }}"

  - sensor:
      - name: "Solar Battery SoC"
        unique_id: solar_battery_soc
        unit_of_measurement: "%"
        device_class: battery
        state_class: measurement
        state: "{{ states('sensor.solaredge_b1_state_of_energy') | float(0) | round(1) }}"
        availability: "{{ states('sensor.solaredge_b1_state_of_energy') | is_number }}"
```

The resulting entity ids **must** be exactly `binary_sensor.solar_grid_connected` and `sensor.solar_battery_soc`. If either already exists (for example with a `_2` suffix), stop and report.

### 4. Apply
- Developer Tools → YAML → **Check configuration**. It must pass.
- Then **restart** Home Assistant, not just a reload. The repo version can differ in `integration`/`utility_meter` sections, and those only change on a restart.

### 5. Verify
- `binary_sensor.solar_grid_connected` is `on` while the grid is up, and follows `binary_sensor.solaredge_i1_grid_status`.
- `sensor.solar_battery_soc` equals `sensor.solaredge_b1_state_of_energy`, rounded to 0.1.
- `sensor.solar_lifetime_energy` has a kWh value; `sensor.solar_lifetime_production` is that value ÷ 1000, in MWh.
- Every entity in `display-entities.txt` exists and is not `unavailable`. Run this in Developer Tools → Template, with the list pasted into `ids`:

  ```jinja
  {% set ids = ['sensor.solar_house_consumption_w', '...'] %}
  {% for e in ids if states(e) in ['unknown', 'unavailable'] or states[e] is none %}{{ e }} = {{ states(e) }}
  {% endfor %}
  ```

  Hourly sensors for hours that haven't happened yet today may read `0`; that's fine. Report anything that is missing or `unavailable`.

### 6. Report back
Tell the user what changed (the diff), the result of the checks, and anything that differed from this handover. The maintainer then flashes the new firmware. Displays still on the old firmware keep working, because no existing entity changed.

## Rollback
Restore `energy.yaml.bak-<date>` (or the backup) and restart.
