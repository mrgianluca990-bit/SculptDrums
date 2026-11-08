# Sculpt Drums

Un solo kit di batteria ibrida con MORPH continuo e round robin delle miscele.
Per macOS, formato **VST3**, JUCE 8.

## Libreria integrata: sei stadi ordinati

Il repository include tutti i **444 WAV** nella cartella `Samples/`. La build
macOS incorpora automaticamente `Samples/` e `kit/kit.json` dentro il VST3.
**Non occorre creare cartelle in `~/Music` né installare i WAV separatamente.**

| MORPH | Directory originale | Carattere |
|---:|---|---|
| 0 | `01 Natural` | Naturale 1 |
| 20 | `02 Natural` | Naturale 2 |
| 40 | `03 processed` | Trattato 1 |
| 60 | `04 processed` | Trattato 2 |
| 80 | `05 processed` | Trattato 3 |
| 100 | `06 Electro` | Elettronico |

L'archivio sorgente originale usa `01 Natural ` con uno spazio finale nel nome;
per evitare errori sul Mac, l'importatore lo normalizza in `01 Natural`.
Gli extra `.asd`, `.DS_Store`, `__MACOSX` e il vecchio `kit.json` non sono distribuiti.

### Installazione

1. Da GitHub Actions scarica `SculptDrums-VST3-macOS` dell'ultima build.
2. Estrai lo ZIP e copia `Sculpt Drums.vst3` in `/Library/Audio/Plug-Ins/VST3/`.
3. Riavvia Reaper o Sculpt DAW e apri lo strumento su una traccia MIDI/Instrument.
4. Aspetta il caricamento: il plugin deve indicare `Kit: ... eventi / 444 WAV`.

Il motore prova prima il kit incorporato nel VST3, poi la vecchia cartella
`~/Music/Sculpt Drums/` per compatibilità di sviluppo. Se entrambi mancano,
produce soltanto i suoni sintetici di prova.

### Aggiornare la libreria dai campioni originali

```sh
python3 tools/import_samples.py '/percorso/Sculpt Drums.zip'
python3 tools/build_ordered_kit.py Samples -o kit/kit.json
python3 tools/audit_install.py .
```

Il generatore raggruppa le riprese `close + over/OH + room` dello stesso colpo
Indra: avanzano insieme nel round robin e conservano il loro offset originale.
Il bilanciamento attuale è fisso (close + 0.25 OH + 0.15 room); il controllo
microfonico indipendente resta un obiettivo futuro.

## Motore

- Una sola manopola **MORPH** (0–100) attraversa i sei stadi ordinati,
  con crossfade a potenza costante fra stadi adiacenti.
- Ogni pezzo ha la propria curva `gamma`.
- Il round robin modifica leggermente il target MORPH a ogni nota;
  l'offset rimane fisso per tutta la durata del colpo.
- Le varianti registrate ruotano per pezzo e stadio; velocity indipendente.
- Se manca un pezzo in un determinato stadio, il motore usa il campione
  disponibile più vicino senza creare duplicati in RAM.
- Volume generale, indicatore VU e choke dei charleston.

### MIDI

| Nota | Pezzo | Nota | Pezzo |
|---:|---|---:|---|
| 36 | Kick | 38 | Snare |
| 37 | Snare rim | 40 | Rimshot |
| 39 | Clap | 42 | Hi-hat closed |
| 44 | Hi-hat foot | 46 | Hi-hat open |
| 45 | Tom | 41 | Floor tom |
| 49 | Crash | 52 | Reverse cymbal |
| 51 | Ride | 53 | Ride bell |
| 55 | Splash | 70 | Shaker |
| 75 | Percussion | 76 | Combo |

## Build

```sh
cmake -S . -B build -G Xcode
cmake --build build --config Release --target SculptDrums_VST3
```

La GitHub Action `.github/workflows/build-macos-vst3.yml` produce lo ZIP VST3
universale Apple Silicon + Intel nella pagina Actions del run. Verifica che
tutti i 444 WAV siano presenti nel bundle prima di apporre la firma ad hoc.

## Verifiche ancora necessarie

- Test di apertura, note MIDI, round robin e livelli in Reaper su Mac.
- Ascolto dei crossfade fra registrazioni molto differenti.
- Allineamento dei transienti e controllo di fase delle riprese OH/Room.
- Uscite separate e controlli indipendenti per i microfoni.
