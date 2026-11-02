/*
    Groovebox — OFAD — AGPL-3.0-or-later
    v0.46 fix8 : montage audio — voir Montage.h.
*/

#include "Montage.h"
#include "Recorder.h"

#include <algorithm>
#include <cmath>

namespace grv
{
using engine::numMontageLanes;
using engine::controlBlock;

namespace
{
constexpr std::uint64_t startMask = (1ull << 48) - 1;
std::uint32_t genOf (std::uint64_t r) noexcept { return (std::uint32_t) (r >> 48); }
juce::int64 startOf (std::uint64_t r) noexcept { return (juce::int64) (r & startMask); }

juce::String laneFileName (int lane)
{
    return lane < engine::numTracks ? "P" + juce::String (lane + 1).paddedLeft ('0', 2) : "R" + juce::String (lane - engine::numTracks + 1);
}
} // namespace

double MontageDoc::endBeat() const noexcept
{
    double e = 0.0;
    for (const auto& c : clips)
        if (const auto* t = takeFor (c.take); t != nullptr && t->rate > 0.0)
            e = std::max (e, c.beat + (double) c.len / t->rate * bpm / 60.0);
    return e;
}

void MontageDoc::pruneTakes()
{
    takes.erase (std::remove_if (takes.begin(), takes.end(), [this] (const MontageTake& t)
    {
        return std::none_of (clips.begin(), clips.end(), [&] (const MontageClip& c) { return c.take == t.id; });
    }), takes.end());
}

//==============================================================================
MontagePlayer::MontagePlayer() : juce::Thread ("Groovebox · lecture du montage")
{
    formats.registerBasicFormats();
    ring.reset (new Chunk[(size_t) ringChunks]);
    auto empty = std::make_shared<Plan>();
    current = empty;
    startThread (juce::Thread::Priority::high);
}

MontagePlayer::~MontagePlayer() { stopReading(); }

void MontagePlayer::stopReading() { stopThread (3000); }

std::shared_ptr<const MontagePlayer::Plan> MontagePlayer::plan() const
{
    const juce::SpinLock::ScopedLockType sl (planLock);
    return current;
}

void MontagePlayer::setDoc (const MontageDoc& doc, double deviceRate)
{
    auto p = std::make_shared<Plan>();
    p->rate = deviceRate > 8000.0 ? deviceRate : 48000.0;
    p->bpm = std::clamp (doc.bpm, 20.0, 400.0);
    const auto spb = p->rate * 60.0 / p->bpm; // échantillons par noire
    std::map<juce::String, int> fileIndex;
    for (const auto& c : doc.clips)
    {
        const auto* t = doc.takeFor (c.take);
        if (t == nullptr || c.len <= 0 || c.lane < 0 || c.lane >= numMontageLanes || t->rate <= 0.0) continue;
        const auto key = t->file.getFullPathName();
        int fi;
        if (auto it = fileIndex.find (key); it != fileIndex.end()) fi = it->second;
        else { fi = (int) p->files.size(); p->files.push_back (t->file); fileIndex[key] = fi; }
        PlanClip pc;
        pc.lane = c.lane; pc.take = fi;
        pc.ratio = t->rate / p->rate;
        pc.start = (juce::int64) std::llround (std::max (0.0, c.beat) * spb);
        pc.len = (juce::int64) std::floor ((double) c.len / pc.ratio);
        pc.src = (double) c.src;
        pc.gain = juce::Decibels::decibelsToGain (std::clamp (c.gainDb, -60.0f, 12.0f), -60.0f);
        pc.fadeIn = std::max<juce::int64> (1, (juce::int64) (std::clamp (c.fadeIn, 0.0f, 30.0f) * p->rate));
        pc.fadeOut = std::max<juce::int64> (1, (juce::int64) (std::clamp (c.fadeOut, 0.0f, 30.0f) * p->rate));
        p->end = std::max (p->end, pc.start + pc.len);
        p->clips.push_back (pc);
    }
    std::sort (p->clips.begin(), p->clips.end(), [] (const PlanClip& a, const PlanClip& b) { return a.start < b.start; });
    p->loopA = (juce::int64) std::llround (std::max (0.0, doc.loopA) * spb);
    p->loopB = (juce::int64) std::llround (std::max (0.0, doc.loopB) * spb);
    p->loop = doc.loop && p->loopB - p->loopA >= (juce::int64) (spb * 0.25);
    p->playhead = (juce::int64) std::llround (std::max (0.0, doc.playhead) * spb);

    const bool loopChanged = loopOn.load() != p->loop || loopA.load() != p->loopA || loopB.load() != p->loopB;
    const bool headChanged = playheadPos.load() != p->playhead;
    const bool rateChanged = std::abs (rateNow.load() - p->rate) > 0.5 || std::abs (bpmNow.load() - p->bpm) > 1.0e-6;
    {
        const juce::SpinLock::ScopedLockType sl (planLock);
        current = p;
    }
    loopOn.store (p->loop); loopA.store (p->loopA); loopB.store (p->loopB);
    endPos.store (p->end); playheadPos.store (p->playhead);
    tailFrames.store ((juce::int64) (p->rate * 2.0));
    bpmNow.store (p->bpm); rateNow.store (p->rate);
    // À l'arrêt : tout est relu depuis le point de départ (une modification s'entend tout de suite).
    // En lecture : ce qui est déjà lu d'avance (≈ 1 s) finit de jouer, puis la modification s'entend ;
    // boucle ou tempo changés en lecture : on relit un peu plus loin (50 ms).
    if (! playingNow.load()) requestAt (startPos());
    else if (loopChanged || rateChanged) requestAt (shownV.load() + (juce::int64) (p->rate * 0.05));
    juce::ignoreUnused (headChanged);
    notify();
}

void MontagePlayer::setPlayhead (double beat)
{
    const auto spb = rateNow.load() * 60.0 / bpmNow.load();
    playheadPos.store ((juce::int64) std::llround (std::max (0.0, beat) * spb));
    requestAt (startPos()); // en lecture, la lecture saute là ; à l'arrêt, on lit d'avance depuis là
    notify();
}

double MontagePlayer::positionBeats() const noexcept
{
    const auto spb = rateNow.load() * 60.0 / bpmNow.load();
    return spb > 0.0 ? (double) mapV (shownV.load()) / spb : 0.0;
}

juce::int64 MontagePlayer::mapV (juce::int64 x) const noexcept
{
    if (! loopOn.load (std::memory_order_relaxed) || exporting.load (std::memory_order_relaxed)) return x;
    const auto a = loopA.load (std::memory_order_relaxed), b = loopB.load (std::memory_order_relaxed);
    if (b <= a || x < b) return x;
    return a + (x - a) % (b - a);
}

juce::int64 MontagePlayer::startPos() const noexcept
{
    const auto ph = playheadPos.load (std::memory_order_relaxed);
    if (loopOn.load (std::memory_order_relaxed) && ! exporting.load (std::memory_order_relaxed)
        && ph >= loopB.load (std::memory_order_relaxed)) return loopA.load (std::memory_order_relaxed);
    return std::max<juce::int64> (0, ph);
}

void MontagePlayer::requestAt (juce::int64 start) noexcept
{
    auto r = req.load();
    std::uint64_t next;
    do
    {
        const auto g = (std::uint64_t) ((genOf (r) + 1) & 0xffff);
        next = (g << 48) | ((std::uint64_t) std::max<juce::int64> (0, start) & startMask);
    } while (! req.compare_exchange_weak (r, next));
}

//==============================================================================
// Fil audio
void MontagePlayer::pull (bool playing, int n, float (*laneL)[controlBlock], float (*laneR)[controlBlock],
                          std::uint32_t& laneMask) noexcept
{
    laneMask = 0;
    for (int l = 0; l < numMontageLanes; ++l) { std::fill (laneL[l], laneL[l] + n, 0.0f); std::fill (laneR[l], laneR[l] + n, 0.0f); }
    const auto r = req.load (std::memory_order_acquire);
    const auto gen = genOf (r);
    auto rdv = rd.load (std::memory_order_relaxed);
    const auto wrv = wr.load (std::memory_order_acquire);
    // les morceaux lus pour une ancienne demande sont jetés (même à l'arrêt : la file se libère)
    while (rdv < wrv && ring[(size_t) (rdv % ringChunks)].gen != gen) ++rdv;

    playingNow.store (playing, std::memory_order_relaxed);
    if (! playing)
    {
        if (wasPlaying)
        {
            wasPlaying = false;
            requestAt (startPos()); // arrêt : la lecture repartira du point de départ
        }
        rd.store (rdv, std::memory_order_release);
        shownV.store (startPos(), std::memory_order_relaxed);
        return;
    }
    if (! wasPlaying || gen != curGen)
    {
        // départ de la lecture, ou saut demandé (point de départ déplacé en lecture)
        wasPlaying = true;
        curGen = gen;
        v = startOf (r);
        endReached.store (false, std::memory_order_relaxed);
    }

    int done = 0;
    while (done < n && rdv < wrv)
    {
        auto& c = ring[(size_t) (rdv % ringChunks)];
        if (c.gen != gen) { ++rdv; continue; }
        const auto at = v + done;
        if (c.start + chunkFrames <= at) { ++rdv; continue; }
        if (c.start > at) { done += (int) std::min<juce::int64> (n - done, c.start - at); continue; } // trou : silence
        const auto off = (int) (at - c.start);
        const auto take = std::min (n - done, chunkFrames - off);
        if (c.mask != 0)
            for (int l = 0; l < numMontageLanes; ++l)
                if (c.mask & (1u << l))
                {
                    std::copy (c.data[l][0] + off, c.data[l][0] + off + take, laneL[l] + done);
                    std::copy (c.data[l][1] + off, c.data[l][1] + off + take, laneR[l] + done);
                }
        laneMask |= c.mask;
        done += take;
        if (off + take == chunkFrames) ++rdv;
    }
    if (done < n)
    {
        // le fil de lecture n'a pas suivi (disque lent) : silence, et on lui demande de reprendre
        // un peu plus loin (0,1 s) ; au plus une demande par demi-seconde
        const auto sr = rateNow.load (std::memory_order_relaxed);
        if (v - lastResync > (juce::int64) (sr * 0.5))
        {
            lastResync = v;
            requestAt (v + n + (juce::int64) (sr * 0.1));
        }
    }
    v += n;
    rd.store (rdv, std::memory_order_release);
    shownV.store (v, std::memory_order_relaxed);
    if (exporting.load (std::memory_order_relaxed) && v >= endPos.load (std::memory_order_relaxed) + tailFrames.load (std::memory_order_relaxed))
        endReached.store (true, std::memory_order_relaxed);
}

//==============================================================================
// Fil de lecture
void MontagePlayer::run()
{
    std::uint32_t myGen = 0xffffffffu;
    juce::int64 writeV = 0;
    while (! threadShouldExit())
    {
        auto p = plan();
        int made = 0;
        for (;;)
        {
            const auto r = req.load (std::memory_order_acquire);
            // nouvelle demande : la liste des clips est relue (elle est publiée avant la demande)
            if (genOf (r) != myGen) { myGen = genOf (r); writeV = startOf (r); p = plan(); }
            const auto w = wr.load (std::memory_order_relaxed);
            if (w - rd.load (std::memory_order_acquire) >= (std::uint64_t) ringChunks || made >= 24 || threadShouldExit()) break;
            auto& c = ring[(size_t) (w % ringChunks)];
            render (*p, writeV, c);
            c.gen = myGen; c.start = writeV;
            wr.store (w + 1, std::memory_order_release);
            writeV += chunkFrames;
            ++made;
        }
        wait (made > 0 ? 1 : 4);
    }
}

void MontagePlayer::render (const Plan& p, juce::int64 vStart, Chunk& c)
{
    c.mask = 0;
    int done = 0;
    while (done < chunkFrames)
    {
        const auto vv = vStart + done;
        const auto t = mapV (vv);
        int len = chunkFrames - done;
        if (loopOn.load() && ! exporting.load() && t < loopB.load() && vv >= loopA.load())
            len = (int) std::min<juce::int64> (len, loopB.load() - t);
        len = std::max (1, len);
        renderSegment (p, t, done, len, c);
        done += len;
    }
    // voies sans clip dans ce morceau : à zéro (le fil audio ne les copie pas)
    for (int l = 0; l < numMontageLanes; ++l)
        if ((c.mask & (1u << l)) == 0) { std::fill (c.data[l][0], c.data[l][0] + chunkFrames, 0.0f); std::fill (c.data[l][1], c.data[l][1] + chunkFrames, 0.0f); }
}

void MontagePlayer::renderSegment (const Plan& p, juce::int64 t, int offset, int len, Chunk& c)
{
    const auto t1 = t + len;
    for (const auto& pc : p.clips)
    {
        if (pc.start >= t1) break;              // triés par début
        if (pc.start + pc.len <= t) continue;
        if (pc.take < 0 || pc.take >= (int) p.files.size()) continue;
        auto* reader = readerFor (p.files[(size_t) pc.take]);
        if (reader == nullptr) continue;
        const auto a = std::max (t, pc.start), b = std::min (t1, pc.start + pc.len);
        const auto count = (int) (b - a);
        if (count <= 0) continue;
        const int lane = pc.lane;
        if ((c.mask & (1u << lane)) == 0)
        {
            std::fill (c.data[lane][0] + 0, c.data[lane][0] + chunkFrames, 0.0f);
            std::fill (c.data[lane][1] + 0, c.data[lane][1] + chunkFrames, 0.0f);
            c.mask |= 1u << lane;
        }
        // lecture du fichier (au besoin, conversion de fréquence en ligne droite)
        const double s0 = pc.src + (double) (a - pc.start) * pc.ratio;
        const auto first = (juce::int64) std::floor (s0);
        const int need = (int) std::ceil ((double) count * pc.ratio) + 3;
        if (scratch.getNumSamples() < need) scratch.setSize (2, need, false, false, true);
        scratch.clear();
        const auto avail = (int) std::max<juce::int64> (0, std::min<juce::int64> (need, reader->lengthInSamples - first));
        if (avail > 0) reader->read (&scratch, 0, avail, first, true, reader->numChannels > 1);
        if (reader->numChannels == 1) scratch.copyFrom (1, 0, scratch, 0, 0, need);
        const float* sl = scratch.getReadPointer (0);
        const float* sr = scratch.getReadPointer (1);
        float* dl = c.data[lane][0] + offset + (int) (a - t);
        float* dr = c.data[lane][1] + offset + (int) (a - t);
        for (int i = 0; i < count; ++i)
        {
            const auto pos = (a - pc.start) + i;
            float env = pc.gain;
            if (pos < pc.fadeIn) env *= (float) pos / (float) pc.fadeIn;
            if (pc.len - pos < pc.fadeOut) env *= (float) (pc.len - pos) / (float) pc.fadeOut;
            float l, r;
            if (pc.ratio == 1.0) { l = sl[i]; r = sr[i]; } // départ entier : s0 == first
            else
            {
                const double x = s0 + (double) i * pc.ratio - (double) first;
                const auto k = (int) x;
                const auto f = (float) (x - (double) k);
                l = sl[k] + (sl[k + 1] - sl[k]) * f;
                r = sr[k] + (sr[k + 1] - sr[k]) * f;
            }
            dl[i] += l * env;
            dr[i] += r * env;
        }
    }
}

juce::AudioFormatReader* MontagePlayer::readerFor (const juce::File& f)
{
    const auto key = f.getFullPathName();
    if (auto it = readers.find (key); it != readers.end()) return it->second.get();
    std::unique_ptr<juce::AudioFormatReader> r (f.existsAsFile() ? formats.createReaderFor (f) : nullptr);
    auto* raw = r.get();
    readers[key] = std::move (r); // un fichier absent n'est pas recherché à chaque morceau
    if (readers.size() > 256) { readers.clear(); return nullptr; }
    return raw;
}

//==============================================================================
namespace montage
{
juce::File sessionFolder()
{
    static const juce::File dir = []
    {
       #if JUCE_ANDROID
        auto root = Recorder::rootFolder().getChildFile ("Montages");
       #else
        auto root = Recorder::rootFolder().getParentDirectory().getChildFile ("Montages");
       #endif
        auto d = root.getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d %Hh%Mm%S"));
        if (d.exists()) d = d.getNonexistentSibling (false);
        return d;
    }();
    return dir;
}

juce::File audioFolderFor (const juce::File& projectFile)
{
    return projectFile.getSiblingFile (projectFile.getFileNameWithoutExtension() + " - audio");
}

juce::ValueTree toTree (const MontageDoc& doc, const juce::File& projectFile)
{
    juce::ValueTree m ("MONTAGE");
    m.setProperty ("bpm", doc.bpm, nullptr);
    if (doc.loop) m.setProperty ("loop", true, nullptr);
    m.setProperty ("loopA", doc.loopA, nullptr);
    m.setProperty ("loopB", doc.loopB, nullptr);
    m.setProperty ("playhead", doc.playhead, nullptr);
    const auto base = projectFile != juce::File() ? projectFile.getParentDirectory() : juce::File();
    for (const auto& t : doc.takes)
    {
        juce::ValueTree tt ("TAKE");
        tt.setProperty ("id", t.id, nullptr);
        // chemin relatif au projet quand le fichier est dans son dossier (le projet se déplace avec)
        const bool rel = base != juce::File() && t.file.isAChildOf (base);
        tt.setProperty ("file", rel ? t.file.getRelativePathFrom (base).replaceCharacter ('\\', '/') : t.file.getFullPathName(), nullptr);
        if (rel) tt.setProperty ("rel", true, nullptr);
        tt.setProperty ("lane", t.lane, nullptr);
        tt.setProperty ("rate", t.rate, nullptr);
        tt.setProperty ("len", (juce::int64) t.length, nullptr);
        tt.setProperty ("peaks", t.peaks, nullptr);
        m.appendChild (tt, nullptr);
    }
    for (const auto& c : doc.clips)
    {
        juce::ValueTree cc ("CLIP");
        cc.setProperty ("lane", c.lane, nullptr);
        cc.setProperty ("take", c.take, nullptr);
        cc.setProperty ("src", (juce::int64) c.src, nullptr);
        cc.setProperty ("len", (juce::int64) c.len, nullptr);
        cc.setProperty ("beat", c.beat, nullptr);
        if (c.gainDb != 0.0f) cc.setProperty ("gain", c.gainDb, nullptr);
        cc.setProperty ("fin", c.fadeIn, nullptr);
        cc.setProperty ("fout", c.fadeOut, nullptr);
        m.appendChild (cc, nullptr);
    }
    return m;
}

MontageDoc fromTree (const juce::ValueTree& m, const juce::File& projectFile)
{
    MontageDoc doc;
    if (! m.isValid() || ! m.hasType ("MONTAGE")) return doc;
    doc.bpm = std::clamp ((double) m.getProperty ("bpm", 120.0), 20.0, 400.0);
    doc.loop = (bool) m.getProperty ("loop", false);
    doc.loopA = std::max (0.0, (double) m.getProperty ("loopA", 0.0));
    doc.loopB = std::max (doc.loopA, (double) m.getProperty ("loopB", 16.0));
    doc.playhead = std::max (0.0, (double) m.getProperty ("playhead", 0.0));
    const auto base = projectFile != juce::File() ? projectFile.getParentDirectory() : juce::File();
    for (const auto& c : m)
    {
        if (c.hasType ("TAKE") && doc.takes.size() < 4096)
        {
            MontageTake t;
            t.id = c.getProperty ("id").toString().substring (0, 64);
            const auto path = c.getProperty ("file").toString();
            if ((bool) c.getProperty ("rel", false))
            {
                if (base == juce::File()) continue; // projet relu sans son dossier : fichiers introuvables
                t.file = base.getChildFile (path);
            }
            else if (juce::File::isAbsolutePath (path)) t.file = juce::File (path);
            else continue;
            t.lane = juce::jlimit (0, numMontageLanes - 1, (int) c.getProperty ("lane", 0));
            t.rate = std::clamp ((double) c.getProperty ("rate", 48000.0), 8000.0, 384000.0);
            t.length = std::max<juce::int64> (0, (juce::int64) c.getProperty ("len", 0));
            t.peaks = standardPeaks (c.getProperty ("peaks").toString().substring (0, 2000000));
            if (t.id.isNotEmpty()) doc.takes.push_back (t);
        }
        else if (c.hasType ("CLIP") && doc.clips.size() < 20000)
        {
            MontageClip k;
            k.lane = juce::jlimit (0, numMontageLanes - 1, (int) c.getProperty ("lane", 0));
            k.take = c.getProperty ("take").toString().substring (0, 64);
            k.src = std::max<juce::int64> (0, (juce::int64) c.getProperty ("src", 0));
            k.len = std::max<juce::int64> (0, (juce::int64) c.getProperty ("len", 0));
            k.beat = std::max (0.0, (double) c.getProperty ("beat", 0.0));
            k.gainDb = std::clamp ((float) (double) c.getProperty ("gain", 0.0), -60.0f, 12.0f);
            k.fadeIn = std::clamp ((float) (double) c.getProperty ("fin", 0.004), 0.0f, 30.0f);
            k.fadeOut = std::clamp ((float) (double) c.getProperty ("fout", 0.004), 0.0f, 30.0f);
            if (k.len > 0 && doc.takeFor (k.take) != nullptr) doc.clips.push_back (k);
        }
    }
    return doc;
}

std::string toJson (const MontageDoc& doc, bool playerOn, double positionBeats, const juce::String& status)
{
    auto num = [] (double x) { char b[32]; std::snprintf (b, sizeof b, "%.6g", std::isfinite (x) ? x : 0.0); return std::string (b); };
    std::string o = "{\"on\":" + std::string (playerOn ? "true" : "false") + ",\"bpm\":" + num (doc.bpm)
                  + ",\"loop\":" + (doc.loop ? "true" : "false") + ",\"loopA\":" + num (doc.loopA) + ",\"loopB\":" + num (doc.loopB)
                  + ",\"playhead\":" + num (doc.playhead) + ",\"pos\":" + num (positionBeats)
                  + ",\"status\":" + juce::JSON::toString (status).toStdString() + ",\"takes\":[";
    bool first = true;
    for (const auto& t : doc.takes)
    {
        if (! first) o += ",";
        first = false;
        o += "[" + juce::JSON::toString (t.id).toStdString() + "," + std::to_string (t.lane) + "," + num (t.rate) + "," + std::to_string (t.length)
           + "," + (t.file.existsAsFile() ? "1" : "0") + "]";
    }
    o += "],\"clips\":[";
    first = true;
    for (const auto& c : doc.clips)
    {
        if (! first) o += ",";
        first = false;
        o += "[" + std::to_string (c.lane) + "," + juce::JSON::toString (c.take).toStdString() + "," + std::to_string (c.src) + "," + std::to_string (c.len)
           + "," + num (c.beat) + "," + num (c.gainDb) + "," + num (c.fadeIn) + "," + num (c.fadeOut) + "]";
    }
    o += "]}";
    return o;
}

bool clipsFromJson (const juce::String& json, MontageDoc& doc)
{
    const auto parsed = juce::JSON::parse (json);
    const auto* arr = parsed.getArray();
    if (arr == nullptr) return false;
    std::vector<MontageClip> clips;
    for (const auto& item : *arr)
    {
        const auto* a = item.getArray();
        if (a == nullptr || a->size() < 8 || clips.size() >= 20000) continue;
        MontageClip c;
        c.lane = juce::jlimit (0, numMontageLanes - 1, (int) (*a)[0]);
        c.take = (*a)[1].toString().substring (0, 64);
        const auto* t = doc.takeFor (c.take);
        if (t == nullptr) continue;
        c.src = juce::jlimit<juce::int64> (0, std::max<juce::int64> (0, t->length - 1), (juce::int64) (double) (*a)[2]);
        c.len = juce::jlimit<juce::int64> (0, t->length - c.src, (juce::int64) (double) (*a)[3]);
        c.beat = std::clamp ((double) (*a)[4], 0.0, 1.0e6);
        c.gainDb = std::clamp ((float) (double) (*a)[5], -60.0f, 12.0f);
        c.fadeIn = std::clamp ((float) (double) (*a)[6], 0.0f, 30.0f);
        c.fadeOut = std::clamp ((float) (double) (*a)[7], 0.0f, 30.0f);
        if (c.len > 0) clips.push_back (c);
    }
    doc.clips = std::move (clips);
    return true;
}

juce::String standardPeaks (const juce::String& peaks)
{
    // format de JUCE (« taille.données ») : relu puis remis en base64 standard (lisible par l'interface)
    if (! peaks.containsChar ('.')) return peaks;
    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (peaks)) return {};
    return juce::Base64::toBase64 (mb.getData(), mb.getSize());
}

TakeResult analyseTake (const juce::File& folder, double placeBeat, double takeBpm, double docBpm, const juce::String& idPrefix,
                        bool wholeSpan)
{
    TakeResult res;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    juce::ignoreUnused (takeBpm);
    res.startBeat = std::max (0.0, placeBeat); // la prise se pose au point de départ du montage
    res.endBeat = res.startBeat;
    for (int lane = 0; lane < numMontageLanes; ++lane)
    {
        const auto file = folder.getChildFile (laneFileName (lane) + ".flac");
        if (! file.existsAsFile()) continue;
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0) continue;
        const auto rate = reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;
        const auto total = reader->lengthInSamples;
        res.endBeat = std::max (res.endBeat, res.startBeat + (double) total / rate * docBpm / 60.0);
        // fenêtres de 512 échantillons : crête de chacune (seuil : -66 dB)
        constexpr int win = 512;
        const auto numWin = (int) ((total + win - 1) / win);
        std::vector<std::uint8_t> active ((size_t) numWin, 0);
        std::vector<float> peaksWin ((size_t) numWin, 0.0f);
        juce::AudioBuffer<float> buf (2, win * 64);
        for (juce::int64 pos = 0; pos < total; pos += win * 64)
        {
            const auto num = (int) std::min<juce::int64> (win * 64, total - pos);
            buf.clear();
            reader->read (&buf, 0, num, pos, true, reader->numChannels > 1);
            for (int w0 = 0; w0 < num; w0 += win)
            {
                float pk = 0.0f;
                for (int ch = 0; ch < (reader->numChannels > 1 ? 2 : 1); ++ch)
                    pk = std::max (pk, buf.getMagnitude (ch, w0, std::min (win, num - w0)));
                const auto wi = (size_t) ((pos + w0) / win);
                if (wi < peaksWin.size()) { peaksWin[wi] = pk; active[wi] = pk > 0.0005f ? 1 : 0; }
            }
        }
        // passages joués : un silence de moins d'une mesure ne coupe pas le clip
        const auto gapWins = std::max (1, (int) std::ceil (4.0 * 60.0 / takeBpm * rate / win));
        std::vector<std::pair<int, int>> segs; // [début, fin) en fenêtres
        int i = 0;
        while (i < numWin)
        {
            if (! active[(size_t) i]) { ++i; continue; }
            int s = i, e = i + 1, quiet = 0;
            for (int k = i + 1; k < numWin; ++k)
            {
                if (active[(size_t) k]) { e = k + 1; quiet = 0; }
                else if (++quiet > gapWins) break;
            }
            segs.push_back ({ std::max (0, s - 1), std::min (numWin, e + 1) });
            i = e + 1;
        }
        // v0.46 fix12 : prise de boucle : la piste jouée devient un seul clip de toute la boucle (calé pile)
        if (wholeSpan && ! segs.empty()) segs.assign (1, { 0, numWin });
        if (segs.empty())
        {
            reader.reset();
            file.deleteFile(); // piste muette pendant toute la prise : pas de fichier inutile
            continue;
        }
        // crêtes pour l'affichage (un octet par bloc de montagePeakFrames)
        juce::MemoryBlock pk;
        const auto per = montagePeakFrames / win;
        for (int w = 0; w < numWin; w += per)
        {
            float m = 0.0f;
            for (int k = w; k < std::min (numWin, w + per); ++k) m = std::max (m, peaksWin[(size_t) k]);
            const auto b = (std::uint8_t) std::clamp ((int) std::lround (std::sqrt (std::min (1.0f, m)) * 255.0f), 0, 255);
            pk.append (&b, 1);
        }
        MontageTake t;
        t.id = idPrefix + "-" + laneFileName (lane);
        t.file = file;
        t.lane = lane;
        t.rate = rate;
        t.length = total;
        t.peaks = juce::Base64::toBase64 (pk.getData(), pk.getSize());
        res.takes.push_back (t);
        for (const auto& [s, e] : segs)
        {
            MontageClip c;
            c.lane = lane;
            c.take = t.id;
            c.src = (juce::int64) s * win;
            c.len = std::min<juce::int64> (total, (juce::int64) e * win) - c.src;
            c.beat = res.startBeat + (double) c.src / rate * docBpm / 60.0;
            c.fadeIn = c.fadeOut = 0.004f;
            if (c.len > 0) res.clips.push_back (c);
        }
    }
    return res;
}

void placeTake (MontageDoc& doc, const TakeResult& r)
{
    // voies de la prise : ce qui était sur la même voie pendant la prise disparaît (comme une bande)
    std::vector<bool> lanes ((size_t) numMontageLanes, false);
    for (const auto& t : r.takes) lanes[(size_t) t.lane] = true;
    std::vector<MontageClip> kept;
    const auto a = r.startBeat, b = r.endBeat;
    for (const auto& c : doc.clips)
    {
        const auto* t = doc.takeFor (c.take);
        if (t == nullptr) continue;
        if (! lanes[(size_t) c.lane]) { kept.push_back (c); continue; }
        const auto beatsPerSample = doc.bpm / 60.0 / t->rate;
        const auto c0 = c.beat, c1 = c.beat + (double) c.len * beatsPerSample;
        if (c1 <= a || c0 >= b) { kept.push_back (c); continue; }
        if (c0 < a)
        {
            auto left = c;
            left.len = (juce::int64) ((a - c0) / beatsPerSample);
            if (left.len > 0) kept.push_back (left);
        }
        if (c1 > b)
        {
            auto right = c;
            const auto cut = (juce::int64) ((b - c0) / beatsPerSample);
            right.src = c.src + cut;
            right.len = c.len - cut;
            right.beat = b;
            if (right.len > 0) kept.push_back (right);
        }
    }
    for (const auto& t : r.takes) doc.takes.push_back (t);
    for (const auto& c : r.clips) kept.push_back (c);
    doc.clips = std::move (kept); // les prises sans clip restent (annuler les retrouve) ; retirées à l'enregistrement du projet
}

void bindToProject (MontageDoc& doc, const juce::File& projectFile)
{
    if (projectFile == juce::File() || doc.takes.empty()) return;
    const auto dest = audioFolderFor (projectFile);
    for (auto& t : doc.takes)
    {
        if (! t.file.existsAsFile() || t.file.isAChildOf (dest)) continue;
        // « Prise … / P01.flac » garde son dossier de prise
        auto target = dest.getChildFile (t.file.getParentDirectory().getFileName()).getChildFile (t.file.getFileName());
        if (target.existsAsFile() && target.getSize() == t.file.getSize()) { t.file = target; continue; }
        if (target.existsAsFile()) target = target.getNonexistentSibling (false);
        if (target.getParentDirectory().createDirectory() && t.file.copyFileTo (target)) t.file = target;
    }
}
} // namespace montage

} // namespace grv
