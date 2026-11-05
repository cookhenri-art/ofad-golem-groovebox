/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Contrôle des préréglages d'usine des synthés : chaque son est joué hors temps réel (phrase choisie
    selon sa catégorie) et mesuré.
      - chaque identifiant de réglage existe et chaque valeur est dans sa plage ;
      - aucun nom en double dans un même instrument ;
      - le son sort (pas de silence), sans valeur invalide ;
      - intensité perçue maximale (LUFS instantané, fenêtres de 400 ms) et crête vraie ;
      - charge de calcul (temps de rendu / durée jouée).
    Usage : groovebox_presetcheck [levels.json]
      Avec un fichier, écrit la correction de niveau (dB) de chaque son des banques pour atteindre la
      cible de sa catégorie ; tools/presets/gen.py la reporte dans le réglage de volume.
    Code de retour : 0 si tout va bien, 1 sinon.
*/

#include "engine/Engine.h"
#include "engine/Catalog.h"
#include "dsp/Loudness.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace grv::engine;

namespace
{
struct Note { double on, off; int note; float vel; };

/** Phrase jouée selon la catégorie (en secondes). */
std::vector<Note> phraseFor (const std::string& cat, double& length)
{
    std::vector<Note> v;
    if (cat == "Acide" || cat == "Basses")
    {
        const int seq[] { 36, 36, 48, 36, 39, 36, 43, 46 };
        for (int k = 0; k < 16; ++k) v.push_back ({ k * 0.24, k * 0.24 + 0.18, seq[k % 8], (k % 4) == 0 ? 1.0f : 0.75f });
        length = 16 * 0.24 + 1.0;
    }
    else if (cat == "Leads")
    {
        const int seq[] { 60, 63, 67, 70, 72, 70, 67, 63 };
        for (int k = 0; k < 8; ++k) v.push_back ({ k * 0.4, k * 0.4 + 0.38, seq[k], 0.85f });
        length = 8 * 0.4 + 1.0;
    }
    else if (cat == "Plucks" || cat == "Claviers")
    {
        for (int k = 0; k < 4; ++k)
            for (int n : { 48, 55, 60, 64 }) v.push_back ({ k * 0.6, k * 0.6 + 0.4, n, 0.8f });
        length = 4 * 0.6 + 1.5;
    }
    else if (cat == "Arpèges" || cat == "Arp\xC3\xA8ges")
    {
        for (int n : { 48, 52, 55 }) v.push_back ({ 0.0, 3.0, n, 0.8f });
        length = 4.0;
    }
    else if (cat == "Effets")
    {
        v.push_back ({ 0.0, 3.0, 60, 0.85f });
        length = 4.5;
    }
    else // Nappes, Cordes, Cuivres, Classiques
    {
        for (int n : { 48, 55, 60, 64 }) v.push_back ({ 0.0, 3.5, n, 0.8f });
        length = 5.5;
    }
    return v;
}

/** Cible d'intensité instantanée maximale (LUFS) selon la catégorie. */
double targetFor (const std::string& cat)
{
    // des sons francs mais avec de la marge pour un mélange à 16 pistes (le maître a son limiteur)
    if (cat == "Nappes" || cat == "Cordes" || cat == "Effets") return -18.0;
    return -16.0;
}

struct Result { std::string type, name, cat; double lufs = -100.0, peakDb = -100.0, load = 0.0, tailDb = -200.0; bool finite = true; };

Result render (const std::string& type, const Preset& pr)
{
    Result r { type, pr.name, pr.category };
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    Engine e;
    e.prepare (sr, block);
    e.setInstrument (0, type, 0.0);
    e.applyPreset (0, -1, pr.name);
    double length = 0.0;
    auto notes = phraseFor (pr.category, length);
    grv::dsp::LoudnessMeter meter;
    meter.prepare (sr);
    grv::dsp::LoudnessReader reader;
    std::vector<float> L (block), R (block);
    const auto blocks = (int) (length * sr / block);
    std::vector<bool> on (notes.size()), off (notes.size());
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        const auto t = b * block / sr;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            if (! on[i] && t >= notes[i].on) { e.pushEvent ({ InputEvent::noteOn, 0, notes[i].note, 0, notes[i].vel, false }); on[i] = true; }
            if (! off[i] && t >= notes[i].off) { e.pushEvent ({ InputEvent::noteOff, 0, notes[i].note, 0, 0.0f, false }); off[i] = true; }
        }
        e.process (nullptr, nullptr, L.data(), R.data(), block, {}, nullptr, 0);
        for (int i = 0; i < block; ++i)
            if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) { r.finite = false; L[(size_t) i] = R[(size_t) i] = 0.0f; }
        meter.process (L.data(), R.data(), block);
        grv::dsp::LoudnessBlock lb;
        while (meter.pop (lb)) { reader.add (lb); r.lufs = std::max (r.lufs, reader.momentary()); }
    }
    const auto secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    r.load = secs / length;
    // toutes les notes relâchées : le son doit s'éteindre (relâchement de 12 s au plus, plus les effets internes)
    const auto tailBlocks = (int) (16.0 * sr / block), lastBlocks = (int) (1.0 * sr / block);
    double tail = 0.0;
    for (int b = 0; b < tailBlocks; ++b)
    {
        e.process (nullptr, nullptr, L.data(), R.data(), block, {}, nullptr, 0);
        if (b >= tailBlocks - lastBlocks)
            for (int i = 0; i < block; ++i) tail += (double) L[(size_t) i] * L[(size_t) i] + (double) R[(size_t) i] * R[(size_t) i];
    }
    r.tailDb = 10.0 * std::log10 (tail / (2.0 * lastBlocks * block) + 1.0e-30);
    r.peakDb = reader.maxTruePeakDb();
    return r;
}
} // namespace

int main (int argc, char* argv[])
{
    const std::vector<std::string> types { "acid", "mono", "poly5", "polyj", "hybrid", "morph", "pads", "ms20", "sh101", "blo", "ob6", "orbit", "fm6", "fm4", "fmx", "muse", "hots", "lyra", "bitm", "dfam", "voc16", "vocstr", "vocspec", "vocsyn", "gluta", "gooddoc" };
    int problems = 0;
    std::map<std::string, std::map<std::string, double>> corrections;
    double worstLoad = 0.0;
    // second argument facultatif : un seul instrument (mise au point)
    const std::string only = argc > 2 ? argv[2] : "";
    for (const auto& type : types)
    {
        if (! only.empty() && type != only) continue;
        Engine probe;
        probe.prepare (48000.0, 256);
        probe.setInstrument (0, type, 0.0);
        const auto* d = probe.instrumentDesc (0);
        if (d == nullptr) { std::printf ("ECHEC : instrument %s introuvable\n", type.c_str()); ++problems; continue; }
        std::set<std::string> names;
        std::map<std::string, int> perCat;
        for (const auto& pr : d->presets)
        {
            if (! names.insert (pr.name).second) { std::printf ("ECHEC %s : nom en double « %s »\n", type.c_str(), pr.name.c_str()); ++problems; }
            for (const auto& [id, v] : pr.values)
            {
                const auto i = d->indexOf (id);
                if (i < 0) { std::printf ("ECHEC %s / %s : réglage inconnu « %s »\n", type.c_str(), pr.name.c_str(), id.c_str()); ++problems; continue; }
                const auto& p = d->params[(size_t) i];
                if (v < p.min - 1.0e-4f || v > p.max + 1.0e-4f)
                { std::printf ("ECHEC %s / %s : %s = %g hors plage [%g, %g]\n", type.c_str(), pr.name.c_str(), id.c_str(), v, p.min, p.max); ++problems; }
            }
            ++perCat[pr.category.empty() ? "Classiques" : pr.category];
            const auto r = render (type, pr);
            worstLoad = std::max (worstLoad, r.load);
            const bool silent = r.lufs < -45.0;
            const bool hot = r.peakDb > -0.5;  // signalé, corrigé par la mise à niveau
            const bool ringing = r.tailDb > -70.0; // le son continue 16 s après le relâchement des notes
            std::printf ("%-7s %-12s %-34s %6.1f LUFS  crête %6.1f dBTP  charge %5.1f %%%s%s%s\n", type.c_str(), r.cat.c_str(), r.name.c_str(),
                         r.lufs, r.peakDb, r.load * 100.0, r.finite ? "" : "  VALEUR INVALIDE", silent ? "  SILENCE" : "", hot ? "  CRÊTE" : "");
            if (ringing) std::printf ("ECHEC %s / %s : le son ne s'\xC3\xA9teint pas (%.1f dB 16 s apr\xC3\xA8s les notes)\n", type.c_str(), pr.name.c_str(), r.tailDb);
            if (! r.finite || silent || ringing) ++problems;
            // vers la cible de la catégorie, sans dépasser -1 dBTP de crête vraie
            if (! pr.category.empty() && ! silent) corrections[type][pr.name] = std::min (targetFor (pr.category) - r.lufs, -1.0 - r.peakDb);
        }
        std::printf ("  %s : %zu sons", type.c_str(), d->presets.size());
        for (const auto& [c, n] : perCat) std::printf (" · %s %d", c.c_str(), n);
        std::printf ("\n");
    }
    std::printf ("Charge la plus forte : %.1f %% d'un cœur (une piste)\n", worstLoad * 100.0);

    if (argc > 1)
    {
        // corrections cumulées : la valeur écrite s'ajoute à celle déjà appliquée par gen.py
        std::ofstream f (argv[1]);
        f << "{\n";
        bool firstType = true;
        for (const auto& [type, m] : corrections)
        {
            f << (firstType ? "" : ",\n") << "  \"" << type << "\": {";
            firstType = false;
            bool first = true;
            for (const auto& [name, db] : m)
            {
                char buf[32];
                std::snprintf (buf, sizeof buf, "%.2f", db);
                f << (first ? "\n" : ",\n") << "    \"";
                for (char c : name) { if (c == '"' || c == '\\') f << '\\'; f << c; }
                f << "\": " << buf;
                first = false;
            }
            f << "\n  }";
        }
        f << "\n}\n";
    }
    std::printf (problems == 0 ? "TOUS LES SONS SONT BONS\n" : "%d PROBL\xC3\x88MES\n", problems);
    return problems == 0 ? 0 : 1;
}
