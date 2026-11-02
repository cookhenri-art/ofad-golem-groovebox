/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Tests des voix du kit de démarrage et du lecteur d'échantillon.
    Sans framework : un échec renvoie un code de sortie non nul (lu par CTest).
*/

#include "dsp/AnalogFilter.h"
#include "dsp/DrumVoices.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace grv::dsp;

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::printf ("[%s] %s\n", condition ? " OK " : "\xC3\x89" "CHEC", what.c_str());
    if (! condition) ++failures;
}

std::vector<float> render (Voice& v, double sr, double seconds, const VoiceParams& p, double velocity = 1.0)
{
    v.prepare (sr);
    v.setParams (p);
    v.trigger (velocity);
    std::vector<float> out ((size_t) (sr * seconds), 0.0f);
    for (size_t i = 0; i < out.size(); i += 64)
        v.renderAdding (out.data() + i, (int) std::min<size_t> (64, out.size() - i));
    return out;
}

struct Stats { bool finite = true; double peak = 0.0, energy = 0.0; };

Stats stats (const std::vector<float>& x)
{
    Stats s;
    for (auto v : x)
    {
        if (! std::isfinite (v)) s.finite = false;
        s.peak = std::max (s.peak, (double) std::abs (v));
        s.energy += (double) v * v;
    }
    return s;
}
} // namespace

int main()
{
    constexpr double sr = 48000.0;

    struct Case { const char* name; std::unique_ptr<Voice> voice; VoiceParams p; };
    std::vector<Case> cases;
    cases.push_back ({ "grosse caisse", std::make_unique<KickDrum>(), { 0, 450, 0.5, 0.35, 6 } });
    cases.push_back ({ "caisse claire", std::make_unique<SnareDrum>(), { 0, 220, 0.55, 0.6, 3 } });
    cases.push_back ({ "clap", std::make_unique<ClapDrum>(), { 0, 300, 0.5, 0.5, 0 } });
    cases.push_back ({ "rimshot", std::make_unique<RimDrum>(), { 0, 60, 0.5, 0.6, 0 } });
    cases.push_back ({ "charley", std::make_unique<HatDrum>(), { 0, 480, 0.6, 0.7, 0 } });
    cases.push_back ({ "tom", std::make_unique<TomDrum>(), { 0, 420, 0.4, 0.2, 0 } });
    cases.push_back ({ "cloche", std::make_unique<CowbellDrum>(), { 0, 260, 0.5, 0.5, 0 } });
    cases.push_back ({ "grosse caisse en T pont\xC3\xA9", std::make_unique<BridgedTDrum> (48.0, 6.0, 0.0), { 0, 900, 0.4, 0.15, 2, 0.35, 0.1 } });
    cases.push_back ({ "conga", std::make_unique<TomDrum> (330.0, 2.0), { 0, 200, 0.2, 0.05, 0, 0.3, 0.0 } });

    for (auto& c : cases)
    {
        const auto x = render (*c.voice, sr, 2.0, c.p);
        const auto s = stats (x);
        check (s.finite, std::string (c.name) + " : aucune valeur invalide");
        check (s.peak > 0.05 && s.peak <= 2.5, std::string (c.name) + " : niveau cr\xC3\xAAte raisonnable (" + std::to_string (s.peak) + ")");
        check (! c.voice->isActive(), std::string (c.name) + " : s'\xC3\xA9teint seule");

        // Pousser tous les réglages aux extrêmes ne doit rien casser.
        const auto y = render (*c.voice, sr, 1.0, { 24, 10, 1.0, 1.0, 24, 1.0, 1.0 });
        const auto z = render (*c.voice, sr, 1.0, { -24, 4000, 0.0, 0.0, 0, 0.0, 0.0 });
        check (stats (y).finite && stats (z).finite && stats (y).peak < 4.0 && stats (z).peak < 4.0, std::string (c.name) + " : stable aux extr\xC3\xAAmes");

        // Plus fort avec plus de vélocité
        const auto soft = stats (render (*c.voice, sr, 0.5, c.p, 0.2)).energy;
        const auto hard = stats (render (*c.voice, sr, 0.5, c.p, 1.0)).energy;
        check (hard > soft * 1.5, std::string (c.name) + " : la v\xC3\xA9locit\xC3\xA9 agit");
    }

    // Étouffement du charley
    {
        HatDrum hat;
        hat.prepare (sr);
        hat.setParams ({ 0, 2000, 0.6, 0.7, 0 });
        hat.trigger (1.0);
        std::vector<float> buf (4800, 0.0f);
        hat.renderAdding (buf.data(), 4800);
        hat.choke();
        std::vector<float> after (4800, 0.0f);
        hat.renderAdding (after.data(), 4800);
        check (! hat.isActive(), "charley ouvert \xC3\xA9touff\xC3\xA9 en moins de 0,1 s");
    }

    // Lecteur d'échantillon : vitesse 1 = reproduction fidèle, +12 demi-tons = deux fois plus court.
    {
        std::vector<float> ramp (4800);
        for (size_t i = 0; i < ramp.size(); ++i) ramp[i] = (float) std::sin (0.01 * (double) i) * 0.5f;

        SampleVoice v;
        v.prepare (sr);
        v.setSample (ramp.data(), (int) ramp.size(), sr);
        v.setParams ({ 0, 4000, 1.0, 0.0, 0, 1.0, 0.0 }); // fin = 100 %, sans attaque
        v.trigger (1.0);
        std::vector<float> out (6000, 0.0f);
        v.renderAdding (out.data(), 6000);
        // La saturation à 0 dB arrondit un peu les crêtes : on compare au signal saturé attendu.
        double err2 = 0.0;
        for (size_t i = 1; i < 4000; ++i)
        {
            const auto expected = softClip ((double) ramp[i]) / softClip (1.0);
            err2 = std::max (err2, std::abs ((double) out[i] - expected));
        }
        check (err2 < 0.02, "\xC3\xA9" "chantillon relu fid\xC3\xA8lement \xC3\xA0 vitesse 1 (\xC3\xA9" "cart " + std::to_string (err2) + ")");

        SampleVoice up;
        up.prepare (sr);
        up.setSample (ramp.data(), (int) ramp.size(), sr);
        up.setParams ({ 12, 4000, 1.0, 0.0, 0, 1.0, 0.0 });
        up.trigger (1.0);
        std::vector<float> o2 (6000, 0.0f);
        up.renderAdding (o2.data(), 6000);
        size_t last = 0;
        for (size_t i = 0; i < o2.size(); ++i) if (std::abs (o2[i]) > 1e-6) last = i;
        check (last > 2300 && last < 2450, "+12 demi-tons : lecture deux fois plus rapide");

        SampleVoice empty;
        empty.prepare (sr);
        empty.trigger (1.0);
        check (! empty.isActive(), "pad sans \xC3\xA9" "chantillon : silencieux");
    }

    // T ponté : la hauteur vient du résonateur, il s'éteint seul, redéclencher ne fait pas de saut
    {
        BridgedTDrum t (200.0, 0.0, 0.0);
        t.prepare (sr);
        t.setParams ({ 0, 400, 0.0, 0.0, 0, 0.5, 0.0 });
        t.trigger (1.0);
        std::vector<float> a (9600, 0.0f);
        for (size_t i = 0; i < a.size(); i += 64) t.renderAdding (a.data() + i, 64);
        int crossings = 0;
        for (size_t i = 2400; i < 7200; ++i) if (a[i - 1] < 0.0f && a[i] >= 0.0f) ++crossings; // 0,1 s
        check (crossings >= 19 && crossings <= 21, "T pont\xC3\xA9 accord\xC3\xA9 \xC3\xA0 200 Hz (" + std::to_string (crossings * 10) + " Hz mesur\xC3\xA9s)");
        // redéclenchement pendant qu'il sonne : l'impulsion s'ajoute, pas de discontinuité
        std::vector<float> b (256, 0.0f);
        t.renderAdding (b.data(), 128);
        t.trigger (1.0);
        t.renderAdding (b.data() + 128, 128);
        double jump = 0.0;
        for (size_t i = 1; i < b.size(); ++i) jump = std::max (jump, std::abs ((double) b[i] - b[i - 1]));
        check (jump < 0.2, "T pont\xC3\xA9 redéclenché sans saut (" + std::to_string (jump) + ")");
    }

    // Dérive analogique : deux coups identiques diffèrent un peu, jamais beaucoup
    {
        KickDrum k;
        const VoiceParams p { 0, 450, 0.5, 0.35, 6, 0.5, 0.0 };
        const auto a = render (k, sr, 0.3, p);
        k.setParams (p); k.trigger (1.0);
        std::vector<float> b (a.size(), 0.0f);
        for (size_t i = 0; i < b.size(); i += 64) k.renderAdding (b.data() + i, (int) std::min<size_t> (64, b.size() - i));
        // la phase n'est pas remise à zéro (circuit réexcité) : on compare les énergies
        double ea = 0.0, eb = 0.0;
        for (size_t i = 0; i < a.size(); ++i) { ea += (double) a[i] * a[i]; eb += (double) b[i] * b[i]; }
        const auto rel = std::abs (eb / std::max (1e-12, ea) - 1.0);
        check (rel > 1.0e-5 && rel < 0.15, "d\xC3\xA9rive : deux coups proches mais pas identiques (\xC3\xA9" "cart d'\xC3\xA9nergie " + std::to_string (rel * 100.0) + " %)");
    }

    // Filtre analogique du pad
    {
        auto gainAt = [&] (int mode, double hz, double cutoff, double reso)
        {
            AnalogFilter f;
            f.prepare (sr);
            double e = 0.0;
            for (int i = 0; i < 24000; ++i)
            {
                const auto y = f.process (0.25 * std::sin (kTwoPi * hz * i / sr), mode, cutoff, reso);
                if (i >= 12000) e += y * y;
            }
            return std::sqrt (e / 12000.0) / (0.25 / std::sqrt (2.0));
        };
        check (gainAt (AnalogFilter::lowPass24, 100.0, 1000.0, 0.0) > 0.85 && gainAt (AnalogFilter::lowPass24, 8000.0, 1000.0, 0.0) < 0.001,
               "\xC3\xA9" "chelle passe-bas 24 dB : laisse les graves, coupe 3 octaves au-dessus");
        check (gainAt (AnalogFilter::highPass24, 8000.0, 1000.0, 0.0) > 0.85 && gainAt (AnalogFilter::highPass24, 100.0, 1000.0, 0.0) < 0.01,
               "passe-haut 24 dB : laisse les aigus, coupe les graves");
        check (gainAt (AnalogFilter::lowPass12, 8000.0, 1000.0, 0.0) < 0.03, "passe-bas 12 dB : coupe les aigus");
        check (gainAt (AnalogFilter::bandPass, 1000.0, 1000.0, 0.5) > 0.7 && gainAt (AnalogFilter::bandPass, 100.0, 1000.0, 0.5) < 0.2, "passe-bande centr\xC3\xA9");
        check (gainAt (AnalogFilter::notch, 1000.0, 1000.0, 0.0) < 0.1 && gainAt (AnalogFilter::notch, 100.0, 1000.0, 0.0) > 0.8, "r\xC3\xA9jection centr\xC3\xA9" "e");
        check (gainAt (AnalogFilter::lowPass24, 1000.0, 1000.0, 0.9) > gainAt (AnalogFilter::lowPass24, 1000.0, 1000.0, 0.0) * 2.0, "r\xC3\xA9sonance : pic \xC3\xA0 la coupure");
        // stabilité : bruit plein niveau, résonance au maximum, coupure qui balaie à chaque échantillon
        bool finite = true; double peak = 0.0;
        for (int mode = 1; mode < AnalogFilter::numModes; ++mode)
        {
            AnalogFilter f; f.prepare (sr); WhiteNoise nz;
            for (int i = 0; i < 48000; ++i)
            {
                const auto y = f.process (nz.next(), mode, 20.0 * std::pow (1000.0, 0.5 + 0.5 * std::sin (i * 0.001)), 1.0);
                finite = finite && std::isfinite (y); peak = std::max (peak, std::abs (y));
            }
        }
        check (finite && peak < 20.0, "filtre stable \xC3\xA0 r\xC3\xA9sonance maximale et coupure balay\xC3\xA9" "e (cr\xC3\xAAte " + std::to_string (peak) + ")");
    }

    std::printf ("\n%s (%d \xC3\xA9" "chec%s)\n", failures == 0 ? "TOUS LES TESTS PASSENT" : "DES TESTS \xC3\x89" "CHOUENT", failures, failures > 1 ? "s" : "");
    return failures == 0 ? 0 : 1;
}
