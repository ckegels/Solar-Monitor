#!/usr/bin/env python3
"""Keep Sigenergy/solar-display.yaml identical to Solaredge/solar-display.yaml.

Also copies Solaredge/includes/ (the C++ headers the firmware uses) to Sigenergy/includes/.

The two firmwares are the same except for the Home Assistant entities they read.
Make changes in Solaredge/solar-display.yaml, then run from the repo root:

    python3 tools/sync_firmwares.py           # rewrite Sigenergy/solar-display.yaml
    python3 tools/sync_firmwares.py --check   # only check; exit 1 if out of sync

Every `platform: homeassistant` entry is matched by its ESPHome `id`. The Sigenergy
entity for that id is taken from the current Sigenergy file. When you add a new
Home Assistant sensor, add its Sigenergy entity to SIGENERGY_NEW below (or add the
sensor to the Sigenergy file by hand) before running this.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOLAREDGE = ROOT / "Solaredge" / "solar-display.yaml"
SIGENERGY = ROOT / "Sigenergy" / "solar-display.yaml"
INCLUDES_SRC = ROOT / "Solaredge" / "includes"
INCLUDES_DST = ROOT / "Sigenergy" / "includes"

# ESPHome id -> Sigenergy entity, for sensors that do not exist in the Sigenergy file yet
SIGENERGY_NEW: dict[str, str] = {}

BLOCK_SPLIT = re.compile(r"(\n(?=\s*- platform: ))")


def ha_entities(text: str) -> dict[str, str]:
    """ESPHome id -> entity_id for every `platform: homeassistant` entry."""
    found = {}
    for block in BLOCK_SPLIT.split(text):
        if not re.match(r"\s*- platform: homeassistant\b", block):
            continue
        ident = re.search(r"\n\s+id: (\w+)", block)
        entity = re.search(r"\n\s+entity_id: (\S+)", block)
        if ident and entity:
            found[ident.group(1)] = entity.group(1)
    return found


def build_sigenergy(solaredge: str, sigenergy: str) -> str:
    se_map = ha_entities(solaredge)
    sig_map = {**ha_entities(sigenergy), **SIGENERGY_NEW}
    missing = sorted(set(se_map) - set(sig_map))
    if missing:
        sys.exit("No Sigenergy entity known for: " + ", ".join(missing) + "\nAdd them to SIGENERGY_NEW in tools/sync_firmwares.py.")
    out = []
    for block in BLOCK_SPLIT.split(solaredge):
        if re.match(r"\s*- platform: homeassistant\b", block):
            ident = re.search(r"\n\s+id: (\w+)", block).group(1)
            block = re.sub(r"(\n\s+entity_id: )\S+", lambda m: m.group(1) + sig_map[ident], block, count=1)
        out.append(block)
    return "".join(out)


def stale_includes() -> list[Path]:
    """Headers in Solaredge/includes/ that are missing or different in Sigenergy/includes/."""
    return [src for src in sorted(INCLUDES_SRC.iterdir())
            if src.is_file() and (not (INCLUDES_DST / src.name).exists()
                                  or (INCLUDES_DST / src.name).read_bytes() != src.read_bytes())]


def main() -> None:
    solaredge = SOLAREDGE.read_text()
    sigenergy = SIGENERGY.read_text()
    wanted = build_sigenergy(solaredge, sigenergy)
    includes = stale_includes()
    if "--check" in sys.argv:
        if wanted == sigenergy and not includes:
            print("In sync: the firmwares differ only in Home Assistant entities, includes/ are identical.")
            return
        print("Out of sync: run python3 tools/sync_firmwares.py")
        sys.exit(1)
    for src in includes:
        (INCLUDES_DST / src.name).write_bytes(src.read_bytes())
        print(f"Copied {src.relative_to(ROOT)} to {INCLUDES_DST.relative_to(ROOT)}/.")
    if wanted == sigenergy:
        print("Firmware already in sync.")
        return
    SIGENERGY.write_text(wanted)
    print(f"Rewrote {SIGENERGY.relative_to(ROOT)} from {SOLAREDGE.relative_to(ROOT)}.")


if __name__ == "__main__":
    main()
