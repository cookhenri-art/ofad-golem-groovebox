/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Boucleur » — principe des pédales de boucle : enregistrer ce qui passe dans le canal, fermer
    la boucle, la rejouer, la superposer (surcouche), l'arrêter ou l'effacer. Longueur libre ou
    calée sur 1 à 8 mesures (fermeture automatique), départ immédiat ou à la mesure suivante,
    vitesse ½ / 1 / 2, lecture à l'envers, niveau de la boucle, part du signal direct, mémoire des
    couches (ce qui reste des couches précédentes à chaque surcouche). 30 s de boucle stéréo.
    La commande « Action » est un réglage : elle s'apprend au MIDI, se verrouille dans une scène
    ou sur un pas comme les autres. Tampon alloué à la préparation (aucune allocation au jeu).
    Architecture propre.
*/

#pragma once

#include "../dsp/Common.h"
#include "../engine/Module.h"
#include "FxBanks.h"

#include <atomic>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace looper
{
    enum Slot { action = 0, length, start, speed, reverse, level, dry, keep, numSlots };
    enum Action { stop = 0, record, play, overdub, erase };
    enum State { empty = 0, armed, recording, playing, overdubbing, stopped };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "looper";
            m.name = "Infinity"; m.role = "Boucleur";
            m.family = "Filtres et performance"; m.form = "pedal";
            m.principle = "P\xC3\xA9" "dale de boucle : enregistrer, fermer, surcouches, arr\xC3\xAAt, effacement ; longueur cal\xC3\xA9" "e sur les mesures, vitesse, envers";
            m.params = {
                C ("action", "Action", { "Arr\xC3\xAAt", "Enregistrer", "Lecture", "Surcouche", "Effacer" }, 0),
                C ("length", "Longueur", { "Libre", "1 mes.", "2 mes.", "4 mes.", "8 mes." }, 0),
                C ("start", "D\xC3\xA9part", { "Imm\xC3\xA9" "diat", "\xC3\x80 la mesure" }, 0),
                C ("speed", "Vitesse", { "\xC2\xBD", "1", "2" }, 1),
                C ("reverse", "Envers", { "Non", "Oui" }),
                P ("level", "Niveau de la boucle", "dB", -24, 6, 0),
                P ("dry", "Signal direct", "%", 0, 100, 100),
                P ("keep", "M\xC3\xA9moire des couches", "%", 0, 100, 100),
            };
            m.sections = {
                { "loop", "Boucle", "global", { "action", "length", "start", "speed", "reverse", "level", "dry", "keep" }, "loop" },
            };
            m.presets = {
                { "Boucle libre", {} },
                { "Une mesure cal\xC3\xA9" "e", { { "length", 1 }, { "start", 1 } } },
                { "Quatre mesures cal\xC3\xA9" "es", { { "length", 3 }, { "start", 1 } } },
                { "Boucle seule (sans direct)", { { "dry", 0 } } },
                { "Surcouches qui s'effacent", { { "keep", 75 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class Looper final : public Effect
{
public:
    static constexpr double maxSeconds = 30.0;

    const ModuleDesc& desc() const noexcept override { return looper::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        bufL.assign ((size_t) (maxSeconds * sr), 0.0f);
        bufR.assign ((size_t) (maxSeconds * sr), 0.0f);
        reset();
    }

    void reset() noexcept override
    {
        state = looper::empty; len = 0; recPos = 0; pos = 0.0; lastAction = -1; outPeak = 0.0f;
    }

    void setPosition (double ppq, bool isPlaying) noexcept override { ppqNow = ppq; transport = isPlaying; }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace looper;
        const auto act = std::clamp ((int) p[action], 0, 4);
        const auto barSamples = 4.0 * 60.0 / std::max (20.0, bpm) * sr;
        static constexpr int barsFor[] { 0, 1, 2, 4, 8 };
        const auto bars = barsFor[std::clamp ((int) p[length], 0, 4)];
        const auto maxLen = (int) bufL.size();
        if (act != lastAction)
        {
            if (lastAction >= 0) onAction (act, bars, barSamples, p[start] > 0.5f);
            lastAction = act;
        }
        // départ à la mesure : on attend le premier temps de la mesure suivante
        int armedOffset = -1;
        if (state == armed)
        {
            if (! transport) armedOffset = 0;
            else
            {
                const auto qps = std::max (20.0, bpm) / 60.0 / sr;
                const auto nextBar = std::ceil (ppqNow / 4.0 - 1.0e-9) * 4.0;
                const auto off = (int) std::floor ((nextBar - ppqNow) / qps);
                if (off < n) armedOffset = std::max (0, off);
            }
        }
        static constexpr double speeds[] { 0.5, 1.0, 2.0 };
        const auto rate = speeds[std::clamp ((int) p[speed], 0, 2)] * (p[reverse] > 0.5f ? -1.0 : 1.0);
        const auto lvl = dbToGain (p[level]);
        const auto dryG = p[dry] / 100.0, keepG = p[keep] / 100.0;
        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const double inL = L[i], inR = R[i];
            if (state == armed && i == armedOffset) { state = recording; recPos = 0; }
            double ol = 0.0, orr = 0.0;
            if (state == recording)
            {
                bufL[(size_t) recPos] = (float) inL; bufR[(size_t) recPos] = (float) inR;
                ++recPos;
                const auto target = bars > 0 ? std::min (maxLen, (int) std::lround (bars * barSamples)) : maxLen;
                if (recPos >= target) closeLoop (bars, barSamples, overdubAfterClose);
            }
            else if ((state == playing || state == overdubbing) && len > 0)
            {
                const auto i0 = (int) pos;
                const auto f = pos - (double) i0;
                const auto i1 = (i0 + 1) % len;
                ol = (double) bufL[(size_t) i0] * (1.0 - f) + (double) bufL[(size_t) i1] * f;
                orr = (double) bufR[(size_t) i0] * (1.0 - f) + (double) bufR[(size_t) i1] * f;
                if (state == overdubbing)
                {
                    bufL[(size_t) i0] = (float) (bufL[(size_t) i0] * keepG + inL);
                    bufR[(size_t) i0] = (float) (bufR[(size_t) i0] * keepG + inR);
                }
                // petit fondu à la jonction (début et fin de la boucle)
                const auto edge = std::min (1.0, std::min (pos, (double) len - pos) / 64.0);
                ol *= edge; orr *= edge;
                pos += rate;
                if (pos >= (double) len) pos -= (double) len;
                if (pos < 0.0) pos += (double) len;
            }
            const auto yl = inL * dryG + ol * lvl, yr = inR * dryG + orr * lvl;
            L[i] = (float) yl; R[i] = (float) yr;
            peak = std::max (peak, (float) std::max (std::abs (yl), std::abs (yr)));
        }
        outPeak = std::max (peak, outPeak * 0.9f);
        stateNow.store (state, std::memory_order_relaxed);
        phaseNow.store (len > 0 ? (float) (pos / (double) len) : (state == recording ? (float) recPos / (float) maxLen : 0.0f), std::memory_order_relaxed);
        lengthNow.store (state == recording ? (float) (recPos / sr) : (float) (len / sr), std::memory_order_relaxed);
    }

    /** 0 : rien ; 1 : niveau de sortie ; 2 : état (partie entière) + position dans la boucle (partie fractionnaire). */
    float meter (int index) const noexcept override
    {
        if (index == 1) return outPeak;
        if (index == 2) return (float) stateNow.load (std::memory_order_relaxed) + std::min (0.999f, phaseNow.load (std::memory_order_relaxed));
        if (index == 3) return lengthNow.load (std::memory_order_relaxed);
        return 0.0f;
    }

private:
    void onAction (int act, int bars, double barSamples, bool atBar) noexcept
    {
        using namespace looper;
        switch (act)
        {
            case record:
                if (state == recording) { closeLoop (bars, barSamples, false); break; }
                state = atBar ? armed : recording; recPos = 0; len = 0; pos = 0.0; overdubAfterClose = false;
                break;
            case play:
                if (state == recording) closeLoop (bars, barSamples, false);
                else if (state == armed) state = empty;
                else if (len > 0) state = playing;
                break;
            case overdub:
                if (state == recording) closeLoop (bars, barSamples, true);
                else if (len > 0) state = overdubbing;
                else { state = atBar ? armed : recording; recPos = 0; len = 0; pos = 0.0; overdubAfterClose = true; }
                break;
            case erase: state = empty; len = 0; recPos = 0; pos = 0.0; break;
            default: // arrêt : la boucle est gardée
                if (state == recording) closeLoop (bars, barSamples, false);
                if (len > 0) { state = stopped; pos = 0.0; } else state = empty;
                break;
        }
    }

    void closeLoop (int bars, double barSamples, bool thenOverdub) noexcept
    {
        // longueur libre : arrondie à la mesure la plus proche quand elle en est proche (± 1/8)
        auto l = recPos;
        if (bars == 0 && barSamples > 0.0)
        {
            const auto b = (double) l / barSamples;
            const auto rb = std::max (1.0, std::round (b));
            if (std::abs (b - rb) < 0.125 && rb * barSamples <= (double) bufL.size()) l = (int) std::lround (rb * barSamples);
        }
        // si la boucle est allongée par l'arrondi, la fin est du silence
        for (int i = recPos; i < l; ++i) { bufL[(size_t) i] = 0.0f; bufR[(size_t) i] = 0.0f; }
        len = std::max (16, l);
        pos = 0.0;
        state = thenOverdub ? looper::overdubbing : looper::playing;
    }

    double sr = 48000.0, pos = 0.0, ppqNow = 0.0;
    bool transport = false, overdubAfterClose = false;
    int state = looper::empty, len = 0, recPos = 0, lastAction = -1;
    float outPeak = 0.0f;
    std::atomic<int> stateNow { 0 };
    std::atomic<float> phaseNow { 0.0f }, lengthNow { 0.0f };
    std::vector<float> bufL, bufR;
};

} // namespace grv::effects
