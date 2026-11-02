/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Lot 2 des effets (v0.38), chacun sur son principe (architecture seulement, aucune reprise de code,
    de façade ni de réglages d'usine) :

      - « Saturation à bande » (principe des émulations de magnétophones de studio) : entrée qui pousse
        la bande, saturation à mémoire (hystérésis simplifiée : la sortie précédente tire la courbe),
        asymétrie (polarisation), bosse du grave de la tête de lecture et perte d'aigus selon la vitesse
        (19, 38, 76 cm/s), pleurage, souffle, sortie, mélange. Suréchantillonnage ×2 IIR.
      - « Saturation à lampe » (principe des étages de triode) : courbe asymétrique (harmoniques paires),
        polarisation, « affaissement » (l'alimentation fléchit sur les attaques : compression douce),
        tonalité inclinée, sortie, mélange. ×2 IIR.
      - « Compresseur à distorsion » (principe des compresseurs à distorsion de studio des années 90) :
        entrée qui pousse un seuil fixe, attaque et relâchement 0 à 10, rapports 1:1 (couleur seule) à
        20:1 et « Nuke », filtres de détection (passe-haut, bande de la voix), distorsion 2 (harmoniques
        paires) ou 3 (impaires), mode « britannique » (tous rapports : compression agressive), mélange.
      - « Sculpteur de transitoires » (principe des « transient designers ») : deux paires de suiveurs
        comparés : l'attaque monte ou descend les débuts de notes, le maintien les queues, sans seuil.
      - « Chorus à trois voix » (principe des chorus à trois lignes à seau déphasées de 120°) : chorus,
        vibrato ou ensemble (deux vitesses mêlées), largeur, ton.
      - « Réverbération de prestige » (principe des grandes réverbérations algorithmiques de studio) :
        réseau de 8 lignes à matrice de Hadamard (écho dense, sans métal), diffusion d'entrée, premières
        réflexions, déclin réglable séparément pour le grave et l'aigu, modulation lente des lignes,
        6 modes (salle, chambre, plaque, église, studio, halo), gel. Algorithmique : aucune latence.

    Temps réel : tout est réservé dans prepare(), aucune allocation ni verrou dans le rendu.
*/

#pragma once

#include "Dynamics.h"
#include "ModFx36.h"
#include "SpaceFx.h"
#include "../dsp/Oversampler.h"

#include <array>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

//==============================================================================
namespace tapesat
{
    enum Slot { drive = 0, bias, speed, bump, treble, wow, hiss, output, mix, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tapesat"; m.name = "Ferrite"; m.role = "Saturation \xC3\xA0 bande"; m.family = "Saturation"; m.form = "pedal";
            m.principle = "Bande pouss\xC3\xA9" "e : saturation \xC3\xA0 m\xC3\xA9moire, polarisation, bosse du grave et perte d'aigus selon la vitesse, pleurage, souffle";
            m.params = {
                P ("drive", "Entr\xC3\xA9" "e", "dB", -6, 24, 6),
                P ("bias", "Polarisation", "%", -100, 100, 0),
                C ("speed", "Vitesse", { "19 cm/s", "38 cm/s", "76 cm/s" }, 1),
                P ("bump", "Bosse du grave", "dB", 0, 6, 2),
                P ("treble", "Aigus", "dB", -12, 6, 0),
                P ("wow", "Pleurage", "%", 0, 100, 10),
                P ("hiss", "Souffle", "%", 0, 100, 0),
                P ("output", "Sortie", "dB", -24, 6, -3),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
            };
            m.sections = { { "tape", "Bande", "dist", { "drive", "bias", "speed", "bump", "treble", "wow", "hiss", "output" }, "main" },
                           { "out", "Sortie", "dist", { "mix" }, "main" } };
            m.presets = { { "Chaleur de bande", {} }, { "Bande pouss\xC3\xA9" "e", { { "drive", 16 }, { "output", -8 } } },
                          { "Cassette fatigu\xC3\xA9" "e", { { "speed", 0 }, { "wow", 60 }, { "hiss", 35 }, { "treble", -6 } } },
                          { "Master 76 cm/s", { { "speed", 2 }, { "drive", 2 }, { "bump", 1 }, { "wow", 0 }, { "output", -1 } } },
                          { "Batterie \xC3\xA9" "cras\xC3\xA9" "e", { { "drive", 20 }, { "bias", 30 }, { "output", -10 } } } };
            return m;
        }();
        return d;
    }
}

class TapeSaturator final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return tapesat::desc(); }
    void prepare (double s, int) override { sr = s; for (auto& c : ch) c.wow.init ((int) (0.02 * sr)); reset(); }
    void reset() noexcept override { for (auto& c : ch) { c.os.reset(); c.dryOs.reset(); c.wow.clear(); c.bump.reset(); c.hf.reset(); c.loss.reset(); c.y = 0.0; c.dc = c.dcx = 0.0; } wowPh = flutPh = 0.0; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace tapesat;
        const auto g = dbToGain (p[drive]);
        const auto b = p[bias] / 100.0 * 0.35;
        const auto sp = std::clamp ((int) p[speed], 0, 2);
        static constexpr double bumpHz[3] { 55.0, 80.0, 110.0 }, lossHz[3] { 9000.0, 15000.0, 20000.0 };
        for (auto& c : ch)
        {
            c.bump.set (SvfEq::bell, bumpHz[sp], 1.1, p[bump], sr);
            c.hf.set (SvfEq::highShelf, 6000.0, 0.7, p[treble], sr);
            c.loss.set (SvfEq::lowpass, std::min (lossHz[sp], 0.45 * sr), 0.6, 0.0, sr);
        }
        const auto wowD = p[wow] / 100.0 * 0.0012 * sr, flD = p[wow] / 100.0 * 0.00012 * sr;
        const auto hissA = p[hiss] / 100.0 * 0.004;
        const auto outG = dbToGain (p[output]), wet = p[mix] / 100.0;
        const auto dcC = std::exp (-kTwoPi * 10.0 / sr);
        for (int i = 0; i < n; ++i)
        {
            wowPh += 0.55 / sr; if (wowPh >= 1.0) wowPh -= 1.0;
            flutPh += 6.7 / sr; if (flutPh >= 1.0) flutPh -= 1.0;
            const auto mod = 2.0 + wowD * (1.0 + std::sin (kTwoPi * wowPh)) + flD * (1.0 + std::sin (kTwoPi * flutPh));
            const double in[2] { L[i], R[i] };
            double out[2];
            for (int k = 0; k < 2; ++k)
            {
                auto& c = ch[(size_t) k];
                double a, bb;
                c.os.up (in[k] * g, a, bb);
                // saturation à mémoire : la sortie précédente tire la courbe (boucle d'hystérésis)
                auto sat = [&] (double x) { const auto y = fastTanh (x + b + 0.18 * c.y) - fastTanh (b); c.y = y; return y; };
                a = sat (a); bb = sat (bb);
                auto y = c.os.down (a, bb) / std::max (1.0, std::sqrt (g));
                c.dc = y - c.dcx + dcC * c.dc; c.dcx = y; y = c.dc;
                y = c.hf.process (c.bump.process (c.loss.process (y)));
                c.wow.push (y);
                y = c.wow.read (mod);
                if (hissA > 0.0) { seed = seed * 1664525u + 1013904223u; y += hissA * ((double) (seed >> 9) / 4194304.0 - 1.0); }
                double da, db; c.dryOs.up (in[k], da, db);
                const auto dry = c.dryOs.down (da, db);
                out[k] = (dry * (1.0 - wet) + y * wet) * outG;
            }
            L[i] = (float) out[0]; R[i] = (float) out[1];
        }
    }
private:
    struct Ch { Oversampler2x os, dryOs; ModLine wow; SvfEq bump, hf, loss; double y = 0.0, dc = 0.0, dcx = 0.0; };
    std::array<Ch, 2> ch;
    double sr = 48000.0, wowPh = 0.0, flutPh = 0.0;
    std::uint32_t seed = 99u;
};

//==============================================================================
namespace tubesat
{
    enum Slot { drive = 0, bias, sag, tone, output, mix, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tubesat"; m.name = "Glow"; m.role = "Saturation \xC3\xA0 lampe"; m.family = "Saturation"; m.form = "pedal";
            m.principle = "\xC3\x89tage de triode : courbe asym\xC3\xA9trique (harmoniques paires), polarisation, affaissement de l'alimentation, tonalit\xC3\xA9";
            m.params = {
                P ("drive", "Gain", "%", 0, 100, 40),
                P ("bias", "Polarisation", "%", -100, 100, 20),
                P ("sag", "Affaissement", "%", 0, 100, 30),
                P ("tone", "Tonalit\xC3\xA9", "%", 0, 100, 50),
                P ("output", "Sortie", "dB", -24, 6, -2),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
            };
            m.sections = { { "tube", "Lampe", "dist", { "drive", "bias", "sag", "tone", "output", "mix" }, "main" } };
            m.presets = { { "Chaleur", {} }, { "Pr\xC3\xA9" "ampli pouss\xC3\xA9", { { "drive", 75 }, { "sag", 50 }, { "output", -8 } } },
                          { "Couleur l\xC3\xA9g\xC3\xA8re (bus)", { { "drive", 15 }, { "sag", 10 }, { "output", 0 }, { "mix", 60 } } },
                          { "Voix sale", { { "drive", 60 }, { "bias", 45 }, { "tone", 62 }, { "output", -6 } } } };
            return m;
        }();
        return d;
    }
}

class TubeSaturator final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return tubesat::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override { for (auto& c : ch) { c.os.reset(); c.dryOs.reset(); c.lo.reset(); c.hi.reset(); c.dc = c.dcx = 0.0; } env = 0.0; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace tubesat;
        const auto dr = p[drive] / 100.0;
        const auto g = 1.0 + 30.0 * dr * dr;
        const auto b = p[bias] / 100.0 * 0.5;
        const auto sagA = p[sag] / 100.0;
        const auto tilt = (p[tone] - 50.0) / 50.0 * 6.0;
        for (auto& c : ch) { c.lo.set (SvfEq::lowShelf, 900.0, 0.6, -tilt, sr); c.hi.set (SvfEq::highShelf, 900.0, 0.6, tilt, sr); }
        const auto outG = dbToGain (p[output]), wet = p[mix] / 100.0;
        const auto eAtt = std::exp (-1.0 / (0.003 * sr)), eRel = std::exp (-1.0 / (0.12 * sr));
        const auto dcC = std::exp (-kTwoPi * 10.0 / sr);
        for (int i = 0; i < n; ++i)
        {
            const auto pk = std::max (std::abs ((double) L[i]), std::abs ((double) R[i])) * g;
            env = pk > env ? pk + (env - pk) * eAtt : pk + (env - pk) * eRel;
            const auto gs = g / (1.0 + sagA * 0.6 * env); // l'alimentation fléchit : le gain baisse sur les attaques
            const double in[2] { L[i], R[i] };
            double out[2];
            for (int k = 0; k < 2; ++k)
            {
                auto& c = ch[(size_t) k];
                double a, bb;
                c.os.up (in[k] * gs, a, bb);
                auto tri = [&] (double x) { const auto v = x + b; const auto y = v > 0.0 ? fastTanh (v) : fastTanh (v * 0.6) / 0.6 * 0.75; return y; };
                const auto off = tri (0.0);
                a = tri (a) - off; bb = tri (bb) - off;
                auto y = c.os.down (a, bb) / std::sqrt (g);
                c.dc = y - c.dcx + dcC * c.dc; c.dcx = y; y = c.dc;
                y = c.hi.process (c.lo.process (y));
                double da, db; c.dryOs.up (in[k], da, db);
                const auto dry = c.dryOs.down (da, db);
                out[k] = (dry * (1.0 - wet) + y * wet) * outG;
            }
            L[i] = (float) out[0]; R[i] = (float) out[1];
        }
    }
private:
    struct Ch { Oversampler2x os, dryOs; SvfEq lo, hi; double dc = 0.0, dcx = 0.0; };
    std::array<Ch, 2> ch;
    double sr = 48000.0, env = 0.0;
};

//==============================================================================
namespace distcomp
{
    enum Slot { input = 0, output, attack, release, ratio, detector, audio, british, mix, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "distcomp"; m.name = "Bruiser"; m.role = "Compresseur \xC3\xA0 distorsion"; m.family = "Compresseurs et dynamique"; m.form = "rack";
            m.principle = "Seuil fixe pouss\xC3\xA9 par l'entr\xC3\xA9" "e, rapports 1:1 \xC3\xA0 20:1 et Nuke, distorsion 2 ou 3, filtres de d\xC3\xA9tection, mode britannique";
            m.params = {
                P ("input", "Entr\xC3\xA9" "e", "dB", 0, 40, 18),
                P ("output", "Sortie", "dB", -30, 10, -6),
                P ("attack", "Attaque", "", 0, 10, 5),
                P ("release", "Rel\xC3\xA2" "chement", "", 0, 10, 5),
                C ("ratio", "Rapport", { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "Nuke" }, 3),
                C ("detector", "D\xC3\xA9tection", { "Normale", "Passe-haut", "Bande voix", "Les deux" }),
                C ("audio", "Distorsion", { "Propre", "Distorsion 2", "Distorsion 3" }),
                C ("british", "Britannique", { "Non", "Oui" }),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
            };
            m.sections = { { "comp", "Compression", "comp", { "input", "output", "attack", "release", "ratio", "detector", "audio", "british" }, "main" },
                           { "out", "Sortie", "comp", { "mix" }, "main" } };
            m.presets = { { "Voix en avant", {} }, { "Batterie \xC3\xA9" "cras\xC3\xA9" "e (Nuke)", { { "ratio", 7 }, { "input", 28 }, { "audio", 2 }, { "mix", 50 } } },
                          { "Couleur 1:1", { { "ratio", 0 }, { "audio", 1 }, { "input", 10 }, { "output", -3 } } },
                          { "Britannique", { { "british", 1 }, { "ratio", 5 }, { "attack", 2 }, { "release", 3 }, { "input", 20 } } },
                          { "Basse tenue", { { "ratio", 2 }, { "detector", 1 }, { "attack", 6 }, { "release", 7 }, { "audio", 1 } } } };
            return m;
        }();
        return d;
    }
}

class DistortionCompressor final : public CompressorBase
{
public:
    const ModuleDesc& desc() const noexcept override { return distcomp::desc(); }
    void reset() noexcept override { CompressorBase::reset(); hp.reset(); bp.reset(); hp2.reset(); bp2.reset(); slow = 0.0; dcL = dcR = dxL = dxR = 0.0; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace distcomp;
        meters (L, R, n, true);
        static constexpr double ratios[8] { 1.0, 2.0, 3.0, 4.0, 6.0, 10.0, 20.0, 1000.0 };
        const auto ri = std::clamp ((int) p[ratio], 0, 7);
        const bool brit = p[british] > 0.5f;
        const auto r = brit ? 30.0 : ratios[ri];
        const auto attMs = 0.05 * std::pow (600.0, p[attack] / 10.0);        // 0,05 à 30 ms
        const auto relMs = 50.0 * std::pow (70.0, p[release] / 10.0);        // 50 ms à 3,5 s
        const auto att = msCoef (ri == 7 ? std::min (attMs, 0.2) : attMs, sr), rel = msCoef (relMs, sr), relSlow = msCoef (relMs * 3.0, sr);
        const auto inG = dbToGain (p[input]), outG = dbToGain (p[output]), wet = p[mix] / 100.0;
        const auto det = std::clamp ((int) p[detector], 0, 3);
        hp.set (SvfEq::highpass, 120.0, 0.7, 0.0, sr); hp2.set (SvfEq::highpass, 120.0, 0.7, 0.0, sr);
        bp.set (SvfEq::bell, 6000.0, 1.0, 6.0, sr); bp2.set (SvfEq::bell, 6000.0, 1.0, 6.0, sr);
        const auto dmode = std::clamp ((int) p[audio], 0, 2);
        const auto thr = -24.0;
        const auto dcC = std::exp (-kTwoPi * 10.0 / sr);
        for (int i = 0; i < n; ++i)
        {
            const double l = L[i] * inG, rr = R[i] * inG;
            double dl = l, dr = rr;
            if (det == 1 || det == 3) { dl = hp.process (dl); dr = hp2.process (dr); }
            if (det == 2 || det == 3) { dl = bp.process (dl); dr = bp2.process (dr); }
            const auto lvl = gainToDb (std::max (std::abs (dl), std::abs (dr)));
            auto target = -compressDb (lvl, thr, r, ri == 0 ? 0.0 : 6.0);
            if (brit) target *= 1.25;                                   // tous les rapports : plus agressif
            if (target > grDb) grDb = target + (grDb - target) * att;
            else
            {
                // relâchement en deux temps selon la durée de la compression
                slow = std::max (0.0, slow + (grDb > 3.0 ? 2.0 / sr : -1.0 / sr));
                const auto k = std::min (1.0, slow);
                grDb = target + (grDb - target) * (rel * (1.0 - k) + relSlow * k);
            }
            const auto g = dbToGain (-grDb);
            double ol = l * g, orr = rr * g;
            // couleur : 1:1 colore aussi ; distorsion 2 = paires (asymétrique), 3 = impaires
            const auto amt = dmode == 0 ? 0.04 : 0.25;
            if (dmode == 2) { ol = fastTanh (ol * (1.0 + amt * 4.0)) / (1.0 + amt * 2.0); orr = fastTanh (orr * (1.0 + amt * 4.0)) / (1.0 + amt * 2.0); }
            else { ol = ol + amt * ol * ol * 0.8; orr = orr + amt * orr * orr * 0.8; ol = fastTanh (ol); orr = fastTanh (orr); }
            dcL = ol - dxL + dcC * dcL; dxL = ol; ol = dcL;
            dcR = orr - dxR + dcC * dcR; dxR = orr; orr = dcR;
            // direct sans l'entrée poussée ; la sortie règle la part compressée
            L[i] = (float) (L[i] * (1.0 - wet) + ol * outG * wet);
            R[i] = (float) (R[i] * (1.0 - wet) + orr * outG * wet);
        }
        meters (L, R, n, false);
    }
private:
    SvfEq hp, bp, hp2, bp2;
    double slow = 0.0, dcL = 0.0, dcR = 0.0, dxL = 0.0, dxR = 0.0;
};

//==============================================================================
namespace transient
{
    enum Slot { attack = 0, sustain, speed, clip, output, mix, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "transient"; m.name = "Snap"; m.role = "Sculpteur de transitoires"; m.family = "Compresseurs et dynamique"; m.form = "pedal";
            m.principle = "Attaque et maintien relev\xC3\xA9s ou creus\xC3\xA9s sans seuil (deux paires de suiveurs compar\xC3\xA9s)";
            m.params = {
                P ("attack", "Attaque", "%", -100, 100, 40),
                P ("sustain", "Maintien", "%", -100, 100, 0),
                C ("speed", "Vitesse", { "Rapide", "Moyenne", "Lente" }, 1),
                C ("clip", "\xC3\x89" "cr\xC3\xAAtage", { "Non", "Oui" }),
                P ("output", "Sortie", "dB", -18, 12, 0),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
            };
            m.sections = { { "tr", "Transitoires", "comp", { "attack", "sustain", "speed", "clip", "output", "mix" }, "main" } };
            m.presets = { { "Attaque en avant", {} }, { "Batterie s\xC3\xA8" "che", { { "attack", 30 }, { "sustain", -60 } } },
                          { "Ambiance relev\xC3\xA9" "e", { { "attack", -20 }, { "sustain", 70 }, { "speed", 2 } } },
                          { "Claque", { { "attack", 90 }, { "clip", 1 }, { "output", -4 } } } };
            return m;
        }();
        return d;
    }
}

class TransientShaper final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return transient::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override { aF = aS = sF = sS = 0.0; gPrev = 0.0; }
    float meter (int i) const noexcept override { return i == 0 ? (float) std::abs (gPrev) : 0.0f; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace transient;
        static constexpr double spd[3] { 0.6, 1.0, 1.8 };
        const auto k = spd[std::clamp ((int) p[speed], 0, 2)];
        auto c = [&] (double ms) { return std::exp (-1.0 / (ms * k * 0.001 * sr)); };
        const auto aFa = c (0.5), aSa = c (18.0), aRel = c (60.0);
        const auto sFr = c (25.0), sSr = c (350.0), sAtt = c (1.0);
        const auto at = p[attack] / 100.0, su = p[sustain] / 100.0;
        const bool clipOn = p[clip] > 0.5f;
        const auto outG = dbToGain (p[output]), wet = p[mix] / 100.0;
        for (int i = 0; i < n; ++i)
        {
            const auto x = std::max (std::abs ((double) L[i]), std::abs ((double) R[i])) + 1.0e-9;
            aF = x > aF ? x + (aF - x) * aFa : x + (aF - x) * aRel;
            aS = x > aS ? x + (aS - x) * aSa : x + (aS - x) * aRel;
            sF = x > sF ? x + (sF - x) * sAtt : x + (sF - x) * sFr;
            sS = x > sS ? x + (sS - x) * sAtt : x + (sS - x) * sSr;
            const auto attD = std::clamp (gainToDb (aF) - gainToDb (aS), 0.0, 24.0);
            const auto susD = std::clamp (gainToDb (sS) - gainToDb (sF), 0.0, 24.0);
            const auto gDb = std::clamp (at * attD * 1.2 + su * susD * 1.0, -30.0, 18.0);
            gPrev = gDb;
            const auto g = dbToGain (gDb) * outG;
            double ol = L[i] * g, orr = R[i] * g;
            if (clipOn) { ol = fastTanh (ol); orr = fastTanh (orr); }
            L[i] = (float) (L[i] * (1.0 - wet) * outG + ol * wet);
            R[i] = (float) (R[i] * (1.0 - wet) * outG + orr * wet);
        }
    }
private:
    double sr = 48000.0, aF = 0.0, aS = 0.0, sF = 0.0, sS = 0.0, gPrev = 0.0;
};

//==============================================================================
namespace chorus3
{
    enum Slot { mode = 0, rate, depth, width, tone, mix, sync, division, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "chorus3"; m.name = "Trinity"; m.role = "Chorus \xC3\xA0 trois voix"; m.family = "Modulation"; m.form = "pedal";
            m.principle = "Trois lignes \xC3\xA0 seau d\xC3\xA9phas\xC3\xA9" "es de 120\xC2\xB0 : chorus, vibrato ou ensemble (deux vitesses), largeur, ton";
            m.params = {
                C ("mode", "Mode", { "Chorus", "Vibrato", "Ensemble" }),
                P ("rate", "Vitesse", "Hz", 0.05f, 8, 0.5f, 0.4f),
                P ("depth", "Profondeur", "%", 0, 100, 50),
                P ("width", "Largeur", "%", 0, 100, 100),
                P ("tone", "Ton", "Hz", 2000, 16000, 8000, 0.4f),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 50),
                // v0.45 : vitesse libre ou calée sur le tempo (ajoutés à la fin : positions inchangées)
                C ("sync", "Synchro", { "Libre", "Tempo" }),
                C ("division", "Division", syncDivisions(), 4),
            };
            m.sections = { { "c3", "Chorus", "lfo", { "mode", "sync", "rate", "division", "depth", "width", "tone", "mix" }, "main" } };
            m.presets = { { "Trois voix", {} }, { "Ensemble de cordes", { { "mode", 2 }, { "depth", 60 }, { "mix", 60 } } },
                          { "Vibrato", { { "mode", 1 }, { "rate", 5 }, { "depth", 35 }, { "mix", 100 } } },
                          { "Large et lent", { { "rate", 0.15f }, { "depth", 80 }, { "mix", 55 } } } };
            return m;
        }();
        return d;
    }
}

class TriChorus final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return chorus3::desc(); }
    void prepare (double s, int) override { sr = s; line.init ((int) (0.04 * sr)); reset(); }
    void reset() noexcept override { line.clear(); lp.fill (0.0); ph1 = ph2 = 0.0; wetPrev = -1.0; }
    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace chorus3;
        const auto hz = p[sync] > 0.5f ? (std::max (20.0, bpm) / 60.0) / syncQuarters ((int) p[division]) : (double) p[rate];
        const auto md = std::clamp ((int) p[mode], 0, 2);
        const auto dp = p[depth] / 100.0 * (md == 1 ? 0.004 : 0.006) * sr;
        const auto base = (md == 1 ? 0.004 : 0.008) * sr;
        const auto wd = p[width] / 100.0;
        const auto lpC = std::exp (-kTwoPi * std::min ((double) p[tone], 0.45 * sr) / sr);
        const auto wet = md == 1 ? 1.0 : p[mix] / 100.0;
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dW = (wet - wet0) / std::max (1, n);
        wetPrev = wet;
        for (int i = 0; i < n; ++i)
        {
            const auto wt = wet0 + dW * (i + 1);
            ph1 += hz / sr; if (ph1 >= 1.0) ph1 -= 1.0;
            ph2 += 6.1 / sr; if (ph2 >= 1.0) ph2 -= 1.0;
            const auto mono = 0.5 * ((double) L[i] + R[i]);
            line.push (mono);
            double v[3];
            for (int k = 0; k < 3; ++k)
            {
                auto mod = std::sin (kTwoPi * (ph1 + k / 3.0));
                if (md == 2) mod = 0.75 * mod + 0.25 * std::sin (kTwoPi * (ph2 + k / 3.0)); // ensemble : vitesse lente + rapide
                const auto y = line.read (base + dp * 0.5 * (1.0 + mod));
                lp[(size_t) k] = y + (lp[(size_t) k] - y) * lpC;
                v[k] = lp[(size_t) k];
            }
            // voix 1 à gauche, 2 au centre, 3 à droite (largeur)
            const auto wl = v[0] * (0.5 + 0.5 * wd) + v[1] * 0.5 + v[2] * (0.5 - 0.5 * wd);
            const auto wr = v[2] * (0.5 + 0.5 * wd) + v[1] * 0.5 + v[0] * (0.5 - 0.5 * wd);
            L[i] = (float) (L[i] * (1.0 - wt) + wl * 0.8 * wt + (md == 1 ? 0.0 : L[i] * 0.3 * wt));
            R[i] = (float) (R[i] * (1.0 - wt) + wr * 0.8 * wt + (md == 1 ? 0.0 : R[i] * 0.3 * wt));
        }
    }
private:
    ModLine line;
    std::array<double, 3> lp {};
    double sr = 48000.0, ph1 = 0.0, ph2 = 0.0, wetPrev = -1.0;
};

//==============================================================================
namespace prestige
{
    enum Slot { mode = 0, predelay, size, decay, lowMult, highMult, xover, early, diffusion, modulation, lowCut, highCut, width, freeze, mix, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "prestige"; m.name = "Sovereign"; m.role = "R\xC3\xA9verb\xC3\xA9ration de prestige"; m.family = "R\xC3\xA9verb\xC3\xA9rations"; m.form = "rack";
            m.principle = "R\xC3\xA9seau de 8 lignes (matrice de Hadamard), premi\xC3\xA8res r\xC3\xA9" "flexions, d\xC3\xA9" "clin s\xC3\xA9par\xC3\xA9 grave / aigu, 6 modes, gel ; sans latence";
            m.params = {
                C ("mode", "Mode", { "Salle", "Chambre", "Plaque", "\xC3\x89glise", "Studio", "Halo" }),
                P ("predelay", "Pr\xC3\xA9-d\xC3\xA9lai", "ms", 0, 250, 20, 0.5f),
                P ("size", "Taille", "%", 25, 200, 100),
                P ("decay", "D\xC3\xA9" "clin", "s", 0.2f, 30, 2.4f, 0.35f),
                P ("lowmult", "D\xC3\xA9" "clin grave", "\xC3\x97", 0.5f, 2, 1.2f),
                P ("highmult", "D\xC3\xA9" "clin aigu", "\xC3\x97", 0.1f, 1.5f, 0.6f),
                P ("xover", "S\xC3\xA9paration", "Hz", 200, 6000, 1500, 0.4f),
                P ("early", "R\xC3\xA9" "flexions", "%", 0, 100, 40),
                P ("diffusion", "Diffusion", "%", 0, 100, 75),
                P ("modulation", "Modulation", "%", 0, 100, 25),
                P ("lowcut", "Coupe-bas", "Hz", 20, 1000, 80, 0.4f),
                P ("highcut", "Coupe-haut", "Hz", 2000, 20000, 14000, 0.4f),
                P ("width", "Largeur", "%", 0, 150, 100),
                C ("freeze", "Gel", { "Non", "Oui" }),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 30),
            };
            m.sections = {
                { "space", "Espace", "reverb", { "mode", "predelay", "size", "decay", "early", "diffusion", "modulation", "mix" }, "main" },
                { "colour", "D\xC3\xA9" "clin et couleur", "reverb", { "lowmult", "highmult", "xover", "lowcut", "highcut", "width", "freeze" }, "main" },
            };
            m.presets = { { "Grande salle", {} }, { "Chambre de bois", { { "mode", 1 }, { "decay", 1.2f }, { "early", 70 } } },
                          { "Plaque brillante", { { "mode", 2 }, { "decay", 1.8f }, { "highmult", 1.0f }, { "early", 0 } } },
                          { "\xC3\x89glise", { { "mode", 3 }, { "decay", 6 }, { "size", 160 }, { "predelay", 40 } } },
                          { "Studio serr\xC3\xA9", { { "mode", 4 }, { "decay", 0.6f }, { "early", 60 }, { "mix", 20 } } },
                          { "Halo infini", { { "mode", 5 }, { "decay", 18 }, { "modulation", 70 }, { "highmult", 0.8f } } } };
            return m;
        }();
        return d;
    }
}

class PrestigeReverb final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return prestige::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& l : lines) l.init ((int) (0.2 * sr) + 64);
        for (auto& a : diff) a.init ((int) (0.02 * sr) + 64);
        pre.init ((int) (0.26 * sr));
        er.init ((int) (0.25 * sr));
        reset();
    }
    void reset() noexcept override
    {
        for (auto& l : lines) l.clear();
        for (auto& a : diff) a.d.clear();
        pre.clear(); er.clear(); lpState.fill (0.0); lfo = 0.0; inLp = 0.0; for (auto& f : lc) f.reset(); for (auto& f : hc) f.reset();
    }
    float meter (int) const noexcept override { return 0.0f; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace prestige;
        static constexpr double sizeOf[6] { 1.3, 0.75, 0.6, 1.9, 0.45, 1.5 }, earlyOf[6] { 1.0, 1.3, 0.2, 1.2, 1.4, 0.6 }, modOf[6] { 1.0, 0.8, 0.6, 1.0, 0.5, 2.2 };
        static constexpr double lenMs[8] { 29.7, 37.1, 41.1, 43.7, 47.3, 53.1, 59.7, 67.3 };
        const auto md = std::clamp ((int) p[mode], 0, 5);
        const auto sz = sizeOf[md] * p[size] / 100.0;
        const auto dec = std::max (0.1, (double) p[decay]);
        const auto xc = std::exp (-kTwoPi * std::min ((double) p[xover], 0.45 * sr) / sr);
        const bool frz = p[freeze] > 0.5f;
        for (int k = 0; k < 8; ++k)
        {
            len[k] = std::clamp (lenMs[k] * sz * 0.001 * sr, 8.0, 0.19 * sr);
            const auto t = len[k] / sr;
            gLo[k] = frz ? 1.0 : std::pow (10.0, -3.0 * t / (dec * p[lowMult]));
            gHi[k] = frz ? 1.0 : std::pow (10.0, -3.0 * t / (dec * p[highMult]));
        }
        const auto difG = 0.45 + 0.3 * p[diffusion] / 100.0;
        static constexpr double difMs[4] { 4.7, 3.6, 12.7, 9.3 };
        for (int k = 0; k < 4; ++k) diff[(size_t) k].len = std::max (1, (int) (difMs[k] * std::min (1.5, sz) * 0.001 * sr));
        const auto preN = std::clamp ((int) (p[predelay] * 0.001 * sr), 1, (int) pre.buf.size() - 4);
        const auto earlyA = p[early] / 100.0 * earlyOf[md] * 0.35;
        const auto modD = p[modulation] / 100.0 * modOf[md] * 0.0009 * sr;
        for (int c = 0; c < 2; ++c) { lc[(size_t) c].set (SvfEq::highpass, p[lowCut], 0.7, 0.0, sr); hc[(size_t) c].set (SvfEq::lowpass, std::min ((double) p[highCut], 0.45 * sr), 0.7, 0.0, sr); }
        const auto wd = p[width] / 100.0, wet = p[mix] / 100.0;
        static constexpr double erMs[10] { 7.1, 11.3, 17.9, 23.3, 31.7, 37.1, 43.9, 53.3, 61.7, 71.1 };
        static constexpr double erG[10] { 0.9, -0.8, 0.7, -0.62, 0.55, -0.5, 0.42, -0.36, 0.3, -0.25 };
        for (int i = 0; i < n; ++i)
        {
            const auto mono = 0.5 * ((double) L[i] + R[i]);
            pre.push (mono);
            auto v = frz ? 0.0 : pre.at (preN);
            er.push (v);
            double erL = 0.0, erR = 0.0;
            for (int k = 0; k < 10; ++k) { const auto t = er.at (std::max (1, (int) (erMs[k] * sz * 0.001 * sr))) * erG[k]; if (k & 1) erR += t; else erL += t; }
            for (auto& a : diff) v = a.process (v, difG);
            lfo += 0.31 / sr; if (lfo >= 1.0) lfo -= 1.0;
            // lecture des 8 lignes (deux modulées en quadrature), déclin séparé grave / aigu
            double y[8];
            for (int k = 0; k < 8; ++k)
            {
                const auto m = (k == 1 || k == 6) ? modD * (1.0 + std::sin (kTwoPi * (lfo + k * 0.13))) : 0.0;
                const auto raw = lines[(size_t) k].atFrac (len[k] + m);
                lpState[(size_t) k] = raw + (lpState[(size_t) k] - raw) * xc;
                y[k] = lpState[(size_t) k] * gLo[k] + (raw - lpState[(size_t) k]) * gHi[k];
            }
            // matrice de Hadamard 8 × 8 (normalisée) : chaque ligne nourrit toutes les autres
            double h[8];
            for (int k = 0; k < 8; ++k) h[k] = y[k];
            for (int s = 1; s < 8; s <<= 1)
                for (int k = 0; k < 8; k += 2 * s)
                    for (int j = k; j < k + s; ++j) { const auto a = h[j], b = h[j + s]; h[j] = a + b; h[j + s] = a - b; }
            for (int k = 0; k < 8; ++k) lines[(size_t) k].push (h[k] * 0.35355339 + (k < 4 ? v : -v) * 0.5);
            auto tl = y[0] + y[2] + y[4] + y[6], tr = y[1] + y[3] + y[5] + y[7];
            tl = tl * 0.5 + erL * earlyA; tr = tr * 0.5 + erR * earlyA;
            const auto mid = 0.5 * (tl + tr), side = 0.5 * (tl - tr) * wd;
            auto ol = hc[0].process (lc[0].process (mid + side)), orr = hc[1].process (lc[1].process (mid - side));
            L[i] = (float) (L[i] * (1.0 - wet) + ol * wet);
            R[i] = (float) (R[i] * (1.0 - wet) + orr * wet);
        }
    }
private:
    std::array<DLine, 8> lines;
    std::array<APLine, 4> diff;
    DLine pre, er;
    std::array<double, 8> lpState {};
    double len[8] {}, gLo[8] {}, gHi[8] {};
    std::array<SvfEq, 2> lc, hc;
    double sr = 48000.0, lfo = 0.0, inLp = 0.0;
};

} // namespace grv::effects
