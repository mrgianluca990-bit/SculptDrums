#!/usr/bin/env python3
"""Inventario dei campioni di Sculpt Drums.
Uso: python3 tools/inventory.py /percorso/Campioni [-o inventario.csv]
Legge i WAV (PCM 16/24/32, float, WAVE_FORMAT_EXTENSIBLE) e classifica per nome/cartella."""
import argparse, csv, math, os, re, struct, sys
from collections import Counter, defaultdict

def read_wav(path):
    with open(path, 'rb') as f:
        d = f.read()
    if d[:4] != b'RIFF' or d[8:12] != b'WAVE':
        raise ValueError('non RIFF/WAVE')
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(d):
        cid, sz = d[pos:pos+4], struct.unpack('<I', d[pos+4:pos+8])[0]
        body = d[pos+8:pos+8+sz]
        if cid == b'fmt ':
            tag, ch, sr, _, _, bits = struct.unpack('<HHIIHH', body[:16])
            if tag == 0xFFFE and len(body) >= 26:
                tag = struct.unpack('<H', body[24:26])[0]
            fmt = (tag, ch, sr, bits)
        elif cid == b'data':
            data = body
        pos += 8 + sz + (sz & 1)
    if not fmt or data is None:
        raise ValueError('fmt/data mancante')
    tag, ch, sr, bits = fmt
    n = len(data) // (bits // 8) // ch
    peak = 0.0
    step = max(1, n // 50000)  # campionamento veloce del picco
    bps = bits // 8
    for i in range(0, n, 1):
        if i % step and i > 2000: continue
        for c in range(ch):
            o = (i * ch + c) * bps
            if tag == 3 and bits == 32: v = struct.unpack('<f', data[o:o+4])[0]
            elif bits == 16: v = struct.unpack('<h', data[o:o+2])[0] / 32768
            elif bits == 24: v = int.from_bytes(data[o:o+3], 'little', signed=True) / 8388608
            elif bits == 32: v = struct.unpack('<i', data[o:o+4])[0] / 2147483648
            else: v = 0
            peak = max(peak, abs(v))
    return dict(ch=ch, sr=sr, bits=bits, fmt='float' if tag == 3 else 'pcm',
                dur=n / sr, peak_db=20 * math.log10(peak) if peak > 0 else -120.0)

PIECES = [  # (regex sul nome, pezzo)
    (r'hh op|open', 'hihat_open'), (r'foot', 'hihat_foot'), (r'closedhh|hh close|^hh\b|hh f', 'hihat_closed'),
    (r'kick|^k\b|^k ', 'kick'), (r'snare|^s \d|^s\.|rim', 'snare'), (r'tom2|tom', 'tom'),
    (r'crash', 'crash'), (r'ride', 'ride'), (r'splash', 'splash'), (r'clap', 'clap'),
    (r'timp|tim ', 'timpano'), (r'rullo', 'roll'), (r'shaker', 'shaker'), (r'combo', 'combo'),
    (r'perc|cowbell|conga|bongo', 'percussion'), (r'cymbal', 'cymbal'),
]

def classify(rel):
    parts = rel.split(os.sep)
    stem = os.path.splitext(parts[-1])[0]
    low = stem.lower().replace('new_', '').strip()
    top = parts[0]
    family = 'electronic' if top in ('Electro', 'Clap') else 'acoustic'
    folder = parts[1].lower() if top == 'Electro' and len(parts) > 2 else ''
    piece = next((p for rx, p in PIECES if re.search(rx, low)), None)
    if not piece and folder:
        piece = {'hihat': 'hihat_closed'}.get(folder, folder)
    piece = piece or 'unknown'
    if family == 'electronic' and 'openhh' in low.replace(' ', ''): piece = 'hihat_open'
    m = re.search(r'(\d+)$', low)
    variant = m.group(1) if m else ''
    pack = re.sub(r'^(kick|snare|closedhh|openhh|rim|clap|tom|combo|shaker|perc\w*|cymbal)\s+', '', low, flags=re.I)
    pack = re.sub(r'\s*\d+$', '', pack).strip() if family == 'electronic' else ''
    dyn = next((d for d in ('pp', 'mp', 'p') if re.search(rf'\b{d}$', low)), '')
    art = next((a for a in ('rim', 'bordo', 'bell', 'foot', 'long') if a in low), '')
    return family, piece, pack, variant, dyn, art

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('root'); ap.add_argument('-o', default='inventario.csv')
    a = ap.parse_args()
    rows, errs = [], []
    for dp, _, fs in os.walk(a.root):
        for fn in sorted(fs):
            if not fn.lower().endswith('.wav'): continue
            p = os.path.join(dp, fn); rel = os.path.relpath(p, a.root)
            try: info = read_wav(p)
            except Exception as e: errs.append((rel, str(e))); continue
            fam, piece, pack, var, dyn, art = classify(rel)
            rows.append(dict(path=rel, family=fam, piece=piece, pack=pack, variant=var, dynamic=dyn,
                             articulation=art, channels=info['ch'], sr=info['sr'], bits=info['bits'],
                             format=info['fmt'], dur_s=round(info['dur'], 3), peak_db=round(info['peak_db'], 1)))
    with open(a.o, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0])); w.writeheader(); w.writerows(sorted(rows, key=lambda r: (r['family'], r['piece'], r['path'])))
    cnt = defaultdict(Counter)
    for r in rows: cnt[r['family']][r['piece']] += 1
    for fam, c in cnt.items():
        print(f'\n[{fam}] {sum(c.values())} file'); [print(f'  {k:14s}{v}') for k, v in c.most_common()]
    print('\nnon leggibili:', errs or 'nessuno'); print('CSV:', a.o)

if __name__ == '__main__': main()
