# Sculpt Drums — Specifiche del plugin

Sculpt Drums sarà uno strumento virtuale di batteria ibrida per macOS, costruito utilizzando i campioni acustici ed elettronici registrati.

L'idea centrale è avere un'unica batteria capace di trasformare progressivamente il proprio carattere, passando dal suono acustico naturale a quello completamente elettronico attraverso una sola manopola principale.

## 1. Libreria e costruzione del kit

Il plugin utilizzerà:

- Colpi singoli provenienti dalle diverse batterie acustiche.
- Campioni elettronici.
- Eventuali varianti dello stesso colpo.
- Riprese microfoniche differenti: close, overhead, room e altre disponibili.
- Diverse intensità e articolazioni, quando presenti.

Tutto il materiale confluirà in un unico kit ibrido. Ogni elemento potrà essere composto da più sorgenti: per esempio, una cassa acustica combinata con un campione elettronico oppure un rullante formato da più registrazioni opportunamente bilanciate.

## 2. Manopola MORPH — Il cuore dello strumento

Corsa da 0 a 100, tre famiglie timbriche. L'archivio ordinato del 2026
introduce sei riferimenti distinti, preservati dal motore VST3:

| MORPH | Archivio | Carattere |
|---:|---|---|
| 0 | `01 Natural` | Acustico naturale 1 |
| 20 | `02 Natural` | Acustico naturale 2 |
| 40 | `03 processed` | Acustico trattato 1 |
| 60 | `04 processed` | Acustico trattato 2 |
| 80 | `05 processed` | Acustico trattato 3 |
| 100 | `06 Electro` | Elettronico |

La versione con mixer usa un kernel coseno esteso su due intervalli MORPH
per lato: le transizioni sovrappongono fino a quattro stadi contemporaneamente,
con normalizzazione energetica. I punti 0 e 100 rimangono puri.
La curva per pezzo cambia l'andamento della transizione senza spostare
le sei posizioni fisse. Quando un particolare elemento non è presente
in uno stadio si usa il campione del riferimento disponibile più vicino.

La transizione deve essere continua, senza cambi improvvisi di suono.

MORPH controlla simultaneamente: bilanciamento tra i campioni, quantità di componenti naturali/trattate/elettroniche, contributo delle riprese OH e room, carattere timbrico complessivo.

Ogni elemento del kit ha curve di miscelazione proprie, pur rispondendo alla stessa manopola: la cassa può trasformarsi in modo diverso dal rullante, con un risultato musicale coerente.

## 3. Round robin delle miscele

Il round robin è basato principalmente sulle variazioni delle miscele dei campioni.

La manopola MORPH stabilisce un valore di riferimento. Esempio: MORPH a 60, variazione ±3 → colpi a 59, 62, 58, 61. Il motore usa una sequenza di variazioni positive e negative, evitando ripetizioni immediate e mantenendo il carattere medio vicino al valore scelto. Le variazioni sono calibrate per ciascun elemento del kit.

Regole fondamentali:

- Ogni pezzo gestisce autonomamente le proprie variazioni.
- Il valore scelto rimane associato al colpo durante il decadimento.
- Le riprese microfoniche dello stesso evento rimangono sincronizzate.
- La variazione non deve provocare salti di volume o problemi di fase.
- Agli estremi della corsa, le variazioni rispettano il carattere acustico o elettronico selezionato.

Più esecuzioni reali dello stesso colpo, se disponibili, contribuiscono alla varietà.

## 4. Dinamica ed espressività MIDI

La velocity gestisce intensità, selezione dei livelli dinamici, risposta in volume ed eventuali variazioni timbriche. Velocity e MORPH sono indipendenti. Previste le articolazioni effettivamente disponibili nei campioni, comprese le aperture del charleston.

## 5. Gestione delle microfonazioni

Quando sono disponibili più riprese dello stesso colpo, il motore le mantiene collegate (es. Snare Top, Snare Bottom, Overhead, Room). Il rapporto cambia progressivamente con MORPH. Si conservano i rapporti temporali e di fase originali; riprese di sessioni diverse sono trattate come sorgenti alternative.

## 6. Automazione del MORPH

Parametro automatizzabile dalla DAW, con smoothing, bilanciamenti progressivi, gestione coerente delle code e controllo degli sbalzi di livello. I colpi già avviati seguono la trasformazione senza ripartire dall'attacco.

## 7. Interfaccia grafica

Quarta proposta grafica, modificata (vedi `gui-mockup.png`): superficie nera opaca, scritta SCULPT DRUMS in alto, grande manopola MORPH centrale con arco luminoso caldo→freddo, tre indicatori delle famiglie di campioni, punti luminosi del round robin, manopola Volume in basso a sinistra, VU meter analogico in basso a destra.

Il pulsante grafico **≡** apre un mixer secondario con 15 fader verticali,
uno per ognuna delle articolazioni/parti presenti nel kit da 125 WAV. Il
guadagno per pezzo è indipendente dalla posizione MORPH, dispone di smoothing
audio di 25 ms ed è salvato nei preset tramite APVTS.

## 8. Formato

macOS, **VST3** (decisione: niente AU). In seguito integrabile in Sculpt DAW. Prevista l'instradamento separato delle componenti del kit verso le uscite della DAW, da definire.

La libreria selezionata di 125 WAV è presente nel repository in `Samples/` e copiata
nel bundle VST3 (`Contents/Resources/SculptDrumsKit/Samples/`) in fase di build.
La cartella musicale esterna non è necessaria. Lo stadio `01 Natural ` dello ZIP
originale è normalizzato in `01 Natural` per evitare ambiguità nei percorsi.

## 9. Da fare

1. Analisi dei campioni originali (inventario).
2. Costruzione del kit ibrido.
3. Preparazione dei campioni trattati ed elettronici mancanti.
4. Motore di campionamento: MIDI, velocity, voci, microfonazioni.
5. MORPH e round robin delle miscele.
6. Integrazione di interfaccia, volume e VU meter.
7. Verifica in DAW: ribattuti, continuità del morphing, fase, livelli, latenza, prestazioni.
