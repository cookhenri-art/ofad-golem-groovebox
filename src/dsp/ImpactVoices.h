/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Voix de la boîte « Impact » : principe des boîtes analogiques compactes et agressives de la
    fin des années 2010 (dix sons : grosse caisse, deux caisses claires, deux toms, cymbale, cloche,
    charleys fermé et ouvert, tambour FM ; distorsion de sortie). Architecture reprise, ni nom, ni
    son d'origine. Chaque voix a une COULEUR (extra2, 0 à 1) qui change son caractère, comme la
    touche de l'appareil : saturation de la grosse caisse, claquant et craquement des caisses
    claires, excitation des toms et des charleys, timbre de la cymbale, enveloppe de hauteur du
    tambour FM calée sur son déclin. La cloche n'en a pas (voix CowbellDrum).
    Réglages communs : accord, déclin, timbre, attaque (snap), saturation du pad, extra1 propre.
    Pur C++, sans allocation dans le rendu.
*/

#pragma once

#include "DrumVoices.h"

namespace grv::dsp
{

/** Saturation de la couleur : courbe quadratique (douce au début), crête conservée, et niveau
    efficace compensé (la couleur change le caractère, pas le volume : +3 dB au plus). */
inline double impactColour (double x, double c, double maxDrive = 6.0) noexcept
{
    if (c <= 0.001) return x;
    const auto k = 1.0 + maxDrive * c * c;
    const auto y = x + (fastTanh (x * k) / fastTanh (k) - x) * c;
    return y / (1.0 + 0.35 * c * (k - 1.0) / maxDrive * 2.0);
}

//==============================================================================
/** Grosse caisse : sinusoïde à fort balayage de hauteur, attaque (impulsion + souffle filtré).
    Timbre = profondeur du balayage, attaque = clic, extra1 = temps du balayage,
    couleur = saturation poussée (le grave s'arrondit, l'attaque mord). */
class ImpactKick final : public Voice
{
public:
    explicit ImpactKick (double baseHz = 46.0, std::uint32_t seed = 0x1A01u) : base (baseHz), drift (seed), noise (seed ^ 0x55AAu) {}
    void prepare (double s) override { sr = s; dc.prepare (sr, 12.0); reset(); }
    void reset() override { amp.reset(); click.reset(); pEnv = 0.0; phase = 0.0; hp.reset(); dc.reset(); drive.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override
    {
        drift.hit();
        gain = velocityToGain (v) * drift.level;
        amp.trigger(); click.trigger (1.0 + 0.8 * acc); pEnv = 1.0;
        phase = 0.0; // départ à zéro : pas de clic parasite, le clic vient du circuit d'attaque
    }
    bool isActive() const override { return amp.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto f0 = base * semis (p.tuneSemis) * drift.pitch;
        amp.setDecay (p.decayMs * drift.decay, sr);
        click.setDecay (2.0 + 6.0 * p.snap, sr);
        const auto sweepCoef = timeConstantCoef (3.0 + 70.0 * p.extra1 * p.extra1, sr);
        const auto depth = 6.0 + 42.0 * p.tone; // demi-tons
        hp.set (2500.0, 0.7, sr);
        const auto c = p.extra2;
        for (int i = 0; i < n; ++i)
        {
            const auto f = f0 * fastExp2 (depth * pEnv / 12.0);
            pEnv *= sweepCoef;
            phase += std::min (0.45, f / sr);
            phase -= std::floor (phase);
            const auto ce = click.next();
            const auto body = fastSin2Pi (phase) * amp.next();
            const auto atk = (hp.process (noise.next()).high * 0.5 + pEnv * pEnv * 0.6) * ce * p.snap;
            const auto x = impactColour (body + atk, c, 7.0);
            out[i] += (float) dc.process (drive.process (x * gain * 0.95));
        }
    }

private:
    VoiceParams p;
    double base, sr = 48000.0, gain = 1.0, pEnv = 0.0, phase = 0.0;
    DecayEnv amp, click;
    AnalogDrift drift;
    WhiteNoise noise;
    Svf hp;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Caisse claire 1 : deux sinusoïdes de corps (petite chute de hauteur) + bruit filtré.
    Timbre = clarté du bruit, attaque = part du bruit (« snap »), extra1 = corps,
    couleur = claquant et craquement (transitoire aigu, bruit plus sec et plus fort). */
class ImpactSnare final : public Voice
{
public:
    explicit ImpactSnare (double baseHz = 185.0, std::uint32_t seed = 0x1A02u) : base (baseHz), drift (seed), noise (seed ^ 0x3C3Cu) {}
    void prepare (double s) override { sr = s; dc.prepare (sr, 20.0); reset(); }
    void reset() override { body.reset(); nz.reset(); crack.reset(); pEnv = 0.0; ph1 = ph2 = 0.0; bp.reset(); hp.reset(); dc.reset(); drive.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override
    {
        drift.hit();
        gain = velocityToGain (v) * drift.level;
        body.trigger(); nz.trigger (1.0 + 0.5 * acc); crack.trigger(); pEnv = 1.0; ph1 = ph2 = 0.0;
    }
    bool isActive() const override { return body.isActive() || nz.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto c = p.extra2;
        const auto f = base * semis (p.tuneSemis) * drift.pitch;
        body.setDecay (std::max (20.0, p.decayMs * 0.45 * drift.decay), sr);
        nz.setDecay (p.decayMs * drift.decay * (1.0 - 0.35 * c), sr);
        crack.setDecay (4.0, sr);
        const auto pCoef = timeConstantCoef (12.0, sr);
        bp.set (1800.0 + 4500.0 * p.tone, 0.8, sr);
        hp.set (900.0 + 2500.0 * c, 0.7, sr);
        const auto bodyLvl = 0.25 + 0.75 * p.extra1, noiseLvl = (0.2 + 0.9 * p.snap) * (1.0 + 0.3 * c);
        for (int i = 0; i < n; ++i)
        {
            const auto bend = fastExp2 (3.0 * pEnv / 12.0);
            pEnv *= pCoef;
            ph1 += std::min (0.45, f * bend / sr); ph1 -= std::floor (ph1);
            ph2 += std::min (0.45, f * 1.62 * bend / sr); ph2 -= std::floor (ph2);
            const auto b = (fastSin2Pi (ph1) + 0.55 * fastSin2Pi (ph2)) * body.next() * bodyLvl;
            const auto w = noise.next();
            const auto nzv = hp.process (bp.process (w).band * 1.4 + w * 0.25).high * nz.next() * noiseLvl;
            const auto cr = w * crack.next() * c * 0.9;
            out[i] += (float) dc.process (drive.process (impactColour ((b + nzv + cr) * 0.8, 0.4 * c) * gain));
        }
    }

private:
    VoiceParams p;
    double base, sr = 48000.0, gain = 1.0, pEnv = 0.0, ph1 = 0.0, ph2 = 0.0;
    DecayEnv body, nz, crack;
    AnalogDrift drift;
    WhiteNoise noise;
    Svf bp, hp;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Caisse claire 2 : bruit « sale » avec un soupçon de clap (salves rapprochées), filtre
    passe-bande accordé. Timbre = hauteur du filtre, attaque = netteté des salves,
    extra1 = nombre de salves (clap), couleur = craquement (anneau sur le bruit, saturation). */
class ImpactSnare2 final : public Voice
{
public:
    explicit ImpactSnare2 (std::uint32_t seed = 0x1A03u) : drift (seed), noise (seed ^ 0x7E7Eu) {}
    void prepare (double s) override { sr = s; dc.prepare (sr, 30.0); reset(); }
    void reset() override { env.reset(); burst = 0.0; t = 0; ring = 0.0; bp.reset(); hp.reset(); dc.reset(); drive.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override
    {
        drift.hit();
        gain = velocityToGain (v) * drift.level * (1.0 + 0.3 * acc);
        env.trigger(); t = 0; burst = 1.0;
    }
    bool isActive() const override { return env.isActive() || burst > 1.0e-4; }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto c = p.extra2;
        const auto tune = semis (p.tuneSemis) * drift.pitch;
        env.setDecay (p.decayMs * drift.decay, sr);
        bp.set (std::min (0.45 * sr, (900.0 + 3200.0 * p.tone) * tune), 1.2, sr);
        hp.set (500.0, 0.7, sr);
        const auto bursts = 1 + (int) std::lround (3.0 * p.extra1); // 1 à 4 salves
        const auto gap = (long) (0.009 * sr);
        const auto burstCoef = timeConstantCoef (1.5 + 4.0 * (1.0 - p.snap), sr);
        const auto ringHz = 330.0 * tune;
        for (int i = 0; i < n; ++i)
        {
            // salves du clap : relancées toutes les 9 ms
            if (t > 0 && t % gap == 0 && t / gap < bursts) burst = 1.0;
            ++t;
            const auto w = noise.next();
            ring += ringHz / sr; ring -= std::floor (ring);
            const auto src = w * (1.0 - c * 0.6) + w * (ring < 0.5 ? 1.0 : -1.0) * c * 0.6;
            const auto e = std::max (env.next() * 0.8, burst);
            burst *= burstCoef;
            const auto x = hp.process (bp.process (src).band * 2.2).high * e;
            out[i] += (float) dc.process (drive.process (impactColour (x, 0.6 * c) * gain));
        }
    }

private:
    VoiceParams p;
    double sr = 48000.0, gain = 1.0, burst = 0.0, ring = 0.0;
    long t = 0;
    DecayEnv env;
    AnalogDrift drift;
    WhiteNoise noise;
    Svf bp, hp;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Tom : sinusoïde à balayage de hauteur réglable (« sweepable pitch »), petit coup de bruit.
    Timbre = profondeur du balayage, attaque = coup de baguette, extra1 = temps du balayage,
    couleur = excitation (deuxième partiel, saturation). */
class ImpactTom final : public Voice
{
public:
    ImpactTom (double baseHz, std::uint32_t seed) : base (baseHz), drift (seed), noise (seed ^ 0x2D2Du) {}
    void prepare (double s) override { sr = s; dc.prepare (sr, 15.0); reset(); }
    void reset() override { amp.reset(); hit.reset(); pEnv = 0.0; ph = ph2 = 0.0; lp.reset(); dc.reset(); drive.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override
    {
        drift.hit();
        gain = velocityToGain (v) * drift.level;
        amp.trigger(); hit.trigger (1.0 + 0.6 * acc); pEnv = 1.0; ph = ph2 = 0.0;
    }
    bool isActive() const override { return amp.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto c = p.extra2;
        const auto f0 = base * semis (p.tuneSemis) * drift.pitch;
        amp.setDecay (p.decayMs * drift.decay, sr);
        hit.setDecay (6.0, sr);
        const auto sweepCoef = timeConstantCoef (8.0 + 180.0 * p.extra1 * p.extra1, sr);
        const auto depth = 2.0 + 22.0 * p.tone;
        lp.set (3000.0, 0.7, sr);
        for (int i = 0; i < n; ++i)
        {
            const auto f = f0 * fastExp2 (depth * pEnv / 12.0);
            pEnv *= sweepCoef;
            ph += std::min (0.45, f / sr); ph -= std::floor (ph);
            ph2 += std::min (0.45, f * 1.51 / sr); ph2 -= std::floor (ph2);
            const auto a = amp.next();
            const auto s = (fastSin2Pi (ph) + c * 0.45 * fastSin2Pi (ph2)) * a;
            const auto h = lp.process (noise.next()).low * hit.next() * p.snap * 0.8;
            out[i] += (float) dc.process (drive.process (impactColour (s + h, 0.55 * c) * gain));
        }
    }

private:
    VoiceParams p;
    double base, sr = 48000.0, gain = 1.0, pEnv = 0.0, ph = 0.0, ph2 = 0.0;
    DecayEnv amp, hit;
    AnalogDrift drift;
    WhiteNoise noise;
    Svf lp;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Métal : 6 oscillateurs carrés inharmoniques + bruit, passe-bande puis passe-haut ; sert à la
    cymbale et aux charleys. Timbre = hauteur du filtre, attaque = part du métal (contre le bruit),
    extra1 = passe-haut. Couleur : cymbale = modulation en anneau des paires (le timbre change),
    charleys = excitation (pic de résonance, plus de souffle). */
class ImpactMetal final : public Voice
{
public:
    enum Kind { cymbal = 0, closedHat, openHat };
    ImpactMetal (Kind k, std::uint32_t seed) : kind (k), drift (seed), noise (seed ^ 0x6B6Bu) {}
    void prepare (double s) override { sr = s; dc.prepare (sr, 40.0); reset(); }
    void reset() override
    {
        for (size_t i = 0; i < oscs.size(); ++i) oscs[i].reset (0.11 * (double) i);
        env.reset(); bp.reset(); hp.reset(); peak.reset(); dc.reset(); drive.reset();
    }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override { drift.hit(); gain = velocityToGain (v) * drift.level * (1.0 + 0.25 * acc); env.trigger(); }
    void choke() override { env.choke(); }
    bool isActive() const override { return env.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        static constexpr std::array<double, 6> ratios { 245.0, 306.0, 365.0, 415.0, 437.0, 619.0 };
        const auto c = p.extra2;
        const auto t = semis (p.tuneSemis) * drift.pitch * (kind == cymbal ? 1.0 : 1.6);
        env.setDecay (p.decayMs * drift.decay, sr);
        const auto band = (kind == cymbal ? 5200.0 : 7600.0) + 5000.0 * p.tone;
        bp.set (std::min (0.45 * sr, band), kind == cymbal ? 0.9 : 1.3, sr);
        hp.set ((kind == cymbal ? 3000.0 : 6000.0) * std::pow (2.0, (p.extra1 - 0.5) * 2.0), 0.7, sr);
        peak.set (std::min (0.45 * sr, band * 1.25), 6.0, sr);
        const auto metalMix = 0.35 + 0.65 * p.snap;
        for (int i = 0; i < n; ++i)
        {
            std::array<double, 6> o {};
            for (size_t k = 0; k < oscs.size(); ++k) o[k] = oscs[k].next (ratios[k] * t, sr);
            double metal = (o[0] + o[1] + o[2] + o[3] + o[4] + o[5]) / 6.0;
            if (kind == cymbal && c > 0.001) metal = metal * (1.0 - c) + (o[0] * o[3] + o[1] * o[4] + o[2] * o[5]) / 3.0 * c;
            const auto w = noise.next() * (kind == cymbal ? 0.6 : 0.9) * (1.0 + (kind != cymbal ? 0.25 * c : 0.0));
            const auto src = metal * metalMix + w * (1.0 - metalMix);
            auto x = hp.process (bp.process (src).band * 2.4).high;
            if (kind != cymbal && c > 0.001) x = (x + peak.process (x).band * 0.6 * c) / (1.0 + 0.4 * c);
            out[i] += (float) dc.process (drive.process (x * env.next() * gain * 1.4));
        }
    }

private:
    Kind kind;
    VoiceParams p;
    double sr = 48000.0, gain = 1.0;
    std::array<BlepSquare, 6> oscs;
    DecayEnv env;
    AnalogDrift drift;
    WhiteNoise noise;
    Svf bp, hp, peak;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Tambour FM : porteuse sinusoïdale modulée par une deuxième (hauteur libre).
    Accord = hauteur de la porteuse, timbre = quantité de FM, extra1 = hauteur du modulateur,
    attaque = claquant (la FM décroît plus vite), couleur = enveloppe de hauteur calée sur le
    déclin (la porteuse descend avec le son). */
class ImpactFm final : public Voice
{
public:
    explicit ImpactFm (double baseHz = 110.0, std::uint32_t seed = 0x1A0Fu) : base (baseHz), drift (seed) {}
    void prepare (double s) override { sr = s; dc.prepare (sr, 15.0); reset(); }
    void reset() override { amp.reset(); fmEnv.reset(); shortP = 0.0; cPh = mPh = 0.0; dc.reset(); drive.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override
    {
        drift.hit();
        gain = velocityToGain (v) * drift.level;
        amp.trigger(); fmEnv.trigger (1.0 + 0.5 * acc); shortP = 1.0; cPh = mPh = 0.0;
    }
    bool isActive() const override { return amp.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto c = p.extra2;
        const auto fc = base * semis (p.tuneSemis) * drift.pitch;
        const auto fm = 30.0 * std::pow (150.0, p.extra1); // 30 Hz à 4,5 kHz
        amp.setDecay (p.decayMs * drift.decay, sr);
        fmEnv.setDecay (std::max (5.0, p.decayMs * (1.0 - 0.85 * p.snap)), sr);
        const auto shortCoef = timeConstantCoef (8.0, sr);
        const auto index = 0.2 + 7.0 * p.tone * p.tone;
        for (int i = 0; i < n; ++i)
        {
            const auto a = amp.next();
            // couleur : la hauteur suit l'enveloppe de déclin (jusqu'à 2 octaves au départ)
            const auto bend = fastExp2 ((2.0 * shortP * (1.0 - c) + 24.0 * c * a) / 12.0);
            shortP *= shortCoef;
            mPh += std::min (0.45, fm / sr); mPh -= std::floor (mPh);
            const auto m = fastSin2Pi (mPh) * index * fmEnv.next();
            cPh += std::min (0.45, fc * bend / sr); cPh -= std::floor (cPh);
            const auto y = fastSin2Pi (cPh + m * 0.159) * a;
            out[i] += (float) dc.process (drive.process (y * gain * 0.9));
        }
    }

private:
    VoiceParams p;
    double base, sr = 48000.0, gain = 1.0, shortP = 0.0, cPh = 0.0, mPh = 0.0;
    DecayEnv amp, fmEnv;
    AnalogDrift drift;
    Drive drive;
    DcBlocker dc;
};

} // namespace grv::dsp
