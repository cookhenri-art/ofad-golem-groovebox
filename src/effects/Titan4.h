/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 — famille « Golem Titan FX », lot G4 : modulations et filtres haut de gamme.
    Fiche d'architecture : claude/P3/SPEC-GOLEM-TITAN-FX.md (projet).

      - Titan Phaseur (principe des phaseurs « collection de circuits ») : 2 à 24 étages passe-tout,
        styles de circuits (écart des étages, signe de la réinjection, couleur), résonance, couleur
        (filtre dans la boucle), intensité, décalage stéréo, saturation d'entrée ; 6 modulations :
        LFO (4 formes, libre ou au tempo), Rythme (motifs de 16 pas), Pas (valeurs tirées au tempo),
        Aléatoire (lissé), Enveloppe (suiveur), Déclenchée (enveloppe sur chaque attaque).

      - Titan Flanger-Doubleur (principe du flanger à bandes et du doubleur de voix) :
          · flanger : balayage auto, manuel, « VSO » (on freine la seconde bande ; servo qui rebondit),
            ou par l'enveloppe ; polarité ± ; passage par zéro (le son direct est retardé de 5 ms,
            latence déclarée : le retard balayé croise le son direct, annulation complète) ; régénération ;
            stéréo (balayage inversé à droite) ;
          · doubleur : deux voix décalées (0–50 ms) à hauteur légèrement modulée, régénération, et
            « doublage auto » : voix à 3 ms décorrélée par une chaîne de passe-tout lentement modulés
            (épaissit sans l'effet de filtre en peigne d'un flanger).

      - Titan Filtre (principe des filtres créatifs analogiques à styles non linéaires) : 2 filtres en
        série, parallèle ou stéréo ; 8 types (passe-bas, passe-haut, passe-bande, réjection, cloche,
        plateaux, passe-tout), pente de 6 à 48 dB/oct, résonance jusqu'à l'auto-oscillation, 9 styles
        de non-linéarité (propre à métal) ; modulation (LFO, rythme 16 pas, suiveur, aléatoire,
        déclenchée) vers l'un, l'autre ou les deux ; saturation de sortie à 7 styles (jusqu'au pompage).

    Aucune allocation dans le rendu. Latence nulle sauf flanger en passage par zéro (5 ms).
*/

#pragma once

#include "Titan2.h"
#include <atomic>

namespace grv::effects
{

namespace titan
{
    inline const std::vector<std::string>& lfoShapes() { static const std::vector<std::string> v { "Sinus", "Triangle", "Rampe", "Carr\xC3\xA9 arrondi" }; return v; }
    inline double lfoShape (int s, double ph) noexcept
    {
        switch (s)
        {
            case 1: return ph < 0.5 ? 2.0 * ph : 2.0 - 2.0 * ph;
            case 2: return ph;
            case 3: return 0.5 + 0.5 * std::tanh (6.0 * dsp::fastSin2Pi (ph));
            default: return 0.5 + 0.5 * dsp::fastSin2Pi (ph);
        }
    }
    /** Motifs de 16 pas (0..1) du mode Rythme. */
    inline const std::vector<std::string>& rhythmNames() { static const std::vector<std::string> v { "Croches", "Galop", "Contretemps", "Escalier", "Pompe", "Clave", "Vague", "Hachures" }; return v; }
    inline double rhythmValue (int pat, int step) noexcept
    {
        static constexpr double r[8][16] {
            { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 },
            { 1, 0, .6, 1, 0, .6, 1, 0, .6, 1, 0, .6, 1, 0, .6, 0 },
            { 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0 },
            { 0, .14, .28, .42, .57, .71, .85, 1, 0, .14, .28, .42, .57, .71, .85, 1 },
            { 0, .3, .6, .9, 0, .3, .6, .9, 0, .3, .6, .9, 0, .3, .6, .9 },
            { 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0 },
            { .5, .69, .85, .96, 1, .96, .85, .69, .5, .31, .15, .04, 0, .04, .15, .31 },
            { 1, .2, .8, .1, 1, .3, .7, 0, 1, .2, .9, .1, .6, .4, 1, 0 },
        };
        return r[std::clamp (pat, 0, 7)][step & 15];
    }
    inline const std::vector<std::string>& modDivisions() { static const std::vector<std::string> v { "1/32", "1/16", "1/8", "1/4", "1/2", "1 mesure", "2 mesures", "4 mesures" }; return v; }
    inline double modDivQuarters (int i) noexcept { static constexpr double q[8] { 0.125, 0.25, 0.5, 1, 2, 4, 8, 16 }; return q[std::clamp (i, 0, 7)]; }

    /** Source de modulation commune (0..1) : LFO, rythme, pas, aléatoire, enveloppe, déclenchée. */
    struct ModSource
    {
        double ph = 0.0, smooth = 0.0, stepVal = 0.0, rndA = 0.0, rndB = 0.0, env = 0.0, trig = 0.0, fast = 0.0, slow = 0.0;
        int lastStep = -1; bool trigUp = false;
        Rand rnd;
        void reset() noexcept { ph = smooth = stepVal = rndA = rndB = env = trig = fast = slow = 0.0; lastStep = -1; trigUp = false; }
        /** mode : 0 LFO, 1 rythme, 2 pas, 3 aléatoire, 4 enveloppe, 5 déclenchée. ppqStep = position en pas (rythme). */
        double next (int mode, int shape, double inc, double level, double envAtt, double envRel, double sens, int pattern, double phaseOff, double smoothC) noexcept
        {
            ph += inc; if (ph >= 1.0) ph -= 1.0;
            double v = 0.0;
            switch (mode)
            {
                case 1: { const int st = (int) (ph * 16.0); v = rhythmValue (pattern, st); break; }
                case 2: { const int st = (int) (ph * 16.0); if (st != lastStep) { lastStep = st; stepVal = 0.5 + 0.5 * rnd.next(); } v = stepVal; break; }
                case 3: { const int st = (int) (ph * 4.0); if (st != lastStep) { lastStep = st; rndA = rndB; rndB = 0.5 + 0.5 * rnd.next(); }
                          const auto f = ph * 4.0 - st; v = rndA + (rndB - rndA) * (f * f * (3.0 - 2.0 * f)); break; }
                case 4: { env = level > env ? level + (env - level) * envAtt : level + (env - level) * envRel;
                          v = std::clamp (env * (0.5 + sens * 12.0), 0.0, 1.0); break; }
                case 5: { fast = level > fast ? level : level + (fast - level) * 0.995; slow = level + (slow - level) * 0.9995;
                          const bool hit = fast > slow * (1.6 - sens * 0.5) + 0.01;
                          if (hit && ! trigUp) trig = 1.0;
                          trigUp = hit;
                          trig *= envRel; v = trig; break; }
                default: { auto p2 = ph + phaseOff; p2 -= std::floor (p2); v = lfoShape (shape, p2); break; }
            }
            smooth = v + (smooth - v) * smoothC;
            return smooth;
        }
    };
}

//==============================================================================
// Titan Phaseur
namespace tphase
{
    enum Slot { style = 0, stages, freq, reso, depth, mode, rate, sync,
                division, shape, pattern, envSens, envRelease, color, intensity, stereo,
                drive, mix, numSlots };

    inline const std::vector<std::string>& styles()
    {
        static const std::vector<std::string> v { "Classique", "Doux", "Large", "Double", "Profond", "Vintage", "R\xC3\xA9sonant", "N\xC3\xA9gatif" };
        return v;
    }
    struct StyleDef { double spread, fbSign, colorMul, curve; };
    inline const StyleDef& styleDef (int i)
    {
        static const StyleDef s[8] {
            { 1.00, 1.0, 1.0, 1.0 }, { 1.15, 1.0, 0.6, 0.8 }, { 1.45, 1.0, 1.0, 1.0 }, { 1.25, 1.0, 1.2, 1.1 },
            { 1.10, 1.0, 0.8, 1.4 }, { 1.30, 1.0, 0.45, 0.9 }, { 1.05, 1.0, 1.5, 1.0 }, { 1.20, -1.0, 1.0, 1.0 },
        };
        return s[std::clamp (i, 0, 7)];
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tphaser"; m.name = "Titan Phaseur"; m.role = "Phaseur"; m.family = titan::family; m.form = "module";
            m.principle = "2 \xC3\xA0 24 \xC3\xA9tages passe-tout, 8 styles de circuits, r\xC3\xA9sonance, couleur, saturation ; LFO, rythme, pas, al\xC3\xA9" "atoire, enveloppe, d\xC3\xA9" "clench\xC3\xA9" "e ; sans latence";
            m.params = {
                C ("style", "Circuit", styles()),
                I ("stages", "\xC3\x89tages", "", 2, 24, 6),
                P ("freq", "Fr\xC3\xA9quence", "Hz", 40, 8000, 600, 0.3f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 40),
                P ("depth", "Profondeur", "%", 0, 100, 60),
                C ("mode", "Modulation", { "LFO", "Rythme", "Pas", "Al\xC3\xA9" "atoire", "Enveloppe", "D\xC3\xA9" "clench\xC3\xA9" "e" }),
                P ("rate", "Vitesse", "Hz", 0.02f, 10, 0.4f, 0.4f),
                C ("sync", "Au tempo", { "Non", "Oui" }),
                C ("division", "Division", titan::modDivisions(), 5),
                C ("shape", "Forme", titan::lfoShapes()),
                C ("pattern", "Motif", titan::rhythmNames()),
                P ("envSens", "Sensibilit\xC3\xA9", "%", 0, 100, 50),
                P ("envRelease", "Retomb\xC3\xA9" "e", "ms", 10, 2000, 250, 0.4f),
                P ("color", "Couleur", "Hz", 500, 20000, 9000, 0.4f),
                P ("intensity", "Intensit\xC3\xA9", "%", 0, 100, 100),
                P ("stereo", "D\xC3\xA9" "calage st\xC3\xA9r\xC3\xA9o", "\xC2\xB0", 0, 180, 90),
                P ("drive", "Saturation", "%", 0, 100, 0),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
            };
            m.sections = {
                { "phaser", "Phaseur", "mod", { "style", "stages", "freq", "reso", "depth", "intensity", "color", "mix" }, "global" },
                { "mod", "Modulation", "lfo", { "mode", "rate", "sync", "division", "shape", "pattern", "envSens", "envRelease" }, "global" },
                { "out", "St\xC3\xA9r\xC3\xA9o et saturation", "global", { "stereo", "drive" }, "global" },
            };
            m.presets = {
                { "Phaseur 6 \xC3\xA9tages", {} },
                { "Pédale 4 \xC3\xA9tages lente", { { "stages", 4 }, { "rate", 0.25f }, { "reso", 25 } } },
                { "12 \xC3\xA9tages profonds", { { "style", 4 }, { "stages", 12 }, { "reso", 65 }, { "depth", 80 } } },
                { "Funk \xC3\xA0 l'enveloppe", { { "mode", 4 }, { "envSens", 60 }, { "freq", 300 }, { "reso", 55 } } },
                { "Rythme au tempo", { { "mode", 1 }, { "sync", 1 }, { "division", 5 }, { "pattern", 1 } } },
                { "Robot en pas", { { "mode", 2 }, { "sync", 1 }, { "division", 4 }, { "reso", 70 } } },
                { "Batterie d\xC3\xA9" "clench\xC3\xA9" "e", { { "mode", 5 }, { "envSens", 50 }, { "envRelease", 180 }, { "stages", 8 } } },
                { "Vintage sale", { { "style", 5 }, { "drive", 50 }, { "stages", 4 } } },
                { "24 \xC3\xA9tages extr\xC3\xAAme", { { "stages", 24 }, { "reso", 80 }, { "style", 6 } } },
            };
            return m;
        }();
        return d;
    }
}

class TitanPhaser final : public Effect
{
public:
    static constexpr int maxStages = 24;
    const ModuleDesc& desc() const noexcept override { return tphase::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override
    {
        for (auto& c : ch) { c.z.fill (0.0); c.fbState = 0.0; c.last = 0.0; c.mod.reset(); }
        ctr = 0;
    }
    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace tphase;
        const auto& stl = styleDef ((int) p[style]);
        const int ns = std::clamp ((int) std::lround (p[stages]), 2, maxStages);
        const double f0 = p[freq], dep = p[depth] / 100.0;
        const double fbAmt = 0.92 * p[reso] / 100.0 * stl.fbSign;
        const int md = (int) p[mode];
        const bool synced = p[sync] > 0.5f && bpm > 1.0;
        const double inc = synced ? 1.0 / (titan::modDivQuarters ((int) p[division]) * 60.0 / bpm * sr) : p[rate] / sr;
        const double sens = p[envSens] / 100.0;
        const double envAtt = std::exp (-1.0 / (0.004 * sr)), envRel = std::exp (-1.0 / (p[envRelease] * 0.001 * sr));
        const double colC = std::exp (-kTwoPi * std::min ((double) p[color] * stl.colorMul, 0.45 * sr) / sr);
        const double inten = p[intensity] / 100.0, wet = p[mix] / 100.0;
        const double off = p[stereo] / 360.0;
        const double drv = p[drive] / 100.0, dg = 1.0 + 6.0 * drv * drv;
        const double smoothC = md == 1 || md == 2 ? std::exp (-1.0 / (0.004 * sr)) : 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double lvl = 0.5 * (std::abs ((double) L[i]) + std::abs ((double) R[i]));
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[c];
                const double m = s.mod.next (md, (int) p[shape], inc, lvl, envAtt, envRel, sens, (int) p[pattern], c == 1 ? off : 0.0, smoothC);
                if ((ctr & 7) == 0 || s.coef[0] == 0.0)
                {
                    // fréquence balayée (4 octaves à fond), écart des étages selon le circuit
                    const double fc = f0 * std::pow (2.0, dep * 4.0 * (std::pow (m, stl.curve) - 0.5));
                    double f = fc / std::pow (stl.spread, 0.5 * (ns - 1) / 2.0);
                    for (int k = 0; k < ns; ++k)
                    {
                        const auto w = std::tan (kPi * std::clamp (f, 20.0, 0.45 * sr) / sr);
                        s.coef[k] = (w - 1.0) / (w + 1.0);
                        if (k & 1) f *= stl.spread;
                    }
                }
                double x = c == 0 ? L[i] : R[i];
                const double dry = x;
                if (drv > 0.0) x = std::tanh (x * dg) / std::sqrt (dg);
                s.fbState = s.last + (s.fbState - s.last) * colC;
                double v = std::tanh (x + fbAmt * s.fbState);
                for (int k = 0; k < ns; ++k)
                {
                    // passe-tout du premier ordre (forme transposée)
                    const auto y = s.coef[k] * v + s.z[k];
                    s.z[k] = v - s.coef[k] * y;
                    v = y;
                }
                s.last = v;
                const double ph = x + inten * v;                     // creux : son direct + son déphasé
                const double out = ph / (1.0 + inten);
                (c == 0 ? L[i] : R[i]) = (float) (dry * (1.0 - wet) + out * wet);
            }
            ++ctr;
        }
    }
private:
    struct Chan { std::array<double, maxStages> z {}, coef {}; double fbState = 0.0, last = 0.0; titan::ModSource mod; };
    std::array<Chan, 2> ch;
    std::uint32_t ctr = 0;
    double sr = 48000.0;
};

//==============================================================================
// Titan Flanger-Doubleur
namespace tflg
{
    enum Slot { mode = 0, sweep, rate, sync, division, depth, manual, vso,
                offset, polarity, regen, throughZero, stereo, shape, envSens, mix,
                autoDouble, numSlots };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tflange"; m.name = "Titan Flanger-Doubleur"; m.role = "Flanger \xC3\xA0 bandes et doubleur"; m.family = titan::family; m.form = "module";
            m.principle = "Flanger \xC3\xA0 deux bandes (auto, manuel, VSO \xC3\xA0 servo, enveloppe), polarit\xC3\xA9, passage par z\xC3\xA9ro (5 ms de latence), r\xC3\xA9g\xC3\xA9n\xC3\xA9ration ; doubleur de voix et doublage auto \xC3\xA0 3 ms sans peigne";
            m.params = {
                C ("mode", "Appareil", { "Flanger", "Doubleur" }),
                C ("sweep", "Balayage", { "Auto", "Manuel", "VSO", "Enveloppe" }),
                P ("rate", "Vitesse", "Hz", 0.02f, 8, 0.25f, 0.4f),
                C ("sync", "Au tempo", { "Non", "Oui" }),
                C ("division", "Division", titan::modDivisions(), 5),
                P ("depth", "Profondeur", "%", 0, 100, 70),
                P ("manual", "Position manuelle", "%", 0, 100, 40),
                P ("vso", "VSO (vitesse de la bande)", "%", -100, 100, 0),
                P ("offset", "D\xC3\xA9" "calage", "ms", 0, 50, 2, 0.5f),
                C ("polarity", "Polarit\xC3\xA9", { "+", "\xE2\x88\x92" }),
                P ("regen", "R\xC3\xA9g\xC3\xA9n\xC3\xA9ration", "%", -95, 95, 30),
                C ("throughZero", "Passage par z\xC3\xA9ro", { "Non", "Oui" }),
                P ("stereo", "St\xC3\xA9r\xC3\xA9o", "%", 0, 100, 50),
                C ("shape", "Forme", titan::lfoShapes(), 1),
                P ("envSens", "Sensibilit\xC3\xA9", "%", 0, 100, 50),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 50),
                C ("autoDouble", "Doublage auto", { "Non", "Oui" }),
            };
            m.sections = {
                { "flanger", "Flanger", "mod", { "mode", "sweep", "rate", "sync", "division", "depth", "manual", "vso" }, "global" },
                { "tone", "Bandes et sortie", "global", { "offset", "polarity", "regen", "throughZero", "stereo", "shape", "envSens", "mix" }, "global" },
                { "double", "Doubleur", "global", { "autoDouble" }, "global" },
            };
            m.presets = {
                { "Flanger \xC3\xA0 bandes", {} },
                { "Passage par z\xC3\xA9ro", { { "throughZero", 1 }, { "polarity", 1 }, { "regen", 0 }, { "mix", 50 }, { "rate", 0.15f } } },
                { "Jet sur la batterie", { { "regen", 75 }, { "depth", 90 }, { "rate", 0.12f } } },
                { "VSO \xC3\xA0 la main", { { "sweep", 2 }, { "vso", 20 }, { "throughZero", 1 }, { "polarity", 1 } } },
                { "Flanger \xC3\xA0 l'enveloppe", { { "sweep", 3 }, { "envSens", 65 }, { "regen", 55 } } },
                { "Doubleur de voix", { { "mode", 1 }, { "offset", 18 }, { "depth", 35 }, { "rate", 0.6f }, { "mix", 45 } } },
                { "Doublage auto", { { "mode", 1 }, { "autoDouble", 1 }, { "mix", 50 } } },
                { "Doubleur large", { { "mode", 1 }, { "offset", 28 }, { "stereo", 100 }, { "depth", 45 }, { "mix", 50 } } },
            };
            return m;
        }();
        return d;
    }
}

class TitanFlanger final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return tflg::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& l : line) l.init ((int) (0.12 * sr) + 64);
        for (auto& l : dryLine) l.init ((int) (0.01 * sr) + 64);
        tzN = (int) (0.005 * sr);
        reset();
    }
    void reset() noexcept override
    {
        for (auto& l : line) l.clear();
        for (auto& l : dryLine) l.clear();
        for (int c = 0; c < 2; ++c) { mod[c].reset(); fb[c] = 0.0; for (auto& z : apz[c]) z = 0.0; for (auto& a : apa[c]) a = 0.0; walk[c] = 0.0; }
        vsoPos = 0.5; vsoVel = 0.0;
    }
    int latency() const noexcept override { return lat.load (std::memory_order_relaxed); }
    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace tflg;
        const bool dbl = p[mode] > 0.5f;
        const int sw = (int) p[sweep];
        const bool tz = ! dbl && p[throughZero] > 0.5f;
        lat.store (tz ? tzN : 0, std::memory_order_relaxed);
        const bool synced = p[sync] > 0.5f && bpm > 1.0;
        const double inc = synced ? 1.0 / (titan::modDivQuarters ((int) p[division]) * 60.0 / bpm * sr) : p[rate] / sr;
        const double dep = p[depth] / 100.0;
        const double pol = p[polarity] > 0.5f ? -1.0 : 1.0;
        const double rg = p[regen] / 100.0;
        const double st = p[stereo] / 100.0;
        const double wet = p[mix] / 100.0;
        const double sens = p[envSens] / 100.0;
        const double envAtt = std::exp (-1.0 / (0.003 * sr)), envRel = std::exp (-1.0 / (0.12 * sr));
        const bool autoD = dbl && p[autoDouble] > 0.5f;
        const double offMs = p[offset];
        const double maxD = (double) line[0].buf.size() - 8.0;
        // VSO : la vitesse de la seconde bande change sa position ; le servo freine et rebondit aux butées
        const double vsoTarget = p[vso] / 100.0 * 0.6;               // positions par seconde
        for (int i = 0; i < n; ++i)
        {
            const double lvl = 0.5 * (std::abs ((double) L[i]) + std::abs ((double) R[i]));
            if (sw == 2)
            {
                vsoVel += (vsoTarget - vsoVel) * (1.0 / (0.15 * sr)) + rnd.next() * 2.0e-6;
                vsoPos += vsoVel / sr;
                if (vsoPos < 0.0) { vsoPos = 0.0; vsoVel = std::abs (vsoVel) * 0.4; }   // rebond du servo
                if (vsoPos > 1.0) { vsoPos = 1.0; vsoVel = -std::abs (vsoVel) * 0.4; }
            }
            for (int c = 0; c < 2; ++c)
            {
                const double x = c == 0 ? L[i] : R[i];
                double pos;                                                 // 0..1 : position du balayage
                if (sw == 1) pos = p[manual] / 100.0;
                else if (sw == 2) pos = vsoPos;
                else if (sw == 3) pos = mod[c].next (4, 0, inc, lvl, envAtt, envRel, sens, 0, 0.0, 0.0);
                else pos = mod[c].next (0, (int) p[shape], inc, lvl, envAtt, envRel, sens, 0, 0.0, 0.0);
                if (c == 1 && st > 0.0 && sw != 1 && sw != 2) pos = pos + (1.0 - 2.0 * pos) * st; // droite balayée à l'envers
                double d, dryV;
                if (! dbl)
                {
                    if (tz)
                    {
                        // passage par zéro : le son direct attend 5 ms ; le retard balayé va de 0 à 10 ms
                        dryLine[c].push (x);
                        dryV = dryLine[c].at (tzN);
                        d = std::clamp (2.0 * tzN * (0.5 + (pos - 0.5) * dep), 1.0, maxD);
                    }
                    else
                    {
                        dryV = x;
                        d = std::clamp ((0.15 + offMs * 0.3 + 9.0 * pos * dep) * 0.001 * sr, 1.0, maxD);
                    }
                }
                else
                {
                    dryV = x;
                    // doubleur : petite dérive aléatoire + balayage lent, d'où une hauteur qui « vit »
                    walk[c] += rnd.next() * 1.0e-4 - walk[c] * 2.0e-5;
                    const double base = autoD ? 3.0 : std::max (3.0, offMs * (c == 1 ? 1.0 + 0.25 * st : 1.0));
                    d = std::clamp ((base + (pos - 0.5) * dep * 6.0 + walk[c] * 20.0) * 0.001 * sr, 1.0, maxD);
                }
                auto v = line[c].atFrac (d);
                if (autoD)
                {
                    // décorrélation : chaîne de passe-tout aux coefficients lentement modulés (pas de peigne)
                    for (int k = 0; k < 6; ++k)
                    {
                        apa[c][k] += ((0.35 + 0.25 * std::sin (k * 1.7 + c)) + walk[c] * 4.0 - apa[c][k]) * 1.0e-3;
                        const auto a = std::clamp (apa[c][k], -0.9, 0.9);
                        const auto y = a * v + apz[c][k];
                        apz[c][k] = v - a * y;
                        v = y;
                    }
                }
                line[c].push (std::clamp (x + rg * fb[c], -4.0, 4.0));
                fb[c] = v;
                const double out = dryV * (1.0 - wet) + pol * v * wet;
                (c == 0 ? L[i] : R[i]) = (float) out;
            }
        }
    }
private:
    std::array<DLine, 2> line, dryLine;
    titan::ModSource mod[2];
    titan::Rand rnd;
    double fb[2] {}, apz[2][6] {}, apa[2][6] {}, walk[2] {};
    double vsoPos = 0.5, vsoVel = 0.0;
    int tzN = 240;
    std::atomic<int> lat { 0 };
    double sr = 48000.0;
};

//==============================================================================
// Titan Filtre
namespace tfilt
{
    enum Slot { routing = 0,
                f1Type, f1Slope, f1Freq, f1Reso, f1Gain, f1Style, f1Drive,
                f2On, f2Type, f2Slope, f2Freq, f2Reso, f2Gain, f2Style, f2Drive,
                modSrc, modTarget, modDepth, rate, sync, division, shape, pattern,
                envAttack, envRelease, envSens,
                satStyle, satDrive, mix, output, numSlots };

    inline const std::vector<std::string>& types() { static const std::vector<std::string> v { "Passe-bas", "Passe-haut", "Passe-bande", "R\xC3\xA9jection", "Cloche", "Plateau grave", "Plateau aigu", "Passe-tout" }; return v; }
    inline const std::vector<std::string>& slopes() { static const std::vector<std::string> v { "6 dB/oct", "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }; return v; }
    inline const std::vector<std::string>& styles() { static const std::vector<std::string> v { "Propre", "Classique", "Doux", "Brut", "Dur", "Creux", "Extr\xC3\xAAme", "Lampe", "M\xC3\xA9tal" }; return v; }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tfilter"; m.name = "Titan Filtre"; m.role = "Filtres cr\xC3\xA9" "atifs"; m.family = titan::family; m.form = "module";
            m.principle = "2 filtres (s\xC3\xA9rie, parall\xC3\xA8le, st\xC3\xA9r\xC3\xA9o), 8 types, 6 \xC3\xA0 48 dB/oct, auto-oscillation, 9 styles non lin\xC3\xA9" "aires ; LFO, rythme, suiveur, al\xC3\xA9" "atoire, d\xC3\xA9" "clench\xC3\xA9" "e ; saturation \xC3\xA0 7 styles ; sans latence";
            m.params = {
                C ("routing", "Montage", { "S\xC3\xA9rie", "Parall\xC3\xA8le", "St\xC3\xA9r\xC3\xA9o (G / D)" }),
                C ("f1Type", "Filtre 1 : type", types()),
                C ("f1Slope", "Filtre 1 : pente", slopes(), 2),
                P ("f1Freq", "Filtre 1 : fr\xC3\xA9quence", "Hz", 20, 20000, 1200, 0.3f),
                P ("f1Reso", "Filtre 1 : r\xC3\xA9sonance", "%", 0, 100, 30),
                P ("f1Gain", "Filtre 1 : gain", "dB", -24, 24, 6),
                C ("f1Style", "Filtre 1 : style", styles(), 1),
                P ("f1Drive", "Filtre 1 : drive", "%", 0, 100, 10),
                C ("f2On", "Filtre 2", { "Non", "Oui" }),
                C ("f2Type", "Filtre 2 : type", types(), 1),
                C ("f2Slope", "Filtre 2 : pente", slopes(), 1),
                P ("f2Freq", "Filtre 2 : fr\xC3\xA9quence", "Hz", 20, 20000, 120, 0.3f),
                P ("f2Reso", "Filtre 2 : r\xC3\xA9sonance", "%", 0, 100, 10),
                P ("f2Gain", "Filtre 2 : gain", "dB", -24, 24, 6),
                C ("f2Style", "Filtre 2 : style", styles(), 0),
                P ("f2Drive", "Filtre 2 : drive", "%", 0, 100, 0),
                C ("modSrc", "Modulation", { "Aucune", "LFO", "Rythme", "Suiveur", "Al\xC3\xA9" "atoire", "D\xC3\xA9" "clench\xC3\xA9" "e" }),
                C ("modTarget", "Vers", { "Filtre 1", "Filtre 2", "Les deux" }),
                P ("modDepth", "Profondeur", "%", -100, 100, 50),
                P ("rate", "Vitesse", "Hz", 0.02f, 20, 1, 0.4f),
                C ("sync", "Au tempo", { "Non", "Oui" }),
                C ("division", "Division", titan::modDivisions(), 3),
                C ("shape", "Forme", titan::lfoShapes()),
                C ("pattern", "Motif", titan::rhythmNames()),
                P ("envAttack", "Attaque", "ms", 0.5f, 200, 5, 0.4f),
                P ("envRelease", "Retomb\xC3\xA9" "e", "ms", 10, 2000, 200, 0.4f),
                P ("envSens", "Sensibilit\xC3\xA9", "%", 0, 100, 50),
                C ("satStyle", "Saturation", { "Aucune", "Chaude", "Croquante", "Lampe", "Bande", "Dure", "Pompe" }),
                P ("satDrive", "Saturation : drive", "%", 0, 100, 20),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
                P ("output", "Sortie", "dB", -24, 12, 0),
            };
            m.sections = {
                { "f1", "Filtre 1", "filter", { "routing", "f1Type", "f1Slope", "f1Freq", "f1Reso", "f1Gain", "f1Style", "f1Drive" }, "global" },
                { "f2", "Filtre 2", "filter", { "f2On", "f2Type", "f2Slope", "f2Freq", "f2Reso", "f2Gain", "f2Style", "f2Drive" }, "global" },
                { "mod", "Modulation", "lfo", { "modSrc", "modTarget", "modDepth", "rate", "sync", "division", "shape", "pattern" }, "global" },
                { "env", "Suiveur et sortie", "global", { "envAttack", "envRelease", "envSens", "satStyle", "satDrive", "mix", "output" }, "global" },
            };
            m.presets = {
                { "Passe-bas classique", {} },
                { "Wah au suiveur", { { "f1Type", 2 }, { "f1Slope", 1 }, { "f1Freq", 500 }, { "f1Reso", 70 }, { "modSrc", 3 }, { "modDepth", 80 }, { "envSens", 60 } } },
                { "Balayage au tempo", { { "modSrc", 1 }, { "sync", 1 }, { "division", 5 }, { "f1Reso", 55 }, { "modDepth", 70 } } },
                { "Rythme qui hache", { { "modSrc", 2 }, { "sync", 1 }, { "division", 5 }, { "pattern", 7 }, { "modDepth", 90 }, { "f1Freq", 400 } } },
                { "Al\xC3\xA9" "atoire \xC3\xA9" "chantillonn\xC3\xA9", { { "modSrc", 4 }, { "sync", 1 }, { "division", 2 }, { "f1Reso", 60 } } },
                { "Bande passante de t\xC3\xA9l\xC3\xA9phone", { { "f1Type", 0 }, { "f1Freq", 3200 }, { "f2On", 1 }, { "f2Type", 1 }, { "f2Freq", 400 }, { "f2Slope", 2 }, { "satStyle", 2 } } },
                { "Auto-oscillation m\xC3\xA9tal", { { "f1Style", 8 }, { "f1Reso", 98 }, { "f1Drive", 40 } } },
                { "Double passe-bande st\xC3\xA9r\xC3\xA9o", { { "routing", 2 }, { "f1Type", 2 }, { "f2On", 1 }, { "f2Type", 2 }, { "f2Freq", 2400 }, { "modSrc", 1 }, { "modTarget", 2 } } },
                { "Pompe sous le kick", { { "modSrc", 5 }, { "modDepth", -80 }, { "envRelease", 300 }, { "satStyle", 6 } } },
            };
            return m;
        }();
        return d;
    }

    /** Non-linéarités des 9 styles (appliquées à l'entrée et à l'état résonant). */
    inline double sat (int st, double x) noexcept
    {
        switch (st)
        {
            case 0: return x;
            case 1: return std::tanh (x);
            case 2: return x / std::sqrt (1.0 + 0.5 * x * x);
            case 3: return std::tanh (1.6 * x + 0.1) - std::tanh (0.1);
            case 4: return std::clamp (x * 1.3, -1.0, 1.0);
            case 5: return std::tanh (x) * (1.0 - 0.3 * std::exp (-x * x * 4.0));
            case 6: return std::sin (std::clamp (x, -3.0, 3.0) * 1.2);
            case 7: return std::tanh (x + 0.2 * x * x) ;
            default: { const auto y = std::tanh (x * 2.0); return y * (1.0 + 0.3 * std::cos (x * 9.0)); }
        }
    }
}

/** Un étage d'état variable (12 dB/oct) TPT, état résonant saturé selon le style (auto-oscillation bornée). */
struct TitanSvfStage
{
    double ic1 = 0.0, ic2 = 0.0;
    void reset() noexcept { ic1 = ic2 = 0.0; }
    struct Out { double lp, bp, hp; };
    Out process (double x, double g, double k, int style) noexcept
    {
        const auto a1 = 1.0 / (1.0 + g * (g + k));
        const auto v3 = x - ic2;
        const auto v1 = a1 * ic1 + g * a1 * v3;
        const auto v2 = ic2 + g * v1;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        // l'état résonant sature : le filtre « chante » sans exploser (pente ramenée à 1 à l'origine,
        // sinon la saturation ajouterait du gain dans la boucle et l'auto-oscillation s'emballerait)
        static constexpr double slope0[9] { 1.0, 1.0, 1.0, 1.584, 1.3, 1.0, 1.2, 1.0, 2.6 };
        if (style > 0) ic1 = tfilt::sat (style, ic1 * 0.6) / (0.6 * slope0[std::clamp (style, 0, 8)]);
        ic1 = std::clamp (ic1, -6.0, 6.0); ic2 = std::clamp (ic2, -6.0, 6.0);
        return { v2, v1, x - k * v1 - v2 };
    }
};

class TitanFilter final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return tfilt::desc(); }
    void prepare (double s, int) override { sr = s; dcL.prepare (sr, 6.0); dcR.prepare (sr, 6.0); reset(); }
    void reset() noexcept override
    {
        for (auto& f : flt) for (auto& c : f.ch) { for (auto& st : c.st) st.reset(); c.one = 0.0; c.eq.reset(); }
        mod.reset(); pump = 0.0; ctr = 0; dcL.reset(); dcR.reset();
    }
    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace tfilt;
        const int rt = (int) p[routing];
        const bool f2 = p[f2On] > 0.5f;
        const int src = (int) p[modSrc];
        const int tgt = (int) p[modTarget];
        const double depth = p[modDepth] / 100.0;
        const bool synced = p[sync] > 0.5f && bpm > 1.0;
        const double inc = synced ? 1.0 / (titan::modDivQuarters ((int) p[division]) * 60.0 / bpm * sr) : p[rate] / sr;
        const double envAtt = std::exp (-1.0 / (p[envAttack] * 0.001 * sr)), envRel = std::exp (-1.0 / (p[envRelease] * 0.001 * sr));
        const double sens = p[envSens] / 100.0;
        static constexpr int modeOf[6] { 0, 0, 1, 4, 3, 5 };
        const double smoothC = src == 2 || src == 4 ? std::exp (-1.0 / (0.003 * sr)) : 0.0;
        FiltP fp[2];
        for (int k = 0; k < 2; ++k)
        {
            const int b = k == 0 ? f1Type : f2Type;
            fp[k].type = (int) p[b]; fp[k].slope = (int) p[b + 1]; fp[k].freq = p[b + 2];
            fp[k].reso = p[b + 3] / 100.0; fp[k].gain = p[b + 4]; fp[k].style = (int) p[b + 5];
            const double dr = p[b + 6] / 100.0; fp[k].drive = 1.0 + 8.0 * dr * dr;
        }
        const int ss = (int) p[satStyle];
        const double sd = p[satDrive] / 100.0, sg = 1.0 + 10.0 * sd * sd;
        const double wet = p[mix] / 100.0, out = dsp::dbToGain (p[output]);
        const double pumpRel = std::exp (-1.0 / (0.18 * sr));
        for (int i = 0; i < n; ++i)
        {
            const double lvl = 0.5 * (std::abs ((double) L[i]) + std::abs ((double) R[i]));
            const double m = src == 0 ? 0.5 : mod.next (modeOf[std::clamp (src, 0, 5)], (int) p[shape], inc, lvl, envAtt, envRel, sens, (int) p[pattern], 0.0, smoothC);
            // la modulation pousse la fréquence de ±4 octaves (le suiveur et le déclenché partent de 0)
            const double mm = (src == 3 || src == 5) ? m : m - 0.5;
            const double oct = depth * 4.0 * mm * (src == 3 || src == 5 ? 1.0 : 2.0) * 0.5;
            if ((ctr++ & 7) == 0)
                for (int k = 0; k < 2; ++k)
                {
                    const bool modIt = src != 0 && (tgt == 2 || tgt == k);
                    const double f = std::clamp (fp[k].freq * (modIt ? std::pow (2.0, oct * 2.0) : 1.0), 16.0, 0.45 * sr);
                    flt[k].g = std::tan (kPi * f / sr);
                    flt[k].k = 2.0 - 1.98 * std::pow (fp[k].reso, 0.7);       // jusqu'à l'auto-oscillation
                    flt[k].oneC = std::exp (-kTwoPi * f / sr);
                    if (fp[k].type >= 3) for (auto& c : flt[k].ch) c.eq.set (fp[k].type == 3 ? dsp::SvfEq::notch : fp[k].type == 4 ? dsp::SvfEq::bell : fp[k].type == 5 ? dsp::SvfEq::lowShelf : fp[k].type == 6 ? dsp::SvfEq::highShelf : dsp::SvfEq::allpass,
                                                                         f, 0.5 + 8.0 * fp[k].reso, fp[k].gain, sr);
                }
            double xs[2] { L[i], R[i] };
            double ys[2];
            for (int c = 0; c < 2; ++c)
            {
                const double x = xs[c];
                double y;
                if (! f2) y = runFilter (0, c, x, fp[0]);
                else if (rt == 0) y = runFilter (1, c, runFilter (0, c, x, fp[0]), fp[1]);
                else if (rt == 1) y = 0.5 * (runFilter (0, c, x, fp[0]) + runFilter (1, c, x, fp[1]));
                else y = c == 0 ? runFilter (0, c, x, fp[0]) : runFilter (1, c, x, fp[1]);
                // saturation de sortie
                switch (ss)
                {
                    case 1: y = std::tanh (y * sg) / std::sqrt (sg); break;
                    case 2: y = (std::tanh (y * sg * 1.5 + 0.1) - std::tanh (0.1)) / std::sqrt (sg * 1.5); break;
                    case 3: y = std::tanh (y * sg + 0.25 * y * y * sg) / std::sqrt (sg); break;
                    case 4: y = std::atan (y * sg) / std::sqrt (sg); break;
                    case 5: y = std::clamp (y * sg, -1.0, 1.0) / std::sqrt (sg); break;
                    case 6: { const auto a = std::abs (y); pump = a > pump ? a : a + (pump - a) * pumpRel; y = y / (1.0 + sg * pump); break; } // pompe : compression
                    default: break;
                }
                ys[c] = y;
            }
            ys[0] = dcL.process (ys[0]); ys[1] = dcR.process (ys[1]);
            for (int c = 0; c < 2; ++c) (c == 0 ? L[i] : R[i]) = (float) std::clamp ((xs[c] * (1.0 - wet) + ys[c] * wet) * out, -8.0, 8.0);
        }
    }
private:
    struct FiltP { int type = 0, slope = 2, style = 0; double freq = 1000, reso = 0, gain = 0, drive = 1; };
    struct FChan { std::array<TitanSvfStage, 4> st; double one = 0.0; dsp::SvfEq eq; };
    struct Filt { std::array<FChan, 2> ch; double g = 0.1, k = 1.4, oneC = 0.0; };

    double runFilter (int k, int c, double x, const FiltP& fp) noexcept
    {
        auto& f = flt[k];
        auto& s = f.ch[(size_t) c];
        x = fp.style > 0 ? tfilt::sat (fp.style, x * fp.drive) / std::sqrt (fp.drive) : x * (fp.drive > 1.0 ? 1.0 : 1.0);
        if (fp.type >= 3) return s.eq.process (x);
        if (fp.slope == 0)
        {
            // 6 dB/oct : un pôle
            s.one = x + (s.one - x) * f.oneC;
            return fp.type == 1 ? x - s.one : s.one;
        }
        // 12 dB par étage : 1, 2, 3 ou 4 étages ; la résonance sur le dernier
        const int ns = std::clamp (fp.slope, 1, 4);
        double v = x;
        for (int j = 0; j < ns; ++j)
        {
            const double kk = j == ns - 1 ? f.k : 1.41421356;
            const auto o = s.st[(size_t) j].process (v, f.g, kk, j == ns - 1 ? fp.style : 0);
            v = fp.type == 0 ? o.lp : fp.type == 1 ? o.hp : o.bp * (j == ns - 1 ? kk : 1.41421356);
        }
        return v;
    }

    std::array<Filt, 2> flt;
    titan::ModSource mod;
    dsp::DcBlocker dcL, dcR;
    double pump = 0.0;
    std::uint32_t ctr = 0;
    double sr = 48000.0;
};

} // namespace grv::effects
