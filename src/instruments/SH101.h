/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Mono à mélangeur » — principe des petits monophoniques japonais du début des années 80 à un
    seul oscillateur et mélangeur de formes :
      - un oscillateur (16' à 2') dont on mélange l'impulsion (largeur manuelle, au LFO ou à
        l'enveloppe), la scie, le sous-oscillateur (carré une ou deux octaves plus bas, ou
        impulsion étroite deux octaves plus bas) et le bruit ;
      - filtre passe-bas 24 dB/oct résonant (auto-oscillation), modulé par l'enveloppe, le LFO
        et le clavier ;
      - une enveloppe ADSR déclenchée par la porte, par la porte et chaque note, ou par le LFO ;
        ampli à l'enveloppe ou à la porte ;
      - LFO triangle, carré, aléatoire ou bruit ; portamento arrêt / marche / auto (notes liées) ;
      - arpégiateur (celui du moteur) ; le séquenceur à pas de la machine d'origine est celui de
        la groovebox.
    Architecture propre : ni schéma, ni nom, ni son d'origine repris.
*/

#pragma once

#include "../synth/SynthBase.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace sh101
{
    enum Slot { range = 0, tune, vcoLfo, pwmSrc, pw, pulseLvl, sawLvl, subLvl, subType, noiseLvl,
                cutoff, reso, envAmt, lfoAmt, kbd, lfoWave, lfoRate, lfoSync,
                attack, decay, sustain, release, envTrig, vcaMode, porta, portaMode, level, lfoDelay, numSlots };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "sh101";
            m.name = "Mono First";
            m.family = "Basses";
            m.principle = "Un oscillateur, m\xC3\xA9langeur impulsion / scie / sous-oscillateur / bruit, filtre 24 dB r\xC3\xA9sonant, enveloppe d\xC3\xA9" "clench\xC3\xA9" "e par la porte ou le LFO, arp\xC3\xA8ge";
            m.params = {
                C ("range", "Registre", { "16'", "8'", "4'", "2'" }, 1),
                P ("tune", "Accord", "dt", -12, 12, 0),
                P ("vcolfo", "Hauteur \xE2\x86\x90 LFO", "%", 0, 100, 0),
                C ("pwmsrc", "Largeur pilot\xC3\xA9" "e par", { "Manuel", "LFO", "Enveloppe" }, 0),
                P ("pw", "Largeur", "%", 0, 100, 0),
                P ("pulse", "Impulsion", "%", 0, 100, 0),
                P ("saw", "Scie", "%", 0, 100, 80),
                P ("sub", "Sous-osc.", "%", 0, 100, 40),
                C ("subtype", "Sous-osc. forme", { "Carr\xC3\xA9 -1 oct.", "Carr\xC3\xA9 -2 oct.", "Impulsion -2 oct." }, 0),
                P ("noise", "Bruit", "%", 0, 100, 0),
                P ("cutoff", "Coupure", "Hz", 30, 16000, 900, 0.3f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 25),
                P ("envamt", "Enveloppe", "%", 0, 100, 45),
                P ("lfoamt", "Mod. LFO", "%", 0, 100, 0),
                P ("kbd", "Suivi clavier", "%", 0, 100, 50),
                C ("lfowave", "Forme du LFO", { "Triangle", "Carr\xC3\xA9", "Al\xC3\xA9" "atoire", "Bruit" }, 0),
                P ("lforate", "Vitesse du LFO", "Hz", 0.05f, 30, 4, 0.35f),
                C ("lfosync", "Synchro", lfoSyncChoices(), 0),
                P ("attack", "Attaque", "ms", 0.5f, 4000, 2, 0.4f),
                P ("decay", "D\xC3\xA9" "clin", "ms", 5, 6000, 350, 0.4f),
                P ("sustain", "Tenue", "%", 0, 100, 40),
                P ("release", "Rel\xC3\xA2" "chement", "ms", 5, 6000, 120, 0.4f),
                C ("envtrig", "D\xC3\xA9" "clenchement", { "Porte + note", "Porte", "LFO" }, 0),
                C ("vcamode", "Ampli", { "Enveloppe", "Porte" }, 0),
                P ("porta", "Portamento", "ms", 0, 1500, 60, 0.4f),
                C ("portamode", "Portamento", { "Arr\xC3\xAAt", "Marche", "Auto (li\xC3\xA9" "es)" }, 0),
                P ("level", "Niveau", "dB", -24, 6, -6),
                P ("lfodelay", "Retard du LFO", "ms", 0, 10000, 0, 0.35f),
            };
            m.sections = {
                { "vco", "Oscillateur", "osc", { "range", "tune", "vcolfo", "pwmsrc", "pw" }, "osc" },
                { "mix", "M\xC3\xA9langeur", "osc", { "pulse", "saw", "sub", "subtype", "noise" }, "osc" },
                { "filter", "Filtre", "filter", { "cutoff", "reso", "envamt", "lfoamt", "kbd" }, "filter" },
                { "env", "Enveloppe", "env", { "attack", "decay", "sustain", "release", "envtrig", "vcamode" }, "env" },
                { "lfo", "LFO", "lfo", { "lfowave", "lforate", "lfosync", "lfodelay" }, "mod" },
                { "global", "Jeu", "global", { "porta", "portamode", "level" }, "global" },
            };
            m.presets = {
                { "Init", { { "saw", 100 }, { "sub", 0 }, { "cutoff", 16000 }, { "reso", 0 }, { "envamt", 0 }, { "sustain", 100 }, { "decay", 600 }, { "level", -11.3f } } },
                { "Basse ronde", { { "level", -2.8f } } },
                { "Basse sous-oscillateur", { { "saw", 30 }, { "sub", 100 }, { "subtype", 1 }, { "cutoff", 350 }, { "reso", 15 }, { "envamt", 35 }, { "decay", 250 }, { "sustain", 20 }, { "level", -3.2f } } },
                { "Basse pulsation", { { "saw", 0 }, { "pulse", 90 }, { "pw", 40 }, { "pwmsrc", 1 }, { "lforate", 0.6f }, { "sub", 50 }, { "cutoff", 600 }, { "reso", 35 }, { "level", -6.5f } } },
                { "Acide doux", { { "reso", 80 }, { "cutoff", 300 }, { "envamt", 70 }, { "decay", 180 }, { "sustain", 0 }, { "portamode", 2 }, { "porta", 80 }, { "level", 2.3f } } },
                { "Lead per\xC3\xA7" "ant", { { "range", 2 }, { "saw", 70 }, { "pulse", 50 }, { "pw", 30 }, { "cutoff", 3000 }, { "reso", 45 }, { "envamt", 30 }, { "vcolfo", 4 }, { "lforate", 5.5f }, { "portamode", 2 }, { "porta", 50 }, { "sustain", 80 }, { "level", -4.3f } } },
                { "Pluck", { { "saw", 60 }, { "pulse", 40 }, { "cutoff", 400 }, { "reso", 30 }, { "envamt", 75 }, { "decay", 220 }, { "sustain", 0 }, { "release", 200 }, { "level", 3.3f } } },
                { "Arp\xC3\xA8ge au LFO", { { "envtrig", 2 }, { "lforate", 8 }, { "lfowave", 1 }, { "cutoff", 700 }, { "envamt", 60 }, { "decay", 90 }, { "sustain", 0 }, { "level", 0.1f } } },
                { "Bruit de vent", { { "saw", 0 }, { "sub", 0 }, { "noise", 100 }, { "cutoff", 1500 }, { "reso", 85 }, { "lfoamt", 50 }, { "lforate", 0.3f }, { "attack", 1200 }, { "sustain", 90 }, { "release", 1500 }, { "level", -4.5f } } },
                { "Percussion", { { "saw", 0 }, { "sub", 0 }, { "noise", 80 }, { "pulse", 40 }, { "cutoff", 2500 }, { "reso", 60 }, { "envamt", 80 }, { "attack", 0.5f }, { "decay", 120 }, { "sustain", 0 }, { "level", 2.8f } } },
                { "Sifflement r\xC3\xA9sonant", { { "saw", 0 }, { "sub", 0 }, { "reso", 100 }, { "cutoff", 800 }, { "kbd", 100 }, { "envamt", 0 }, { "sustain", 100 }, { "level", -3.3f } } },
                { "Al\xC3\xA9" "atoire au filtre", { { "lfowave", 2 }, { "lfosync", 9 }, { "lfoamt", 60 }, { "cutoff", 700 }, { "reso", 55 }, { "sustain", 80 }, { "level", -5.4f } } },
            };
            HubSpec hub;
            hub.envs = 0; hub.lfos = 0; hub.slots = 4; hub.arp = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            return m;
        }();
        return d;
    }
}

class Sh101Voice
{
public:
    void prepare (double s) { sr = s; env.prepare (s); reset(); }
    void reset() noexcept { osc.reset(); sawBl.reset(); subBl.reset(); sub = 0.0; subCount = 0; lastPhase = 0.0; filter.reset(); env.reset(); gate = false; active = false; lfoPhase = 0.0; held = 0.0; gateAmp = 0.0; }
    void setSpread (int, int) noexcept {}

    void start (int n, float, bool legatoStart, int) noexcept
    {
        pitch.to ((double) n);
        legatoNow = legatoStart && active;
        if (! legatoNow) lfoAge = 0.0; // retard du LFO : repart à chaque note détachée
        if (! active) pitch.jump ((double) n);
        retrigPending = true;
        gate = true; active = true;
    }
    void release() noexcept { gate = false; retrigPending = false; env.gate (false); } // une note en attente (accord relâché) ne rouvre pas l'enveloppe
    bool isActive() const noexcept { return active; }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace sh101;
        env.set (p[sh101::attack], p[sh101::decay], p[sh101::sustain] / 100.0, p[sh101::release]);
        const auto trig = (int) p[envTrig];
        if (retrigPending)
        {
            retrigPending = false;
            // porte + note : chaque note relance l'enveloppe ; porte : seulement après un silence
            env.gate (true, trig == 0 || ! legatoNow);
            glideOn = (int) p[portaMode] == 1 || ((int) p[portaMode] == 2 && legatoNow);
        }
        static constexpr double rangeOff[] { -12.0, 0.0, 12.0, 24.0 };
        const auto base = rangeOff[std::clamp ((int) p[range], 0, 3)] + p[tune];
        const auto glideCoef = glideOn && p[porta] > 0.5f ? std::exp (-1.0 / std::max (1.0, p[porta] * 0.001 * sr * 0.3)) : 0.0;
        const auto lfoHz = (double) p[lfoRate];
        const auto lw = (int) p[lfoWave];
        const auto lfoDelaySec = p[sh101::lfoDelay] * 0.001;
        const auto pwm = (int) p[pwmSrc];
        const auto pwManual = p[pw] / 100.0;
        const auto gPulse = p[pulseLvl] / 100.0, gSaw = p[sawLvl] / 100.0, gSub = p[subLvl] / 100.0, gNoise = p[noiseLvl] / 100.0;
        const auto st = (int) p[subType];
        const auto res = p[reso] / 100.0 * 1.02;
        const auto cutLog = std::log2 ((double) p[cutoff]);
        const auto eAmt = p[envAmt] / 100.0 * 8.0, lAmt = p[lfoAmt] / 100.0 * 4.0, kAmt = p[kbd] / 100.0;
        const auto pAmt = p[vcoLfo] / 100.0 * 12.0;
        const bool gateVca = (int) p[vcaMode] == 1;
        const auto out = dbToGain (p[level]) * 2.0; // +6 dB de réserve : les sons percussifs atteignent le niveau commun
        const auto gateCoef = std::exp (-1.0 / (0.002 * sr));

        for (int i = 0; i < n; ++i)
        {
            // LFO
            lfoPhase += lfoHz / sr;
            bool cycled = false;
            if (lfoPhase >= 1.0) { lfoPhase -= 1.0; cycled = true; rng = rng * 1664525u + 1013904223u; held = (double) (rng >> 8) / 8388608.0 - 1.0; }
            double lfo;
            switch (lw)
            {
                case 0: lfo = 1.0 - 4.0 * std::abs (lfoPhase - 0.5); break;
                case 1: lfo = lfoPhase < 0.5 ? 1.0 : -1.0; break;
                case 2: lfo = held; break;
                default: rng2 = rng2 * 1664525u + 1013904223u; lfo = (double) (rng2 >> 8) / 8388608.0 - 1.0; break;
            }
            if (trig == 2 && cycled && gate) env.gate (true, true); // déclenchement par le LFO
            lfoAge += 1.0 / sr;
            lfo *= lfoDelaySec > 0.0 ? std::clamp ((lfoAge - lfoDelaySec) / (0.5 * lfoDelaySec), 0.0, 1.0) : 1.0;
            const auto e = env.next();
            gateAmp = (gate ? 1.0 : 0.0) + (gateAmp - (gate ? 1.0 : 0.0)) * gateCoef;

            const auto note = pitch.next (glideCoef) + base + pAmt * lfo;
            const auto dt = noteHz (note) / sr;
            const auto width = std::clamp (pwm == 0 ? 0.5 - 0.45 * pwManual : pwm == 1 ? 0.5 - 0.45 * pwManual * (0.5 + 0.5 * lfo) : 0.5 - 0.45 * pwManual * e, 0.04, 0.5);
            // v0.22 : scie et sous-oscillateur corrigés par BLEP à l'instant exact du bouclage de
            // l'oscillateur, et alignés sur l'impulsion (même retard d'un échantillon)
            const auto phPre = osc.getPhase();
            double wrapT = -1.0;
            const auto pulse = osc.next (Osc::pulse, dt, width, -1.0, &wrapT);
            auto subValue = [st] (int c) { return st == 0 ? ((c & 1) ? 1.0 : -1.0) : st == 1 ? ((c & 2) ? 1.0 : -1.0) : ((c & 3) == 0 ? 1.0 : -0.33); };
            const auto subNow = subValue (subCount);
            if (wrapT > 0.0)
            {
                sawBl.step (-2.0, wrapT);
                // sous-oscillateur : bascules sur le bouclage de l'oscillateur (division par 2 ou par 4)
                ++subCount;
                const auto jump = subValue (subCount) - subNow;
                if (jump != 0.0) subBl.step (jump, wrapT);
            }
            const auto saw = sawBl.push (2.0 * phPre - 1.0);
            sub = subBl.push (subNow);
            rng3 = rng3 * 1664525u + 1013904223u;
            const auto noise = (double) (rng3 >> 8) / 8388608.0 - 1.0;
            const auto x = (pulse * gPulse + saw * gSaw + sub * gSub + noise * gNoise) * 0.45 + noise * 1.0e-5; // souffle de fond : amorce l'auto-oscillation

            const auto fc = fastExp2 (cutLog + eAmt * e + lAmt * lfo + kAmt * (note - 60.0) / 12.0);
            const auto y = filter.process (x, std::clamp (fc, 20.0, 0.45 * sr), res, sr, Ladder::lp24, 1.2);
            const auto a = gateVca ? gateAmp : e;
            const auto v = y * a * out;
            L[i] += (float) v;
            R[i] += (float) v;
        }
        if (! gate && ! env.isActive() && gateAmp < 1.0e-4) active = false;
    }

private:
    double sr = 48000.0, sub = 0.0, lastPhase = 0.0, lfoPhase = 0.0, held = 0.0, gateAmp = 0.0, lfoAge = 1.0e9;
    int subCount = 0;
    bool gate = false, active = false, retrigPending = false, legatoNow = false, glideOn = false;
    std::uint32_t rng = 0x2468ACEu, rng2 = 0x13579BDu, rng3 = 0xFEDCBA9u;
    Osc osc;
    BlepLine sawBl, subBl;
    Ladder filter;
    Adsr env;
    Glide pitch;
};

class Sh101Synth final : public SynthBase<Sh101Voice, 1>
{
public:
    const ModuleDesc& desc() const noexcept override { return sh101::desc(); }
protected:
    void configure (const float*) noexcept override { setMode (true, true, 1); }
    const float* adjust (const float* p) noexcept override { return syncRates (p, sh101::numSlots, { { sh101::lfoRate, sh101::lfoSync } }); }
};

} // namespace grv::instruments
