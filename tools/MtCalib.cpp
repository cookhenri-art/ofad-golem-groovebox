/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Calibrage des kits d'usine de Micro Tonnerre (après la 1.1) : joue chaque canal de chaque kit
    seul, à pleine vélocité, et écrit sa crête (dB) dans un fichier JSON, lu par
      python3 tools/presets/kits_microton.py --calibrate <fichier>
    Contrôle aussi qu'aucun son ne reste muet, ne donne de valeur invalide ni ne sonne sans fin.
    Outil de développement (hors du projet CMake) :
      g++ -std=c++20 -O2 -Isrc tools/MtCalib.cpp -o mtcalib && ./mtcalib mesures.json
*/

#include "instruments/MicroTonnerre.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace grv;

int main (int argc, char** argv)
{
    const auto& d = instruments::mt::desc();
    const double sr = 48000.0;
    const char* out = argc > 1 ? argv[1] : "mt_mesures.json";
    std::FILE* f = std::fopen (out, "w");
    if (f == nullptr) return 1;
    std::fprintf (f, "{\n");
    int problems = 0;
    bool firstKit = true;
    for (const auto& pr : d.presets)
    {
        if (pr.name == "Init") continue;
        std::vector<float> p (d.params.size());
        for (size_t i = 0; i < p.size(); ++i) p[i] = d.params[i].def;
        for (const auto& [id, value] : pr.values) if (const auto i = d.indexOf (id); i >= 0) p[(size_t) i] = value;
        std::fprintf (f, "%s \"%s\": [", firstKit ? "" : ",\n", pr.name.c_str());
        firstKit = false;
        for (int c = 0; c < instruments::mt::numChannels; ++c)
        {
            instruments::MicroTonnerre inst;
            inst.prepare (sr, 32);
            const int len = (int) (sr * 12.0);
            std::vector<float> L ((size_t) len), R ((size_t) len);
            inst.noteOn (c, 1.0f);
            for (int i = 0; i < len; i += 32) inst.render (L.data() + i, R.data() + i, 32, p.data());
            double pk = 0.0, tail = 0.0;
            bool bad = false;
            for (int i = 0; i < len; ++i)
            {
                if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) bad = true;
                pk = std::max ({ pk, (double) std::abs (L[(size_t) i]), (double) std::abs (R[(size_t) i]) });
                if (i > len - (int) sr) tail = std::max (tail, (double) std::abs (L[(size_t) i]));
            }
            const auto db = 20.0 * std::log10 (pk + 1.0e-12);
            if (bad || db < -60.0 || tail > 1.0e-4)
            {
                ++problems;
                std::printf ("PROBLEME %s canal %d : crete %.1f dB, fin %.2g%s\n", pr.name.c_str(), c + 1, db, tail, bad ? ", valeur invalide" : "");
            }
            std::fprintf (f, "%s%.2f", c ? ", " : "", db);
        }
        std::fprintf (f, "]");
    }
    std::fprintf (f, "\n}\n");
    std::fclose (f);
    std::printf ("%d kits mesures, %d probleme(s)\n", (int) d.presets.size() - 1, problems);
    return problems == 0 ? 0 : 2;
}
