/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Sources de modulation d'un canal (6 par canal), calculées au rythme de contrôle.
      - LFO : libre (Hz) ou synchronisé au tempo, 6 formes, phase, redéclenchement ;
      - Générateur de fonction : 4 étapes (temps + niveau), une fois ou en boucle ;
      - Suiveur d'enveloppe : suit le niveau du canal (attaque, relâchement, sensibilité), ou en
        side-chain (v0.23) l'audio d'une autre piste, ou ses notes (chaque note relance l'enveloppe) ;
      - Séquenceur de modulation : 16 pas, longueur réglable, lissage.
    Sortie : LFO dans [-1, 1], les autres dans [0, 1]. La profondeur est appliquée par le moteur.
*/

#pragma once

#include "Params.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace grv::engine
{

enum ModSrcSlot
{
    msType = 0, msSync, msRate, msDivision, msShape, msPhase, msRetrig, msSmooth,
    msT1, msT2, msT3, msT4, msL1, msL2, msL3, msL4, msLoop,
    msAttack, msRelease, msGain,
    msSeqLength,
    msDelay,          // 21 : retard du LFO à chaque note (v0.13.2)
    msFollowSrc,      // 22 : source du suiveur (v0.23) : cette piste, audio ou notes d'une autre piste
    msPolarity,       // 23 : polarité du LFO (1.1) : ± autour du réglage, + vers le haut, − vers le bas
    msStep1 = 24 // 24..39 : 16 pas
};

inline const ModuleDesc& modSourceDesc()
{
    static const ModuleDesc d = []
    {
        ModuleDesc m;
        m.type = "modsrc";
        m.name = "Source de modulation";
        m.params.resize (modSourceSlots);
        m.params[msType] = C ("type", "Type", { "LFO", "Fonction", "Suiveur", "S\xC3\xA9quenceur" });
        m.params[msSync] = C ("sync", "Synchro", { "Libre", "Tempo" }, 1);
        m.params[msRate] = P ("rate", "Vitesse", "Hz", 0.02f, 40.0f, 2.0f, 0.3f);
        m.params[msDivision] = C ("division", "Division", { "16 mes.", "8 mes.", "4 mes.", "2 mes.", "1 mes.", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T" }, 6);
        m.params[msShape] = C ("shape", "Forme", { "Sinus", "Triangle", "Dent de scie", "Carr\xC3\xA9", "Al\xC3\xA9" "atoire", "Al\xC3\xA9" "atoire liss\xC3\xA9" });
        m.params[msPhase] = P ("phase", "Phase", "\xC2\xB0", 0, 360, 0);
        // 1.1 : « Un cycle », « Demi-cycle » (repartent à chaque note, font un tour ou un demi-tour puis
        // restent sur leur dernière valeur) et « Maintien » (le LFO tourne librement, sa valeur est figée
        // à chaque note) ajoutés à la fin — projets d'avant la version 20 convertis (3 choix avant)
        m.params[msRetrig] = C ("retrig", "Red\xC3\xA9" "clenchement", { "Libre", "Note", "Mesure", "Un cycle", "Demi-cycle", "Maintien" });
        m.params[msSmooth] = P ("smooth", "Lissage", "%", 0, 100, 0);
        m.params[msT1] = P ("t1", "Temps 1", "ms", 1, 4000, 20, 0.3f);
        m.params[msT2] = P ("t2", "Temps 2", "ms", 1, 4000, 200, 0.3f);
        m.params[msT3] = P ("t3", "Temps 3", "ms", 1, 4000, 300, 0.3f);
        m.params[msT4] = P ("t4", "Temps 4", "ms", 1, 4000, 500, 0.3f);
        m.params[msL1] = P ("l1", "Niveau 1", "%", 0, 100, 100);
        m.params[msL2] = P ("l2", "Niveau 2", "%", 0, 100, 50);
        m.params[msL3] = P ("l3", "Niveau 3", "%", 0, 100, 70);
        m.params[msL4] = P ("l4", "Niveau 4", "%", 0, 100, 0);
        m.params[msLoop] = C ("loop", "Boucle", { "Une fois", "Boucle" });
        m.params[msAttack] = P ("attack", "Attaque", "ms", 0.1f, 200, 5, 0.4f);
        m.params[msRelease] = P ("release", "Rel\xC3\xA2" "chement", "ms", 5, 2000, 150, 0.4f);
        m.params[msGain] = P ("gain", "Sensibilit\xC3\xA9", "dB", -12, 36, 12);
        m.params[msSeqLength] = I ("length", "Longueur", "pas", 1, 16, 16);
        m.params[msDelay] = P ("delay", "Retard", "ms", 0, 10000, 0, 0.35f);
        m.params[msFollowSrc] = C ("fsrc", "Source", followSourceChoices());
        // 1.1 : polarité du LFO — ± : le LFO va de -1 à +1 autour de la valeur du réglage (comme avant) ;
        // + : de 0 à +1 (il part du réglage et monte, sans jamais se bloquer en bas) ; − : de 0 à -1
        m.params[msPolarity] = C ("polarity", "Polarit\xC3\xA9", { "\xC2\xB1", "+", "\xE2\x88\x92" });
        for (int i = 0; i < 16; ++i)
            m.params[(size_t) (msStep1 + i)] = P ("step" + std::to_string (i + 1), "Pas " + std::to_string (i + 1), "%", 0, 100, (i % 4 == 0) ? 100.0f : 40.0f);

        m.sections = {
            { "lfo", "LFO", "lfo", { "type", "sync", "rate", "division", "shape", "phase", "retrig", "polarity", "smooth", "delay" }, "lfo" },
            { "func", "Fonction", "func", { "t1", "l1", "t2", "l2", "t3", "l3", "t4", "l4" }, "func" },
            { "follow", "Suiveur", "follow", { "type", "fsrc", "attack", "release", "gain", "smooth" }, "follow" },
            { "seq", "S\xC3\xA9quenceur", "seq", { "type", "division", "length", "smooth", "retrig" }, "seq" },
            { "seqsteps1", "Pas 1-8", "seq", { "step1", "step2", "step3", "step4", "step5", "step6", "step7", "step8" }, "seq" },
            { "seqsteps2", "Pas 9-16", "seq", { "step9", "step10", "step11", "step12", "step13", "step14", "step15", "step16" }, "seq" },
        };
        return m;
    }();
    return d;
}

/** Durée d'une division, en noires. */
inline double divisionQuarters (int division) noexcept
{
    static constexpr double q[] { 64.0, 32.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return q[std::clamp (division, 0, (int) std::size (q) - 1)];
}

class ModSource
{
public:
    void reset() noexcept { oneShotDone = true; oneShotAcc = oneShotLast = heldOut = 0.0; holdValid = false; phase = 0.0; value = 0.0; smoothed = 0.0; stage = 0; stageTime = 0.0; env = 0.0; held = 0.0; seqIndex = 0; rng = 0x1234567u; started = false; sideHit = 0.0f; sideRising = false; sideTarget = 0.0; }

    void trigger() noexcept { triggerPending = true; delayRestart = true; }
    /** Side-chain par les notes (v0.23) : une note de la piste suivie relance l'enveloppe. */
    void sideNote (float velocity) noexcept { sideHit = std::max (sideHit, velocity); }
    void barStart() noexcept { barPending = true; }

    /** Avance de dt secondes. p = réglages réels de la source ; level = niveau crête du canal ;
        ppq = position en noires (synchro). */
    float advance (const float* p, double dt, double bpm, double ppq, bool playing, float level) noexcept
    {
        const auto type = (int) p[msType];
        const auto retrig = (int) p[msRetrig];
        // 1.1 : « Un cycle » (3) et « Demi-cycle » (4) repartent à chaque note, comme « Note » (1)
        const bool noteMode = retrig == 1 || retrig == 3 || retrig == 4;
        const bool trig = (triggerPending && noteMode) || (barPending && retrig == 2);
        const bool latch = triggerPending && retrig == 5; // « Maintien » : valeur figée à chaque note
        triggerPending = barPending = false;

        double out = 0.0;
        switch (type)
        {
            case 0:
            {
                out = lfo (p, dt, bpm, ppq, playing, trig);
                // 1.1 : un cycle / demi-cycle depuis la dernière note, puis la dernière valeur reste ;
                // maintien : la valeur du LFO libre est relevée à chaque note et tenue jusqu'à la suivante
                if (retrig == 3 || retrig == 4)
                {
                    if (trig) { oneShotDone = false; oneShotAcc = 0.0; }
                    if (oneShotDone) out = oneShotLast;
                    else
                    {
                        oneShotAcc += lfoAdvance;
                        if (oneShotAcc >= (retrig == 3 ? 1.0 : 0.5)) { oneShotDone = true; out = lfoAt (p, retrig == 3 ? 1.0 : 0.5); }
                        oneShotLast = out;
                    }
                }
                else if (retrig == 5)
                {
                    if (latch || ! holdValid) { heldOut = out; holdValid = true; }
                    out = heldOut;
                }
                // polarité : ± (-1..+1), + (0..+1), − (0..-1)
                if (const auto pol = (int) p[msPolarity]; pol == 1) out = 0.5 * (out + 1.0);
                else if (pol == 2) out = -0.5 * (out + 1.0);
                // retard : muet pendant le retard après chaque note, puis entrée en fondu (moitié du retard)
                const auto d = (double) p[msDelay] * 0.001;
                if (delayRestart) { delayRestart = false; delayClock = 0.0; }
                if (d > 0.0)
                {
                    delayClock += dt;
                    const auto k = delayClock <= d ? 0.0 : std::min (1.0, (delayClock - d) / (0.5 * d));
                    out *= k;
                }
                break;
            }
            case 1: out = func (p, dt, trig || ! started, retrig == 0 || retrig == 5); break; // Libre : la fonction tourne en boucle
            case 2: out = follow (p, dt, level); break;
            default: out = seq (p, dt, bpm, ppq, playing, trig); break;
        }
        started = true;

        // Lissage (sauf suiveur, déjà lissé par ses temps)
        const auto smooth = type == 2 ? 0.0 : (double) p[msSmooth] / 100.0;
        if (smooth > 0.0)
        {
            const auto tau = 0.001 + smooth * smooth * 0.5;
            smoothed += (out - smoothed) * (1.0 - std::exp (-dt / tau));
            out = smoothed;
        }
        else smoothed = out;

        value = (float) out;
        return value;
    }

    float current() const noexcept { return value; }
    /** Pas en cours du séquenceur de modulation (affichage). */
    int step() const noexcept { return seqIndex; }
    bool bipolar (const float* p) const noexcept { return (int) p[msType] == 0 && (int) p[msPolarity] == 0; }

private:
    /** Valeur des formes déterministes à la position x (0..1) d'un cycle, phase comprise (fin d'un
        cycle ou d'un demi-cycle). Les formes aléatoires gardent leur dernière valeur. */
    double lfoAt (const float* p, double x) const noexcept
    {
        auto ph = x + (double) p[msPhase] / 360.0;
        ph -= std::floor (ph);
        switch ((int) p[msShape])
        {
            case 0: return std::sin (2.0 * 3.14159265358979 * ph);
            case 1: return 1.0 - 4.0 * std::abs (ph - 0.5);
            case 2: return x >= 1.0 && ph == 0.0 ? 1.0 : 2.0 * ph - 1.0; // la dent de scie finit en haut
            case 3: return ph < 0.5 ? 1.0 : -1.0;
            default: return oneShotLast;
        }
    }

    double lfo (const float* p, double dt, double bpm, double ppq, bool playing, bool trig) noexcept
    {
        const bool sync = (int) p[msSync] == 1;
        const auto shape = (int) p[msShape];
        const auto phaseOffset = (double) p[msPhase] / 360.0;
        const auto prev = phase;
        {
            const auto hz = sync ? (bpm / 60.0) / divisionQuarters ((int) p[msDivision]) : (double) p[msRate];
            lfoAdvance = hz * dt; // part de cycle parcourue (un cycle / demi-cycle)
        }

        if (sync && playing && (int) p[msRetrig] == 0)
            phase = std::fmod (ppq / divisionQuarters ((int) p[msDivision]), 1.0); // calé sur la mesure
        else
        {
            const auto hz = sync ? (bpm / 60.0) / divisionQuarters ((int) p[msDivision]) : (double) p[msRate];
            if (trig) phase = 0.0;
            phase += hz * dt;
            phase -= std::floor (phase);
        }

        auto ph = phase + phaseOffset;
        ph -= std::floor (ph);
        if (ph < prev - 0.5 || trig) { held = rand(); }  // nouveau cycle : nouvelle valeur aléatoire

        switch (shape)
        {
            case 0: return std::sin (2.0 * 3.14159265358979 * ph);
            case 1: return 1.0 - 4.0 * std::abs (ph - 0.5);
            case 2: return 2.0 * ph - 1.0;
            case 3: return ph < 0.5 ? 1.0 : -1.0;
            case 4: return held;
            default:
            {
                // Aléatoire lissé : interpolation cosinus entre deux tirages
                if (ph < lastPh) { from = to; to = rand(); }
                lastPh = ph;
                const auto t = 0.5 - 0.5 * std::cos (3.14159265358979 * ph);
                return from + (to - from) * t;
            }
        }
    }

    double func (const float* p, double dt, bool trig, bool freeRun) noexcept
    {
        const double times[4] { p[msT1] * 0.001, p[msT2] * 0.001, p[msT3] * 0.001, p[msT4] * 0.001 };
        const double levels[4] { p[msL1] / 100.0, p[msL2] / 100.0, p[msL3] / 100.0, p[msL4] / 100.0 };
        if (trig) { stage = 0; stageTime = 0.0; startLevel = env; }
        if (stage < 4)
        {
            stageTime += dt;
            const auto t = std::min (1.0, stageTime / std::max (1.0e-4, times[stage]));
            env = startLevel + (levels[stage] - startLevel) * t;
            if (t >= 1.0)
            {
                startLevel = levels[stage];
                stageTime = 0.0;
                ++stage;
                if (stage == 4 && ((int) p[msLoop] == 1 || freeRun)) stage = 0;
            }
        }
        return env;
    }

    double follow (const float* p, double dt, float level) noexcept
    {
        const auto src = (int) p[msFollowSrc];
        if (src > numTracks)
        {
            // notes d'une autre piste : montée en « attaque » vers la vélocité, puis relâchement
            if (sideHit > 0.0f) { sideTarget = std::min (1.0, (double) sideHit * std::pow (10.0, std::min (0.0f, p[msGain]) / 20.0)); sideHit = 0.0f; sideRising = true; }
            if (sideRising)
            {
                env += (sideTarget - env) * (1.0 - std::exp (-dt / std::max (1.0e-4, p[msAttack] * 0.001)));
                if (env >= sideTarget * 0.97) sideRising = false;
            }
            else env *= std::exp (-dt / std::max (1.0e-4, p[msRelease] * 0.001));
            return env;
        }
        const auto x = std::min (1.0, (double) level * std::pow (10.0, p[msGain] / 20.0));
        const auto tau = x > env ? p[msAttack] * 0.001 : p[msRelease] * 0.001;
        env += (x - env) * (1.0 - std::exp (-dt / std::max (1.0e-4, tau)));
        return env;
    }

    double seq (const float* p, double dt, double bpm, double ppq, bool playing, bool trig) noexcept
    {
        const auto length = std::clamp ((int) p[msSeqLength], 1, 16);
        const auto stepQ = divisionQuarters ((int) p[msDivision]);
        const auto rt = (int) p[msRetrig];
        if (playing && rt != 1 && rt != 3 && rt != 4)
            seqIndex = (int) std::floor (ppq / stepQ) % length;
        else
        {
            if (trig) { seqPos = 0.0; }
            seqPos += dt * (bpm / 60.0) / stepQ;
            seqIndex = (int) std::floor (seqPos) % length;
        }
        return p[msStep1 + std::max (0, seqIndex)] / 100.0;
    }

    double rand() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (double) rng / 2147483648.0 - 1.0;
    }

    double phase = 0.0, held = 0.0, from = 0.0, to = 0.0, lastPh = 0.0;
    double env = 0.0, startLevel = 0.0, stageTime = 0.0, smoothed = 0.0, seqPos = 0.0, sideTarget = 0.0;
    float sideHit = 0.0f;
    bool sideRising = false;
    int stage = 4, seqIndex = 0;
    float value = 0.0f;
    bool triggerPending = false, barPending = false, started = false, delayRestart = false;
    // 1.1 : un cycle / demi-cycle, maintien
    double lfoAdvance = 0.0, oneShotAcc = 0.0, oneShotLast = 0.0, heldOut = 0.0;
    bool oneShotDone = true, holdValid = false;
    double delayClock = 1.0e9; // sans note jouée : pas de retard
    std::uint32_t rng = 0x1234567u;
};

} // namespace grv::engine
