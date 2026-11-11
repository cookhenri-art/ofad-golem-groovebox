/*
    Groovebox — OFAD — AGPL-3.0-or-later
    v0.46.1 : tests de sécurité (audit du 4 novembre 2026).
      - F1 / F2 : un projet reçu d'un tiers ne fait ni lire ni recopier un fichier hors des dossiers
        du projet et de GOLEM (chemins réseau « \\serveur », remontées « .. », fichiers non audio) ;
      - F3 : balises ou JSON imbriqués sans fin refusés avant la lecture (pas de plantage) ;
      - F9 : projets abîmés au hasard (octets changés, fichier coupé) relus sans plantage.
    Lancé par ctest (groovebox_SecurityTests) ; aucun accès réseau.
*/

#include "PluginProcessor.h"
#include "Montage.h"
#include "Recorder.h"
#include "ParseGuard.h"

#include <cmath>
#include <cstdio>
#include <string>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "\xC3\x89" "CHEC", what);
    if (! ok) ++failures;
}

static std::string nested (const char* open, const char* close, int n)
{
    std::string s;
    for (int i = 0; i < n; ++i) s += open;
    for (int i = 0; i < n; ++i) s += close;
    return s;
}

static juce::String readProject (grv::GrooveboxProcessor& proc, const std::string& text)
{
    juce::MemoryInputStream in (text.data(), text.size(), false);
    return proc.readProjectFile (in);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    // --- F3 : garde-fous de profondeur -----------------------------------------------------
    {
        const std::string ok = "<?xml version=\"1.0\"?><!-- <a><a><a> --><A x=\"/>\" y='<b>'><B><C/><C/></B><![CDATA[<z><z>]]></A>";
        check (grv::xmlNestingWithin (ok.data(), ok.size()), "XML normal accept\xC3\xA9 (commentaires, CDATA, guillemets)");
        const auto deep = nested ("<a>", "</a>", 100000);
        check (! grv::xmlNestingWithin (deep.data(), deep.size()), "XML imbriqu\xC3\xA9 100 000 fois refus\xC3\xA9");
        const auto flat = nested ("<a/>", "", 100000);
        check (grv::xmlNestingWithin (flat.data(), flat.size()), "100 000 balises vides c\xC3\xB4te \xC3\xA0 c\xC3\xB4te accept\xC3\xA9" "es");
        const auto at64 = nested ("<a>", "</a>", grv::maxXmlDepth), at65 = nested ("<a>", "</a>", grv::maxXmlDepth + 1);
        check (grv::xmlNestingWithin (at64.data(), at64.size()) && ! grv::xmlNestingWithin (at65.data(), at65.size()), "limite exacte : 64 niveaux oui, 65 non");
        const auto jdeep = nested ("[", "]", 200000);
        check (! grv::jsonNestingWithin (jdeep.data(), jdeep.size()), "JSON imbriqu\xC3\xA9 200 000 fois refus\xC3\xA9");
        const std::string jok = "{\"name\":\"[[[[[[[[[[\\\"{{{{{{{\",\"values\":{\"a\":1,\"b\":[1,2,3]}}";
        check (grv::jsonNestingWithin (jok.data(), jok.size()), "JSON normal accept\xC3\xA9 (crochets dans les cha\xC3\xAEnes ignor\xC3\xA9s)");
    }

    grv::GrooveboxProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);

    {
        const auto deep = "<GROOVEBOX>" + nested ("<PROJECT>", "</PROJECT>", 300000) + "</GROOVEBOX>";
        const auto err = readProject (proc, deep);
        check (err.isNotEmpty(), "projet aux balises imbriqu\xC3\xA9" "es sans fin : refus\xC3\xA9 sans plantage");
        juce::MemoryBlock state;
        state.append ("VC2!", 4);
        const auto size = (juce::uint32) deep.size() + 1;
        state.append (&size, 4);
        state.append (deep.data(), deep.size() + 1);
        proc.setStateInformation (state.getData(), (int) state.getSize());
        check (true, "\xC3\xA9tat de s\xC3\xA9" "ance pi\xC3\xA9g\xC3\xA9 (logiciel h\xC3\xB4te) : ignor\xC3\xA9 sans plantage");
    }

    // --- F1 / F2 : prises du montage -------------------------------------------------------
    const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("golem-securite-" + juce::String (juce::Time::currentTimeMillis()));
    work.createDirectory();
    const auto projectFolder = work.getChildFile ("Projets");
    projectFolder.createDirectory();
    const auto inside = projectFolder.getChildFile ("Morceau - audio").getChildFile ("Prise 1").getChildFile ("P01.flac");
    inside.getParentDirectory().createDirectory();
    inside.replaceWithText ("x");
    const auto secret = work.getChildFile ("secret.flac");
    secret.replaceWithText ("donnees privees");
    {
        using grv::montage::takeFileAllowed;
        check (takeFileAllowed (inside, projectFolder), "prise dans le dossier du projet : accept\xC3\xA9" "e");
        check (takeFileAllowed (grv::Recorder::rootFolder().getChildFile ("Prise").getChildFile ("P01.flac"), {}), "prise dans les enregistrements de GOLEM : accept\xC3\xA9" "e");
       #if JUCE_WINDOWS
        check (! takeFileAllowed (juce::File ("\\\\192.0.2.1\\partage\\P01.flac"), projectFolder), "chemin r\xC3\xA9seau \\\\serveur\\partage : refus\xC3\xA9");
       #endif
        check (! takeFileAllowed (projectFolder.getChildFile ("../secret.flac"), projectFolder), "remont\xC3\xA9" "e \xC2\xAB .. \xC2\xBB hors du projet : refus\xC3\xA9" "e");
        check (! takeFileAllowed (secret, projectFolder), "fichier hors des dossiers autoris\xC3\xA9s : refus\xC3\xA9");
        check (! takeFileAllowed (projectFolder.getChildFile ("notes.txt"), projectFolder), "fichier non audio : refus\xC3\xA9");
    }
    {
        // projet piégé : une prise désignée par un chemin absolu, une autre par « .. », une réseau
        juce::ValueTree m ("MONTAGE");
        auto take = [&m] (const juce::String& id, const juce::String& file, bool rel)
        {
            juce::ValueTree t ("TAKE");
            t.setProperty ("id", id, nullptr); t.setProperty ("file", file, nullptr);
            if (rel) t.setProperty ("rel", true, nullptr);
            t.setProperty ("len", 48000, nullptr); t.setProperty ("rate", 48000.0, nullptr);
            m.appendChild (t, nullptr);
            juce::ValueTree c ("CLIP");
            c.setProperty ("take", id, nullptr); c.setProperty ("len", 48000, nullptr);
            m.appendChild (c, nullptr);
        };
        take ("abs", secret.getFullPathName(), false);
        take ("up", "../secret.flac", true);
        take ("unc", "\\\\192.0.2.1\\partage\\P01.flac", false);
        take ("unc2", "//192.0.2.1/partage/P01.flac", false);
        take ("ok", "Morceau - audio/Prise 1/P01.flac", true);
        const auto projectFile = projectFolder.getChildFile ("Morceau.gbx");
        const auto doc = grv::montage::fromTree (m, projectFile);
        check (doc.takes.size() == 1 && doc.takes[0].id == "ok", "projet pi\xC3\xA9g\xC3\xA9 : seule la prise l\xC3\xA9gitime est gard\xC3\xA9" "e");

        // F2 : à l'enregistrement, rien n'est recopié depuis l'extérieur
        auto docAll = doc;
        grv::MontageTake bad;
        bad.id = "abs"; bad.file = secret; bad.length = 48000; bad.rate = 48000.0;
        docAll.takes.push_back (bad);
        grv::montage::bindToProject (docAll, projectFolder.getChildFile ("Copie.gbx"));
        check (! projectFolder.getChildFile ("Copie - audio").getChildFile (work.getFileName()).getChildFile ("secret.flac").exists(),
               "enregistrement : un fichier ext\xC3\xA9rieur n'est jamais recopi\xC3\xA9 \xC3\xA0 c\xC3\xB4t\xC3\xA9 du projet");
    }

    // --- contre-vérification (4 nov. 2026, soir) : contournements essayés ---------------------
    {
        using grv::montage::takeFileAllowed;
        // chemin absolu qui commence dans les enregistrements de GOLEM puis remonte
        const auto viaUp = juce::File (grv::Recorder::rootFolder().getFullPathName() + "/../../../../../../etc/x.wav");
        check (! takeFileAllowed (viaUp, {}), "[contre] chemin absolu enregistrements/../../.. : refus\xC3\xA9");
        // dossier voisin dont le nom commence pareil (« EnregistrementsX ») : pas un enfant
        const auto twin = juce::File (grv::Recorder::rootFolder().getFullPathName() + "X").getChildFile ("x.wav");
        check (! takeFileAllowed (twin, {}), "[contre] dossier voisin au nom proche : refus\xC3\xA9");
        // extension en majuscules (une vraie prise copiée d'un autre système)
        check (takeFileAllowed (projectFolder.getChildFile ("Morceau - audio").getChildFile ("P02.WAV"), projectFolder), "[contre] .WAV en majuscules : accept\xC3\xA9");
       #if ! JUCE_WINDOWS
        // lien symbolique rangé dans le dossier du projet et nommé .wav, qui pointe vers un fichier privé
        const auto link = projectFolder.getChildFile ("Morceau - audio").getChildFile ("lien.wav");
        secret.createSymbolicLink (link, true);
        check (! takeFileAllowed (link, projectFolder), "[contre] lien symbolique .wav vers un fichier priv\xC3\xA9 : refus\xC3\xA9");
        link.deleteFile();
       #endif
    }
    {
        // projet en UTF-16 (le texte est décodé selon son en-tête) aux balises imbriquées sans fin
        juce::MemoryOutputStream u16;
        u16.writeByte ((char) 0xFF); u16.writeByte ((char) 0xFE);
        const auto body = juce::String ("<GrooveboxState>") + juce::String (nested ("<a>", "</a>", 200000)) + "</GrooveboxState>";
        for (auto p = body.getCharPointer(); ! p.isEmpty(); ++p) { const auto c = (juce::uint16) *p; u16.writeShort ((short) c); }
        juce::MemoryInputStream in (u16.getData(), u16.getDataSize(), false);
        check (proc.readProjectFile (in).isNotEmpty(), "[contre] projet UTF-16 imbriqu\xC3\xA9 sans fin : refus\xC3\xA9 sans plantage");
        // entités du DTD (« milliard de rires ») : pas d'explosion de mémoire ni de plantage
        std::string bomb = "<?xml version=\"1.0\"?><!DOCTYPE GrooveboxState [ <!ENTITY l0 \"hahahahahahahahahahaha\">";
        for (int i = 1; i <= 9; ++i) { bomb += " <!ENTITY l" + std::to_string (i) + " \""; for (int k = 0; k < 10; ++k) bomb += "&l" + std::to_string (i - 1) + ";"; bomb += "\">"; }
        bomb += " ]><GrooveboxState><PROJECT n=\"&l9;\">&l9;</PROJECT></GrooveboxState>";
        readProject (proc, bomb);
        check (true, "[contre] entit\xC3\xA9s DTD en cascade : lu sans explosion");
        // balise dont l'attribut contient « > » et « /> » (le compteur ne doit pas se tromper)
        std::string tricky = "<GrooveboxState>";
        for (int i = 0; i < 100000; ++i) tricky += "<a x=\"/>\" y='>'>";
        check (readProject (proc, tricky).isNotEmpty(), "[contre] attributs contenant > et /> : imbrication d\xC3\xA9tect\xC3\xA9" "e, pas de plantage");
    }

    // --- F9 : projets abîmés au hasard ---------------------------------------------------------
    {
        juce::MemoryOutputStream out;
        check (proc.writeProjectFile (out), "projet de d\xC3\xA9part \xC3\xA9" "crit");
        const std::string base (static_cast<const char*> (out.getData()), out.getDataSize());
        juce::Random rnd (20261104);
        int runs = 0;
        for (int i = 0; i < 150; ++i)
        {
            auto t = base;
            const int edits = 1 + rnd.nextInt (40);
            for (int k = 0; k < edits && ! t.empty(); ++k)
            {
                const auto at = (size_t) rnd.nextInt ((int) t.size());
                switch (rnd.nextInt (4))
                {
                    case 0: t[at] = (char) rnd.nextInt (256); break;
                    case 1: t.erase (at, (size_t) rnd.nextInt (64)); break;
                    case 2: t.insert (at, rnd.nextBool() ? "<a>" : "\"-99999999999\""); break;
                    default: t.resize (at); break;
                }
            }
            readProject (proc, t);
            ++runs;
        }
        char what[96];
        std::snprintf (what, sizeof what, "%d projets ab\xC3\xAEm\xC3\xA9s relus sans plantage", runs);
        check (runs == 150, what);
        check (readProject (proc, base).isEmpty(), "projet intact relu ensuite normalement");
    }

    // --- v0.47.0 : MP3 accepté à l'import : un vrai MP3 se charge, des MP3 abîmés sont refusés sans plantage
    {
        // un vrai MP3 (1 s de sinus) fait par l'encodeur du projet
        std::vector<float> l (44100), r (44100);
        for (size_t k = 0; k < l.size(); ++k) l[k] = r[k] = 0.4f * (float) std::sin (2.0 * 3.14159265 * 440.0 * (double) k / 44100.0);
        juce::MemoryBlock mp3;
        bool encoded = false;
        {
            const auto wav = work.getChildFile ("essai.wav"), mp3f = work.getChildFile ("essai.mp3");
            work.createDirectory();
            {
                juce::WavAudioFormat fmt;
                std::unique_ptr<juce::OutputStream> os (wav.createOutputStream());
                std::unique_ptr<juce::AudioFormatWriter> w (os != nullptr ? fmt.createWriterFor (os.get(), 44100.0, 2, 16, {}, 0) : nullptr);
                if (w != nullptr)
                {
                    os.release();
                    const float* ch[] { l.data(), r.data() };
                    w->writeFromFloatArrays (ch, 2, (int) l.size());
                }
            }
            std::atomic<bool> abort { false };
            juce::String e;
            encoded = grv::Recorder::encodeMp3 (wav, mp3f, abort, [] (float) {}, e) && mp3f.loadFileAsData (mp3);
        }
        check (encoded && mp3.getSize() > 1000, "MP3 fait par l'encodeur");
        check (encoded && proc.loadSample (0, 0, "essai.mp3", mp3).isEmpty(), "MP3 charg\xC3\xA9 dans un pad");
        // MP3 abîmés : en-têtes de trames suivis de hasard, morceaux coupés, étiquette ID3 qui ment
        juce::Random rng (0x4D5033);
        int runs = 0;
        for (int k = 0; k < 120; ++k)
        {
            juce::MemoryBlock bad;
            if (k % 3 == 0 && encoded) { bad = mp3; for (int j = 0; j < 40; ++j) ((char*) bad.getData())[rng.nextInt ((int) bad.getSize())] = (char) rng.nextInt (256); }
            else if (k % 3 == 1 && encoded) bad.append (mp3.getData(), (size_t) rng.nextInt ((int) mp3.getSize()));
            else
            {
                const unsigned char id3[] { 'I', 'D', '3', 3, 0, 0, 0x7F, 0x7F, 0x7F, 0x7F };
                if (k % 2) bad.append (id3, sizeof id3);
                for (int f = 0; f < 200; ++f)
                {
                    const unsigned char hdr[] { 0xFF, 0xFB, (unsigned char) rng.nextInt (256), (unsigned char) rng.nextInt (256) };
                    bad.append (hdr, sizeof hdr);
                    for (int j = 0; j < 100 + rng.nextInt (400); ++j) { const auto b = (char) rng.nextInt (256); bad.append (&b, 1); }
                }
            }
            proc.loadSample (1, 0, "abime.mp3", bad);
            ++runs;
        }
        check (runs == 120, "120 MP3 ab\xC3\xAEm\xC3\xA9s lus sans plantage");
    }

    work.deleteRecursively();
    proc.releaseResources();
    // --- 1.1 (lot 3) : écran AUDIO — prise traitée après ou pendant, longueur et calage --------
    for (const bool during : { false, true })
    {
        proc.getEngine().setInstrument (0, "acid", 0.0);
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("master", true); o->setProperty ("masterTreated", true);
        o->setProperty ("tracks", 1); o->setProperty ("sum", false); o->setProperty ("returns", 0);
        o->setProperty ("during", during); o->setProperty ("dest", 1); o->setProperty ("format", 0); o->setProperty ("follow", 0);
        juce::Array<juce::var> chain;
        { juce::DynamicObject::Ptr c = new juce::DynamicObject(); c->setProperty ("type", "rdenoise"); juce::Array<juce::var> v; v.add (0.0); c->setProperty ("values", v); c->setProperty ("bypass", false); chain.add (juce::var (c.get())); }
        o->setProperty ("chain", chain);
        const auto err = proc.audioRecStartEx (juce::var (o.get()));
        check (err.isEmpty(), during ? "\xC3\xA9" "cran AUDIO : prise trait\xC3\xA9" "e pendant, d\xC3\xA9marr\xC3\xA9" "e" : "\xC3\xA9" "cran AUDIO : prise trait\xC3\xA9" "e apr\xC3\xA8s, d\xC3\xA9marr\xC3\xA9" "e");
        juce::AudioBuffer<float> buf (2, 256);
        for (int b = 0; b < 400; ++b)
        {
            juce::MidiBuffer midi;
            if (b % 40 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 48 + (b / 40) % 12, (juce::uint8) 110), 0);
            if (b % 40 == 30) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            buf.clear();
            proc.processBlock (buf, midi);
            // au rythme réel (256 échantillons ≈ 5,8 ms) : plus vite, la file d'écriture (≈ 2 s) déborde
            // quand la machine est chargée et le fichier traité perd des échantillons (« dropped »)
            juce::Thread::sleep (5);
        }
        proc.audioRecStop();
        for (int k = 0; k < 600 && (proc.audioRecStatus().contains ("\"processing\":true") || proc.audioRecStatus().contains ("\"encoding\":true")); ++k) juce::Thread::sleep (50);
        const auto st = juce::JSON::parse (proc.audioRecStatus());
        if ((double) st.getProperty ("dropped", 0.0) > 0.0) std::printf ("  perdu par la file d'\xC3\xA9" "criture : %.3f s\n", (double) st.getProperty ("dropped", 0.0));
        const auto treated = st.getProperty ("treated", juce::var());
        check (treated.isArray() && treated.size() == 2, "\xC3\xA9" "cran AUDIO : 2 fichiers trait\xC3\xA9" "s (master trait\xC3\xA9, piste 1) dans \xC2\xAB Son trait\xC3\xA9 \xC2\xBB");
        // longueur et calage : le master traité (réduction 0 dB : identité, latence retirée) = le master brut
        juce::File raw, done;
        for (const auto& f : grv::Recorder::rootFolder().findChildFiles (juce::File::findFiles, true, "Master*.wav"))
            if (raw == juce::File() || f.getLastModificationTime() > raw.getLastModificationTime()) raw = f;
        for (const auto& f : grv::GrooveboxProcessor::processedFolder().findChildFiles (juce::File::findFiles, true, "*Master*.wav"))
            if (done == juce::File() || f.getLastModificationTime() > done.getLastModificationTime()) done = f;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> ra (raw.existsAsFile() ? wav.createReaderFor (raw.createInputStream().release(), true) : nullptr);
        std::unique_ptr<juce::AudioFormatReader> rb (done.existsAsFile() ? wav.createReaderFor (done.createInputStream().release(), true) : nullptr);
        bool sameLen = false, aligned = false; double energy = 0.0;
        if (ra != nullptr && rb != nullptr)
        {
            // prise libre traitée après : la queue des effets est gardée (le fichier peut s'allonger)
            sameLen = during ? ra->lengthInSamples == rb->lengthInSamples : rb->lengthInSamples >= ra->lengthInSamples;
            const int n = (int) std::min (ra->lengthInSamples, rb->lengthInSamples);
            juce::AudioBuffer<float> a (2, n), b2 (2, n);
            ra->read (&a, 0, n, 0, true, true); rb->read (&b2, 0, n, 0, true, true);
            double d = 0.0;
            for (int i = 0; i < n; ++i) { const double x = a.getSample (0, i); energy += x * x; const double y = b2.getSample (0, i) - x; d += y * y; }
            aligned = energy > 1.0e-6 && d < 0.01 * energy;
            std::printf ("  prise %s : %d \xC3\xA9" "chantillons, \xC3\xA9" "nergie %.3f, \xC3\xA9" "cart %.2e\n", during ? "pendant" : "apr\xC3\xA8s", n, energy, d);
        }
        check (sameLen, "\xC3\xA9" "cran AUDIO : longueur du fichier trait\xC3\xA9 (pile la prise ; apr\xC3\xA8s une prise libre : avec sa queue)");
        check (aligned, "\xC3\xA9" "cran AUDIO : le fichier trait\xC3\xA9 est recal\xC3\xA9 (latence retir\xC3\xA9" "e)");
    }

    // --- 1.1 : écoute de l'écran AUDIO ---------------------------------------------------------
    {
        // un bouton tourné ne reconstruit pas la chaîne (même suite d'effets) : pas de remise à zéro
        auto chainOf = [] (double cutoff)
        {
            juce::Array<juce::var> chain;
            juce::DynamicObject::Ptr c = new juce::DynamicObject(); c->setProperty ("type", "tvintage");
            juce::Array<juce::var> v; c->setProperty ("values", v); c->setProperty ("bypass", false); chain.add (juce::var (c.get()));
            juce::DynamicObject::Ptr f = new juce::DynamicObject(); f->setProperty ("type", "filter");
            juce::Array<juce::var> fv; juce::ignoreUnused (cutoff); f->setProperty ("values", fv); f->setProperty ("bypass", false); chain.add (juce::var (f.get()));
            return juce::var (chain);
        };
        auto run = [&] (bool stage)
        {
            proc.getEngine().setInstrument (0, "acid", 0.0);
            proc.setMonitorChain (juce::var (juce::Array<juce::var>()));
            proc.setMonitorChain (chainOf (2000.0));
            juce::AudioBuffer<float> buf (2, 256);
            double tailEnergy = 0.0;
            for (int b = 0; b < 120; ++b)
            {
                juce::MidiBuffer midi;
                if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0);
                if (b == 20) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                if (stage && b == 20) proc.setMonitorChain (chainOf (2000.0)); // même réglage, renvoyé comme un bouton tourné
                buf.clear(); proc.processBlock (buf, midi);
                if (b >= 20 && b < 60) for (int i = 0; i < 256; ++i) tailEnergy += (double) buf.getSample (0, i) * buf.getSample (0, i);
            }
            proc.setMonitorChain (juce::var (juce::Array<juce::var>()));
            return tailEnergy;
        };
        const auto a1 = run (false), a2 = run (true);
        check (a1 > 1.0e-6 && std::abs (a1 - a2) < 0.02 * a1, "\xC3\xA9" "cran AUDIO : un r\xC3\xA9glage tourn\xC3\xA9 pendant l'\xC3\xA9" "coute ne remet pas la cha\xC3\xAE" "ne \xC3\xA0 z\xC3\xA9ro (pas de craquement)");
        std::printf ("  queue de r\xC3\xA9verb\xC3\xA9ration : %.4f sans, %.4f avec r\xC3\xA9glage renvoy\xC3\xA9\n", a1, a2);

        // écoute limitée aux sources de la prise : une piste vide cochée → silence ; arrêt → le mixage revient
        auto energyWith = [&] (bool on)
        {
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            juce::Array<juce::var> tr; tr.add (7); o->setProperty ("tracks", tr); o->setProperty ("on", on);
            proc.setMonitorSources (juce::var (o.get()));
            juce::AudioBuffer<float> buf (2, 256);
            double e = 0.0;
            for (int b = 0; b < 40; ++b)
            {
                juce::MidiBuffer midi;
                if (b % 10 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0);
                buf.clear(); proc.processBlock (buf, midi);
                if (b >= 4) for (int i = 0; i < 256; ++i) e += (double) buf.getSample (0, i) * buf.getSample (0, i);
            }
            return e;
        };
        const auto eOn = energyWith (true), eOff = energyWith (false);
        check (eOff > 1.0e-4 && eOn < 1.0e-6 * std::max (1.0, eOff), "\xC3\xA9" "cran AUDIO : \xC3\xA0 l'\xC3\xA9" "coute, on n'entend que les pistes coch\xC3\xA9" "es (piste vide : silence)");
        std::printf ("  \xC3\xA9" "coute piste 8 seule : %.2e ; mixage : %.2e\n", eOn, eOff);
    }

    std::printf ("%s\n", failures == 0 ? "Tous les tests de s\xC3\xA9" "curit\xC3\xA9 passent." : "Des tests de s\xC3\xA9" "curit\xC3\xA9 \xC3\xA9" "chouent.");
    return failures == 0 ? 0 : 1;
}
