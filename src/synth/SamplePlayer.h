/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Tête de lecture d'échantillon stéréo : interpolation d'Hermite à 4 points, lecture avant ou
    arrière, boucle avant ou aller-retour avec fondu à la jointure, bornes de début et de fin.
    Ne possède pas les données : lit un SampleBuffer appartenant au moteur.
*/

#pragma once

#include "SynthCore.h"
#include "../engine/Module.h"

namespace grv::synth
{

using grv::engine::SampleBuffer;

class SamplePlayer
{
public:
    enum LoopMode { noLoop = 0, loopForward, loopPingPong };

    struct Region
    {
        double start = 0.0, end = 1.0;          // fractions de la longueur
        double loopStart = 0.0, loopEnd = 1.0;  // fractions, dans [start, end]
        LoopMode loop = noLoop;
        bool reverse = false;
    };

    void setSample (const SampleBuffer* s) noexcept { sample = s; active = false; }
    const SampleBuffer* getSample() const noexcept { return sample; }
    bool hasSample() const noexcept { return sample != nullptr && sample->data.size() > 4; }

    void start (const Region& r) noexcept
    {
        if (! hasSample()) { active = false; return; }
        region = r;
        computeBounds();
        pos = region.reverse ? hi : lo;
        dir = region.reverse ? -1.0 : 1.0;
        active = true;
        wrapped = false;
        // v0.22 : départ au milieu du fichier (découpe, début déplacé, lecture inversée) → fondu
        // d'entrée de 1 ms, sinon la première valeur non nulle fait un clic
        const auto startsInside = region.reverse ? hi < (double) (length() - 2) : lo > 0.5;
        fadeInLen = startsInside ? std::max (1, (int) (0.001 * fileRate())) : 0;
        fadeInLeft = fadeInLen;
    }

    void stop() noexcept { active = false; }
    bool isActive() const noexcept { return active; }
    double position() const noexcept { return length() > 0 ? pos / (double) length() : 0.0; }

    /** Lit un échantillon stéréo et avance. rate = pas de lecture (1 = vitesse d'origine au taux du fichier). */
    void next (double rate, double& l, double& r) noexcept
    {
        if (! active) { l = r = 0.0; return; }
        read (pos, l, r);
        double g = 1.0;
        if (fadeInLeft > 0) { g = 1.0 - (double) fadeInLeft / (double) (fadeInLen + 1); --fadeInLeft; }
        if (region.loop == noLoop)
        {
            // v0.22 : fondu de sortie de 2 ms avant la fin (une fin coupée au milieu d'une onde claquait)
            const auto left = (dir > 0.0 ? hi - pos : pos - lo) / std::max (1.0e-6, std::abs (rate));
            const auto endLen = 0.002 * fileRate();
            if (left < endLen) g *= std::max (0.0, left / endLen);
        }

        // Fondu de boucle (puissance constante). La matière qui sert au fondu est prise avant le
        // début de boucle si elle existe (fondu avant la jointure), sinon après la fin de boucle
        // (fondu juste après la jointure, v0.22) ; à défaut (boucle sur tout le fichier), le
        // fondu va vers la valeur du point de reprise, comme avant.
        if (region.loop == loopForward && lHi - lLo > 4.0 * fadeLen)
        {
            const auto last = (double) (length() - 1);
            const bool before = lLo >= fadeLen + 2.0, after = lHi + fadeLen + 2.0 <= last;
            auto blend = [&] (double t, double other) // t : 0 = son courant, 1 = autre lecture
            {
                double l2, r2;
                read (other, l2, r2);
                const auto a = std::sqrt (1.0 - t), b = std::sqrt (t);
                l = l * a + l2 * b;
                r = r * a + r2 * b;
            };
            if (dir > 0.0)
            {
                const auto toEnd = lHi - pos, fromStart = pos - lLo;
                if ((before || ! after) && toEnd < fadeLen && toEnd >= 0.0) blend (1.0 - toEnd / fadeLen, lLo - toEnd);
                else if (! before && after && wrapped && fromStart < fadeLen && fromStart >= 0.0) blend (1.0 - fromStart / fadeLen, lHi + fromStart);
            }
            else
            {
                // lecture inversée : mêmes règles, sens opposé (v0.22)
                const auto toStart = pos - lLo, fromEnd = lHi - pos;
                if ((after || ! before) && toStart < fadeLen && toStart >= 0.0) blend (1.0 - toStart / fadeLen, lHi + toStart);
                else if (! after && before && wrapped && fromEnd < fadeLen && fromEnd >= 0.0) blend (1.0 - fromEnd / fadeLen, lLo - fromEnd);
            }
        }
        if (g < 1.0) { l *= g; r *= g; }

        pos += rate * dir;
        if (region.loop == noLoop)
        {
            if (pos >= hi || pos < lo) active = false;
        }
        else if (region.loop == loopForward)
        {
            if (dir > 0.0 && pos >= lHi) { pos = lLo + (pos - lHi); wrapped = true; }
            else if (dir < 0.0 && pos < lLo) { pos = lHi - (lLo - pos); wrapped = true; }
            if (pos > hi || pos < lo) pos = std::clamp (pos, lo, hi);
        }
        else
        {
            if (pos >= lHi) { pos = lHi - (pos - lHi); dir = -1.0; }
            else if (pos < lLo) { pos = lLo + (lLo - pos); dir = 1.0; }
        }
    }

    double fileRate() const noexcept { return sample ? sample->sampleRate : 44100.0; }
    int length() const noexcept { return sample ? (int) sample->data.size() : 0; }

private:
    void computeBounds() noexcept
    {
        const auto n = (double) (length() - 1);
        auto s = std::clamp (region.start, 0.0, 1.0), e = std::clamp (region.end, 0.0, 1.0);
        if (e < s) std::swap (s, e);
        if (e - s < 1.0e-4) e = std::min (1.0, s + 1.0e-4);
        lo = s * n; hi = e * n;
        auto ls = std::clamp (region.loopStart, s, e), le = std::clamp (region.loopEnd, s, e);
        if (le - ls < 1.0e-4) { ls = s; le = e; }
        lLo = ls * n; lHi = le * n;
        fadeLen = std::min (0.005 * fileRate(), 0.25 * (lHi - lLo));
    }

    void read (double p, double& l, double& r) const noexcept
    {
        const auto& d = sample->data;
        const auto n = (int) d.size();
        const auto i = (int) std::floor (p);
        const auto f = p - (double) i;
        auto h = [&] (const std::vector<float>& v)
        {
            auto at = [&] (int k) { return (double) v[(size_t) std::clamp (k, 0, n - 1)]; };
            const auto xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const auto c1 = 0.5 * (x1 - xm1);
            const auto c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
            const auto c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        };
        l = h (d);
        r = sample->right.size() == d.size() ? h (sample->right) : l;
    }

    const SampleBuffer* sample = nullptr;
    Region region;
    double pos = 0.0, dir = 1.0, lo = 0.0, hi = 0.0, lLo = 0.0, lHi = 0.0, fadeLen = 0.0;
    int fadeInLen = 0, fadeInLeft = 0;
    bool active = false, wrapped = false;
};

/** Découpage automatique d'un échantillon : parts égales ou sur les transitoires.
    Fil de l'interface. Renvoie au plus maxSlices paires (début, fin) en fractions. */
inline std::vector<std::pair<float, float>> computeSlices (const SampleBuffer& s, bool onTransients, int maxSlices)
{
    std::vector<std::pair<float, float>> out;
    const auto n = (int) s.data.size();
    maxSlices = std::clamp (maxSlices, 1, 16);
    if (n < 64) return out;
    std::vector<int> starts;
    if (! onTransients)
        for (int k = 0; k < maxSlices; ++k) starts.push_back ((int) ((double) k * n / maxSlices));
    else
    {
        // flux d'énergie par fenêtres de 512 ; pics au-dessus d'un seuil adaptatif, espacés d'au moins 60 ms
        constexpr int hop = 512;
        std::vector<double> e;
        for (int i = 0; i + hop <= n; i += hop)
        {
            double sum = 0.0;
            for (int k = 0; k < hop; ++k) sum += (double) s.data[(size_t) (i + k)] * s.data[(size_t) (i + k)];
            e.push_back (std::sqrt (sum / hop));
        }
        std::vector<std::pair<double, int>> cand;
        for (size_t i = 1; i < e.size(); ++i)
        {
            const auto flux = std::max (0.0, e[i] - e[i - 1]);
            double local = 0.0;
            for (size_t k = i >= 8 ? i - 8 : 0; k < i; ++k) local += e[k];
            local /= (double) std::max<size_t> (1, std::min<size_t> (8, i));
            if (flux > 0.02 && e[i] > 1.6 * local) cand.push_back ({ flux, (int) i * hop });
        }
        std::sort (cand.begin(), cand.end(), [] (auto& a, auto& b) { return a.first > b.first; });
        const auto minGap = (int) (0.06 * s.sampleRate);
        starts.push_back (0);
        for (auto& c : cand)
        {
            if ((int) starts.size() >= maxSlices) break;
            bool ok = true;
            for (auto st : starts) if (std::abs (st - c.second) < minGap) { ok = false; break; }
            if (ok) starts.push_back (c.second);
        }
        std::sort (starts.begin(), starts.end());
    }
    for (size_t k = 0; k < starts.size(); ++k)
    {
        const auto a = (float) starts[k] / (float) n;
        const auto b = k + 1 < starts.size() ? (float) starts[k + 1] / (float) n : 1.0f;
        out.push_back ({ a, b });
    }
    return out;
}

/** Attaques d'un échantillon (fractions de la longueur, triées, la première vaut 0), 64 au plus,
    espacées d'au moins 40 ms. Fil de l'interface (au chargement). */
inline std::vector<float> computeOnsets (const SampleBuffer& s, int maxOnsets = 64)
{
    std::vector<float> out;
    const auto n = (int) s.data.size();
    if (n < 1024) return out;
    constexpr int hop = 256;
    std::vector<double> e;
    for (int i = 0; i + hop <= n; i += hop)
    {
        double sum = 0.0;
        for (int k = 0; k < hop; ++k)
        {
            const auto l = (double) s.data[(size_t) (i + k)];
            const auto r = s.right.size() == s.data.size() ? (double) s.right[(size_t) (i + k)] : l;
            sum += 0.5 * (l * l + r * r);
        }
        e.push_back (std::sqrt (sum / hop));
    }
    double peak = 1.0e-9;
    for (auto v : e) peak = std::max (peak, v);
    std::vector<std::pair<double, int>> cand;
    for (size_t i = 1; i < e.size(); ++i)
    {
        const auto flux = std::max (0.0, e[i] - e[i - 1]);
        double local = 0.0;
        for (size_t k = i >= 8 ? i - 8 : 0; k < i; ++k) local += e[k];
        local /= (double) std::max<size_t> (1, std::min<size_t> (8, i));
        if (flux > 0.04 * peak && e[i] > 1.5 * local) cand.push_back ({ flux, (int) (i - 1) * hop });
    }
    std::sort (cand.begin(), cand.end(), [] (auto& a, auto& b) { return a.first > b.first; });
    const auto minGap = (int) (0.04 * s.sampleRate);
    std::vector<int> starts { 0 };
    for (auto& c : cand)
    {
        if ((int) starts.size() >= maxOnsets) break;
        bool ok = true;
        for (auto st : starts) if (std::abs (st - c.second) < minGap) { ok = false; break; }
        if (ok) starts.push_back (c.second);
    }
    std::sort (starts.begin(), starts.end());
    for (auto st : starts) out.push_back ((float) st / (float) n);
    return out;
}

} // namespace grv::synth
