/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Basse acide » — principe des basses monophoniques à séquenceur des années 80 :
    un oscillateur (dent de scie ou carré), filtre à diodes à 4 pôles résonant, enveloppe de filtre
    à déclin seul, accent (vélocité au-dessus d'un seuil) qui raccourcit et renforce l'enveloppe
    et s'accumule sur les accents rapprochés, glissé entre notes liées (une note qui dure plus
    d'un pas glisse vers la suivante).
    Architecture propre : ni schéma, ni nom, ni son d'origine repris.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace acid
{
    enum Slot { wave = 0, tune, fine, cutoff, reso, envmod, decay, accent, glide, drive, ampDecay,
                accentThreshold, level, keytrack, dist, numSlots };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "acid";
            m.name = "Acid Lover";
            m.family = "Basses";
            m.principle = "Basse monophonique \xC3\xA0 filtre \xC3\xA0 diodes, accent et gliss\xC3\xA9";
            m.params = {
                C ("wave", "Forme", { "Scie", "Carr\xC3\xA9" }),
                P ("tune", "Accord", "dt", -12, 12, 0),
                P ("fine", "Fin", "ct", -50, 50, 0),
                P ("cutoff", "Coupure", "Hz", 40, 8000, 420, 0.3f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 65),
                P ("envmod", "Modulation", "%", 0, 100, 55),
                P ("decay", "D\xC3\xA9" "clin", "ms", 150, 2500, 500, 0.5f),
                P ("accent", "Accent", "%", 0, 100, 60),
                P ("glide", "Gliss\xC3\xA9", "ms", 10, 300, 60, 0.5f),
                P ("drive", "Saturation", "dB", 0, 24, 3),
                P ("ampdecay", "D\xC3\xA9" "clin ampli", "ms", 100, 4000, 1500, 0.4f),
                P ("accthr", "Seuil accent", "%", 50, 100, 80),
                P ("level", "Niveau", "dB", -24, 6, -4),
                P ("keytrack", "Suivi clavier", "%", 0, 100, 20),
                P ("dist", "Distorsion", "%", 0, 100, 0),
            };
            m.sections = {
                { "global", "Global", "global", { "wave", "tune", "fine", "glide", "accthr", "level" }, "global" },
                { "filter", "Filtre", "filter", { "cutoff", "reso", "envmod", "decay", "accent", "keytrack", "drive" }, "filter" },
                { "env", "Enveloppe", "env", { "envmod", "decay", "accent" }, "env" },
                { "amp", "Amplitude", "amp", { "ampdecay", "dist", "level" }, "amp" },
            };
            m.presets = {
                { "Acide classique", {} },
                { "Hurlement", { { "cutoff", 300 }, { "reso", 88 }, { "envmod", 80 }, { "decay", 350 }, { "accent", 85 }, { "dist", 45 } } },
                { "Basse ronde", { { "wave", 1 }, { "cutoff", 260 }, { "reso", 30 }, { "envmod", 30 }, { "decay", 900 }, { "accent", 30 } } },
                { "Squelch", { { "cutoff", 180 }, { "reso", 95 }, { "envmod", 95 }, { "decay", 220 }, { "accent", 100 }, { "drive", 12 } } },
            };
            addFactoryBank (m); // banque d'usine : 50 sons par catégories (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

class AcidVoice
{
public:
    void prepare (double s) { sr = s; filter.prepare (s); dc.prepare (s); reset(); }
    void reset() noexcept { osc.reset(); filter.reset(); distClip.reset(); dc.reset(); amp = 0.0; menv = 0.0; accentCap = 0.0; gate = false; active = false; }

    void setSpread (int, int) noexcept {}
    void start (int n, float velocity, bool legatoStart, int) noexcept
    {
        pitch.to ((double) n);
        if (velocity >= 0.0f) vel = velocity;
        if (! legatoStart)
        {
            if (! active) pitch.jump ((double) n);
            menv = 1.0;              // l'enveloppe de filtre repart
            ampDecayEnv = 1.0;
            accentOn = false;
            pendingAccent = true;
        }
        sliding = legatoStart;
        gate = true;
        active = true;
    }

    void release() noexcept { gate = false; }
    bool isActive() const noexcept { return active; }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace acid;
        if (pendingAccent) { accentOn = vel * 100.0f >= p[accentThreshold]; pendingAccent = false; }
        const auto accAmt = accentOn ? p[accent] / 100.0 : 0.0;
        // accent : enveloppe plus courte et plus marquée
        const auto decayMs = accentOn ? std::min ((double) p[decay], 220.0) : (double) p[decay];
        const auto menvCoef = std::exp (std::log (0.01) / std::max (1.0, decayMs * 0.001 * sr));
        const auto ampCoef = std::exp (std::log (0.05) / std::max (1.0, p[ampDecay] * 0.001 * sr));
        const auto relCoef = std::exp (std::log (0.001) / (0.008 * sr));
        const auto attackCoef = std::exp (-1.0 / (0.003 * sr));
        const auto glideCoef = sliding ? std::exp (-1.0 / std::max (1.0, p[acid::glide] * 0.001 * sr * 0.5)) : 0.0;
        const auto accentCapCoef = std::exp (-1.0 / (0.12 * sr));
        const auto res = p[reso] / 100.0;
        const auto envAmt = p[envmod] / 100.0;
        const auto baseNote = (double) p[tune] + (double) p[fine] / 100.0;
        const auto driveGain = dbToGain (p[drive]);
        const auto distAmt = p[dist] / 100.0;
        const auto gain = dbToGain (p[acid::level]) * (1.0 + 0.6 * accAmt) / (1.0 + 0.8 * res * res);
        const auto pw = 0.5;
        const auto waveform = p[wave] < 0.5f ? Osc::saw : Osc::pulse;

        for (int i = 0; i < n; ++i)
        {
            const auto note = pitch.next (glideCoef) + baseNote;
            const auto hz = noteHz (note);

            // Enveloppes : attaque de 3 ms, déclin lent tant que la note est tenue, relâchement de 8 ms
            if (gate) { amp = 1.0 + (amp - 1.0) * attackCoef; ampDecayEnv *= ampCoef; }
            else amp *= relCoef;
            const auto a = amp * (0.15 + 0.85 * ampDecayEnv);
            menv *= menvCoef;
            // accent cumulé : charge d'un condensateur par les accents (effet « sweep » sur les accents rapprochés)
            accentCap = (accentOn ? menv * accAmt : 0.0) + (accentCap - (accentOn ? menv * accAmt : 0.0)) * accentCapCoef;

            const auto octaves = envAmt * 4.0 * menv + accAmt * 2.5 * menv + accentCap * 2.0
                               + (p[keytrack] / 100.0) * (note - 48.0) / 12.0;
            const auto fc = std::clamp ((double) p[cutoff] * fastExp2 (octaves), 20.0, 0.45 * sr);

            auto x = osc.next (waveform, hz / sr, pw);
            x = filter.process (x * driveGain * 0.5, fc, res);
            // saturation anti-repliée (ADAA, v0.22)
            if (distAmt > 0.0) x = x * (1.0 - distAmt) + distClip.process (x * (1.0 + distAmt * 11.0)) * 0.6 * distAmt;
            else distClip.reset();
            const auto y = dc.process (x) * a * gain;
            L[i] += (float) y;
            R[i] += (float) y;
        }
        if (! gate && amp < 1.0e-4) { active = false; amp = 0.0; }
    }

private:
    double sr = 48000.0, amp = 0.0, ampDecayEnv = 1.0, menv = 0.0, accentCap = 0.0;
    float vel = 1.0f;
    bool gate = false, active = false, accentOn = false, pendingAccent = false, sliding = false;
    Osc osc;
    DiodeLadder filter;
    AdaaSoftClip distClip;
    Glide pitch;
    DcBlocker dc;
};

class AcidBass final : public SynthBase<AcidVoice, 1>
{
public:
    const ModuleDesc& desc() const noexcept override { return acid::desc(); }
protected:
    void configure (const float*) noexcept override { setMode (true, true, 1); }
};

} // namespace grv::instruments
