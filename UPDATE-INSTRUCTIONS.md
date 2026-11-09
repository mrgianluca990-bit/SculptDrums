# Sculpt Drums — kit definitivo con 125 WAV

Questo aggiornamento allinea `kit/kit.json` ai **125 WAV effettivamente presenti
su GitHub** (21 + 38 + 6 + 16 + 10 + 34). Non aggiunge né elimina alcun WAV.

## Pubblicare la correzione sul repository

L'archivio `SculptDrums-125-samples-fix.zip` contiene soltanto:

- `kit/kit.json` aggiornato alla selezione corrente;
- `tools/build_ordered_kit.py`, `tools/import_samples.py` e `tools/audit_install.py`;
- `.github/workflows/build-macos-vst3.yml`, con controllo della libreria prima del build;
- `CMakeLists.txt` e documentazione aggiornata.

Se hai già clonato il repository, estrai lo ZIP sopra la copia locale e fai:

```sh
python3 tools/audit_install.py .
git add kit/kit.json .github/workflows/build-macos-vst3.yml \
  CMakeLists.txt README.md UPDATE-INSTRUCTIONS.md docs/SPECIFICHE.md \
  tools/build_ordered_kit.py tools/import_samples.py tools/audit_install.py
git commit -m "fix(drums): align kit manifest to 125 selected WAV"
git push origin main
```

Il controllo deve restituire `WAV nel manifest: 125`, `WAV sul disco: 125`
e `WAV mancanti: 0`. Se restituisce numeri differenti, verifica che nel tuo
clone i campioni corrispondano alla selezione pubblicata sul repository.

## Testare il VST3

Dopo il push vai in GitHub Actions e attendi una nuova compilazione verde.
Scarica l'artifact `SculptDrums-VST3-macOS`, estrailo e sostituisci il plugin
in `/Library/Audio/Plug-Ins/VST3/`. Chiudi e riapri la DAW.

All'avvio lo stato atteso è `Kit: ... eventi / 125 WAV` **senza mancanti**.
Verifica le sei posizioni MORPH (0, 20, 40, 60, 80, 100), la velocity, il
round robin, il volume e il VU meter.

Le categorie clap, shaker e combo non sono incluse nella selezione attuale:
non ricevono un suono MIDI dedicato.
