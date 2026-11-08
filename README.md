# Sculpt Drums

Batteria ibrida acustico/elettronica con manopola MORPH e round robin delle miscele.
Formato: **VST3** (macOS). Framework: JUCE 8 (scaricato da CMake).

## Build
```
cmake -B build -G Xcode
cmake --build build --config Release
```
Il VST3 viene copiato in `~/Library/Audio/Plug-Ins/VST3/`.

## Stato
- [x] Scaffold, parametri MORPH / Volume / RR Range
- [x] Curve di miscela per pezzo + round robin delle miscele
- [x] GUI (MORPH, Volume, VU, indicatori, punti RR)
- [ ] Campioni reali (ora: segnaposto sintetici, 3 pezzi x 3 livelli)
- [ ] Velocity layers, microfoni, articolazioni charleston
- [ ] Uscite separate
