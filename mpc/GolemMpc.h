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
#include "engine/Module.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace grv::mpcport
{

using grv::engine::ModuleDesc;
using grv::engine::ParamDesc;

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
/** Partie commune : réglages, programmes, volume. */
class GolemBase : public mpc::Plugin
{
public:
    GolemBase (const mpc::PluginInfo& info, const ModuleDesc& d) : mpc::Plugin (info), desc (d)
    {
        names = mpcNames (d);
        addParam (mpc::floatParam ("mpcvol", "Volume", -60.0f, 6.0f, 0.0f, "dB", 1, 1.0f));
        for (size_t i = 0; i < d.params.size(); ++i) addParam (mpcParam (d.params[i], names[i]));
        plain.assign (d.params.size(), 0.0f);
        lastNorm.assign (d.params.size(), -1.0f);
        for (size_t i = 0; i < d.params.size(); ++i) plain[i] = d.params[i].def;

        // programme 0 : réglages par défaut, puis les sons d'usine
        std::vector<float> defs (d.params.size() + 1, 0.0f);
        defs[0] = 0.0f;
        for (size_t i = 0; i < d.params.size(); ++i) defs[i + 1] = param ((int) i + 1).toPlain (param ((int) i + 1).toNormalised (d.params[i].def));
        addProgram ("Init", defs);
        for (const auto& pr : d.presets)
        {
            auto v = defs;
            for (const auto& kv : pr.values)
            {
                const int i = d.indexOf (kv.first);
                if (i >= 0) v[(size_t) i + 1] = kv.second;
            }
            addProgram (cut (fold (pr.name), 24).c_str(), v);
        }
    }

    std::string layout() const { return layoutJson (desc, names); }

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
    explicit InstrumentPlugin (const mpc::PluginInfo& info, std::unique_ptr<Module> m = std::make_unique<Module>())
        : GolemBase (info, m->desc()), module (std::move (m)) {}

    void prepare (double sampleRate, int maxBlockSize) override
    {
        pullParams();
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
    }

    void reset() override { module->allNotesOff(); module->reset(); }

    void process (mpc::ProcessContext& ctx) override
    {
        pullParams();
        const auto& tr = ctx.transport;
        if (tr.tempoValid && tr.bpm > 0.0) module->setTempo (tr.bpm);
        if (tr.valid)
        {
            if (tr.playing != wasPlaying) { if (tr.playing) module->transportStarted(); else module->transportStopped(); wasPlaying = tr.playing; }
            module->setPosition (tr.ppqValid ? tr.ppqPos : 0.0, tr.playing);
        }
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
    }

    /** Morceaux de 128 échantillons au plus : comme dans GOLEM (certains instruments, les batteries
        par exemple, ont des tampons internes de cette taille). La MPC peut envoyer des blocs de 512. */
    static constexpr int maxChunk = 128;

    void renderRange (float* L, float* R, int from, int to) noexcept
    {
        for (int p = from; p < to;)
        {
            const int m = std::min (maxChunk, to - p);
            module->render (L + p, R + p, m, plain.data());
            p += m;
        }
    }

private:
    int mapNote (int note) const noexcept
    {
        if (! desc.drumKit) return note;
        if (note >= 36 && note < 52) return note - 36;   // pads MPC (A01..A16)
        if (note >= 0 && note < 16) return note;
        return -1;
    }

    void handle (const mpc::MidiEvent& ev) noexcept
    {
        if (ev.isNoteOn())
        {
            const int k = mapNote (ev.note());
            if (k >= 0) module->noteOn (k, (float) ev.velocity() / 127.0f);
        }
        else if (ev.isNoteOff())
        {
            const int k = mapNote (ev.note());
            if (k >= 0) module->noteOff (k);
        }
        else if (ev.isAllNotesOff()) module->allNotesOff();
        else if (ev.isPitchBend()) { bend = std::clamp ((float) ev.pitchBend() / 8192.0f, -1.0f, 1.0f); module->setExpression (bend, wheel, pressure); }
        else if (ev.isAftertouch()) { pressure = (float) ev.data[1] / 127.0f; module->setExpression (bend, wheel, pressure); }
        else if (ev.isController())
        {
            if (ev.controller() == 1) { wheel = (float) ev.controllerValue() / 127.0f; module->setExpression (bend, wheel, pressure); }
            module->controller (ev.controller(), (float) ev.controllerValue() / 127.0f);
        }
    }

    std::unique_ptr<Module> module;
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
        : GolemBase (info, m->desc()), module (std::move (m)) {}

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

} // namespace grv::mpcport

/** Lu par le générateur (dlopen du greffon natif) : pages de l'interface. */
#define GOLEM_MPC_LAYOUT(PluginClass)                                                         \
    extern "C" __attribute__ ((visibility ("default"))) const char* golem_mpc_layout()        \
    {                                                                                         \
        static std::string s;                                                                 \
        if (s.empty()) { PluginClass p; s = p.layout(); }                                     \
        return s.c_str();                                                                     \
    }
