#!/usr/bin/env python3
# Groovebox — OFAD — AGPL-3.0-or-later
# Écrit src/instruments/FactoryBanks.h à partir des banques (bank_*.py).
# Le niveau de chaque son est corrigé par levels.json (mesuré par groovebox_presetcheck) pour que
# toutes les banques sonnent au même niveau perçu.
#   python3 tools/presets/gen.py            -> écrit l'en-tête
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from banks_common import cpp_string, cpp_number, CATEGORY_ORDER, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, ACID
import bank_acid, bank_mono, bank_poly5, bank_polyj, bank_hybrid, bank_morph, bank_pads
import bank_fm6, bank_fm4, bank_muse, bank_fmx
import bank_hots, bank_lyra, bank_bitm
import bank_gooddoc
import bank_ms20, bank_sh101, bank_ob6, bank_dfam, bank_blo, bank_orbit, bank_gluta
import bank_voc16, bank_vocstr, bank_vocspec, bank_vocsyn

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "src", "instruments", "FactoryBanks.h")
LEVELS = os.path.join(HERE, "levels.json")

# réglage de volume de chaque instrument : (identifiant, valeur d'origine, min, max)
LEVEL_PARAM = {
    "acid": ("level", -4, -24, 6), "mono": ("level", -6, -24, 6), "poly5": ("level", -3, -24, 6),
    "polyj": ("level", -8, -24, 6), "hybrid": ("level", -4, -24, 6), "morph": ("level", -10, -24, 6),
    "pads": ("level", -10, -24, 6),
    # v0.45.1 : synthés FM et World Tour (au moins 10 sons par catégorie)
    "fm6": ("level", -18, -30, 6), "fm4": ("level", -17, -30, 6), "muse": ("level", -14, -30, 6), "fmx": ("level", -18, -30, 6),
    # v0.46 : famille Drones
    "hots": ("level", -15, -30, 6), "lyra": ("level", -14, -30, 6), "bitm": ("level", -16, -30, 6),
    # v0.47 (maj. 5 nov.) : Good Doctor, puis les banques portées à 90 sons
    "gooddoc": ("level", -5.5, -30, 6),
    "ms20": ("level", -6, -24, 6), "sh101": ("level", -6, -24, 6), "ob6": ("level", -6, -24, 6), "dfam": ("volume", -6, -24, 6),
    "blo": ("level", -8, -24, 6), "orbit": ("level", -8, -24, 6), "gluta": ("level", -5.5, -24, 6),
    "voc16": ("level", -6, -30, 6), "vocstr": ("level", -6, -30, 6), "vocspec": ("level", -6, -30, 6), "vocsyn": ("level", -6, -30, 6),
}
# v0.47 (maj. 5 nov.) : au moins 90 sons par synthé — sons déjà dans la description de l'instrument (Init compris)
OWN_PRESETS = {"acid": 5, "mono": 9, "poly5": 7, "polyj": 7, "hybrid": 10, "morph": 11, "pads": 9, "gooddoc": 1,
               "ms20": 16, "sh101": 12, "ob6": 15, "dfam": 12, "blo": 17, "orbit": 19, "gluta": 13,
               "voc16": 10, "vocstr": 10, "vocspec": 12, "vocsyn": 10}
MIN_TOTAL = 90
# v0.47 (maj. 5 nov.) : réglage qui reprend le gain manquant quand le volume est au maximum (vocodeurs : gain de la voix d'entrée)
OVERFLOW = {t: ("srcgain", -24, 24) for t in ("voc16", "vocstr", "vocspec", "vocsyn")}
PENDING = set()  # toutes les banques ont au moins 90 sons
TEN_PER_CATEGORY = {"fm6", "fm4", "muse", "fmx", "hots", "lyra", "bitm"}

# catégories des sons déjà livrés (banque « Classiques » sinon)
CLASSIC = {
    "acid": {"Acide classique": ACID, "Hurlement": ACID, "Basse ronde": BASS, "Squelch": ACID},
    "mono": {"Sub rond": BASS, "Basse grasse": BASS, "Grogne réinjectée": BASS, "Lead hurlant": LEAD, "Synchro balayée": LEAD,
             "Duo quinte": LEAD, "Enveloppe en boucle": BASS, "Battement lent": BASS},
    "poly5": {"Cuivres": BRASS, "Nappe chaude": PAD, "Cloches poly-mod": KEYS, "Synchro hurlante": LEAD, "Basse unisson": BASS,
              "Vibrato à la molette": LEAD},
    "polyj": {"Cordes chorus": STR, "Basse ronde": BASS, "Accords house": KEYS, "Nappe lente": PAD, "Cloche douce": KEYS,
              "Arpège brillant": ARP},
    "hybrid": {"Poly brillant": KEYS, "Basse FM": BASS, "Lead déchiré": LEAD, "Nappe mouvante": PAD, "Pluck numérique": PLUCK,
               "Scies denses": PAD, "Synchro balayée": LEAD, "Table vivante": PAD, "Basse mono 2": BASS},
    "morph": {"Balayage": PAD, "Voix de verre": PAD, "Basse mutante": BASS, "Cloches métalliques": KEYS, "Synchro balayée": LEAD,
              "Pile chorale": PAD, "Voyelles": FX, "Peigne métallique": FX, "Lead unisson": LEAD, "Basse sous-oscillateur": BASS},
    "pads": {"Nappe large": PAD, "Supersaw": PAD, "Pluck trance": PLUCK, "Choeur doux": PAD, "Lead épais": LEAD,
             "Quatre oscillateurs": PAD, "Pluck à la phase": PLUCK, "Basse arpège": ARP},
}

BANKS = [bank_acid, bank_mono, bank_poly5, bank_polyj, bank_hybrid, bank_morph, bank_pads, bank_fm6, bank_fm4, bank_muse, bank_fmx, bank_hots, bank_lyra, bank_bitm, bank_gooddoc,
         bank_ms20, bank_sh101, bank_ob6, bank_dfam, bank_blo, bank_orbit, bank_gluta, bank_voc16, bank_vocstr, bank_vocspec, bank_vocsyn]


def main():
    levels = json.load(open(LEVELS, encoding="utf-8")) if os.path.exists(LEVELS) else {}
    out = ["/*",
           "    Groovebox — OFAD — AGPL-3.0-or-later",
           "",
           "    Banques d'usine : 50 sons par synthé, rangés par catégories classiques (basses, leads, nappes,",
           "    plucks, claviers, cordes, cuivres, arpèges, effets). Réglages originaux construits sur les recettes",
           "    de chaque catégorie, adaptées à l'architecture de chaque instrument ; aucun préréglage d'un autre",
           "    fabricant n'est recopié. Le niveau de chaque son est égalisé (mesure de l'intensité perçue).",
           "",
           "    FICHIER ÉCRIT PAR tools/presets/gen.py : modifier les banques là-bas, pas ici.",
           "*/",
           "",
           "#pragma once",
           "",
           '#include "../engine/Params.h"',
           '#include "../synth/ModHub.h"',
           "",
           "#include <algorithm>",
           "#include <string>",
           "#include <vector>",
           "",
           "namespace grv::instruments",
           "{",
           "namespace banks",
           "{",
           ""]
    total = 0
    for mod in BANKS:
        b = mod.build()
        inst = b.instrument
        if inst in TEN_PER_CATEGORY:
            for cat in (BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX):
                n = sum(1 for it in b.items if it[1] == cat)
                assert n >= 10, f"{inst} : {n} sons {cat} (au moins 10)"
        elif inst in OWN_PRESETS and inst not in PENDING:
            n = sum(1 for it in b.items if it[1] != "Init") + OWN_PRESETS[inst]
            assert n >= MIN_TOTAL, f"{inst} : {n} sons au total (au moins {MIN_TOTAL})"
        else:
            assert sum(1 for it in b.items if it[1] != "Init") == 50, f"{inst} : {len(b)} sons au lieu de 50 (+ Init)"
        clash = set(CLASSIC.get(inst, {})) & {n for n, _, _ in b.items}
        assert not clash, f"{inst} : noms déjà pris {clash}"
        lid, ldef, lmin, lmax = LEVEL_PARAM[inst]
        out.append(f"inline std::vector<grv::engine::Preset> {inst}Bank()")
        out.append("{")
        out.append("    return {")
        for name, cat, values in b.items:
            vals = dict(values)
            off = levels.get(inst, {}).get(name)
            if off is not None:
                want = round(float(vals.get(lid, ldef)) + float(off), 1)
                over = OVERFLOW.get(inst)
                if over and want > lmax:
                    # v0.47 (maj. 5 nov.) : vocodeurs — le gain qui manque passe par le gain de la voix d'entrée (la sortie le suit)
                    oid, omin, omax = over
                    vals[oid] = max(omin, min(omax, round(float(vals.get(oid, 0)) + want - lmax, 1)))
                    want = lmax
                if want > lmax + 3.0 or want < lmin - 0.5:
                    print(f"  ATTENTION {inst} / {name} : niveau voulu {want} dB hors plage, son à retravailler")
                vals[lid] = max(lmin, min(lmax, want))
            pairs = ", ".join("{ %s, %s }" % (cpp_string(k), cpp_number(v)) for k, v in vals.items())
            out.append("        { %s, { %s }, %s }," % (cpp_string(name), pairs, cpp_string(cat)))
            total += 1
        out.append("    };")
        out.append("}")
        out.append("")
    out.append("/** Catégorie d'un son déjà livré avant les banques. */")
    out.append("inline std::string classicCategory (const std::string& type, const std::string& name)")
    out.append("{")
    for inst, m in CLASSIC.items():
        for n, c in m.items():
            out.append(f"    if (type == {cpp_string(inst)} && name == {cpp_string(n)}) return {cpp_string(c)};")
    out.append("    return {};")
    out.append("}")
    out.append("")
    out.append("/** Correction de niveau (dB) d'un son déjà livré, mesurée comme celles des banques. */")
    out.append("inline float classicLevelOffset (const std::string& type, const std::string& name)")
    out.append("{")
    for inst, m in CLASSIC.items():
        for n in m:
            off = levels.get(inst, {}).get(n)
            if off:
                out.append(f"    if (type == {cpp_string(inst)} && name == {cpp_string(n)}) return {cpp_number(round(float(off), 1))};")
    out.append("    return 0.0f;")
    out.append("}")
    out.append("")
    out.append("} // namespace banks")
    out.append("")
    out.append("/** Ajoute la banque d'usine de l'instrument (appelé à la fin de chaque description). */")
    out.append("inline void addFactoryBank (grv::engine::ModuleDesc& m)")
    out.append("{")
    out.append("    for (auto& p : m.presets)")
    out.append("    {")
    out.append("        if (p.category.empty()) p.category = banks::classicCategory (m.type, p.name);")
    out.append("        // niveau des sons déjà livrés mis au niveau des banques")
    out.append("        const auto off = banks::classicLevelOffset (m.type, p.name);")
    out.append("        const auto li = m.indexOf (\"level\");")
    out.append("        if (off == 0.0f || li < 0) continue;")
    out.append("        const auto& lp = m.params[(size_t) li];")
    out.append("        bool found = false;")
    out.append("        for (auto& v : p.values) if (v.first == \"level\") { v.second = std::clamp (v.second + off, lp.min, lp.max); found = true; }")
    out.append("        if (! found) p.values.push_back ({ \"level\", std::clamp (lp.def + off, lp.min, lp.max) });")
    out.append("    }")
    out.append("    std::vector<grv::engine::Preset> bank;")
    for mod in BANKS:
        inst = mod.build().instrument
        out.append(f"    if (m.type == {cpp_string(inst)}) bank = banks::{inst}Bank();")
    out.append("    for (auto& p : bank) m.presets.push_back (std::move (p));")
    out.append("    grv::synth::resolveHubPresetDests (m); // destinations de matrice écrites « mx1_dst@réglage »")
    out.append("}")
    out.append("")
    out.append("} // namespace grv::instruments")
    out.append("")
    open(OUT, "w", encoding="utf-8").write("\n".join(out))
    print(f"{total} sons -> {OUT}")


if __name__ == "__main__":
    main()
