/*
    OFAD GOLEM — portage des instruments et des effets vers le kit de greffons MPC — AGPL-3.0-or-later

    Adaptateur générique : un module GOLEM (grv::engine::Instrument ou grv::engine::Effect) devient
    un greffon mpc::Plugin, sans toucher à son DSP.

      - Réglage 0 : « Volume » de sortie du greffon (identifiant « mpcvol »). L'hôte bouscule le
        réglage 0 à chaque préparation : on y met donc un réglage sans effet coûteux.
      - Réglages 1..n : ceux du module, dans l'ordre de sa description (positions figées dans GOLEM :
        l'ordre et les identifiants restent stables d'une version à l'autre).
      - Noms courts (≈ 16 caractères), sans accents (police de l'écran inconnue), uniques
        (le fichier d'interface désigne les réglages par leur nom).
      - Notes MIDI découpées au bon échantillon ; batteries : notes 36 à 51 → pads 1 à 16.
      - Tempo et position de lecture de l'hôte transmis au module.
      - Sons d'usine du module → programmes de l'hôte (programme 0 : réglages par défaut).

    Règles temps réel : rien n'est alloué dans process() ; tout est créé à la construction ou dans
    prepare(). La disposition des pages (golem_mpc_layout) est lue par le générateur, jamais par l'hôte.
*/
#pragma once

#include "mpc_plugin.h"
#include "mpc_resources.h"
#include "engine/Module.h"
#include "engine/Speech.h"
#include "engine/Arpeggiator.h"
#include "synth/SamplePlayer.h"

#include <array>
#include <atomic>
#include <chrono>
#include <dirent.h>
#include <mutex>
#include <functional>
#include <sys/stat.h>
#include <thread>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined (GRV_MPC_EN)
/** Version anglaise des greffons (build_mpc.py --lang en) : traduction d'un texte français de GOLEM,
    nullptr si le texte n'a pas de traduction. Définie dans le .cpp généré du greffon (table du module). */
const char* golemEn (const std::string& fr);
#endif

namespace grv::mpcport
{

using grv::engine::ModuleDesc;
using grv::engine::ParamDesc;

/** Texte affiché dans la langue du greffon (français, ou anglais avec GRV_MPC_EN). */
inline std::string L (const std::string& fr)
{
#if defined (GRV_MPC_EN)
    if (const char* t = golemEn (fr)) return t;
#endif
    return fr;
}

/** Fiche du module dans la langue du greffon : noms des réglages, choix, sections, sons d'usine, pads.
    Les identifiants et les valeurs ne changent jamais (sauvegardes et sons identiques dans les deux langues). */
inline const ModuleDesc& localized (const ModuleDesc& d)
{
#if defined (GRV_MPC_EN)
    static std::vector<std::pair<const ModuleDesc*, std::unique_ptr<ModuleDesc>>> done; // une fiche par type (construction, hors du fil audio)
    for (const auto& e : done) if (e.first == &d) return *e.second;
    auto t = std::make_unique<ModuleDesc> (d);
    t->name = L (t->name);
    for (auto& p : t->params)
    {
        p.name = L (p.name);
        for (auto& c : p.choices) c = L (c);
        // unités affichées en toutes lettres (les autres sont des symboles : %, ms, dB, Hz…)
        if (p.unit == "pas") p.unit = "steps";
        else if (p.unit == "img") p.unit = "fr";
        else if (p.unit == "demi-tons") p.unit = "semitones";
    }
    for (auto& sec : t->sections) sec.name = L (sec.name);
    for (auto& pr : t->presets) { pr.name = L (pr.name); pr.category = L (pr.category); }
    for (auto& n : t->padNames) n = L (n);
    for (auto& n : t->machineNames) n = L (n);
    for (auto& v : t->machineLabels) for (auto& n : v) n = L (n);
    done.emplace_back (&d, std::move (t));
    return *done.back().second;
#else
    return d;
#endif
}

//==============================================================================
/** UTF-8 → ASCII (français) : « Résonance » → « Resonance ». */
inline std::string fold (const std::string& s)
{
    std::string out;
    out.reserve (s.size());
    for (size_t i = 0; i < s.size();)
    {
        const auto c = (unsigned char) s[i];
        if (c < 0x80) { out += (char) c; ++i; continue; }
        // point de code
        unsigned cp = 0; int len = 1;
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        for (int k = 1; k < len && i + (size_t) k < s.size(); ++k) cp = (cp << 6) | ((unsigned char) s[i + (size_t) k] & 0x3F);
        i += (size_t) len;
        const char* r = "";
        switch (cp)
        {
            case 0xE0: case 0xE2: case 0xE4: case 0xE1: r = "a"; break;
            case 0xC0: case 0xC2: case 0xC4: case 0xC1: r = "A"; break;
            case 0xE9: case 0xE8: case 0xEA: case 0xEB: r = "e"; break;
            case 0xC9: case 0xC8: case 0xCA: case 0xCB: r = "E"; break;
            case 0xEE: case 0xEF: case 0xED: r = "i"; break;
            case 0xCE: case 0xCF: r = "I"; break;
            case 0xF4: case 0xF6: case 0xF3: r = "o"; break;
            case 0xD4: case 0xD6: r = "O"; break;
            case 0xF9: case 0xFB: case 0xFC: case 0xFA: r = "u"; break;
            case 0xD9: case 0xDB: case 0xDC: r = "U"; break;
            case 0xE7: r = "c"; break;
            case 0xC7: r = "C"; break;
            case 0x153: r = "oe"; break;
            case 0x152: r = "OE"; break;
            case 0xE6: r = "ae"; break;
            case 0xF1: r = "n"; break;
            case 0x2019: case 0x2018: r = "'"; break;
            case 0xAB: case 0xBB: case 0x201C: case 0x201D: r = "\""; break;
            case 0x2013: case 0x2014: case 0x2212: r = "-"; break;
            case 0xD7: r = "x"; break;
            case 0xB0: r = "deg"; break;
            case 0xB5: r = "u"; break;
            case 0xB7: case 0x2022: r = "-"; break;
            case 0x2026: r = "..."; break;
            case 0xA0: case 0x202F: r = " "; break;
            case 0x2192: r = ">"; break;
            case 0x2190: r = "<"; break;
            case 0xBD: r = "1/2"; break;
            case 0x266F: r = "#"; break;
            case 0x266D: r = "b"; break;
            default: r = ""; break;
        }
        out += r;
    }
    // espaces multiples
    std::string t;
    for (char ch : out) if (! (ch == ' ' && (t.empty() || t.back() == ' '))) t += ch;
    while (! t.empty() && t.back() == ' ') t.pop_back();
    return t;
}

inline std::string cut (std::string s, size_t n)
{
    if (s.size() > n) { s.resize (n); while (! s.empty() && s.back() == ' ') s.pop_back(); }
    return s;
}

/** Abréviation d'un préfixe : « Couche 1 » → « C1 », « Modulation 2 » → « M2 ». */
inline std::string abbrev (const std::string& folded)
{
    std::string a;
    bool start = true;
    for (char ch : folded)
    {
        if (ch == ' ' || ch == '-' || ch == '/' || ch == '\'') { start = true; continue; }
        if (ch >= '0' && ch <= '9') { a += ch; start = false; continue; }
        if (start) { a += (char) std::toupper ((unsigned char) ch); start = false; }
    }
    return a.empty() ? std::string ("X") : a;
}

constexpr size_t maxName = 16;

/** Raccourcit un libellé trop long : retire « de », « d' », « du », « des », « la », « le », « les ». */
inline std::string squeeze (const std::string& s)
{
    static const char* stop[] = { "de ", "d'", "du ", "des ", "la ", "le ", "les ", "l'", "a l'", "au ", "aux " };
    std::string out;
    size_t i = 0;
    while (i < s.size())
    {
        bool removed = false;
        if (i == 0 || s[i - 1] == ' ')
            for (const char* w : stop)
            {
                const size_t n = std::strlen (w);
                if (s.compare (i, n, w) == 0 && i > 0) { i += n; removed = true; break; }
            }
        if (! removed) out += s[i++];
    }
    return out;
}

/** Section (première) de chaque réglage, -1 si aucune. */
inline std::vector<int> sectionOf (const ModuleDesc& d)
{
    std::vector<int> s (d.params.size(), -1);
    for (size_t k = 0; k < d.sections.size(); ++k)
        for (const auto& id : d.sections[k].params)
        {
            const int i = d.indexOf (id);
            if (i >= 0 && s[(size_t) i] < 0) s[(size_t) i] = (int) k;
        }
    return s;
}

/** Mots longs → abréviations (seulement si le libellé dépasse la place). */
inline std::string shorten (std::string n)
{
    static const std::pair<const char*, const char*> words[] = {
        { "Frequence", "Freq" }, { "frequence", "freq" }, { "Distorsion", "Dist" }, { "distorsion", "dist" },
        { "Melange", "Mix" }, { "melange", "mix" }, { "Resonance", "Reso" }, { "resonance", "reso" },
        { "Polarite inversee", "Polarite" }, { "Verrou (hasard)", "Verrou" }, { "Sample accorde", "Accorde" },
        { "Modulation", "Mod" }, { "modulation", "mod" }, { "Enveloppe", "Env" }, { "enveloppe", "env" },
        { "Oscillateur", "Osc" }, { "oscillateur", "osc" }, { "Panoramique", "Pan" }, { "panoramique", "pan" },
        { "Asymetrie", "Asym" }, { "asymetrie", "asym" }, { "Algorithme", "Algo" }, { "algorithme", "algo" },
        { "Reinjection", "Reinj" }, { "reinjection", "reinj" }, { "Dynamique", "Dyn" }, { "dynamique", "dyn" },
        { "Destination", "Dest" }, { "destination", "dest" }, { "Separation", "Sep" }, { "Quantite", "Qte" },
        { "Profondeur", "Prof" }, { "profondeur", "prof" }, { "Velocite", "Velo" }, { "velocite", "velo" },
        { "Sensibilite", "Sens" }, { "sensibilite", "sens" }, { "Relachement", "Relache" }, { "relachement", "relache" },
        { "automatique", "auto" }, { "Alignement", "Align" }, { "passe-", "p-" }, { "Passe-", "P-" },
        // version anglaise
        { "Frequency", "Freq" }, { "frequency", "freq" }, { "Resonance", "Reso" }, { "Envelope", "Env" }, { "envelope", "env" },
        { "Oscillator", "Osc" }, { "oscillator", "osc" }, { "Feedback", "Fdbk" }, { "feedback", "fdbk" }, { "Distortion", "Dist" },
        { "distortion", "dist" }, { "Velocity", "Velo" }, { "velocity", "velo" }, { "Sensitivity", "Sens" }, { "Release", "Rel" },
        { "release", "rel" }, { "Attack", "Atk" }, { "attack", "atk" }, { "Decay", "Dec" }, { "decay", "dec" }, { "Sustain", "Sus" },
        { "Amount", "Amt" }, { "amount", "amt" }, { "Depth", "Dpth" }, { "depth", "dpth" }, { "Position", "Pos" }, { "position", "pos" },
        { "Transpose", "Transp" }, { "Polarity", "Polar" }, { "Low-pass", "LP" }, { "High-pass", "HP" }, { "Band-pass", "BP" },
        { "Modulator", "Mod" }, { "Asymmetry", "Asym" }, { "Saturation", "Satur" }, { "Threshold", "Thresh" }, { "Character", "Char" },
        { "low-pass", "LP" }, { "high-pass", "HP" }, { "band-pass", "BP" }, { "fade-out", "f-out" }, { "fade-in", "f-in" },
        { "Synthesis level", "Synth lvl" }, { "synthesis level", "synth lvl" },
    };
    if (n.size() <= maxName) return n;
    n = squeeze (n);
    for (const auto& w : words)
    {
        if (n.size() <= maxName) break;
        for (size_t at = n.find (w.first); at != std::string::npos; at = n.find (w.first, at + std::strlen (w.second)))
            n.replace (at, std::strlen (w.first), w.second);
    }
    return n;
}

/** Abréviation plus longue d'un préfixe (« Finaliseur » → « Fin », « Filtres » → « Fil »). */
inline std::string abbrev2 (const std::string& folded)
{
    std::string a, digits;
    size_t k = 0;
    while (k < folded.size() && folded[k] != ' ' && a.size() < 3) a += folded[k++];
    for (char ch : folded) if (ch >= '0' && ch <= '9') digits += ch;
    if (! a.empty()) a[0] = (char) std::toupper ((unsigned char) a[0]);
    return a + digits;
}

/** Noms des réglages vus par le MPC (sans le Volume du greffon), uniques. */
inline std::vector<std::string> mpcNames (const ModuleDesc& d)
{
    const auto sec = sectionOf (d);
    const size_t np = d.params.size();
    // préfixe (« Couche 1 : », « Grave - ») et suite du libellé
    std::vector<std::string> pre (np), tail (np);
    for (size_t i = 0; i < np; ++i)
    {
        auto n = fold (d.params[i].name);
        size_t c = n.find (" : "), w = 3;
        if (c == std::string::npos) { c = n.find (" - "); w = 3; }
        // « : » entre parenthèses (« Canal de T1 (T2 a T4 : les suivants) ») : pas un préfixe, on ôte la parenthèse
        if (c != std::string::npos && c > 0)
        {
            const auto head = n.substr (0, c);
            if (std::count (head.begin(), head.end(), '(') > std::count (head.begin(), head.end(), ')'))
            {
                auto o = n.find (" (");
                tail[i] = o != std::string::npos ? n.substr (0, o) : n;
                continue;
            }
        }
        if (c != std::string::npos && c > 0)
        {
            pre[i] = n.substr (0, c);
            auto t = n.substr (c + w);
            for (size_t at = t.find (" : "); at != std::string::npos; at = t.find (" : ")) t.replace (at, 3, " ");
            if (! t.empty()) t[0] = (char) std::toupper ((unsigned char) t[0]);
            tail[i] = t;
        }
        else tail[i] = n;
    }
    auto make = [&] (size_t i, int level)
    {
        std::string n = tail[i];
        if (! pre[i].empty()) n = (level == 0 ? abbrev (pre[i]) : abbrev2 (pre[i])) + " " + n;
        return cut (shorten (n), maxName);
    };
    std::vector<std::string> base (np);
    for (size_t i = 0; i < np; ++i) base[i] = make (i, 0);
    auto dupCount = [&] (size_t i)
    {
        int c = 0;
        for (size_t j = 0; j < np; ++j) if (base[j] == base[i]) ++c;
        return c;
    };
    // 1) préfixe plus long pour les doublons qui en ont un
    {
        std::vector<std::string> next = base;
        for (size_t i = 0; i < np; ++i) if (dupCount (i) > 1 && ! pre[i].empty()) next[i] = make (i, 1);
        base = next;
    }
    // 2) préfixe de la section pour les doublons restants sans préfixe
    {
        std::vector<std::string> next = base;
        for (size_t i = 0; i < np; ++i)
            if ((dupCount (i) > 1 || base[i] == "Volume") && pre[i].empty() && sec[i] >= 0)
                next[i] = cut (shorten (abbrev2 (fold (d.sections[(size_t) sec[i]].name)) + " " + base[i]), maxName);
        base = next;
    }
    // 3) numéros
    std::vector<std::string> out (np);
    auto taken = [&] (const std::string& s, size_t upto)
    {
        if (s == "Volume") return true; // réservé au réglage 0
        for (size_t j = 0; j < upto; ++j) if (out[j] == s) return true;
        return false;
    };
    for (size_t i = 0; i < np; ++i)
    {
        std::string n = base[i];
        if (taken (n, i))
            for (int k = 2; k < 1000; ++k)
            {
                auto suffix = " " + std::to_string (k);
                auto m = cut (n, maxName - suffix.size()) + suffix;
                if (! taken (m, i)) { n = m; break; }
            }
        out[i] = n;
    }
    return out;
}

/** Réglage MPC équivalent (mêmes bornes, même courbe que GOLEM). */
inline mpc::Param mpcParam (const ParamDesc& p, const std::string& name)
{
    mpc::Param q;
    q.id = p.id;
    q.name = name;
    q.unit = fold (p.unit);
    if (! p.choices.empty())
    {
        q.kind = mpc::Param::Kind::Choice;
        for (const auto& c : p.choices) q.choices.push_back (cut (fold (c), 14));
        if (q.choices.size() == 1) q.choices.push_back (q.choices[0]);
        q.min = 0; q.max = (float) q.choices.size() - 1;
        q.def = std::clamp (std::round (p.def), 0.0f, q.max);
    }
    else if (p.integer)
    {
        q.kind = mpc::Param::Kind::Int;
        q.min = p.min; q.max = p.max; q.def = std::round (p.def);
    }
    else
    {
        q.kind = mpc::Param::Kind::Float;
        q.min = p.min; q.max = p.max; q.def = p.def; q.skew = p.skew;
        const auto range = std::fabs (p.max - p.min);
        q.decimals = range >= 100.0f ? 0 : (range >= 10.0f ? 1 : 2);
    }
    return q;
}

/** Texte affiché (l'hôte n'ajoute pas l'unité). */
inline bool formatGolem (const ParamDesc& p, float v, std::string& out)
{
    if (! p.choices.empty()) return false; // texte du choix
    char buf[48];
    if (p.unit == "pan")
    {
        const int x = (int) std::lround (v);
        if (x == 0) out = "C";
        else { std::snprintf (buf, sizeof (buf), "%s%d", x < 0 ? "L" : "R", std::abs (x)); out = buf; }
        return true;
    }
    const auto range = std::fabs (p.max - p.min);
    const int dec = p.integer ? 0 : (std::fabs (v) >= 1000.0f || range >= 100.0f ? 0 : (range >= 10.0f ? 1 : 2));
    if (p.unit == "Hz" && std::fabs (v) >= 1000.0f) std::snprintf (buf, sizeof (buf), "%.2fk", (double) v / 1000.0);
    else if (p.unit == "ms" && std::fabs (v) >= 1000.0f) std::snprintf (buf, sizeof (buf), "%.2fs", (double) v / 1000.0);
    else
    {
        std::snprintf (buf, sizeof (buf), "%.*f", dec, (double) v);
        if (buf[0] == '-' && std::strspn (buf + 1, "0.") == std::strlen (buf + 1)) std::memmove (buf, buf + 1, std::strlen (buf));
        const auto u = fold (p.unit);
        if (! u.empty() && u != "dt" && std::strlen (buf) + u.size() + 1 < sizeof (buf))
        {
            if (u != "%" && u != ":1" && u != "x") std::strcat (buf, " ");
            std::strcat (buf, u.c_str());
        }
    }
    out = buf;
    return true;
}

/** Nom de page (touche de fonction : 8 caractères). */
inline std::string pageName (const std::string& folded)
{
    static const std::pair<const char*, const char*> words[] = {
        { "Egaliseur", "EQ" }, { "Distorsion", "Dist" }, { "Transitoires", "Transit" }, { "Convolution", "Convol" },
        { "Finaliseur", "Final" }, { "Multibande", "Multib" }, { "Faconneur", "Faconn" }, { "Modulation", "Mod" },
        { "Amplitude", "Ampli" }, { "Oscillateur", "Osc" }, { "Oscillateurs", "Osc" }, { "Enveloppe", "Env" },
        { "Enveloppes", "Env" }, { "Bas-medium", "Bas-med" }, { "Haut-medium", "Haut-med" }, { "Reverberation", "Reverb" },
        { "Compresseur", "Comp" }, { "Saturation", "Satur" }, { "Arpegiateur", "Arpeg" }, { "Sequenceur", "Sequ" },
        { "Generateur", "Gener" }, { "Resonateur", "Reson" }, { "Harmoniques", "Harmo" }, { "Echantillon", "Sample" },
        // version anglaise
        { "Equalizer", "EQ" }, { "Distortion", "Dist" }, { "Transients", "Transnt" }, { "Oscillators", "Osc" }, { "Oscillator", "Osc" },
        { "Envelopes", "Env" }, { "Envelope", "Env" }, { "Arpeggiator", "Arp" }, { "Sequencer", "Seq" }, { "Generator", "Gen" },
        { "Resonator", "Reson" }, { "Harmonics", "Harmo" }, { "Compressor", "Comp" }, { "Modulation", "Mod" }, { "Finalizer", "Final" },
        { "Multiband", "Multib" }, { "Low-mid", "Low-mid" }, { "High-mid", "Hi-mid" },
    };
    std::string n = folded;
    for (const auto& w : words)
        for (size_t at = n.find (w.first); at != std::string::npos; at = n.find (w.first, at + std::strlen (w.second)))
            n.replace (at, std::strlen (w.first), w.second);
    if (n.size() > 8)
    {
        n = squeeze (n);
        for (const char* et : { " et ", " & " }) { const auto at = n.find (et); if (at != std::string::npos && at > 0) n = n.substr (0, at); }
    }
    if (n.size() > 8) { const auto sp = n.find (' '); if (sp != std::string::npos && sp >= 3 && n.find_first_of ("0123456789") == std::string::npos) n = n.substr (0, sp); }
    return cut (n, 8);
}

//==============================================================================
/** Disposition des pages : sections en rangées de 4 (rangées = groupes de Q-Links), 4 rangées par page. */
/** Fiche du module au format du catalogue de l'interface (ui/src/engine/catalog.json), pour l'habillage
    des greffons propres à la MPC (pas encore dans GOLEM). */
inline std::string catalogJson (const ModuleDesc& d)
{
    auto esc = [] (const std::string& s)
    {
        std::string o;
        for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
        return "\"" + o + "\"";
    };
    auto num = [] (float v) { char b[32]; std::snprintf (b, sizeof (b), "%g", (double) v); return std::string (b); };
    std::string o = "{\"type\": " + esc (d.type) + ", \"name\": " + esc (d.name) + ", \"family\": " + esc (d.family)
                  + ", \"principle\": " + esc (d.principle) + ", \"drumKit\": " + (d.drumKit ? "true" : "false") + ", \"params\": [";
    for (size_t i = 0; i < d.params.size(); ++i)
    {
        const auto& p = d.params[i];
        o += (i ? ", " : "") + std::string ("{\"id\": ") + esc (p.id) + ", \"name\": " + esc (p.name) + ", \"unit\": " + esc (p.unit)
           + ", \"min\": " + num (p.min) + ", \"max\": " + num (p.max) + ", \"def\": " + num (p.def) + ", \"skew\": " + num (p.skew);
        if (p.integer) o += ", \"integer\": true";
        if (! p.choices.empty())
        {
            o += ", \"choices\": [";
            for (size_t k = 0; k < p.choices.size(); ++k) o += (k ? ", " : "") + esc (p.choices[k]);
            o += "]";
        }
        o += "}";
    }
    o += "], \"sections\": [";
    for (size_t i = 0; i < d.sections.size(); ++i)
    {
        const auto& s = d.sections[i];
        o += (i ? ", " : "") + std::string ("{\"id\": ") + esc (s.id) + ", \"name\": " + esc (s.name) + ", \"visual\": " + esc (s.visual)
           + ", \"tab\": " + esc (s.tab) + ", \"params\": [";
        bool first = true;
        for (const auto& id : s.params)
        {
            const int k = d.indexOf (id);
            if (k < 0) continue;
            o += (first ? "" : ", ") + std::to_string (k);
            first = false;
        }
        o += "]}";
    }
    o += "], \"presets\": [\"Init\"";
    for (const auto& pr : d.presets) o += ", " + esc (pr.name);
    o += "], \"padNames\": [";
    for (size_t i = 0; i < d.padNames.size(); ++i) o += (i ? ", " : "") + esc (d.padNames[i]);
    o += "], \"padKinds\": []}";
    return o;
}

inline std::string layoutJson (const ModuleDesc& d, const std::vector<std::string>& names)
{
    auto esc = [] (const std::string& s)
    {
        std::string o;
        for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
        return o;
    };
    struct Page { std::string name; std::vector<std::string> controls; }; // "" = case vide
    std::vector<Page> pages;
    std::vector<bool> placed (d.params.size(), false);
    auto widgetFor = [&] (int i)
    {
        const auto& p = d.params[(size_t) i];
        if (! p.choices.empty())
        {
            bool numeric = true;
            for (const auto& c : p.choices) if (c.empty() || c.find_first_not_of ("0123456789.-+/ ") != std::string::npos) numeric = false;
            if (numeric) return std::string ("knob");
            return std::string (p.choices.size() <= 4 ? "switch" : "knob\", \"overlay\": \"menu");
        }
        return std::string ("knob");
    };
    auto control = [&] (int i)
    {
        return std::string ("{ \"param\": \"") + esc (names[(size_t) i]) + "\", \"widget\": \"" + widgetFor (i) + "\" }";
    };
    auto addRows = [&] (const std::string& title, const std::vector<int>& idx)
    {
        // découpe en rangées de 4
        std::vector<std::vector<int>> rows;
        for (size_t k = 0; k < idx.size(); k += 4)
            rows.emplace_back (idx.begin() + (long) k, idx.begin() + (long) std::min (idx.size(), k + 4));
        for (auto& r : rows)
        {
            if (pages.empty() || pages.back().controls.size() >= 16)
            {
                const bool cont = ! pages.empty() && pages.back().name == pageName (title);
                pages.push_back ({ cont ? cut (abbrev (title) + " suite", 8) : pageName (title), {} });
            }
            auto& pg = pages.back();
            for (int i : r) pg.controls.push_back (control (i));
            while (pg.controls.size() % 4) pg.controls.push_back ("");
        }
    };
    // Préfixe d'un réglage (« Couche 1 » pour « Couche 1 : passe-bas »), vide sinon.
    auto prefixOf = [&] (size_t i)
    {
        const auto n = fold (d.params[i].name);
        const auto c = n.find (" : ");
        return c == std::string::npos ? std::string() : n.substr (0, c);
    };
    // Une section commence une nouvelle page si elle ne tient pas dans la page en cours ; les
    // réglages hors section qui portent son nom en préfixe la suivent (réglages cachés derrière
    // un panneau dédié dans GOLEM : filtres, égaliseur, distorsion de chaque couche…).
    auto place = [&] (const std::string& name, std::vector<int> idx)
    {
        for (size_t i = 0; i < d.params.size(); ++i)
            if (! placed[i] && ! name.empty() && prefixOf (i) == name) { idx.push_back ((int) i); placed[i] = true; }
        if (idx.empty()) return;
        const size_t rowsNeeded = std::min<size_t> (4, (idx.size() + 3) / 4);
        if (pages.empty() || pages.back().controls.size() + rowsNeeded * 4 > 16)
            pages.push_back ({ pageName (name), {} });
        addRows (name, idx);
    };
    for (const auto& s : d.sections)
    {
        std::vector<int> idx;
        for (const auto& id : s.params)
        {
            const int i = d.indexOf (id);
            if (i >= 0 && ! placed[(size_t) i]) { idx.push_back (i); placed[(size_t) i] = true; }
        }
        place (fold (s.name), idx);
    }
    // Restes : groupés par préfixe, puis page « Plus »
    for (size_t i = 0; i < d.params.size(); ++i)
        if (! placed[i] && ! prefixOf (i).empty()) place (prefixOf (i), {});
    {
        std::vector<int> rest;
        for (size_t i = 0; i < d.params.size(); ++i) if (! placed[i]) { rest.push_back ((int) i); placed[i] = true; }
        if (! rest.empty()) place ("Plus", rest);
    }
    // Volume du greffon : sur la dernière page s'il reste une case, sinon sur une page « Sortie ».
    const size_t maxPages = 32;
    if (pages.size() > maxPages) pages.resize (maxPages); // au-delà : réglables par automation seulement
    const std::string vol = "{ \"param\": \"Volume\", \"widget\": \"fader\" }";
    if (! pages.empty() && pages.back().controls.size() < 16) pages.back().controls.push_back (vol);
    else if (pages.size() < maxPages) pages.push_back ({ "Sortie", { vol } });
    std::string json = "{\n  \"qlinkGroups\": \"rows\",\n  \"pages\": [\n";
    for (size_t k = 0; k < pages.size(); ++k)
    {
        auto& pg = pages[k];
        json += "    { \"name\": \"" + esc (pg.name) + "\", \"controls\": [";
        for (size_t c = 0; c < pg.controls.size(); ++c)
        {
            json += (c ? ", " : " ");
            json += pg.controls[c].empty() ? std::string ("\"empty\"") : pg.controls[c];
        }
        json += " ] }";
        json += (k + 1 < pages.size()) ? ",\n" : "\n";
    }
    json += "  ]\n}\n";
    return json;
}


//==============================================================================
/** Réglages en plus de ceux de GOLEM, propres à la MPC (fichiers, phrases, piste au clavier). */
struct Extras
{
    // Fichiers lus dans Content/<folder>/ du greffon : WAV (échantillons) ou .fwsc (firmwares du FM Player).
    std::string folder;
    std::vector<std::string> exts;
    int slots = 0;                 // emplacements (un réglage « Sample N » chacun)
    std::string label = "Sample";
    bool firmware = false;
    int banks = 0;                 // Pieuvre 8 : l'emplacement k est copié dans chaque banque (b × 16 + k)
    // Pieuvre 8 : notes 36 à 43 → pistes T1 à T8, les autres jouent la piste choisie (chromatique)
    bool laneNotes = false;
    int lanes = 0;
    // FM Player : toutes les notes (16 à 127) jouent la piste choisie, comme un clavier
    bool allNotes = false;
    // Vocodeurs : phrases dites par la voix interne (texte GOLEM ; « @en » à la fin : anglais)
    std::vector<std::pair<std::string, std::string>> phrases;
    // Glutamake : position de la bille recopiée dans deux réglages en lecture seule (« Bille X / Y »),
    // que l'écran tactile montre sur le pavé de la physique
    bool ballOut = false;
};

constexpr int maxFiles = 255;   // 26 nov. 2026 : 255 fichiers par dossier (valeurs 1 à 255, 0 = aucun), pas à pas sur la machine
constexpr int maxSlots = 16;
constexpr double maxSeconds = 90.0; // échantillon coupé au-delà (mémoire de la machine)

inline std::string lowerCopy (std::string s)
{
    for (auto& c : s) c = (char) std::tolower ((unsigned char) c);
    return s;
}
inline std::string lowerExt (const std::string& n)
{
    const auto dot = n.rfind ('.');
    std::string e = dot == std::string::npos ? std::string() : n.substr (dot);
    for (auto& c : e) c = (char) std::tolower ((unsigned char) c);
    return e;
}

/** Fichiers du dossier (triés sans tenir compte de la casse). Hors du fil audio. */
inline std::vector<std::string> listFiles (const std::string& dir, const std::vector<std::string>& exts)
{
    std::vector<std::string> out;
    if (DIR* d = opendir (dir.c_str()))
    {
        while (auto* e = readdir (d))
        {
            std::string n = e->d_name;
            if (n.empty() || n[0] == '.') continue;
            const auto x = lowerExt (n);
            if (std::find (exts.begin(), exts.end(), x) != exts.end()) out.push_back (n);
        }
        closedir (d);
    }
    std::sort (out.begin(), out.end(), [] (const std::string& a, const std::string& b)
    {
        std::string la = a, lb = b;
        for (auto& c : la) c = (char) std::tolower ((unsigned char) c);
        for (auto& c : lb) c = (char) std::tolower ((unsigned char) c);
        return la < lb;
    });
    if ((int) out.size() > maxFiles) out.resize ((size_t) maxFiles);
    return out;
}

/** Dossiers de la carte, de la clé et du disque interne où il y a des fichiers à charger.
    Dossier 0 : Content/Samples du greffon. Les autres : trouvés en parcourant les disques montés à côté
    de celui du greffon (« /media/<disque> » sur la machine), hors du fil audio, dans un fil à part. */
struct SampleFolder { std::string path, label; std::vector<std::string> files; };
struct SampleLibrary { std::vector<SampleFolder> folders; };
constexpr int maxFolders = 512; // 26 nov. 2026 : 512 dossiers, parcourus pas à pas (liste de l'habillage)

inline bool isDirectory (const std::string& p)
{
    struct stat st {};
    return ::stat (p.c_str(), &st) == 0 && S_ISDIR (st.st_mode);
}

inline std::string parentOf (const std::string& p)
{
    const auto s = p.find_last_of ('/');
    return s == std::string::npos || s == 0 ? std::string ("/") : p.substr (0, s);
}

inline std::string baseOf (const std::string& p)
{
    const auto s = p.find_last_of ('/');
    return s == std::string::npos ? p : p.substr (s + 1);
}

/** Libellé court d'un dossier : chemin depuis le disque, la fin gardée si c'est long. */
/** Nom montré d'un dossier : nom du disque (« Interne » pour le disque de l'utilisateur de la Force / MPC),
    puis le chemin dans ce disque ; les noms trop longs gardent leur fin. */
inline std::string folderLabel (const std::string& path, const std::string& root, const std::string& rootName)
{
    std::string l = path.compare (0, root.size(), root) == 0 ? path.substr (root.size()) : path;
    while (! l.empty() && l[0] == '/') l.erase (0, 1);
    l = fold (l);
    const auto head = fold (rootName);
    if (l.empty()) return head;
    if (head.size() + 1 + l.size() <= 34) return head + "/" + l;
    const size_t keep = head.size() + 4 < 30 ? 30 - head.size() - 4 : 12;
    return head + "/.../" + l.substr (l.size() - std::min (keep, l.size())); // le disque et la fin du chemin
}

struct SampleRoot { std::string path, name; };

/** Disques à parcourir (26 nov. 2026, après essai sur la Force : le disque interne n'était pas trouvé).
    D'abord le disque du greffon (le dossier qui contient « VST2 Plugins »), puis le disque de l'utilisateur de
    la Force / MPC (/media/az01-internal-sd, /media/acvs-content ou /storage selon les versions), puis tous
    les autres disques montés sous /media, /mnt et /run/media (carte SD, clé USB, SSD). La partition système
    (az01-internal, qui sur certaines machines sert aussi de disque interne) vient en dernier, sans ses dossiers
    « system » et « Settings » ; un même disque monté deux fois n'est parcouru qu'une fois. */
inline std::vector<SampleRoot> sampleRoots()
{
    std::vector<SampleRoot> roots;
    std::vector<std::pair<unsigned long long, unsigned long long>> ids;
    auto add = [&] (const std::string& p, const std::string& name)
    {
        struct stat st {};
        if (p.empty() || p == "/" || stat (p.c_str(), &st) != 0 || ! S_ISDIR (st.st_mode)) return;
        const std::pair<unsigned long long, unsigned long long> id { (unsigned long long) st.st_dev, (unsigned long long) st.st_ino };
        if (std::find (ids.begin(), ids.end(), id) != ids.end()) return;
        ids.push_back (id);
        roots.push_back ({ p, name });
    };
    auto nameOf = [] (const std::string& p)
    {
        const auto b = baseOf (p);
        if (b == "az01-internal-sd" || b == "acvs-content" || b == "storage" || b == "sdcard") return std::string ("Interne");
        return b;
    };
    auto children = [] (const std::string& dir)
    {
        std::vector<std::string> out;
        if (DIR* d = opendir (dir.c_str()))
        {
            while (auto* e = readdir (d))
            {
                const std::string n = e->d_name;
                if (n.empty() || n[0] == '.' || n == "az01-internal" || n == "secure-media") continue;
                const std::string p = dir + "/" + n;
                if (isDirectory (p)) out.push_back (p);
            }
            closedir (d);
        }
        std::sort (out.begin(), out.end());
        return out;
    };
    const std::string plug = mpc::pluginFolder();
    const std::string drive = parentOf (parentOf (plug));
    add (drive, nameOf (drive));
    for (const char* p : { "/media/az01-internal-sd", "/media/acvs-content", "/storage", "/sdcard" }) add (p, "Interne");
    for (const auto& p : children ("/media")) add (p, nameOf (p));
    for (const auto& p : children ("/mnt")) add (p, nameOf (p));
    for (const auto& u : children ("/run/media")) for (const auto& p : children (u)) add (p, nameOf (p));
    add ("/media/az01-internal", "Interne"); // selon les machines, partition des réglages ou disque de l'utilisateur : en dernier
    for (const auto& p : children (parentOf (drive))) add (p, nameOf (p)); // voisins du disque du greffon (ailleurs qu'une Force)
    return roots;
}

/** Parcours d'un disque, le moins profond d'abord (un dossier rangé à la racine passe avant les milliers de
    dossiers d'une extension) : profondeur 7, 6000 dossiers vus, keep retenus au plus, dans l'ordre du
    parcours ; s'arrête dès que run passe à faux. */
inline void scanFolders (const SampleRoot& root, const std::vector<std::string>& exts, const std::string& skip, int keep,
                         std::vector<SampleFolder>& out, const std::atomic<bool>& run)
{
    std::vector<std::pair<std::string, int>> queue { { root.path, 0 } };
    std::vector<SampleFolder> got;
    int seen = 0;
    for (size_t qi = 0; qi < queue.size() && run.load() && (int) got.size() < keep && seen < 6000; ++qi)
    {
        const auto dir = queue[qi].first;
        const int depth = queue[qi].second;
        ++seen;
        if (dir == skip) continue;
        std::vector<std::string> subs;
        DIR* d = opendir (dir.c_str());
        if (d == nullptr) continue;
        bool hasFile = false;
        while (auto* e = readdir (d))
        {
            const std::string n = e->d_name;
            if (n.empty() || n[0] == '.' || n == "System Volume Information" || n == "$RECYCLE.BIN" || n == "lost+found") continue;
            const std::string p = dir + "/" + n;
            const auto x = lowerExt (n);
            if (std::find (exts.begin(), exts.end(), x) != exts.end()) hasFile = true;
            else if (depth == 0 && (n == "system" || n == "Settings" || n == "Updates")) continue;
            else if (depth < 7 && (e->d_type == DT_DIR || ((e->d_type == DT_UNKNOWN || e->d_type == DT_LNK) && isDirectory (p)))) subs.push_back (p);
        }
        closedir (d);
        if (hasFile) got.push_back ({ dir, folderLabel (dir, root.path, root.name), listFiles (dir, exts) });
        std::sort (subs.begin(), subs.end());
        for (auto& p : subs) queue.push_back ({ std::move (p), depth + 1 });
    }
    for (auto& g : got) out.push_back (std::move (g));
}

/** WAV (PCM 8/16/24/32 bits, flottant 32/64 bits, mono ou stéréo ; au-delà : deux premières voies). */
inline bool loadWav (const std::vector<std::uint8_t>& b, grv::engine::SampleBuffer& s)
{
    auto u16 = [&] (size_t p) { return (unsigned) b[p] | ((unsigned) b[p + 1] << 8); };
    auto u32 = [&] (size_t p) { return (std::uint32_t) b[p] | ((std::uint32_t) b[p + 1] << 8) | ((std::uint32_t) b[p + 2] << 16) | ((std::uint32_t) b[p + 3] << 24); };
    if (b.size() < 44 || std::memcmp (b.data(), "RIFF", 4) != 0 || std::memcmp (b.data() + 8, "WAVE", 4) != 0) return false;
    unsigned fmt = 0, ch = 0, bits = 0; std::uint32_t rate = 0;
    size_t dataPos = 0, dataLen = 0;
    for (size_t p = 12; p + 8 <= b.size();)
    {
        const std::uint32_t len = u32 (p + 4);
        if (std::memcmp (b.data() + p, "fmt ", 4) == 0 && p + 8 + 16 <= b.size())
        {
            fmt = u16 (p + 8); ch = u16 (p + 10); rate = u32 (p + 12); bits = u16 (p + 22);
            if (fmt == 0xFFFE && len >= 26 && p + 8 + 26 <= b.size()) fmt = u16 (p + 8 + 24); // WAVE_FORMAT_EXTENSIBLE
        }
        else if (std::memcmp (b.data() + p, "data", 4) == 0) { dataPos = p + 8; dataLen = std::min<size_t> (len, b.size() - dataPos); break; }
        p += 8 + len + (len & 1);
    }
    if (dataPos == 0 || ch == 0 || rate < 4000 || rate > 384000 || (fmt != 1 && fmt != 3)) return false;
    const unsigned bytes = bits / 8;
    if (bytes == 0 || (fmt == 3 && bits != 32 && bits != 64) || (fmt == 1 && bits > 32)) return false;
    size_t frames = dataLen / (bytes * ch);
    frames = std::min (frames, (size_t) (maxSeconds * rate));
    s.data.assign (frames, 0.0f);
    if (ch > 1) s.right.assign (frames, 0.0f); else s.right.clear();
    auto sample = [&] (size_t p) -> float
    {
        if (fmt == 3)
        {
            if (bits == 32) { float f; std::memcpy (&f, b.data() + p, 4); return f; }
            double d; std::memcpy (&d, b.data() + p, 8); return (float) d;
        }
        switch (bits)
        {
            case 8:  return ((int) b[p] - 128) / 128.0f;
            case 16: return (float) (std::int16_t) u16 (p) / 32768.0f;
            case 24: { std::int32_t v = (std::int32_t) ((std::uint32_t) b[p] << 8 | (std::uint32_t) b[p + 1] << 16 | (std::uint32_t) b[p + 2] << 24); return (float) (v >> 8) / 8388608.0f; }
            default: return (float) (std::int32_t) u32 (p) / 2147483648.0f;
        }
    };
    for (size_t i = 0; i < frames; ++i)
    {
        const size_t p = dataPos + i * bytes * ch;
        const float l = sample (p);
        if (! std::isfinite (l)) continue;
        s.data[i] = std::clamp (l, -4.0f, 4.0f);
        if (ch > 1) { const float r = sample (p + bytes); s.right[i] = std::isfinite (r) ? std::clamp (r, -4.0f, 4.0f) : 0.0f; }
    }
    s.sampleRate = (double) rate;
    return ! s.data.empty();
}

/** Fichier → SampleBuffer, dans le fil de chargement. Défini par le greffon (firmware ou WAV). */
using Loader = grv::engine::SampleBuffer* (*) (const std::string& path, const std::string& name, bool firmware);

inline grv::engine::SampleBuffer* defaultLoader (const std::string& path, const std::string& name, bool firmware)
{
    std::vector<std::uint8_t> bytes;
    if (! mpc::readFile (path, bytes)) return nullptr;
    if (firmware) return nullptr; // le FM Player fournit son propre chargeur
    auto* s = new grv::engine::SampleBuffer();
    if (! loadWav (bytes, *s)) { delete s; return nullptr; }
    s->name = name;
    s->onsets = grv::synth::computeOnsets (*s, 64);
    return s;
}

/** 26 nov. 2026 : forme du sample à l'écran de la machine, en colonnes (voir GolemBase::waveTick). */
inline constexpr int kWaveCols = 96, kWaveLevels = 16;

/** Chargement des fichiers hors du fil audio. Le fil audio ne fait que lire / écrire des atomiques. */
class FileLoader
{
public:
    // crêtes du fichier de chaque emplacement, 96 colonnes de 0 à 15 (fil de chargement seulement)
    std::array<std::array<std::uint8_t, kWaveCols>, 16> peaks {};
    std::array<int, 16> peakGen {};
    // découpes de chaque emplacement : 16 parts égales, sur les attaques (comme « Découper » de GOLEM)
    std::array<std::vector<std::pair<float, float>>, 16> slicesEq {}, slicesOn {};
    std::vector<std::string> exts;
    bool firmware = false;
    Loader loader = &defaultLoader;
    int slots = 0;

    // Demande d'un emplacement (fil audio → fil de chargement) : dossier × 256 + fichier, -1 rien,
    // HOLD : garder ce qui est chargé, -(10 + n) : fichier retrouvé par son chemin (état d'un projet)
    static constexpr int HOLD = -2;
    static int encode (int folder, int file) noexcept { return folder * 256 + file; }

    ~FileLoader()
    {
        stop();
        for (auto* l : libs) delete l;
        for (auto* h : husks) delete h;
    }

    /** Dossier 0 (Content/Samples du greffon), tout de suite ; les autres viennent du parcours. */
    void init (const std::string& contentDir)
    {
        auto* l = new SampleLibrary();
        l->folders.push_back ({ contentDir, "Greffon : Samples", listFiles (contentDir, exts) });
        libs.push_back (l);
        lib.store (l, std::memory_order_release);
        ownDir = contentDir;
    }
    const SampleLibrary* library() const noexcept { return lib.load (std::memory_order_acquire); }
    bool scanning() const noexcept { return scanBusy.load(); }

    void start()
    {
        if (running.load()) return;
        for (int k = 0; k < maxSlots; ++k) { want[(size_t) k].store (-1); have[(size_t) k] = -1; }
        running.store (true);
        worker = std::thread ([this] { loop(); });
        if (! scanDone)
        {
            scanBusy.store (true);
            scanner = std::thread ([this] { scan(); });
        }
    }
    void stop()
    {
        running.store (false);
        if (scanner.joinable()) scanner.join();
        if (worker.joinable()) worker.join();
        for (auto& r : ready) { auto* p = r.exchange (nullptr); if (p != nullptr && p != empty()) bury (p); }
        for (auto& t : trash) { auto* p = t.exchange (nullptr); if (p != nullptr && p != empty()) bury (p); }
        for (auto& a : applied) { if (a != nullptr && a != empty()) bury (a); a = nullptr; }
    }
    /** 27 nov. 2026 (Kick Forge sur la Force : en changeant de sample, l'ancien revenait) : les instruments
        reconnaissent un sample à son adresse (caches de rendu) ; si un nouveau sample prenait l'adresse d'un
        ancien libéré, le cache rendait l'ancien son. On libère donc le son (les données), mais la petite
        fiche du sample reste réservée jusqu'à la fermeture du greffon : jamais deux samples à la même adresse. */
    void bury (grv::engine::SampleBuffer* p)
    {
        std::vector<float>().swap (p->data);
        std::vector<float>().swap (p->right);
        std::vector<float>().swap (p->onsets);
        p->original.reset();
        p->runtime.reset();
        std::string().swap (p->name);
        husks.push_back (p);
    }
    static grv::engine::SampleBuffer* empty() { static grv::engine::SampleBuffer e; return &e; }

    /** Fil audio : fichier voulu pour chaque emplacement ; renvoie le nouveau tampon à appliquer (ou nullptr). */
    void request (int slot, int token) noexcept { want[(size_t) slot].store (token, std::memory_order_relaxed); }

    /** Fil de l'interface (état d'un projet) : chemin à charger pour un emplacement, absent de la bibliothèque. */
    int setOverride (int slot, const std::string& path)
    {
        std::lock_guard<std::mutex> g (pathLock);
        override_[(size_t) slot] = path;
        return -(10 + ++overrideGen[(size_t) slot]);
    }
    /** Chemin du fichier chargé dans l'emplacement ("" : rien). */
    std::string loadedPath (int slot) const
    {
        std::lock_guard<std::mutex> g (pathLock);
        return loaded[(size_t) slot];
    }
    grv::engine::SampleBuffer* take (int slot) noexcept { return ready[(size_t) slot].exchange (nullptr, std::memory_order_acq_rel); }
    /** Fil audio : l'ancien tampon part à la corbeille (libéré par le fil de chargement). */
    void retire (int slot, grv::engine::SampleBuffer* now) noexcept
    {
        auto* old = applied[(size_t) slot];
        applied[(size_t) slot] = now;
        if (old == nullptr || old == empty()) return;
        for (auto& t : trash)
        {
            grv::engine::SampleBuffer* expected = nullptr;
            if (t.compare_exchange_strong (expected, old)) return;
        }
        // corbeille pleine (très rare) : gardé en mémoire plutôt que libéré dans le fil audio
    }

private:
public:
    std::function<void()> onTick; // 26 nov. 2026 : navigateur de samples, servi par ce fil (lecture des dossiers)
private:
    void loop()
    {
        while (running.load())
        {
            if (onTick) onTick();
            for (auto& t : trash) { auto* p = t.exchange (nullptr); if (p != nullptr && p != empty()) bury (p); }
            for (int k = 0; k < slots; ++k)
            {
                const int w = want[(size_t) k].load (std::memory_order_relaxed);
                if (w == HOLD || w == have[(size_t) k] || ready[(size_t) k].load() != nullptr) continue;
                grv::engine::SampleBuffer* sb = empty();
                std::string path, name;
                if (w >= 0)
                {
                    const auto* l = library();
                    const int f = w / 256, i = w % 256;
                    if (l != nullptr && f < (int) l->folders.size() && i < (int) l->folders[(size_t) f].files.size())
                    {
                        name = l->folders[(size_t) f].files[(size_t) i];
                        path = l->folders[(size_t) f].path + "/" + name;
                    }
                }
                else if (w <= -10)
                {
                    std::lock_guard<std::mutex> g (pathLock);
                    path = override_[(size_t) k];
                    name = baseOf (path);
                }
                if (! path.empty())
                {
                    auto* got = loader (path, name, firmware);
                    if (got != nullptr) sb = got;
                }
                {
                    std::lock_guard<std::mutex> g (pathLock);
                    loaded[(size_t) k] = sb == empty() ? std::string() : path;
                }
                have[(size_t) k] = w;
                computePeaks (k, sb == empty() ? nullptr : sb);
                if (std::getenv ("GOLEM_MPC_DEBUG")) std::fprintf (stderr, "[golem-mpc] emplacement %d : fichier %d -> %s\n", k, w, sb == empty() ? "vide" : "charge");
                ready[(size_t) k].store (sb, std::memory_order_release);
            }
            std::this_thread::sleep_for (std::chrono::milliseconds (15));
        }
    }

    /** Parcours des disques (fil à part) ; la bibliothèque complète remplace la première d'un coup. */
    void scan()
    {
        const auto roots = sampleRoots();
        const int room = maxFolders - 1;
        std::vector<std::vector<SampleFolder>> per (roots.size());
        for (size_t r = 0; r < roots.size() && running.load(); ++r)
            scanFolders (roots[r], exts, ownDir, room, per[r], running);
        // partage équitable : chaque disque à tour de rôle donne son dossier suivant (les moins profonds
        // d'abord), puis chaque disque est rangé par ordre alphabétique, le disque du greffon en tête
        std::vector<size_t> take (roots.size(), 0);
        for (int total = 0; total < room;)
        {
            bool any = false;
            for (size_t r = 0; r < roots.size() && total < room; ++r)
                if (take[r] < per[r].size()) { ++take[r]; ++total; any = true; }
            if (! any) break;
        }
        // les dossiers de l'utilisateur de tous les disques d'abord, les extensions Akai (souvent des
        // centaines de dossiers) à la fin
        std::vector<SampleFolder> found, late;
        for (size_t r = 0; r < roots.size(); ++r)
        {
            per[r].resize (take[r]);
            std::sort (per[r].begin(), per[r].end(), [] (const SampleFolder& a, const SampleFolder& b) { return a.path < b.path; });
            for (auto& g : per[r]) (g.path.find ("/Expansions/") != std::string::npos ? late : found).push_back (std::move (g));
        }
        for (auto& g : late) found.push_back (std::move (g));
        if (running.load())
        {
            auto* l = new SampleLibrary (*library());
            if (! l->folders.empty())
                l->folders[0].label = found.empty() ? std::string ("Greffon (aucun autre dossier)")
                                                    : "Greffon (+ " + std::to_string (found.size()) + " dossiers)";
            for (auto& f : found)
                if ((int) l->folders.size() < maxFolders) l->folders.push_back (std::move (f));
            libs.push_back (l);
            lib.store (l, std::memory_order_release);
            scanDone = true;
        }
        scanBusy.store (false);
    }

    std::vector<SampleLibrary*> libs;              // toutes les versions publiées (libérées à la fin)
    std::atomic<const SampleLibrary*> lib { nullptr };
    std::string ownDir;
    std::thread scanner;
    std::atomic<bool> scanBusy { false };
    void computePeaks (int k, const grv::engine::SampleBuffer* sb)
    {
        if (k < 0 || k >= (int) peaks.size()) return;
        auto& pk = peaks[(size_t) k];
        pk.fill (0);
        slicesEq[(size_t) k].clear();
        slicesOn[(size_t) k].clear();
        if (sb != nullptr && ! sb->data.empty())
        {
            slicesEq[(size_t) k] = grv::synth::computeSlices (*sb, false, 16);
            slicesOn[(size_t) k] = grv::synth::computeSlices (*sb, true, 16);
        }
        if (sb != nullptr && ! sb->data.empty())
        {
            const size_t n = sb->data.size();
            std::array<float, kWaveCols> m {};
            float top = 1.0e-9f;
            for (int c = 0; c < kWaveCols; ++c)
            {
                const size_t lo = n * (size_t) c / kWaveCols, hi = std::max (lo + 1, n * (size_t) (c + 1) / kWaveCols);
                const size_t step = std::max<size_t> (1, (hi - lo) / 600);
                float v = 0.0f;
                for (size_t i = lo; i < hi && i < n; i += step)
                {
                    v = std::max (v, std::fabs (sb->data[i]));
                    if (i < sb->right.size()) v = std::max (v, std::fabs (sb->right[i]));
                }
                m[(size_t) c] = v;
                top = std::max (top, v);
            }
            for (int c = 0; c < kWaveCols; ++c)
                pk[(size_t) c] = (std::uint8_t) std::lround (std::min (1.0f, m[(size_t) c] / top) * (kWaveLevels - 1));
        }
        ++peakGen[(size_t) k];
    }
    std::vector<grv::engine::SampleBuffer*> husks;   // fiches des samples libérés (voir bury)
    bool scanDone = false;
    mutable std::mutex pathLock;
    std::array<std::string, maxSlots> override_ {}, loaded {};
    std::array<int, maxSlots> overrideGen {};
    std::thread worker;
    std::atomic<bool> running { false };
    std::array<std::atomic<int>, maxSlots> want {};
    std::array<int, maxSlots> have {};
    std::array<std::atomic<grv::engine::SampleBuffer*>, maxSlots> ready {};
    std::array<grv::engine::SampleBuffer*, maxSlots> applied {};
    std::array<std::atomic<grv::engine::SampleBuffer*>, 32> trash {};
};

//==============================================================================
/** Partie commune : réglages, programmes, volume. */
class GolemBase : public mpc::Plugin
{
public:
    GolemBase (const mpc::PluginInfo& info, const ModuleDesc& d) : mpc::Plugin (info), desc (d)
    {
        names = mpcNames (d);
        addParam (mpc::floatParam ("mpcvol", "Volume", -60.0f, 6.0f, 0.0f, "dB", 1, 1.0f));
        // « Arpège en marche » : arrêté par défaut sur la machine (on joue avec son séquenceur) ; les sons
        // d'usine qui ont un arpège le mettent en marche (plus bas)
        const int arpOnIdx = d.indexOf ("arp_on"), arpModeIdx = d.indexOf ("arp_mode");
        // Kick Killer : « Note jouée » sur la machine, dans Init et dans les sons d'usine (le séquenceur
        // interne à 8 pas faisait des hauteurs au hasard ; c'est le séquenceur de la MPC qui joue)
        const int dfamSeqIdx = d.type == "dfam" ? d.indexOf ("seqmode") : -1;
        // Kick Forge : comme dans GOLEM, « Gain automatique » sur Oui et « Volume » (page KICK) à 0 dB dans Init
        // (25 nov. 2026 : sans le gain auto, la distorsion des couches baissait le niveau au lieu de s'entendre)
        // 27 nov. 2026 : Pieuvre 8 sur la machine : zones de notes calées sur les pads de la Force (Do1 = note 36 =
        // premier pad, comme les autres samplers) : T1 de Do1 à Si1, T2 de Do2 à Si2… (dans GOLEM, T1 part de Do0)
        auto mpcDef = [&] (size_t i) -> float
        {
            const auto& id = d.params[i].id;
            if (d.type == "octa" && id.size() >= 6 && id[0] == 't' && id[2] == '_' && (id.compare (3, 3, "zlo") == 0 || id.compare (3, 3, "zhi") == 0))
                return (float) std::min (127, 36 + 12 * (id[1] - '1') + (id[4] == 'h' ? 11 : 0));
            return (int) i == arpOnIdx ? 0.0f : ((int) i == dfamSeqIdx ? 1.0f : d.params[i].def);
        };
        for (size_t i = 0; i < d.params.size(); ++i)
        {
            auto q = mpcParam (d.params[i], names[i]);
            if ((int) i == arpOnIdx) q.def = 0.0f;
            if ((int) i == dfamSeqIdx) q.def = 1.0f;
            if (mpcDef (i) != d.params[i].def && (int) i != arpOnIdx && (int) i != dfamSeqIdx) q.def = mpcDef (i);   // zones de Pieuvre
            addParam (q);
        }
        plain.assign (d.params.size(), 0.0f);
        lastNorm.assign (d.params.size(), -1.0f);
        for (size_t i = 0; i < d.params.size(); ++i) plain[i] = mpcDef (i);

        // programme 0 : réglages par défaut, puis les sons d'usine
        std::vector<float> defs (d.params.size() + 1, 0.0f);
        defs[0] = 0.0f;
        for (size_t i = 0; i < d.params.size(); ++i) defs[i + 1] = param ((int) i + 1).toPlain (param ((int) i + 1).toNormalised (param ((int) i + 1).def));
        addProgram ("Init", defs);
        for (const auto& pr : d.presets)
        {
            auto v = defs;
            bool arpSet = false;
            for (const auto& kv : pr.values)
            {
                const int i = d.indexOf (kv.first);
                if (i >= 0) v[(size_t) i + 1] = kv.second;
                if (i >= 0 && i == arpOnIdx) arpSet = true;
            }
            if (arpOnIdx >= 0 && arpModeIdx >= 0 && ! arpSet && v[(size_t) arpModeIdx + 1] >= 0.5f) v[(size_t) arpOnIdx + 1] = 1.0f;
            if (dfamSeqIdx >= 0) v[(size_t) dfamSeqIdx + 1] = 1.0f;
            addProgram (cut (fold (pr.name), 24).c_str(), v);
        }
    }

    std::string layout() const { return layoutJson (desc, names); }
    std::string catalog() const { return catalogJson (desc); }

    bool formatValue (int index, float plainValue, std::string& out) override
    {
        if (index <= 0 || index > (int) desc.params.size()) return false;
        return formatGolem (desc.params[(size_t) index - 1], plainValue, out);
    }

protected:
    /** Recopie les valeurs de l'hôte (unités réelles) ; seuls les réglages changés sont recalculés. */
    void pullParams() noexcept
    {
        for (size_t i = 0; i < plain.size(); ++i)
        {
            const float n = getNormalised ((int) i + 1);
            if (n != lastNorm[i]) { lastNorm[i] = n; plain[i] = get ((int) i + 1); }
        }
        const float db = get (0);
        targetGain = db <= -59.9f ? 0.0f : std::pow (10.0f, db / 20.0f);
    }

    void applyGain (float* l, float* r, int n) noexcept
    {
        // rampe sur le bloc (pas de clic)
        const float g0 = gain, g1 = targetGain;
        if (g0 == g1 && g1 == 1.0f) return;
        const float step = (g1 - g0) / (float) std::max (1, n);
        float g = g0;
        for (int k = 0; k < n; ++k) { g += step; l[k] *= g; if (r) r[k] *= g; }
        gain = g1;
    }

    const ModuleDesc& desc;
    std::vector<std::string> names;
    std::vector<float> plain, lastNorm;
    float gain = 1.0f, targetGain = 1.0f;
};

//==============================================================================
template <class Module>
class InstrumentPlugin : public GolemBase
{
public:
    explicit InstrumentPlugin (const mpc::PluginInfo& info, std::unique_ptr<Module> m = std::make_unique<Module>(), Extras x = {}, Loader ld = nullptr)
        : GolemBase (info, localized (m->desc())), module (std::move (m)), ex (std::move (x))
    {
        const int base = (int) desc.params.size() + 1;
        for (auto& x : lastDir) x.store (-1);
        // arpège de piste (dans GOLEM, c'est le moteur qui le joue ; ici, le greffon)
        arpParam = desc.indexOf ("arp_mode");
        arpOnParam = desc.indexOf ("arp_on");
        arpStepsParam = arpParam >= 0 && arpParam + 6 < (int) desc.params.size() && desc.params[(size_t) arpParam + 6].id == "arp_steps" ? arpParam + 6 : -1;
        if (ex.slots > 0)
        {
            files.exts = ex.exts;
            files.init (mpc::contentPath (ex.folder));
            files.firmware = ex.firmware;
            files.slots = std::min (ex.slots, maxSlots);
            if (ld != nullptr) files.loader = ld;
            firstFile = base;
            for (int k = 0; k < files.slots; ++k)
            {
                const auto id = "mpc_file" + std::to_string (k + 1);
                const auto nm = files.slots == 1 ? ex.label : ex.label + " " + std::to_string (k + 1);
                addParam (mpc::intParam (id.c_str(), nm.c_str(), 0, maxFiles, 0));
            }
        }
        if (ex.lanes > 0)
        {
            laneParam = numParams();
            addParam (mpc::intParam ("mpc_lane", L ("Piste clavier").c_str(), 1, ex.lanes, 1));
            // 26 nov. 2026 (essai sur la Force : seule la piste 1 jouait) : par défaut, chaque pad joue sa
            // piste, quelle que soit la disposition des pads (note 36 = T1, 37 = T2 ... 43 = T8, puis on
            // recommence) ; « Comme GOLEM » : les notes vont au réglage « Clavier » de Pieuvre 8 (piste
            // choisie, en chromatique, ou chaque piste dans sa zone de notes).
            padLaneParam = numParams();
            addParam (mpc::choiceParam ("mpc_padlanes", L ("Pads").c_str(), { L ("1 pad = 1 piste"), L ("Clavier (ci-dessous)") }, 0));
        }
        if (! ex.phrases.empty())
        {
            phraseParam = numParams();
            std::vector<std::string> labels;
            for (const auto& ph : ex.phrases) { labels.push_back (cut (fold (ph.first), 20)); scripts.push_back (grv::engine::compileSpeech (ph.second)); }
            mpc::Param q;
            q.id = "mpc_phrase"; q.name = "Phrase"; q.kind = mpc::Param::Kind::Choice;
            q.choices = labels; q.min = 0; q.max = (float) labels.size() - 1; q.def = 0;
            addParam (q);
        }
        // Batteries : disposition des pads. La Force donne aux greffons tiers le clavier « mélodique »
        // (pas le mode batterie à 16 pads de ses propres pistes) : en gamme de Do, le carré 4x4 en bas
        // à gauche envoie 16 notes blanches à la suite, qu'on range alors sur les pads 1 à 16.
        if (desc.drumKit && ex.lanes == 0 && ! ex.allNotes)
        {
            padParam = numParams();
            mpc::Param q;
            q.id = "mpc_pads"; q.name = L ("Disposition pads"); q.kind = mpc::Param::Kind::Choice;
            q.choices = { L ("Notes 36-51"), L ("Do majeur"), L ("Do mineur") }; q.min = 0; q.max = 2; q.def = 0;
            addParam (q);
        }
        // Réglages ajoutés à la fin (identifiants permanents) : le dossier de chaque emplacement de sample
        // (n'importe quel dossier de la carte, de la clé ou du disque interne qui contient des fichiers)
        if (files.slots > 0)
        {
            firstDir = numParams();
            for (int k = 0; k < files.slots; ++k)
            {
                const auto id = "mpc_dir" + std::to_string (k + 1);
                const auto nm = files.slots == 1 ? L ("Dossier") : L ("Dossier") + " " + std::to_string (k + 1);
                addParam (mpc::intParam (id.c_str(), nm.c_str(), 0, maxFolders - 1, 0));
            }
        }
        // Réglages ajoutés à la fin (identifiants permanents) : la bille de Glutamake, écrite par le moteur
        if (ex.ballOut)
        {
            ballParam = numParams();
            auto bx = mpc::floatParam ("mpc_ballx", L ("Bille X").c_str(), -1.0f, 1.0f, 0.0f, "", 2, 1.0f);
            auto by = mpc::floatParam ("mpc_bally", L ("Bille Y").c_str(), -1.0f, 1.0f, 0.0f, "", 2, 1.0f);
            bx.automatable = false;
            by.automatable = false;
            addParam (bx);
            addParam (by);
        }
        // Batteries : « Muet » et « Solo » de chaque son, ajoutés à la fin (identifiants permanents). Les
        // greffons tiers n'ont pas le mode batterie de la machine : ses pads ne coupent pas les sons.
        // Un son coupé garde son niveau réglé : le greffon le joue à -48 dB (silence) tant qu'il l'est.
        if (desc.drumKit && ex.lanes == 0 && ! ex.allNotes)
        {
            const int np = std::min ((int) desc.padNames.size(), maxPads);
            int found = 0;
            for (int k = 0; k < np; ++k)
            {
                char id[16];
                std::snprintf (id, sizeof (id), "p%02d_level", k + 1);
                int li = desc.indexOf (id);
                if (li < 0) { std::snprintf (id, sizeof (id), "z%02d_level", k + 1); li = desc.indexOf (id); }
                padLevel[(size_t) k] = li;
                if (li >= 0) found = k + 1;
            }
            if (found > 0)
            {
                numMutePads = found;
                const std::vector<std::string> noYes { L ("Non"), L ("Oui") };
                firstMute = numParams();
                for (int k = 0; k < found; ++k)
                {
                    mpc::Param q;
                    q.id = std::string ("mpc_mute") + (k < 9 ? "0" : "") + std::to_string (k + 1);
                    q.name = L ("Muet") + " " + std::to_string (k + 1); q.kind = mpc::Param::Kind::Choice;
                    q.choices = noYes; q.min = 0; q.max = 1; q.def = 0;
                    addParam (q);
                }
                firstSolo = numParams();
                for (int k = 0; k < found; ++k)
                {
                    mpc::Param q;
                    q.id = std::string ("mpc_solo") + (k < 9 ? "0" : "") + std::to_string (k + 1);
                    q.name = L ("Solo") + " " + std::to_string (k + 1); q.kind = mpc::Param::Kind::Choice;
                    q.choices = noYes; q.min = 0; q.max = 1; q.def = 0;
                    addParam (q);
                }
            }
        }
        // 26 nov. 2026 : réglages ajoutés à la fin (identifiants permanents), en lecture seule : le nom du
        // dossier et celui du fichier de chaque emplacement, écrits par le greffon. L'écran tactile ne sait
        // faire avancer un réglage cran par cran qu'avec une liste fixe de libellés (ses numéros) : le nom
        // en clair est montré à côté, par ces deux réglages.
        if (files.slots > 0)
        {
            firstName = numParams();
            for (int k = 0; k < files.slots; ++k)
            {
                const auto num = files.slots == 1 ? std::string() : " " + std::to_string (k + 1);
                auto pd = mpc::intParam (("mpc_dname" + std::to_string (k + 1)).c_str(), (L ("Nom dossier") + num).c_str(), 0, maxFolders - 1, 0);
                auto pf = mpc::intParam (("mpc_fname" + std::to_string (k + 1)).c_str(), (L ("Nom fichier") + num).c_str(), 0, maxFiles, 0);
                pd.automatable = false;
                pf.automatable = false;
                addParam (pd);
                addParam (pf);
            }
            // navigateur de samples à l'écran (26 nov. 2026, après essai : une molette qui fait défiler des
            // numéros n'est pas un explorateur). Comme le navigateur de la Force : d'abord les disques
            // (Interne, carte SD, clé USB), toucher un disque ou un dossier l'ouvre (sous-dossiers puis
            // fichiers WAV), la ligne du haut remonte, toucher un fichier le charge dans l'emplacement
            // choisi. Boutons : page précédente / suivante. Ce sont des bascules : chaque appui change leur
            // valeur, le fil de chargement y répond (browse()).
            firstNav = numParams();
            {
                std::vector<std::string> slotNames;
                for (int k = 0; k < files.slots; ++k) slotNames.push_back (L (ex.label) + " " + std::to_string (k + 1));
                auto ps = mpc::choiceParam ("mpc_navslot", L ("Nav emplacement").c_str(), slotNames, 0);
                addParam (ps); // automatisable : la Force n'applique les touchers de l'écran et la molette qu'à ceux-là
            }
            for (const char* id : { "mpc_navprev", "mpc_navnext", "mpc_navup" })
            {
                const char* nm = id[7] == 'p' ? "Nav page precedente" : id[7] == 'n' ? "Nav page suivante" : "Nav retour";
                auto b = mpc::boolParam (id, L (nm).c_str(), false);
                addParam (b); // automatisable (sinon l'écran de la Force ne peut pas le changer : essai du 26 nov.)
            }
            for (int r = 0; r < navRows; ++r)
            {
                auto b = mpc::boolParam (("mpc_navrow" + std::to_string (r + 1)).c_str(), (L ("Nav ligne") + " " + std::to_string (r + 1)).c_str(), false);
                addParam (b); // automatisable, idem
            }
            for (int r = 0; r <= navRows; ++r) // 0 : la ligne du haut (où l'on est), puis les 10 lignes
            {
                auto t = mpc::intParam (("mpc_navtxt" + std::to_string (r)).c_str(), (L ("Nav texte") + " " + std::to_string (r)).c_str(), 0, 1023, 0);
                addParam (t); // automatisable : sinon la machine ignore l'avis « a bougé » et ne relit pas le texte
            }
            // 26 nov. 2026 : forme du sample (comme dans GOLEM) : 96 colonnes, chacune une image choisie par
            // la valeur de son réglage (hauteur de la crête, dans / hors de la lecture, boucle, repères).
            // Seulement pour les sampleurs qu'on sait dessiner (Chromasample, Cutsample, Poussière, Pieuvre).
            waveKind = desc.type == "keys" ? 1 : (desc.type == "sampler" || desc.type == "sp") ? 2 : desc.type == "octa" ? 3 : 0;
            if (waveKind > 0)
            {
                firstWave = numParams();
                for (int c = 0; c < kWaveCols; ++c)
                    addParam (mpc::intParam (("mpc_wave" + std::to_string (c + 1)).c_str(), ("Onde " + std::to_string (c + 1)).c_str(), 0, waveFrames - 1, 0));
                waveTxtParam = numParams();
                addParam (mpc::intParam ("mpc_wavetxt", "Onde texte", 0, 1023, 0));
                if (desc.type == "sampler")
                {
                    // Cutsample : « Découper ×16 » et « Sur les attaques » de GOLEM (un appui = les 16 zones
                    // prennent les découpes du sample 1)
                    firstCut = numParams();
                    addParam (mpc::boolParam ("mpc_cuteq", "Decouper en 16", false));
                    addParam (mpc::boolParam ("mpc_cuton", "Decouper sur attaques", false));
                }
                // réglages surveillés par piste (la forme montre la dernière piste réglée)
                std::vector<std::string> ids;
                if (waveKind == 1) { waveLanes = 1; ids = { "start", "end", "loop", "loopstart", "loopend" }; }
                else if (waveKind == 2) { waveLanes = 16; ids = { "start", "end" }; }
                else { waveLanes = 8; ids = { "slot", "start", "length", "loop", "slices" }; }
                waveStride = (int) ids.size();
                for (int l = 0; l < waveLanes; ++l)
                    for (const auto& id : ids)
                    {
                        char full[32];
                        if (waveKind == 1) std::snprintf (full, sizeof (full), "%s", id.c_str());
                        else if (waveKind == 2) std::snprintf (full, sizeof (full), "z%02d_%s", l + 1, id.c_str());
                        else std::snprintf (full, sizeof (full), "t%d_%s", l + 1, id.c_str());
                        const int i = desc.indexOf (full);
                        waveWatch.push_back (i >= 0 ? i + 1 : -1);
                    }
                waveLast.assign (waveWatch.size(), -1.0e9f);
                sliceCntIdx = desc.type == "sampler" ? desc.indexOf ("slicecnt") + 1 : desc.type == "sp" ? desc.indexOf ("slices") + 1 : 0;
                slicePageIdx = desc.type == "sampler" ? desc.indexOf ("slicepage") + 1 : 0;
                waveShown.fill (-1);
            }
        }
    }

    bool formatValue (int index, float plainValue, std::string& out) override
    {
        // 26 nov. 2026 : nom du dossier et du fichier choisis, montrés en clair à côté des sélecteurs (les
        // sélecteurs eux-mêmes avancent d'un cran à la fois et affichent leur numéro)
        if (firstNav > 0 && index >= firstNav + navTxt && index <= firstNav + navTxt + navRows)
        {
            out = navText (index - firstNav - navTxt);
            return true;
        }
        if (waveTxtParam > 0 && index == waveTxtParam)
        {
            std::lock_guard<std::mutex> g (navLock);
            out = waveTitle;
            return true;
        }
        if (firstWave > 0 && index >= firstWave && index < firstWave + kWaveCols) { out = " "; return true; }
        if (firstName > 0 && index >= firstName && index < firstName + 2 * files.slots)
        {
            const int k = (index - firstName) / 2;
            const int src = (index - firstName) % 2 == 0 ? firstDir + k : firstFile + k;
            if (src >= 0) return formatValue (src, get (src), out);
        }
        const auto* lib = files.library();
        if (firstDir > 0 && index >= firstDir && index < firstDir + files.slots && lib != nullptr)
        {
            const int d = (int) std::lround (plainValue);
            if (d < (int) lib->folders.size()) out = lib->folders[(size_t) d].label;
            else out = files.scanning() ? "(recherche...)" : "-";
            return true;
        }
        if (firstFile > 0 && index >= firstFile && index < firstFile + files.slots && lib != nullptr)
        {
            const int k = index - firstFile;
            const int f = (int) std::lround (plainValue);
            const int d = firstDir > 0 ? getInt (firstDir + k) : 0;
            if (hold[(size_t) k].load() && f == holdVal[(size_t) k].load())
            {
                const auto p = files.loadedPath (k);
                out = p.empty() ? std::string ("-") : "= " + cut (fold (baseOf (p)), 26);
            }
            else if (f <= 0) out = "-";
            else if (d < (int) lib->folders.size() && f - 1 < (int) lib->folders[(size_t) d].files.size())
                out = cut (fold (lib->folders[(size_t) d].files[(size_t) f - 1]), 28);
            else out = "(absent)";
            return true;
        }
        return GolemBase::formatValue (index, plainValue, out);
    }

    /** Fichiers retenus par leur chemin complet (l'ordre des dossiers peut changer d'un jour à l'autre).
        Anciens projets : le nom seul, cherché dans Content/Samples du greffon. */
    void saveExtraState (std::vector<std::uint8_t>& out) override
    {
        for (int k = 0; k < files.slots; ++k)
        {
            std::string n = files.loadedPath (k);
            if (n.empty())
            {
                // pas encore chargé (projet ouvert à l'instant) : le choix des réglages
                const auto* lib = files.library();
                const int f = getInt (firstFile + k), d = firstDir > 0 ? getInt (firstDir + k) : 0;
                if (lib != nullptr && f > 0 && d < (int) lib->folders.size() && f - 1 < (int) lib->folders[(size_t) d].files.size())
                    n = lib->folders[(size_t) d].path + "/" + lib->folders[(size_t) d].files[(size_t) f - 1];
            }
            out.insert (out.end(), n.begin(), n.end());
            out.push_back (0);
        }
    }
    void loadExtraState (const std::uint8_t* data, size_t size) override
    {
        size_t pos = 0;
        const auto* lib = files.library();
        for (int k = 0; k < files.slots && pos < size; ++k)
        {
            std::string n;
            while (pos < size && data[pos] != 0) n += (char) data[pos++];
            ++pos;
            if (n.empty() || lib == nullptr) continue;
            const bool full = n.find ('/') != std::string::npos;
            const std::string dir = full ? parentOf (n) : lib->folders[0].path;
            const std::string name = full ? baseOf (n) : n;
            bool found = false;
            for (size_t d = 0; d < lib->folders.size() && ! found; ++d)
            {
                if (lib->folders[d].path != dir) continue;
                const auto& fs = lib->folders[d].files;
                const auto it = std::find (fs.begin(), fs.end(), name);
                if (it == fs.end()) break;
                if (firstDir > 0) { set (firstDir + k, (float) d); lastDir[(size_t) k].store ((int) d); }
                set (firstFile + k, (float) (it - fs.begin() + 1));
                hold[(size_t) k].store (false);
                found = true;
            }
            if (! found && full)
            {
                // dossier pas (encore) dans la bibliothèque : chargé par son chemin, gardé jusqu'au prochain choix
                overrideTok[(size_t) k].store (files.setOverride (k, n));
                if (firstDir > 0) lastDir[(size_t) k].store (getInt (firstDir + k));
                holdVal[(size_t) k].store (getInt (firstFile + k));
                hold[(size_t) k].store (true);
            }
        }
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        pullParams();
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        arp.clear();
        arpWasOn = false;
        module->prepare (sampleRate, std::min (128, std::max (32, maxBlockSize)));
        tmpR.assign ((size_t) std::max (32, maxBlockSize) * 2, 0.0f);
        // Mise en route hors du fil audio (prepare vient du fil de l'interface) : une note jouée puis
        // coupée. Les instruments qui calculent leur son à l'avance (Kick Forge) font ici leur tout
        // premier calcul, au lieu de le faire dans le fil audio au premier coup.
        std::vector<float> l (64, 0.0f), r (64, 0.0f);
        module->noteOn (desc.drumKit ? 0 : 60, 0.8f);
        module->render (l.data(), r.data(), 64, plain.data());
        module->allNotesOff();
        module->reset();
        if (files.slots > 0)
        {
            if ((firstNav > 0 || firstWave > 0) && ! files.onTick) files.onTick = [this] { browse(); waveTick(); };
            files.start();
        }
        if (phraseParam >= 0) { appliedPhrase = -1; }
    }

    void reset() override { arp.clear(); arpWasOn = false; numKeys = 0; direct.fill (false); module->allNotesOff(); module->reset(); }

    void process (mpc::ProcessContext& ctx) override
    {
        pullParams();
        applyMutes();
        const auto& tr = ctx.transport;
        blockBpm = tr.tempoValid && tr.bpm > 0.0 ? tr.bpm : 120.0;
        blockPpq = tr.valid && tr.ppqValid ? tr.ppqPos : 0.0;
        blockPlaying = tr.valid && tr.playing && tr.ppqValid;
        if (tr.tempoValid && tr.bpm > 0.0) module->setTempo (tr.bpm);
        if (tr.valid)
        {
            if (tr.playing != wasPlaying) { if (tr.playing) module->transportStarted(); else module->transportStopped(); wasPlaying = tr.playing; }
            module->setPosition (tr.ppqValid ? tr.ppqPos : 0.0, tr.playing);
        }
        applyExtras();
        float* L = ctx.numOutputs > 0 ? ctx.outputs[0] : nullptr;
        float* R = ctx.numOutputs > 1 ? ctx.outputs[1] : nullptr;
        if (L == nullptr) return;
        if (R == nullptr) R = tmpR.data(); // sortie mono (jamais le cas sur MPC)
        const int n = ctx.numFrames;
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);

        int pos = 0;
        for (int e = 0; e < ctx.numMidiEvents; ++e)
        {
            const auto& ev = ctx.midi[e];
            const int at = std::clamp (ev.sampleOffset, pos, n);
            if (at > pos) { renderRange (L, R, pos, at); pos = at; }
            handle (ev);
        }
        if (pos < n) renderRange (L, R, pos, n);
        applyGain (L, R, n);
        if (ballParam >= 0)
        {
            // position de la bille (-1 à 1, y vers le haut) pour l'écran : simple écriture atomique
            float st[16];
            if (module->status (st, 16) >= 2)
            {
                set (ballParam, std::clamp (st[0], -1.0f, 1.0f));
                set (ballParam + 1, std::clamp (st[1], -1.0f, 1.0f));
            }
        }
    }

    /** Morceaux de 128 échantillons au plus : comme dans GOLEM (certains instruments, les batteries
        par exemple, ont des tampons internes de cette taille). La MPC peut envoyer des blocs de 512. */
    static constexpr int maxChunk = 128;

    void renderRange (float* L, float* R, int from, int to) noexcept
    {
        for (int p = from; p < to;)
        {
            const int m = std::min (maxChunk, to - p);
            runArp (p, m);
            module->render (L + p, R + p, m, plain.data());
            p += m;
        }
    }

    bool arpActive() const noexcept
    {
        return arpParam >= 0 && plain[(size_t) arpParam] >= 0.5f && (arpOnParam < 0 || plain[(size_t) arpOnParam] >= 0.5f);
    }

    /** Arpège : mêmes réglages et même calcul que le moteur de GOLEM (Engine::runArpeggiators). */
    void runArp (int offset, int n) noexcept
    {
        auto off = [this] (int note) { module->noteOff (note); };
        if (! arpActive())
        {
            if (arpWasOn) arp.flush (off);
            arpWasOn = false;
            lastHold = false;
            return;
        }
        arpWasOn = true;
        // maintien mis en marche pendant que des notes sont tenues : elles sont retenues tout de suite
        // (sinon il fallait relâcher et rejouer l'accord)
        const bool holdNow = plain[(size_t) arpParam + 4] > 0.5f;
        if (holdNow && ! lastHold)
            for (int i = 0; i < numKeys; ++i) { arp.noteOff (keys[(size_t) i]); arp.noteOn (keys[(size_t) i], keyVel[(size_t) i], true); }
        lastHold = holdNow;
        static constexpr double rates[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 0.75, 1.0 / 12.0 };
        const float* q = plain.data() + arpParam;
        grv::engine::Arpeggiator::Settings s;
        s.mode = (int) q[0];
        s.octaves = (int) std::lround (q[1]);
        s.stepQuarters = rates[std::clamp ((int) q[2], 0, 7)];
        s.gate = q[3] / 100.0;
        s.hold = q[4] > 0.5f;
        s.swing = q[5] / 100.0;
        if (arpStepsParam >= 0)
        {
            s.steps = (int) std::lround (plain[(size_t) arpStepsParam]);
            s.transpose = plain.data() + arpStepsParam + 1;
            s.velocity = plain.data() + arpStepsParam + 17;
        }
        const double pps = blockBpm / 60.0 / sr;
        arp.process (s, blockPpq + pps * offset, blockPlaying, pps, n,
                     [this] (int note, float vel) { module->noteOn (note, vel); }, off);
    }

private:
    /** Sons coupés (Muet, ou un autre son en Solo) : leur niveau passe à -48 dB, le silence des batteries
        (le moteur fait la transition sur un bloc, sans clic). Le réglage lui-même ne change pas : il est
        relu au bloc suivant (lastNorm à -1), puis de nouveau remplacé tant que le son reste coupé. */
    void applyMutes() noexcept
    {
        if (firstMute < 0) return;
        bool anySolo = false;
        for (int k = 0; k < numMutePads && ! anySolo; ++k) anySolo = getInt (firstSolo + k) != 0;
        for (int k = 0; k < numMutePads; ++k)
        {
            const int li = padLevel[(size_t) k];
            if (li < 0) continue;
            const bool off = getInt (firstMute + k) != 0 || (anySolo && getInt (firstSolo + k) == 0);
            if (off) { plain[(size_t) li] = -48.0f; lastNorm[(size_t) li] = -1.0f; }
        }
    }

    // ---- navigateur de samples (voir les réglages « Nav ») ----
    // Comme le navigateur de la Force : la liste des disques, puis on entre dans un dossier (ses
    // sous-dossiers, puis ses fichiers WAV), on remonte par la ligne du haut, on touche un fichier pour le
    // charger dans l'emplacement choisi. Tout se fait dans le fil de chargement (lecture des dossiers) ;
    // l'écran lit les textes sous verrou ; le fil audio n'y touche jamais.
    static constexpr int navRows = 10, navPrev = 1, navNext = 2, navUp = 3, navRow = 4, navTxt = navRow + navRows;
    struct NavEntry { std::string name, path; bool dir = false; };

    /** Texte d'une ligne du navigateur (0 : la ligne du haut, où l'on est). Fil de l'interface. */
    std::string navText (int line)
    {
        std::lock_guard<std::mutex> g (navLock);
        const int n = (int) navList.size();
        const int pages = std::max (1, (n + navRows - 1) / navRows), page = std::min (navPageW, pages - 1);
        if (line == 0)
            return (navCwd.empty() ? L ("Disques") : "<  " + navTitle) + "   " + std::to_string (page + 1) + "/" + std::to_string (pages);
        const int i = page * navRows + line - 1;
        if (i < 0 || i >= n) return " ";
        const auto& e = navList[(size_t) i];
        if (e.dir) return fold (e.name) + "  >";
        return (e.path == navLoadedW ? "* " : "") + fold (e.name);
    }

    std::vector<NavEntry> navListDir (const std::string& dir) const
    {
        std::vector<NavEntry> dirs, wavs;
        if (DIR* d = opendir (dir.c_str()))
        {
            while (auto* e = readdir (d))
            {
                const std::string n = e->d_name;
                if (n.empty() || n[0] == '.' || n == "System Volume Information" || n == "$RECYCLE.BIN" || n == "lost+found") continue;
                const std::string p = dir + "/" + n;
                const auto x = lowerExt (n);
                if (std::find (files.exts.begin(), files.exts.end(), x) != files.exts.end()) wavs.push_back ({ n, p, false });
                else if (e->d_type == DT_DIR || ((e->d_type == DT_UNKNOWN || e->d_type == DT_LNK) && isDirectory (p))) dirs.push_back ({ n, p, true });
                if (dirs.size() + wavs.size() > 3000) break;
            }
            closedir (d);
        }
        auto byName = [] (const NavEntry& a, const NavEntry& b) { return lowerCopy (a.name) < lowerCopy (b.name); };
        std::sort (dirs.begin(), dirs.end(), byName);
        std::sort (wavs.begin(), wavs.end(), byName);
        for (auto& w : wavs) dirs.push_back (std::move (w));
        return dirs;
    }

    /** Fil de chargement, toutes les 15 ms : appuis sur les boutons, lecture des dossiers, chargement. */
    void browse()
    {
        if (firstNav <= 0) return;
        auto val = [&] (int k) { return getInt (firstNav + k); };
        if (! navInitW)
        {
            // valeurs des boutons à l'ouverture (projet rechargé) : pas des appuis
            for (int k = navPrev; k < navTxt; ++k) navLastV[(size_t) k] = val (k);
            navInitW = true;
            navRelist = true;
        }
        auto pressed = [&] (int k) { const int v = val (k); const bool p = v != navLastV[(size_t) k]; navLastV[(size_t) k] = v; return p; };
        bool changed = false;
        std::string cwd;
        int page, n;
        { std::lock_guard<std::mutex> g (navLock); cwd = navCwd; page = navPageW; n = (int) navList.size(); }
        const int pages = std::max (1, (n + navRows - 1) / navRows);
        if (pressed (navPrev)) { page = page > 0 ? page - 1 : pages - 1; changed = true; }
        if (pressed (navNext)) { page = page + 1 < pages ? page + 1 : 0; changed = true; }
        if (pressed (navUp) && ! cwd.empty())
        {
            const bool isRoot = std::find (navRoots.begin(), navRoots.end(), cwd) != navRoots.end();
            cwd = isRoot ? std::string() : parentOf (cwd);
            navRelist = true;
        }
        for (int r = 0; r < navRows; ++r)
        {
            if (! pressed (navRow + r)) continue;
            NavEntry e;
            {
                std::lock_guard<std::mutex> g (navLock);
                const int i = page * navRows + r;
                if (i >= (int) navList.size()) continue;
                e = navList[(size_t) i];
            }
            if (e.dir) { cwd = e.path; navRelist = true; }
            else
            {
                // fichier touché : chargé par son chemin dans l'emplacement choisi
                const int k = std::clamp (val (0), 0, files.slots - 1);
                overrideTok[(size_t) k].store (files.setOverride (k, e.path));
                if (desc.type == "kickforge")
                {
                    // Kick Forge : la couche passe en « Mes samples » (dernier choix de la famille) et s'allume
                    const auto fam = "l" + std::to_string (k + 1) + "_fam", on = "l" + std::to_string (k + 1) + "_on";
                    const int fi = desc.indexOf (fam), oi = desc.indexOf (on);
                    if (fi >= 0) { set (fi + 1, (float) (desc.params[(size_t) fi].choices.size() - 1)); notifyHostParameterChanged (fi + 1); }
                    if (oi >= 0) { set (oi + 1, 1.0f); notifyHostParameterChanged (oi + 1); }
                }
                if (firstDir > 0) lastDir[(size_t) k].store (getInt (firstDir + k));
                holdVal[(size_t) k].store (getInt (firstFile + k));
                hold[(size_t) k].store (true);
                changed = true;
            }
        }
        if (navRelist)
        {
            navRelist = false;
            std::vector<NavEntry> list;
            std::string title;
            if (cwd.empty())
            {
                // les disques : le dossier du greffon, puis tous les disques trouvés
                navRoots.clear();
                const auto* lib = files.library();
                if (lib != nullptr && ! lib->folders.empty())
                    list.push_back ({ "Greffon : Samples", lib->folders[0].path, true });
                for (const auto& r : sampleRoots())
                {
                    navRoots.push_back (r.path);
                    const bool az = baseOf (r.path) == "az01-internal";
                    list.push_back ({ r.name == "Interne" ? std::string (az ? "Interne (az01-internal)" : "Interne (disque de la Force)") : r.name, r.path, true });
                }
                if (lib != nullptr && ! lib->folders.empty()) navRoots.push_back (lib->folders[0].path);
            }
            else
            {
                list = navListDir (cwd);
                title = baseOf (cwd);
                for (const auto& r : sampleRoots())
                    if (cwd.compare (0, r.path.size(), r.path) == 0) { title = folderLabel (cwd, r.path, r.name); break; }
            }
            std::lock_guard<std::mutex> g (navLock);
            navCwd = cwd;
            navTitle = title;
            navList = std::move (list);
            page = 0;
            changed = true;
        }
        const int slot = val (0);
        const auto loaded = files.loadedPath (std::clamp (slot, 0, std::max (0, files.slots - 1)));
        {
            std::lock_guard<std::mutex> g (navLock);
            if (slot != navSlotW || loaded != navLoadedW) { navSlotW = slot; navLoadedW = loaded; changed = true; }
            const int np = std::max (1, ((int) navList.size() + navRows - 1) / navRows);
            navPageW = std::clamp (page, 0, np - 1);
        }
        if (changed)
        {
            // La machine ne relit le texte d'un réglage que si on lui dit qu'il a bougé (essai du 26 nov. :
            // l'écran restait figé sur la liste des disques alors que le greffon avait bien ouvert le dossier).
            // On change la valeur des 11 lignes de texte puis on prévient la machine (hors du fil audio).
            navGen = (navGen + 1) & 1023;
            for (int r = 0; r <= navRows; ++r)
            {
                set (firstNav + navTxt + r, (float) navGen);
                notifyHostParameterChanged (firstNav + navTxt + r);
            }
        }
    }

    /** Emplacement de fichier joué par une piste (-1 : aucun : enregistreur, emplacement vide). */
    int waveFileOf (int lane) const
    {
        if (waveKind == 1) return 0;
        if (waveKind == 2)
        {
            const int k = std::clamp (lane, 0, files.slots - 1);
            return files.loadedPath (k).empty() ? 0 : k;   // emplacement vide : la machine joue le sample 1
        }
        const int si = waveWatch[(size_t) (lane * waveStride)];
        const int v = si > 0 ? (int) std::lround (get (si)) : 1;
        int i = -1;
        if (v >= 1 && v <= 16) i = v - 1;                     // « Échantillon 1 à 16 »
        else if (v >= 17 && v < 17 + 128) i = (v - 17) % 16;  // « T· 1 à 16 » : même emplacement de chaque banque
        return i >= 0 && i < files.slots ? i : -1;
    }

    /** Fil de chargement : redessine la forme quand la piste réglée, ses réglages ou son fichier changent. */
    void waveTick()
    {
        if (firstWave < 0) return;
        if (firstCut > 0)
            for (int b = 0; b < 2; ++b)
            {
                const int v = getInt (firstCut + b);
                if (cutLast[(size_t) b] >= 0 && v != cutLast[(size_t) b])
                {
                    const auto& sl = b == 0 ? files.slicesEq[0] : files.slicesOn[0];
                    for (int z = 0; z < 16; ++z)
                    {
                        const float a = z < (int) sl.size() ? sl[(size_t) z].first * 100.0f : 0.0f;
                        const float e = z < (int) sl.size() ? sl[(size_t) z].second * 100.0f : 0.0f;
                        for (int j = 0; j < 2; ++j)
                        {
                            const int pi = waveWatch[(size_t) (z * waveStride + j)];
                            if (pi > 0) { set (pi, j == 0 ? a : e); notifyHostParameterChanged (pi); }
                        }
                    }
                    if (sliceCntIdx > 0) { set (sliceCntIdx, 0.0f); notifyHostParameterChanged (sliceCntIdx); }   // zones libres
                    // la forme reste sur la zone montrée et se redessine avec les nouvelles découpes
                    for (size_t j = 0; j < waveWatch.size(); ++j) waveLast[j] = waveWatch[j] > 0 ? get (waveWatch[j]) : 0.0f;
                    waveFileSeen = -2;
                }
                cutLast[(size_t) b] = v;
            }
        if (waveInit && waveKind == 2 && cutCount() > 0)
        {
            // 27 nov. 2026 : avec « Découpes » sur 2 à 128, le moteur impose des parts égales et Début / Fin des
            // zones ne servaient à rien. Dès qu'on touche un Début ou une Fin : chaque zone prend sa part
            // actuelle, « Découpes » repasse sur « Zones libres », et le réglage touché garde sa valeur.
            int hit = -1;
            float hitV = 0.0f;
            for (size_t j = 0; j < waveWatch.size(); ++j)
                if (waveWatch[j] > 0 && get (waveWatch[j]) != waveLast[j]) { hit = (int) j; hitV = get (waveWatch[j]); }
            if (hit >= 0)
            {
                const int cnt = cutCount(), page = cutPage();
                for (int z = 0; z < waveLanes; ++z)
                {
                    const int idx = page * 16 + z;
                    if (idx >= cnt) continue;
                    for (int j = 0; j < 2; ++j)
                    {
                        const int pi = waveWatch[(size_t) (z * waveStride + j)];
                        if (pi > 0) { set (pi, (float) (idx + j) * 100.0f / (float) cnt); notifyHostParameterChanged (pi); }
                    }
                }
                set (waveWatch[(size_t) hit], hitV); notifyHostParameterChanged (waveWatch[(size_t) hit]);
                set (sliceCntIdx, 0.0f); notifyHostParameterChanged (sliceCntIdx);
                for (size_t j = 0; j < waveWatch.size(); ++j) waveLast[j] = waveWatch[j] > 0 ? get (waveWatch[j]) : 0.0f;
                waveLane = hit / std::max (1, waveStride);
                waveFileSeen = -2;
            }
        }
        if (sliceCntIdx > 0)
        {
            const float v = get (sliceCntIdx) * 10.0f + (slicePageIdx > 0 ? get (slicePageIdx) : 0.0f);
            if (v != sliceSeen) { sliceSeen = v; waveFileSeen = -2; }   // Découpes / Page changés : redessiner
        }
        bool changed = ! waveInit;
        for (size_t j = 0; j < waveWatch.size(); ++j)
        {
            const float v = waveWatch[j] > 0 ? get (waveWatch[j]) : 0.0f;
            if (v != waveLast[j])
            {
                if (waveInit) { waveLane = (int) j / std::max (1, waveStride); changed = true; }
                waveLast[j] = v;
            }
        }
        waveInit = true;
        const int fs = waveFileOf (waveLane);
        const int pg = fs >= 0 ? files.peakGen[(size_t) fs] : -1;
        if (fs != waveFileSeen || pg != wavePeakSeen) { waveFileSeen = fs; wavePeakSeen = pg; changed = true; }
        if (! changed) return;

        auto val = [&] (int a) { const int i = waveWatch[(size_t) (waveLane * waveStride + a)]; return i > 0 ? get (i) : 0.0f; };
        double a = 0.0, b = 1.0, la = -1.0, lb = -1.0;
        int count = 0;
        if (waveKind == 1)
        {
            a = val (0) / 100.0; b = waveWatch[1] > 0 ? val (1) / 100.0 : 1.0;
            if (val (2) > 0.5f) { la = val (3) / 100.0; lb = val (4) / 100.0; }
        }
        else if (waveKind == 2)
        {
            a = val (0) / 100.0; b = val (1) / 100.0;
            const int cnt = cutCount();
            if (cnt > 0)
            {
                // Cutsample, « Découpes » : la zone joue la part (page x 16 + zone) sur cnt
                const int idx = cutPage() * 16 + waveLane;
                if (idx < cnt) { a = (double) idx / cnt; b = (double) (idx + 1) / cnt; }
                else { a = b = 0.0; }
            }
            else if (desc.type == "sp" && sliceCntIdx > 0 && fs == 0)
            {
                // Poussière, « Découpage » : Début / Fin du pad se règlent dans sa part du sample du pad 1
                const int mode = std::clamp ((int) std::lround (get (sliceCntIdx)), 0, 2);
                if (mode > 0)
                {
                    double pa = waveLane / 16.0, pb = (waveLane + 1) / 16.0;
                    if (mode == 2)
                    {
                        const auto& on = files.slicesOn[0];
                        if (waveLane < (int) on.size()) { pa = on[(size_t) waveLane].first; pb = on[(size_t) waveLane].second; }
                    }
                    const double w = pb - pa;
                    a = pa + w * val (0) / 100.0; b = pa + w * val (1) / 100.0;
                }
            }
        }
        else
        {
            static constexpr int grid[8] { 0, 2, 4, 8, 16, 32, 64, 0 };
            count = grid[std::clamp ((int) std::lround (val (4)), 0, 7)];
            a = val (1) / 100.0; b = 1.0;
            if (count > 0)
            {
                const int idx = std::clamp ((int) std::floor (val (1) / 100.0 * count), 0, count - 1);
                a = (double) idx / count; b = (double) (idx + 1) / count;
            }
            b = a + (b - a) * std::max (0.001, std::clamp (val (2) / 100.0, 0.0, 1.0));
            if (val (3) > 0.5f) { la = a; lb = b; }
        }
        if (b < a) std::swap (a, b);
        if (la > lb) std::swap (la, lb);
        const auto* pk = fs >= 0 ? &files.peaks[(size_t) fs] : nullptr;
        auto colOf = [] (double x) { return std::clamp ((int) std::floor (x * kWaveCols), 0, kWaveCols - 1); };
        std::array<int, kWaveCols> fr {};
        for (int c = 0; c < kWaveCols; ++c)
        {
            const double x = (c + 0.5) / kWaveCols;
            const int lvl = pk != nullptr ? (*pk)[(size_t) c] : 0;
            const int kind = (la >= 0.0 && x >= la && x <= lb) ? 2 : (x >= a && x <= b) ? 1 : 0;
            fr[(size_t) c] = kind * kWaveLevels + lvl;
        }
        if (count > 1 && count <= 64)
            for (int k = 1; k < count; ++k) fr[(size_t) colOf ((double) k / count)] = 3 * kWaveLevels + 2;   // découpes
        if (waveKind == 2 && cutCount() > 0)
        {
            const int cnt = cutCount(), p0 = cutPage() * 16;
            for (int k = std::max (1, p0); k <= std::min (cnt - 1, p0 + 16); ++k) fr[(size_t) colOf ((double) k / cnt)] = 3 * kWaveLevels + 2;
        }
        else if (waveKind == 2)
            for (int z = 0; z < waveLanes; ++z)   // débuts des autres zones qui lisent le même sample
                if (z != waveLane && waveFileOf (z) == fs)
                {
                    const int pi = waveWatch[(size_t) (z * waveStride)], pe = waveWatch[(size_t) (z * waveStride + 1)];
                    if (pi > 0 && pe > 0 && get (pe) > get (pi)) fr[(size_t) colOf (get (pi) / 100.0)] = 3 * kWaveLevels + 2;
                }
        if (la >= 0.0) fr[(size_t) colOf (la)] = 3 * kWaveLevels + 1;                                    // début de boucle
        fr[(size_t) colOf (a)] = 3 * kWaveLevels;                                                          // début
        fr[(size_t) colOf (std::max (a, b - 1.0e-6))] = 3 * kWaveLevels;                                   // fin

        std::string name = fs >= 0 ? baseOf (files.loadedPath (fs)) : std::string();
        if (name.empty()) name = fs >= 0 ? L ("sample du greffon") : L ("pas de sample (enregistreur)");
        std::string title = waveKind == 1 ? name
                          : (L (waveKind == 3 ? "Piste" : desc.type == "sp" ? "Pad" : "Zone") + " " + std::to_string (waveLane + 1) + "  \xC2\xB7  " + name);
        for (int c = 0; c < kWaveCols; ++c)
            if (fr[(size_t) c] != waveShown[(size_t) c])
            {
                waveShown[(size_t) c] = fr[(size_t) c];
                set (firstWave + c, (float) fr[(size_t) c]);
                notifyHostParameterChanged (firstWave + c);
            }
        title = fold (title);
        bool newTitle;
        {
            std::lock_guard<std::mutex> g (navLock);
            newTitle = title != waveTitle;
            waveTitle = title;
        }
        if (newTitle)
        {
            waveTxtGen = (waveTxtGen + 1) & 1023;
            set (waveTxtParam, (float) waveTxtGen);
            notifyHostParameterChanged (waveTxtParam);
        }
    }

    void applyExtras() noexcept
    {
        for (int k = 0; k < files.slots; ++k)
        {
            // dossier changé : le sample chargé reste tant qu'on n'a pas choisi un fichier du nouveau dossier
            const int d = firstDir > 0 ? getInt (firstDir + k) : 0;
            const int f = getInt (firstFile + k);
            const int ld = lastDir[(size_t) k].load (std::memory_order_relaxed);
            if (d != ld)
            {
                if (ld >= 0 && ! hold[(size_t) k].load()) { holdVal[(size_t) k].store (f); overrideTok[(size_t) k].store (0); hold[(size_t) k].store (true); }
                lastDir[(size_t) k].store (d, std::memory_order_relaxed);
            }
            if (hold[(size_t) k].load() && f != holdVal[(size_t) k].load()) hold[(size_t) k].store (false);
            // noms montrés à côté des sélecteurs : leur valeur suit le dossier et le fichier (la machine relit
            // alors leur texte)
            if (firstName > 0)
            {
                if (getInt (firstName + 2 * k) != d) set (firstName + 2 * k, (float) d);
                if (getInt (firstName + 2 * k + 1) != f) set (firstName + 2 * k + 1, (float) f);
            }
            int tok = f <= 0 ? -1 : FileLoader::encode (d, f - 1);
            if (hold[(size_t) k].load())
            {
                const int o = overrideTok[(size_t) k].load();
                tok = o < 0 ? o : FileLoader::HOLD;
            }
            files.request (k, tok);
            if (auto* sb = files.take (k))
            {
                const auto* use = sb == FileLoader::empty() ? nullptr : sb;
                if (ex.banks > 0) for (int b = 0; b < ex.banks; ++b) module->setSample (b * 16 + k, use);
                else module->setSample (k, use);
                files.retire (k, sb);
            }
        }
        if (laneParam >= 0) module->setPlayLane (getInt (laneParam) - 1);
        if (phraseParam >= 0)
        {
            const int ph = std::clamp (getInt (phraseParam), 0, (int) scripts.size() - 1);
            if (ph != appliedPhrase) { module->setSpeech (scripts[(size_t) ph].get()); appliedPhrase = ph; }
        }
    }

    int mapNote (int note) const noexcept
    {
        if (ex.allNotes) return note >= 16 ? note : -1;
        if (ex.laneNotes)
        {
            const int nl = std::max (1, ex.lanes);
            if (padLaneParam >= 0 && getInt (padLaneParam) == 1) return note >= 16 ? note : -1; // comme GOLEM
            if (note >= 16) return ((note - 36) % nl + nl) % nl;                   // chaque pad joue sa piste
            return -1;
        }
        if (! desc.drumKit) return note;
        if (padParam >= 0 && getInt (padParam) >= 1)
        {
            // gamme de la Force à partir de C1 = 36, une note de la gamme par pad : pads 1 à 16
            //   Do majeur : C D E F G A B (notes blanches) ; Do mineur : C D Eb F G Ab Bb
            static constexpr int major[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
            static constexpr int minor[12] = { 0, -1, 1, 2, -1, 3, -1, 4, 5, -1, 6, -1 };
            const int* deg = getInt (padParam) == 2 ? minor : major;
            if (note < 0 || deg[note % 12] < 0) return -1;
            const int d = (note / 12) * 7 + deg[note % 12] - 21;
            return d >= 0 && d < 16 ? d : -1;
        }
        if (note >= 36 && note < 52) return note - 36;   // pads MPC (A01..A16)
        if (note >= 0 && note < 16) return note;
        return -1;
    }

    void handle (const mpc::MidiEvent& ev) noexcept
    {
        if (ev.isNoteOn())
        {
            const int k = mapNote (ev.note());
            const float vel = (float) ev.velocity() / 127.0f;
            if (k >= 0 && arpActive()) { arp.noteOn (k, vel, plain[(size_t) arpParam + 4] > 0.5f); pushKey (k, vel); }
            else if (k >= 0) { module->noteOn (k, vel); direct[(size_t) k] = true; }
        }
        else if (ev.isNoteOff())
        {
            const int k = mapNote (ev.note());
            if (k >= 0)
            {
                arp.noteOff (k);
                popKey (k);
                // une note jouée sans l'arpège reçoit toujours son relâchement, même si l'arpège a été
                // mis en marche entre-temps (sinon elle restait tenue)
                if (direct[(size_t) k]) { module->noteOff (k); direct[(size_t) k] = false; }
            }
        }
        else if (ev.isAllNotesOff()) { arp.clear(); numKeys = 0; direct.fill (false); module->allNotesOff(); }
        else if (ev.isPitchBend()) { bend = std::clamp ((float) ev.pitchBend() / 8192.0f, -1.0f, 1.0f); module->setExpression (bend, wheel, pressure); }
        else if (ev.isAftertouch()) { pressure = (float) ev.data[1] / 127.0f; module->setExpression (bend, wheel, pressure); }
        else if (ev.isController())
        {
            if (ev.controller() == 1) { wheel = (float) ev.controllerValue() / 127.0f; module->setExpression (bend, wheel, pressure); }
            module->controller (ev.controller(), (float) ev.controllerValue() / 127.0f);
        }
    }

    std::unique_ptr<Module> module;
    Extras ex;
    FileLoader files;
    // forme du sample (fil de chargement ; titre lu par l'interface sous navLock)
    static constexpr int waveFrames = 3 * kWaveLevels + 3;   // 3 couleurs x 16 hauteurs, puis 3 repères
    int waveKind = 0, firstWave = -1, waveTxtParam = -1, waveLanes = 0, waveStride = 0, waveLane = 0;
    int waveFileSeen = -2, wavePeakSeen = -1, waveTxtGen = 0, firstCut = -1, sliceCntIdx = 0, slicePageIdx = 0;
    /** Cutsample : nombre de parts égales imposé par « Découpes » (0 : zones libres). */
    int cutCount() const { static constexpr int c[8] { 0, 2, 4, 8, 16, 32, 64, 128 }; return sliceCntIdx > 0 && desc.type == "sampler" ? c[std::clamp ((int) std::lround (get (sliceCntIdx)), 0, 7)] : 0; }
    int cutPage() const { return slicePageIdx > 0 ? std::clamp ((int) std::lround (get (slicePageIdx)), 0, 7) : 0; }
    std::array<int, 2> cutLast { -1, -1 };
    float sliceSeen = -1.0f;
    bool waveInit = false;
    std::vector<int> waveWatch;
    std::vector<float> waveLast;
    std::array<int, kWaveCols> waveShown {};
    std::string waveTitle;
    int padLaneParam = -1, firstNav = -1, firstName = -1, firstFile = -1, laneParam = -1, phraseParam = -1, padParam = -1, appliedPhrase = -1, ballParam = -1, firstDir = -1;
    static constexpr int maxPads = 16;
    std::array<int, maxPads> padLevel {};   // index (GOLEM) du niveau de chaque son, -1 sans niveau
    int firstMute = -1, firstSolo = -1, numMutePads = 0;
    grv::engine::Arpeggiator arp;
    int arpParam = -1, arpOnParam = -1, arpStepsParam = -1;
    bool arpWasOn = false, blockPlaying = false, lastHold = false;
    std::array<int, 16> keys {};          // touches tenues qui vont à l'arpège, dans l'ordre joué
    std::array<float, 16> keyVel {};
    int numKeys = 0;
    std::array<bool, 128> direct {};      // notes envoyées directement à l'instrument (sans l'arpège)
    void pushKey (int k, float v) noexcept
    {
        for (int i = 0; i < numKeys; ++i) if (keys[(size_t) i] == k) { keyVel[(size_t) i] = v; return; }
        if (numKeys < 16) { keys[(size_t) numKeys] = k; keyVel[(size_t) numKeys] = v; ++numKeys; }
    }
    void popKey (int k) noexcept
    {
        for (int i = 0; i < numKeys; ++i)
            if (keys[(size_t) i] == k)
            {
                for (int j = i; j + 1 < numKeys; ++j) { keys[(size_t) j] = keys[(size_t) j + 1]; keyVel[(size_t) j] = keyVel[(size_t) j + 1]; }
                --numKeys;
                return;
            }
    }
    double sr = 44100.0, blockBpm = 120.0, blockPpq = 0.0;
    // emplacements de sample : dossier vu au dernier bloc, « garder le chargé » et la valeur alors en place
    std::array<std::atomic<int>, maxSlots> lastDir {}, holdVal {}, overrideTok {};
    // navigateur (fil de chargement ; textes lus par l'interface sous navLock)
    std::mutex navLock;
    std::string navCwd, navTitle, navLoadedW;
    std::vector<NavEntry> navList;
    std::vector<std::string> navRoots;
    std::array<int, 16> navLastV {};
    int navPageW = 0, navSlotW = -1, navGen = 0;
    bool navInitW = false, navRelist = true;
    std::array<std::atomic<bool>, maxSlots> hold {};
    std::vector<std::unique_ptr<grv::engine::SpeechScript>> scripts;
    std::vector<float> tmpR;
    bool wasPlaying = false;
    float bend = 0.0f, wheel = 0.0f, pressure = 0.0f;
};

//==============================================================================
template <class Module>
class EffectPlugin : public GolemBase
{
public:
    explicit EffectPlugin (const mpc::PluginInfo& info, std::unique_ptr<Module> m = std::make_unique<Module>())
        : GolemBase (info, localized (m->desc())), module (std::move (m)) {}

    void prepare (double sampleRate, int maxBlockSize) override
    {
        const int mb = std::min (128, std::max (32, maxBlockSize)); // morceaux de 128 au plus (comme dans GOLEM)
        module->prepare (sampleRate, mb);
        bufL.assign ((size_t) mb, 0.0f);
        bufR.assign ((size_t) mb, 0.0f);
        setLatency (module->latency());
    }

    void reset() override { module->reset(); }

    void process (mpc::ProcessContext& ctx) override
    {
        pullParams();
        const auto& tr = ctx.transport;
        const double bpm = tr.tempoValid && tr.bpm > 0.0 ? tr.bpm : 120.0;
        if (tr.valid) module->setPosition (tr.ppqValid ? tr.ppqPos : 0.0, tr.playing);
        const int n = ctx.numFrames;
        if (ctx.numOutputs < 1) return;
        float* L = ctx.outputs[0];
        float* R = ctx.numOutputs > 1 ? ctx.outputs[1] : nullptr;
        const float* inL = ctx.numInputs > 0 && ctx.inputs ? ctx.inputs[0] : nullptr;
        const float* inR = ctx.numInputs > 1 && ctx.inputs ? ctx.inputs[1] : inL;
        // traitement sur place dans des tampons à soi (taille du bloc annoncée : jamais dépassée)
        for (int pos = 0; pos < n;)
        {
            const int m = std::min ((int) bufL.size(), n - pos);
            for (int k = 0; k < m; ++k)
            {
                bufL[(size_t) k] = inL ? inL[pos + k] : 0.0f;
                bufR[(size_t) k] = inR ? inR[pos + k] : bufL[(size_t) k];
            }
            module->process (bufL.data(), bufR.data(), m, plain.data(), bpm);
            for (int k = 0; k < m; ++k)
            {
                L[pos + k] = bufL[(size_t) k];
                if (R) R[pos + k] = bufR[(size_t) k];
            }
            pos += m;
        }
        applyGain (L, R, n);
    }

private:
    std::unique_ptr<Module> module;
    std::vector<float> bufL, bufR;
};

//==============================================================================
/** Vocodeur en effet : le son de la piste où il est inséré est la voix (modulateur) ; la porteuse joue
    la note et l'accord choisis dans les réglages (un effet ne reçoit pas de notes sur la machine). */
template <class Module>
class VocoderFxPlugin : public GolemBase
{
public:
    explicit VocoderFxPlugin (const mpc::PluginInfo& info, std::unique_ptr<Module> m = std::make_unique<Module>())
        : GolemBase (info, localized (m->desc())), module (std::move (m))
    {
        srcIndex = desc.indexOf ("src");
        noteParam = numParams();
        addParam (mpc::intParam ("mpc_note", L ("Note porteuse").c_str(), 24, 84, 48));
        chordParam = numParams();
        mpc::Param q;
        q.id = "mpc_chord"; q.name = L ("Accord porteuse"); q.kind = mpc::Param::Kind::Choice;
        q.choices = { L ("Une note"), L ("Octave"), L ("Quinte"), L ("Majeur"), L ("Mineur"), L ("Sus4"), L ("Septieme"), L ("Mineur 7"), L ("Neuvieme") };
        q.min = 0; q.max = (float) q.choices.size() - 1; q.def = 4;
        addParam (q);
    }

    bool formatValue (int index, float plainValue, std::string& out) override
    {
        if (index == noteParam)
        {
#if defined (GRV_MPC_EN)
            static const char* nm[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
#else
            static const char* nm[12] = { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
#endif
            const int n = (int) std::lround (plainValue);
            out = std::string (nm[((n % 12) + 12) % 12]) + std::to_string (n / 12 - 2);
            return true;
        }
        return GolemBase::formatValue (index, plainValue, out);
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        const int mb = std::min (128, std::max (32, maxBlockSize));
        pullParams();
        module->prepare (sampleRate, mb);
        inBufL.assign ((size_t) mb, 0.0f);
        inBufR.assign ((size_t) mb, 0.0f);
        scratch.assign ((size_t) mb, 0.0f);
        held = -1;
    }

    void reset() override { module->allNotesOff(); module->reset(); held = -1; }

    void process (mpc::ProcessContext& ctx) override
    {
        pullParams();
        const auto& tr = ctx.transport;
        if (tr.tempoValid && tr.bpm > 0.0) module->setTempo (tr.bpm);
        if (tr.valid) module->setPosition (tr.ppqValid ? tr.ppqPos : 0.0, tr.playing);
        if (srcIndex >= 0) plain[(size_t) srcIndex] = 1.0f;   // modulateur : « Entrée audio » = le son de la piste
        // porteuse : note et accord tenus
        const int note = getInt (noteParam), chord = getInt (chordParam);
        const int key = note * 16 + chord;
        if (key != held)
        {
            module->allNotesOff();
            static const int iv[9][4] = { { 0, -1, -1, -1 }, { 0, 12, -1, -1 }, { 0, 7, 12, -1 }, { 0, 4, 7, 12 }, { 0, 3, 7, 12 },
                                          { 0, 5, 7, 12 }, { 0, 4, 7, 10 }, { 0, 3, 7, 10 }, { 0, 4, 7, 14 } };
            const int c = std::clamp (chord, 0, 8);
            for (int k = 0; k < 4; ++k)
                if (iv[c][k] >= 0) module->noteOn (note + iv[c][k], 0.85f);
            held = key;
        }
        const int n = ctx.numFrames;
        if (ctx.numOutputs < 1) return;
        float* L = ctx.outputs[0];
        float* R = ctx.numOutputs > 1 ? ctx.outputs[1] : nullptr;
        const float* inL = ctx.numInputs > 0 && ctx.inputs ? ctx.inputs[0] : nullptr;
        const float* inR = ctx.numInputs > 1 && ctx.inputs ? ctx.inputs[1] : inL;
        for (int pos = 0; pos < n;)
        {
            const int m = std::min ((int) inBufL.size(), n - pos);
            for (int k = 0; k < m; ++k)
            {
                inBufL[(size_t) k] = inL ? inL[pos + k] : 0.0f;
                inBufR[(size_t) k] = inR ? inR[pos + k] : inBufL[(size_t) k];
            }
            module->setInput (inBufL.data(), inBufR.data());
            float* r = R ? R + pos : tmpR (m);
            module->render (L + pos, r, m, plain.data());
            pos += m;
        }
        applyGain (L, R, n);
    }

private:
    float* tmpR (int m) { if ((int) scratch.size() < m) scratch.resize ((size_t) m); return scratch.data(); }

    std::unique_ptr<Module> module;
    std::vector<float> inBufL, inBufR, scratch;
    int srcIndex = -1, noteParam = -1, chordParam = -1, held = -1;
};

} // namespace grv::mpcport

/** Lu par le générateur (dlopen du greffon natif) : pages de l'interface. */
#define GOLEM_MPC_LAYOUT(PluginClass)                                                         \
    extern "C" __attribute__ ((visibility ("default"))) const char* golem_mpc_layout()        \
    {                                                                                         \
        static std::string s;                                                                 \
        if (s.empty()) { PluginClass p; s = p.layout(); }                                     \
        return s.c_str();                                                                     \
    }                                                                                         \
    extern "C" __attribute__ ((visibility ("default"))) const char* golem_mpc_catalog()       \
    {                                                                                         \
        static std::string s;                                                                 \
        if (s.empty()) { PluginClass p; s = p.catalog(); }                                    \
        return s.c_str();                                                                     \
    }
