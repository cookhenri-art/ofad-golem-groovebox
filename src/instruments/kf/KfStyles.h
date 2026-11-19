/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Kick Forge (1.2) — styles et tirages au hasard.
    Un style dit quelles familles de morceaux empiler, à quels volumes, quels décalages (en
    double-croches), quelles longueurs et quels effets du rack allumer, avec des fourchettes. Le
    bouton « Hasard » tire un kick complet dans le style choisi ; en « Construction libre », il garde
    les morceaux choisis et ne fait que varier leurs réglages (volume, hauteur, étirement,
    décalage, fondus). Une couche verrouillée n'est jamais touchée. Les sons d'usine sont des
    tirages fixes de chaque style (graine connue), calculés à l'ouverture.
*/

#pragma once

#include "KfRender.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace grv::instruments::kf
{

namespace styles
{
    struct R2 { float lo = 0.0f, hi = 0.0f; };
    struct LayerRule
    {
        int fam;
        std::vector<int> flavors;
        R2 vol { 0.0f, 0.0f };
        R2 off16 { 0.0f, 0.0f }; // décalage, en double-croches
        R2 len { 0.0f, 0.0f };   // 0 = tout le morceau
        R2 fo { 5.0f, 30.0f };
        R2 lp { 20000.0f, 20000.0f };
        float hp = 20.0f;
        R2 fi { 0.0f, 0.0f };
        R2 drv { 0.0f, 0.0f };
        R2 pitch { 0.0f, 0.0f };
    };
    struct ModRule { int module; std::vector<std::pair<const char*, R2>> v; };
    struct Style { std::vector<LayerRule> layers; std::vector<ModRule> mods; };

    inline const std::vector<Style>& all()
    {
        auto L = [] (int fam, std::vector<int> fl, R2 vol, R2 off = {}, R2 len = {}, R2 fo = { 5, 30 }, R2 lp = { 20000, 20000 }, float hp = 20, R2 fi = {}, R2 drv = {}, R2 pitch = {})
        { LayerRule r; r.fam = fam; r.flavors = std::move (fl); r.vol = vol; r.off16 = off; r.len = len; r.fo = fo; r.lp = lp; r.hp = hp; r.fi = fi; r.drv = drv; r.pitch = pitch; return r; };
        static const std::vector<Style> s {
            // 0 Construction libre : pas de recette (le hasard varie les réglages des morceaux choisis)
            {},
            // 1 Rumble
            { { L (fPunch, { 1, 0 }, { 0, 0 }), L (fClick, { 0, 3 }, { -8, -4 }), L (fRumble, { 0, 3 }, { -14, -9 }, { 0, 0.25f }, {}, { 20, 60 }, { 300, 900 }, 30, { 10, 40 }) },
              { { mDist, { { "type", { 0, 0 } }, { "drive", { 2, 5 } } } }, { mFinal, { { "boost", { 1, 3 } }, { "mono", { 120, 120 } }, { "clip", { 5, 20 } } } }, { mEq, { { "lsg", { 0, 2 } } } } } },
            // 2 Reverse bass
            { { L (fPunch, { 0, 3 }, { 0, 0 }, {}, { 120, 220 }, { 20, 60 }), L (fClick, { 0, 2 }, { -9, -5 }), L (fReverse, { 0, 1, 2 }, { -10, -6 }, { 1, 1 }), L (fBody, { 2 }, { -10, -6 }) },
              { { mFinal, { { "boost", { 2, 4 } }, { "clip", { 20, 40 } } } }, { mMulti, { { "depth", { 20, 40 } } } } } },
            // 3 Industriel
            { { L (fPunch, { 4, 2 }, { 0, 0 }), L (fCrunch, { 2 }, { -5, -1 }), L (fRumble, { 2 }, { -12, -8 }, { 0, 0.25f }, {}, { 20, 60 }), L (fEnhSchranz, { 0 }, { -12, -8 }) },
              { { mDist, { { "type", { 2, 3 } }, { "drive", { 10, 20 } } } }, { mConv, { { "ir", { 4, 7 } }, { "wet", { 15, 30 } } } }, { mFinal, { { "clip", { 30, 60 } } } }, { mFilt, { { "lp", { 6000, 12000 } } } } } },
            // 4 Psy
            { { L (fPunch, { 0, 3 }, { 0, 0 }, {}, { 180, 280 }, { 30, 70 }), L (fClick, { 0, 2, 4 }, { -6, -2 }), L (fBody, { 2, 0 }, { -5, -2 }, {}, { 200, 300 }, { 30, 60 }) },
              { { mEq, { { "b1f", { 250, 400 } }, { "b1g", { -4, -1 } } } }, { mTrans, { { "attack", { 20, 50 } } } }, { mFinal, { { "boost", { 2, 4 } }, { "clip", { 0, 15 } } } } } },
            // 5 Schranz
            { { L (fPunch, { 2, 4 }, { 0, 0 }), L (fCrunch, { 1 }, { -4, 0 }), L (fRumble, { 5 }, { -11, -7 }), L (fEnhSchranz, { 0, 1, 2 }, { -10, -6 }) },
              { { mDist, { { "type", { 2, 2 } }, { "drive", { 14, 24 } } } }, { mMulti, { { "depth", { 40, 70 } } } }, { mFinal, { { "clip", { 40, 70 } } } }, { mFilt, { { "hp", { 30, 40 } } } } } },
            // 6 Rumble propre
            { { L (fPunch, { 0, 1 }, { 0, 0 }), L (fClick, { 0 }, { -10, -6 }), L (fRumble, { 1 }, { -14, -10 }, { 0, 0.25f }, {}, { 20, 60 }, { 400, 1200 }, 30, { 10, 30 }), L (fBody, { 0 }, { -10, -6 }) },
              { { mEq, { { "lsg", { 0, 2 } } } }, { mFinal, { { "boost", { 1, 3 } }, { "clip", { 5, 20 } } } } } },
            // 7 Newstyle
            { { L (fPunch, { 6, 2 }, { 0, 0 }), L (fCrunch, { 0, 3 }, { -4, -1 }), L (fPunchTop, { 1, 2 }, { -8, -4 }), L (fEnhTop, { 0, 2 }, { -12, -8 }), L (fBody, { 3 }, { -6, -3 }) },
              { { mDist, { { "type", { 0, 2 } }, { "drive", { 12, 20 } } } }, { mConv, { { "ir", { 8, 10 } }, { "wet", { 10, 20 } } } }, { mFinal, { { "clip", { 30, 60 } } } }, { mMs, { { "width", { 60, 100 } } } } } },
            // 8 Rawstyle
            { { L (fPunch, { 7, 6 }, { 0, 0 }), L (fCrunch, { 0, 4 }, { -3, 0 }), L (fPunchTop, { 1 }, { -6, -3 }), L (fEnhSchranz, { 1 }, { -12, -8 }) },
              { { mDist, { { "type", { 2, 3 } }, { "drive", { 18, 28 } } } }, { mMulti, { { "depth", { 50, 80 } } } }, { mFinal, { { "clip", { 50, 80 } } } }, { mFilt, { { "lp", { 8000, 14000 } } } } } },
            // 9 Hard techno
            { { L (fPunch, { 1, 2 }, { 0, 0 }), L (fCrunch, { 0, 1 }, { -6, -2 }), L (fRumble, { 0, 5 }, { -12, -8 }, { 0, 0.25f }, {}, { 20, 60 }), L (fClick, { 1, 3 }, { -9, -5 }) },
              { { mDist, { { "type", { 0, 1 } }, { "drive", { 6, 14 } } } }, { mFinal, { { "boost", { 2, 4 } }, { "clip", { 20, 40 } } } }, { mTrans, { { "attack", { 10, 30 } } } } } },
            // 10 Gabber
            { { L (fPunch, { 6, 2 }, { 0, 0 }, {}, { 250, 400 }, { 20, 60 }), L (fCrunch, { 3, 0 }, { -4, -1 }), L (fClick, { 1, 0 }, { -6, -2 }) },
              { { mDist, { { "type", { 2, 3 } }, { "drive", { 24, 34 } } } }, { mFinal, { { "clip", { 60, 90 } } } }, { mFilt, { { "hp", { 35, 50 } } } }, { mEq, { { "b1f", { 900, 1600 } }, { "b1g", { 3, 6 } } } } } },
            // 11 Frenchcore
            { { L (fPunch, { 2, 6 }, { 0, 0 }), L (fCrunch, { 0, 3 }, { -3, 0 }), L (fPunchTop, { 1, 2 }, { -6, -3 }), L (fBody, { 3 }, { -8, -4 }, {}, { 300, 600 }, { 40, 120 }) },
              { { mDist, { { "type", { 2, 2 } }, { "drive", { 20, 30 } } } }, { mMulti, { { "depth", { 40, 70 } } } }, { mFinal, { { "clip", { 50, 80 } } } } } },
            // 12 Hardstyle
            { { L (fPunch, { 6, 0 }, { 0, 0 }), L (fPunchTop, { 0, 1 }, { -6, -3 }), L (fCrunch, { 0 }, { -6, -2 }), L (fEnhReverb, { 0, 1 }, { -14, -9 }), L (fBody, { 3, 1 }, { -6, -3 }) },
              { { mDist, { { "type", { 0, 0 } }, { "drive", { 12, 18 } } } }, { mConv, { { "ir", { 9, 9 } }, { "wet", { 8, 15 } } } }, { mFinal, { { "clip", { 30, 50 } } } } } },
            // 13 Techno
            { { L (fPunch, { 1, 5 }, { 0, 0 }), L (fClick, { 0, 3 }, { -10, -6 }), L (fRumble, { 0, 3 }, { -15, -11 }, { 0, 0.5f }, {}, { 20, 60 }, { 200, 500 }, 30) },
              { { mDist, { { "type", { 0, 0 } }, { "drive", { 2, 6 } } } }, { mEq, { { "lsg", { 0, 2 } } } }, { mFinal, { { "boost", { 1, 3 } }, { "clip", { 0, 15 } } } } } },
        };
        return s;
    }

    struct Rand
    {
        std::uint32_t s;
        explicit Rand (std::uint32_t seed) : s (seed * 2654435761u + 0x9e3779b9u) { if (s == 0) s = 1; next(); next(); }
        std::uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float u() noexcept { return (float) (next() >> 8) / 16777216.0f; }
        float r (R2 x) noexcept { return x.lo + (x.hi - x.lo) * u(); }
        float r (float a, float b) noexcept { return a + (b - a) * u(); }
        int i (int n) noexcept { return n <= 1 ? 0 : (int) (next() % (std::uint32_t) n); }
    };
} // namespace styles

/** Réglages par défaut (valeurs réelles), dans l'ordre des positions. */
inline std::vector<float> defaults();

/** Remet une couche à ses réglages neutres (hors marche, famille, morceau, verrou). */
inline void neutralLayer (std::vector<float>& p, int l)
{
    const auto def = defaults();
    for (int q = lVol; q < numLayerParams; ++q)
        if (q != lLock) p[(size_t) (l * numLayerParams + q)] = def[(size_t) (l * numLayerParams + q)];
}

/** Tirage au hasard.
    mode 0 : tout le kick dans le style choisi (construction libre : varie les réglages des morceaux) ;
    mode 1 : une couche (même famille, autre morceau, réglages variés) ;
    mode 2 : vider (toutes les couches non verrouillées éteintes, rack éteint) ;
    keepFades : les fondus de chaque couche restent tels quels. bpm : pour les décalages au tempo. */
inline void randomize (std::vector<float>& p, int mode, int layer, std::uint32_t seed, double bpm, bool keepFades)
{
    using namespace styles;
    if ((int) p.size() < numSlots()) return;
    Rand R (seed);
    const auto s16 = 15000.0 / std::clamp (bpm, 40.0, 300.0); // une double-croche en ms
    auto at = [&] (int l, int q) -> float& { return p[(size_t) (l * numLayerParams + q)]; };
    auto locked = [&] (int l) { return at (l, lLock) > 0.5f; };
    auto varyLayer = [&] (int l, float amount)
    {
        at (l, lVol) = std::clamp (at (l, lVol) + R.r (-3.0f, 3.0f) * amount, -48.0f, 12.0f);
        at (l, lPitch) = std::clamp (std::round (at (l, lPitch) + R.r (-1.4f, 1.4f) * amount), -24.0f, 24.0f);
        at (l, lFine) = std::clamp (at (l, lFine) + R.r (-20.0f, 20.0f) * amount, -100.0f, 100.0f);
        at (l, lStretch) = std::clamp (at (l, lStretch) * R.r (1.0f - 0.12f * amount, 1.0f + 0.12f * amount), 0.25f, 4.0f);
        if (at (l, lFam) != (float) fPunch) at (l, lOffset) = std::clamp (at (l, lOffset) + R.r (-4.0f, 6.0f) * amount, -20.0f, 1000.0f);
        if (! keepFades)
        {
            at (l, lFoLen) = std::clamp (at (l, lFoLen) * R.r (1.0f - 0.3f * amount, 1.0f + 0.3f * amount) + R.r (0.0f, 4.0f) * amount, 0.0f, 2000.0f);
            at (l, lFoCurve) = std::clamp (at (l, lFoCurve) + R.r (-25.0f, 25.0f) * amount, -100.0f, 100.0f);
        }
    };
    auto clearRack = [&]
    {
        const auto def = defaults();
        for (int s = moduleBase()[0]; s < numSlots(); ++s) p[(size_t) s] = def[(size_t) s];
    };

    if (mode == 2)
    {
        for (int l = 0; l < numLayers; ++l) if (! locked (l)) { at (l, lOn) = 0; at (l, lMute) = 0; at (l, lSolo) = 0; }
        clearRack();
        return;
    }
    if (mode == 1)
    {
        if (layer < 0 || layer >= numLayers || locked (layer)) return;
        const auto fam = std::clamp ((int) std::lround (at (layer, lFam)), 0, numFamilies - 1);
        if (fam != fSample && partCount (fam) > 0) at (layer, lPart) = (float) R.i (partCount (fam));
        varyLayer (layer, 0.6f);
        return;
    }
    const int style = std::clamp ((int) std::lround (p[gStyle]), 0, (int) all().size() - 1);
    if (style == 0)
    {
        // construction libre : les mêmes morceaux, placés un peu autrement
        for (int l = 0; l < numLayers; ++l) if (at (l, lOn) > 0.5f && ! locked (l)) varyLayer (l, 1.0f);
        return;
    }
    const auto& st = all()[(size_t) style];
    int next = 0;
    for (const auto& rule : st.layers)
    {
        while (next < numLayers && locked (next)) ++next;
        if (next >= numLayers) break;
        const int l = next++;
        const auto fadeIn = std::pair { at (l, lFiLen), at (l, lFiCurve) }, fadeOut = std::pair { at (l, lFoLen), at (l, lFoCurve) };
        neutralLayer (p, l);
        at (l, lOn) = 1; at (l, lFam) = (float) rule.fam;
        const auto& fi = families()[(size_t) rule.fam];
        const int flavor = rule.flavors.empty() ? R.i ((int) fi.flavors.size()) : rule.flavors[(size_t) R.i ((int) rule.flavors.size())];
        at (l, lPart) = (float) (flavor * fi.perFlavor + R.i (fi.perFlavor));
        at (l, lVol) = R.r (rule.vol);
        at (l, lOffset) = (float) std::round (R.r (rule.off16) * s16);
        at (l, lLen) = rule.len.hi > 0.0f ? R.r (rule.len) : 4000.0f;
        at (l, lPitch) = std::round (R.r (rule.pitch));
        at (l, lHp) = rule.hp;
        at (l, lLp) = R.r (rule.lp);
        at (l, lDrv) = R.r (rule.drv);
        if (keepFades) { at (l, lFiLen) = fadeIn.first; at (l, lFiCurve) = fadeIn.second; at (l, lFoLen) = fadeOut.first; at (l, lFoCurve) = fadeOut.second; }
        else { at (l, lFiLen) = R.r (rule.fi); at (l, lFoLen) = R.r (rule.fo); at (l, lFoCurve) = R.r (-40.0f, 40.0f); }
    }
    for (int l = next; l < numLayers; ++l) if (! locked (l)) at (l, lOn) = 0;
    clearRack();
    for (const auto& m : st.mods)
    {
        p[(size_t) moduleBase()[(size_t) m.module]] = 1;
        for (const auto& [id, range] : m.v)
        {
            const auto s = modParam (m.module, id);
            if (s >= 0) p[(size_t) s] = id == std::string ("type") || id == std::string ("ir") ? (float) std::round (R.r (range)) : R.r (range);
        }
    }
}

} // namespace grv::instruments::kf
