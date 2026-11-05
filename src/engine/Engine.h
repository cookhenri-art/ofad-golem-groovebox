/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Moteur du groovebox, en C++ pur (testable sans JUCE).

      16 pistes ──► instrument ──► 4 inserts ──► volume / panoramique ──┬──► bus principal
                                                                       └──► envois A–D ──► retours A–D (4 inserts) ──► bus principal
      Bus principal ──► 4 inserts ──► volume ──► sortie

    Réglages : un tableau unique d'adresses (voir Params.h). Chaque bloc de contrôle (32 échantillons),
    la valeur jouée de chaque réglage est : valeur de base → verrou du pas en cours → scènes A/B au
    crossfader → scène Break → modulations. Tout ce qui est structurel (instruments, effets,
    assignations de modulation, scènes, conditions et verrous par pas) est publié sans verrou
    (Published.h) ; le fil audio n'alloue jamais.
*/

#pragma once

#include "Module.h"
#include "Arpeggiator.h"
#include "../dsp/Loudness.h"
#include "../dsp/ScopeRing.h"
#include "../dsp/EqFilters.h"
#include "ModSources.h"
#include "Published.h"
#include "Sequencer.h"
#include "Song.h"
#include "Speech.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace grv::engine
{

constexpr int numScenes = 32;
constexpr int breakScene = 32; // scène appliquée tant que Break est enfoncé
constexpr int controlBlock = 32;

const ModuleDesc& mixerDesc (int channel);

struct ModAssign
{
    int source;        // 0..5 = sources du canal, 6 = vélocité de la dernière note
    int target;        // emplacement local du réglage modulé
    int amountIndex;   // 0..63 : la profondeur vit dans le réglage modAmountBase + amountIndex
};

struct ChannelGraph
{
    bool used = false;
    Instrument* instrument = nullptr;
    std::array<Effect*, numInserts> inserts {};
    std::array<std::array<Effect*, padInserts>, numPads> padFx {}; // inserts par pad (batteries)
    bool anyPadFx = false;
    std::vector<ModAssign> mods;
    std::array<const ParamDesc*, slotsPerChannel> desc {};
    std::array<const SampleBuffer*, maxSamples> samples {}; // échantillons de la piste (pads, sampler ; 128 : Pieuvre 8)
    const SpeechScript* speech = nullptr;           // v0.33 : phrase tapée (voix interne des vocodeurs)
    int usedSlots = 0; // emplacements à recalculer (jusqu'au dernier décrit)
    // plages d'emplacements décrits (mixage + instrument, chaque insert, modulation) : seules
    // celles-ci sont recopiées à chaque bloc de contrôle
    std::array<std::pair<int, int>, 8 + numPads * padInserts> ranges {};
    int numRanges = 0;
    int arpParam = -1; // réglage « arp_mode » de l'instrument (arpégiateur de piste), -1 sans
    int arpOnParam = -1; // v0.43.6 : réglage « arp_on » (marche / arrêt, le mode reste choisi), -1 sans
    int glideParam = -1; // v0.46 fix27 : réglage de portamento de l'instrument (« glide » ou « porta »), -1 sans
};

struct Graph
{
    std::array<ChannelGraph, numChannels> ch;
    // v0.26 : contenu de chaque emplacement de piste (jeton stable) ; quand il change, le fil
    // audio déplace l'état des pistes (lissages, arpège, position de lecture) avec elles
    std::array<std::uint8_t, numTracks> owner {};
};

struct SceneLock { int slot; float norm; };
struct SceneSet
{
    std::array<std::vector<SceneLock>, numScenes + 1> scenes; // triées par adresse
    std::array<bool, numScenes + 1> muted {};
};

/** Événement d'entrée d'un bloc (pads, MIDI, interface). Les commandes d'expression portent leur
    valeur dans velocity : pitch bend -1..1, molette 0..1, pression 0..1, pédale 0 ou 1. */
struct InputEvent
{
    enum Type : int { noteOn, noteOff, pitchBend, modWheel, pressure, sustain, recordAction } type; // recordAction : note = action
    int track, note, offset;
    float velocity;
    bool record;
};

/** Description d'une piste côté interface (fil de l'interface). */
struct TrackInfo
{
    std::string name;
    std::string instrumentType;          // vide = piste libre
    std::array<std::string, numInserts> insertTypes;
    std::array<std::string, numInserts> insertPresets;
    std::string preset;
    int color = 0;
    int output = 0;                      // v0.24 : 0 = bus principal, 1 à 4 = bus de groupe
    std::string speech;                  // v0.33 : phrase tapée de la voix interne (vocodeurs)
};

/** v0.32 : prise de son du mixage (enregistrement audio). Appelée depuis le fil audio, à la fin
    de chaque bloc de calcul (≤ 32 échantillons) : sortie principale, sortie de chaque piste
    (après effets et volume) et sortie de chaque retour. Ne doit ni allouer, ni attendre. */
struct AudioTap
{
    virtual ~AudioTap() = default;
    /** Vrai si les sorties des pistes sont demandées (pistes séparées). */
    virtual bool wantsStems() const noexcept = 0;
    /** v0.46 fix8 : position du bloc qui suit (noires, début et fin) et lecture en cours, juste avant tapBlock. */
    virtual void tapPosition (double /*ppq*/, double /*ppqEnd*/, bool /*playing*/) noexcept {}
    /** v0.46.2 : le morceau finit à cette position (noires), dans le bloc qui suit. */
    virtual void tapStopAt (double /*ppq*/) noexcept {}
    /** tracksL/R : numTracks pointeurs (une piste non jouée dans ce bloc : mask à 0, données à ignorer). */
    virtual void tapBlock (const float* outL, const float* outR,
                           const float* const* tracksL, const float* const* tracksR, std::uint32_t trackMask,
                           const float* const* returnsL, const float* const* returnsR, int n) noexcept = 0;
};

/** v0.46 fix8 : MONTAGE AUDIO. En mode montage, les clips enregistrés remplacent la sortie des
    pistes (après effets et volume, comme ils ont été enregistrés) et celle des retours ; les
    groupes et le bus principal restent calculés en direct. Appelé depuis le fil audio pour chaque
    bloc de calcul (≤ controlBlock échantillons). Ne doit ni allouer, ni attendre, ni lire le disque. */
constexpr int numMontageLanes = numTracks + numReturns;
struct MontageFeed
{
    virtual ~MontageFeed() = default;
    /** Mode montage allumé (les clips jouent à la place des instruments). */
    virtual bool montageActive() const noexcept = 0;
    /** Remplit les voies (pistes puis retours) pour n échantillons ; à l'arrêt, du silence.
        laneMask : bit à 1 pour chaque voie qui a du son dans ce bloc (les autres sont à zéro). */
    virtual void pull (bool playing, int n, float (*laneL)[controlBlock], float (*laneR)[controlBlock], std::uint32_t& laneMask) noexcept = 0;
};

class Engine
{
public:
    Engine();
    ~Engine();

    //==========================================================================
    // Fil de l'interface

    void prepare (double sampleRate, int maxBlock);
    double getSampleRate() const noexcept { return sampleRate; }

    /** Met un instrument sur une piste (type vide = piste libre). Remet ses réglages d'origine. */
    bool setInstrument (int track, const std::string& type, double nowMs);
    /** v0.26 : déplace les pistes (la piste t passe en newIndex[t]) avec leurs instruments, effets,
        inserts de pad, échantillons, réglages, modulations, patterns et état de jeu : une boucle en
        cours continue, rien n'est recréé. Fil de l'interface. Faux si ce n'est pas une permutation. */
    bool permuteTracks (const std::array<int, numTracks>& newIndex, double nowMs);
    /** Met un effet dans un insert d'un canal (type vide = emplacement libre). */
    bool setInsert (int channel, int slot, const std::string& type, double nowMs);
    void moveInsert (int channel, int from, int to, double nowMs);
    /** v0.46 : correspondance des adresses quand deux inserts d'un canal échangent leur place (réglages, interrupteur). */
    static std::function<int (int)> insertSwapMap (int channel, int from, int to);
    /** Met un effet dans un insert d'un pad de batterie (type vide = libre). */
    bool setPadInsert (int track, int pad, int slot, const std::string& type, double nowMs);
    const std::string& getPadInsertType (int track, int pad, int slot) const
    { return padInsertTypes[(size_t) std::clamp (track, 0, numTracks - 1)][(size_t) std::clamp (pad, 0, numPads - 1)][(size_t) std::clamp (slot, 0, padInserts - 1)]; }
    const ModuleDesc* padEffectDesc (int track, int pad, int slot) const noexcept
    {
        if (track < 0 || track >= numTracks || pad < 0 || pad >= numPads || slot < 0 || slot >= padInserts) return nullptr;
        const auto& e = padEffects[(size_t) track][(size_t) pad][(size_t) slot];
        return e ? &e->desc() : nullptr;
    }
    /** Module d'un emplacement : -1 instrument, 0..3 insert, 100 + pad × 2 + n : insert de pad. */
    const ModuleDesc* moduleAt (int channel, int slot, int& baseSlot) const noexcept;

    void setTrackPresetName (int track, const std::string& name) { if (track >= 0 && track < numTracks) info[(size_t) track].preset = name; }
    void setTrackName (int track, const std::string& name) { if (track >= 0 && track < numTracks) info[(size_t) track].name = name; }
    /** v0.33 : phrase tapée de la voix interne (160 octets au plus) ; vide = phrase de démonstration. */
    void setTrackSpeech (int track, const std::string& text, double nowMs);
    void setTrackColor (int track, int color) { if (track >= 0 && track < numTracks) info[(size_t) track].color = color; }
    /** Sortie de la piste (v0.24) : 0 = bus principal, 1 à numGroups = bus de groupe. Tout fil. */
    void setTrackOutput (int track, int output) noexcept
    {
        if (track < 0 || track >= numTracks) return;
        output = std::clamp (output, 0, numGroups);
        info[(size_t) track].output = output;
        trackOutput[(size_t) track].store (output, std::memory_order_relaxed);
    }
    int getTrackOutput (int track) const noexcept { return track >= 0 && track < numTracks ? trackOutput[(size_t) track].load (std::memory_order_relaxed) : 0; }
    /** v0.42 : piste interne (ligne) jouée au clavier et en MIDI sur un instrument à pistes internes. */
    /** v0.43 : écoute d'une partie d'un échantillon de la piste (fil de l'interface). */
    void previewSample (int track, int index, float a, float b) noexcept
    { if (track >= 0 && track < numTracks && instruments[(size_t) track] != nullptr) instruments[(size_t) track]->preview (index, a, b); }
    void touchInstrument (int track, int kind, float a, float b) noexcept // v0.47
    { if (track >= 0 && track < numTracks && instruments[(size_t) track] != nullptr) instruments[(size_t) track]->touch (kind, a, b); }
    void setPlayLane (int track, int lane) noexcept { if (track >= 0 && track < numTracks) playLane[(size_t) track].store (std::clamp (lane, 0, 15), std::memory_order_relaxed); }
    int getPlayLane (int track) const noexcept { return track >= 0 && track < numTracks ? playLane[(size_t) track].load (std::memory_order_relaxed) : 0; }

    /** v0.31 : coupure et solo des retours et des groupes (canaux 16 et plus, sauf le principal),
        noms des groupes. Lus par le fil audio (atomiques). Un retour n'est pas touché par les solos. */
    static bool isBus (int ch) noexcept { return ch >= numTracks && ch < numChannels && ch != masterChannel; }
    void setBusMute (int ch, bool on) noexcept { if (isBus (ch)) busMute[(size_t) ch].store (on, std::memory_order_relaxed); }
    void setBusSolo (int ch, bool on) noexcept { if (isBus (ch) && ch >= groupBase) busSolo[(size_t) ch].store (on, std::memory_order_relaxed); }
    bool getBusMute (int ch) const noexcept { return isBus (ch) && busMute[(size_t) ch].load (std::memory_order_relaxed); }
    bool getBusSolo (int ch) const noexcept { return isBus (ch) && busSolo[(size_t) ch].load (std::memory_order_relaxed); }
    void setGroupName (int k, const std::string& name) { if (k >= 0 && k < numGroups) groupNames[(size_t) k] = name; }
    const std::string& getGroupName (int k) const { static const std::string none; return k >= 0 && k < numGroups ? groupNames[(size_t) k] : none; }
    const TrackInfo& getTrackInfo (int track) const { return info[(size_t) std::clamp (track, 0, numTracks - 1)]; }
    const std::array<std::string, numInserts>& getInsertTypes (int channel) const { return insertTypes[(size_t) std::clamp (channel, 0, numChannels - 1)]; }
    /** Dernier préréglage appliqué à chaque insert (affichage, sauvegarde). */
    const std::array<std::string, numInserts>& getInsertPresets (int channel) const { return insertPresetNames[(size_t) std::clamp (channel, 0, numChannels - 1)]; }
    void setInsertPresetName (int channel, int slot, const std::string& name)
    { if (channel >= 0 && channel < numChannels && slot >= 0 && slot < numInserts) insertPresetNames[(size_t) channel][(size_t) slot] = name; }

    /** État de l'instrument d'une piste pour l'interface (fil de l'interface). */
    int instrumentStatus (int track, float* out, int max) const noexcept
    {
        return track >= 0 && track < numTracks && instruments[(size_t) track] ? instruments[(size_t) track]->status (out, max) : 0;
    }
    /** Instrument d'une piste (fil de l'interface : lecture de son état seulement). */
    const Instrument* instrumentAt (int track) const noexcept
    {
        return track >= 0 && track < numTracks ? instruments[(size_t) track].get() : nullptr;
    }

    /** Description de l'instrument d'une piste ou de l'effet d'un insert (nullptr si vide). */
    const ModuleDesc* instrumentDesc (int track) const noexcept
    {
        return track >= 0 && track < numTracks && instruments[(size_t) track] ? &instruments[(size_t) track]->desc() : nullptr;
    }
    const ModuleDesc* effectDesc (int channel, int slot) const noexcept
    {
        if (channel < 0 || channel >= numChannels || slot < 0 || slot >= numInserts) return nullptr;
        const auto& e = effects[(size_t) channel][(size_t) slot];
        return e ? &e->desc() : nullptr;
    }

    /** Chaîne d'effets (catalogue) : remplace les 4 inserts du canal. */
    bool applyChain (int channel, const std::string& chainName, double nowMs);

    /** Préréglage d'un module (instrument : slot = -1 ; insert : 0..3). */
    bool applyPreset (int channel, int slot, const std::string& presetName);

    /** Réglages d'un module (slot -1 : instrument, 0..3 : insert) en valeurs lisibles, rangés par
        identifiant (préréglages de l'utilisateur). prefix non vide : seulement les réglages qui
        commencent par prefix (un pad : "p05_"), rendus sans le préfixe. */
    std::vector<std::pair<std::string, float>> moduleValues (int channel, int slot, const std::string& prefix = {}) const;

    /** Applique des valeurs lisibles rangées par identifiant. Les réglages absents reprennent leur
        valeur d'origine (un préréglage décrit tout le son). prefix : voir moduleValues.
        presetName non vide : nom affiché du préréglage du module. Faux si le module n'existe pas. */
    bool applyValues (int channel, int slot, const std::vector<std::pair<std::string, float>>& values,
                      const std::string& prefix, const std::string& presetName);

    // Réglages (valeurs normalisées 0..1)
    void setParam (int global, float norm) noexcept;
    float getParam (int global) const noexcept;
    float getEffective (int global) const noexcept;
    const ParamDesc* describe (int global) const noexcept;
    /** Adresse d'un réglage par identifiant de module (zone "inst", "fx1".."fx4", "mix", "mod1".."mod6"). */
    int findParam (int channel, const std::string& area, const std::string& id) const;
    void resetParams (int channel, int from, int count) noexcept;

    // Modulations
    int addModulation (int channel, int source, int target, float amount, double nowMs);
    void removeModulation (int channel, int target, double nowMs);
    std::vector<ModAssign> getModulations (int channel) const;

    // Scènes
    void setSceneLock (int scene, int global, float norm, double nowMs);
    void clearSceneLock (int scene, int global, double nowMs);
    void clearScene (int scene, double nowMs);
    void setSceneMuted (int scene, bool muted, double nowMs);
    const SceneSet& getScenes() const noexcept { return *scenes.editable(); }
    /** Remplace toutes les scènes d'un coup (retour en arrière) : une seule publication. */
    void replaceScenes (std::unique_ptr<SceneSet> s, double nowMs);

    // Conditions et verrous par pas
    void setStepExtra (int track, int lane, int step, Condition c, int a, int b, int prob, double nowMs);
    void setStepLock (int track, int lane, int step, int global, float norm, double nowMs);
    /** Note d'arpège enregistrée : rejouée sans repasser par l'arpégiateur. */
    void setStepDirectP (int track, int pattern, int lane, int step, bool direct, double nowMs);
    void clearStepLock (int track, int lane, int step, int global, double nowMs);
    void clearStepExtras (int track, int lane, int step, double nowMs);
    void clearTrackExtras (int track, double nowMs);
    // Mêmes fonctions pour un pattern donné (sans pattern : le pattern édité de la piste)
    void setStepExtraP (int track, int pattern, int lane, int step, Condition c, int a, int b, int prob, double nowMs);
    void setStepLockP (int track, int pattern, int lane, int step, int global, float norm, double nowMs);
    /** Répétitions d'un pas (1 à 8) et pente de vélocité (-100 à 100 %). */
    void setStepRatchetP (int track, int pattern, int lane, int step, int count, int slope, double nowMs);
    void setStepRatchet (int track, int lane, int step, int count, int slope, double nowMs)
    { setStepRatchetP (track, seq->getEditing (track), lane, step, count, slope, nowMs); }
    /** Répétitions sur plusieurs pas du pattern édité. Une seule publication. */
    void setStepsRatchet (int track, const std::vector<std::pair<int, int>>& cells, int count, int slope, double nowMs);
    /** Flam (0 = sans, sinon écart en ms) sur des pas d'un pattern (pattern < 0 : le pattern édité). */
    void setStepsFlam (int track, int pattern, const std::vector<std::pair<int, int>>& cells, int ms, double nowMs);
    void clearStepLockP (int track, int pattern, int lane, int step, int global, double nowMs);
    void clearStepExtrasP (int track, int pattern, int lane, int step, double nowMs);

    /** Un pas écrit par une édition groupée : note, et ses conditions et verrous. */
    struct CellWrite
    {
        int lane = 0, step = 0, vel = 0, note = 60, len8 = 8, micro = 0;
        bool hasExtra = false;
        StepExtra extra;
    };
    /** Édition groupée du pattern édité d'une piste (sélection : déplacer, copier, coller,
        dupliquer, effacer, régler plusieurs pas). Les pas de clear sont vidés (note, condition,
        verrous), puis les pas de writes sont écrits. Conditions et verrous : une seule publication. */
    void editCells (int track, const std::vector<std::pair<int, int>>& clear, const std::vector<CellWrite>& writes, double nowMs);
    /** Verrou d'un réglage sur plusieurs pas du pattern édité (clear : le retirer). Une seule publication. */
    void setStepLocks (int track, const std::vector<std::pair<int, int>>& cells, int global, float norm, bool clear, double nowMs);
    void clearPatternExtras (int track, int pattern, double nowMs);
    /** v0.31 : recopie conditions et verrous des pas from..from+count-1 vers to..to+count-1 du
        même pattern (toutes les lignes) ; les pas de destination sont remplacés. Une seule publication. */
    void copyStepRangeExtras (int track, int pattern, int from, int to, int count, double nowMs);
    /** v0.46 fix16 : conditions, répétitions, flams et verrous suivent un décalage du pattern
        (Sequencer::shiftPattern, mêmes règles : en boucle sur la longueur de chaque ligne). */
    void shiftPatternExtras (int track, int pattern, int lane, int dir, double nowMs);
    void copyPatternExtras (int track, int from, int to, double nowMs) { copyPatternExtras (track, from, track, to, nowMs); }
    /** Vers une autre piste : les verrous des réglages de la piste source suivent sur la piste de
        destination quand le réglage y existe (même identifiant), sinon ils sont retirés. */
    void copyPatternExtras (int srcTrack, int from, int track, int to, double nowMs);
    const PatternExtras& getExtras() const noexcept { return *extras.editable(); }
    /** Remplace conditions et verrous d'un coup (retour en arrière) : une seule publication. */
    void replaceExtras (std::unique_ptr<PatternExtras> e, double nowMs);

    // Échantillons (batteries : index = pad ; sampler : 0)
    void setSample (int track, int index, std::unique_ptr<SampleBuffer> sample, double nowMs);
    const SampleBuffer* getSample (int track, int index) const noexcept;

    /** Regroupe plusieurs changements de structure en une seule publication (chargement d'un projet). */
    void beginBatch() noexcept { ++batchDepth; }
    void endBatch (double nowMs)
    {
        if (batchDepth <= 0 || --batchDepth > 0) return;
        bin.restamp (nowMs);
        if (batchDirty) { batchDirty = false; publishGraph (nowMs); }
    }

    /** Ménage de la corbeille (appeler régulièrement). */
    void collectGarbage (double nowMs) { bin.collect (nowMs); }

    Sequencer& sequencer() noexcept { return *seq; }
    const Sequencer& sequencer() const noexcept { return *seq; }

    std::uint32_t getStructureVersion() const noexcept { return structureVersion.load(); }
    /** Change à chaque modification des scènes (sauvegarde automatique, historique). */
    std::uint32_t getSceneVersion() const noexcept { return sceneVersion.load(); }
    std::uint32_t getParamVersion (int channel) const noexcept { return paramVersion[(size_t) channel].load (std::memory_order_relaxed); }

    // Fil de l'interface : notes jouées à la main (pads de l'interface, clavier d'ordinateur)
    bool pushEvent (const InputEvent& e) noexcept { return uiEvents.push (e); }

    /** Piste choisie dans l'interface : reçoit les notes MIDI. */
    std::atomic<int> focusTrack { 0 };
    std::array<std::atomic<float>, numTracks> bendShown {}, wheelShown {};
    /** v0.32 : prise de son (enregistrement audio) ; nullptr = aucune. Posée et retirée par le fil
        de l'interface ; l'appelant attend la fin du bloc en cours avant de détruire la prise. */
    std::atomic<AudioTap*> audioTap { nullptr };
    /** v0.46 fix8 : lecteur du montage audio (nullptr = aucun). Posé une fois par le processeur. */
    std::atomic<MontageFeed*> montageFeed { nullptr };
    /** v0.32.1 : molettes de la piste (hauteur -1..1, modulation 0..1) telles que le moteur les
        applique, pour que l'interface les montre aussi quand elles viennent du contrôleur MIDI. */
    float getBendShown (int track) const noexcept { return track >= 0 && track < numTracks ? bendShown[(size_t) track].load (std::memory_order_relaxed) : 0.0f; }
    float getWheelShown (int track) const noexcept { return track >= 0 && track < numTracks ? wheelShown[(size_t) track].load (std::memory_order_relaxed) : 0.0f; }

    /** Vrai si la piste porte une batterie (16 sons sur les pads). Tout fil. */
    bool isDrumTrack (int track) const noexcept
    {
        if (track < 0 || track >= numTracks) return false;
        const auto* g = graph.get();
        const auto* inst = g != nullptr ? g->ch[(size_t) track].instrument : nullptr;
        return inst != nullptr && inst->desc().drumKit;
    }
    /** v0.46 : vrai si les notes MIDI de la piste jouent ses 16 pads (notes 36 à 51) — une batterie,
        mais pas Pieuvre 8, qui se joue au clavier comme un instrument (piste interne choisie). Tout fil. */
    bool isPadNoteTrack (int track) const noexcept
    {
        if (track < 0 || track >= numTracks) return false;
        const auto* g = graph.get();
        const auto* inst = g != nullptr ? g->ch[(size_t) track].instrument : nullptr;
        return inst != nullptr && inst->desc().drumKit && inst->desc().type != "octa";
    }

    /** Sources de modulation d'un canal à leur réglage d'origine : LFO, LFO, fonction (enveloppe),
        fonction, suiveur d'enveloppe, séquenceur de modulation. */
    void applyModSourceDefaults (int channel) noexcept;

    /** Remet les réglages d'un module à leurs valeurs d'origine. */
    void applyDefaults (int channel, int base, const ModuleDesc& d) noexcept;

    //==========================================================================
    // Morceau (arrangeur) — fil de l'interface

    const Song& getSong() const noexcept { return *song.editable(); }
    /** Remplace le morceau (rangées bornées à 128, courbes à 64 de 4 096 points). */
    void setSong (Song next, double nowMs);
    std::uint32_t getSongVersion() const noexcept { return songVersion.load(); }
    /** Enregistrer le jeu : les lancements deviennent des rangées, mesure par mesure. */
    void setRecordPlay (bool on) noexcept { if (on && ! recPlay.load()) { capRow = -1; capLastBar = -1; capOffsetOk = false; } recPlay.store (on); }
    bool getRecordPlay() const noexcept { return recPlay.load(); }
    /** Enregistrer les mouvements : les réglages bougés deviennent des courbes. */
    void setRecordMoves (bool on) noexcept { recMoves.store (on); }
    bool getRecordMoves() const noexcept { return recMoves.load(); }
    /** Les gestes s'enregistrent-ils dans le morceau ? (« Enr. les mouvements », ou ENR. en mode morceau) */
    bool isSongRecording() const noexcept;
    /** Un réglage bougé à l'écran (fil de l'interface) ou au contrôleur (fil audio). */
    void recordMoveUi (int global, float norm);
    void recordMoveAudio (int global, float norm) noexcept;
    /** v0.46 : coupure / solo d'une piste changés à la main : enregistrés dans le morceau (« Enr. les
        mouvements ») et la courbe se tait le temps du geste. Fil de l'interface / fil audio. */
    /** Coupure / solo enregistrés dans le morceau (« Enr. les mouvements ») ; track : canal (piste, retour, groupe). */
    void recordSwitchUi (int track, bool solo, bool on);
    void recordSwitchAudio (int track, bool solo, bool on) noexcept;
    /** v0.46 : crossfader des scènes bougé à la main (écran ou contrôleur) : enregistré dans le morceau. */
    void recordFaderUi (float position);
    void recordFaderAudio (float position) noexcept;
    /** Position du crossfader imposée par sa courbe du morceau (-1 : aucune). Tout fil. */
    float getLaneFader() const noexcept { return laneFader.load (std::memory_order_relaxed); }
    /** À appeler régulièrement : range les mesures et mouvements enregistrés dans le morceau. */
    void songTick (double nowMs);
    /** Courbes de pattern : effacées (pattern -1 : tous ceux de la piste), copiées vers un autre
        pattern ou une autre piste (le même réglage s'il y existe). Fil de l'interface. */
    void clearPatternLanes (int track, int pattern, double nowMs);
    void copyPatternLanes (int srcTrack, int from, int track, int to, double nowMs);
    /** Enregistrement des patterns (ENR.) : les réglages des pistes bougés pendant la lecture
        deviennent des courbes lisses du pattern joué (Song::patLanes) ; les notes sorties de
        l'arpégiateur sont écrites telles qu'on les entend. Rangés par songTick(). */
    struct ArpMark { int track = -1, pattern = 0, lane = 0, pos = 0; };
    /** Scène à lancer demandée par une rangée (−1 : rien). */
    int takeSongScene() noexcept { return songSceneOut.exchange (-1); }
    /** Rangée jouée et position dans le morceau (pour l'affichage). */
    int getSongRow() const noexcept { return songRowShown.load (std::memory_order_relaxed); }
    double getSongBeat() const noexcept { return songBeatShown.load (std::memory_order_relaxed); }

    //==========================================================================
    // Fil audio

    struct BlockInfo
    {
        HostTime host;
        double bpm = 120.0;
        double swing = 50.0;
        float fader = 0.0f;   // 0 = tout A, 1 = tout B
        int sceneA = 0, sceneB = 8;
        bool breakActive = false;
        // v0.46 fix22 : pavé vectoriel (principe des synthés vectoriels, appliqué à tout le mix) :
        // quatre coins A B C D, poids des coins (somme 1). Mode 0 : quatre scènes mêlées (remplace
        // le crossfader A / B) ; mode 1 : quatre effets du bus principal dosés (son direct / traité).
        bool vecOn = false;
        int vecMode = 0;
        std::array<float, 4> vecW { 0.25f, 0.25f, 0.25f, 0.25f };
        std::array<int, 4> vecScene { 0, 1, 2, 3 };  // scènes 0..31
        std::array<int, 4> vecFx { 1, 2, 3, 4 };     // insert du bus principal 1..4 (0 : aucun)
    };

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples,
                  const BlockInfo& info, const InputEvent* events, int numEvents) noexcept;

    // Lecture par l'interface
    std::uint32_t getHitCount (int track, int lane) const noexcept { return hits[(size_t) track][(size_t) lane].load (std::memory_order_relaxed); }
    /** Notes jouées par note exacte (clavier et pads mélodiques allumés) : tenue en ce moment, et compteur de coups. */
    /** Position de la piste dans son pattern (pas, avec la fraction ; -1 à l'arrêt). Tout fil. */
    float getTrackPhase (int track) const noexcept { return track >= 0 && track < numTracks ? trackPhase[(size_t) track].load (std::memory_order_relaxed) : -1.0f; }
    bool isNoteDown (int track, int note) const noexcept
    { return track >= 0 && track < numTracks && note >= 0 && note < 128 && ((noteDown[(size_t) track][(size_t) (note >> 6)].load (std::memory_order_relaxed) >> (note & 63)) & 1u) != 0; }
    std::uint32_t getNoteHits (int track, int note) const noexcept
    { return track >= 0 && track < numTracks && note >= 0 && note < 128 ? noteHits[(size_t) track][(size_t) note].load (std::memory_order_relaxed) : 0; }
    float getPeak (int channel) const noexcept { return peaks[(size_t) channel].load (std::memory_order_relaxed); }
    /** v0.46 : crêtes gauche et droite de la sortie principale (le panoramique se voit sur le bus principal). */
    float getOutPeak (int side) const noexcept { return (side == 0 ? outPeakL : outPeakR).load (std::memory_order_relaxed); }
    /** Valeur et pas en cours d'une source de modulation (affichage, tout fil). */
    float getModValue (int channel, int source) const noexcept
    { return channel >= 0 && channel < numChannels && source >= 0 && source < numModSources ? modLive[(size_t) channel][(size_t) source].load (std::memory_order_relaxed) : 0.0f; }
    int getModStep (int channel, int source) const noexcept
    { return channel >= 0 && channel < numChannels && source >= 0 && source < numModSources ? modLiveStep[(size_t) channel][(size_t) source].load (std::memory_order_relaxed) : 0; }
    float getEffectMeter (int channel, int slot, int index) const noexcept;
    /** Analyseur d'un insert (fil de l'interface) : derniers échantillons de sortie, 0 si l'effet n'en a pas. */
    int effectAnalysis (int channel, int slot, float* dst, int count) const noexcept
    {
        if (channel < 0 || channel >= numChannels || slot < 0 || slot >= numInserts) return 0;
        const auto& e = effects[(size_t) channel][(size_t) slot];
        return e ? e->analysis (dst, count) : 0;
    }
    /** Mesures du bus principal : tranches de 100 ms (intensité, crête vraie, efficace). Fil de l'interface. */
    bool popLoudness (dsp::LoudnessBlock& b) noexcept { return loudness.pop (b); }
    /** v0.45.1 : derniers échantillons de la sortie principale (analyseur de spectre). */
    const dsp::ScopeRing& outputScope() const noexcept { return scope; }

private:
    /** Valeur de base d'un réglage, et valeur jouée affichée en attendant le prochain bloc (sinon
        l'interface montrerait l'ancienne valeur jouée : préréglages, valeurs d'origine). */
    void storeBase (int global, float norm) noexcept
    {
        base[(size_t) global].store (norm, std::memory_order_relaxed);
        effective[(size_t) global].store (norm, std::memory_order_relaxed);
    }
    void publishGraph (double nowMs);
    void publishScenes (std::unique_ptr<SceneSet>, double nowMs);
    void publishExtras (std::unique_ptr<PatternExtras>, double nowMs);

    // Fil audio
    struct ChannelState
    {
        std::array<float, slotsPerChannel> plain {};
        std::array<float, slotsPerChannel> eff {};
        std::array<float, slotsPerChannel> overrideNorm {};
        std::array<std::uint8_t, slotsPerChannel> touched {};
        std::array<int, slotsPerChannel> touchedList {};
        int numTouched = 0;
        std::array<int, slotsPerChannel> prevTouched {};
        int numPrevTouched = 0;
        std::uint32_t seenVersion = 0xFFFFFFFFu;
        const Instrument* seenInstrument = nullptr;
        std::array<ModSource, numModSources> mods;
        std::array<float, numModSources + 1> modValues {};
        float level = 0.0f, lastVelocity = 0.0f;
        float gainL = 1.0f, gainR = 1.0f;
        float groupScale = 1.0f;               // v0.24 (bus de groupe) : volume du groupe appliqué aux envois de ses pistes
        float muteGain = 1.0f;                 // coupure / solo de la piste, en rampe de 6 ms (le son s'arrête, traînes comprises)
        std::array<float, numInserts> fxOn { 1.0f, 1.0f, 1.0f, 1.0f }; // v0.46 : insert actif (1) / contourné (0), en fondu de 6 ms
        grv::dsp::SvfEqStereo eqLow, eqMid, eqHigh; // égaliseur de la tranche (rien n'est calculé à plat)
        bool eqOn = false;
        std::array<float, numReturns> sendGain {};
        // verrous du dernier pas déclenché sur chaque ligne, valables jusqu'au pas suivant de la ligne
        std::array<std::array<SceneLock, 32>, 16> plocks {};
        std::array<int, 16> numPlocks {};
        int anyPlocks = 0;
        // glissé des verrous (pas avec la fonction « glissé ») : valeur de départ, durée, avancement
        std::array<std::array<float, 32>, 16> plockFrom {};
        std::array<double, 16> slideLen {}, slidePos {};
        // échantillons transmis à l'instrument (synchronisés avec le graphe à chaque bloc)
        const Instrument* sampleOwner = nullptr;
        std::array<const SampleBuffer*, maxSamples> appliedSamples {};
        const SpeechScript* appliedSpeech = nullptr;
        // jeu : expression, pédale de maintien, arpégiateur
        float bend = 0.0f, wheel = 0.0f, pressure = 0.0f;
        bool sustainDown = false, arpOn = false;
        std::array<bool, 128> direct {}, sustained {}; // notes envoyées sans l'arpège ; notes tenues par la pédale
        Arpeggiator arp;
        const Instrument* arpOwner = nullptr;
    };

    struct PendingOff { int track, note; double remaining; };

    void touch (ChannelState& c, int local, float norm) noexcept;
    void updateParams (const Graph& g, const SceneSet& s, const BlockInfo& bi, float fader, int n, double ppq) noexcept;
    void renderChunk (const Graph& g, float* outL, float* outR, int n, double bpm, double ppq, bool playing) noexcept;
    void dispatch (const Graph& g, int track, int note, float velocity, bool on, bool direct = false) noexcept;
    bool arpActive (const Graph& g, int track) const noexcept;
    void play (const Graph& g, int track, int note, float velocity, bool on) noexcept; // directement à l'instrument
    void runArpeggiators (const Graph& g, double ppq, bool playing, double bpm, int n, int pos) noexcept;
    void expression (const Graph& g, int track, int kind, float value) noexcept;

    double sampleRate = 48000.0;
    int maxBlockSize = 512;

    std::unique_ptr<Sequencer> seq;
    std::unique_ptr<std::atomic<float>[]> base;
    std::unique_ptr<std::atomic<float>[]> effective;
    std::array<std::atomic<std::uint32_t>, numChannels> paramVersion {};
    std::atomic<std::uint32_t> structureVersion { 0 };

    RetireBin bin;
    Published<Graph> graph;
    Published<SceneSet> scenes;
    Published<PatternExtras> extras;

    std::atomic<std::uint32_t> sceneVersion { 0 };
    int batchDepth = 0;
    bool batchDirty = false;

    // Fil de l'interface : modules possédés
    std::array<std::unique_ptr<Instrument>, numTracks> instruments;
    std::array<std::array<std::unique_ptr<Effect>, numInserts>, numChannels> effects;
    std::array<std::vector<ModAssign>, numChannels> modAssigns;
    std::array<TrackInfo, numTracks> info;
    std::array<std::uint8_t, numTracks> slotOwner {}; // jetons des pistes (fil de l'interface, v0.26)
    std::array<std::atomic<int>, numTracks> trackOutput {}; // sortie lue par le fil audio (v0.24)
    std::array<std::atomic<bool>, numChannels> busMute {}, busSolo {}; // v0.31 : retours et groupes
    std::array<std::string, numGroups> groupNames;                       // v0.31 : vide = « Groupe n »
    std::array<std::array<std::string, numInserts>, numChannels> insertTypes, insertPresetNames;
    std::array<std::array<std::array<std::unique_ptr<Effect>, padInserts>, numPads>, numTracks> padEffects;
    std::array<std::array<std::array<std::string, padInserts>, numPads>, numTracks> padInsertTypes;
    std::array<std::array<std::unique_ptr<SampleBuffer>, maxSamples>, numTracks> samples;
    std::array<std::unique_ptr<SpeechScript>, numTracks> speechScripts; // v0.33

    SpscQueue<InputEvent, 1024> uiEvents;

    // Morceau
    Published<Song> song;
    std::atomic<std::uint32_t> songVersion { 0 };
    std::atomic<bool> recPlay { false }, recMoves { false };
    SpscQueue<BarCapture, 256> barCaptures;          // fil audio -> interface
    // grandes files et tampons alloués une fois à la construction (hors du fil audio) : le moteur
    // reste petit, même construit sur la pile (Windows : pile de 1 Mo)
    std::unique_ptr<SpscQueue<MoveRecord, 4096>> audioMoves = std::make_unique<SpscQueue<MoveRecord, 4096>>(); // fil audio -> interface
    std::vector<MoveRecord> uiMoves;                 // fil de l'interface
    std::unique_ptr<SpscQueue<ArpMark, 2048>> arpMarks = std::make_unique<SpscQueue<ArpMark, 2048>>(); // ENR. : notes d'arpège écrites (fil audio -> interface)
    void storeArpMarks (const std::vector<ArpMark>& marks, double nowMs);
    std::array<std::atomic<float>, numTracks> trackPhase {};  // fil audio -> interface : pas dans le pattern (-1 à l'arrêt)
    std::atomic<int> songSceneOut { -1 }, songRowShown { -1 };
    std::atomic<double> songBeatShown { -1.0 };
    // réglages tenus à la main pendant l'enregistrement : leur courbe n'est pas relue
    static constexpr int numHeld = 16;
    std::array<std::atomic<int>, numHeld> heldGlobal {};
    std::array<std::atomic<std::int64_t>, numHeld> heldUntil {};
    std::atomic<int> heldNext { 0 };
    std::atomic<std::int64_t> sampleClock { 0 };     // échantillons écoulés (fil audio)
    // interface : enregistrement en cours
    int capRow = -1;                                 // rangée en cours d'enregistrement (jeu en direct)
    long long capLastBar = -1;
    // v0.46 fix7 : « Enr. le jeu » : écart entre la position de la lecture et celle du morceau (la
    // première mesure enregistrée devient une rangée ajoutée au morceau). Les mouvements enregistrés
    // en même temps tombent ainsi au même endroit du morceau que la musique.
    double capOffset = 0.0;
    bool capOffsetOk = false;
    std::vector<double> laneLastBeat;                // par courbe : dernier point de la prise en cours
    std::vector<double> laneLastMs;
    double lastSongPublish = 0.0;
    bool songDirty = false;
    // fil audio
    int songRowNow = -1;
    bool songWasActive = false;
    long long lastCapBar = -1;
    bool songStoppedAtEnd = false;
    long long songKey = -1;      // tour × 128 + rangée lancée
    void songBlock (const Song& sg) noexcept;
    void captureBar() noexcept;
    void holdGlobal (int global) noexcept;
    bool isHeld (int global) const noexcept;
    void addMove (Song& sg, const MoveRecord& m, double nowMs);
    /** Courbes renvoyées vers d'autres réglages (effet déplacé) ou retirées (−1 : module remplacé). */
    void remapSongLanes (const std::function<int (int)>& f, double nowMs);

    // Fil audio
    std::unique_ptr<std::array<ChannelState, numChannels>> state;
    std::array<PendingOff, 2048> pendingOffs {}; // répétitions comprises : jamais de note-off perdu en pratique
    int numPendingOffs = 0;
    float smoothedFader = 0.0f;
    std::array<float, 4> smoothedVec { 0.25f, 0.25f, 0.25f, 0.25f }; // v0.46 fix22 : poids des coins, lissés
    bool vecScenesOn = false;                                       // pavé vectoriel en mode scènes (ce bloc)
    std::array<float, numInserts> vecFxTarget {};                   // mode effets : dosage de chaque insert du principal
    std::array<bool, numInserts> vecFxMapped {};                    // insert du principal piloté par le pavé
    std::array<float, numInserts> vecFxGain {};                     // dosage lissé
    std::atomic<float> laneFader { -1.0f }; // v0.46 : crossfader piloté par sa courbe
    double lastPpq = -1.0;
    bool wasRunning = false; // séquenceur en marche au bloc précédent (arrêt : notes relâchées)
    std::array<int, numTracks> trackWasPlaying {}; // pattern joué au bloc précédent, −1 = piste arrêtée
    // side-chain (v0.23) : sortie de chaque piste, bloc en cours et bloc précédent
    std::array<std::array<float, controlBlock>, numTracks> scCurL {}, scCurR {}, scPrevL {}, scPrevR {};
    std::array<bool, numTracks> scCurValid {}, scPrevValid {};
    std::array<bool, numTracks> absorbedNow {}; // piste prise par la suivante (voix d'un vocodeur, v0.25)
    std::array<std::uint8_t, numTracks> audioOwner {}; // contenu des pistes vu par le fil audio (v0.26)
    void adoptTrackOrder (const Graph& g) noexcept;
    std::array<float, controlBlock> scSilence {};
    std::array<std::array<std::atomic<std::uint32_t>, 16>, numTracks> hits {};
    std::array<std::atomic<int>, numTracks> playLane {}; // v0.42 : ligne jouée au clavier (Pieuvre 8)
    std::array<std::array<std::atomic<std::uint64_t>, 2>, numTracks> noteDown {};   // écrit par le fil audio seul
    std::array<std::array<std::atomic<std::uint32_t>, 128>, numTracks> noteHits {};
    std::array<bool, numTracks> arpRecording {};   // ENR. avec arpège : ce sont ses notes qui s'écrivent
    void setNoteDown (int track, int note, bool down) noexcept;
    std::array<std::atomic<float>, numChannels> peaks {};
    std::atomic<float> outPeakL { 0.0f }, outPeakR { 0.0f };
    std::array<std::array<std::atomic<float>, numModSources>, numChannels> modLive {};
    std::array<std::array<std::atomic<int>, numModSources>, numChannels> modLiveStep {};
    struct BlockEvent { int offset; int track; int lane; int note; float vel; bool on; double len; const StepExtra* extra; bool fromSeq; int kind = 0; std::uint8_t flags = 0; double stepLen = 0.0; bool repeat = false; float accent = 0.0f; double glideSamples = 0.0; };
    static constexpr int kindRecord = 100; // kind = kindRecord + action
    std::unique_ptr<std::array<BlockEvent, 4096>> blockEvents = std::make_unique<std::array<BlockEvent, 4096>>();
    dsp::LoudnessMeter loudness;
    dsp::ScopeRing scope;
};

} // namespace grv::engine
