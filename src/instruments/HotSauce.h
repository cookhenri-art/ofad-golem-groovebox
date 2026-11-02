/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Hot Sauce » — principe des synthés logiciels hybrides à moteurs multiples (architecture
    reprise ; ni nom, ni façade, ni son, ni table d'onde d'origine) :
      - deux moteurs en parallèle (A et B), chacun d'un type au choix :
          Analogique (deux oscillateurs à forme continue désaccordés),
          Table d'ondes (8 images calculées, balayées : sinus, triangle, scie, carré, voyelles,
            métal, impulsion ; pli de l'onde),
          Harmonique (additif, 16 partiels : pente et parité),
          Modal (excitation brève + frottement, 6 résonateurs : corde, barre, plaque ; tenue) ;
      - modulation du moteur B par le moteur A (FM par la phase) ;
      - moteur utilitaire : sous-oscillateur sinus et bruit coloré ;
      - deux filtres : d'état à fondu continu passe-bas → bande → passe-haut, et échelle 24 dB à
        saturation ; routage continu série → parallèle ; enveloppes du filtre et de l'ampli ;
      - 3 LFO, 2 enveloppes et 16 routages (modulation commune), arpège ; 8 voix.
    Modèle : tables de 2048 points par image et par octave (spectre limité, sans repliement),
    lecture interpolée ; partiels par récurrence (une multiplication par partiel) ; aucune latence.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

#include <array>
#include <cmath>
#include <vector>

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace hots
{
    constexpr int numFrames = 8, numMips = 9, tableSize = 2048;
    // moteur : type, octave, demi-tons, fin, réglage 1, réglage 2, niveau
    enum EngField { eType = 0, eOct, eSemi, eFine, eP1, eP2, eLevel, engStride };
    enum Global
    {
        engA = 0,
        engB = engA + engStride,
        fmAB = engB + engStride, sub, noiseLvl, noiseColor,
        f1Cut, f1Res, f1Morph, f2Cut, f2Res, f2Drive, route, fEnv, fKey,
        fA, fD, fS, fR, aa, ad, as, ar, velAmp,
        voices, vMode, glide, tune, spread, level,
        numSlots
    };

    /** Tables : 8 images × 9 octaves (512 partiels au plus, divisés par deux à chaque octave). */
    inline const std::vector<float>& tables()
    {
        static const std::vector<float> t = []
        {
            std::vector<float> v ((size_t) numFrames * numMips * (tableSize + 1), 0.0f);
            auto amp = [] (int f, int k) -> double
            {
                switch (f)
                {
                    case 0: return k == 1 ? 1.0 : 0.0;                                        // sinus
                    case 1: return (k & 1) ? ((k % 4 == 1) ? 1.0 : -1.0) / (double) (k * k) : 0.0; // triangle
                    case 2: return 1.0 / k;                                                     // scie
                    case 3: return (k & 1) ? 1.0 / k : 0.0;                                     // carré
                    case 4: { const auto a = std::exp (-0.5 * std::pow ((k * 110.0 - 700.0) / 180.0, 2.0)) + 0.7 * std::exp (-0.5 * std::pow ((k * 110.0 - 1200.0) / 220.0, 2.0));
                              return (0.15 / k + a) ; }                                          // voyelle « a »
                    case 5: { const auto a = std::exp (-0.5 * std::pow ((k * 110.0 - 400.0) / 140.0, 2.0)) + 0.5 * std::exp (-0.5 * std::pow ((k * 110.0 - 800.0) / 160.0, 2.0));
                              return (0.12 / k + a); }                                           // voyelle « o »
                    case 6: { std::uint32_t h = 0x9E3779B9u * (std::uint32_t) (k + 17); h ^= h >> 15; h *= 0x85EBCA6Bu; h ^= h >> 13;
                              return ((double) (h & 0xFFFF) / 65535.0) / std::sqrt ((double) k); } // métal (amplitudes figées)
                    default: return 1.0 / std::sqrt ((double) k);                               // impulsion
                }
            };
            for (int f = 0; f < numFrames; ++f)
                for (int mip = 0; mip < numMips; ++mip)
                {
                    const int H = std::max (1, 512 >> mip);
                    auto* dst = v.data() + ((size_t) f * numMips + (size_t) mip) * (tableSize + 1);
                    double peak = 1.0e-9;
                    std::vector<double> tmp ((size_t) tableSize, 0.0);
                    for (int k = 1; k <= H; ++k)
                    {
                        const auto a = amp (f, k);
                        if (a == 0.0) continue;
                        // sin(k θ) par récurrence le long de la table
                        const auto w = 6.283185307179586 * k / tableSize;
                        const auto c2 = 2.0 * std::cos (w);
                        double s0 = 0.0, s1 = std::sin (w);
                        tmp[0] += 0.0;
                        for (int i = 1; i < tableSize; ++i) { tmp[(size_t) i] += a * s1; const auto s2 = c2 * s1 - s0; s0 = s1; s1 = s2; }
                    }
                    for (auto x : tmp) peak = std::max (peak, std::abs (x));
                    for (int i = 0; i < tableSize; ++i) dst[i] = (float) (tmp[(size_t) i] / peak);
                    dst[tableSize] = dst[0];
                }
            return v;
        }();
        return t;
    }

    inline double tableRead (int frame, int mip, double ph) noexcept
    {
        const auto* t = tables().data() + ((size_t) frame * numMips + (size_t) mip) * (tableSize + 1);
        const auto pos = ph * tableSize;
        const auto i = (int) pos;
        const auto f = pos - i;
        return t[i] + f * (t[i + 1] - t[i]);
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> types { "Analogique", "Table d'ondes", "Harmonique", "Modal" };
            ModuleDesc m;
            m.type = "hots";
            m.name = "Hot Sauce";
            m.family = "Drones";
            m.principle = "Hybride \xC3\xA0 deux moteurs (analogique, table d'ondes, harmonique, modal), FM de A vers B, sous-oscillateur et bruit, deux filtres s\xC3\xA9rie \xE2\x86\x92 parall\xC3\xA8le, 3 LFO et 2 enveloppes";
            for (const char* e : { "a", "b" })
            {
                const std::string id (e), N = std::string ("Moteur ") + (char) (e[0] - 32) + " \xC2\xB7 ";
                m.params.push_back (C (id + "type", N + "type", types, e[0] == 'a' ? 1 : 0));
                m.params.push_back (I (id + "oct", N + "octave", "", -2, 2, 0));
                m.params.push_back (I (id + "semi", N + "demi-tons", "dt", -12, 12, 0));
                m.params.push_back (P (id + "fine", N + "fin", "ct", -50, 50, e[0] == 'a' ? 0.0f : 7.0f));
                m.params.push_back (P (id + "p1", N + "r\xC3\xA9glage 1", "%", 0, 100, e[0] == 'a' ? 30.0f : 50.0f));
                m.params.push_back (P (id + "p2", N + "r\xC3\xA9glage 2", "%", 0, 100, 20));
                m.params.push_back (P (id + "lvl", N + "niveau", "%", 0, 100, e[0] == 'a' ? 80.0f : 0.0f));
            }
            for (auto& x : std::vector<ParamDesc> {
                     P ("fmab", "FM A \xE2\x86\x92 B", "%", 0, 100, 0),
                     P ("sub", "Sous-oscillateur", "%", 0, 100, 0),
                     P ("noise", "Bruit", "%", 0, 100, 0),
                     P ("ncolor", "Couleur du bruit", "%", 0, 100, 60),
                     P ("f1cut", "Coupure 1", "Hz", 20, 18000, 8000, 0.3f),
                     P ("f1res", "R\xC3\xA9sonance 1", "%", 0, 100, 10),
                     P ("f1morph", "Bas \xE2\x86\x92 haut", "%", 0, 100, 0),
                     P ("f2cut", "Coupure 2", "Hz", 20, 18000, 18000, 0.3f),
                     P ("f2res", "R\xC3\xA9sonance 2", "%", 0, 100, 0),
                     P ("f2drive", "Saturation 2", "%", 0, 100, 0),
                     P ("route", "S\xC3\xA9rie \xE2\x86\x92 parall\xC3\xA8le", "%", 0, 100, 0),
                     P ("fenv", "Qt\xC3\xA9 env.", "%", -100, 100, 25),
                     P ("fkey", "Clavier", "%", 0, 100, 40),
                     P ("fa", "Attaque", "ms", 0.5f, 10000, 2, 0.3f),
                     P ("fd", "D\xC3\xA9" "clin", "ms", 5, 10000, 600, 0.3f),
                     P ("fs", "Maintien", "%", 0, 100, 30),
                     P ("fr", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 500, 0.3f),
                     P ("aa", "Attaque", "ms", 0.5f, 10000, 3, 0.3f),
                     P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 900, 0.3f),
                     P ("as", "Maintien", "%", 0, 100, 80),
                     P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 500, 0.3f),
                     P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 35),
                     I ("voices", "Voix", "", 1, 8, 8),
                     C ("vmode", "Jeu", { "Poly", "Mono", "Mono legato" }),
                     P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                     P ("tune", "Transposition", "dt", -24, 24, 0),
                     P ("spread", "Largeur st\xC3\xA9r\xC3\xA9o", "%", 0, 100, 35),
                     P ("level", "Volume", "dB", -30, 6, -15) })
                m.params.push_back (x);
            m.sections = {
                { "enga", "Moteur A", "osc", { "atype", "aoct", "asemi", "afine", "ap1", "ap2", "alvl" }, "osc" },
                { "engb", "Moteur B", "osc", { "btype", "boct", "bsemi", "bfine", "bp1", "bp2", "blvl", "fmab" }, "osc" },
                { "util", "Utilitaire", "mix", { "sub", "noise", "ncolor" }, "osc" },
                { "filters", "Filtres", "filter", { "f1cut", "f1res", "f1morph", "f2cut", "f2res", "f2drive", "route", "fenv", "fkey" }, "filter" },
                { "fenvs", "Enveloppe filtre", "env", { "fa", "fd", "fs", "fr" }, "env" },
                { "amp", "Ampli", "amp", { "aa", "ad", "as", "ar", "velamp" }, "amp" },
                { "global", "Global", "global", { "voices", "vmode", "glide", "tune", "spread", "level" }, "global" },
            };
            using V = std::vector<std::pair<std::string, float>>;
            m.presets = {
                { "Init", V {}, "Init" },
                { "Sauce piquante", V { { "atype", 1 }, { "ap1", 55 }, { "btype", 0 }, { "blvl", 45 }, { "bp1", 50 }, { "bp2", 40 }, { "f1cut", 1800 }, { "f1res", 35 }, { "fenv", 45 },
                                        { "aa", 400 }, { "ar", 1800 }, { "mx1_src", 6 }, { "mx1_dst@ap1", 0 }, { "mx1_amt", 30 }, { "lf1_rate", 0.2f }, { "level", -15 } }, "Nappes" },
                { "Cloche modale", V { { "atype", 3 }, { "ap1", 85 }, { "ap2", 70 }, { "as", 0 }, { "ad", 4000 }, { "ar", 3000 }, { "level", -6.5f } }, "Effets" },
            };
            HubSpec hub;
            hub.envs = 2; hub.lfos = 3; hub.firstLfo = 1; hub.slots = 16; hub.arp = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m); // banque d'usine, au moins 10 sons par catégorie (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

/** Un moteur d'une voix : état de chacun des 4 types (un seul sert à la fois). */
struct HotsEngine
{
    MorphOsc o1, o2;
    double ph = 0.0;                         // table d'ondes, harmonique
    std::array<double, 6> y1 {}, y2 {};      // résonateurs du modal
    double excite = 0.0;                     // impulsion du modal au départ de la note
    void reset() noexcept { o1.reset(); o2.reset (0.37); ph = 0.0; y1.fill (0.0); y2.fill (0.0); excite = 0.0; }
};

class HotsVoice
{
public:
    void prepare (double s) { sr = s; fenv.prepare (s); aenv.prepare (s); reset(); }
    void reset() noexcept
    {
        a.reset(); b.reset(); fenv.reset(); aenv.reset(); f1.reset(); f2.reset(); dc.reset();
        subPh = 0.0; nlp = 0.0; lastA = 0.0;
        active = pending = releasePending = false;
    }
    void setIndex (int i) noexcept { static constexpr double spots[8] { 0.15, -0.15, 0.5, -0.5, 0.8, -0.8, 0.35, -0.35 }; pan = spots[i & 7]; noise = dsp::WhiteNoise (0xC0FFEEu + 7919u * (std::uint32_t) i); }
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
        using namespace hots;
        if (pending)
        {
            pending = false;
            if (p[glide] > 0.5f && from >= 0 && legatoPending) pitch.to ((double) target);
            else if (p[glide] > 0.5f && from >= 0) { pitch.jump ((double) from); pitch.to ((double) target); }
            else pitch.jump ((double) target);
            if (! legatoPending) { fenv.gate (true); aenv.gate (true); a.excite = b.excite = 1.0; }
            relFlag = false;
        }
        if (releasePending) { releasePending = false; relFlag = true; fenv.gate (false); aenv.gate (false); }
        fenv.set (p[fA], p[fD], p[fS] / 100.0, p[fR]);
        aenv.set (p[aa], p[ad], p[as] / 100.0, p[ar]);
        const bool gate = aenv.isActive() && ! released();

        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-(double) n / (p[glide] * 0.001 * sr * 0.35));
        const auto note = pitch.next (glideCoef) + p[tune] + uni * 0.1;
        EngSetup ea = setup (p, engA, note), eb = setup (p, engB, note);
        const auto fmAmt = std::pow (p[fmAB] / 100.0, 2.0) * 1.5;
        const auto subL = p[sub] / 100.0, nL = p[noiseLvl] / 100.0;
        const auto nCoef = 1.0 - std::exp (-6.283 * (200.0 * std::pow (90.0, p[noiseColor] / 100.0)) / sr);
        const auto subInc = std::clamp (noteHz (note - 12.0) / sr, 0.0, 0.45);
        const auto keyOct = (p[fKey] / 100.0) * (note - 60.0) / 12.0;
        const auto envOct = (p[fEnv] / 100.0) * 7.0;
        const auto r1 = p[f1Res] / 100.0, r2 = p[f2Res] / 100.0, morph = p[f1Morph] / 100.0;
        const auto dv = p[f2Drive] / 100.0, drive2 = 1.0 + dv * dv * 8.0;
        const auto par = p[route] / 100.0;
        const auto gain = dbToGain (p[level]) * velocityGain (vel, p[velAmp] / 100.0);
        const auto sp = p[spread] / 100.0;
        const auto pv = std::clamp (pan * sp + uni * sp * 0.8, -1.0, 1.0);
        const auto gL = std::sqrt (0.5 * (1.0 - pv)) * 1.4142, gR = std::sqrt (0.5 * (1.0 + pv)) * 1.4142;
        SvfCoefs fc1;
        double fc2 = 1000.0;

        for (int i = 0; i < n; ++i)
        {
            const auto xa = ea.lvl > 0.0 || fmAmt > 0.0 ? engine (a, ea, 0.0, gate) : 0.0;
            const auto xb = eb.lvl > 0.0 ? engine (b, eb, fmAmt * xa, gate) : 0.0;
            double x = xa * ea.lvl + xb * eb.lvl;
            if (subL > 0.0) { subPh += subInc; if (subPh >= 1.0) subPh -= 1.0; x += subL * fastSin2Pi (subPh); }
            if (nL > 0.0) { nlp += (noise.next() - nlp) * nCoef; x += nL * nlp * 0.8; }
            x *= 0.5;
            const auto fe = fenv.next();
            if ((i & 7) == 0)
            {
                const auto oct = envOct * fe + keyOct;
                fc1.set (std::clamp (p[f1Cut] * fastExp2 (oct), 16.0, 0.45 * sr), r1, sr);
                fc2 = std::clamp (p[f2Cut] * fastExp2 (oct), 16.0, 0.45 * sr);
            }
            const auto o = f1.process (x, fc1);
            const auto y1 = morph < 0.5 ? o.low + (o.band - o.low) * (morph * 2.0) : o.band + (o.high - o.band) * ((morph - 0.5) * 2.0);
            // série : F1 puis F2 ; parallèle : F1 + F2 ; fondu continu entre les deux
            const auto ser = f2.process (y1 * 0.5, fc2, r2, sr, Ladder::lp24, drive2) * 2.0;
            const auto pr = par > 0.0 ? 0.5 * (y1 + f2b.process (x * 0.5, fc2, r2, sr, Ladder::lp24, drive2) * 2.0) : 0.0;
            const auto y = dc.process (ser + (pr - ser) * par) * aenv.next() * gain;
            L[i] += (float) (y * gL);
            R[i] += (float) (y * gR);
        }
        if (! aenv.isActive()) active = false;
    }

private:
    struct EngSetup
    {
        int type = 0;
        double dt = 0.0, p1 = 0.0, p2 = 0.0, lvl = 0.0;
        int mip = 0;
        std::array<double, 6> mf {}, mr {}, mc {}, ms {};   // modal : fréquences (rad/éch.), rayons, 2 r cos w, sin w
    };
    bool released() const noexcept { return relFlag; }

    EngSetup setup (const float* p, int base, double note) const noexcept
    {
        using namespace hots;
        EngSetup e;
        e.type = std::clamp ((int) std::lround (p[base + eType]), 0, 3);
        const auto nt = note + 12.0 * p[base + eOct] + p[base + eSemi] + p[base + eFine] / 100.0;
        const auto hz = noteHz (nt);
        e.dt = std::clamp (hz / sr, 1.0e-7, 0.45);
        e.p1 = p[base + eP1] / 100.0;
        e.p2 = p[base + eP2] / 100.0;
        e.lvl = p[base + eLevel] / 100.0;
        // octave de table : 512 partiels sous 20 Hz environ, divisés par deux à chaque octave
        e.mip = std::clamp ((int) std::ceil (std::log2 (std::max (1.0, hz * 512.0 / (0.45 * sr)))), 0, numMips - 1);
        if (e.type == 3)
        {
            // matière : corde (harmoniques) → barre → plaque (partiels serrés), fondu continu
            static constexpr double str[6] { 1, 2, 3, 4, 5, 6 }, bar[6] { 1, 2.756, 5.404, 8.933, 13.34, 18.64 }, plate[6] { 1, 1.59, 2.14, 2.30, 2.65, 2.92 };
            const auto m = e.p1 * 2.0;
            const auto decayS = 0.05 * std::pow (160.0, e.p2); // 50 ms à 8 s
            for (int k = 0; k < 6; ++k)
            {
                const auto ratio = m < 1.0 ? str[k] + (bar[k] - str[k]) * m : bar[k] + (plate[k] - bar[k]) * (m - 1.0);
                const auto f = hz * ratio;
                e.mf[(size_t) k] = f < 0.45 * sr ? 6.283185307179586 * f / sr : 0.0;
                e.mr[(size_t) k] = std::exp (-1.0 / (std::max (0.01, decayS / (1.0 + 0.6 * k)) * sr));
                e.mc[(size_t) k] = 2.0 * e.mr[(size_t) k] * std::cos (e.mf[(size_t) k]);
                e.ms[(size_t) k] = std::sin (e.mf[(size_t) k]);
            }
        }
        return e;
    }

    double engine (HotsEngine& g, const EngSetup& e, double fm, bool gate) noexcept
    {
        using namespace hots;
        switch (e.type)
        {
            case 0: // analogique : deux oscillateurs à forme continue, désaccord
            {
                const auto shape = e.p1 * 4.0, det = e.p2 * e.p2 * 0.03;
                return 0.6 * (g.o1.next (shape, e.dt * (1.0 - det), fm) + g.o2.next (shape, e.dt * (1.0 + det), fm));
            }
            case 1: // table d'ondes : image balayée, pli
            {
                g.ph += e.dt; if (g.ph >= 1.0) g.ph -= 1.0;
                auto ph = g.ph + fm; ph -= std::floor (ph);
                const auto pos = e.p1 * (numFrames - 1);
                const auto f0 = std::min (numFrames - 2, (int) pos);
                const auto t = pos - f0;
                auto v = tableRead (f0, e.mip, ph) * (1.0 - t) + tableRead (f0 + 1, e.mip, ph) * t;
                if (e.p2 > 0.0) { const auto k = 1.0 + e.p2 * 3.0; v = fastSin2Pi (0.25 * v * k); }
                return v;
            }
            case 2: // harmonique : 16 partiels, pente et parité
            {
                g.ph += e.dt; if (g.ph >= 1.0) g.ph -= 1.0;
                auto ph = g.ph + fm; ph -= std::floor (ph);
                const auto tilt = 0.3 + e.p1 * 2.2;
                const auto s1 = fastSin2Pi (ph), c2 = 2.0 * fastSin2Pi (ph + 0.25);
                double s0 = 0.0, sk = s1, sum = 0.0, norm = 0.0;
                for (int k = 1; k <= 16; ++k)
                {
                    if (k * e.dt >= 0.45) break;
                    // parité : 0 tous, 50 impairs seuls, 100 pairs mis en avant
                    const bool odd = (k & 1) != 0;
                    const auto par = e.p2 < 0.5 ? (odd ? 1.0 : 1.0 - e.p2 * 2.0) : (odd ? (k == 1 ? 1.0 : 1.0 - (e.p2 - 0.5) * 1.6) : (e.p2 - 0.5) * 2.0);
                    const auto a = par / std::pow ((double) k, tilt);
                    sum += a * sk; norm += a;
                    const auto sn = c2 * sk - s0; s0 = sk; sk = sn;
                }
                return norm > 0.0 ? sum / std::max (1.0, norm * 0.6) : 0.0;
            }
            default: // modal : impulsion au départ + frottement tant que la note est tenue
            {
                double ex = g.excite;
                g.excite *= 0.995;
                if (g.excite < 1.0e-4) g.excite = 0.0;
                if (gate) ex += 0.02 * noise.next();
                ex += fm * 0.1;
                double sum = 0.0;
                for (int k = 0; k < 6; ++k)
                {
                    const auto w = e.mf[(size_t) k];
                    if (w <= 0.0) continue;
                    const auto r = e.mr[(size_t) k];
                    // entrée pondérée par sin w : une impulsion donne une amplitude voisine de 1 quelle que soit la hauteur
                    const auto y = e.mc[(size_t) k] * g.y1[(size_t) k] - r * r * g.y2[(size_t) k] + ex * e.ms[(size_t) k];
                    g.y2[(size_t) k] = g.y1[(size_t) k]; g.y1[(size_t) k] = y;
                    sum += y / (1.0 + 0.5 * k);
                }
                return fastTanh (sum * 0.8);
            }
        }
    }

    double sr = 48000.0, pan = 0.0, uni = 0.0, subPh = 0.0, nlp = 0.0, lastA = 0.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool active = false, pending = false, releasePending = false, legatoPending = false, relFlag = false;
    HotsEngine a, b;
    Adsr fenv, aenv;
    StateFilter f1;
    Ladder f2, f2b;
    Glide pitch;
    DcBlocker dc;
    dsp::WhiteNoise noise;
};

class HotsSynth final : public SynthBase<HotsVoice, 8>
{
public:
    HotsSynth() { (void) hots::tables(); for (int i = 0; i < 8; ++i) voices[(size_t) i].setIndex (i); }
    const ModuleDesc& desc() const noexcept override { return hots::desc(); }

protected:
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[hots::voices], 1, 8);
        switch ((int) p[hots::vMode])
        {
            case 1: setMode (true, false, v); break;
            case 2: setMode (true, true, v); break;
            default: setMode (false, true, v); break;
        }
    }
};

} // namespace grv::instruments
