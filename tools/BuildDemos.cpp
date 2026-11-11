/*
    Groovebox — OFAD — AGPL-3.0-or-later
    1.1 : fabrique des projets de démonstration à partir d'une description JSON (une par style),
    puis les écoute « aux instruments » : rendu hors temps réel du morceau entier, sonie intégrée,
    crête vraie, sonie de chaque section et de chaque piste seule, équilibre des bandes, charge CPU.
    Écrit aussi le morceau en fichier MIDI (src/engine/MidiFile.h).

    Usage : groovebox_demos <description.json> <sortie.gbx> [--wav fichier.wav] [--mid fichier.mid]
                                                        [--tracks] [--bench]
*/

#include "PluginProcessor.h"
#include "dsp/Loudness.h"
#include "engine/MidiFile.h"

#include <cstdlib>
#include <cstdio>
#include <map>

namespace
{
using namespace grv;
using namespace grv::engine;

juce::var load (const juce::File& f)
{
    juce::var v;
    if (const auto r = juce::JSON::parse (f.loadFileAsString(), v); r.failed()) { std::printf ("JSON illisible : %s\n", r.getErrorMessage().toRawUTF8()); std::exit (2); }
    return v;
}

int patternIndex (const juce::var& v)
{
    if (v.isInt() || v.isDouble()) return (int) v;
    const auto s = v.toString();
    if (s == "stop") return songStop;
    if (s == "keep" || s.isEmpty()) return songKeep;
    return juce::jlimit (0, Sequencer::numPatterns - 1, (int) (s[0] - 'A'));
}

Division divisionFrom (const juce::String& s)
{
    static const std::map<juce::String, Division> m { { "1/8", Division::eighth }, { "1/8T", Division::eighthTriplet }, { "1/16", Division::sixteenth },
        { "1/16T", Division::sixteenthTriplet }, { "1/32", Division::thirtySecond }, { "1/4", Division::quarter }, { "1/32T", Division::thirtySecondTriplet }, { "1/64", Division::sixtyFourth } };
    const auto it = m.find (s);
    return it != m.end() ? it->second : Division::sixteenth;
}

struct Builder
{
    GrooveboxProcessor& proc;
    Engine& e;
    int errors = 0;

    int param (int ch, const juce::String& area, const juce::String& id)
    {
        const auto g = e.findParam (ch, area.toStdString(), id.toStdString());
        if (g < 0) { std::printf ("  ! réglage inconnu : canal %d %s.%s\n", ch, area.toRawUTF8(), id.toRawUTF8()); ++errors; }
        return g;
    }
    float norm (int g, double plain)
    {
        const auto* d = e.describe (g);
        return d != nullptr ? d->toNorm ((float) plain) : 0.0f;
    }
    void set (int ch, const juce::String& area, const juce::String& id, double plain)
    {
        if (const auto g = param (ch, area, id); g >= 0) e.setParam (g, norm (g, plain));
    }
    void setAll (int ch, const juce::String& area, const juce::var& obj)
    {
        if (auto* o = obj.getDynamicObject())
            for (const auto& p : o->getProperties()) set (ch, area, p.name.toString(), (double) p.value);
    }
    void inserts (int ch, const juce::var& list)
    {
        for (int s = 0; s < list.size() && s < numInserts; ++s)
        {
            const auto& fx = list[s];
            const auto type = fx["type"].toString().toStdString();
            if (! e.setInsert (ch, s, type, 0.0)) { std::printf ("  ! effet inconnu : %s\n", type.c_str()); ++errors; continue; }
            if (fx.hasProperty ("preset") && ! e.applyPreset (ch, s, fx["preset"].toString().toStdString()))
            { std::printf ("  ! préréglage d'effet inconnu : %s / %s\n", type.c_str(), fx["preset"].toString().toRawUTF8()); ++errors; }
            setAll (ch, "fx" + juce::String (s + 1), fx["params"]);
        }
    }

    void build (const juce::var& spec)
    {
        proc.clearProject();
        auto& seq = e.sequencer();
        e.beginBatch();
        const auto& tracks = spec["tracks"];
        for (int t = 0; t < tracks.size() && t < numTracks; ++t)
        {
            const auto& tr = tracks[t];
            const auto inst = tr["inst"].toString().toStdString();
            if (! e.setInstrument (t, inst, 0.0)) { std::printf ("  ! instrument inconnu : %s\n", inst.c_str()); ++errors; continue; }
            e.setTrackName (t, tr["name"].toString().toStdString());
            e.setTrackColor (t, (int) tr.getProperty ("color", t));
            if (tr.hasProperty ("preset") && ! e.applyPreset (t, -1, tr["preset"].toString().toStdString()))
            { std::printf ("  ! préréglage inconnu : %s / %s\n", inst.c_str(), tr["preset"].toString().toRawUTF8()); ++errors; }
            setAll (t, "inst", tr["params"]);
            setAll (t, "mix", tr["mix"]);
            inserts (t, tr["inserts"]);
            if (tr.hasProperty ("accent")) seq.setAccentAmount (t, (int) tr["accent"]);
            // modulations : { "source": 0..5, "target": "cutoff", "amount": -100..100 } ; réglages de la source : "srcParams"
            const auto& mods = tr["mods"];
            for (int m = 0; m < mods.size(); ++m)
            {
                const auto& md = mods[m];
                const int src = (int) md["source"];
                setAll (t, "mod" + juce::String (src + 1), md["srcParams"]);
                if (const auto g = param (t, md.getProperty ("area", "inst").toString(), md["target"].toString()); g >= 0)
                    e.addModulation (t, src, g, (float) (double) md["amount"] / 100.0f, 0.0);
            }
            // patterns
            if (auto* pats = tr["patterns"].getDynamicObject())
                for (const auto& pp : pats->getProperties())
                {
                    const int p = patternIndex (pp.name.toString());
                    const auto& pd = pp.value;
                    seq.setPatternLength (t, p, (int) pd.getProperty ("len", 16));
                    seq.setPatternDivision (t, p, divisionFrom (pd.getProperty ("div", "1/16").toString()));
                    seq.setPatternSpeed (t, p, (int) pd.getProperty ("speed", 4));
                    if (auto* ll = pd["laneLen"].getDynamicObject())
                        for (const auto& l : ll->getProperties()) seq.setLaneLengthP (t, p, l.name.toString().getIntValue(), (int) l.value);
                    std::map<int, int> flags;
                    const auto& steps = pd["steps"];
                    for (int i = 0; i < steps.size(); ++i)
                    {
                        // [ligne, pas, vélocité, note, durée en 1/8 de pas, micro, drapeaux (1 accent, 2 glissé), {extra}]
                        const auto& s = steps[i];
                        const int lane = (int) s[0], st = (int) s[1];
                        seq.setStepP (t, p, lane, st, (int) s[2]);
                        if (s.size() > 3) seq.setNoteP (t, p, lane, st, (int) s[3]);
                        if (s.size() > 4) seq.setLength8P (t, p, lane, st, (int) s[4]);
                        if (s.size() > 5) seq.setMicroP (t, p, lane, st, (int) s[5]);
                        if (s.size() > 6 && (int) s[6] != 0) flags[st] |= (int) s[6];
                        if (s.size() > 7)
                        {
                            const auto& x = s[7];
                            if (x.hasProperty ("cond") || x.hasProperty ("prob"))
                                e.setStepExtraP (t, p, lane, st, (Condition) (int) x.getProperty ("cond", 0), (int) x.getProperty ("a", 1), (int) x.getProperty ("b", 2), (int) x.getProperty ("prob", 100), 0.0);
                            if (x.hasProperty ("ratchet")) e.setStepRatchetP (t, p, lane, st, (int) x["ratchet"], (int) x.getProperty ("slope", 0), 0.0);
                            if (auto* lk = x["locks"].getDynamicObject())
                                for (const auto& l : lk->getProperties())
                                {
                                    auto key = l.name.toString(); // « inst.cutoff » ou « fx1.mix »
                                    const auto area = key.upToFirstOccurrenceOf (".", false, false), id = key.fromFirstOccurrenceOf (".", false, false);
                                    if (const auto g = param (t, area, id); g >= 0) e.setStepLockP (t, p, lane, st, g, norm (g, (double) l.value), 0.0);
                                }
                        }
                    }
                    for (const auto& [st, f] : flags) seq.setStepFlagsP (t, p, st, f);
                }
            e.setTrackName (t, tr["name"].toString().toStdString());
        }
        // retours A à D, puis principal
        const auto& rets = spec["returns"];
        for (int r = 0; r < rets.size() && r < 4; ++r) { inserts (numTracks + r, rets[r]["inserts"]); setAll (numTracks + r, "mix", rets[r]["mix"]); }
        inserts (masterChannel, spec["master"]["inserts"]);
        setAll (masterChannel, "mix", spec["master"]["mix"]);

        // tempo et swing (réglages de l'hôte)
        for (auto* prm : proc.getParameters())
            if (auto* pid = dynamic_cast<juce::RangedAudioParameter*> (prm))
            {
                if (pid->getParameterID() == "bpm" && spec.hasProperty ("bpm")) pid->setValueNotifyingHost (pid->convertTo0to1 ((float) (double) spec["bpm"]));
                if (pid->getParameterID() == "swing" && spec.hasProperty ("swing")) pid->setValueNotifyingHost (pid->convertTo0to1 ((float) (double) spec["swing"]));
            }

        // morceau
        Song song;
        song.songMode = true;
        song.loop = (bool) spec["song"].getProperty ("loop", true);
        const auto& rows = spec["song"]["rows"];
        for (int r = 0; r < rows.size() && r < maxSongRows; ++r)
        {
            SongRow row;
            row.bars = juce::jlimit (1, maxRowBars, (int) rows[r].getProperty ("bars", 4));
            row.scene = (int) rows[r].getProperty ("scene", -1);
            if (auto* pt = rows[r]["pat"].getDynamicObject())
                for (const auto& p : pt->getProperties()) row.pat[(size_t) juce::jlimit (0, numTracks - 1, p.name.toString().getIntValue())] = (std::int8_t) patternIndex (p.value);
            song.rows.push_back (row);
        }
        const auto& autos = spec["auto"];
        for (int a = 0; a < autos.size(); ++a)
        {
            const auto& au = autos[a];
            const int ch = (int) au["ch"];
            if (const auto g = param (ch, au["area"].toString(), au["id"].toString()); g >= 0)
            {
                AutoLane lane;
                lane.global = g;
                const auto& pts = au["points"];
                for (int i = 0; i < pts.size(); ++i) lane.pts.push_back ({ (float) (double) pts[i][0], norm (g, (double) pts[i][1]) });
                std::sort (lane.pts.begin(), lane.pts.end(), [] (const AutoPoint& x, const AutoPoint& y) { return x.beat < y.beat; });
                song.lanes.push_back (std::move (lane));
            }
        }
        e.setSong (std::move (song), 0.0);
        for (int t = 0; t < numTracks; ++t) seq.setEditing (t, 0);
        e.endBatch (0.0);
    }
};

struct Analysis
{
    double integrated = -100, maxTp = -100, peak = 0;
    std::vector<double> sectionLufs;
    std::array<double, 5> bands {}; // < 60, 60-250, 250-2k, 2k-8k, > 8k (énergie relative en dB)
    bool finite = true;
};

/** Rend le morceau entier (ou mute toutes les pistes sauf `solo`), renvoie la mesure. */
Analysis render (GrooveboxProcessor& p, double seconds, int solo, juce::AudioBuffer<float>* keep, const std::vector<double>& sectionStarts)
{
    // taille des blocs : 256, ou GBX_BS (essais : blocs irréguliers des cartes son et des téléphones)
    static const int bs = std::getenv ("GBX_BS") != nullptr ? std::atoi (std::getenv ("GBX_BS")) : 256;
    constexpr double sr = 48000.0;
    auto& e = p.getEngine();
    auto& seq = e.sequencer();
    for (int t = 0; t < numTracks; ++t) seq.setTrackMute (t, solo >= 0 && t != solo);
    seq.requestStop();
    juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m;
    for (int i = 0; i < 40; ++i) { b.clear(); p.processBlock (b, m); }
    p.reset();
    seq.requestPlay();
    grv::dsp::LoudnessMeter meter; meter.prepare (sr);
    grv::dsp::LoudnessReader all;
    std::vector<grv::dsp::LoudnessReader> secs (sectionStarts.size());
    // bandes : filtres d'état simples sur la somme mono
    struct OnePole { double a = 0, z = 0; double lp (double x) { z += a * (x - z); return z; } };
    auto coef = [sr] (double f) { return 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * f / sr); };
    OnePole l60 { coef (60) }, l250 { coef (250) }, l2k { coef (2000) }, l8k { coef (8000) };
    std::array<double, 5> energy {};
    Analysis a;
    const int total = (int) (seconds * sr);
    if (keep != nullptr) keep->setSize (2, total + bs, false, true, false);
    for (int pos = 0; pos < total; pos += bs)
    {
        b.clear(); p.processBlock (b, m);
        meter.process (b.getReadPointer (0), b.getReadPointer (1), bs);
        const double tSec = pos / sr;
        int sec = -1;
        for (size_t s = 0; s < sectionStarts.size(); ++s) if (tSec >= sectionStarts[s]) sec = (int) s;
        grv::dsp::LoudnessBlock lb;
        while (meter.pop (lb)) { all.add (lb); if (sec >= 0) secs[(size_t) sec].add (lb); }
        for (int i = 0; i < bs; ++i)
        {
            const float L = b.getSample (0, i), R = b.getSample (1, i);
            if (! std::isfinite (L) || ! std::isfinite (R)) a.finite = false;
            a.peak = std::max (a.peak, (double) std::max (std::abs (L), std::abs (R)));
            const double x = 0.5 * (L + R);
            const double s60 = l60.lp (x), s250 = l250.lp (x), s2k = l2k.lp (x), s8k = l8k.lp (x);
            energy[0] += s60 * s60; energy[1] += (s250 - s60) * (s250 - s60); energy[2] += (s2k - s250) * (s2k - s250);
            energy[3] += (s8k - s2k) * (s8k - s2k); energy[4] += (x - s8k) * (x - s8k);
        }
        if (keep != nullptr) for (int c = 0; c < 2; ++c) keep->copyFrom (c, pos, b, c, 0, std::min (bs, total + bs - pos));
    }
    a.integrated = all.integrated();
    a.maxTp = all.maxTruePeakDb();
    for (auto& s : secs) a.sectionLufs.push_back (s.integrated());
    const double sum = std::max (1.0e-12, energy[0] + energy[1] + energy[2] + energy[3] + energy[4]);
    for (int k = 0; k < 5; ++k) a.bands[(size_t) k] = 10.0 * std::log10 (std::max (1.0e-12, energy[(size_t) k] / sum));
    for (int t = 0; t < numTracks; ++t) seq.setTrackMute (t, false);
    return a;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3) { std::printf ("Usage : groovebox_demos <description.json> <sortie.gbx> [--wav f] [--mid f] [--tracks] [--bench]\n"); return 1; }
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto spec = load (cwd.getChildFile (argv[1]));
    juce::String wavPath, midPath; bool perTrack = false, bench = false;
    for (int i = 3; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--wav" && i + 1 < argc) wavPath = argv[++i];
        else if (a == "--mid" && i + 1 < argc) midPath = argv[++i];
        else if (a == "--tracks") perTrack = true;
        else if (a == "--bench") bench = true;
    }

    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
    GrooveboxProcessor proc;
    constexpr double sr = 48000.0; const int bs = std::getenv ("GBX_BS") != nullptr ? std::atoi (std::getenv ("GBX_BS")) : 256;
    proc.setPlayConfigDetails (2, 2, sr, bs);
    proc.prepareToPlay (sr, bs);
    Builder b { proc, proc.getEngine() };
    b.build (spec);

    // Mix « aux chiffres » (l'oreille de l'utilisateur affine ensuite) :
    //  1. chaque piste qui a un « rel » est réglée pour sonner rel LU sous la piste de référence (« ref »), seule ;
    //  2. puis le gain d'entrée du principal (« makeup » du premier insert qui en a un) vise la sonie intégrée « target ».
    {
        auto& e = proc.getEngine();
        const auto& tracks = spec["tracks"];
        const double bpm0 = (double) spec.getProperty ("bpm", 120);
        const double secs0 = e.getSong().totalBeats() * 60.0 / bpm0 + 1.0;
        const int ref = (int) spec.getProperty ("ref", 0);
        bool anyRel = false;
        for (int t = 0; t < tracks.size(); ++t) anyRel = anyRel || tracks[t].hasProperty ("rel");
        if (anyRel)
            for (int pass = 0; pass < 2; ++pass)
            {
                const double refL = render (proc, secs0, ref, nullptr, {}).integrated;
                for (int t = 0; t < tracks.size(); ++t)
                {
                    if (t == ref || ! tracks[t].hasProperty ("rel")) continue;
                    const double l = render (proc, secs0, t, nullptr, {}).integrated;
                    if (l < -69.0) continue;
                    const auto g = e.findParam (t, "mix", "volume");
                    const auto* dsc = e.describe (g);
                    const double cur = dsc->toPlain (e.getParam (g));
                    const double want = juce::jlimit (-40.0, 6.0, cur + (refL + (double) tracks[t]["rel"]) - l);
                    e.setParam (g, dsc->toNorm ((float) want));
                    if (pass == 1) std::printf ("  équilibre : piste %d %-16s %+5.1f dB (seule %5.1f LUFS, réf. %5.1f)\n", t + 1, tracks[t]["name"].toString().toRawUTF8(), want, l, refL);
                }
            }
        if (spec.hasProperty ("target"))
        {
            const double target = (double) spec["target"];
            // premier insert du principal qui a un gain d'entrée « makeup » (le bus de colle)
            int g = -1;
            for (int k = 1; k <= 4 && g < 0; ++k) g = e.findParam (masterChannel, "fx" + std::to_string (k), "makeup");
            if (g >= 0)
            {
                const auto* dsc = e.describe (g);
                for (int it = 0; it < 4; ++it)
                {
                    const double l = render (proc, secs0, -1, nullptr, {}).integrated;
                    const double cur = dsc->toPlain (e.getParam (g));
                    const double next = juce::jlimit (0.0, 20.0, cur + 0.9 * (target - l));
                    std::printf ("  principal : %.1f LUFS → gain d'entrée %.1f dB\n", l, next);
                    if (std::abs (target - l) < 0.3) break;
                    e.setParam (g, dsc->toNorm ((float) next));
                }
            }
        }
    }

    // écrit le projet, puis le relit dans un processeur neuf : c'est lui qu'on écoute (même chose que l'utilisateur)
    {
        juce::MemoryOutputStream mo;
        if (! proc.writeProjectFile (mo)) { std::printf ("Écriture du projet impossible\n"); return 3; }
        const auto out = cwd.getChildFile (argv[2]);
        out.getParentDirectory().createDirectory();
        out.replaceWithData (mo.getData(), mo.getDataSize());
        std::printf ("%s : %d Ko\n", out.getFileName().toRawUTF8(), (int) (mo.getDataSize() / 1024));
    }
    GrooveboxProcessor play;
    play.setPlayConfigDetails (2, 2, sr, bs);
    play.prepareToPlay (sr, bs);
    {
        juce::FileInputStream in (cwd.getChildFile (argv[2]));
        if (const auto err = play.readProjectFile (in); err.isNotEmpty()) { std::printf ("Relecture : %s\n", err.toRawUTF8()); return 4; }
    }
    const double bpm = (double) spec.getProperty ("bpm", 120);
    const auto& song = play.getEngine().getSong();
    const double seconds = song.totalBeats() * 60.0 / bpm + 2.0;
    std::vector<double> starts;
    std::vector<juce::String> names;
    const auto& rows = spec["song"]["rows"];
    for (int r = 0; r < (int) song.rows.size(); ++r)
        if (rows[r].hasProperty ("label")) { starts.push_back (song.rowStart (r) * 60.0 / bpm); names.push_back (rows[r]["label"].toString()); }

    juce::AudioBuffer<float> mixBuf;
    const auto a = render (play, seconds, -1, wavPath.isNotEmpty() ? &mixBuf : nullptr, starts);
    std::printf ("Durée %.0f s · sonie intégrée %.1f LUFS · crête vraie %.1f dBTP · crête %.2f%s\n", seconds, a.integrated, a.maxTp, a.peak, a.finite ? "" : " · VALEURS INVALIDES");
    std::printf ("Bandes (part de l'énergie) : <60 %.1f · 60-250 %.1f · 250-2k %.1f · 2k-8k %.1f · >8k %.1f dB\n", a.bands[0], a.bands[1], a.bands[2], a.bands[3], a.bands[4]);
    for (size_t s = 0; s < starts.size(); ++s) std::printf ("  %-12s %6.1f LUFS\n", names[s].toRawUTF8(), a.sectionLufs[s]);
    if (perTrack)
        for (int t = 0; t < spec["tracks"].size(); ++t)
        {
            const auto at = render (play, seconds, t, nullptr, {});
            std::printf ("  piste %d %-16s %6.1f LUFS · crête %.1f dBTP · <60 %.0f · 60-250 %.0f · 250-2k %.0f · 2k-8k %.0f · >8k %.0f\n", t + 1,
                         spec["tracks"][t]["name"].toString().toRawUTF8(), at.integrated, at.maxTp, at.bands[0], at.bands[1], at.bands[2], at.bands[3], at.bands[4]);
        }
    if (wavPath.isNotEmpty())
    {
        const auto f = cwd.getChildFile (wavPath);
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
        juce::WavAudioFormat wav;
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
        { os.release(); w->writeFromAudioSampleBuffer (mixBuf, 0, (int) (seconds * sr)); }
    }
    if (midPath.isNotEmpty())
    {
        const auto bytes = midifile::exportProject (play.getEngine(), bpm);
        cwd.getChildFile (midPath).replaceWithData (bytes.data(), bytes.size());
        std::printf ("MIDI : %d octets\n", (int) bytes.size());
    }
    if (bench)
    {
        auto& seq = play.getEngine().sequencer();
        seq.requestPlay();
        juce::AudioBuffer<float> buf (2, bs); juce::MidiBuffer m;
        const int n = (int) (20.0 * sr / bs);
        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int i = 0; i < n; ++i) { buf.clear(); play.processBlock (buf, m); }
        const auto secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        std::printf ("CPU : %.1f %% d'un cœur (PC de compilation)\n", secs / 20.0 * 100.0);
    }
    if (b.errors > 0) std::printf ("%d erreur(s) dans la description\n", b.errors);
    return b.errors > 0 ? 5 : 0;
}
