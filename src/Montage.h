/*
    Groovebox — OFAD — AGPL-3.0-or-later

    v0.46 fix8 : MONTAGE AUDIO (onglet Lancement › Audio).

    - Une PRISE enregistre chaque piste (après ses effets et son volume) et chaque retour d'effet
      dans un fichier FLAC (le silence n'y prend presque pas de place), pendant la lecture.
    - À l'arrêt, chaque fichier est découpé en CLIPS là où la piste a vraiment sonné (coupures,
      solos, silences d'au moins une mesure séparent les clips). Les clips se posent sur la ligne
      de temps du montage (en noires, au tempo du montage) et remplacent ce qui s'y trouvait.
    - Le montage se retravaille : couper, déplacer, copier, dupliquer, supprimer, volume et fondus
      de chaque clip. Il est rangé avec le projet : dossier « <projet> - audio » à côté du .gbx.
    - Lecture (mode montage) : les clips jouent à la place des instruments ; les groupes et le bus
      principal restent calculés en direct. Un fil de lecture lit les fichiers en avance (≈ 1,4 s)
      dans une file sans verrou ; le fil audio ne touche jamais le disque.
*/

#pragma once

#include "engine/Engine.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>

#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace grv
{

struct MontageTake
{
    juce::String id;           // unique dans le projet
    juce::File file;           // chemin complet (relatif au projet dans le fichier .gbx)
    int lane = 0;              // 0..15 pistes, 16..19 retours
    double rate = 48000.0;     // fréquence d'échantillonnage du fichier
    juce::int64 length = 0;    // échantillons
    juce::String peaks;        // crêtes pour l'affichage : 1 octet (0..255) par bloc de peakFrames, en base64
};

struct MontageClip
{
    int lane = 0;
    juce::String take;
    juce::int64 src = 0, len = 0;     // morceau du fichier (échantillons du fichier)
    double beat = 0.0;                // début sur la ligne de temps (noires, au tempo du montage)
    float gainDb = 0.0f;              // volume du clip
    float fadeIn = 0.004f, fadeOut = 0.004f; // secondes (jamais 0 : pas de clic aux coupes)
};

struct MontageDoc
{
    double bpm = 120.0;               // tempo du montage (celui de la première prise)
    bool loop = false;
    double loopA = 0.0, loopB = 16.0; // noires
    double playhead = 0.0;            // noires : départ de la lecture
    std::vector<MontageTake> takes;
    std::vector<MontageClip> clips;

    const MontageTake* takeFor (const juce::String& id) const noexcept
    {
        for (const auto& t : takes) if (t.id == id) return &t;
        return nullptr;
    }
    /** Fin du dernier clip (noires). */
    double endBeat() const noexcept;
    /** Retire les prises qu'aucun clip n'utilise plus (les fichiers restent sur le disque). */
    void pruneTakes();
};

constexpr int montagePeakFrames = 1024;

/** Lecteur du montage : rend les clips en avance dans un fil à part, le fil audio les lit. */
class MontagePlayer final : public engine::MontageFeed, private juce::Thread
{
public:
    MontagePlayer();
    ~MontagePlayer() override;

    // Fil de l'interface ----------------------------------------------------------------
    void setDoc (const MontageDoc& doc, double deviceRate);
    void setActive (bool on) noexcept { active.store (on); }
    bool isActive() const noexcept { return active.load(); }
    /** Nouveau point de départ (noires) ; en lecture, la lecture y saute. */
    void setPlayhead (double beat);
    /** Position actuelle (noires sur la ligne de temps), pour l'interface. */
    double positionBeats() const noexcept;
    /** Export : la boucle est ignorée et la fin du montage (+ traîne) est signalée. */
    void setExporting (bool on) noexcept { exporting.store (on); endReached.store (false); }
    bool exportFinished() const noexcept { return endReached.load(); }
    /** v0.46.2 : le fil de lecture a déjà préparé le début (≈ 0,1 s) depuis le point de départ demandé :
        on peut lancer la lecture sans manquer l'attaque du premier son. */
    bool primed() const noexcept;
    /** v0.46.2 : fin du dernier clip (échantillons depuis le début de la ligne de temps). */
    juce::int64 endFrames() const noexcept { return endPos.load(); }
    void stopReading();

    // engine::MontageFeed (fil audio) ----------------------------------------------------
    bool montageActive() const noexcept override { return active.load (std::memory_order_relaxed); }
    void pull (bool playing, int n, float (*laneL)[engine::controlBlock], float (*laneR)[engine::controlBlock],
               std::uint32_t& laneMask) noexcept override;

private:
    struct PlanClip
    {
        int lane = 0, take = -1;
        juce::int64 start = 0, len = 0;   // ligne de temps, échantillons de la carte son
        double src = 0.0, ratio = 1.0;    // départ dans le fichier ; échantillons du fichier par échantillon de sortie
        float gain = 1.0f;
        juce::int64 fadeIn = 1, fadeOut = 1;
    };
    struct Plan
    {
        double rate = 48000.0, bpm = 120.0;
        bool loop = false;
        juce::int64 loopA = 0, loopB = 0, end = 0, playhead = 0;
        std::vector<PlanClip> clips;
        std::vector<juce::File> files;
    };
    static constexpr int chunkFrames = 256;
    static constexpr int ringChunks = 192;      // ≈ 1 s d'avance à 48 kHz
    struct Chunk
    {
        std::uint32_t gen = 0;
        juce::int64 start = 0;
        std::uint32_t mask = 0;
        float data[engine::numMontageLanes][2][chunkFrames];
    };

    void run() override;
    void render (const Plan& p, juce::int64 v, Chunk& c);
    void renderSegment (const Plan& p, juce::int64 t, int offset, int len, Chunk& c);
    juce::AudioFormatReader* readerFor (const juce::File& f);
    std::shared_ptr<const Plan> plan() const;
    juce::int64 mapV (juce::int64 v) const noexcept;
    juce::int64 startPos() const noexcept;
    void requestAt (juce::int64 start) noexcept;

    std::atomic<bool> active { false }, exporting { false }, endReached { false }, playingNow { false };
    mutable juce::SpinLock planLock;
    std::shared_ptr<const Plan> current;
    // copie des réglages utiles au fil audio
    std::atomic<bool> loopOn { false };
    std::atomic<juce::int64> loopA { 0 }, loopB { 0 }, endPos { 0 }, playheadPos { 0 }, tailFrames { 96000 };
    std::atomic<double> bpmNow { 120.0 }, rateNow { 48000.0 };

    std::unique_ptr<Chunk[]> ring;
    std::atomic<std::uint64_t> wr { 0 }, rd { 0 };
    std::atomic<std::uint64_t> req { 0 };        // génération (16 bits) | départ (48 bits)
    // fil audio
    juce::int64 v = 0, lastResync = -1000000000;
    std::uint32_t curGen = 0;
    bool wasPlaying = false;
    std::atomic<juce::int64> shownV { 0 };
    // fil de lecture
    juce::AudioFormatManager formats;
    std::map<juce::String, std::unique_ptr<juce::AudioFormatReader>> readers;
    juce::AudioBuffer<float> scratch { 2, 8192 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MontagePlayer)
};

namespace montage
{
/** Dossier des prises quand le projet n'a pas encore de fichier. */
juce::File sessionFolder();
/** Dossier audio d'un projet : « <nom> - audio » à côté du fichier. */
juce::File audioFolderFor (const juce::File& projectFile);
/** v0.46.1 (audit F1/F2) : fichier de prise acceptable dans un projet (audio, dans le dossier du
    projet ou dans ceux de GOLEM, jamais de chemin réseau ni de « .. »). */
bool takeFileAllowed (const juce::File& file, const juce::File& projectFolder);

juce::ValueTree toTree (const MontageDoc& doc, const juce::File& projectFile);
MontageDoc fromTree (const juce::ValueTree& tree, const juce::File& projectFile);
std::string toJson (const MontageDoc& doc, bool playerOn, double positionBeats, const juce::String& status);
/** Clips envoyés par l'interface : [[voie, prise, src, long., noire, dB, fondu1, fondu2], …]. */
bool clipsFromJson (const juce::String& json, MontageDoc& doc);

/** Range une prise terminée : découpe chaque fichier en clips là où il a sonné, supprime les
    fichiers muets, calcule les crêtes, et pose les clips (en remplaçant ce qui était là).
    Fil quelconque (lent : relit les fichiers). takeStartPpq : position (noires) du début. */
struct TakeResult { std::vector<MontageTake> takes; std::vector<MontageClip> clips; double startBeat = 0.0, endBeat = 0.0; };
TakeResult analyseTake (const juce::File& folder, double placeBeat, double takeBpm, double docBpm, const juce::String& idPrefix,
                        bool wholeSpan = false); // fix12 : vrai = un clip par piste jouée, de toute la prise (boucle)
/** Crêtes en base64 standard (les prises de la fix8 étaient dans le format propre à JUCE). */
juce::String standardPeaks (const juce::String& peaks);
/** Pose les clips d'une prise : ceux qui étaient sur ses voies pendant la prise sont retirés ou rognés. */
void placeTake (MontageDoc& doc, const TakeResult& r);
/** Copie les fichiers des prises dans le dossier audio du projet (s'ils n'y sont pas déjà). */
void bindToProject (MontageDoc& doc, const juce::File& projectFile);
} // namespace montage

} // namespace grv
