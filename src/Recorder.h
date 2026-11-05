/*
    Groovebox — OFAD — AGPL-3.0-or-later

    v0.32 : enregistrement audio de ce qu'on joue (boucles, scènes ou mode morceau).

    - Ce qui est enregistré : le master (ce qu'on entend), chaque piste séparément (après ses
      effets et son volume) avec les retours d'effets, ou les deux.
    - Formats : WAV 24 bits (qualité studio, à la fréquence de la carte son) ou MP3 320 kbit/s
      (encodeur LAME, qualité 2). Le MP3 est fait APRÈS l'arrêt, à partir des WAV, dans un fil à
      part : rien de lourd pendant qu'on joue.
    - Fil audio : aucune allocation, aucun verrou, aucun accès disque. Les échantillons passent
      par la file sans verrou de juce::AudioFormatWriter::ThreadedWriter (≈ 2 s de réserve par
      fichier) ; un fil d'écriture vide les files sur le disque. Si le disque ne suit pas, les
      échantillons perdus sont comptés et signalés (jamais de blocage du son).
    - Dossier : <Documents>/Groovebox/Enregistrements (ordinateur) ; <Musique>/Groovebox
      (Android), sinon le dossier privé de l'application. Un sous-dossier daté par prise.
      L'interface ne reçoit que des noms, jamais de chemin absolu ; elle ne peut pas choisir
      le dossier.
*/

#pragma once

#include "engine/Engine.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace grv
{

class Recorder final : public engine::AudioTap
{
public:
    enum class Mode { master = 0, stems, both };
    enum class Format { wav = 0, mp3, flac };   // flac (v0.46 fix8) : prises du montage audio

    Recorder();
    ~Recorder() override;

    struct TrackInfo { int index = 0; juce::String name; };

    /** Fil de l'interface. Ouvre les fichiers puis arme la prise. Faux (et message) si impossible. */
    bool start (Mode mode, Format format, double sampleRate, const std::vector<TrackInfo>& tracks,
                const std::array<bool, engine::numReturns>& returnsUsed, juce::String& error);

    /** v0.46 fix8 : prise du montage audio — pistes séparées en FLAC dans « folder », écrites
        seulement pendant la lecture ; la position (noires) du premier échantillon écrit est gardée.
        Les noms de fichiers : « P01.flac » … « P16.flac », « R1.flac » … « R4.flac ». */
    bool startMontageTake (const juce::File& folder, double sampleRate, const std::vector<TrackInfo>& tracks,
                           const std::array<bool, engine::numReturns>& returnsUsed, juce::String& error);
    /** Position (noires) du premier échantillon écrit de la prise ; < 0 si rien n'a été écrit. */
    double takeStartPpq() const noexcept { return firstPpq.load(); }
    std::int64_t takeSamples() const noexcept { return samples.load(); }
    /** v0.46 fix12 : longueur maximale de la prise du montage (échantillons ; < 0 : sans limite). À
        régler AVANT startMontageTake. La prise s'arrête d'elle-même à l'échantillon près (boucle). */
    void setTakeLimit (std::int64_t frames) noexcept { takeLimit = frames; }
    /** Vrai quand la prise limitée a tout écrit (le fil de l'interface la range alors). */
    bool takeFull() const noexcept { return full.load(); }
    double takeRate() const noexcept { return rate; }

    /** Fil de l'interface. Arrête la prise, ferme les fichiers ; en MP3, lance la conversion. */
    void stop();

    /** v0.46.2 : prise calée sur la lecture (à régler AVANT start) : rien n'est écrit tant que la
        lecture ne joue pas ; la prise part au premier échantillon joué (ou à « startPpq » si la
        lecture tourne déjà : début du prochain tour de boucle), et s'arrête d'elle-même, à
        l'échantillon près, quand la lecture s'arrête (fin du morceau, ARRÊT) ou à la fin du tour
        de boucle demandé par requestStopAtCycle. takeFull() dit alors qu'il faut la ranger. */
    void setFollowPlayback (bool on, double startPpq = -1.0) noexcept { onlyWhilePlaying = on; startAt.store (on ? startPpq : -1.0); loopMode.store (false); }
    /** v0.46.2 : « boucle parfaite » (à régler AVANT start, avec setFollowPlayback) : la prise attend
        qu'on lui donne le début d'un tour (setLoopStart, fil de l'interface, lecture en cours), laisse
        donc passer un tour d'élan (queues d'échos, compression en place), puis enregistre exactement
        un tour et s'arrête d'elle-même. */
    void setLoopTake (bool on) noexcept { loopMode.store (on); startAt.store (-1.0); }
    bool wantsLoopStart() const noexcept { return active.load() && loopMode.load() && firstPpq.load() < 0.0 && startAt.load() < 0.0; }
    void setLoopStart (double startPpq, double cycleQuarters) noexcept { loopLen.store (cycleQuarters); startAt.store (startPpq); }
    bool followsPlayback() const noexcept { return followMode.load(); }
    /** Fil de l'interface : la prise calée s'arrêtera à la fin du tour de boucle en cours (durée du
        tour en noires, comptée depuis le début de la prise ; ≤ 0 : la mesure). Faux si la prise n'a
        pas encore commencé ou n'est pas calée : il faut alors l'arrêter tout de suite. */
    bool requestStopAtCycle (double cycleQuarters) noexcept;
    bool stopPending() const noexcept { return stopAtPpq.load() >= 0.0; }

    bool isRecording() const noexcept { return active.load(); }
    bool isEncoding() const noexcept { return encoding.load(); }

    /** État pour l'interface (JSON) : prise, durée, fichiers, conversion, pertes, dossier. */
    /** v0.46 fix20 : dossier de la dernière prise, tel que l'utilisateur le retrouve (téléphone :
        « Mes fichiers › Audio › Groovebox › … »). Fil des messages. */
    juce::String lastFolderShown() const;
    static juce::String describeFolder (const juce::File& dir);
    juce::String statusJson() const;

    /** Dossier de la dernière prise (pour l'ouvrir dans l'explorateur). */
    juce::File lastFolder() const;

    /** Dossier racine des enregistrements (créé à la demande). */
    static juce::File rootFolder();

    /** Conversion WAV → MP3 (320 kbit/s, qualité 2). Publique pour les outils de contrôle. */
    static bool encodeMp3 (const juce::File& wav, const juce::File& mp3, const std::atomic<bool>& abort,
                           const std::function<void (float)>& progress, juce::String& error);

    // engine::AudioTap (fil audio)
    bool wantsStems() const noexcept override { return active.load (std::memory_order_relaxed) && stems; }
    void tapPosition (double ppq, double ppqEnd, bool playing) noexcept override
    {
        blockPpq = ppq; blockPpqEnd = ppqEnd; blockPlaying = playing;
        nowPpq.store (ppq, std::memory_order_relaxed); nowPlaying.store (playing, std::memory_order_relaxed);
    }
    /** v0.46.2 : fin du morceau à cette position (fil audio, avant tapPosition/tapBlock du bloc). */
    void tapStopAt (double ppq) noexcept override;
    void tapBlock (const float* outL, const float* outR,
                   const float* const* tracksL, const float* const* tracksR, std::uint32_t trackMask,
                   const float* const* returnsL, const float* const* returnsR, int n) noexcept override;

private:
    struct Stream
    {
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
        juce::File file;
    };

    void closeAll();
    void encodeAll();

    juce::TimeSliceThread writerThread { "Groovebox · enregistrement audio" };
    std::vector<Stream> streams;                       // fixé avant d'armer, inchangé pendant la prise
    int masterStream = -1;
    std::array<int, engine::numTracks> trackStream {};
    std::array<int, engine::numReturns> returnStream {};
    bool stems = false;
    bool onlyWhilePlaying = false;                     // v0.46 fix8 : prise du montage
    juce::File fixedFolder;                            // vide : dossier daté dans rootFolder()
    bool montageNames = false;
    std::atomic<double> firstPpq { -1.0 };
    std::int64_t takeLimit = -1;                       // v0.46 fix12 : fixé avant d'armer
    std::atomic<bool> full { false };
    double blockPpq = 0.0, blockPpqEnd = 0.0;          // fil audio
    bool blockPlaying = false;
    std::atomic<double> startAt { -1.0 };              // v0.46.2 : début prévu (prise calée, boucle parfaite)
    std::atomic<bool> loopMode { false };              // v0.46.2 : boucle parfaite (un tour d'élan, un tour pris)
    std::atomic<double> loopLen { 0.0 };
    std::atomic<std::int64_t> loopFrames { -1 };       // v0.46.2 : longueur exacte d'un tour (échantillons)
    std::atomic<std::int64_t> loopExtra { 0 };         // v0.46.2 : prise un peu plus longue : de quoi fondre la jonction
    /** v0.46.2 : jonction sans saut : les premiers échantillons de la boucle passent en fondu depuis la
        vraie suite du dernier (prise au-delà du tour), puis le fichier est ramené à un tour exact. */
    static bool seamLoop (const juce::File& wavFile, std::int64_t frames, std::int64_t extra);
    std::atomic<bool> followMode { false };            // v0.46.2 : prise calée en cours
    std::atomic<double> stopAtPpq { -1.0 };            // v0.46.2 : arrêt demandé (noires) ; < 0 : aucun
    std::atomic<double> nowPpq { 0.0 };                // v0.46.2 : position lue par le fil de l'interface
    std::atomic<bool> nowPlaying { false };

    std::atomic<bool> active { false };
    std::atomic<int> busy { 0 };                       // blocs du fil audio en cours d'écriture
    std::atomic<std::int64_t> samples { 0 };
    std::atomic<std::int64_t> dropped { 0 };
    double rate = 48000.0;
    Format format = Format::wav;
    Mode mode = Mode::master;

    mutable juce::CriticalSection lock;                // état partagé avec le fil de conversion
    juce::File folder;
    juce::StringArray files;                           // noms des fichiers de la dernière prise
    juce::String lastError;

    std::unique_ptr<juce::Thread> encoder;
    std::atomic<bool> encoding { false };
    std::atomic<bool> encodeAbort { false };
    std::atomic<float> encodeProgress { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Recorder)
};

} // namespace grv
