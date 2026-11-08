#!/usr/bin/env python3
"""Controlla che tutti i 444 WAV del kit siano davvero nella cartella del Mac."""
import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", type=Path, default=Path.home() / "Music" / "Sculpt Drums")
    args = parser.parse_args()
    kit_file = args.root.expanduser() / "kit.json"
    if not kit_file.is_file():
        parser.error(f"Manca il nuovo manifest: {kit_file}")
    kit = json.loads(kit_file.read_text(encoding="utf-8"))
    if kit.get("version") != 2:
        parser.error("kit.json versione 1: copiare il nuovo kit/kit.json (versione 2)")

    base = args.root.expanduser() / kit.get("sampleFolder", ".")
    missing = []
    count = 0
    for piece in kit["pieces"]:
        for stage in piece["stages"]:
            for entry in stage:
                for relative in entry.values():
                    count += 1
                    if not (base / relative).is_file():
                        missing.append(relative)

    print(f"WAV nel manifest: {count}")
    print(f"WAV mancanti: {len(missing)}")
    for name in missing[:30]:
        print(f"  MANCANTE: {name}")
    if missing:
        raise SystemExit(1)
    print("Libreria completa: cartelle e file corrispondono al nuovo kit.")


if __name__ == "__main__":
    main()
