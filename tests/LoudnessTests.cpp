/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Tests des mesures du bus principal : intensité (BS.1770), crête vraie, efficace.
*/

#include "dsp/Loudness.h"
#include "engine/Catalog.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace grv::dsp;

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "\xC3\x89" "CHEC", what);
    if (! ok) ++failures;
}

static constexpr double kPi = 3.14159265358979323846;

int main()
{
    for (const double sr : { 44100.0, 48000.0, 96000.0 })
    {
        LoudnessMeter m; LoudnessReader r;
        m.prepare (sr);
        // sinus de 997 Hz à -20 dBFS sur les deux canaux : -20 LUFS (à 0,2 LU près, norme : ±0,1 dB de pondération)
        const auto amp = std::pow (10.0, -20.0 / 20.0);
        std::vector<float> L ((size_t) sr * 4), R ((size_t) sr * 4);
        for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = (float) (amp * std::sin (2.0 * kPi * 997.0 * (double) i / sr));
        for (size_t i = 0; i < L.size(); i += 256) m.process (L.data() + i, R.data() + i, (int) std::min<size_t> (256, L.size() - i));
        LoudnessBlock b;
        while (m.pop (b)) r.add (b);
        char what[160];
        std::snprintf (what, sizeof what, "%.0f Hz : 997 Hz \xC3\xA0 -20 dBFS = %.2f LUFS (instantan\xC3\xA9), %.2f (int\xC3\xA9gr\xC3\xA9)", sr, r.momentary(), r.integrated());
        check (std::abs (r.momentary() + 20.0) < 0.2 && std::abs (r.integrated() + 20.0) < 0.2, what);
        std::snprintf (what, sizeof what, "%.0f Hz : efficace %.2f dB (attendu -23,01)", sr, r.rmsDb());
        check (std::abs (r.rmsDb() + 23.01) < 0.1, what);
    }
    {
        // Crête vraie : sinus à fs/4 décalé de 45° : échantillons à 0,707 mais crête réelle à 1
        LoudnessMeter m; LoudnessReader r;
        m.prepare (48000.0);
        std::vector<float> L (48000);
        for (size_t i = 0; i < L.size(); ++i) L[i] = (float) std::sin (2.0 * kPi * 12000.0 * (double) i / 48000.0 + kPi / 4.0);
        m.process (L.data(), L.data(), (int) L.size());
        LoudnessBlock b;
        while (m.pop (b)) r.add (b);
        char what[120];
        std::snprintf (what, sizeof what, "crête vraie d'un sinus à fs/4 décalé : %.2f dBTP (échantillons à -3,01 dB)", r.truePeakDb());
        check (r.truePeakDb() > -0.6 && r.truePeakDb() < 0.3, what);
    }
    {
        // Seuil absolu : le silence ne compte pas dans l'intensité intégrée
        LoudnessMeter m; LoudnessReader r;
        m.prepare (48000.0);
        std::vector<float> L (48000 * 2, 0.0f);
        for (size_t i = 0; i < 48000; ++i) L[i] = (float) (0.1 * std::sin (2.0 * kPi * 997.0 * (double) i / 48000.0));
        m.process (L.data(), L.data(), (int) L.size());
        LoudnessBlock b;
        while (m.pop (b)) r.add (b);
        char what[120];
        std::snprintf (what, sizeof what, "silence exclu de l'int\xC3\xA9gr\xC3\xA9" "e : %.2f LUFS (attendu -20 \xC2\xB1 1)", r.integrated());
        check (std::abs (r.integrated() + 20.0) < 1.0, what); // les fenêtres de transition comptent (seuil relatif)
    }
    {
        // Limiteur en mode crête vraie : un signal poussé de 12 dB ne dépasse pas le plafond de plus de 0,5 dB
        for (const bool tp : { false, true })
        {
            auto fx = grv::engine::createEffect ("limiter");
            fx->prepare (48000.0, 32);
            const auto& d = fx->desc();
            std::vector<float> p (d.params.size());
            for (size_t i = 0; i < p.size(); ++i) p[i] = d.params[i].def;
            p[(size_t) d.indexOf ("gain")] = 12.0f;
            p[(size_t) d.indexOf ("ceiling")] = -1.0f;
            p[(size_t) d.indexOf ("truePeak")] = tp ? 1.0f : 0.0f;
            std::vector<float> L (48000), R (48000);
            for (size_t i = 0; i < L.size(); ++i)
            {
                const auto x = 0.5 * std::sin (2.0 * kPi * 11000.0 * (double) i / 48000.0 + 0.7) + 0.3 * std::sin (2.0 * kPi * 7000.0 * (double) i / 48000.0);
                L[i] = R[i] = (float) x;
            }
            for (size_t i = 0; i < L.size(); i += 32) fx->process (L.data() + i, R.data() + i, 32, p.data(), 120.0);
            LoudnessMeter m; LoudnessReader r;
            m.prepare (48000.0);
            m.process (L.data() + 4800, R.data() + 4800, (int) L.size() - 4800);
            LoudnessBlock b;
            while (m.pop (b)) r.add (b);
            char what[140];
            std::snprintf (what, sizeof what, "limiteur %s : cr\xC3\xAA" "te vraie max %.2f dBTP (plafond -1)", tp ? "en cr\xC3\xAA" "te vraie" : "classique", r.maxTruePeakDb());
            if (tp) check (r.maxTruePeakDb() < -0.5, what);
            else std::printf ("[info] %s\n", what);
        }
    }
    std::printf (failures == 0 ? "\nTOUS LES TESTS PASSENT (0 \xC3\xA9" "chec)\n" : "\nDES TESTS \xC3\x89" "CHOUENT (%d \xC3\xA9" "chec(s))\n", failures);
    return failures == 0 ? 0 : 1;
}
