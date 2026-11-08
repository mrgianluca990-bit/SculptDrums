# Sculpt Drums — aggiornamento con libreria integrata

Questo pacchetto contiene **codice, kit/kit.json e 444 WAV** distribuiti in
`Samples/`. La build GitHub Actions li incorpora nel VST3 macOS.

## A. Pubblicare nel repository (da Terminale sul Mac)

1. Clona, se necessario: `git clone https://github.com/mrgianluca990-bit/SculptDrums.git`.
2. Scompatta `SculptDrums-complete-library-update.zip` e copia **il contenuto**
   sopra la cartella locale `SculptDrums`, senza cancellare gli altri file.
3. Dal Terminale nella cartella del repository esegui:

   ```sh
   python3 tools/audit_install.py .
   git add CMakeLists.txt .github/workflows/build-macos-vst3.yml README.md \
     UPDATE-INSTRUCTIONS.md Source/ docs/ kit/ tools/ Samples/ .gitignore
   git commit -m "feat(drums): embed all 444 WAV in VST3"
   git push origin main
   ```

   La verifica deve indicare `WAV nel manifest: 444` e `WAV mancanti: 0`.

## B. Installare il nuovo VST3

1. Apri GitHub → Actions → ultima build `Sculpt Drums VST3 macOS`.
2. Quando il run è verde, scarica l'artifact `SculptDrums-VST3-macOS`.
3. Estrai `Sculpt Drums.vst3` dallo ZIP (se necessario, da un secondo ZIP interno).
4. Chiudi la DAW e sostituisci il VST3 nella cartella `/Library/Audio/Plug-Ins/VST3/`.
5. Riavvia Reaper, rifai la scansione dei plugin e inserisci **Sculpt Drums**
   come strumento virtuale su una traccia MIDI.
6. Aspetta il caricamento in background e verifica lo stato:
   `Kit: ... eventi / 444 WAV`, senza campioni mancanti.

Il plugin contiene già tutti i WAV. **Non serve** la cartella
`~/Music/Sculpt Drums/`.

## C. Test da effettuare

- Suona MIDI 36 (kick), 38 (snare), 42 (hi-hat closed).
- Muovi MORPH nelle posizioni 0, 20, 40, 60, 80 e 100.
- Verifica le differenze acustiche a sinistra ed elettroniche a destra.
- Prova i ribattuti e il round robin, il volume e il meter.

La prima compilazione macOS e il funzionamento nella DAW **vanno verificati**
dopo il push. La firma ad hoc non equivale alla notarizzazione Apple: se
macOS blocca il plugin, autorizza il singolo file, senza disabilitare
Gatekeeper globalmente.
