/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Processeur audio : enveloppe JUCE du moteur (src/engine).
      - 16 pistes (un instrument au choix chacune), 4 inserts par canal, 4 retours, bus principal ;
      - séquenceur 16 pistes × 8 patterns × 16 lignes × 512 pas, lancement des patterns (seuls ou en
        rangée), conditions, verrous par pas, Fill et Break ;
      - apprentissage MIDI : un CC vers n'importe quel réglage, le crossfader, Break, Fill, Lecture ;
      - 32 scènes + scène Break, crossfader A/B sur tous les réglages ;
      - 6 sources de modulation par canal ;
      - réglages globaux et 8 macros exposés à l'automation du logiciel hôte ;
      - sauvegarde de tout dans le projet (structure, réglages par identifiant stable, séquences,
        scènes, échantillons).
*/

#pragma once

#include "engine/OfflineChain.h"
#include <deque>
#include <map>
#include <thread>

#include <JuceHeader.h>

#include "engine/Engine.h"
#include "Recorder.h"
#include "Montage.h"
#include "AndroidPerfHint.h"
#include "WifiRemote.h"

#include <algorithm>
#include <vector>

#include <array>
#include <atomic>

namespace grv
{

namespace ParamID
{
    inline const juce::String bpm    { "bpm" };
    inline const juce::String swing  { "swing" };
    inline const juce::String xfade  { "xfade" };  // 0 = tout A, 1 = tout B
    inline const juce::String sceneA { "sceneA" }; // 1..32
    inline const juce::String sceneB { "sceneB" }; // 1..32
    inline juce::String macro (int i) { return "macro" + juce::String (i + 1); }
    // v0.46 fix22 : pavé vectoriel (coins A B C D : scènes ou effets du principal)
    inline const juce::String vecOn    { "vecOn" };    // 0 / 1
    inline const juce::String vecMode  { "vecMode" };  // 0 scènes, 1 effets
    inline const juce::String vecX     { "vecX" };     // 0 = gauche (A, C), 1 = droite (B, D)
    inline const juce::String vecY     { "vecY" };     // 0 = haut (A, B), 1 = bas (C, D)
    inline const juce::String vecOrbit { "vecOrbit" }; // 0 arrêt, 1 cercle, 2 infini, 3 losange, 4 carré, 5 triangle, 6 ricochet, 7 hasard, 8 dessinée
    inline const juce::String vecRate  { "vecRate" };  // 0..8 : 1/2, 1, 2, 4, 8, 16, 32, 64, 128 mesures par tour
    inline const juce::String vecDepth { "vecDepth" }; // rayon du mouvement 0..1
    inline const juce::String vecHeight { "vecHeight" }; // v0.46 fix28 : hauteur de la forme (aplatir / étirer), 1 = d'origine
    inline const juce::String vecRot { "vecRot" };     // v0.46 fix28 : rotation de la forme du mouvement (degrés)
    inline juce::String vecScene (int k) { return juce::String ("vecS") + juce::String::charToString ((juce::juce_wchar) ('A' + k)); } // 1..32
    inline juce::String vecFx (int k) { return juce::String ("vecF") + juce::String::charToString ((juce::juce_wchar) ('A' + k)); }    // 0..4
}

class GrooveboxProcessor final : public juce::AudioProcessor,
                                 private juce::Timer
{
public:
    static constexpr int numMacros = 8;
    static constexpr int firstPadNote = 36; // C1 : convention des contrôleurs à pads

    enum class PadMode { play = 0, mute = 1, scenes = 2, solo = 3, steps = 4 }; // 1.0 : « Effacer » remplacé par « Solo »

    GrooveboxProcessor();
    ~GrooveboxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; } // après la 1.1 : pistes MIDI (appareils externes)
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    // Fil de l'interface

    engine::Engine& getEngine() noexcept { return engine; }

    /** Projet de départ : batterie, basse acide, poly chorus, nappes, retours réverbération et écho. */
    void loadDefaultProject();
    void clearProject();

    /** Charge un fichier audio (octets bruts) dans une piste. Renvoie un message d'erreur ou vide. */
    juce::String loadSample (int track, int index, const juce::String& fileName, juce::MemoryBlock fileBytes);
    /** 1.1 : échantillon déposé par l'utilisateur sur un pad de synthèse : déclin au maximum (lecture entière). */
    void openSampleDecay (int track, int index);
    /** Projet complet en XML (réglages globaux, structure, séquences, échantillons). */
    /** asTemplate (v0.46) : modèle — réglages, sons et effets gardés, patterns, verrous et morceau retirés. */
    bool writeProjectFile (juce::OutputStream& out, bool asTemplate = false);
    /** Ouvre un projet XML. Renvoie un message d'erreur ou vide. */
    juce::String readProjectFile (juce::InputStream& in);

    /** Découpe l'échantillon 0 d'une piste « Sampler à découpes » (parts égales ou transitoires). */
    int autoSlice (int track, bool onTransients, int count);

    // Annuler / rétablir et photo des réglages (fil de l'interface). L'historique est fait de
    // copies du projet sans les échantillons ; historyTick() est appelé par l'interface
    // (30 fois par seconde) et range une copie quand le projet a changé puis s'est posé.
    void historyTick();
    bool undo();
    bool redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void takePhoto();
    bool recallPhoto();
    bool hasPhoto() const { return photo.isValid(); }
    /** Garde le tampon de l'enregistreur d'une piste « sampler de performance » dans un emplacement d'échantillon. */
    juce::String keepRecording (int track, int lane, int index); // v0.42 : enregistreur de la piste interne « lane »
    /** v0.40 : éditeur audio d'un emplacement (Pieuvre 8) : 0 inverser, 1 normaliser, 2 fondu d'entrée,
        3 fondu de sortie, 4 rogner, 5 silence ; sur la sélection [a, b] (fractions). L'échantillon
        modifié remplace l'ancien (Ctrl+Z le rend). Renvoie un message d'erreur, vide si tout va bien. */
    juce::String editSample (int track, int index, int op, double a, double b);
    /** v0.40 : copier un emplacement vers un autre (même piste ou autre piste). */
    juce::String copySample (int fromTrack, int fromIndex, int toTrack, int toIndex);
    /** v0.41 : copie d'une partie d'un échantillon (fractions a..b) vers un autre emplacement, de
        n'importe quelle piste (réserve commune de Pieuvre 8). */
    juce::String copySampleRegion (int fromTrack, int fromIndex, double a, double b, int toTrack, int toIndex);

    // 1.1 (lots 1 et 2) : TRAITER un échantillon par une chaîne de 4 effets, hors temps réel
    /** Lance le traitement de l'emplacement (track, index) sur la sélection [a, b] (fractions ; 0..1 :
        tout le son). chain : tableau d'objets { type, values (réglages réels), bypass }. preview :
        le résultat n'est que pré-écouté ; sinon il remplace l'échantillon (version précédente gardée
        pour Annuler). keepOriginal : copie de l'original dans « Son traité/Originaux ». */
    juce::String procStart (int track, int index, double a, double b, const juce::var& chain, bool keepTail, bool preview, bool keepOriginal);
    /** État du traitement en cours ou du dernier ; applique le résultat quand il est prêt (fil de l'interface). */
    juce::var procStatus();
    void procCancel();
    /** Pré-écoute : rejoue le dernier résultat (play) ou l'arrête. */
    void procAudition (bool play);
    /** Annuler le dernier remplacement de cet emplacement (8 versions gardées). */
    juce::String sampleUndo (int track, int index);
    int sampleUndoCount (int track, int index) const;
    /** « Son traité » : dossier des sons traités (ordinateur : Documents/Groovebox/Son traité ;
        Android : Musique/Groovebox/Son traité). */
    static juce::File processedFolder();
    /** Lit une chaîne d'effets venue de l'interface (bornée : 4 emplacements, 256 réglages). */
    static std::vector<engine::ChainSlot> parseChain (const juce::var& chain);
    /** v0.33 : prises de son des vocodeurs terminées → échantillon 1 de la piste (fil de l'interface). */
    void keepVoiceCaptures();

    // v0.32 : enregistrement audio de ce qu'on joue (master, pistes séparées ou les deux ; WAV ou MP3)
    /** v0.46.2 : follow : prise calée sur la lecture (démarre avec elle, s'arrête pile à la fin du
        morceau, à l'ARRÊT, ou à la fin du tour de boucle quand on rappuie sur « ■ Audio »). */
    juce::String audioRecStart (int mode, int format, int follow);   // follow : 0 libre, 1 calée, 2 boucle parfaite ; vide si la prise a démarré, sinon le message
    void audioRecStop();
    /** v0.46.2 : fil de l'interface (minuteur) : range la prise calée finie d'elle-même. */
    /** 1.1 (lot 3) : prise de l'écran AUDIO (sources cochées, chaîne de 4 effets, destination). */
    juce::String audioRecStartEx (const juce::var& options);
    /** 1.1 (lot 3) : le fichier de la dernière prise (traité si possible) va dans ce pad (annulable). */
    juce::String audioRecToPad (int track, int index);
    /** 1.1 (lot 3) : « Écouter le traitement » : la sortie principale passe dans cette chaîne (latence
        comprise) ; chaîne vide : arrêt. La prise, elle, reste le son d'origine. Renvoie la latence (ms). */
    double setMonitorChain (const juce::var& chain);
    /** 1.1 : écoute de l'écran AUDIO limitée à ce qu'on enregistre (pistes, retours, entrée) ; off : tout. */
    void setMonitorSources (const juce::var& sources);
    void audioRecTick();
    juce::String audioRecStatus() const { return recorder.statusJson(); }
    bool audioRecReveal();                               // ouvre le dossier de la dernière prise (ordinateur)

    // v0.46 fix8 : montage audio (onglet Lancement › Audio) — fil de l'interface
    std::string montageJson() const;
    void montageSetClips (const juce::String& json);
    void montageSetFlags (bool on, bool loop, double loopA, double loopB);
    void montageSetPlayhead (double beat);
    juce::String montageRecord (bool start);             // vide si c'est parti, sinon le message
    juce::String montageExport (int format, bool loopOnly = false); // 0 WAV, 1 MP3 ; boucle seule ou tout ; vide si c'est parti
    juce::String montagePeaks (const juce::String& takeId) const;
    bool montageRecording() const noexcept { return montageTakeOn; }
    bool montageIsOn() const noexcept { return montagePlayer.isActive(); }
    double montagePosition() const noexcept { return montagePlayer.positionBeats(); }
    /** Fichier du projet en cours de lecture ou d'écriture (chemins des prises relatifs à lui). */
    void setProjectFileContext (const juce::File& f) { projectContext = f; }
    /** Outils de contrôle (sans boucle de messages) : ce que fait la minuterie pour le montage. */
    void pumpMontage() { montageTick(); }
    bool montageExporting() const noexcept { return montageExportOn; }
    bool montageCutting() const noexcept { return montageJob != nullptr; }
    const MontageDoc& getMontageDoc() const noexcept { return montageDoc; }

    void launchScene (int scene, bool otherSlot) noexcept;
    void setGlobal (const juce::String& id, float plainValue);
    float getGlobal (const juce::String& id) const;

    /** Rôle des pads de l'interface, suivi aussi par les pads du contrôleur MIDI (notes 36 à 51). */
    /** Rôle des pads (interface et contrôleur MIDI). En mode Pas, les 16 pads posent ou retirent
        les pas de la page affichée, sur la ligne choisie de la piste au premier plan. */
    void setPadMode (PadMode m, int sceneBank = 0, int lane = 0, int page = 0) noexcept
    {
        padMode.store ((int) m); padBank.store (juce::jlimit (0, 1, sceneBank));
        padLane.store (juce::jlimit (0, engine::Sequencer::numLanes - 1, lane)); padPage.store (juce::jlimit (0, engine::Sequencer::maxSteps / 16 - 1, page));
    }
    PadMode getPadMode() const noexcept { return (PadMode) padMode.load(); }
    // Fill du séquenceur = Fill || Break || Break temporisé, recalculé par le fil audio à chaque bloc
    // (un seul écrivain : pas de Fill bloqué quand deux fils écrivent en même temps)
    void setBreak (bool on) noexcept { breakHeld.store (on); }
    void setFill (bool on) noexcept { fillHeld.store (on); }
    /** Break temporisé : actif tout de suite, relâché seul sur le temps le plus proche de
        « maintenant + beats » (beats ≤ 0 : l'arrêter). Le compte se fait sur le fil audio. */
    void timedBreak (double beats) noexcept { timedBreakRequest.store (beats > 0.0 ? std::clamp (beats, 0.25, 64.0) : -1.0); }
    bool getBreak() const noexcept { return breakHeld.load() || timedBreakOn.load(); }
    bool getTimedBreak() const noexcept { return timedBreakOn.load(); }
    bool getFill() const noexcept { return fillHeld.load(); }

    /** Macros du logiciel hôte : chacune pilote jusqu'à macroLinks réglages, chacun sur sa plage
        (de lo à hi, valeurs normalisées ; lo > hi : sens inverse). Fil de l'interface. */
    static constexpr int macroLinks = 8;
    /** v0.46 : base / ref = point d'ancrage du mode relatif (valeur du réglage, valeur de la macro au
        moment de l'affectation) ; -1 : à prendre au moment de l'affectation. */
    struct MacroLink { int global = -1; float lo = 0.0f, hi = 1.0f, base = -1.0f, ref = -1.0f; };
    /** Ajoute le réglage à la macro (pleine plage), ou l'en retire s'il y est déjà. Vrai : il y est. */
    bool toggleMacroTarget (int macro, int global) noexcept;
    void setMacroRange (int macro, int global, float lo, float hi) noexcept;
    void clearMacro (int macro) noexcept;
    std::vector<MacroLink> getMacroLinks (int macro) const;
    /** pushValue : la valeur de la macro est écrite tout de suite dans les réglages visés ; sinon
        ils gardent la leur jusqu'au prochain mouvement de la macro (retour en arrière). */
    void setMacroLinks (int macro, const std::vector<MacroLink>& links, bool pushValue) noexcept;
    float getMacroValue (int macro) const noexcept { return macro >= 0 && macro < numMacros ? pMacro[(size_t) macro]->load() : 0.0f; }
    /** v0.46 : mode de la macro. Absolu : chaque réglage suit la position de la macro sur sa plage
        (l'affecter l'y place). Relatif : affecter ne change rien ; le réglage part de sa valeur et
        bouge avec la macro (vers « De » quand elle descend, vers « À » quand elle monte) ; le
        retoucher à la main le réancre. */
    /** v0.46 : déplacer un insert : réglages, interrupteur, modulations, scènes, courbes (moteur), et
        aussi commandes MIDI apprises et macros qui le pilotent. Fil de l'interface. */
    void moveInsert (int channel, int from, int to);
    void setMacroRelative (int macro, bool relative) noexcept;
    bool isMacroRelative (int macro) const noexcept { return macro >= 0 && macro < numMacros && macroRel[(size_t) macro].load(); }
    /** Courbe du mode relatif : passe par (ref, base), atteint lo en 0 et hi en 1. */
    static float relativeMacro (float v, float lo, float hi, float base, float ref) noexcept
    {
        if (v >= ref) return ref >= 0.9999f ? base : base + (hi - base) * (v - ref) / (1.0f - ref);
        return ref <= 1.0e-4f ? base : base - (base - lo) * (ref - v) / ref;
    }

    // Rangées de lancement (principe Force) : chaque rangée peut rappeler une scène de réglages
    void launchRow (int row);
    /** v0.46 : rangée lancée en dernier (-1 : aucune), pour « rangée suivante / précédente ». */
    int getLastRow() const noexcept { return lastRow.load (std::memory_order_relaxed); }
    void setRowScene (int row, int scene) noexcept
    {
        if (row < 0 || row >= engine::Sequencer::numPatterns) return;
        rowScene[(size_t) row].store (scene >= 0 && scene < engine::numScenes ? scene : -1);
        rowVersion.fetch_add (1);
    }
    int getRowScene (int row) const noexcept { return row >= 0 && row < engine::Sequencer::numPatterns ? rowScene[(size_t) row].load() : -1; }

    // Apprentissage MIDI. Cibles : adresse globale d'un réglage (>= 0) ou commande spéciale.
    enum MidiTarget { midiNone = -1, midiXfade = -2, midiBreak = -3, midiFill = -4, midiPlay = -5,
                      midiStart = -6, midiStop = -7, midiRecord = -8, midiRecMoves = -9, midiSongMode = -10,
                      midiRowPrev = -11, midiRowNext = -12, midiRowStop = -13, // v0.46 : rangée précédente / suivante, tout arrêter
                      midiPhoto = -14, midiRecall = -15, // v0.46 : prendre la photo des réglages, la rappeler
                      midiRowBase = -20,      // v0.46 : -20 - rangée : lancer la rangée A à P (-20 à -35 ; fix29 : I à P)
                      midiLastSpecial = -35,
                      midiMuteBase = -100,   // -100 - piste : coupure de la piste (pistes 1 à 16 : -100 à -115)
                      midiSoloBase = -200,   // -200 - piste : solo de la piste (-200 à -215)
                      midiBusMuteBase = -300, // v0.46 : -300 - (canal - 16) : coupure d'un retour, du principal ou d'un groupe
                      midiBusSoloBase = -400, // v0.46 : -400 - (canal - 16) : solo d'un groupe
                      midiKnobBase = -500,    // v0.46 fix24 : -500 - k : bouton assignable k (1 à 16 : -500 à -515)
                      midiHostBase = -600,    // v0.46 fix24 : -600 - h : réglage global (tempo, swing, scènes, pavé, macros…)
                      midiUiBase = -700 };    // v0.46 fix24 : -700 - n : valeur d'un écran pilotée par l'interface (64 au plus)
    static constexpr int numBusTargets = engine::numChannels - engine::numTracks;
    static constexpr int numKnobs = 16, numUiCtl = 64;
    /** 1.1 : une télécommande (téléphone) a parlé depuis l'ouverture. */
    std::atomic<bool> remoteSeen { false };
    // 1.1 : retour vers la télécommande (application autonome) : sortie MIDI du même nom que l'entrée
    // qui a dit « bonjour », valeurs déjà envoyées (pour n'envoyer que les changements), noms des pistes
    void remoteFeedbackTick();
    // 1.1 : changement de programme sur le canal d'une piste → préréglage d'usine n° N de son instrument
    // (sélection de banque CC 0 : 0 = instrument, 1 à 4 = effet de l'insert 1 à 4) ; appliqué par le fil des messages
    std::array<std::atomic<int>, engine::numTracks> pendingPreset {};
    std::array<int, 16> bankMsb {}, bankLsb {};   // 1.1 : banque (CC 0 / CC 32) par canal — fil audio seulement
    void presetRequestsTick();
    void soundPoolTick(); // après la 1.1 : sons du pas (préréglages verrouillés sur des pas) résolus pour le fil audio
    double lastSoundPoolMs = 0.0;
    std::string lastSoundSig;
public:
    std::atomic<int> userPresetGen { 0 }; // après la 1.1 : préréglages de l'utilisateur changés (sons du pas à relire)
    /** Après la 1.1 : valeurs (identifiant, valeur réelle) et nom du son n° prog + 1 d'un module ; faux s'il n'existe pas. */
    static bool presetByNumber (const engine::ModuleDesc& d, int prog, std::vector<std::pair<std::string, float>>& values, juce::String& name);
private:
    void remoteSendAll (const std::vector<uint8_t>& m); // vers la télécommande (câble et Wi-Fi)
    std::unique_ptr<grv::WifiServer> wifi;               // 1.1 : télécommande par le Wi-Fi (PC)
    int wifiHelloSeen = 0;
public:
    /** 1.1 : télécommande Wi-Fi : écouter ou non (application autonome) ; adresses de l'appareil. */
    bool setWifiRemote (bool on);
    bool wifiRemoteOn() const { return wifi != nullptr && wifi->running(); }
    uint32_t remoteInstanceId() const noexcept { return remoteId; }
private:
    const uint32_t remoteId = (uint32_t) juce::Random::getSystemRandom().nextInt (0x0FFFFFFF) + 1u;
public:
    std::unique_ptr<juce::MidiOutput> remoteBack;
    juce::String remoteBackName;
    int remoteHelloSeen = 0, remoteTicks = 0;
    bool remoteListening = false;
    std::vector<int> remoteSent;
    // 1.1 : traitement hors temps réel d'un échantillon (fil à part) et historique des remplacements
    struct ProcJob
    {
        std::atomic<bool> busy { false }, cancel { false }, ready { false };
        std::atomic<float> progress { 0.0f };
        int track = -1, index = -1; bool preview = false, keepOriginal = false, applied = false;
        juce::String error, name;
        double rate = 48000.0; int latency = 0, tail = 0;
        std::vector<float> l, r;  // résultat complet (son entier, sélection traitée)
    };
    std::unique_ptr<ProcJob> proc = std::make_unique<ProcJob>();
    std::atomic<engine::OfflineChain*> monitorChain { nullptr };   // 1.1 (lot 3) : écoute du traitement
    std::atomic<int> monitorBusy { 0 };
    double monitorMs = 0.0;                                         // 1.1 : latence de la chaîne d'écoute (fil de l'interface)
    juce::MidiBuffer midiOutBuf;
    std::array<std::uint32_t, 128> sentKey {};                      // après la 1.1 : derniers messages envoyés (anti-boucle)
    std::array<double, 128> sentAt {};
    int sentHead = 0;                                    // après la 1.1 : sortie MIDI du bloc (pistes MIDI)
    std::atomic<int> lastCcIn { -1 }, lastCcSeq { 0 };               // après la 1.1 : dernier CC reçu ((canal << 8) | numéro)
    std::thread procThread;
    std::shared_ptr<engine::SampleBuffer> auditionHold, auditionPrev;
    struct SampleVersion { juce::String name; std::shared_ptr<const std::vector<std::uint8_t>> bytes; };
    std::map<int, std::deque<SampleVersion>> sampleHistory; // clé : track * maxSamples + index
    void pushSampleVersion (int track, int index);
    std::array<int, 2> remoteVecShown { -1, -1 };                   // 1.1 : dernière position jouée du pavé envoyée
    std::array<std::string, engine::numChannels> remoteNames {};   // pistes puis groupes (noms)
    std::array<std::string, numKnobs> remoteKnobLabels {};
    std::array<std::string, engine::numTracks> remotePresets {};    // 1.1 : préréglages de chaque piste (instrument et inserts)
    std::array<uint8_t, engine::numChannels> remoteLevels {};        // pistes, principal, retours, groupes
    juce::String remoteTargetLabel (int target, juce::String& sub, int& color) const; // 1.1 : libellé d'un bouton assignable
    /** v0.46 fix24 : réglages globaux pilotables (MIDI, boutons assignables), dans cet ordre (ne pas réordonner :
        les numéros sont gardés dans les projets et les affectations MIDI). */
    static juce::String hostCtlId (int h)
    {
        static const char* const ids[] { "bpm", "swing", "sceneA", "sceneB", "vecOn", "vecMode", "vecX", "vecY", "vecOrbit", "vecRate", "vecDepth",
                                         "macro1", "macro2", "macro3", "macro4", "macro5", "macro6", "macro7", "macro8",
                                         "vecSA", "vecSB", "vecSC", "vecSD", "vecFA", "vecFB", "vecFC", "vecFD",
                                         "vecRot", "vecHeight" }; // v0.46 fix28 (à la fin : les numéros déjà gardés ne bougent pas)
        return h >= 0 && h < (int) std::size (ids) ? juce::String (ids[h]) : juce::String();
    }
    static constexpr int numHostCtl = 29;
    static constexpr int midiLowestTarget = midiUiBase - numUiCtl + 1;
    static bool isMidiSpecial (int target) noexcept { return target <= midiXfade && target >= midiLastSpecial; }
    static bool isKnobTarget (int t) noexcept { return t <= midiKnobBase && t > midiKnobBase - numKnobs; }
    static bool isHostTarget (int t) noexcept { return t <= midiHostBase && t > midiHostBase - numHostCtl; }
    static bool isUiTarget (int t) noexcept { return t <= midiUiBase && t > midiUiBase - numUiCtl; }
    /** v0.46 fix24 : cible valable pour une commande MIDI ou un bouton assignable (hors réglages >= 0). */
    static bool isMidiExtra (int t) noexcept { return isKnobTarget (t) || isHostTarget (t) || isUiTarget (t); }

    // --- v0.46 fix24 : 16 boutons assignables -------------------------------------------------
    // Chacun pilote une cible (réglage du moteur, commande spéciale, réglage global, valeur d'écran),
    // depuis l'interface ou une commande MIDI apprise. Le fil audio applique la valeur au bloc suivant.
    /** v0.46 fix26 : ajoute à un état relu les réglages absents (versions plus anciennes), à leur valeur d'origine. */
    void fillMissingParams (juce::ValueTree& tree) const;
    void setKnob (int k, float v) noexcept
    {
        if (k < 0 || k >= numKnobs) return;
        v = juce::jlimit (0.0f, 1.0f, v);
        knobVal[(size_t) k].store (v); knobPending[(size_t) k].store (v);
    }
    float getKnob (int k) const noexcept { return k >= 0 && k < numKnobs ? knobVal[(size_t) k].load() : 0.0f; }
    /** La cible ne peut pas être un autre bouton assignable. */
    void setKnobTarget (int k, int target) noexcept
    {
        if (k < 0 || k >= numKnobs) return;
        if (! (target >= 0 && target < engine::numSlots) && ! isMidiSpecial (target) && ! isMidiTrackTarget (target) && ! isHostTarget (target) && ! isUiTarget (target)) target = midiNone;
        knobTarget[(size_t) k].store (target); midiVersion.fetch_add (1);
    }
    int getKnobTarget (int k) const noexcept { return k >= 0 && k < numKnobs ? knobTarget[(size_t) k].load() : (int) midiNone; }
    /** v0.46 fix28 : LFO des boutons : 4 sources propres aux boutons assignables (les sources 1 à 4 du bus
        principal, réglées dans le même panneau que celles des synthés) ; chaque bouton en suit une (0 = aucune)
        avec une profondeur -1..1, autour de sa position. */
    static constexpr int knobLfoChannel = engine::masterChannel, numKnobLfos = 4;
    void setKnobLfo (int k, int src, float depth) noexcept
    {
        if (k < 0 || k >= numKnobs) return;
        knobLfo[(size_t) k].store (juce::jlimit (0, numKnobLfos, src)); knobDepth[(size_t) k].store (juce::jlimit (-1.0f, 1.0f, depth));
        midiVersion.fetch_add (1);
    }
    int getKnobLfo (int k) const noexcept { return k >= 0 && k < numKnobs ? knobLfo[(size_t) k].load() : 0; }
    float getKnobDepth (int k) const noexcept { return k >= 0 && k < numKnobs ? knobDepth[(size_t) k].load() : 0.0f; }
    /** Valeur envoyée par le bouton avec son LFO (-1 : pas de LFO). */
    float getKnobOut (int k) const noexcept { return k >= 0 && k < numKnobs ? knobOut[(size_t) k].load() : -1.0f; }
    /** Table des valeurs d'écran (texte JSON tenu par l'interface : numéro → identifiant, nom), gardée dans le projet. */
    void setCtlTable (const juce::String& t) { ctlTable = t.substring (0, 20000); }
    const juce::String& getCtlTable() const noexcept { return ctlTable; }
    /** Valeurs d'écran poussées par le MIDI ou un bouton assignable : l'interface les applique. */
    unsigned getUiCtlVersion() const noexcept { return uiCtlVersion.load(); }
    float getUiCtl (int n) const noexcept { return n >= 0 && n < numUiCtl ? uiCtlVal[(size_t) n].load() : 0.0f; }
    unsigned getUiCtlSeq (int n) const noexcept { return n >= 0 && n < numUiCtl ? uiCtlSeq[(size_t) n].load() : 0u; }
    static bool isMidiTrackTarget (int target) noexcept
    {
        return (target <= midiMuteBase && target > midiMuteBase - engine::numTracks) || (target <= midiSoloBase && target > midiSoloBase - engine::numTracks)
            || (target <= midiBusMuteBase && target > midiBusMuteBase - numBusTargets)
            || (target <= midiBusSoloBase && target > midiBusSoloBase - numBusTargets && engine::numTracks + midiBusSoloBase - target >= engine::groupBase);
    }
    static constexpr int numMidiKeys = 16 * 128;
    /** Le prochain CC ou la prochaine note reçus seront affectés à cette cible (midiNone = annuler). */
    void startMidiLearn (int target) noexcept { learnTarget.store (target); }
    /** v0.46 : canaux MIDI. Canal des notes et des commandes : 0 = tous, 1 à 16 ; commandes apprises
        actives ou non ; canal propre à une piste (0 = elle joue quand elle est choisie). */
    void setMidiRoute (int noteChan, int ctrlChan, bool ctrlOn) noexcept
    {
        midiNoteChan.store (juce::jlimit (0, 16, noteChan)); midiCtrlChan.store (juce::jlimit (0, 16, ctrlChan)); midiCtrlOn.store (ctrlOn);
        midiVersion.fetch_add (1);
    }
    void setTrackMidiChannel (int track, int chan) noexcept
    {
        if (track >= 0 && track < engine::numTracks) { trackMidiChan[(size_t) track].store (juce::jlimit (0, 16, chan)); midiVersion.fetch_add (1); }
    }
    int getMidiNoteChannel() const noexcept { return midiNoteChan.load(); }
    int getMidiCtrlChannel() const noexcept { return midiCtrlChan.load(); }
    bool getMidiCtrlOn() const noexcept { return midiCtrlOn.load(); }
    int getTrackMidiChannel (int track) const noexcept { return track >= 0 && track < engine::numTracks ? trackMidiChan[(size_t) track].load() : 0; }
    /** GOLEM 1.0 : canal MIDI propre à une piste interne T (FM Player, Pieuvre 8) : 0 = elle suit la T
        choisie à l'écran ; un canal = ce canal joue toujours cette T (avant le canal de la piste). */
    static constexpr int maxLaneChans = 8;
    void setLaneMidiChannel (int track, int lane, int chan) noexcept
    {
        if (track >= 0 && track < engine::numTracks && lane >= 0 && lane < maxLaneChans)
        { laneMidiChan[(size_t) track][(size_t) lane].store (juce::jlimit (0, 16, chan)); midiVersion.fetch_add (1); }
    }
    int getLaneMidiChannel (int track, int lane) const noexcept
    {
        return track >= 0 && track < engine::numTracks && lane >= 0 && lane < maxLaneChans ? laneMidiChan[(size_t) track][(size_t) lane].load() : 0;
    }
    int getMidiLearn() const noexcept { return learnTarget.load(); }
    void clearMidiTarget (int target) noexcept;
    /** Affectations : (genre 0 = CC, 1 = note ; canal 0..15 ; numéro ; cible). */
    struct MidiMapping { int kind, channel, number, target; };
    std::vector<MidiMapping> getMidiMappings() const;
    void setMidiMapping (int kind, int channel, int number, int target) noexcept;
    /** Apprentissage sur une commande déjà affectée : en attente de la réponse (remplacer ou non).
        Renvoie faux s'il n'y en a pas ; sinon genre, canal, numéro, ancienne et nouvelle cibles. */
    bool getMidiConflict (int& kind, int& channel, int& number, int& oldTarget, int& newTarget) const noexcept;
    void resolveMidiConflict (bool replace) noexcept;
    void transportCommand (int target) noexcept;
    std::uint32_t getMidiVersion() const noexcept { return midiVersion.load(); }
    /** v0.32.1 : réglages écrits depuis le fil audio (commande MIDI apprise, macro de l'hôte), pour que
        l'interface montre le bouton bouger. File circulaire sans verrou : le fil audio écrit, l'éditeur lit. */
    static constexpr std::uint32_t touchedSize = 256;
    void markTouched (int global) noexcept
    {
        const auto i = touchedWrite.load (std::memory_order_relaxed);
        touchedRing[i % touchedSize].store (global, std::memory_order_relaxed);
        touchedWrite.store (i + 1, std::memory_order_release);
    }
    /** Lit les réglages touchés depuis `readIndex` (le met à jour) ; sans doublon. */
    std::vector<int> drainTouched (std::uint32_t& readIndex) const
    {
        const auto w = touchedWrite.load (std::memory_order_acquire);
        if (w - readIndex > touchedSize) readIndex = w - touchedSize;
        std::vector<int> out;
        for (; readIndex != w; ++readIndex)
        {
            const auto g = touchedRing[readIndex % touchedSize].load (std::memory_order_relaxed);
            if (g >= 0 && std::find (out.begin(), out.end(), g) == out.end()) out.push_back (g);
        }
        return out;
    }

    double getCurrentBpm() const noexcept { return currentBpm.load (std::memory_order_relaxed); }
    std::uint32_t getProjectVersion() const noexcept { return projectVersion.load(); }
    /** Charge du calcul audio (v0.23) : part du temps d'un bloc passée à le calculer, moyenne
        lissée et pointe récente (1 = 100 %, la limite avant les craquements). */
    float getCpuAverage() const noexcept { return cpuAvg.load (std::memory_order_relaxed); }
    float getCpuPeak() const noexcept { return cpuPeak.load (std::memory_order_relaxed); }
    void setVecPath (const juce::String& text, bool store = true);
    juce::String getVecPath() const;
    float getVecShownX() const noexcept { return vecShownX.load (std::memory_order_relaxed); }
    float getVecShownY() const noexcept { return vecShownY.load (std::memory_order_relaxed); }
    float getAudioPeriodMs() const noexcept { return audioPeriodMs.load (std::memory_order_relaxed); }
    int getAudioBlockSize() const noexcept { return audioBlockSize.load (std::memory_order_relaxed); }
    /** v0.46 fix17 : état Android du fil audio : -1 hors Android ; sinon 100 si l'indice de performance
        est actif, plus le nombre de grands cœurs où il est posé. */
    /** v0.46 fix21 : l'utilisateur vient de toucher l'interface (téléphone : garder l'écran allumé). Fil des messages. */
    void noteUserActivity() noexcept;
    int getAndroidAudioStatus() const noexcept
    {
       #if JUCE_ANDROID
        return (perfHint.active() ? 100 : 0) + perfHint.fastCores();
       #else
        return -1;
       #endif
    }

    /** Pistes (v0.23) : déplacer une piste (tout la suit : instrument, effets, séquences, scènes,
        morceau, modulations, échantillons), ou insérer une piste libre avant une autre.
        Fil de l'interface. Renvoie un message d'erreur, vide si c'est fait. */
    juce::String moveTrack (int from, int to);
    juce::String insertTrack (int at);
    /** v0.41 : copie de piste à piste (principe de l'Octatrack) : la piste « to » devient la copie de
        « from » — instrument, réglages, effets, modulations, séquences (pas, verrous, conditions),
        verrous de scène, échantillons, nom, couleur, sortie. Une étape d'historique (Ctrl+Z). */
    juce::String copyTrack (int from, int to);
    /** v0.42 : Pieuvre 8 — copie d'une piste interne sur une autre (réglages, 2 effets, lignes de pas,
        verrous) ; ses emplacements restent ceux de la piste copiée (dans la réserve). Ctrl+Z. */
    juce::String copyLane (int track, int from, int to);
    /** v0.43 : Pieuvre 8 — réglages d'une piste interne (ou des 8 : lane = -1) remis à l'origine, avec
        leurs 2 effets ; séquences, échantillons et attributs des emplacements gardés. Ctrl+Z. */
    juce::String resetLanes (int track, int lane);

    /** Adresse stable d'un réglage : canal + zone (« mix », « inst », « fx1 »…, « mod1 »…) + identifiant. */
    struct Address { int channel = -1; juce::String area, id; };
    Address addressOf (int global) const;
    int globalOf (const Address&) const;

    juce::AudioProcessorValueTreeState state;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;

    juce::ValueTree projectToTree (bool withSamples = true) const;
    /** Morceau (rangées et courbes) : arbre du projet, ou rien s'il est vide. */
    juce::ValueTree songToTree() const;
    void songFromTree (const juce::ValueTree& tree, double nowMs);
    void projectFromTree (const juce::ValueTree&);
    /** Contenu d'un projet (réglages, modulations, scènes, séquences…) sur le projet en place. */
    void applyProjectContent (const juce::ValueTree& project, bool withSamplesAndMidi, double nowMs);
    /** Valeur normalisée rangée par une version plus ancienne du format, convertie (choix ajoutés, réserves de niveau). */
    float migrateValue (int version, int global, float norm) const;
    /** Rappel léger (historique, photo) : instruments et effets gardés quand ils n'ont pas changé,
        échantillons gardés. */
    void restoreSnapshot (const juce::ValueTree& snapshot);
    juce::ValueTree snapshotForHistory() const;
    /** Range l'état courant dans l'historique s'il a changé depuis la dernière étape. */
    bool commitHistory();
    /** Oublie les étapes les plus anciennes quand les échantillons gardés dépassent le budget mémoire. */
    void trimHistory();
    /** Change dès qu'un réglage, une séquence ou la structure change (sauvegarde auto, historique). */
    std::uint64_t changeStamp() const;
    juce::String loadSampleBytes (int track, int index, const juce::String& fileName, std::shared_ptr<const std::vector<std::uint8_t>> bytes);
    /** v0.49 : firmware du lecteur FM-1 (.fwsc) dans l'emplacement 1 d'une piste FM Player. */
    juce::String loadFirmwareBytes (int track, int index, const juce::String& fileName, std::shared_ptr<const std::vector<std::uint8_t>> bytes);

    /** Échantillon gardé par l'historique : les octets du fichier sont partagés, jamais recopiés. */
    struct SampleRef : juce::ReferenceCountedObject
    {
        SampleRef (std::string n, std::shared_ptr<const std::vector<std::uint8_t>> b) : name (std::move (n)), bytes (std::move (b)) {}
        std::string name;
        std::shared_ptr<const std::vector<std::uint8_t>> bytes;
        using Ptr = juce::ReferenceCountedObjectPtr<SampleRef>;
    };
    mutable std::array<std::array<SampleRef::Ptr, engine::maxSamples>, engine::numTracks> sampleRefs;
    static constexpr size_t maxHistory = 50;
    std::vector<juce::ValueTree> undoStack, redoStack;
    juce::ValueTree committed, photo;
    juce::SpinLock photoLock; // photo lue par la sauvegarde (fil de l'hôte possible)
    std::uint64_t historyStamp = 0;
    int historyCountdown = 0;

    engine::Engine engine;
    std::array<std::atomic<int>, touchedSize> touchedRing {};
    std::atomic<std::uint32_t> touchedWrite { 0 };
    Recorder recorder;    // v0.32 : prise de son du mixage (après le moteur : détruit avant lui)
    // v0.46 fix8 : montage audio
    MontageDoc montageDoc;
    MontagePlayer montagePlayer;
    juce::File projectContext;
    juce::File montageTakeFolder;
    bool montageTakeOn = false, montageExportOn = false, montageExportLoop = false;
    int montageExportWait = -1; // v0.46.2 : export armé, lecture pas encore lancée (début en préparation)
    double montageTakeBpm = 120.0, montageTakeAt = 0.0; // fix11 : point de départ de la prise (noires, sur une mesure)
    bool montageTakeLoop = false; // fix12 : prise limitée à la boucle (s'arrête seule à sa fin)
    juce::String montageStatus;
    std::unique_ptr<juce::Thread> montageJob;
    std::shared_ptr<montage::TakeResult> montageJobResult;
    std::shared_ptr<std::atomic<bool>> montageJobDone;
    int montageTakeCount = 0;
    void montageTick();
    juce::AudioFormatManager formats;

    std::atomic<float>* pBpm = nullptr;
    std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pXfade = nullptr;
    std::atomic<float>* pSceneA = nullptr;
    std::atomic<float>* pSceneB = nullptr;
    // v0.46 fix22 : pavé vectoriel
    std::atomic<float>* pVecOn = nullptr; std::atomic<float>* pVecMode = nullptr;
    std::atomic<float>* pVecX = nullptr; std::atomic<float>* pVecY = nullptr;
    std::atomic<float>* pVecOrbit = nullptr; std::atomic<float>* pVecRate = nullptr; std::atomic<float>* pVecDepth = nullptr; std::atomic<float>* pVecRot = nullptr; std::atomic<float>* pVecHeight = nullptr;
    std::array<std::atomic<float>*, 4> pVecScene {}, pVecFx {};
    double vecPhase = 0.0;                                    // fil audio : tour du mouvement (0..1)
    std::array<double, 4> vecRand { 0.5, 0.5, 0.5, 0.5 };     // fil audio : hasard (points de départ et d'arrivée x, y)
    std::uint32_t vecSeed = 0x9E3779B9u;
    std::atomic<float> vecShownX { 0.5f }, vecShownY { 0.5f }; // position jouée (avec le mouvement), pour l'écran
    struct VecPath { std::array<float, 128> x {}, y {}; int count = 0; };
    std::array<VecPath, 2> vecPaths {};                      // forme dessinée (double tampon)
    std::atomic<int> vecPathIndex { 0 };
    std::array<std::atomic<float>*, numMacros> pMacro {};
    struct MacroSlot { std::atomic<int> global { -1 }; std::atomic<float> lo { 0.0f }, hi { 1.0f }, base { -1.0f }, ref { 0.5f }, written { -1.0f }; };
    std::array<std::atomic<bool>, numMacros> macroRel {}; // v0.46 : macros en mode relatif
    std::atomic<int> lastRow { -1 }; // v0.46 : dernière rangée lancée
    std::atomic<int> midiNoteChan { 0 }, midiCtrlChan { 0 }; // v0.46 : 0 = tous les canaux
    std::atomic<bool> midiCtrlOn { true };
    std::array<std::atomic<int>, engine::numTracks> trackMidiChan {};
    std::array<std::array<std::atomic<int>, 8>, engine::numTracks> laneMidiChan {}; // GOLEM 1.0 : canal par piste interne T
    std::array<std::array<MacroSlot, macroLinks>, numMacros> macroSlots {};
    std::array<std::atomic<float>, numMacros> lastMacro {};

    std::atomic<int> padMode { 0 }, padBank { 0 }, padLane { 0 }, padPage { 0 };
    std::atomic<bool> breakHeld { false }, fillHeld { false }, timedBreakOn { false };
    std::atomic<double> timedBreakRequest { 0.0 };
    double timedBreakBeats = 0.0, lastBlockBeats = 0.0; // fil audio : temps restants, durée du bloc précédent
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<std::uint32_t> projectVersion { 0 };
    std::atomic<float> cpuAvg { 0.0f }, cpuPeak { 0.0f };
    struct LoadMeter { juce::int64 last = 0; double used = 0.0, wall = 0.0, maxUsed = 0.0; int count = 0; } loadMeter; // fil audio seulement
    std::atomic<float> audioPeriodMs { 0.0f };  // temps mesuré entre deux blocs
    std::atomic<int> audioBlockSize { 0 };
   #if JUCE_ANDROID
    AndroidPerfHint perfHint; // v0.46 fix16 : garde le fil audio sur des cœurs rapides
    int inputStartTicks = 0;  // v0.46 fix16 : fermeture du micro au démarrage (-1 : faite)
    double lastUserActivity = 0.0; // v0.46 fix21 : dernier toucher de l'interface (ms)
    bool screenKeptOn = false;     // v0.46 fix21 : écran gardé allumé
    int bgTicks = 0;               // v0.46.2 : battements sans la main (Android)
   #endif
    std::atomic<bool> inBackground { false }; // v0.46.2 : appli derrière une autre : calcul suspendu
   #if JUCE_ANDROID
   #endif

    // déplacements de pistes dans l'historique (v0.23) : ce que la copie d'historique ne porte pas
    // (patterns joués, photo, commandes MIDI) est renuméroté en défaisant ou refaisant l'étape
    struct MoveStep { juce::ValueTree before, redoMarker; std::array<int, engine::numTracks> newIndex {}; };
    std::vector<MoveStep> moveSteps;
    std::vector<const engine::SampleBuffer*> reuseSamples; // échantillons décodés réutilisables (restoreSnapshot)
    /** v0.49 : FM Player — mémoire flash d'un firmware lue dans le projet, reprise à son chargement. */
    std::array<std::shared_ptr<std::vector<std::uint8_t>>, engine::numTracks> pendingFlash;
    void prepareTrackPermutation (const std::array<int, engine::numTracks>& newIndex);
    void applyTrackPermutationExtras (const std::array<int, engine::numTracks>& newIndex);

    std::array<std::atomic<int>, engine::Sequencer::numPatterns> rowScene {};
    std::atomic<int> pendingRowScene { -1 }, pendingSceneLaunch { -1 };
    std::atomic<std::uint32_t> rowVersion { 0 };

    std::array<std::atomic<int>, numMidiKeys> ccMap {}, noteMap {};
    std::atomic<int> learnTarget { midiNone };
    // v0.46 fix24 : boutons assignables, réglages globaux et valeurs d'écran pilotés
    std::array<std::atomic<float>, numKnobs> knobVal {}, knobPending {};
    std::array<std::atomic<int>, numKnobs> knobTarget {};
    std::array<std::atomic<int>, numKnobs> knobLfo {};        // v0.46 fix28 : LFO des boutons (0 = aucun, 1..4)
    std::array<std::atomic<float>, numKnobs> knobDepth {}, knobOut {};
    std::array<bool, numKnobs> knobLfoActive {};               // fil audio : le LFO menait la cible au bloc précédent
    std::array<float, numKnobs> knobLfoSent {};
    std::array<int, numKnobs> knobLfoTarget {};
    std::array<float, numKnobs> knobLfoBase {};                // fil audio : position du bouton pendant la modulation                // fil audio : cible menée par le LFO (pour la rendre si elle change)
    int knobLfoBlocks = 0;
    std::array<float, numKnobs> knobLastAudio {};       // fil audio : dernier envoi (front des commandes)
    std::array<std::atomic<float>, numHostCtl> hostPending {};
    std::array<std::atomic<float>, numUiCtl> uiCtlVal {};
    std::array<std::atomic<unsigned>, numUiCtl> uiCtlSeq {};
    std::atomic<unsigned> uiCtlVersion { 0 };
    juce::String ctlTable;
    std::array<juce::RangedAudioParameter*, numHostCtl> hostParams {}; // relevés à la construction
    std::atomic<int> conflictKey { -1 }, conflictOld { midiNone }, conflictNew { midiNone }; // clé : genre × 2048 + canal × 128 + numéro
    std::atomic<int> songModeToggles { 0 };
    std::atomic<int> photoRequests { 0 }, recallRequests { 0 }; // v0.46 : photo / rappel au contrôleur (fil de l'interface)
    std::atomic<std::uint32_t> midiVersion { 0 };
    std::atomic<float> xfadeFromMidi { -1.0f };
    std::atomic<int> playToggleRequests { 0 };

   #if JUCE_ANDROID
    // Android : le système peut fermer l'application sans prévenir ; le projet est enregistré
    // automatiquement (au plus toutes les 15 s, seulement s'il a changé) et rouvert au démarrage.
    static juce::File autosaveFile();
    void autosaveIfChanged();
    std::uint64_t lastAutosaveStamp = 0;
    int autosaveTicks = 0;
    bool restoredFromAutosave = false;
    std::shared_ptr<std::atomic<bool>> autosaveBusy = std::make_shared<std::atomic<bool>> (false);
    double constructedAt = 0.0;
    bool launchOk = false; // v0.49 (essai 3)
   #endif

    std::array<engine::InputEvent, 1024> midiEvents {};
    std::vector<float> silentIn, monoOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveboxProcessor)
};

} // namespace grv
