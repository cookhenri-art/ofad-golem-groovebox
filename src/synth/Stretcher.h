/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Lecture d'échantillon avec étirement (principe des samplers de performance) :
      - « Non » : lecture rééchantillonnée — la hauteur suit la vitesse ;
      - « Normal » : grains de 46 ms en fenêtre de Hann, recouverts à moitié ; la position avance au
        tempo, la hauteur se règle à part ;
      - « Beat » : un grain par double croche de l'échantillon, démarré pile sur la grille : les
        attaques restent nettes (boucles de batterie), fondu court entre deux grains.
    Lecture avant, arrière, boucle avant ou aller-retour, sur une région [début, fin]. Ne possède
    pas les données : lit une vue (tampon du moteur ou de l'enregistreur). Fil audio, sans allocation.
    v0.40 : point de boucle (le premier passage part du début, les suivants du point de boucle),
    fondu de boucle réglable (aussi en lecture simple : plus de clic à la jointure), nombre de tours
    (ensuite la lecture va à la fin et s'arrête), distance à la fin (fondu de sortie).
*/

#pragma once

#include "SynthCore.h"
#include "../engine/Module.h"

namespace grv::synth
{

/** Vue en lecture seule sur un tampon audio stéréo (ou mono : right == nullptr). */
struct AudioView
{
    const float* left = nullptr;
    const float* right = nullptr;
    int length = 0;
    double sampleRate = 44100.0;

    bool valid() const noexcept { return left != nullptr && length > 4; }
    static AudioView of (const grv::engine::SampleBuffer* s) noexcept
    {
        AudioView v;
        if (s == nullptr || s->data.size() < 5) return v;
        v.left = s->data.data();
        v.right = s->right.size() == s->data.size() ? s->right.data() : nullptr;
        v.length = (int) s->data.size();
        v.sampleRate = s->sampleRate;
        return v;
    }
};

class Stretcher
{
public:
    enum Mode { off = 0, normal, beat };
    enum Loop { noLoop = 0, loopForward, loopPingPong };

    struct Setup
    {
        double start = 0.0, end = 1.0;   // fractions de la longueur de la vue
        Loop loop = noLoop;
        Mode mode = off;
        bool reverse = false;
        double sliceSamples = 0.0;       // mode Beat : durée d'une double croche de l'échantillon (images)
        double loopStart = -1.0;         // v0.40 : point de boucle (fraction de la vue ; < 0 : le début)
        double xfade = 0.005;            // v0.40 : fondu de boucle (secondes de l'échantillon)
        int loopCount = 0;               // v0.40 : tours de boucle (0 : sans fin)
    };

    void prepare (double sampleRate) noexcept { sr = sampleRate; grainLen = std::max (64, (int) (0.046 * sr)); }

    void start (const AudioView& v, const Setup& s) noexcept
    {
        view = v;
        setup = s;
        active = view.valid();
        if (! active) return;
        const auto n = (double) (view.length - 1);
        auto a = std::clamp (s.start, 0.0, 1.0), b = std::clamp (s.end, 0.0, 1.0);
        if (b < a) std::swap (a, b);
        if (b - a < 1.0e-5) b = std::min (1.0, a + 1.0e-5);
        lo = a * n; hi = b * n;
        llo = s.loopStart > a && s.loopStart < b ? s.loopStart * n : lo;
        if (hi - llo < 32.0) llo = lo;
        lo0 = lo; hi0 = hi; llo0 = llo; offset = 0.0; // v0.41 : région d'origine (glissement)
        loops = 0; loopMode = s.loop;
        pos = s.reverse ? hi : lo;
        dir = s.reverse ? -1.0 : 1.0;
        for (auto& g : grains) g.on = false;
        untilNext = 0;
        lastSlice = -1;
        ending = false;
    }

    void stop() noexcept { active = false; for (auto& g : grains) g.on = false; }
    /** v0.41 : fait glisser la région jouée (début, fin, point de boucle et position) de d images,
        sans sortir de la vue. Appelée à chaque image, par petits pas : la lecture « frotte » le son
        (sans étirement) ou les grains suivants partent de la nouvelle place (étirements). */
    void shift (double d) noexcept
    {
        if (! active || d == 0.0) return;
        // décalage cumulé depuis le départ : la région d'origine glisse, bornée à la vue (elle se
        // raccourcit au bord sans se perdre : revenue en arrière, elle retrouve sa longueur)
        const auto last = (double) (view.length - 1);
        offset = std::clamp (offset + d, -hi0, last - lo0);
        lo = std::clamp (lo0 + offset, 0.0, last);
        hi = std::clamp (hi0 + offset, 0.0, last);
        if (hi - lo < 32.0) lo = std::max (0.0, hi - 32.0);
        llo = std::clamp (llo0 + offset, lo, hi);
        pos = std::clamp (pos + d, lo, hi);
    }
    bool isActive() const noexcept { return active; }
    /** Position de lecture, en fraction de la vue (affichage). */
    double position() const noexcept { return view.length > 1 ? pos / (double) (view.length - 1) : 0.0; }
    /** v0.40 : images de l'échantillon jusqu'à la fin de la lecture (très grand tant que ça boucle). */
    double toEnd() const noexcept
    {
        if (! active) return 0.0;
        if (loopMode != noLoop) return 1.0e12;
        return dir > 0.0 ? std::max (0.0, hi - pos) : std::max (0.0, pos - lo);
    }

    /** Une image. posInc : avance de la position (images de la vue par image de sortie, signe =
        sens) ; pitch : rapport de hauteur des grains (modes étirés). */
    void next (double posInc, double pitch, double& l, double& r) noexcept
    {
        l = r = 0.0;
        if (! active) return;
        if (setup.mode == off)
        {
            if (ending) { active = false; return; }
            const auto inc = posInc * pitch;
            if (std::abs (inc) < 1.0e-6) return; // vitesse nulle : arrêt sur image, sans continu
            read (pos, l, r);
            // v0.40 : fondu de boucle — avant la jointure, la fin se mêle à ce qui précède le point de reprise
            if (loopMode == loopForward && dir * (inc >= 0.0 ? 1.0 : -1.0) > 0.0)
            {
                const auto span = hi - llo, F = std::min (setup.xfade * view.sampleRate, 0.5 * span);
                const auto toSeam = hi - pos;
                if (F > 2.0 && toSeam >= 0.0 && toSeam < F && llo - F >= 0.0)
                {
                    const auto t = 1.0 - toSeam / F;
                    double l2, r2;
                    read (pos - span, l2, r2);
                    const auto ka = std::sqrt (1.0 - t), kb = std::sqrt (t);
                    l = l * ka + l2 * kb; r = r * ka + r2 * kb;
                }
            }
            advance (inc);
            return;
        }

        const auto absInc = std::abs (posInc);
        const auto effDir = (posInc < 0.0 ? -1.0 : 1.0) * dir;
        const auto gInc = pitch * effDir;
        if (setup.mode == normal)
        {
            if (--untilNext <= 0 && ! ending)
            {
                launch (pos, gInc, grainLen, grainLen / 2, grainLen / 2);
                untilNext = grainLen / 2;
            }
        }
        else
        {
            // Beat : un grain à chaque double croche franchie par la position
            const auto slice = std::max (1.0, setup.sliceSamples);
            const auto rel = (pos - lo) / slice;
            const auto k = (long long) std::floor (rel + 1.0e-9);
            if ((k != lastSlice || untilNext <= 0) && ! ending)
            {
                const auto at = lo + (double) k * slice;
                // le grain joue sa double croche et rien d'autre : ralenti, il laisse un silence avant la suivante
                const auto len = (int) std::clamp (std::min (slice / std::max (1.0e-6, absInc), slice / std::max (1.0e-6, pitch)), 32.0, 8.0 * sr);
                const auto period = (int) std::clamp (slice / std::max (1.0e-6, absInc), 32.0, 8.0 * sr);
                const auto fadeIn = std::max (1, (int) (0.0002 * sr)), fadeOut = (int) (0.006 * sr); // attaque presque franche : les transitoires restent
                for (auto& g : grains) if (g.on) g.beginFade (fadeOut); // le grain précédent s'efface
                // sans silence, le grain déborde du temps du fondu ; avec un silence, il s'éteint avant la double croche suivante
                launch (k == lastSlice || effDir < 0.0 ? pos : at, gInc, period > len ? len : len + fadeOut, fadeIn, fadeOut);
                lastSlice = k;
                untilNext = period;
            }
            --untilNext;
        }

        bool any = false;
        for (auto& g : grains)
        {
            if (! g.on) continue;
            any = true;
            double gl = 0.0, gr = 0.0;
            // sans boucle, un grain ne lit rien au-delà de la région (la découpe suivante ne déborde pas)
            if (loopMode != noLoop || (g.src >= lo && g.src <= hi)) read (g.src, gl, gr);
            if (loopMode == loopForward)
            {
                // v0.22 : un grain qui franchit la jointure de la boucle y passe en fondu (comme la
                // tête de lecture) : matière d'avant la boucle si elle existe, sinon d'après (fondu
                // juste après la jointure), sinon fondu vers la valeur du point de reprise
                const auto llw = g.inc >= 0.0 ? llo : lo; // à l'envers : toute la région
                const auto span = hi - llw, F = std::min (std::max (0.001, setup.xfade) * sr, 0.25 * span);
                const auto last = (double) (view.length - 1);
                const bool fwd = g.inc >= 0.0;
                const bool behind = fwd ? llw >= F + 2.0 : hi + F + 2.0 <= last;   // matière avant la jointure
                const bool ahead = fwd ? hi + F + 2.0 <= last : llw >= F + 2.0;     // matière après la jointure
                const auto toSeam = fwd ? hi - g.src : g.src - llw, fromSeam = fwd ? g.src - llw : hi - g.src;
                double t = -1.0, other = 0.0;
                if (F > 2.0 && (behind || ! ahead) && toSeam >= 0.0 && toSeam < F)
                {
                    t = 1.0 - toSeam / F;
                    other = std::clamp (fwd ? g.src - span : g.src + span, behind ? 0.0 : llw, behind ? last : hi);
                }
                else if (F > 2.0 && ! behind && ahead && g.wrapped && fromSeam >= 0.0 && fromSeam < F)
                {
                    t = 1.0 - fromSeam / F;
                    other = fwd ? g.src + span : g.src - span;
                }
                if (t >= 0.0)
                {
                    double l2, r2;
                    read (other, l2, r2);
                    const auto ka = std::sqrt (1.0 - t), kb = std::sqrt (t);
                    gl = gl * ka + l2 * kb; gr = gr * ka + r2 * kb;
                }
            }
            const auto w = g.window();
            l += gl * w; r += gr * w;
            g.src += g.inc;
            if (loopMode == loopForward)
            {
                if (g.src > hi) { g.src -= (hi - llo); g.wrapped = true; }
                if (g.src < lo) { g.src += (hi - lo); g.wrapped = true; }
            }
            else if (loopMode == loopPingPong)
            {
                if (g.src > hi) { g.src = hi - (g.src - hi); g.inc = -g.inc; }
                else if (g.src < llo) { g.src = llo + (llo - g.src); g.inc = -g.inc; }
            }
            if (++g.age >= g.len) g.on = false;
        }
        if (! ending) advance (posInc);
        else if (! any) active = false;
    }

private:
    struct Grain
    {
        double src = 0.0, inc = 1.0;
        int age = 0, len = 1, fadeIn = 1, fadeOut = 1;
        bool on = false, wrapped = false;
        double window() const noexcept
        {
            // montée et descente en demi-cosinus (la somme de deux grains de Hann à mi-chemin vaut 1)
            if (age < fadeIn) return 0.5 - 0.5 * std::cos (kPi * (double) age / (double) fadeIn);
            const auto toEnd = len - age;
            if (toEnd < fadeOut) return 0.5 - 0.5 * std::cos (kPi * (double) toEnd / (double) fadeOut);
            return 1.0;
        }
        void beginFade (int f) noexcept
        {
            const auto remaining = len - age;
            if (remaining > f) { len = age + f; fadeOut = f; }
        }
    };

    void launch (double src, double inc, int len, int fadeIn, int fadeOut) noexcept
    {
        Grain* slot = nullptr;
        for (auto& g : grains) if (! g.on) { slot = &g; break; }
        if (slot == nullptr) { slot = &grains[0]; for (auto& g : grains) if (g.age > slot->age) slot = &g; }
        slot->src = src; slot->inc = inc; slot->age = 0; slot->wrapped = false;
        slot->len = std::max (2, len); slot->fadeIn = std::max (1, fadeIn); slot->fadeOut = std::max (1, fadeOut);
        slot->on = true;
    }

    /** Avance la position de lecture dans la région (boucles, fin). */
    void advance (double inc) noexcept
    {
        pos += inc * dir;
        if (loopMode == noLoop)
        {
            if (pos > hi || pos < lo) { ending = true; pos = std::clamp (pos, lo, hi); }
            return;
        }
        // v0.40 : un tour de plus ; le dernier tour fait, la lecture va à la fin (sans boucle)
        auto lap = [this] { if (setup.loopCount > 0 && ++loops >= setup.loopCount) loopMode = noLoop; };
        if (loopMode == loopForward)
        {
            // au bouclage seulement, la grille du mode Beat repart (même double croche possible) ;
            // en avant, la boucle reprend au point de boucle ; à l'envers, toute la région
            if (pos > hi) { const auto span = std::max (1.0, hi - llo); lap(); if (loopMode != noLoop) { pos -= span * std::ceil ((pos - hi) / span); lastSlice = -1; } else { ending = true; pos = hi; } }
            else if (pos < lo) { const auto span = std::max (1.0, hi - lo); lap(); if (loopMode != noLoop) { pos += span * std::ceil ((lo - pos) / span); lastSlice = -1; } else { ending = true; pos = lo; } }
        }
        else
        {
            if (pos > hi) { pos = hi - (pos - hi); dir = -dir; lastSlice = -1; lap(); }
            else if (pos < llo) { pos = llo + (llo - pos); dir = -dir; lastSlice = -1; lap(); }
        }
    }

    void read (double p, double& l, double& r) const noexcept
    {
        if (p < 0.0 || p > (double) (view.length - 1)) { l = r = 0.0; return; }
        const auto n = view.length;
        const auto i = (int) p;
        const auto f = p - (double) i;
        auto h = [&] (const float* v)
        {
            auto at = [&] (int k) { return (double) v[std::clamp (k, 0, n - 1)]; };
            const auto xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const auto c1 = 0.5 * (x1 - xm1);
            const auto c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
            const auto c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        };
        l = h (view.left);
        r = view.right != nullptr ? h (view.right) : l;
    }

    AudioView view;
    Setup setup;
    std::array<Grain, 3> grains {};
    double sr = 48000.0, pos = 0.0, dir = 1.0, lo = 0.0, hi = 0.0, llo = 0.0;
    double lo0 = 0.0, hi0 = 0.0, llo0 = 0.0, offset = 0.0;
    Loop loopMode = noLoop; // v0.40 : la boucle en cours (passe à « sans » après le dernier tour)
    int loops = 0;
    int grainLen = 2048, untilNext = 0;
    long long lastSlice = -1;
    bool active = false, ending = false;
};

} // namespace grv::synth
