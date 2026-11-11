/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Machines » de la batterie analogique (principe des batteries analogiques modernes à
    machines) : pour chaque pad, on choisit un modèle de synthèse — plusieurs grosses caisses
    (dure, classique, FM, plastique, soyeuse, acoustique), caisses claires (dure, classique, FM,
    naturelle, acoustique), rimshots, clap, toms, charleys, cymbales, cloches, et des machines
    utilitaires (bruit, impulsion, double oscillateur). Chaque machine a SES 7 réglages (les
    libellés changent avec la machine) : c'est ce qui donne à chaque modèle son caractère.

    Les machines réutilisent les circuits de DrumVoices.h ou ajoutent les leurs (grosse caisse à
    oscillateur balayé avec tenue / pli / poussière, FM à deux opérateurs, modèle modal de fût,
    générateur de bruit, impulsion, double oscillateur). Architecture et noms propres : aucun
    nom, son ni échantillon d'origine.
    Classes pures C++, sans allocation au rendu.
*/

#pragma once

#include "DrumVoices.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace grv::dsp
{

//==============================================================================
/** Grosse caisse à oscillateur balayé, en quatre caractères :
      0 dure : tenue réglable avant le déclin, forme de l'oscillateur, click ;
      1 plastique : base plus haute, balayage court, pli de l'onde (son « plastique ») ;
      2 soyeuse : sinusoïde ronde, poussière (craquements) et click adouci ;
      3 pointue : balayage très court et profond, attaque tranchante.
    tone = profondeur du balayage, snap = click, x1 = temps du balayage, x2 = forme / poussière,
    x3 = tenue / pli / douceur du click / dureté. */
class KickMachine final : public Voice
{
public:
    KickMachine (int character, double baseHz, double sweepRangeSemis, double sweepTimeMs, std::uint32_t seed)
        : kind (character), base (baseHz), range (sweepRangeSemis), sweepTime (sweepTimeMs), drift (seed), noise (seed ^ 0x51DEu), dust (seed * 7u + 3u), noiseSeed (seed ^ 0x51DEu) {}
    void prepare (double s) override { sr = s; dc.prepare (sr); reset(); }
    void reset() override { stage = 0; amp = 0.0; pitchEnv = 0.0; clickEnv = 0.0; clickLp = 0.0; dustEnv = 0.0; tail = lastBody = 0.0; shaper.reset(); dc.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override
    {
        drift.hit();
        gain = velocityToGain (v) * drift.level;
        // accent : départ du balayage un peu plus haut et attaque plus marquée (impulsion plus forte)
        pitchEnv = 1.0 + 0.2 * acc; clickEnv = 1.0 + 0.8 * acc; dustEnv = 1.0;
        // 1.1 : même point de départ à CHAQUE coup (attaque et claquant identiques ; avant, une
        // voix qui sonnait encore gardait sa phase : un coup sur deux ou trois sonnait autrement) ;
        // la queue du coup précédent est fondue en ~2 ms, sans clic ; transitoire identique
        tail = stage == 0 ? 0.0 : lastBody;
        phase = 0.0; clickLp = 0.0;
        noise = WhiteNoise (noiseSeed);
        holdLeft = kind == 0 ? (long) (p.extra3 * p.extra3 * 0.3 * sr) : 0;
        stage = 1;
    }
    bool isActive() const override { return stage != 0; }

    void renderAdding (float* out, int n) override
    {
        if (stage == 0) return;
        const auto f0 = base * semis (p.tuneSemis) * drift.pitch;
        const auto decayCoef = decay60Coef (p.decayMs * drift.decay, sr);
        const auto sweepMs = sweepTime * (0.12 + 1.8 * p.extra1);
        const auto pitchCoef = timeConstantCoef (sweepMs, sr);
        const auto attackCoef = timeConstantCoef (kind == 3 ? 0.08 : kind == 2 ? 0.9 : 0.25, sr);
        const auto clickCoef = timeConstantCoef (kind == 2 ? 3.0 + 6.0 * p.extra3 : 1.2, sr);
        const auto clickLpCoef = std::exp (-kTwoPi * (kind == 2 ? 1500.0 + 4000.0 * (1.0 - p.extra3) : 6500.0) / sr);
        const auto sweep = p.tone * range * (kind == 3 ? 1.0 + p.extra3 : 1.0);
        const auto shapeAmt = kind == 0 || kind == 3 ? p.extra2 : kind == 1 ? 0.2 : 0.0;
        const auto fold = kind == 1 ? p.extra3 : 0.0;
        const auto dustAmt = kind == 2 ? p.extra2 : 0.0;
        const auto dustCoef = decay60Coef (std::max (60.0, p.decayMs * 0.5), sr);
        const auto tailCoef = timeConstantCoef (0.5, sr);
        for (int i = 0; i < n; ++i)
        {
            switch (stage)
            {
                case 1: amp = 1.2 + (amp - 1.2) * attackCoef; if (amp >= 1.0) { amp = 1.0; stage = holdLeft > 0 ? 2 : 3; } break;
                case 2: if (--holdLeft <= 0) stage = 3; break;
                case 3: amp *= decayCoef; if (amp < 1.0e-5 && clickEnv < 1.0e-5) { stage = 0; amp = 0.0; } break;
                default: break;
            }
            phase += std::min (0.45, f0 * fastExp2 (sweep * pitchEnv / 12.0) / sr);
            phase -= std::floor (phase);
            pitchEnv *= pitchCoef;
            auto body = fastSin2Pi (phase);
            if (fold > 0.001) body = fastSin2Pi (0.25 * body * (1.0 + 3.0 * fold)); // pli : la sinusoïde se replie
            body = colour (body, shapeAmt) * amp;
            lastBody = body;
            body += tail; // fondu de la queue du coup précédent (2 ms)
            tail *= tailCoef;
            clickLp = noise.next() * (1.0 - clickLpCoef) + clickLp * clickLpCoef;
            const auto click = clickLp * clickEnv * p.snap * (kind == 3 ? 3.0 : 2.0);
            clickEnv *= clickCoef;
            double crackle = 0.0;
            if (dustAmt > 0.001)
            {
                // poussière : grains de bruit épars, sous le corps
                const auto r = dust.next();
                crackle = (std::abs (r) > 0.985 ? r : 0.0) * dustAmt * 0.6 * dustEnv;
                dustEnv *= dustCoef;
            }
            const auto x = (body + click + crackle) * gain;
            out[i] += (float) dc.process (drive.process (shaper.process (x)));
            if (stage == 0) break;
        }
    }

private:
    int kind;
    double base, range, sweepTime;
    VoiceParams p;
    double sr = 48000.0, gain = 1.0, phase = 0.0, amp = 0.0, pitchEnv = 0.0, clickEnv = 0.0, clickLp = 0.0, dustEnv = 0.0, tail = 0.0, lastBody = 0.0;
    long holdLeft = 0;
    int stage = 0;
    AnalogDrift drift;
    WhiteNoise noise, dust;
    std::uint32_t noiseSeed;
    AdaaSoftClip shaper;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** FM à deux opérateurs (grosse caisse ou caisse claire FM) : une porteuse sinusoïdale balayée,
    modulée par un modulateur à son propre déclin et son propre balayage ; caisse claire : bruit.
    tone = indice de modulation, snap = click (bruit pour la caisse claire), x1 = rapport du
    modulateur, x2 = déclin de la modulation, x3 = balayage de la modulation (déclin du bruit). */
class FmDrum final : public Voice
{
public:
    FmDrum (double baseHz, bool snareVariant, std::uint32_t seed) : base (baseHz), snare (snareVariant), drift (seed), noise (seed ^ 0xF00Du), noiseSeed (seed ^ 0xF00Du) {}
    void prepare (double s) override { sr = s; dc.prepare (sr); reset(); }
    void reset() override { env.reset(); modEnv.reset(); noiseEnv.reset(); pitch = 0.0; tail = lastC = 0.0; hp.reset(); dc.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override
    {
        drift.hit();
        gain = velocityToGain (v) * drift.level;
        // 1.1 : coup identique à CHAQUE fois (phases à zéro, même rafale de bruit) ; avant, un coup sur
        // un son encore fort gardait ses phases et sonnait autrement ; la queue est fondue en ~2 ms
        tail = env.isActive() ? lastC : 0.0;
        cPhase = 0.0; mPhase = 0.0; noise = WhiteNoise (noiseSeed);
        env.trigger(); modEnv.trigger(); noiseEnv.trigger(); pitch = 1.0;
    }
    bool isActive() const override { return env.isActive() || noiseEnv.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto f = base * semis (p.tuneSemis) * drift.pitch;
        env.setDecay (p.decayMs * drift.decay, sr);
        modEnv.setDecay (10.0 + 990.0 * p.extra2 * p.extra2, sr);
        noiseEnv.setDecay (snare ? 20.0 + 500.0 * p.extra3 : 6.0, sr);
        const auto ratio = 0.5 + 7.5 * p.extra1 * p.extra1;
        const auto index = p.tone * 9.0;
        const auto pitchCoef = timeConstantCoef (snare ? 6.0 : 14.0, sr);
        const auto modSweep = (p.extra3 - (snare ? 0.0 : 0.5)) * (snare ? 0.0 : 24.0); // demi-tons
        hp.set (snare ? 1800.0 : 3000.0, 0.7, sr);
        const auto tailCoef = timeConstantCoef (0.5, sr);
        for (int i = 0; i < n; ++i)
        {
            const auto bend = fastExp2 ((snare ? 4.0 : 12.0) * pitch / 12.0);
            pitch *= pitchCoef;
            const auto me = modEnv.next();
            mPhase += std::min (0.45, f * ratio * bend * fastExp2 (modSweep * me / 12.0) / sr);
            mPhase -= std::floor (mPhase);
            const auto m = fastSin2Pi (mPhase) * index * me;
            cPhase += std::min (0.45, f * bend / sr);
            cPhase -= std::floor (cPhase);
            auto c = fastSin2Pi (cPhase + m * 0.159) * env.next();
            lastC = c;
            c += tail; tail *= tailCoef; // fondu de la queue du coup précédent
            const auto nz = hp.process (noise.next()).high * noiseEnv.next() * p.snap * (snare ? 1.4 : 0.8);
            out[i] += (float) dc.process (drive.process ((c + nz) * gain * 0.9));
        }
    }

private:
    double base;
    bool snare;
    VoiceParams p;
    double sr = 48000.0, gain = 1.0, pitch = 0.0, cPhase = 0.0, mPhase = 0.0, tail = 0.0, lastC = 0.0;
    DecayEnv env, modEnv, noiseEnv;
    AnalogDrift drift;
    WhiteNoise noise;
    std::uint32_t noiseSeed;
    Svf hp;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Modèle modal d'un fût (machines « acoustiques ») : 4 modes de membrane excités par une batte
    (impulsion + bruit court) ; caisse claire : timbre (bruit qui suit l'énergie de la peau).
    tone = peau (amortissement des modes hauts), snap = batte (dureté), x1 = résonance du fût,
    x2 = taille (écart des modes, glissé au coup), x3 = étouffoir (kick) / timbre (caisse claire). */
class ModalDrum final : public Voice
{
public:
    ModalDrum (double baseHz, bool snareVariant, std::uint32_t seed) : base (baseHz), snare (snareVariant), drift (seed), noise (seed ^ 0xACu) {}
    void prepare (double s) override { sr = s; dc.prepare (sr); reset(); }
    void reset() override { for (auto& r : modes) r.reset(); beater.reset(); wiresEnv.reset(); bp.reset(); lp.reset(); dc.reset(); pitch = 0.0; pending = 0.0; }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override { drift.hit(); gain = velocityToGain (v) * drift.level; pending = gain; beater.trigger (gain); wiresEnv.trigger (gain); pitch = 1.0; }
    bool isActive() const override { return pending > 0.0 || modes[0].amplitude() + modes[1].amplitude() > 2.0e-5 || wiresEnv.isActive() || beater.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        static constexpr std::array<double, 4> kickRatios { 1.0, 1.59, 2.14, 2.30 };
        static constexpr std::array<double, 4> snareRatios { 1.0, 1.6, 2.2, 2.9 };
        const auto& ratios = snare ? snareRatios : kickRatios;
        const auto f = base * semis (p.tuneSemis) * drift.pitch;
        const auto damp = snare ? 1.0 : 1.0 - 0.75 * p.extra3;
        const auto spread = 0.8 + 0.4 * p.extra2;
        std::array<double, 4> coef {};
        for (int k = 0; k < 4; ++k)
        {
            // les modes hauts s'éteignent plus vite, selon la peau ; le fût prolonge le mode grave
            const auto ms = p.decayMs * drift.decay * damp * (k == 0 ? 0.6 + 0.8 * p.extra1 : (0.2 + 0.5 * (1.0 - p.tone)) / (double) k);
            coef[(size_t) k] = decay60Coef (std::max (8.0, ms), sr);
        }
        if (pending > 0.0)
        {
            for (int k = 0; k < 4; ++k) modes[(size_t) k].excite (pending * (k == 0 ? 0.8 : 0.35 * (0.3 + p.snap)));
            pending = 0.0;
        }
        beater.setDecay (1.5 + 8.0 * (1.0 - p.snap), sr);
        wiresEnv.setDecay (p.decayMs * drift.decay * 0.8, sr);
        lp.set (1500.0 + 9000.0 * p.snap, 0.7, sr);
        bp.set (4200.0 * semis (p.tuneSemis * 0.5), 0.9, sr);
        const auto pitchCoef = timeConstantCoef (20.0, sr);
        const auto glide = (p.extra2 - 0.3) * 0.25;
        for (int i = 0; i < n; ++i)
        {
            const auto bend = 1.0 + glide * pitch;
            pitch *= pitchCoef;
            double y = 0.0;
            for (int k = 0; k < 4; ++k)
                y += modes[(size_t) k].tick (f * (k == 0 ? 1.0 : ratios[(size_t) k] * spread) * bend, coef[(size_t) k], sr) * (k == 0 ? 1.0 : 0.5);
            const auto hit = lp.process (noise.next()).low * beater.next() * 0.7;
            double wires = 0.0;
            if (snare) wires = bp.process (noise.next()).band * wiresEnv.next() * (0.3 + 1.6 * p.extra3) * (0.4 + std::min (1.0, modes[0].amplitude()));
            out[i] += (float) dc.process (drive.process ((y + hit + wires) * 0.8));
        }
    }

private:
    double base;
    bool snare;
    VoiceParams p;
    double sr = 48000.0, gain = 1.0, pitch = 0.0, pending = 0.0;
    std::array<Resonator, 4> modes;
    DecayEnv beater, wiresEnv;
    AnalogDrift drift;
    WhiteNoise noise;
    Svf bp, lp;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Générateur de bruit (machine utilitaire) : bruit blanc dans un passe-bas résonant balayé et
    un passe-haut, attaque réglable. tune = coupure du passe-bas, tone = passe-haut, snap = attaque,
    x1 = résonance, x2 = profondeur du balayage, x3 = temps du balayage. */
class NoiseMachine final : public Voice
{
public:
    explicit NoiseMachine (std::uint32_t seed) : drift (seed), noise (seed ^ 0x401Eu) {}
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { env.reset(); lp.reset(); hp.reset(); rise = 1.0; sweep = 0.0; }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override { drift.hit(); gain = velocityToGain (v) * drift.level; env.trigger(); rise = 0.0; sweep = 1.0; }
    bool isActive() const override { return env.isActive(); }
    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        env.setDecay (p.decayMs * drift.decay, sr);
        const auto riseStep = 1.0 / std::max (1.0, p.snap * p.snap * 0.2 * sr);
        const auto sweepCoef = timeConstantCoef (5.0 + 600.0 * p.extra3 * p.extra3, sr);
        const auto depth = (p.extra2 - 0.5) * 2.0 * 6.0; // ±6 octaves
        hp.set (20.0 * std::pow (500.0, p.tone), 0.7, sr);
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0) lp.set (std::clamp (2000.0 * fastExp2 (p.tuneSemis / 6.0 + depth * sweep), 30.0, 0.45 * sr), 0.6 + 12.0 * p.extra1 * p.extra1, sr);
            sweep *= sweepCoef;
            rise = std::min (1.0, rise + riseStep);
            const auto x = hp.process (lp.process (noise.next()).low).high * env.next() * rise * gain;
            out[i] += (float) drive.process (x * 3.0);
        }
    }
private:
    VoiceParams p;
    double sr = 48000.0, gain = 1.0, rise = 1.0, sweep = 0.0;
    DecayEnv env;
    AnalogDrift drift;
    WhiteNoise noise;
    Svf lp, hp;
    Drive drive;
};

/** Impulsion (machine utilitaire) : une impulsion dans un filtre résonant — clics, toc, bips.
    tune = hauteur du filtre, decay = déclin, tone = polarité / forme (pic → creux), snap = attaque
    adoucie, x1 = résonance, x2 = brillance, x3 = deuxième impulsion (écho court). */
class ImpulseMachine final : public Voice
{
public:
    explicit ImpulseMachine (std::uint32_t seed) : drift (seed) {}
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { r.reset(); pending = 0.0; echoAt = -1; t = 0; lp.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override { drift.hit(); pending = velocityToGain (v) * drift.level; t = 0; echoAt = p.extra3 > 0.05 ? (long) ((0.004 + 0.06 * p.extra3) * sr) : -1; }
    bool isActive() const override { return pending > 0.0 || r.amplitude() > 2.0e-5 || echoAt >= 0; }
    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto f = 800.0 * semis (p.tuneSemis) * drift.pitch;
        const auto c = decay60Coef (std::max (2.0, p.decayMs * (0.2 + 0.8 * p.extra1)), sr);
        lp.set (500.0 + 15000.0 * p.extra2, 0.7, sr);
        for (int i = 0; i < n; ++i)
        {
            double in = 0.0;
            if (pending > 0.0) { in = pending * (p.tone < 0.5 ? 1.0 : -1.0); pending = 0.0; }
            if (echoAt >= 0 && t == echoAt) { in += 0.5 * (p.tone < 0.5 ? 1.0 : -1.0); echoAt = -1; }
            ++t;
            const auto y = r.tick (f, c, sr, in * (1.0 - 0.8 * p.snap));
            out[i] += (float) drive.process (lp.process (y + in * 0.3 * (1.0 - p.snap)).low);
        }
    }
private:
    VoiceParams p;
    double sr = 48000.0, pending = 0.0;
    long echoAt = -1, t = 0;
    Resonator r;
    AnalogDrift drift;
    Svf lp;
    Drive drive;
};

/** Double oscillateur (machine utilitaire, basses et bips) : deux oscillateurs (sinus → triangle
    → carré) à enveloppes séparées, balayage de hauteur commun.
    tune = hauteur 1, decay = déclin 1, tone = écart de l'oscillateur 2 (0 à +24 demi-tons),
    snap = mélange 1 / 2, x1 = déclin 2, x2 = profondeur du balayage (±24 demi-tons),
    x3 = temps du balayage. */
class DualVcoMachine final : public Voice
{
public:
    explicit DualVcoMachine (std::uint32_t seed) : drift (seed) {}
    void prepare (double s) override { sr = s; dc.prepare (sr); reset(); }
    void reset() override { e1.reset(); e2.reset(); bend = 0.0; dc.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override
    {
        drift.hit(); gain = velocityToGain (v) * drift.level;
        if (e1.level() < 0.03) { ph1 = 0.0; ph2 = 0.0; } // v0.30 : attaque identique à chaque coup
        e1.trigger(); e2.trigger(); bend = 1.0;
    }
    bool isActive() const override { return e1.isActive() || e2.isActive(); }
    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto f1 = 110.0 * semis (p.tuneSemis) * drift.pitch;
        const auto f2 = f1 * semis (24.0 * p.tone);
        e1.setDecay (p.decayMs * drift.decay, sr);
        e2.setDecay (10.0 + 3990.0 * p.extra1 * p.extra1, sr);
        const auto depth = (p.extra2 - 0.5) * 48.0;
        const auto bendCoef = timeConstantCoef (2.0 + 400.0 * p.extra3 * p.extra3, sr);
        for (int i = 0; i < n; ++i)
        {
            const auto b = fastExp2 (depth * bend / 12.0);
            bend *= bendCoef;
            ph1 += std::min (0.45, f1 * b / sr); ph1 -= std::floor (ph1);
            ph2 += std::min (0.45, f2 * b / sr); ph2 -= std::floor (ph2);
            const auto o1 = 1.0 - 4.0 * std::abs (ph1 - 0.5);      // triangle
            const auto o2 = s2.next (f2 * b, sr) * 0.6;           // carré à bande limitée
            const auto x = (o1 * (1.0 - p.snap) * e1.next() + o2 * p.snap * e2.next()) * gain;
            out[i] += (float) dc.process (drive.process (x * 0.8));
        }
    }
private:
    VoiceParams p;
    double sr = 48000.0, gain = 1.0, bend = 0.0, ph1 = 0.0, ph2 = 0.0;
    DecayEnv e1, e2;
    BlepSquare s2;
    AnalogDrift drift;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
// Catalogue des machines

struct MachineDef
{
    std::string name;                      // nom affiché
    std::string kind;                      // famille (couleur du pad)
    std::array<std::string, 7> labels;     // accord, déclin, timbre, attaque, x1, x2, x3
    std::function<std::unique_ptr<Voice> (std::uint32_t seed)> make; // nullptr : échantillon seul
};

inline const std::vector<MachineDef>& machineCatalog()
{
    static const std::vector<MachineDef> m = []
    {
        const std::string tn = "Accord", dc = "D\xC3\xA9" "clin";
        auto mk = [] (auto f) { return std::function<std::unique_ptr<Voice> (std::uint32_t)> (f); };
        std::vector<MachineDef> v;
        v.push_back ({ "\xC3\x89" "chantillon seul", "sample", { tn, dc, "Filtre", "D\xC3\xA9" "part", "Fin", "Attaque", "\xE2\x80\x94" }, nullptr });
        // Grosses caisses
        v.push_back ({ "GC dure", "kick", { tn, dc, "Balayage", "Click", "Temps balayage", "Forme", "Tenue" },
                       mk ([] (std::uint32_t s) { return std::make_unique<KickMachine> (0, 50.0, 48.0, 22.0, s); }) });
        v.push_back ({ "GC classique", "kick", { tn, dc, "Balayage", "Attaque", "Temps balayage", "Couleur", "Souffle" },
                       mk ([] (std::uint32_t s) { return std::make_unique<BridgedTDrum> (48.0, 6.0, 0.25, s); }) });
        v.push_back ({ "GC FM", "kick", { tn, dc, "Indice FM", "Click", "Rapport", "D\xC3\xA9" "clin FM", "Balayage FM" },
                       mk ([] (std::uint32_t s) { return std::make_unique<FmDrum> (50.0, false, s); }) });
        v.push_back ({ "GC plastique", "kick", { tn, dc, "Balayage", "Click", "Temps balayage", "Forme", "Pli" },
                       mk ([] (std::uint32_t s) { return std::make_unique<KickMachine> (1, 62.0, 30.0, 8.0, s); }) });
        v.push_back ({ "GC soyeuse", "kick", { tn, dc, "Balayage", "Click", "Temps balayage", "Poussi\xC3\xA8re", "Douceur click" },
                       mk ([] (std::uint32_t s) { return std::make_unique<KickMachine> (2, 46.0, 30.0, 30.0, s); }) });
        v.push_back ({ "GC pointue", "kick", { tn, dc, "Balayage", "Click", "Temps balayage", "Forme", "Duret\xC3\xA9" },
                       mk ([] (std::uint32_t s) { return std::make_unique<KickMachine> (3, 52.0, 60.0, 5.0, s); }) });
        v.push_back ({ "GC acoustique", "kick", { tn, dc, "Peau", "Batte", "R\xC3\xA9sonance f\xC3\xBBt", "Taille", "\xC3\x89touffoir" },
                       mk ([] (std::uint32_t s) { return std::make_unique<ModalDrum> (55.0, false, s); }) });
        // Caisses claires
        v.push_back ({ "CC dure", "snare", { tn, dc, "Brillance", "Claquant", "Couleur bruit", "Corps", "Balayage" },
                       mk ([] (std::uint32_t s) { return std::make_unique<SnareDrum> (200.0, 380.0, 1800.0, 0.9, s); }) });
        v.push_back ({ "CC classique", "snare", { tn, dc, "Brillance", "Claquant", "Couleur bruit", "Corps", "Glissement" },
                       mk ([] (std::uint32_t s) { return std::make_unique<SnareDrum> (238.0, 476.0, 2200.0, 0.15, s); }) });
        v.push_back ({ "CC FM", "snare", { tn, dc, "Indice FM", "Bruit", "Rapport", "D\xC3\xA9" "clin FM", "D\xC3\xA9" "clin bruit" },
                       mk ([] (std::uint32_t s) { return std::make_unique<FmDrum> (190.0, true, s); }) });
        v.push_back ({ "CC naturelle", "snare", { tn, dc, "Brillance", "Claquant", "Couleur bruit", "Corps", "Glissement" },
                       mk ([] (std::uint32_t s) { return std::make_unique<SnareDrum> (180.0, 330.0, 1000.0, 0.6, s); }) });
        v.push_back ({ "CC acoustique", "snare", { tn, dc, "Peau", "Batte", "R\xC3\xA9sonance f\xC3\xBBt", "Taille", "Timbre" },
                       mk ([] (std::uint32_t s) { return std::make_unique<ModalDrum> (190.0, true, s); }) });
        // Rimshots, claves, clap
        v.push_back ({ "Rim dur", "rim", { tn, dc, "Mode aigu", "Claquant", "Passe-haut", "Couleur", "Bruit" },
                       mk ([] (std::uint32_t s) { return std::make_unique<RimDrum> (1700.0, 480.0, s); }) });
        v.push_back ({ "Rim classique", "rim", { tn, dc, "Mode aigu", "Claquant", "Passe-haut", "Couleur", "Bruit" },
                       mk ([] (std::uint32_t s) { return std::make_unique<RimDrum> (1667.0, 455.0, s); }) });
        v.push_back ({ "Claves", "rim", { tn, dc, "Mode aigu", "Claquant", "Passe-haut", "Couleur", "Bruit" },
                       mk ([] (std::uint32_t s) { return std::make_unique<RimDrum> (2500.0, 2500.0, s); }) });
        v.push_back ({ "Clap classique", "clap", { tn, dc, "Timbre", "\xC3\x89" "cart", "Salves", "Queue", "Al\xC3\xA9" "a" },
                       mk ([] (std::uint32_t s) { return std::make_unique<ClapDrum> (s); }) });
        // Toms
        v.push_back ({ "Tom classique", "tom", { tn, dc, "Balayage", "Bruit", "Temps balayage", "Couleur", "Souffle" },
                       mk ([] (std::uint32_t s) { return std::make_unique<TomDrum> (110.0, 12.0, s); }) });
        v.push_back ({ "Tom dur", "tom", { tn, dc, "Balayage", "Click", "Temps balayage", "Forme", "Tenue" },
                       mk ([] (std::uint32_t s) { return std::make_unique<KickMachine> (0, 110.0, 24.0, 14.0, s); }) });
        v.push_back ({ "Tom basse", "tom", { tn, dc, "Balayage", "Attaque", "Temps balayage", "Couleur", "Souffle" },
                       mk ([] (std::uint32_t s) { return std::make_unique<BridgedTDrum> (66.0, 8.0, 0.5, s); }) });
        // Charleys, cymbales, cloches
        v.push_back ({ "Charley classique", "hatClosed", { tn, dc, "Timbre", "M\xC3\xA9tal", "Passe-haut", "R\xC3\xA9sonance", "Bande basse" },
                       mk ([] (std::uint32_t s) { return std::make_unique<HatDrum> (0, 6500.0, s, 0); }) });
        v.push_back ({ "Charley m\xC3\xA9tallique", "hatClosed", { tn, dc, "Timbre", "M\xC3\xA9tal", "Passe-haut", "R\xC3\xA9sonance", "Bande basse" },
                       mk ([] (std::uint32_t s) { return std::make_unique<HatDrum> (0, 7200.0, s, 1); }) });
        v.push_back ({ "Charley num\xC3\xA9rique", "hatClosed", { tn, dc, "Timbre", "M\xC3\xA9tal", "Passe-haut", "R\xC3\xA9sonance", "Bande basse" },
                       mk ([] (std::uint32_t s) { return std::make_unique<HatDrum> (6, 7000.0, s, 0); }) });
        v.push_back ({ "Cymbale classique", "hatOpen", { tn, dc, "Timbre", "M\xC3\xA9tal", "Passe-haut", "R\xC3\xA9sonance", "Bande basse" },
                       mk ([] (std::uint32_t s) { return std::make_unique<HatDrum> (0, 5200.0, s, 0); }) });
        v.push_back ({ "Cymbale m\xC3\xA9tallique", "hatOpen", { tn, dc, "Timbre", "M\xC3\xA9tal", "Passe-haut", "R\xC3\xA9sonance", "Bande basse" },
                       mk ([] (std::uint32_t s) { return std::make_unique<HatDrum> (0, 5600.0, s, 1); }) });
        v.push_back ({ "Ride", "hatOpen", { tn, dc, "Timbre", "M\xC3\xA9tal", "Passe-haut", "R\xC3\xA9sonance", "Bande basse" },
                       mk ([] (std::uint32_t s) { return std::make_unique<HatDrum> (6, 6000.0, s, 1); }) });
        v.push_back ({ "Cloche classique", "cowbell", { tn, dc, "Timbre", "Attaque", "\xC3\x89" "cart", "R\xC3\xA9sonance", "Pente rapide" },
                       mk ([] (std::uint32_t s) { return std::make_unique<CowbellDrum> (s); }) });
        // Utilitaires
        v.push_back ({ "Bruit", "sample", { "Coupure", dc, "Passe-haut", "Attaque", "R\xC3\xA9sonance", "Balayage", "Temps balayage" },
                       mk ([] (std::uint32_t s) { return std::make_unique<NoiseMachine> (s); }) });
        v.push_back ({ "Impulsion", "rim", { "Hauteur", dc, "Polarit\xC3\xA9", "Douceur", "R\xC3\xA9sonance", "Brillance", "\xC3\x89" "cho" },
                       mk ([] (std::uint32_t s) { return std::make_unique<ImpulseMachine> (s); }) });
        v.push_back ({ "Double oscillateur", "tom", { tn, dc, "\xC3\x89" "cart osc. 2", "M\xC3\xA9lange", "D\xC3\xA9" "clin 2", "Balayage", "Temps balayage" },
                       mk ([] (std::uint32_t s) { return std::make_unique<DualVcoMachine> (s); }) });
        return v;
    }();
    return m;
}

inline int machineIndex (const std::string& name)
{
    const auto& c = machineCatalog();
    for (size_t i = 0; i < c.size(); ++i) if (c[i].name == name) return (int) i;
    return 0;
}

} // namespace grv::dsp
