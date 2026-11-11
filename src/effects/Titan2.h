/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 — famille « Golem Titan FX », lot G2 : limiteur de mastering et delay créatif.
    Fiche d'architecture : claude/P3/SPEC-GOLEM-TITAN-FX.md (projet).

      - Titan Limiteur (principe des limiteurs de mastering à plusieurs algorithmes) : 8 styles, chacun
        une façon d'attaquer et de relâcher (relâchement à deux étages qui suit le programme), une part
        d'écrêtage doux avant la limitation, une mise en valeur des transitoires (Dynamique), un pompage
        voulu (Percutant, Bus). Anticipation de 0 à 5 ms (latence déclarée : le moteur la compense),
        minimum glissant exact sur la fenêtre, crête vraie (points intermédiaires estimés), lien des voies
        pour l'attaque et pour le relâchement, suréchantillonnage ×2 / ×4 à phase minimale, écoute à gain
        unité (on entend ce que fait le limiteur sans l'effet de volume).

      - Titan Écho (principe des delays à bande « modernes ») : deux lignes (gauche / droite) libres ou au
        tempo, réinjection, réinjection croisée, ping-pong ; changement de temps « bande » (la hauteur
        glisse) ou « étirement » (fondu entre deux têtes, sans glissement) ; pleurage et scintillement ;
        dans la boucle : 2 filtres résonants, saturation, basse-fidélité, diffusion, décalage de hauteur ;
        ducking (Dynamique), gel.

    Aucune allocation dans le rendu.
*/

#pragma once

#include "Titan1.h"
#include "../dsp/Oversampler.h"
#include <atomic>

namespace grv::effects
{

//==============================================================================
// Titan Limiteur
namespace tlim
{
    enum Slot { gain = 0, ceiling, style, lookahead, attack, release, truePeak, unity, linkTrans, linkRel, oversampling, numSlots };

    inline const std::vector<std::string>& styles()
    {
        static const std::vector<std::string> v { "Transparent", "Percutant", "Dynamique", "Polyvalent", "Agressif", "Moderne", "Bus", "S\xC3\xBBr" };
        return v;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tlimit"; m.name = "Titan Limiteur"; m.role = "Limiteur de mastering"; m.family = titan::family; m.form = "module";
            m.principle = "8 styles de limitation (transparent \xC3\xA0 agressif, bus, s\xC3\xBBr), anticipation 0\xE2\x80\x93" "5 ms compens\xC3\xA9" "e, cr\xC3\xAA" "te vraie, lien des voies, sur\xC3\xA9" "chantillonnage, \xC3\xA9" "coute \xC3\xA0 gain unit\xC3\xA9";
            m.params = {
                P ("gain", "Gain", "dB", 0, 30, 0),
                P ("ceiling", "Plafond", "dB", -12, 0, -1),
                C ("style", "Style", styles(), 5),
                P ("lookahead", "Anticipation", "ms", 0, 5, 1),
                P ("attack", "Attaque", "%", 0, 100, 50),
                P ("release", "Rel\xC3\xA2" "chement", "%", 0, 100, 50),
                C ("truePeak", "Cr\xC3\xAA" "te vraie", { "Non", "Oui" }, 1),
                C ("unity", "\xC3\x89" "coute \xC3\xA0 gain unit\xC3\xA9", { "Non", "Oui" }),
                P ("linkTrans", "Lien : transitoires", "%", 0, 100, 75),
                P ("linkRel", "Lien : rel\xC3\xA2" "chement", "%", 0, 100, 100),
                C ("oversampling", "Sur\xC3\xA9" "chantillonnage", { "Aucun", "\xC3\x97" "2", "\xC3\x97" "4" }, 1),
            };
            m.sections = {
                { "limit", "Limiteur", "comp", { "gain", "ceiling", "style", "lookahead", "attack", "release", "truePeak", "unity" }, "global" },
                { "adv", "Avanc\xC3\xA9", "global", { "linkTrans", "linkRel", "oversampling" }, "global" },
            };
            m.presets = {
                { "Moderne, plafond \xE2\x88\x92" "1 dB", {} },
                { "Mastering transparent", { { "style", 0 }, { "gain", 4 }, { "lookahead", 2 } } },
                { "Club fort", { { "style", 4 }, { "gain", 8 }, { "ceiling", -0.3f } } },
                { "Musique dynamique", { { "style", 2 }, { "gain", 3 }, { "release", 60 } } },
                { "Bus de batterie qui pompe", { { "style", 6 }, { "gain", 6 }, { "lookahead", 0.3f }, { "truePeak", 0 }, { "ceiling", -2 } } },
                { "Voix percutante", { { "style", 1 }, { "gain", 5 }, { "ceiling", -3 } } },
                { "S\xC3\xBBr pour diffusion", { { "style", 7 }, { "gain", 2 }, { "lookahead", 4 }, { "oversampling", 2 } } },
            };
            return m;
        }();
        return d;
    }

    /** Caractère des styles : part d'anticipation utilisée pour l'attaque, relâchements rapide et lent (ms),
        écrêtage doux avant limitation, transitoires mises en avant, pompage (relâchement linéaire). */
    struct StyleDef { double attFrac, relFast, relSlow, clip, trans, pump; };
    inline const StyleDef& styleDef (int i)
    {
        static const StyleDef s[8] {
            { 1.00,  60.0,  700.0, 0.00, 0.0, 0.0 },  // transparent
            { 0.55,  35.0,   35.0, 0.10, 0.0, 0.7 },  // percutant
            { 0.85,  50.0,  450.0, 0.00, 0.6, 0.0 },  // dynamique
            { 0.90,  45.0,  320.0, 0.06, 0.0, 0.0 },  // polyvalent
            { 0.30,  15.0,  120.0, 0.55, 0.0, 0.2 },  // agressif
            { 1.00,  25.0,  520.0, 0.15, 0.2, 0.0 },  // moderne
            { 0.40, 120.0,  120.0, 0.10, 0.0, 1.0 },  // bus
            { 1.00, 150.0, 1300.0, 0.00, 0.0, 0.0 },  // sûr
        };
        return s[std::clamp (i, 0, 7)];
    }
}

class TitanLimiter final : public Effect
{
public:
    static constexpr int maxOs = 4;
    const ModuleDesc& desc() const noexcept override { return tlim::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        const int maxLook = (int) (0.0055 * sr * maxOs) + 8;
        for (auto& b : dly) b.assign ((size_t) maxLook + 1, 0.0);
        for (auto& q : dq) q.assign ((size_t) maxLook + 2, {});
        reset();
    }
    void reset() noexcept override
    {
        for (auto& b : dly) std::fill (b.begin(), b.end(), 0.0);
        for (int c = 0; c < 2; ++c) { head[c] = tail[c] = 0; g[c] = 1.0; slow[c] = 0.0; os2[c].reset(); os4[c].reset(); dn2[c].reset(); dn4[c].reset(); tEnvF[c] = tEnvS[c] = 0.0; prevX[c][0] = prevX[c][1] = prevX[c][2] = 0.0; }
        w = 0; t = 0; grShown = 0.0; gGuard = 1.0; for (auto& a : gh) for (auto& v : a) v = 0.0;
    }
    int latency() const noexcept override { return lookN.load (std::memory_order_relaxed); }
    float meter (int i) const noexcept override { return i == 0 ? (float) grShown : 0.0f; }

    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace tlim;
        const auto& stl = styleDef ((int) p[style]);
        const int osSel = std::clamp ((int) p[oversampling], 0, 2);
        const int os = osSel == 0 ? 1 : osSel == 1 ? 2 : 4;
        const double fs = sr * os;
        const int look = (int) std::lround (p[lookahead] * 0.001 * sr);            // au rythme de base (latence)
        const bool guard = p[truePeak] > 0.5f && os > 1;
        lookN.store (look + (guard ? 2 : 0), std::memory_order_relaxed);
        const double guardRel = std::exp (-1.0 / (0.005 * sr));
        const int lookOs = std::max (1, look * os);
        const double gIn = dsp::dbToGain (p[gain]);
        const double ceil = dsp::dbToGain (p[ceiling]);
        const bool tp = p[truePeak] > 0.5f, uni = p[unity] > 0.5f;
        const double attScale = 0.25 + 1.5 * p[attack] / 100.0;
        const double relScale = std::pow (4.0, (p[release] - 50.0) / 50.0);
        const int attN = std::max (1, (int) (lookOs * std::min (1.0, stl.attFrac * attScale)));
        const double attC = std::exp (-5.0 / attN);                                  // > 99 % du trajet sur attN
        const double susK = 1.0 - std::exp (-1.0 / (0.35 * fs));                       // mémoire de la limitation tenue
        const double relF = std::exp (-1.0 / (stl.relFast * relScale * 0.001 * fs));
        const double relS = std::exp (-1.0 / (stl.relSlow * relScale * 0.001 * fs));
        const double linkT = p[linkTrans] / 100.0, linkR = p[linkRel] / 100.0;
        const double clipK = stl.clip;
        const double trK = stl.trans;
        const double tF = std::exp (-1.0 / (0.002 * fs)), tS = std::exp (-1.0 / (0.04 * fs));
        // si l'anticipation est nulle, l'attaque doit être immédiate (pas de dépassement du plafond)
        const bool instant = look == 0;
        const int dlySize = (int) dly[0].size();
        double grMax = 0.0;

        for (int i = 0; i < n; ++i)
        {
            double xs[2][maxOs];
            for (int c = 0; c < 2; ++c)
            {
                const double x = (c == 0 ? L[i] : R[i]) * gIn;
                if (os == 1) xs[c][0] = x;
                else if (os == 2) os2[c].up (x, xs[c][0], xs[c][1]);
                else os4[c].up (x, xs[c]);
            }
            double ys[2][maxOs];
            for (int k = 0; k < os; ++k)
            {
                double req[2], cur[2];
                for (int c = 0; c < 2; ++c)
                {
                    double x = xs[c][k];
                    // transitoires mises en avant (Dynamique, Moderne) : enveloppe rapide − lente
                    if (trK > 0.0)
                    {
                        const auto a = std::abs (x);
                        tEnvF[c] = a > tEnvF[c] ? a : a + (tEnvF[c] - a) * tF;
                        tEnvS[c] = a + (tEnvS[c] - a) * tS;
                        const auto boost = 1.0 + trK * 0.5 * std::clamp ((tEnvF[c] - tEnvS[c]) / (tEnvS[c] + 1.0e-4), 0.0, 1.0);
                        x *= boost;
                    }
                    // écrêtage doux avant limitation (Agressif, Moderne…) : arrondit les crêtes au-dessus du plafond
                    if (clipK > 0.0)
                    {
                        const auto th = ceil * (1.0 - 0.5 * clipK);
                        const auto a = std::abs (x);
                        if (a > th) { const auto over = (a - th) / (ceil * 1.5 - th + 1.0e-9); x = std::copysign (th + (ceil * 1.5 - th) * std::tanh (over), x); }
                    }
                    cur[c] = x;
                    // crête vraie : on estime les points intermédiaires (Hermite) si pas assez suréchantillonné
                    double pk = std::abs (x);
                    if (tp && os < 4)
                    {
                        const auto a = prevX[c][0], b = prevX[c][1], d = prevX[c][2];
                        pk = std::max (pk, std::abs (dsp::hermite4 (a, b, d, x, 0.5)));
                        pk = std::max (pk, std::abs (dsp::hermite4 (a, b, d, x, 0.25)));
                        pk = std::max (pk, std::abs (dsp::hermite4 (a, b, d, x, 0.75)));
                    }
                    prevX[c][0] = prevX[c][1]; prevX[c][1] = prevX[c][2]; prevX[c][2] = x;
                    req[c] = pk > ceil ? ceil / pk : 1.0;
                    pushMin (c, req[c], lookOs);
                }
                // écriture dans la ligne d'anticipation
                for (int c = 0; c < 2; ++c) dly[c][(size_t) w] = cur[c];
                const int rd = (w - lookOs + dlySize) % dlySize;
                w = (w + 1) % dlySize; ++t;
                double tgt[2] { winMin (0), winMin (1) };
                const double linked = std::min (tgt[0], tgt[1]);
                double gNew[2];
                for (int c = 0; c < 2; ++c)
                {
                    const auto target = tgt[c] + (linked - tgt[c]) * linkT;
                    double gg = g[c];
                    if (target < gg) gg = instant ? target : target + (gg - target) * attC;
                    else if (stl.pump > 0.0) gg = std::min (target, gg + (1.0 - relF) * (0.15 + 0.85 * stl.pump)); // pompage : remontée linéaire
                    else
                    {
                        // relâchement qui suit le programme : rapide après une crête isolée, lent après
                        // une limitation tenue (on ne fait pas « respirer » une nappe compressée)
                        const auto relC = relF + (relS - relF) * slow[c];
                        gg = target + (gg - target) * relC;
                    }
                    slow[c] += ((gg < 0.97 ? 1.0 : 0.0) - slow[c]) * susK;
                    gNew[c] = gg;
                }
                // lien du relâchement
                const double gl = std::min (gNew[0], gNew[1]);
                for (int c = 0; c < 2; ++c) { g[c] = gNew[c] + (gl - gNew[c]) * (gNew[c] > tgt[c] ? linkR : 0.0); }
                for (int c = 0; c < 2; ++c)
                {
                    auto y = dly[c][(size_t) rd] * g[c];
                    // garde-fou final : jamais au-dessus du plafond
                    y = std::clamp (y, -ceil, ceil);
                    ys[c][k] = y;
                }
                grMax = std::max (grMax, -dsp::gainToDb (std::min (g[0], g[1])));
            }
            double outS[2];
            for (int c = 0; c < 2; ++c)
            {
                double y = os == 1 ? ys[c][0] : os == 2 ? dn2[c].down (ys[c][0], ys[c][1]) : dn4[c].down (ys[c]);
                outS[c] = std::clamp (y, -ceil, ceil);                    // le filtre de retour peut dépasser d'un cheveu
            }
            // crête vraie après suréchantillonnage : le filtre de retour peut créer des crêtes entre les
            // échantillons ; garde de 2 échantillons (latence déclarée) qui les ramène sous le plafond
            if (guard)
            {
                for (int c = 0; c < 2; ++c) { gh[c][0] = gh[c][1]; gh[c][1] = gh[c][2]; gh[c][2] = gh[c][3]; gh[c][3] = outS[c]; }
                double est = 0.0;
                for (int c = 0; c < 2; ++c)
                    for (double f : { 0.25, 0.5, 0.75 })
                    {
                        est = std::max (est, std::abs (dsp::hermite4 (gh[c][0], gh[c][1], gh[c][2], gh[c][3], f)));
                        est = std::max (est, std::abs (dsp::hermite4 (gh[c][1], gh[c][1], gh[c][2], gh[c][3], f)));
                    }
                const double need = est > ceil ? ceil / est : 1.0;
                gGuard = need < gGuard ? need : need + (gGuard - need) * guardRel;
                outS[0] = gh[0][1] * gGuard; outS[1] = gh[1][1] * gGuard;
            }
            for (int c = 0; c < 2; ++c)
            {
                double y = outS[c];
                if (uni) y /= gIn;                                        // écoute à gain unité
                (c == 0 ? L[i] : R[i]) = (float) y;
            }
        }
        grShown = std::max (grMax, grShown * 0.9);
    }

private:
    /** Minimum glissant exact (file monotone) des gains demandés sur la fenêtre d'anticipation. */
    struct Item { double v; std::int64_t idx; };
    void pushMin (int c, double v, int win) noexcept
    {
        auto& q = dq[c];
        const int cap = (int) q.size();
        while (tail[c] != head[c] && q[(size_t) ((tail[c] - 1 + cap) % cap)].v >= v) tail[c] = (tail[c] - 1 + cap) % cap;
        q[(size_t) tail[c]] = { v, t };
        tail[c] = (tail[c] + 1) % cap;
        while (head[c] != tail[c] && q[(size_t) head[c]].idx <= t - win - 1) head[c] = (head[c] + 1) % cap;
    }
    double winMin (int c) const noexcept { return head[c] == tail[c] ? 1.0 : dq[c][(size_t) head[c]].v; }

    std::array<std::vector<double>, 2> dly;
    std::array<std::vector<Item>, 2> dq;
    int head[2] {}, tail[2] {};
    int w = 0;
    std::int64_t t = 0;
    double g[2] { 1.0, 1.0 }, slow[2] { 0.0, 0.0 }, tEnvF[2] {}, tEnvS[2] {}, prevX[2][3] {};
    dsp::Oversampler2x os2[2], dn2[2];
    dsp::Oversampler4x os4[2], dn4[2];
    std::atomic<int> lookN { 0 };
    double gh[2][4] {}, gGuard = 1.0;
    double sr = 48000.0, grShown = 0.0;
};

//==============================================================================
// Titan Écho
namespace techo
{
    enum Slot { sync = 0, timeL, timeR, divL, divR, feedback, cross, pingpong,
                timeMode, wow, flutter, freeze, mix, width,
                drive, lofi, diffuse, dynamics, pitch,
                f1Type, f1Freq, f1Reso, f2Type, f2Freq, f2Reso, numSlots };

    inline const std::vector<std::string>& divisions()
    {
        static const std::vector<std::string> v { "1/32", "1/16 triolet", "1/16", "1/16 point\xC3\xA9" "e", "1/8 triolet", "1/8", "1/8 point\xC3\xA9" "e",
                                                  "1/4 triolet", "1/4", "1/4 point\xC3\xA9" "e", "1/2", "1 mesure" };
        return v;
    }
    inline double divQuarters (int i) noexcept
    {
        static constexpr double q[12] { 0.125, 1.0 / 6, 0.25, 0.375, 1.0 / 3, 0.5, 0.75, 2.0 / 3, 1.0, 1.5, 2.0, 4.0 };
        return q[std::clamp (i, 0, 11)];
    }
    inline const std::vector<std::string>& filterTypes()
    {
        static const std::vector<std::string> v { "Aucun", "Passe-bas", "Passe-haut", "Passe-bande" };
        return v;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "techo"; m.name = "Titan \xC3\x89" "cho"; m.role = "Delay cr\xC3\xA9" "atif"; m.family = titan::family; m.form = "module";
            m.principle = "Deux lignes \xC3\xA0 bande (libres ou au tempo), r\xC3\xA9injection crois\xC3\xA9" "e, ping-pong, temps \xC2\xAB bande \xC2\xBB ou \xC2\xAB \xC3\xA9tirement \xC2\xBB ; dans la boucle : 2 filtres, saturation, basse-fid\xC3\xA9lit\xC3\xA9, diffusion, hauteur ; ducking, gel";
            m.params = {
                C ("sync", "Temps", { "Libre", "Au tempo" }, 1),
                P ("timeL", "Temps gauche", "ms", 1, 2000, 375, 0.4f),
                P ("timeR", "Temps droite", "ms", 1, 2000, 500, 0.4f),
                C ("divL", "Division gauche", divisions(), 6),
                C ("divR", "Division droite", divisions(), 8),
                P ("feedback", "R\xC3\xA9injection", "%", 0, 110, 40),
                P ("cross", "R\xC3\xA9injection crois\xC3\xA9" "e", "%", 0, 100, 0),
                C ("pingpong", "Ping-pong", { "Non", "Ping-pong", "Ping-pong invers\xC3\xA9" }),
                C ("timeMode", "Changement de temps", { "Bande (glisse)", "\xC3\x89tirement (fondu)" }),
                P ("wow", "Pleurage", "%", 0, 100, 10),
                P ("flutter", "Scintillement", "%", 0, 100, 5),
                C ("freeze", "Gel", { "Non", "Oui" }),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 30),
                P ("width", "Largeur", "%", 0, 150, 100),
                P ("drive", "Saturation", "%", 0, 100, 15),
                P ("lofi", "Basse-fid\xC3\xA9lit\xC3\xA9", "%", 0, 100, 0),
                P ("diffuse", "Diffusion", "%", 0, 100, 0),
                P ("dynamics", "Ducking", "%", 0, 100, 0),
                P ("pitch", "Hauteur", "demi-tons", -12, 12, 0),
                C ("f1Type", "Filtre 1", filterTypes(), 1),
                P ("f1Freq", "Filtre 1 : fr\xC3\xA9quence", "Hz", 40, 20000, 6000, 0.3f),
                P ("f1Reso", "Filtre 1 : r\xC3\xA9sonance", "%", 0, 100, 15),
                C ("f2Type", "Filtre 2", filterTypes(), 2),
                P ("f2Freq", "Filtre 2 : fr\xC3\xA9quence", "Hz", 20, 5000, 150, 0.3f),
                P ("f2Reso", "Filtre 2 : r\xC3\xA9sonance", "%", 0, 100, 0),
            };
            m.sections = {
                { "time", "Temps", "delay", { "sync", "timeL", "timeR", "divL", "divR", "feedback", "cross", "pingpong" }, "global" },
                { "tape", "Bande", "delay", { "timeMode", "wow", "flutter", "freeze", "mix", "width" }, "global" },
                { "fx", "Effets dans la boucle", "global", { "drive", "lofi", "diffuse", "dynamics", "pitch" }, "global" },
                { "filters", "Filtres", "filter", { "f1Type", "f1Freq", "f1Reso", "f2Type", "f2Freq", "f2Reso" }, "global" },
            };
            m.presets = {
                { "Bande au tempo", {} },
                { "Ping-pong large", { { "pingpong", 1 }, { "divL", 5 }, { "feedback", 55 }, { "mix", 30 } } },
                { "Slap de voix", { { "sync", 0 }, { "timeL", 95 }, { "timeR", 110 }, { "feedback", 10 }, { "mix", 22 }, { "f1Freq", 4500 } } },
                { "Dub qui monte", { { "divL", 6 }, { "divR", 6 }, { "feedback", 95 }, { "drive", 45 }, { "f1Freq", 2500 }, { "f1Reso", 35 }, { "wow", 25 } } },
                { "Diffus et sombre", { { "diffuse", 70 }, { "feedback", 60 }, { "f1Freq", 2200 }, { "mix", 35 } } },
                { "Cristaux (quinte)", { { "pitch", 7 }, { "feedback", 60 }, { "diffuse", 40 }, { "mix", 30 } } },
                { "Octave descendante", { { "pitch", -12 }, { "feedback", 55 }, { "mix", 28 } } },
                { "Cassette us\xC3\xA9" "e", { { "lofi", 60 }, { "wow", 55 }, { "flutter", 40 }, { "drive", 35 }, { "feedback", 45 } } },
                { "Voix avec ducking", { { "dynamics", 70 }, { "feedback", 45 }, { "mix", 30 } } },
            };
            return m;
        }();
        return d;
    }
}

/** Décaleur de hauteur à deux têtes (fenêtres triangulaires croisées) : léger, sans latence déclarée. */
class GrainShifter
{
public:
    void prepare (double sr) { size = 1; while (size < (int) (0.12 * sr)) size <<= 1; buf.assign ((size_t) size, 0.0f); win = (int) (0.05 * sr); reset(); }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; ph = 0.0; }
    double process (double x, double ratio) noexcept
    {
        buf[(size_t) w] = (float) x;
        w = (w + 1) & (size - 1);
        if (std::abs (ratio - 1.0) < 1.0e-4) return x;
        ph += (1.0 - ratio);                                   // la tête recule (monte) ou avance (descend)
        const double W = (double) win;
        while (ph < 0.0) ph += W;
        while (ph >= W) ph -= W;
        auto read = [&] (double d) { d += 2.0; const auto i = (int) d; const auto f = d - i;
                                     const auto a = buf[(size_t) ((w - i) & (size - 1))], b = buf[(size_t) ((w - i - 1) & (size - 1))]; return a + (b - a) * f; };
        const double d1 = ph, d2 = std::fmod (ph + 0.5 * W, W);
        const double g1 = 1.0 - std::abs (2.0 * d1 / W - 1.0), g2 = 1.0 - std::abs (2.0 * d2 / W - 1.0);
        return read (d1) * g1 + read (d2) * g2;
    }
private:
    std::vector<float> buf;
    int size = 1, w = 0, win = 1;
    double ph = 0.0;
};

class TitanEcho final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return techo::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        for (auto& l : line) l.init ((int) (2.2 * sr) + 64);
        for (auto& d : diff) d.init ((int) (0.03 * sr) + 64);
        for (auto& g : shift) g.prepare (sr);
        duck.set (3.0, 300.0, sr);
        dcL.prepare (sr, 8.0); dcR.prepare (sr, 8.0);
        reset();
    }
    void reset() noexcept override
    {
        for (auto& l : line) l.clear();
        for (auto& d : diff) d.d.clear();
        for (auto& g : shift) g.reset();
        for (auto& f : filt) f.reset();
        for (int c = 0; c < 2; ++c) { cur[c] = -1.0; oldD[c] = 0.0; xf[c] = 0.0; lofiHold[c] = 0.0; lofiPh[c] = 0.0; fbOut[c] = 0.0; lofiLp[c] = 0.0; }
        wowPh = 0.0; flPh = 0.0; drift = 0.0; duck.reset(); dcL.reset(); dcR.reset();
    }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace techo;
        const bool synced = p[sync] > 0.5f && bpm > 1.0;
        double target[2];
        target[0] = synced ? divQuarters ((int) p[divL]) * 60.0 / bpm * sr : p[timeL] * 0.001 * sr;
        target[1] = synced ? divQuarters ((int) p[divR]) * 60.0 / bpm * sr : p[timeR] * 0.001 * sr;
        const double maxD = (double) line[0].buf.size() - 0.02 * sr - 8.0;
        for (auto& tg : target) tg = std::clamp (tg, 16.0, maxD);
        const bool stretch = p[timeMode] > 0.5f;
        const bool frz = p[freeze] > 0.5f;
        const double fb = frz ? 1.0 : p[feedback] / 100.0;
        const double xfb = frz ? 0.0 : p[cross] / 100.0;
        const int pp = (int) p[pingpong];
        const double wowD = p[wow] / 100.0 * 0.004 * sr, flD = p[flutter] / 100.0 * 0.0004 * sr;
        const double drv = p[drive] / 100.0;
        const double lofiAmt = p[lofi] / 100.0;
        const double dif = p[diffuse] / 100.0;
        const double ratio = std::pow (2.0, std::clamp ((double) p[pitch], -12.0, 12.0) / 12.0);
        const double wet = p[mix] / 100.0, wd = p[width] / 100.0;
        const double dk = p[dynamics] / 100.0;
        const int ft[2] { (int) p[f1Type], (int) p[f2Type] };
        const double ff[2] { std::min ((double) p[f1Freq], 0.45 * sr), std::min ((double) p[f2Freq], 0.45 * sr) };
        const double fq[2] { 0.6 + 6.0 * p[f1Reso] / 100.0, 0.6 + 6.0 * p[f2Reso] / 100.0 };
        static constexpr dsp::SvfEq::Type types[4] { dsp::SvfEq::allpass, dsp::SvfEq::lowpass, dsp::SvfEq::highpass, dsp::SvfEq::bandpass };
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 2; ++k)
                if (ft[k] > 0) filt[(size_t) (c * 2 + k)].set (types[std::clamp (ft[k], 0, 3)], ff[k], fq[k], 0.0, sr);
        static constexpr double difMs[4] { 7.3, 11.1, 9.1, 13.7 };
        for (int k = 0; k < 4; ++k) diff[(size_t) k].len = std::clamp ((int) (difMs[k] * 0.001 * sr), 2, (int) diff[0].d.buf.size() - 8);
        const double lofiRate = 1.0 - 0.85 * lofiAmt;                     // fraction des échantillons gardés
        const double lofiBits = 16.0 - 11.0 * lofiAmt;
        const double lofiQ = std::pow (2.0, lofiBits - 1.0);
        const double lofiLpC = std::exp (-kTwoPi * (18000.0 - 14000.0 * lofiAmt) / sr);
        const double slew = std::exp (-1.0 / (0.25 * sr));             // bande : le temps glisse
        const double xfStep = 1.0 / (0.06 * sr);                       // étirement : fondu de 60 ms
        for (int c = 0; c < 2; ++c) if (cur[c] < 0.0) { cur[c] = target[c]; oldD[c] = target[c]; xf[c] = 1.0; }

        for (int i = 0; i < n; ++i)
        {
            const double inL = frz ? 0.0 : (double) L[i], inR = frz ? 0.0 : (double) R[i];
            // pleurage (lent, avec dérive aléatoire) et scintillement (rapide)
            wowPh += 0.55 / sr; if (wowPh >= 1.0) wowPh -= 1.0;
            flPh += 8.3 / sr; if (flPh >= 1.0) flPh -= 1.0;
            drift += (rnd.next() * 0.002 - drift * 0.0005);
            const double mod = wowD * (0.5 + 0.5 * dsp::fastSin2Pi (wowPh) + 0.3 * drift) + flD * (0.5 + 0.5 * dsp::fastSin2Pi (flPh));
            double rd[2];
            for (int c = 0; c < 2; ++c)
            {
                if (! stretch)
                {
                    cur[c] = target[c] + (cur[c] - target[c]) * slew;
                    rd[c] = line[c].atFrac (cur[c] + mod + (c ? 0.31 * mod : 0.0));
                }
                else
                {
                    if (std::abs (target[c] - cur[c]) > 1.0 && xf[c] >= 1.0) { oldD[c] = cur[c]; cur[c] = target[c]; xf[c] = 0.0; }
                    const auto a = line[c].atFrac (cur[c] + mod);
                    if (xf[c] < 1.0)
                    {
                        const auto b = line[c].atFrac (oldD[c] + mod);
                        rd[c] = b + (a - b) * xf[c];
                        xf[c] = std::min (1.0, xf[c] + xfStep);
                    }
                    else rd[c] = a;
                }
            }
            // boucle : filtres, saturation, basse-fidélité, diffusion, hauteur
            double proc[2];
            for (int c = 0; c < 2; ++c)
            {
                double v = rd[c];
                for (int k = 0; k < 2; ++k) if (ft[k] > 0) v = filt[(size_t) (c * 2 + k)].process (v);
                if (lofiAmt > 0.0)
                {
                    lofiPh[c] += lofiRate;
                    if (lofiPh[c] >= 1.0) { lofiPh[c] -= 1.0; lofiHold[c] = std::round (v * lofiQ) / lofiQ; }
                    lofiLp[c] = lofiHold[c] + (lofiLp[c] - lofiHold[c]) * lofiLpC;
                    v = lofiLp[c];
                }
                if (dif > 0.0) { const auto g = 0.55 * dif; v = diff[(size_t) (c * 2 + 1)].process (diff[(size_t) (c * 2)].process (v, g), -g); }
                if (std::abs (ratio - 1.0) > 1.0e-4) v = shift[c].process (v, ratio);
                // saturation de bande : toujours un peu (borne la réinjection > 100 %), plus avec le réglage
                const auto k = 1.0 + 4.0 * drv * drv;
                v = std::tanh (v * k) / k;                       // gain unité aux faibles niveaux
                proc[c] = v;
            }
            // écriture : entrée + réinjection (+ croisée) ; ping-pong : l'entrée part à gauche et se renvoie
            double wl, wr;
            if (pp == 0)
            {
                wl = inL + fb * proc[0] + xfb * proc[1];
                wr = inR + fb * proc[1] + xfb * proc[0];
            }
            else
            {
                const auto mono = 0.5 * (inL + inR);
                const auto first = pp == 1 ? mono : 0.0, second = pp == 2 ? mono : 0.0;
                wl = first + fb * proc[1] + xfb * proc[0];
                wr = second + fb * proc[0] + xfb * proc[1];
            }
            line[0].push (std::clamp (wl, -4.0, 4.0));
            line[1].push (std::clamp (wr, -4.0, 4.0));
            // sortie : ce qu'on lit (avant réinjection), largeur, ducking
            auto ol = dcL.process (rd[0]), orr = dcR.process (rd[1]);
            const auto mid = 0.5 * (ol + orr), side = 0.5 * (ol - orr) * wd;
            ol = mid + side; orr = mid - side;
            const auto env = duck.process (std::max (std::abs (inL), std::abs (inR)));
            const double dg = dk > 0.0 ? 1.0 - dk * std::clamp ((dsp::gainToDb (env) + 40.0) / 34.0, 0.0, 1.0) : 1.0;
            L[i] = (float) ((double) L[i] * (1.0 - wet) + ol * wet * dg);
            R[i] = (float) ((double) R[i] * (1.0 - wet) + orr * wet * dg);
        }
    }

private:
    std::array<DLine, 2> line;
    std::array<APLine, 4> diff;
    std::array<GrainShifter, 2> shift;
    std::array<dsp::SvfEq, 4> filt;
    dsp::EnvelopeFollower duck;
    dsp::DcBlocker dcL, dcR;
    titan::Rand rnd;
    double cur[2] { -1.0, -1.0 }, oldD[2] {}, xf[2] {}, lofiHold[2] {}, lofiPh[2] {}, lofiLp[2] {}, fbOut[2] {};
    double sr = 48000.0, wowPh = 0.0, flPh = 0.0, drift = 0.0;
};

} // namespace grv::effects
