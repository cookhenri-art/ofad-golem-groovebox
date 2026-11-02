/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Poly à filtre d'état » — principe des polyphoniques analogiques récents à 6 voix construits
    autour du filtre d'état à 2 pôles des modules américains des années 70 (architecture reprise,
    ni nom, ni panneau, ni son d'origine) :
      - deux oscillateurs par voix à forme continue : A de la scie à l'impulsion (largeur réglable),
        synchronisable sur B ; B du triangle à la scie puis à l'impulsion, fin, mode basse
        fréquence, suivi du clavier débrayable ; sous-oscillateur carré une octave sous A ;
      - mélangeur A, sous-oscillateur, B, bruit blanc ;
      - filtre d'état 2 pôles (12 dB/oct) à mode continu passe-bas → réjection → passe-haut, ou
        passe-bande ; enveloppe bipolaire, vélocité vers la quantité d'enveloppe, suivi du clavier
        (arrêt, moitié, plein) ;
      - X-MOD : l'enveloppe du filtre et l'oscillateur B (bipolaires) modulent la fréquence, la
        forme et la largeur de A, la coupure et le mode du filtre ;
      - LFO (sinus, scie, scie inversée, carré, aléatoire) vers les fréquences A et B, les
        largeurs, la coupure, le mode du filtre et l'ampli ; quantité initiale + molette ;
      - pression vers le filtre et le LFO ; « instabilité » (dérive des oscillateurs et du filtre
        voix par voix) ; portamento, unisson. 8 voix (6 sur la machine d'origine).
*/

#pragma once

#include "../synth/SynthBase.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace ob6
{
    enum Slot
    {
        voices = 0, unison, uniVoices, uniDetune, glide, vintage, tune, level, velAmp, velEnv,
        aFreq, aShape, aPw, aSync, bFreq, bFine, bShape, bPw, bLow, bKey,
        mixA, mixSub, mixB, mixNoise,
        cutoff, reso, envAmt, keytrack, fMode, bandpass,
        fA, fD, fS, fR, aA, aD, aS, aR,
        lfoRate, lfoSync, lfoShape, lfoAmt, lfoFreqA, lfoFreqB, lfoPw, lfoCut, lfoMode, lfoAmp,
        xEnv, xOscB, xFreqA, xShapeA, xPwA, xCut, xMode,
        atFilter, atLfo,
        numSlots
    };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            ModuleDesc m;
            m.type = "ob6";
            m.name = "\xC3\x89tat de Gr\xC3\xA2" "ce";
            m.family = "Nappes";
            m.principle = "Polyphonique analogique \xC3\xA0 2 oscillateurs \xC3\xA0 forme continue, filtre d'\xC3\xA9tat 2 p\xC3\xB4les passe-bas \xE2\x86\x92 r\xC3\xA9jection \xE2\x86\x92 passe-haut, X-Mod";
            m.params = {
                I ("voices", "Voix", "", 1, 8, 6),
                C ("unison", "Unisson", onOff),
                I ("univoices", "Voix d'unisson", "", 2, 8, 4),
                P ("unidetune", "D\xC3\xA9saccord unisson", "ct", 0, 50, 10),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("vintage", "Instabilit\xC3\xA9", "%", 0, 100, 20),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("level", "Volume", "dB", -24, 6, -6),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 30),
                P ("velenv", "V\xC3\xA9l. \xE2\x86\x92 env. filtre", "%", 0, 100, 30),

                I ("afreq", "Fr\xC3\xA9quence A", "dt", -24, 24, 0),
                P ("ashape", "Forme A (scie \xE2\x86\x92 impulsion)", "%", 0, 100, 0),
                P ("apw", "Largeur A", "%", 5, 95, 50),
                C ("async", "Synchro A sur B", onOff),
                I ("bfreq", "Fr\xC3\xA9quence B", "dt", -24, 24, 0),
                P ("bfine", "Fin B", "ct", -50, 50, 7),
                P ("bshape", "Forme B (tri. \xE2\x86\x92 scie \xE2\x86\x92 imp.)", "%", 0, 100, 50),
                P ("bpw", "Largeur B", "%", 5, 95, 50),
                C ("blow", "B basse fr\xC3\xA9q.", onOff),
                C ("bkey", "B clavier", onOff, 1),

                P ("mixa", "Osc. A", "%", 0, 100, 80),
                P ("mixsub", "Sous-oscillateur", "%", 0, 100, 0),
                P ("mixb", "Osc. B", "%", 0, 100, 70),
                P ("mixnoise", "Bruit", "%", 0, 100, 0),

                P ("cutoff", "Coupure", "Hz", 20, 18000, 1800, 0.3f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 20),
                P ("envamt", "Qt\xC3\xA9 env.", "%", -100, 100, 40),
                C ("keytrack", "Clavier", { "Arr\xC3\xAAt", "Moiti\xC3\xA9", "Plein" }, 1),
                P ("fmode", "Mode (bas \xE2\x86\x92 r\xC3\xA9jection \xE2\x86\x92 haut)", "%", 0, 100, 0),
                C ("bandpass", "Passe-bande", onOff),

                P ("fa", "Attaque", "ms", 0.5f, 10000, 5, 0.3f),
                P ("fd", "D\xC3\xA9" "clin", "ms", 5, 10000, 700, 0.3f),
                P ("fs", "Maintien", "%", 0, 100, 35),
                P ("fr", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 600, 0.3f),
                P ("aa", "Attaque", "ms", 0.5f, 10000, 3, 0.3f),
                P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 1000, 0.3f),
                P ("as", "Maintien", "%", 0, 100, 80),
                P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 700, 0.3f),

                P ("lforate", "Fr\xC3\xA9quence", "Hz", 0.05f, 25, 4, 0.4f),
                C ("lfosync", "Synchro", lfoSyncChoices()),
                C ("lfoshape", "Forme", { "Sinus", "Scie", "Scie inv.", "Carr\xC3\xA9", "Al\xC3\xA9" "atoire" }),
                P ("lfoamt", "Quantit\xC3\xA9 initiale", "%", 0, 100, 0),
                C ("lfofreqa", "Fr\xC3\xA9q. A", onOff, 1), C ("lfofreqb", "Fr\xC3\xA9q. B", onOff, 1), C ("lfopw", "Largeurs", onOff),
                C ("lfocut", "Coupure", onOff), C ("lfomode", "Mode du filtre", onOff), C ("lfoamp", "Ampli", onOff),

                P ("xenv", "Env. filtre", "%", -100, 100, 0),
                P ("xoscb", "Osc. B", "%", -100, 100, 0),
                C ("xfreqa", "Fr\xC3\xA9q. A", onOff), C ("xshapea", "Forme A", onOff), C ("xpwa", "Largeur A", onOff),
                C ("xcut", "Coupure", onOff), C ("xmode", "Mode du filtre", onOff),

                P ("atfilter", "Pression \xE2\x86\x92 filtre", "%", 0, 100, 0),
                P ("atlfo", "Pression \xE2\x86\x92 LFO", "%", 0, 100, 0),
            };
            m.sections = {
                { "global", "Global", "global", { "voices", "unison", "univoices", "unidetune", "glide", "vintage", "tune", "level" }, "global" },
                { "osca", "Oscillateur A", "osc", { "afreq", "ashape", "apw", "async", "mixa", "mixsub" }, "osc" },
                { "oscb", "Oscillateur B", "osc", { "bfreq", "bfine", "bshape", "bpw", "blow", "bkey", "mixb" }, "osc" },
                { "mix", "M\xC3\xA9langeur", "mix", { "mixa", "mixsub", "mixb", "mixnoise" }, "osc" },
                { "filter", "Filtre d'\xC3\xA9tat", "filter", { "cutoff", "reso", "envamt", "keytrack", "fmode", "bandpass", "velenv", "atfilter" }, "filter" },
                { "fenv", "Enveloppe filtre", "env", { "fa", "fd", "fs", "fr", "envamt" }, "env" },
                { "aenv", "Enveloppe ampli", "env", { "aa", "ad", "as", "ar", "velamp", "level" }, "amp" },
                { "lfo", "LFO", "lfo", { "lfosync", "lforate", "lfoshape", "lfoamt", "atlfo" }, "mod" },
                { "lfodest", "LFO vers", "mod", { "lfofreqa", "lfofreqb", "lfopw", "lfocut", "lfomode", "lfoamp" }, "mod" },
                { "xmod", "X-Mod", "mod", { "xenv", "xoscb", "xfreqa", "xshapea", "xpwa", "xcut", "xmode" }, "mod" },
            };
            const std::string N = "Nappes", K = "Claviers", B = "Basses", L = "Leads", BR = "Cuivres", S = "Cordes", PL = "Plucks", FX = "Effets";
            m.presets = {
                { "Init", { { "mixb", 0 }, { "cutoff", 18000 }, { "envamt", 0 }, { "as", 100 }, { "level", -8.9f } }, "Init" },
                { "Cuivres d'\xC3\xA9tat", { { "level", -9.2f }, { "ashape", 0 }, { "bshape", 50 }, { "bfine", 9 }, { "cutoff", 700 }, { "reso", 15 }, { "envamt", 55 }, { "fa", 70 }, { "fd", 700 }, { "fs", 45 },
                                               { "aa", 40 }, { "ar", 400 }, { "vintage", 30 } }, BR },
                { "Nappe en r\xC3\xA9jection", { { "level", -10.1f }, { "fmode", 50 }, { "cutoff", 1400 }, { "reso", 55 }, { "lfoamt", 25 }, { "lfofreqa", 0 }, { "lfofreqb", 0 }, { "lfomode", 1 },
                                                    { "lforate", 0.2f }, { "aa", 900 }, { "ar", 2500 }, { "fa", 1200 }, { "envamt", 20 } }, N },
                { "Nappe chaude", { { "level", -3.4f }, { "ashape", 30 }, { "apw", 35 }, { "bshape", 55 }, { "cutoff", 1100 }, { "envamt", 25 }, { "fa", 1500 }, { "fr", 2500 }, { "aa", 1200 }, { "ar", 2800 },
                                    { "lfoamt", 12 }, { "lfofreqa", 0 }, { "lfofreqb", 0 }, { "lfopw", 1 }, { "lforate", 0.5f }, { "vintage", 35 } }, N },
                { "Cordes passe-haut", { { "level", -6.9f }, { "fmode", 85 }, { "cutoff", 500 }, { "reso", 25 }, { "aa", 600 }, { "ar", 1800 }, { "envamt", 10 }, { "lfoamt", 8 }, { "lforate", 5.5f } }, S },
                { "Accords bande", { { "level", -2.3f }, { "bandpass", 1 }, { "cutoff", 1200 }, { "reso", 40 }, { "envamt", 35 }, { "fd", 500 }, { "fs", 20 }, { "ad", 1500 }, { "as", 50 } }, K },
                { "Cloches X-Mod", { { "async", 0 }, { "bfreq", 19 }, { "xenv", 60 }, { "xfreqa", 1 }, { "mixb", 0 }, { "cutoff", 4000 }, { "envamt", 15 }, { "fs", 0 }, { "as", 0 },
                                     { "ad", 2000 }, { "ar", 2000 } }, K },
                { "Synchro balay\xC3\xA9" "e", { { "level", -4.4f }, { "async", 1 }, { "bfreq", -12 }, { "xenv", 70 }, { "xfreqa", 1 }, { "mixb", 0 }, { "cutoff", 2500 }, { "reso", 30 }, { "unison", 1 }, { "univoices", 2 } }, L },
                { "Lead unisson", { { "level", -12.6f }, { "unison", 1 }, { "univoices", 6 }, { "unidetune", 14 }, { "ashape", 0 }, { "bshape", 50 }, { "cutoff", 2200 }, { "envamt", 30 }, { "glide", 50 } }, L },
                { "Basse ronde", { { "level", -4.2f }, { "unison", 1 }, { "univoices", 2 }, { "afreq", -12 }, { "bfreq", -12 }, { "mixsub", 60 }, { "cutoff", 400 }, { "envamt", 50 }, { "fd", 300 }, { "fs", 10 },
                                   { "keytrack", 0 } }, B },
                { "Basse en anneau", { { "level", -1.8f }, { "unison", 1 }, { "univoices", 2 }, { "afreq", -12 }, { "xoscb", 40 }, { "xfreqa", 1 }, { "bfreq", -5 }, { "mixb", 0 }, { "cutoff", 900 }, { "envamt", 40 },
                                       { "fd", 250 }, { "fs", 0 } }, B },
                { "Pluck d'\xC3\xA9tat", { { "cutoff", 300 }, { "reso", 30 }, { "envamt", 70 }, { "fd", 220 }, { "fs", 0 }, { "ad", 500 }, { "as", 0 }, { "ar", 400 } }, PL },
                { "Mode qui tourne", { { "level", -8.9f }, { "lfoamt", 60 }, { "lfofreqa", 0 }, { "lfofreqb", 0 }, { "lfomode", 1 }, { "lforate", 1.2f }, { "reso", 60 }, { "cutoff", 900 }, { "aa", 300 }, { "ar", 1500 } }, FX },
                { "Vent filtr\xC3\xA9", { { "level", -0.9f }, { "mixa", 0 }, { "mixb", 0 }, { "mixnoise", 100 }, { "bandpass", 1 }, { "cutoff", 900 }, { "reso", 85 }, { "lfoamt", 40 }, { "lfofreqa", 0 },
                                          { "lfofreqb", 0 }, { "lfocut", 1 }, { "lforate", 0.15f }, { "aa", 1500 }, { "ar", 3000 } }, FX },
                { "Clavier \xC3\xA0 la largeur", { { "level", -14.4f }, { "ashape", 100 }, { "bshape", 100 }, { "lfoamt", 30 }, { "lfofreqa", 0 }, { "lfofreqb", 0 }, { "lfopw", 1 }, { "lforate", 0.8f },
                                                   { "cutoff", 2500 }, { "envamt", 20 }, { "ad", 1200 }, { "as", 60 } }, K },
            };
            HubSpec hub;
            hub.envs = 0; hub.lfos = 0; hub.slots = 0; hub.arp = true;
            addModHub (m, hub);
            return m;
        }();
        return d;
    }
}

struct Ob6Shared { double lfo = 0.0, bendSemis = 0.0, pressure = 0.0; };

class Ob6Voice
{
public:
    void prepare (double s) { sr = s; fenv.prepare (s); aenv.prepare (s); reset(); }
    void reset() noexcept { aSaw.reset(); aPul.reset(); sub.reset(); bTri.reset(); bSaw.reset(); bPul.reset(); filter.reset(); fenv.reset(); aenv.reset(); dc.reset(); active = false; }
    void setSpread (int index, int count) noexcept { spread = count > 1 ? (2.0 * index / (count - 1) - 1.0) : 0.0; }
    void seed (std::uint32_t s) noexcept { rng = s; }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; active = true;
        rng = rng * 1664525u + 1013904223u;
        noteDrift = (double) (rng >> 8) / 8388608.0 - 1.0;
    }
    void release() noexcept { fenv.gate (false); aenv.gate (false); }
    bool isActive() const noexcept { return active; }

    void render (float* L, float* R, int n, const float* p, const Ob6Shared& sh) noexcept
    {
        using namespace ob6;
        if (pending)
        {
            pending = false;
            if (p[glide] > 0.5f && from >= 0) pitch.jump ((double) from); else pitch.jump ((double) target);
            pitch.to ((double) target);
            if (! legatoPending || ! aenv.isActive()) { fenv.gate (true); aenv.gate (true); }
        }
        fenv.set (p[fA], p[fD], p[fS] / 100.0, p[fR]);
        aenv.set (p[aA], p[aD], p[aS] / 100.0, p[aR]);
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto vint = p[vintage] / 100.0;
        // instabilité : désaccord propre à la note et lente dérive de chaque voix
        driftPhase += (0.07 + 0.05 * std::abs (noteDrift)) * n / sr;
        if (driftPhase >= 1.0) driftPhase -= 1.0;
        const auto wander = fastSin2Pi (driftPhase) * vint * 0.06;
        const auto detune = spread * p[uniDetune] / 100.0 + noteDrift * vint * 0.12 + wander;
        const auto gain = dbToGain (p[level]) * velocityGain (vel, p[velAmp] / 100.0) * 1.4;
        const auto res = p[reso] / 100.0 * 0.96;
        const auto envDepth = (p[envAmt] / 100.0) * (1.0 + (p[velEnv] / 100.0) * (vel - 0.6));
        static constexpr double kt[3] { 0.0, 0.5, 1.0 };
        const auto ktAmt = kt[std::clamp ((int) p[keytrack], 0, 2)];
        const bool syncOn = p[aSync] > 0.5f, low = p[bLow] > 0.5f, bKeyOn = p[bKey] > 0.5f, bp = p[bandpass] > 0.5f;
        const auto lfo = sh.lfo;
        const auto lA = p[lfoFreqA] > 0.5f ? lfo * 1.5 : 0.0, lB = p[lfoFreqB] > 0.5f ? lfo * 1.5 : 0.0;
        const auto lPw = p[lfoPw] > 0.5f ? lfo * 0.35 : 0.0, lCut = p[lfoCut] > 0.5f ? lfo * 3.0 : 0.0;
        const auto lMode = p[lfoMode] > 0.5f ? lfo * 0.5 : 0.0;
        const auto lAmp = p[lfoAmp] > 0.5f ? (1.0 + 0.45 * lfo) / 1.45 : 1.0; // trémolo
        const auto xE = p[xEnv] / 100.0, xB = p[xOscB] / 100.0;
        const bool toFreq = p[xFreqA] > 0.5f, toShape = p[xShapeA] > 0.5f, toPw = p[xPwA] > 0.5f, toCut = p[xCut] > 0.5f, toMode = p[xMode] > 0.5f;
        const auto ma = p[mixA] / 100.0, ms = p[mixSub] / 100.0, mb = p[mixB] / 100.0, mn = p[mixNoise] / 100.0;
        const auto cutDrift = 1.0 + noteDrift * vint * 0.05 + wander * 0.5;
        const auto atOct = (p[atFilter] / 100.0) * 3.0 * sh.pressure;
        const auto panL = 1.0 - 0.35 * std::max (0.0, spread), panR = 1.0 + 0.35 * std::min (0.0, spread);

        for (int i = 0; i < n; ++i)
        {
            const auto fe = fenv.next();
            const auto ae = aenv.next();
            const auto note = pitch.next (glideCoef) + p[tune] + detune + sh.bendSemis;

            // oscillateur B : triangle → scie → impulsion (maître de la synchro)
            const auto bNote = (bKeyOn ? note : 60.0) + std::round (p[bFreq]) + p[bFine] / 100.0 - (low ? 48.0 : 0.0) + lB;
            const auto dtb = std::clamp (noteHz (bNote) / sr, 1.0e-7, 0.45);
            const auto bpw = std::clamp (p[bPw] / 100.0 + lPw, 0.05, 0.95);
            double wrap = -1.0;
            const auto sB = bSaw.next (Osc::saw, dtb, bpw, -1.0, &wrap);
            const auto shB = p[bShape] / 100.0;
            double b;
            if (shB < 0.5) { const auto t = bTri.next (Osc::triangle, dtb, bpw); b = t + (sB - t) * (shB * 2.0); bPul.next (Osc::pulse, dtb, bpw); }
            else { const auto q = bPul.next (Osc::pulse, dtb, bpw); b = sB + (q - sB) * ((shB - 0.5) * 2.0); bTri.next (Osc::triangle, dtb, bpw); }

            // X-Mod : enveloppe du filtre et oscillateur B, bipolaires
            const auto xm = xE * fe + xB * b;
            const auto aNote = note + std::round (p[aFreq]) + (toFreq ? xm * 24.0 : 0.0) + lA;
            const auto dta = std::clamp (noteHz (aNote) / sr, 1.0e-7, 0.45);
            const auto apw = std::clamp (p[aPw] / 100.0 + (toPw ? xm * 0.4 : 0.0) + lPw, 0.05, 0.95);
            const auto sAt = syncOn ? wrap : -1.0;
            const auto shA = std::clamp (p[aShape] / 100.0 + (toShape ? xm : 0.0), 0.0, 1.0);
            const auto saw = aSaw.next (Osc::saw, dta, apw, sAt);
            const auto pul = aPul.next (Osc::pulse, dta, apw, sAt);
            const auto a = saw + (pul - saw) * shA;
            const auto so = sub.next (Osc::pulse, dta * 0.5, 0.5);

            rng = rng * 1664525u + 1013904223u;
            const auto noise = (double) (rng >> 8) / 8388608.0 - 1.0;
            const auto x = (a * ma + so * ms + (low ? 0.0 : b * mb) + noise * mn) * 0.38;

            const auto octaves = envDepth * 7.0 * fe + (toCut ? xm * 4.0 : 0.0) + lCut + atOct + ktAmt * (note - 60.0) / 12.0;
            const auto fc = std::clamp ((double) p[cutoff] * cutDrift * fastExp2 (octaves), 16.0, 0.45 * sr);
            const auto o = filter.process (x, fc, res, sr);
            double y;
            if (bp) y = o.band * 1.3;
            else
            {
                // mode continu : passe-bas → réjection (bas + haut) → passe-haut
                const auto mm = std::clamp (p[fMode] / 100.0 + (toMode ? xm * 0.5 : 0.0) + lMode, 0.0, 1.0);
                y = (o.low * (1.0 - mm) + o.high * mm) * (1.0 + (1.0 - std::abs (2.0 * mm - 1.0)));
            }
            const auto v = dc.process (y) * ae * gain * lAmp;
            L[i] += (float) (v * panL);
            R[i] += (float) (v * panR);
        }
        if (! aenv.isActive()) active = false;
    }

private:
    double sr = 48000.0, spread = 0.0, noteDrift = 0.0, driftPhase = 0.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false;
    std::uint32_t rng = 0x0B6u;
    Osc aSaw, aPul, sub, bTri, bSaw, bPul;
    StateFilter filter;
    Adsr fenv, aenv;
    Glide pitch;
    DcBlocker dc;
};

class Ob6Synth final : public Instrument
{
public:
    Ob6Synth() { for (int i = 0; i < maxVoices; ++i) voices[(size_t) i].seed (0x6B1Du * (std::uint32_t) (i + 1)); }

    const ModuleDesc& desc() const noexcept override { return ob6::desc(); }
    void prepare (double s, int) override { sr = s; for (auto& v : voices) v.prepare (s); pbRange = ob6::desc().indexOf ("pb_range"); reset(); }
    void reset() noexcept override { for (auto& v : voices) v.reset(); alloc.clear(); stack.clear(); }

    void noteOn (int note, float velocity) noexcept override
    {
        const auto prev = lastNote;
        lastNote = note;
        if (unisonOn)
        {
            const bool wasHeld = ! stack.empty();
            stack.push (note);
            for (int i = 0; i < uniCount; ++i) { voices[(size_t) i].setSpread (i, uniCount); voices[(size_t) i].start (note, velocity, wasHeld, prev); }
            return;
        }
        const auto i = alloc.noteOn (note, voiceCount, [this] (int k) { return voices[(size_t) k].isActive(); });
        voices[(size_t) i].setSpread (0, 1);
        voices[(size_t) i].start (note, velocity, false, prev);
    }

    void noteOff (int note) noexcept override
    {
        if (unisonOn)
        {
            stack.remove (note);
            for (int i = 0; i < uniCount; ++i)
                if (stack.empty()) voices[(size_t) i].release();
                else voices[(size_t) i].start (stack.top(), -1.0f, true, note);
            return;
        }
        const auto i = alloc.noteOff (note);
        if (i >= 0) voices[(size_t) i].release();
    }

    void allNotesOff() noexcept override { for (auto& v : voices) v.release(); alloc.clear(); stack.clear(); }
    void setTempo (double b) noexcept override { bpm = b; }
    void setExpression (float b, float w, float pr) noexcept override { bendT = b; wheelT = w; pressT = pr; }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        using namespace ob6;
        const bool uni = p[unison] > 0.5f;
        if (uni != unisonOn) { allNotesOff(); unisonOn = uni; }
        voiceCount = std::clamp ((int) p[ob6::voices], 1, maxVoices);
        uniCount = std::clamp ((int) p[uniVoices], 2, maxVoices);
        const auto k = std::min (1.0, (double) n / (0.004 * sr));
        bend += (bendT - bend) * k; wheel += (wheelT - wheel) * k; press += (pressT - press) * k;

        // LFO commun : quantité initiale + molette + pression
        const auto rate = lfoSyncHz ((int) p[lfoSync], p[lfoRate], bpm);
        const auto prev = lfoPhase;
        lfoPhase += rate * (double) n / sr;
        if (lfoPhase >= 1.0) lfoPhase -= std::floor (lfoPhase);
        if (lfoPhase < prev) { rng = rng * 1664525u + 1013904223u; held = (double) (rng >> 8) / 8388608.0 - 1.0; }
        double l;
        switch ((int) p[lfoShape])
        {
            case 1: l = 2.0 * lfoPhase - 1.0; break;
            case 2: l = 1.0 - 2.0 * lfoPhase; break;
            case 3: l = lfoPhase < 0.5 ? 1.0 : -1.0; break;
            case 4: l = held; break;
            default: l = fastSin2Pi (lfoPhase); break;
        }
        Ob6Shared sh;
        sh.lfo = l * std::clamp (p[lfoAmt] / 100.0 + wheel + press * p[atLfo] / 100.0, 0.0, 1.0);
        sh.bendSemis = pbRange >= 0 ? bend * p[pbRange] : bend * 2.0;
        sh.pressure = press;

        const auto active = unisonOn ? uniCount : voiceCount;
        for (int i = 0; i < maxVoices; ++i)
        {
            auto& v = voices[(size_t) i];
            if (! v.isActive()) continue;
            if (i >= active) v.release();
            v.render (L, R, n, p, sh);
        }
    }

    bool isActive() const noexcept override { for (const auto& v : voices) if (v.isActive()) return true; return false; }

private:
    static constexpr int maxVoices = 8;
    std::array<Ob6Voice, maxVoices> voices;
    VoiceAllocator<maxVoices> alloc;
    NoteStack stack;
    double sr = 48000.0, bpm = 120.0, lfoPhase = 0.0, held = 0.0;
    double bend = 0.0, wheel = 0.0, press = 0.0, bendT = 0.0, wheelT = 0.0, pressT = 0.0;
    std::uint32_t rng = 0x0B60u;
    bool unisonOn = false;
    int voiceCount = 6, uniCount = 4, lastNote = -1, pbRange = -1;
};

} // namespace grv::instruments
