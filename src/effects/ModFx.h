/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Effets de modulation et de performance :
      - « Chorus » : 2 à 4 voix de lignes à retard modulées, principe des ensembles à BBD ;
      - « Phaser » : 4 à 12 étages de passe-tout, réinjection, décalage stéréo, synchro tempo ;
      - « Répétition » (stutter) : capture d'une tranche calée sur le tempo puis répétition, avec
        probabilité, porte, chute de hauteur (effet d'arrêt de bande) et décroissance ;
      - « Filtre » : filtre multimode résonant modulé par LFO (libre ou tempo) et par l'enveloppe
        du signal (effet « auto-wah »).
    Architectures propres. Aucune latence.
*/

#pragma once

#include "../dsp/EqFilters.h"
#include "../engine/Module.h"
#include "../synth/SynthCore.h"

#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

inline const std::vector<std::string>& syncDivisions()
{
    static const std::vector<std::string> d { "16 mes.", "8 mes.", "4 mes.", "2 mes.", "1 mes.", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T" };
    return d;
}
inline double syncQuarters (int i) noexcept
{
    static constexpr double q[13] { 64.0, 32.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return q[std::clamp (i, 0, 12)];
}

//==============================================================================
namespace chorus
{
    enum Slot { voices = 0, rate, depth, delay, feedback, tone, width, mix, sync, division, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "chorus"; m.name = "Moire"; m.role = "Chorus"; m.family = "Modulation"; m.form = "pedal";
            m.principle = "Ensemble de 2 \xC3\xA0 4 voix de lignes \xC3\xA0 retard modul\xC3\xA9" "es (principe BBD)";
            m.params = {
                C ("voices", "Voix", { "2", "3", "4" }),
                P ("rate", "Vitesse", "Hz", 0.03f, 8, 0.6f, 0.4f),
                P ("depth", "Profondeur", "ms", 0, 8, 2.2f),
                P ("delay", "Retard", "ms", 1, 30, 7),
                P ("feedback", "R\xC3\xA9injection", "%", -90, 90, 0),
                P ("tone", "Tonalit\xC3\xA9", "Hz", 1500, 20000, 9000, 0.4f),
                P ("width", "Largeur", "%", 0, 100, 100),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 50),
                // v0.45 : vitesse libre ou calée sur le tempo (ajoutés à la fin : positions inchangées)
                C ("sync", "Synchro", { "Libre", "Tempo" }),
                C ("division", "Division", syncDivisions(), 4),
            };
            m.sections = { { "chorus", "Chorus", "lfo", { "voices", "sync", "rate", "division", "depth", "delay", "feedback", "tone", "width", "mix" }, "main" } };
            m.presets = {
                { "Ensemble", {} },
                { "Doux", { { "rate", 0.3f }, { "depth", 1.2f }, { "mix", 35 } } },
                { "Flanger", { { "voices", 0 }, { "delay", 1.5f }, { "depth", 1.2f }, { "feedback", 70 }, { "rate", 0.15f } } },
                { "Vibrato", { { "voices", 0 }, { "rate", 5.5f }, { "depth", 1.0f }, { "mix", 100 }, { "width", 0 } } },
            };
            return m;
        }();
        return d;
    }
}

class Chorus final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return chorus::desc(); }
    void prepare (double s, int) override
    {
        sr = s; size = 1;
        while (size < (int) (0.05 * sr)) size <<= 1;
        bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f);
        reset();
    }
    void reset() noexcept override { std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f); w = 0; phase = 0.0; lpL = lpR = 0.0; fbL = fbR = 0.0; wetPrev = -1.0; }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace chorus;
        const auto hz = p[sync] > 0.5f ? (std::max (20.0, bpm) / 60.0) / syncQuarters ((int) p[division]) : (double) p[rate];
        const auto nv = 2 + std::clamp ((int) p[voices], 0, 2);
        const auto depthS = p[depth] * 0.001 * sr, delayS = p[delay] * 0.001 * sr;
        const auto fb = p[feedback] / 100.0;
        const auto lpC = std::exp (-kTwoPi * std::min ((double) p[tone], 0.45 * sr) / sr);
        const auto wd = p[width] / 100.0, wet = p[mix] / 100.0;
        const auto mask = size - 1;
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n); // mélange en rampe (v0.22)
        wetPrev = wet;
        for (int i = 0; i < n; ++i)
        {
            const auto wt = wet0 + dWet * (double) (i + 1);
            const double inL = L[i], inR = R[i];
            bufL[(size_t) w] = (float) (inL + fb * fbL);
            bufR[(size_t) w] = (float) (inR + fb * fbR);
            phase += hz / sr; if (phase >= 1.0) phase -= 1.0;
            double yl = 0.0, yr = 0.0;
            for (int v = 0; v < nv; ++v)
            {
                const auto ph = phase + (double) v / nv;
                const auto tri = 1.0 - 4.0 * std::abs (ph - std::floor (ph) - 0.5);
                const auto d = delayS + depthS * 0.5 * (1.0 + tri);
                const auto sl = read (bufL, d, mask), sR = read (bufR, d, mask);
                const auto pan = nv == 1 ? 0.0 : (2.0 * v / (nv - 1) - 1.0) * wd;
                yl += (v % 2 ? sR : sl) * (1.0 - std::max (0.0, pan));
                yr += (v % 2 ? sR : sl) * (1.0 + std::min (0.0, pan));
            }
            yl /= nv * 0.5; yr /= nv * 0.5;
            lpL = yl + (lpL - yl) * lpC; lpR = yr + (lpR - yr) * lpC;
            fbL = std::tanh (lpL); fbR = std::tanh (lpR);
            w = (w + 1) & mask;
            L[i] = (float) (inL * (1.0 - wt * 0.5) + lpL * wt * 0.7);
            R[i] = (float) (inR * (1.0 - wt * 0.5) + lpR * wt * 0.7);
        }
    }

private:
    double read (const std::vector<float>& b, double d, int mask) const noexcept
    {
        auto pos = (double) w - std::clamp (d, 1.0, (double) size - 3.0);
        const auto fl = std::floor (pos);
        const auto i0 = (int) fl;
        const auto f = pos - fl;
        // Hermite (v0.22) : moins de perte d'aigus et de bruit de modulation que le linéaire
        auto at = [&] (int j) { return (double) b[(size_t) (j & mask)]; };
        return hermite4 (at (i0 - 1), at (i0), at (i0 + 1), at (i0 + 2), f);
    }
    double sr = 48000.0, phase = 0.0, lpL = 0.0, lpR = 0.0, fbL = 0.0, fbR = 0.0, wetPrev = -1.0;
    int size = 1, w = 0;
    std::vector<float> bufL, bufR;
};

//==============================================================================
namespace phaser
{
    enum Slot { stages = 0, sync, rate, division, depth, center, feedback, stereo, mix, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "phaser"; m.name = "Whirl"; m.role = "Phaser"; m.family = "Modulation"; m.form = "pedal";
            m.principle = "4 \xC3\xA0 12 \xC3\xA9tages de passe-tout modul\xC3\xA9s, r\xC3\xA9injection, d\xC3\xA9" "calage st\xC3\xA9r\xC3\xA9o";
            m.params = {
                C ("stages", "\xC3\x89tages", { "4", "6", "8", "12" }, 1),
                C ("sync", "Synchro", { "Libre", "Tempo" }),
                P ("rate", "Vitesse", "Hz", 0.02f, 10, 0.35f, 0.4f),
                C ("division", "Division", syncDivisions(), 4),
                P ("depth", "Profondeur", "%", 0, 100, 70),
                P ("center", "Centre", "Hz", 100, 8000, 900, 0.4f),
                P ("feedback", "R\xC3\xA9injection", "%", -95, 95, 40),
                P ("stereo", "D\xC3\xA9" "calage st\xC3\xA9r\xC3\xA9o", "\xC2\xB0", 0, 180, 90),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 50),
            };
            m.sections = { { "phaser", "Phaser", "lfo", { "stages", "sync", "rate", "division", "depth", "center", "feedback", "stereo" }, "main" },
                           { "levels", "Niveaux", "lfo", { "mix" }, "main" } };
            m.presets = { { "Classique", {} }, { "Lent et large", { { "rate", 0.08f }, { "stages", 3 }, { "depth", 90 } } },
                          { "Cal\xC3\xA9 sur la mesure", { { "sync", 1 }, { "division", 4 }, { "feedback", 70 } } } };
            return m;
        }();
        return d;
    }
}

class Phaser final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return phaser::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override { for (auto& c : ap) c.fill ({}); fb.fill (0.0); phase = 0.0; wetPrev = -1.0; }
    void setPosition (double ppq, bool playing) noexcept override { hostPpq = ppq; hostPlaying = playing; }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace phaser;
        static constexpr int stageCount[4] { 4, 6, 8, 12 };
        const auto ns = stageCount[std::clamp ((int) p[stages], 0, 3)];
        const bool synced = p[sync] > 0.5f;
        const auto hz = synced ? (bpm / 60.0) / syncQuarters ((int) p[division]) : (double) p[rate];
        const auto dpt = p[depth] / 100.0;
        const auto fbk = p[feedback] / 100.0;
        const auto offs = p[stereo] / 360.0;
        const auto wet = p[mix] / 100.0;
        if (synced && hostPlaying) phase = std::fmod (hostPpq / syncQuarters ((int) p[division]), 1.0);
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n); // mélange en rampe (v0.22)
        wetPrev = wet;
        for (int i = 0; i < n; ++i)
        {
            const auto wt = wet0 + dWet * (double) (i + 1);
            phase += hz / sr; if (phase >= 1.0) phase -= 1.0;
            const double in[2] { L[i], R[i] };
            double out[2];
            for (int c = 0; c < 2; ++c)
            {
                auto ph = phase + (c == 1 ? offs : 0.0); ph -= std::floor (ph);
                const auto lfo = 0.5 - 0.5 * std::cos (kTwoPi * ph);
                const auto fc = std::clamp ((double) p[center] * std::pow (2.0, (lfo - 0.5) * 2.0 * 3.0 * dpt), 30.0, 0.45 * sr);
                const auto t = std::tan (kPi * fc / sr);
                const auto a = (t - 1.0) / (t + 1.0);
                auto x = in[c] + fbk * fb[(size_t) c];
                for (int k = 0; k < ns; ++k)
                {
                    auto& s = ap[(size_t) c][(size_t) k];
                    const auto y = a * x + s.x1 - a * s.y1;
                    s.x1 = x; s.y1 = y;
                    x = y;
                }
                fb[(size_t) c] = std::tanh (x);
                out[c] = in[c] * (1.0 - 0.5 * wt) + x * 0.5 * wt;
            }
            L[i] = (float) out[0]; R[i] = (float) out[1];
        }
    }

private:
    struct St { double x1 = 0.0, y1 = 0.0; };
    std::array<std::array<St, 12>, 2> ap {};
    std::array<double, 2> fb {};
    double sr = 48000.0, phase = 0.0, hostPpq = 0.0, wetPrev = -1.0;
    bool hostPlaying = false;
};

//==============================================================================
namespace stutter
{
    enum Slot { division = 0, length, chance, gate, pitch, decay, mix, mode, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "stutter"; m.name = "Staccato"; m.role = "R\xC3\xA9p\xC3\xA9tition"; m.family = "Filtres et performance"; m.form = "pedal";
            m.principle = "Capture d'une tranche cal\xC3\xA9" "e sur le tempo et r\xC3\xA9p\xC3\xA9tition (stutter), probabilit\xC3\xA9, chute de hauteur";
            m.params = {
                C ("division", "Tranche", { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64" }, 3),
                C ("length", "Dur\xC3\xA9" "e", { "1/4", "1/2", "1 mes.", "2 mes." }, 1),
                P ("chance", "Probabilit\xC3\xA9", "%", 0, 100, 100),
                P ("gate", "Porte", "%", 10, 100, 100),
                P ("pitch", "Chute de hauteur", "dt", -24, 0, 0),
                P ("decay", "D\xC3\xA9" "croissance", "%", 0, 100, 0),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
                C ("mode", "D\xC3\xA9" "clenchement", { "Arr\xC3\xAAt", "Toujours", "Hasard" }),
            };
            m.sections = { { "stutter", "R\xC3\xA9p\xC3\xA9tition", "seq", { "mode", "division", "length", "chance", "gate", "pitch", "decay", "mix" }, "main" } };
            m.presets = { { "Roulement 1/16", { { "mode", 1 } } }, { "Hasard 1/32", { { "mode", 2 }, { "division", 5 }, { "chance", 35 } } },
                          { "Arr\xC3\xAAt de bande", { { "mode", 1 }, { "division", 1 }, { "pitch", -24 }, { "decay", 40 } } } };
            return m;
        }();
        return d;
    }
}

/** Répétition : quand le mode est actif, à chaque début de « durée » (calée sur la mesure), décide
    (probabilité) de répéter la tranche qui vient d'être capturée pendant toute la durée. */
class Stutter final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return stutter::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        capSize = (int) (sr * 2.1) + 8; // une noire à 30 BPM au plus
        capL.assign ((size_t) capSize, 0.0f); capR.assign ((size_t) capSize, 0.0f);
        reset();
    }
    void reset() noexcept override { repeating = false; capPos = 0; playPos = 0.0; env = 0.0; lastWindow = -1; samplesSince = 0.0; armNext = false; declick.prepare (sr); }
    void setPosition (double ppq, bool playing) noexcept override { hostPpq = ppq; hostPlaying = playing; }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace stutter;
        const auto md = (int) p[mode];
        static constexpr double sliceQ[7] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 0.0625 };
        static constexpr double lenQ[4] { 1.0, 2.0, 4.0, 8.0 };
        const auto qps = std::max (20.0, bpm) / 60.0 / sr; // noires par échantillon
        const auto slice = std::clamp (sliceQ[std::clamp ((int) p[division], 0, 6)] / qps, 16.0, (double) capSize - 4.0);
        const auto windowQ = lenQ[std::clamp ((int) p[length], 0, 3)];
        const auto gateLen = slice * p[gate] / 100.0;
        const auto wet = p[mix] / 100.0;
        const auto fade = std::exp (-1.0 / (0.002 * sr));
        double ppq = hostPlaying ? hostPpq : internalPpq;

        for (int i = 0; i < n; ++i)
        {
            bool jump = armNext; // v0.22 : un saut de signal à cet échantillon est compensé (anti-clic)
            armNext = false;
            const auto window = (long long) std::floor (ppq / windowQ);
            if (window != lastWindow)
            {
                jump = jump || repeating;
                lastWindow = window;
                rng = rng * 1664525u + 1013904223u;
                // « Toujours » : chaque durée est répétée ; « Hasard » : selon la probabilité
                repeating = md == 1 || (md == 2 && (double) (rng >> 8) / 16777216.0 * 100.0 < p[chance]);
                capPos = 0; playPos = 0.0; repeatIndex = 0; samplesSince = 0.0;
            }
            const double inL = L[i], inR = R[i];
            double ol = inL, orr = inR;
            if (repeating)
            {
                if (capPos < (int) slice) // capture de la première tranche (on l'entend en direct)
                {
                    capL[(size_t) capPos] = (float) inL; capR[(size_t) capPos] = (float) inR;
                    ++capPos;
                    env = 1.0;
                }
                else
                {
                    if (playPos == 0.0) jump = true; // passage du direct à la première répétition
                    // répétitions : hauteur qui chute progressivement, décroissance, porte
                    const auto progress = std::min (1.0, samplesSince / (windowQ / qps));
                    const auto rate = std::pow (2.0, p[pitch] * progress / 12.0);
                    const auto within = std::fmod (playPos, slice);
                    const bool open = within < gateLen;
                    env = (open ? 1.0 : 0.0) + (env - (open ? 1.0 : 0.0)) * fade;
                    const auto gain = env * std::pow (1.0 - 0.9 * p[decay] / 100.0, (double) repeatIndex);
                    const auto idx = std::clamp ((int) within, 0, capSize - 2);
                    const auto f = within - std::floor (within);
                    const auto sl = capL[(size_t) idx] + (capL[(size_t) idx + 1] - capL[(size_t) idx]) * f;
                    const auto sR = capR[(size_t) idx] + (capR[(size_t) idx + 1] - capR[(size_t) idx]) * f;
                    ol = inL * (1.0 - wet) + sl * gain * wet;
                    orr = inR * (1.0 - wet) + sR * gain * wet;
                    const auto before = playPos;
                    playPos += rate;
                    if (std::floor (playPos / slice) != std::floor (before / slice)) { ++repeatIndex; armNext = true; }
                }
                samplesSince += 1.0;
            }
            L[i] = (float) ol; R[i] = (float) orr;
            if (jump) declick.arm();
            declick.process (L + i, R + i, 1);
            ppq += qps;
        }
        internalPpq = ppq;
    }

private:
    double sr = 48000.0, hostPpq = 0.0, internalPpq = 0.0, playPos = 0.0, env = 0.0, samplesSince = 0.0;
    synth::Declick declick; // sauts de la répétition (v0.22)
    bool hostPlaying = false, repeating = false, armNext = false;
    int capSize = 1, capPos = 0, repeatIndex = 0;
    long long lastWindow = -1;
    std::uint32_t rng = 0xABCDEFu;
    std::vector<float> capL, capR;
};

//==============================================================================
namespace filterfx
{
    enum Slot { type = 0, cutoff, reso, drive, sync, rate, division, lfoDepth, envDepth, envSpeed, mix, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "filter"; m.name = "Iris"; m.role = "Filtre"; m.family = "Filtres et performance"; m.form = "pedal";
            m.principle = "Filtre multimode r\xC3\xA9sonant modul\xC3\xA9 par LFO et par l'enveloppe du signal";
            m.params = {
                C ("type", "Type", { "Passe-bas 24", "Passe-bas 12", "Passe-bande", "Passe-haut 12", "Passe-haut 24" }),
                P ("cutoff", "Coupure", "Hz", 20, 20000, 20000, 0.3f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 20),
                P ("drive", "Saturation", "dB", 0, 24, 0),
                C ("sync", "Synchro", { "Libre", "Tempo" }, 1),
                P ("rate", "Vitesse", "Hz", 0.02f, 20, 1, 0.4f),
                C ("division", "Division", syncDivisions(), 6),
                P ("lfodepth", "LFO", "%", -100, 100, 0),
                P ("envdepth", "Enveloppe", "%", -100, 100, 0),
                P ("envspeed", "R\xC3\xA9" "action", "ms", 5, 1000, 80, 0.5f),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
            };
            m.sections = { { "filter", "Filtre", "filter", { "type", "cutoff", "reso", "drive", "mix" }, "main" },
                           { "mod", "Modulation", "lfo", { "sync", "rate", "division", "lfodepth", "envdepth", "envspeed" }, "mod" } };
            m.presets = { { "Ouvert", {} }, { "Auto-wah", { { "type", 2 }, { "cutoff", 400 }, { "reso", 60 }, { "envdepth", 80 }, { "envspeed", 60 } } },
                          { "Balayage 1 mes.", { { "cutoff", 1200 }, { "reso", 45 }, { "division", 4 }, { "lfodepth", 70 } } },
                          { "Coupe-bas de break", { { "type", 4 }, { "cutoff", 600 }, { "reso", 30 } } } };
            return m;
        }();
        return d;
    }
}

class FilterFx final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return filterfx::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override { for (auto& f : lad) f.reset(); for (auto& f : svf) f.reset(); env = 0.0; phase = 0.0; cutLog = -1.0; wetPrev = -1.0; }
    void setPosition (double ppq, bool playing) noexcept override { hostPpq = ppq; hostPlaying = playing; }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace filterfx;
        const auto t = std::clamp ((int) p[type], 0, 4);
        const bool synced = p[sync] > 0.5f;
        const auto hz = synced ? (bpm / 60.0) / syncQuarters ((int) p[division]) : (double) p[rate];
        if (synced && hostPlaying) phase = std::fmod (hostPpq / syncQuarters ((int) p[division]), 1.0);
        const auto res = p[reso] / 100.0;
        const auto drv = dbToGain (p[drive]);
        const auto lfoOct = p[lfoDepth] / 100.0 * 4.0, envOct = p[envDepth] / 100.0 * 5.0;
        const auto envC = std::exp (-1.0 / (p[envSpeed] * 0.001 * sr));
        const auto wet = p[mix] / 100.0;
        static constexpr synth::Ladder::Mode modes[5] { synth::Ladder::lp24, synth::Ladder::lp12, synth::Ladder::bp12, synth::Ladder::hp12, synth::Ladder::hp24 };
        // filtre grand ouvert sans modulation : transparent
        if (t <= 1 && p[cutoff] >= 19900.0f && lfoOct == 0.0 && envOct == 0.0 && res < 0.01) { cutLog = -1.0; wetPrev = -1.0; return; }
        // v0.22 : coupure lissée (en octaves, ~5 ms) et mélange en rampe : un bouton tourné ou une
        // modulation par bloc ne fait plus de marches audibles
        const auto targetLog = std::log2 (std::max (20.0, (double) p[cutoff]));
        if (cutLog < 0.0) cutLog = targetLog;
        const auto cutGlide = 1.0 - std::exp (-1.0 / (0.005 * sr));
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n);
        wetPrev = wet;
        for (int i = 0; i < n; ++i)
        {
            const auto wt = wet0 + dWet * (double) (i + 1);
            cutLog += (targetLog - cutLog) * cutGlide;
            phase += hz / sr; if (phase >= 1.0) phase -= 1.0;
            const auto lfo = fastSin2Pi (phase);
            const double in[2] { L[i], R[i] };
            env = std::max (std::abs (in[0]) + std::abs (in[1]), env * envC);
            const auto fc = std::clamp (fastExp2 (cutLog + lfo * lfoOct + std::min (1.0, env) * envOct), 20.0, 0.45 * sr);
            for (int c = 0; c < 2; ++c)
            {
                const auto y = lad[(size_t) c].process (in[c] * 0.5, fc, res, sr, modes[t], drv) * 2.0;
                (c == 0 ? L : R)[i] = (float) (in[c] * (1.0 - wt) + y * wt);
            }
        }
    }

private:
    std::array<synth::Ladder, 2> lad;
    std::array<synth::StateFilter, 2> svf;
    double sr = 48000.0, env = 0.0, phase = 0.0, hostPpq = 0.0, cutLog = -1.0, wetPrev = -1.0;
    bool hostPlaying = false;
};

} // namespace grv::effects
