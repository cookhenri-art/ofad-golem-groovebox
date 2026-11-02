/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Poly chorus » — principe des polyphoniques à oscillateurs numériquement contrôlés du début
    des années 80 (architecture reprise, ni nom, ni panneau, ni son d'origine) : un oscillateur par
    voix (scie et impulsion cumulables, modulation de largeur manuelle, par LFO ou par enveloppe,
    registre 16' / 8' / 4', sous-oscillateur, bruit), passe-haut à 4 positions, filtre résonant
    24 dB (enveloppe à polarité inversable, LFO, clavier), une seule enveloppe (filtre et ampli),
    ampli sur enveloppe ou porte, LFO à montée progressive déclenché automatiquement ou à la main
    (molette), levier de hauteur vers l'oscillateur et le filtre, MAINTIEN des notes, ARPÈGE, et le
    chorus stéréo à lignes à retard (boutons I et II, cumulables) qui fait le caractère de ces
    machines. 8 voix.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace polyj
{
    enum Slot
    {
        voices = 0, level, velAmp, tune, drift,
        range, sawOn, pulseOn, pw, pwmSrc, pwmDepth, subLevel, noiseLevel, lfoPitch,
        hpf, cutoff, reso, envAmt, envPolarity, lfoFilter, keytrack, velFilter,
        eA, eD, eS, eR, vcaMode,
        lfoRate, lfoDelay, chorusMode,
        lfoSync, // v0.9 : synchro au tempo
        lfoTrig, bendVcf, // v0.10 (levier → oscillateur : pb_range du hub)
        numSlots
    };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            ModuleDesc m;
            m.type = "polyj";
            m.name = "Chorus 80";
            m.family = "Polys";
            m.principle = "Polyphonique \xC3\xA0 oscillateur num\xC3\xA9riquement contr\xC3\xB4l\xC3\xA9, filtre 24 dB, arp\xC3\xA8ge, chorus st\xC3\xA9r\xC3\xA9o \xC3\xA0 lignes \xC3\xA0 retard";
            m.params = {
                I ("voices", "Voix", "", 1, 8, 8),
                P ("level", "Niveau VCA", "dB", -24, 6, -8),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 25),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("drift", "D\xC3\xA9rive", "%", 0, 100, 5),

                C ("range", "Registre", { "16'", "8'", "4'" }, 1),
                C ("saw", "Scie", onOff, 1),
                C ("pulse", "Impulsion", onOff),
                P ("pw", "Largeur", "%", 0, 100, 0),
                C ("pwmsrc", "Mod. largeur", { "Manuelle", "LFO", "Enveloppe" }),
                P ("pwmdepth", "Profondeur", "%", 0, 100, 50),
                P ("sub", "Sous-osc.", "%", 0, 100, 0),
                P ("noise", "Bruit", "%", 0, 100, 0),
                P ("lfopitch", "LFO", "ct", 0, 100, 0),

                C ("hpf", "Passe-haut", { "0", "1", "2", "3" }, 0),
                P ("cutoff", "Fr\xC3\xA9quence", "Hz", 20, 18000, 2200, 0.3f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 10),
                P ("envamt", "Enveloppe", "%", 0, 100, 35),
                C ("envpol", "Polarit\xC3\xA9", { "Positive", "N\xC3\xA9gative" }),
                P ("lfofilter", "LFO", "%", 0, 100, 0),
                P ("keytrack", "Clavier", "%", 0, 100, 50),
                P ("velfilter", "V\xC3\xA9l. filtre", "%", 0, 100, 15),

                P ("ea", "A", "ms", 0.5f, 3000, 4, 0.3f),
                P ("ed", "D", "ms", 5, 10000, 900, 0.3f),
                P ("es", "S", "%", 0, 100, 60),
                P ("er", "R", "ms", 5, 10000, 600, 0.3f),
                C ("vca", "Ampli", { "Enveloppe", "Porte" }),

                P ("lforate", "Vitesse", "Hz", 0.1f, 20, 3, 0.4f),
                P ("lfodelay", "Mont\xC3\xA9" "e", "ms", 0, 3000, 0, 0.5f),
                C ("chorus", "Chorus", { "Arr\xC3\xAAt", "I", "II", "I + II" }, 1),
                C ("lfosync", "Synchro", lfoSyncChoices()),
                C ("lfotrig", "D\xC3\xA9" "clenchement LFO", { "Auto", "Manuel (molette)" }),
                P ("bendvcf", "Levier \xE2\x86\x92 filtre", "%", 0, 100, 0),
            };
            m.sections = {
                { "global", "Global", "global", { "voices", "tune", "drift", "velamp", "chorus", "level", "bendvcf" }, "global" },
                { "dco", "Oscillateur", "osc", { "range", "saw", "pulse", "pw", "pwmsrc", "pwmdepth", "sub", "noise" }, "osc" },
                { "filter", "Filtre", "filter", { "hpf", "cutoff", "reso", "envamt", "envpol", "lfofilter", "keytrack", "velfilter" }, "filter" },
                { "env", "Enveloppe", "env", { "ea", "ed", "es", "er", "envamt", "vca" }, "env" },
                { "amp", "Amplitude", "amp", { "vca", "velamp", "level", "chorus" }, "amp" },
                { "lfo", "LFO", "lfo", { "lfosync", "lforate", "lfodelay", "lfotrig", "lfopitch", "lfofilter", "pwmdepth" }, "mod" },
            };
            m.presets = {
                { "Cordes chorus", { { "saw", 1 }, { "pulse", 1 }, { "pwmsrc", 1 }, { "pwmdepth", 40 }, { "lforate", 0.8f }, { "ea", 400 }, { "er", 1200 }, { "chorus", 3 } } },
                { "Basse ronde", { { "range", 0 }, { "saw", 0 }, { "pulse", 1 }, { "pw", 30 }, { "sub", 70 }, { "cutoff", 500 }, { "envamt", 40 }, { "ed", 300 }, { "es", 20 }, { "chorus", 0 } } },
                { "Accords house", { { "saw", 1 }, { "cutoff", 1400 }, { "reso", 30 }, { "envamt", 45 }, { "ed", 250 }, { "es", 0 }, { "er", 250 }, { "chorus", 1 } } },
                { "Nappe lente", { { "pulse", 1 }, { "pwmsrc", 1 }, { "pwmdepth", 70 }, { "lforate", 0.4f }, { "ea", 1800 }, { "es", 90 }, { "er", 3000 }, { "chorus", 2 } } },
                { "Cloche douce", { { "saw", 0 }, { "pulse", 1 }, { "pw", 70 }, { "range", 2 }, { "cutoff", 3500 }, { "es", 0 }, { "ed", 1500 }, { "er", 1500 } } },
                { "Arp\xC3\xA8ge brillant", { { "saw", 1 }, { "cutoff", 2600 }, { "reso", 40 }, { "envamt", 30 }, { "ed", 180 }, { "es", 0 }, { "er", 160 }, { "chorus", 1 },
                                         { "arp_mode", 3 }, { "arp_oct", 2 }, { "arp_rate", 3 }, { "arp_gate", 50 } } },
            };
            HubSpec hub;
            hub.envs = 0; hub.lfos = 0; hub.slots = 0; hub.arp = true;
            addModHub (m, hub);
            addFactoryBank (m); // banque d'usine : 50 sons par catégories (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

class PolyJVoice
{
public:
    void prepare (double s) { sr = s; env.prepare (s); reset(); }
    void reset() noexcept { osc.reset(); oscP.reset(); sub.reset(); filter.reset(); hp.reset(); env.reset(); dc.reset(); active = false; gateOpen = false; gateAmp = 0.0; }
    void start (int note, float velocity) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        current = note;
        env.gate (true);
        gateOpen = true;
        active = true;
        rng = rng * 1664525u + 1013904223u;
        drift = (double) (rng >> 8) / 8388608.0 - 1.0;
    }
    void release() noexcept { env.gate (false); gateOpen = false; }
    bool isActive() const noexcept { return active; }
    void seed (std::uint32_t s) noexcept { rng = s; }

    void render (float* L, float* R, int n, const float* p, double lfo, double bend, double bendSemis) noexcept
    {
        using namespace polyj;
        env.set (p[eA], p[eD], p[eS] / 100.0, p[eR]);
        static constexpr double hpHz[4] { 10.0, 120.0, 300.0, 700.0 };
        const auto hpFc = hpHz[std::clamp ((int) p[hpf], 0, 3)];
        const auto oct = 12.0 * (std::round (p[range]) - 1.0);
        const auto saw = p[sawOn] > 0.5f, pulse = p[pulseOn] > 0.5f;
        const auto src = (int) p[pwmSrc];
        const auto depth = p[pwmDepth] / 100.0;
        const auto res = p[reso] / 100.0;
        const auto polarity = p[envPolarity] > 0.5f ? -1.0 : 1.0;
        const auto gateMode = p[vcaMode] > 0.5f;
        const auto gain = dbToGain (p[level] + 6.0f) * velocityGain (vel, p[velAmp] / 100.0); // +6 dB de réserve (projets v5)
        const auto velOct = (p[velFilter] / 100.0) * 2.0 * (vel - 0.5);
        // levier : hauteur de l'oscillateur et coupure du filtre
        const auto noteBase = (double) current + p[tune] + oct + drift * p[polyj::drift] / 100.0 * 0.1 + lfo * p[lfoPitch] / 100.0 + bend * bendSemis;
        const auto bendOct = bend * (p[bendVcf] / 100.0) * 2.0;
        const auto dt = noteHz (noteBase) / sr;
        const auto subDt = dt * 0.5;
        const auto ms = p[subLevel] / 100.0, mn = p[noiseLevel] / 100.0;
        const auto gateCoef = std::exp (-1.0 / (0.002 * sr));

        for (int i = 0; i < n; ++i)
        {
            const auto e = env.next();
            gateAmp = (gateOpen ? 1.0 : 0.0) + (gateAmp - (gateOpen ? 1.0 : 0.0)) * gateCoef;
            // Largeur d'impulsion : manuelle, LFO ou enveloppe (de 50 % à 95 %)
            double width = p[pw] / 100.0;
            if (src == 1) width = depth * (0.5 + 0.5 * lfo);
            else if (src == 2) width = depth * e;
            const auto pwv = 0.5 + 0.45 * std::clamp (width, 0.0, 1.0);

            double x = 0.0;
            if (saw) x += osc.next (Osc::saw, dt, 0.5);
            if (pulse) x += oscP.next (Osc::pulse, dt, pwv);
            x += sub.next (Osc::pulse, subDt, 0.5) * ms + noise.next() * mn;

            x = hp.highpass (x, hpFc, sr);
            const auto octaves = polarity * (p[envAmt] / 100.0) * 7.0 * e + lfo * (p[lfoFilter] / 100.0) * 3.0 + velOct + bendOct
                               + (p[keytrack] / 100.0) * (noteBase - 60.0) / 12.0;
            const auto fc = std::clamp ((double) p[cutoff] * fastExp2 (octaves), 16.0, 0.45 * sr);
            const auto y = filter.process (x * 0.35, fc, res, sr, Ladder::lp24, 1.0);
            const auto amp = gateMode ? gateAmp : e;
            const auto out = dc.process (y) * amp * gain;
            L[i] += (float) out;
            R[i] += (float) out;
        }
        if (! env.isActive() && gateAmp < 1.0e-4) active = false;
    }

private:
    double sr = 48000.0, drift = 0.0, gateAmp = 0.0;
    float vel = 1.0f;
    int current = 60;
    bool active = false, gateOpen = false;
    std::uint32_t rng = 1;
    Osc osc, oscP, sub;
    OnePole hp;
    Ladder filter;
    Adsr env;
    WhiteNoise noise { 0x51AB1u };
    DcBlocker dc;
};

class PolyChorus final : public Instrument
{
public:
    PolyChorus() { for (int i = 0; i < maxVoices; ++i) voices[(size_t) i].seed (0x5EEDu * (std::uint32_t) (i + 3)); }

    const ModuleDesc& desc() const noexcept override { return polyj::desc(); }
    void prepare (double s, int) override
    {
        sr = s; for (auto& v : voices) v.prepare (s); chorus.prepare (s);
        arpMode = polyj::desc().indexOf ("arp_mode"); arpOn = polyj::desc().indexOf ("arp_on"); arpHold = polyj::desc().indexOf ("arp_hold"); pbRange = polyj::desc().indexOf ("pb_range");
        reset();
    }
    void reset() noexcept override { for (auto& v : voices) v.reset(); alloc.clear(); chorus.reset(); lfoFade = 0.0; held.fill (false); latched.fill (false); physical = 0; }

    void noteOn (int note, float velocity) noexcept override
    {
        note = std::clamp (note, 0, 127);
        // maintien (sans arpège) : la première note jouée après avoir tout relâché libère l'accord retenu
        if (physical == 0 && hold) releaseLatched();
        ++physical;
        held[(size_t) note] = true;
        latched[(size_t) note] = false;
        bool any = false;
        for (const auto& v : voices) any = any || v.isActive();
        if (! any) lfoFade = 0.0; // le LFO à montée progressive repart quand on rejoue après un silence
        const auto i = alloc.noteOn (note, voiceCount, [this] (int k) { return voices[(size_t) k].isActive(); });
        voices[(size_t) i].start (note, velocity);
    }
    void noteOff (int note) noexcept override
    {
        note = std::clamp (note, 0, 127);
        if (held[(size_t) note]) { held[(size_t) note] = false; physical = std::max (0, physical - 1); }
        if (hold) { latched[(size_t) note] = true; return; }
        const auto i = alloc.noteOff (note);
        if (i >= 0) voices[(size_t) i].release();
    }
    void allNotesOff() noexcept override { for (auto& v : voices) v.release(); alloc.clear(); held.fill (false); latched.fill (false); physical = 0; }
    void setTempo (double b) noexcept override { bpm = b; }
    void setExpression (float b, float w, float) noexcept override { bendT = b; wheelT = w; }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        // maintien de l'instrument seulement quand l'arpège est arrêté (sinon c'est l'arpège qui maintient)
        const bool h = arpHold >= 0 && arpMode >= 0 && p[arpHold] > 0.5f && (p[arpMode] < 0.5f || (arpOn >= 0 && p[arpOn] < 0.5f));
        if (hold && ! h) releaseLatched();
        hold = h;
        for (int pos = 0; pos < n; pos += 64) renderChunk (L + pos, R + pos, std::min (64, n - pos), p);
    }

    void renderChunk (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace polyj;
        voiceCount = std::clamp ((int) p[polyj::voices], 1, maxVoices);
        const auto k = std::min (1.0, (double) n / (0.004 * sr));
        bend += (bendT - bend) * k; wheel += (wheelT - wheel) * k;
        // LFO triangle avec montée progressive ; déclenchement manuel : la molette l'ouvre
        const auto fadeStep = p[lfoDelay] < 1.0f ? 1.0 : (double) n / (p[lfoDelay] * 0.001 * sr);
        lfoFade = std::min (1.0, lfoFade + fadeStep);
        double l = 0.0;
        const auto lfoHz = lfoSyncHz ((int) p[lfoSync], p[lfoRate], bpm);
        for (int i = 0; i < n; ++i) l = lfo.next (1, lfoHz, sr);
        l *= p[lfoTrig] > 0.5f ? wheel : lfoFade;

        float bufL[64] {}, bufR[64] {};
        const auto m = std::min (n, 64);
        bool any = false;
        for (int i = 0; i < maxVoices; ++i)
        {
            auto& v = voices[(size_t) i];
            if (! v.isActive()) continue;
            if (i >= voiceCount) v.release();
            v.render (bufL, bufR, m, p, l, bend, pbRange >= 0 ? (double) p[pbRange] : 2.0);
            any = true;
        }
        const auto mode = (int) p[chorusMode];
        if (mode > 0)
        {
            // I : lent et profond ; II : plus rapide ; I+II : vibrato rapide et doux
            static constexpr double chorusRate[4] { 0.0, 0.5, 0.83, 9.0 };
            static constexpr double chorusDepth[4] { 0.0, 1.85, 1.85, 0.25 };
            chorus.process (bufL, bufR, m, chorusRate[mode], chorusDepth[mode], 3.5, 1.0, 7500.0);
            any = true; // la queue du chorus s'éteint d'elle-même
        }
        if (! any) return;
        for (int i = 0; i < m; ++i) { L[i] += bufL[i]; R[i] += bufR[i]; }
    }

    bool isActive() const noexcept override { for (const auto& v : voices) if (v.isActive()) return true; return false; }

private:
    void releaseLatched() noexcept
    {
        for (int nn = 0; nn < 128; ++nn)
            if (latched[(size_t) nn])
            {
                latched[(size_t) nn] = false;
                if (held[(size_t) nn]) continue;
                const auto i = alloc.noteOff (nn);
                if (i >= 0) voices[(size_t) i].release();
            }
    }

    static constexpr int maxVoices = 8;
    std::array<PolyJVoice, maxVoices> voices;
    VoiceAllocator<maxVoices> alloc;
    SimpleLfo lfo;
    BbdChorus chorus;
    std::array<bool, 128> held {}, latched {};
    double sr = 48000.0, lfoFade = 0.0, bpm = 120.0, bend = 0.0, wheel = 0.0, bendT = 0.0, wheelT = 0.0;
    int voiceCount = maxVoices, arpMode = -1, arpOn = -1, arpHold = -1, pbRange = -1, physical = 0;
    bool hold = false;
};

} // namespace grv::instruments
