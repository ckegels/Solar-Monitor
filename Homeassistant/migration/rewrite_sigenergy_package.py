#!/usr/bin/env python3
"""Rewrite the Sigenergy package (energy.yaml) to the shared display entity names.

Two phases, run one after the other (see HANDOVER-sigenergy-ha.md):

    python3 rewrite_sigenergy_package.py phase-a /config/packages/energy.yaml
    python3 rewrite_sigenergy_package.py phase-b /config/packages/energy.yaml

phase-a  Only adds unique_ids to the utility meters (energy_um_<new key>) and the
         integration sensors (energy_int_<name>). Names stay as they are, so after a
         restart every entity keeps its current entity_id but is now in the entity
         registry (and can be renamed with its history).
phase-b  Renames every old object id from sigenergy-entity-renames.csv to the new one
         (keys, names, references, automation targets). unique_id lines are never
         changed. Adds the "solar_lifetime_energy" sensor the display reads.

Each run writes a backup next to the file (<file>.bak-phase-a / .bak-phase-b) and
refuses to run a phase twice. Only the Python standard library is used.
"""
import csv
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MAPPING = HERE / "sigenergy-entity-renames.csv"

LIFETIME_BLOCK = """
  # --- Used by the display: lifetime PV energy in kWh (shared name with the SolarEdge package) ---
  - sensor:
      - name: "solar_lifetime_energy"
        unique_id: solar_lifetime_energy
        icon: mdi:solar-power
        unit_of_measurement: "kWh"
        device_class: energy
        state_class: total_increasing
        state: >
          {% set e = 'sensor.sigen_plant_total_pv_generation' %}
          {% set v = states(e) | float(0) %}
          {% set unit = state_attr(e, 'unit_of_measurement') %}
          {{ (v / 1000 if unit == 'Wh' else (v * 1000 if unit == 'MWh' else v)) | round(2) }}
        availability: >
          {{ states('sensor.sigen_plant_total_pv_generation') | is_number }}
"""


def object_map() -> dict[str, str]:
    """old object id -> new object id, including the utility meter keys of the hourly meters."""
    out: dict[str, str] = {}
    with open(MAPPING, newline="") as f:
        for row in csv.DictReader(f):
            old = row["old_entity_id"].split(".", 1)[1]
            new = row["new_entity_id"].split(".", 1)[1]
            if row["kind"] == "utility_meter_tariff":
                old, new = old[:-4], new[:-4]  # strip _h00.._h23 -> utility meter key
            if out.get(old, new) != new:
                sys.exit(f"mapping conflict for {old}")
            out[old] = new
    return out


def section(text: str, key: str) -> tuple[int, int]:
    """Start/end offsets of a top-level YAML section (e.g. 'utility_meter')."""
    m = re.search(rf"^{key}:\s*$", text, re.M)
    if not m:
        sys.exit(f"no top-level '{key}:' section found")
    nxt = re.search(r"^[a-z_]+:\s*$", text[m.end():], re.M)
    return m.end(), (m.end() + nxt.start() if nxt else len(text))


def phase_a(text: str, omap: dict[str, str]) -> str:
    if "unique_id: energy_um_" in text or "unique_id: energy_int_" in text:
        sys.exit("phase-a was already applied (energy_um_/energy_int_ unique_ids present)")
    # utility meters: insert unique_id right after the key line
    s, e = section(text, "utility_meter")
    body = re.sub(r"^  (\w+):\s*\n",
                  lambda m: f"  {m.group(1)}:\n    unique_id: energy_um_{omap.get(m.group(1), m.group(1))}\n",
                  text[s:e], flags=re.M)
    text = text[:s] + body + text[e:]
    # integration sensors: insert unique_id after their name line
    s, e = section(text, "sensor")
    body = re.sub(r"(  - platform: integration\n(?:    .*\n)*?    name: (\w+)\n)",
                  r"\1    unique_id: energy_int_\2\n", text[s:e])
    return text[:s] + body + text[e:]


def phase_b(text: str, omap: dict[str, str]) -> str:
    if "unique_id: energy_um_" not in text:
        sys.exit("run phase-a first (and restart Home Assistant) before phase-b")
    if 'name: "solar_lifetime_energy"' in text:
        sys.exit("phase-b was already applied (solar_lifetime_energy present)")
    pattern = re.compile(r"(?<![\w])(" + "|".join(sorted(map(re.escape, omap), key=len, reverse=True)) + r")(?![\w])")
    lines = []
    for line in text.split("\n"):
        stripped = line.lstrip()
        if stripped.startswith("#") or stripped.startswith("unique_id:"):
            lines.append(line)  # never touch comments or unique_ids
        else:
            lines.append(pattern.sub(lambda m: omap[m.group(1)], line))
    text = "\n".join(lines)
    # add the lifetime sensor as the last block of the template: list
    s, _ = section(text, "sensor")
    insert_at = text.rfind("\nsensor:", 0, s)
    text = text[:insert_at].rstrip("\n") + "\n" + LIFETIME_BLOCK + text[insert_at:]
    left = [o for o in omap if re.search(rf"^(?!\s*(#|unique_id:)).*(?<![\w]){re.escape(o)}(?![\w])", text, re.M)]
    if left:
        sys.exit(f"old ids still present after rewrite: {left[:5]}")
    return text


def main() -> None:
    if len(sys.argv) != 3 or sys.argv[1] not in ("phase-a", "phase-b"):
        sys.exit(__doc__)
    phase, path = sys.argv[1], Path(sys.argv[2])
    text = path.read_text()
    omap = object_map()
    new = phase_a(text, omap) if phase == "phase-a" else phase_b(text, omap)
    backup = path.with_name(f"{path.name}.bak-{phase}")
    shutil.copy2(path, backup)
    path.write_text(new)
    print(f"{phase}: wrote {path} (backup: {backup})")


if __name__ == "__main__":
    main()
