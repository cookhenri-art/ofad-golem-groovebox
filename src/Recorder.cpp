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

    /** 1.1 (lot 3) : écrivain « traité pendant la prise ». Le fil d'écriture lui passe le son brut ;
        il le fait passer dans la chaîne (versions directes des outils), retire la latence au début et
        la rend à la fin (silence poussé à la fermeture) : le fichier a pile la longueur de la prise,
        calé au premier échantillon. Jamais dans le fil audio. */
    class ProcessingWriter final : public juce::AudioFormatWriter
    {
    public:
        ProcessingWriter (std::unique_ptr<juce::AudioFormatWriter> w, const std::vector<engine::ChainSlot>& chain, double sr, double bpm)
            : juce::AudioFormatWriter (nullptr, "GOLEM", sr, 2, 32), inner (std::move (w))
        {
            usesFloatingPointData = true;
            ready = inner != nullptr && oc.prepare (chain, sr, bpm, false).empty();
            if (ready) pending = oc.streamStart();
        }
        ~ProcessingWriter() override
        {
            // fin de la prise : la latence encore dans la chaîne ressort (silence en entrée)
            while (ready && pending > 0)
            {
                const int m = std::min (chunk, pending);
                std::fill (l, l + m, 0.0f); std::fill (r, r + m, 0.0f);
                const int k = oc.streamProcess (l, r, m);
                emit (k); pending -= m;
            }
            inner.reset();
        }
        bool write (const int** data, int numSamples) override
        {
            if (inner == nullptr) return false;
            const auto** f = reinterpret_cast<const float**> (data);
            const float* sl = f[0];
            const float* sr2 = f[1] != nullptr ? f[1] : f[0];
            for (int i = 0; i < numSamples; i += chunk)
            {
                const int m = std::min (chunk, numSamples - i);
                std::copy (sl + i, sl + i + m, l); std::copy (sr2 + i, sr2 + i + m, r);
                const int k = ready ? oc.streamProcess (l, r, m) : m;
                if (! emit (k)) return false;
            }
            return true;
        }
        bool flush() override { return inner != nullptr && inner->flush(); }
    private:
        bool emit (int k) { if (k <= 0) return true; const float* ch[2] { l, r }; return inner->writeFromFloatArrays (ch, 2, k); }
        static constexpr int chunk = 1024;
        std::unique_ptr<juce::AudioFormatWriter> inner;
        engine::OfflineChain oc;
        bool ready = false;
        int pending = 0;
        float l[chunk] {}, r[chunk] {};
    };

    bool chainActive (const std::vector<engine::ChainSlot>& c)
    {
        for (const auto& x : c) if (! x.type.empty() && ! x.bypass) return true;
        return false;
    }
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
    if (processing.load()) { error = juce::String::fromUTF8 ("Le traitement de la prise précédente n'est pas fini"); return false; }
    if (encoder != nullptr) { encoder->stopThread (1000); encoder.reset(); }
    closeAll();

    mode = m; format = f;
    rate = sampleRate > 8000.0 && sampleRate <= 384000.0 ? sampleRate : 48000.0;
    const bool custom = opt.custom && fixedFolder == juce::File();
    const bool withChain = custom && chainActive (opt.chain);
    stems = custom ? (opt.trackMask != 0 || opt.returnMask != 0) : m != Mode::master;
    trackStream.fill (-1); returnStream.fill (-1); masterStream = -1;
    masterTreatedStream = -1; sumStream = -1; inputStream = -1;

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
    const int bits = custom && opt.float32 && format == Format::wav ? 32 : 24;
    auto open = [&] (const juce::String& baseName, int kind = 0, bool treated = false) -> int
    {
        auto file = dir.getChildFile (juce::File::createLegalFileName (baseName) + (format == Format::flac ? ".flac" : ".wav"));
        if (file.exists()) file = file.getNonexistentSibling (false);
        std::unique_ptr<juce::OutputStream> os = file.createOutputStream();
        if (os == nullptr) return -1;
        auto options = juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (bits);
        std::unique_ptr<juce::AudioFormatWriter> w;
        if (format == Format::flac) w = flac.createWriterFor (os, options.withQualityOptionIndex (5));
        else w = wav.createWriterFor (os, options.withMetadata (juce::WavAudioFormat::bwavOriginator, "OFAD GOLEM"));
        if (w == nullptr) return -1;
        if (treated) w = std::make_unique<ProcessingWriter> (std::move (w), opt.chain, rate, opt.bpm); // 1.1 : traité au fil de l'écriture
        Stream s;
        s.writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w.release(), writerThread, fifo);
        s.file = file;
        s.label = baseName; s.kind = kind; s.treated = treated;
        streams.push_back (std::move (s));
        return (int) streams.size() - 1;
    };
    const auto writeError = juce::String::fromUTF8 ("Écriture impossible dans le dossier des enregistrements");

    streams.reserve (2 + tracks.size() + engine::numReturns);
    if (custom)
    {
        // 1.1 (lot 3) : les sources cochées. Traité « pendant » : le fichier est écrit traité ; « après » :
        // on écrit le brut (gardé), traité à l'arrêt
        const bool during = withChain && opt.during;
        const bool wantMaster = opt.master || (opt.masterTreated && ! withChain) || (opt.masterTreated && ! during);
        if (wantMaster && (masterStream = open ("Master", 0)) < 0) { error = writeError; closeAll(); dir.deleteRecursively(); return false; }
        if (opt.masterTreated && during && (masterTreatedStream = open (juce::String::fromUTF8 ("Master traité"), 1, true)) < 0) { error = writeError; closeAll(); dir.deleteRecursively(); return false; }
        bool anyTrack = false;
        for (const auto& t : tracks)
            if (t.index >= 0 && t.index < engine::numTracks && (opt.trackMask & (1u << t.index)) != 0)
            {
                anyTrack = true;
                if (opt.sumTracks) continue;
                trackStream[(size_t) t.index] = open ("P" + juce::String (t.index + 1).paddedLeft ('0', 2) + (t.name.isNotEmpty() ? " " + t.name : juce::String()), 2, during);
            }
        if (anyTrack && opt.sumTracks) sumStream = open ("Pistes", 3, during);
        for (int r = 0; r < engine::numReturns; ++r)
            if ((opt.returnMask & (1u << r)) != 0 && returnsUsed[(size_t) r])
                returnStream[(size_t) r] = open ("Retour " + juce::String::charToString ((juce::juce_wchar) ('A' + r)), 4, during);
        if (opt.input) inputStream = open (juce::String::fromUTF8 ("Entrée"), 5, during);
        stems = anyTrack;
        for (auto x : returnStream) if (x >= 0) stems = true;
    }
    else
    {
    if (m != Mode::stems)
        if ((masterStream = open ("Master")) < 0) { error = writeError; closeAll(); dir.deleteRecursively(); return false; }
    if (stems)
    {
        for (const auto& t : tracks)
            if (t.index >= 0 && t.index < engine::numTracks)
            {
                const auto name = "P" + juce::String (t.index + 1).paddedLeft ('0', 2) + (! montageNames && t.name.isNotEmpty() ? " " + t.name : juce::String());
                trackStream[(size_t) t.index] = open (name);
            }
        for (int r = 0; r < engine::numReturns; ++r)
            if (returnsUsed[(size_t) r]) returnStream[(size_t) r] = open (montageNames ? "R" + juce::String (r + 1) : "Retour " + juce::String::charToString ((juce::juce_wchar) ('A' + r)), 4);
    }
    }
    if (streams.empty()) { error = juce::String::fromUTF8 ("Aucune piste à enregistrer"); dir.deleteRecursively(); return false; }

    {
        const juce::ScopedLock sl (lock);
        folder = dir;
        files.clear();
        lastError.clear();
    }
    {
        const juce::ScopedLock sl (lock);
        treatedFiles.clear(); treatedShown.clear(); treatedDir = juce::File(); result = juce::File(); taken.clear();
    }
    barsLimit.store (custom && onlyWhilePlaying ? opt.barsQuarters : 0.0);
    if (! custom) opt = TakeOptions {};
    samples.store (0); dropped.store (0); encodeProgress.store (0.0f);
    firstPpq.store (-1.0);
    full.store (false);
    stopAtPpq.store (-1.0);
    followMode.store (onlyWhilePlaying && fixedFolder == juce::File()); // la prise du montage a sa propre fin
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
        if (! blockPlaying)
        {
            // v0.46.2 : prise calée : la lecture s'est arrêtée (ARRÊT) : la prise est finie
            if (followMode.load (std::memory_order_relaxed) && firstPpq.load (std::memory_order_relaxed) >= 0.0) full.store (true);
            else if (startAt.load (std::memory_order_relaxed) >= 0.0) startAt.store (-1.0, std::memory_order_relaxed); // arrêtée avant le départ prévu : on repartira avec la lecture
            busy.fetch_sub (1); return;
        }
        if (full.load (std::memory_order_relaxed)) { busy.fetch_sub (1); return; } // déjà finie : on attend qu'on la range
        if (firstPpq.load (std::memory_order_relaxed) < 0.0)
        {
            auto bar = std::ceil (blockPpq / 4.0 - 1.0e-7) * 4.0;
            auto at = startAt.load (std::memory_order_relaxed);
            // v0.46.2 : boucle parfaite : on attend le début de tour donné par le fil de l'interface ; s'il
            // est déjà passé (rare : lecture qui saute), on prend le tour suivant
            if (loopMode.load (std::memory_order_relaxed))
            {
                if (at < 0.0) { busy.fetch_sub (1); return; }
                const auto len = loopLen.load (std::memory_order_relaxed);
                if (len > 1.0e-6) while (at < blockPpq - 1.0e-7) at += len;
                startAt.store (at, std::memory_order_relaxed);
            }
            // v0.46.2 : prise calée armée pendant la lecture : départ au prochain tour de boucle
            if (at >= 0.0 && at >= blockPpq - 1.0e-7) bar = at;
            const auto span = blockPpqEnd - blockPpq;
            if (at <= -1.5) { from = 0; bar = blockPpq; } // export : dès le premier échantillon joué
            else if (std::abs (blockPpq - bar) < 1.0e-7 || (blockPpq < 1.0e-7 && at < 0.0)) from = 0;
            else if (span > 0.0 && bar < blockPpqEnd) from = std::clamp ((int) std::lround ((bar - blockPpq) / span * n), 0, n - 1);
            else { busy.fetch_sub (1); return; } // on attend la mesure suivante
            firstPpq.store (std::max (0.0, blockPpq < 1.0e-7 && at < 0.0 ? 0.0 : bar), std::memory_order_relaxed);
            // 1.1 (lot 3) : prise de N mesures : la fin est posée dès le premier échantillon
            if (const auto q = barsLimit.load (std::memory_order_relaxed); q > 1.0e-6 && ! loopMode.load (std::memory_order_relaxed))
                stopAtPpq.store (firstPpq.load (std::memory_order_relaxed) + q, std::memory_order_relaxed);
            // boucle parfaite : exactement un tour, plus 10 ms (la suite réelle, pour fondre la jonction)
            if (loopMode.load (std::memory_order_relaxed))
                if (const auto len = loopLen.load (std::memory_order_relaxed); len > 1.0e-6 && span > 0.0)
                {
                    const auto framesPerQ = (double) n / span;
                    const auto extra = (std::int64_t) std::llround (0.010 * rate);
                    loopFrames.store ((std::int64_t) std::llround (len * framesPerQ), std::memory_order_relaxed);
                    loopExtra.store (extra, std::memory_order_relaxed);
                    stopAtPpq.store (firstPpq.load (std::memory_order_relaxed) + len + (double) extra / framesPerQ, std::memory_order_relaxed);
                }
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
    // v0.46.2 : arrêt demandé (fin du tour de boucle, fin du morceau) : pile à l'échantillon
    if (onlyWhilePlaying)
        if (const auto stopAt = stopAtPpq.load (std::memory_order_relaxed); stopAt >= 0.0 && blockPpqEnd > stopAt + 1.0e-9)
        {
            const auto span = blockPpqEnd - blockPpq;
            const int end = stopAt <= blockPpq || span <= 0.0 ? 0 : std::clamp ((int) std::lround ((stopAt - blockPpq) / span * n), 0, n);
            full.store (true);
            count = std::min (count, end - from);
            if (count <= 0) { busy.fetch_sub (1); return; }
        }
    std::int64_t lost = 0;
    auto put = [&] (int s, const float* l, const float* r)
    {
        if (s < 0) return;
        const float* ch[2] { l + from, r + from };
        if (! streams[(size_t) s].writer->write (ch, count)) lost += count;
    };
    put (masterStream, outL, outR);
    put (masterTreatedStream, outL, outR);
    if (inputStream >= 0) { if (inPtrL != nullptr) put (inputStream, inPtrL, inPtrR); else put (inputStream, silence, silence); }
    if (sumStream >= 0 && stems)
    {
        // 1.1 (lot 3) : une seule piste, somme des pistes cochées
        std::fill (sumL.begin(), sumL.begin() + n, 0.0f); std::fill (sumR.begin(), sumR.begin() + n, 0.0f);
        for (int t = 0; t < engine::numTracks; ++t)
            if ((opt.trackMask & (1u << t)) != 0 && (trackMask & (1u << t)) != 0)
                for (int i = 0; i < n; ++i) { sumL[(size_t) i] += tracksL[t][i]; sumR[(size_t) i] += tracksR[t][i]; }
        put (sumStream, sumL.data(), sumR.data());
    }
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

bool Recorder::requestStopAtCycle (double cycleQuarters) noexcept
{
    if (! active.load() || ! followMode.load() || ! nowPlaying.load()) return false;
    const auto first = firstPpq.load();
    if (first < 0.0) return false;
    const auto cycle = cycleQuarters > 1.0e-6 ? cycleQuarters : 4.0;
    // le tour en cours se finit ; une prise d'au moins un tour entier
    const auto k = std::max (1.0, std::ceil ((nowPpq.load() - first) / cycle - 1.0e-7));
    const auto target = first + k * cycle;
    const auto prev = stopAtPpq.load();
    stopAtPpq.store (prev >= 0.0 ? std::min (prev, target) : target);
    return true;
}

void Recorder::tapStopAt (double ppq) noexcept
{
    if (! active.load (std::memory_order_relaxed) || ! followMode.load (std::memory_order_relaxed)
        || firstPpq.load (std::memory_order_relaxed) < 0.0) return;
    const auto prev = stopAtPpq.load (std::memory_order_relaxed);
    stopAtPpq.store (prev >= 0.0 ? std::min (prev, ppq) : ppq, std::memory_order_relaxed);
}

bool Recorder::seamLoop (const juce::File& wavFile, std::int64_t frames, std::int64_t extra)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (wavFile.createInputStream().release(), true));
    if (reader == nullptr || frames <= 0 || reader->lengthInSamples < frames || frames > (std::int64_t) 64 * 1024 * 1024) return false;
    const auto total = reader->lengthInSamples;
    const auto channels = (int) juce::jlimit (1u, 2u, reader->numChannels);
    juce::AudioBuffer<float> buf (channels, (int) total);
    reader->read (&buf, 0, (int) total, 0, true, channels > 1);
    const auto sr = reader->sampleRate;
    const auto bits = (int) reader->bitsPerSample;
    reader.reset();
    // fondu de la jonction : au début, la vraie suite de la fin (échantillons au-delà du tour) cède la
    // place, en k échantillons, au début du tour : la fin s'enchaîne sur le début sans saut
    const auto k = (int) std::min<std::int64_t> ({ extra, total - frames, frames / 2 });
    for (int c = 0; c < channels; ++c)
    {
        auto* d = buf.getWritePointer (c);
        for (int i = 0; i < k; ++i)
        {
            const auto w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * ((float) i + 0.5f) / (float) k);
            d[i] = d[(size_t) frames + (size_t) i] * (1.0f - w) + d[i] * w;
        }
    }
    const auto tmp = wavFile.getSiblingFile (wavFile.getFileNameWithoutExtension() + ".tmp");
    bool ok = false;
    {
        std::unique_ptr<juce::OutputStream> os = tmp.createOutputStream();
        if (os != nullptr)
        {
            auto options = juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (channels).withBitsPerSample (bits > 0 ? bits : 24)
                                                           .withMetadata (juce::WavAudioFormat::bwavOriginator, "OFAD GOLEM");
            if (auto w = wav.createWriterFor (os, options))
                ok = w->writeFromAudioSampleBuffer (buf, 0, (int) frames);
        }
    }
    ok = ok && tmp.moveFileTo (wavFile);
    if (! ok) tmp.deleteFile();
    return ok;
}

void Recorder::stop()
{
    if (! active.exchange (false)) return;
    // le bloc audio en cours finit d'écrire (quelques microsecondes)
    for (int i = 0; busy.load() != 0 && i < 2000; ++i) std::this_thread::sleep_for (std::chrono::microseconds (100));

    juce::StringArray names;
    juce::Array<juce::File> written;
    std::vector<Taken> took;
    for (const auto& s : streams) { names.add (s.file.getFileName()); written.add (s.file); took.push_back ({ s.file, s.label, s.kind, s.treated }); }
    const bool wasLoop = loopMode.load() && loopFrames.load() > 0;
    const bool wasFollow = followMode.load();
    closeAll(); // vide les files et ferme les fichiers (en-têtes WAV complets)
    // v0.46.2 : boucle parfaite : jonction fondue, longueur ramenée à un tour exact
    if (loopMode.load() && loopFrames.load() > 0 && format != Format::flac)
        for (const auto& f : written) seamLoop (f, loopFrames.load(), loopExtra.load());
    barsLimit.store (0.0);
    loopFrames.store (-1);
    onlyWhilePlaying = false;
    takeLimit = -1;
    startAt.store (-1.0);
    loopMode.store (false);
    followMode.store (false);
    stopAtPpq.store (-1.0);
    {
        const juce::ScopedLock sl (lock);
        files = names;
        taken = std::move (took);
        takenLoop = wasLoop; takenKeepLength = wasFollow;
    }
    // 1.1 (lot 3) : prise de l'écran AUDIO : traitement, rangement, MP3 dans un fil à part
    post = std::move (opt); opt = TakeOptions {};
    doneDest.store (post.custom ? post.dest : 0);
    if (post.custom)
    {
        if (post.dest >= 2 && format == Format::mp3) format = Format::wav; // un pad se charge en WAV
        struct PostThread final : juce::Thread
        {
            explicit PostThread (Recorder& r) : juce::Thread ("Groovebox · traitement de la prise"), owner (r) {}
            void run() override { owner.postAll(); }
            Recorder& owner;
        };
        encodeAbort.store (false);
        encodeProgress.store (0.0f);
        processing.store (true);
        encoder = std::make_unique<PostThread> (*this);
        encoder->startThread (juce::Thread::Priority::low);
        return;
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

//==============================================================================
// 1.1 (lot 3) : la prise de l'écran AUDIO, après l'arrêt (fil à part)
bool Recorder::processFile (const juce::File& raw, const juce::File& out, bool loop, bool keepLength, float base, float span, juce::String& err)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (raw.createInputStream().release(), true));
    if (reader == nullptr) { err = juce::String::fromUTF8 ("Prise illisible : ") + raw.getFileName(); return false; }
    const auto total = reader->lengthInSamples;
    const auto sr = reader->sampleRate;
    if (total <= 0) return false;
    if (total > (juce::int64) (sr * 600.0)) { err = juce::String::fromUTF8 ("Prise trop longue pour le traitement (10 minutes au plus)"); return false; }
    const int n = (int) total;
    const auto bits = (int) reader->bitsPerSample;
    juce::AudioBuffer<float> buf (2, n);
    reader->read (&buf, 0, n, 0, true, true);
    reader.reset();
    engine::OfflineChain oc;
    if (const auto e = oc.prepare (post.chain, sr, post.bpm, true); ! e.empty()) { err = juce::String::fromUTF8 (e.c_str()); return false; }
    std::atomic<float> prog { 0.0f };
    std::atomic<bool> done { false };
    std::thread watch ([&] { while (! done.load()) { encodeProgress.store (base + span * prog.load()); std::this_thread::sleep_for (std::chrono::milliseconds (100)); } });
    auto res = oc.render (buf.getReadPointer (0), buf.getReadPointer (1), n, loop || ! keepLength, 20.0, &prog, &encodeAbort);
    done.store (true); watch.join();
    if (! res.error.empty()) { err = juce::String::fromUTF8 (res.error.c_str()); return false; }
    if (res.right.empty()) res.right = res.left;
    // boucle : la queue (échos, réverbération) revient au début : la boucle traitée se recolle sans trou
    if (loop && (int) res.left.size() > n)
        for (int i = n; i < (int) res.left.size(); ++i) { res.left[(size_t) (i % n)] += res.left[(size_t) i]; res.right[(size_t) (i % n)] += res.right[(size_t) i]; }
    if (loop || keepLength) { res.left.resize ((size_t) n); res.right.resize ((size_t) n); }
    const auto m = (int) res.left.size();
    std::unique_ptr<juce::OutputStream> os = out.createOutputStream();
    if (os == nullptr) { err = juce::String::fromUTF8 ("Écriture impossible : ") + out.getFileName(); return false; }
    auto options = juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (bits == 32 ? 32 : 24)
                                                   .withMetadata (juce::WavAudioFormat::bwavOriginator, "OFAD GOLEM");
    auto w = wav.createWriterFor (os, options);
    if (w == nullptr) { err = juce::String::fromUTF8 ("Écriture impossible : ") + out.getFileName(); return false; }
    const float* ch[2] { res.left.data(), res.right.data() };
    return w->writeFromFloatArrays (ch, 2, m);
}

void Recorder::postAll()
{
    std::vector<Taken> list; juce::File dir; bool loop = false, keepLength = false;
    {
        const juce::ScopedLock sl (lock);
        list = taken; dir = folder; loop = takenLoop; keepLength = takenKeepLength;
    }
    const bool withChain = chainActive (post.chain);
    // dossier des fichiers traités : la prise elle-même, ou « Son traité/<date> »
    juce::File outDir = dir;
    if (post.dest != 0 && post.treatedRoot != juce::File())
    {
        outDir = post.treatedRoot.getChildFile (dir.getFileName());
        if (outDir.exists()) outDir = outDir.getNonexistentSibling (false);
        if (! outDir.createDirectory()) outDir = dir;
    }
    const auto secs = list.empty() ? 0.0 : (double) samples.load() / rate;
    const auto bars = secs * post.bpm / 240.0;
    const auto barsRound = std::round (bars);
    auto niceName = [&] (const juce::String& label, bool treated)
    {
        juce::StringArray parts;
        if (post.project.isNotEmpty()) parts.add (post.project);
        parts.add (label == juce::String::fromUTF8 ("Master traité") ? juce::String ("Master") : label);
        parts.add (juce::String (juce::roundToInt (post.bpm)) + " BPM");
        if (barsRound >= 1.0 && std::abs (bars - barsRound) < 0.02 * barsRound) parts.add (juce::String ((int) barsRound) + (barsRound > 1.0 ? " mesures" : " mesure"));
        if (treated) parts.add (juce::String::fromUTF8 ("traité"));
        auto f = outDir.getChildFile (juce::File::createLegalFileName (parts.joinIntoString (juce::String::fromUTF8 (" â "))) + ".wav");
        return f.exists() ? f.getNonexistentSibling (false) : f;
    };
    juce::StringArray outNames; juce::Array<juce::File> outFiles;
    juce::File best; int bestRank = -1;
    auto offer = [&] (const juce::File& f, int kind) // le fichier du pad : somme > piste > master traité > master
    {
        const int rank = kind == 3 ? 4 : kind == 2 || kind == 5 ? 3 : kind == 1 ? 2 : kind == 4 ? 1 : 0;
        if (rank > bestRank) { bestRank = rank; best = f; }
    };
    juce::String err;
    // ce qu'il faut traiter après la prise : tout sauf le master brut (seulement si « Master traité » est coché)
    std::vector<const Taken*> todo;
    for (const auto& t : list)
        if (withChain && ! t.treated && (t.kind != 0 || post.masterTreated) && ! (t.kind == 0 && post.during)) todo.push_back (&t);
    int k = 0;
    const auto count = (float) std::max<size_t> (1, todo.size());
    for (const auto& t : list)
    {
        if (encodeAbort.load()) break;
        const bool doIt = std::find (todo.begin(), todo.end(), &t) != todo.end();
        if (t.treated)
        {
            // déjà traité au fil de l'eau : il prend son nom et sa place
            const auto f = niceName (t.label, true);
            if (t.file.moveFileTo (f)) { outNames.add (f.getFileName()); outFiles.add (f); offer (f, t.kind); }
            else { outNames.add (t.file.getFileName()); outFiles.add (t.file); offer (t.file, t.kind); }
            continue;
        }
        if (doIt)
        {
            const auto f = niceName (t.label, true);
            juce::String e;
            if (processFile (t.file, f, loop, keepLength, (float) k / count, 1.0f / count, e)) { outNames.add (f.getFileName()); outFiles.add (f); offer (f, t.kind == 0 ? 1 : t.kind); }
            else { f.deleteFile(); if (err.isEmpty()) err = e; }
            ++k;
            continue;
        }
        // brut : sans chaîne et vers « Son traité », une copie bien nommée ; sinon il reste dans la prise
        if (! withChain && post.dest == 1 && outDir != dir)
        {
            const auto f = niceName (t.label, false);
            if (t.file.copyFileTo (f)) { outNames.add (f.getFileName()); outFiles.add (f); }
        }
        offer (t.file, t.kind);
    }
    {
        const juce::ScopedLock sl (lock);
        treatedDir = outDir; treatedFiles = outNames; treatedShown = describeFolder (outDir);
        result = best;
        if (err.isNotEmpty()) lastError = err;
    }
    processing.store (false);
    if (format != Format::mp3 || encodeAbort.load()) { encodeProgress.store (1.0f); return; }
    // MP3 : la prise brute (comme avant), puis les fichiers traités
    encoding.store (true);
    encodeAll();
    encoding.store (true);
    juce::StringArray done;
    for (int i = 0; i < outFiles.size() && ! encodeAbort.load(); ++i)
    {
        const auto mp3File = outFiles[i].withFileExtension ("mp3");
        juce::String e;
        if (encodeMp3 (outFiles[i], mp3File, encodeAbort, [&] (float p) { encodeProgress.store (((float) i + p) / (float) outFiles.size()); }, e))
        { outFiles[i].deleteFile(); done.add (mp3File.getFileName()); }
        else done.add (outFiles[i].getFileName());
    }
    {
        const juce::ScopedLock sl (lock);
        treatedFiles = done;
    }
    encodeProgress.store (1.0f);
    encoding.store (false);
}

juce::File Recorder::resultFile() const
{
    const juce::ScopedLock sl (lock);
    return processing.load() ? juce::File() : result;
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
    // v0.46.2 : en-tête « Info » de LAME (réécrit à la fin) : il dit le retard de l'encodeur et le
    // remplissage final ; les logiciels qui le lisent rendent le MP3 sans blanc ajouté (boucles).
    // Étiquette ID3 v1 seulement (à la fin) : l'en-tête reste au tout début du fichier.
    lame_set_bWriteVbrTag (gf, 1);
    id3tag_init (gf);
    id3tag_v1_only (gf);
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
    if (ok)
    {
        // l'en-tête « Info » prend la place de la trame vide réservée au début du fichier
        std::vector<unsigned char> tag (2880);
        const auto tagBytes = lame_get_lametag_frame (gf, tag.data(), tag.size());
        if (tagBytes > 0 && tagBytes <= tag.size())
        {
            out->flush();
            if (! out->setPosition (0) || ! out->write (tag.data(), tagBytes)) { ok = false; error = juce::String::fromUTF8 ("Écriture du MP3 impossible"); }
        }
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

// v0.46 fix20 : dossier montré à l'utilisateur, tel qu'il le retrouve (jamais le chemin complet).
// Téléphone : dans le dossier Musique, que les gestionnaires de fichiers rangent sous « Audio ».
juce::String Recorder::describeFolder (const juce::File& dir)
{
    if (dir == juce::File()) return {};
   #if JUCE_ANDROID
    const auto music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    if (music != juce::File() && dir.isAChildOf (music))
        return juce::String::fromUTF8 ("Mes fichiers \xE2\x80\xBA Audio \xE2\x80\xBA ") + dir.getRelativePathFrom (music).replaceCharacter ('\\', '/');
    return juce::String::fromUTF8 ("dossier priv\xC3\xA9 de l'application \xE2\x80\x94 ") + dir.getFileName();
   #else
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    if (dir.isAChildOf (docs))
        return juce::String::fromUTF8 ("Documents/") + dir.getRelativePathFrom (docs).replaceCharacter ('\\', '/');
    const auto root = dir.getParentDirectory();
    return (root.getParentDirectory().getFileName() == "Groovebox" ? "Groovebox/" + root.getFileName() : root.getFileName()) + "/" + dir.getFileName();
   #endif
}

juce::String Recorder::lastFolderShown() const
{
    juce::File dir;
    { const juce::ScopedLock sl (lock); dir = folder; }
    return describeFolder (dir);
}

juce::String Recorder::statusJson() const
{
    juce::File dir; juce::StringArray names, tnames; juce::String err, tShown; bool hasRes = false;
    {
        const juce::ScopedLock sl (lock);
        dir = folder; names = files; err = lastError; tnames = treatedFiles; tShown = treatedShown; hasRes = result != juce::File() && result.existsAsFile();
    }
    juce::String tl;
    for (const auto& n : tnames) tl << (tl.isEmpty() ? "" : ",") << jsonEscape (n);
    const auto secs = (double) samples.load() / rate;
    const auto shown = describeFolder (dir);
    juce::String fl;
    for (const auto& n : names) fl << (fl.isEmpty() ? "" : ",") << jsonEscape (n);
    return "{\"active\":" + juce::String (active.load() ? "true" : "false")
         + ",\"encoding\":" + juce::String (encoding.load() ? "true" : "false")
         + ",\"progress\":" + juce::String (encodeProgress.load(), 3)
         + ",\"seconds\":" + juce::String (secs, 2)
         + ",\"dropped\":" + juce::String (dropped.load() > 0 ? (double) dropped.load() / rate : 0.0, 3)
         + ",\"mode\":" + juce::String ((int) mode)
         + ",\"format\":\"" + juce::String (format == Format::mp3 ? "mp3" : "wav") + "\""
         + ",\"follow\":" + juce::String (followMode.load() ? "true" : "false")
         + ",\"waiting\":" + juce::String (active.load() && followMode.load() && firstPpq.load() < 0.0 ? "true" : "false")
         + ",\"stopping\":" + juce::String (active.load() && stopAtPpq.load() >= 0.0 ? "true" : "false")
         + ",\"folder\":" + jsonEscape (shown)
         + ",\"files\":[" + fl + "]"
         + ",\"processing\":" + juce::String (processing.load() ? "true" : "false")
         + ",\"treatedFolder\":" + jsonEscape (tShown)
         + ",\"treated\":[" + tl + "]"
         + ",\"hasResult\":" + juce::String (hasRes ? "true" : "false")
         + ",\"dest\":" + juce::String (doneDest.load())
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
