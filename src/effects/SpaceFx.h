/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Espaces de caractère (v0.35), chacun sur son principe (architecture seulement) :

      - « Réverbération vintage » (principe des réverbérations numériques de studio des années 70-80) :
        diffusion d'entrée (4 passe-tout), réservoir en « huit » à deux moitiés (passe-tout modulés,
        lignes, amortissement, déclin ; topologie publiée par J. Dattorro, 1997), sorties prises en
        plusieurs points de chaque moitié. Modes (salle, plaque, chambre, concert, ambiance, lumineuse)
        = tailles, diffusions, amortissements et modulations différents ; « Couleur » : 1970
        (bande passante réduite, modulation plus marquée), 1980 (aigus adoucis), actuelle (pleine bande).
      - « Plaque » (principe des plaques à tôle tendue) : même réservoir réglé en plaque très dense,
        amortisseur (déclin 0,4 à 6 s), pré-délai, brillance, coupe des graves.
      - « Ressort » (principe des réverbérations à ressorts d'amplis et d'échos à bande) : 1 à 3 ressorts,
        chacun une boucle avec une chaîne de 48 passe-tout du premier ordre (dispersion : les aigus
        arrivent avant les graves, le « boing » caractéristique), filtre de ton, légère modulation.
      - « Écho à bande à têtes » (principe des échos à bande à trois têtes de lecture avec ressort) :
        bande en boucle, 3 têtes (1, 2, 3 × l'écart), sélecteur de têtes, intensité jusqu'à
        l'auto-oscillation (bornée par la saturation de bande), pleurage et scintillement (« usure »),
        filtres grave / aigu dans la boucle, ressort intégré, vitesse libre ou calée au tempo.

    Temps réel : toutes les lignes sont réservées dans prepare() (aucune allocation dans le rendu),
    longueurs en puissance de 2 (masque), lecture fractionnaire linéaire pour les lignes modulées.
*/

#pragma once

#include "../dsp/EqFilters.h"
#include "../engine/Module.h"
#include "Dynamics.h"

#include <array>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

/** Ligne à retard circulaire (taille puissance de 2), lecture entière ou fractionnaire. */
struct DLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void init (int maxLen)
    {
        int n = 1; while (n < maxLen + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void push (double x) noexcept { buf[(size_t) w] = (float) x; w = (w + 1) & mask; }
    /** Valeur écrite il y a d échantillons (d ≥ 1). */
    double at (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }
    double atFrac (double d) const noexcept
    {
        const auto i = (int) d; const auto f = d - i;
        const auto a = at (i), b = at (i + 1);
        return a + (b - a) * f;
    }
};

/** Passe-tout de Schroeder sur une ligne (g positif ou négatif). */
struct APLine
{
    DLine d; int len = 1;
    void init (int maxLen) { d.init (maxLen); }
    double process (double x, double g) noexcept
    {
        const auto delayed = d.at (len);
        const auto v = x - g * delayed;
        d.push (v);
        return delayed + g * v;
    }
    double processMod (double x, double g, double lenF) noexcept
    {
        const auto delayed = d.atFrac (std::max (1.0, lenF));
        const auto v = x - g * delayed;
        d.push (v);
        return delayed + g * v;
    }
};

/** Réservoir de réverbération en « huit » (topologie de Dattorro). Longueurs données à 29 761 Hz. */
class FigureEightTank
{
public:
    void prepare (double s)
    {
        sr = s;
        const auto k = sr / 29761.0 * 2.2; // marge : taille jusqu'à ×2
        const auto L = [k] (int v) { return (int) (v * k) + 64; };
        for (int i = 0; i < 4; ++i) inAp[(size_t) i].init (L (inLen[i]));
        apL1.init (L (672) + 64); apR1.init (L (908) + 64);
        dL1.init (L (4453)); dR1.init (L (4217));
        apL2.init (L (1800)); apR2.init (L (2656));
        dL2.init (L (3720)); dR2.init (L (3163));
        pre.init ((int) (0.25 * sr) + 8);
        reset();
    }
    void reset() noexcept
    {
        for (auto& a : inAp) a.d.clear();
        apL1.d.clear(); apR1.d.clear(); dL1.clear(); dR1.clear(); apL2.d.clear(); apR2.d.clear(); dL2.clear(); dR2.clear(); pre.clear();
        bw = dampL = dampR = 0.0; lfo = 0.0; fbL = fbR = 0.0;
    }
    /** Réglages du réservoir. size : 0,4 à 2 ; decaySec : RT60 ; damp : 0..1 (aigus amortis) ;
        bandwidth : 0..1 (aigus d'entrée) ; mod : 0..1 ; diffusion : 0..1 ; predelayMs. */
    void configure (double size, double decaySec, double damp, double bandwidth, double mod, double diffusion, double predelayMs) noexcept
    {
        const auto k = sr / 29761.0 * std::clamp (size, 0.3, 2.0);
        for (int i = 0; i < 4; ++i) inAp[(size_t) i].len = std::max (1, (int) (inLen[i] * k));
        lenL1 = 672.0 * k; lenR1 = 908.0 * k;
        nL1 = std::max (1, (int) (4453 * k)); nR1 = std::max (1, (int) (4217 * k));
        apL2.len = std::max (1, (int) (1800 * k)); apR2.len = std::max (1, (int) (2656 * k));
        nL2 = std::max (1, (int) (3720 * k)); nR2 = std::max (1, (int) (3163 * k));
        // RT60 → gain par passage (deux décroissances par demi-boucle)
        const auto halfLoop = (672.0 + 4453.0 + 1800.0 + 3720.0) * k / sr;
        decay = std::clamp (std::pow (10.0, -3.0 * halfLoop / (2.0 * std::max (0.05, decaySec))), 0.0, 0.9995);
        dampC = std::clamp (damp, 0.0, 0.95);
        bwC = std::clamp (bandwidth, 0.05, 0.9999);
        modDepth = mod * 12.0 * sr / 29761.0;
        modInc = (0.35 + mod * 0.9) / sr;
        dif1 = 0.5 + 0.3 * diffusion; dif2 = 0.4 + 0.3 * diffusion;
        decDif1 = 0.5 + 0.25 * diffusion; decDif2 = 0.35 + 0.2 * diffusion;
        preLen = std::clamp ((int) (predelayMs * 0.001 * sr), 1, (int) pre.buf.size() - 4);
        scale = k;
    }
    void setFreeze (bool f) noexcept { freeze = f; }
    /** Une entrée mono → sortie stéréo de la queue. */
    void process (double x, double& outL, double& outR) noexcept
    {
        pre.push (x);
        double v = pre.at (preLen);
        bw += (v - bw) * bwC; v = bw;
        if (freeze) v = 0.0;
        v = inAp[0].process (v, dif1); v = inAp[1].process (v, dif1);
        v = inAp[2].process (v, dif2); v = inAp[3].process (v, dif2);
        lfo += modInc; if (lfo >= 1.0) lfo -= 1.0;
        const auto m = modDepth * fastSin2Pi (lfo), m2 = modDepth * fastSin2Pi (lfo + 0.25);
        const auto dec = freeze ? 1.0 : decay;
        const auto dmp = freeze ? 0.0 : dampC;
        // moitié gauche
        auto a = apL1.processMod (v + fbR * dec, -decDif1, lenL1 + m);
        dL1.push (a);
        auto t = dL1.at (nL1);
        dampL = t * (1.0 - dmp) + dampL * dmp;
        auto b = apL2.process (dampL * dec, decDif2);
        dL2.push (b);
        const auto toR = dL2.at (nL2);
        // moitié droite
        auto c = apR1.processMod (v + fbL * dec, -decDif1, lenR1 + m2);
        dR1.push (c);
        auto u = dR1.at (nR1);
        dampR = u * (1.0 - dmp) + dampR * dmp;
        auto e = apR2.process (dampR * dec, decDif2);
        dR2.push (e);
        const auto toL = dR2.at (nR2);
        fbL = toR; fbR = toL;
        // sorties (points de lecture de Dattorro, mis à l'échelle)
        auto tap = [this] (const DLine& d, int v0) { return d.at (std::max (1, (int) (v0 * scale))); };
        auto tapAp = [this] (const APLine& d, int v0) { return d.d.at (std::max (1, (int) (v0 * scale))); };
        outL = 0.6 * (tap (dR1, 266) + tap (dR1, 2974) - tapAp (apR2, 1913) + tap (dR2, 1996) - tap (dL1, 1990) - tapAp (apL2, 187) - tap (dL2, 1066));
        outR = 0.6 * (tap (dL1, 353) + tap (dL1, 3627) - tapAp (apL2, 1228) + tap (dL2, 2673) - tap (dR1, 2111) - tapAp (apR2, 335) - tap (dR2, 121));
    }
private:
    static constexpr int inLen[4] { 142, 107, 379, 277 };
    double sr = 48000.0, decay = 0.5, dampC = 0.3, bwC = 0.9, modDepth = 0.0, modInc = 0.0, lfo = 0.0, scale = 1.0;
    double dif1 = 0.75, dif2 = 0.625, decDif1 = 0.7, decDif2 = 0.5, bw = 0.0, dampL = 0.0, dampR = 0.0, fbL = 0.0, fbR = 0.0;
    double lenL1 = 672.0, lenR1 = 908.0;
    int nL1 = 4453, nR1 = 4217, nL2 = 3720, nR2 = 3163, preLen = 1;
    bool freeze = false;
    std::array<APLine, 4> inAp;
    APLine apL1, apR1, apL2, apR2;
    DLine dL1, dR1, dL2, dR2, pre;
};

//==============================================================================
namespace vintverb
{
    enum Slot { mode = 0, color, size, decay, predelay, damping, bass, modulation, width, lowCut, mix, freeze, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "vintverb"; m.name = "Heirloom"; m.role = "R\xC3\xA9verb\xC3\xA9ration vintage"; m.family = "R\xC3\xA9" "verb\xC3\xA9rations"; m.form = "rack";
            m.principle = "R\xC3\xA9verb\xC3\xA9rations num\xC3\xA9riques des ann\xC3\xA9" "es 70-80 : r\xC3\xA9servoir en huit, 6 modes, couleurs 1970 / 1980 / actuelle";
            m.params = {
                C ("mode", "Mode", { "Salle", "Plaque", "Chambre", "Concert", "Ambiance", "Lumineuse" }),
                C ("color", "Couleur", { "1970", "1980", "Actuelle" }, 1),
                P ("size", "Taille", "%", 0, 100, 55),
                P ("decay", "D\xC3\xA9" "clin", "s", 0.2f, 20, 2.4f, 0.4f),
                P ("predelay", "Pr\xC3\xA9-d\xC3\xA9lai", "ms", 0, 200, 15, 0.5f),
                P ("damping", "Aigus amortis", "%", 0, 100, 35),
                P ("bass", "Graves", "%", 0, 100, 50),
                P ("modulation", "Modulation", "%", 0, 100, 35),
                P ("width", "Largeur", "%", 0, 150, 100),
                P ("lowcut", "Coupe-bas", "Hz", 20, 600, 60, 0.4f),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 30),
                C ("freeze", "Gel", { "Non", "Oui" }),
            };
            m.sections = {
                { "space", "Espace", "reverb", { "mode", "color", "size", "decay", "predelay", "mix" }, "main" },
                { "tone", "Couleur de la queue", "reverb", { "damping", "bass", "modulation", "width", "lowcut", "freeze" }, "tone" },
            };
            m.presets = {
                { "Salle des ann\xC3\xA9" "es 80", { { "mode", 0 }, { "color", 1 }, { "decay", 2.6f }, { "size", 60 } }, "Salles" },
                { "Plaque vocale", { { "mode", 1 }, { "color", 1 }, { "decay", 1.8f }, { "predelay", 25 }, { "damping", 25 } }, "Plaques" },
                { "Chambre douce", { { "mode", 2 }, { "color", 0 }, { "decay", 1.4f }, { "size", 40 }, { "damping", 50 } }, "Pi\xC3\xA8" "ces" },
                { "Concert immense", { { "mode", 3 }, { "color", 2 }, { "decay", 6 }, { "size", 90 }, { "predelay", 40 } }, "Salles" },
                { "Ambiance batterie", { { "mode", 4 }, { "color", 1 }, { "decay", 0.7f }, { "size", 25 }, { "mix", 25 } }, "Pi\xC3\xA8" "ces" },
                { "Nappe lumineuse", { { "mode", 5 }, { "color", 2 }, { "decay", 9 }, { "size", 85 }, { "modulation", 70 }, { "damping", 10 }, { "mix", 40 } }, "Effets" },
                { "Caisse claire 1979", { { "mode", 1 }, { "color", 0 }, { "decay", 1.2f }, { "predelay", 5 }, { "mix", 35 } }, "Plaques" },
            };
            return m;
        }();
        return d;
    }
}

class VintageReverb final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return vintverb::desc(); }
    void prepare (double s, int) override { sr = s; tank.prepare (s); reset(); }
    void reset() noexcept override { tank.reset(); for (auto& f : post) f.reset(); lc.reset(); bassL.reset(); bassR.reset(); }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace vintverb;
        // modes : taille, diffusion, amortissement ajouté, modulation ajoutée
        struct Mode { double size, dif, damp, mod; };
        static constexpr Mode modes[6] { { 1.0, 0.8, 0.0, 0.0 }, { 0.75, 1.0, -0.1, 0.0 }, { 0.55, 0.6, 0.15, -0.1 },
                                         { 1.5, 0.9, 0.05, 0.1 }, { 0.4, 0.5, 0.2, -0.2 }, { 1.3, 1.0, -0.2, 0.3 } };
        const auto& md = modes[std::clamp ((int) p[mode], 0, 5)];
        const auto col = std::clamp ((int) p[color], 0, 2);
        const auto sz = md.size * (0.5 + p[size] / 100.0);
        const auto dmp = std::clamp (p[damping] / 100.0 * 0.8 + md.damp + (col == 0 ? 0.15 : 0.0), 0.0, 0.92);
        const auto bwc = col == 0 ? 0.55 : col == 1 ? 0.75 : 0.95;
        const auto mod = std::clamp (p[modulation] / 100.0 + md.mod + (col == 0 ? 0.25 : 0.0), 0.0, 1.0);
        tank.configure (sz, p[decay], dmp, bwc, mod, md.dif, p[predelay]);
        tank.setFreeze (p[freeze] > 0.5f);
        // couleur d'époque : bande passante de sortie (1970 : 7 kHz, 1980 : 11 kHz)
        const auto cut = col == 0 ? 7000.0 : col == 1 ? 11000.0 : 18000.0;
        for (auto& f : post) f.set (SvfEq::lowpass, std::min (cut, 0.45 * sr), 0.7, 0.0, sr);
        lc.set (SvfEq::highpass, p[lowCut], 0.7, 0.0, sr);
        // graves de la queue : plateau grave (−6 à +4 dB)
        const auto bassDb = (p[bass] - 50.0) / 50.0 * (p[bass] < 50.0f ? 6.0 : 4.0);
        bassL.set (SvfEq::lowShelf, 250.0, 0.7, bassDb, sr); bassR.set (SvfEq::lowShelf, 250.0, 0.7, bassDb, sr);
        const auto wet = p[mix] / 100.0, w = p[width] / 100.0;
        for (int i = 0; i < n; ++i)
        {
            const double dl = L[i], dr = R[i];
            const auto in = lc.process (0.5 * (dl + dr));
            double ol, orr;
            tank.process (in, ol, orr);
            ol = bassL.process (post[0].process (ol)); orr = bassR.process (post[1].process (orr));
            const auto mid = 0.5 * (ol + orr), side = 0.5 * (ol - orr) * w;
            L[i] = (float) (dl * (1.0 - wet) + (mid + side) * wet);
            R[i] = (float) (dr * (1.0 - wet) + (mid - side) * wet);
        }
    }
private:
    double sr = 48000.0;
    FigureEightTank tank;
    std::array<SvfEq, 2> post;
    SvfEq lc, bassL, bassR;
};

//==============================================================================
namespace plate
{
    enum Slot { decay = 0, predelay, brightness, lowCut, mix, width, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "plate"; m.name = "Gold Leaf"; m.role = "Plaque"; m.family = "R\xC3\xA9" "verb\xC3\xA9rations"; m.form = "pedal";
            m.principle = "Plaque \xC3\xA0 t\xC3\xB4le tendue : tr\xC3\xA8s dense, amortisseur, pr\xC3\xA9-d\xC3\xA9lai, brillance";
            m.params = {
                P ("decay", "D\xC3\xA9" "clin", "s", 0.4f, 6, 2, 0.5f),
                P ("predelay", "Pr\xC3\xA9-d\xC3\xA9lai", "ms", 0, 150, 10, 0.5f),
                P ("brightness", "Brillance", "%", 0, 100, 60),
                P ("lowcut", "Coupe-bas", "Hz", 20, 500, 120, 0.4f),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 30),
                P ("width", "Largeur", "%", 0, 150, 100),
            };
            m.sections = { { "plate", "Plaque", "reverb", { "decay", "predelay", "brightness", "lowcut", "mix", "width" }, "main" } };
            m.presets = {
                { "Voix classique", { { "decay", 1.8f }, { "predelay", 30 }, { "brightness", 65 } }, "Voix" },
                { "Caisse claire", { { "decay", 1.2f }, { "predelay", 5 }, { "brightness", 75 }, { "mix", 35 } }, "Batterie" },
                { "Longue et sombre", { { "decay", 4.5f }, { "brightness", 30 }, { "mix", 25 } }, "Effets" },
                { "Courte et brillante", { { "decay", 0.7f }, { "brightness", 90 }, { "mix", 20 } }, "Batterie" },
            };
            return m;
        }();
        return d;
    }
}

class PlateReverb final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return plate::desc(); }
    void prepare (double s, int) override { sr = s; tank.prepare (s); reset(); }
    void reset() noexcept override { tank.reset(); lc.reset(); }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace plate;
        const auto bright = p[brightness] / 100.0;
        tank.configure (0.7, p[decay], 0.6 * (1.0 - bright) + 0.02, 0.6 + 0.39 * bright, 0.15, 1.0, p[predelay]);
        lc.set (SvfEq::highpass, p[lowCut], 0.7, 0.0, sr);
        const auto wet = p[mix] / 100.0, w = p[width] / 100.0;
        for (int i = 0; i < n; ++i)
        {
            const double dl = L[i], dr = R[i];
            double ol, orr;
            tank.process (lc.process (0.5 * (dl + dr)), ol, orr);
            const auto mid = 0.5 * (ol + orr), side = 0.5 * (ol - orr) * w;
            L[i] = (float) (dl * (1.0 - wet) + (mid + side) * wet);
            R[i] = (float) (dr * (1.0 - wet) + (mid - side) * wet);
        }
    }
private:
    double sr = 48000.0;
    FigureEightTank tank;
    SvfEq lc;
};

//==============================================================================
/** Un ressort : boucle (retard + chaîne de passe-tout dispersifs + filtre de ton). */
class Spring
{
public:
    static constexpr int stages = 48;
    void prepare (double s, double lenMs)
    {
        sr = s; baseLen = lenMs * 0.001 * sr;
        line.init ((int) (baseLen * 1.6) + 64);
        reset();
    }
    void reset() noexcept { line.clear(); ap.fill (0.0); lp = 0.0; hpS = 0.0; ph = 0.0; }
    /** tension 0..1 : dispersion (coefficient des passe-tout) ; feedback : gain de boucle ; tone : 0..1 */
    double process (double x, double tension, double feedback, double toneC, double lenScale) noexcept
    {
        const auto a = -0.45 - 0.4 * tension; // coefficient des passe-tout du premier ordre
        ph += 0.7 / sr; if (ph >= 1.0) ph -= 1.0;
        const auto d = std::max (2.0, baseLen * lenScale * (1.0 + 0.002 * fastSin2Pi (ph)));
        auto y = line.atFrac (d);
        // dispersion : y passe dans la chaîne
        for (int k = 0; k < stages; ++k)
        {
            const auto v = a * y + ap[(size_t) k];
            ap[(size_t) k] = y - a * v;
            y = v;
        }
        lp += (y - lp) * toneC;          // les ressorts n'ont pas d'aigus
        hpS += (lp - hpS) * 0.004;       // ni de graves profonds
        const auto out = lp - hpS;
        line.push (x + std::tanh (out * feedback));
        return out;
    }
private:
    double sr = 48000.0, baseLen = 1000.0, lp = 0.0, hpS = 0.0, ph = 0.0;
    DLine line;
    std::array<double, stages> ap {};
};

namespace spring
{
    enum Slot { springs = 0, decay, tension, tone, drip, mix, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "spring"; m.name = "Spiral"; m.role = "Ressort"; m.family = "R\xC3\xA9" "verb\xC3\xA9rations"; m.form = "pedal";
            m.principle = "R\xC3\xA9verb\xC3\xA9ration \xC3\xA0 ressorts : 1 \xC3\xA0 3 ressorts dispersifs (le \xC2\xAB boing \xC2\xBB), ton, \xC3\xA9" "claboussure";
            m.params = {
                I ("springs", "Ressorts", "", 1, 3, 2),
                P ("decay", "D\xC3\xA9" "clin", "%", 0, 100, 55),
                P ("tension", "Tension", "%", 0, 100, 50),
                P ("tone", "Ton", "%", 0, 100, 50),
                P ("drip", "\xC3\x89" "claboussure", "%", 0, 100, 30),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 30),
            };
            m.sections = { { "spring", "Ressort", "reverb", { "springs", "decay", "tension", "tone", "drip", "mix" }, "main" } };
            m.presets = {
                { "Ampli de guitare", { { "springs", 2 }, { "decay", 50 }, { "tension", 45 }, { "tone", 45 } }, "Classiques" },
                { "Dub", { { "springs", 3 }, { "decay", 80 }, { "drip", 70 }, { "mix", 45 } }, "Effets" },
                { "Surf", { { "springs", 2 }, { "decay", 65 }, { "drip", 85 }, { "tension", 70 }, { "mix", 40 } }, "Effets" },
                { "Discret", { { "springs", 1 }, { "decay", 35 }, { "mix", 18 } }, "Classiques" },
            };
            return m;
        }();
        return d;
    }
}

class SpringReverb final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return spring::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        static constexpr double lens[3] { 33.0, 41.0, 37.0 };
        for (int k = 0; k < 3; ++k) sp[(size_t) k].prepare (s, lens[k]);
        reset();
    }
    void reset() noexcept override { for (auto& x : sp) x.reset(); dripHp.reset(); }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace spring;
        const auto count = std::clamp ((int) std::lround (p[springs]), 1, 3);
        const auto fb = 0.55 + 0.42 * p[decay] / 100.0;
        const auto ten = p[tension] / 100.0;
        const auto toneC = 1.0 - std::exp (-2.0 * kPi * (2000.0 + 3500.0 * p[tone] / 100.0) / sr);
        const auto drp = p[drip] / 100.0;
        dripHp.set (SvfEq::highpass, 1500.0, 0.7, 0.0, sr);
        const auto wet = p[mix] / 100.0;
        const auto g = 0.9 / std::sqrt ((double) count);
        for (int i = 0; i < n; ++i)
        {
            const double dl = L[i], dr = R[i];
            auto in = 0.5 * (dl + dr);
            // éclaboussure : les attaques aiguës secouent les ressorts plus fort
            in += dripHp.process (in) * drp * 3.0;
            double ol = 0.0, orr = 0.0;
            for (int k = 0; k < count; ++k)
            {
                const auto y = sp[(size_t) k].process (in * 0.6, ten, fb, toneC, 1.0);
                if (k == 1) orr += y; else if (k == 0) ol += y; else { ol += 0.6 * y; orr += 0.6 * y; }
            }
            if (count == 1) orr = ol;
            ol *= g; orr *= g;
            L[i] = (float) (dl * (1.0 - wet) + ol * wet);
            R[i] = (float) (dr * (1.0 - wet) + orr * wet);
        }
    }
private:
    double sr = 48000.0;
    std::array<Spring, 3> sp;
    SvfEq dripHp;
};

//==============================================================================
namespace tapeecho
{
    enum Slot { heads = 0, sync, speed, division, intensity, echoLevel, springLevel, bass, treble, wear, drive, mix, numSlots };
    inline const std::vector<std::string>& headChoices()
    {
        static const std::vector<std::string> v { "T\xC3\xAA" "te 1", "T\xC3\xAA" "te 2", "T\xC3\xAA" "te 3", "1 + 2", "2 + 3", "1 + 3", "1 + 2 + 3" };
        return v;
    }
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tapeecho"; m.name = "Reel"; m.role = "\xC3\x89" "cho \xC3\xA0 bande \xC3\xA0 t\xC3\xAA" "tes"; m.family = "\xC3\x89" "chos"; m.form = "pedal";
            m.principle = "Bande en boucle, 3 t\xC3\xAA" "tes de lecture, intensit\xC3\xA9 jusqu'\xC3\xA0 l'auto-oscillation, usure, ressort int\xC3\xA9gr\xC3\xA9";
            m.params = {
                C ("heads", "T\xC3\xAA" "tes", headChoices(), 3),
                C ("sync", "Vitesse", { "Libre", "Tempo" }),
                P ("speed", "\xC3\x89" "cart", "ms", 40, 400, 140, 0.6f),
                C ("division", "Division (t\xC3\xAA" "te 1)", { "1/32", "1/16 t", "1/16", "1/8 t", "1/16 p", "1/8", "1/4 t", "1/8 p", "1/4" }, 5),
                P ("intensity", "Intensit\xC3\xA9", "%", 0, 110, 45),
                P ("echo", "\xC3\x89" "cho", "%", 0, 100, 60),
                P ("spring", "Ressort", "%", 0, 100, 0),
                P ("bass", "Grave", "dB", -12, 6, 0),
                P ("treble", "Aigu", "dB", -12, 6, -2),
                P ("wear", "Usure", "%", 0, 100, 30),
                P ("drive", "Saturation", "%", 0, 100, 25),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
            };
            m.sections = {
                { "tape", "Bande", "delay", { "heads", "sync", "speed", "division", "intensity", "echo" }, "main" },
                { "tone", "Ton et usure", "delay", { "spring", "bass", "treble", "wear", "drive", "mix" }, "tone" },
            };
            m.presets = {
                { "Dub classique", { { "heads", 4 }, { "intensity", 70 }, { "speed", 170 }, { "spring", 30 }, { "treble", -4 } }, "Dub" },
                { "Slapback", { { "heads", 0 }, { "speed", 90 }, { "intensity", 10 }, { "echo", 45 } }, "Classiques" },
                { "Triolet galopant", { { "heads", 6 }, { "sync", 1 }, { "division", 3 }, { "intensity", 40 } }, "Rythmes" },
                { "Auto-oscillation", { { "heads", 1 }, { "intensity", 105 }, { "wear", 50 }, { "drive", 50 } }, "Effets" },
                { "Bande fatigu\xC3\xA9" "e", { { "heads", 3 }, { "wear", 90 }, { "treble", -8 }, { "drive", 45 } }, "Effets" },
                { "Ressort seul", { { "echo", 0 }, { "spring", 60 } }, "Classiques" },
            };
            return m;
        }();
        return d;
    }
}

class TapeEcho final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return tapeecho::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        tape.init ((int) (s * 3.3 * 1.05) + 64); // 3 têtes × 1,1 s au plus, marge de pleurage
        spr.prepare (s, 37.0);
        reset();
    }
    void reset() noexcept override
    {
        tape.clear(); spr.reset(); wowPh = fltPh = 0.0; lpS = hpS = 0.0; bassF.reset(); trebF.reset(); headSm = -1.0; noise = 1u;
    }
    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace tapeecho;
        static constexpr double div[9] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0 }; // en noires
        double head1 = p[sync] > 0.5f ? div[std::clamp ((int) p[division], 0, 8)] * 60.0 / std::max (20.0, bpm) * 1000.0 : (double) p[speed];
        head1 = std::clamp (head1, 20.0, 1100.0) * 0.001 * sr;
        if (headSm < 0.0) headSm = head1;
        static constexpr int combos[7][3] { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 0 }, { 0, 1, 1 }, { 1, 0, 1 }, { 1, 1, 1 } };
        const auto& c = combos[std::clamp ((int) p[heads], 0, 6)];
        const auto active = (double) (c[0] + c[1] + c[2]);
        const auto fbAmt = p[intensity] / 100.0 / std::sqrt (active) * 0.95;
        const auto outNorm = 1.0 / std::sqrt (active);
        const auto echoG = p[echoLevel] / 100.0;
        const auto sprG = p[springLevel] / 100.0;
        const auto wr = p[wear] / 100.0;
        const auto drv = 0.5 + 2.5 * p[drive] / 100.0;
        const auto wet = p[mix] / 100.0;
        bassF.set (SvfEq::lowShelf, 200.0, 0.7, p[bass], sr);
        trebF.set (SvfEq::highShelf, 3000.0, 0.7, p[treble], sr);
        const auto lpC = 1.0 - std::exp (-2.0 * kPi * (9000.0 - 5500.0 * wr) / sr); // la bande use les aigus
        const auto hpC = 1.0 - std::exp (-2.0 * kPi * 80.0 / sr);
        const auto smooth = 1.0 - std::exp (-1.0 / (0.15 * sr));                      // le moteur change de vitesse en glissant
        for (int i = 0; i < n; ++i)
        {
            const double dl = L[i], dr = R[i];
            headSm += (head1 - headSm) * smooth;
            wowPh += 0.55 / sr; if (wowPh >= 1.0) wowPh -= 1.0;
            fltPh += 6.3 / sr; if (fltPh >= 1.0) fltPh -= 1.0;
            const auto wobble = 1.0 + wr * (0.006 * fastSin2Pi (wowPh) + 0.0015 * fastSin2Pi (fltPh));
            double echo = 0.0;
            for (int h = 0; h < 3; ++h)
                if (c[h] != 0) echo += tape.atFrac (headSm * (h + 1) * wobble);
            // boucle : filtres de bande, grave / aigu, saturation (borne l'auto-oscillation)
            lpS += (echo - lpS) * lpC; hpS += (lpS - hpS) * hpC;
            auto loop = trebF.process (bassF.process (lpS - hpS));
            noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
            const auto hiss = ((double) noise / 4294967296.0 - 0.5) * 0.0006 * wr;
            const auto in = 0.5 * (dl + dr);
            tape.push (std::tanh ((in + loop * fbAmt) * drv) / drv + hiss);
            const auto rev = sprG > 0.0 ? spr.process ((in + loop * echoG) * 0.5, 0.55, 0.85, 0.35, 1.0) * sprG : 0.0;
            const auto out = loop * echoG * outNorm + rev;
            L[i] = (float) (dl + out * wet);
            R[i] = (float) (dr + out * wet);
        }
    }
private:
    double sr = 48000.0, wowPh = 0.0, fltPh = 0.0, lpS = 0.0, hpS = 0.0, headSm = -1.0;
    std::uint32_t noise = 1u;
    DLine tape;
    Spring spr;
    SvfEq bassF, trebF;
};

} // namespace grv::effects
