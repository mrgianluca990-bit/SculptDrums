# Sculpt Drums

Un solo kit di batteria ibrida con MORPH continuo e round robin delle miscele.
Per macOS, formato **VST3**, JUCE 8.

## Libreria integrata: sei stadi ordinati

Il repository include i **125 WAV selezionati** nella cartella `Samples/`. La build
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
4. Aspetta il caricamento: il plugin deve indicare `Kit: ... eventi / 125 WAV`.

Il motore prova prima il kit incorporato nel VST3, poi la vecchia cartella
`~/Music/Sculpt Drums/` per compatibilità di sviluppo. Se entrambi mancano,
produce soltanto i suoni sintetici di prova.

### Aggiornare la selezione dei campioni

```sh
python3 tools/build_ordered_kit.py Samples -o kit/kit.json
python3 tools/audit_install.py .
```

Il manifest viene rigenerato dai soli WAV presenti in `Samples/`: non esiste
un numero minimo fisso di campioni. Se aggiungi o rimuovi WAV, rigenera sempre
`kit/kit.json` prima della build. `tools/import_samples.py` è disponibile per
importare nuovi WAV da uno ZIP, ma **reimportare l'archivio originale ripristina
anche i campioni scartati**.

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
| 44 | Hi-hat foot | 46 | Hi-hat open |
| 42 | Hi-hat closed | 45 | Tom |
| 41 | Floor tom | 49 | Crash |
| 52 | Reverse cymbal | 51 | Ride |
| 53 | Ride bell | 55 | Splash |
| 75 | Percussion | | |

## Build

```sh
cmake -S . -B build -G Xcode
cmake --build build --config Release --target SculptDrums_VST3
```

La GitHub Action `.github/workflows/build-macos-vst3.yml` produce lo ZIP VST3
universale Apple Silicon + Intel nella pagina Actions del run. Verifica che
tutti i WAV elencati nel manifest siano presenti nel repository e nel bundle
prima di apporre la firma ad hoc.

## Verifiche ancora necessarie

- Test di apertura, note MIDI, round robin e livelli in Reaper su Mac.
- Ascolto dei crossfade fra registrazioni molto differenti.
- Allineamento dei transienti e controllo di fase delle riprese OH/Room.
- Uscite separate e controlli indipendenti per i microfoni.
