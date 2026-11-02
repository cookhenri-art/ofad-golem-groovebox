/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Modulateurs internes des effets (v0.36), sur le principe des emplacements de modulation des
    distorsions créatives modernes : chaque emplacement est un LFO (libre ou au tempo), un suiveur
    d'enveloppe (le son qui entre pilote le réglage), une fonction (une forme jouée une fois à chaque
    attaque du son) ou un séquenceur 16 pas (la forme lue en marches), avec 8 formes, une quantité
    bipolaire et une destination parmi les réglages continus de l'effet.

    La modulation se fait dans le domaine normalisé du réglage (0..1, même courbe que le bouton) :
    +100 % parcourt toute la course du bouton. Calcul une fois par bloc de contrôle (32 échantillons
    dans le moteur), aucune allocation.
*/

#pragma once

#include "ModFx.h"   // syncDivisions(), syncQuarters()

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;

namespace fxmod
{
    constexpr int slotParams = 6;
    enum SlotParam { mType = 0, mRate, mDivision, mShape, mAmount, mDest };
    enum Type { off = 0, lfo, follower, function, sequencer };

    inline const std::vector<std::string>& types()
    {
        static const std::vector<std::string> t { "Arr\xC3\xAAt", "LFO", "Suiveur", "Fonction", "S\xC3\xA9quenceur" };
        return t;
    }
    inline const std::vector<std::string>& shapes()
    {
        static const std::vector<std::string> s { "Sinus", "Triangle", "Scie montante", "Scie descendante", "Carr\xC3\xA9",
                                                  "Hasard", "Hasard liss\xC3\xA9", "Pompe" };
        return s;
    }
    inline const std::vector<std::string>& divisions()
    {
        static const std::vector<std::string> d = []
        {
            std::vector<std::string> v { "Libre" };
            for (const auto& s : syncDivisions()) v.push_back (s);
            return v;
        }();
        return d;
    }

    /** Ajoute `count` emplacements (« m1type », « m1rate »… ) à la description, et leur section par
        emplacement (onglet « mod »). destNames : libellés des destinations, dans l'ordre de `dests`. */
    inline void addSlots (ModuleDesc& m, int count, const std::vector<std::string>& destNames, int defaultDest = 0, int destStep = 1)
    {
        for (int s = 0; s < count; ++s)
        {
            const auto pre = "m" + std::to_string (s + 1);
            const auto num = std::to_string (s + 1);
            m.params.push_back (C (pre + "type", "Modulation " + num, types()));
            m.params.push_back (P (pre + "rate", "Vitesse", "Hz", 0.02f, 20, 1, 0.35f));
            m.params.push_back (C (pre + "div", "Division", divisions(), 0));
            m.params.push_back (C (pre + "shape", "Forme", shapes()));
            m.params.push_back (P (pre + "amt", "Quantit\xC3\xA9", "%", -100, 100, 30));
            m.params.push_back (C (pre + "dest", "Destination", destNames, std::min (defaultDest + s * destStep, (int) destNames.size() - 1)));
            m.sections.push_back ({ "mod" + num, "Modulation " + num, "lfo",
                                    { pre + "type", pre + "dest", pre + "shape", pre + "rate", pre + "div", pre + "amt" }, "mod" });
        }
    }
}

/** Un emplacement de modulation. value() : -1..1 (LFO, séquenceur) ou 0..1 (suiveur, fonction). */
class FxModulator
{
public:
    void reset() noexcept { phase = 0.0; val = 0.0; env = envSlow = 0.0; fnPos = -1.0; held = heldPrev = 0.0; heldNext = rnd(); lastStep = -1; armed = true; }

    /** Avance de n échantillons ; p : les 6 réglages de l'emplacement ; peak : crête du bloc d'entrée. */
    double tick (const float* p, int n, double sr, double bpm, double ppq, bool playing, double peak) noexcept
    {
        using namespace fxmod;
        const auto type = (int) p[mType];
        if (type == off) { val = 0.0; return 0.0; }
        const auto div = (int) p[mDivision];
        const auto synced = div > 0;
        const auto quarters = synced ? syncQuarters (div - 1) : 0.0;
        const auto hz = synced ? (bpm / 60.0) / quarters : (double) p[mRate];
        const auto shp = (int) p[mShape];
        const auto dt = (double) n / sr;

        // suiveur : attaque 3 ms, relâchement selon la vitesse (lent à gauche, rapide à droite)
        const auto aC = std::exp (-dt / 0.003), rC = std::exp (-dt * std::max (0.5, hz * 2.0));
        env = peak > env ? peak + (env - peak) * aC : peak + (env - peak) * rC;
        envSlow = peak + (envSlow - peak) * std::exp (-dt / 0.08);

        if (type == follower)
        {
            const auto db = 20.0 * std::log10 (env + 1.0e-9);
            val = std::clamp ((db + 54.0) / 54.0, 0.0, 1.0);
            return val;
        }
        if (type == function)
        {
            // une attaque (le niveau rapide dépasse le lent de 6 dB) relance la forme
            const bool onset = env > 2.0 * envSlow + 1.0e-4;
            if (onset && armed) { fnPos = 0.0; armed = false; held = rnd(); heldPrev = held; }
            if (! onset && env < 1.2 * envSlow) armed = true;
            if (fnPos >= 0.0)
            {
                fnPos += dt * hz;
                if (fnPos >= 1.0) fnPos = 1.0;
                val = shapeAt (shp, fnPos, true);
                if (fnPos >= 1.0) fnPos = -1.0;
            }
            return val;
        }
        // LFO et séquenceur : phase libre ou calée sur la position du morceau
        if (synced && playing) phase = std::fmod (ppq / quarters, 1.0);
        else { phase += dt * hz; phase -= std::floor (phase); }
        if (type == sequencer)
        {
            const auto step = (int) std::floor (phase * 16.0);
            if (step != lastStep) { lastStep = step; heldPrev = held; held = rnd() * 2.0 - 1.0; }
            val = (shp == 5 || shp == 6) ? held : shapeAt (shp, (step + 0.5) / 16.0, false);
            return val;
        }
        // LFO : hasard = nouvelle valeur à chaque cycle (lissée ou non)
        if (shp == 5 || shp == 6)
        {
            if (phase < lastPhase) { heldPrev = held; held = heldNext; heldNext = rnd() * 2.0 - 1.0; }
            lastPhase = phase;
            val = shp == 5 ? held : heldPrev + (held - heldPrev) * (0.5 - 0.5 * std::cos (kPi * phase));
            return val;
        }
        lastPhase = phase;
        val = shapeAt (shp, phase, false);
        return val;
    }

    double value() const noexcept { return val; }

private:
    /** Forme à la position t (0..1) ; unipolaire (fonction) ou bipolaire. */
    double shapeAt (int shp, double t, bool uni) const noexcept
    {
        double u; // 0..1
        switch (shp)
        {
            case 0: u = uni ? std::sin (kPi * t) : 0.5 - 0.5 * std::cos (kTwoPi * t); break;   // sinus (bosse pour la fonction)
            case 1: u = 1.0 - std::abs (2.0 * t - 1.0); break;                                    // triangle
            case 2: u = t; break;                                                                  // scie montante
            case 3: u = 1.0 - t; break;                                                            // scie descendante
            case 4: u = t < 0.5 ? 1.0 : 0.0; break;                                                // carré
            case 7: u = 1.0 - std::pow (1.0 - t, 3.0); break;                                      // pompe : creux puis remontée
            default: u = held * 0.5 + 0.5; break;
        }
        if (shp == 7 && ! uni) u = 1.0 - std::pow (1.0 - t, 3.0);
        return uni ? u : u * 2.0 - 1.0;
    }
    double rnd() noexcept { seed = seed * 1664525u + 1013904223u; return (double) (seed >> 8) / 16777216.0; }

    double phase = 0.0, lastPhase = 0.0, val = 0.0, env = 0.0, envSlow = 0.0, fnPos = -1.0, held = 0.0, heldPrev = 0.0, heldNext = 0.3;
    int lastStep = -1;
    bool armed = true;
    std::uint32_t seed = 0x2545F491u;
};

/** Banque de N emplacements : applique les modulations à une copie des réglages (domaine normalisé). */
template <int N>
class FxModBank
{
public:
    void reset() noexcept { for (auto& m : mods) m.reset(); }

    /** base : réglages du bloc ; out : copie modulée ; slot0 : index du premier réglage « m1type » ;
        dests : index des réglages atteints par chaque choix de destination. */
    void apply (const ModuleDesc& d, const float* base, float* out, int numParams, int slot0, const int* dests, int numDests,
                int n, double sr, double bpm, double ppq, bool playing, double peak) noexcept
    {
        std::copy (base, base + numParams, out);
        std::array<double, 64> add {};
        bool any = false;
        for (int s = 0; s < N; ++s)
        {
            const auto* sp = base + slot0 + s * fxmod::slotParams;
            const auto v = mods[(size_t) s].tick (sp, n, sr, bpm, ppq, playing, peak);
            if ((int) sp[fxmod::mType] == fxmod::off) continue;
            const auto di = std::clamp ((int) sp[fxmod::mDest], 0, numDests - 1);
            const auto target = dests[di];
            if (target < 0 || target >= numParams || target >= 64) continue;
            add[(size_t) target] += v * sp[fxmod::mAmount] / 100.0;
            any = true;
        }
        if (! any) return;
        for (int i = 0; i < numParams && i < 64; ++i)
            if (add[(size_t) i] != 0.0)
            {
                const auto& pd = d.params[(size_t) i];
                out[i] = pd.toPlain ((float) std::clamp (pd.toNorm (base[i]) + add[(size_t) i], 0.0, 1.0));
            }
    }
    double value (int s) const noexcept { return mods[(size_t) s].value(); }

private:
    std::array<FxModulator, N> mods;
};

} // namespace grv::effects
