/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Mesures du bus principal (recommandation ITU-R BS.1770-4 / EBU R 128) :
      - intensité perçue : pondération K (filtre en plateau + passe-haut « RLB »), énergie moyenne
        par tranches de 100 ms ; l'interface en déduit l'intensité instantanée (400 ms), courte
        (3 s) et intégrée (fenêtres de 400 ms recouvrantes, seuils -70 LUFS puis -10 LU) ;
      - crête vraie : suréchantillonnage ×4 par filtre polyphase (sinus cardinal fenêtré, 48 points) ;
      - niveau efficace (RMS) sans pondération.
    Fil audio : process() n'alloue pas. Les tranches de 100 ms partent dans une file sans verrou.
*/

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>

namespace grv::dsp
{

struct LoudnessBlock
{
    float kMeanSquare = 0.0f;   // énergie pondérée K (somme des deux canaux), 100 ms
    float meanSquare = 0.0f;    // énergie non pondérée (moyenne des deux canaux), 100 ms
    float truePeak = 0.0f;      // crête vraie de la tranche (linéaire)
};

class LoudnessMeter
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        designKWeighting();
        blockLength = std::max (1, (int) std::lround (sr * 0.1));
        // Filtre polyphase ×4 : sinus cardinal fenêtré (Kaiser β = 8), coupure à la fréquence de Nyquist d'origine
        constexpr double pi = 3.14159265358979323846;
        for (int k = 0; k < taps * phases; ++k)
        {
            const auto t = (double) (k - (taps * phases - 1) / 2.0) / phases;
            const auto sinc = std::abs (t) < 1.0e-9 ? 1.0 : std::sin (pi * t) / (pi * t);
            const auto x = (2.0 * k) / (taps * phases - 1) - 1.0;
            const auto w = bessel0 (8.0 * std::sqrt (std::max (0.0, 1.0 - x * x))) / bessel0 (8.0);
            coef[(size_t) (k % phases)][(size_t) (k / phases)] = (float) (sinc * w);
        }
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : state) s = {};
        history.fill (0.0f);
        histPos = 0; count = 0; kSum = 0.0; sum = 0.0; peak = 0.0f;
    }

    void process (const float* left, const float* right, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const double l = left[i], r = right[i];
            const auto kl = biquad (state[0], stage1, biquad (state[1], stage0, l));
            const auto kr = biquad (state[2], stage1, biquad (state[3], stage0, r));
            kSum += kl * kl + kr * kr;
            sum += 0.5 * (l * l + r * r);

            // crête vraie : 4 points interpolés entre chaque échantillon, sur les deux canaux
            history[(size_t) histPos] = (float) l;
            history[(size_t) (histPos + taps)] = (float) l;
            historyR[(size_t) histPos] = (float) r;
            historyR[(size_t) (histPos + taps)] = (float) r;
            histPos = (histPos + 1) % taps;
            for (int ph = 0; ph < phases; ++ph)
            {
                float yl = 0.0f, yr = 0.0f;
                const auto& c = coef[(size_t) ph];
                for (int k = 0; k < taps; ++k)
                {
                    yl += c[(size_t) k] * history[(size_t) (histPos + k)];
                    yr += c[(size_t) k] * historyR[(size_t) (histPos + k)];
                }
                peak = std::max (peak, std::max (std::abs (yl), std::abs (yr)));
            }

            if (++count >= blockLength)
            {
                blocks.push ({ (float) (kSum / count), (float) (sum / count), peak });
                count = 0; kSum = 0.0; sum = 0.0; peak = 0.0f;
            }
        }
    }

    /** Fil de l'interface : tranches de 100 ms arrivées depuis le dernier appel. */
    bool pop (LoudnessBlock& b) noexcept { return blocks.pop (b); }

private:
    struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State { double z1 = 0, z2 = 0; };

    static double biquad (State& s, const Coefs& c, double x) noexcept
    {
        const auto y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    static double bessel0 (double x) noexcept
    {
        double sumv = 1.0, term = 1.0;
        for (int k = 1; k < 25; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sumv += term; }
        return sumv;
    }

    /** Coefficients de la pondération K pour n'importe quelle fréquence d'échantillonnage
        (prototypes analogiques de la norme, transformation bilinéaire). */
    void designKWeighting() noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const auto K = std::tan (pi * f0 / sr);
            const auto Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const auto a0 = 1.0 + K / Q + K * K;
            stage0 = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                       2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const auto K = std::tan (pi * f0 / sr);
            const auto a0 = 1.0 + K / Q + K * K;
            stage1 = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
    }

    template <typename T, size_t N>
    struct Fifo
    {
        bool push (const T& v) noexcept
        {
            const auto w = write.load (std::memory_order_relaxed), next = (w + 1) % N;
            if (next == read.load (std::memory_order_acquire)) return false;
            buf[w] = v; write.store (next, std::memory_order_release); return true;
        }
        bool pop (T& v) noexcept
        {
            const auto r = read.load (std::memory_order_relaxed);
            if (r == write.load (std::memory_order_acquire)) return false;
            v = buf[r]; read.store ((r + 1) % N, std::memory_order_release); return true;
        }
        T buf[N] {};
        std::atomic<size_t> write { 0 }, read { 0 };
    };

    static constexpr int taps = 12, phases = 4;
    double sr = 48000.0;
    Coefs stage0, stage1;
    std::array<State, 4> state {};
    std::array<std::array<float, taps>, phases> coef {};
    std::array<float, taps * 2> history {}, historyR {};
    int histPos = 0, count = 0, blockLength = 4800;
    double kSum = 0.0, sum = 0.0;
    float peak = 0.0f;
    Fifo<LoudnessBlock, 256> blocks;
};

/** Fil de l'interface : instantanée, courte, intégrée, à partir des tranches de 100 ms. */
class LoudnessReader
{
public:
    void reset() noexcept { numBlocks = 0; ring.fill ({}); gatedSum = 0.0; gatedCount = 0; histogram.fill (0); integratedDirty = true; maxTruePeak = 0.0f; }

    void add (const LoudnessBlock& b) noexcept
    {
        ring[(size_t) (numBlocks % ringSize)] = b;
        ++numBlocks;
        lastTruePeak = b.truePeak;
        maxTruePeak = std::max (maxTruePeak, b.truePeak);
        // fenêtres de 400 ms recouvrantes à 75 % : une par tranche de 100 ms
        if (numBlocks >= 4)
        {
            const auto l = lufs (mean (4));
            if (l > -70.0)
            {
                const auto bin = std::clamp ((int) std::floor ((l + 70.0) * 10.0), 0, (int) histogram.size() - 1);
                ++histogram[(size_t) bin];
                integratedDirty = true;
            }
        }
    }

    double momentary() const noexcept { return numBlocks >= 4 ? lufs (mean (4)) : -100.0; }
    double shortTerm() const noexcept { return numBlocks >= 30 ? lufs (mean (30)) : (numBlocks >= 4 ? lufs (mean ((int) numBlocks)) : -100.0); }
    double rmsDb() const noexcept
    {
        if (numBlocks < 3) return -100.0;
        double s = 0.0;
        for (int i = 0; i < 3; ++i) s += ring[(size_t) ((numBlocks - 1 - i) % ringSize)].meanSquare;
        return 10.0 * std::log10 (std::max (1.0e-12, s / 3.0));
    }
    double truePeakDb() const noexcept { return 20.0 * std::log10 (std::max (1.0e-6f, lastTruePeak)); }
    double maxTruePeakDb() const noexcept { return 20.0 * std::log10 (std::max (1.0e-6f, maxTruePeak)); }

    double integrated() noexcept
    {
        if (! integratedDirty) return lastIntegrated;
        integratedDirty = false;
        // seuil absolu -70 LUFS (déjà appliqué), puis seuil relatif à -10 LU de la moyenne
        double e = 0.0; long long n = 0;
        for (size_t i = 0; i < histogram.size(); ++i) if (histogram[i] > 0) { e += histogram[i] * fromLufs (binCentre (i)); n += histogram[i]; }
        if (n == 0) return lastIntegrated = -100.0;
        const auto rel = lufs (e / (double) n) - 10.0;
        e = 0.0; n = 0;
        for (size_t i = 0; i < histogram.size(); ++i)
            if (histogram[i] > 0 && binCentre (i) >= rel) { e += histogram[i] * fromLufs (binCentre (i)); n += histogram[i]; }
        return lastIntegrated = n > 0 ? lufs (e / (double) n) : -100.0;
    }

private:
    static double lufs (double ms) noexcept { return -0.691 + 10.0 * std::log10 (std::max (1.0e-12, ms)); }
    static double fromLufs (double l) noexcept { return std::pow (10.0, (l + 0.691) / 10.0); }
    static double binCentre (size_t i) noexcept { return -70.0 + ((double) i + 0.5) / 10.0; }

    double mean (int blocksBack) const noexcept
    {
        blocksBack = (int) std::min<long long> (blocksBack, std::min<long long> (numBlocks, ringSize));
        double s = 0.0;
        for (int i = 0; i < blocksBack; ++i) s += ring[(size_t) ((numBlocks - 1 - i) % ringSize)].kMeanSquare;
        return blocksBack > 0 ? s / blocksBack : 0.0;
    }

    static constexpr long long ringSize = 32;
    std::array<LoudnessBlock, (size_t) ringSize> ring {};
    long long numBlocks = 0;
    std::array<long long, 760> histogram {}; // -70 à +6 LUFS par pas de 0,1 LU
    double gatedSum = 0.0; long long gatedCount = 0;
    bool integratedDirty = true;
    double lastIntegrated = -100.0;
    float lastTruePeak = 0.0f, maxTruePeak = 0.0f;
};

} // namespace grv::dsp
