/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 (lot 4) : famille « Restauration » — outils de nettoyage du son, sur le principe des suites
    de restauration du marché (fonctions reprises, jamais les noms ni le graphisme). Pensés d'abord
    pour le traitement HORS TEMPS RÉEL (éditeur d'échantillon, écran Audio : la latence y est
    compensée et ne s'entend pas) ; utilisables aussi en direct, avec leur latence annoncée.

      - Anti-ronflement (0 ms) : coupe-bandes très étroits sur le 50 / 60 Hz du secteur et ses
        harmoniques (jusqu'à 32), fondamentale détectée ; mode dynamique : n'enlève que la part
        stable (le plancher) de chaque harmonique, la musique qui passe par là reste.
      - Débruiteur (≈ 43 ms) : spectral, fenêtre de 2048 (4096 hors temps réel), recouvrement 75 %,
        bruit estimé par statistiques minimales (ou appris / figé), gain de Wiener à rapport a priori
        « décision dirigée » (pas de bruit musical), lissage en fréquence, attaques préservées.
      - Anti-clics / anti-craquements (≈ 21 ms) : modèle autorégressif par segment, clics détectés
        sur le résidu de prédiction (seuil robuste : médiane des écarts), trous comblés par
        interpolation autorégressive aux moindres carrés.
      - Dé-esseur spectral : mode « filtres » sans latence (bande haute atténuée seule, séparation
        complémentaire) ou mode spectral (≈ 21 ms) : seuls les points du spectre des sifflantes qui
        dépassent leur voisinage baissent.
      - Anti-plosives (0 ms) : bouffées graves (« p », « b ») détectées, coupe-bas dynamique.

    Chaque outil a « Écoute : ce qui est retiré » (le delta) pour vérifier qu'on n'abîme pas le son.
    Aucune allocation dans process() : tout est réservé dans prepare().
*/

#pragma once

#include "../dsp/Common.h"
#include "../dsp/EqFilters.h"
#include "../engine/Module.h"

#include "../dsp/Fft.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace resto
{
    inline const std::vector<std::string>& listenChoices() { static const std::vector<std::string> c { "Son nettoy\xC3\xA9", "Ce qui est retir\xC3\xA9" }; return c; }
    inline float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
}

//==============================================================================
/** Transformée de Fourier à court terme en flux : fenêtres racine de Hann (analyse et synthèse),
    recouvrement 75 %, latence exacte de N échantillons. frame (bins, nb) reçoit les nb = N/2 + 1
    points complexes d'une trame et les modifie sur place. */
class StreamStft
{
public:
    void prepare (int order)
    {
        N = 1 << order; H = N / 4; nb = N / 2 + 1;
        fft.prepare (order);
        win.assign ((size_t) N, 0.0f);
        for (int i = 0; i < N; ++i) win[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / N));
        ring.assign ((size_t) N, 0.0f); ola.assign ((size_t) (2 * N), 0.0f);
        work.assign ((size_t) N, {});
        // fenêtres analyse × synthèse recouvertes : leur somme (constante) normalise la sortie
        float s = 0.0f;
        for (int k = 0; k < N; k += H) s += win[(size_t) k] * win[(size_t) k];
        norm = 1.0f / s;
        reset();
    }
    void reset() noexcept
    {
        std::fill (ring.begin(), ring.end(), 0.0f); std::fill (ola.begin(), ola.end(), 0.0f);
        pos = 0; t = 0; count = 0;
    }
    int size() const noexcept { return N; }
    int hop() const noexcept { return H; }
    int bands() const noexcept { return nb; }

    template <class F>
    float tick (float x, F&& frame) noexcept
    {
        ring[(size_t) pos] = x;
        pos = (pos + 1) & (N - 1);
        if (++count >= H)
        {
            count = 0;
            for (int i = 0; i < N; ++i) work[(size_t) i] = { ring[(size_t) ((pos + i) & (N - 1))] * win[(size_t) i], 0.0f };
            fft.forward (work.data());
            frame (work.data(), nb);                       // points 0 .. N/2
            work[0] = { work[0].real(), 0.0f };
            work[(size_t) (N / 2)] = { work[(size_t) (N / 2)].real(), 0.0f };
            for (int k = 1; k < N / 2; ++k) work[(size_t) (N - k)] = std::conj (work[(size_t) k]); // spectre d'un signal réel
            fft.inverse (work.data());
            // trame de t-N+1 à t (t : échantillon courant)
            const long long start = t - N + 1;
            for (int i = 0; i < N; ++i)
            {
                const auto idx = (size_t) (((start + i) % (2 * N) + 2 * N) % (2 * N));
                ola[idx] += work[(size_t) i].real() * win[(size_t) i] * norm;
            }
        }
        const auto outIdx = (size_t) (((t - N) % (2 * N) + 2 * N) % (2 * N));
        const float y = ola[outIdx];
        ola[outIdx] = 0.0f;
        ++t;
        return y;
    }

private:
    int N = 2048, H = 512, nb = 1025, pos = 0, count = 0;
    long long t = 0;
    float norm = 1.0f;
    Fft fft;
    std::vector<float> win, ring, ola;
    std::vector<std::complex<float>> work;
};

//==============================================================================
// Anti-ronflement
namespace rdehum
{
    enum Slot { fund = 0, harmonics, width, reduction, mode, listen, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "rdehum"; m.name = "Anti-ronflement"; m.role = "Ronflement du secteur"; m.family = "Restauration"; m.form = "rack";
            m.principle = "Coupe-bandes tr\xC3\xA8s \xC3\xA9troits sur le 50 ou 60 Hz du secteur et ses harmoniques, fondamentale d\xC3\xA9tect\xC3\xA9" "e ; sans latence";
            m.params = {
                C ("fund", "Fondamentale", { "Auto", "50 Hz", "60 Hz" }),
                I ("harmonics", "Harmoniques", "", 1, 32, 12),
                P ("width", "Largeur", "Hz", 0.5f, 12.0f, 3.0f, 0.5f),
                P ("reduction", "R\xC3\xA9" "duction", "dB", 0, 60, 40),
                C ("mode", "Mode", { "Fixe", "Dynamique" }, 1),
                C ("listen", "\xC3\x89" "coute", resto::listenChoices()),
            };
            m.sections = { { "main", "Anti-ronflement", "global", { "fund", "harmonics", "width", "reduction", "mode", "listen" }, "" } };
            m.presets = {
                { "Secteur 50 Hz", {} },
                { "Secteur 60 Hz", { { "fund", 2 } } },
                { "Guitare et ampli (fort)", { { "harmonics", 20 }, { "width", 4 }, { "reduction", 50 } } },
                { "L\xC3\xA9ger, musique riche", { { "harmonics", 6 }, { "width", 1.5f }, { "reduction", 24 } } },
            };
            return m;
        }();
        return d;
    }
}

class Dehum final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return rdehum::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override
    {
        for (auto& ch : bp) for (auto& f : ch) f.reset();
        for (auto& ch : det) for (auto& f : ch) f.reset();
        env.fill ({}); for (auto& c : curMin) c.fill (1.0e30); for (auto& c : prevMin) c.fill (1.0e30); subCount = 0;
        e50 = e60 = 0.0; f0 = 50.0; lastF = -1; lastW = -1; lastH = -1;
    }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        const int sel = (int) p[rdehum::fund];
        const int nh = std::clamp ((int) p[rdehum::harmonics], 1, maxH);
        const double w = std::max (0.25, (double) p[rdehum::width]);
        const float g = 1.0f - resto::dbToGain (-p[rdehum::reduction]);
        const bool dyn = (int) p[rdehum::mode] == 1, listen = (int) p[rdehum::listen] == 1;
        // fondamentale : 50 ou 60 Hz, ou détectée (énergie des 3 premières harmoniques de chaque réseau)
        if (sel == 1) f0 = 50.0; else if (sel == 2) f0 = 60.0;
        else
        {
            const double a = std::exp (-(double) n / (0.8 * sr));
            double s50 = 0.0, s60 = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double x = 0.5 * (L[i] + R[i]);
                for (int k = 0; k < 3; ++k)
                {
                    const auto y5 = det[0][(size_t) k].process (x), y6 = det[1][(size_t) k].process (x);
                    s50 += y5 * y5; s60 += y6 * y6;
                }
            }
            e50 = a * e50 + (1 - a) * s50; e60 = a * e60 + (1 - a) * s60;
            if (lastDet != (int) sr) { setupDetectors(); lastDet = (int) sr; }
            f0 = e60 > 1.5 * e50 ? 60.0 : 50.0;
        }
        if (f0 != lastF || w != lastW || nh != lastH) setup (nh, w);
        const double atk = std::exp (-1.0 / (0.05 * sr));
        const int subLen = (int) (0.5 * sr); // plancher : plus petit niveau de la dernière seconde (deux demi-secondes)
        for (int i = 0; i < n; ++i)
        {
            float xin[2] = { L[i], R[i] };
            for (int c = 0; c < 2; ++c)
            {
                double removed = 0.0;
                for (int h = 0; h < nh; ++h)
                {
                    auto y = bp[(size_t) c][(size_t) h].process (xin[c]);
                    if (dyn)
                    {   // n'enlever que le plancher stable de l'harmonique (le ronflement), pas la musique qui la traverse
                        auto& e = env[(size_t) c][(size_t) h];
                        e = atk * e + (1 - atk) * std::abs (y);
                        auto& cm = curMin[(size_t) c][(size_t) h];
                        cm = std::min (cm, e);
                        const double fl = std::min (cm, prevMin[(size_t) c][(size_t) h]);
                        if (e > 1.0e-9 && fl < 1.0e29) y *= std::min (1.0, fl * 1.5 / e);
                    }
                    removed += y;
                }
                if (c == 1 && dyn && ++subCount >= subLen)
                {
                    subCount = 0;
                    for (int cc = 0; cc < 2; ++cc) for (int h = 0; h < maxH; ++h) { prevMin[(size_t) cc][(size_t) h] = curMin[(size_t) cc][(size_t) h]; curMin[(size_t) cc][(size_t) h] = env[(size_t) cc][(size_t) h]; }
                }
                const auto rem = (float) removed * g;
                const auto out = listen ? rem : xin[c] - rem;
                if (c == 0) L[i] = out; else R[i] = out;
            }
        }
    }
private:
    static constexpr int maxH = 32;
    void setup (int nh, double w)
    {
        lastF = f0; lastW = w; lastH = nh;
        for (auto& ch : bp)
            for (int h = 0; h < maxH; ++h)
            {
                const double fc = std::min (f0 * (h + 1), 0.45 * sr);
                ch[(size_t) h].set (SvfEq::bandpass, fc, std::max (0.5, fc / w), 0.0, sr);
            }
    }
    void setupDetectors()
    {
        for (int k = 0; k < 3; ++k) { det[0][(size_t) k].set (SvfEq::bandpass, 50.0 * (k + 1), 25.0, 0.0, sr); det[1][(size_t) k].set (SvfEq::bandpass, 60.0 * (k + 1), 25.0, 0.0, sr); }
    }
    double sr = 48000.0, f0 = 50.0, lastF = -1, lastW = -1, e50 = 0, e60 = 0;
    int lastH = -1, lastDet = -1;
    std::array<std::array<SvfEq, maxH>, 2> bp {};
    std::array<std::array<SvfEq, 3>, 2> det {};
    std::array<std::array<double, maxH>, 2> env {}, curMin {}, prevMin {};
    int subCount = 0;
};

//==============================================================================
// Débruiteur
namespace rdenoise
{
    enum Slot { reduction = 0, sensitivity, smoothing, transients, mode, listen, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "rdenoise"; m.name = "D\xC3\xA9" "bruiteur"; m.role = "Souffle et bruit de fond"; m.family = "Restauration"; m.form = "rack";
            m.principle = "Spectral : bruit estim\xC3\xA9 (statistiques minimales), appris ou fig\xC3\xA9, gain sans bruit musical ; latence 43 ms en direct, compens\xC3\xA9" "e au traitement";
            m.params = {
                P ("reduction", "R\xC3\xA9" "duction", "dB", 0, 40, 12),
                P ("sensitivity", "Sensibilit\xC3\xA9", "dB", -10, 10, 0),
                P ("smoothing", "Lissage", "%", 0, 100, 60),
                P ("transients", "Attaques pr\xC3\xA9serv\xC3\xA9" "es", "%", 0, 100, 50),
                C ("mode", "Bruit", { "Suivi auto", "Apprendre", "Fig\xC3\xA9" }),
                C ("listen", "\xC3\x89" "coute", resto::listenChoices()),
            };
            m.sections = { { "main", "D\xC3\xA9" "bruiteur", "global", { "reduction", "sensitivity", "smoothing", "transients", "mode", "listen" }, "" } };
            m.presets = {
                { "Souffle l\xC3\xA9ger", {} },
                { "Souffle fort (cassette, ampli)", { { "reduction", 20 }, { "sensitivity", 3 } } },
                { "Voix dans une pi\xC3\xA8" "ce bruyante", { { "reduction", 16 }, { "transients", 70 } } },
                { "Bruit de fond doux (musique)", { { "reduction", 8 }, { "smoothing", 80 }, { "sensitivity", -3 } } },
            };
            return m;
        }();
        return d;
    }
}

class Denoise final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return rdenoise::desc(); }
    void setOffline (bool o) noexcept override { offline = o; }
    void prepare (double s, int) override
    {
        sr = s;
        const int order = offline ? 12 : 11;
        for (auto& c : ch) { c.stft.prepare (order); const auto nb = (size_t) c.stft.bands(); c.P.assign (nb, 0.0f); c.N.assign (nb, 0.0f); c.G.assign (nb, 1.0f);
                             c.gam.assign (nb, 0.0f); c.learnSum.assign (nb, 0.0f); c.tmp.assign (nb, 1.0f); c.frames = 0; c.learnFrames = 0; c.lastE = 0.0f;
                             c.cur.assign (nb, 1.0e30f); for (auto& u : c.sub) u.assign (nb, 1.0e30f); c.subCount = 0; c.subPos = 0; }
        // retard du signal sec (écoute du retiré)
        dry.assign ((size_t) ch[0].stft.size() * 2, {});
        reset();
    }
    void reset() noexcept override
    {
        for (auto& c : ch) { c.stft.reset(); std::fill (c.P.begin(), c.P.end(), 0.0f); std::fill (c.N.begin(), c.N.end(), 0.0f); std::fill (c.G.begin(), c.G.end(), 1.0f);
                             std::fill (c.gam.begin(), c.gam.end(), 0.0f); c.frames = 0; c.lastE = 0.0f;
                             std::fill (c.cur.begin(), c.cur.end(), 1.0e30f); for (auto& u : c.sub) std::fill (u.begin(), u.end(), 1.0e30f); c.subCount = 0; c.subPos = 0; }
        std::fill (dry.begin(), dry.end(), std::array<float, 2> {}); dpos = 0;
    }
    int latency() const noexcept override { return ch[0].stft.size(); }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        const float gmin = resto::dbToGain (-p[rdenoise::reduction]);
        const float over = resto::dbToGain (p[rdenoise::sensitivity] * 0.5f); // marge au-dessus du bruit estimé
        const float smooth = p[rdenoise::smoothing] * 0.01f, keep = p[rdenoise::transients] * 0.01f;
        const int mode = (int) p[rdenoise::mode];
        const bool listen = (int) p[rdenoise::listen] == 1;
        if (mode != 1 && lastMode == 1) for (auto& c : ch) finishLearning (c);
        if (mode == 1 && lastMode != 1) for (auto& c : ch) { std::fill (c.learnSum.begin(), c.learnSum.end(), 0.0f); c.learnFrames = 0; }
        lastMode = mode;
        const int D = ch[0].stft.size();
        for (int i = 0; i < n; ++i)
        {
            float xin[2] = { L[i], R[i] };
            const auto& d = dry[(size_t) dpos]; // sec retardé de D
            const float dl = d[0], dr = d[1];
            dry[(size_t) dpos] = { xin[0], xin[1] };
            dpos = (dpos + 1) % (D);
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                const float y = s.stft.tick (xin[c], [&] (std::complex<float>* b, int nb) { frame (s, b, nb, gmin, over, smooth, keep, mode); });
                const float out = listen ? (c == 0 ? dl : dr) - y : y;
                if (c == 0) L[i] = out; else R[i] = out;
            }
        }
    }
private:
    static constexpr int numSub = 8;   // statistiques minimales : minimum sur 8 sous-fenêtres de 0,25 s (2 s en tout)
    struct Chan { StreamStft stft; std::vector<float> P, N, G, gam, learnSum, tmp, cur; std::array<std::vector<float>, numSub> sub; int frames = 0, learnFrames = 0, subCount = 0, subPos = 0; float lastE = 0.0f; };
    void finishLearning (Chan& c) noexcept
    {
        if (c.learnFrames < 4) return;
        for (size_t k = 0; k < c.N.size(); ++k) c.N[k] = c.learnSum[k] / (float) c.learnFrames;
    }
    void frame (Chan& c, std::complex<float>* b, int nb, float gmin, float over, float smooth, float keep, int mode) noexcept
    {
        const double hopSec = (double) c.stft.hop() / sr;
        const float aP = (float) std::exp (-hopSec / 0.2);                  // puissance lissée (0,2 s) : son minimum suit le bruit
        const int subLen = std::max (1, (int) std::lround (0.25 / hopSec));
        const int warm = c.stft.size() / c.stft.hop();                       // trames encore partiellement vides au départ
        const float dd = 0.90f + 0.08f * smooth;                             // rapport a priori « décision dirigée »
        float E = 0.0f;
        for (int k = 0; k < nb; ++k)
        {
            const float pw = std::norm (b[k]);
            E += pw;
            c.P[(size_t) k] = c.frames == 0 ? pw : aP * c.P[(size_t) k] + (1.0f - aP) * pw;
            if (mode == 1) c.learnSum[(size_t) k] += pw;
            else if (mode == 0 && c.frames >= warm)
                c.cur[(size_t) k] = std::min (c.cur[(size_t) k], c.P[(size_t) k]);
        }
        if (mode == 0 && c.frames >= warm)
        {   // bruit = plus petite puissance lissée des 2 dernières secondes
            if (++c.subCount >= subLen) { c.sub[(size_t) c.subPos] = c.cur; c.subPos = (c.subPos + 1) % numSub; std::fill (c.cur.begin(), c.cur.end(), 1.0e30f); c.subCount = 0; }
            for (int k = 0; k < nb; ++k)
            {
                float m = c.cur[(size_t) k];
                for (const auto& u : c.sub) m = std::min (m, u[(size_t) k]);
                c.N[(size_t) k] = m < 1.0e29f ? m : c.P[(size_t) k];
            }
        }
        if (mode == 1) ++c.learnFrames;
        ++c.frames;
        // le bruit est lisse d'un point du spectre à l'autre, une note tenue fait un pic : l'estimation
        // d'un point ne dépasse pas trois fois le plus petit de ses voisins (une note n'est jamais prise pour du bruit)
        if (mode == 0)
        {
            for (int k = 0; k < nb; ++k)
            {
                float lo = c.N[(size_t) k];
                for (int j = std::max (0, k - 12); j <= std::min (nb - 1, k + 12); ++j) lo = std::min (lo, c.N[(size_t) j]);
                c.tmp[(size_t) k] = std::min (c.N[(size_t) k], 3.0f * lo);
            }
            for (int k = 0; k < nb; ++k) c.N[(size_t) k] = c.tmp[(size_t) k];
        }
        // attaque : énergie de la trame bien au-dessus de la précédente
        const bool onset = E > 4.0f * c.lastE && E > 1.0e-9f;
        c.lastE = E;
        for (int k = 0; k < nb; ++k)
        {
            const float nk = std::max (1.0e-12f, c.N[(size_t) k] * over * (mode == 1 ? 1.2f : 2.0f)); // biais du minimum compensé
            const float gamma = std::norm (b[k]) / nk;
            const float xi = dd * c.G[(size_t) k] * c.G[(size_t) k] * c.gam[(size_t) k] + (1.0f - dd) * std::max (0.0f, gamma - 1.0f);
            c.gam[(size_t) k] = gamma;
            c.tmp[(size_t) k] = std::max (gmin, xi / (1.0f + xi));
        }
        for (int k = 0; k < nb; ++k)
        {   // lissage en fréquence (trois points) : pas de notes isolées (« bruit musical »)
            const float a = c.tmp[(size_t) std::max (0, k - 1)], m = c.tmp[(size_t) k], z = c.tmp[(size_t) std::min (nb - 1, k + 1)];
            float g = 0.25f * a + 0.5f * m + 0.25f * z;
            if (onset) g = g + (1.0f - g) * keep;
            c.G[(size_t) k] = g;
            b[k] *= (mode == 1 ? 1.0f : g); // en apprentissage, le son passe tel quel
        }
    }
    double sr = 48000.0;
    bool offline = false;
    int lastMode = 0, dpos = 0;
    std::array<Chan, 2> ch;
    std::vector<std::array<float, 2>> dry;
};

//==============================================================================
// Anti-clics / anti-craquements
namespace rdeclick
{
    enum Slot { sensitivity = 0, maxLen, mode, listen, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "rdeclick"; m.name = "Anti-clics"; m.role = "Clics et craquements"; m.family = "Restauration"; m.form = "rack";
            m.principle = "Mod\xC3\xA8le autor\xC3\xA9gressif : clics d\xC3\xA9tect\xC3\xA9s sur le r\xC3\xA9sidu de pr\xC3\xA9" "diction, trous combl\xC3\xA9s par interpolation ; latence 21 ms en direct, compens\xC3\xA9" "e au traitement";
            m.params = {
                P ("sensitivity", "Sensibilit\xC3\xA9", "", 1, 10, 5),
                P ("maxLen", "Taille max", "ms", 0.1f, 3.0f, 1.0f, 0.5f),
                C ("mode", "Mode", { "Clics", "Craquements" }),
                C ("listen", "\xC3\x89" "coute", resto::listenChoices()),
            };
            m.sections = { { "main", "Anti-clics", "global", { "sensitivity", "maxLen", "mode", "listen" }, "" } };
            m.presets = {
                { "Clics de montage", {} },
                { "Vinyle : craquements", { { "mode", 1 }, { "sensitivity", 6 }, { "maxLen", 0.5f } } },
                { "Vinyle ab\xC3\xAEm\xC3\xA9", { { "mode", 1 }, { "sensitivity", 8 }, { "maxLen", 1.5f } } },
                { "Bruits de bouche (voix)", { { "sensitivity", 4 }, { "maxLen", 2.5f } } },
            };
            return m;
        }();
        return d;
    }
}

class Declick final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return rdeclick::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& c : ch) { c.buf.assign ((size_t) W, 0.0f); c.orig.assign ((size_t) W, 0.0f); }
        a.assign ((size_t) order + 1, 0.0); r.assign ((size_t) order + 1, 0.0); tmpA.assign ((size_t) order + 1, 0.0);
        e.assign ((size_t) W, 0.0f); mask.assign ((size_t) W, 0); absE.assign ((size_t) W, 0.0f);
        Bm.assign ((size_t) (maxGap * maxGap), 0.0); rhs.assign ((size_t) maxGap, 0.0); col.assign ((size_t) (maxGap * (maxGap + 4 * order + 8)), 0.0);
        reset();
    }
    void reset() noexcept override
    {
        for (auto& c : ch) { std::fill (c.buf.begin(), c.buf.end(), 0.0f); std::fill (c.orig.begin(), c.orig.end(), 0.0f); }
        fill = 0; outPos = 0;
    }
    int latency() const noexcept override { return Ctx + B; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        sens = p[rdeclick::sensitivity];
        crackle = (int) p[rdeclick::mode] == 1;
        maxRun = std::clamp ((int) (p[rdeclick::maxLen] * 0.001 * sr), 2, maxGap);
        const bool listen = (int) p[rdeclick::listen] == 1;
        for (int i = 0; i < n; ++i)
        {
            // fenêtre de trois blocs : [réparé, en sortie | à réparer | à venir, en remplissage] ; latence 2 blocs
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                const float x = c == 0 ? L[i] : R[i];
                s.buf[(size_t) (Ctx + B + fill)] = x; s.orig[(size_t) (Ctx + B + fill)] = x;
                const float y = s.buf[(size_t) fill], o = s.orig[(size_t) fill];
                const float out = listen ? o - y : y;
                if (c == 0) L[i] = out; else R[i] = out;
            }
            if (++fill >= B)
            {
                fill = 0;
                for (auto& s : ch) repair (s);
                for (auto& s : ch)
                {   // la fenêtre avance d'un bloc
                    std::copy (s.buf.begin() + B, s.buf.end(), s.buf.begin());
                    std::copy (s.orig.begin() + B, s.orig.end(), s.orig.begin());
                    std::fill (s.buf.end() - B, s.buf.end(), 0.0f);
                    std::fill (s.orig.end() - B, s.orig.end(), 0.0f);
                }
            }
        }
    }
private:
    static constexpr int B = 512, Ctx = 512, W = Ctx + B + Ctx, order = 24, maxGap = 160;
    struct Chan { std::vector<float> buf, orig; };
    void repair (Chan& s) noexcept
    {
        // modèle AR sur la fenêtre entière (autocorrélation, Levinson-Durbin)
        const auto* x = s.buf.data();
        for (int k = 0; k <= order; ++k) { double acc = 0.0; for (int t = k; t < W; ++t) acc += (double) x[t] * x[t - k]; r[(size_t) k] = acc; }
        if (r[0] < 1.0e-10) return;
        r[0] *= 1.0001;
        std::fill (a.begin(), a.end(), 0.0); a[0] = 1.0;
        double err = r[0];
        for (int i = 1; i <= order; ++i)
        {
            double acc = r[(size_t) i];
            for (int j = 1; j < i; ++j) acc += a[(size_t) j] * r[(size_t) (i - j)];
            const double k = -acc / err;
            std::copy (a.begin(), a.end(), tmpA.begin());
            for (int j = 1; j < i; ++j) a[(size_t) j] = tmpA[(size_t) j] + k * tmpA[(size_t) (i - j)];
            a[(size_t) i] = k;
            err *= (1.0 - k * k);
            if (err <= 1.0e-12) return;
        }
        // résidu de prédiction et seuil robuste (médiane des écarts absolus)
        for (int t = 0; t < W; ++t)
        {
            double acc = x[t];
            for (int k = 1; k <= order && t - k >= 0; ++k) acc += a[(size_t) k] * x[t - k];
            e[(size_t) t] = t >= order ? (float) acc : 0.0f;
            absE[(size_t) t] = std::abs (e[(size_t) t]);
        }
        auto mid = absE.begin() + W / 2;
        std::nth_element (absE.begin() + order, mid, absE.end());
        const float mad = std::max (1.0e-7f, *mid * 1.4826f);
        const float thr = mad * (crackle ? (13.0f - sens) * 0.8f : (14.0f - sens) * 1.1f);
        std::fill (mask.begin(), mask.end(), 0);
        for (int t = Ctx - order; t < Ctx + B + order; ++t)
            if (std::abs (e[(size_t) t]) > thr)
                for (int u = std::max (0, t - 2); u <= std::min (W - 1, t + order / 4); ++u) mask[(size_t) u] = 1; // le résidu déborde après le clic
        // chaque trou (suite de points marqués) comblé par interpolation AR aux moindres carrés
        for (int t = Ctx - order; t < Ctx + B + order;)
        {
            if (! mask[(size_t) t]) { ++t; continue; }
            int end = t; while (end < W && mask[(size_t) end]) ++end;
            const int m = end - t;
            if (m <= maxRun && t - 2 * order >= 0 && end + 2 * order <= W) interpolate (s.buf.data(), t, m);
            t = end;
        }
    }
    /** Points [g, g+m) inconnus : minimise l'énergie du résidu AR sur la zone ; système m × m (Cholesky). */
    void interpolate (float* x, int g, int m) noexcept
    {
        const int t0 = g, t1 = g + m + order; // équations e_t pour t dans [t0, t1)
        const int rows = t1 - t0;
        // A_u (rows × m) et partie connue c (rows)
        for (int i = 0; i < rows * m; ++i) col[(size_t) i] = 0.0;
        double cvec[maxGap + order + 2];
        for (int ri = 0; ri < rows; ++ri)
        {
            const int t = t0 + ri;
            double c = 0.0;
            for (int k = 0; k <= order; ++k)
            {
                const int idx = t - k;
                const double ak = a[(size_t) k];
                if (idx >= g && idx < g + m) col[(size_t) (ri * m + (idx - g))] += ak;
                else c += ak * x[idx];
            }
            cvec[ri] = c;
        }
        // normales : (AᵀA) u = −Aᵀ c
        for (int i = 0; i < m; ++i)
        {
            for (int j = 0; j <= i; ++j)
            {
                double acc = 0.0;
                for (int ri = 0; ri < rows; ++ri) acc += col[(size_t) (ri * m + i)] * col[(size_t) (ri * m + j)];
                Bm[(size_t) (i * m + j)] = acc;
            }
            double acc = 0.0;
            for (int ri = 0; ri < rows; ++ri) acc += col[(size_t) (ri * m + i)] * cvec[ri];
            rhs[(size_t) i] = -acc;
            Bm[(size_t) (i * m + i)] += 1.0e-9;
        }
        // Cholesky en place (triangle bas)
        for (int i = 0; i < m; ++i)
        {
            for (int j = 0; j <= i; ++j)
            {
                double sum = Bm[(size_t) (i * m + j)];
                for (int k = 0; k < j; ++k) sum -= Bm[(size_t) (i * m + k)] * Bm[(size_t) (j * m + k)];
                if (i == j) { if (sum <= 0.0) return; Bm[(size_t) (i * m + i)] = std::sqrt (sum); }
                else Bm[(size_t) (i * m + j)] = sum / Bm[(size_t) (j * m + j)];
            }
        }
        for (int i = 0; i < m; ++i) { double sum = rhs[(size_t) i]; for (int k = 0; k < i; ++k) sum -= Bm[(size_t) (i * m + k)] * rhs[(size_t) k]; rhs[(size_t) i] = sum / Bm[(size_t) (i * m + i)]; }
        for (int i = m - 1; i >= 0; --i) { double sum = rhs[(size_t) i]; for (int k = i + 1; k < m; ++k) sum -= Bm[(size_t) (k * m + i)] * rhs[(size_t) k]; rhs[(size_t) i] = sum / Bm[(size_t) (i * m + i)]; }
        for (int i = 0; i < m; ++i) if (std::isfinite (rhs[(size_t) i])) x[g + i] = (float) std::clamp (rhs[(size_t) i], -4.0, 4.0);
    }
    double sr = 48000.0;
    float sens = 5.0f; bool crackle = false; int maxRun = 48, fill = 0, outPos = 0;
    std::array<Chan, 2> ch;
    std::vector<double> a, r, tmpA, Bm, rhs, col;
    std::vector<float> e, absE;
    std::vector<unsigned char> mask;
};

//==============================================================================
// Dé-esseur spectral
namespace rdeess
{
    enum Slot { freq = 0, thresh, amount, mode, listen, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "rdeess"; m.name = "D\xC3\xA9-esseur spectral"; m.role = "Sifflantes"; m.family = "Restauration"; m.form = "rack";
            m.principle = "Sifflantes att\xC3\xA9nu\xC3\xA9" "es dans leur bande seulement, quand elles d\xC3\xA9passent : mode filtres sans latence, ou spectral (21 ms en direct)";
            m.params = {
                P ("freq", "Fr\xC3\xA9quence", "Hz", 2000, 12000, 5500, 0.5f),
                P ("thresh", "Seuil", "dB", -60, 0, -30),
                P ("amount", "R\xC3\xA9" "duction max", "dB", 0, 24, 8),
                C ("mode", "Mode", { "Filtres (0 ms)", "Spectral" }),
                C ("listen", "\xC3\x89" "coute", resto::listenChoices()),
            };
            m.sections = { { "main", "D\xC3\xA9-esseur", "global", { "freq", "thresh", "amount", "mode", "listen" }, "" } };
            m.presets = {
                { "Voix", {} },
                { "Voix aigu\xC3\xAB (femme, enfant)", { { "freq", 7500 }, { "thresh", -32 } } },
                { "Voix grave (homme)", { { "freq", 4500 }, { "thresh", -28 } } },
                { "Spectral pr\xC3\xA9" "cis", { { "mode", 1 }, { "amount", 10 } } },
                { "Charleston trop pr\xC3\xA9sente", { { "freq", 8000 }, { "thresh", -26 }, { "amount", 6 } } },
            };
            return m;
        }();
        return d;
    }
}

class SpectralDeesser final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return rdeess::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& c : ch) { c.stft.prepare (10); c.dry.assign ((size_t) c.stft.size(), 0.0f); }
        reset();
    }
    void reset() noexcept override
    {
        for (auto& c : ch) { c.stft.reset(); c.lp.reset(); c.det.reset(); std::fill (c.dry.begin(), c.dry.end(), 0.0f); c.dpos = 0; }
        env = 0.0; gain = 1.0; lastF = -1.0;
    }
    int latency() const noexcept override { return spectral ? ch[0].stft.size() : 0; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        const double f = p[rdeess::freq];
        const float thr = resto::dbToGain (p[rdeess::thresh]);
        const float maxRed = resto::dbToGain (-p[rdeess::amount]);
        spectral = (int) p[rdeess::mode] == 1;
        const bool listen = (int) p[rdeess::listen] == 1;
        if (f != lastF)
        {
            lastF = f;
            for (auto& c : ch) { c.lp.set (SvfEq::lowpass, f, 0.7071, 0.0, sr); c.det.set (SvfEq::highpass, f, 0.7071, 0.0, sr); }
        }
        const double atk = std::exp (-1.0 / (0.0015 * sr)), rel = std::exp (-1.0 / (0.06 * sr));
        if (! spectral)
        {
            for (int i = 0; i < n; ++i)
            {
                // détection : niveau de la bande haute (lié aux deux voies)
                const double h = std::max (std::abs (ch[0].det.process (L[i])), std::abs (ch[1].det.process (R[i])));
                env = h > env ? atk * env + (1 - atk) * h : rel * env + (1 - rel) * h;
                const double target = env > thr ? std::max ((double) maxRed, (double) thr / env) : 1.0;
                gain = target < gain ? target : 0.995 * gain + 0.005 * target;
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = ch[(size_t) c];
                    const float x = c == 0 ? L[i] : R[i];
                    const float lo = (float) s.lp.process (x), hi = x - lo; // séparation complémentaire : lo + hi = x
                    const float y = lo + hi * (float) gain;
                    const float out = listen ? x - y : y;
                    if (c == 0) L[i] = out; else R[i] = out;
                }
            }
            return;
        }
        const int D = ch[0].stft.size();
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                const float x = c == 0 ? L[i] : R[i];
                const float d = s.dry[(size_t) s.dpos]; s.dry[(size_t) s.dpos] = x; s.dpos = (s.dpos + 1) % D;
                const float y = s.stft.tick (x, [&] (std::complex<float>* b, int nb)
                {
                    const int k0 = std::clamp ((int) (f / sr * 2.0 * (nb - 1)), 1, nb - 1);
                    double band = 0.0; for (int k = k0; k < nb; ++k) band += std::norm (b[k]);
                    const double lvl = 2.0 * std::sqrt (band) / (2.0 * (nb - 1)); // valeur efficace de la bande (Parseval, fenêtre de Hann)
                    if (lvl <= thr) return;
                    const double red = std::max ((double) maxRed, (double) thr / lvl);
                    // seuls les points qui dépassent leur voisinage (sifflante large) baissent ; les notes tenues restent
                    for (int k = k0; k < nb; ++k)
                    {
                        const double loc = (std::norm (b[std::max (k0, k - 4)]) + std::norm (b[std::min (nb - 1, k + 4)])) * 0.5;
                        const double tonal = std::norm (b[k]) > 6.0 * loc ? 1.0 : 0.0;
                        b[k] *= (float) (tonal > 0.5 ? 1.0 : red);
                    }
                });
                const float out = listen ? d - y : y;
                if (c == 0) L[i] = out; else R[i] = out;
            }
    }
private:
    struct Chan { StreamStft stft; SvfEq lp, det; std::vector<float> dry; int dpos = 0; };
    double sr = 48000.0, env = 0.0, gain = 1.0, lastF = -1.0;
    bool spectral = false;
    std::array<Chan, 2> ch;
};

//==============================================================================
// Anti-plosives
namespace rdeplosive
{
    enum Slot { sensitivity = 0, freq, amount, listen, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "rdeplosive"; m.name = "Anti-plosives"; m.role = "\xC2\xAB P \xC2\xBB et \xC2\xAB B \xC2\xBB qui tapent"; m.family = "Restauration"; m.form = "rack";
            m.principle = "Bouff\xC3\xA9" "es graves d\xC3\xA9tect\xC3\xA9" "es (consonnes explosives, souffle sur le micro), coupe-bas dynamique ; sans latence";
            m.params = {
                P ("sensitivity", "Sensibilit\xC3\xA9", "", 1, 10, 5),
                P ("freq", "Fr\xC3\xA9quence", "Hz", 60, 300, 150, 0.5f),
                P ("amount", "R\xC3\xA9" "duction", "dB", 0, 30, 15),
                C ("listen", "\xC3\x89" "coute", resto::listenChoices()),
            };
            m.sections = { { "main", "Anti-plosives", "global", { "sensitivity", "freq", "amount", "listen" }, "" } };
            m.presets = { { "Voix", {} }, { "Voix tr\xC3\xA8s pr\xC3\xA8s du micro", { { "sensitivity", 7 }, { "amount", 22 } } }, { "Vent l\xC3\xA9ger", { { "freq", 220 }, { "sensitivity", 6 } } } };
            return m;
        }();
        return d;
    }
}

class Deplosive final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return rdeplosive::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override { for (auto& c : lp) c.reset(); lowFast = lowSlow = full = 0.0; amt = 0.0; lastF = -1.0; hold = 0; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        const double f = p[rdeplosive::freq];
        if (f != lastF) { lastF = f; for (auto& c : lp) c.set (SvfEq::lowpass, f, 0.7071, 0.0, sr); }
        const double ratioNeed = 14.0 - p[rdeplosive::sensitivity]; // bouffée : le grave dépasse nettement le reste
        const double maxCut = 1.0 - resto::dbToGain (-p[rdeplosive::amount]);
        const bool listen = (int) p[rdeplosive::listen] == 1;
        const double af = std::exp (-1.0 / (0.004 * sr)), as = std::exp (-1.0 / (0.25 * sr)), rel = std::exp (-1.0 / (0.08 * sr));
        for (int i = 0; i < n; ++i)
        {
            float lo[2];
            for (int c = 0; c < 2; ++c) lo[c] = (float) lp[(size_t) c].process (c == 0 ? L[i] : R[i]);
            const double lv = std::max (std::abs (lo[0]), std::abs (lo[1]));
            const double hv = std::max (std::abs (L[i] - lo[0]), std::abs (R[i] - lo[1]));
            lowFast = af * lowFast + (1 - af) * lv; lowSlow = as * lowSlow + (1 - as) * lv; full = af * full + (1 - af) * hv;
            const bool burst = lowFast > 1.8 * lowSlow && lowFast > ratioNeed * 0.12 * (full + 1.0e-6);
            if (burst) hold = (int) (0.06 * sr);              // une bouffée dure : on la tient 60 ms au moins
            amt = hold > 0 ? std::max (amt, maxCut) : rel * amt;
            if (hold > 0) --hold;
            for (int c = 0; c < 2; ++c)
            {
                const float x = c == 0 ? L[i] : R[i];
                const float y = x - lo[c] * (float) amt; // le grave seul baisse pendant la bouffée
                const float out = listen ? x - y : y;
                if (c == 0) L[i] = out; else R[i] = out;
            }
        }
    }
private:
    double sr = 48000.0, lowFast = 0, lowSlow = 0, full = 0, amt = 0, lastF = -1;
    int hold = 0;
    std::array<SvfEq, 2> lp {};
};

} // namespace grv::effects
