"""
Groovebox — OFAD — AGPL-3.0-or-later
1.1 : petite bibliothèque pour écrire les projets de démonstration en Python (grilles de pas,
lignes de notes, accords, arpèges, morceau, automatisations). Produit la description JSON que lit
l'outil groovebox_demos (tools/BuildDemos.cpp).

Conventions : 16 pas par mesure ; notes MIDI avec do 4 = 60 (la 1 = 33, fa 1 = 29) ; durée d'une
note en huitièmes de pas (8 = un pas) ; micro-décalage en 1/64 de pas.
"""

import json

NOTE = {'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3, 'E': 4, 'F': 5, 'F#': 6, 'Gb': 6,
        'G': 7, 'G#': 8, 'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11}

ACCENT, SLIDE = 1, 2
# conditions (Sequencer.h) : 1 A:B, 2 Fill, 3 sans Fill, 8 première fois, 9 pas la première
AB, FILL, NOTFILL, FIRST, NOTFIRST = 1, 2, 3, 8, 9


def n(name):
    """« A1 » → 33 ; « C#4 » → 61."""
    i = 1 if len(name) > 1 and name[1] in '#b' else 0
    return NOTE[name[:i + 1]] + 12 * (int(name[i + 1:]) + 1)


def hz(note):
    return 440.0 * 2 ** ((note - 69) / 12)


CHORD = {'': [0, 4, 7], 'm': [0, 3, 7], '7': [0, 4, 7, 10], 'm7': [0, 3, 7, 10], 'maj7': [0, 4, 7, 11],
         'm9': [0, 3, 7, 10, 14], 'sus2': [0, 2, 7], 'sus4': [0, 5, 7], '5': [0, 7, 12], 'm11': [0, 3, 7, 10, 14, 17],
         'add9': [0, 4, 7, 14], 'madd9': [0, 3, 7, 14]}


def chord(root, kind='m', inv=0):
    """Notes d'un accord : chord('A3', 'm') → [57, 60, 64] ; inv : renversements vers le haut."""
    r = n(root) if isinstance(root, str) else root
    notes = [r + i for i in CHORD[kind]]
    for _ in range(inv):
        notes = notes[1:] + [notes[0] + 12]
    return notes


class Pat:
    """Un pattern : len pas (16 = une mesure), lignes, notes, conditions, verrous."""

    def __init__(self, bars=1, div='1/16', speed=4):
        self.len = 16 * bars
        self.div = div
        self.speed = speed
        self.steps = {}       # (ligne, pas) → [vel, note, len8, micro, flags, extra]
        self.lane_len = {}

    # --- batteries : une ligne = un pad ---
    def grid(self, lane, s, vel=100, ghost=None, acc=None, at=0, note=60, len8=6):
        """Grille « x...x... » : x = vel, X = accent (acc, 127 par défaut), o = fantôme (ghost),
        chiffres 1-9 = vélocité 14×chiffre ; espaces et | ignorés ; répétée jusqu'à la fin du pattern."""
        s = s.replace(' ', '').replace('|', '')
        ghost = ghost if ghost is not None else max(1, int(vel * 0.45))
        acc = acc if acc is not None else min(127, int(vel * 1.25))
        k = 0
        while at + k < self.len:
            c = s[k % len(s)]
            v = {'x': vel, 'X': acc, 'o': ghost}.get(c, 14 * int(c) if c.isdigit() and c != '0' else 0)
            if v:
                self.steps[(lane, at + k)] = [v, note, len8, 0, 0, None]
            k += 1
            if k >= len(s) and at + k >= self.len:
                break
        return self

    def hit(self, lane, step, vel=100, note=60, len8=6, micro=0, flags=0, **extra):
        self.steps[(lane, step)] = [vel, note, len8, micro, flags, extra or None]
        return self

    def extra(self, lane, step, **extra):
        if (lane, step) in self.steps:
            cur = self.steps[(lane, step)][5] or {}
            cur.update(extra)
            self.steps[(lane, step)][5] = cur
        return self

    def lock(self, lane, step, **locks):
        """Verrous : lock(0, 4, **{'inst.cutoff': 1200})."""
        if (lane, step) in self.steps:
            cur = self.steps[(lane, step)][5] or {}
            cur.setdefault('locks', {}).update(locks)
            self.steps[(lane, step)][5] = cur
        return self

    def roll(self, lane, start, count, every=1, v0=50, v1=120, len8=4):
        """Roulement : count coups tous les every pas, vélocité de v0 à v1."""
        for i in range(count):
            v = int(v0 + (v1 - v0) * i / max(1, count - 1))
            self.steps[(lane, start + i * every)] = [v, 60, len8, 0, 0, None]
        return self

    # --- mélodie : ligne de notes, une case par pas ---
    def line(self, s, lane=0, vel=100, len8=6, at=0, acc=None):
        """« A1 . A2> . C2~ - . » : note, '>' accent, '~' glissé vers la suivante, '-' tenue (prolonge),
        '.' silence, ':v' vélocité (A1:80). Répété jusqu'à la fin du pattern si plus court."""
        toks = s.split()
        acc = acc if acc is not None else min(127, vel + 20)
        k = 0
        last = None
        while at + k < self.len:
            t = toks[k % len(toks)]
            st = at + k
            if t == '-':
                if last is not None:
                    self.steps[last][2] += 8
            elif t != '.':
                flags = 0
                v = vel
                if ':' in t:
                    t, vv = t.split(':')
                    v = int(vv)
                while t and t[-1] in '>~':
                    if t[-1] == '>':
                        flags |= ACCENT
                        v = max(v, acc)
                    else:
                        flags |= SLIDE
                    t = t[:-1]
                self.steps[(lane, st)] = [v, n(t), len8, 0, flags, None]
                last = (lane, st)
            else:
                last = None
            k += 1
            if k >= len(toks) and k % len(toks) == 0 and at + k >= self.len:
                break
        return self

    def chords(self, at, notes, len8=120, vel=95):
        """Un accord (liste de notes) sur les lignes 0, 1, 2… au pas at."""
        for i, nt in enumerate(notes):
            self.steps[(i, at)] = [vel, nt, len8, 0, 0, None]
        return self

    def arp(self, at, notes, order, count, every=1, vel=95, len8=5, lane=0):
        """Arpège : order = indices dans notes (« 0 1 2 1 »), count notes tous les every pas."""
        idx = [int(x) for x in order.split()]
        for i in range(count):
            self.steps[(lane, at + i * every)] = [vel, notes[idx[i % len(idx)] % len(notes)] + 12 * (idx[i % len(idx)] // len(notes)), len8, 0, 0, None]
        return self

    def to_json(self):
        steps = []
        flags_at = {}
        for (lane, st), (v, nt, l8, mi, fl, ex) in sorted(self.steps.items()):
            if st >= self.len:
                continue
            row = [lane, st, int(v), int(nt), int(l8), int(mi), int(fl)]
            if ex:
                row.append(ex)
            steps.append(row)
        d = {'len': self.len, 'div': self.div, 'speed': self.speed, 'steps': steps}
        if self.lane_len:
            d['laneLen'] = {str(k): v for k, v in self.lane_len.items()}
        return d


def fx(type_, preset=None, **params):
    d = {'type': type_, 'params': params}
    if preset:
        d['preset'] = preset
    return d


class Demo:
    def __init__(self, file, name, bpm, swing=50, key=''):
        self.file, self.name, self.bpm, self.swing, self.key = file, name, bpm, swing, key
        self.tracks, self.returns, self.master = [], [], {'inserts': [], 'mix': {}}
        self.rows, self.autos = [], []
        self.target, self.ref = None, 0
        self.master['mix'] = {'volume': 0}

    def track(self, inst, name, preset=None, params=None, mix=None, inserts=None, color=None, mods=None, accent=None, rel=None):
        t = {'inst': inst, 'name': name, 'params': params or {}, 'mix': mix or {}, 'inserts': inserts or [],
             'patterns': {}, 'mods': mods or []}
        if rel is not None:
            t['rel'] = rel  # sonie visée, seule, par rapport à la piste de référence (LU)
        if preset:
            t['preset'] = preset
        if color is not None:
            t['color'] = color
        if accent is not None:
            t['accent'] = accent
        self.tracks.append(t)
        return len(self.tracks) - 1

    def pattern(self, t, letter, pat):
        self.tracks[t]['patterns'][letter] = pat.to_json()

    def ret(self, inserts, mix=None):
        self.returns.append({'inserts': inserts, 'mix': mix or {}})

    def row(self, label, bars, **pats):
        """row('Intro', 16, t0='A', t1='stop') ; les pistes non nommées gardent ce qu'elles jouent."""
        p = {k[1:]: v for k, v in pats.items()}
        r = {'bars': bars, 'pat': p}
        if label:
            r['label'] = label
        self.rows.append(r)

    def bar_of(self, label):
        b = 0
        for r in self.rows:
            if r.get('label') == label:
                return b
            b += r['bars']
        raise KeyError(label)

    def beat(self, bar):
        return 4.0 * bar

    def auto(self, ch, area, id_, points):
        """points : [(mesure, valeur)…] en mesures depuis le début (converties en noires)."""
        self.autos.append({'ch': ch, 'area': area, 'id': id_, 'points': [[4.0 * b, v] for b, v in points]})

    def to_json(self):
        extra = {'target': self.target} if self.target is not None else {}
        return {**extra, 'ref': self.ref, 'name': self.name, 'bpm': self.bpm, 'swing': self.swing, 'key': self.key, 'tracks': self.tracks,
                'returns': self.returns, 'master': self.master, 'song': {'rows': self.rows, 'loop': True},
                'auto': self.autos}

    def save(self, path):
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(self.to_json(), f, ensure_ascii=False, indent=1)


RET_A, RET_B, RET_C, RET_D, MASTER = 16, 17, 18, 19, 20
