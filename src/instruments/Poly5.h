/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Poly analogique » — principe des polyphoniques analogiques à 5 voix de la fin des années 70
    (architecture reprise, ni nom, ni panneau, ni son d'origine) :
      - deux oscillateurs par voix. A : dent de scie et impulsion (cumulables), largeur,
        synchronisable sur B. B : dent de scie, triangle et impulsion (cumulables), fin, largeur,
        mode basse fréquence (B devient une source de modulation lente), suivi du clavier débrayable ;
      - mélangeur A, B, bruit ;
      - POLY-MOD : l'enveloppe de filtre et l'oscillateur B (par voix) modulent la fréquence de A,
        la largeur de A et le filtre (cloches, synchros balayées) ;
      - LFO à trois formes cumulables (scie, triangle, carré), synchronisable au tempo ;
      - WHEEL-MOD : mélange LFO ↔ bruit envoyé, selon la molette (et une quantité initiale), vers la
        fréquence de A, de B, les largeurs de A et de B et le filtre ;
      - filtre en échelle 24 dB, deux caractères (révision ancienne, plus mordante et sans perte de
        grave ; révision récente, plus douce), enveloppe, suivi du clavier ;
      - deux enveloppes ADSR ; interrupteur « relâchement » ; vélocité et pression vers le filtre
        et l'ampli, pression vers la quantité de Wheel-Mod ;
      - portamento polyphonique, unisson, dérive « vintage » par voix, pitch bend. 8 voix.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace poly5
{
    enum Slot
    {
        voices = 0, glide, unison, unisonDetune, vintage, velAmp, velFilter, level, tune,
        aSemi, aPw, aSync, bSemi, bFine, bPw, bLow, bKey,
        mixA, mixB, mixNoise,
        pmEnv, pmOscB, pmFreqA, pmPwA, pmFilter,
        cutoff, reso, envAmt, keytrack,
        fA, fD, fS, fR, aA, aD, aS, aR,
        lfoRate, lfoSync,
        // v0.10 : formes cumulables, Wheel-Mod, révision du filtre, relâchement, pression
        aSaw, aPulse, bSaw, bTri, bPulse,
        lSaw, lTri, lSquare,
        wmMix, wmInit, wmFreqA, wmFreqB, wmPwA, wmPwB, wmFilter,
        filterRev, releaseOn, atFilter, atWheel,
        numSlots
    };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            ModuleDesc m;
            m.type = "poly5";
            m.name = "Poly Five";
            m.family = "Polys";
            m.principle = "Polyphonique analogique \xC3\xA0 2 oscillateurs, Poly-Mod, Wheel-Mod, filtre en \xC3\xA9" "chelle 24 dB";
            m.params = {
                I ("voices", "Voix", "", 1, 8, 8),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                C ("unison", "Unisson", onOff),
                P ("unidetune", "\xC3\x89" "cart unisson", "ct", 0, 50, 12),
                P ("vintage", "Vintage", "%", 0, 100, 25),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 30),
                P ("velfilter", "V\xC3\xA9l. filtre", "%", 0, 100, 20),
                P ("level", "Volume", "dB", -24, 6, -3),
                P ("tune", "Accord", "dt", -24, 24, 0),

                I ("asemi", "Fr\xC3\xA9quence A", "dt", -24, 24, 0),
                P ("apw", "Largeur A", "%", 5, 95, 50),
                C ("async", "Synchro", onOff),
                I ("bsemi", "Fr\xC3\xA9quence B", "dt", -24, 24, 0),
                P ("bfine", "Fin B", "ct", -50, 50, 6),
                P ("bpw", "Largeur B", "%", 5, 95, 50),
                C ("blow", "Basse fr\xC3\xA9q.", onOff),
                C ("bkey", "Clavier", onOff, 1),

                P ("mixa", "Osc. A", "%", 0, 100, 80),
                P ("mixb", "Osc. B", "%", 0, 100, 60),
                P ("mixnoise", "Bruit", "%", 0, 100, 0),

                P ("pmenv", "Env. filtre", "%", -100, 100, 0),
                P ("pmoscb", "Osc. B", "%", 0, 100, 0),
                C ("pmfreqa", "Fr\xC3\xA9q. A", onOff),
                C ("pmpwa", "Largeur A", onOff),
                C ("pmfilter", "Filtre", onOff),

                P ("cutoff", "Coupure", "Hz", 20, 18000, 1600, 0.3f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 15),
                P ("envamt", "Qt\xC3\xA9 env.", "%", -100, 100, 40),
                P ("keytrack", "Clavier", "%", 0, 100, 50),

                P ("fa", "Attaque", "ms", 0.5f, 10000, 5, 0.3f),
                P ("fd", "D\xC3\xA9" "clin", "ms", 5, 10000, 800, 0.3f),
                P ("fs", "Maintien", "%", 0, 100, 30),
                P ("fr", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 600, 0.3f),
                P ("aa", "Attaque", "ms", 0.5f, 10000, 3, 0.3f),
                P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 1200, 0.3f),
                P ("as", "Maintien", "%", 0, 100, 75),
                P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 700, 0.3f),

                P ("lforate", "Fr\xC3\xA9quence", "Hz", 0.05f, 25, 4.5f, 0.4f),
                C ("lfosync", "Synchro", lfoSyncChoices()),

                C ("asaw", "Scie", onOff, 1), C ("apulse", "Impulsion", onOff),
                C ("bsaw", "Scie", onOff, 1), C ("btri", "Triangle", onOff), C ("bpulse", "Impulsion", onOff),
                C ("lsaw", "Scie", onOff), C ("ltri", "Triangle", onOff, 1), C ("lsquare", "Carr\xC3\xA9", onOff),
                P ("wmmix", "Source LFO \xE2\x86\x94 bruit", "%", 0, 100, 0),
                P ("wminit", "Quantit\xC3\xA9 initiale", "%", 0, 100, 0),
                C ("wmfreqa", "Fr\xC3\xA9q. A", onOff, 1), C ("wmfreqb", "Fr\xC3\xA9q. B", onOff, 1),
                C ("wmpwa", "Largeur A", onOff), C ("wmpwb", "Largeur B", onOff), C ("wmfilter", "Filtre", onOff),
                C ("filterrev", "R\xC3\xA9vision du filtre", { "Ancienne (mordante)", "R\xC3\xA9" "cente (douce)" }, 1),
                C ("release", "Rel\xC3\xA2" "chement", onOff, 1),
                P ("atfilter", "Pression \xE2\x86\x92 filtre", "%", 0, 100, 0),
                P ("atwheel", "Pression \xE2\x86\x92 Wheel-Mod", "%", 0, 100, 0),
            };
            m.sections = {
                { "global", "Global", "global", { "voices", "glide", "unison", "unidetune", "vintage", "tune", "level", "release" }, "global" },
                { "osca", "Oscillateur A", "osc", { "asemi", "asaw", "apulse", "apw", "async", "mixa" }, "osc" },
                { "oscb", "Oscillateur B", "osc", { "bsemi", "bfine", "bsaw", "btri", "bpulse", "bpw", "blow", "bkey" }, "osc" },
                { "mix", "M\xC3\xA9langeur", "mix", { "mixa", "mixb", "mixnoise" }, "osc" },
                { "filter", "Filtre", "filter", { "cutoff", "reso", "envamt", "keytrack", "filterrev", "velfilter", "atfilter" }, "filter" },
                { "fenv", "Enveloppe filtre", "env", { "fa", "fd", "fs", "fr", "envamt" }, "env" },
                { "aenv", "Enveloppe ampli", "env", { "aa", "ad", "as", "ar", "velamp", "level" }, "amp" },
                { "polymod", "Poly-Mod", "mod", { "pmenv", "pmoscb", "pmfreqa", "pmpwa", "pmfilter" }, "mod" },
                { "lfo", "LFO", "lfo", { "lfosync", "lforate", "lsaw", "ltri", "lsquare" }, "mod" },
                { "wheelmod", "Wheel-Mod", "mod", { "wmmix", "wminit", "wmfreqa", "wmfreqb", "wmpwa", "wmpwb", "wmfilter", "atwheel" }, "mod" },
            };
            m.presets = {
                { "Cuivres", { { "bfine", 8 }, { "cutoff", 900 }, { "envamt", 55 }, { "fa", 60 }, { "fd", 600 }, { "fs", 40 }, { "aa", 30 } } },
                { "Nappe chaude", { { "asaw", 1 }, { "apulse", 1 }, { "apw", 35 }, { "cutoff", 1200 }, { "aa", 900 }, { "ar", 2500 }, { "fa", 1500 },
                                    { "wminit", 20 }, { "wmfreqa", 0 }, { "wmfreqb", 0 }, { "wmpwa", 1 }, { "lforate", 0.6f } } },
                { "Cloches poly-mod", { { "async", 1 }, { "bsemi", 19 }, { "pmenv", 60 }, { "pmfreqa", 1 }, { "envamt", 20 }, { "fs", 0 }, { "as", 0 }, { "ad", 1800 }, { "ar", 1800 } } },
                { "Synchro hurlante", { { "async", 1 }, { "bsemi", -12 }, { "pmenv", 70 }, { "pmfreqa", 1 }, { "mixb", 0 }, { "cutoff", 3000 }, { "reso", 30 } } },
                { "Basse unisson", { { "unison", 1 }, { "unidetune", 15 }, { "asemi", -12 }, { "cutoff", 600 }, { "envamt", 50 }, { "fd", 300 }, { "fs", 10 }, { "filterrev", 0 } } },
                { "Vibrato \xC3\xA0 la molette", { { "wmfreqa", 1 }, { "wmfreqb", 1 }, { "lforate", 5.5f } } },
            };
            HubSpec hub;
            hub.envs = 0; hub.lfos = 0; hub.slots = 0; hub.arp = true; // arpège ajouté (les poly récents de la même famille en ont un)
            addModHub (m, hub);
            addFactoryBank (m); // banque d'usine : 50 sons par catégories (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

/** Expression et LFO partagés par les voix (calculés une fois par bloc). */
struct Poly5Shared
{
    double lfo = 0.0, wheelMod = 0.0, noise = 0.0, bendSemis = 0.0, pressure = 0.0;
};

class Poly5Voice
{
public:
    void prepare (double s) { sr = s; fenv.prepare (s); aenv.prepare (s); reset(); }
    void reset() noexcept { oa.reset(); oa2.reset(); ob.reset(); ob2.reset(); ob3.reset(); filter.reset(); fenv.reset(); aenv.reset(); dc.reset(); active = false; }
    void setSpread (int index, int count) noexcept { spread = count > 1 ? (2.0 * index / (count - 1) - 1.0) : 0.0; }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note;
        from = prevNote;
        legatoPending = legatoStart && ! (pending && ! legatoPending); // un départ non lié encore en attente (accord dans le même bloc) le reste
        pending = true;
        active = true;
        // dérive propre à chaque note (désaccord, coupure)
        rngState = rngState * 1664525u + 1013904223u;
        noteDrift = (double) (rngState >> 8) / 8388608.0 - 1.0;
    }
    void release() noexcept { fenv.gate (false); aenv.gate (false); }
    bool isActive() const noexcept { return active; }

    void render (float* L, float* R, int n, const float* p, const Poly5Shared& sh) noexcept
    {
        using namespace poly5;
        if (pending)
        {
            pending = false;
            if (p[glide] > 0.5f && from >= 0) pitch.jump ((double) from); else pitch.jump ((double) target);
            pitch.to ((double) target);
            if (! legatoPending || ! aenv.isActive()) { fenv.gate (true); aenv.gate (true); }
        }
        // interrupteur « relâchement » : coupé, la note s'arrête vite au relâchement de la touche
        const bool rel = p[releaseOn] > 0.5f;
        fenv.set (p[fA], p[fD], p[fS] / 100.0, rel ? p[fR] : 8.0);
        aenv.set (p[aA], p[aD], p[aS] / 100.0, rel ? p[aR] : 8.0);
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto vint = p[vintage] / 100.0;
        const auto detune = spread * p[unisonDetune] / 100.0 + noteDrift * vint * 0.08;
        const auto gain = dbToGain (p[level]) * velocityGain (vel, p[velAmp] / 100.0);
        const auto res = p[reso] / 100.0;
        const bool aSawOn = p[aSaw] > 0.5f, aPulOn = p[aPulse] > 0.5f;
        const bool bSawOn = p[bSaw] > 0.5f, bTriOn = p[bTri] > 0.5f, bPulOn = p[bPulse] > 0.5f;
        const auto syncOn = p[aSync] > 0.5f, low = p[bLow] > 0.5f, bKeyOn = p[bKey] > 0.5f;
        const auto pmE = p[pmEnv] / 100.0, pmB = p[pmOscB] / 100.0;
        const auto toFreq = p[pmFreqA] > 0.5f, toPw = p[pmPwA] > 0.5f, toFilter = p[pmFilter] > 0.5f;
        const auto ma = p[mixA] / 100.0, mb = p[mixB] / 100.0, mnz = p[mixNoise] / 100.0;
        const auto velOct = (p[velFilter] / 100.0) * 2.0 * (vel - 0.5) + (p[atFilter] / 100.0) * 3.0 * sh.pressure;
        const auto cutoffDrift = 1.0 + noteDrift * vint * 0.06;
        const auto panL = 1.0 - 0.35 * std::max (0.0, spread), panR = 1.0 + 0.35 * std::min (0.0, spread);
        // Wheel-Mod : une source (LFO ↔ bruit) vers des destinations séparées
        const auto wm = sh.wheelMod;
        const auto wmA = p[wmFreqA] > 0.5f ? wm * 2.0 : 0.0, wmB = p[wmFreqB] > 0.5f ? wm * 2.0 : 0.0;
        const auto wmPwAmtA = p[wmPwA] > 0.5f ? wm * 0.4 : 0.0, wmPwAmtB = p[wmPwB] > 0.5f ? wm * 0.4 : 0.0;
        const auto wmOct = p[wmFilter] > 0.5f ? wm * 3.0 : 0.0;
        // révision du filtre : l'ancienne entre plus fort et garde le grave à forte résonance
        const bool oldRev = p[filterRev] < 0.5f;
        const auto fDrive = oldRev ? 1.7 : 1.0;
        const auto fIn = oldRev ? 0.3 : 0.35;

        for (int i = 0; i < n; ++i)
        {
            const auto fe = fenv.next();
            const auto ae = aenv.next();
            const auto note = pitch.next (glideCoef) + p[tune] + detune + sh.bendSemis;

            // Oscillateur B (maître de la synchro) : formes cumulables
            const auto bNote = (bKeyOn ? note : 60.0) + std::round (p[bSemi]) + p[bFine] / 100.0 - (low ? 48.0 : 0.0) + wmB;
            const auto dtb = noteHz (bNote) / sr;
            double wrap = -1.0;
            const auto bpw = std::clamp (p[bPw] / 100.0 + wmPwAmtB, 0.05, 0.95);
            double b = 0.0;
            int bCount = 0;
            // la première forme active fournit l'instant de bouclage (synchro)
            if (bSawOn) { b += ob.next (Osc::saw, dtb, bpw, -1.0, &wrap); ++bCount; }
            if (bTriOn) { b += ob2.next (Osc::triangle, dtb, bpw, -1.0, bCount == 0 ? &wrap : nullptr); ++bCount; }
            if (bPulOn) { b += ob3.next (Osc::pulse, dtb, bpw, -1.0, bCount == 0 ? &wrap : nullptr); ++bCount; }
            if (bCount == 0) ob.next (Osc::saw, dtb, bpw, -1.0, &wrap); // B muet : il cadence encore la synchro
            if (bCount > 1) b *= 0.7;

            // Poly-Mod (par voix)
            const auto pm = pmE * fe + pmB * b;
            const auto aNote = note + std::round (p[aSemi]) + (toFreq ? pm * 24.0 : 0.0) + wmA;
            const auto dta = noteHz (aNote) / sr;
            const auto apw = std::clamp (p[aPw] / 100.0 + (toPw ? pm * 0.45 : 0.0) + wmPwAmtA, 0.05, 0.95);
            const auto sAt = syncOn ? wrap : -1.0;
            double a = 0.0;
            if (aSawOn) a += oa.next (Osc::saw, dta, apw, sAt);
            if (aPulOn) a += oa2.next (Osc::pulse, dta, apw, sAt);
            if (aSawOn && aPulOn) a *= 0.7;

            const auto x = a * ma + (low ? 0.0 : b * mb) + noise.next() * mnz;
            const auto octaves = (p[envAmt] / 100.0) * 7.0 * fe + (toFilter ? pm * 4.0 : 0.0) + velOct + wmOct
                               + (p[keytrack] / 100.0) * (note - 60.0) / 12.0;
            const auto fc = std::clamp ((double) p[cutoff] * cutoffDrift * fastExp2 (octaves), 16.0, 0.45 * sr);
            auto f = filter.process (x * fIn, fc, res, sr, Ladder::lp24, fDrive);
            if (oldRev) f *= 1.0 + 0.5 * res; // moins de perte de grave à forte résonance
            const auto y = dc.process (f) * ae * gain;
            L[i] += (float) (y * panL);
            R[i] += (float) (y * panR);
        }
        if (! aenv.isActive()) active = false;
    }

private:
    double sr = 48000.0, spread = 0.0, noteDrift = 0.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false;
    std::uint32_t rngState = 0x1234u;
    Osc oa, oa2, ob, ob2, ob3;
    Ladder filter;
    Adsr fenv, aenv;
    Glide pitch;
    WhiteNoise noise { 0xB0B5u };
    DcBlocker dc;
public:
    void seed (std::uint32_t s) noexcept { rngState = s; }
};

class Poly5 final : public Instrument
{
public:
    Poly5() { for (int i = 0; i < maxVoices; ++i) voices[(size_t) i].seed (0x9E37u * (std::uint32_t) (i + 1)); }

    const ModuleDesc& desc() const noexcept override { return poly5::desc(); }
    void prepare (double s, int) override { sr = s; for (auto& v : voices) v.prepare (s); pbRange = poly5::desc().indexOf ("pb_range"); reset(); }
    void reset() noexcept override { for (auto& v : voices) v.reset(); alloc.clear(); stack.clear(); }

    void noteOn (int note, float velocity) noexcept override
    {
        const auto prev = lastNote;
        lastNote = note;
        if (unisonOn)
        {
            const bool wasHeld = ! stack.empty();
            stack.push (note);
            for (int i = 0; i < voiceCount; ++i) { voices[(size_t) i].setSpread (i, voiceCount); voices[(size_t) i].start (note, velocity, wasHeld, prev); }
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
            for (int i = 0; i < voiceCount; ++i)
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
        using namespace poly5;
        const bool uni = p[unison] > 0.5f;
        const auto count = std::clamp ((int) p[poly5::voices], 1, maxVoices);
        if (uni != unisonOn) { allNotesOff(); unisonOn = uni; }
        voiceCount = count;
        // expression lissée (pas de saut audible)
        const auto k = std::min (1.0, (double) n / (0.004 * sr));
        bend += (bendT - bend) * k; wheel += (wheelT - wheel) * k; press += (pressT - press) * k;

        // LFO : formes cumulables (la moyenne des formes allumées)
        const auto rate = lfoSyncHz ((int) p[lfoSync], p[lfoRate], bpm);
        lfoPhase += rate * (double) n / sr;
        if (lfoPhase >= 1.0) lfoPhase -= std::floor (lfoPhase);
        double l = 0.0; int shapes = 0;
        if (p[lSaw] > 0.5f) { l += 2.0 * lfoPhase - 1.0; ++shapes; }
        if (p[lTri] > 0.5f) { l += 1.0 - 4.0 * std::abs (lfoPhase - 0.5); ++shapes; }
        if (p[lSquare] > 0.5f) { l += lfoPhase < 0.5 ? 1.0 : -1.0; ++shapes; }
        if (shapes > 1) l /= (double) shapes;
        // bruit filtré pour la source de Wheel-Mod
        noiseLp += (noise.next() - noiseLp) * 0.05;

        Poly5Shared sh;
        sh.lfo = l;
        const auto mix = p[wmMix] / 100.0;
        const auto amount = std::clamp (p[wmInit] / 100.0 + wheel + press * p[atWheel] / 100.0, 0.0, 1.0);
        sh.wheelMod = (l * (1.0 - mix) + noiseLp * 3.0 * mix) * amount;
        sh.bendSemis = pbRange >= 0 ? bend * p[pbRange] : bend * 2.0;
        sh.pressure = press;

        for (int i = 0; i < maxVoices; ++i)
        {
            auto& v = voices[(size_t) i];
            if (! v.isActive()) continue;
            if (i >= voiceCount) v.release();
            v.render (L, R, n, p, sh);
        }
    }

    bool isActive() const noexcept override { for (const auto& v : voices) if (v.isActive()) return true; return false; }

private:
    static constexpr int maxVoices = 8;
    std::array<Poly5Voice, maxVoices> voices;
    VoiceAllocator<maxVoices> alloc;
    NoteStack stack;
    WhiteNoise noise { 0x57EEu };
    double sr = 48000.0, bpm = 120.0, lfoPhase = 0.0, noiseLp = 0.0;
    double bend = 0.0, wheel = 0.0, press = 0.0, bendT = 0.0, wheelT = 0.0, pressT = 0.0;
    bool unisonOn = false;
    int voiceCount = maxVoices, lastNote = -1, pbRange = -1;
};

} // namespace grv::instruments
