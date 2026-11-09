#!/usr/bin/env python3
"""Importa i soli campioni WAV dell'archivio Sculpt Drums nel repository.

Uso: python3 tools/import_samples.py '/percorso/Sculpt Drums.zip'
Esclude file accessori e normalizza '01 Natural ' in '01 Natural'.
"""

import argparse
import hashlib
import zipfile
from pathlib import Path, PurePosixPath

from build_ordered_kit import STAGES, canonical_path


def import_wavs(source: Path, destination: Path) -> dict[str, str]:
    checksums = {}
    with zipfile.ZipFile(source) as archive:
        for entry in archive.infolist():
            if not entry.filename.startswith("Sculpt Drums/") or not entry.filename.lower().endswith(".wav"):
                continue
            original = entry.filename.removeprefix("Sculpt Drums/")
            relative = canonical_path(original)
            path = PurePosixPath(relative)
            if (len(path.parts) < 2 or path.parts[0] not in STAGES
                    or any(part in (".", "..") for part in path.parts)):
                raise ValueError(f"Percorso non valido: {entry.filename}")
            if relative in checksums:
                raise ValueError(f"Nome WAV duplicato: {relative}")
            output = destination.joinpath(*path.parts)
            output.parent.mkdir(parents=True, exist_ok=True)
            digest = hashlib.sha256()
            with archive.open(entry) as input_file, output.open("wb") as target:
                while block := input_file.read(1024 * 1024):
                    target.write(block)
                    digest.update(block)
            checksums[relative] = digest.hexdigest()
    if not checksums:
        raise ValueError("Nessun WAV trovato nell'archivio")
    return checksums


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--output", type=Path, default=Path("Samples"))
    args = parser.parse_args()
    digests = import_wavs(args.archive.expanduser(), args.output)
    total = sum(p.stat().st_size for p in args.output.rglob("*.wav"))
    print(f"Importati {len(digests)} WAV ({total / 1024 / 1024:.1f} MiB) in {args.output}")


if __name__ == "__main__":
    main()
