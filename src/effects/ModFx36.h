/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Modulations v0.36, chacune sur son principe (architecture seulement, aucune reprise de code,
    de façade ni de réglages d'usine) :

      - « Flanger » (principe des flangers à seau BBD des pédales des années 70) : une ligne courte
        (0,2 à 12 ms) balayée par un LFO triangle sur une échelle exponentielle (le creux descend
        régulièrement à l'oreille), réinjection positive ou négative (résonance « métallique »),
        mode « Matrice » (le LFO s'arrête : le Manuel fixe le peigne, comme le filtre matrix de ces
        pédales), mode « Inversé » (le signal retardé est inversé : creux plus profonds, son « jet »),
        filtre passe-bas d'anti-repliement du seau (Ton) et légère saturation dans la boucle.
      - « Phaser double » (principe des phasers doubles de studio : deux phasers de 12 étages, deux
        générateurs de balayage) : phaser A et phaser B, chacun 4 à 12 étages, centre, profondeur,
        vitesse (libre ou au tempo), réinjection ; routage série (A puis B), parallèle (A + B) ou
        stéréo (A à gauche, B à droite).
      - « Dimension » (principe des chorus spatiaux de studio des années 80 à 4 réglages) : deux
        lignes à seau modulées en opposition par un triangle lent ; chaque côté reçoit sa ligne et
        l'inverse atténué de l'autre : large, sans battement marqué, compatible mono. 4 réglages de
        plus en plus marqués (profondeur, vitesse), largeur, ton.

    Aucune latence ; lignes réservées dans prepare() ; lecture Hermite.
*/

#pragma once

#include "ModFx.h"
#include "FxBanks.h"

#include <array>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

/** Ligne courte modulée (Hermite), taille puissance de 2. */
struct ModLine
{
    std::vector<float> b;
    int mask = 0, w = 0;
    void init (int len) { int n = 1; while (n < len + 8) n <<= 1; b.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() noexcept { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    void push (double x) noexcept { b[(size_t) w] = (float) x; w = (w + 1) & mask; }
    double read (double d) const noexcept
    {
        d = std::clamp (d, 1.0, (double) mask - 4.0);
        const auto pos = (double) w - d;
        const auto fl = std::floor (pos);
        const auto i0 = (int) fl;
        const auto f = pos - fl;
        auto at = [&] (int j) { return (double) b[(size_t) (j & mask)]; };
        return hermite4 (at (i0 - 1), at (i0), at (i0 + 1), at (i0 + 2), f);
    }
};

//==============================================================================
namespace flanger
{
    enum Slot { manual = 0, sync, rate, division, depth, feedback, mode, tone, stereo, mix, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "flanger"; m.name = "Contrail"; m.role = "Flanger"; m.family = "Modulation"; m.form = "pedal";
            m.principle = "Flanger \xC3\xA0 seau (BBD) : ligne courte balay\xC3\xA9" "e, r\xC3\xA9injection \xC2\xB1, mode matrice (balayage fig\xC3\xA9), mode invers\xC3\xA9";
            m.params = {
                P ("manual", "Manuel", "ms", 0.2f, 12, 2.5f, 0.45f),
                C ("sync", "Synchro", { "Libre", "Tempo" }),
                P ("rate", "Vitesse", "Hz", 0.02f, 8, 0.25f, 0.4f),
                C ("division", "Division", syncDivisions(), 4),
                P ("depth", "Profondeur", "%", 0, 100, 60),
                P ("feedback", "R\xC3\xA9sonance", "%", -95, 95, 55),
                C ("mode", "Mode", { "Normal", "Invers\xC3\xA9", "Matrice" }),
                P ("tone", "Ton", "Hz", 2000, 18000, 9000, 0.4f),
                P ("stereo", "D\xC3\xA9" "calage st\xC3\xA9r\xC3\xA9o", "\xC2\xB0", 0, 180, 90),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 50),
            };
            m.sections = { { "flanger", "Flanger", "lfo", { "manual", "rate", "division", "depth", "feedback", "mode", "sync", "tone" }, "main" },
                           { "space", "Espace", "lfo", { "stereo", "mix" }, "main" } };
            m.presets = {
                { "Avion", {} },
                { "Lent et profond", { { "rate", 0.08f }, { "depth", 90 }, { "feedback", 75 }, { "manual", 4 } } },
                { "M\xC3\xA9tallique", { { "feedback", -85 }, { "manual", 1.2f }, { "depth", 40 } } },
                { "Matrice (peigne fig\xC3\xA9)", { { "mode", 2 }, { "manual", 1.6f }, { "feedback", 70 }, { "mix", 60 } } },
                { "Jet invers\xC3\xA9", { { "mode", 1 }, { "feedback", 30 }, { "depth", 80 }, { "mix", 55 } } },
                { "Cal\xC3\xA9 sur 2 mesures", { { "sync", 1 }, { "division", 3 }, { "depth", 75 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class Flanger final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return flanger::desc(); }
    void prepare (double s, int) override { sr = s; for (auto& l : line) l.init ((int) (0.03 * sr)); reset(); }
    void reset() noexcept override { for (auto& l : line) l.clear(); fb.fill (0.0); lp.fill (0.0); phase = 0.0; wetPrev = -1.0; curD.fill (-1.0); }
    void setPosition (double ppq, bool playing) noexcept override { hostPpq = ppq; hostPlaying = playing; }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace flanger;
        const bool synced = p[sync] > 0.5f;
        const auto md = std::clamp ((int) p[mode], 0, 2);
        const auto hz = synced ? (bpm / 60.0) / syncQuarters ((int) p[division]) : (double) p[rate];
        if (synced && hostPlaying) phase = std::fmod (hostPpq / syncQuarters ((int) p[division]), 1.0);
        const auto base = p[manual] * 0.001 * sr;
        const auto dpt = md == 2 ? 0.0 : p[depth] / 100.0;
        const auto fbk = std::clamp (p[feedback] / 100.0, -0.95, 0.95);
        const auto lpC = std::exp (-kTwoPi * std::min ((double) p[tone], 0.45 * sr) / sr);
        const auto offs = p[stereo] / 360.0;
        const auto sign = md == 1 ? -1.0 : 1.0;
        const auto wet = p[mix] / 100.0;
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n);
        wetPrev = wet;
        const auto glide = 1.0 - std::exp (-1.0 / (0.02 * sr)); // le Manuel glisse (pas de clic)
        for (int i = 0; i < n; ++i)
        {
            const auto wt = wet0 + dWet * (double) (i + 1);
            phase += hz / sr; if (phase >= 1.0) phase -= 1.0;
            const double in[2] { L[i], R[i] };
            double out[2];
            for (int c = 0; c < 2; ++c)
            {
                auto ph = phase + (c == 1 ? offs : 0.0); ph -= std::floor (ph);
                const auto tri = 1.0 - 4.0 * std::abs (ph - 0.5);              // -1 … 1
                const auto target = std::clamp (base * std::pow (2.0, tri * dpt * 2.5), 0.25, 0.028 * sr);
                auto& cd = curD[(size_t) c];
                cd = cd < 0.0 ? target : cd + (target - cd) * glide;
                // seau : entrée + réinjection (saturée), sortie filtrée (anti-repliement)
                line[(size_t) c].push (std::tanh (in[c] + fbk * fb[(size_t) c]));
                auto y = line[(size_t) c].read (cd);
                lp[(size_t) c] = y + (lp[(size_t) c] - y) * lpC;
                y = lp[(size_t) c];
                fb[(size_t) c] = y;
                // somme direct + retardé (inversé en mode Inversé) : peigne ; niveau compensé
                out[c] = in[c] * (1.0 - 0.5 * wt) + sign * y * 0.5 * wt;
            }
            L[i] = (float) out[0]; R[i] = (float) out[1];
        }
    }

private:
    std::array<ModLine, 2> line;
    std::array<double, 2> fb {}, lp {}, curD { -1.0, -1.0 };
    double sr = 48000.0, phase = 0.0, hostPpq = 0.0, wetPrev = -1.0;
    bool hostPlaying = false;
};

//==============================================================================
namespace phaser2
{
    enum Slot { routing = 0, sync, stagesA, rateA, divisionA, depthA, centerA, feedbackA,
                stagesB, rateB, divisionB, depthB, centerB, feedbackB, mix, numSlots };
    inline const std::vector<std::string>& stageChoices() { static const std::vector<std::string> s { "4", "6", "8", "12" }; return s; }
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "phaser2"; m.name = "Twinspin"; m.role = "Phaser double"; m.family = "Modulation"; m.form = "pedal";
            m.principle = "Deux phasers de 4 \xC3\xA0 12 \xC3\xA9tages, deux balayages (libres ou au tempo), s\xC3\xA9rie, parall\xC3\xA8le ou st\xC3\xA9r\xC3\xA9o";
            m.params = {
                C ("routing", "Routage", { "S\xC3\xA9rie", "Parall\xC3\xA8le", "St\xC3\xA9r\xC3\xA9o" }, 1),
                C ("sync", "Synchro", { "Libre", "Tempo" }),
                C ("stagesA", "\xC3\x89tages A", stageChoices(), 3),
                P ("rateA", "Vitesse A", "Hz", 0.02f, 10, 0.3f, 0.4f),
                C ("divisionA", "Division A", syncDivisions(), 4),
                P ("depthA", "Profondeur A", "%", 0, 100, 70),
                P ("centerA", "Centre A", "Hz", 100, 8000, 700, 0.4f),
                P ("feedbackA", "R\xC3\xA9injection A", "%", -95, 95, 45),
                C ("stagesB", "\xC3\x89tages B", stageChoices(), 1),
                P ("rateB", "Vitesse B", "Hz", 0.02f, 10, 0.47f, 0.4f),
                C ("divisionB", "Division B", syncDivisions(), 5),
                P ("depthB", "Profondeur B", "%", 0, 100, 60),
                P ("centerB", "Centre B", "Hz", 100, 8000, 1600, 0.4f),
                P ("feedbackB", "R\xC3\xA9injection B", "%", -95, 95, 20),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 50),
            };
            m.sections = {
                { "a", "Phaser A", "lfo", { "stagesA", "rateA", "divisionA", "depthA", "centerA", "feedbackA" }, "main" },
                { "b", "Phaser B", "lfo", { "stagesB", "rateB", "divisionB", "depthB", "centerB", "feedbackB" }, "main" },
                { "route", "Routage", "lfo", { "routing", "sync", "mix" }, "main" },
            };
            m.presets = {
                { "Deux vitesses", {} },
                { "S\xC3\xA9rie profond", { { "routing", 0 }, { "feedbackA", 70 }, { "feedbackB", 50 }, { "rateB", 0.13f } } },
                { "St\xC3\xA9r\xC3\xA9o tournant", { { "routing", 2 }, { "rateA", 0.21f }, { "rateB", 0.23f }, { "centerB", 700 } } },
                { "Cal\xC3\xA9 sur le tempo", { { "sync", 1 }, { "divisionA", 4 }, { "divisionB", 6 } } },
                { "Doux", { { "stagesA", 0 }, { "stagesB", 0 }, { "depthA", 40 }, { "depthB", 35 }, { "feedbackA", 10 }, { "feedbackB", 0 }, { "mix", 40 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class DualPhaser final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return phaser2::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override { for (auto& u : unit) { for (auto& c : u.ap) c.fill ({}); u.fb.fill (0.0); u.phase = 0.0; } wetPrev = -1.0; }
    void setPosition (double ppq, bool playing) noexcept override { hostPpq = ppq; hostPlaying = playing; }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace phaser2;
        static constexpr int stageCount[4] { 4, 6, 8, 12 };
        const bool synced = p[sync] > 0.5f;
        const auto rt = std::clamp ((int) p[routing], 0, 2);
        for (int u = 0; u < 2; ++u)
        {
            auto& un = unit[(size_t) u];
            const auto o = u * 6;
            un.stages = stageCount[std::clamp ((int) p[stagesA + o], 0, 3)];
            const auto div = (int) p[divisionA + o];
            un.hz = synced ? (bpm / 60.0) / syncQuarters (div) : (double) p[rateA + o];
            if (synced && hostPlaying) un.phase = std::fmod (hostPpq / syncQuarters (div) + 0.25 * u, 1.0);
            un.depth = p[depthA + o] / 100.0; un.center = p[centerA + o]; un.fbk = p[feedbackA + o] / 100.0;
        }
        const auto wet = p[mix] / 100.0;
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n);
        wetPrev = wet;
        for (int i = 0; i < n; ++i)
        {
            const auto wt = wet0 + dWet * (double) (i + 1);
            double a[2], c[2];
            for (int u = 0; u < 2; ++u) { auto& un = unit[(size_t) u]; un.phase += un.hz / sr; if (un.phase >= 1.0) un.phase -= 1.0; un.coef = coefOf (un); }
            const double in[2] { L[i], R[i] };
            double out[2];
            if (rt == 2)
            {
                // stéréo : A à gauche, B à droite (chaque côté garde son entrée)
                out[0] = run (unit[0], 0, in[0]);
                out[1] = run (unit[1], 1, in[1]);
            }
            else
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    a[ch] = run (unit[0], ch, in[ch]);
                    c[ch] = run (unit[1], ch, rt == 0 ? a[ch] : in[ch]);
                    out[ch] = rt == 0 ? c[ch] : 0.5 * (a[ch] + c[ch]);
                }
            }
            L[i] = (float) (in[0] * (1.0 - 0.5 * wt) + out[0] * 0.5 * wt);
            R[i] = (float) (in[1] * (1.0 - 0.5 * wt) + out[1] * 0.5 * wt);
        }
    }

private:
    struct St { double x1 = 0.0, y1 = 0.0; };
    struct Unit { std::array<std::array<St, 12>, 2> ap {}; std::array<double, 2> fb {}; double phase = 0.0, hz = 0.3, depth = 0.7, center = 700.0, fbk = 0.4, coef = 0.0; int stages = 12; };
    std::array<Unit, 2> unit;
    double sr = 48000.0, hostPpq = 0.0, wetPrev = -1.0;
    bool hostPlaying = false;

    double coefOf (const Unit& u) const noexcept
    {
        const auto lfo = 0.5 - 0.5 * std::cos (kTwoPi * u.phase);
        const auto fc = std::clamp (u.center * std::pow (2.0, (lfo - 0.5) * 6.0 * u.depth), 30.0, 0.45 * sr);
        const auto t = std::tan (kPi * fc / sr);
        return (t - 1.0) / (t + 1.0);
    }
    static double run (Unit& u, int ch, double x) noexcept
    {
        x += u.fbk * u.fb[(size_t) ch];
        for (int k = 0; k < u.stages; ++k)
        {
            auto& s = u.ap[(size_t) ch][(size_t) k];
            const auto y = u.coef * x + s.x1 - u.coef * s.y1;
            s.x1 = x; s.y1 = y; x = y;
        }
        u.fb[(size_t) ch] = std::tanh (x);
        return x;
    }
};

//==============================================================================
namespace dimension
{
    enum Slot { mode = 0, width, tone, mix, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "dimension"; m.name = "Expanse"; m.role = "Chorus spatial"; m.family = "Modulation"; m.form = "pedal";
            m.principle = "Chorus spatial \xC3\xA0 4 r\xC3\xA9glages : deux lignes \xC3\xA0 seau en opposition, crois\xC3\xA9" "es et invers\xC3\xA9" "es ; large et compatible mono";
            m.params = {
                C ("mode", "R\xC3\xA9glage", { "1", "2", "3", "4" }, 1),
                P ("width", "Largeur", "%", 0, 100, 100),
                P ("tone", "Ton", "Hz", 3000, 16000, 9500, 0.4f),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
            };
            m.sections = { { "dimension", "Dimension", "lfo", { "mode", "width", "tone", "mix" }, "main" } };
            m.presets = { { "R\xC3\xA9glage 2", {} }, { "R\xC3\xA9glage 1 (subtil)", { { "mode", 0 } } }, { "R\xC3\xA9glage 3", { { "mode", 2 } } },
                          { "R\xC3\xA9glage 4 (marqu\xC3\xA9)", { { "mode", 3 } } }, { "Nappe large", { { "mode", 2 }, { "mix", 80 } } } };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class Dimension final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return dimension::desc(); }
    void prepare (double s, int) override { sr = s; for (auto& l : line) l.init ((int) (0.02 * sr)); reset(); }
    void reset() noexcept override { for (auto& l : line) l.clear(); lp.fill (0.0); phase = 0.0; wetPrev = -1.0; depthCur = rateCur = -1.0; }

    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace dimension;
        // réglages 1 à 4 : (vitesse Hz, profondeur ms) de plus en plus marqués ; le 4 est plus rapide
        static constexpr double rateOf[4] { 0.25, 0.25, 0.25, 0.5 }, depthOf[4] { 0.35, 0.7, 1.2, 1.2 };
        const auto md = std::clamp ((int) p[mode], 0, 3);
        const auto wd = p[width] / 100.0;
        const auto lpC = std::exp (-kTwoPi * std::min ((double) p[tone], 0.45 * sr) / sr);
        const auto wet = p[mix] / 100.0;
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n);
        wetPrev = wet;
        if (depthCur < 0.0) { depthCur = depthOf[md]; rateCur = rateOf[md]; }
        const auto base = 0.0056 * sr;
        for (int i = 0; i < n; ++i)
        {
            const auto wt = wet0 + dWet * (double) (i + 1);
            depthCur += (depthOf[md] - depthCur) * 1.0e-4; rateCur += (rateOf[md] - rateCur) * 1.0e-4; // changement de réglage sans saut
            phase += rateCur / sr; if (phase >= 1.0) phase -= 1.0;
            const auto tri = 1.0 - 4.0 * std::abs (phase - 0.5);
            const auto dS = depthCur * 0.001 * sr;
            const auto mono = 0.5 * ((double) L[i] + R[i]);
            line[0].push (mono); line[1].push (mono);
            double y[2];
            for (int c = 0; c < 2; ++c)
            {
                const auto d = base + dS * (c == 0 ? tri : -tri);   // les deux lignes en opposition
                const auto v = line[(size_t) c].read (d);
                lp[(size_t) c] = v + (lp[(size_t) c] - v) * lpC;
                y[c] = lp[(size_t) c];
            }
            // chaque côté : sa ligne moins l'autre atténuée (largeur) ; la somme mono reste pleine
            const auto wl = y[0] - 0.6 * wd * y[1], wr = y[1] - 0.6 * wd * y[0];
            const auto norm = 1.0 / (1.0 - 0.3 * wd);
            L[i] = (float) (L[i] * (1.0 - 0.35 * wt) + wl * 0.55 * norm * wt);
            R[i] = (float) (R[i] * (1.0 - 0.35 * wt) + wr * 0.55 * norm * wt);
        }
    }

private:
    std::array<ModLine, 2> line;
    std::array<double, 2> lp {};
    double sr = 48000.0, phase = 0.0, wetPrev = -1.0, depthCur = -1.0, rateCur = -1.0;
};

} // namespace grv::effects
