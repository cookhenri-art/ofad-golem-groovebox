/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Voix de batterie modélisées sur les principes des circuits analogiques.

    Ce qui fait le son d'une boîte analogique, et qui est reproduit ici :
      - le **T ponté** (808 et suivantes) : un résonateur que l'impulsion de déclenchement fait
        sonner ; la hauteur vient des composants, le déclin du facteur de qualité. Redéclencher
        pendant qu'il sonne ajoute l'impulsion à ce qui sonne déjà (pas de remise à zéro) ;
        à forte amplitude il comprime un peu et baisse légèrement de hauteur ;
      - l'**oscillateur à balayage** (grosses caisses « dures », type 909 et batteries analogiques
        modernes) : oscillateur dont la hauteur part haut et redescend, puis mise en forme ;
      - les **métaux** : 6 oscillateurs carrés non harmoniques, deux bandes passantes, un VCA,
        un passe-haut ;
      - le **bruit** analogique filtré, les salves de bruit du clap, la cloche à 2 carrés ;
      - la **dérive** : tolérance propre à chaque voix (deux machines ne sonnent pas pareil),
        petite variation à chaque coup, lente dérive d'échauffement. C'est ce qui rend vivante
        une suite de coups identiques.

    Chaque voix a 7 réglages : accord, déclin, timbre, attaque, saturation, et deux réglages
    propres à son circuit (extra1, extra2). Niveau, panoramique, filtre et enveloppe d'amplitude
    sont ceux du pad (DrumKit.h).
    Classes pures C++, sans allocation dans le rendu, sans dépendance JUCE.
*/

#pragma once

#include "Common.h"
#include "KickVoice.h"

#include <array>

namespace grv::dsp
{

struct VoiceParams
{
    double tuneSemis = 0.0;   // -24..+24 demi-tons autour de la hauteur de base de la voix
    double decayMs   = 300.0; // déclin à -60 dB
    double tone      = 0.5;   // 0..1, sens propre à chaque voix (balayage, timbre, filtre…)
    double snap      = 0.5;   // 0..1, attaque / claquant / métal / départ de l'échantillon
    double driveDb   = 0.0;   // 0..24 dB avant saturation
    double extra1    = 0.5;   // 0..1, réglage propre au circuit (voir chaque voix)
    double extra2    = 0.5;   // 0..1, réglage propre au circuit
    double extra3    = 0.5;   // 0..1, 3e réglage propre (v0.10) ; 0,5 = comportement d'avant
};

class Voice
{
public:
    virtual ~Voice() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual void reset() = 0;
    virtual void setParams (const VoiceParams&) = 0;
    virtual void trigger (double velocity) = 0;
    /** Étouffement rapide (charley ouvert coupé par le charley fermé). */
    virtual void choke() {}
    virtual bool isActive() const = 0;
    /** Ajoute le rendu mono de la voix dans out[0..n). */
    virtual void renderAdding (float* out, int n) = 0;

    /** Accent du prochain coup (0..1), donné juste avant trigger(). Le niveau de l'accent est
        appliqué par le pad (DrumKit) ; chaque voix y ajoute ce que son circuit fait d'une
        impulsion de déclenchement plus forte (attaque, part du bruit, non-linéarités). */
    void setAccent (double amount) noexcept { acc = std::clamp (amount, 0.0, 1.0); }

protected:
    double acc = 0.0;
};

inline double semis (double s) noexcept { return std::pow (2.0, s / 12.0); }
inline double velocityToGain (double v) noexcept { return 0.2 + 0.8 * std::clamp (v, 0.0, 1.0); }

//==============================================================================
/** Dérive analogique d'une voix : tolérance des composants (fixe pour la voix), petite
    variation à chaque coup, dérive lente d'échauffement (marche aléatoire bornée). */
class AnalogDrift
{
public:
    explicit AnalogDrift (std::uint32_t seed) noexcept : rng (seed)
    {
        tolPitch = semis (0.08 * rng.next()); // ±8 centièmes de demi-ton
        tolDecay = 1.0 + 0.05 * rng.next();   // ±5 %
    }

    void hit() noexcept
    {
        walk = std::clamp (walk * 0.997 + 0.04 * rng.next(), -1.0, 1.0);
        // v0.30 : variation d'un coup à l'autre ramenée sous le seuil d'écoute (la régularité prime ;
        // la tolérance fixe de la voix garde le caractère analogique)
        pitch = tolPitch * semis (0.008 * rng.next() + 0.02 * walk); // ±0,8 cent par coup, ±2 cents d'échauffement
        decay = tolDecay * (1.0 + 0.008 * rng.next());
        level = 1.0 + 0.01 * rng.next();
    }

    double pitch = 1.0, decay = 1.0, level = 1.0;

private:
    WhiteNoise rng;
    double tolPitch = 1.0, tolDecay = 1.0, walk = 0.0;
};

/** Saturation commune : crête 1 conservée, petits signaux amplifiés quand on pousse le drive. */
class Drive
{
public:
    void reset() noexcept { shaper.reset(); }
    void set (double db) noexcept { gain = dbToGain (db); norm = 1.0 / softClip (gain); }
    double process (double x) noexcept { return shaper.process (x * gain) * norm; }

private:
    AdaaSoftClip shaper;
    double gain = 1.0, norm = 1.0 / softClip (1.0);
};

/** Enveloppe à attaque instantanée et déclin exponentiel (décharge d'un condensateur), avec étouffement. */
class DecayEnv
{
public:
    void trigger (double level = 1.0) noexcept { value = level; choking = false; }
    void choke() noexcept { choking = true; }
    void setDecay (double ms, double sampleRate) noexcept
    {
        coef = decay60Coef (ms, sampleRate);
        chokeCoef = decay60Coef (6.0, sampleRate);
    }
    double next() noexcept { const auto v = value; value *= choking ? chokeCoef : coef; return v; }
    bool isActive() const noexcept { return value > 1.0e-5; }
    double level() const noexcept { return value; }
    void reset() noexcept { value = 0.0; choking = false; }

private:
    double value = 0.0, coef = 0.0, chokeCoef = 0.0;
    bool choking = false;
};

//==============================================================================
/** Résonateur amorti : modèle du T ponté. Une impulsion ajoutée à l'état le fait sonner
    (sinusoïde qui démarre à zéro, sans clic), le déclin suit le facteur de qualité.
    Non-linéarité du circuit : à forte amplitude la hauteur baisse un peu et le son se tasse. */
class Resonator
{
public:
    void reset() noexcept { x = y = 0.0; }
    void excite (double amount) noexcept { x += amount; }
    double amplitude() const noexcept { return std::sqrt (x * x + y * y); }

    /** freqHz : hauteur ; decayCoef : coefficient par échantillon (decay60Coef) ; input : impulsion. */
    double tick (double freqHz, double decayCoef, double sampleRate, double input = 0.0) noexcept
    {
        const auto a2 = std::min (4.0, x * x + y * y);
        const auto w = std::min (0.45, freqHz / sampleRate * (1.0 - bend * a2));
        const auto c = fastSin2Pi (w + 0.25), s = fastSin2Pi (w);
        // tassement : le déclin s'accélère légèrement quand l'amplitude dépasse 1
        const auto r = decayCoef * (a2 > 1.0 ? 1.0 - squash * (a2 - 1.0) : 1.0);
        const auto nx = (x * c - y * s) * r + input;
        const auto ny = (x * s + y * c) * r;
        x = nx; y = ny;
        return y;
    }

    double bend = 0.02, squash = 0.0005;

private:
    double x = 0.0, y = 0.0;
};

/** Mise en forme progressive : 0 = sinusoïde pure, 1 = forme arrondie riche en harmoniques
    (étage de transistor poussé). Crête conservée. */
inline double colour (double y, double amount) noexcept
{
    if (amount <= 0.001) return y;
    const auto k = 1.0 + 7.0 * amount;
    return y + (softClip (y * k) / softClip (k) - y) * amount;
}

//==============================================================================
/** Grosse caisse à oscillateur balayé (batteries analogiques modernes, type 909).
    Timbre = profondeur du balayage, attaque = click, extra1 = temps du balayage (2 à 80 ms),
    extra2 = forme (sinusoïde → onde arrondie). */
class KickDrum final : public Voice
{
public:
    explicit KickDrum (double baseHz = 50.0, double sweepRangeSemis = 48.0, double sweepTimeMs = 22.0, std::uint32_t seed = 0x51A7u)
        : base (baseHz), sweepRange (sweepRangeSemis), sweepTime (sweepTimeMs), drift (seed) {}
    void prepare (double sr) override { kick.prepare (sr); }
    void reset() override { kick.reset(); }
    void setParams (const VoiceParams& p) override
    {
        KickVoice::Params k;
        k.tuneHz = base * semis (p.tuneSemis) * drift.pitch;
        k.decayMs = p.decayMs * drift.decay;
        k.sweepSemis = p.tone * sweepRange;
        k.sweepMs = sweepTime * (0.15 + 1.7 * p.extra1);
        k.click = p.snap;
        k.driveDb = p.driveDb;
        k.levelDb = 0.0;
        k.shape = p.extra2;
        kick.setParams (k);
    }
    void trigger (double v) override
    {
        drift.hit();
        kick.accentAttack = 1.0 + 0.8 * acc; // accent : coup plus fort sur le circuit d'enveloppe, attaque plus marquée
        kick.trigger (std::clamp (v * drift.level, 0.0, 1.0), 0.0);
    }
    bool isActive() const override { return kick.isActive(); }
    void renderAdding (float* out, int n) override { kick.renderAdding (out, n); }

private:
    KickVoice kick;
    double base, sweepRange, sweepTime;
    AnalogDrift drift;
};

//==============================================================================
/** Voix à T ponté : grosse caisse de type 808, toms, congas, claves.
    Timbre = petit balayage de hauteur au coup, attaque = impulsion audible + souffle,
    extra1 = temps du balayage, extra2 = couleur (étage poussé). */
class BridgedTDrum final : public Voice
{
public:
    BridgedTDrum (double baseHz = 110.0, double sweepRangeSemis = 12.0, double noiseAmount = 0.6, std::uint32_t seed = 0x7A5Au)
        : base (baseHz), sweepRange (sweepRangeSemis), noiseAmt (noiseAmount), drift (seed), noise (seed * 2654435761u + 1u) {}
    void prepare (double s) override { sr = s; dc.prepare (sr); reset(); }
    void reset() override { res.reset(); tailRes.reset(); pitch = 0.0; click = 0.0; noiseEnv.reset(); clickHp.reset(); noiseLp.reset(); drive.reset(); dc.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override
    {
        drift.hit();
        const auto g = velocityToGain (v) * drift.level;
        // accent : l'impulsion plus forte pousse le T ponté dans sa zone non linéaire (léger
        // fléchissement de hauteur et tassement au départ) ; front et souffle suivent l'impulsion
        const auto push = 1.0 + 0.6 * acc;
        // 1.1 : chaque coup excite un résonateur au repos (attaque identique d'un coup à l'autre) ;
        // la résonance encore en cours part sur un second résonateur qui s'éteint en ~8 ms (sans
        // clic). Avant, le nouveau coup s'ajoutait à la résonance : selon sa phase, l'attaque
        // changeait un coup sur deux ou trois.
        tailRes = res; res.reset();
        res.excite (g * push);
        pitch = 1.0; click = g * push; noiseEnv.trigger (g * push);
    }
    bool isActive() const override { return res.amplitude() > 2.0e-5 || tailRes.amplitude() > 2.0e-5 || click > 1.0e-4 || noiseEnv.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto f = base * semis (p.tuneSemis) * drift.pitch;
        const auto dcoef = decay60Coef (p.decayMs * drift.decay, sr);
        const auto tailCoef = decay60Coef (8.0, sr);
        const auto pcoef = timeConstantCoef (2.0 + 78.0 * p.extra1 * p.extra1, sr);
        const auto ccoef = timeConstantCoef (0.35, sr);
        const auto sweep = p.tone * sweepRange;
        noiseEnv.setDecay (35.0, sr);
        clickHp.set (900.0, 0.7, sr);
        noiseLp.set (2600.0 + 2500.0 * p.tone, 0.7, sr);
        for (int i = 0; i < n; ++i)
        {
            const auto fNow = f * fastExp2 (sweep * pitch / 12.0);
            const auto y = res.tick (fNow, dcoef, sr) + tailRes.tick (fNow, tailCoef, sr);
            pitch *= pcoef;
            const auto c = clickHp.process (click).high * p.snap * 0.9; // front de l'impulsion de déclenchement
            click *= ccoef;
            const auto nz = noiseLp.process (noise.next()).low * noiseEnv.next() * p.snap * noiseAmt * 2.0 * p.extra3;
            out[i] += (float) dc.process (drive.process (colour (y, p.extra2) + c + nz) * 0.95);
        }
    }

private:
    double base, sweepRange, noiseAmt;
    VoiceParams p;
    double sr = 48000.0, pitch = 0.0, click = 0.0;
    Resonator res, tailRes;
    AnalogDrift drift;
    DecayEnv noiseEnv;
    Svf clickHp, noiseLp;
    WhiteNoise noise;
    Drive drive;
    DcBlocker dc;
};

/** Tom : T ponté à balayage plus marqué et souffle d'attaque. */
class TomDrum final : public Voice
{
public:
    explicit TomDrum (double baseHz = 110.0, double sweepRangeSemis = 12.0, std::uint32_t seed = 0x70Au)
        : v (baseHz, sweepRangeSemis, 0.6, seed) {}
    void prepare (double s) override { v.prepare (s); }
    void reset() override { v.reset(); }
    void setParams (const VoiceParams& p) override { v.setParams (p); }
    void trigger (double x) override { v.trigger (x); }
    bool isActive() const override { return v.isActive(); }
    void renderAdding (float* out, int n) override { v.renderAdding (out, n); }

private:
    BridgedTDrum v;
};

//==============================================================================
/** Caisse claire : deux résonateurs accordés (corps, léger glissé au coup) et bruit filtré
    à sa propre enveloppe (le timbre « snappy »).
    Timbre = brillance du bruit, claquant = part du bruit, extra1 = couleur du bruit (passe-haut
    de 400 Hz à 5 kHz), extra2 = longueur du corps. */
class SnareDrum final : public Voice
{
public:
    SnareDrum (double mode1Hz = 180.0, double mode2Hz = 330.0, double noiseHighpassHz = 1400.0, double bendAmount = 0.6, std::uint32_t seed = 0x5A4Eu)
        : f1 (mode1Hz), f2 (mode2Hz), hpHz (noiseHighpassHz), bendAmt (bendAmount), drift (seed), noise (seed ^ 0x1234567u) {}
    void prepare (double s) override { sr = s; dc.prepare (sr); reset(); }
    void reset() override { r1.reset(); r2.reset(); noiseEnv.reset(); hp.reset(); lp.reset(); drive.reset(); dc.reset(); pitch = 0.0; }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override
    {
        drift.hit();
        gain = velocityToGain (v) * drift.level;
        // accent : il change l'équilibre corps / bruit (le timbre claque davantage)
        noiseAcc = 1.0 + 0.5 * acc;
        r1.excite (0.65 * gain); r2.excite (0.35 * gain);
        noiseEnv.trigger(); pitch = 1.0;
    }
    bool isActive() const override { return r1.amplitude() > 2.0e-5 || noiseEnv.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto t = semis (p.tuneSemis) * drift.pitch;
        const auto bodyMs = std::clamp (p.decayMs * (0.15 + 0.6 * p.extra2), 25.0, 900.0) * drift.decay;
        const auto c1 = decay60Coef (bodyMs, sr), c2 = decay60Coef (bodyMs * 0.7, sr);
        noiseEnv.setDecay (p.decayMs * drift.decay, sr);
        // couleur du bruit : passe-haut de 400 Hz à 5 kHz autour de la valeur du circuit
        hp.set (hpHz * std::pow (2.0, (p.extra1 - 0.5) * 3.6), 0.7, sr);
        lp.set (2500.0 + p.tone * 11000.0, 0.7, sr);
        const auto pitchCoef = timeConstantCoef (8.0, sr);
        const auto bodyMix = 1.0 - p.snap * 0.55;
        const auto noiseMix = 0.25 + p.snap * 0.95;

        for (int i = 0; i < n; ++i)
        {
            const auto bend = 1.0 + bendAmt * 2.0 * p.extra3 * pitch;
            pitch *= pitchCoef;
            const auto b = r1.tick (f1 * t * bend, c1, sr) + r2.tick (f2 * t * bend, c2, sr);
            const auto nz = lp.process (hp.process (noise.next()).high).low * noiseEnv.next() * gain;
            out[i] += (float) dc.process (drive.process ((b * bodyMix + nz * noiseMix * noiseAcc * 1.6) * 0.8));
        }
    }

private:
    double f1, f2, hpHz, bendAmt;
    VoiceParams p;
    double sr = 48000.0, pitch = 0.0, gain = 1.0, noiseAcc = 1.0;
    Resonator r1, r2;
    AnalogDrift drift;
    DecayEnv noiseEnv;
    Svf hp, lp;
    WhiteNoise noise;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Clap : salves de bruit rapprochées (enveloppe en dents de scie), puis une queue de bruit
    plus longue (la « réverbération » du circuit), le tout en passe-bande.
    Timbre = hauteur du filtre, écart = espacement des salves, extra1 = nombre de salves (1 à 6),
    extra2 = niveau de la queue. */
class ClapDrum final : public Voice
{
public:
    explicit ClapDrum (std::uint32_t seed = 0xC1A9u) : drift (seed), noise (seed ^ 0x2468ACEu) {}
    void prepare (double s) override { sr = s; dc.prepare (sr); reset(); }
    void reset() override { t = -1; bp.reset(); hp.reset(); drive.reset(); dc.reset(); tail = 0.0; }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override { drift.hit(); gain = velocityToGain (v) * drift.level; t = 0; tail = 0.0; burst = 0.0; nextBurst = 0.0; }
    bool isActive() const override { return t >= 0; }

    void renderAdding (float* out, int n) override
    {
        if (t < 0) return;
        const auto spread = (6.0 + p.snap * 10.0) * 0.001 * sr * drift.decay;
        const auto bursts = 1 + (int) std::lround (std::clamp (p.extra1, 0.0, 1.0) * 5.0);
        const auto burstCoef = decay60Coef (9.0, sr);
        const auto tailCoef = decay60Coef (p.decayMs * drift.decay, sr);
        const auto tailLevel = 0.15 + 1.35 * p.extra2;
        bp.set (1000.0 * semis (p.tuneSemis) * drift.pitch * (0.7 + 0.8 * p.tone), 1.8, sr);
        hp.set (450.0, 0.7, sr);
        const auto lastBurst = (long) ((double) (bursts - 1) * spread);

        for (int i = 0; i < n; ++i)
        {
            if (t <= lastBurst && (double) t >= nextBurst)
            {
                burst = 1.0;
                // aléa : au-delà de 0,5, l'écart entre les salves varie (0,5 = régulier, comme avant)
                nextBurst += spread * (1.0 + std::max (0.0, p.extra3 - 0.5) * 1.4 * jitter.next());
            }
            const auto env = std::max (t <= lastBurst + (long) spread ? burst : 0.0, tail * tailLevel);
            burst *= burstCoef;
            if (t == lastBurst) tail = 1.0;
            tail *= tailCoef;

            const auto x = hp.process (bp.process (noise.next()).band).high * env * 3.2;
            out[i] += (float) dc.process (drive.process (x * gain));

            ++t;
            if (t > lastBurst + (long) spread && tail < 1.0e-5) { t = -1; break; }
        }
    }

private:
    VoiceParams p;
    double sr = 48000.0, gain = 1.0, tail = 0.0, burst = 0.0, nextBurst = 0.0;
    long t = -1;
    AnalogDrift drift;
    Svf bp, hp;
    WhiteNoise noise, jitter { 0x7E57u };
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Rimshot / claves : deux T pontés aigus très amortis, front d'impulsion claquant.
    Timbre = part du mode aigu, claquant = impulsion, extra1 = passe-haut (corps plus ou moins
    mince), extra2 = couleur. */
class RimDrum final : public Voice
{
public:
    RimDrum (double highHz = 1700.0, double lowHz = 480.0, std::uint32_t seed = 0x413u) : fHigh (highHz), fLow (lowHz), drift (seed), noise (seed ^ 0x13579BDu) {}
    void prepare (double s) override { sr = s; dc.prepare (sr); reset(); }
    void reset() override { r1.reset(); r2.reset(); clickEnv.reset(); hp.reset(); drive.reset(); dc.reset(); pending = 0.0; }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb + 4.0); }
    void trigger (double v) override
    {
        drift.hit();
        gain = velocityToGain (v) * drift.level;
        pending = gain;          // les résonateurs sont excités au rendu, avec les réglages du coup
        clickEnv.trigger (gain * (1.0 + 0.6 * acc)); // accent : claquement plus marqué
    }
    bool isActive() const override { return pending > 0.0 || r1.amplitude() + r2.amplitude() > 2.0e-5 || clickEnv.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto t = semis (p.tuneSemis) * drift.pitch;
        const auto c = decay60Coef (p.decayMs * drift.decay, sr);
        clickEnv.setDecay (2.0, sr);
        hp.set (120.0 * std::pow (8.0, p.extra1), 0.7, sr);
        if (pending > 0.0) { r1.excite (pending * (0.3 + 0.7 * p.tone)); r2.excite (pending * (1.0 - 0.6 * p.tone)); pending = 0.0; }
        for (int i = 0; i < n; ++i)
        {
            const auto x = r1.tick (fHigh * t, c, sr) + r2.tick (fLow * t, c, sr)
                         + noise.next() * clickEnv.next() * p.snap * 0.8 * 2.0 * p.extra3;
            out[i] += (float) dc.process (drive.process (colour (hp.process (x).high, p.extra2) * 0.7));
        }
    }

private:
    double fHigh, fLow;
    VoiceParams p;
    double sr = 48000.0, gain = 1.0, pending = 0.0;
    Resonator r1, r2;
    AnalogDrift drift;
    DecayEnv clickEnv;
    Svf hp;
    WhiteNoise noise;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Charleys et cymbales : six oscillateurs carrés non harmoniques (fréquences du circuit 808),
    deux bandes passantes (la basse plus longue sur les cymbales), VCA, passe-haut.
    Timbre = hauteur des bandes, métal = part des oscillateurs face au bruit, extra1 = passe-haut,
    extra2 = résonance des bandes. Le charley fermé étouffe l'ouvert. */
class HatDrum final : public Voice
{
public:
    /** bits > 0 : le métal passe par une quantification basse résolution et un rééchantillonnage
        (caractère des cymbales stockées en 6 bits des boîtes hybrides de la fin des années 80). */
    /** ratioSet : 0 = rapports du circuit des années 80, 1 = jeu « métallique » plus serré et plus
        inharmonique. */
    explicit HatDrum (int quantiseBits = 0, double bandBaseHz = 6500.0, std::uint32_t seed = 0x4A7u, int ratioSet = 0)
        : bits (quantiseBits), bandBase (bandBaseHz), set (ratioSet), drift (seed), noise (seed ^ 0x0F1E2D3u) {}
    void prepare (double s) override { sr = s; dc.prepare (sr, 20.0); reset(); }
    void reset() override
    {
        for (size_t i = 0; i < oscs.size(); ++i) oscs[i].reset (0.13 * (double) i);
        env.reset(); lowEnv.reset(); bpHigh.reset(); bpLow.reset(); hp.reset(); drive.reset(); dc.reset();
    }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override { drift.hit(); gain = velocityToGain (v) * drift.level; env.trigger(); lowEnv.trigger(); }
    void choke() override { env.choke(); lowEnv.choke(); }
    bool isActive() const override { return env.isActive() || lowEnv.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        static constexpr std::array<double, 6> classic { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
        static constexpr std::array<double, 6> metallic { 263.0, 400.0, 421.0, 474.0, 587.0, 845.0 };
        const auto& ratios = set == 1 ? metallic : classic;
        const auto t = semis (p.tuneSemis) * drift.pitch;
        env.setDecay (p.decayMs * drift.decay, sr);
        // extra3 : longueur de la bande basse (0,5 = 1,6 fois la haute)
        lowEnv.setDecay (p.decayMs * drift.decay * (0.4 + 2.4 * p.extra3), sr);
        const auto q = 0.6 + 3.4 * p.extra2;
        bpHigh.set (bandBase + p.tone * 5000.0, q, sr);
        bpLow.set ((bandBase + p.tone * 5000.0) * 0.48, q, sr);
        const auto levels = bits > 0 ? std::pow (2.0, bits - 1) : 0.0;
        const auto holdLen = bits > 0 ? std::max (1, (int) std::lround (sr / 32000.0)) : 1;
        hp.set ((3200.0 + p.tone * 2000.0) * std::pow (2.0, (p.extra1 - 0.5) * 2.4), 0.7, sr);

        for (int i = 0; i < n; ++i)
        {
            double metal = 0.0;
            for (size_t o = 0; o < oscs.size(); ++o)
                metal += oscs[o].next (ratios[o] * t, sr);
            auto src = metal * (1.0 / 6.0) * p.snap + noise.next() * (1.0 - p.snap) * 0.8;
            if (bits > 0)
            {
                if (--holdCount <= 0) { held = std::round (src * levels) / levels; holdCount = holdLen; }
                src = held;
            }
            const auto x = bpHigh.process (src).band * env.next() + 0.35 * bpLow.process (src).band * lowEnv.next();
            out[i] += (float) dc.process (drive.process (hp.process (x).high * 2.6 * gain));
        }
    }

private:
    int bits;
    double bandBase;
    int set;
    double held = 0.0;
    int holdCount = 0;
    VoiceParams p;
    double sr = 48000.0, gain = 1.0;
    std::array<BlepSquare, 6> oscs;
    AnalogDrift drift;
    DecayEnv env, lowEnv;
    Svf bpHigh, bpLow, hp;
    WhiteNoise noise;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Cloche : deux oscillateurs carrés dans un passe-bande, enveloppe à deux pentes (circuit 808).
    Timbre = hauteur du filtre, attaque = part de la pente rapide, extra1 = écart entre les deux
    oscillateurs, extra2 = résonance du filtre. */
class CowbellDrum final : public Voice
{
public:
    explicit CowbellDrum (std::uint32_t seed = 0xC0Bu) : drift (seed) {}
    void prepare (double s) override { sr = s; dc.prepare (sr, 30.0); reset(); }
    void reset() override { o1.reset(); o2.reset (0.27); fast.reset(); slow.reset(); bp.reset(); drive.reset(); dc.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }
    void trigger (double v) override { drift.hit(); gain = velocityToGain (v) * drift.level; fast.trigger(); slow.trigger(); }
    bool isActive() const override { return slow.isActive() || fast.isActive(); }

    void renderAdding (float* out, int n) override
    {
        if (! isActive()) return;
        const auto t = semis (p.tuneSemis) * drift.pitch;
        const auto ratio = (800.0 / 540.0) * std::pow (2.0, (p.extra1 - 0.5) * 0.5);
        fast.setDecay (45.0 * 2.0 * p.extra3 + 5.0, sr);
        slow.setDecay (p.decayMs * drift.decay, sr);
        bp.set (2640.0 * t * (0.6 + 0.8 * p.tone), 1.0 + 4.0 * p.extra2, sr);
        for (int i = 0; i < n; ++i)
        {
            const auto x = (o1.next (540.0 * t, sr) + o2.next (540.0 * ratio * t, sr)) * 0.5;
            const auto e = fast.next() * p.snap + slow.next() * (1.0 - 0.5 * p.snap);
            out[i] += (float) dc.process (drive.process (bp.process (x).band * e * gain * 1.6));
        }
    }

private:
    VoiceParams p;
    double sr = 48000.0, gain = 1.0;
    BlepSquare o1, o2;
    AnalogDrift drift;
    DecayEnv fast, slow;
    Svf bp;
    Drive drive;
    DcBlocker dc;
};

//==============================================================================
/** Lecteur d'échantillon mono. Les données appartiennent à l'appelant, qui garantit leur durée
    de vie tant que la voix peut les lire. Deux têtes de lecture : redéclencher fait fondre
    l'ancienne en 3 ms au lieu de produire un clic.
    Accord = vitesse de lecture, déclin = fondu (4000 ms = jusqu'au bout), filtre = passe-bas,
    départ = point de départ, extra1 = point de fin, extra2 = attaque (fondu d'entrée, 0 à 50 ms). */
class SampleVoice final : public Voice
{
public:
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { for (auto& h : heads) h = {}; lp.reset(); drive.reset(); }
    void setParams (const VoiceParams& v) override { p = v; drive.set (p.driveDb); }

    void setSample (const float* newData, int newLength, double newSampleRate) noexcept
    {
        reset();
        data = newData;
        length = newData != nullptr ? newLength : 0;
        fileRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    }

    bool hasSample() const noexcept { return data != nullptr && length > 1; }

    void trigger (double v) override
    {
        if (! hasSample()) return;
        heads[1] = heads[0];
        heads[1].fading = heads[1].active;
        auto& h = heads[0];
        h.active = true;
        h.fading = false;
        h.pos = 0.0;
        h.pending = true;        // départ et attaque lus au rendu, avec les réglages du coup
        h.env = 1.0;
        h.fade = 1.0;
        h.rise = 1.0;
        h.gain = velocityToGain (v);
    }

    bool isActive() const override { return heads[0].active || heads[1].active; }

    void renderAdding (float* out, int n) override
    {
        if (! isActive() || ! hasSample()) return;
        const auto rate = fileRate / sr * semis (p.tuneSemis);
        const auto decayCoef = p.decayMs >= 3990.0 ? 1.0 : decay60Coef (p.decayMs, sr);
        const auto fadeCoef = decay60Coef (3.0, sr);
        const auto riseStep = 1.0 / std::max (1.0, p.extra2 * 0.05 * sr);
        const auto startPos = std::clamp (p.snap, 0.0, 1.0) * 0.95 * (double) (length - 1);
        const auto endPos = std::max (startPos + 2.0, std::clamp (p.extra1, 0.02, 1.0) * (double) (length - 1));
        const bool filtered = p.tone < 0.99;
        if (filtered) lp.set (200.0 * std::pow (100.0, p.tone), 0.707, sr);

        for (int i = 0; i < n; ++i)
        {
            double x = 0.0;
            for (auto& h : heads)
            {
                if (! h.active) continue;
                if (h.pending) { h.pos = startPos; h.rise = p.extra2 > 0.001 ? 0.0 : 1.0; h.pending = false; }
                x += readAt (h.pos) * h.env * h.fade * h.gain * h.rise;
                h.pos += rate;
                h.env *= decayCoef;
                h.rise = std::min (1.0, h.rise + riseStep);
                if (h.fading) h.fade *= fadeCoef;
                if (h.pos >= endPos || h.pos >= (double) (length - 1) || h.env < 1.0e-5 || h.fade < 1.0e-4) h.active = false;
            }
            if (filtered) x = lp.process (x).low;
            out[i] += (float) drive.process (x);
        }
    }

private:
    struct Head { bool active = false, fading = false, pending = false; double pos = 0.0, env = 0.0, fade = 1.0, gain = 1.0, rise = 1.0; };

    /** Interpolation d'Hermite à 4 points. */
    double readAt (double pos) const noexcept
    {
        const auto i = (int) pos;
        const auto f = pos - (double) i;
        auto at = [this] (int k) { return (double) data[std::clamp (k, 0, length - 1)]; };
        const auto xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const auto c1 = 0.5 * (x1 - xm1);
        const auto c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const auto c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    VoiceParams p;
    double sr = 48000.0, fileRate = 44100.0;
    const float* data = nullptr;
    int length = 0;
    std::array<Head, 2> heads {};
    Svf lp;
    Drive drive;
};

} // namespace grv::dsp
