# Sculpt Drums — aggiornamento a sei stadi

Questo pacchetto contiene **solo sorgenti e manifest**; nessun WAV viene
pubblicato su GitHub. Conservare lo ZIP originale `Sculpt Drums.zip` sul Mac.

## A. Aggiornare il repository

1. Scaricare e scompattare `SculptDrums-six-stage-update.zip`.
2. Se non esiste già una copia locale, crearla con
   `git clone https://github.com/mrgianluca990-bit/SculptDrums.git`.
3. Copiare i file e le sottocartelle contenuti nel pacchetto **dentro la copia
   locale del repository SculptDrums**, preservando i percorsi `Source/`,
   `tools/`, `kit/`, `docs/`. Non eliminare `.github/` o `CMakeLists.txt`.
4. Nella cartella del repository eseguire:

   ```sh
   git add README.md UPDATE-INSTRUCTIONS.md .gitignore Source/ docs/SPECIFICHE.md kit/kit.json tools/build_ordered_kit.py tools/audit_install.py tools/test_ordered_kit.py tools/test_morph.cpp
   git commit -m "feat(drums): six ordered morph stages and 444 sample manifest"
   git push origin main
   ```

5. Il push su `main` attiverà la GitHub Action VST3 già presente. Attendere
   il completamento con esito verde e scaricare l'artifact della nuova build.

## B. Preparare la libreria sul Mac

1. Scompattare l'archivio audio originale in `~/Music/` in modo da ottenere
   `~/Music/Sculpt Drums/01 Natural `, `02 Natural`, ..., `06 Electro`.
2. **Sostituire solo** `~/Music/Sculpt Drums/kit.json` con il nuovo `kit/kit.json`.
3. Da Terminale, dentro la copia del repository, eseguire:

   ```sh
   python3 tools/audit_install.py "$HOME/Music/Sculpt Drums"
   ```

   Il risultato atteso è `WAV nel manifest: 444` e `WAV mancanti: 0`.

## C. Installare il VST3 aggiornato

1. Estrarre il nuovo artifact GitHub Actions (a volte ci sono due ZIP annidati).
2. Chiudere Reaper e sostituire la versione precedente di `Sculpt Drums.vst3`
   nella cartella `/Library/Audio/Plug-Ins/VST3/` oppure in quella dell'utente.
3. Se macOS blocca il **nostro plugin non notarizzato**, seguire la procedura
   di autorizzazione specifica (non disabilitare Gatekeeper per tutto il Mac).
4. Aprire Reaper, eseguire la scansione VST e verificare le note 36 (cassa),
   38 (rullante), 42 (hi-hat chiuso). Suonare ad ogni riferimento MORPH:
   0, 20, 40, 60, 80, 100.

La build VST3 e l'ascolto in DAW **restano da verificare** dopo il commit.
