# Sculpt Drums

Un solo kit di batteria ibrida con MORPH continuo e round robin delle miscele.
Per macOS, formato **VST3**, JUCE 8.

## Libreria reale: sei stadi ordinati

La cartella `Sculpt Drums` del nuovo archivio deve risiedere in
`~/Music/Sculpt Drums/`, con il seguente ordine:

| MORPH | Directory originale | Carattere |
|---:|---|---|
| 0 | `01 Natural ` (spazio finale nel nome originale) | Naturale 1 |
| 20 | `02 Natural` | Naturale 2 |
| 40 | `03 processed` | Trattato 1 |
| 60 | `04 processed` | Trattato 2 |
| 80 | `05 processed` | Trattato 3 |
| 100 | `06 Electro` | Elettronico |

I **444 WAV** sono forniti separatamente, non nel repository GitHub.
Gli extra `.asd`, `.DS_Store` e `__MACOSX` non sono utilizzati.

**Importante:** il `kit.json` contenuto nell'archivio originale è vecchio
(versione 1). Sostituirlo con il file `kit/kit.json` di questo repository,
senza modificare i WAV.

### Installazione dei campioni

1. Estrai lo ZIP originale in `~/Music/`, mantenendo la struttura `Sculpt Drums/`.
2. Copia il nuovo `kit/kit.json` in `~/Music/Sculpt Drums/kit.json`, sovrascrivendo
   **solo** il vecchio manifest, non i WAV.
3. Chiudi e riapri la DAW: il kit viene caricato in background all'avvio del plugin.
4. Il plugin deve indicare `Kit: ... eventi / 444 WAV` anziché `suoni di prova`.

Prima di aprire la DAW si può controllare che tutti i file siano stati estratti:

```sh
python3 tools/audit_install.py "$HOME/Music/Sculpt Drums"
```

L'esito atteso è `WAV mancanti: 0`. È importante conservare lo spazio finale
nel nome originale `01 Natural `, altrimenti i 21 WAV della prima cartella
non verranno trovati.

Per rigenerare il manifest dopo ulteriori aggiunte:

```sh
python3 tools/build_ordered_kit.py '/percorso/Sculpt Drums.zip' -o kit/kit.json
# Oppure da una cartella già estratta:
python3 tools/build_ordered_kit.py "$HOME/Music/Sculpt Drums" -o kit/kit.json
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
universale Apple Silicon + Intel nella pagina Actions del run.

## Verifiche ancora necessarie

- Test di apertura, note MIDI, round robin e livelli in Reaper su Mac.
- Ascolto dei crossfade fra registrazioni molto differenti.
- Allineamento dei transienti e controllo di fase delle riprese OH/Room.
- Uscite separate e controlli indipendenti per i microfoni.
