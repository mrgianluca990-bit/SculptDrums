# Sculpt Drums — MORPH Editor / 125 WAV

**Patch sorgenti per la build JUCE 8 di Sculpt Drums con sei stadi MORPH.**
Non aggiunge e non sostituisce campioni: il contenuto di `Samples/` e la mappatura `kit/kit.json` non vengono toccati. Mantiene il round robin automatico.

## Installazione

Scaricare lo ZIP, estrarlo in `~/Downloads` e aprire Terminale:

```bash
cd ~/Downloads
git clone https://github.com/mrgianluca990-bit/SculptDrums.git SculptDrums-morph
python3 ~/Downloads/sculpt-morph-editor-update/tools/apply_morph_editor.py ~/Downloads/SculptDrums-morph
cd ~/Downloads/SculptDrums-morph
python3 tools/audit_install.py .
clang++ -std=c++17 -Wall -Wextra -pedantic tools/test_morph_curves.cpp -o /tmp/sculpt-morph-test
/tmp/sculpt-morph-test
```

Se il repository è già clonato, usare il percorso della copia aggiornata, senza eseguire `git clone` una seconda volta. Verificare il diff prima del push:

```bash
git status --short
git diff --stat
```

I file `*.pre-morph.bak` e `*.pre-presets.bak` sono backup locali e **non devono essere committati**. Eseguire i seguenti comandi dalla radice del repository:

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp Source/PluginEditor.h Source/PluginEditor.cpp Source/MorphEngine.h
git add Source/MorphEditorState.h Source/MorphPresetJson.h Source/MorphEditorComponents.h Source/MorphProcessorMethods.inc Source/MorphEditorMethods.inc
git add Source/PresetStore.h Source/PresetEditorMethods.inc tools/test_morph_curves.cpp .github/workflows/build-macos-vst3.yml
git commit -m "feat: editable morph curves and per-WAV gain"
git push origin main
```

Se i preset interni non erano ancora presenti nel repository, lo script integra prima anche la loro patch originale. In questo caso aggiungere anche al commit `Source/PresetStore.h` e `Source/PresetEditorMethods.inc` (già nel comando precedente), e verificare che compaiano i controlli preset.

## Utilizzo

- Il pulsante **EDIT MORPH** apre l'editor; non cambia la schermata principale.
- **KICK / SNARE / ...** seleziona l'elemento della batteria.
- La curva mostra il contributo energetico dei sei stadi. La linea verticale mostra il MORPH principale attuale.
- **POSIZIONE** cambia il centro di ciascuno degli stadi 2–5; Natural 0 e Electro 100 rimangono fissi.
- **OVERLAP** regola la larghezza del contributo di ogni stadio (da 5 a 70 punti MORPH; default 40).
- Ogni riga WAV ha un gain da **−24 a +12 dB** (default 0 dB); il tasto triangolare ascolta solo quel file, anche se è un overhead/room.
- **PLAY MIX** esegue il pezzo selezionato, con il MORPH corrente e round robin automatico.
- I campioni e la scelta delle varianti round robin restano fissi e automatici.

## Preset / progetto

- I preset interni sono salvati in `~/Library/Application Support/Sculpt Drums/presets.json` formato v2: MORPH principale, 15 fader e configurazione MORPH Editor. Preset v1 compatibili; le nuove regolazioni assumono il valore di default quando mancanti.
- Lo stato della sessione DAW include anche le curve e i gain dei WAV; nessun campione audio viene serializzato nel preset.
- Il controllo Volume generale e il RR Range non fanno parte dei preset interni, come nella versione precedente.

## QA minima macOS dopo la build

1. Controllare che la GitHub Action compili, pacchettizzi e pubblichi l'artifact VST3 (non basta il test C++).
2. Aprire in Reaper, verificare `Kit: ... / 125 WAV`, aprire `EDIT MORPH`, scorrere tutti gli strumenti.
3. Regolare gain del close, overhead e room di un elemento Natural, ascoltare le differenze.
4. Regolare posizione 2/3 e overlap 3/4, suonare una sequenza costante e attraversare 0–100 ascoltando i passaggi.
5. Richiamare preset dopo aver modificato gain e curva. Ripetere dopo chiusura/riapertura Reaper e riapertura della sessione.
6. Verificare CPU e dropout con pad MIDI rapidi, 48/96 kHz, buffer 64/128/512.

### Limiti delle verifiche eseguite

La patch è stata verificata staticamente e il kernel di normalizzazione delle curve è stato compilato e testato su Linux con C++17. **La compilazione dell'intero VST3 JUCE su macOS e il test sonoro devono ancora essere eseguiti dalla GitHub Action e in Reaper.**
