#!/usr/bin/env python3
"""Genera kit.json (proposta di kit ibrido) a partire da inventario.csv.
Uso: python3 tools/build_kit.py tools/inventario.csv -o kit.json
Poi copia kit.json e la cartella Campioni/ in ~/Music/Sculpt Drums/"""
import argparse, csv, json, math, statistics
from collections import defaultdict

# id pezzo -> (nota MIDI, gamma curva MORPH, gruppo choke, pezzi acustici, pezzi elettronici, articolazione)
PIECES = [
    ('kick',        36, 1.00, 0, ['kick'],         ['kick'],         None),
    ('snare',       38, 0.85, 0, ['snare'],        ['snare'],        None),
    ('snare_rim',   37, 0.85, 0, ['snare'],        ['snare'],        'rim'),
    ('clap',        39, 1.00, 0, ['clap'],         ['clap'],         None),
    ('hihat_closed',42, 1.20, 1, ['hihat_closed'], ['hihat_closed'], None),
    ('hihat_foot',  44, 1.20, 1, ['hihat_foot'],   ['hihat_closed'], None),
    ('hihat_open',  46, 1.20, 1, ['hihat_open'],   ['hihat_open'],   None),
    ('tom',         45, 1.00, 0, ['tom'],          ['tom'],          None),
    ('crash',       49, 1.10, 0, ['crash'],        ['cymbal', 'crash'], None),
    ('ride',        51, 1.10, 0, ['ride'],         ['ride'],         None),
    ('splash',      55, 1.10, 0, ['splash'],       ['cymbal'],       None),
]
VEL = {'pp': [0, 40], 'p': [41, 70], 'mp': [71, 100], '': [0, 127]}

def is_art(r, art):  # True se il campione appartiene all'articolazione richiesta
    has = r['articulation'] in ('rim', 'bordo') or 'rim' in r['path'].lower()
    return has if art == 'rim' else not has

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('csv'); ap.add_argument('-o', default='kit.json')
    a = ap.parse_args()
    rows = list(csv.DictReader(open(a.csv)))
    for r in rows: r['dur_s'] = float(r['dur_s'])
    used, kit = set(), []
    for pid, note, gamma, choke, ac, el, art in PIECES:
        nat = [r for r in rows if r['family'] == 'acoustic' and r['piece'] in ac and is_art(r, art or '')]
        if art is None: nat = [r for r in nat if is_art(r, '')]
        ele = [r for r in rows if r['family'] == 'electronic' and r['piece'] in el and is_art(r, art or '')]
        ele = [r for r in ele if 'revcymbal' not in r['path'].lower()]
        if art == 'rim': ele = [r for r in ele if 'rim' in r['path'].lower()]
        elif art is None: ele = [r for r in ele if 'rim' not in r['path'].lower()]
        chosen = []
        if ele:
            ref = [r['dur_s'] for r in nat] or [r['dur_s'] for r in ele]
            target = statistics.median(ref)
            packs = defaultdict(list)
            for r in ele: packs[r['pack'] or r['path']].append(r)
            dist = lambda g: abs(math.log(statistics.median(x['dur_s'] for x in g) / target))
            # prima i pack con >=2 varianti, per durata più vicina; si accumula fino a 4 varianti
            for g in sorted(packs.values(), key=lambda g: (len(g) < 2, dist(g))):
                chosen += sorted(g, key=lambda x: x['path'])
                if len(chosen) >= 4: break
            chosen = chosen[:4]
        entry = dict(id=pid, note=note, gamma=gamma, choke=choke,
                     natural=[dict(file=r['path'], vel=VEL.get(r['dynamic'], [0, 127])) for r in sorted(nat, key=lambda x: x['path'])],
                     treated=[],  # vuoto = derivato dal naturale (saturazione) finché non ci sono campioni trattati veri
                     electronic=[dict(file=r['path']) for r in chosen])
        if not nat and chosen: entry['electronicOnly'] = True
        if nat or chosen: kit.append(entry)
        used |= {r['path'] for r in nat} | {r['path'] for r in chosen}
    unassigned = sorted({r['path'] for r in rows if r['family'] == 'acoustic'} - used)
    json.dump(dict(version=1, sampleFolder='Campioni', pieces=kit, unassignedAcoustic=unassigned),
              open(a.o, 'w'), indent=1, ensure_ascii=False)
    for p in kit:
        print(f"{p['id']:13s} nota {p['note']}  naturali {len(p['natural']):2d}  elettronici {len(p['electronic'])}"
              + (f"  <- {p['electronic'][0]['file'].rsplit('/',1)[0]}/{p['electronic'][0]['file'].split('/')[-1]}" if p['electronic'] else '  (nessun elettronico)'))
    print('\nacustici non assegnati:', *unassigned, sep='\n  ')

if __name__ == '__main__': main()
