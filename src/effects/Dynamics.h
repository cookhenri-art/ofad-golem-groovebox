/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Compresseurs et limiteur, chacun sur son principe (architectures propres) :
      - « Compresseur optique » : cellule photo-résistive à attaque douce et relâchement en deux
        temps qui s'allonge avec la durée de compression (voix, basse, bus doux) ;
      - « Compresseur FET » : attaque très rapide, rapports 4 à 20 et « tous les boutons »,
        coloration harmonique (batterie, voix rock, parallèle) ;
      - « Compresseur de bus » : rapports 2/4/10, attaques et relâchements à crans, relâchement
        automatique, filtre de détection (cohésion du bus batterie et du mix) ;
      - « Compresseur moderne » : seuil, rapport, genou, attaque, relâchement, plage, détection
        crête ou efficace, compensation automatique, filtre de détection ;
      - « Limiteur » : sans anticipation (latence nulle), relâchement réglable, écrêtage doux
        final, plafond garanti.
    Toutes les détections sont stéréo liées. Mesure 0 : réduction de gain (dB).
    v0.23 : side-chain sur les quatre compresseurs — la détection écoute la piste choisie (sortie
    après ses inserts, avant son volume : une piste coupée « fantôme » pilote toujours) au lieu de
    l'entrée, par exemple la grosse caisse qui fait respirer la basse.
*/

#pragma once

#include "../dsp/EqFilters.h"
#include "../dsp/Oversampler.h"
#include "../engine/Module.h"
#include "FxBanks.h"

#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

/** Gain de compression statique à genou doux (dB). */
inline double compressDb (double levelDb, double thresholdDb, double ratio, double kneeDb) noexcept
{
    const auto over = levelDb - thresholdDb;
    if (kneeDb > 0.0 && std::abs (over) <= kneeDb * 0.5)
        return (1.0 / ratio - 1.0) * (over + kneeDb * 0.5) * (over + kneeDb * 0.5) / (2.0 * kneeDb);
    return over > 0.0 ? (1.0 / ratio - 1.0) * over : 0.0;
}

inline double msCoef (double ms, double sr) noexcept { return std::exp (-1.0 / std::max (1.0, ms * 0.001 * sr)); }

class CompressorBase : public Effect
{
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override { env = 0.0; grDb = 0.0; charge = 0.0; hpL.reset(); hpR.reset(); }
    float meter (int i) const noexcept override { return i == 0 ? (float) grDb : (i == 1 ? inMeter : outMeter); }
    void setSidechainSources (const float* const* l, const float* const* r) noexcept override { scL = l; scR = r; }
protected:
    const float* const* scL = nullptr;
    const float* const* scR = nullptr;
    /** Signal écouté par la détection : la piste choisie (1..16) ou l'entrée (0 = interne). */
    void detectInput (int src, const float* L, const float* R, const float*& dL, const float*& dR) const noexcept
    {
        dL = L; dR = R;
        if (src >= 1 && src <= numTracks && scL != nullptr && scR != nullptr && scL[src - 1] != nullptr && scR[src - 1] != nullptr)
        { dL = scL[src - 1]; dR = scR[src - 1]; }
    }
    double sr = 48000.0, env = 0.0, grDb = 0.0, charge = 0.0;
    SvfEq hpL, hpR;
    float inMeter = 0.0f, outMeter = 0.0f;

    void meters (const float* L, const float* R, int n, bool input) noexcept
    {
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
        auto& m = input ? inMeter : outMeter;
        m = std::max (pk, m * 0.9f);
    }
};

//==============================================================================
namespace opto
{
    enum Slot { peak = 0, gain, mode, emphasis, mix, scSrc, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "opto"; m.name = "Lumen"; m.role = "Compresseur optique"; m.family = "Compresseurs et dynamique"; m.form = "rack";
            m.principle = "Cellule photo-r\xC3\xA9sistive : attaque douce, rel\xC3\xA2" "chement en deux temps d\xC3\xA9pendant du programme";
            m.params = {
                P ("peak", "R\xC3\xA9" "duction", "%", 0, 100, 40),
                P ("gain", "Gain", "dB", 0, 30, 6),
                C ("mode", "Mode", { "Compression", "Limitation" }),
                P ("emphasis", "Aigus d\xC3\xA9tect\xC3\xA9s", "%", 0, 100, 0),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
                C ("scsrc", "Side-chain", sidechainChoices()),
            };
            m.sections = { { "comp", "Compresseur optique", "comp", { "peak", "gain", "mode", "emphasis", "mix", "scsrc" }, "main" } };
            m.presets = {
                { "Voix", { { "peak", 45 }, { "gain", 8 } } },
                { "Basse", { { "peak", 55 }, { "gain", 11 } } },
                { "Guitare", { { "peak", 35 }, { "gain", 5 } } },
                { "Nappe", { { "peak", 25 }, { "gain", 3 } } },
                { "Bus doux", { { "peak", 20 }, { "gain", 2 }, { "mix", 70 } } },
                { "Limitation voix", { { "peak", 60 }, { "gain", 17 }, { "mode", 1 }, { "emphasis", 40 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class OptoCompressor final : public CompressorBase
{
public:
    const ModuleDesc& desc() const noexcept override { return opto::desc(); }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace opto;
        meters (L, R, n, true);
        const auto thr = -p[peak] * 0.42;                 // 0 à -42 dB
        const auto ratio = p[mode] > 0.5f ? 12.0 : 3.0;
        const auto makeup = dbToGain (p[gain]);
        const auto wet = p[mix] / 100.0;
        const auto emph = p[emphasis] / 100.0;
        hpL.set (SvfEq::highShelf, 2500.0, 0.7, emph * 12.0, sr);
        const auto att = msCoef (10.0, sr);
        const auto detC = msCoef (5.0, sr);
        const auto chargeUp = 1.0 - msCoef (1500.0, sr), chargeDown = msCoef (4000.0, sr);
        const float* dL; const float* dR;
        detectInput ((int) p[scSrc], L, R, dL, dR);
        for (int i = 0; i < n; ++i)
        {
            const double l = L[i], r = R[i];
            const auto mono = 0.5 * ((double) dL[i] + (double) dR[i]);
            const auto det = emph > 0.0 ? hpL.process (mono) : mono;
            env = std::max (std::abs (det), env * detC);
            const auto target = -compressDb (gainToDb (env), thr, ratio, 8.0); // réduction positive
            if (target > grDb) { grDb = target + (grDb - target) * att; charge += (1.0 - charge) * chargeUp * std::min (1.0, target / 6.0); }
            else
            {
                // relâchement : rapide (60 ms) jusqu'à la moitié, puis lent (0,5 à 5 s selon la charge de la cellule)
                const auto relMs = grDb > 2.0 * target + 1.0 ? 60.0 : 500.0 + 4500.0 * charge;
                grDb = target + (grDb - target) * msCoef (relMs, sr);
                charge *= chargeDown;
            }
            const auto g = dbToGain (-grDb) * makeup;
            L[i] = (float) (l * (1.0 - wet) + l * g * wet);
            R[i] = (float) (r * (1.0 - wet) + r * g * wet);
        }
        meters (L, R, n, false);
    }
};

//==============================================================================
namespace fet
{
    enum Slot { input = 0, output, attack, release, ratio, mix, scSrc, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "fet"; m.name = "Thunder"; m.role = "Compresseur FET"; m.family = "Compresseurs et dynamique"; m.form = "rack";
            m.principle = "\xC3\x89tage \xC3\xA0 transistor \xC3\xA0 effet de champ : attaque tr\xC3\xA8s rapide, coloration, mode tous rapports";
            m.params = {
                P ("input", "Entr\xC3\xA9" "e", "dB", 0, 48, 18),
                P ("output", "Sortie", "dB", -24, 24, 0),
                P ("attack", "Attaque", "\xC2\xB5s", 20, 800, 200, 0.5f),
                P ("release", "Rel\xC3\xA2" "chement", "ms", 50, 1100, 250, 0.5f),
                C ("ratio", "Rapport", { "4:1", "8:1", "12:1", "20:1", "Tous" }),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
                C ("scsrc", "Side-chain", sidechainChoices()),
            };
            m.sections = { { "comp", "Compresseur FET", "comp", { "input", "output", "attack", "release", "ratio", "mix", "scsrc" }, "main" } };
            m.presets = {
                { "Batterie", { { "input", 24 }, { "attack", 400 }, { "release", 150 }, { "ratio", 1 }, { "output", 6 } } },
                { "Voix rock", { { "input", 20 }, { "attack", 250 }, { "release", 300 }, { "ratio", 0 }, { "output", 6 } } },
                { "Basse", { { "input", 18 }, { "attack", 600 }, { "release", 200 }, { "ratio", 0 }, { "output", 5 } } },
                { "\xC3\x89" "crasement parall\xC3\xA8le", { { "input", 36 }, { "ratio", 4 }, { "attack", 20 }, { "release", 80 }, { "mix", 35 }, { "output", 0 } } },
                { "Caisse claire", { { "input", 22 }, { "attack", 800 }, { "release", 100 }, { "ratio", 2 }, { "output", 6 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class FetCompressor final : public CompressorBase
{
public:
    const ModuleDesc& desc() const noexcept override { return fet::desc(); }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace fet;
        meters (L, R, n, true);
        static constexpr double ratios[5] { 4.0, 8.0, 12.0, 20.0, 30.0 };
        const auto ri = std::clamp ((int) p[ratio], 0, 4);
        const bool allButtons = ri == 4;
        const auto inG = dbToGain (p[input]);
        constexpr double thr = -18.0;
        // L'entrée pousse dans un seuil fixe : on compense pour qu'un signal à -12 dBFS
        // ressorte au même niveau, le bouton Sortie reste un réglage relatif.
        constexpr double nominal = -12.0;
        const auto driven = nominal + p[input];
        auto nomGr = -compressDb (driven, thr, ratios[ri], allButtons ? 0.0 : 3.0);
        if (allButtons) nomGr *= 1.25;
        const auto outG = dbToGain (p[output] + nominal - (driven - nomGr));
        const auto att = std::exp (-1.0 / std::max (1.0, p[attack] * 1.0e-6 * sr));
        const auto rel = msCoef (p[release], sr);
        const auto wet = p[mix] / 100.0;
        const float* dL; const float* dR;
        detectInput ((int) p[scSrc], L, R, dL, dR);
        for (int i = 0; i < n; ++i)
        {
            const double l = L[i] * inG, r = R[i] * inG;
            const auto lvl = std::max (std::abs ((double) dL[i]), std::abs ((double) dR[i])) * inG;
            env = lvl > env ? lvl + (env - lvl) * att : lvl + (env - lvl) * rel;
            auto target = -compressDb (gainToDb (env), thr, ratios[ri], allButtons ? 0.0 : 3.0);
            if (allButtons) target *= 1.25; // sur-compression caractéristique du mode « tous »
            grDb = target;
            const auto g = dbToGain (-grDb);
            // coloration : légère saturation qui croît avec la réduction de gain
            const auto colour = 0.05 + 0.02 * std::min (20.0, grDb) + (allButtons ? 0.15 : 0.0);
            // v0.22 : saturation anti-repliée (ADAA)
            const auto yl = satL.process (l * g * (1.0 + colour)) / (1.0 + colour);
            const auto yr = satR.process (r * g * (1.0 + colour)) / (1.0 + colour);
            L[i] = (float) (L[i] * (1.0 - wet) + yl * outG * wet);
            R[i] = (float) (R[i] * (1.0 - wet) + yr * outG * wet);
        }
        meters (L, R, n, false);
    }
    void reset() noexcept override { CompressorBase::reset(); satL.reset(); satR.reset(); }
private:
    dsp::AdaaTanh satL, satR;
};

//==============================================================================
namespace buscomp
{
    enum Slot { threshold = 0, ratio, attack, release, makeup, mix, scHpf, scSrc, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "bus"; m.name = "Cement"; m.role = "Compresseur de bus"; m.family = "Compresseurs et dynamique"; m.form = "rack";
            m.principle = "Compresseur de bus \xC3\xA0 crans : rapports 2, 4, 10, rel\xC3\xA2" "chement automatique, filtre de d\xC3\xA9tection";
            m.params = {
                P ("threshold", "Seuil", "dB", -40, 0, -12),
                C ("ratio", "Rapport", { "2:1", "4:1", "10:1" }, 1),
                C ("attack", "Attaque", { "0,1 ms", "0,3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 3),
                C ("release", "Rel\xC3\xA2" "chement", { "0,1 s", "0,3 s", "0,6 s", "1,2 s", "Auto" }, 4),
                P ("makeup", "Gain", "dB", 0, 20, 2),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
                P ("schpf", "Filtre d\xC3\xA9tection", "Hz", 20, 500, 20, 0.5f),
                C ("scsrc", "Side-chain", sidechainChoices()),
            };
            m.sections = { { "comp", "Compresseur de bus", "comp", { "threshold", "ratio", "attack", "release", "makeup", "mix", "schpf", "scsrc" }, "main" } };
            m.presets = {
                { "Bus batterie", { { "threshold", -16 }, { "ratio", 1 }, { "attack", 4 }, { "release", 4 }, { "makeup", 4 }, { "schpf", 60 } } },
                { "Colle du mix", { { "threshold", -10 }, { "ratio", 0 }, { "attack", 4 }, { "release", 4 }, { "makeup", 1.5f }, { "schpf", 90 } } },
                { "Pompe", { { "threshold", -24 }, { "ratio", 2 }, { "attack", 2 }, { "release", 1 }, { "makeup", 16 } } },
                { "Master doux", { { "threshold", -6 }, { "ratio", 0 }, { "attack", 5 }, { "release", 4 }, { "makeup", 1 }, { "schpf", 120 } } },
                { "Parall\xC3\xA8le", { { "threshold", -30 }, { "ratio", 2 }, { "attack", 1 }, { "release", 1 }, { "makeup", 14 }, { "mix", 40 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class BusCompressor final : public CompressorBase
{
public:
    const ModuleDesc& desc() const noexcept override { return buscomp::desc(); }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace buscomp;
        meters (L, R, n, true);
        static constexpr double ratios[3] { 2.0, 4.0, 10.0 };
        static constexpr double atts[6] { 0.1, 0.3, 1.0, 3.0, 10.0, 30.0 };
        static constexpr double rels[4] { 100.0, 300.0, 600.0, 1200.0 };
        const auto rIdx = (int) p[release];
        const auto att = msCoef (atts[std::clamp ((int) p[attack], 0, 5)], sr);
        const auto mk = dbToGain (p[makeup]);
        const auto wet = p[mix] / 100.0;
        hpL.set (SvfEq::highpass, p[scHpf], 0.7, 0.0, sr);
        hpR.set (SvfEq::highpass, p[scHpf], 0.7, 0.0, sr);
        const auto filt = p[scHpf] > 25.0f;
        const float* sL; const float* sR;
        detectInput ((int) p[scSrc], L, R, sL, sR);
        for (int i = 0; i < n; ++i)
        {
            const double l = L[i], r = R[i];
            const double xl = sL[i], xr = sR[i];
            const auto dl = filt ? hpL.process (xl) : xl, dr = filt ? hpR.process (xr) : xr;
            const auto lvl = std::max (std::abs (dl), std::abs (dr));
            const auto target = -compressDb (gainToDb (lvl), p[threshold], ratios[std::clamp ((int) p[ratio], 0, 2)], 2.0);
            if (target > grDb) grDb = target + (grDb - target) * att;
            else
            {
                // automatique : rapide sur les transitoires, lent sur la compression soutenue
                const auto relMs = rIdx >= 4 ? (grDb - target > 3.0 ? 120.0 : 900.0) : rels[std::clamp (rIdx, 0, 3)];
                grDb = target + (grDb - target) * msCoef (relMs, sr);
            }
            const auto g = dbToGain (-grDb) * mk;
            L[i] = (float) (l * (1.0 - wet) + l * g * wet);
            R[i] = (float) (r * (1.0 - wet) + r * g * wet);
        }
        meters (L, R, n, false);
    }
};

//==============================================================================
namespace modern
{
    enum Slot { threshold = 0, ratio, knee, attack, release, range, makeup, autoGain, detection, scHpf, mix, scSrc,
                style, character, colour, autoThr, numSlots }; // v0.36 : à partir de style (anciens projets : Propre, sans caractère)
    enum Style { clean = 0, classic, optoS, vocal, mastering, busS, punch, pumping, variMu, upward, numStyles };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "comp"; m.name = "Scalpel"; m.role = "Compresseur moderne"; m.family = "Compresseurs et dynamique"; m.form = "rack";
            m.principle = "Compresseur polyvalent \xC3\xA0 10 styles (propre, classique, opto, voix, mastering, bus, percussif, pompage, vari-mu, montant), "
                          "caract\xC3\xA8re (lampe, diode, brillant), seuil automatique ; sans anticipation (aucune latence)";
            m.params = {
                P ("threshold", "Seuil", "dB", -60, 0, -18),
                P ("ratio", "Rapport", ":1", 1, 20, 3, 0.5f),
                P ("knee", "Genou", "dB", 0, 24, 6),
                P ("attack", "Attaque", "ms", 0.05f, 250, 10, 0.4f),
                P ("release", "Rel\xC3\xA2" "chement", "ms", 5, 2500, 120, 0.4f),
                P ("range", "Plage", "dB", 0, 48, 24),
                P ("makeup", "Gain", "dB", -12, 24, 0),
                C ("autogain", "Compensation", { "Non", "Oui" }, 1),
                C ("detection", "D\xC3\xA9tection", { "Cr\xC3\xAA" "te", "Efficace" }),
                P ("schpf", "Filtre d\xC3\xA9tection", "Hz", 20, 500, 20, 0.5f),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
                C ("scsrc", "Side-chain", sidechainChoices()),
                C ("style", "Style", { "Propre", "Classique", "Opto", "Voix", "Mastering", "Bus", "Percussif", "Pompage", "Vari-mu", "Montant" }),
                C ("character", "Caract\xC3\xA8re", { "Aucun", "Lampe", "Diode", "Brillant" }),
                P ("colour", "Couleur", "%", 0, 100, 40),
                C ("autothr", "Seuil automatique", { "Non", "Oui" }),
            };
            m.sections = {
                { "comp", "Compression", "comp", { "style", "threshold", "ratio", "knee", "attack", "release", "range", "makeup" }, "main" },
                { "colour", "Caract\xC3\xA8re", "comp", { "character", "colour", "autothr", "mix" }, "main" },
                { "options", "Options", "comp", { "autogain", "detection", "schpf", "scsrc" }, "main" },
            };
            m.presets = {
                { "Voix", { { "threshold", -20 }, { "ratio", 3 }, { "attack", 5 }, { "release", 80 }, { "schpf", 80 } } },
                { "Voix rap", { { "threshold", -24 }, { "ratio", 5 }, { "attack", 2 }, { "release", 60 }, { "knee", 3 } } },
                { "Batterie", { { "threshold", -16 }, { "ratio", 4 }, { "attack", 15 }, { "release", 90 } } },
                { "Basse", { { "threshold", -18 }, { "ratio", 4 }, { "attack", 20 }, { "release", 150 }, { "detection", 1 } } },
                { "Guitare", { { "threshold", -16 }, { "ratio", 3 }, { "attack", 12 }, { "release", 120 } } },
                { "Piano", { { "threshold", -14 }, { "ratio", 2 }, { "attack", 25 }, { "release", 200 }, { "detection", 1 } } },
                { "Master", { { "threshold", -8 }, { "ratio", 1.6f }, { "knee", 12 }, { "attack", 30 }, { "release", 300 }, { "detection", 1 }, { "schpf", 100 } } },
                { "Voix opto", { { "style", 2 }, { "threshold", -24 }, { "ratio", 4 }, { "character", 1 }, { "colour", 35 } } },
                { "Bus qui colle", { { "style", 5 }, { "threshold", -14 }, { "ratio", 2 }, { "attack", 30 }, { "release", 200 }, { "schpf", 90 } } },
                { "Batterie percutante", { { "style", 6 }, { "threshold", -20 }, { "ratio", 6 }, { "attack", 1 }, { "release", 70 } } },
                { "Pompage (side-chain)", { { "style", 7 }, { "threshold", -26 }, { "ratio", 10 }, { "attack", 2 }, { "release", 180 } } },
                { "Vari-mu chaud", { { "style", 8 }, { "threshold", -20 }, { "ratio", 6 }, { "character", 1 }, { "colour", 55 } } },
                { "Remonter les d\xC3\xA9tails", { { "style", 9 }, { "threshold", -30 }, { "ratio", 2 }, { "range", 10 }, { "autogain", 0 } } },
                { "Mastering doux", { { "style", 4 }, { "threshold", -12 }, { "ratio", 1.5f }, { "knee", 18 }, { "attack", 40 }, { "release", 400 } } },
                { "Seuil automatique", { { "autothr", 1 }, { "threshold", -24 }, { "ratio", 3 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class ModernCompressor final : public CompressorBase
{
public:
    const ModuleDesc& desc() const noexcept override { return modern::desc(); }
    void reset() noexcept override { CompressorBase::reset(); lvlAvg = -18.0; shL.reset(); shR.reset(); dcL = dcR = 0.0; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace modern;
        meters (L, R, n, true);
        const auto sty = std::clamp ((int) p[style], 0, numStyles - 1);
        // styles (v0.36) : temps, genou et détection selon le principe de chaque style ; « Propre » = v0.35
        double attMs = p[attack], relMs = p[release], kneeDb = p[knee];
        bool rms = p[detection] > 0.5f, feedback = false;
        double rmsMs = 10.0;
        switch (sty)
        {
            case classic:   feedback = true; kneeDb = std::max (kneeDb, 4.0); break;
            case optoS:     attMs = std::max (attMs, 10.0); kneeDb = std::max (kneeDb, 10.0); rms = true; break;
            case vocal:     kneeDb = std::max (kneeDb, 12.0); attMs = std::max (attMs, 2.0); rms = true; rmsMs = 5.0; break;
            case mastering: kneeDb = std::max (kneeDb, 12.0); rms = true; rmsMs = 30.0; break;
            case busS:      feedback = true; break;
            case punch:     attMs = std::max (attMs, 12.0); relMs = std::min (relMs, 90.0); break;     // laisse passer l'attaque
            case variMu:    kneeDb = std::max (kneeDb, 8.0); rms = true; break;
            default: break;
        }
        const auto att = msCoef (attMs, sr), rel = msCoef (relMs, sr), relSlow = msCoef (relMs * 4.0, sr);
        const auto rmsC = msCoef (rmsMs, sr);
        const auto avgC = msCoef (1500.0, sr);
        const auto r = std::max (1.0, (double) p[ratio]);
        const bool autoT = p[autoThr] > 0.5f;
        const auto thr = autoT ? (double) p[threshold] + (lvlAvg + 18.0) : (double) p[threshold];
        const auto autoMk = p[autoGain] > 0.5f && sty != upward ? -compressDb (0.0, p[threshold], r, kneeDb) * 0.5 : 0.0;
        const auto mk = dbToGain (p[makeup] + autoMk);
        const auto wet = p[mix] / 100.0;
        hpL.set (SvfEq::highpass, p[scHpf], 0.7, 0.0, sr);
        hpR.set (SvfEq::highpass, p[scHpf], 0.7, 0.0, sr);
        const auto filt = p[scHpf] > 25.0f;
        const auto chr = std::clamp ((int) p[character], 0, 3);
        const auto col = p[colour] / 100.0;
        if (chr == 3) { shL.set (SvfEq::highShelf, 7000.0, 0.7, 4.0 * col, sr); shR.set (SvfEq::highShelf, 7000.0, 0.7, 4.0 * col, sr); }
        const auto dcC = std::exp (-kTwoPi * 8.0 / sr);
        const float* sL; const float* sR;
        detectInput ((int) p[scSrc], L, R, sL, sR);
        for (int i = 0; i < n; ++i)
        {
            const double l = L[i], rr = R[i];
            const double xl = sL[i], xr = sR[i];
            const auto dl = filt ? hpL.process (xl) : xl, dr = filt ? hpR.process (xr) : xr;
            double lvlDb;
            if (rms) { const auto sq = 0.5 * (dl * dl + dr * dr); env = sq + (env - sq) * rmsC; lvlDb = 10.0 * std::log10 (std::max (1.0e-12, env)); }
            else lvlDb = gainToDb (std::max (std::abs (dl), std::abs (dr)));
            if (autoT) lvlAvg = lvlDb > -70.0 ? lvlDb + (lvlAvg - lvlDb) * avgC : lvlAvg;
            // contre-réaction : la détection écoute la sortie (niveau d'entrée moins la réduction en cours)
            const auto det = feedback ? lvlDb - grDb : lvlDb;
            double target;
            if (sty == upward)
            {
                // montant : remonte ce qui est sous le seuil (jusqu'à la plage), ne touche pas au-dessus
                const auto under = thr - det;
                target = under > 0.0 && det > -70.0 ? -std::min ((double) p[range], under * (1.0 - 1.0 / r)) : 0.0;
            }
            else
            {
                auto rr2 = r;
                if (sty == variMu) rr2 = 1.0 + (r - 1.0) * std::clamp ((det - thr) / 12.0, 0.0, 1.0); // rapport qui grandit avec le dépassement
                target = std::min ((double) p[range], -compressDb (det, thr, rr2, kneeDb));
            }
            if (target > grDb) grDb = target + (grDb - target) * att;
            else if (sty == optoS || sty == busS)
            {
                // relâchement selon le programme : rapide d'abord, lent ensuite (charge de la cellule)
                charge = std::max (0.0, charge + (grDb > 1.0 ? 1.0 / sr : -0.5 / sr));
                const auto k = std::min (1.0, charge * 2.0);
                grDb = target + (grDb - target) * (rel * (1.0 - k) + relSlow * k);
            }
            else grDb = target + (grDb - target) * rel;
            auto g = dbToGain (-grDb);
            if (sty == pumping) g = std::pow (g, 1.6); // creux plus profonds, remontée qui traîne : pompage marqué
            g *= mk;
            double ol = l * g, orr = rr * g;
            if (chr == 1 || chr == 2)
            {
                // lampe : saturation douce asymétrique (harmoniques paires) ; diode : plus dure ; dosée par la réduction
                const auto drive = 1.0 + col * (2.0 + 0.15 * std::max (0.0, grDb));
                const auto bias = chr == 1 ? 0.12 * col : 0.0;
                auto sat = [&] (double x) { return chr == 1 ? (std::tanh ((x + bias) * drive) - std::tanh (bias * drive)) / drive
                                                            : std::clamp (x * drive, -1.0, 1.0) * (1.0 - 0.2 * col) / drive + std::tanh (x * drive) * 0.2 * col / drive; };
                ol = sat (ol) * (1.0 + 0.3 * col); orr = sat (orr) * (1.0 + 0.3 * col);
                dcL = ol + (dcL - ol) * dcC; dcR = orr + (dcR - orr) * dcC;   // retire le continu dû à l'asymétrie
                ol -= dcL; orr -= dcR;
            }
            else if (chr == 3) { ol = shL.process (ol); orr = shR.process (orr); }
            L[i] = (float) (l * (1.0 - wet) + ol * wet);
            R[i] = (float) (rr * (1.0 - wet) + orr * wet);
        }
        meters (L, R, n, false);
    }

private:
    double lvlAvg = -18.0, dcL = 0.0, dcR = 0.0;
    SvfEq shL, shR;
};

//==============================================================================
namespace limit
{
    enum Slot { gain = 0, ceiling, release, style, clip, truePeak, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "limiter"; m.name = "Ceiling"; m.role = "Limiteur"; m.family = "Compresseurs et dynamique"; m.form = "rack";
            m.principle = "Limiteur sans anticipation (latence nulle), plafond garanti, \xC3\xA9" "cr\xC3\xAA" "tage doux final, mode cr\xC3\xAA" "te vraie";
            m.params = {
                P ("gain", "Gain", "dB", 0, 24, 0),
                P ("ceiling", "Plafond", "dB", -12, 0, -0.3f),
                P ("release", "Rel\xC3\xA2" "chement", "ms", 1, 1000, 80, 0.4f),
                C ("style", "Style", { "Transparent", "Fort" }),
                C ("clip", "\xC3\x89" "cr\xC3\xAA" "tage doux", { "Non", "Oui" }, 1),
                C ("truePeak", "Cr\xC3\xAA" "te vraie", { "Non", "Oui" }),
            };
            m.sections = { { "limiter", "Limiteur", "comp", { "gain", "ceiling", "release", "style", "clip", "truePeak" }, "main" } };
            m.presets = {
                { "S\xC3\xA9" "curit\xC3\xA9", {} },
                { "Master fort", { { "gain", 6 }, { "release", 40 }, { "style", 1 } } },
                { "Master transparent", { { "gain", 3 }, { "release", 150 }, { "ceiling", -1 }, { "truePeak", 1 } } },
                { "Streaming (-1 dBTP)", { { "gain", 4 }, { "release", 120 }, { "ceiling", -1 }, { "truePeak", 1 } } },
                { "Bus batterie", { { "gain", 4 }, { "release", 20 }, { "style", 1 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class Limiter final : public CompressorBase
{
public:
    const ModuleDesc& desc() const noexcept override { return limit::desc(); }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace limit;
        meters (L, R, n, true);
        const auto inG = dbToGain (p[gain]);
        const auto ceilIn = dbToGain (p[ceiling]);
        const bool strong = p[style] > 0.5f;
        const auto rel = msCoef (strong ? p[release] * 0.5 : p[release], sr);
        const bool soft = p[clip] > 0.5f;
        const bool tp = p[truePeak] > 0.5f;
        // en crête vraie, une marge de 0,5 dB sous le plafond absorbe les dépassements entre échantillons
        const auto ceil = tp ? ceilIn * 0.944 : ceilIn;
        const auto attSlow = msCoef (60.0, sr), relSlow = msCoef (600.0, sr);
        for (int i = 0; i < n; ++i)
        {
            auto l = (double) L[i] * inG, r = (double) R[i] * inG;
            // style « fort » : un écrêtage doux avant le limiteur absorbe les transitoires
            // (ADAA depuis la v0.22 : l'écrêtage doux ne replie plus les aigus)
            if (strong) { l = ceil * clipL.process (l / ceil * 0.9) / 0.9; r = ceil * clipR.process (r / ceil * 0.9) / 0.9; }
            auto pk = std::max (std::abs (l), std::abs (r));
            if (tp)
            {
                // crête vraie (approchée, sans anticipation) : la détection voit aussi les points
                // intermédiaires d'un suréchantillonnage ×2
                double a, b, c, d;
                osL.up (l, a, b); osR.up (r, c, d);
                pk = std::max ({ pk, std::abs (a), std::abs (b), std::abs (c), std::abs (d) });
            }
            // v0.31 : deux relâchements (moins de pompage). L'étage rapide prend chaque crête
            // tout de suite, la tient 8 ms (sous la demi-période des graves) puis relâche avec le
            // « Relâchement » ; l'étage lent suit la réduction qui dure et relâche en ~0,6 s : un
            // coup isolé ne fait plus baisser le reste du mix longtemps, un passage dense reste stable.
            const auto need = pk > ceil ? gainToDb (pk / ceil) : 0.0;
            if (need >= fastGr) { fastGr = need; hold = (int) (0.008 * sr); }
            else if (hold > 0) --hold;
            else fastGr *= rel;
            slowGr = need > slowGr ? need + (slowGr - need) * attSlow : slowGr * relSlow;
            gainState = dbToGain (-std::max (fastGr, slowGr));
            l *= gainState; r *= gainState;
            if (soft)
            {
                // écrêtage doux au-dessus de 90 % du plafond, puis plafond garanti
                auto sc = [ceil] (double x) { const auto k = 0.9 * ceil; const auto a = std::abs (x);
                                              return a <= k ? x : std::copysign (k + (ceil - k) * std::tanh ((a - k) / (ceil - k)), x); };
                l = sc (l); r = sc (r);
            }
            L[i] = (float) std::clamp (l, -ceil, ceil);
            R[i] = (float) std::clamp (r, -ceil, ceil);
        }
        grDb = -gainToDb (gainState);
        meters (L, R, n, false);
    }
    void reset() noexcept override { CompressorBase::reset(); gainState = 1.0; fastGr = slowGr = 0.0; hold = 0; osL.reset(); osR.reset(); clipL.reset(); clipR.reset(); }
private:
    double gainState = 1.0, fastGr = 0.0, slowGr = 0.0;
    int hold = 0;
    dsp::Oversampler2x osL, osR;
    dsp::AdaaTanh clipL, clipR;
};

} // namespace grv::effects
