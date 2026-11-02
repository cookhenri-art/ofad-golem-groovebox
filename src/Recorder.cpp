/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Enregistrement audio (v0.32) : voir Recorder.h.
*/

#include "Recorder.h"

#include <lame.h>

#include <thread>

namespace grv
{

namespace
{
    constexpr int fifoSeconds = 2;          // réserve par fichier entre le fil audio et le disque
    constexpr int maxBlock = engine::controlBlock;

    // un bloc de silence pour une piste qui ne joue pas pendant ce bloc
    const float silence[maxBlock] {};

    juce::String jsonEscape (const juce::String& s) { return juce::JSON::toString (juce::var (s)); }
}

Recorder::Recorder()
{
    trackStream.fill (-1);
    returnStream.fill (-1);
}

Recorder::~Recorder()
{
    stop();
    encodeAbort.store (true);
    if (encoder != nullptr) encoder->stopThread (20000);
    writerThread.stopThread (5000);
}

juce::File Recorder::rootFolder()
{
   #if JUCE_ANDROID
    // Musique/Groovebox : visible dans le gestionnaire de fichiers (Android 11 et plus) ;
    // sinon le dossier privé de l'application
    auto music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    if (music.getFullPathName().isNotEmpty())
    {
        auto dir = music.getChildFile ("Groovebox");
        if (dir.createDirectory() && dir.hasWriteAccess()) return dir;
    }
    auto priv = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Enregistrements");
    priv.createDirectory();
    return priv;
   #else
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Groovebox").getChildFile ("Enregistrements");
    dir.createDirectory();
    return dir;
   #endif
}

bool Recorder::start (Mode m, Format f, double sampleRate, const std::vector<TrackInfo>& tracks,
                      const std::array<bool, engine::numReturns>& returnsUsed, juce::String& error)
{
    if (active.load()) { error = juce::String::fromUTF8 ("Un enregistrement est déjà en cours"); return false; }
    if (encoding.load()) { error = juce::String::fromUTF8 ("La conversion en MP3 de la prise précédente n'est pas finie"); return false; }
    if (encoder != nullptr) { encoder->stopThread (1000); encoder.reset(); }
    closeAll();

    mode = m; format = f;
    rate = sampleRate > 8000.0 && sampleRate <= 384000.0 ? sampleRate : 48000.0;
    stems = m != Mode::master;
    trackStream.fill (-1); returnStream.fill (-1); masterStream = -1;

    // dossier de la prise : date et heure, jamais un dossier existant (montage : dossier donné)
    juce::File dir;
    if (fixedFolder != juce::File())
    {
        dir = fixedFolder;
        if (! dir.createDirectory()) { error = juce::String::fromUTF8 ("Dossier de la prise impossible à créer"); return false; }
    }
    else
    {
        const auto root = rootFolder();
        if (! root.isDirectory()) { error = juce::String::fromUTF8 ("Dossier des enregistrements impossible à créer"); return false; }
        dir = root.getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d %Hh%Mm%S"));
        if (dir.exists()) dir = dir.getNonexistentSibling (false);
        if (! dir.createDirectory()) { error = juce::String::fromUTF8 ("Dossier de la prise impossible à créer"); return false; }
    }

    if (! writerThread.isThreadRunning()) writerThread.startThread (juce::Thread::Priority::high);

    const auto fifo = juce::jlimit (8192, 1 << 20, (int) (rate * fifoSeconds));
    juce::WavAudioFormat wav;
    juce::FlacAudioFormat flac;
    auto open = [&] (const juce::String& baseName) -> int
    {
        auto file = dir.getChildFile (juce::File::createLegalFileName (baseName) + (format == Format::flac ? ".flac" : ".wav"));
        if (file.exists()) file = file.getNonexistentSibling (false);
        std::unique_ptr<juce::OutputStream> os = file.createOutputStream();
        if (os == nullptr) return -1;
        auto options = juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (24);
        std::unique_ptr<juce::AudioFormatWriter> w;
        if (format == Format::flac) w = flac.createWriterFor (os, options.withQualityOptionIndex (5));
        else w = wav.createWriterFor (os, options.withMetadata (juce::WavAudioFormat::bwavOriginator, "OFAD GOLEM"));
        if (w == nullptr) return -1;
        Stream s;
        s.writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w.release(), writerThread, fifo);
        s.file = file;
        streams.push_back (std::move (s));
        return (int) streams.size() - 1;
    };

    streams.reserve (1 + tracks.size() + engine::numReturns);
    if (m != Mode::stems)
        if ((masterStream = open ("Master")) < 0) { error = juce::String::fromUTF8 ("Écriture impossible dans le dossier des enregistrements"); closeAll(); dir.deleteRecursively(); return false; }
    if (stems)
    {
        for (const auto& t : tracks)
            if (t.index >= 0 && t.index < engine::numTracks)
            {
                const auto name = "P" + juce::String (t.index + 1).paddedLeft ('0', 2) + (! montageNames && t.name.isNotEmpty() ? " " + t.name : juce::String());
                trackStream[(size_t) t.index] = open (name);
            }
        for (int r = 0; r < engine::numReturns; ++r)
            if (returnsUsed[(size_t) r]) returnStream[(size_t) r] = open (montageNames ? "R" + juce::String (r + 1) : "Retour " + juce::String::charToString ((juce::juce_wchar) ('A' + r)));
    }
    if (streams.empty()) { error = juce::String::fromUTF8 ("Aucune piste à enregistrer"); dir.deleteRecursively(); return false; }

    {
        const juce::ScopedLock sl (lock);
        folder = dir;
        files.clear();
        lastError.clear();
    }
    samples.store (0); dropped.store (0); encodeProgress.store (0.0f);
    firstPpq.store (-1.0);
    full.store (false);
    active.store (true); // en dernier : le fil audio peut écrire
    return true;
}

bool Recorder::startMontageTake (const juce::File& dir, double sampleRate, const std::vector<TrackInfo>& tracks,
                                 const std::array<bool, engine::numReturns>& returnsUsed, juce::String& error)
{
    fixedFolder = dir; onlyWhilePlaying = true; montageNames = true;
    const bool ok = start (Mode::stems, Format::flac, sampleRate, tracks, returnsUsed, error);
    fixedFolder = juce::File(); montageNames = false;
    if (! ok) { onlyWhilePlaying = false; takeLimit = -1; }
    return ok;
}

void Recorder::tapBlock (const float* outL, const float* outR,
                         const float* const* tracksL, const float* const* tracksR, std::uint32_t trackMask,
                         const float* const* returnsL, const float* const* returnsR, int n) noexcept
{
    busy.fetch_add (1); // d'abord : stop() attend que ce bloc soit fini
    if (! active.load() || n <= 0 || n > maxBlock) { busy.fetch_sub (1); return; }
    int from = 0; // v0.46 fix11 : premier échantillon écrit du bloc (prise calée sur une mesure)
    // v0.46 fix8 : prise du montage : seulement pendant la lecture. v0.46 fix11 : elle part au
    // début d'une mesure (dans ce bloc, à l'échantillon près) : la prise est calée en rythme
    if (onlyWhilePlaying)
    {
        if (! blockPlaying) { busy.fetch_sub (1); return; }
        if (firstPpq.load (std::memory_order_relaxed) < 0.0)
        {
            const auto bar = std::ceil (blockPpq / 4.0 - 1.0e-7) * 4.0;
            const auto span = blockPpqEnd - blockPpq;
            if (std::abs (blockPpq - bar) < 1.0e-7 || blockPpq < 1.0e-7) from = 0;
            else if (span > 0.0 && bar < blockPpqEnd) from = std::clamp ((int) std::lround ((bar - blockPpq) / span * n), 0, n - 1);
            else { busy.fetch_sub (1); return; } // on attend la mesure suivante
            firstPpq.store (std::max (0.0, blockPpq < 1.0e-7 ? 0.0 : bar), std::memory_order_relaxed);
        }
    }
    // v0.46 fix12 : prise limitée (boucle du montage) : on n'écrit que jusqu'au dernier échantillon
    int count = n - from;
    if (onlyWhilePlaying && takeLimit >= 0)
    {
        const auto left = takeLimit - samples.load (std::memory_order_relaxed);
        if (left <= 0) { full.store (true); busy.fetch_sub (1); return; }
        if (left <= count) { count = (int) left; full.store (true); }
    }
    std::int64_t lost = 0;
    auto put = [&] (int s, const float* l, const float* r)
    {
        if (s < 0) return;
        const float* ch[2] { l + from, r + from };
        if (! streams[(size_t) s].writer->write (ch, count)) lost += count;
    };
    put (masterStream, outL, outR);
    if (stems)
    {
        for (int t = 0; t < engine::numTracks; ++t)
            if (const auto s = trackStream[(size_t) t]; s >= 0)
            {
                const bool on = (trackMask & (1u << t)) != 0;
                put (s, on ? tracksL[t] : silence, on ? tracksR[t] : silence);
            }
        for (int r = 0; r < engine::numReturns; ++r) put (returnStream[(size_t) r], returnsL[r], returnsR[r]);
    }
    samples.fetch_add (count, std::memory_order_relaxed);
    if (lost > 0) dropped.fetch_add (lost, std::memory_order_relaxed);
    busy.fetch_sub (1);
}

void Recorder::stop()
{
    if (! active.exchange (false)) return;
    // le bloc audio en cours finit d'écrire (quelques microsecondes)
    for (int i = 0; busy.load() != 0 && i < 2000; ++i) std::this_thread::sleep_for (std::chrono::microseconds (100));

    juce::StringArray names;
    for (const auto& s : streams) names.add (s.file.getFileName());
    closeAll(); // vide les files et ferme les fichiers (en-têtes WAV complets)
    onlyWhilePlaying = false;
    takeLimit = -1;
    {
        const juce::ScopedLock sl (lock);
        files = names;
    }
    if (format != Format::mp3) return;

    // conversion en MP3 dans un fil à part
    struct EncodeThread final : juce::Thread
    {
        explicit EncodeThread (Recorder& r) : juce::Thread ("Groovebox · conversion MP3"), owner (r) {}
        void run() override { owner.encodeAll(); }
        Recorder& owner;
    };
    encodeAbort.store (false);
    encodeProgress.store (0.0f);
    encoding.store (true);
    encoder = std::make_unique<EncodeThread> (*this);
    encoder->startThread (juce::Thread::Priority::low);
}

void Recorder::closeAll()
{
    // les ThreadedWriter écrivent le reste de leur file puis ferment le fichier
    streams.clear();
    trackStream.fill (-1); returnStream.fill (-1); masterStream = -1;
}

void Recorder::encodeAll()
{
    juce::File dir; juce::StringArray names;
    {
        const juce::ScopedLock sl (lock);
        dir = folder; names = files;
    }
    juce::StringArray done;
    juce::String err;
    for (int i = 0; i < names.size() && ! encodeAbort.load(); ++i)
    {
        const auto wavFile = dir.getChildFile (names[i]);
        const auto mp3File = wavFile.withFileExtension ("mp3");
        const auto base = (float) i / (float) names.size();
        const auto span = 1.0f / (float) names.size();
        juce::String e;
        if (encodeMp3 (wavFile, mp3File, encodeAbort, [&] (float p) { encodeProgress.store (base + span * p); }, e))
        {
            wavFile.deleteFile(); // le WAV de travail n'est plus utile : la prise est en MP3
            done.add (mp3File.getFileName());
        }
        else
        {
            done.add (names[i]); // le WAV reste : rien n'est perdu
            if (err.isEmpty()) err = e;
        }
    }
    {
        const juce::ScopedLock sl (lock);
        files = done;
        if (err.isNotEmpty()) lastError = err;
    }
    encodeProgress.store (1.0f);
    encoding.store (false);
}

bool Recorder::encodeMp3 (const juce::File& wavFile, const juce::File& mp3File, const std::atomic<bool>& abort,
                          const std::function<void (float)>& progress, juce::String& error)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (wavFile.createInputStream().release(), true));
    if (reader == nullptr) { error = juce::String::fromUTF8 ("WAV illisible : ") + wavFile.getFileName(); return false; }
    const auto sr = (int) reader->sampleRate;
    const auto channels = (int) juce::jlimit (1u, 2u, reader->numChannels);
    const auto total = reader->lengthInSamples;

    lame_t gf = lame_init();
    if (gf == nullptr) { error = juce::String::fromUTF8 ("Encodeur MP3 indisponible"); return false; }
    lame_set_num_channels (gf, channels);
    lame_set_in_samplerate (gf, sr);
    if (sr > 48000) lame_set_out_samplerate (gf, 48000); // le MP3 s'arrête à 48 kHz
    lame_set_VBR (gf, vbr_off);
    lame_set_brate (gf, 320);
    lame_set_quality (gf, 2);                              // 0 = le plus lent, 2 = très bonne qualité
    lame_set_mode (gf, channels == 2 ? JOINT_STEREO : MONO);
    lame_set_bWriteVbrTag (gf, 0);                         // débit constant : pas d'en-tête Xing à réécrire
    id3tag_init (gf);
    id3tag_set_title (gf, mp3File.getFileNameWithoutExtension().toRawUTF8());
    id3tag_set_comment (gf, "OFAD GOLEM");
    if (lame_init_params (gf) < 0) { lame_close (gf); error = juce::String::fromUTF8 ("Réglages MP3 refusés"); return false; }

    std::unique_ptr<juce::FileOutputStream> out (mp3File.createOutputStream());
    if (out == nullptr || ! out->openedOk()) { lame_close (gf); error = juce::String::fromUTF8 ("Écriture du MP3 impossible"); return false; }
    out->setPosition (0); out->truncate();

    constexpr int chunk = 4096;
    juce::AudioBuffer<float> buf (2, chunk);
    std::vector<unsigned char> mp3 ((size_t) (chunk * 5 / 4 + 7200));
    bool ok = true;
    for (juce::int64 pos = 0; pos < total && ok; pos += chunk)
    {
        if (abort.load()) { ok = false; error = juce::String::fromUTF8 ("Conversion interrompue"); break; }
        const auto num = (int) std::min<juce::int64> (chunk, total - pos);
        reader->read (&buf, 0, num, pos, true, true);
        const float* r = channels == 2 ? buf.getReadPointer (1) : buf.getReadPointer (0);
        const auto bytes = lame_encode_buffer_ieee_float (gf, buf.getReadPointer (0), r, num, mp3.data(), (int) mp3.size());
        if (bytes < 0 || ! out->write (mp3.data(), (size_t) bytes)) { ok = false; error = juce::String::fromUTF8 ("Erreur de l'encodeur MP3"); }
        if (progress) progress ((float) (pos + num) / (float) std::max<juce::int64> (1, total));
    }
    if (ok)
    {
        const auto bytes = lame_encode_flush (gf, mp3.data(), (int) mp3.size());
        if (bytes < 0 || ! out->write (mp3.data(), (size_t) bytes)) { ok = false; error = juce::String::fromUTF8 ("Erreur de l'encodeur MP3"); }
    }
    lame_close (gf);
    out->flush();
    const bool written = ok && out->getStatus().wasOk();
    out.reset();
    if (! written) mp3File.deleteFile();
    return written;
}

juce::File Recorder::lastFolder() const
{
    const juce::ScopedLock sl (lock);
    return folder;
}

juce::String Recorder::statusJson() const
{
    juce::File dir; juce::StringArray names; juce::String err;
    {
        const juce::ScopedLock sl (lock);
        dir = folder; names = files; err = lastError;
    }
    const auto secs = (double) samples.load() / rate;
    // dossier montré à l'utilisateur : à partir du dossier « Groovebox » (jamais le chemin complet)
    juce::String shown;
    if (dir != juce::File())
    {
        const auto root = dir.getParentDirectory();
        shown = (root.getParentDirectory().getFileName() == "Groovebox" ? "Groovebox/" + root.getFileName() : root.getFileName())
                + "/" + dir.getFileName();
    }
    juce::String fl;
    for (const auto& n : names) fl << (fl.isEmpty() ? "" : ",") << jsonEscape (n);
    return "{\"active\":" + juce::String (active.load() ? "true" : "false")
         + ",\"encoding\":" + juce::String (encoding.load() ? "true" : "false")
         + ",\"progress\":" + juce::String (encodeProgress.load(), 3)
         + ",\"seconds\":" + juce::String (secs, 2)
         + ",\"dropped\":" + juce::String (dropped.load() > 0 ? (double) dropped.load() / rate : 0.0, 3)
         + ",\"mode\":" + juce::String ((int) mode)
         + ",\"format\":\"" + juce::String (format == Format::mp3 ? "mp3" : "wav") + "\""
         + ",\"folder\":" + jsonEscape (shown)
         + ",\"files\":[" + fl + "]"
         + ",\"error\":" + jsonEscape (err)
         + ",\"canReveal\":" + juce::String (
           #if JUCE_ANDROID || JUCE_IOS
             "false"
           #else
             "true"
           #endif
           ) + "}";
}

} // namespace grv
