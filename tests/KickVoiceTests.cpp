/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Tests du moteur de grosse caisse du test technique.
    Sans framework : un échec renvoie un code de sortie non nul (lu par CTest).
*/

#include "dsp/KickVoice.h"

#include <cmath>
#include <cstdio>
#include <vector>

using grv::dsp::KickVoice;

namespace
{
int failures = 0;

void check (bool condition, const char* what)
{
    std::printf ("[%s] %s\n", condition ? " OK " : "\xC3\x89" "CHEC", what);
    if (! condition)
        ++failures;
}

std::vector<float> render (double sampleRate, double seconds, KickVoice::Params params, double velocity = 1.0)
{
    KickVoice voice;
    voice.prepare (sampleRate);
    voice.setParams (params);
    voice.trigger (velocity);

    std::vector<float> out ((size_t) (sampleRate * seconds), 0.0f);
    constexpr int block = 64;

    for (size_t i = 0; i < out.size(); i += block)
        voice.renderAdding (out.data() + i, (int) std::min<size_t> (block, out.size() - i));

    return out;
}

double peak (const std::vector<float>& x, size_t from, size_t to)
{
    double p = 0.0;
    for (auto i = from; i < to && i < x.size(); ++i)
        p = std::max (p, (double) std::abs (x[i]));
    return p;
}

/** Instant (en secondes) où l'enveloppe crête passe sous threshold après le maximum. */
double timeBelow (const std::vector<float>& x, double sampleRate, double threshold)
{
    const size_t window = (size_t) (0.005 * sampleRate);
    for (size_t i = window; i + window < x.size(); i += window)
        if (peak (x, i, i + window) < threshold)
            return (double) i / sampleRate;
    return -1.0;
}
} // namespace

int main()
{
    KickVoice::Params p;

    // 1. Aucune valeur invalide, niveau borné
    {
        auto x = render (48000.0, 2.0, p);
        bool finite = true;
        for (auto s : x) finite = finite && std::isfinite (s);
        check (finite, "aucun NaN ni infini sur 2 s");
        check (peak (x, 0, x.size()) <= 1.5, "cr\xC3\xAAte <= 1,5 \xC3\xA0 0 dB de niveau");
        check (peak (x, 0, (size_t) (0.1 * 48000)) > 0.3, "\xC3\xA9nergie pr\xC3\xA9sente dans les 100 premi\xC3\xA8res ms");
    }

    // 2. Le son s'éteint : déclin 450 ms -> silence (< -60 dB) bien avant 2 s
    {
        auto x = render (48000.0, 2.0, p);
        check (peak (x, (size_t) (1.8 * 48000), x.size()) < 0.001, "silence (< -60 dB) apr\xC3\xA8s 1,8 s");
    }

    // 3. Déterminisme : deux rendus identiques
    {
        auto a = render (48000.0, 0.5, p);
        auto b = render (48000.0, 0.5, p);
        check (a == b, "rendu d\xC3\xA9terministe");
    }

    // 4. Indépendance vis-à-vis de la fréquence d'échantillonnage (temps de déclin à -20 dB)
    {
        auto x44 = render (44100.0, 2.0, p);
        auto x96 = render (96000.0, 2.0, p);
        const auto t44 = timeBelow (x44, 44100.0, 0.1);
        const auto t96 = timeBelow (x96, 96000.0, 0.1);
        check (t44 > 0.0 && t96 > 0.0 && std::abs (t44 - t96) <= 0.05 * t44 + 0.006,
               "temps de d\xC3\xA9" "clin identique \xC3\xA0 44,1 kHz et 96 kHz (\xC2\xB1" "5 %)");
    }

    // 5. Redéclenchement sans discontinuité (transitoire coupé pour isoler le corps)
    {
        auto q = p;
        q.click = 0.0;
        KickVoice voice;
        voice.prepare (48000.0);
        voice.setParams (q);
        voice.trigger (1.0);

        std::vector<float> x (48000, 0.0f);
        voice.renderAdding (x.data(), 12000);       // 250 ms de son
        voice.trigger (1.0);                        // redéclenchement pendant que ça sonne
        voice.renderAdding (x.data() + 12000, 36000);

        double maxStep = 0.0;
        for (size_t i = 11990; i < 12010; ++i)
            maxStep = std::max (maxStep, (double) std::abs (x[i + 1] - x[i]));

        check (maxStep < 0.25, "pas de clic au red\xC3\xA9" "clenchement (\xC3\xA9" "cart entre \xC3\xA9" "chantillons < 0,25)");
    }

    // 6. La vélocité change le niveau
    {
        auto loud = render (48000.0, 0.2, p, 1.0);
        auto soft = render (48000.0, 0.2, p, 0.2);
        check (peak (soft, 0, soft.size()) < peak (loud, 0, loud.size()), "v\xC3\xA9locit\xC3\xA9 faible = plus doux");
    }

    std::printf ("\n%s (%d \xC3\xA9" "chec(s))\n", failures == 0 ? "TOUS LES TESTS PASSENT" : "DES TESTS \xC3\x89" "CHOUENT", failures);
    return failures == 0 ? 0 : 1;
}
