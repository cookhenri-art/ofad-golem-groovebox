# Groovebox — OFAD — AGPL-3.0-or-later
# Outils communs aux banques d'usine : catégories, fusion d'archétypes, écriture du C++.
# Les sons sont des réglages originaux construits à partir des recettes classiques de chaque
# catégorie (aucun préréglage d'un autre fabricant n'est recopié).

BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, ACID = (
    "Basses", "Leads", "Nappes", "Plucks", "Claviers", "Cordes", "Cuivres", "Arpèges", "Effets", "Acide")
# v0.47 (maj. 5 nov.) : catégories des synthés de percussion (comme les sons d'origine de Kick Killer)
PERC, SEQ, VOICE = "Percussions", "Séquences", "Voix"
# « Init » : le point de départ (un oscillateur simple, filtre ouvert, enveloppes de base, rien d'autre)
INIT = "Init"

CATEGORY_ORDER = [INIT, ACID, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, PERC, SEQ, VOICE]


def mix(*parts, **kw):
    """Fusionne des dictionnaires de réglages (le dernier l'emporte)."""
    out = {}
    for p in parts:
        out.update(p)
    out.update(kw)
    return out


class Bank:
    def __init__(self, instrument):
        self.instrument = instrument
        self.items = []  # (nom, catégorie, réglages)

    def add(self, name, category, values):
        assert category in CATEGORY_ORDER, category
        assert all(n != name for n, _, _ in self.items), f"{self.instrument}: nom en double {name}"
        # « as », « async »… sont des mots réservés en Python : on les écrit as_, async_
        clean = {(k[:-1] if k.endswith("_") else k): v for k, v in values.items()}
        self.items.append((name, category, clean))

    def __len__(self):
        return len(self.items)


def cpp_string(text):
    """Chaîne C++ en ASCII : les caractères non ASCII deviennent des échappements \\x (UTF-8)."""
    out = '"'
    prev_hex = False
    for ch in text:
        if ord(ch) < 128:
            if prev_hex and ch in "0123456789abcdefABCDEF":
                out += '" "'
            if ch in '"\\':
                out += "\\" + ch
            else:
                out += ch
            prev_hex = False
        else:
            for b in ch.encode("utf-8"):
                out += "\\x%02X" % b
            prev_hex = True
    return out + '"'


def cpp_number(v):
    if isinstance(v, bool):
        return "1" if v else "0"
    if float(v).is_integer():
        return str(int(v))
    s = ("%.4f" % float(v)).rstrip("0").rstrip(".")
    return s + "f"
