#!/usr/bin/env python3
"""Controlla che i WAV nel repository corrispondano al manifest kit/kit.json."""
import argparse
import json
from collections import Counter
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", type=Path, default=Path("."))
    args = parser.parse_args()
    kit_file = args.root.expanduser() / "kit" / "kit.json"
    if not kit_file.is_file():
        kit_file = args.root.expanduser() / "kit.json"
    if not kit_file.is_file():
        parser.error(f"Manca il nuovo manifest: {kit_file}")
    kit = json.loads(kit_file.read_text(encoding="utf-8"))
    if kit.get("version") != 2:
        parser.error("Serve il manifest kit.json versione 2")

    base = args.root.expanduser() / kit.get("sampleFolder", ".")
    missing = []
    referenced = []
    for piece in kit["pieces"]:
        for stage in piece["stages"]:
            for entry in stage:
                for relative in entry.values():
                    referenced.append(relative)
                    if not (base / relative).is_file():
                        missing.append(relative)

    actual = {str(path.relative_to(base)) for path in base.rglob("*.wav")}
    extra = sorted(actual - set(referenced))
    duplicates = [path for path, count in Counter(referenced).items() if count > 1]
    print(f"WAV nel manifest: {len(referenced)}")
    print(f"WAV sul disco: {len(actual)}")
    print(f"WAV mancanti: {len(missing)}")
    for name in missing[:30]:
        print(f"  MANCANTE: {name}")
    if extra:
        print(f"WAV non mappati: {extra[:20]}")
    if duplicates:
        print(f"WAV mappati più volte: {duplicates[:20]}")
    if missing or extra or duplicates or len(referenced) != kit.get("sampleFiles"):
        raise SystemExit(1)
    print("Libreria completa: cartelle e file corrispondono al nuovo kit.")


if __name__ == "__main__":
    main()
