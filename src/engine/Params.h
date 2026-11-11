/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Description des réglages d'un module (instrument, effet, source de modulation) et
    adressage unique de tous les réglages du groovebox.

    Chaque canal (16 pistes, 4 retours, bus principal) dispose de 2048 emplacements (v0.10 ; 1024 avant) :
        0..15       mixage du canal (volume, panoramique, envois, contournement des inserts)
        16..1039    instrument (1024)
        1040..1295  inserts 1 à 4 (64 chacun)
        1296..1535  sources de modulation 1 à 6 (40 chacune)
        1536..1599  profondeurs de modulation (64 assignations)
    Un réglage a donc une adresse globale : canal × 2048 + emplacement. Les projets et les
    préréglages sont enregistrés par identifiants (canal, zone, réglage), pas par adresse.
    Scènes, verrous par pas, Break, modulations et sauvegarde passent tous par ces adresses.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace grv::engine
{

constexpr int numTracks = 16;
constexpr int numReturns = 4;
constexpr int masterChannel = numTracks + numReturns;      // 20
// Bus de groupe (v0.24) : après le bus principal (son numéro ne change pas : anciens projets intacts)
constexpr int numGroups = 7;                              // v0.46 : 7 groupes (4 avant)
constexpr int groupBase = masterChannel + 1;               // 21 à 24
constexpr int numChannels = groupBase + numGroups;         // 28
inline constexpr bool isGroupChannel (int channel) noexcept { return channel >= groupBase && channel < groupBase + numGroups; }
constexpr int slotsPerChannel = 8192; // 2048 jusqu'à la v0.13.0, 4096 jusqu'à la v0.38 (projets enregistrés par identifiants : sans effet)
constexpr int numSlots = numChannels * slotsPerChannel;

constexpr int mixerBase = 0;        // 16
// Le projet est enregistré par identifiants (canal, zone, réglage) : ces positions peuvent changer
// d'une version à l'autre sans casser les anciens projets.
constexpr int instrumentBase = 16;
constexpr int instrumentSlots = 3072; // 1024 jusqu'à la v0.41 (v0.42 : Pieuvre 8 à 8 pistes internes)
constexpr int insertBase = instrumentBase + instrumentSlots; // 3088 : 4 × 128
constexpr int numInserts = 4;
constexpr int insertSlots = 128; // 64 jusqu'à la v0.38 (Hopscotch à 64 pas : 93 réglages)
constexpr int modSourceBase = insertBase + numInserts * insertSlots; // 3600 : 6 × 40
constexpr int numModSources = 6;
constexpr int modSourceSlots = 40;
constexpr int modAmountBase = modSourceBase + numModSources * modSourceSlots; // 3840 : 64
constexpr int numModAssigns = 64;
// v0.42 : emplacements d'échantillons d'une piste (16 pads d'une batterie ; 128 pour Pieuvre 8, réserve de ses 8 pistes internes)
constexpr int maxSamples = 128;
// Inserts par pad (batteries) : 2 effets du catalogue par pad, chacun avec son interrupteur
constexpr int numPads = 16;
constexpr int padInserts = 2;
constexpr int padBypassBase = modAmountBase + numModAssigns;             // 3904 : 16 × 2
constexpr int padInsertBase = 4096;                                      // 4096 : 16 × 2 × 128 (jusqu'à 8192)
inline constexpr int padInsertSlot (int pad, int slot) noexcept { return padInsertBase + (pad * padInserts + slot) * insertSlots; }
static_assert (padBypassBase + numPads * padInserts <= padInsertBase, "interrupteurs des inserts de pad");
// après la 1.1 : « Son du pas » d'une piste — numéro de préréglage verrouillé sur un pas (0 : aucun) ;
// le pas joue avec tous les réglages de ce préréglage, la piste revient à son son au pas suivant sans verrou
constexpr int trackSoundSlot = padBypassBase + numPads * padInserts;    // 3936
constexpr int trackSoundMax = 1023;
static_assert (trackSoundSlot < padInsertBase, "son du pas");
static_assert (padInsertBase + numPads * padInserts * insertSlots <= slotsPerChannel, "les zones d'un canal dépassent leurs emplacements");

// Réglages de mixage du canal
enum MixerSlot { mixVolume = 0, mixPan, mixSendA, mixSendB, mixSendC, mixSendD,
                 mixBypass1, mixBypass2, mixBypass3, mixBypass4, mixWidth, mixSendPre,
                 mixEqLow, mixEqMid, mixEqMidFreq, mixEqHigh, numMixerSlots }; // égaliseur 3 bandes de la tranche (v0.18.1)
static_assert (numMixerSlots <= instrumentBase, "les réglages de mixage débordent sur ceux de l'instrument");

inline int globalSlot (int channel, int local) noexcept { return channel * slotsPerChannel + local; }
inline int channelOf (int global) noexcept { return global / slotsPerChannel; }
inline int localOf (int global) noexcept { return global % slotsPerChannel; }

//==============================================================================
/** Description d'un réglage. Les noms sont en UTF-8. */
struct ParamDesc
{
    std::string id;       // identifiant stable (sauvegarde), ex. "cutoff"
    std::string name;     // libellé affiché, ex. "Coupure"
    std::string unit;     // unité d'affichage : "Hz", "ms", "%", "dB", "dt", "ct", "pan", "" …
    float min = 0.0f, max = 1.0f, def = 0.0f;
    float skew = 1.0f;    // même convention que JUCE : norm = ((v - min) / (max - min)) ^ skew
    std::vector<std::string> choices; // réglage à crans nommés (formes d'onde, modes…)
    bool integer = false; // réglage à crans numériques
    bool keepOnPreset = false; // v0.25 : réglage de branchement (source d'un vocodeur…) gardé quand on choisit un son
    bool noMod = false;     // après la 1.1 : verrouillable (pas, scènes) mais jamais modulé (numéro de son, banque)
    bool fixed = false;     // après la 1.1 : réglage posé une fois pour toutes (numéro de CC, canal) : ni scène, ni verrou, ni modulation, ni courbe
    bool momentary = false; // GOLEM 1.0 : bouton maintenu (touches du FM Player) : une note apprise l'appuie puis le relâche

    bool stepped() const noexcept { return ! choices.empty() || integer; }

    float toPlain (float norm) const noexcept
    {
        norm = std::clamp (norm, 0.0f, 1.0f);
        if (! choices.empty())
            return (float) std::min ((int) choices.size() - 1, (int) std::floor (norm * (float) choices.size()));
        auto v = min + (max - min) * (skew == 1.0f ? norm : std::pow (norm, 1.0f / skew));
        if (integer) v = std::round (v);
        return v;
    }

    float toNorm (float plain) const noexcept
    {
        if (! choices.empty())
        {
            const auto n = (int) choices.size();
            const auto i = std::clamp ((int) std::lround (plain), 0, n - 1);
            return n <= 1 ? 0.0f : ((float) i + 0.5f) / (float) n;
        }
        if (max <= min) return 0.0f;
        const auto x = std::clamp ((plain - min) / (max - min), 0.0f, 1.0f);
        return skew == 1.0f ? x : std::pow (x, skew);
    }
};

/** Side-chain (v0.23) : « Interne » puis les 16 pistes (réglage « scsrc » des compresseurs). */
inline const std::vector<std::string>& sidechainChoices()
{
    static const std::vector<std::string> v = []
    {
        std::vector<std::string> c { "Interne" };
        for (int t = 1; t <= numTracks; ++t) c.push_back ("Piste " + std::to_string (t));
        return c;
    }();
    return v;
}

/** Source du suiveur (v0.23) : la piste elle-même, l'audio d'une autre piste ou ses notes. */
inline const std::vector<std::string>& followSourceChoices()
{
    static const std::vector<std::string> v = []
    {
        std::vector<std::string> c { "Cette piste" };
        for (int t = 1; t <= numTracks; ++t) c.push_back ("Audio piste " + std::to_string (t));
        for (int t = 1; t <= numTracks; ++t) c.push_back ("Notes piste " + std::to_string (t));
        return c;
    }();
    return v;
}

/** Fabriques concises de descriptions. */
inline ParamDesc P (std::string id, std::string name, std::string unit, float mn, float mx, float def, float skew = 1.0f)
{
    ParamDesc d; d.id = std::move (id); d.name = std::move (name); d.unit = std::move (unit);
    d.min = mn; d.max = mx; d.def = def; d.skew = skew; return d;
}
inline ParamDesc C (std::string id, std::string name, std::vector<std::string> choices, int def = 0)
{
    ParamDesc d; d.id = std::move (id); d.name = std::move (name);
    d.choices = std::move (choices); d.min = 0; d.max = (float) d.choices.size() - 1; d.def = (float) def; return d;
}
inline ParamDesc I (std::string id, std::string name, std::string unit, int mn, int mx, int def)
{
    ParamDesc d; d.id = std::move (id); d.name = std::move (name); d.unit = std::move (unit);
    d.min = (float) mn; d.max = (float) mx; d.def = (float) def; d.integer = true; return d;
}

/** Section d'un module : un onglet de l'écran SON ou EFFETS. Jusqu'à 8 réglages par page. */
struct SectionDesc
{
    std::string id;          // "osc", "filter", "env"…
    std::string name;        // « Oscillateur »…
    std::string visual;      // indication d'affichage pour l'interface : "osc", "filter", "env", "comp", "dist", "eq"…
    std::vector<std::string> params; // identifiants des réglages, dans l'ordre des encodeurs
    std::string tab;         // onglet de l'en-tête (« global », « osc », « filter », « env », « amp », « mod ») ; vide = section seule
};

struct Preset
{
    std::string name;
    std::vector<std::pair<std::string, float>> values; // identifiant -> valeur réelle (unités du réglage)
    std::string category;                               // banque d'usine (Basses, Nappes…) ; vide = « Classiques »
};

/** Description complète d'un type de module. */
struct ModuleDesc
{
    std::string type;        // identifiant du type, ex. "acid"
    std::string name;        // nom affiché, ex. « Basse acide »
    std::string family;      // « Batteries », « Basses », « Polys », « Nappes », « Sampler », « Entrée »
    std::string principle;   // l'architecture reprise
    std::string role;        // v0.35.1, effets : rôle quand le nom est un nom propre (« Iris » : « Filtre »)
    std::string form;        // effets : « pedal » (pédalier), « rack » (unité 19 pouces) ou « module » (v0.35.1 : interface dédiée)
    std::vector<ParamDesc> params;
    std::vector<SectionDesc> sections;
    std::vector<Preset> presets;
    bool drumKit = false;    // 16 sons sur les 16 pads (séquenceur à lignes)
    bool laneNotes = false;  // v0.42 : lignes qui jouent aussi la note du pas (Pieuvre 8 : note = 128 + ligne × 128 + note du pas)
    bool laneLowKeys = false; // GOLEM 1.0 : avec laneNotes, les notes 0 à 15 sont aussi des touches (FM Player), pas des pads
    std::vector<std::string> padNames, padKinds; // batteries : nom et famille de chaque pad
    // Modulation commune des synthés (synth/ModHub.h) : destinations de la matrice (index des
    // réglages, 0 = aucune) et nombre d'enveloppes, de LFO et de routages ajoutés.
    std::vector<int> modDests;
    int hubEnvs = 0, hubLfos = 0, hubSlots = 0;
    int hubExtEnvs = 0, hubExtLfos = 0, hubMacros = 0; // enveloppes et LFO propres à l'instrument, sources de la matrice
    // Batteries à machines : nom de chaque machine et libellés de ses réglages de voix (les
    // libellés affichés d'un pad changent avec la machine choisie)
    std::vector<std::string> machineNames;
    std::vector<std::vector<std::string>> machineLabels;
    /** Tables d'ondes (wave morph) : allure de chaque onde de la bibliothèque (64 points, -1..1), pour l'interface. */
    std::vector<std::vector<float>> waveShapes;

    int indexOf (const std::string& id) const noexcept
    {
        for (size_t i = 0; i < params.size(); ++i)
            if (params[i].id == id) return (int) i;
        return -1;
    }
};

/** Version 7 du format (projets, photos, scènes, préréglages perso) : les divisions au tempo
    commencent à 16 et 8 mesures (deux choix insérés avant « 4 mes. »). Vrai pour une liste
    de choix concernée. */
inline bool hasLongDivisions (const ParamDesc& d) noexcept
{
    if (d.choices.size() < 4) return false;
    const auto at = d.choices[0] == "16 mes." ? 0 : (d.choices[1] == "16 mes." ? 1 : -1);
    return at >= 0 && d.choices[(size_t) at + 1] == "8 mes.";
}
/** Choix d'avant la version 7 (index) → choix d'aujourd'hui (même division). */
inline int widenDivisionIndex (const ParamDesc& d, int oldIndex) noexcept
{
    const bool freeFirst = d.choices[0] != "16 mes."; // « Libre » d'abord : il garde sa place
    if (freeFirst && oldIndex <= 0) return 0;
    return std::clamp (oldIndex + 2, 0, (int) d.choices.size() - 1);
}

} // namespace grv::engine
