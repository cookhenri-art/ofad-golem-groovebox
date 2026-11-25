"""
OFAD GOLEM for MPC & Force — version anglaise des greffons — AGPL-3.0-or-later

Même traduction que l'interface de GOLEM (ui/src/i18n/en.json, recherche de ui/src/i18n/index.ts :
texte exact, nombres remplacés par « # », modèles « {1} », noms composés « A · B »), plus :
  - les textes repliés en ASCII et en capitales des greffons (« FREQUENCE » → « FREQUENCY ») ;
  - mpc/i18n_en_extra.json : les textes propres aux greffons (onglets, abréviations, noms courts).
Utilisé par build_mpc.py --lang en (table du greffon) et skins.py (images et onglets de l'écran).
"""
import json
import os
import re
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))

_REP = {"œ": "oe", "Œ": "OE", "æ": "ae", "’": "'", "‘": "'", "«": '"', "»": '"', "–": "-", "—": "-", "×": "x",
        "°": "deg", "µ": "u", "·": "-", "…": "...", "→": ">", "←": "<", "½": "1/2", "♯": "#", "♭": "b"}


def fold(s):
    """Même repli ASCII que le greffon (GolemMpc.h, skins.py)."""
    out = []
    for ch in s:
        if ch in _REP:
            out.append(_REP[ch])
            continue
        n = unicodedata.normalize("NFD", ch)
        base = "".join(c for c in n if unicodedata.category(c) != "Mn")
        out.append(base if all(ord(c) < 128 for c in base) else "")
    return " ".join("".join(out).split())


def _load():
    d = json.load(open(os.path.join(ROOT, "ui", "src", "i18n", "en.json"), encoding="utf-8"))
    extra_p = os.path.join(HERE, "i18n_en_extra.json")
    if os.path.isfile(extra_p):
        d.update({k: v for k, v in json.load(open(extra_p, encoding="utf-8")).items() if not k.startswith("_")})
    return d


DICT = _load()
NUM = re.compile(r"-?\d+(?:[.,]\d+)?")
# clés repliées (sans accents) et en capitales : les textes des greffons sont repliés en ASCII
FOLDED, UPPER = {}, {}
for _k, _v in sorted(DICT.items(), key=lambda kv: len(kv[0])):
    FOLDED.setdefault(fold(_k), _v)
    UPPER.setdefault(fold(_k).upper(), _v.upper())
EN_VALUES = set(DICT.values()) | {fold(v) for v in DICT.values()}

_PATS = []
for _k in sorted((k for k in DICT if re.search(r"\{\d\}", k)), key=lambda k: -len(re.sub(r"\{\d\}", "", k))):
    parts = re.split(r"(\{\d\})", _k)
    src = "".join("([\\s\\S]*?)" if re.fullmatch(r"\{\d\}", p) else re.escape(p).replace("\\#", "#").replace("#", r"(-?\d+(?:[.,]\d+)?)") for p in parts)
    names = re.findall(r"\{(\d)\}", _k)
    _PATS.append((re.compile("^" + src + "$"), DICT[_k], names))

_cache = {}
_EN_WORDS = {w.lower() for v in DICT.values() for w in re.findall(r"[A-Za-z]+", v)}


def _part_ok(src, tp):
    """Un morceau capté par un motif est acceptable s'il est traduit, ou si c'est un nombre, un sigle ou de l'anglais."""
    if tp != src:
        return not looks_french(tp)
    words = re.findall(r"[^\W\d_]+", src)
    return all(w.isupper() and len(w) <= 5 or (w.lower() in _EN_WORDS and not looks_french(w)) for w in words)


def _num(table, s):
    if not re.search(r"\d", s):
        return None
    nums = NUM.findall(s)
    t = table.get(NUM.sub("#", s))
    if t is None:
        return None
    it = iter(nums)
    return re.sub("#", lambda m: next(it, "#"), t)


def _lookup(s, depth, strict=False):
    if not s or depth > 4:
        return s
    for table in ((DICT,) if strict else (DICT, FOLDED, UPPER)):
        if s in table:
            return table[s]
        t = _num(table, s)
        if t is not None:
            return t
    if len(s) < 220:
        for rx, out, names in _PATS:
            m = rx.match(s)
            if m:
                parts = {}
                for i, nm in enumerate(names):
                    if nm in parts:
                        continue
                    src_part = m.group(i + 1) or ""
                    tp = translate(src_part, depth + 1)
                    parts[nm] = tp
                # un morceau resté en français : ce motif ne convient pas (évite « Band étroite »)
                if any(not _part_ok(m.group(i + 1) or "", parts[nm]) for i, nm in enumerate(names)):
                    continue
                return re.sub(r"\{(\d)\}", lambda g: parts.get(g.group(1), ""), out)
    m = re.match(r"^([·:,–—-]\s*)?([\s\S]*?)(\s*[·:,–—-])?$", s)
    if m and (m.group(1) or m.group(3)) and m.group(2):
        inner = translate(m.group(2), depth + 1)
        if inner != m.group(2):
            return (m.group(1) or "") + inner + (m.group(3) or "")
    for sep in (" · ", " - ", " : ", "+", " / ", " & "):
        if sep in s:
            parts = s.split(sep)
            out = [translate(p, depth + 1) for p in parts]
            if any(o != p for o, p in zip(out, parts)):
                return sep.join(out)
    return s


def translate(text, depth=0):
    """Texte français de GOLEM → anglais (inchangé si inconnu ou déjà anglais)."""
    if not isinstance(text, str):
        return text
    m = re.match(r"^(\s*)([\s\S]*?)(\s*)$", text)
    if not m or not m.group(2) or not re.search(r"[A-Za-zÀ-ÿ]", m.group(2)):
        return text
    core = re.sub(r"\s+", " ", m.group(2))
    if core in _cache:
        t = _cache[core]
    else:
        t = _lookup(core, depth)
        if t == core and fold(core) != core and core == core.upper():   # capitales accentuées (« ÉGALISEUR ») : repli ASCII
            f = _lookup(fold(core), depth)
            t = f if f != fold(core) else core
        mn = re.match(r"^(\d+(?:-\d+)?\s+)(\D[\s\S]*)$", core)
        if t == core and mn and depth == 0 and (residual_french(mn.group(2)) or mn.group(2).islower()):   # « 1-4 oscillateur »
            r = translate(mn.group(2))
            if r != mn.group(2):
                t = mn.group(1) + r
        if t == core and depth == 0 and core[:1].islower():        # « oscillateur », « paire 1-2 » : même mot qu'en capitale
            c = _lookup(core[0].upper() + core[1:], depth)
            if c != core[0].upper() + core[1:] and c[:1].isalpha():
                t = c[0].lower() + c[1:] if c[1:2].islower() or len(c) == 1 else c
        _cache[core] = t
    return text if t == core else m.group(1) + t + m.group(3)


# ------------------------------------------------------------------ noms des greffons
PREFIX = {"Basse": "Bass", "Nappe": "Pad", "Compresseur": "Compressor", "Limiteur": "Limiter", "Multibande": "Multiband",
          "Distorsion": "Distortion", "Transitoires": "Transients", "Filtre": "Filter", "Vocodeur": "Vocoder",
          "Retro": "Retro", "Resto": "Restoration"}


def plugin_name(name, golem_name=None):
    """« Basse - Acid Lover » → « Bass - Acid Lover » ; le nom propre suit la version anglaise de GOLEM."""
    pre, _, base = name.partition(" - ")
    # même nom que la version anglaise de GOLEM (texte exact du dictionnaire, sans les tables repliées)
    en_base = _lookup(golem_name, 0, strict=True) if golem_name else base
    if not golem_name or en_base == golem_name:   # pas de traduction : nom propre tel quel (sans accents)
        en_base = base
    return "%s - %s" % (PREFIX.get(pre, pre), fold(en_base))


def plugin_uid(uid):
    """Identifiant à 4 lettres de la version anglaise : « OIac » → « Oiac » (les deux versions cohabitent)."""
    return uid[0] + uid[1].lower() + uid[2:]


_FR_HINT = re.compile(r"[àâäéèêëîïôöùûüçœÀÂÉÈÊÎÔÛÇ]|\b(?:de|des|du|la|le|les|et|ou|pour|avec|sans|aigu|grave|réglage|reglage|"
                      r"vitesse|niveau|déclin|declin|maintien|relâchement|relachement|coupure|largeur|hauteur|accord|"
                      r"mélange|melange|réinjection|reinjection|fréquence|frequence|sortie|entrée|entree|onde|puce|"
                      r"filtre|enveloppe|retard|temps|profondeur|couche|son|sons|pas|aléatoire|aleatoire)\b", re.I)


_FR_WORDS = None


def residual_french(s):
    """Un mot français connu (clés du dictionnaire, absent des traductions) reste dans le texte."""
    global _FR_WORDS
    if _FR_WORDS is None:
        _FR_WORDS = {fold(w).lower() for k in DICT for w in re.findall(r"[^\W\d_]{4,}", k)} - _EN_WORDS
    s = re.sub(r"\b[Dd][Ee]-(?=[A-Za-z])", "", s or "")      # « De-esser », « DE-CLICK » : anglais
    return looks_french(s) or any(fold(w).lower() in _FR_WORDS for w in re.findall(r"[^\W\d_]{4,}", s or ""))


def looks_french(s):
    return bool(_FR_HINT.search(s or ""))
