/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Effets à pas » (Hopscotch) — principe des multi-effets séquencés en rythme (Effectrix, puis
    Looperator pour les réglages par pas) : une grille de 16 à 64 pas (4 mesures) calée sur le tempo ;
    chaque pas reçoit un effet ET un réglage de cet effet, joués pendant toute sa durée :
      - le réglage « de base » de chaque effet suit les réglages communs du module (coupure, bits,
        tranche répétée, temps d'écho…) : c'est le comportement d'avant la v0.39, inchangé ;
      - des préréglages fixes par effet (« Répète qui monte », « Filtre qui s'ouvre », « Écho sombre »…) ;
      - 4 réglages personnels (Perso 1 à 4) : un effet et ses 4 réglages, à composer soi-même.
    Effets : Répète (une tranche rejouée en boucle, hauteur qui monte ou descend, déclin, porte),
    Inverse (le passé lu à l'envers, vitesse réglable), Filtre (passe-bas / haut / bande, balayage
    sur le pas), Écrase (bits, échantillonnage, saturation), Arrêt bande (ou démarrage), Hachure
    (porte rythmique), Écho (traîne qui continue après le pas), Ralenti (lecture à 1/2, 3/4, 1/4),
    Volume (fondus, silence, pompe).
    Séquenceur arrêté : le son passe sans effet. Fondus courts à chaque changement (pas de clic).
    Les effets qui lisent la mémoire ne lisent jamais plus loin que ce qui vient d'être écrit
    (aucune latence ajoutée).
    Architecture propre (aucune reprise de code ni de réglages d'un produit existant).
*/

#pragma once

#include "../dsp/EqFilters.h"
#include "../engine/Module.h"
#include "../synth/SynthCore.h"
#include "FxBanks.h"

#include <array>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace stepfx
{
    // Genres d'effet (un pas joue un genre avec 4 réglages A à D, en unités réelles)
    enum Type { none = 0, repeatFx, reverseFx, filterFx, crushFx, tapeStopFx, chopFx, echoFx, slowFx, volumeFx, numTypes };
    constexpr int maxSteps = 64, numPerso = 4, persoParams = 5;
    // Positions des réglages : celles d'avant la v0.39 d'abord (projets enregistrés par identifiants)
    enum Slot { step1 = 0, steps = 16, division, repeatSize, cutoff, resonance, bits, echoTime, echoFeedback, mix,
                step17, persoBase = step17 + (maxSteps - 16), numSlots = persoBase + numPerso * persoParams };
    inline constexpr int stepSlot (int s) noexcept { return s < 16 ? step1 + s : step17 + (s - 16); }

    inline const std::vector<std::string>& typeNames()
    {
        static const std::vector<std::string> v { "\xE2\x80\x94", "R\xC3\xA9p\xC3\xA8te", "Inverse", "Filtre", "\xC3\x89" "crase", "Arr\xC3\xAAt bande",
                                                  "Hachure", "\xC3\x89" "cho", "Ralenti", "Volume" };
        return v;
    }

    /** Un choix de pas : genre, et préréglage (-1 = réglages communs du module, -2 - k = Perso k). */
    struct Entry { std::string name; int type; int preset; std::array<double, 4> v; };
    constexpr int baseSet = -1;
    inline constexpr int persoSet (int k) noexcept { return -2 - k; }

    inline const std::vector<Entry>& entries()
    {
        static const std::vector<Entry> e = []
        {
            std::vector<Entry> x;
            auto add = [&] (const char* n, int t, std::array<double, 4> v = {}, int pr = 0) { x.push_back ({ n, t, pr, v }); };
            auto base = [&] (const char* n, int t) { x.push_back ({ n, t, baseSet, {} }); };
            add ("\xE2\x80\x94", none, {}, baseSet);
            // les 7 effets d'avant la v0.39, dans leur ordre : convertis à la relecture des anciens projets
            base ("R\xC3\xA9p\xC3\xA8te", repeatFx);
            base ("Inverse", reverseFx);
            base ("Filtre", filterFx);
            base ("\xC3\x89" "crase", crushFx);
            base ("Arr\xC3\xAAt bande", tapeStopFx);
            base ("Hachure", chopFx);
            base ("\xC3\x89" "cho", echoFx);
            // Répète : tranche (0 = 1/64 … 4 = 1/4), hauteur sur le pas (demi-tons), déclin (%), porte (%)
            add ("R\xC3\xA9p\xC3\xA8te 1/64", repeatFx, { 0, 0, 0, 100 });
            add ("R\xC3\xA9p\xC3\xA8te 1/32", repeatFx, { 1, 0, 0, 100 });
            add ("R\xC3\xA9p\xC3\xA8te 1/16", repeatFx, { 2, 0, 0, 100 });
            add ("R\xC3\xA9p\xC3\xA8te 1/8", repeatFx, { 3, 0, 0, 100 });
            add ("R\xC3\xA9p\xC3\xA8te qui monte", repeatFx, { 2, 12, 0, 100 });
            add ("R\xC3\xA9p\xC3\xA8te qui descend", repeatFx, { 2, -12, 0, 100 });
            add ("R\xC3\xA9p\xC3\xA8te qui s'\xC3\xA9teint", repeatFx, { 1, 0, 90, 100 });
            add ("R\xC3\xA9p\xC3\xA8te serr\xC3\xA9", repeatFx, { 2, 0, 0, 40 });
            // Inverse : vitesse (×), déclin (%)
            add ("Inverse lent", reverseFx, { 0.5, 0, 0, 0 });
            add ("Inverse rapide", reverseFx, { 2.0, 0, 0, 0 });
            add ("Inverse qui s'\xC3\xA9teint", reverseFx, { 1.0, 90, 0, 0 });
            // Filtre : coupure (Hz), résonance (%), type (passe-bas, passe-haut, passe-bande), balayage (octaves sur le pas)
            add ("Filtre ferm\xC3\xA9", filterFx, { 300, 30, 0, 0 });
            add ("Filtre passe-haut", filterFx, { 1500, 30, 1, 0 });
            add ("Filtre passe-bande", filterFx, { 1000, 60, 2, 0 });
            add ("Filtre qui s'ouvre", filterFx, { 200, 50, 0, 4 });
            add ("Filtre qui se ferme", filterFx, { 6000, 50, 0, -4 });
            add ("Filtre r\xC3\xA9sonant", filterFx, { 800, 90, 0, 0 });
            // Écrase : bits, maintien (échantillons), saturation (dB)
            add ("\xC3\x89" "crase 8 bits", crushFx, { 8, 2, 0, 0 });
            add ("\xC3\x89" "crase 4 bits", crushFx, { 4, 4, 0, 0 });
            add ("\xC3\x89" "crase satur\xC3\xA9", crushFx, { 6, 3, 18, 0 });
            add ("\xC3\x89" "crase basse d\xC3\xA9" "finition", crushFx, { 10, 24, 6, 0 });
            // Arrêt bande : durée (part du pas), courbe (0 linéaire → 1 sec), sens (0 arrêt, 1 démarrage)
            add ("Arr\xC3\xAAt bande court", tapeStopFx, { 0.5, 0, 0, 0 });
            add ("Arr\xC3\xAAt bande sec", tapeStopFx, { 0.5, 1, 0, 0 });
            add ("Arr\xC3\xAAt bande au d\xC3\xA9marrage", tapeStopFx, { 0.75, 0, 1, 0 });
            // Hachure : vitesse (0 = 1/64 … 3 = 1/8), ouverture (%), profondeur (%), douceur (%)
            add ("Hachure 1/32", chopFx, { 1, 50, 100, 0 });
            add ("Hachure 1/16", chopFx, { 2, 50, 100, 0 });
            add ("Hachure douce", chopFx, { 1, 50, 100, 70 });
            add ("Hachure l\xC3\xA9g\xC3\xA8re", chopFx, { 0, 50, 50, 30 });
            // Écho : temps (0 = 1/16 … 5 = 1/4.), répétitions (%), couleur (Hz du passe-bas de la boucle)
            add ("\xC3\x89" "cho 1/8", echoFx, { 2, 50, 16000, 0 });
            add ("\xC3\x89" "cho 1/4 point\xC3\xA9" "e", echoFx, { 5, 45, 16000, 0 });
            add ("\xC3\x89" "cho sombre", echoFx, { 3, 60, 1500, 0 });
            add ("\xC3\x89" "cho long", echoFx, { 2, 85, 6000, 0 });
            // Ralenti : vitesse de lecture (×), déclin (%)
            add ("Ralenti 1/2", slowFx, { 0.5, 0, 0, 0 });
            add ("Ralenti 3/4", slowFx, { 0.75, 0, 0, 0 });
            add ("Ralenti 1/4", slowFx, { 0.25, 0, 0, 0 });
            // Volume : forme (fondu sortant, fondu entrant, silence, pompe), profondeur (%)
            add ("Volume : fondu sortant", volumeFx, { 0, 100, 0, 0 });
            add ("Volume : fondu entrant", volumeFx, { 1, 100, 0, 0 });
            add ("Volume : silence", volumeFx, { 2, 100, 0, 0 });
            add ("Volume : pompe", volumeFx, { 3, 100, 0, 0 });
            for (int k = 0; k < numPerso; ++k) x.push_back ({ "Perso " + std::to_string (k + 1), none, persoSet (k), {} });
            return x;
        }();
        return e;
    }
    inline const std::vector<std::string>& entryNames()
    {
        static const std::vector<std::string> v = [] { std::vector<std::string> n; for (const auto& e : entries()) n.push_back (e.name); return n; }();
        return v;
    }
    /** Choix d'un pas d'avant la v0.39 (8 choix : —, Répète … Écho) → choix actuel (les mêmes, en tête de liste). */
    inline int fromOldChoice (int old) noexcept { return std::clamp (old, 0, 7); }

    /** Réglage personnel (0 à 1) → unité réelle, selon le genre d'effet. */
    inline double persoPlain (int type, int k, double u) noexcept
    {
        u = std::clamp (u, 0.0, 1.0);
        switch (type)
        {
            case repeatFx:   { const double r[4] { std::round (u * 4.0), -12.0 + 24.0 * u, 100.0 * u, 25.0 + 75.0 * u }; return r[k]; }
            case reverseFx:  { const double r[4] { 0.5 * std::pow (4.0, u), 100.0 * u, 0, 0 }; return r[k]; }
            case filterFx:   { const double r[4] { 80.0 * std::pow (150.0, u), 100.0 * u, std::round (u * 2.0), -4.0 + 8.0 * u }; return r[k]; }
            case crushFx:    { const double r[4] { 3.0 + std::round (9.0 * u), 1.0 + std::round (31.0 * u), 24.0 * u, 0 }; return r[k]; }
            case tapeStopFx: { const double r[4] { 0.25 + 0.75 * u, u, std::round (u), 0 }; return r[k]; }
            case chopFx:     { const double r[4] { std::round (u * 3.0), 10.0 + 80.0 * u, 100.0 * u, 100.0 * u }; return r[k]; }
            case echoFx:     { const double r[4] { std::round (u * 5.0), 90.0 * u, 800.0 * std::pow (20.0, u), 0 }; return r[k]; }
            case slowFx:     { const double r[4] { 0.25 + 0.5 * u, 100.0 * u, 0, 0 }; return r[k]; }
            case volumeFx:   { const double r[4] { std::round (u * 3.0), 100.0 * u, 0, 0 }; return r[k]; }
            default: return 0.0;
        }
    }

    inline double divisionQuarters (int i) noexcept { static constexpr double q[] { 0.125, 0.25, 0.5, 1.0 }; return q[std::clamp (i, 0, 3)]; }
    inline double sliceQuarters (int i) noexcept { static constexpr double q[] { 1.0 / 16, 0.125, 0.25, 0.5, 1.0 }; return q[std::clamp (i, 0, 4)]; }
    inline double echoQuarters (int i) noexcept { static constexpr double q[] { 0.25, 0.375, 0.5, 0.75, 1.0, 1.5 }; return q[std::clamp (i, 0, 5)]; }
    inline double chopQuarters (int i) noexcept { static constexpr double q[] { 0.0625, 0.125, 0.25, 0.5 }; return q[std::clamp (i, 0, 3)]; }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "stepfx";
            m.name = "Hopscotch"; m.role = "Effets \xC3\xA0 pas";
            m.family = "Filtres et performance"; m.form = "module";
            m.principle = "Grille de 16 \xC3\xA0 64 pas cal\xC3\xA9" "e sur le tempo : un effet et son r\xC3\xA9glage par pas (pr\xC3\xA9r\xC3\xA9glages par effet et 4 r\xC3\xA9glages perso)";
            for (int s = 1; s <= 16; ++s) m.params.push_back (C ("s" + std::to_string (s), "Pas " + std::to_string (s), entryNames(), 0));
            m.params.push_back (I ("steps", "Longueur", "pas", 1, maxSteps, 16));
            m.params.push_back (C ("division", "Pas de", { "1/32", "1/16", "1/8", "1/4" }, 1));
            m.params.push_back (C ("repeat", "Tranche r\xC3\xA9p\xC3\xA9t\xC3\xA9" "e", { "1/64", "1/32", "1/16", "1/8" }, 1));
            m.params.push_back (P ("cutoff", "Coupure", "Hz", 80, 12000, 700, 0.3f));
            m.params.push_back (P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 55));
            m.params.push_back (I ("bits", "Bits", "", 3, 12, 6));
            m.params.push_back (C ("echotime", "Temps de l'\xC3\xA9" "cho", { "1/16", "1/16.", "1/8", "1/8." }, 1));
            m.params.push_back (P ("echofb", "R\xC3\xA9p\xC3\xA9titions", "%", 0, 90, 50));
            m.params.push_back (P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100));
            // v0.39 : pas 17 à 64 (4 mesures de 1/16), puis les 4 réglages perso
            for (int s = 17; s <= maxSteps; ++s) m.params.push_back (C ("s" + std::to_string (s), "Pas " + std::to_string (s), entryNames(), 0));
            static constexpr int defType[numPerso] { repeatFx, filterFx, tapeStopFx, echoFx };
            static constexpr float defVals[numPerso][4] { { 50, 50, 0, 100 }, { 30, 50, 0, 50 }, { 100, 0, 0, 0 }, { 40, 50, 100, 0 } };
            for (int k = 0; k < numPerso; ++k)
            {
                const auto pre = "p" + std::to_string (k + 1), nm = "Perso " + std::to_string (k + 1);
                m.params.push_back (C (pre + "type", nm + " \xC2\xB7 effet", typeNames(), defType[k]));
                for (int j = 0; j < 4; ++j)
                    m.params.push_back (P (pre + (char) ('a' + j), nm + " \xC2\xB7 r\xC3\xA9glage " + (char) ('A' + j), "%", 0, 100, defVals[k][j]));
            }
            for (int b = 0; b < maxSteps / 8; ++b)
            {
                SectionDesc s { "steps" + std::to_string (b + 1), "Pas " + std::to_string (b * 8 + 1) + " \xC3\xA0 " + std::to_string (b * 8 + 8), "global", {}, "steps" };
                for (int i = 1; i <= 8; ++i) s.params.push_back ("s" + std::to_string (b * 8 + i));
                m.sections.push_back (s);
            }
            m.sections.push_back ({ "settings", "R\xC3\xA9glages", "global", { "steps", "division", "repeat", "cutoff", "reso", "bits", "echotime", "echofb" }, "settings" });
            m.sections.push_back ({ "out", "Sortie", "global", { "mix" }, "settings" });
            for (int k = 0; k < numPerso; ++k)
            {
                const auto pre = "p" + std::to_string (k + 1);
                m.sections.push_back ({ "perso" + std::to_string (k + 1), "Perso " + std::to_string (k + 1), "global",
                                        { pre + "type", pre + "a", pre + "b", pre + "c", pre + "d" }, "perso" });
            }
            auto find = [] (const char* n) { const auto& v = entryNames(); return (int) (std::find (v.begin(), v.end(), std::string (n)) - v.begin()); };
            // grille : 0 = rien, 1 à 7 = effets de base (comme avant la v0.39), ou un nom de préréglage
            auto grid = [] (std::initializer_list<int> v)
            {
                std::vector<std::pair<std::string, float>> out;
                int s = 1;
                for (auto x : v) { if (x != 0) out.push_back ({ "s" + std::to_string (s), (float) x }); ++s; }
                return out;
            };
            const int rUp = find ("R\xC3\xA9p\xC3\xA8te qui monte"), r32 = find ("R\xC3\xA9p\xC3\xA8te 1/32"), fOpen = find ("Filtre qui s'ouvre"),
                      tStop = find ("Arr\xC3\xAAt bande sec"), pump = find ("Volume : pompe"), slow = find ("Ralenti 1/2"), eDark = find ("\xC3\x89" "cho sombre"),
                      fade = find ("Volume : fondu sortant"), rev = find ("Inverse rapide");
            auto bars = [] (std::vector<std::pair<int, int>> v, int len)
            {
                std::vector<std::pair<std::string, float>> out;
                for (auto [s, x] : v) out.push_back ({ "s" + std::to_string (s), (float) x });
                out.push_back ({ "steps", (float) len });
                return out;
            };
            m.presets = {
                { "Vide", {} },
                { "B\xC3\xA9gaiement", grid ({ 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1 }) },
                { "Retour en arri\xC3\xA8re", grid ({ 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 2, 0, 2, 2 }) },
                { "Fin de mesure \xC3\xA9" "cras\xC3\xA9" "e", grid ({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 5, 5 }) },
                { "Hach\xC3\xA9 filtr\xC3\xA9", grid ({ 6, 0, 3, 0, 6, 0, 3, 3, 6, 0, 3, 0, 6, 6, 3, 3 }) },
                { "\xC3\x89" "chos sur les contretemps", grid ({ 0, 0, 7, 0, 0, 0, 7, 0, 0, 0, 7, 0, 0, 7, 7, 0 }) },
                { "Tout m\xC3\xA9lang\xC3\xA9", grid ({ 0, 1, 0, 2, 3, 0, 6, 1, 0, 5, 0, 4, 7, 2, 1, 5 }) },
                // v0.39 : sur 2 et 4 mesures, avec les préréglages des effets
                { "Roulement qui monte (2 mesures)", bars ({ { 13, r32 }, { 14, r32 }, { 15, rUp }, { 16, rUp }, { 29, r32 }, { 30, rUp }, { 31, rUp }, { 32, tStop } }, 32) },
                { "Mont\xC3\xA9" "e filtr\xC3\xA9" "e (4 mesures)", bars ({ { 49, fOpen }, { 50, fOpen }, { 51, fOpen }, { 52, fOpen }, { 53, fOpen }, { 54, fOpen }, { 55, fOpen },
                                                                  { 56, fOpen }, { 61, r32 }, { 62, r32 }, { 63, rUp }, { 64, rUp } }, 64) },
                { "Pompe et ralentis (2 mesures)", bars ({ { 1, pump }, { 5, pump }, { 9, pump }, { 13, pump }, { 15, slow }, { 17, pump }, { 21, pump }, { 25, pump },
                                                          { 27, rev }, { 29, eDark }, { 31, fade }, { 32, fade } }, 32) },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class StepFx final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return stepfx::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        size = 1;
        while (size < (int) (sr * 4.0)) size <<= 1;
        bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f);
        echoSize = 1;
        while (echoSize < (int) (sr * 3.0)) echoSize <<= 1; // 1/4 pointée à 40 BPM : 2,25 s
        echoL.assign ((size_t) echoSize, 0.0f); echoR.assign ((size_t) echoSize, 0.0f);
        reset();
    }

    void reset() noexcept override
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f);
        std::fill (echoL.begin(), echoL.end(), 0.0f); std::fill (echoR.begin(), echoR.end(), 0.0f);
        w = ew = 0; lastStep = -1; k = 0; current = 0; curType = stepfx::none; mixNow = 0.0; fade = 1.0; holdL = holdR = 0.0; holdN = 0;
        ph = 0.0; echoD = echoTarget; echoEntry = -1; tone[0] = tone[1] = 0.0; stepNow = -1;
        declick.prepare (sr);
        for (auto& f : flt) f.reset();
    }

    void setPosition (double ppq, bool isPlaying) noexcept override { ppqStart = ppq; playing = isPlaying; }

    /** Mesure 0 : pas joué (1 à 64, 0 à l'arrêt) — la tête de lecture de l'écran. */
    float meter (int i) const noexcept override { return i == 0 ? (float) (stepNow + 1) : 0.0f; }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace stepfx;
        qPerSample = std::max (20.0, bpm) / 60.0 / sr;
        const auto stepQ = divisionQuarters ((int) p[division]);
        const auto stepsN = std::clamp ((int) std::lround (p[steps]), 1, maxSteps);
        stepSamples = std::max (1.0, stepQ / qPerSample);
        const auto wet = p[mix] / 100.0;
        const auto mixStep = 1.0 / (0.004 * sr), fadeStep = 1.0 / (0.002 * sr);
        const auto mask = size - 1, emask = echoSize - 1;
        resolve (current, p);                  // réglages du pas en cours (automation, perso modifiés)
        if (echoEntry >= 0) resolveEcho (p);   // l'écho garde le réglage de son dernier pas

        for (int i = 0; i < n; ++i)
        {
            const double inL = L[i], inR = R[i];
            bufL[(size_t) w] = (float) inL; bufR[(size_t) w] = (float) inR;
            bool jump = false; // v0.22 : saut de signal (changement de pas ou d'effet) compensé en 2 ms

            int entry = 0;
            if (playing)
            {
                const auto q = ppqStart + i * qPerSample;
                const auto idx = (long long) std::floor (q / stepQ + 1.0e-9);
                const auto step = (int) (((idx % stepsN) + stepsN) % stepsN);
                stepNow = step;
                if (idx != lastStep)
                {
                    lastStep = idx; stepStart = w; k = 0; ph = 0.0; fade = 0.0;
                    jump = readsMemory (curType); // lecture de la mémoire relancée
                }
                entry = std::clamp ((int) p[stepSlot (step)], 0, (int) entries().size() - 1);
            }
            else stepNow = -1;
            if (entry != current)
            {
                current = entry; fade = 0.0; jump = true;
                resolve (current, p);
                if (curType == echoFx) { const bool first = echoEntry < 0; echoEntry = current; resolveEcho (p); if (first) echoD = echoTarget; }
            }

            const auto t = std::min (1.0, (double) k / stepSamples); // avancée dans le pas
            double yl = inL, yr = inR;
            switch (curType)
            {
                case repeatFx:
                {
                    // première tranche : le son passe (elle s'enregistre) ; ensuite elle est relue en boucle
                    double pos, phm;
                    if (k < slice) { phm = (double) k; pos = (double) stepStart + phm; }
                    else
                    {
                        phm = std::fmod (ph, slice);
                        pos = (double) stepStart + phm;
                        ph += set[1] == 0.0 ? 1.0 : std::exp2 (set[1] * t / 12.0); // hauteur qui glisse sur le pas
                    }
                    readAt (pos, yl, yr);
                    const auto gateEnd = slice * set[3] / 100.0;
                    const auto edge = std::clamp (std::min (phm, gateEnd - phm) / 48.0, 0.0, 1.0); // fondu en bord de tranche
                    const auto g = edge * (1.0 - set[2] / 100.0 * t);
                    yl *= g; yr *= g;
                    break;
                }
                case reverseFx:
                {
                    const auto back = 1.0 + (double) k * set[0];
                    if (back < (double) size - 64.0) readAt ((double) stepStart - back, yl, yr); else yl = yr = 0.0;
                    const auto g = 1.0 - set[1] / 100.0 * t;
                    yl *= g; yr *= g;
                    break;
                }
                case filterFx:
                    if (set[3] != 0.0 && (k & 31) == 0) setFilter (set[0] * std::exp2 (set[3] * t));
                    yl = flt[0].process (inL); yr = flt[1].process (inR);
                    break;
                case crushFx:
                {
                    if (--holdN <= 0) { holdN = holdLen; holdL = inL; holdR = inR; }
                    yl = std::round (holdL * quant) / quant; yr = std::round (holdR * quant) / quant;
                    if (driveG > 1.0) { yl = std::tanh (yl * driveG) * driveComp; yr = std::tanh (yr * driveG) * driveComp; }
                    break;
                }
                case tapeStopFx:
                {
                    // vitesse 1 → 0 (arrêt) ou 0 → 1 (démarrage) sur la durée réglée ; jamais en avance sur l'écriture
                    const auto nDur = std::max (1.0, stepSamples * set[0]);
                    const auto kk = (double) k, u = std::min (1.0, kk / nDur), e = 1.0 + 3.0 * set[1];
                    double off, speed;
                    if (set[2] < 0.5) { off = nDur * (1.0 - std::pow (1.0 - u, e + 1.0)) / (e + 1.0); speed = std::pow (1.0 - u, e); }
                    else { off = nDur * std::pow (u, e + 1.0) / (e + 1.0) + std::max (0.0, kk - nDur); speed = kk >= nDur ? 1.0 : std::pow (u, e); }
                    readAt ((double) stepStart + off, yl, yr);
                    const auto g = std::sqrt (std::max (0.0, speed));
                    yl *= g; yr *= g;
                    break;
                }
                case slowFx:
                {
                    readAt ((double) stepStart + (double) k * set[0], yl, yr);
                    const auto g = 1.0 - set[1] / 100.0 * t;
                    yl *= g; yr *= g;
                    break;
                }
                case chopFx:
                {
                    const auto cph = std::fmod ((double) k, chopPeriod) / chopPeriod, duty = set[1] / 100.0;
                    const auto slope = 40.0 / (1.0 + set[3] / 100.0 * 19.0);
                    const auto g = cph < duty ? std::min (1.0, cph * slope) : std::max (0.0, 1.0 - (cph - duty) * slope);
                    const auto gg = 1.0 - set[2] / 100.0 * (1.0 - g);
                    yl = inL * gg; yr = inR * gg;
                    break;
                }
                case volumeFx:
                {
                    const auto d = set[1] / 100.0;
                    const auto shape = (int) set[0];
                    const auto g = shape == 0 ? 1.0 - d * t : shape == 1 ? 1.0 - d * (1.0 - t) : shape == 2 ? 1.0 - d : 1.0 - d * (1.0 - t) * (1.0 - t);
                    yl = inL * g; yr = inR * g;
                    break;
                }
                default: break;
            }

            // écho : alimenté pendant les pas « Écho », sa traîne continue ensuite
            double el, er;
            {
                echoD += (echoTarget - echoD) * 0.0005; // changement de temps glissé (pas de saut)
                auto pos = (double) ew - echoD; if (pos < 0.0) pos += echoSize;
                const auto i0 = (int) pos; const auto f = pos - (double) i0;
                el = echoL[(size_t) (i0 & emask)] * (1.0 - f) + echoL[(size_t) ((i0 + 1) & emask)] * f;
                er = echoR[(size_t) (i0 & emask)] * (1.0 - f) + echoR[(size_t) ((i0 + 1) & emask)] * f;
                if (toneC < 1.0) { tone[0] += (el - tone[0]) * toneC; tone[1] += (er - tone[1]) * toneC; el = tone[0]; er = tone[1]; }
                const auto send = curType == echoFx ? 1.0 : 0.0;
                echoL[(size_t) ew] = (float) (inL * send + er * echoFb);   // croisé : de gauche à droite
                echoR[(size_t) ew] = (float) (inR * send + el * echoFb);
                ew = (ew + 1) & emask;
            }

            // mélange avec fondus : entrée et sortie d'un effet, changement d'effet
            fade = std::min (1.0, fade + fadeStep);
            const auto target = curType != none && curType != echoFx ? 1.0 : 0.0;
            mixNow += std::clamp (target - mixNow, -mixStep, mixStep);
            const auto m = mixNow * wet;
            // les effets qui lisent la mémoire repartent en fondu à chaque pas (pas de saut)
            const auto fd = readsMemory (curType) ? fade : 1.0;
            const auto fl = yl * fd + inL * (1.0 - fd), fr = yr * fd + inR * (1.0 - fd);
            L[i] = (float) (inL * (1.0 - m) + fl * m + el * wet);
            R[i] = (float) (inR * (1.0 - m) + fr * m + er * wet);
            if (jump) declick.arm();
            declick.process (L + i, R + i, 1);
            w = (w + 1) & mask;
            ++k;
        }
        ppqStart += n * qPerSample;
    }

private:
    static bool readsMemory (int type) noexcept
    {
        using namespace stepfx;
        return type == repeatFx || type == reverseFx || type == tapeStopFx || type == slowFx;
    }

    /** Lecture interpolée de la mémoire (position absolue en échantillons). */
    void readAt (double pos, double& l, double& r) const noexcept
    {
        const auto mask = size - 1;
        const auto i0 = (int) std::floor (pos); const auto f = pos - (double) i0;
        l = bufL[(size_t) (i0 & mask)] * (1.0 - f) + bufL[(size_t) ((i0 + 1) & mask)] * f;
        r = bufR[(size_t) (i0 & mask)] * (1.0 - f) + bufR[(size_t) ((i0 + 1) & mask)] * f;
    }

    void setFilter (double fc) noexcept
    {
        static constexpr SvfEq::Type types[3] { SvfEq::lowpass, SvfEq::highpass, SvfEq::bandpass };
        const auto c = std::clamp (fc, 20.0, 0.45 * sr);
        for (auto& f : flt) f.set (types[std::clamp ((int) set[2], 0, 2)], c, 0.7 + set[1] / 100.0 * 9.0, 0.0, sr);
    }

    /** Réglages réels A à D d'un choix de pas : réglages communs (de base), préréglage, ou perso. */
    static void plainOf (int entry, const float* p, int& type, std::array<double, 4>& v) noexcept
    {
        using namespace stepfx;
        const auto& e = entries()[(size_t) std::clamp (entry, 0, (int) entries().size() - 1)];
        type = e.type;
        if (e.preset >= 0) { v = e.v; return; }
        if (e.preset <= persoSet (0))
        {
            const auto kk = persoSet (0) - e.preset;
            const auto* q = p + persoBase + kk * persoParams;
            type = std::clamp ((int) q[0], 0, numTypes - 1);
            for (int j = 0; j < 4; ++j) v[(size_t) j] = persoPlain (type, j, q[1 + j] / 100.0);
            return;
        }
        // réglages communs du module : le son d'avant la v0.39
        switch (type)
        {
            case repeatFx:   v = { std::round ((double) p[repeatSize]), 0, 0, 100 }; break;
            case reverseFx:  v = { 1, 0, 0, 0 }; break;
            case filterFx:   v = { p[cutoff], p[resonance], 0, 0 }; break;
            case crushFx:    { const auto b = std::clamp ((double) std::lround (p[bits]), 3.0, 12.0); v = { b, 2.0 + (12.0 - b), 0, 0 }; break; }
            case tapeStopFx: v = { 1, 0, 0, 0 }; break;
            case chopFx:     v = { 0, 50, 100, 0 }; break;
            case echoFx:     v = { std::round ((double) p[echoTime]), p[echoFeedback], 16000, 0 }; break;
            default:         v = {}; break;
        }
    }

    void resolve (int entry, const float* p) noexcept
    {
        using namespace stepfx;
        plainOf (entry, p, curType, set);
        switch (curType)
        {
            case repeatFx: slice = std::max (32.0, sliceQuarters ((int) set[0]) / qPerSample); break;
            case filterFx: setFilter (set[0] * std::exp2 (set[3] * std::min (1.0, (double) k / stepSamples))); break;
            case crushFx:
            {
                const auto b = std::clamp ((int) set[0], 3, 12);
                quant = std::ldexp (1.0, b - 1); holdLen = std::max (1, (int) set[1]);
                driveG = std::pow (10.0, set[2] / 20.0); driveComp = 1.0 / std::sqrt (driveG);
                break;
            }
            case chopFx: chopPeriod = std::max (8.0, chopQuarters ((int) set[0]) / qPerSample); break;
            default: break;
        }
    }

    void resolveEcho (const float* p) noexcept
    {
        using namespace stepfx;
        int type = 0; std::array<double, 4> v {};
        plainOf (echoEntry, p, type, v);
        if (type != echoFx) { echoEntry = -1; return; }
        echoTarget = std::clamp (echoQuarters ((int) v[0]) / qPerSample, 16.0, (double) echoSize - 4.0);
        echoFb = std::clamp (v[1], 0.0, 90.0) / 100.0;
        toneC = v[2] >= 15999.0 ? 1.0 : 1.0 - std::exp (-kTwoPi * std::clamp (v[2], 200.0, 0.45 * sr) / sr);
    }

    double sr = 48000.0, ppqStart = 0.0, mixNow = 0.0, fade = 1.0, holdL = 0.0, holdR = 0.0, qPerSample = 0.0, stepSamples = 1.0;
    double slice = 64.0, ph = 0.0, quant = 32.0, driveG = 1.0, driveComp = 1.0, chopPeriod = 64.0;
    double echoD = 4800.0, echoTarget = 4800.0, echoFb = 0.5, toneC = 1.0;
    std::array<double, 2> tone {};
    std::array<double, 4> set {};
    synth::Declick declick; // sauts aux changements de pas et d'effet (v0.22)
    bool playing = false;
    int size = 1, echoSize = 1, w = 0, ew = 0, stepStart = 0, k = 0, current = 0, curType = 0, holdN = 0, holdLen = 2, echoEntry = -1, stepNow = -1;
    long long lastStep = -1;
    std::vector<float> bufL, bufR, echoL, echoR;
    std::array<SvfEq, 2> flt;
};

} // namespace grv::effects
