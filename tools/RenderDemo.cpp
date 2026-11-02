/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Outil de vérification : fait tourner le processeur hors temps réel (sans interface ni carte
    son), lance le séquenceur et écrit le résultat dans un fichier WAV.
    Vérifie au passage que le son sort, sans valeur invalide ni saturation numérique.
    Usage : groovebox_render [fichier.wav] [secondes] [--state]
            groovebox_render --check   (sauvegarde puis relecture du projet complet)
*/

#include "PluginProcessor.h"
#include "effects/Mastering.h"
#include "dsp/Spectrum.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    const juce::File out (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                   : juce::File::getCurrentWorkingDirectory().getChildFile ("groovebox-demo.wav"));
    const auto seconds = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 8.0;

    grv::GrooveboxProcessor proc;
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);

    // Projet de départ (batterie, basse acide, accords, nappe, réverbération, écho, limiteur),
    // avec un état sauvegardé puis relu pour vérifier la sauvegarde au passage.
    juce::MemoryBlock saved;
    proc.getStateInformation (saved);
    proc.setStateInformation (saved.getData(), (int) saved.getSize());
    proc.getEngine().sequencer().requestPlay();

    if (argc > 2 && juce::String (argv[1]) == "--clips")
    {
        // Extraits d'écoute : un son seul, un coup par temps, 6 s, un WAV par cas (comparaison avant / après)
        const juce::File dir { juce::String (argv[2]) };
        dir.createDirectory();
        struct Case { const char* name; const char* kit; int mach; };
        const Case cases[] = { { "gc-dure", "drumA", 1 }, { "gc-fm", "drumA", 3 }, { "gc-pointue", "drumA", 6 }, { "tom-dur", "drumA", 18 },
                               { "cc-fm", "drumA", 10 }, { "909-gc", "drum90", -1 }, { "nerve-gc", "drumNrv", -1 }, { "percussion-sequenceur", "dfam", -1 } };
        for (const auto& c : cases)
        {
            proc.clearProject();
            auto& e = proc.getEngine();
            e.setInstrument (0, c.kit, 0.0);
            if (c.mach >= 0) e.setParam (e.findParam (0, "inst", "p01_mach"), (float) c.mach / 29.0f);
            for (int st = 0; st < 16; ++st) e.sequencer().setStepP (0, 0, 0, st, (st % 4) == 0 ? 100 : 0);
            e.sequencer().requestStop();
            juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m;
            for (int i = 0; i < 300; ++i) { b.clear(); proc.processBlock (b, m); }
            e.sequencer().requestPlay();
            const int total = (int) (6.0 * sr);
            juce::AudioBuffer<float> all (1, total);
            for (int pos = 0; pos + block <= total; pos += block)
            {
                b.clear(); proc.processBlock (b, m);
                all.copyFrom (0, pos, b, 0, 0, block);
            }
            const auto f = dir.getChildFile (juce::String (c.name) + ".wav");
            f.deleteFile();
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            juce::WavAudioFormat wav;
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 1, 16, {}, 0)))
            {
                os.release();
                w->writeFromAudioSampleBuffer (all, 0, total);
            }
        }
        return 0;
    }

    // v0.46 : --project <fichier.gbx> [secondes] : relit un projet et le joue (projet de démonstration)
    if (argc > 2 && juce::String (argv[1]) == "--project")
    {
        grv::GrooveboxProcessor pp;
        pp.setPlayConfigDetails (2, 2, 48000.0, 256);
        pp.prepareToPlay (48000.0, 256);
        juce::FileInputStream in (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        if (! in.openedOk()) { std::printf ("Projet introuvable\n"); return 2; }
        const auto err = pp.readProjectFile (in);
        if (err.isNotEmpty()) { std::printf ("Erreur : %s\n", err.toRawUTF8()); return 3; }
        int used = 0;
        for (int t = 0; t < grv::engine::numTracks; ++t) used += pp.getEngine().getTrackInfo (t).instrumentType.empty() ? 0 : 1;
        pp.getEngine().sequencer().requestPlay();
        const auto secs = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 8.0;
        juce::AudioBuffer<float> b (2, 256);
        juce::MidiBuffer m;
        float peak = 0.0f; bool finite = true;
        for (int i = 0; i < (int) (secs * 48000.0 / 256.0); ++i)
        {
            b.clear(); pp.processBlock (b, m);
            for (int c = 0; c < 2; ++c) for (int k = 0; k < 256; ++k) { const auto v = b.getSample (c, k); finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
        }
        std::printf ("Projet relu : %d pistes, cr\xC3\xAAte %.3f, fini %d\n", used, peak, (int) finite);
        return finite && peak > 0.001f ? 0 : 4;
    }

    // v0.46 fix8 : --montage <fichier.gbx> <dossier> : prise du montage (pistes en FLAC), découpage en
    // clips, lecture du montage à la place des instruments, enregistrement et relecture du projet
    if (argc > 3 && juce::String (argv[1]) == "--montage")
    {
        const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]);
        dir.createDirectory();
        const auto gbx = dir.getChildFile ("Test montage.gbx");
        int failures = 0;
        auto check = [&] (bool ok, const char* what) { std::printf ("  %s : %s\n", what, ok ? "ok" : "\xC3\x89" "CHEC"); failures += ok ? 0 : 1; };
        juce::AudioBuffer<float> b (2, 256);
        juce::MidiBuffer m;
        auto play = [&] (grv::GrooveboxProcessor& p, double secs) -> float
        {
            float pk = 0.0f;
            for (int i = 0; i < (int) (secs * 48000.0 / 256.0); ++i)
            {
                b.clear(); p.processBlock (b, m);
                pk = std::max (pk, b.getMagnitude (0, 256));
                if (i % 8 == 0) p.pumpMontage();
            }
            return pk;
        };
        {
            grv::GrooveboxProcessor pp;
            pp.setPlayConfigDetails (2, 2, 48000.0, 256);
            pp.prepareToPlay (48000.0, 256);
            juce::FileInputStream in (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
            if (! in.openedOk() || pp.readProjectFile (in).isNotEmpty()) { std::printf ("Projet illisible\n"); return 2; }
            pp.setProjectFileContext (gbx);
            const auto err = pp.montageRecord (true);
            check (err.isEmpty(), "prise d\xC3\xA9marr\xC3\xA9" "e");
            pp.getEngine().sequencer().requestPlay();
            const auto live = play (pp, 6.0);
            pp.montageRecord (false);
            for (int k = 0; k < 600 && pp.montageCutting(); ++k) { juce::Thread::sleep (50); pp.pumpMontage(); }
            const auto& doc = pp.getMontageDoc();
            std::printf ("  prise : %d fichiers, %d clips, fin %.2f noires (direct : cr\xC3\xAAte %.3f)\n", (int) doc.takes.size(), (int) doc.clips.size(), doc.endBeat(), live);
            check (! doc.clips.empty() && doc.endBeat() > 4.0, "clips d\xC3\xA9" "coup\xC3\xA9s");
            bool inFolder = true;
            for (const auto& t : doc.takes) inFolder = inFolder && t.file.existsAsFile() && t.file.isAChildOf (grv::montage::audioFolderFor (gbx));
            check (inFolder, "fichiers FLAC dans \xC2\xAB Test montage - audio \xC2\xBB");
            // lecture du montage : les clips à la place des instruments
            pp.montageSetFlags (true, false, 0.0, 16.0);
            pp.montageSetPlayhead (0.0);
            play (pp, 0.2); // le fil de lecture prend de l'avance
            pp.getEngine().sequencer().requestPlay();
            const auto mont = play (pp, 4.0);
            pp.getEngine().sequencer().requestStop();
            play (pp, 0.1);
            std::printf ("  montage relu : cr\xC3\xAAte %.3f (direct %.3f)\n", mont, live);
            check (mont > 0.25f * live && mont < 2.0f * live, "montage entendu au bon niveau");
            // clips retirés : silence
            const auto saved = juce::JSON::toString (juce::JSON::parse (pp.montageJson()).getProperty ("clips", {}));
            pp.montageSetClips ("[]");
            play (pp, 4.0); // les traînes des effets des groupes et du principal s'éteignent
            pp.getEngine().sequencer().requestPlay();
            const auto none = play (pp, 2.0);
            pp.getEngine().sequencer().requestStop();
            play (pp, 0.1);
            std::printf ("  sans clip : cr\xC3\xAAte %.5f (%s)\n", none, pp.montageJson().substr (0, 60).c_str());
            check (none < 0.002f, "sans clip : silence (instruments muets en mode montage)");
            pp.montageSetClips (saved);
            // v0.46 fix11 : 2e prise à la tête de lecture placée après la 1re (mesure 5), alors que la
            // lecture tourne déjà : elle part au début d'une mesure et se pose à la tête de lecture,
            // sans toucher la 1re prise
            {
                const auto firstEnd = pp.getMontageDoc().endBeat();
                const auto firstClips = (int) pp.getMontageDoc().clips.size();
                pp.montageSetFlags (false, false, 0.0, 16.0);
                pp.montageSetPlayhead (16.0);
                pp.getEngine().sequencer().requestPlay();
                play (pp, 2.3);                 // la lecture tourne (pas sur une mesure)
                check (pp.montageRecord (true).isEmpty(), "2e prise d\xC3\xA9marr\xC3\xA9" "e en lecture");
                play (pp, 4.0);
                pp.montageRecord (false);
                for (int k = 0; k < 600 && pp.montageCutting(); ++k) { juce::Thread::sleep (50); pp.pumpMontage(); }
                const auto& d2 = pp.getMontageDoc();
                double minNew = 1.0e9; int before = 0;
                for (const auto& c : d2.clips) { if (c.beat >= 15.9) minNew = std::min (minNew, c.beat); else ++before; }
                std::printf ("  2e prise : 1er clip \xC3\xA0 %.3f noires (attendu 16), %d clips de la 1re gard\xC3\xA9s sur %d (fin 1re : %.2f)\n", minNew, before, firstClips, firstEnd);
                check (minNew >= 16.0 - 1.0e-3 && minNew < 16.5, "2e prise pos\xC3\xA9" "e \xC3\xA0 la t\xC3\xAAte de lecture");
                check (before == firstClips || firstEnd > 16.0, "1re prise intacte");
                pp.getEngine().sequencer().requestStop();
                play (pp, 0.1);
            }
            // v0.46 fix12 : prise avec la Boucle allumée (noires 32 à 40) : elle s'arrête seule, à
            // l'échantillon près, et chaque piste jouée devient un clip pile de la longueur de la boucle
            {
                pp.montageSetFlags (false, true, 32.0, 40.0);
                pp.montageSetPlayhead (0.0);
                pp.getEngine().sequencer().requestPlay();
                play (pp, 1.1);
                check (pp.montageRecord (true).isEmpty(), "prise de boucle d\xC3\xA9marr\xC3\xA9" "e");
                const auto bpm = pp.getCurrentBpm() > 0.0 ? pp.getCurrentBpm() : 120.0;
                int guard = 0; // au plus : attente d'une mesure + la boucle + marge
                const auto maxBlocks = (int) ((12.0 * 60.0 / bpm + 1.0) * 48000.0 / 256.0);
                while (pp.montageRecording() && guard++ < maxBlocks) play (pp, 256.0 / 48000.0);
                check (! pp.montageRecording(), "prise de boucle arr\xC3\xAAt\xC3\xA9" "e toute seule");
                for (int k = 0; k < 600 && pp.montageCutting(); ++k) { juce::Thread::sleep (50); pp.pumpMontage(); }
                const auto& d3 = pp.getMontageDoc();
                const auto want = (juce::int64) std::llround (8.0 * 60.0 / bpm * 48000.0);
                int inLoop = 0, exact = 0;
                for (const auto& c : d3.clips)
                    if (std::abs (c.beat - 32.0) < 1.0e-6) { ++inLoop; if (c.src == 0 && c.len == want) ++exact; }
                std::printf ("  prise de boucle : %d clips \xC3\xA0 la noire 32, %d de %lld \xC3\xA9" "chantillons pile\n", inLoop, exact, (long long) want);
                check (inLoop > 0 && exact == inLoop, "clips de la boucle pile sur la boucle");
                pp.getEngine().sequencer().requestStop();
                play (pp, 0.1);
                pp.montageSetFlags (false, false, 0.0, 16.0);
            }
            // projet écrit puis relu : les prises suivent, chemins relatifs
            {
                juce::FileOutputStream fos (gbx);
                fos.setPosition (0); fos.truncate();
                check (pp.writeProjectFile (fos), "projet \xC3\xA9" "crit");
            }
        }
        {
            const auto text = gbx.loadFileAsString();
            check (text.contains ("rel=\"1\"") && ! text.contains (dir.getFullPathName() + "/Test"), "chemins relatifs au projet");
            grv::GrooveboxProcessor p2;
            p2.setPlayConfigDetails (2, 2, 48000.0, 256);
            p2.prepareToPlay (48000.0, 256);
            p2.setProjectFileContext (gbx);
            juce::FileInputStream in (gbx);
            check (p2.readProjectFile (in).isEmpty() && ! p2.getMontageDoc().clips.empty(), "montage relu avec le projet");
            p2.montageSetFlags (true, true, 0.0, 8.0);
            p2.montageSetPlayhead (0.0);
            play (p2, 0.2);
            p2.getEngine().sequencer().requestPlay();
            const auto again = play (p2, 4.0);
            std::printf ("  projet relu, montage en boucle : cr\xC3\xAAte %.3f\n", again);
            check (again > 0.05f, "montage relu entendu");
            p2.getEngine().sequencer().requestStop();
            play (p2, 0.1);
            // export en temps réel (WAV)
            check (p2.montageExport (0).isEmpty(), "export lanc\xC3\xA9");
            int guard = 0;
            while (p2.montageExporting() && guard++ < 4000) play (p2, 0.05);
            check (! p2.montageExporting(), "export termin\xC3\xA9 tout seul \xC3\xA0 la fin du montage");
        }
        std::printf ("Montage : %d \xC3\xA9" "chec(s)\n", failures);
        return failures == 0 ? 0 : 5;
    }

    if (argc > 1 && juce::String (argv[1]) == "--pump")
    {
        // Pompage : nappe continue (220 Hz, -12 dBFS) + grosse caisse forte toutes les 500 ms ;
        // réduction de gain vue par la nappe 20, 100 et 250 ms après chaque coup, crête de sortie
        auto run = [&] (grv::engine::Effect& fx, const std::vector<float>& prm, const char* name)
        {
            fx.prepare (sr, block); fx.reset();
            const int total = (int) (6.0 * sr);
            std::vector<float> l ((size_t) total), r ((size_t) total);
            for (int i = 0; i < total; ++i)
            {
                const double t = i / sr, tk = std::fmod (t, 0.5);
                const double pad = 0.25 * std::sin (2.0 * 3.14159265 * 220.0 * t);
                const double kick = 2.0 * std::exp (-tk / 0.08) * std::sin (2.0 * 3.14159265 * (55.0 * tk + 40.0 * 0.01 * (1.0 - std::exp (-tk / 0.01))));
                l[(size_t) i] = r[(size_t) i] = (float) (pad + kick);
            }
            std::vector<float> gr ((size_t) total);
            float outPk = 0.0f;
            for (int pos = 0; pos + 32 <= total; pos += 32)
            {
                fx.process (l.data() + pos, r.data() + pos, 32, prm.data(), 120.0);
                for (int i = 0; i < 32; ++i) { gr[(size_t) (pos + i)] = fx.meter (0); outPk = std::max (outPk, std::abs (l[(size_t) (pos + i)])); }
            }
            double g20 = 0, g100 = 0, g250 = 0; int cnt = 0;
            for (double k = 2.0; k < 5.5; k += 0.5, ++cnt)
            {
                g20 += gr[(size_t) ((k + 0.02) * sr)]; g100 += gr[(size_t) ((k + 0.10) * sr)]; g250 += gr[(size_t) ((k + 0.25) * sr)];
            }
            std::printf ("%-34s reduction apres le coup : 20 ms %.1f dB, 100 ms %.1f dB, 250 ms %.1f dB ; crete sortie %.2f dB\n",
                         name, g20 / cnt, g100 / cnt, g250 / cnt, 20.0 * std::log10 (std::max (1.0e-9f, outPk)));
        };
        grv::effects::Limiter lim;
        std::vector<float> lp (lim.desc().params.size());
        for (size_t k = 0; k < lp.size(); ++k) lp[k] = lim.desc().params[k].def;
        run (lim, lp, "Limiteur (relachement 80 ms)");
        grv::effects::Mastering ms;
        std::vector<float> mp (ms.desc().params.size());
        for (size_t k = 0; k < mp.size(); ++k) mp[k] = ms.desc().params[k].def;
        mp[grv::effects::master::drive] = 0.0f;
        run (ms, mp, "Mastering (gain 0, defauts)");
        mp[grv::effects::master::dynOn] = 0.0f; mp[grv::effects::master::clipAmt] = 0.0f;
        run (ms, mp, "Mastering (maximiseur seul)");
        {
            for (size_t k = 0; k < mp.size(); ++k) mp[k] = ms.desc().params[k].def;
            std::vector<float> a ((size_t) block), b ((size_t) block);
            for (int i = 0; i < block; ++i) a[(size_t) i] = b[(size_t) i] = 0.3f * (float) std::sin (i * 0.05);
            const int blocks = (int) (6.0 * sr / block);
            const auto t0 = juce::Time::getHighResolutionTicks();
            for (int k = 0; k < blocks; ++k) ms.process (a.data(), b.data(), block, mp.data(), 120.0);
            std::printf ("Mastering, charge : %.2f %% d'un coeur\n", 100.0 * juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) / 6.0);
        }
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--kick2")
    {
        // Batterie analogique : chaque grosse caisse (GC dure…acoustique), un coup par temps,
        // écart de chaque coup à la moyenne des coups (dB) — repère le coup qui diffère
        proc.clearProject();
        auto& e = proc.getEngine();
        e.setInstrument (0, "drumA", 0.0);
        const int mach = e.findParam (0, "inst", "p01_mach");
        for (int k = 1; k <= 29; ++k)
        {
            e.setParam (mach, (float) k / 29.0f);
            for (int st = 0; st < 16; ++st) e.sequencer().setStepP (0, 0, 0, st, (st % 4) == 0 ? 100 : 0);
            e.sequencer().requestStop();
            juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m;
            for (int i = 0; i < 400; ++i) { b.clear(); proc.processBlock (b, m); }
            e.sequencer().requestPlay();
            const int total = (int) (10.0 * sr);
            std::vector<float> mono ((size_t) total);
            for (int pos = 0; pos + block <= total; pos += block)
            {
                b.clear(); proc.processBlock (b, m);
                for (int i = 0; i < block; ++i) mono[(size_t) (pos + i)] = b.getSample (0, i);
            }
            // premier coup repéré, les suivants à chaque temps (période exacte du tempo)
            std::vector<int> on;
            int first = 0;
            while (first < total && std::abs (mono[(size_t) first]) < 0.01f) ++first;
            const double period = 60.0 / proc.getCurrentBpm() * sr;
            for (int h = 0; first + (int) std::lround (h * period) < total; ++h) on.push_back (first + (int) std::lround (h * period));
            const int L = (int) (0.15 * sr);
            while (! on.empty() && on.back() + L >= total) on.pop_back();
            std::vector<double> mean ((size_t) L, 0.0);
            for (auto o : on) for (int i = 0; i < L; ++i) mean[(size_t) i] += mono[(size_t) (o + i)] / (double) on.size();
            double worst = -200.0;
            for (auto o : on)
            {
                double num = 0, den = 0;
                for (int i = 0; i < L; ++i) { const double d = mono[(size_t) (o + i)] - mean[(size_t) i]; num += d * d; den += mean[(size_t) i] * mean[(size_t) i]; }
                worst = std::max (worst, 10.0 * std::log10 (std::max (1e-12, num / std::max (1e-12, den))));
            }
            std::printf ("machine %2d : %d coups, pire ecart a la moyenne %.0f dB\n", k, (int) on.size(), worst);
        }
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--kick")
    {
        // Régularité de la grosse caisse : chaque coup comparé au premier (écart en dB)
        proc.clearProject();
        auto& e = proc.getEngine();
        for (const char* d : { "drumA", "drum80", "drum90", "drumImp", "drumNrv", "dfam" })
        {
            e.setInstrument (0, d, 0.0);
            for (int st = 0; st < 16; ++st) e.sequencer().setStepP (0, 0, 0, st, (st % 4) == 0 ? 100 : 0);
            proc.prepareToPlay (sr, block);
            e.sequencer().requestStop();
            juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m;
            for (int i = 0; i < 200; ++i) { b.clear(); proc.processBlock (b, m); }
            e.sequencer().requestPlay();
            const int total = (int) (8.5 * sr);
            std::vector<float> mono ((size_t) total);
            for (int pos = 0; pos + block <= total; pos += block)
            {
                b.clear(); proc.processBlock (b, m);
                for (int i = 0; i < block; ++i) mono[(size_t) (pos + i)] = b.getSample (0, i);
            }
            // repérage des coups : premier échantillon au-dessus de 0,02 après 100 ms de calme relatif
            std::vector<int> on;
            int first = 0;
            while (first < total && std::abs (mono[(size_t) first]) < 0.01f) ++first;
            const double period = 60.0 / proc.getCurrentBpm() * sr;
            for (int h = 0; first + (int) std::lround (h * period) < total; ++h) on.push_back (first + (int) std::lround (h * period));
            const int L = (int) (0.12 * sr);
            std::printf ("%s : %d coups ; ecart a chaque coup par rapport au 1er (dB), debut :", d, (int) on.size());
            for (size_t h = 1; h < on.size() && h < 12; ++h)
            {
                if (on[h] + L >= total) break;
                double num = 0, den = 0;
                for (int i = 0; i < L; ++i) { const double a0 = mono[(size_t) (on[0] + i)], a1 = mono[(size_t) (on[h] + i)]; num += (a1 - a0) * (a1 - a0); den += a0 * a0; }
                std::printf (" %.1f", 10.0 * std::log10 (std::max (1e-12, num / std::max (1e-12, den))));
            }
            std::printf ("\n   intervalles (echantillons) :");
            for (size_t h = 1; h < on.size() && h < 10; ++h) std::printf (" %d", on[h] - on[h - 1]);
            std::printf ("\n");
        }
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--rec")
    {
        // v0.32 : enregistrement audio du projet de départ (master + pistes + retours), en WAV puis
        // en MP3. Le calcul va plus vite que le temps réel : on laisse au fil d'écriture le temps
        // de suivre (comme en vrai, où chaque bloc arrive à son heure).
        juce::AudioBuffer<float> b (2, block);
        juce::MidiBuffer m;
        auto take = [&] (int fmt, double secs)
        {
            const auto err = proc.audioRecStart (2, fmt);
            if (err.isNotEmpty()) { std::printf ("ECHEC demarrage : %s\n", err.toRawUTF8()); return false; }
            const int blocks = (int) (secs * sr / block);
            double peak = 0.0;
            for (int i = 0; i < blocks; ++i)
            {
                b.clear(); proc.processBlock (b, m);
                peak = std::max (peak, (double) b.getMagnitude (0, block));
                if (i % 8 == 7) juce::Thread::sleep (2);
            }
            proc.audioRecStop();
            for (int i = 0; i < 3000 && proc.audioRecStatus().contains ("\"encoding\":true"); ++i) juce::Thread::sleep (10);
            std::printf ("%s : crete du rendu %.3f\n%s\n", fmt == 1 ? "MP3" : "WAV", peak, proc.audioRecStatus().toRawUTF8());
            return true;
        };
        if (! take (0, 6.0) || ! take (1, 6.0)) return 1;
        // contrôle : relire le master WAV de la première prise
        const auto root = grv::Recorder::rootFolder();
        auto dirs = root.findChildFiles (juce::File::findDirectories, false);
        std::sort (dirs.begin(), dirs.end(), [] (const juce::File& x, const juce::File& y) { return x.getFileName() < y.getFileName(); });
        if (dirs.size() >= 2)
        {
            const auto wavDir = dirs[dirs.size() - 2], mp3Dir = dirs[dirs.size() - 1];
            juce::WavAudioFormat wav;
            if (std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (wavDir.getChildFile ("Master.wav").createInputStream().release(), true)); r != nullptr)
            {
                juce::AudioBuffer<float> all (2, (int) r->lengthInSamples);
                r->read (&all, 0, (int) r->lengthInSamples, 0, true, true);
                std::printf ("Master.wav : %.2f s, %d bits, %.0f Hz, crete %.3f\n", (double) r->lengthInSamples / r->sampleRate,
                             (int) r->bitsPerSample, r->sampleRate, (double) all.getMagnitude (0, all.getNumSamples()));
            }
            for (const auto& d : { wavDir, mp3Dir })
                for (const auto& f : d.findChildFiles (juce::File::findFiles, false))
                    std::printf ("  %s/%s  %lld octets\n", d.getFileName().toRawUTF8(), f.getFileName().toRawUTF8(), (long long) f.getSize());
        }
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--bench")
    {
        // Charge du calcul (temps / durée du bloc), pour comparer machine de bureau et tablette
        const auto measure = [&] (const char* what, int blk)
        {
            proc.prepareToPlay (sr, blk);
            proc.getEngine().sequencer().requestPlay();
            juce::AudioBuffer<float> b (2, blk);
            juce::MidiBuffer m;
            const int blocks = (int) (6.0 * sr / blk);
            for (int i = 0; i < blocks / 6; ++i) { b.clear(); proc.processBlock (b, m); }
            const auto t0 = juce::Time::getHighResolutionTicks();
            for (int i = 0; i < blocks; ++i) { b.clear(); proc.processBlock (b, m); }
            const auto used = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            std::printf ("%-46s bloc %4d : %.2f %%\n", what, blk, 100.0 * used / 6.0);
        };
        const auto t = 0.0;
        for (int blk : { 256, 128 }) measure ("projet de depart", blk);
        {
            proc.getEngine().sequencer().requestStop();
            juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m;
            for (int i = 0; i < 2000; ++i) { b.clear(); proc.processBlock (b, m); } // queues finies
            const auto t0 = juce::Time::getHighResolutionTicks();
            for (int i = 0; i < 1125; ++i) { b.clear(); proc.processBlock (b, m); }
            std::printf ("%-46s bloc  256 : %.2f %%\n", "projet de depart, a l'arret", 100.0 * juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) / 6.0);
        }
        {
            auto& e0 = proc.getEngine();
            e0.setInstrument (3, "", t); measure ("  sans la nappe (pads)", 256);
            e0.setInstrument (2, "", t); measure ("  sans les accords (polyj)", 256);
            e0.setInstrument (1, "", t); measure ("  sans la basse acide", 256);
            e0.setInsert (grv::engine::numTracks + 0, 0, "", t); measure ("  sans la reverb", 256);
            e0.setInsert (grv::engine::numTracks + 1, 0, "", t); measure ("  sans l'echo", 256);
            e0.setInsert (grv::engine::masterChannel, 0, "", t); measure ("  sans le limiteur (reste drumA)", 256);
        }
        proc.clearProject();
        for (int blk : { 256, 128 }) measure ("projet vide", blk);
        auto& e = proc.getEngine();
        for (const char* d : { "drum80", "drum90", "drumA", "drumImp", "drumNrv", "sp" })
        {
            e.setInstrument (0, d, t);
            for (int st = 0; st < 16; st += 2) e.sequencer().setStepP (0, 0, 0, st, 100);
            for (int st = 2; st < 16; st += 4) e.sequencer().setStepP (0, 0, 2, st, 100);
            for (int st = 0; st < 16; ++st) e.sequencer().setStepP (0, 0, 6, st, 80);
            for (int i = 0; i < 64; ++i) { juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m; proc.processBlock (b, m); }
            measure ((std::string ("une batterie : ") + d).c_str(), 256);
        }
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--check")
    {
        // v0.45.1 : analyseur de spectre — un sinus de 1 kHz à -6 dB, mono : bonne bande, bon niveau, pas de côtés
        {
            grv::dsp::SpectrumAnalyzer an;
            std::vector<float> l ((size_t) an.fftSize), r ((size_t) an.fftSize);
            for (int i = 0; i < an.fftSize; ++i) l[(size_t) i] = r[(size_t) i] = (float) (0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / 48000.0));
            std::array<float, grv::dsp::SpectrumAnalyzer::numBands> mid {}, side {};
            an.computeFrom (l.data(), r.data(), 48000.0, mid, side);
            int best = 0; float sideMax = -200.0f;
            for (int b = 0; b < grv::dsp::SpectrumAnalyzer::numBands; ++b) { if (mid[(size_t) b] > mid[(size_t) best]) best = b; sideMax = std::max (sideMax, side[(size_t) b]); }
            const auto hz = grv::dsp::SpectrumAnalyzer::bandHz (best);
            int lowBand = 0; while (grv::dsp::SpectrumAnalyzer::bandHz (lowBand) < 100.0) ++lowBand;
            const bool ok = hz > 880.0 && hz < 1130.0 && std::abs (mid[(size_t) best] + 6.0f) < 1.5f && sideMax < -80.0f && mid[(size_t) lowBand] < -60.0f;
            std::printf ("Analyseur de spectre : bande %.0f Hz %.1f dB, c\xC3\xB4t\xC3\xA9s %.0f dB, 100 Hz %.0f dB : %s\n", hz, mid[(size_t) best], sideMax, mid[(size_t) lowBand], ok ? "OK" : "ECHEC");
            if (! ok) return 1;
        }
        // Sauvegarde puis relecture dans un second processeur : patterns, lancement, MIDI, verrous
        auto& seq = proc.getEngine().sequencer();
        seq.setStepP (0, 3, 2, 7, 99); seq.setPatternLength (0, 3, 48); seq.setPlaying (0, 3); seq.setEditing (0, 3);
        seq.setLaunchQuant (3); seq.setRecordQuantize (50);
        proc.getEngine().setStepExtraP (0, 3, 2, 7, grv::engine::Condition::fill, 1, 2, 70, 0.0);
        proc.getEngine().setStepRatchetP (0, 3, 2, 7, 3, -40, 0.0);
        proc.getEngine().setStepsFlam (0, 3, { { 2, 8 } }, 25, 0.0);
        seq.setStepP (0, 3, 2, 8, 90);
        seq.setStepFlagsP (0, 3, 5, grv::engine::stepAccent | grv::engine::stepSlide);
        seq.setLaneLengthP (0, 3, 4, 7);
        seq.setPatternDivision (2, 1, grv::engine::Division::eighthTriplet);
        seq.setAccentAmount (0, 80);
        proc.setRowScene (3, 12);
        proc.setMidiMapping (0, 1, 74, proc.getEngine().findParam (1, "inst", "cutoff"));
        proc.setMidiMapping (1, 9, 40, grv::GrooveboxProcessor::midiBreak);
        proc.setMidiMapping (1, 9, 41, grv::GrooveboxProcessor::midiMuteBase - 2);  // coupure de la piste 3
        proc.setMidiMapping (0, 9, 20, grv::GrooveboxProcessor::midiSoloBase - 15); // solo de la piste 16
        proc.getEngine().applyChain (grv::engine::masterChannel, "Bus principal", 0.0);
        juce::MemoryBlock state;
        proc.getStateInformation (state);
        grv::GrooveboxProcessor other;
        other.setStateInformation (state.getData(), (int) state.getSize());
        auto& s2 = other.getEngine().sequencer();
        const auto* ex = other.getEngine().getExtras().find (0, 3, 2, 7);
        const auto* exFlam = other.getEngine().getExtras().find (0, 3, 2, 8);
        bool hasCutoff = false, hasBreak = false, hasMute = false, hasSolo = false;
        for (const auto& m : other.getMidiMappings())
        {
            hasCutoff = hasCutoff || (m.kind == 0 && m.channel == 1 && m.number == 74 && m.target == other.getEngine().findParam (1, "inst", "cutoff"));
            hasBreak = hasBreak || (m.kind == 1 && m.channel == 9 && m.number == 40 && m.target == grv::GrooveboxProcessor::midiBreak);
            hasMute = hasMute || (m.kind == 1 && m.number == 41 && m.target == grv::GrooveboxProcessor::midiMuteBase - 2);
            hasSolo = hasSolo || (m.kind == 0 && m.number == 20 && m.target == grv::GrooveboxProcessor::midiSoloBase - 15);
        }
        const bool ok = s2.getStepP (0, 3, 2, 7) == 99 && s2.getPatternLength (0, 3) == 48 && s2.getPlaying (0) == 3 && s2.getEditing (0) == 3
                     && s2.getLaunchQuant() == 3 && s2.getRecordQuantize() == 50 && ex != nullptr && ex->cond == grv::engine::Condition::fill && ex->prob == 70 && ex->ratchet == 3 && ex->ratchetSlope == -40
                     && other.getRowScene (3) == 12 && hasCutoff && hasBreak && hasMute && hasSolo
                     && exFlam != nullptr && exFlam->flam == 25 && s2.getStepFlagsP (0, 3, 5) == (grv::engine::stepAccent | grv::engine::stepSlide)
                     && s2.getLaneLengthP (0, 3, 4) == 7 && s2.getAccentAmount (0) == 80
                     && s2.getPatternDivision (2, 1) == grv::engine::Division::eighthTriplet && s2.getPatternDivision (2, 0) == grv::engine::Division::sixteenth
                     && other.getEngine().getInsertTypes (grv::engine::masterChannel)[2] == "limiter"
                     && s2.getStepP (0, 0, 0, 0) > 0; // pattern 1 du projet de démonstration
        std::printf ("Sauvegarde et relecture (%d octets) : %s\n", (int) state.getSize(), ok ? "OK" : "\xC3\x89" "CHEC");

        // Annuler / rétablir / photo : réglage, pas, instrument, échantillon, rappel de photo
        bool histOk = true;
        double worstMs = 0.0;
        {
            auto& e = proc.getEngine();
            auto& sq = e.sequencer();
            auto tick = [&] { for (int k = 0; k < 40; ++k) proc.historyTick(); };
            auto timed = [&] (auto f) { const auto t0 = juce::Time::getMillisecondCounterHiRes(); const auto r = f(); worstMs = std::max (worstMs, juce::Time::getMillisecondCounterHiRes() - t0); return r; };
            auto near = [] (float a, float b) { return std::abs (a - b) < 1.0e-5f; };
            juce::MemoryBlock wav;
            {
                juce::WavAudioFormat fmt;
                auto* mos = new juce::MemoryOutputStream (wav, false);
                std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (mos, 48000.0, 1, 16, {}, 0));
                juce::AudioBuffer<float> sine (1, 4800);
                for (int i = 0; i < 4800; ++i) sine.setSample (0, i, 0.5f * std::sin (0.05f * (float) i));
                if (w != nullptr) w->writeFromAudioSampleBuffer (sine, 0, 4800);
            }
            tick();
            const auto g = e.findParam (1, "inst", "cutoff");
            const auto before = e.getParam (g);
            const auto type3 = e.getTrackInfo (3).instrumentType;
            const auto step = sq.getStepP (2, 0, 0, 5);
            e.setParam (g, 0.123f); tick();
            sq.setStepP (2, 0, 0, 5, 77); tick();
            e.setInstrument (3, "acid", 0.0); tick();
            e.setInstrument (5, "octa", 0.0); tick();
            histOk = histOk && proc.loadSample (5, 1, "sinus.wav", wav).isEmpty(); tick();
            const auto* bytes = e.getSample (5, 1) != nullptr ? e.getSample (5, 1)->original.get() : nullptr;
            e.setInstrument (5, "acid", 0.0); tick();
            histOk = histOk && e.getSample (5, 1) == nullptr;
            histOk = histOk && timed ([&] { return proc.undo(); }) && e.getTrackInfo (5).instrumentType == "octa"
                   && e.getSample (5, 1) != nullptr && e.getSample (5, 1)->original.get() == bytes; // échantillon revenu sans copie
            histOk = histOk && proc.redo() && e.getTrackInfo (5).instrumentType == "acid";
            histOk = histOk && proc.undo();
            proc.takePhoto();
            e.setParam (g, 0.9f); tick();
            histOk = histOk && timed ([&] { return proc.recallPhoto(); }) && near (e.getParam (g), 0.123f);
            histOk = histOk && proc.undo() && near (e.getParam (g), 0.9f);             // le rappel s'annule
            histOk = histOk && proc.undo() && near (e.getParam (g), 0.123f);           // photo
            histOk = histOk && proc.undo() && e.getSample (5, 1) == nullptr;            // échantillon retiré
            histOk = histOk && proc.undo() && e.getTrackInfo (5).instrumentType.empty() == (e.instrumentDesc (5) == nullptr);
            histOk = histOk && proc.undo() && e.getTrackInfo (3).instrumentType == type3;
            histOk = histOk && proc.undo() && sq.getStepP (2, 0, 0, 5) == step;
            histOk = histOk && proc.undo() && near (e.getParam (g), before);
            histOk = histOk && proc.redo() && near (e.getParam (g), 0.123f) && sq.getPlaying (0) == 3; // le jeu en cours n'est pas touché
            histOk = histOk && proc.hasPhoto();
            {
                // v0.46 : modèle — réglages gardés, patterns, verrous et morceau retirés
                grv::GrooveboxProcessor tp;
                auto& te = tp.getEngine();
                const auto gv = te.findParam (0, "mix", "volume");
                te.setParam (gv, 0.42f);
                te.sequencer().setStepP (0, 0, 0, 3, 99);
                te.setStepLockP (0, 0, 0, 3, gv, 0.9f, 0.0);
                auto song = te.getSong(); song.rows.resize (2); te.setSong (std::move (song), 0.0);
                juce::MemoryOutputStream tos;
                const bool wrote = tp.writeProjectFile (tos, true);
                grv::GrooveboxProcessor tq;
                juce::MemoryInputStream tis (tos.getData(), tos.getDataSize(), false);
                const auto terr = tq.readProjectFile (tis);
                auto& qe = tq.getEngine();
                bool emptySteps = true;
                for (int st = 0; st < 64; ++st) emptySteps = emptySteps && qe.sequencer().getStepP (0, 0, 0, st) == 0;
                const bool tplOk = wrote && terr.isEmpty() && near (qe.getParam (gv), 0.42f) && emptySteps
                    && qe.getExtras().find (0, 0, 0, 3) == nullptr && qe.getSong().rows.empty() && qe.getTrackInfo (0).instrumentType == te.getTrackInfo (0).instrumentType;
                std::printf ("  mod\xC3\xA8le : r\xC3\xA9glages gard\xC3\xA9s, \xC3\xA9" "critures retir\xC3\xA9" "es %d\n", (int) tplOk);
                histOk = histOk && tplOk;
            }
            {
                // v0.46 : le rappel ne touche pas au morceau (rangées enregistrées après la photo)
                grv::GrooveboxProcessor sp;
                auto& se = sp.getEngine();
                const auto gv = se.findParam (0, "mix", "volume");
                se.setParam (gv, 0.3f);
                sp.takePhoto();
                auto song = se.getSong();
                song.rows.resize (3);
                se.setSong (std::move (song), 0.0);
                se.setParam (gv, 0.8f);
                const bool keepSong = sp.recallPhoto() && se.getSong().rows.size() == 3 && near (se.getParam (gv), 0.3f);
                std::printf ("  rappel sans toucher au morceau %d\n", (int) keepSong);
                histOk = histOk && keepSong;
            }
            const auto name0 = e.getTrackInfo (0).name;
            e.setTrackName (0, "Renomm\xC3\xA9" "e"); tick();                           // un nom seul fait aussi une étape
            histOk = histOk && proc.undo() && e.getTrackInfo (0).name == name0;
            // verrou, condition, longueur de pattern et coupure de voie, défaits ensemble
            const auto* exBefore = e.getExtras().find (1, 0, 0, 2);
            const bool hadExtra = exBefore != nullptr;
            const auto len1 = sq.getPatternLength (1, 0);
            e.setStepLockP (1, 0, 0, 2, g, 0.33f, 0.0);
            e.setStepExtraP (1, 0, 0, 2, grv::engine::Condition::fill, 1, 2, 40, 0.0);
            sq.setPatternLength (1, 0, 32); sq.setLaneMute (1, 3, true); tick();
            histOk = histOk && proc.undo() && (e.getExtras().find (1, 0, 0, 2) != nullptr) == hadExtra
                   && sq.getPatternLength (1, 0) == len1 && ! sq.getLaneMute (1, 3);
            histOk = histOk && proc.redo() && e.getExtras().find (1, 0, 0, 2) != nullptr && e.getExtras().find (1, 0, 0, 2)->prob == 40
                   && sq.getPatternLength (1, 0) == 32 && sq.getLaneMute (1, 3);
            histOk = histOk && proc.undo();
            sq.setPatternDivision (3, 0, grv::engine::Division::eighthTriplet); tick(); // division d'un pattern
            histOk = histOk && proc.undo() && sq.getPatternDivision (3, 0) == grv::engine::Division::sixteenth;
            e.setSceneLock (2, g, 0.7f, 0.0); tick();                               // une scène aussi
            histOk = histOk && proc.undo() && e.getScenes().scenes[2].empty();
            // contrôle au repos (une copie par seconde) : coût sur le fil de l'interface
            double worstIdle = 0.0;
            for (int k = 0; k < 100; ++k)
            {
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                proc.historyTick();
                worstIdle = std::max (worstIdle, juce::Time::getMillisecondCounterHiRes() - t0);
            }
            std::printf ("Contr\xC3\xB4le d'historique au repos : %.2f ms au pire\n", worstIdle);
        }
        std::printf ("Annuler / r\xC3\xA9tablir / photo (pire rappel %.1f ms) : %s\n", worstMs, histOk ? "OK" : "\xC3\x89" "CHEC");

        // Projet de version 3 (8 modes d'arpège) : le mode « Haut-bas » reste « Haut-bas »
        bool arpOk = false;
        {
            grv::GrooveboxProcessor a3;
            a3.getEngine().setInstrument (6, "mono", 0.0);
            juce::MemoryOutputStream mos;
            a3.writeProjectFile (mos);
            auto xml = mos.toString();
            const auto oldNorm = juce::String (3.0 / 7.0, 9); // Haut-bas, rangé sur 8 choix
            for (int v = 4; v <= 19; ++v) xml = xml.replace ("version=\"" + juce::String (v) + "\"", "version=\"3\"");
            xml = xml.replace ("<PARAMS>", "<PARAMS><P c=\"6\" a=\"inst\" i=\"arp_mode\" v=\"" + oldNorm + "\"/>");
            grv::GrooveboxProcessor b3;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = b3.readProjectFile (mis);
            const auto g = b3.getEngine().findParam (6, "inst", "arp_mode");
            arpOk = err.isEmpty() && g >= 0 && (int) std::lround (b3.getEngine().describe (g)->toPlain (b3.getEngine().getParam (g))) == 3;
        }
        std::printf ("Modes d'arp\xC3\xA8ge des anciens projets : %s\n", arpOk ? "OK" : "\xC3\x89" "CHEC");

        // Coupure de piste depuis le contrôleur : chaque appui de la note apprise bascule la piste 3
        bool midiMuteOk = false;
        {
            juce::AudioBuffer<float> b3 (2, block);
            juce::MidiBuffer m3;
            m3.addEvent (juce::MidiMessage::noteOn (10, 41, (juce::uint8) 100), 0);
            const bool before = proc.getEngine().sequencer().getTrackMute (2);
            proc.processBlock (b3, m3);
            const bool after1 = proc.getEngine().sequencer().getTrackMute (2);
            m3.clear(); m3.addEvent (juce::MidiMessage::noteOn (10, 41, (juce::uint8) 100), 0);
            b3.clear(); proc.processBlock (b3, m3);
            midiMuteOk = after1 != before && proc.getEngine().sequencer().getTrackMute (2) == before;
            // v0.46 : coupure d'un groupe (G2) par une note, solo d'un groupe (G1) par un bouton CC
            grv::GrooveboxProcessor bp;
            bp.prepareToPlay (sr, block);
            const auto g2 = grv::engine::groupBase + 1, g1 = grv::engine::groupBase;
            bp.setMidiMapping (1, 9, 50, grv::GrooveboxProcessor::midiBusMuteBase - (g2 - grv::engine::numTracks));
            bp.setMidiMapping (0, 9, 30, grv::GrooveboxProcessor::midiBusSoloBase - (g1 - grv::engine::numTracks));
            juce::MidiBuffer mb; juce::AudioBuffer<float> bb (2, block);
            mb.addEvent (juce::MidiMessage::noteOn (10, 50, (juce::uint8) 100), 0);
            mb.addEvent (juce::MidiMessage::controllerEvent (10, 30, 127), 1);
            bp.processBlock (bb, mb);
            const bool busOn = bp.getEngine().getBusMute (g2) && bp.getEngine().getBusSolo (g1);
            mb.clear(); bb.clear();
            mb.addEvent (juce::MidiMessage::noteOn (10, 50, (juce::uint8) 100), 0);
            mb.addEvent (juce::MidiMessage::controllerEvent (10, 30, 0), 1);
            bp.processBlock (bb, mb);
            const bool busOff = ! bp.getEngine().getBusMute (g2) && ! bp.getEngine().getBusSolo (g1);
            std::printf ("  bus : coupure / solo %d, rel\xC3\xA2" "ch\xC3\xA9" "s %d\n", (int) busOn, (int) busOff);
            // v0.46 : déplacer un insert : la commande MIDI apprise et la macro suivent l'effet
            auto& be = bp.getEngine();
            be.setInsert (0, 0, "sculptor", 0.0); be.setInsert (0, 1, "comp", 0.0);
            const auto gMix = be.findParam (0, "fx1", "mix");
            bp.setMidiMapping (0, 9, 40, gMix);
            bp.toggleMacroTarget (5, gMix);
            bp.moveInsert (0, 0, 1);
            const auto moved = be.findParam (0, "fx2", "mix");
            bool midiFollows = false;
            for (const auto& m : bp.getMidiMappings()) midiFollows = midiFollows || (m.kind == 0 && m.number == 40 && m.target == moved);
            const bool macroFollows = bp.getMacroLinks (5).size() == 1 && bp.getMacroLinks (5)[0].global == moved && be.getInsertTypes (0)[1] == "sculptor";
            std::printf ("  insert d\xC3\xA9plac\xC3\xA9 : MIDI %d, macro %d\n", (int) midiFollows, (int) macroFollows);
            // v0.46 : rangées au contrôleur : suivante, suivante, précédente, puis D directement
            using GP = grv::GrooveboxProcessor;
            bp.setMidiMapping (1, 9, 60, GP::midiRowNext); bp.setMidiMapping (1, 9, 61, GP::midiRowPrev); bp.setMidiMapping (1, 9, 62, GP::midiRowBase - 3);
            auto hit = [&] (int note) { juce::MidiBuffer m; juce::AudioBuffer<float> a (2, block); m.addEvent (juce::MidiMessage::noteOn (10, note, (juce::uint8) 100), 0); bp.processBlock (a, m); return bp.getLastRow(); };
            const int r1 = hit (62), r2 = hit (60), r3 = hit (60), r4 = hit (61);
            const bool rowsOk = r1 == 3 && r2 == 4 && r3 == 5 && r4 == 4;
            std::printf ("  rang\xC3\xA9" "es au contr\xC3\xB4leur : %d %d %d %d\n", r1, r2, r3, r4);
            // v0.46 : canaux MIDI — piste 4 sur le canal 5 ; commandes limitées au canal 2
            be.setInstrument (3, "mono", 0.0);
            bp.setTrackMidiChannel (3, 5);
            {
                juce::MidiBuffer m; juce::AudioBuffer<float> a (2, block);
                m.addEvent (juce::MidiMessage::noteOn (5, 60, (juce::uint8) 100), 0);
                bp.processBlock (a, m);
            }
            const bool chanNote = be.isNoteDown (3, 60);
            bp.setMidiRoute (0, 2, true);
            {
                juce::MidiBuffer m; juce::AudioBuffer<float> a (2, block);
                m.addEvent (juce::MidiMessage::controllerEvent (10, 30, 127), 0); // solo G1 appris sur le canal 10 : ignoré
                bp.processBlock (a, m);
            }
            const bool ctrlFiltered = ! be.getBusSolo (g1);
            bp.setMidiRoute (0, 0, true);
            std::printf ("  canaux MIDI : piste sur son canal %d, commandes filtr\xC3\xA9" "es %d\n", (int) chanNote, (int) ctrlFiltered);
            midiMuteOk = midiMuteOk && busOn && busOff && midiFollows && macroFollows && rowsOk && chanNote && ctrlFiltered;
        }
        std::printf ("Coupure de piste au contr\xC3\xB4leur MIDI : %s\n", midiMuteOk ? "OK" : "\xC3\x89" "CHEC");

        // Apprentissage sur une commande déjà prise : rien ne change avant la réponse ; « Remplacer »
        // la donne à la nouvelle cible ; la touche Arrêt apprise arrête la lecture
        bool midiConflictOk = false;
        {
            juce::AudioBuffer<float> bc (2, block);
            juce::MidiBuffer mc;
            proc.startMidiLearn (grv::GrooveboxProcessor::midiStop);
            mc.addEvent (juce::MidiMessage::noteOn (10, 41, (juce::uint8) 100), 0); // déjà : coupure de la piste 3
            proc.processBlock (bc, mc);
            int kind = -1, ch = -1, num = -1, oldT = 0, newT = 0;
            const bool asked = proc.getMidiConflict (kind, ch, num, oldT, newT) && kind == 1 && ch == 9 && num == 41
                            && oldT == grv::GrooveboxProcessor::midiMuteBase - 2 && newT == grv::GrooveboxProcessor::midiStop;
            proc.resolveMidiConflict (true);
            bool mapped = false;
            for (const auto& m : proc.getMidiMappings()) mapped = mapped || (m.kind == 1 && m.channel == 9 && m.number == 41 && m.target == grv::GrooveboxProcessor::midiStop);
            proc.getEngine().sequencer().requestPlay();
            bc.clear(); mc.clear(); proc.processBlock (bc, mc);
            const bool wasRunning = proc.getEngine().sequencer().isRunning();
            mc.addEvent (juce::MidiMessage::noteOn (10, 41, (juce::uint8) 100), 0);
            bc.clear(); proc.processBlock (bc, mc);
            mc.clear(); bc.clear(); proc.processBlock (bc, mc);
            midiConflictOk = asked && mapped && wasRunning && ! proc.getEngine().sequencer().isRunning();
            if (! midiConflictOk) std::printf ("  (demand\xC3\xA9 %d, appris %d, en lecture %d, arr\xC3\xAAt\xC3\xA9 %d)\n", (int) asked, (int) mapped, (int) wasRunning, (int) ! proc.getEngine().sequencer().isRunning());
        }
        std::printf ("Commande MIDI d\xC3\xA9j\xC3\xA0 affect\xC3\xA9" "e, touche Arr\xC3\xAA" "t : %s\n", midiConflictOk ? "OK" : "\xC3\x89" "CHEC");

        // Projet de version 6 : la division « 1/4 » d'une source de modulation reste « 1/4 » (16 et 8 mesures ajoutées)
        bool divOk = false;
        {
            grv::GrooveboxProcessor a6;
            juce::MemoryOutputStream mos;
            a6.writeProjectFile (mos);
            auto xml = mos.toString();
            xml = xml.replace ("version=\"19\"", "version=\"6\"");
            xml = xml.replace ("<PARAMS>", "<PARAMS><P c=\"2\" a=\"mod1\" i=\"division\" v=\"" + juce::String (4.5 / 11.0, 9) + "\"/>");
            grv::GrooveboxProcessor b6;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = b6.readProjectFile (mis);
            const auto g = b6.getEngine().findParam (2, "mod1", "division");
            const auto* d = g >= 0 ? b6.getEngine().describe (g) : nullptr;
            divOk = err.isEmpty() && d != nullptr && d->choices[(size_t) std::lround (d->toPlain (b6.getEngine().getParam (g)))] == "1/4";
        }
        std::printf ("Divisions des anciens projets : %s\n", divOk ? "OK" : "\xC3\x89" "CHEC");

        // Notes longues (au-delà de 32 pas) : écrites, enregistrées et relues sans perte
        bool longOk = false;
        {
            grv::GrooveboxProcessor a;
            auto& sq = a.getEngine().sequencer();
            sq.setStep (4, 0, 2, 100); sq.setLength8 (4, 0, 2, 1024);
            sq.setStep (4, 1, 5, 100); sq.setLength8 (4, 1, 5, 12);
            const bool clamped = (sq.setLength8 (4, 2, 0, 5000), sq.getLength8 (4, 2, 0) == 1024);
            juce::MemoryOutputStream mos;
            a.writeProjectFile (mos);
            grv::GrooveboxProcessor b;
            juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
            const auto err = b.readProjectFile (mis);
            auto& sb = b.getEngine().sequencer();
            longOk = clamped && err.isEmpty() && sb.getLength8 (4, 0, 2) == 1024 && sb.getLength8 (4, 1, 5) == 12;
        }
        std::printf ("Notes de 128 pas : %s\n", longOk ? "OK" : "\xC3\x89" "CHEC");

        // Projet de version 8 : la destination d'un LFO du sampler de performance reste la même
        bool octaDstOk = false;
        {
            grv::GrooveboxProcessor a8;
            a8.getEngine().setInstrument (6, "octa", 0.0);
            juce::MemoryOutputStream mos;
            a8.writeProjectFile (mos);
            auto xml = mos.toString().replace ("version=\"19\"", "version=\"8\"");
            const auto gNew = a8.getEngine().findParam (6, "inst", "t1_mx1_dst");
            const auto oldCount = gNew >= 0 ? (int) a8.getEngine().describe (gNew)->choices.size() - 8 : 1;
            xml = xml.replace ("<PARAMS>", "<PARAMS><P c=\"6\" a=\"inst\" i=\"mx1_dst\" v=\"" + juce::String ((3.0 + 0.5) / oldCount, 9) + "\"/>");
            grv::GrooveboxProcessor b8;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = b8.readProjectFile (mis);
            const auto g = b8.getEngine().findParam (6, "inst", "t1_mx1_dst");
            octaDstOk = err.isEmpty() && g >= 0 && (int) std::lround (b8.getEngine().describe (g)->toPlain (b8.getEngine().getParam (g))) == 3;
        }
        std::printf ("Destinations du sampler des anciens projets : %s\n", octaDstOk ? "OK" : "\xC3\x89" "CHEC");

        // Projet de version 9 : machine Pickup (5e de 5 machines) reste Pickup (2e de 2)
        bool machineOk = false;
        {
            grv::GrooveboxProcessor a9;
            a9.getEngine().setInstrument (6, "octa", 0.0);
            juce::MemoryOutputStream mos;
            a9.writeProjectFile (mos);
            auto xml = mos.toString().replace ("version=\"19\"", "version=\"9\"");
            xml = xml.replace ("<PARAMS>", "<PARAMS><P c=\"6\" a=\"inst\" i=\"machine\" v=\"" + juce::String (4.5 / 5.0, 9) + "\"/>");
            grv::GrooveboxProcessor b9;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = b9.readProjectFile (mis);
            const auto g = b9.getEngine().findParam (6, "inst", "t1_machine");
            machineOk = err.isEmpty() && g >= 0 && (int) std::lround (b9.getEngine().describe (g)->toPlain (b9.getEngine().getParam (g))) == 1;
        }
        // Projet de version 10 : Static (2e de 3) devient Flex, Pickup (3e de 3) reste Pickup
        for (const auto& [old, expected] : { std::pair<double, int> { 1.5 / 3.0, 0 }, std::pair<double, int> { 2.5 / 3.0, 1 } })
        {
            grv::GrooveboxProcessor a10;
            a10.getEngine().setInstrument (6, "octa", 0.0);
            juce::MemoryOutputStream mos;
            a10.writeProjectFile (mos);
            auto xml = mos.toString().replace ("version=\"19\"", "version=\"10\"");
            xml = xml.replace ("<PARAMS>", "<PARAMS><P c=\"6\" a=\"inst\" i=\"machine\" v=\"" + juce::String (old, 9) + "\"/>");
            grv::GrooveboxProcessor b10;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = b10.readProjectFile (mis);
            const auto g = b10.getEngine().findParam (6, "inst", "t1_machine");
            machineOk = machineOk && err.isEmpty() && g >= 0 && (int) std::lround (b10.getEngine().describe (g)->toPlain (b10.getEngine().getParam (g))) == expected;
        }
        std::printf ("Machine Pickup des anciens projets : %s\n", machineOk ? "OK" : "\xC3\x89" "CHEC");

        // Projet de version 11 : générateurs de l'orbiteur rangés sur 7 choix (A à F, bruit) ; ils gardent
        // leur choix (E reste E, le bruit reste le bruit) maintenant qu'il y en a 8 (position W)
        bool orbitOk = true;
        for (const auto& [old, expected] : { std::pair<double, int> { 4.5 / 7.0, 4 }, std::pair<double, int> { 6.5 / 7.0, 6 } })
        {
            grv::GrooveboxProcessor a11;
            a11.getEngine().setInstrument (6, "orbit", 0.0);
            juce::MemoryOutputStream mos;
            a11.writeProjectFile (mos);
            auto xml = mos.toString().replace ("version=\"19\"", "version=\"11\"");
            xml = xml.replace ("<PARAMS>", "<PARAMS><P c=\"6\" a=\"inst\" i=\"bg2\" v=\"" + juce::String (old, 9) + "\"/>");
            grv::GrooveboxProcessor b11;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = b11.readProjectFile (mis);
            const auto g = b11.getEngine().findParam (6, "inst", "bg2");
            orbitOk = orbitOk && err.isEmpty() && g >= 0 && (int) std::lround (b11.getEngine().describe (g)->toPlain (b11.getEngine().getParam (g))) == expected;
        }
        std::printf ("G\xC3\xA9n\xC3\xA9rateurs de l'orbiteur des anciens projets : %s\n", orbitOk ? "OK" : "\xC3\x89" "CHEC");

        // Projet de version 13 : sources des vocodeurs rangées sur 19 choix ; « Piste 16 » le reste
        // maintenant que « Prise de son » est ajoutée à la fin (20 choix) ; phrase tapée relue
        bool vocOk = true;
        for (const auto& [old, expected] : { std::pair<double, int> { 18.5 / 19.0, 18 }, std::pair<double, int> { 1.5 / 19.0, 1 } })
        {
            grv::GrooveboxProcessor a13;
            a13.getEngine().setInstrument (6, "voc16", 0.0);
            a13.getEngine().setTrackSpeech (6, "Bonjour \xC3\xA0 tous", 0.0);
            juce::MemoryOutputStream mos;
            a13.writeProjectFile (mos);
            auto xml = mos.toString().replace ("version=\"19\"", "version=\"13\"");
            xml = xml.replace ("<PARAMS>", "<PARAMS><P c=\"6\" a=\"inst\" i=\"src\" v=\"" + juce::String (old, 9) + "\"/>");
            grv::GrooveboxProcessor b13;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = b13.readProjectFile (mis);
            const auto g = b13.getEngine().findParam (6, "inst", "src");
            vocOk = vocOk && err.isEmpty() && g >= 0 && (int) std::lround (b13.getEngine().describe (g)->toPlain (b13.getEngine().getParam (g))) == expected
                  && b13.getEngine().getTrackInfo (6).speech == "Bonjour \xC3\xA0 tous";
        }
        std::printf ("Sources des vocodeurs et phrase tap\xC3\xA9" "e des anciens projets : %s\n", vocOk ? "OK" : "\xC3\x89" "CHEC");

        // Projet de version 14 : algorithme de l'Atmosphère rangé sur 5 choix ; « Floraison » le reste
        // maintenant qu'il y en a 12 (v0.35) ; les nouveaux réglages prennent leur valeur d'origine
        bool atmosOk = true;
        for (const auto& [old, expected] : { std::pair<double, int> { 4.5 / 5.0, 4 }, std::pair<double, int> { 2.5 / 5.0, 2 } })
        {
            grv::GrooveboxProcessor a14;
            a14.getEngine().setInsert (6, 1, "atmos", 0.0);
            juce::MemoryOutputStream mos;
            a14.writeProjectFile (mos);
            auto xml = mos.toString().replace ("version=\"19\"", "version=\"14\"");
            xml = xml.replace ("<PARAMS>", "<PARAMS><P c=\"6\" a=\"fx2\" i=\"mode\" v=\"" + juce::String (old, 9) + "\"/>");
            grv::GrooveboxProcessor b14;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = b14.readProjectFile (mis);
            const auto g = b14.getEngine().findParam (6, "fx2", "mode");
            const auto gp = b14.getEngine().findParam (6, "fx2", "persist");
            atmosOk = atmosOk && err.isEmpty() && g >= 0 && gp >= 0
                   && (int) std::lround (b14.getEngine().describe (g)->toPlain (b14.getEngine().getParam (g))) == expected
                   && b14.getEngine().getParam (gp) < 0.5f;
        }
        std::printf ("Algorithmes de l'Atmosph\xC3\xA8re des anciens projets : %s\n", atmosOk ? "OK" : "\xC3\x89" "CHEC");

        // Projet de version 15 : pas de Hopscotch rangés sur 8 choix, longueur sur 1 à 16 pas (v0.39 : 51 choix, 64 pas)
        bool hopOk = false;
        {
            grv::GrooveboxProcessor a15;
            a15.getEngine().setInsert (6, 0, "stepfx", 0.0);
            juce::MemoryOutputStream mos;
            a15.writeProjectFile (mos);
            auto xml = mos.toString().replace ("version=\"19\"", "version=\"15\"");
            xml = xml.replace ("<PARAMS>", "<PARAMS><P c=\"6\" a=\"fx1\" i=\"s3\" v=\"" + juce::String (3.5 / 8.0, 9) + "\"/>"
                                           "<P c=\"6\" a=\"fx1\" i=\"steps\" v=\"" + juce::String (7.0 / 15.0, 9) + "\"/>");
            grv::GrooveboxProcessor b15;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = b15.readProjectFile (mis);
            const auto gs = b15.getEngine().findParam (6, "fx1", "s3"), gl = b15.getEngine().findParam (6, "fx1", "steps");
            hopOk = err.isEmpty() && gs >= 0 && gl >= 0
                 && (int) std::lround (b15.getEngine().describe (gs)->toPlain (b15.getEngine().getParam (gs))) == 3      // Filtre
                 && (int) std::lround (b15.getEngine().describe (gl)->toPlain (b15.getEngine().getParam (gl))) == 8;     // 8 pas
        }
        std::printf ("Pas de Hopscotch des anciens projets : %s\n", hopOk ? "OK" : "\xC3\x89" "CHEC");

        // Projet de version 16 : emplacement de Pieuvre 8 rangé sur 17 choix (v0.40 : réserve commune, 153 choix)
        bool slotOk = false;
        {
            grv::GrooveboxProcessor a16;
            a16.getEngine().setInstrument (6, "octa", 0.0);
            juce::MemoryOutputStream mos;
            a16.writeProjectFile (mos);
            auto xml = mos.toString().replace ("version=\"19\"", "version=\"16\"");
            xml = xml.replace ("<PARAMS>", "<PARAMS><P c=\"6\" a=\"inst\" i=\"slot\" v=\"" + juce::String (5.5 / 17.0, 9) + "\"/>");
            grv::GrooveboxProcessor b16;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = b16.readProjectFile (mis);
            const auto g = b16.getEngine().findParam (6, "inst", "t1_slot");
            slotOk = err.isEmpty() && g >= 0 && (int) std::lround (b16.getEngine().describe (g)->toPlain (b16.getEngine().getParam (g))) == 5;
        }
        std::printf ("Emplacements de Pieuvre 8 des anciens projets : %s\n", slotOk ? "OK" : "\xC3\x89" "CHEC");


        // Inserts de pad : effet, réglage et interrupteur enregistrés puis relus ; annuler les retire
        bool padFxOk = false;
        {
            grv::GrooveboxProcessor a;
            auto& e = a.getEngine();
            e.setInstrument (3, "drumA", 0.0);
            e.setPadInsert (3, 5, 1, "atmos", 0.0);
            const auto g = e.findParam (3, "pfx6_2", "decay");
            if (g >= 0) e.setParam (g, e.describe (g)->toNorm (12.0f));
            if (const auto b = e.findParam (3, "padbyp", "b6_2"); b >= 0) e.setParam (b, 1.0f);
            juce::MemoryOutputStream mos;
            a.writeProjectFile (mos);
            grv::GrooveboxProcessor b;
            juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
            const auto err = b.readProjectFile (mis);
            auto& eb = b.getEngine();
            const auto gb = eb.findParam (3, "pfx6_2", "decay");
            const auto bb = eb.findParam (3, "padbyp", "b6_2");
            padFxOk = err.isEmpty() && eb.getPadInsertType (3, 5, 1) == "atmos" && gb >= 0 && std::abs (eb.describe (gb)->toPlain (eb.getParam (gb)) - 12.0f) < 0.05f
                   && bb >= 0 && eb.getParam (bb) > 0.5f;
        }
        std::printf ("Inserts de pad enregistr\xC3\xA9s et relus : %s\n", padFxOk ? "OK" : "\xC3\x89" "CHEC");

        // Morceau : rangées, modes et courbe (réglage d'un insert, par adresse) enregistrés puis relus ;
        // annuler revient au morceau d'avant
        bool songOk = false;
        {
            grv::GrooveboxProcessor a;
            auto& e = a.getEngine();
            e.setInstrument (2, "acid", 0.0);
            e.setInsert (2, 1, "delay", 0.0);
            const auto g = e.findParam (2, "fx2", "mix");
            grv::engine::Song sg;
            sg.songMode = true; sg.loop = false; sg.autoRead = true;
            grv::engine::SongRow r1, r2;
            r1.bars = 3; r1.scene = 4; r1.pat[2] = 1; r2.bars = 2; r2.pat[2] = grv::engine::songStop;
            sg.rows = { r1, r2 };
            grv::engine::AutoLane lane;
            lane.global = g; lane.enabled = false; lane.pts = { { 0.0f, 0.25f }, { 6.5f, 0.75f } };
            sg.lanes = { lane };
            // courbe de pattern (piste 3, pattern C) sur la coupure de la basse acide
            const auto cut = e.findParam (2, "inst", "cutoff");
            grv::engine::PatternLane pl;
            pl.track = 2; pl.pattern = 2; pl.lane.global = cut; pl.lane.pts = { { 1.0f, 0.1f }, { 9.0f, 0.9f } };
            sg.patLanes = { pl };
            e.setSong (sg, 0.0);
            juce::MemoryOutputStream mos;
            a.writeProjectFile (mos);
            grv::GrooveboxProcessor b;
            juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
            const auto err = b.readProjectFile (mis);
            const auto& s2 = b.getEngine().getSong();
            const auto g2 = b.getEngine().findParam (2, "fx2", "mix");
            songOk = err.isEmpty() && g >= 0 && s2.songMode && ! s2.loop && s2.rows.size() == 2 && s2.rows[0].bars == 3 && s2.rows[0].scene == 4
                  && s2.rows[0].pat[2] == 1 && s2.rows[1].pat[2] == grv::engine::songStop && s2.rows[1].pat[0] == grv::engine::songKeep
                  && s2.lanes.size() == 1 && s2.lanes[0].global == g2 && ! s2.lanes[0].enabled && s2.lanes[0].pts.size() == 2
                  && std::abs (s2.lanes[0].valueAt (3.25) - 0.5f) < 1.0e-3f
                  && cut >= 0 && s2.patLanes.size() == 1 && s2.patLanes[0].track == 2 && s2.patLanes[0].pattern == 2
                  && s2.patLanes[0].lane.global == b.getEngine().findParam (2, "inst", "cutoff") && std::abs (s2.patLanes[0].lane.valueAt (5.0) - 0.5f) < 1.0e-3f;
            // historique : un morceau vidé se rétablit par Ctrl+Z
            auto tick = [&] { for (int k = 0; k < 40; ++k) b.historyTick(); };
            tick();
            b.getEngine().setSong ({}, 0.0);
            tick();
            songOk = songOk && b.getEngine().getSong().rows.empty() && b.undo() && b.getEngine().getSong().rows.size() == 2;
        }
        std::printf ("Morceau enregistr\xC3\xA9 et relu, annulation : %s\n", songOk ? "OK" : "\xC3\x89" "CHEC");

        // Macros : une macro pilote deux réglages, chacun sur sa plage ; enregistrées et relues
        bool macroOk = false;
        {
            grv::GrooveboxProcessor mp;
            mp.prepareToPlay (sr, block);
            auto& e = mp.getEngine();
            e.setInstrument (0, "mono", 0.0);
            const auto ga = e.findParam (0, "inst", "cutoff"), gb = e.findParam (0, "mix", "volume");
            mp.setMacroRelative (2, false); // mode réel : les réglages prennent la position de la macro
            mp.toggleMacroTarget (2, ga);
            mp.toggleMacroTarget (2, gb);
            mp.setMacroRange (2, gb, 0.8f, 0.2f); // sens inverse
            mp.setGlobal ("macro3", 1.0f);
            juce::AudioBuffer<float> bm (2, block);
            juce::MidiBuffer mm;
            mp.processBlock (bm, mm);
            const bool moved = std::abs (e.getParam (ga) - 1.0f) < 1.0e-4f && std::abs (e.getParam (gb) - 0.2f) < 1.0e-4f;
            juce::MemoryOutputStream mos;
            mp.writeProjectFile (mos);
            grv::GrooveboxProcessor mq;
            juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
            const auto err = mq.readProjectFile (mis);
            const auto links = mq.getMacroLinks (2);
            macroOk = moved && err.isEmpty() && links.size() == 2 && links[1].global == gb && std::abs (links[1].lo - 0.8f) < 1.0e-3f && std::abs (links[1].hi - 0.2f) < 1.0e-3f
                   && ! mp.toggleMacroTarget (2, ga) && mp.getMacroLinks (2).size() == 1 && ! mq.isMacroRelative (2);
            // v0.46 : mode relatif — l'affectation ne change pas les réglages ; ils bougent avec la
            // macro en gardant leur écart, atteignent « De » et « À » aux bouts ; retouché à la main,
            // un réglage est réancré ; mode et ancrages enregistrés et relus
            grv::GrooveboxProcessor rp;
            rp.prepareToPlay (sr, block);
            auto& r = rp.getEngine();
            r.setInstrument (0, "mono", 0.0);
            const auto ra = r.findParam (0, "inst", "cutoff"), rb = r.findParam (0, "inst", "reso");
            r.setParam (ra, 0.3f); r.setParam (rb, 0.7f);
            rp.setGlobal ("macro4", 0.5f);
            rp.processBlock (bm, mm);
            rp.toggleMacroTarget (3, ra); rp.toggleMacroTarget (3, rb);
            rp.processBlock (bm, mm);
            const bool kept = rp.isMacroRelative (3) && std::abs (r.getParam (ra) - 0.3f) < 1.0e-4f && std::abs (r.getParam (rb) - 0.7f) < 1.0e-4f;
            rp.setGlobal ("macro4", 0.75f); rp.processBlock (bm, mm);
            const bool together = std::abs (r.getParam (ra) - 0.65f) < 1.0e-3f && std::abs (r.getParam (rb) - 0.85f) < 1.0e-3f;
            rp.setGlobal ("macro4", 0.0f); rp.processBlock (bm, mm);
            const bool ends = r.getParam (ra) < 1.0e-3f && r.getParam (rb) < 1.0e-3f;
            r.setParam (ra, 0.4f); // retouché à la main, macro à 0
            rp.setGlobal ("macro4", 0.5f); rp.processBlock (bm, mm);
            const bool reanchored = std::abs (r.getParam (ra) - 0.7f) < 1.0e-3f;
            juce::MemoryOutputStream ros;
            rp.writeProjectFile (ros);
            grv::GrooveboxProcessor rq;
            juce::MemoryInputStream ris (ros.getData(), ros.getDataSize(), false);
            const bool relSaved = rq.readProjectFile (ris).isEmpty() && rq.isMacroRelative (3) && rq.getMacroLinks (3).size() == 2
                && rq.getMacroLinks (3)[0].base >= 0.0f;
            std::printf ("  r\xC3\xA9" "el %d ; relatif : gard\xC3\xA9 %d, ensemble %d, bouts %d, r\xC3\xA9" "ancr\xC3\xA9 %d, enregistr\xC3\xA9 %d\n",
                         (int) macroOk, (int) kept, (int) together, (int) ends, (int) reanchored, (int) relSaved);
            macroOk = macroOk && kept && together && ends && reanchored && relSaved;
        }
        std::printf ("Macros \xC3\xA0 plusieurs r\xC3\xA9glages : %s\n", macroOk ? "OK" : "\xC3\x89" "CHEC");

        // v0.23 : déplacer une piste — instrument, séquence, réglages, side-chain qui la désigne
        // et nom d'origine suivent ; Ctrl+Z revient ; insérer place une piste libre
        bool moveOk = false;
        {
            grv::GrooveboxProcessor mv;
            auto& e = mv.getEngine();
            auto& sq = e.sequencer();
            e.setInstrument (5, "acid", 0.0);
            sq.setStep (5, 0, 3, 99);
            const auto gv = e.findParam (5, "mix", "volume");
            e.setParam (gv, e.describe (gv)->toNorm (-7.0f));
            e.setInsert (7, 0, "comp", 0.0);
            e.setTrackOutput (5, 2);
            const auto gs = e.findParam (7, "fx1", "scsrc");
            if (gs >= 0) e.setParam (gs, e.describe (gs)->toNorm (6.0f)); // « Piste 6 » = la piste 5 (0..15)
            const auto typeOf = [&] (int t) { return e.getTrackInfo (t).instrumentType; };
            const auto before1 = typeOf (1);
            const auto err = mv.moveTrack (5, 1);
            const auto gv2 = e.findParam (1, "mix", "volume");
            const auto gs2 = e.findParam (7, "fx1", "scsrc");
            const bool moved = err.isEmpty() && typeOf (1) == "acid" && typeOf (2) == before1 && sq.getStep (1, 0, 3) == 99 && sq.getStep (5, 0, 3) == 0
                && gv2 >= 0 && std::abs (e.describe (gv2)->toPlain (e.getParam (gv2)) + 7.0f) < 0.05f
                && gs2 >= 0 && std::lround (e.describe (gs2)->toPlain (e.getParam (gs2))) == 2
                && e.getTrackOutput (1) == 2 && e.getTrackOutput (5) == 0;
            const bool undone = mv.undo() && typeOf (5) == "acid" && typeOf (1) == before1 && sq.getStep (5, 0, 3) == 99;
            // insérer : la première piste libre (la plus loin après 3) vient en 3
            int freeBefore = 0;
            for (int t = 0; t < 16; ++t) freeBefore += typeOf (t).empty() ? 1 : 0;
            const auto t3 = typeOf (3);
            const auto ins = mv.insertTrack (3);
            const bool inserted = freeBefore == 0 ? ! ins.isEmpty() : (ins.isEmpty() && typeOf (3).empty() && typeOf (4) == t3);
            moveOk = moved && undone && inserted;
            std::printf ("  d\xC3\xA9plac\xC3\xA9" "e %d, annul\xC3\xA9" "e %d, insertion %d\n", (int) moved, (int) undone, (int) inserted);
        }
        std::printf ("Pistes d\xC3\xA9plac\xC3\xA9" "es et ins\xC3\xA9r\xC3\xA9" "es : %s\n", moveOk ? "OK" : "\xC3\x89" "CHEC");

        // v0.41 : copie de piste à piste (principe de l'Octatrack) — instrument, pas, réglages, effets
        // recopiés ; la source ne change pas ; Ctrl+Z revient ; rangs des pistes Pieuvre recalculés
        bool copyOk = false;
        {
            grv::GrooveboxProcessor cp;
            auto& e = cp.getEngine();
            auto& sq = e.sequencer();
            const auto typeOf = [&] (int t) { return e.getTrackInfo (t).instrumentType; };
            e.setInstrument (2, "acid", 0.0);
            sq.setStep (2, 0, 5, 77);
            const auto gv = e.findParam (2, "mix", "volume");
            e.setParam (gv, e.describe (gv)->toNorm (-7.0f));
            e.setInsert (2, 1, "comp", 0.0);
            const auto before9 = typeOf (9);
            sq.setStep (9, 0, 1, 50);
            const auto err = cp.copyTrack (2, 9);
            const auto gv9 = e.findParam (9, "mix", "volume");
            const bool copied = err.isEmpty() && typeOf (9) == "acid" && typeOf (2) == "acid" && sq.getStep (9, 0, 5) == 77 && sq.getStep (9, 0, 1) == 0
                && sq.getStep (2, 0, 5) == 77 && e.getInsertTypes (9)[1] == "comp" && e.getInsertTypes (2)[1] == "comp"
                && gv9 >= 0 && std::abs (e.describe (gv9)->toPlain (e.getParam (gv9)) + 7.0f) < 0.05f;
            const bool undone = cp.undo() && typeOf (9) == before9 && sq.getStep (9, 0, 5) == 0 && sq.getStep (9, 0, 1) == 50;
            // v0.42 : sélection d'un échantillon copiée dans un autre emplacement de la réserve
            // (T1 · 1 → T2 · 4), piste copiée avec ses échantillons, piste interne copiée sur une autre
            e.setInstrument (3, "octa", 0.0);
            juce::MemoryBlock wav;
            {
                juce::WavAudioFormat fmt;
                auto* mos = new juce::MemoryOutputStream (wav, false);
                std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (mos, 48000.0, 1, 16, {}, 0));
                juce::AudioBuffer<float> ramp (1, 4800);
                for (int i = 0; i < 4800; ++i) ramp.setSample (0, i, 0.8f * (float) i / 4800.0f);
                if (w != nullptr) w->writeFromAudioSampleBuffer (ramp, 0, 4800);
            }
            bool region = cp.loadSample (3, 0, "rampe.wav", wav).isEmpty() && cp.copySampleRegion (3, 0, 0.5, 0.75, 3, 19).isEmpty();
            const auto* ex = e.getSample (3, 19);
            region = region && ex != nullptr && std::abs ((int) ex->data.size() - 1200) <= 2 && std::abs (ex->data[0] - 0.4f) < 0.01f
                && ex->name.find ("(extrait)") != std::string::npos;
            const auto err4 = cp.copyTrack (3, 12);
            region = region && err4.isEmpty() && e.getSample (12, 0) != nullptr && e.getSample (12, 0)->data.size() == 4800 && e.getSample (12, 19) != nullptr;
            // T1 (emplacement 1, hauteur +5, pas 2, début de l'emplacement 20 %) copiée sur T4 : même pas, et
            // v0.43.2 : son PROPRE emplacement 1 (copie du son et de ses attributs), indépendant de T1
            const auto gt = e.findParam (3, "inst", "t1_tune");
            e.setParam (gt, e.describe (gt)->toNorm (5.0f));
            const auto ga1 = e.findParam (3, "inst", "e1st");
            e.setParam (ga1, e.describe (ga1)->toNorm (20.0f));
            sq.setStep (3, 0, 2, 99);
            const auto errL = cp.copyLane (3, 0, 3);
            const auto g4 = e.findParam (3, "inst", "t4_tune"), s4 = e.findParam (3, "inst", "t4_slot"), ga49 = e.findParam (3, "inst", "e49st");
            bool lane = errL.isEmpty() && g4 >= 0 && std::abs (e.describe (g4)->toPlain (e.getParam (g4)) - 5.0f) < 0.01f
                && s4 >= 0 && std::lround (e.describe (s4)->toPlain (e.getParam (s4))) == 1 && sq.getStep (3, 3, 2) == 99 && sq.getStep (3, 0, 2) == 99
                && e.getSample (3, 48) != nullptr && e.getSample (3, 48)->data.size() == 4800
                && ga49 >= 0 && std::abs (e.describe (ga49)->toPlain (e.getParam (ga49)) - 20.0f) < 0.05f;
            // vider l'emplacement de T4 ne vide pas celui de T1 (son à part entière)
            e.setSample (3, 48, nullptr, 0.0);
            lane = lane && e.getSample (3, 48) == nullptr && e.getSample (3, 0) != nullptr && e.getSample (3, 0)->data.size() == 4800;
            // v0.43 : T4 remise à l'origine (hauteur 0, son propre emplacement 1), pas gardés ; Ctrl+Z la rend
            // v0.46 : un verrou de pas de T4 et un verrou de scène sur T4 disparaissent aussi, et
            // l'attribut « début » de son emplacement 1 revient à 0
            e.setStepLockP (3, 0, 3, 5, g4, 0.9f, 0.0);
            e.setSceneLock (7, g4, 0.8f, 0.0);
            const auto errR = cp.resetLanes (3, 3);
            const auto* lk = e.getExtras().find (3, 0, 3, 5);
            bool sceneGone = true;
            for (const auto& l : e.getScenes().scenes[7]) sceneGone = sceneGone && l.slot != g4;
            const bool resetAll = (lk == nullptr || lk->locks.empty()) && sceneGone && std::abs (e.describe (ga49)->toPlain (e.getParam (ga49))) < 0.05f;
            std::printf ("  remise \xC3\xA0 l'origine compl\xC3\xA8te (verrous, sc\xC3\xA8ne, attributs) %d\n", (int) resetAll);
            const bool reset = resetAll && errR.isEmpty() && std::abs (e.describe (g4)->toPlain (e.getParam (g4))) < 0.01f
                && std::lround (e.describe (s4)->toPlain (e.getParam (s4))) == 1 && sq.getStep (3, 3, 2) == 99
                && std::abs (e.describe (gt)->toPlain (e.getParam (gt)) - 5.0f) < 0.01f
                && cp.undo() && std::abs (e.describe (g4)->toPlain (e.getParam (g4)) - 5.0f) < 0.01f;
            std::printf ("  piste interne remise \xC3\xA0 l'origine %d\n", (int) reset);
            const bool ranks = lane && reset, moveRanks = true;
            std::printf ("  piste interne copi\xC3\xA9" "e %d\n", (int) lane);
            copyOk = copied && undone && ranks && moveRanks && region;
            std::printf ("  s\xC3\xA9lection copi\xC3\xA9" "e et piste copi\xC3\xA9" "e avec ses \xC3\xA9" "chantillons %d\n", (int) region);
            std::printf ("  copi\xC3\xA9" "e %d, annul\xC3\xA9" "e %d\n", (int) copied, (int) undone);
        }
        std::printf ("Copie de piste \xC3\xA0 piste : %s\n", copyOk ? "OK" : "\xC3\x89" "CHEC");

        // v0.42 : projet de version 17 — deux pistes Pieuvre (4 et 8) regroupées dans la piste 4 (T1, T2)
        bool mergeOk = false;
        {
            grv::GrooveboxProcessor mp;
            auto& e = mp.getEngine();
            auto& sq = e.sequencer();
            e.setInstrument (4, "octa", 0.0);
            e.setInstrument (8, "octa", 0.0);
            juce::MemoryBlock wav;
            {
                juce::WavAudioFormat fmt;
                auto* mos = new juce::MemoryOutputStream (wav, false);
                std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (mos, 48000.0, 1, 16, {}, 0));
                juce::AudioBuffer<float> sine (1, 4800);
                for (int i = 0; i < 4800; ++i) sine.setSample (0, i, 0.5f * std::sin (0.05f * (float) i));
                if (w != nullptr) w->writeFromAudioSampleBuffer (sine, 0, 4800);
            }
            mp.loadSample (8, 2, "sinus.wav", wav);
            sq.setStep (8, 0, 6, 88); sq.setNote (8, 0, 6, 64);
            e.setInsert (8, 0, "chorus", 0.0);
            const auto gt = e.findParam (8, "inst", "t1_tune");
            e.setParam (gt, e.describe (gt)->toNorm (-3.0f));
            juce::MemoryOutputStream mos;
            mp.writeProjectFile (mos);
            // écrit comme avant la v0.42 : identifiants sans « t1_ », une piste du mixeur par Pieuvre
            auto xml = mos.toString().replace ("version=\"19\"", "version=\"17\"").replace ("i=\"t1_", "i=\"");
            grv::GrooveboxProcessor mq;
            juce::MemoryInputStream mis (xml.toRawUTF8(), xml.getNumBytesAsUTF8(), false);
            const auto err = mq.readProjectFile (mis);
            auto& f = mq.getEngine();
            const auto g2 = f.findParam (4, "inst", "t2_tune");
            mergeOk = err.isEmpty() && f.getTrackInfo (4).instrumentType == "octa" && f.getTrackInfo (8).instrumentType.empty()
                && f.getSample (4, 16 + 2) != nullptr && f.sequencer().getStep (4, 1, 6) == 88 && f.sequencer().getNote (4, 1, 6) == 64
                && g2 >= 0 && std::abs (f.describe (g2)->toPlain (f.getParam (g2)) + 3.0f) < 0.01f
                && f.getPadInsertType (4, 1, 0) == "chorus" && f.getInsertTypes (4)[0] == "eq" && f.getInsertTypes (4)[1] == "comp";
            std::printf ("  T2 : \xC3\xA9" "chantillon %d, pas %d, note %d, hauteur %d, effet %s ; sortie %s + %s\n", (int) (f.getSample (4, 18) != nullptr),
                         f.sequencer().getStep (4, 1, 6), f.sequencer().getNote (4, 1, 6), g2 >= 0 ? (int) std::lround (f.describe (g2)->toPlain (f.getParam (g2))) : 99,
                         f.getPadInsertType (4, 1, 0).c_str(), f.getInsertTypes (4)[0].c_str(), f.getInsertTypes (4)[1].c_str());
        }
        std::printf ("Pistes Pieuvre des anciens projets regroup\xC3\xA9" "es : %s\n", mergeOk ? "OK" : "\xC3\x89" "CHEC");

        // Bus de groupe (v0.24) : sortie des pistes, inserts et volume du groupe enregistrés, annulables
        bool groupOk = false;
        {
            grv::GrooveboxProcessor gp;
            auto& e = gp.getEngine();
            e.setInstrument (2, "acid", 0.0);
            e.setTrackOutput (2, 3);
            e.setInsert (grv::engine::groupBase + 2, 1, "comp", 0.0);
            const auto gv = e.findParam (grv::engine::groupBase + 2, "mix", "volume");
            e.setParam (gv, e.describe (gv)->toNorm (-9.0f));
            juce::MemoryBlock mb;
            gp.getStateInformation (mb);
            grv::GrooveboxProcessor gq;
            gq.setStateInformation (mb.getData(), (int) mb.getSize());
            auto& f = gq.getEngine();
            const auto fv = f.findParam (grv::engine::groupBase + 2, "mix", "volume");
            const bool saved = f.getTrackOutput (2) == 3 && f.getInsertTypes (grv::engine::groupBase + 2)[1] == "comp"
                && fv >= 0 && std::abs (f.describe (fv)->toPlain (f.getParam (fv)) + 9.0f) < 0.05f;
            // un ancien projet (sans « out ») revient sur le bus principal
            f.setTrackOutput (2, 0);
            gq.setStateInformation (mb.getData(), (int) mb.getSize());
            const bool reloaded = f.getTrackOutput (2) == 3;
            groupOk = saved && reloaded;
            std::printf ("  enregistr\xC3\xA9 %d, recharg\xC3\xA9 %d\n", (int) saved, (int) reloaded);
        }
        std::printf ("Bus de groupe : %s\n", groupOk ? "OK" : "\xC3\x89" "CHEC");

        // Break temporisé : 2 temps à 120 BPM = 1 s ; actif tout de suite, relâché seul
        juce::AudioBuffer<float> b2 (2, block);
        juce::MidiBuffer m2;
        proc.timedBreak (2.0);
        proc.processBlock (b2, m2);
        const bool on = proc.getBreak();
        for (int i = 0; i < (int) (1.5 * sr) / block; ++i) { b2.clear(); proc.processBlock (b2, m2); }
        const bool off = ! proc.getBreak();
        if (! (on && off)) std::printf ("  (break pos\xC3\xA9 %d, rel\xC3\xA2" "ch\xC3\xA9 %d)\n", (int) on, (int) off);
        std::printf ("Break temporis\xC3\xA9 : %s\n", on && off ? "OK" : "\xC3\x89" "CHEC");
        return ok && on && off && histOk && arpOk && midiMuteOk && midiConflictOk && divOk && longOk && octaDstOk && machineOk && orbitOk && vocOk && atmosOk && padFxOk && songOk && macroOk && moveOk && copyOk && mergeOk && groupOk ? 0 : 4;
    }

    if (argc > 3 && juce::String (argv[3]) == "--state")
    {
        std::printf ("%d octets d'\xC3\xA9tat\n%s\n", (int) saved.getSize(), juce::String::fromUTF8 ((const char*) saved.getData() + 8, std::min (3000, (int) saved.getSize() - 8)).toRawUTF8());
        return 0;
    }

    const auto total = (int) (seconds * sr);
    juce::AudioBuffer<float> all (2, total);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    bool finite = true;
    float peak = 0.0f;

    for (int pos = 0; pos < total; pos += block)
    {
        const auto n = std::min (block, total - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, n);
        view.clear();
        proc.processBlock (view, midi);
        for (int c = 0; c < 2; ++c)
        {
            all.copyFrom (c, pos, view, c, 0, n);
            for (int i = 0; i < n; ++i)
            {
                const auto v = view.getSample (c, i);
                finite = finite && std::isfinite (v);
                peak = std::max (peak, std::abs (v));
            }
        }
    }

    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (out);
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (writer == nullptr) { std::printf ("Impossible d'\xC3\xA9" "crire %s\n", out.getFullPathName().toRawUTF8()); return 2; }
    writer->writeFromAudioSampleBuffer (all, 0, total);
    writer.reset();

    std::printf ("Rendu : %s\nCr\xC3\xAAte : %.3f (%.1f dBFS)\nValeurs valides : %s\n",
                 out.getFullPathName().toRawUTF8(), peak, juce::Decibels::gainToDecibels (peak), finite ? "oui" : "NON");
    return finite && peak > 0.05f && peak < 2.0f ? 0 : 1;
}
