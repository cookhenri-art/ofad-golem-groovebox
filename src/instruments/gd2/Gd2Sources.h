/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Good Doctor 2 — sources : tables propres, sources calculées (granulaire et spectral), analyse
    spectrale, tables personnelles et analyse des échantillons chargés.

    Tout ce qui coûte (FFT, tables limitées en bande, analyse) est fait hors du fil audio :
      - les tables et sources calculées, une fois, à la création du premier instrument ;
      - les échantillons chargés dans les emplacements de la piste, au chargement (PluginProcessor
        appelle buildRuntime, rangé dans SampleBuffer::runtime). Le fil audio ne fait que lire.
*/

#pragma once

#include "../GoodDoctor.h"

#include <memory>

namespace grv::instruments::gd2
{

using namespace grv::engine;
using namespace grv::synth;

constexpr int frames = gdoc::frames;                 // 12 ondes par table calculée
constexpr int newTables = 4;
constexpr int builtinTables = gdoc::numTables + newTables; // 16
constexpr int userSlots = 3;                          // tables perso : emplacements d'échantillon 2, 3, 4
constexpr int numTableChoices = builtinTables + userSlots;
constexpr int userFrameSize = 2048, userMaxFrames = 64;
constexpr int partials = 64, specFrames = 64;
constexpr int numCalcSources = 6;
constexpr double calcRootNote = 48.0;                // les sources calculées sont jouées en do 3

inline const std::vector<std::string>& tableNames()
{
    static const std::vector<std::string> n = []
    {
        auto v = gdoc::tableNames();
        for (const char* s : { "Kick (frappe)", "Hoover", "Sinus \xE2\x86\x92 carr\xC3\xA9", "Voix (formants)",
                               "Perso 1 (empl. 2)", "Perso 2 (empl. 3)", "Perso 3 (empl. 4)" })
            v.push_back (s);
        return v;
    }();
    return n;
}

inline const std::vector<std::string>& calcSourceNames()
{
    static const std::vector<std::string> n { "Ch\xC5\x93ur", "Souffle", "Cloches", "Voix", "Cordes", "Grondement" };
    return n;
}

/** Les 4 tables nouvelles (12 ondes chacune). Onde (t, k) : t * frames + k. */
inline const WaveBank& newBank()
{
    static const WaveBank b = []
    {
        using cd = std::complex<double>;
        WaveBank w;
        constexpr int F = frames;
        auto s2p = [] (double x) { return std::sin (kTwoPi * x); };
        // 0 kick : un sinus de plus en plus écrêté (corps → frappe saturée des basses dures)
        for (int k = 0; k < F; ++k)
        {
            const auto t = (double) k / (F - 1);
            const auto g = 1.0 + 24.0 * t * t;
            w.addWaveTime ([=] (double x) { return std::tanh (g * s2p (x)) / std::tanh (g); });
        }
        // 1 hoover : deux scies à largeur d'impulsion qui se croisent, octave dessous en fond
        for (int k = 0; k < F; ++k)
        {
            const auto t = (double) k / (F - 1);
            const auto pw = 0.5 - 0.42 * t;
            w.addWaveTime ([=] (double x)
            {
                const auto s1 = 2.0 * (x - std::floor (x)) - 1.0;
                const auto y = x + pw;
                const auto s2 = 2.0 * (y - std::floor (y)) - 1.0;
                const auto h = 0.5 * x;
                const auto s3 = 2.0 * (h - std::floor (h)) - 1.0;
                return 0.45 * (s1 - s2) + 0.35 * s3;
            });
        }
        // 2 sinus → carré : harmoniques impaires ajoutées une à une, adoucies
        for (int k = 0; k < F; ++k)
        {
            const auto t = (double) k / (F - 1);
            const auto maxH = 1.0 + t * 40.0;
            w.addWave ([=] (int h)
            {
                if (h % 2 == 0) return cd();
                const auto f = std::clamp (maxH - (double) h + 1.0, 0.0, 1.0);
                return cd (0.0, -0.5 * f / (double) h);
            });
        }
        // 3 voix : deux formants A → E → I → O → U sur une scie
        static constexpr double F1[5] { 730, 530, 300, 570, 330 }, F2[5] { 1090, 1840, 2290, 840, 1020 };
        for (int k = 0; k < F; ++k)
        {
            const auto t = 4.0 * (double) k / (F - 1);
            const auto i0 = std::min (3, (int) t);
            const auto fr = t - i0;
            const auto f1 = F1[i0] + (F1[i0 + 1] - F1[i0]) * fr, f2 = F2[i0] + (F2[i0 + 1] - F2[i0]) * fr;
            w.addWave ([=] (int h)
            {
                const auto hz = 130.81 * h;
                auto bump = [] (double x, double c, double bw) { return std::exp (-0.5 * std::pow ((x - c) / bw, 2)); };
                const auto g = 0.15 + 1.6 * bump (hz, f1, 110.0) + 1.0 * bump (hz, f2, 160.0);
                return cd (0.0, -0.5 * g / std::sqrt ((double) h));
            });
        }
        return w;
    }();
    return b;
}

/** Lecture d'une onde d'une table calculée (0..15) : niveau d'octave m, phase 0..1. */
inline float readBuiltin (int table, int k, WaveBank::Mip m, double phs) noexcept
{
    if (table < gluta::numTables) return gluta::bank().read (table * frames + k, m, phs);
    if (table < gdoc::numTables) return gdoc::ownBank().read ((table - gluta::numTables) * frames + k, m, phs);
    return newBank().read ((table - gdoc::numTables) * frames + k, m, phs);
}

//==============================================================================
// Analyse spectrale : amplitudes de 64 partiels harmoniques, par tranche de temps

struct Spectrum
{
    int frames = 0;
    std::vector<float> amp;      // frames * partials, crête de chaque tranche ramenée à 1
    double rootHz = 130.81;
    float at (int f, int k) const noexcept { return amp[(size_t) (f * partials + k)]; }
};

/** Fondamentale d'un extrait (autocorrélation normalisée, 40–1000 Hz) ; 0 si rien de net. */
inline double estimateRoot (const float* x, int n, double sr)
{
    const auto minLag = (int) (sr / 1000.0), maxLag = std::min (n / 2, (int) (sr / 40.0));
    if (maxLag <= minLag + 2) return 0.0;
    std::vector<double> r ((size_t) (maxLag + 1), 0.0);
    double best = 0.0;
    for (int lag = minLag; lag <= maxLag && lag < n; ++lag)
    {
        double num = 0.0, e1 = 0.0, e2 = 0.0;
        const auto len = n - lag;
        for (int i = 0; i < len; i += 2) { num += (double) x[i] * x[i + lag]; e1 += (double) x[i] * x[i]; e2 += (double) x[i + lag] * x[i + lag]; }
        r[(size_t) lag] = num / std::sqrt (e1 * e2 + 1.0e-12);
        best = std::max (best, r[(size_t) lag]);
    }
    if (best <= 0.5) return 0.0;
    // la première crête presque aussi haute que la meilleure (pas un multiple de la période),
    // affinée par une parabole sur ses voisines
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

/** Spectre par tranches d'un son mono (fenêtre de Hann de 4096 points, 64 tranches au plus).
    root : fondamentale connue (0 = estimée au milieu du son, 130,81 Hz si rien de net). */
inline Spectrum analyse (const float* x, int n, double sr, double root)
{
    Spectrum s;
    constexpr int N = 4096;
    if (n < 256 || x == nullptr) return s;
    if (root <= 0.0)
    {
        const auto mid = std::max (0, n / 2 - 4096);
        root = estimateRoot (x + mid, std::min (8192, n - mid), sr);
        if (root <= 0.0) root = 130.81;
    }
    s.rootHz = root;
    const auto count = std::clamp ((n - N) / 2048 + 1, 1, specFrames);
    const auto hop = count > 1 ? std::max (1, (n - N) / (count - 1)) : 0;
    s.frames = count;
    s.amp.assign ((size_t) (count * partials), 0.0f);
    std::vector<std::complex<double>> bins ((size_t) N);
    for (int f = 0; f < count; ++f)
    {
        const auto start = f * hop;
        for (int i = 0; i < N; ++i)
        {
            const auto j = start + i;
            const auto w = 0.5 - 0.5 * std::cos (kTwoPi * i / (N - 1));
            bins[(size_t) i] = { j < n ? (double) x[j] * w : 0.0, 0.0 };
        }
        fft (bins, false);
        float pk = 1.0e-9f;
        for (int k = 0; k < partials; ++k)
        {
            const auto c = root * (k + 1) * N / sr;
            if (c >= N / 2 - 2) break;
            // crête autour de l'harmonique (±3 %), le son réel peut être un peu faux
            const auto lo = std::max (1, (int) (c * 0.97)), hi = std::min (N / 2 - 1, (int) (c * 1.03) + 1);
            double m = 0.0;
            for (int b = lo; b <= hi; ++b) m = std::max (m, std::abs (bins[(size_t) b]));
            s.amp[(size_t) (f * partials + k)] = (float) m;
            pk = std::max (pk, (float) m);
        }
        for (int k = 0; k < partials; ++k) s.amp[(size_t) (f * partials + k)] /= pk;
    }
    return s;
}

//==============================================================================
// Sources calculées (granulaire et spectral) : 3 s en do 3, à 48 kHz

struct CalcSource
{
    std::vector<float> data;
    double sr = 48000.0;
    Spectrum spec;
};

inline const std::array<CalcSource, numCalcSources>& calcSources()
{
    static const std::array<CalcSource, numCalcSources> s = []
    {
        std::array<CalcSource, numCalcSources> a;
        constexpr double sr = 48000.0, f0 = 130.81;
        constexpr int n = (int) (sr * 3.0);
        std::uint32_t rng = 0x6D2B79F5u;
        auto rnd = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (double) rng / 2147483648.0 - 1.0; };
        auto bump = [] (double x, double c, double bw) { return std::exp (-0.5 * std::pow ((x - c) / bw, 2)); };
        for (auto& c : a) { c.data.assign ((size_t) n, 0.0f); c.sr = sr; }
        // 0 chœur : 5 voix légèrement désaccordées, voyelle « a » qui glisse vers « o », vibrato lent
        {
            std::array<double, 5> ph {}, det { -0.11, -0.05, 0.0, 0.06, 0.12 };
            for (int i = 0; i < n; ++i)
            {
                const auto t = i / sr;
                const auto vow = 0.5 + 0.5 * std::sin (kTwoPi * 0.17 * t);
                const auto fa = 730.0 + (570.0 - 730.0) * vow, fb = 1090.0 + (840.0 - 1090.0) * vow;
                double y = 0.0;
                for (size_t v = 0; v < 5; ++v)
                {
                    const auto f = f0 * std::pow (2.0, (det[v] + 0.08 * std::sin (kTwoPi * (4.5 + v * 0.3) * t)) / 12.0);
                    ph[v] += f / sr; ph[v] -= std::floor (ph[v]);
                    for (int h = 1; h <= 30; ++h)
                    {
                        const auto hz = f * h;
                        if (hz > 7000.0) break;
                        y += (0.12 + bump (hz, fa, 120.0) + 0.7 * bump (hz, fb, 160.0)) / h * std::sin (kTwoPi * ph[v] * h);
                    }
                }
                a[0].data[(size_t) i] = (float) (y * 0.12);
            }
        }
        // 1 souffle : bruit dans un passe-bande qui balaie lentement, avec une trace de la note
        {
            StateFilter f;
            double ph = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const auto t = i / sr;
                const auto fc = 600.0 * std::pow (6.0, 0.5 + 0.5 * std::sin (kTwoPi * 0.23 * t));
                const auto b = f.process (rnd(), fc, 0.5, sr).band;
                ph += f0 * 2.0 / sr; ph -= std::floor (ph);
                a[1].data[(size_t) i] = (float) (0.9 * b + 0.08 * std::sin (kTwoPi * ph));
            }
        }
        // 2 cloches : partiels inharmoniques frappés toutes les 0,75 s
        {
            static constexpr double ratio[7] { 1.0, 2.76, 4.07, 5.43, 6.8, 8.21, 11.66 };
            for (int i = 0; i < n; ++i)
            {
                const auto t = i / sr;
                const auto tk = std::fmod (t, 0.75);
                double y = 0.0;
                for (int k = 0; k < 7; ++k) y += std::exp (-tk * (1.5 + k * 1.3)) * std::sin (kTwoPi * f0 * 2.0 * ratio[k] * t) / (1.0 + k * 0.6);
                a[2].data[(size_t) i] = (float) (y * 0.3);
            }
        }
        // 3 voix : scie à formants, voyelles a e i o u qui se suivent
        {
            static constexpr double F1[5] { 730, 530, 300, 570, 330 }, F2[5] { 1090, 1840, 2290, 840, 1020 };
            double ph = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const auto t = i / sr;
                const auto v = std::fmod (t / 0.6, 5.0);
                const auto i0 = (int) v, i1 = (i0 + 1) % 5;
                const auto fr = std::clamp ((v - i0) * 3.0 - 2.0, 0.0, 1.0);
                const auto fa = F1[i0] + (F1[i1] - F1[i0]) * fr, fb = F2[i0] + (F2[i1] - F2[i0]) * fr;
                const auto f = f0 * std::pow (2.0, 0.1 * std::sin (kTwoPi * 5.2 * t) / 12.0);
                ph += f / sr; ph -= std::floor (ph);
                double y = 0.0;
                for (int h = 1; h <= 40; ++h)
                {
                    const auto hz = f * h;
                    if (hz > 8000.0) break;
                    y += (0.08 + 1.4 * bump (hz, fa, 90.0) + bump (hz, fb, 130.0)) / std::sqrt ((double) h) * std::sin (kTwoPi * ph * h);
                }
                a[3].data[(size_t) i] = (float) (y * 0.14);
            }
        }
        // 4 cordes : ensemble de scies douces, attaque lente
        {
            std::array<double, 4> ph {}, det { -0.09, -0.03, 0.04, 0.1 };
            for (int i = 0; i < n; ++i)
            {
                const auto t = i / sr;
                double y = 0.0;
                for (size_t v = 0; v < 4; ++v)
                {
                    const auto f = f0 * std::pow (2.0, (det[v] + 0.05 * std::sin (kTwoPi * (0.4 + v * 0.13) * t)) / 12.0);
                    ph[v] += f / sr; ph[v] -= std::floor (ph[v]);
                    for (int h = 1; h <= 24; ++h) y += std::sin (kTwoPi * ph[v] * h) / h * std::exp (-h * 0.09);
                }
                a[4].data[(size_t) i] = (float) (y * 0.2 * std::min (1.0, t / 0.4));
            }
        }
        // 5 grondement : FM grave et sale, rugosité qui va et vient
        {
            double pc = 0.0, pm = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const auto t = i / sr;
                const auto idx = 2.0 + 3.0 * (0.5 + 0.5 * std::sin (kTwoPi * 0.31 * t));
                pm += f0 * 0.5 * 1.01 / sr; pm -= std::floor (pm);
                pc += f0 * 0.5 / sr; pc -= std::floor (pc);
                const auto y = std::sin (kTwoPi * pc + idx * std::sin (kTwoPi * pm)) + 0.15 * rnd() * (0.5 + 0.5 * std::sin (kTwoPi * 3.0 * t));
                a[5].data[(size_t) i] = (float) (std::tanh (1.6 * y) * 0.6);
            }
        }
        for (auto& c : a)
        {
            float pk = 1.0e-6f;
            for (auto v : c.data) pk = std::max (pk, std::abs (v));
            for (auto& v : c.data) v = v / pk * 0.9f;
            c.spec = analyse (c.data.data(), (int) c.data.size(), c.sr, (c.data.empty() ? 0.0 : f0 * (&c == &a[2] ? 2.0 : (&c == &a[1] ? 2.0 : (&c == &a[5] ? 0.5 : 1.0)))));
        }
        return a;
    }();
    return s;
}

/** Spectres des 16 tables calculées (12 tranches = 12 ondes), pour le mode spectral. */
inline const std::array<Spectrum, builtinTables>& tableSpectra()
{
    static const std::array<Spectrum, builtinTables> s = []
    {
        std::array<Spectrum, builtinTables> a;
        constexpr int N = 512;
        std::vector<std::complex<double>> bins ((size_t) N);
        for (int t = 0; t < builtinTables; ++t)
        {
            auto& sp = a[(size_t) t];
            sp.frames = frames;
            sp.rootHz = 0.0;
            sp.amp.assign ((size_t) (frames * partials), 0.0f);
            for (int k = 0; k < frames; ++k)
            {
                for (int i = 0; i < N; ++i) bins[(size_t) i] = { (double) readBuiltin (t, k, WaveBank::Mip {}, (double) i / N), 0.0 };
                fft (bins, false);
                float pk = 1.0e-9f;
                for (int h = 0; h < partials; ++h) { const auto m = (float) std::abs (bins[(size_t) (h + 1)]); sp.amp[(size_t) (k * partials + h)] = m; pk = std::max (pk, m); }
                for (int h = 0; h < partials; ++h) sp.amp[(size_t) (k * partials + h)] /= pk;
            }
        }
        return a;
    }();
    return s;
}

//==============================================================================
// Échantillons chargés : table personnelle (emplacements 2 à 4) ou spectre (emplacement 1)

struct SampleRuntime
{
    int tableFrames = 0;
    WaveBank table;        // ondes limitées en bande (tables perso)
    Spectrum spec;         // tranches (emplacement 1 : source granulaire et spectrale)
};

/** Construit l'analyse d'un échantillon chargé dans l'emplacement index (hors du fil audio). */
inline std::shared_ptr<void> buildRuntime (int index, const SampleBuffer& s)
{
    if (s.data.size() < 16) return {};
    auto r = std::make_shared<SampleRuntime>();
    const auto n = (int) s.data.size();
    if (index == 0)
    {
        r->spec = analyse (s.data.data(), n, s.sampleRate, 0.0);
        return r;
    }
    if (index < 1 || index > userSlots) return {};
    // table au format courant : ondes de 2048 points à la suite ; plus court : un seul cycle étiré
    if (n >= userFrameSize)
    {
        const auto count = std::min (userMaxFrames, n / userFrameSize);
        for (int f = 0; f < count; ++f)
        {
            const float* w = s.data.data() + (size_t) f * userFrameSize;
            r->table.addWaveTime ([w] (double x) { return (double) w[(size_t) std::clamp ((int) (x * userFrameSize), 0, userFrameSize - 1)]; });
        }
        r->tableFrames = count;
    }
    else
    {
        const float* w = s.data.data();
        r->table.addWaveTime ([w, n] (double x)
        {
            const auto p = x * n;
            const auto i = (int) p;
            const auto f = p - i;
            return (double) w[(size_t) (i % n)] * (1.0 - f) + (double) w[(size_t) ((i + 1) % n)] * f;
        });
        r->tableFrames = 1;
    }
    return r;
}

} // namespace grv::instruments::gd2
