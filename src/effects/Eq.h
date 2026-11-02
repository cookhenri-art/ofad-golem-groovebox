/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Égaliseur » — égaliseur paramétrique à 8 bandes, chacune statique ou dynamique (le gain de
    la bande varie avec le niveau du signal dans la bande : dé-esseur, grave qui se resserre sur
    les coups de grosse caisse…). Cloches, plateaux, coupe-bas et coupe-haut 12/24 dB, coupe-bande.
    Coupe-bas et coupe-haut jusqu'à 48 dB/octave (Butterworth). Traitement stéréo, mid, side ou
    mid/side (bandes 1 à 4 sur le mid, 5 à 8 sur le side). Analyseur de spectre de la sortie.
    v0.36 (principe de la dynamique spectrale des égaliseurs modernes, sans transformée de Fourier ni
    latence) : attaque et relâchement réglables ; en dynamique « spectrale », chaque cloche dynamique est découpée en 8 sous-bandes
    à détecteur propre, qui ne réduisent (ou ne relèvent) que les fréquences qui dépassent le seuil.
    Filtres à variables d'état (modulables sans clic), aucune latence.
*/

#pragma once

#include "../dsp/EqFilters.h"
#include "../engine/Module.h"

#include <array>
#include <atomic>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace eq
{
    constexpr int numBands = 8;
    constexpr int bandParams = 7;
    enum BandSlot { bOn = 0, bType, bFreq, bGain, bQ, bRange, bThreshold };
    enum Type { bell = 0, lowShelf, highShelf, lowCut12, lowCut24, highCut12, highCut24, notch,
                lowCut36, lowCut48, highCut36, highCut48 };
    enum GlobalSlot { gOutput = numBands * bandParams, gChannels, numSlots };
    // v0.36 : attaque et relâchement de la dynamique, mode spectral (réglages ajoutés après « channels »,
    // communs aux 8 bandes : 64 réglages au plus par effet ; les anciens projets gardent 4 ms / 80 ms et la
    // dynamique simple)
    enum ExtraSlot { xAttack = numSlots, xRelease, xSpectral, numSlotsAll };
    constexpr int numSub = 8; // sous-bandes du mode spectral
    enum Channels { stereo = 0, midOnly, sideOnly, midSide };

    inline std::string bandId (int b, const char* name) { return "b" + std::to_string (b + 1) + "_" + name; }

    inline const std::vector<std::string>& typeNames()
    {
        static const std::vector<std::string> t { "Cloche", "Plateau bas", "Plateau haut", "Coupe-bas 12", "Coupe-bas 24",
                                                  "Coupe-haut 12", "Coupe-haut 24", "Coupe-bande",
                                                  "Coupe-bas 36", "Coupe-bas 48", "Coupe-haut 36", "Coupe-haut 48" };
        return t;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "eq"; m.name = "Prism"; m.role = "\xC3\x89galiseur 8 bandes"; m.family = "\xC3\x89galiseurs"; m.form = "module";
            m.principle = "Param\xC3\xA9trique 8 bandes statiques, dynamiques ou \xC3\xA0 dynamique spectrale (attaque, rel\xC3\xA2" "chement), sans latence";
            static constexpr int defType[numBands] { lowCut12, lowShelf, bell, bell, bell, bell, highShelf, highCut12 };
            static constexpr float defFreq[numBands] { 30, 100, 250, 800, 2500, 5000, 10000, 20000 };
            static constexpr int defOn[numBands] { 0, 1, 1, 1, 1, 1, 1, 0 };
            for (int b = 0; b < numBands; ++b)
            {
                m.params.push_back (C (bandId (b, "on"), "Bande " + std::to_string (b + 1), { "Arr\xC3\xAAt", "Marche" }, defOn[b]));
                m.params.push_back (C (bandId (b, "type"), "Type", typeNames(), defType[b]));
                m.params.push_back (P (bandId (b, "freq"), "Fr\xC3\xA9quence", "Hz", 20, 20000, defFreq[b], 0.3f));
                m.params.push_back (P (bandId (b, "gain"), "Gain", "dB", -24, 24, 0));
                m.params.push_back (P (bandId (b, "q"), "Largeur (Q)", "", 0.1f, 18, 0.9f, 0.4f));
                m.params.push_back (P (bandId (b, "range"), "Dynamique", "dB", -24, 24, 0));
                m.params.push_back (P (bandId (b, "thr"), "Seuil", "dB", -60, 0, -24));
                SectionDesc s;
                s.id = "band" + std::to_string (b + 1);
                s.name = "Bande " + std::to_string (b + 1);
                s.visual = "eq";
                s.tab = "bands";
                for (auto* id : { "on", "type", "freq", "gain", "q", "range", "thr" }) s.params.push_back (bandId (b, id));
                m.sections.push_back (s);
            }
            m.params.push_back (P ("output", "Sortie", "dB", -24, 24, 0));
            m.params.push_back (C ("channels", "Canaux", { "St\xC3\xA9r\xC3\xA9o", "Mid seul", "Side seul", "Mid / Side (1-4 mid, 5-8 side)" }));
            m.sections.push_back ({ "output", "Sortie", "eq", { "output", "channels" }, "bands" });
            m.params.push_back (P ("dynatt", "Attaque", "ms", 0.5f, 200, 4, 0.4f));
            m.params.push_back (P ("dynrel", "Rel\xC3\xA2" "chement", "ms", 5, 2000, 80, 0.4f));
            m.params.push_back (C ("spectral", "Dynamique", { "Simple", "Spectrale" }));
            m.sections.push_back ({ "dynamics", "Dynamique", "eq", { "spectral", "dynatt", "dynrel" }, "bands" });

            auto band = [] (int b, std::vector<std::pair<std::string, float>> v)
            {
                std::vector<std::pair<std::string, float>> out;
                for (auto& [k, x] : v) out.push_back ({ bandId (b, k.c_str()), x });
                return out;
            };
            auto join = [] (std::initializer_list<std::vector<std::pair<std::string, float>>> parts)
            {
                std::vector<std::pair<std::string, float>> out;
                for (auto& p : parts) out.insert (out.end(), p.begin(), p.end());
                return out;
            };
            m.presets = {
                { "Neutre", {} },
                { "Voix", join ({ band (0, { { "on", 1 }, { "freq", 90 } }), band (2, { { "freq", 300 }, { "gain", -3 }, { "q", 1.2f } }),
                                  band (4, { { "freq", 3000 }, { "gain", 2.5f }, { "q", 0.8f } }),
                                  band (5, { { "freq", 6500 }, { "q", 3 }, { "range", -8 }, { "thr", -30 } }), // dé-esseur
                                  band (6, { { "freq", 12000 }, { "gain", 2 } }) }) },
                { "Grosse caisse", join ({ band (0, { { "on", 1 }, { "freq", 35 } }), band (1, { { "freq", 60 }, { "gain", 3 } }),
                                           band (2, { { "freq", 320 }, { "gain", -5 }, { "q", 1.5f } }), band (4, { { "freq", 3500 }, { "gain", 3 }, { "q", 1.2f } }) }) },
                { "Caisse claire", join ({ band (0, { { "on", 1 }, { "freq", 80 } }), band (2, { { "freq", 200 }, { "gain", 2 } }),
                                           band (3, { { "freq", 900 }, { "gain", -3 }, { "q", 1.4f } }), band (5, { { "freq", 5000 }, { "gain", 3 } }) }) },
                { "Basse", join ({ band (0, { { "on", 1 }, { "freq", 30 } }), band (1, { { "freq", 80 }, { "gain", 2 } }),
                                   band (2, { { "freq", 250 }, { "gain", -2.5f } }), band (4, { { "freq", 1500 }, { "gain", 2 }, { "q", 1 } }) }) },
                { "Guitare", join ({ band (0, { { "on", 1 }, { "freq", 100 } }), band (3, { { "freq", 600 }, { "gain", -2 } }),
                                     band (4, { { "freq", 2800 }, { "gain", 2 } }), band (7, { { "on", 1 }, { "freq", 12000 } }) }) },
                { "Piano", join ({ band (0, { { "on", 1 }, { "freq", 40 } }), band (2, { { "freq", 350 }, { "gain", -2 } }), band (6, { { "freq", 9000 }, { "gain", 2 } }) }) },
                { "Nappe (place pour la voix)", join ({ band (0, { { "on", 1 }, { "freq", 180 } }), band (4, { { "freq", 2500 }, { "gain", -3 }, { "q", 0.7f } }) }) },
                { "Bus batterie", join ({ band (1, { { "freq", 70 }, { "gain", 2 } }), band (3, { { "freq", 450 }, { "gain", -2 } }), band (6, { { "freq", 10000 }, { "gain", 2 } }) }) },
                { "Master", join ({ band (0, { { "on", 1 }, { "freq", 25 } }), band (1, { { "freq", 90 }, { "gain", 1 } }), band (6, { { "freq", 12000 }, { "gain", 1.5f } }) }) },
                { "D\xC3\xA9-esseur spectral", join ({ band (5, { { "freq", 7000 }, { "q", 1.2f }, { "range", -10 }, { "thr", -34 }, }) , { { "spectral", 1 }, { "dynatt", 1 }, { "dynrel", 60 } } }) },
                { "R\xC3\xA9sonances de voix", join ({ band (3, { { "freq", 1200 }, { "q", 0.6f }, { "range", -6 }, { "thr", -30 } }),
                                                       band (4, { { "freq", 3200 }, { "q", 0.8f }, { "range", -5 }, { "thr", -32 } }), { { "spectral", 1 }, { "dynatt", 5 }, { "dynrel", 120 } } }) },
                { "Bus (graves tenus)", join ({ band (1, { { "freq", 90 }, { "range", -4 }, { "thr", -20 } }), { { "dynatt", 20 }, { "dynrel", 250 } } }) },
                { "T\xC3\xA9l\xC3\xA9phone", join ({ band (0, { { "on", 1 }, { "type", lowCut24 }, { "freq", 400 } }), band (7, { { "on", 1 }, { "type", highCut24 }, { "freq", 3200 } }),
                                         band (4, { { "freq", 1500 }, { "gain", 6 } }) }) },
            };
            return m;
        }();
        return d;
    }
}

class Equalizer final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return eq::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override
    {
        for (auto& b : bands)
        {
            b.f1.reset(); b.f2.reset(); b.f3.reset(); b.f4.reset(); b.sc.reset(); b.env = 0.0;
            for (auto& f : b.sub) f.reset();
            for (auto& f : b.subSc) f.reset();
            for (auto& f : b.subSc2) f.reset();
            b.subEnv.fill (0.0);
        }
    }
    float meter (int i) const noexcept override { return i >= 0 && i < eq::numBands ? (float) bands[(size_t) i].dynGain : 0.0f; }

    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace eq;
        for (int b = 0; b < numBands; ++b)
        {
            const auto* bp = p + b * bandParams;
            const auto envAtt = std::exp (-1.0 / (std::max (0.5, (double) p[xAttack]) * 0.001 * sr));
            const auto envRel = std::exp (-1.0 / (std::max (5.0, (double) p[xRelease]) * 0.001 * sr));
            auto& st = bands[(size_t) b];
            st.active = bp[bOn] > 0.5f;
            if (! st.active) continue;
            const auto type = (int) bp[bType];
            st.type = type;
            st.dynamic = std::abs (bp[bRange]) > 0.05f && (type == bell || type == lowShelf || type == highShelf);
            // bande inutile : gain nul et pas de dynamique
            if ((type == bell || type == lowShelf || type == highShelf) && std::abs (bp[bGain]) < 0.01f && ! st.dynamic) { st.active = false; continue; }

            double gain = bp[bGain];
            st.spectral = st.dynamic && type == bell && p[xSpectral] > 0.5f;
            if (st.spectral)
            {
                // dynamique spectrale : la bande est découpée en 8 sous-bandes, chacune avec son détecteur
                // et sa cloche dynamique ; seules les fréquences qui dépassent le seuil sont touchées
                const auto bwOct = std::clamp (1.4 / std::max (0.1, (double) bp[bQ]), 0.5, 6.0);
                const auto step = bwOct / (numSub - 1);
                const auto subQ = std::clamp (1.4 / step, 1.0, 12.0);
                double sum = 0.0;
                for (int k = 0; k < numSub; ++k)
                {
                    const auto f = std::clamp ((double) bp[bFreq] * std::pow (2.0, bwOct * ((double) k / (numSub - 1) - 0.5)), 20.0, 0.45 * sr);
                    // détection du 4e ordre (deux passe-bande) : une sous-bande n'entend pas ses voisines éloignées
                    st.subSc[(size_t) k].set (SvfEq::bandpass, f, subQ, 0.0, sr);
                    st.subSc2[(size_t) k].set (SvfEq::bandpass, f, subQ, 0.0, sr);
                    auto& e = st.subEnv[(size_t) k];
                    for (int i = 0; i < n; ++i)
                    {
                        const auto x = std::abs (st.subSc2[(size_t) k].process (st.subSc[(size_t) k].process (0.5 * ((double) L[i] + R[i]))));
                        e = x > e ? x + (e - x) * envAtt : x + (e - x) * envRel;
                    }
                    const auto over = std::clamp ((gainToDb (e) + 6.0 - bp[bThreshold]) / 12.0, 0.0, 1.0);
                    // forme de la cloche principale : les sous-bandes du bord agissent moins
                    const auto shape = std::exp (-std::pow (((double) k / (numSub - 1) - 0.5) * 2.0, 2.0) * 0.7);
                    const auto g = bp[bRange] * over * shape;
                    st.sub[(size_t) k].set (SvfEq::bell, f, subQ * 0.7, g * 0.55, sr);
                    sum += bp[bRange] * over * shape;
                }
                st.dynGain = gain + sum / numSub;
            }
            else if (st.dynamic)
            {
                // détection sur un passe-bande à la fréquence de la bande (signal mono), bloc par bloc
                st.sc.set (SvfEq::bandpass, bp[bFreq], std::max (0.5, (double) bp[bQ]), 0.0, sr);
                for (int i = 0; i < n; ++i)
                {
                    const auto x = std::abs (st.sc.process (0.5 * ((double) L[i] + R[i])));
                    st.env = x > st.env ? x + (st.env - x) * envAtt : x + (st.env - x) * envRel;
                }
                const auto over = std::clamp ((gainToDb (st.env) - bp[bThreshold]) / 12.0, 0.0, 1.0);
                gain += bp[bRange] * over;
            }
            if (! st.spectral) st.dynGain = gain;
            const auto q = (double) bp[bQ];
            const auto k = q / 0.9; // Q d'origine 0,9 = Butterworth
            switch (type)
            {
                case bell:      st.f1.set (SvfEq::bell, bp[bFreq], q, gain, sr); break;
                case lowShelf:  st.f1.set (SvfEq::lowShelf, bp[bFreq], std::min (q, 2.0), gain, sr); break;
                case highShelf: st.f1.set (SvfEq::highShelf, bp[bFreq], std::min (q, 2.0), gain, sr); break;
                case lowCut12:  st.f1.set (SvfEq::highpass, bp[bFreq], q * 0.79, 0.0, sr); break;
                case highCut12: st.f1.set (SvfEq::lowpass, bp[bFreq], q * 0.79, 0.0, sr); break;
                case lowCut24: case highCut24:
                {
                    const auto t = type == lowCut24 ? SvfEq::highpass : SvfEq::lowpass;
                    st.f1.set (t, bp[bFreq], 0.5412 * k, 0.0, sr); st.f2.set (t, bp[bFreq], 1.3066 * k, 0.0, sr);
                    st.stages = 2;
                    break;
                }
                case lowCut36: case highCut36:
                {
                    const auto t = type == lowCut36 ? SvfEq::highpass : SvfEq::lowpass;
                    st.f1.set (t, bp[bFreq], 0.5176 * k, 0.0, sr); st.f2.set (t, bp[bFreq], 0.7071 * k, 0.0, sr);
                    st.f3.set (t, bp[bFreq], 1.9319 * k, 0.0, sr);
                    st.stages = 3;
                    break;
                }
                case lowCut48: case highCut48:
                {
                    const auto t = type == lowCut48 ? SvfEq::highpass : SvfEq::lowpass;
                    st.f1.set (t, bp[bFreq], 0.5098 * k, 0.0, sr); st.f2.set (t, bp[bFreq], 0.6013 * k, 0.0, sr);
                    st.f3.set (t, bp[bFreq], 0.9000 * k, 0.0, sr); st.f4.set (t, bp[bFreq], 2.5629 * k, 0.0, sr);
                    st.stages = 4;
                    break;
                }
                default:        st.f1.set (SvfEq::notch, bp[bFreq], q, 0.0, sr); break;
            }
            if (type != lowCut24 && type != highCut24 && type != lowCut36 && type != highCut36 && type != lowCut48 && type != highCut48) st.stages = 1;
        }
        const auto out = dbToGain (p[gOutput]);
        const auto mode = std::clamp ((int) p[gChannels], 0, 3);
        for (int i = 0; i < n; ++i)
        {
            // en mid/side, la paire de canaux traitée devient (mid, side)
            double a = L[i], b = R[i];
            if (mode != stereo) { const auto m = 0.5 * (a + b), sd = 0.5 * (a - b); a = m; b = sd; }
            for (int bi = 0; bi < numBands; ++bi)
            {
                auto& st = bands[(size_t) bi];
                if (! st.active) continue;
                const bool onA = mode != sideOnly && (mode != midSide || bi < 4);
                const bool onB = mode == stereo || mode == sideOnly || (mode == midSide && bi >= 4);
                if (onA) { a = st.f1.l.process (a); if (st.stages > 1) a = st.f2.l.process (a); if (st.stages > 2) a = st.f3.l.process (a); if (st.stages > 3) a = st.f4.l.process (a); }
                if (onB) { b = st.f1.r.process (b); if (st.stages > 1) b = st.f2.r.process (b); if (st.stages > 2) b = st.f3.r.process (b); if (st.stages > 3) b = st.f4.r.process (b); }
                if (st.spectral)
                    for (auto& f : st.sub) { if (onA) a = f.l.process (a); if (onB) b = f.r.process (b); }
            }
            if (mode != stereo) { const auto l = a + b, r = a - b; a = l; b = r; }
            L[i] = (float) (a * out);
            R[i] = (float) (b * out);
            // analyseur : sortie mono dans un anneau lu par l'interface
            ring[(size_t) ringPos].store ((float) (0.5 * (a + b) * out), std::memory_order_relaxed);
            ringPos = (ringPos + 1) & (ringSize - 1);
        }
        ringWrite.store (ringPos, std::memory_order_release);
    }

    int analysis (float* dst, int count) const noexcept override
    {
        count = std::min (count, ringSize);
        const auto w = ringWrite.load (std::memory_order_acquire);
        for (int i = 0; i < count; ++i) dst[i] = ring[(size_t) ((w - count + i + ringSize) & (ringSize - 1))].load (std::memory_order_relaxed);
        return count;
    }

private:
    static constexpr int ringSize = 4096;
    std::array<std::atomic<float>, ringSize> ring {};
    int ringPos = 0;
    std::atomic<int> ringWrite { 0 };
    struct Band
    {
        SvfEqStereo f1, f2, f3, f4; SvfEq sc; double env = 0.0, dynGain = 0.0; int type = 0, stages = 1; bool active = false, dynamic = false, spectral = false;
        std::array<SvfEqStereo, eq::numSub> sub; std::array<SvfEq, eq::numSub> subSc, subSc2; std::array<double, eq::numSub> subEnv {};
    };
    std::array<Band, eq::numBands> bands;
    double sr = 48000.0;
};

} // namespace grv::effects
