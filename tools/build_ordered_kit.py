#!/usr/bin/env python3
"""Crea il kit a sei stadi dai WAV inclusi nel repository.

Uso:
    python3 tools/build_ordered_kit.py '/percorso/Sculpt Drums.zip' -o kit/kit.json
    python3 tools/build_ordered_kit.py Samples -o kit/kit.json
    python3 tools/build_ordered_kit.py lista-wav.txt -o kit/kit.json

I percorsi nel manifest sono relativi alla cartella Samples inclusa nel VST3.
"""

import argparse
import json
import re
import zipfile
from collections import Counter
from pathlib import Path

STAGES = (
    "01 Natural",  # nello ZIP originale c'è uno spazio finale, qui normalizzato
    "02 Natural",
    "03 processed",
    "04 processed",
    "05 processed",
    "06 Electro",
)

# Un solo kit MIDI, a prescindere dal numero di batterie registrate.
PIECES = (
    ("kick", 36, 1.00, 0),
    ("snare", 38, 0.85, 0),
    ("snare_rim", 37, 0.85, 0),
    ("snare_rimshot", 40, 0.85, 0),
    ("clap", 39, 1.00, 0),
    ("hihat_closed", 42, 1.20, 1),
    ("hihat_foot", 44, 1.20, 1),
    ("hihat_open", 46, 1.20, 1),
    ("tom", 45, 1.00, 0),
    ("floor_tom", 41, 1.00, 0),
    ("crash", 49, 1.10, 0),
    ("reverse_cymbal", 52, 1.10, 0),
    ("ride", 51, 1.10, 0),
    ("ride_bell", 53, 1.10, 0),
    ("splash", 55, 1.10, 0),
    ("shaker", 70, 1.00, 0),
    ("percussion", 75, 1.00, 0),
    ("combo", 76, 1.00, 0),
)


def names_from_source(source: Path):
    if source.is_file() and source.suffix.lower() == ".txt":
        return sorted(name for line in source.read_text(encoding="utf-8").splitlines()
                      if (name := line.rstrip("\r")))
    if source.is_file() and source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive:
            return sorted(
                name.removeprefix("Sculpt Drums/")
                for name in archive.namelist()
                if name.startswith("Sculpt Drums/")
                and name.lower().endswith(".wav")
                and not name.startswith("__MACOSX/")
            )
    if source.is_dir():
        return sorted(str(file.relative_to(source)) for file in source.rglob("*.wav"))
    raise ValueError(f"Archivio ZIP o cartella non trovata: {source}")


def classify(stage: int, relative: str) -> str:
    parts = relative.split("/")
    name = parts[-1].lower().removesuffix(".wav")
    if stage == 5:
        category = parts[1].lower()
        if category == "kick": return "kick"
        if category == "snare": return "snare"
        if category == "clap": return "clap"
        if category == "tom": return "tom"
        if category == "shaker": return "shaker"
        if category == "combo": return "combo"
        if category == "percussion": return "percussion"
        if category == "hihat": return "hihat_open" if name.startswith("openhh") else "hihat_closed"
        if category == "cymbal":
            if name.startswith("revcymbal"): return "reverse_cymbal"
            return "ride" if name.startswith("ride") else "crash"
        raise ValueError(f"Categoria elettronica non riconosciuta: {relative}")

    if "rimshot" in name: return "snare_rimshot"
    if "bordo" in name or "rim" in name: return "snare_rim"
    if "ride bell" in name: return "ride_bell"
    if "hh op" in name or "hithat open" in name or "hihat open" in name: return "hihat_open"
    if "hh f" in name or "hithat piede" in name or "hihat piede" in name: return "hihat_foot"
    if name.startswith("hh ") or "hithat chiuso" in name or "hihat chiuso" in name:
        return "hihat_closed"
    if "crash" in name: return "crash"
    if "splash" in name: return "splash"
    if "ride" in name: return "ride"
    if "timpano" in name or name.startswith("tim ") or name.startswith("tom2") or name.startswith("tom2_"):
        return "floor_tom"
    if "tom" in name: return "tom"
    if "snare" in name or "rullo" in name or re.fullmatch(r"s [0-9]+", name): return "snare"
    if "kick" in name or "cassa" in name or re.fullmatch(r"k [0-9]+", name): return "kick"
    raise ValueError(f"Nessun pezzo MIDI identificato per {relative}")


def canonical_path(relative):
    """Rimuove solo lo spazio finale della prima cartella dello ZIP originale."""
    parts = relative.split("/", 1)
    if len(parts) == 2 and parts[0] == "01 Natural ":
        return "01 Natural/" + parts[1]
    return relative


def make_kit(wav_names):
    wav_names = [canonical_path(name) for name in wav_names]
    if len(wav_names) != len(set(wav_names)):
        raise ValueError("Due file collidono dopo la normalizzazione dei percorsi")
    samples = {name for name in wav_names}
    entries = {(piece, stage): [] for piece, *_ in PIECES for stage in range(6)}
    assigned = set()

    # Le riprese Indra hanno la stessa origine/evento. Close + OH + Room
    # devono avanzare insieme, non come tre varianti round robin distinte.
    natural_groups = {}
    for relative in wav_names:
        parts = relative.split("/")
        if len(parts) < 2 or parts[0] not in STAGES:
            raise ValueError(f"Percorso fuori dalle sei cartelle attese: {relative}")
        stage = STAGES.index(parts[0])
        piece = classify(stage, relative)

        if stage == 0:
            name = parts[-1][:-4]
            mic = re.search(r"\s+(over|room)$", name, flags=re.IGNORECASE)
            base = name[:mic.start()] if mic else name
            key = (piece, base.lower())
            group = natural_groups.setdefault(key, {})
            group["oh" if mic and mic.group(1).lower() == "over"
                  else "room" if mic else "file"] = relative
        else:
            entries[(piece, stage)].append({"file": relative})
        assigned.add(relative)

    for (piece, _), group in sorted(natural_groups.items()):
        if "file" not in group:
            raise ValueError(f"Ripresa OH/Room priva del close: {group}")
        entries[(piece, 0)].append(group)

    pieces = []
    for piece, note, gamma, choke in PIECES:
        stages = [sorted(entries[(piece, idx)], key=lambda e: e["file"]) for idx in range(6)]
        if any(stages):
            pieces.append(dict(id=piece, note=note, gamma=gamma, choke=choke, stages=stages))

    # Ogni file deve essere rappresentato nel manifest esattamente una volta,
    # anche quando è una componente microfonica di un solo evento.
    referenced = [path for p in pieces for stage in p["stages"]
                  for entry in stage for path in entry.values()]
    if Counter(referenced) != Counter(samples):
        raise ValueError(f"File persi o doppi: {Counter(samples) - Counter(referenced)}")

    return dict(version=2, sampleFolder="Samples",
                morphStages=[dict(name=name, position=i * 20) for i, name in enumerate(STAGES)],
                pieces=pieces, sampleFiles=len(samples))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("-o", "--output", type=Path, default=Path("kit/kit.json"))
    args = parser.parse_args()
    kit = make_kit(names_from_source(args.source.expanduser()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(kit, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{kit['sampleFiles']} WAV assegnati a {len(kit['pieces'])} pezzi MIDI")
    for piece in kit["pieces"]:
        print(f"{piece['note']:3} {piece['id']:18} " + " / ".join(str(len(s)) for s in piece["stages"]))


if __name__ == "__main__":
    main()
