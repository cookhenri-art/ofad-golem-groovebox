/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Mono semi-modulaire » — principe des monophoniques semi-modulaires japonais de la fin des
    années 70 à deux filtres en série :
      - deux oscillateurs : le 1er en triangle, scie, impulsion à largeur réglable ou bruit blanc ;
        le 2e en scie, carré, impulsion étroite ou modulation en anneau des deux ;
        tailles 32' à 4', écart du 2e oscillateur, portamento ;
      - filtre passe-haut puis filtre passe-bas, 12 dB/oct, de type Sallen-Key à réinjection
        saturée : la résonance (« peak ») crie puis auto-oscille, le son se déchire quand on pousse ;
      - générateur de modulation à forme réglable (rampe descendante → triangle → rampe montante)
        avec sortie carrée et échantillonneur-bloqueur ;
      - enveloppe 1 : retard, attaque, relâchement (modulation de hauteur) ;
      - enveloppe 2 : maintien, attaque, déclin, tenue, relâchement (filtres et amplificateur) ;
      - les commandes de modulation des panneaux de connexion par défaut (oscillateurs et filtres
        modulés par le générateur et les enveloppes), en réglages.
    Architecture propre : ni schéma, ni nom, ni son d'origine repris (filtre d'après la
    littérature publique des filtres à réinjection de type Sallen-Key en topologie TPT).
*/

#pragma once

#include "../synth/SynthBase.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace ms20
{
    enum Slot { v1wave = 0, v1scale, v1pw, v2wave, v2scale, v2pitch, v1level, v2level,
                tune, porta, pitchMg, pitchEg1,
                hpCut, hpPeak, hpMg, hpEg2, lpCut, lpPeak, lpMg, lpEg2, drive,
                e1Delay, e1Attack, e1Release, e2Hold, e2Attack, e2Decay, e2Sustain, e2Release,
                mgShape, mgRate, mgSync, modSrc, velAmp, level, mgDelay, numSlots };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "ms20";
            m.name = "Hurleur 200";
            m.family = "Basses";
            m.principle = "Deux oscillateurs, filtres passe-haut et passe-bas 12 dB \xC3\xA0 r\xC3\xA9injection satur\xC3\xA9" "e qui crient, g\xC3\xA9n\xC3\xA9rateur de modulation \xC3\xA0 forme r\xC3\xA9glable, \xC3\xA9" "chantillonneur-bloqueur";
            const std::vector<std::string> scales { "32'", "16'", "8'", "4'" };
            m.params = {
                C ("v1wave", "Forme osc. 1", { "Triangle", "Scie", "Impulsion", "Bruit" }, 1),
                C ("v1scale", "Taille osc. 1", scales, 2),
                P ("v1pw", "Largeur d'impulsion", "%", 5, 50, 50),
                C ("v2wave", "Forme osc. 2", { "Scie", "Carr\xC3\xA9", "Impulsion \xC3\xA9troite", "Anneau" }, 0),
                C ("v2scale", "Taille osc. 2", scales, 2),
                P ("v2pitch", "\xC3\x89" "cart osc. 2", "dt", -12, 12, 0),
                P ("v1level", "Niveau osc. 1", "%", 0, 100, 80),
                P ("v2level", "Niveau osc. 2", "%", 0, 100, 0),
                P ("tune", "Accord", "dt", -12, 12, 0),
                P ("porta", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("pitchmg", "Hauteur \xE2\x86\x90 g\xC3\xA9n\xC3\xA9rateur", "%", 0, 100, 0),
                P ("pitcheg1", "Hauteur \xE2\x86\x90 env. 1", "%", -100, 100, 0),
                P ("hpcut", "Coupure passe-haut", "Hz", 20, 10000, 40, 0.3f),
                P ("hppeak", "R\xC3\xA9sonance passe-haut", "%", 0, 100, 0),
                P ("hpmg", "Passe-haut \xE2\x86\x90 g\xC3\xA9n\xC3\xA9rateur", "%", 0, 100, 0),
                P ("hpeg2", "Passe-haut \xE2\x86\x90 env. 2", "%", -100, 100, 0),
                P ("lpcut", "Coupure passe-bas", "Hz", 40, 18000, 1200, 0.3f),
                P ("lppeak", "R\xC3\xA9sonance passe-bas", "%", 0, 100, 35),
                P ("lpmg", "Passe-bas \xE2\x86\x90 g\xC3\xA9n\xC3\xA9rateur", "%", 0, 100, 0),
                P ("lpeg2", "Passe-bas \xE2\x86\x90 env. 2", "%", -100, 100, 45),
                P ("drive", "Saturation", "%", 0, 100, 30),
                P ("e1delay", "Retard", "ms", 0, 2000, 0, 0.4f),
                P ("e1attack", "Attaque", "ms", 0.5f, 5000, 5, 0.4f),
                P ("e1release", "Rel\xC3\xA2" "chement", "ms", 5, 8000, 300, 0.4f),
                P ("e2hold", "Maintien", "ms", 0, 2000, 0, 0.4f),
                P ("e2attack", "Attaque", "ms", 0.5f, 5000, 3, 0.4f),
                P ("e2decay", "D\xC3\xA9" "clin", "ms", 5, 8000, 400, 0.4f),
                P ("e2sustain", "Tenue", "%", 0, 100, 60),
                P ("e2release", "Rel\xC3\xA2" "chement", "ms", 5, 8000, 250, 0.4f),
                P ("mgshape", "Forme du g\xC3\xA9n\xC3\xA9rateur", "%", 0, 100, 50),
                P ("mgrate", "Vitesse", "Hz", 0.05f, 40, 3, 0.35f),
                C ("mgsync", "Synchro", lfoSyncChoices(), 0),
                C ("modsrc", "Source de modulation", { "Rampe / triangle", "Carr\xC3\xA9", "\xC3\x89" "chantillonn\xC3\xA9-bloqu\xC3\xA9" }, 0),
                P ("velamp", "V\xC3\xA9locit\xC3\xA9 \xE2\x86\x92 ampli", "%", 0, 100, 30),
                P ("level", "Niveau", "dB", -24, 6, -6),
                P ("mgdelay", "Retard du g\xC3\xA9n\xC3\xA9rateur", "ms", 0, 10000, 0, 0.35f),
            };
            m.sections = {
                { "vco", "Oscillateurs", "osc", { "v1wave", "v1scale", "v1pw", "v1level", "v2wave", "v2scale", "v2pitch", "v2level" }, "osc" },
                { "pitch", "Hauteur", "global", { "tune", "porta", "pitchmg", "pitcheg1" }, "osc" },
                { "hpf", "Passe-haut", "filter", { "hpcut", "hppeak", "hpmg", "hpeg2" }, "filter" },
                { "lpf", "Passe-bas", "filter", { "lpcut", "lppeak", "lpmg", "lpeg2", "drive" }, "filter" },
                { "eg1", "Enveloppe 1 (hauteur)", "env", { "e1delay", "e1attack", "e1release" }, "env" },
                { "eg2", "Enveloppe 2 (filtres, ampli)", "env", { "e2hold", "e2attack", "e2decay", "e2sustain", "e2release" }, "env" },
                { "mg", "G\xC3\xA9n\xC3\xA9rateur de modulation", "lfo", { "mgshape", "mgrate", "mgsync", "modsrc", "mgdelay" }, "mod" },
                { "out", "Sortie", "amp", { "velamp", "level" }, "amp" },
            };
            m.presets = {
                { "Init", { { "lppeak", 0 }, { "lpeg2", 0 }, { "lpcut", 18000 }, { "drive", 0 }, { "e2sustain", 100 }, { "e2decay", 600 }, { "level", -8.2f } } },
                { "Basse qui grogne", { { "level", -4.4f } } },
                { "Basse \xC3\xA0 deux oscillateurs", { { "v2level", 70 }, { "v2scale", 1 }, { "lpcut", 500 }, { "lppeak", 45 }, { "lpeg2", 60 }, { "e2decay", 300 }, { "e2sustain", 30 }, { "drive", 55 }, { "level", -0.3f } } },
                { "Cri du passe-bas", { { "lpcut", 700 }, { "lppeak", 92 }, { "lpeg2", 50 }, { "drive", 70 }, { "e2decay", 600 }, { "level", -3.7f } } },
                { "Sifflement auto-oscillant", { { "v1level", 0 }, { "lppeak", 100 }, { "lpcut", 900 }, { "lpeg2", 0 }, { "e2sustain", 100 }, { "level", 0.4f } } },
                { "Lead d\xC3\xA9" "chir\xC3\xA9", { { "v1scale", 3 }, { "v2level", 60 }, { "v2pitch", 7 }, { "hpcut", 300 }, { "hppeak", 60 }, { "lpcut", 2500 }, { "lppeak", 70 }, { "drive", 90 }, { "porta", 60 }, { "level", -1.2f } } },
                { "Anneau m\xC3\xA9tallique", { { "v2wave", 3 }, { "v2level", 90 }, { "v1level", 0 }, { "v2pitch", 5 }, { "lpcut", 4000 }, { "lppeak", 40 }, { "e2decay", 500 }, { "e2sustain", 0 }, { "level", -4.9f } } },
                { "Percussion \xC3\xA9" "lectronique", { { "v1wave", 3 }, { "v1level", 100 }, { "hpcut", 800 }, { "hppeak", 70 }, { "lpcut", 3000 }, { "lppeak", 60 }, { "e2attack", 0.5f }, { "e2decay", 160 }, { "e2sustain", 0 }, { "lpeg2", 30 }, { "level", 4.2f } } },
                { "Sir\xC3\xA8ne", { { "pitchmg", 60 }, { "mgrate", 0.6f }, { "mgshape", 100 }, { "lppeak", 60 }, { "lpcut", 2500 }, { "e2sustain", 100 }, { "level", -8.2f } } },
                { "Bloqu\xC3\xA9 al\xC3\xA9" "atoire", { { "modsrc", 2 }, { "mgsync", 9 }, { "lpmg", 80 }, { "lpcut", 900 }, { "lppeak", 80 }, { "e2sustain", 90 }, { "level", -6.8f } } },
                { "Wah au g\xC3\xA9n\xC3\xA9rateur", { { "lpmg", 60 }, { "mgrate", 4 }, { "lppeak", 75 }, { "lpcut", 700 }, { "lpeg2", 0 }, { "e2sustain", 100 }, { "level", -7.3f } } },
                { "Coup de hauteur", { { "pitcheg1", 40 }, { "e1release", 120 }, { "lpcut", 1200 }, { "e2decay", 250 }, { "e2sustain", 0 }, { "level", 2.0f } } },
                { "Nappe filtr\xC3\xA9" "e", { { "v1wave", 0 }, { "v2level", 60 }, { "v2pitch", 0.12f }, { "e2attack", 900 }, { "e2release", 1800 }, { "e2sustain", 90 }, { "lpcut", 1500 }, { "lpmg", 25 }, { "mgrate", 0.3f }, { "level", -5.9f } } },
                { "Retard de vibrato", { { "pitcheg1", 0 }, { "pitchmg", 8 }, { "mgrate", 5.5f }, { "mgshape", 50 }, { "lpcut", 2200 }, { "v1scale", 3 }, { "level", -5.1f } } },
                { "Passe-haut fin", { { "hpcut", 1200 }, { "hppeak", 40 }, { "lpcut", 6000 }, { "lppeak", 10 }, { "level", 1.0f } } },
                { "Effet d'onde", { { "v1wave", 3 }, { "hpcut", 2000 }, { "hppeak", 95 }, { "lpcut", 2400 }, { "lppeak", 95 }, { "hpmg", 40 }, { "lpmg", 40 }, { "mgrate", 0.2f }, { "e2sustain", 100 }, { "level", -11.4f } } },
            };
            return m;
        }();
        return d;
    }
}

/** Filtre 12 dB/oct à réinjection de type Sallen-Key (passe-bas ou passe-haut), en topologie
    sans retard (TPT) : trois intégrateurs d'un pôle et une boucle saturée. k de 0 à 2 :
    auto-oscillation vers 2. */
class SallenKey12
{
public:
    void reset() noexcept { z1 = z2 = z3 = 0.0; }

    double process (double x, bool highpass, double fc, double k, double sat, double sr) noexcept
    {
        k = std::max (0.01, k); // résonance nulle : le chemin reste ouvert
        const auto g = std::tan (kPi * std::clamp (fc, 15.0, 0.45 * sr) / sr);
        const auto G = g / (1.0 + g);
        const auto a0 = 1.0 / (1.0 - k * G + k * G * G);
        if (! highpass)
        {
            const auto y1 = onePole (x, z1, G);                                    // passe-bas 1
            const auto s35 = (k - k * G) / (1.0 + g) * z2 + (-1.0 / (1.0 + g)) * z3;
            auto u = a0 * (y1 + s35);
            u = fastTanh (u * sat) / sat;                                          // boucle saturée
            const auto y = k * onePole (u, z2, G);                                 // passe-bas 2
            onePoleHp (y, z3, G);                                                  // réinjection
            return k > 0.0 ? y / k : y;
        }
        const auto y1 = onePoleHp (x, z1, G);                                       // passe-haut 1
        const auto s35 = (-G / (1.0 + g)) * z2 + (1.0 / (1.0 + g)) * z3;
        const auto u = a0 * y1 + s35;
        auto y = k * u;
        y = fastTanh (y * sat) / sat;
        onePole (onePoleHp (y, z2, G), z3, G);
        return k > 0.0 ? y / k : y;
    }

private:
    static double onePole (double x, double& z, double G) noexcept { const auto v = (x - z) * G; const auto lp = v + z; z = lp + v; return lp; }
    static double onePoleHp (double x, double& z, double G) noexcept { const auto v = (x - z) * G; const auto lp = v + z; z = lp + v; return x - lp; }
    double z1 = 0.0, z2 = 0.0, z3 = 0.0;
};

class Ms20Voice
{
public:
    void prepare (double s) { sr = s; eg2.prepare (s); reset(); }
    void reset() noexcept
    {
        o1.reset(); o2.reset (0.25); hp.reset(); lp.reset(); eg2.reset(); ringBl.reset(); ringSign = 1.0; lastA = 0.0;
        e1 = 0.0; e1Stage = 0; e1Count = 0.0; holdLeft = 0; gate = false; active = false; mgPhase = 0.0; sh = 0.0;
    }
    void setSpread (int, int) noexcept {}

    void start (int n, float velocity, bool legatoStart, int) noexcept
    {
        pitch.to ((double) n);
        if (velocity >= 0.0f) vel = velocity;
        if (! legatoStart || ! active)
        {
            if (! active) pitch.jump ((double) n);
            e1Stage = 1; e1Count = 0.0;              // retard puis attaque
            mgAge = 0.0;                             // retard du générateur : repart à chaque note
            eg2.gate (true, true);
        }
        gate = true; active = true; holdLeft = 0; pendingHold = false;
    }
    void release() noexcept
    {
        gate = false;
        e1Stage = e1Stage == 0 ? 0 : 3;
        pendingHold = true;                           // le maintien prolonge la porte de l'enveloppe 2
    }
    bool isActive() const noexcept { return active; }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace ms20;
        if (pendingHold) { pendingHold = false; holdLeft = (long) (p[e2Hold] * 0.001 * sr); if (holdLeft <= 0) eg2.gate (false); }
        eg2.set (p[e2Attack], p[e2Decay], p[e2Sustain] / 100.0, p[e2Release]);
        static constexpr double scaleOff[] { -24.0, -12.0, 0.0, 12.0 };
        const auto base1 = scaleOff[std::clamp ((int) p[v1scale], 0, 3)] + p[tune];
        const auto base2 = scaleOff[std::clamp ((int) p[v2scale], 0, 3)] + p[tune] + p[v2pitch];
        const auto w1 = (int) p[v1wave], w2 = (int) p[v2wave];
        const auto pw1 = p[v1pw] / 100.0;
        const auto lvl1 = p[v1level] / 100.0, lvl2 = p[v2level] / 100.0;
        const auto glideCoef = p[porta] > 0.5f ? std::exp (-1.0 / std::max (1.0, p[porta] * 0.001 * sr * 0.3)) : 0.0;
        const auto mgHz = (double) p[mgRate];
        const auto shape = std::clamp (p[mgShape] / 100.0, 0.01, 0.99);
        const auto src = (int) p[modSrc];
        const auto mgDelaySec = p[mgDelay] * 0.001;
        const auto e1AtkInc = 1.0 / std::max (1.0, p[e1Attack] * 0.001 * sr);
        const auto e1RelCoef = std::exp (std::log (0.001) / std::max (1.0, p[e1Release] * 0.001 * sr));
        const auto e1DelaySamples = p[e1Delay] * 0.001 * sr;
        const auto hk = 2.04 * std::pow (p[hpPeak] / 100.0, 0.7), lk = 2.04 * std::pow (p[lpPeak] / 100.0, 0.7); // au-delà de 2 : auto-oscillation (bornée par la saturation)
        const auto sat = 0.6 + 2.4 * p[drive] / 100.0;
        const auto velG = 1.0 - (p[velAmp] / 100.0) * (1.0 - vel);
        const auto out = dbToGain (p[level]) * velG * 1.4; // +3 dB de réserve
        const auto hpCutLog = std::log2 ((double) p[hpCut]), lpCutLog = std::log2 ((double) p[lpCut]);

        for (int i = 0; i < n; ++i)
        {
            // générateur de modulation : rampe ↔ triangle (sommet à « shape »), carré, bloqué
            mgPhase += mgHz / sr;
            if (mgPhase >= 1.0) { mgPhase -= 1.0; rng = rng * 1664525u + 1013904223u; sh = (double) (rng >> 8) / 8388608.0 - 1.0; }
            const auto ramp = mgPhase < shape ? -1.0 + 2.0 * mgPhase / shape : 1.0 - 2.0 * (mgPhase - shape) / (1.0 - shape);
            mgAge += 1.0 / sr;
            const auto mgK = mgDelaySec > 0.0 ? std::clamp ((mgAge - mgDelaySec) / (0.5 * mgDelaySec), 0.0, 1.0) : 1.0;
            const auto mg = (src == 0 ? ramp : src == 1 ? (mgPhase < shape ? 1.0 : -1.0) : sh) * mgK;

            // enveloppe 1 : retard, attaque, tenue, relâchement
            switch (e1Stage)
            {
                case 1: if ((e1Count += 1.0) >= e1DelaySamples) e1Stage = 2; break;
                case 2: e1 = std::min (1.0, e1 + e1AtkInc); break;
                case 3: e1 *= e1RelCoef; if (e1 < 1.0e-5) { e1 = 0.0; e1Stage = 0; } break;
                default: break;
            }
            if (holdLeft > 0 && --holdLeft == 0 && ! gate) eg2.gate (false);
            const auto e2 = eg2.next();

            const auto note = pitch.next (glideCoef) + p[pitchMg] / 100.0 * 24.0 * mg + p[pitchEg1] / 100.0 * 48.0 * e1;
            const auto dt1 = noteHz (note + base1) / sr, dt2 = noteHz (note + base2) / sr;

            double a;
            switch (w1)
            {
                case 0: a = o1.next (Osc::triangle, dt1, 0.5); break;
                case 1: a = o1.next (Osc::saw, dt1, 0.5); break;
                case 2: a = o1.next (Osc::pulse, dt1, pw1); break;
                default: rng2 = rng2 * 1664525u + 1013904223u; a = (double) (rng2 >> 8) / 8388608.0 - 1.0; break;
            }
            double b;
            switch (w2)
            {
                case 0: b = o2.next (Osc::saw, dt2, 0.5); break;
                case 1: b = o2.next (Osc::pulse, dt2, 0.5); break;
                case 2: b = o2.next (Osc::pulse, dt2, 0.1); break;
                default: // anneau : carré de l'osc. 2 inversé au signe de l'osc. 1
                {
                    const auto sq = o2.next (Osc::pulse, dt2, 0.5);
                    const auto sgn = a >= 0.0 ? 1.0 : -1.0;
                    if (sgn != ringSign)
                    {
                        // v0.22 : le basculement au passage par zéro de l'osc. 1 est corrigé par BLEP
                        // (instant estimé entre les deux échantillons) : il repliait fortement
                        const auto tau = lastA != a ? std::clamp (lastA / (lastA - a), 0.0, 1.0) : 0.5;
                        ringBl.step (sq * (sgn - ringSign), tau - 1.0);
                        ringSign = sgn;
                    }
                    b = ringBl.push (sq * sgn);
                    break;
                }
            }
            if (w2 != 3) ringSign = a >= 0.0 ? 1.0 : -1.0;
            lastA = a;
            rng3 = rng3 * 1664525u + 1013904223u;
            auto x = (a * lvl1 + b * lvl2) * 0.7 + ((double) (rng3 >> 8) / 8388608.0 - 1.0) * 1.0e-5; // souffle de fond : amorce l'auto-oscillation

            // passe-haut puis passe-bas, coupures modulées (octaves)
            const auto fhp = fastExp2 (hpCutLog + p[hpMg] / 100.0 * 3.0 * mg + p[hpEg2] / 100.0 * 6.0 * e2);
            const auto flp = fastExp2 (lpCutLog + p[lpMg] / 100.0 * 3.0 * mg + p[lpEg2] / 100.0 * 7.0 * e2);
            x = hp.process (x, true, fhp, hk, sat, sr);
            x = lp.process (x, false, flp, lk, sat, sr);

            const auto y = x * e2 * out;
            L[i] += (float) y;
            R[i] += (float) y;
        }
        if (! gate && holdLeft <= 0 && ! eg2.isActive()) active = false;
    }

private:
    double sr = 48000.0, e1 = 0.0, e1Count = 0.0, mgPhase = 0.0, sh = 0.0, mgAge = 1.0e9;
    int e1Stage = 0;
    long holdLeft = 0;
    float vel = 1.0f;
    bool gate = false, active = false, pendingHold = false;
    std::uint32_t rng = 0x1234567u, rng2 = 0x89ABCDEu, rng3 = 0x5A5A5A5u;
    Osc o1, o2;
    BlepLine ringBl;
    double ringSign = 1.0, lastA = 0.0;
    SallenKey12 hp, lp;
    Adsr eg2;
    Glide pitch;
};

class Ms20Synth final : public SynthBase<Ms20Voice, 1>
{
public:
    const ModuleDesc& desc() const noexcept override { return ms20::desc(); }
protected:
    void configure (const float*) noexcept override { setMode (true, true, 1); }
    const float* adjust (const float* p) noexcept override { return syncRates (p, ms20::numSlots, { { ms20::mgRate, ms20::mgSync } }); }
};

} // namespace grv::instruments
