# Sculpt Drums

Batteria ibrida acustico/elettronica con manopola MORPH e round robin delle miscele.
Formato: **VST3** (macOS). Framework: JUCE 8 (scaricato da CMake). Specifiche in `docs/SPECIFICHE.md`.

## Build
```
cmake -B build -G Xcode
cmake --build build --config Release
```
Il VST3 viene copiato in `~/Library/Audio/Plug-Ins/VST3/`.

## Campioni e kit (non sono nel repo)
Il plugin cerca tutto in `~/Music/Sculpt Drums/`:
```
~/Music/Sculpt Drums/
├── kit.json        <- da kit/kit.json di questo repo
└── Campioni/       <- la cartella dei campioni, così com'è
```
Se `kit.json` non c'è, il plugin parte con suoni di prova e lo scrive sotto il titolo.

Per rigenerare il kit dopo aver cambiato i campioni:
```
python3 tools/inventory.py /percorso/Campioni -o tools/inventario.csv
python3 tools/build_kit.py tools/inventario.csv -o kit/kit.json
```

## Come funziona il kit
- Ogni pezzo ha tre livelli: **naturale**, **trattato**, **elettronico** (MORPH 0 / 50 / 100).
- Il livello **trattato** è derivato al caricamento dal naturale (saturazione parallela) finché non ci sono campioni trattati veri: si possono aggiungere nel campo `treated` di `kit.json`.
- Le varianti numerate dei campioni ruotano a ogni colpo; pp/p/mp della cassa rispondono alla velocity.
- Charleston chiuso, a pedale e aperto sono in choke tra loro.
- Mappa MIDI: 36 cassa, 37 rim, 38 rullante, 39 clap, 42 hh chiuso, 44 hh pedale, 45 tom, 46 hh aperto, 49 crash, 51 ride, 55 splash.

## Stato
- [x] Scaffold, parametri MORPH / Volume / RR Range, GUI
- [x] Curve di miscela per pezzo + round robin delle miscele
- [x] Caricamento WAV reali da kit.json, livelli allineati, choke charleston
- [ ] Campioni trattati veri e più livelli di velocity
- [ ] Riprese microfoniche (OH/room), uscite separate
- [ ] Verifica in DAW (ribattuti, fase, latenza)
