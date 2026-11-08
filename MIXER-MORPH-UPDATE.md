# Sculpt Drums — Mixer + MORPH più morbido

Questo aggiornamento riguarda **solo il codice**. Mantiene intatti i 125 WAV
selezionati, l'ordine Natural → Processed → Electro e il manifest già pubblicato.

## Novità

1. **Mixer a 15 canali**: un fader per ogni suono MIDI disponibile; cliccare
   **≡** in alto a destra per aprire/chiudere il pannello. Doppio clic su un
   fader per ripristinare 0 dB. I livelli sono automatizzabili e salvabili
   nei preset e nella sessione.
2. **MORPH ampliato**: fino a quattro stadi possono sovrapporsi, invece di
   soli due. I livelli sono normalizzati a potenza costante. I punti 0 e 100
   rimangono puri, rispettivamente Natural ed Electro.
3. **Round robin invariato**: ogni colpo riceve una lieve variazione del target
   attorno alla posizione MORPH; le riprese OH e Room dello stesso evento
   restano accoppiate.
4. **Volume smussato**: ogni fader introduce smoothing audio di 25 ms per
   evitare clic durante automazione o trascinamento.

## Caricamento nel repository

Scarica lo ZIP `SculptDrums-mixer-wide-morph-update.zip` e, dal Terminale sul Mac:

```sh
cd "$HOME/Downloads"
git clone https://github.com/mrgianluca990-bit/SculptDrums.git SculptDrums-mixer
ditto -x -k "$HOME/Downloads/SculptDrums-mixer-wide-morph-update.zip" "$HOME/Downloads/SculptDrums-mixer"
cd "$HOME/Downloads/SculptDrums-mixer"
python3 tools/audit_install.py .
git add Source/ tools/test_morph.cpp tools/test_mixer.cpp README.md \
  docs/SPECIFICHE.md MIXER-MORPH-UPDATE.md \
  .github/workflows/build-macos-vst3.yml
git commit -m "feat(drums): mixer for 15 pieces and wide morph crossfades"
git push origin main
```

Il push avvia la compilazione macOS su GitHub Actions. Lo ZIP **non contiene
WAV**, quindi non può ripristinare i campioni precedentemente eliminati.

## Checklist di ascolto

- Apri lo strumento in Reaper e attendi `Kit: ... eventi / 125 WAV`.
- Prova il MORPH a 0 / 10 / 20 / 30 / 40 / 50 / 60 / 70 / 80 / 90 / 100.
- Verifica che a 0 sia completamente naturale e a 100 completamente Electro.
- Apri il mixer (≡), riduci solo il rullante e verifica che cassa e piatti
  rimangano invariati, anche muovendo MORPH.
- Automatizza un fader e il MORPH: ascolta eventuali clic o sbalzi di volume.
- Verifica i ribattuti e il round robin; richiudi il mixer e controlla il VU.
