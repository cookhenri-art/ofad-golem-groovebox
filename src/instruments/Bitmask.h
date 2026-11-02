/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Komodo Dragon » — principe de la synthèse par masques de bits (bitmask) des synthés
    numériques récents (architecture reprise ; ni nom, ni façade, ni son, ni masque d'origine) :
      - on part d'un sinus ; sa période est découpée en segments (8 à 256) ; un masque de 8 bits,
        répété le long de la période, dit pour chaque segment s'il garde le sinus (bit à 1) ou s'il
        le coupe, l'inverse ou le redresse (bit à 0) : 256 masques par oscillateur ;
      - deux masques par oscillateur (A et B) et un passage de l'un à l'autre (« delta »), réglé à
        la main et par une enveloppe delta (attaque, déclin, en boucle) : le timbre se transforme ;
      - 2 oscillateurs par voix (le 2e transposé et désaccordé), un bruit échantillonné-bloqué ;
      - filtre d'état 12 dB (passe-bas, bande, haut, réjection) résonant, suivi du clavier ;
      - enveloppe de hauteur, enveloppes du filtre et de l'ampli ; 2 LFO, 1 enveloppe et 8
        routages, arpège ; 10 voix.
    Modèle : gain de segment adouci sur environ deux échantillons à chaque bord (moins de
    repliement qu'un découpage net) ; hauteur et enveloppes par bloc ou par échantillon ; aucune
    latence ajoutée.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

#include <array>
#include <cmath>

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace bitm
{
    enum Slot
    {
        res = 0, mMode, o1A, o1B, o2A, o2B, o2Semi, o2Fine, mix1, mix2, noise, noiseHz,
        dAmt, dEnv, dA, dD, dLoop, pAmt, pDec,
        fType, fCut, fRes, fEnv, fKey, fA, fD, fS, fR,
        aa, ad, as, ar, velAmp,
        voices, vMode, glide, tune, spread, level,
        numSlots
    };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            ModuleDesc m;
            m.type = "bitm";
            m.name = "Komodo Dragon";
            m.family = "Drones";
            m.principle = "Synth\xC3\xA8se par masques de bits : sinus d\xC3\xA9" "coup\xC3\xA9 en segments, masques de 8 bits (256 par oscillateur), passage d'un masque \xC3\xA0 l'autre par une enveloppe delta, filtre d'\xC3\xA9tat 12 dB";
            m.params = {
                C ("res", "Segments", { "8", "16", "32", "64", "128", "256" }, 0),
                C ("mmode", "Bit \xC3\xA0 z\xC3\xA9ro", { "Coupe", "Inverse", "Redresse" }, 0),
                I ("o1a", "Osc. 1 \xC2\xB7 masque A", "", 0, 255, 255),
                I ("o1b", "Osc. 1 \xC2\xB7 masque B", "", 0, 255, 85),
                I ("o2a", "Osc. 2 \xC2\xB7 masque A", "", 0, 255, 255),
                I ("o2b", "Osc. 2 \xC2\xB7 masque B", "", 0, 255, 51),
                I ("o2semi", "Osc. 2 \xC2\xB7 demi-tons", "dt", -24, 24, 0),
                P ("o2fine", "Osc. 2 \xC2\xB7 fin", "ct", -50, 50, 6),
                P ("mix1", "Osc. 1", "%", 0, 100, 80),
                P ("mix2", "Osc. 2", "%", 0, 100, 0),
                P ("noise", "Bruit", "%", 0, 100, 0),
                P ("noisehz", "Bruit \xC2\xB7 \xC3\xA9" "chantillonnage", "Hz", 100, 24000, 24000, 0.3f),
                P ("damt", "Delta", "%", 0, 100, 0),
                P ("denv", "Enveloppe delta", "%", -100, 100, 0),
                P ("da", "Delta \xC2\xB7 attaque", "ms", 1, 10000, 300, 0.3f),
                P ("dd", "Delta \xC2\xB7 d\xC3\xA9" "clin", "ms", 5, 10000, 900, 0.3f),
                C ("dloop", "Delta en boucle", onOff),
                P ("pamt", "Enveloppe de hauteur", "dt", -24, 24, 0),
                P ("pdec", "Hauteur \xC2\xB7 d\xC3\xA9" "clin", "ms", 5, 3000, 120, 0.3f),
                C ("ftype", "Filtre", { "Passe-bas", "Passe-bande", "Passe-haut", "R\xC3\xA9jection" }),
                P ("fcut", "Coupure", "Hz", 20, 18000, 6000, 0.3f),
                P ("fres", "R\xC3\xA9sonance", "%", 0, 100, 15),
                P ("fenv", "Qt\xC3\xA9 env.", "%", -100, 100, 20),
                P ("fkey", "Clavier", "%", 0, 100, 50),
                P ("fa", "Attaque", "ms", 0.5f, 10000, 2, 0.3f),
                P ("fd", "D\xC3\xA9" "clin", "ms", 5, 10000, 500, 0.3f),
                P ("fs", "Maintien", "%", 0, 100, 30),
                P ("fr", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 400, 0.3f),
                P ("aa", "Attaque", "ms", 0.5f, 10000, 2, 0.3f),
                P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 800, 0.3f),
                P ("as", "Maintien", "%", 0, 100, 80),
                P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 300, 0.3f),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 40),
                I ("voices", "Voix", "", 1, 10, 10),
                C ("vmode", "Jeu", { "Poly", "Mono", "Mono legato" }),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("tune", "Transposition", "dt", -24, 24, 0),
                P ("spread", "Largeur st\xC3\xA9r\xC3\xA9o", "%", 0, 100, 30),
                P ("level", "Volume", "dB", -30, 6, -16),
            };
            m.sections = {
                { "mask", "Masques", "osc", { "res", "mmode", "o1a", "o1b", "o2a", "o2b", "o2semi", "o2fine" }, "osc" },
                { "mix", "M\xC3\xA9langeur", "mix", { "mix1", "mix2", "noise", "noisehz" }, "osc" },
                { "delta", "Delta", "env", { "damt", "denv", "da", "dd", "dloop" }, "env" },
                { "pitch", "Hauteur", "env", { "pamt", "pdec" }, "env" },
                { "filter", "Filtre", "filter", { "ftype", "fcut", "fres", "fenv", "fkey" }, "filter" },
                { "fenvs", "Enveloppe filtre", "env", { "fa", "fd", "fs", "fr" }, "env" },
                { "amp", "Ampli", "amp", { "aa", "ad", "as", "ar", "velamp" }, "amp" },
                { "global", "Global", "global", { "voices", "vmode", "glide", "tune", "spread", "level" }, "global" },
            };
            using V = std::vector<std::pair<std::string, float>>;
            m.presets = {
                { "Init", V {}, "Init" },
                { "Dragon qui respire", V { { "o1a", 255 }, { "o1b", 170 }, { "res", 1 }, { "denv", 70 }, { "da", 1500 }, { "dd", 2500 }, { "dloop", 1 }, { "aa", 600 }, { "ar", 2000 },
                                            { "mix2", 50 }, { "o2a", 238 }, { "o2b", 102 }, { "spread", 70 }, { "level", -16 } }, "Nappes" },
                { "Basse de masques", V { { "o1a", 255 }, { "o1b", 15 }, { "res", 0 }, { "mmode", 1 }, { "denv", 60 }, { "da", 5 }, { "dd", 250 }, { "fcut", 1400 }, { "fenv", 40 },
                                          { "fd", 250 }, { "vmode", 2 }, { "voices", 1 }, { "tune", -12 }, { "level", -0.7f } }, "Basses" },
            };
            HubSpec hub;
            hub.envs = 1; hub.lfos = 2; hub.firstLfo = 1; hub.slots = 8; hub.arp = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m); // banque d'usine, au moins 10 sons par catégorie (FactoryBanks.h)
            return m;
        }();
        return d;
    }

    /** Gain du segment (0..1) à la phase ph : bit du masque répété tous les 8 segments, bords
        adoucis sur `w` (en fraction de segment). */
    inline double maskGain (int mask, double ph, int nseg, double w) noexcept
    {
        const auto x = ph * nseg;
        const auto seg = (int) x;
        const auto u = x - seg;
        const auto cur = (double) ((mask >> (seg & 7)) & 1);
        if (u >= w) return cur;
        const auto prev = (double) ((mask >> ((seg + 7) & 7)) & 1);
        const auto t = u / w;
        return prev + (cur - prev) * t * t * (3.0 - 2.0 * t);
    }
}

/** Enveloppe delta : monte en `a`, redescend en `d` ; en boucle tant que la touche est tenue. */
class BitmDeltaEnv
{
public:
    void reset() noexcept { v = 0.0; st = idle; }
    void gate (bool on) noexcept { if (on) st = up; else if (st != idle) st = idle; }
    double next (double aS, double dS, bool loop, double sr) noexcept
    {
        switch (st)
        {
            case up: v += 1.0 / std::max (1.0, aS * sr); if (v >= 1.0) { v = 1.0; st = down; } break;
            case down: v -= 1.0 / std::max (1.0, dS * sr); if (v <= 0.0) { v = 0.0; st = loop ? up : idle; } break;
            default: break;
        }
        return v;
    }
private:
    enum Stage { idle, up, down };
    double v = 0.0;
    Stage st = idle;
};

class BitmVoice
{
public:
    void prepare (double s) { sr = s; fenv.prepare (s); aenv.prepare (s); reset(); }
    void reset() noexcept
    {
        fenv.reset(); aenv.reset(); denv.reset(); filt.reset(); dc.reset();
        ph1 = ph2 = 0.0; shHeld = 0.0; shPh = 0.0; pEnv = 0.0;
        active = pending = releasePending = false;
    }
    void setIndex (int i) noexcept { static constexpr double spots[8] { 0.12, -0.12, 0.45, -0.45, 0.75, -0.75, 0.3, -0.3 }; pan = spots[i & 7]; rng = dsp::WhiteNoise (0x51ED27u + 977u * (std::uint32_t) i); }
    void setSpread (int index, int count) noexcept { uni = count > 1 ? 2.0 * index / (count - 1) - 1.0 : 0.0; }
    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; releasePending = false; active = true;
    }
    void release() noexcept { if (active) releasePending = true; }
    bool isActive() const noexcept { return active; }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace bitm;
        if (pending)
        {
            pending = false;
            if (p[glide] > 0.5f && from >= 0 && legatoPending) pitch.to ((double) target);
            else if (p[glide] > 0.5f && from >= 0) { pitch.jump ((double) from); pitch.to ((double) target); }
            else pitch.jump ((double) target);
            if (! legatoPending) { fenv.gate (true); aenv.gate (true); denv.gate (true); pEnv = 1.0; }
        }
        if (releasePending) { releasePending = false; fenv.gate (false); aenv.gate (false); denv.gate (false); }
        fenv.set (p[fA], p[fD], p[fS] / 100.0, p[fR]);
        aenv.set (p[aa], p[ad], p[as] / 100.0, p[ar]);

        // hauteur : portamento + enveloppe de hauteur (décroissance exponentielle par bloc)
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-(double) n / (p[glide] * 0.001 * sr * 0.35));
        const auto pitchNow = pitch.next (glideCoef);
        const auto note = pitchNow + p[tune] + uni * 0.1 + p[pAmt] * pEnv;
        pEnv *= std::exp (-(double) n / std::max (1.0, p[pDec] * 0.001 * sr * 0.3));
        const auto f1 = noteHz (note), f2 = noteHz (note + p[o2Semi] + p[o2Fine] / 100.0);
        const auto dt1 = std::clamp (f1 / sr, 1.0e-7, 0.45), dt2 = std::clamp (f2 / sr, 1.0e-7, 0.45);
        static constexpr int segs[6] { 8, 16, 32, 64, 128, 256 };
        const int nseg = segs[std::clamp ((int) std::lround (p[res]), 0, 5)];
        // bord adouci : deux échantillons, au plus la moitié d'un segment
        const auto w1 = std::min (0.5, 2.0 * dt1 * nseg), w2 = std::min (0.5, 2.0 * dt2 * nseg);
        const int mode = std::clamp ((int) std::lround (p[mMode]), 0, 2);
        const int a1 = (int) p[o1A], b1 = (int) p[o1B], a2 = (int) p[o2A], b2 = (int) p[o2B];
        const auto m1 = p[mix1] / 100.0, m2 = p[mix2] / 100.0, mn = p[noise] / 100.0;
        const auto shInc = std::clamp ((double) p[noiseHz] / sr, 0.0, 1.0);
        const auto dBase = p[dAmt] / 100.0, dDepth = p[dEnv] / 100.0;
        const auto daS = p[dA] * 0.001, ddS = p[dD] * 0.001;
        const bool dl = p[dLoop] > 0.5f;
        const int ft = std::clamp ((int) std::lround (p[fType]), 0, 3);
        const auto rs = p[fRes] / 100.0;
        const auto keyOct = (p[fKey] / 100.0) * (note - 60.0) / 12.0;
        const auto envOct = (p[fEnv] / 100.0) * 7.0;
        const auto gain = dbToGain (p[level]) * velocityGain (vel, p[velAmp] / 100.0);
        const auto sp = p[spread] / 100.0;
        const auto pv = std::clamp (pan * sp + uni * sp * 0.8, -1.0, 1.0);
        const auto gL = std::sqrt (0.5 * (1.0 - pv)) * 1.4142, gR = std::sqrt (0.5 * (1.0 + pv)) * 1.4142;
        SvfCoefs fc;

        auto shape = [mode] (double s, double g) noexcept
        {
            switch (mode)
            {
                case 1: return s * (2.0 * g - 1.0);                 // inverse
                case 2: return s * g + std::abs (s) * (1.0 - g);   // redresse
                default: return s * g;                              // coupe
            }
        };

        for (int i = 0; i < n; ++i)
        {
            const auto d = std::clamp (dBase + dDepth * denv.next (daS, ddS, dl, sr), 0.0, 1.0);
            double x = 0.0;
            if (m1 > 0.0)
            {
                const auto g = maskGain (a1, ph1, nseg, w1) * (1.0 - d) + maskGain (b1, ph1, nseg, w1) * d;
                x += m1 * shape (fastSin2Pi (ph1), g);
            }
            if (m2 > 0.0)
            {
                const auto g = maskGain (a2, ph2, nseg, w2) * (1.0 - d) + maskGain (b2, ph2, nseg, w2) * d;
                x += m2 * shape (fastSin2Pi (ph2), g);
            }
            ph1 += dt1; if (ph1 >= 1.0) ph1 -= 1.0;
            ph2 += dt2; if (ph2 >= 1.0) ph2 -= 1.0;
            if (mn > 0.0)
            {
                shPh += shInc;
                if (shPh >= 1.0) { shPh -= 1.0; shHeld = rng.next(); }
                x += mn * shHeld * 0.6;
            }
            const auto fe = fenv.next();
            if ((i & 7) == 0) fc.set (std::clamp (p[fCut] * fastExp2 (envOct * fe + keyOct), 16.0, 0.45 * sr), rs, sr);
            const auto o = filt.process (x, fc);
            x = ft == 0 ? o.low : ft == 1 ? o.band : ft == 2 ? o.high : o.low + o.high;
            const auto y = dc.process (x) * aenv.next() * gain;
            L[i] += (float) (y * gL);
            R[i] += (float) (y * gR);
        }
        if (! aenv.isActive()) active = false;
    }

private:
    double sr = 48000.0, pan = 0.0, uni = 0.0, ph1 = 0.0, ph2 = 0.0, shPh = 0.0, shHeld = 0.0, pEnv = 0.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool active = false, pending = false, releasePending = false, legatoPending = false;
    Adsr fenv, aenv;
    BitmDeltaEnv denv;
    StateFilter filt;
    Glide pitch;
    DcBlocker dc;
    dsp::WhiteNoise rng;
};

class BitmSynth final : public SynthBase<BitmVoice, 10>
{
public:
    BitmSynth() { for (int i = 0; i < 10; ++i) voices[(size_t) i].setIndex (i); }
    const ModuleDesc& desc() const noexcept override { return bitm::desc(); }

protected:
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[bitm::voices], 1, 10);
        switch ((int) p[bitm::vMode])
        {
            case 1: setMode (true, false, v); break;
            case 2: setMode (true, true, v); break;
            default: setMode (false, true, v); break;
        }
    }
};

} // namespace grv::instruments
