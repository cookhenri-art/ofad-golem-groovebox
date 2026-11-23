/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Kick Forge (1.2) — calcul du kick, hors du fil audio (fil de calcul de l'instrument).
    Chaîne d'une couche :
        morceau calculé ou sample  →  inversion  →  hauteur (rééchantillonnage)  →  étirement
        (WSOLA, garde la hauteur)  →  longueur, fondus d'entrée et de sortie à courbe  →  filtres
        passe-haut / passe-bas  →  égaliseur 3 bandes  →  transitoires  →  distorsion
        (suréchantillonnée ×2)  →  polarité, volume  →  décalage dans le temps.
    Alignement de phase (réglable) : le grave de chaque couche est tourné en phase (après son
    attaque, qui reste intacte) pour s'additionner au grave des autres au lieu de s'annuler.
    Somme des couches → rack de 9 modules dans l'ordre choisi (égaliseur, égaliseur M/S,
    distorsion, façonneur de kick, transitoires, convolution, finaliseur, multibande, filtres) →
    gain automatique à l'INTENSITÉ perçue (pondération K, comme le bus de GOLEM : le kick sonne
    aussi fort que les grosses caisses des batteries, quel que soit son clic) + limiteur de crête
    à anticipation (crête sous -1 dBFS, 6 dB de réduction au plus).
    Comme le calcul n'est pas fait en temps réel, tout est pris en qualité maximale ; ce qu'on
    entend est exactement ce que l'afficheur montre.
*/

#pragma once

#include "KfParts.h"
#include "../gd2/Gd2Sources.h"
#include "../gd2/Gd2Fx.h"
#include "../../dsp/Fft.h"
#include "../../engine/Module.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace grv::instruments::kf
{

constexpr int numLayers = 8;
constexpr double maxSeconds = 4.0;

enum LayerParam
{
    lOn = 0, lFam, lPart, lVol, lPitch, lFine, lStretch, lOffset, lLen, lFiLen, lFiCurve, lFoLen, lFoCurve,
    lMute, lSolo, lPhase, lReverse, lLock, lKeyTrack, lHp, lLp, lEqLo, lEqMidF, lEqMidG, lEqHi, lTrAtk, lTrSus,
    lDrvType, lDrv, lDrvMix,
    numLayerParams
};
inline const char* layerIds[numLayerParams] { "on", "fam", "part", "vol", "pitch", "fine", "stretch", "offset", "len", "filen", "ficurve", "folen", "focurve",
                                              "mute", "solo", "phase", "rev", "lock", "keytrk", "hp", "lp", "eqlo", "eqmidf", "eqmidg", "eqhi", "tratk", "trsus",
                                              "drvtype", "drv", "drvmix" };
inline std::string layerPrefix (int l) { return "l" + std::to_string (l + 1) + "_"; }

constexpr int gBase = numLayers * numLayerParams; // 240
enum GlobalParam { gStyle = gBase, gKey, gPitchMode, gVelSens, gVolume, gAutogain, gEnd };

/** Modules du rack (positions figées). */
enum Module { mEq = 0, mMs, mDist, mShaper, mTrans, mConv, mFinal, mMulti, mFilt, numModules };
inline const char* moduleNames[numModules] { "\xC3\x89galiseur", "\xC3\x89galiseur M/S", "Distorsion", "Fa\xC3\xA7onneur de kick", "Transitoires",
                                            "Convolution", "Finaliseur", "Multibande", "Filtres" };
inline const char* modulePrefix[numModules] { "eq", "ms", "ds", "ks", "tr", "cv", "fn", "mb", "fl" };
// réglages propres à chaque module (après sa marche et son rang)
inline const std::vector<std::vector<const char*>>& moduleParams()
{
    static const std::vector<std::vector<const char*>> m {
        { "lsf", "lsg", "b1f", "b1g", "b1q", "b2f", "b2g", "b2q", "hsf", "hsg", "mix" },
        { "midlo", "midhi", "sidehp", "sideg", "width", "mix" },
        { "type", "drive", "pre", "pref", "tone", "mix" },
        { "amount", "n1", "n2", "n3", "n4", "n5", "n6", "n7", "n8", "mix" },
        { "attack", "duck", "speed", "mix" },
        { "ir", "wet", "stereo", "tone", "speed", "c1", "c2t", "c2", "c3", "mix" },
        { "boostf", "boost", "mono", "clip", "mix" },
        { "depth", "time", "up", "down", "low", "mid", "high", "mix" },
        { "hp", "hpres", "lp", "lpres", "mix" },
    };
    return m;
}
/** Index du premier réglage de chaque module (sa marche), puis rang, puis réglages propres. */
inline const std::array<int, numModules + 1>& moduleBase()
{
    static const std::array<int, numModules + 1> b = []
    {
        std::array<int, numModules + 1> r {};
        int at = gEnd;
        for (int m = 0; m < numModules; ++m) { r[(size_t) m] = at; at += 2 + (int) moduleParams()[(size_t) m].size(); }
        r[numModules] = at;
        return r;
    }();
    return b;
}
/** Réglages ajoutés après le rack (positions figées à la suite) : alignement de phase des couches. */
inline int phaseAlignSlot() { return moduleBase()[numModules]; }
inline int numSlots() { return moduleBase()[numModules] + 1; }
inline int modParam (int module, const char* id)
{
    const auto& ids = moduleParams()[(size_t) module];
    for (size_t i = 0; i < ids.size(); ++i) if (std::string (ids[i]) == id) return moduleBase()[(size_t) module] + 2 + (int) i;
    return -1;
}

//==============================================================================
/** Réponses impulsionnelles calculées de la convolution (catégories, comme une banque d'IR). */
inline const std::vector<std::pair<const char*, const char*>>& irList()
{
    static const std::vector<std::pair<const char*, const char*>> l {
        { "Clics et claquements", "Claquement sec" }, { "Clics et claquements", "Claquement double" }, { "Clics et claquements", "Clic de bois" }, { "Clics et claquements", "Clic m\xC3\xA9tallique" },
        { "Impulsions diverses", "Bo\xC3\xAEte" }, { "Impulsions diverses", "Tube" }, { "Impulsions diverses", "Ressort" }, { "Impulsions diverses", "T\xC3\xB4le" },
        { "R\xC3\xA9verb\xC3\xA9rations", "Pi\xC3\xA8" "ce" }, { "R\xC3\xA9verb\xC3\xA9rations", "Petite salle" }, { "R\xC3\xA9verb\xC3\xA9rations", "Grande salle" },
        { "R\xC3\xA9verb\xC3\xA9rations", "Plaque brillante" }, { "R\xC3\xA9verb\xC3\xA9rations", "Plaque sombre" }, { "R\xC3\xA9verb\xC3\xA9rations", "Hangar" },
        { "Queues de rumble", "Rumble court" }, { "Queues de rumble", "Rumble long" }, { "Queues de rumble", "Rumble sombre" }, { "Queues de rumble", "Rumble granuleux" },
        { "Chutes de sub", "Chute courte" }, { "Chutes de sub", "Chute longue" }, { "Chutes de sub", "Chute profonde" },
        { "Textures", "Grain" }, { "Textures", "Souffle" }, { "Textures", "Cr\xC3\xA9pitement" },
        { "Corps tonals", "Corps bois" }, { "Corps tonals", "Corps m\xC3\xA9tal" }, { "Corps tonals", "Corps verre" },
    };
    return l;
}

namespace detail
{
    /** FFT préparée de l'ordre demandé, gardée par le fil qui calcule (sa préparation coûte plus que la
        transformée elle-même). */
    inline dsp::Fft& fftFor (int order)
    {
        thread_local std::array<std::unique_ptr<dsp::Fft>, 26> ffts;
        auto& f = ffts[(size_t) std::clamp (order, 1, 25)];
        if (f == nullptr) { f = std::make_unique<dsp::Fft>(); f->prepare (std::clamp (order, 1, 25)); }
        return *f;
    }

    using Buf = std::vector<float>;

    /** Fondamentale d'un kick (autocorrélation normalisée, 25–500 Hz) ; 0 si rien de net. */
    inline double kickPitch (const float* x, int n, double sr)
    {
        const auto minLag = (int) (sr / 500.0), maxLag = std::min (n / 2, (int) (sr / 25.0));
        if (maxLag <= minLag + 2) return 0.0;
        std::vector<double> r ((size_t) (maxLag + 2), 0.0);
        double best = 0.0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double num = 0.0, e1 = 0.0, e2 = 0.0;
            for (int i = 0; i + lag < n; i += 2) { num += (double) x[i] * x[i + lag]; e1 += (double) x[i] * x[i]; e2 += (double) x[i + lag] * x[i + lag]; }
            r[(size_t) lag] = num / std::sqrt (e1 * e2 + 1.0e-12);
            best = std::max (best, r[(size_t) lag]);
        }
        if (best <= 0.5) return 0.0;
        for (int lag = minLag + 1; lag < maxLag; ++lag)
        {
            const auto a = r[(size_t) lag - 1], b = r[(size_t) lag], c = r[(size_t) lag + 1];
            if (b >= 0.9 * best && b >= a && b >= c)
            {
                const auto den = a - 2.0 * b + c;
                const auto d = std::abs (den) > 1.0e-12 ? 0.5 * (a - c) / den : 0.0;
                return sr / ((double) lag + std::clamp (d, -0.5, 0.5));
            }
        }
        return 0.0;
    }

    /** Rééchantillonnage d'un rapport donné (interpolation de Hermite) : rate > 1 = plus aigu et plus court. */
    inline Buf resample (const Buf& in, double rate)
    {
        if (in.empty() || std::abs (rate - 1.0) < 1.0e-6) return in;
        const auto n = (int) in.size();
        const auto outN = std::max (1, (int) std::floor ((n - 1) / rate));
        Buf out ((size_t) outN);
        auto at = [&] (int i) { return in[(size_t) std::clamp (i, 0, n - 1)]; };
        for (int k = 0; k < outN; ++k)
        {
            const auto pos = k * rate;
            const auto i = (int) pos;
            const auto f = pos - i;
            const double y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
            const auto c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
            out[(size_t) k] = (float) (((c3 * f + c2) * f + c1) * f + y1);
        }
        return out;
    }

    /** Étirement temporel WSOLA (garde la hauteur) : factor > 1 = plus long. */
    inline Buf stretch (const Buf& in, double factor, double sr)
    {
        if (in.size() < 64 || std::abs (factor - 1.0) < 1.0e-3) return in;
        const int N = std::max (256, (int) (sr * 0.03)) & ~1, H = N / 2, tol = N / 4;
        const auto inN = (int) in.size();
        const auto outN = (int) (inN * factor);
        Buf out ((size_t) outN + (size_t) N, 0.0f), wsum ((size_t) outN + (size_t) N, 0.0f);
        Buf win ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = (float) (0.5 - 0.5 * std::cos (dsp::kTwoPi * i / N));
        auto in0 = [&] (int i) { return i >= 0 && i < inN ? in[(size_t) i] : 0.0f; };
        int prevIn = 0; // début du bloc d'entrée précédent
        for (int outPos = 0; outPos < outN; outPos += H)
        {
            const int nominal = (int) (outPos / factor);
            int best = nominal;
            if (outPos > 0)
            {
                // cherche autour de la position nominale le bloc qui prolonge le mieux le précédent
                double bestC = -1.0e30;
                const int target = prevIn + H;
                for (int d = -tol; d <= tol; d += 2)
                {
                    const int c = nominal + d;
                    double s = 0.0;
                    for (int i = 0; i < H; i += 4) s += (double) in0 (target + i) * in0 (c + i);
                    if (s > bestC) { bestC = s; best = c; }
                }
            }
            prevIn = best;
            for (int i = 0; i < N; ++i)
            {
                out[(size_t) (outPos + i)] += in0 (best + i) * win[(size_t) i];
                wsum[(size_t) (outPos + i)] += win[(size_t) i];
            }
        }
        // le tout début reste l'attaque d'origine (pas de fenêtre qui monte au départ)
        for (int i = 0; i < std::min (H, inN); ++i) { out[(size_t) i] = in[(size_t) i]; wsum[(size_t) i] = 1.0f; }
        out.resize ((size_t) outN);
        for (int i = 0; i < outN; ++i) if (wsum[(size_t) i] > 1.0e-3f) out[(size_t) i] /= std::max (wsum[(size_t) i], 0.5f);
        return out;
    }

    /** Forme de fondu : x 0..1, courbe -1..1 (0 linéaire, >0 lent au départ, <0 rapide au départ). */
    inline double fadeShape (double x, double c) noexcept
    {
        x = std::clamp (x, 0.0, 1.0);
        if (c >= 0.0) return std::pow (x, 1.0 + 4.0 * c);
        return 1.0 - std::pow (1.0 - x, 1.0 - 4.0 * c);
    }

    /** Façonneur de transitoires : attack -1..1 (relève ou adoucit l'attaque), duck 0..1 (creuse le
        maintien juste après l'attaque), temps en secondes. */
    inline void transient (float* x, int n, double sr, double attack, double duck, double timeS)
    {
        if (std::abs (attack) < 1.0e-3 && duck < 1.0e-3) return;
        const auto fa = std::exp (-1.0 / (0.0008 * sr)), fr = std::exp (-1.0 / (std::max (0.003, timeS * 0.5) * sr));
        const auto sa = std::exp (-1.0 / (std::max (0.003, timeS) * sr)), sr2 = std::exp (-1.0 / (std::max (0.01, timeS * 3.0) * sr));
        double ef = 0.0, es = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto a = std::abs ((double) x[i]);
            ef = a > ef ? a + (ef - a) * fa : a + (ef - a) * fr;
            es = a > es ? a + (es - a) * sa : a + (es - a) * sr2;
            const auto tr = std::clamp ((ef - es) / (es + 1.0e-4), 0.0, 4.0); // part transitoire
            auto gDb = attack * 12.0 * std::min (1.0, tr);
            gDb -= duck * 18.0 * std::clamp (1.0 - tr, 0.0, 1.0) * std::min (1.0, es * 8.0);
            x[i] = (float) (x[i] * dsp::dbToGain (gDb));
        }
    }

    /** Deux convolutions du même signal réel x par h0 et h1 en trois transformées : h0 + j·h1 est
        transformé d'un coup, et la partie réelle et imaginaire du résultat sont x∗h0 et x∗h1. */
    inline std::pair<Buf, Buf> convolve2 (const Buf& x, const Buf& h0, const Buf& h1)
    {
        if (x.empty() || (h0.empty() && h1.empty())) return { Buf (x.size(), 0.0f), Buf (x.size(), 0.0f) };
        const auto hn = std::max (h0.size(), h1.size());
        const auto total = x.size() + hn - 1;
        int order = 1;
        while ((size_t) (1 << order) < total) ++order;
        auto& fft = fftFor (order);
        const auto N = (size_t) fft.size();
        std::vector<std::complex<float>> a (N), b (N);
        for (size_t i = 0; i < x.size(); ++i) a[i] = { x[i], 0.0f };
        for (size_t i = 0; i < hn; ++i) b[i] = { i < h0.size() ? h0[i] : 0.0f, i < h1.size() ? h1[i] : 0.0f };
        fft.forward (a.data()); fft.forward (b.data());
        for (size_t i = 0; i < N; ++i) a[i] *= b[i];
        fft.inverse (a.data());
        Buf y0 (total), y1 (total);
        for (size_t i = 0; i < total; ++i) { y0[i] = a[i].real(); y1[i] = a[i].imag(); }
        return { std::move (y0), std::move (y1) };
    }

    /** Convolution par FFT (une seule transformée, hors temps réel). */
    inline Buf convolve (const Buf& x, const Buf& h)
    {
        if (x.empty() || h.empty()) return Buf (x.size(), 0.0f);
        const auto total = x.size() + h.size() - 1;
        int order = 1;
        while ((size_t) (1 << order) < total) ++order;
        auto& fft = fftFor (order);
        const auto N = (size_t) fft.size();
        std::vector<std::complex<float>> a (N), b (N);
        for (size_t i = 0; i < x.size(); ++i) a[i] = { x[i], 0.0f };
        for (size_t i = 0; i < h.size(); ++i) b[i] = { h[i], 0.0f };
        fft.forward (a.data()); fft.forward (b.data());
        for (size_t i = 0; i < N; ++i) a[i] *= b[i];
        fft.inverse (a.data());
        Buf y (total);
        for (size_t i = 0; i < total; ++i) y[i] = a[i].real();
        return y;
    }

    /** Réponse impulsionnelle n° ir (calculée), côté 0 ou 1 (deux tirages différents pour la stéréo). */
    inline Buf makeIr (int ir, int side, double keyHz, double sr)
    {
        Rng R ((std::uint32_t) (ir * 31337 + side * 977 + 5));
        const int cat = ir < 4 ? 0 : ir < 8 ? 1 : ir < 14 ? 2 : ir < 18 ? 3 : ir < 21 ? 4 : ir < 24 ? 5 : 6;
        double len = 0.3;
        switch (cat)
        {
            case 0: len = 0.04 + 0.03 * (ir % 4); break;
            case 1: len = 0.15 + 0.1 * (ir % 4); break;
            case 2: { static const double l[6] { 0.45, 0.8, 1.8, 1.2, 1.0, 2.6 }; len = l[ir - 8]; break; }
            case 3: len = ir == 15 ? 1.4 : 0.7; break;
            case 4: len = ir == 19 ? 1.2 : ir == 20 ? 1.6 : 0.6; break;
            case 5: len = 0.6; break;
            default: len = 0.5; break;
        }
        Buf h ((size_t) std::max (32.0, len * sr), 0.0f);
        const auto n = (int) h.size();
        F lp (0, cat == 2 && ir == 11 ? 12000.0 : cat == 2 && ir == 12 ? 2500.0 : cat == 3 ? 220.0 : cat == 4 ? 180.0 : 7000.0, 0.7, sr);
        F bp (1, cat == 6 ? (ir == 24 ? 700.0 : ir == 25 ? 2200.0 : 4200.0) : 1500.0, cat == 6 ? 18.0 : 2.0, sr);
        double ph = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto t = (double) i / sr;
            const auto tn = t / len;
            double y = 0.0;
            switch (cat)
            {
                case 0: y = (i == 0 ? 1.0 : 0.0) + (ir == 1 && i == (int) (0.012 * sr) ? 0.7 : 0.0) + R.noise() * std::exp (-tn * 9.0) * 0.4; y = bp (y) * (ir == 2 || ir == 3 ? 3.0 : 1.0) + (ir == 0 ? (i == 0 ? 1.0 : 0.0) : 0.0); break;
                case 1:
                {
                    // résonances courtes : peigne (tube), boîte (modes), ressort (dispersion), tôle (métal)
                    const auto per = sr / (ir == 5 ? 180.0 : ir == 6 ? 95.0 : 330.0);
                    y = (std::fmod ((double) i, per) < 1.0 ? 1.0 : 0.0) * std::exp (-tn * 5.0) + R.noise() * 0.15 * std::exp (-tn * 7.0);
                    if (ir == 7) y = bp (R.noise()) * std::exp (-tn * 4.0) * 3.0;
                    break;
                }
                case 2: y = R.noise() * std::exp (-tn * 6.9) * (t < 0.002 ? t / 0.002 : 1.0) * (1.0 + (R.u() < 0.002 && t < 0.08 ? 4.0 : 0.0)); break;
                case 3:
                {
                    ph += keyHz * (1.0 + 0.002 * R.noise()) / sr;
                    const auto pos = std::fmod (t, 0.1) / 0.1;
                    y = (0.5 * std::sin (dsp::kTwoPi * ph) + R.noise() * (ir == 17 ? 0.9 : 0.4)) * std::exp (-tn * 4.0) * (1.0 - 0.6 * std::exp (-pos * 6.0));
                    break;
                }
                case 4:
                {
                    const auto f = keyHz * (1.0 + 2.0 * std::exp (-t / (ir == 20 ? 0.25 : 0.08)));
                    ph += f / sr;
                    y = std::sin (dsp::kTwoPi * ph) * std::exp (-tn * 5.0);
                    break;
                }
                case 5: y = (ir == 23 ? (R.u() < 0.01 ? R.noise() * 3.0 : 0.0) : R.noise() * (ir == 22 ? 0.6 : 1.0) * (0.5 + 0.5 * std::sin (t * 40.0 + side))) * std::exp (-tn * 4.0); break;
                default: y = bp (R.noise()) * std::exp (-tn * 5.0) * 4.0 + (i == 0 ? 0.5 : 0.0); break;
            }
            h[(size_t) i] = (float) lp (y);
        }
        double e = 0.0;
        for (auto v : h) e += (double) v * v;
        const auto g = 1.0 / std::sqrt (e + 1.0e-12) * 0.7;
        for (auto& v : h) v = (float) (v * g);
        return h;
    }
    /** Intensité d'un coup, en LUFS équivalents d'une boucle de 4 temps à 140 BPM : énergie
        pondérée K (norme BS.1770, deux canaux) sur les 0,43 s qui suivent le coup. Un kick plus
        court compte son silence, comme dans une boucle ; un kick plus long n'est compté que sur
        son premier temps. */
    inline double hitLoudness (const Buf& L, const Buf& R, double sr)
    {
        struct Bq { double b0, b1, b2, a1, a2, z1 = 0, z2 = 0; double run (double x) { const auto y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
        auto design = [sr]
        {
            std::array<Bq, 2> st {};
            {
                const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
                const auto K = std::tan (dsp::kPi * f0 / sr), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416), a0 = 1.0 + K / Q + K * K;
                st[0] = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
            }
            {
                const double f0 = 38.13547087602444, Q = 0.5003270373238773;
                const auto K = std::tan (dsp::kPi * f0 / sr), a0 = 1.0 + K / Q + K * K;
                st[1] = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
            }
            return st;
        };
        auto kl = design(), kr = design();
        const auto win = (int) (sr * 60.0 / 140.0);
        double e = 0.0;
        for (int i = 0; i < win; ++i)
        {
            const double l = i < (int) L.size() ? L[(size_t) i] : 0.0, r = i < (int) R.size() ? R[(size_t) i] : 0.0;
            const auto a = kl[1].run (kl[0].run (l)), b = kr[1].run (kr[0].run (r));
            e += a * a + b * b;
        }
        return -0.691 + 10.0 * std::log10 (e / win + 1.0e-20);
    }

    /** Limiteur de crête à anticipation, hors temps réel : la crête reste sous ceiling, sans
        distorsion (gain lissé : maintien du minimum sur ±look, moyenne glissante, relâchement
        exponentiel). Rend la réduction appliquée (dB, ≤ 0) dans gr (la plus forte des deux). */
    inline void peakLimit (Buf& L, Buf& R, double ceiling, double sr, Buf& gr)
    {
        const auto n = (int) L.size();
        if (n == 0) return;
        const auto look = std::max (1, (int) (0.0015 * sr));
        std::vector<double> a ((size_t) n), b ((size_t) n), g ((size_t) n);
        bool any = false;
        for (int i = 0; i < n; ++i)
        {
            const auto pk = std::max (std::abs ((double) L[(size_t) i]), std::abs ((double) R[(size_t) i]));
            a[(size_t) i] = pk > ceiling ? ceiling / pk : 1.0;
            any = any || pk > ceiling;
        }
        if (! any) return;
        // minimum glissant sur [i - look, i + look] (file monotone)
        {
            std::vector<int> q; q.reserve ((size_t) n);
            size_t head = 0;
            int next = 0;
            for (int i = 0; i < n; ++i)
            {
                for (; next <= std::min (n - 1, i + look); ++next)
                {
                    while (q.size() > head && a[(size_t) q.back()] >= a[(size_t) next]) q.pop_back();
                    q.push_back (next);
                }
                while (q[head] < i - look) ++head;
                b[(size_t) i] = a[(size_t) q[head]];
            }
        }
        // moyenne glissante sur [i - look/2, i + look/2] : tous les termes ≤ a[i] (gain sûr)
        {
            const auto h = std::max (1, look / 2);
            std::vector<double> pre ((size_t) n + 1, 0.0);
            for (int i = 0; i < n; ++i) pre[(size_t) i + 1] = pre[(size_t) i] + b[(size_t) i];
            for (int i = 0; i < n; ++i)
            {
                const auto lo = std::max (0, i - h), hi = std::min (n - 1, i + h);
                g[(size_t) i] = (pre[(size_t) hi + 1] - pre[(size_t) lo]) / (hi - lo + 1);
            }
        }
        // relâchement : remonte doucement (40 ms) sans jamais dépasser le gain sûr
        const auto kRel = 1.0 - std::exp (-1.0 / (0.04 * sr));
        double cur = g[0];
        for (int i = 0; i < n; ++i)
        {
            cur = std::min (g[(size_t) i], cur + (1.0 - cur) * kRel);
            L[(size_t) i] = (float) (L[(size_t) i] * cur); R[(size_t) i] = (float) (R[(size_t) i] * cur);
            if (i < (int) gr.size()) gr[(size_t) i] = (float) std::min ((double) gr[(size_t) i], 20.0 * std::log10 (std::max (1.0e-6, cur)));
        }
    }

    /** Grave d'un signal (sous fc, transition douce jusqu'à 2·fc) et sa transformée de Hilbert
        (même grave tourné de 90°), par une FFT (hors temps réel). */
    inline void lowBand (const Buf& x, double fc, double sr, Buf& lo, Buf& hb)
    {
        const auto n = (int) x.size();
        int order = 1;
        while ((1 << order) < 2 * n) ++order;
        auto& fft = fftFor (order);
        const auto N = fft.size();
        std::vector<std::complex<float>> X ((size_t) N);
        for (int i = 0; i < n; ++i) X[(size_t) i] = { x[(size_t) i], 0.0f };
        fft.forward (X.data());
        // grave (G) et sa transformée de Hilbert (H = -j·signe(f)·G), tous deux réels : une seule
        // transformée inverse de G + j·H donne le grave en partie réelle et Hilbert en partie imaginaire
        for (int k = 0; k < N; ++k)
        {
            const auto f = (double) std::min (k, N - k) * sr / N;
            const auto m = f <= fc ? 1.0 : f >= 2.0 * fc ? 0.0 : 0.5 + 0.5 * std::cos (dsp::kPi * (f - fc) / fc);
            const auto G = X[(size_t) k] * (float) m;
            // Hilbert : -j sur les fréquences positives, +j sur les négatives, 0 au continu et à Nyquist
            const std::complex<float> H = (k == 0 || k == N / 2) ? std::complex<float> (0.0f, 0.0f)
                                        : k < N / 2 ? G * std::complex<float> (0.0f, -1.0f) : G * std::complex<float> (0.0f, 1.0f);
            X[(size_t) k] = G + std::complex<float> (0.0f, 1.0f) * H;
        }
        fft.inverse (X.data());
        lo.assign ((size_t) n, 0.0f); hb.assign ((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i) { lo[(size_t) i] = X[(size_t) i].real(); hb[(size_t) i] = X[(size_t) i].imag(); }
    }
} // namespace detail

//==============================================================================
/** Ce que le calcul rend : le kick (stéréo) et les vues de l'afficheur. */
struct Result
{
    std::vector<float> L, R;
    int length = 0;
    struct LayerView { bool on = false; int fam = 0, part = 0; std::string name; int start = 0, len = 0; std::vector<float> wave; };
    std::array<LayerView, numLayers> layers;
    std::vector<float> sum, post, gr; // somme avant effets, après effets, réduction du finaliseur (dB, ≤ 0)
    double peakDb = -120.0, keyHz = 43.65, bpm = 120.0, gain = 1.0, loudness = -100.0;
    std::array<double, numLayers> detectedHz {}; // tonalité détectée des samples
};

/** Mémoire du fil de calcul (1.2, régularité sous LFO) : les morceaux de la bibliothèque et les couches
    déjà traitées (avec leur grave pour l'alignement de phase) sont gardés ; un nouveau calcul ne refait
    que ce qui a changé. Le son est identique, au bit près, à un calcul sans mémoire. Un seul fil à la fois. */
struct RenderCache
{
    struct Part { std::uint64_t key = 0; std::vector<float> x; unsigned tick = 0; };
    struct Layer
    {
        std::uint64_t key = 0; unsigned tick = 0;
        std::vector<float> wave; int start = 0; std::string name; double detected = 0.0;
        bool lowDone = false; std::vector<float> lo, hb; double eLo = 0.0, eAll = 0.0;
    };
    std::vector<Part> parts;
    std::vector<Layer> layers;
    unsigned tick = 0;
    static constexpr size_t maxParts = 48, maxLayers = 40;
    // capacité fixée d'avance : les adresses des entrées ne bougent pas pendant un calcul
    RenderCache() { parts.reserve (maxParts); layers.reserve (maxLayers); }

    template <typename T> static T* find (std::vector<T>& v, std::uint64_t key) { for (auto& e : v) if (e.key == key) return &e; return nullptr; }
    template <typename T> T& slot (std::vector<T>& v, size_t maxN, std::uint64_t key)
    {
        if (v.size() < maxN) { v.emplace_back(); v.back().key = key; return v.back(); }
        auto* old = &v[0];
        for (auto& e : v) if (e.tick < old->tick) old = &e;
        *old = T {}; old->key = key;
        return *old;
    }
};

struct Input
{
    std::vector<float> p;                                  // réglages (valeurs réelles)
    std::array<const engine::SampleBuffer*, numLayers> samples {};
    double bpm = 120.0, sr = 48000.0;
    RenderCache* cache = nullptr;                          // mémoire du fil de calcul (facultative)
};

namespace detail
{
    inline std::uint64_t hashMix (std::uint64_t h, std::uint64_t v) noexcept { h ^= v; h *= 1099511628211ull; return h; }
    inline std::uint64_t hashDouble (std::uint64_t h, double d) noexcept { std::uint64_t b; std::memcpy (&b, &d, 8); return hashMix (h, b); }
    inline std::uint64_t hashFloats (std::uint64_t h, const float* p, int n) noexcept { for (int i = 0; i < n; ++i) { std::uint32_t u; std::memcpy (&u, p + i, 4); h = hashMix (h, u); } return h; }
}

inline double keyToHz (int midi) noexcept { return 440.0 * std::pow (2.0, (midi - 69) / 12.0); }

/** Calcule le kick entier. */
inline Result render (const Input& in)
{
    using namespace detail;
    Result r;
    const auto& p = in.p;
    const auto sr = in.sr;
    if ((int) p.size() < numSlots() || sr <= 0.0) return r;
    const auto keyMidi = 24 + std::clamp ((int) std::lround (p[gKey]), 0, 23);
    r.keyHz = keyToHz (keyMidi);
    r.bpm = in.bpm;
    const auto maxN = (int) (maxSeconds * sr);
    std::vector<double> mixL ((size_t) maxN, 0.0), mixR ((size_t) maxN, 0.0);
    int used = 0;
    std::vector<std::pair<int, int>> layerSig; // couches audibles : (couche, début)

    bool anySolo = false;
    for (int l = 0; l < numLayers; ++l)
        if (p[(size_t) (l * numLayerParams + lOn)] > 0.5f && p[(size_t) (l * numLayerParams + lSolo)] > 0.5f) anySolo = true;

    auto* cache = in.cache;
    if (cache != nullptr) ++cache->tick;
    std::array<RenderCache::Layer*, numLayers> cached {};   // couches de ce calcul en mémoire

    for (int l = 0; l < numLayers; ++l)
    {
        const float* q = p.data() + l * numLayerParams;
        auto& view = r.layers[(size_t) l];
        if (q[lOn] < 0.5f) continue;
        const int fam = std::clamp ((int) std::lround (q[lFam]), 0, numFamilies - 1);
        const int part = std::max (0, (int) std::lround (q[lPart]));
        view.on = true; view.fam = fam; view.part = part;
        const bool audibleL = q[lMute] < 0.5f && (! anySolo || q[lSolo] > 0.5f);
        // couche déjà traitée avec ces réglages (coupure et solo exceptés), cette tonalité, ce tempo
        std::uint64_t lkey = 0;
        if (cache != nullptr)
        {
            float qq[numLayerParams];
            std::memcpy (qq, q, sizeof (qq));
            qq[lMute] = qq[lSolo] = 0.0f;
            lkey = hashFloats (1469598103934665603ull, qq, numLayerParams);
            lkey = hashDouble (hashDouble (hashDouble (lkey, r.keyHz), in.bpm), sr);
            lkey = hashMix (lkey, (std::uint64_t) (std::uintptr_t) (fam == fSample ? in.samples[(size_t) l] : nullptr));
            lkey = hashMix (lkey, (std::uint64_t) l + 1);
            if (auto* e = RenderCache::find (cache->layers, lkey))
            {
                e->tick = cache->tick;
                view.name = e->name; view.start = e->start; view.len = (int) e->wave.size(); view.wave = e->wave;
                r.detectedHz[(size_t) l] = e->detected;
                cached[(size_t) l] = e;
                if (audibleL) layerSig.push_back ({ l, e->start });
                continue;
            }
        }
        // 1. source
        Buf x;
        if (fam == fSample)
        {
            const auto* s = in.samples[(size_t) l];
            if (s != nullptr && ! s->data.empty() && ! s->firmware)
            {
                const auto n = std::min ((int) s->data.size(), (int) (maxSeconds * s->sampleRate));
                Buf mono ((size_t) n);
                for (int i = 0; i < n; ++i) mono[(size_t) i] = s->right.empty() ? s->data[(size_t) i] : 0.5f * (s->data[(size_t) i] + s->right[(size_t) i]);
                x = resample (mono, s->sampleRate / sr);
                view.name = s->name;
                // tonalité détectée : la plus forte période dans le premier tiers après l'attaque
                const auto a = std::min ((int) x.size() - 1, (int) (0.03 * sr));
                const auto len = std::min ((int) x.size() - a, (int) (0.25 * sr));
                const auto f0 = len > 512 ? kickPitch (x.data() + a, len, sr) : 0.0;
                r.detectedHz[(size_t) l] = f0;
                if (q[lKeyTrack] > 0.5f && f0 > 20.0)
                {
                    // ramène le sample sur la tonalité du kick (l'octave la plus proche)
                    auto d = 12.0 * std::log2 (r.keyHz / f0);
                    d -= 12.0 * std::round (d / 12.0);
                    x = resample (x, std::pow (2.0, d / 12.0));
                }
            }
            else view.name = "(glissez un sample)";
        }
        else
        {
            if (cache != nullptr)
            {
                auto pk = hashMix (hashMix (1469598103934665603ull, (std::uint64_t) fam), (std::uint64_t) part);
                pk = hashDouble (hashDouble (hashDouble (pk, r.keyHz), in.bpm), sr);
                if (auto* e = RenderCache::find (cache->parts, pk)) { e->tick = cache->tick; x = e->x; }
                else { x = renderPart (fam, part, r.keyHz, in.bpm, sr); auto& n = cache->slot (cache->parts, RenderCache::maxParts, pk); n.x = x; n.tick = cache->tick; }
            }
            else x = renderPart (fam, part, r.keyHz, in.bpm, sr);
            view.name = partName (fam, part);
        }
        if (x.empty()) continue;
        // 2. inversion, hauteur, étirement
        if (q[lReverse] > 0.5f) std::reverse (x.begin(), x.end());
        const auto semis = (double) q[lPitch] + q[lFine] / 100.0;
        if (std::abs (semis) > 1.0e-3) x = resample (x, std::pow (2.0, semis / 12.0));
        x = stretch (x, std::clamp ((double) q[lStretch], 0.25, 4.0), sr);
        // 3. longueur et fondus
        const auto lenN = std::min ((int) x.size(), std::max (16, (int) (q[lLen] * 0.001 * sr)));
        x.resize ((size_t) lenN);
        const auto fi = (int) (std::max (0.0f, q[lFiLen]) * 0.001 * sr), fo = (int) (std::max (0.0f, q[lFoLen]) * 0.001 * sr);
        for (int i = 0; i < lenN; ++i)
        {
            double g = 1.0;
            if (fi > 0 && i < fi) g *= fadeShape ((double) i / fi, q[lFiCurve] / 100.0);
            if (fo > 0 && i >= lenN - fo) g *= fadeShape ((double) (lenN - i) / fo, q[lFoCurve] / 100.0);
            x[(size_t) i] = (float) (x[(size_t) i] * g);
        }
        // 4. tranche de la couche : égaliseur, transitoires, distorsion, puis filtres
        {
            dsp::SvfEq hp1, hp2, lp1, lp2, lo, mid, hi;
            const auto hpF = (double) q[lHp], lpF = (double) q[lLp];
            const bool useHp = hpF > 21.0, useLp = lpF < 19900.0;
            if (useHp) { hp1.set (dsp::SvfEq::highpass, hpF, 0.54, 0.0, sr); hp2.set (dsp::SvfEq::highpass, hpF, 1.31, 0.0, sr); }
            if (useLp) { lp1.set (dsp::SvfEq::lowpass, lpF, 0.54, 0.0, sr); lp2.set (dsp::SvfEq::lowpass, lpF, 1.31, 0.0, sr); }
            const bool eqOn = std::abs (q[lEqLo]) > 0.05f || std::abs (q[lEqMidG]) > 0.05f || std::abs (q[lEqHi]) > 0.05f;
            lo.set (dsp::SvfEq::lowShelf, 100.0, 0.7, q[lEqLo], sr);
            mid.set (dsp::SvfEq::bell, std::clamp ((double) q[lEqMidF], 40.0, 16000.0), 1.0, q[lEqMidG], sr);
            hi.set (dsp::SvfEq::highShelf, 6000.0, 0.7, q[lEqHi], sr);
            if (eqOn) for (auto& s : x) s = (float) hi.process (mid.process (lo.process ((double) s)));
            transient (x.data(), (int) x.size(), sr, q[lTrAtk] / 100.0, q[lTrSus] < 0.0f ? -q[lTrSus] / 100.0 : 0.0, 0.02);
            if (q[lTrSus] > 0.0f) transient (x.data(), (int) x.size(), sr, -q[lTrSus] / 200.0, 0.0, 0.03); // maintien relevé = attaque adoucie
            if (q[lDrv] > 0.05f && q[lDrvMix] > 0.5f)
            {
                gd2::Gd2Dist d; d.prepare (sr);
                Buf r2 (x);
                d.process (x.data(), r2.data(), (int) x.size(), std::clamp ((int) std::lround (q[lDrvType]), 0, 9), q[lDrv], 0, 1000.0, q[lDrvMix] / 100.0);
            }
            // 1.2 : passe-haut et passe-bas EN DERNIER (après la distorsion, qui recréait les aigus
            // qu'un passe-bas placé avant avait coupés : le réglage semblait sans effet)
            if (useHp || useLp)
                for (auto& s : x)
                {
                    double y = s;
                    if (useHp) y = hp2.process (hp1.process (y));
                    if (useLp) y = lp2.process (lp1.process (y));
                    s = (float) y;
                }
        }
        // 5. polarité, volume, décalage
        const auto g = (q[lVol] <= -47.9f ? 0.0 : dsp::dbToGain (q[lVol])) * (q[lPhase] > 0.5f ? -1.0 : 1.0);
        const auto start = (int) std::lround (q[lOffset] * 0.001 * sr);
        view.start = start; view.len = (int) x.size();
        const bool audible = q[lMute] < 0.5f && (! anySolo || q[lSolo] > 0.5f);
        // vue de la couche : la couche telle qu'elle sonne (avant les effets du rack)
        view.wave.assign (x.size(), 0.0f);
        for (size_t i = 0; i < x.size(); ++i) view.wave[i] = (float) (x[i] * g);
        if (audible) layerSig.push_back ({ l, start });
        if (cache != nullptr)
        {
            auto& e = cache->slot (cache->layers, RenderCache::maxLayers, lkey);
            e.tick = cache->tick; e.wave = view.wave; e.start = start; e.name = view.name; e.detected = r.detectedHz[(size_t) l];
            cached[(size_t) l] = &e;
        }
    }

    // alignement de phase : la couche au grave le plus fort sert de référence ; chaque autre
    // couche tourne son grave (sous 160 Hz) de l'angle qui l'ajoute le mieux au grave déjà là.
    // L'attaque de la couche (ses 4 premières ms) reste intacte : la rotation entre en 16 ms.
    if (p[(size_t) phaseAlignSlot()] > 0.5f && layerSig.size() > 1)
    {
        struct Low { int idx; Buf lo, hb; double e; };
        std::vector<Low> lows;
        for (size_t k = 0; k < layerSig.size(); ++k)
        {
            const auto layer = layerSig[k].first;
            const auto& w = r.layers[(size_t) layer].wave;
            Low a; a.idx = (int) k;
            double eLo = 0.0, eAll = 0.0;
            auto* ce = cached[(size_t) layer];
            if (ce != nullptr && ce->lowDone) { a.lo = ce->lo; a.hb = ce->hb; eLo = ce->eLo; eAll = ce->eAll; }
            else
            {
                lowBand (w, 160.0, sr, a.lo, a.hb);
                for (size_t i = 0; i < w.size(); ++i) { eLo += (double) a.lo[i] * a.lo[i]; eAll += (double) w[i] * w[i]; }
                if (ce != nullptr) { ce->lo = a.lo; ce->hb = a.hb; ce->eLo = eLo; ce->eAll = eAll; ce->lowDone = true; }
            }
            a.e = eLo;
            if (eAll > 1.0e-12 && eLo > 0.1 * eAll) lows.push_back (std::move (a)); // couches sans grave : rien à aligner
        }
        std::sort (lows.begin(), lows.end(), [] (const Low& a, const Low& b) { return a.e > b.e; });
        std::vector<double> S ((size_t) maxN, 0.0);
        const auto ramp0 = (int) (0.004 * sr), ramp1 = (int) (0.020 * sr), span = (int) (0.4 * sr);
        auto weight = [&] (int i) { return i <= ramp0 ? 0.0 : i >= ramp1 ? 1.0 : 0.5 - 0.5 * std::cos (dsp::kPi * (i - ramp0) / (double) (ramp1 - ramp0)); };
        for (size_t o = 0; o < lows.size(); ++o)
        {
            auto& a = lows[o];
            const auto [layer, start] = layerSig[(size_t) a.idx];
            auto& w = r.layers[(size_t) layer].wave;
            double c = 1.0, sn = 0.0;
            if (o > 0 && a.e > 1.0e-4 * lows[0].e)
            {
                double pa = 0.0, pb = 0.0;
                for (int i = 0; i < std::min ((int) w.size(), span); ++i)
                {
                    const auto t = start + i;
                    if (t < 0 || t >= maxN) continue;
                    const auto wt = weight (i);
                    pa += wt * S[(size_t) t] * a.lo[(size_t) i]; pb += wt * S[(size_t) t] * a.hb[(size_t) i];
                }
                const auto th = std::atan2 (pb, pa);
                if (std::abs (th) > 0.15) { c = std::cos (th); sn = std::sin (th); }
            }
            for (int i = 0; i < (int) w.size(); ++i)
            {
                const auto wt = weight (i);
                const auto rot = wt * ((c - 1.0) * a.lo[(size_t) i] + sn * a.hb[(size_t) i]);
                w[(size_t) i] = (float) (w[(size_t) i] + rot);
                const auto t = start + i;
                if (t >= 0 && t < maxN) S[(size_t) t] += a.lo[(size_t) i] + rot;
            }
        }
    }
    for (const auto& [layer, start] : layerSig)
    {
        const auto& w = r.layers[(size_t) layer].wave;
        for (int i = 0; i < (int) w.size(); ++i)
        {
            const auto t = start + i;
            if (t < 0 || t >= maxN) continue;
            mixL[(size_t) t] += w[(size_t) i]; mixR[(size_t) t] += w[(size_t) i];
            used = std::max (used, t + 1);
        }
    }
    used = std::max (used, (int) (0.05 * sr));
    // longueur : la fin du dernier son audible (et les queues du rack)
    Buf L ((size_t) used), R ((size_t) used);
    for (int i = 0; i < used; ++i) { L[(size_t) i] = (float) mixL[(size_t) i]; R[(size_t) i] = (float) mixR[(size_t) i]; }
    r.sum = L;
    Buf gr ((size_t) used, 0.0f);

    // rack : modules allumés, dans l'ordre de leur rang
    std::vector<std::pair<int, int>> order;
    for (int m = 0; m < numModules; ++m)
        if (p[(size_t) moduleBase()[(size_t) m]] > 0.5f) order.push_back ({ (int) std::lround (p[(size_t) moduleBase()[(size_t) m] + 1]) * 16 + m, m });
    std::sort (order.begin(), order.end());
    auto P = [&] (int m, const char* id) { return (double) p[(size_t) modParam (m, id)]; };
    auto mixBack = [] (Buf& a, const Buf& dry, double mix) { if (mix >= 0.999) return; for (size_t i = 0; i < a.size() && i < dry.size(); ++i) a[i] = (float) (dry[i] + (a[i] - dry[i]) * mix); };
    for (const auto& [rank, m] : order)
    {
        (void) rank;
        const Buf dryL = L, dryR = R;
        const auto mix = std::clamp (P (m, "mix") / 100.0, 0.0, 1.0);
        switch (m)
        {
            case mEq:
            {
                std::array<dsp::SvfEqStereo, 4> e;
                e[0].set (dsp::SvfEq::lowShelf, P (m, "lsf"), 0.7, P (m, "lsg"), sr);
                e[1].set (dsp::SvfEq::bell, P (m, "b1f"), P (m, "b1q"), P (m, "b1g"), sr);
                e[2].set (dsp::SvfEq::bell, P (m, "b2f"), P (m, "b2q"), P (m, "b2g"), sr);
                e[3].set (dsp::SvfEq::highShelf, P (m, "hsf"), 0.7, P (m, "hsg"), sr);
                for (size_t i = 0; i < L.size(); ++i)
                {
                    double l = L[i], rr = R[i];
                    for (auto& f : e) { l = f.l.process (l); rr = f.r.process (rr); }
                    L[i] = (float) l; R[i] = (float) rr;
                }
                break;
            }
            case mMs:
            {
                dsp::SvfEq lo, hi, shp1, shp2;
                lo.set (dsp::SvfEq::lowShelf, 120.0, 0.7, P (m, "midlo"), sr);
                hi.set (dsp::SvfEq::highShelf, 3000.0, 0.7, P (m, "midhi"), sr);
                shp1.set (dsp::SvfEq::highpass, P (m, "sidehp"), 0.54, 0.0, sr);
                shp2.set (dsp::SvfEq::highpass, P (m, "sidehp"), 1.31, 0.0, sr);
                const auto sg = dsp::dbToGain (P (m, "sideg")) * P (m, "width") / 100.0;
                for (size_t i = 0; i < L.size(); ++i)
                {
                    auto mm = 0.5 * (L[i] + R[i]), s = 0.5 * (L[i] - R[i]);
                    mm = hi.process (lo.process (mm));
                    s = shp2.process (shp1.process (s)) * sg;
                    L[i] = (float) (mm + s); R[i] = (float) (mm - s);
                }
                break;
            }
            case mDist:
            {
                gd2::Gd2Dist d; d.prepare (sr);
                d.process (L.data(), R.data(), (int) L.size(), std::clamp ((int) std::lround (P (m, "type")), 0, 9), P (m, "drive"),
                           std::clamp ((int) std::lround (P (m, "pre")), 0, 3), P (m, "pref"), 1.0);
                if (P (m, "tone") < 19900.0)
                {
                    dsp::SvfEqStereo t; t.set (dsp::SvfEq::lowpass, P (m, "tone"), 0.707, 0.0, sr);
                    for (size_t i = 0; i < L.size(); ++i) { L[i] = (float) t.l.process (L[i]); R[i] = (float) t.r.process (R[i]); }
                }
                break;
            }
            case mShaper:
            {
                // courbe de gain en 8 points répartis sur la longueur du kick
                const auto amt = std::clamp (P (m, "amount") / 100.0, 0.0, 1.0);
                const char* ids[8] { "n1", "n2", "n3", "n4", "n5", "n6", "n7", "n8" };
                std::array<double, 8> nodes {};
                for (int k = 0; k < 8; ++k) nodes[(size_t) k] = std::clamp (P (m, ids[k]) / 100.0, 0.0, 1.0);
                const auto n = (int) L.size();
                for (int i = 0; i < n; ++i)
                {
                    const auto pos = (double) i / std::max (1, n - 1) * 7.0;
                    const auto k = std::min (6, (int) pos);
                    const auto f = pos - k;
                    const auto c = nodes[(size_t) k] + (nodes[(size_t) k + 1] - nodes[(size_t) k]) * (0.5 - 0.5 * std::cos (dsp::kPi * f));
                    const auto g = 1.0 + (c - 1.0) * amt;
                    L[(size_t) i] = (float) (L[(size_t) i] * g); R[(size_t) i] = (float) (R[(size_t) i] * g);
                }
                break;
            }
            case mTrans:
            {
                static const double div[4] { 1.0 / 16.0, 1.0 / 32.0, 1.0 / 48.0, 1.0 / 64.0 };
                const auto timeS = 4.0 * 60.0 / in.bpm * div[std::clamp ((int) std::lround (P (m, "speed")), 0, 3)];
                transient (L.data(), (int) L.size(), sr, P (m, "attack") / 100.0, P (m, "duck") / 100.0, timeS);
                transient (R.data(), (int) R.size(), sr, P (m, "attack") / 100.0, P (m, "duck") / 100.0, timeS);
                break;
            }
            case mConv:
            {
                const int ir = std::clamp ((int) std::lround (P (m, "ir")), 0, (int) irList().size() - 1);
                auto h0 = makeIr (ir, 0, r.keyHz, sr), h1 = makeIr (ir, 1, r.keyHz, sr);
                const auto speed = std::pow (2.0, P (m, "speed") / 100.0); // -100..100 % : une octave de chaque côté
                h0 = resample (h0, speed); h1 = resample (h1, speed);
                // courbe du mouillé dans le temps (3 points : départ, milieu réglable, fin)
                auto curve = [&] (Buf& h)
                {
                    const auto n = (int) h.size();
                    const auto c1 = P (m, "c1") / 100.0, c2 = P (m, "c2") / 100.0, c3 = P (m, "c3") / 100.0, t2 = std::clamp (P (m, "c2t") / 100.0, 0.01, 0.99);
                    for (int i = 0; i < n; ++i)
                    {
                        const auto x = (double) i / std::max (1, n - 1);
                        const auto g = x < t2 ? c1 + (c2 - c1) * x / t2 : c2 + (c3 - c2) * (x - t2) / (1.0 - t2);
                        h[(size_t) i] = (float) (h[(size_t) i] * g);
                    }
                };
                curve (h0); curve (h1);
                const auto st = std::clamp (P (m, "stereo") / 100.0, 0.0, 1.0);
                const auto monoIn = [&] { Buf mono (L.size()); for (size_t i = 0; i < L.size(); ++i) mono[i] = 0.5f * (L[i] + R[i]); return mono; }();
                auto [wl, wr] = convolve2 (monoIn, h0, h1);
                // ton : -100 sombre … +100 brillant
                const auto tone = P (m, "tone");
                dsp::SvfEq tl, tr;
                tl.set (tone < 0.0 ? dsp::SvfEq::lowpass : dsp::SvfEq::highShelf, tone < 0.0 ? 18000.0 * std::pow (0.05, -tone / 100.0) : 3000.0, 0.7, tone > 0.0 ? tone * 0.12 : 0.0, sr);
                tr = tl;
                const auto wet = P (m, "wet") / 100.0;
                const auto n = std::min ((int) wl.size(), maxN);
                if (n > (int) L.size()) { L.resize ((size_t) n, 0.0f); R.resize ((size_t) n, 0.0f); }
                Buf dl = L, dr = R;
                for (int i = 0; i < n; ++i)
                {
                    const auto a = tl.process (wl[(size_t) i]), b = tr.process (wr[(size_t) i]);
                    const auto mid = 0.5 * (a + b), side = 0.5 * (a - b) * st;
                    L[(size_t) i] = (float) (dl[(size_t) i] + wet * (mid + side));
                    R[(size_t) i] = (float) (dr[(size_t) i] + wet * (mid - side));
                }
                gr.resize (L.size(), 0.0f);
                break;
            }
            case mFinal:
            {
                // grave relevé, grave en mono sous la fréquence choisie, écrêtage doux
                dsp::SvfEqStereo b; b.set (dsp::SvfEq::lowShelf, P (m, "boostf"), 0.7, P (m, "boost"), sr);
                dsp::SvfEq s1, s2; s1.set (dsp::SvfEq::highpass, P (m, "mono"), 0.54, 0.0, sr); s2.set (dsp::SvfEq::highpass, P (m, "mono"), 1.31, 0.0, sr);
                const auto drive = 1.0 + 3.0 * std::clamp (P (m, "clip") / 100.0, 0.0, 1.0);
                for (size_t i = 0; i < L.size(); ++i)
                {
                    auto l = b.l.process (L[i]), rr = b.r.process (R[i]);
                    const auto mm = 0.5 * (l + rr), s = s2.process (s1.process (0.5 * (l - rr)));
                    l = mm + s; rr = mm - s;
                    const auto pre = std::max (std::abs (l), std::abs (rr)) * drive;
                    auto clipOne = [] (double x) { const auto a = std::abs (x); return a < 0.7 ? x : std::copysign (0.7 + 0.3 * std::tanh ((a - 0.7) / 0.3), x); };
                    l = clipOne (l * drive); rr = clipOne (rr * drive);
                    const auto post = std::max (std::abs (l), std::abs (rr));
                    if (i < gr.size() && pre > 1.0e-6) gr[i] = (float) std::min ((double) gr[i], 20.0 * std::log10 (std::max (1.0e-6, post / pre)));
                    L[i] = (float) l; R[i] = (float) rr;
                }
                break;
            }
            case mMulti:
            {
                gd2::Gd2Comp c; c.prepare (sr);
                const double bands[3] { P (m, "low"), P (m, "mid"), P (m, "high") };
                c.process (L.data(), R.data(), (int) L.size(), true, P (m, "depth") / 100.0, P (m, "time") / 100.0, P (m, "up") / 100.0, P (m, "down") / 100.0,
                           0.0, 0.0, bands, -12.0, 4.0);
                break;
            }
            case mFilt:
            {
                dsp::SvfEqStereo h1, h2, l1, l2;
                const auto hq = 0.707 + P (m, "hpres") / 100.0 * 6.0, lq = 0.707 + P (m, "lpres") / 100.0 * 6.0;
                h1.set (dsp::SvfEq::highpass, P (m, "hp"), 0.54, 0.0, sr); h2.set (dsp::SvfEq::highpass, P (m, "hp"), hq, 0.0, sr);
                l1.set (dsp::SvfEq::lowpass, P (m, "lp"), 0.54, 0.0, sr); l2.set (dsp::SvfEq::lowpass, P (m, "lp"), lq, 0.0, sr);
                const bool hp = P (m, "hp") > 21.0, lp = P (m, "lp") < 19900.0;
                for (size_t i = 0; i < L.size(); ++i)
                {
                    double l = L[i], rr = R[i];
                    if (hp) { l = h2.l.process (h1.l.process (l)); rr = h2.r.process (h1.r.process (rr)); }
                    if (lp) { l = l2.l.process (l1.l.process (l)); rr = l2.r.process (l1.r.process (rr)); }
                    L[i] = (float) l; R[i] = (float) rr;
                }
                break;
            }
            default: break;
        }
        // les modules qui allongent (convolution) : le direct garde sa longueur, complété par du silence
        Buf dl2 = dryL, dr2 = dryR;
        dl2.resize (L.size(), 0.0f); dr2.resize (R.size(), 0.0f);
        mixBack (L, dl2, mix); mixBack (R, dr2, mix);
    }
    // fin du son : coupe le silence final (sous -80 dB), petit fondu
    int end = (int) L.size();
    while (end > (int) (0.02 * sr) && std::max (std::abs (L[(size_t) end - 1]), std::abs (R[(size_t) end - 1])) < 1.0e-4f) --end;
    end = std::min ((int) L.size(), end + (int) (0.002 * sr));
    L.resize ((size_t) end); R.resize ((size_t) end); gr.resize ((size_t) end, 0.0f);
    for (int i = 0; i < std::min (end, (int) (0.003 * sr)); ++i)
    {
        const auto g = (float) i / (float) (0.003 * sr);
        L[(size_t) (end - 1 - i)] *= g; R[(size_t) (end - 1 - i)] *= g;
    }
    // gain automatique à l'intensité : le kick sonne comme les grosses caisses des batteries de
    // GOLEM (cible -15 LUFS en boucle de 4 temps), puis le limiteur garde la crête sous -1 dBFS
    // (6 dB de réduction au plus : au-delà, c'est le gain qui baisse, pour garder l'attaque)
    double pk = 0.0;
    for (int i = 0; i < end; ++i) pk = std::max ({ pk, (double) std::abs (L[(size_t) i]), (double) std::abs (R[(size_t) i]) });
    r.peakDb = 20.0 * std::log10 (pk + 1.0e-12);
    r.loudness = hitLoudness (L, R, sr);
    r.gain = 1.0;
    if (p[gAutogain] > 0.5f && pk > 1.0e-6)
    {
        constexpr double target = -15.0, ceiling = 0.8913, maxGr = 0.5; // -1 dBFS, -6 dB
        auto g = dsp::dbToGain (std::clamp (target - r.loudness, -40.0, 40.0));
        if (pk * g * maxGr > ceiling) g = ceiling / (pk * maxGr);
        r.gain = g;
        for (int i = 0; i < end; ++i) { L[(size_t) i] = (float) (L[(size_t) i] * g); R[(size_t) i] = (float) (R[(size_t) i] * g); }
        peakLimit (L, R, ceiling, sr, gr);
    }
    r.post = L;
    r.gr = gr;
    r.L = std::move (L); r.R = std::move (R);
    r.length = end;
    return r;
}

} // namespace grv::instruments::kf
