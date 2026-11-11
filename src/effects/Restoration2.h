/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 (lot 5) : restauration lourde, famille « Restauration » (suite de Restoration.h).

      - Anti-écrêtage : redessine les crêtes coupées d'un enregistrement saturé. Points écrêtés
        repérés (au niveau d'écrêtage, détecté ou réglé), puis :
          · en direct (≈ 11 ms) : courbe d'Hermite tendue entre les points sains, jamais sous le
            niveau d'écrêtage ;
          · hors temps réel (qualité maximale) : interpolation autorégressive aux moindres carrés,
            puis projection répétée sur les solutions COHÉRENTES (les points écrêtés restent au moins
            au niveau d'écrêtage, du bon signe) — le principe des « déclippeurs » parcimonieux (SPADE),
            sous une forme bornée en calcul.
      - Dé-réverb : réduit l'écho d'une pièce.
          · en direct (≈ 43 ms) : suppression spectrale de la réverbération tardive (modèle de
            décroissance exponentielle réglé par la durée de réverbération T60) ;
          · hors temps réel : prédiction linéaire pondérée par point du spectre (principe WPE, version
            récursive) : la queue prédite à partir des trames passées est retranchée.
*/

#pragma once

#include "Restoration.h"

namespace grv::effects
{

//==============================================================================
// Anti-écrêtage
namespace rdeclip
{
    enum Slot { detect = 0, level, maxLen, output, listen, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "rdeclip"; m.name = "Anti-\xC3\xA9" "cr\xC3\xAA" "tage"; m.role = "Cr\xC3\xAA" "tes coup\xC3\xA9" "es"; m.family = "Restauration"; m.form = "rack";
            m.principle = "Redessine les cr\xC3\xAA" "tes coup\xC3\xA9" "es d'un son satur\xC3\xA9 ; qualit\xC3\xA9 maximale au traitement (reconstruction coh\xC3\xA9rente), version l\xC3\xA9g\xC3\xA8re en direct (11 ms)";
            m.params = {
                C ("detect", "Niveau d'\xC3\xA9" "cr\xC3\xAA" "tage", { "D\xC3\xA9tect\xC3\xA9", "R\xC3\xA9gl\xC3\xA9" }),
                P ("level", "Niveau r\xC3\xA9gl\xC3\xA9", "dB", -24, 0, -0.5f),
                P ("maxLen", "Taille max", "ms", 0.2f, 6.0f, 2.5f, 0.5f),
                P ("output", "Sortie", "dB", -12, 0, -3),
                C ("listen", "\xC3\x89" "coute", resto::listenChoices()),
            };
            m.sections = { { "main", "Anti-\xC3\xA9" "cr\xC3\xAA" "tage", "global", { "detect", "level", "maxLen", "output", "listen" }, "" } };
            m.presets = {
                { "Prise satur\xC3\xA9" "e", {} },
                { "Fortement \xC3\xA9" "cr\xC3\xAA" "t\xC3\xA9", { { "maxLen", 5 }, { "output", -6 } } },
                { "Boucle limit\xC3\xA9" "e trop fort", { { "detect", 1 }, { "level", -3 }, { "output", -4 } } },
            };
            return m;
        }();
        return d;
    }
}

class Declip final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return rdeclip::desc(); }
    void setOffline (bool o) noexcept override { offline = o; }
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& c : ch) { c.buf.assign ((size_t) W, 0.0f); c.orig.assign ((size_t) W, 0.0f); }
        a.assign ((size_t) order + 1, 0.0); r.assign ((size_t) order + 1, 0.0); tmpA.assign ((size_t) order + 1, 0.0);
        Bm.assign ((size_t) (maxGap * maxGap), 0.0); rhs.assign ((size_t) maxGap, 0.0); col.assign ((size_t) (maxGap * (maxGap + order + 2)), 0.0);
        mask.assign ((size_t) W, 0);
        reset();
    }
    void reset() noexcept override
    {
        for (auto& c : ch) { std::fill (c.buf.begin(), c.buf.end(), 0.0f); std::fill (c.orig.begin(), c.orig.end(), 0.0f); }
        fill = 0; peakHold = 0.0f;
    }
    int latency() const noexcept override { return 2 * B; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        manual = (int) p[rdeclip::detect] == 1;
        manualLevel = resto::dbToGain (p[rdeclip::level]);
        maxRun = std::clamp ((int) (p[rdeclip::maxLen] * 0.001 * sr), 2, maxGap);
        const float out = resto::dbToGain (p[rdeclip::output]);
        const bool listen = (int) p[rdeclip::listen] == 1;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                const float x = c == 0 ? L[i] : R[i];
                s.buf[(size_t) (2 * B + fill)] = x; s.orig[(size_t) (2 * B + fill)] = x;
                const float y = s.buf[(size_t) fill], o = s.orig[(size_t) fill];
                const float v = listen ? (y - o) : y * out;
                if (c == 0) L[i] = v; else R[i] = v;
            }
            if (++fill >= B)
            {
                fill = 0;
                for (auto& s : ch) repair (s);
                for (auto& s : ch)
                {
                    std::copy (s.buf.begin() + B, s.buf.end(), s.buf.begin()); std::copy (s.orig.begin() + B, s.orig.end(), s.orig.begin());
                    std::fill (s.buf.end() - B, s.buf.end(), 0.0f); std::fill (s.orig.end() - B, s.orig.end(), 0.0f);
                }
            }
        }
    }
private:
    static constexpr int B = 256, W = 3 * B, order = 20, maxGap = 300;
    struct Chan { std::vector<float> buf, orig; };

    void repair (Chan& s) noexcept
    {
        auto* x = s.buf.data();
        const auto* o = s.orig.data();
        // niveau d'écrêtage : réglé, ou plus haute valeur récente (un son écrêté y reste « collé »)
        float pk = 0.0f;
        for (int t = 0; t < W; ++t) pk = std::max (pk, std::abs (o[t]));
        peakHold = std::max (pk, peakHold * 0.98f);
        const float lvl = manual ? manualLevel : peakHold * 0.999f;
        if (lvl < 1.0e-4f) return;
        std::fill (mask.begin(), mask.end(), 0);
        int marked = 0;
        // auto : une coupure numérique est un PLATEAU (au moins 3 points égaux au niveau) ; une crête
        // de sinus pur n'y reste jamais — un son propre n'est donc pas touché
        const float flat = 1.0e-4f * lvl;
        // repérage jusqu'au bout de la fenêtre : les coupures à venir ne faussent pas le modèle AR
        const int lo = B - order, hi = W;
        for (int t = lo; t < hi; ++t)
        {
            if (std::abs (o[t]) < lvl) continue;
            if (manual) { mask[(size_t) t] = 1; ++marked; continue; }
            int a0 = t, b0 = t;
            while (a0 > 0 && std::abs (o[a0 - 1]) >= lvl && std::abs (o[a0 - 1] - o[t]) <= flat) --a0;
            while (b0 + 1 < W && std::abs (o[b0 + 1]) >= lvl && std::abs (o[b0 + 1] - o[t]) <= flat) ++b0;
            if (b0 - a0 + 1 >= 3) { mask[(size_t) t] = 1; ++marked; }
        }
        if (marked == 0) return;
        if (offline) fitAR (x);
        for (int t = B - order; t < 2 * B + order && t < W;)
        {
            if (! mask[(size_t) t]) { ++t; continue; }
            int end = t; while (end < W && mask[(size_t) end]) ++end;
            const int m = end - t;
            if (m <= maxRun && t - 2 * order >= 0 && end + order < W)
            {
                bool nextClose = false; // une autre coupure dans le contexte droit : le modèle AR n'aurait pas d'appui sain
                for (int q = end; q < end + order; ++q) nextClose = nextClose || mask[(size_t) q];
                if (offline && arOk && ! nextClose) interpolateConsistent (x, o, t, m, lvl);
                else hermite (x, o, t, m, lvl);
            }
            t = end;
        }
    }

    /** Direct : courbe d'Hermite entre les deux points sains, pentes prises à côté ; jamais sous le niveau. */
    static void hermite (float* x, const float* o, int g, int m, float lvl) noexcept
    {
        const int a0 = g - 1, b0 = g + m;
        const float ya = x[a0], yb = x[b0];
        const float ma = x[a0] - x[a0 - 1], mb = x[b0 + 1] - x[b0];
        const float sgn = o[g] >= 0.0f ? 1.0f : -1.0f;
        const int span = m + 1;
        for (int i = 1; i <= m; ++i)
        {
            const float u = (float) i / (float) span, u2 = u * u, u3 = u2 * u;
            float v = (2 * u3 - 3 * u2 + 1) * ya + (u3 - 2 * u2 + u) * ma * span + (-2 * u3 + 3 * u2) * yb + (u3 - u2) * mb * span;
            if (v * sgn < lvl) v = sgn * lvl;                    // cohérent : au moins le niveau coupé
            v = std::clamp (v, -4.0f, 4.0f);
            x[g + i - 1] = v;
        }
    }

    /** Modèle AR par la méthode de covariance (moindres carrés sur les seuls points sains, sans
        fenêtre) : des pôles nets, qui prolongent bien les sons tenus dans un long trou. */
    void fitAR (const float* x) noexcept
    {
        arOk = false;
        constexpr int P = order;
        double phi[P][P] {}, psi[P] {};
        int rows = 0;
        for (int t = P; t < W; ++t)
        {
            bool clean = true;
            for (int k = 0; k <= P && clean; ++k) clean = ! mask[(size_t) (t - k)];
            if (! clean) continue;
            ++rows;
            for (int i = 0; i < P; ++i)
            {
                const double xi = x[t - 1 - i];
                psi[i] += xi * x[t];
                for (int j = 0; j <= i; ++j) phi[i][j] += xi * x[t - 1 - j];
            }
        }
        if (rows < 3 * P) return;
        double tr = 0.0; for (int i = 0; i < P; ++i) tr += phi[i][i];
        if (tr < 1.0e-12) return;
        for (int i = 0; i < P; ++i) phi[i][i] += 1.0e-6 * tr / P;     // régularisation légère
        // Cholesky (triangle inférieur), puis phi · c = psi ; prédiction x[t] ≈ Σ c_i x[t-1-i]
        for (int i = 0; i < P; ++i)
            for (int j = 0; j <= i; ++j)
            {
                double sum = phi[i][j];
                for (int k = 0; k < j; ++k) sum -= phi[i][k] * phi[j][k];
                if (i == j) { if (sum <= 0.0) return; phi[i][i] = std::sqrt (sum); }
                else phi[i][j] = sum / phi[j][j];
            }
        double y[P];
        for (int i = 0; i < P; ++i) { double sum = psi[i]; for (int k = 0; k < i; ++k) sum -= phi[i][k] * y[k]; y[i] = sum / phi[i][i]; }
        for (int i = P - 1; i >= 0; --i) { double sum = y[i]; for (int k = i + 1; k < P; ++k) sum -= phi[k][i] * y[k]; y[i] = sum / phi[i][i]; }
        a[0] = 1.0;
        for (int i = 0; i < P; ++i) { if (! std::isfinite (y[i])) return; a[(size_t) (i + 1)] = -y[i]; }
        arOk = true;
    }

    /** Hors temps réel : interpolation AR aux moindres carrés, puis projection sur les solutions
        cohérentes (au moins le niveau, bon signe) ; les points remontés sont figés et on recommence. */
    void interpolateConsistent (float* x, const float* o, int g, int m, float lvl) noexcept
    {
        hermite (x, o, g, m, lvl);                       // point de départ cohérent
        float h[maxGap];
        std::copy (x + g, x + g + m, h);
        // l'interpolation AR seule tend vers zéro dans un long trou : si elle ne remonte pas au-dessus
        // du niveau coupé sur l'essentiel du trou, la courbe tendue (déjà cohérente) est gardée
        {
            int idx0[maxGap]; for (int i = 0; i < m; ++i) idx0[i] = i;
            if (! solve (x, g, m, idx0, m)) { std::copy (h, h + m, x + g); return; }
            const float sgn0 = o[g] >= 0.0f ? 1.0f : -1.0f;
            int ok = 0; for (int i = 0; i < m; ++i) ok += x[g + i] * sgn0 >= lvl ? 1 : 0;
            if (ok < (m * 4) / 5) { std::copy (h, h + m, x + g); return; }
        }
        unsigned char fixedPt[maxGap] {};
        for (int it = 0; it < 6; ++it)
        {
            int unknown = 0;
            int idx[maxGap];
            for (int i = 0; i < m; ++i) if (! fixedPt[i]) idx[unknown++] = i;
            if (unknown == 0) break;
            if (! solve (x, g, m, idx, unknown)) { hermite (x, o, g, m, lvl); return; }
            bool changed = false;
            const float sgn = o[g] >= 0.0f ? 1.0f : -1.0f;
            for (int i = 0; i < m; ++i)
                if (x[g + i] * sgn < lvl) { x[g + i] = sgn * lvl; if (! fixedPt[i]) { fixedPt[i] = 1; changed = true; } }
            if (! changed) break;
        }
        for (int i = 0; i < m; ++i) x[g + i] = std::clamp (x[g + i], -4.0f, 4.0f);
    }

    /** Moindres carrés : inconnues = points idx[0..u) du trou [g, g+m) ; le reste est connu. */
    bool solve (float* x, int g, int m, const int* idx, int u) noexcept
    {
        const int t0 = g, t1 = g + m + order, rows = t1 - t0;
        if (rows * u > (int) col.size()) return false;
        int pos[maxGap]; for (int i = 0; i < m; ++i) pos[i] = -1;
        for (int j = 0; j < u; ++j) pos[idx[j]] = j;
        std::fill (col.begin(), col.begin() + rows * u, 0.0);
        double cvec[maxGap + order + 2];
        for (int ri = 0; ri < rows; ++ri)
        {
            const int t = t0 + ri; double c = 0.0;
            for (int k = 0; k <= order; ++k)
            {
                const int at = t - k; const double ak = a[(size_t) k];
                const int rel = at - g;
                if (rel >= 0 && rel < m && pos[rel] >= 0) col[(size_t) (ri * u + pos[rel])] += ak;
                else c += ak * x[at];
            }
            cvec[ri] = c;
        }
        for (int i = 0; i < u; ++i)
        {
            for (int j = 0; j <= i; ++j) { double acc = 0.0; for (int ri = 0; ri < rows; ++ri) acc += col[(size_t) (ri * u + i)] * col[(size_t) (ri * u + j)]; Bm[(size_t) (i * u + j)] = acc; }
            double acc = 0.0; for (int ri = 0; ri < rows; ++ri) acc += col[(size_t) (ri * u + i)] * cvec[ri];
            rhs[(size_t) i] = -acc; Bm[(size_t) (i * u + i)] += 1.0e-9;
        }
        for (int i = 0; i < u; ++i)
            for (int j = 0; j <= i; ++j)
            {
                double sum = Bm[(size_t) (i * u + j)];
                for (int k = 0; k < j; ++k) sum -= Bm[(size_t) (i * u + k)] * Bm[(size_t) (j * u + k)];
                if (i == j) { if (sum <= 0.0) return false; Bm[(size_t) (i * u + i)] = std::sqrt (sum); }
                else Bm[(size_t) (i * u + j)] = sum / Bm[(size_t) (j * u + j)];
            }
        for (int i = 0; i < u; ++i) { double sum = rhs[(size_t) i]; for (int k = 0; k < i; ++k) sum -= Bm[(size_t) (i * u + k)] * rhs[(size_t) k]; rhs[(size_t) i] = sum / Bm[(size_t) (i * u + i)]; }
        for (int i = u - 1; i >= 0; --i) { double sum = rhs[(size_t) i]; for (int k = i + 1; k < u; ++k) sum -= Bm[(size_t) (k * u + i)] * rhs[(size_t) k]; rhs[(size_t) i] = sum / Bm[(size_t) (i * u + i)]; }
        for (int j = 0; j < u; ++j) { if (! std::isfinite (rhs[(size_t) j])) return false; x[g + idx[j]] = (float) rhs[(size_t) j]; }
        return true;
    }

    double sr = 48000.0;
    bool offline = false, manual = false, arOk = false;
    float manualLevel = 1.0f, peakHold = 0.0f;
    int maxRun = 120, fill = 0;
    std::array<Chan, 2> ch;
    std::vector<double> a, r, tmpA, Bm, rhs, col;
    std::vector<unsigned char> mask;
};

//==============================================================================
// Dé-réverb
namespace rdereverb
{
    enum Slot { t60 = 0, amount, early, smoothing, listen, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "rdereverb"; m.name = "D\xC3\xA9-r\xC3\xA9verb"; m.role = "\xC3\x89" "cho d'une pi\xC3\xA8" "ce"; m.family = "Restauration"; m.form = "rack";
            m.principle = "R\xC3\xA9" "duit l'\xC3\xA9" "cho d'une pi\xC3\xA8" "ce : pr\xC3\xA9" "diction lin\xC3\xA9" "aire pond\xC3\xA9r\xC3\xA9" "e au traitement, suppression de la queue en direct (43 ms)";
            m.params = {
                P ("t60", "Dur\xC3\xA9" "e de la pi\xC3\xA8" "ce", "s", 0.2f, 4.0f, 0.8f, 0.5f),
                P ("amount", "R\xC3\xA9" "duction", "dB", 0, 24, 10),
                P ("early", "D\xC3\xA9" "but de la queue", "ms", 20, 120, 50),
                P ("smoothing", "Lissage", "%", 0, 100, 50),
                C ("listen", "\xC3\x89" "coute", resto::listenChoices()),
            };
            m.sections = { { "main", "D\xC3\xA9-r\xC3\xA9verb", "global", { "t60", "amount", "early", "smoothing", "listen" }, "" } };
            m.presets = {
                { "Pi\xC3\xA8" "ce moyenne", {} },
                { "Petite pi\xC3\xA8" "ce s\xC3\xA8" "che", { { "t60", 0.4f }, { "amount", 8 } } },
                { "Grande salle", { { "t60", 2.0f }, { "amount", 14 }, { "early", 80 } } },
                { "Voix lointaine (fort)", { { "t60", 1.2f }, { "amount", 18 } } },
            };
            return m;
        }();
        return d;
    }
}

class Dereverb final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return rdereverb::desc(); }
    void setOffline (bool o) noexcept override { offline = o; }
    void prepare (double s, int) override
    {
        sr = s;
        const int order = offline ? 12 : 11;
        for (auto& c : ch)
        {
            c.stft.prepare (order);
            const auto nb = (size_t) c.stft.bands();
            c.P.assign (nb, 0.0f); c.G.assign (nb, 1.0f); c.tmp.assign (nb, 1.0f);
            c.hist.assign ((size_t) histLen * nb, 0.0f); c.hpos = 0;
            c.dry.assign ((size_t) c.stft.size(), 0.0f); c.dpos = 0;
            // prédiction linéaire pondérée (hors temps réel) : K trames passées, retard D trames
            c.past.assign ((size_t) (K + D) * nb, {}); c.ppos = 0;
            c.g.assign ((size_t) K * nb, {}); c.Rinv.assign ((size_t) K * K * nb, {});
        }
        reset();
    }
    void reset() noexcept override
    {
        for (auto& c : ch)
        {
            c.stft.reset(); std::fill (c.P.begin(), c.P.end(), 0.0f); std::fill (c.G.begin(), c.G.end(), 1.0f);
            std::fill (c.hist.begin(), c.hist.end(), 0.0f); c.hpos = 0; std::fill (c.dry.begin(), c.dry.end(), 0.0f); c.dpos = 0;
            std::fill (c.past.begin(), c.past.end(), std::complex<float> {}); c.ppos = 0; std::fill (c.g.begin(), c.g.end(), std::complex<float> {});
            const auto nb = (size_t) c.stft.bands();
            std::fill (c.Rinv.begin(), c.Rinv.end(), std::complex<float> {});
            for (size_t k = 0; k < nb; ++k) for (int i = 0; i < K; ++i) c.Rinv[(k * K + (size_t) i) * K + (size_t) i] = { 1.0e3f, 0.0f };
            c.frames = 0;
        }
    }
    int latency() const noexcept override { return ch[0].stft.size(); }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        const float gmin = resto::dbToGain (-p[rdereverb::amount]);
        const float t60s = std::max (0.1f, p[rdereverb::t60]);
        const float earlyMs = p[rdereverb::early], smooth = p[rdereverb::smoothing] * 0.01f;
        const bool listen = (int) p[rdereverb::listen] == 1;
        const int D2 = ch[0].stft.size();
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                const float x = c == 0 ? L[i] : R[i];
                const float d = s.dry[(size_t) s.dpos]; s.dry[(size_t) s.dpos] = x; s.dpos = (s.dpos + 1) % D2;
                const float y = s.stft.tick (x, [&] (std::complex<float>* b, int nb)
                {
                    if (offline) wpe (s, b, nb, gmin);
                    else lateSuppress (s, b, nb, gmin, t60s, earlyMs, smooth);
                });
                const float out = listen ? d - y : y;
                if (c == 0) L[i] = out; else R[i] = out;
            }
    }
private:
    static constexpr int histLen = 64, K = 6, D = 2;
    struct Chan
    {
        StreamStft stft; std::vector<float> P, G, tmp, hist, dry; int hpos = 0, dpos = 0, ppos = 0, frames = 0;
        std::vector<std::complex<float>> past, g, Rinv;
    };

    /** Direct : puissance de la queue tardive prédite par la décroissance exponentielle de la pièce. */
    void lateSuppress (Chan& c, std::complex<float>* b, int nb, float gmin, float t60s, float earlyMs, float smooth) noexcept
    {
        const double hopSec = (double) c.stft.hop() / sr;
        const int nd = std::clamp ((int) std::lround (earlyMs * 0.001 / hopSec), 1, histLen - 1);
        const float decay = (float) std::exp (-2.0 * 3.0 * std::log (10.0) / t60s * nd * hopSec); // énergie restante après nd trames
        const float aP = (float) std::exp (-hopSec / 0.02);
        const float aG = 0.3f + 0.65f * smooth;
        const size_t base = (size_t) c.hpos * (size_t) nb;
        const size_t old = (size_t) ((c.hpos - nd + histLen) % histLen) * (size_t) nb;
        for (int k = 0; k < nb; ++k)
        {
            const float pw = std::norm (b[k]);
            c.P[(size_t) k] = aP * c.P[(size_t) k] + (1.0f - aP) * pw;
            c.hist[base + (size_t) k] = c.P[(size_t) k];
            const float late = decay * c.hist[old + (size_t) k];
            const float g = std::max (gmin, 1.0f - late / std::max (1.0e-12f, c.P[(size_t) k]));
            c.tmp[(size_t) k] = g;
        }
        c.hpos = (c.hpos + 1) % histLen;
        for (int k = 0; k < nb; ++k)
        {
            const float g = 0.25f * c.tmp[(size_t) std::max (0, k - 1)] + 0.5f * c.tmp[(size_t) k] + 0.25f * c.tmp[(size_t) std::min (nb - 1, k + 1)];
            c.G[(size_t) k] = g < c.G[(size_t) k] ? g : aG * c.G[(size_t) k] + (1.0f - aG) * g;
            b[k] *= c.G[(size_t) k];
        }
    }

    /** Hors temps réel : prédiction linéaire pondérée récursive par point du spectre (principe WPE). */
    void wpe (Chan& c, std::complex<float>* b, int nb, float gmin) noexcept
    {
        constexpr float alpha = 0.995f;
        const float mixAmount = 1.0f - gmin;      // réduction 0 dB : rien n'est retranché
        const int hist = K + D;
        for (int k = 0; k < nb; ++k)
        {
            // vecteur des trames passées, retardées de D
            std::complex<float> xv[K];
            for (int j = 0; j < K; ++j) xv[j] = c.past[(size_t) (((c.ppos - D - j - 1) % hist + hist) % hist) * (size_t) nb + (size_t) k];
            auto* gk = &c.g[(size_t) k * K];
            auto* Ri = &c.Rinv[(size_t) k * K * K];
            std::complex<float> pred {};
            for (int j = 0; j < K; ++j) pred += std::conj (gk[j]) * xv[j];
            const auto X = b[k];
            const auto dk = X - pred;
            // poids : puissance du son voulu (estimée par la sortie), bornée
            const float lam = std::max (1.0e-8f, std::norm (dk));
            std::complex<float> Rx[K];
            for (int i = 0; i < K; ++i) { std::complex<float> acc {}; for (int j = 0; j < K; ++j) acc += Ri[i * K + j] * xv[j]; Rx[i] = acc; }
            std::complex<float> den = alpha * lam;
            for (int j = 0; j < K; ++j) den += std::conj (xv[j]) * Rx[j];
            if (std::abs (den) > 1.0e-12f && std::isfinite (den.real()))
            {
                std::complex<float> kg[K];
                for (int i = 0; i < K; ++i) kg[i] = Rx[i] / den;
                for (int i = 0; i < K; ++i) gk[i] += kg[i] * std::conj (dk);
                // R⁻¹ ← (R⁻¹ − k xᴴ R⁻¹) / α
                std::complex<float> xR[K];
                for (int j = 0; j < K; ++j) { std::complex<float> acc {}; for (int i = 0; i < K; ++i) acc += std::conj (xv[i]) * Ri[i * K + j]; xR[j] = acc; }
                for (int i = 0; i < K; ++i) for (int j = 0; j < K; ++j) Ri[i * K + j] = (Ri[i * K + j] - kg[i] * xR[j]) / alpha;
                // garde-fou : coefficients bornés
                float gn = 0.0f; for (int i = 0; i < K; ++i) gn += std::norm (gk[i]);
                if (! std::isfinite (gn) || gn > 4.0f) { for (int i = 0; i < K; ++i) gk[i] = {}; for (int i = 0; i < K * K; ++i) Ri[i] = (i % (K + 1) == 0) ? std::complex<float> (1.0e3f, 0.0f) : std::complex<float> {}; }
            }
            c.past[(size_t) c.ppos * (size_t) nb + (size_t) k] = X;
            auto out = X - mixAmount * pred;
            if (std::norm (out) > std::norm (X)) out = X;   // la prédiction n'ajoute jamais d'énergie
            b[k] = out;
        }
        c.ppos = (c.ppos + 1) % hist;
        ++c.frames;
    }

    double sr = 48000.0;
    bool offline = false;
    std::array<Chan, 2> ch;
};

} // namespace grv::effects
