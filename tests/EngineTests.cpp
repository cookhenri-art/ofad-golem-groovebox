/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Tests du moteur : routage, réglages, scènes, verrous par pas, modulations, échantillons.
*/

#include "engine/Engine.h"
#include "engine/Catalog.h"
#include "instruments/DrumKit.h"
#include "effects/Distortion.h"
#include "effects/Gate.h"
#include "synth/SynthCore.h"
#include "engine/Arpeggiator.h"
#include "synth/ModHub.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace grv::engine;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { std::printf ("ECHEC %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

struct Render
{
    float peak = 0.0f;
    double rms = 0.0;
    bool finite = true;
};

static Render run (Engine& e, int blocks, Engine::BlockInfo bi = {}, int blockSize = 256)
{
    std::vector<float> L ((size_t) blockSize), R ((size_t) blockSize);
    Render r;
    double sum = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        e.process (nullptr, nullptr, L.data(), R.data(), blockSize, bi, nullptr, 0);
        for (int i = 0; i < blockSize; ++i)
        {
            r.finite = r.finite && std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]);
            r.peak = std::max (r.peak, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
            sum += (double) L[(size_t) i] * L[(size_t) i];
        }
    }
    r.rms = std::sqrt (sum / (double) (blocks * blockSize));
    return r;
}

static void testSilenceAndDrums()
{
    Engine e;
    e.prepare (48000.0, 256);
    auto r = run (e, 50);
    CHECK (r.finite && r.peak == 0.0f);

    CHECK (e.setInstrument (0, "drumA", 0.0));
    CHECK (! e.setInstrument (0, "inconnu", 0.0));
    auto& s = e.sequencer();
    for (int i = 0; i < 16; i += 4) s.setStep (0, 0, i, 120);
    for (int i = 2; i < 16; i += 4) s.setStep (0, 4, i, 90);
    s.requestPlay();
    r = run (e, 400);
    CHECK (r.finite);
    CHECK (r.peak > 0.05f && r.peak < 4.0f);
    CHECK (e.getHitCount (0, 0) >= 4);
    CHECK (e.getHitCount (0, 4) >= 3);
    std::printf ("  batterie : cr\xC3\xAAte %.3f, rms %.4f\n", r.peak, r.rms);
}

static void testParamsAndMixer()
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (1, "drum80", 0.0);
    const auto vol = e.findParam (1, "mix", "volume");
    CHECK (vol == globalSlot (1, mixVolume));
    const auto* d = e.describe (vol);
    CHECK (d != nullptr && d->unit == "dB");
    CHECK (std::abs (d->toPlain (e.getParam (vol)) - 0.0f) < 0.01f);

    const auto decay = e.findParam (1, "inst", "p01_decay");
    CHECK (decay == globalSlot (1, instrumentBase + 1));
    CHECK (e.findParam (1, "inst", "nexistepas") == -1);

    e.sequencer().setStep (1, 0, 0, 127);
    e.sequencer().requestPlay();
    const auto loud = run (e, 40);
    e.setParam (vol, 0.0f); // -60 dB = silence
    run (e, 2);
    e.sequencer().requestStop(); run (e, 2);
    e.sequencer().requestPlay();
    const auto quiet = run (e, 40);
    CHECK (loud.peak > 0.05f);
    CHECK (quiet.peak < 0.001f);

    // valeurs invalides ignorées
    e.setParam (-5, 0.5f);
    e.setParam (numSlots + 3, 0.5f);
    e.setParam (vol, std::nanf (""));
    CHECK (e.getParam (vol) == 0.0f);
}

static void testScenes()
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    const auto decay = e.findParam (0, "inst", "p01_decay");
    const auto base = e.getParam (decay);
    e.setSceneLock (0, decay, 0.0f, 0.0);
    e.setSceneLock (8, decay, 1.0f, 0.0);
    Engine::BlockInfo bi;
    bi.sceneA = 0; bi.sceneB = 8;
    bi.fader = 0.0f; run (e, 200, bi);
    CHECK (std::abs (e.getEffective (decay) - 0.0f) < 0.02f);
    bi.fader = 1.0f; run (e, 200, bi);
    CHECK (std::abs (e.getEffective (decay) - 1.0f) < 0.02f);
    bi.fader = 0.5f; run (e, 200, bi);
    CHECK (std::abs (e.getEffective (decay) - 0.5f) < 0.03f);
    CHECK (e.getParam (decay) == base); // la valeur de base ne bouge pas

    // Break
    e.setSceneLock (breakScene, decay, 0.25f, 0.0);
    bi.breakActive = true; run (e, 4, bi);
    CHECK (std::abs (e.getEffective (decay) - 0.25f) < 0.01f);
    bi.breakActive = false; run (e, 200, bi);
    CHECK (std::abs (e.getEffective (decay) - 0.5f) < 0.03f);

    e.setSceneMuted (0, true, 0.0);
    e.setSceneMuted (8, true, 0.0);
    run (e, 4, bi);
    CHECK (std::abs (e.getEffective (decay) - base) < 1.0e-6f);
}

static void testStepLocksAndConditions()
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    const auto tone = e.findParam (0, "inst", "p01_tone");
    const auto base = e.getParam (tone);
    auto& s = e.sequencer();
    s.setStep (0, 0, 0, 100);
    s.setStep (0, 0, 8, 100);
    e.setStepLock (0, 0, 8, tone, 0.9f, 0.0);
    s.requestPlay();
    // 120 BPM : un pas de 1/16 = 6000 échantillons. Pas 8 à 48000.
    run (e, 200); // jusqu'à 51200 : le pas 8 vient d'être joué
    CHECK (std::abs (e.getEffective (tone) - 0.9f) < 1.0e-4f);
    run (e, 180); // pas 0 de la mesure suivante (96000) : sans verrou
    CHECK (std::abs (e.getEffective (tone) - base) < 1.0e-4f);

    // Condition 1:2 : jouée une boucle sur deux
    Engine e2;
    e2.prepare (48000.0, 256);
    e2.setInstrument (0, "drumA", 0.0);
    e2.sequencer().setStep (0, 0, 0, 100);
    e2.setStepExtra (0, 0, 0, Condition::ab, 1, 2, 100, 0.0);
    e2.sequencer().requestPlay();
    run (e2, 750); // 4 boucles d'une mesure (2 s chacune à 120 BPM = 96000 éch. ; 750 × 256 = 192000)
    CHECK (e2.getHitCount (0, 0) == 1);
    run (e2, 750);
    CHECK (e2.getHitCount (0, 0) == 2);
}

static void testModulation()
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    const auto tone = e.findParam (0, "inst", "p01_tone");
    const auto local = localOf (tone);
    // source 1 : LFO libre 5 Hz sinus
    e.setParam (e.findParam (0, "mod1", "sync"), 0.0f);
    e.setParam (e.findParam (0, "mod1", "rate"), modSourceDesc().params[msRate].toNorm (5.0f));
    const auto idx = e.addModulation (0, 0, local, 0.4f, 0.0);
    CHECK (idx == 0);
    CHECK (e.getModulations (0).size() == 1);
    float mn = 1.0f, mx = 0.0f;
    for (int i = 0; i < 400; ++i) { run (e, 1); mn = std::min (mn, e.getEffective (tone)); mx = std::max (mx, e.getEffective (tone)); }
    const auto base = e.getParam (tone);
    CHECK (mx > base + 0.3f || mx > 0.99f);
    CHECK (mn < base - 0.3f || mn < 0.01f);
    // Même cible : met à jour au lieu d'ajouter
    CHECK (e.addModulation (0, 1, local, -0.2f, 0.0) == 0);
    CHECK (e.getModulations (0).size() == 1);
    e.removeModulation (0, local, 0.0);
    CHECK (e.getModulations (0).empty());
    run (e, 2);
    CHECK (std::abs (e.getEffective (tone) - base) < 1.0e-6f);
}

// v0.13.2 : retard du LFO de modulation — muet après chaque note pendant le retard, puis en fondu
static void testLfoDelay()
{
    const auto& d = modSourceDesc();
    std::vector<float> p (d.params.size());
    for (size_t i = 0; i < p.size(); ++i) p[i] = d.params[i].def;
    p[msType] = 0.0f; p[msSync] = 0.0f; p[msRate] = 5.0f; p[msShape] = 3.0f; p[msDelay] = 400.0f; p[msRetrig] = 0.0f;
    ModSource m;
    m.reset();
    m.trigger();
    double early = 0.0, late = 0.0;
    for (int k = 0; k < 100; ++k)
    {
        const auto v = std::abs (m.advance (p.data(), 0.01, 120.0, 0.0, false, 0.0f));
        if (k < 38) early = std::max (early, (double) v);
        if (k > 70) late = std::max (late, (double) v);
    }
    std::printf ("  retard du LFO : %.3f pendant le retard, %.3f apr\xC3\xA8s\n", early, late);
    CHECK (early < 1.0e-6 && late > 0.99);
}

static void testSeqModSource()
{
    // Source « séquenceur » : la valeur suit les pas, transport arrêté (pas libres) comme en lecture (calés sur la mesure)
    for (bool playing : { false, true })
    {
        Engine e;
        e.prepare (48000.0, 256);
        CHECK (e.setInstrument (0, "hybrid", 0.0));
        const auto cut = e.findParam (0, "inst", "cutoff");
        CHECK (cut >= 0);
        e.setParam (cut, 0.4f);
        const auto& md = modSourceDesc();
        e.setParam (e.findParam (0, "mod1", "type"), md.params[msType].toNorm (3.0f));
        e.setParam (e.findParam (0, "mod1", "division"), md.params[msDivision].toNorm (8.0f)); // 1/16
        for (int i = 0; i < 16; ++i) e.setParam (e.findParam (0, "mod1", "step" + std::to_string (i + 1)), md.params[(size_t) (msStep1 + i)].toNorm (i % 2 == 0 ? 100.0f : 0.0f));
        CHECK (e.addModulation (0, 0, localOf (cut), 0.5f, 0.0) >= 0);
        if (playing) e.sequencer().requestPlay();
        float mn = 1.0f, mx = 0.0f;
        for (int i = 0; i < 400; ++i) { run (e, 1); mn = std::min (mn, e.getEffective (cut)); mx = std::max (mx, e.getEffective (cut)); }
        std::printf ("  source s\xC3\xA9quenceur (%s) : coupure de %.2f \xC3\xA0 %.2f\n", playing ? "lecture" : "arr\xC3\xAAt", mn, mx);
        CHECK (mn < 0.42f && mx > 0.85f);
    }
    // Source « fonction » (3e source, réglée d'origine pour repartir à chaque note), puis en mode libre (boucle)
    {
        Engine e;
        e.prepare (48000.0, 256);
        CHECK (e.setInstrument (0, "hybrid", 0.0));
        const auto cut = e.findParam (0, "inst", "cutoff");
        e.setParam (cut, 0.2f);
        const auto& md = modSourceDesc();
        CHECK (std::abs (md.params[msRetrig].toPlain (e.getParam (e.findParam (0, "mod3", "retrig"))) - 1.0f) < 0.01f);
        CHECK (e.addModulation (0, 2, localOf (cut), 0.6f, 0.0) >= 0);
        run (e, 200); // la fonction a fini son tour de départ
        const auto rest = e.getEffective (cut);
        e.pushEvent ({ InputEvent::noteOn, 0, 60, 0, 0.9f, false });
        float peak = 0.0f;
        for (int i = 0; i < 20; ++i) { run (e, 1); peak = std::max (peak, e.getEffective (cut)); }
        std::printf ("  source fonction : repos %.2f, apr\xC3\xA8s une note %.2f\n", rest, peak);
        CHECK (rest < 0.25f && peak > 0.7f);
        // libre : tourne en boucle sans note
        e.setParam (e.findParam (0, "mod3", "retrig"), md.params[msRetrig].toNorm (0.0f));
        float mn = 1.0f, mx = 0.0f;
        for (int i = 0; i < 400; ++i) { run (e, 1); mn = std::min (mn, e.getEffective (cut)); mx = std::max (mx, e.getEffective (cut)); }
        CHECK (mx > 0.7f && mn < 0.25f);
    }
}

static void testSamplesAndSwap()
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    auto smp = std::make_unique<SampleBuffer>();
    smp->sampleRate = 48000.0;
    smp->data.resize (4800);
    for (size_t i = 0; i < smp->data.size(); ++i) smp->data[i] = 0.5f * std::sin ((float) i * 0.05f);
    e.setSample (0, 14, std::move (smp), 0.0);
    CHECK (e.getSample (0, 14) != nullptr);
    e.sequencer().setStep (0, 14, 0, 127);
    e.sequencer().requestPlay();
    auto r = run (e, 30);
    CHECK (r.peak > 0.05f);

    // Changement d'instrument en cours de lecture, puis ménage de la corbeille
    for (int i = 0; i < 10; ++i)
    {
        e.setInstrument (0, (i % 2) ? "drum80" : "drum90", (double) i * 1000.0);
        r = run (e, 10);
        CHECK (r.finite);
        e.collectGarbage ((double) i * 1000.0);
    }
    CHECK (e.getSample (0, 14) == nullptr);
    e.collectGarbage (1.0e9);
}

static void testInserts()
{
    Engine e;
    e.prepare (48000.0, 256);
    CHECK (! e.setInsert (0, 0, "inexistant", 0.0));
    for (const auto* d : effectCatalog())
    {
        CHECK (e.setInsert (0, 0, d->type, 0.0));
        CHECK (e.getInsertTypes (0)[0] == d->type);
    }
}

// v0.46 fix16 : décaler l'écriture d'un pas, en boucle ; fonctions des pas, conditions et verrous suivent
// v0.46 fix22 : pavé vectoriel en mode scènes — quatre scènes mêlées selon les poids des coins
static void testVectorScenes()
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    const auto decay = e.findParam (0, "inst", "p01_decay");
    const auto base = e.getParam (decay);
    e.setSceneLock (0, decay, 0.0f, 0.0);
    e.setSceneLock (1, decay, 1.0f, 0.0);
    e.setSceneLock (2, decay, 0.2f, 0.0);
    // scène 3 vide : le coin D garde la valeur de base
    Engine::BlockInfo bi;
    bi.vecOn = true; bi.vecMode = 0; bi.vecScene = { 0, 1, 2, 3 };
    bi.vecW = { 0.0f, 1.0f, 0.0f, 0.0f }; run (e, 200, bi);
    CHECK (std::abs (e.getEffective (decay) - 1.0f) < 0.02f);
    bi.vecW = { 0.25f, 0.25f, 0.25f, 0.25f }; run (e, 200, bi);
    CHECK (std::abs (e.getEffective (decay) - (0.0f + 1.0f + 0.2f + base) * 0.25f) < 0.03f);
    bi.vecW = { 0.0f, 0.0f, 0.0f, 1.0f }; run (e, 200, bi);
    CHECK (std::abs (e.getEffective (decay) - base) < 0.02f);
    bi.vecOn = false; bi.sceneA = 0; bi.sceneB = 1; bi.fader = 0.0f; run (e, 200, bi); // retour au crossfader
    CHECK (std::abs (e.getEffective (decay) - 0.0f) < 0.02f);
}

static void testShiftPattern()
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    auto& s = e.sequencer();
    const auto tone = e.findParam (0, "inst", "p01_tone");
    s.setStep (0, 0, 0, 100); s.setStep (0, 0, 15, 90); s.setNote (0, 0, 15, 64);
    s.setStepFlags (0, 15, stepAccent);
    e.setStepLock (0, 0, 15, tone, 0.7f, 0.0);
    s.setStep (0, 1, 3, 80);
    s.setLaneLength (0, 1, 4); // ligne 2 sur 4 pas
    s.shiftPattern (0, s.getEditing (0), -1, 1);
    e.shiftPatternExtras (0, s.getEditing (0), -1, 1, 0.0);
    CHECK (s.getStep (0, 0, 1) == 100 && s.getStep (0, 0, 0) == 90 && s.getNote (0, 0, 0) == 64);
    CHECK (s.getStepFlags (0, 0) == stepAccent && s.getStepFlags (0, 15) == 0);
    CHECK (e.getExtras().find (0, 0, 0, 0) != nullptr && e.getExtras().find (0, 0, 0, 15) == nullptr);
    CHECK (s.getStep (0, 1, 0) == 80 && s.getStep (0, 1, 3) == 0 && s.getStep (0, 1, 4) == 0); // ligne de 4 pas : 3 → 0
    // une seule ligne, vers la gauche : la ligne 1 bouge, pas les fonctions des pas
    s.shiftPattern (0, s.getEditing (0), 0, -1);
    e.shiftPatternExtras (0, s.getEditing (0), 0, -1, 0.0);
    CHECK (s.getStep (0, 0, 0) == 100 && s.getStep (0, 0, 15) == 90 && s.getStepFlags (0, 0) == stepAccent);
    CHECK (e.getExtras().find (0, 0, 0, 15) != nullptr && s.getStep (0, 1, 0) == 80);
}

static void testCpu()
{
    // 16 pistes chargées, sequence dense : mesure du temps de calcul
    Engine e;
    e.prepare (48000.0, 256);
    const auto& cat = instrumentCatalog();
    for (int t = 0; t < 16; ++t)
    {
        e.setInstrument (t, cat[(size_t) t % cat.size()]->type, 0.0);
        for (int lane = 0; lane < 4; ++lane)
            for (int st = lane; st < 16; st += 2) { e.sequencer().setStep (t, lane, st, 100); e.sequencer().setNote (t, lane, st, 48 + lane * 4 + t % 12); }
    }
    e.sequencer().requestPlay();
    const auto blocks = 48000 * 10 / 256;
    const auto t0 = std::chrono::steady_clock::now();
    const auto r = run (e, blocks);
    const auto sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("  16 pistes : %.1f %% d'un c\xC5\x93ur (10 s rendues en %.3f s), cr\xC3\xAAte %.2f\n", sec / 10.0 * 100.0, sec, r.peak);
    CHECK (r.finite);
}

static void setPlain (Engine& e, int ch, const char* area, const char* id, float plain)
{
    const auto g = e.findParam (ch, area, id);
    CHECK (g >= 0);
    if (const auto* d = e.describe (g)) e.setParam (g, d->toNorm (plain));
}

static void testPadSendsAndPreFader()
{
    // Envoi d'un pad vers le retour A (réverbération), piste à zéro : la réverbération sonne quand même
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    e.setInsert (numTracks + 0, 0, "reverb", 0.0);
    setPlain (e, 0, "mix", "volume", -60.0f);           // piste coupée : il ne reste que l'envoi du pad
    setPlain (e, 0, "inst", "p01_sendA", 100.0f);
    e.sequencer().setStep (0, 0, 0, 120);
    e.sequencer().requestPlay();
    auto r = run (e, 200);
    CHECK (r.finite && r.peak > 0.005f);
    std::printf ("  envoi du pad vers le retour A : cr\xC3\xAAte %.3f\n", r.peak);

    // Sans envoi de pad, envoi de piste avant le fader : la réverbération sonne aussi, fader fermé
    Engine e2;
    e2.prepare (48000.0, 256);
    e2.setInstrument (0, "drumA", 0.0);
    e2.setInsert (numTracks + 0, 0, "reverb", 0.0);
    setPlain (e2, 0, "mix", "volume", -60.0f);
    setPlain (e2, 0, "mix", "sendA", 100.0f);
    run (e2, 4);                                        // laisse les gains rejoindre leur valeur
    e2.sequencer().setStep (0, 0, 0, 120);
    e2.sequencer().requestPlay();
    r = run (e2, 200);
    CHECK (r.peak < 1.0e-6f);                           // envoi après le fader : rien
    setPlain (e2, 0, "mix", "sendPre", 1.0f);
    e2.sequencer().requestPlay();
    r = run (e2, 200);
    CHECK (r.peak > 0.005f);                            // envoi avant le fader : la réverbération sonne
}

static void testPadFx()
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    for (int type = 0; type < 6; ++type)
    {
        setPlain (e, 0, "inst", "p01_fx", (float) type);
        setPlain (e, 0, "inst", "p01_fxAmt", 80.0f);
        e.sequencer().setStep (0, 0, 0, 120);
        e.sequencer().requestPlay();
        const auto r = run (e, 100);
        CHECK (r.finite && r.peak > 0.001f && r.peak < 4.0f);
    }
}

static void testChains()
{
    // Chaque chaîne du catalogue n'utilise que des effets et des préréglages qui existent
    for (const auto& c : chainCatalog())
    {
        CHECK (c.inserts.size() <= (size_t) numInserts);
        for (const auto& [type, preset] : c.inserts)
        {
            const auto* d = findEffect (type);
            CHECK (d != nullptr);
            bool found = false;
            if (d != nullptr) for (const auto& p : d->presets) found = found || p.name == preset;
            if (! found) std::printf ("  pr\xC3\xA9r\xC3\xA9glage absent : %s / %s\n", type.c_str(), preset.c_str());
            CHECK (found);
        }
    }
    Engine e;
    e.prepare (48000.0, 256);
    CHECK (e.applyChain (masterChannel, "Bus principal", 0.0));
    CHECK (e.getInsertTypes (masterChannel)[0] == "eq" && e.getInsertTypes (masterChannel)[2] == "limiter" && e.getInsertTypes (masterChannel)[3].empty());
    CHECK (! e.applyChain (0, "inconnue", 0.0));
}

static void testPatternsInEngine()
{
    // Les verrous et conditions suivent le pattern : un verrou du pattern 2 ne touche pas le pattern 1
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    auto& s = e.sequencer();
    s.setStepP (0, 1, 0, 0, 120);
    e.setStepExtraP (0, 1, 0, 0, Condition::fill, 1, 2, 100, 0.0);
    CHECK (e.getExtras().find (0, 1, 0, 0) != nullptr && e.getExtras().find (0, 0, 0, 0) == nullptr);
    e.copyPatternExtras (0, 1, 3, 0.0);
    CHECK (e.getExtras().find (0, 3, 0, 0) != nullptr);
    e.clearPatternExtras (0, 1, 0.0);
    CHECK (e.getExtras().find (0, 1, 0, 0) == nullptr && e.getExtras().find (0, 3, 0, 0) != nullptr);
}

static void testPadStrip()
{
    // Filtre analogique, VCA, groupe d'étouffement et sensibilité à la vélocité d'un pad
    using namespace grv::instruments;
    const auto& desc = kitAnalogDesc();
    auto energy = [&] (auto&& setup, auto&& play, double fromS, double toS)
    {
        DrumKit kit (desc, kitAnalogPads());
        kit.prepare (48000.0, 64);
        std::vector<float> p (desc.params.size());
        for (size_t i = 0; i < p.size(); ++i) p[i] = desc.params[i].def;
        setup (p);
        std::vector<float> L (64), R (64);
        double e = 0.0;
        for (int b = 0; b < 48000 / 64; ++b)
        {
            play (kit, b);
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            kit.render (L.data(), R.data(), 64, p.data());
            const auto t = b * 64 / 48000.0;
            if (t >= fromS && t < toS) for (auto x : L) e += (double) x * x;
        }
        return e;
    };
    auto idx = [&] (const std::string& id) { for (size_t i = 0; i < desc.params.size(); ++i) if (desc.params[i].id == id) return i; return (size_t) 0; };
    const auto hit = [] (int pad) { return [pad] (DrumKit& k, int b) { if (b == 0) k.noteOn (pad, 1.0f); }; };
    const auto none = [] (std::vector<float>&) {};

    // charley fermé (pad 5) dans le passe-bas 24 dB à 300 Hz : presque tout disparaît
    const auto open = energy (none, hit (4), 0.0, 0.2);
    const auto filtered = energy ([&] (std::vector<float>& p) { p[idx ("p05_ftype")] = 1; p[idx ("p05_fcut")] = 300; }, hit (4), 0.0, 0.2);
    CHECK (open > 0.0 && filtered < open * 0.05);
    // enveloppe du filtre : ouvrir la coupure au coup rend le son plus fort qu'un filtre fermé
    const auto withEnv = energy ([&] (std::vector<float>& p) { p[idx ("p05_ftype")] = 1; p[idx ("p05_fcut")] = 300; p[idx ("p05_fenv")] = 100; p[idx ("p05_fdec")] = 80; }, hit (4), 0.0, 0.2);
    CHECK (withEnv > filtered * 3.0);
    // VCA : déclin de 40 ms sur la basse longue, plus rien après 0,3 s
    const auto longTail = energy (none, hit (9), 0.3, 0.6);
    const auto shortTail = energy ([&] (std::vector<float>& p) { p[idx ("p10_adec")] = 40; }, hit (9), 0.3, 0.6);
    CHECK (longTail > 0.0 && shortTail < longTail * 1.0e-3);
    // attaque lente du VCA : le début est adouci
    const auto fastStart = energy (none, hit (0), 0.0, 0.01);
    const auto slowStart = energy ([&] (std::vector<float>& p) { p[idx ("p01_aatk")] = 200; }, hit (0), 0.0, 0.01);
    CHECK (slowStart < fastStart * 0.1);
    // groupe d'étouffement : le rimshot coupe la basse longue du même groupe
    const auto both = [] (DrumKit& k, int b) { if (b == 0) k.noteOn (9, 1.0f); if (b == 75) k.noteOn (3, 1.0f); };
    const auto free = energy (none, both, 0.3, 0.6);
    const auto choked = energy ([&] (std::vector<float>& p) { p[idx ("p10_choke")] = 1; p[idx ("p04_choke")] = 1; }, both, 0.3, 0.6);
    CHECK (free > 0.0 && choked < free * 1.0e-3);
    // sensibilité à la vélocité nulle : un coup doux sonne comme un coup fort (à la dérive près)
    const auto soft = energy ([&] (std::vector<float>& p) { p[idx ("p01_vel")] = 0; }, [] (DrumKit& k, int b) { if (b == 1) k.noteOn (0, 0.1f); }, 0.0, 0.5);
    const auto hard = energy ([&] (std::vector<float>& p) { p[idx ("p01_vel")] = 0; }, [] (DrumKit& k, int b) { if (b == 1) k.noteOn (0, 1.0f); }, 0.0, 0.5);
    CHECK (std::abs (soft / hard - 1.0) < 0.15);
    std::printf ("  pad : filtre %.4f, VCA %.2e, \xC3\xA9touffement %.2e\n", filtered / open, shortTail / longTail, choked / free);
}

static void testEditCells()
{
    // Sélection du séquenceur : déplacer un pas avec sa condition et son verrou, en une édition
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    auto& s = e.sequencer();
    s.setStep (0, 2, 4, 100);
    const auto g = e.findParam (0, "inst", "p03_tune");
    e.setStepExtra (0, 2, 4, Condition::fill, 1, 2, 60, 0.0);
    e.setStepLock (0, 2, 4, g, 0.8f, 0.0);
    Engine::CellWrite w;
    w.lane = 5; w.step = 9; w.vel = 90; w.note = 60; w.len8 = 16; w.micro = 3; w.hasExtra = true;
    w.extra.cond = Condition::fill; w.extra.prob = 60; w.extra.locks = { { g, 0.8f }, { -5, 0.1f }, { g + 1, 2.0f } };
    e.editCells (0, { { 2, 4 } }, { w }, 0.0);
    CHECK (s.getStep (0, 2, 4) == 0 && e.getExtras().find (0, 0, 2, 4) == nullptr);
    CHECK (s.getStep (0, 5, 9) == 90 && s.getLength8 (0, 5, 9) == 16 && s.getMicro (0, 5, 9) == 3);
    const auto* x = e.getExtras().find (0, 0, 5, 9);
    CHECK (x != nullptr && x->cond == Condition::fill && x->prob == 60 && x->locks.size() == 2); // adresse invalide écartée
    // verrou sur plusieurs pas, puis retrait
    s.setStep (0, 0, 0, 100); s.setStep (0, 0, 4, 100);
    e.setStepLocks (0, { { 0, 0 }, { 0, 4 } }, g, 0.25f, false, 0.0);
    CHECK (e.getExtras().find (0, 0, 0, 0) != nullptr && e.getExtras().find (0, 0, 0, 4) != nullptr);
    e.setStepLocks (0, { { 0, 0 }, { 0, 4 } }, g, 0.0f, true, 0.0);
    CHECK (e.getExtras().find (0, 0, 0, 0) == nullptr && e.getExtras().find (0, 0, 0, 4) == nullptr);
}

static void testUserPresetValues()
{
    // Préréglage perso d'un pad : lu sur le pad 3, appliqué au pad 7 ; le reste ne bouge pas
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    setPlain (e, 0, "inst", "p03_decay", 777.0f);
    setPlain (e, 0, "inst", "p03_ftype", 2.0f);
    setPlain (e, 0, "inst", "p01_decay", 123.0f);
    const auto v = e.moduleValues (0, -1, "p03_");
    CHECK (v.size() == 25 + 12); // v0.10 : + réglages de machine du pad
    CHECK (e.applyValues (0, -1, v, "p07_", {}));
    auto plain = [&] (const char* id) { const auto g = e.findParam (0, "inst", id); return e.describe (g)->toPlain (e.getParam (g)); };
    CHECK (std::abs (plain ("p07_decay") - 777.0f) < 1.0f && (int) plain ("p07_ftype") == 2);
    CHECK (std::abs (plain ("p01_decay") - 123.0f) < 1.0f);
    // valeurs absentes : origine ; valeurs hors plage : bornées
    CHECK (e.applyValues (0, -1, { { "decay", 1.0e9f } }, "p07_", {}));
    CHECK (std::abs (plain ("p07_decay") - 4000.0f) < 1.0f && (int) plain ("p07_ftype") == 0);
}

static void testDistortionAlgorithms()
{
    // Les 14 algorithmes, avec ou sans asymétrie, drive au maximum, tous les routages : rien d'invalide
    using namespace grv::effects;
    Distortion fx;
    fx.prepare (48000.0, 256);
    const auto& d = fx.desc();
    CHECK (d.params[(size_t) dist::typeA].choices.size() == 14);
    std::vector<float> p (d.params.size());
    for (size_t i = 0; i < p.size(); ++i) p[i] = d.params[i].def;
    std::vector<float> L (256), R (256);
    for (int type = 0; type < 14; ++type)
        for (float bias : { -100.0f, 0.0f, 60.0f })
            for (int route = 0; route < 5; ++route)
            {
                p[dist::typeA] = (float) type; p[dist::typeB] = (float) (13 - type);
                p[dist::driveA] = 100; p[dist::driveB] = 100; p[dist::mixB] = 100;
                p[dist::biasA] = bias; p[dist::biasB] = -bias; p[dist::routing] = (float) route;
                bool ok = true; float peak = 0.0f;
                for (int b = 0; b < 20; ++b)
                {
                    for (int i = 0; i < 256; ++i) { L[(size_t) i] = 0.8f * std::sin (0.05f * (float) (b * 256 + i)); R[(size_t) i] = L[(size_t) i]; }
                    fx.process (L.data(), R.data(), 256, p.data(), 120.0);
                    for (int i = 0; i < 256; ++i) { ok = ok && std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]); peak = std::max (peak, std::abs (L[(size_t) i])); }
                }
                CHECK (ok && peak < 8.0f);
            }
}

static void testGateDeesser()
{
    using namespace grv::effects;
    GateDeesser fx;
    fx.prepare (48000.0, 256);
    const auto& d = fx.desc();
    std::vector<float> p (d.params.size());
    for (size_t i = 0; i < p.size(); ++i) p[i] = d.params[i].def;
    std::vector<float> L (256), R (256);
    auto run = [&] (float amp, float w, int blocks) {
        float peak = 0.0f; bool ok = true;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 256; ++i) { L[(size_t) i] = amp * std::sin (w * (float) (b * 256 + i)); R[(size_t) i] = L[(size_t) i]; }
            fx.process (L.data(), R.data(), 256, p.data(), 120.0);
            if (b >= blocks / 2) for (int i = 0; i < 256; ++i) { ok = ok && std::isfinite (L[(size_t) i]); peak = std::max (peak, std::abs (L[(size_t) i])); }
        }
        return ok ? peak : -1.0f;
    };
    const float lowW = 2.0f * 3.14159265f * 200.0f / 48000.0f, highW = 2.0f * 3.14159265f * 8000.0f / 48000.0f;
    // Porte : un signal fort passe, un signal faible (-66 dB) est atténué de la plage (40 dB)
    p[gate::gThresh] = -40; p[gate::gRange] = 40;
    CHECK (run (0.5f, lowW, 80) > 0.45f);
    const auto quiet = run (0.0005f, lowW, 200);
    CHECK (quiet >= 0.0f && quiet < 0.0005f * 0.03f);
    CHECK (fx.meter (1) > 35.0f);
    // Porte au minimum : toujours ouverte
    p[gate::gThresh] = gate::gateOffDb;
    CHECK (std::abs (run (0.0005f, lowW, 40) - 0.0005f) < 0.00005f);
    // Dé-esseur sous le seuil : le signal sort intact (séparation complémentaire)
    p[gate::dsAmount] = 10; p[gate::dsThresh] = -10;
    // 8 kHz à 48 kHz : la crête échantillonnée tombe à 60° (0,866)
    { const auto v = run (0.05f, highW, 40); CHECK (std::abs (v - 0.05f * 0.8660254f) < 0.001f); }
    // Sifflante forte : réduite ; grave fort : intact en mode « bande haute »
    CHECK (run (0.8f, highW, 60) < 0.8f * 0.6f);
    CHECK (fx.meter (2) > 4.0f);
    CHECK (run (0.8f, lowW, 60) > 0.75f);
    // Mode large et écoute : valeurs valides
    p[gate::dsMode] = 1; CHECK (run (0.8f, highW, 40) > 0.0f);
    p[gate::dsListen] = 1; CHECK (run (0.8f, lowW, 40) < 0.2f);
}

static void testLfoSync()
{
    // LFO d'instrument : libre = vitesse en Hz ; 1/4 à 120 BPM = 2 Hz ; 1 mesure = 0,5 Hz ; 1/8T = 6 Hz
    using grv::synth::lfoSyncHz;
    CHECK (std::abs (lfoSyncHz (0, 7.0, 120.0) - 7.0) < 1e-9);
    CHECK (std::abs (lfoSyncHz (7, 7.0, 120.0) - 2.0) < 1e-9);
    CHECK (std::abs (lfoSyncHz (5, 7.0, 120.0) - 0.5) < 1e-9);
    CHECK (std::abs (lfoSyncHz (13, 7.0, 120.0) - 6.0) < 1e-9);
    CHECK (std::abs (lfoSyncHz (1, 7.0, 120.0) - 0.03125) < 1e-9); // 16 mesures à 120 BPM : une période en 32 s
    CHECK (std::abs (lfoSyncHz (99, 7.0, 120.0) - 7.0) < 1e-9);
    CHECK ((int) grv::synth::lfoSyncChoices().size() == 18);
    // Projets d'avant la version 7 : le même choix est retrouvé (deux divisions longues insérées)
    const auto& md = modSourceDesc();
    CHECK (hasLongDivisions (md.params[msDivision]) && widenDivisionIndex (md.params[msDivision], 4) == 6);
    ParamDesc sync; sync.choices = grv::synth::lfoSyncChoices();
    CHECK (hasLongDivisions (sync) && widenDivisionIndex (sync, 0) == 0 && widenDivisionIndex (sync, 5) == 7);
}

static void testModSourceDefaults()
{
    // Réglage d'origine des 6 sources : LFO, LFO, fonction, fonction, suiveur, séquenceur
    Engine e;
    e.prepare (48000.0, 256);
    const int expected[numModSources] { 0, 0, 1, 1, 2, 3 };
    const auto& d = modSourceDesc();
    for (int ch : { 0, 5, masterChannel })
        for (int s = 0; s < numModSources; ++s)
        {
            const auto plain = d.params[(size_t) msType].toPlain (e.getParam (globalSlot (ch, modSourceBase + s * modSourceSlots + msType)));
            CHECK ((int) std::lround (plain) == expected[s]);
        }
}


/** Arpégiateur : ordre des notes, octaves, maintien, calage sur la grille. */
static void testArpeggiator()
{
    struct Hit { long long at; int note; };
    auto runArp = [] (Arpeggiator& a, const Arpeggiator::Settings& s, int blocks, std::vector<Hit>& out, long long& t)
    {
        const double pps = 120.0 / 60.0 / 48000.0;
        for (int b = 0; b < blocks; ++b)
        {
            a.process (s, (double) t * pps, true, pps, 32, [&] (int n, float) { out.push_back ({ t, n }); }, [] (int) {});
            t += 32;
        }
    };
    Arpeggiator a;
    Arpeggiator::Settings s;
    s.mode = Arpeggiator::up; s.octaves = 1; s.stepQuarters = 0.25; s.gate = 0.5;
    for (int n : { 67, 60, 64 }) a.noteOn (n, 0.8f, false);
    std::vector<Hit> hits;
    long long t = 0;
    runArp (a, s, 48000 / 32, hits, t); // 1 s = 8 doubles croches
    CHECK (hits.size() >= 8 && hits.size() <= 9);
    CHECK (hits.size() >= 4 && hits[0].note == 60 && hits[1].note == 64 && hits[2].note == 67 && hits[3].note == 60);
    // pas suivants sur la grille de la double croche (6000 échantillons à 120 BPM), à 32 près
    CHECK (hits.size() >= 3 && std::llabs (hits[2].at - 12000) <= 32);

    Arpeggiator d;
    s.mode = Arpeggiator::downUp; s.octaves = 2;
    for (int n : { 60, 64 }) d.noteOn (n, 0.8f, false);
    hits.clear(); t = 0;
    runArp (d, s, 48000 * 2 / 32, hits, t);
    CHECK (hits.size() >= 6 && hits[0].note == 76 && hits[1].note == 72 && hits[2].note == 64 && hits[3].note == 60 && hits[4].note == 64);

    // maintien : les notes relâchées continuent ; une nouvelle note après avoir tout relâché remplace l'accord
    Arpeggiator h;
    s.mode = Arpeggiator::up; s.octaves = 1; s.hold = true;
    h.noteOn (60, 1.0f, true); h.noteOn (63, 1.0f, true);
    h.noteOff (60); h.noteOff (63);
    hits.clear(); t = 0;
    runArp (h, s, 48000 / 32, hits, t);
    CHECK (hits.size() >= 7);
    h.noteOn (70, 1.0f, true);
    hits.clear();
    runArp (h, s, 48000 / 32, hits, t);
    bool only70 = ! hits.empty();
    for (auto& x : hits) only70 = only70 && x.note == 70;
    CHECK (only70);

    // nouveaux modes : suites attendues sur Do-Mi-Sol-Si (60 64 67 71), une octave
    {
        auto seqOf = [&] (int mode, int count)
        {
            Arpeggiator m;
            Arpeggiator::Settings ms;
            ms.mode = mode; ms.octaves = 1; ms.stepQuarters = 0.25; ms.gate = 0.5;
            for (int n : { 67, 60, 71, 64 }) m.noteOn (n, 0.8f, false);
            std::vector<Hit> out;
            long long tt = 0;
            runArp (m, ms, 48000 * 4 / 32, out, tt);
            std::vector<int> notes;
            for (int i = 0; i < count && i < (int) out.size(); ++i) notes.push_back (out[(size_t) i].note);
            return notes;
        };
        using V = std::vector<int>;
        CHECK (seqOf (Arpeggiator::upDownIncl, 9) == V ({ 60, 64, 67, 71, 71, 67, 64, 60, 60 }));
        CHECK (seqOf (Arpeggiator::downUpIncl, 8) == V ({ 71, 67, 64, 60, 60, 64, 67, 71 }));
        CHECK (seqOf (Arpeggiator::converge, 5) == V ({ 60, 71, 64, 67, 60 }));
        CHECK (seqOf (Arpeggiator::diverge, 5) == V ({ 67, 64, 71, 60, 67 }));
        CHECK (seqOf (Arpeggiator::convergeDiverge, 8) == V ({ 60, 71, 64, 67, 67, 64, 71, 60 }));
        CHECK (seqOf (Arpeggiator::pedalLow, 7) == V ({ 60, 64, 60, 67, 60, 71, 60 }));
        CHECK (seqOf (Arpeggiator::pedalHigh, 7) == V ({ 71, 60, 71, 64, 71, 67, 71 }));
        CHECK (seqOf (Arpeggiator::skipUp, 6) == V ({ 60, 67, 64, 71, 67, 60 }));
        CHECK (seqOf (Arpeggiator::stairsUp, 9) == V ({ 60, 64, 67, 64, 67, 71, 67, 71, 60 }));
        CHECK (seqOf (Arpeggiator::stairsDown, 6) == V ({ 71, 67, 64, 67, 64, 60 }));
        CHECK (seqOf (Arpeggiator::playedReverse, 4) == V ({ 64, 71, 60, 67 }));
        CHECK (seqOf (Arpeggiator::repeatUp, 6) == V ({ 60, 60, 64, 64, 67, 67 }));
        // mélangé : chaque tour de 4 contient les 4 notes une seule fois
        const auto sh = seqOf (Arpeggiator::shuffle, 12);
        bool eachOnce = sh.size() == 12;
        for (int r = 0; r < 3 && eachOnce; ++r)
        {
            V turn (sh.begin() + r * 4, sh.begin() + r * 4 + 4);
            std::sort (turn.begin(), turn.end());
            eachOnce = turn == V ({ 60, 64, 67, 71 });
        }
        CHECK (eachOnce);
        // marche aléatoire : jamais plus d'une note d'écart
        const auto w = seqOf (Arpeggiator::randomWalk, 24);
        const V sorted { 60, 64, 67, 71 };
        bool steps = w.size() == 24;
        for (size_t i = 1; i < w.size() && steps; ++i)
        {
            const auto a1 = std::find (sorted.begin(), sorted.end(), w[i - 1]) - sorted.begin();
            const auto a2 = std::find (sorted.begin(), sorted.end(), w[i]) - sorted.begin();
            steps = std::abs ((int) (a2 - a1)) == 1;
        }
        CHECK (steps);
        // une seule note tenue : aucun mode ne sort de la note
        for (int mode = 1; mode < Arpeggiator::numModes; ++mode)
        {
            Arpeggiator one;
            Arpeggiator::Settings os;
            os.mode = mode; os.octaves = 1; os.stepQuarters = 0.25;
            one.noteOn (62, 0.8f, false);
            std::vector<Hit> out;
            long long tt = 0;
            runArp (one, os, 48000 / 32, out, tt);
            bool ok = ! out.empty();
            for (auto& x : out) ok = ok && x.note == 62;
            CHECK (ok);
        }
        CHECK ((int) grv::synth::arpModes().size() == Arpeggiator::numModes);
    }

    // arpège à pas : vélocité 0 = silence, transposition
    Arpeggiator st;
    float tr[16] {}, ve[16];
    for (auto& v : ve) v = 100.0f;
    tr[1] = 12.0f; ve[2] = 0.0f;
    s.hold = false; s.steps = 3; s.transpose = tr; s.velocity = ve;
    st.noteOn (48, 1.0f, false);
    hits.clear(); t = 0;
    runArp (st, s, 48000 / 32, hits, t);
    CHECK (hits.size() >= 4 && hits[0].note == 48 && hits[1].note == 60 && hits[2].note == 48 && hits[2].at > 15000);
}

/** Arpège et pitch bend dans le moteur (piste « Basse mono »). */
static void testArpAndExpressionInEngine()
{
    Engine e;
    e.prepare (48000.0, 256);
    CHECK (e.setInstrument (0, "mono", 0.0));
    const auto mode = e.findParam (0, "inst", "arp_mode");
    CHECK (mode >= 0);
    e.setParam (mode, e.describe (mode)->toNorm (1.0f)); // Haut
    for (int n : { 60, 64, 67 }) CHECK (e.pushEvent ({ InputEvent::noteOn, 0, n, 0, 0.9f, false }));
    run (e, 48000 / 256);
    const auto h60 = e.getHitCount (0, 60 % 16), h64 = e.getHitCount (0, 64 % 16), h67 = e.getHitCount (0, 67 % 16);
    std::printf ("  arp\xC3\xA8ge dans le moteur : %u %u %u\n", h60, h64, h67);
    CHECK (h60 >= 2 && h64 >= 2 && h67 >= 2 && h60 + h64 + h67 >= 7);
    for (int n : { 60, 64, 67 }) e.pushEvent ({ InputEvent::noteOff, 0, n, 0, 0.0f, false });
    run (e, 40);

    // pitch bend : +1 avec une plage de 2 demi-tons, onde sinus pure → 2 demi-tons plus haut
    Engine f;
    f.prepare (48000.0, 256);
    CHECK (f.setInstrument (0, "mono", 0.0));
    auto set = [&] (const char* id, float v) { const auto g = f.findParam (0, "inst", id); if (g >= 0) f.setParam (g, f.describe (g)->toNorm (v)); };
    set ("o1shape", 0.0f); set ("mix2", 0.0f); set ("mixsub", 0.0f); set ("cutoff", 16000.0f); set ("drift", 0.0f); set ("reso", 0.0f); set ("as", 100.0f);
    set ("envamt", 0.0f); set ("keytrack", 0.0f); set ("velfilter", 0.0f); set ("drive", 0.0f);
    f.pushEvent ({ InputEvent::pitchBend, 0, 0, 0, 1.0f, false });
    f.pushEvent ({ InputEvent::noteOn, 0, 57, 0, 1.0f, false }); // La 220 Hz
    std::vector<float> L (256), R (256);
    for (int b = 0; b < 40; ++b) f.process (nullptr, nullptr, L.data(), R.data(), 256, {}, nullptr, 0);
    int crossings = 0;
    float prev = 0.0f;
    const int blocks = 48000 / 256;
    for (int b = 0; b < blocks; ++b)
    {
        f.process (nullptr, nullptr, L.data(), R.data(), 256, {}, nullptr, 0);
        for (int i = 0; i < 256; ++i) { if (prev <= 0.0f && L[(size_t) i] > 0.0f) ++crossings; prev = L[(size_t) i]; }
    }
    const auto hz = crossings * 48000.0 / (blocks * 256.0);
    std::printf ("  pitch bend +2 dt sur La 220 Hz : %.1f Hz (attendu 246.9)\n", hz);
    CHECK (std::abs (hz - 246.94) < 3.0);
}

/** Glissé par pas : la note tient jusqu'à la suivante (pas de silence entre deux notes liées). */
static void testSlideInEngine()
{
    auto gapLevel = [] (bool slide)
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "acid", 0.0);
        auto& s = e.sequencer();
        s.setStep (0, 0, 0, 100); s.setNote (0, 0, 0, 45); s.setLength8 (0, 0, 0, 4);
        s.setStep (0, 0, 1, 100); s.setNote (0, 0, 1, 45); s.setLength8 (0, 0, 1, 4);
        if (slide) s.setStepFlags (0, 0, stepSlide);
        s.requestPlay();
        // 1 pas = 6000 échantillons à 120 BPM ; on mesure de 0,7 à 0,95 pas
        std::vector<float> L (100), R (100);
        double sum = 0.0; int count = 0;
        for (int b = 0; b < 60; ++b)
        {
            Engine::BlockInfo bi; bi.bpm = 120.0;
            e.process (nullptr, nullptr, L.data(), R.data(), 100, bi, nullptr, 0);
            const auto at = b * 100;
            if (at >= 4200 && at < 5700) for (auto v : L) { sum += (double) v * v; ++count; }
        }
        return std::sqrt (sum / std::max (1, count));
    };
    const auto plain = gapLevel (false), tied = gapLevel (true);
    std::printf ("  glissé : niveau entre deux pas %.4f (sans) / %.4f (avec)\n", plain, tied);
    CHECK (tied > plain * 5.0 && tied > 0.01);
}

// v0.12.7 : boucle Pickup des pistes choisies (la piste elle-même et les pistes non cochées n'y
// entrent pas), pas d'enregistrement, pas sans déclenchement, glissé des verrous
static void recAction (Engine& e, int track, int action)
{
    std::vector<float> L (256), R (256);
    InputEvent ev { InputEvent::recordAction, track, action, 0, 0.0f, false };
    e.process (nullptr, nullptr, L.data(), R.data(), 256, {}, &ev, 1);
}

static void testOctaInEngine()
{
    auto loopPeak = [] (bool drumsChosen)
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        e.setInstrument (1, "octa", 0.0);
        setPlain (e, 1, "inst", "t1_machine", 1.0f);   // Pickup
        setPlain (e, 1, "inst", "t1_recsrc", 1.0f);    // pistes choisies
        setPlain (e, 1, "inst", "t1_picklen", 0.0f);   // boucle libre
        if (! drumsChosen) setPlain (e, 1, "inst", "t1_rt1", 0.0f);
        auto& s = e.sequencer();
        for (int k = 0; k < 16; k += 4) s.setStep (0, 0, k, 110);
        s.requestPlay();
        run (e, 4);
        recAction (e, 1, 0);                         // enregistre la boucle
        run (e, 200);                                // ~1 s de batterie
        recAction (e, 1, 0);                         // ferme la boucle
        run (e, 4);
        setPlain (e, 0, "mix", "volume", -96.0f);    // la batterie se tait : seule la boucle reste
        s.setStep (0, 0, 0, 0); s.setStep (0, 0, 4, 0); s.setStep (0, 0, 8, 0); s.setStep (0, 0, 12, 0);
        run (e, 100);
        return run (e, 200).rms;
    };
    const auto chosen = loopPeak (true), notChosen = loopPeak (false);
    std::printf ("  boucle Pickup des pistes choisies : %.4f (batterie cochée) / %.6f (non cochée)\n", chosen, notChosen);
    CHECK (chosen > 0.005 && notChosen < chosen * 0.02);

    // pas d'enregistrement : l'enregistreur part au pas 4
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "octa", 0.0);
        auto& s = e.sequencer();
        s.setStep (0, 0, 4, 100);
        s.setStepFlags (0, 4, stepRec | stepTrigless);
        s.requestPlay();
        float st[48] {};
        run (e, 100); // 25600 < pas 4 (24000) ? non : 24000 est dépassé à 25600
        e.instrumentStatus (0, st, 48);
        CHECK (st[1] == 1.0f); // v0.42 : état de T1 (après la piste jouée)
        CHECK (e.getHitCount (0, 0) == 0); // sans déclenchement : aucune note
    }

    // glissé des verrous : la hauteur verrouillée au pas 1 y arrive progressivement
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "octa", 0.0);
        const auto tune = e.findParam (0, "inst", "t1_tune");
        auto& s = e.sequencer();
        s.setStep (0, 0, 0, 100);
        s.setStep (0, 0, 1, 100);
        e.setStepLock (0, 0, 1, tune, 1.0f, 0.0);
        s.setStepFlags (0, 1, stepSlide);
        s.requestPlay();
        run (e, 35); // 8960 : au milieu du pas 1 (6000 à 12000)
        const auto mid = e.getEffective (tune);
        run (e, 12);  // 12032 : pas 2 commencé, plus de verrou
        std::printf ("  glissé des verrous : %.3f au milieu du pas\n", mid);
        CHECK (mid > 0.6f && mid < 0.9f);
    }
}

// Arrêt du séquenceur : les notes jouées par la séquence s'éteignent (relâchement compris), y compris
// avec l'arpège, des notes très longues et un son tenu (wave morph « Peigne qui tourne »)
static void testStopSilences()
{
    struct Case { const char* type; const char* preset; bool arp; int len8; };
    const Case cases[] { { "morph", "Peigne qui tourne", false, 64 }, { "morph", "Peigne qui tourne", true, 16 }, { "morph", "Peigne qui tourne", false, 1024 },
                         { "pads", "", false, 1024 }, { "mono", "", false, 1024 }, { "poly5", "", true, 32 }, { "hybrid", "", false, 512 } };
    for (const auto& k : cases)
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (4, k.type, 0.0);
        if (*k.preset) e.applyPreset (4, -1, k.preset);
        if (k.arp) setPlain (e, 4, "inst", "arp_mode", 1.0f);
        auto& s = e.sequencer();
        for (int st = 0; st < 16; st += 4) { s.setStep (4, 0, st, 100); s.setNote (4, 0, st, 48 + st); s.setLength8 (4, 0, st, k.len8); }
        s.requestPlay();
        const auto playing = run (e, 300).rms;
        s.requestStop();
        run (e, (int) (16.0 * 48000.0 / 256.0));
        const auto after = run (e, 190).rms;
        std::printf ("  arr\xC3\xAAt : %s %s%s (%d) %.4f -> %.7f\n", k.type, k.preset, k.arp ? " arp\xC3\xA8ge" : "", k.len8, playing, after);
        CHECK (playing > 0.001 && after < 3.0e-4);
    }
}

// v0.16 : morceau — rangées jouées en mode morceau (bascule exacte à la mesure, boucle, fin),
// jeu en direct enregistré en rangées, courbes relues, mouvements enregistrés
static void testSong()
{
    const int barBlocks = 375; // 4 noires à 120 BPM, blocs de 256 à 48 kHz
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        auto& s = e.sequencer();
        s.setStepP (0, 0, 0, 0, 100); s.setStepP (0, 1, 1, 0, 100); // un pas en A (pattern 0) et en B (pattern 1)
        Song sg;
        sg.songMode = true; sg.loop = true;
        SongRow a, b; a.bars = 1; b.bars = 2; a.pat[0] = 0; b.pat[0] = 1; b.pat[1] = songStop;
        sg.rows = { a, b };
        e.setSong (sg, 0.0);
        s.requestPlay();
        run (e, barBlocks / 2);
        const auto p1 = s.getPlaying (0), r1 = e.getSongRow();
        run (e, barBlocks);                        // 1,5 mesure : rangée 2
        const auto p2 = s.getPlaying (0), r2 = e.getSongRow();
        run (e, barBlocks * 2);                    // 3,5 mesures : retour à la rangée 1 (boucle de 3 mesures)
        const auto p3 = s.getPlaying (0), r3 = e.getSongRow();
        std::printf ("  morceau : rang\xC3\xA9" "es %d %d %d, patterns %d %d %d\n", r1, r2, r3, p1, p2, p3);
        CHECK (p1 == 0 && p2 == 1 && p3 == 0 && r1 == 0 && r2 == 1 && r3 == 0);
        // sans boucle : arrêt en fin de morceau
        sg.loop = false;
        e.setSong (sg, 0.0);
        run (e, barBlocks * 4);
        CHECK (! s.isRunning() && s.getPlaying (0) == -1);
    }
    {
        // boucle choisie : rangées 2 et 3 (A 1 mes., B 1 mes., C 1 mes., A 1 mes.) — la rangée 1 n'est
        // jouée qu'une fois, la 4 jamais : 1 → 2 → 3 → 2 → 3…
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        auto& s = e.sequencer();
        for (int p = 0; p < 3; ++p) s.setStepP (0, p, 0, 0, 100);
        Song sg;
        sg.songMode = true; sg.loop = true; sg.loopFrom = 1; sg.loopTo = 2;
        SongRow r0, r1, r2, r3;
        r0.bars = r1.bars = r2.bars = r3.bars = 1;
        r0.pat[0] = 0; r1.pat[0] = 1; r2.pat[0] = 2; r3.pat[0] = 0;
        sg.rows = { r0, r1, r2, r3 };
        e.setSong (sg, 0.0);
        s.requestPlay();
        int seen[6] {};
        run (e, barBlocks / 2);
        for (int k = 0; k < 6; ++k) { seen[k] = e.getSongRow(); run (e, barBlocks); }
        std::printf ("  boucle 2-3 : rang\xC3\xA9" "es %d %d %d %d %d %d\n", seen[0], seen[1], seen[2], seen[3], seen[4], seen[5]);
        CHECK (seen[0] == 0 && seen[1] == 1 && seen[2] == 2 && seen[3] == 1 && seen[4] == 2 && seen[5] == 1);
        CHECK (std::abs (e.getSong().wrap (4.0 * 5.5) - 4.0 * 1.5) < 1.0e-9); // mesure 6,5 → rangée 2, à mi-mesure
        // une seule rangée bouclée
        sg.loopFrom = sg.loopTo = 3;
        CHECK (std::abs (sg.wrap (4.0 * 6.25) - 4.0 * 3.25) < 1.0e-9);
        // bornes : la fin avant le début est corrigée, au-delà du morceau ramenée à la dernière rangée
        sg.loopFrom = 3; sg.loopTo = 1;
        e.setSong (sg, 0.0);
        CHECK (e.getSong().loopFrom == 1 && e.getSong().loopTo == 3);
        sg.loopFrom = 2; sg.loopTo = 90;
        CHECK (sg.loopLast() == 3 && sg.loopFirst() == 2);
    }
    {
        // jeu en direct : A pendant 2 mesures, puis B lancé (à la mesure) pendant 1 mesure
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        auto& s = e.sequencer();
        s.setLaunchQuant (2);
        e.setRecordPlay (true);
        s.requestPlay();
        run (e, 4); e.songTick (0.0);
        run (e, barBlocks + barBlocks / 2); e.songTick (100.0);
        s.launch (0, 1);
        run (e, barBlocks); e.songTick (200.0);
        run (e, barBlocks / 4); e.songTick (300.0);
        const auto& rows = e.getSong().rows;
        std::printf ("  jeu enregistr\xC3\xA9 : %zu rang\xC3\xA9" "es", rows.size());
        for (const auto& r : rows) std::printf (" [%d mes. %d]", r.bars, (int) r.pat[0]);
        std::printf ("\n");
        CHECK (rows.size() == 2 && rows[0].bars == 2 && rows[0].pat[0] == 0 && rows[1].pat[0] == 1);
    }
    {
        // courbe : volume de la piste 1 de 0 (temps 0) à 1 (temps 4) ; à 2 temps, la valeur jouée vaut 0,5
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        const auto g = e.findParam (0, "mix", "volume");
        CHECK (g >= 0);
        Song sg;
        AutoLane lane; lane.global = g; lane.pts = { { 0.0f, 0.0f }, { 4.0f, 1.0f } };
        sg.lanes = { lane };
        e.setSong (sg, 0.0);
        e.sequencer().requestPlay();
        run (e, barBlocks / 2);
        const auto mid = e.getEffective (g);
        // tenu à la main : la courbe se tait, le bouton fait foi
        e.setParam (g, 0.9f); e.recordMoveUi (g, 0.9f);
        run (e, 4);
        const auto held = e.getEffective (g);
        std::printf ("  courbe : %.3f, tenue %.3f\n", mid, held);
        CHECK (std::abs (mid - 0.5f) < 0.03f && std::abs (held - 0.9f) < 1.0e-3f);
    }
    {
        // v0.44.1 : trou gommé dans une courbe — le réglage y reprend sa valeur normale
        AutoLane l; l.pts = { { 0.0f, 0.8f }, { 1.0f, 0.8f }, { 1.0001f, autoGap }, { 3.0f, 0.2f } };
        const auto a = l.valueAt (0.5), b = l.valueAt (2.0), c = l.valueAt (3.5);
        CHECK (std::abs (a - 0.8f) < 1.0e-4f && b < 0.0f && std::abs (c - 0.2f) < 1.0e-4f);
        CHECK (autoNorm (-0.5f) == autoGap && autoNorm (1.5f) == 1.0f);
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        const auto g = e.findParam (0, "mix", "volume");
        e.setParam (g, 0.4f);
        Song sg;
        AutoLane lane; lane.global = g; lane.pts = { { 0.0f, autoGap }, { 64.0f, autoGap } };
        sg.lanes = { lane };
        e.setSong (sg, 0.0);
        CHECK (! e.getSong().lanes.empty() && e.getSong().lanes[0].pts[0].norm < 0.0f); // le trou survit au rangement
        e.sequencer().requestPlay();
        run (e, barBlocks / 2);
        const auto inGap = e.getEffective (g);
        std::printf ("  courbe trou\xC3\xA9" "e : %.3f (valeur normale 0,4)\n", inGap);
        CHECK (std::abs (inGap - 0.4f) < 1.0e-3f);
    }
    {
        // mouvements enregistrés depuis l'écran : une courbe naît, ses points suivent le geste
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        const auto g = e.findParam (0, "mix", "volume");
        e.setRecordMoves (true);
        e.sequencer().requestPlay();
        double ms = 0.0;
        for (int k = 0; k <= 8; ++k)
        {
            run (e, 47);                           // ~1/2 temps
            e.setParam (g, (float) k / 8.0f); e.recordMoveUi (g, (float) k / 8.0f);
            ms += 250.0; e.songTick (ms);
        }
        const auto* lane = e.getSong().laneFor (g);
        std::printf ("  mouvements : %zu points\n", lane != nullptr ? lane->pts.size() : (size_t) 0);
        // une courbe suit son effet déplacé, disparaît avec un module remplacé
        CHECK (e.setInsert (0, 0, "delay", 0.0));
        const auto gfx = e.findParam (0, "fx1", "mix");
        Song s2 = e.getSong();
        AutoLane lfx; lfx.global = gfx; lfx.pts = { { 0.0f, 0.3f } };
        s2.lanes.push_back (lfx);
        e.setSong (s2, 0.0);
        e.moveInsert (0, 0, 2, 0.0);
        const auto moved = e.getSong().laneFor (e.findParam (0, "fx3", "mix")) != nullptr && e.getSong().laneFor (gfx) == nullptr;
        e.setInsert (0, 2, "reverb", 0.0);
        const auto dropped = e.getSong().lanes.size() == 1 && e.getSong().laneFor (g) != nullptr;
        e.setInstrument (0, "acid", 0.0);
        CHECK (moved && dropped && e.getSong().laneFor (g) != nullptr); // la courbe du volume (mixeur) reste
        CHECK (lane != nullptr && lane->pts.size() == 9 && std::abs (lane->pts.back().norm - 1.0f) < 1.0e-4f
               && lane->pts.front().beat < lane->pts.back().beat);
    }
}

// v0.16 : enregistrement en direct — une note tenue garde sa durée (relâchement), batterie et mélodie
static void testRecordHeldNotes()
{
    for (const bool drum : { false, true })
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (3, drum ? "sp" : "mono", 0.0);
        auto& s = e.sequencer();
        s.setRecording (true);
        s.requestPlay();
        run (e, 4);
        const int note = drum ? 5 : 60;
        e.pushEvent ({ InputEvent::noteOn, 3, note, 0, 0.9f, true });
        run (e, 94);                                   // ~4 pas de double croche à 120 BPM
        e.pushEvent ({ InputEvent::noteOff, 3, note, 0, 0.0f, false });
        run (e, 4);
        int len8 = 0;
        for (int l = 0; l < Sequencer::numLanes && len8 == 0; ++l)
            for (int st = 0; st < 4 && len8 == 0; ++st)
                if (s.getStepP (3, 0, l, st) > 0) len8 = s.getLength8P (3, 0, l, st);
        std::printf ("  note tenue enregistr\xC3\xA9" "e (%s) : %.2f pas\n", drum ? "pads" : "clavier", len8 / 8.0);
        CHECK (len8 >= 28 && len8 <= 36);
    }
}

// v0.16 : notes exactes allumées (clavier, pads mélodiques), arpège et mouvements enregistrés,
// arrêt du séquenceur qui fait taire une boucle sans fin de l'Octatrack
static void testLitNotesArpRecordAndLocks()
{
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (2, "mono", 0.0);
        const auto h0 = e.getNoteHits (2, 36);
        e.pushEvent ({ InputEvent::noteOn, 2, 36, 0, 0.8f, false });
        run (e, 2);
        const bool down = e.isNoteDown (2, 36) && ! e.isNoteDown (2, 48) && e.getNoteHits (2, 36) == h0 + 1;
        e.pushEvent ({ InputEvent::noteOff, 2, 36, 0, 0.0f, false });
        run (e, 2);
        std::printf ("  note exacte allum\xC3\xA9" "e : %s puis %s\n", down ? "oui" : "non", e.isNoteDown (2, 36) ? "tenue" : "\xC3\xA9teinte");
        CHECK (down && ! e.isNoteDown (2, 36));
    }
    {
        // arpège : ses notes s'écrivent (plusieurs pas), marquées « sans arpège » à la relecture
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (3, "mono", 0.0);
        setPlain (e, 3, "inst", "arp_mode", 1.0f);
        auto& s = e.sequencer();
        s.setRecording (true);
        s.requestPlay();
        run (e, 4);
        e.pushEvent ({ InputEvent::noteOn, 3, 48, 0, 0.9f, true });
        e.pushEvent ({ InputEvent::noteOn, 3, 55, 0, 0.9f, true });
        run (e, 180);                                   // ~1 mesure à 120 BPM
        e.pushEvent ({ InputEvent::noteOff, 3, 48, 0, 0.0f, false });
        e.pushEvent ({ InputEvent::noteOff, 3, 55, 0, 0.0f, false });
        run (e, 4);
        e.songTick (1000.0);
        int notes = 0, direct = 0;
        for (int l = 0; l < Sequencer::numLanes; ++l)
            for (int st = 0; st < 16; ++st)
                if (s.getStepP (3, 0, l, st) > 0)
                {
                    ++notes;
                    if (const auto* x = e.getExtras().find (3, 0, l, st); x != nullptr && x->direct) ++direct;
                }
        std::printf ("  arp\xC3\xA8ge enregistr\xC3\xA9 : %d notes, %d rejou\xC3\xA9" "es sans arp\xC3\xA8ge\n", notes, direct);
        CHECK (notes >= 6 && direct == notes);
    }
    {
        // mouvement enregistré avec ENR. : une courbe lisse du pattern, relue en continu à chaque tour
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (1, "mono", 0.0);
        auto& s = e.sequencer();
        for (int st = 0; st < 16; ++st) { s.setStep (1, 0, st, 100); s.setNote (1, 0, st, 48); }
        const auto cut = e.findParam (1, "inst", "cutoff");
        CHECK (cut >= 0);
        s.setRecording (true);
        s.requestPlay();
        run (e, 30);
        for (int i = 0; i <= 40; ++i)                  // montée de 0 à 1 sur ~7 pas, un geste toutes les 21 ms
        {
            e.recordMoveUi (cut, (float) i / 40.0f);
            run (e, 4);
            e.songTick (1000.0 + i * 21.0);
        }
        s.setRecording (false);
        const auto li = e.getSong().patLaneIndex (1, 0, cut);
        CHECK (li >= 0);
        if (li < 0) return;
        const auto& lane = e.getSong().patLanes[(size_t) li].lane;
        bool rising = true;
        for (size_t k = 1; k < lane.pts.size(); ++k) rising = rising && lane.pts[k].norm >= lane.pts[k - 1].norm && lane.pts[k].beat > lane.pts[k - 1].beat;
        const auto first = lane.pts.front().beat, last = lane.pts.back().beat;
        // relecture au tour suivant : au milieu de la montée, plusieurs valeurs différentes dans un même pas
        run (e, 375 - 30 - 41 * 4 + 30 + 4 * 6);       // fin du 1er tour, puis milieu de la montée
        std::vector<float> seen;
        for (int b = 0; b < 20; ++b) { run (e, 1); seen.push_back (e.getEffective (cut)); }
        int distinct = 1;
        for (size_t k = 1; k < seen.size(); ++k) distinct += std::abs (seen[k] - seen[k - 1]) > 1.0e-5f ? 1 : 0;
        std::printf ("  mouvement enregistr\xC3\xA9 : courbe de %zu points (pas %.2f \xC3\xA0 %.2f), relue en continu : %d valeurs sur 20 blocs (%.3f \xE2\x86\x92 %.3f)\n",
                     lane.pts.size(), (double) first, (double) last, distinct, (double) seen.front(), (double) seen.back());
        CHECK (lane.pts.size() >= 20 && rising && last - first > 5.0f && distinct >= 10 && seen.back() > seen.front());
        // sans ENR. : rien n'est écrit dans le pattern
        const auto count = e.getSong().patLanes.size();
        e.recordMoveUi (e.findParam (1, "inst", "reso"), 0.75f);
        e.songTick (3000.0);
        CHECK (e.getSong().patLanes.size() == count);
        // copie vers une autre piste : même motif ; la courbe suit le même réglage de l'autre instrument
        e.setInstrument (5, "mono", 0.0);
        s.copyPattern (1, 0, 5, 2);
        e.copyPatternExtras (1, 0, 5, 2, 4000.0);
        e.copyPatternLanes (1, 0, 5, 2, 4000.0);
        const auto cut5 = e.findParam (5, "inst", "cutoff");
        const bool moved = e.getSong().patLaneIndex (5, 2, cut5) >= 0;
        std::printf ("  pattern copi\xC3\xA9 vers une autre piste : notes %s, courbe %s\n", s.getStepP (5, 2, 0, 3) > 0 ? "oui" : "non", moved ? "suivie" : "perdue");
        CHECK (s.getStepP (5, 2, 0, 3) > 0 && s.getNoteP (5, 2, 0, 3) == 48 && moved);
        // pattern effacé : ses courbes aussi
        e.clearPatternLanes (5, 2, 5000.0);
        CHECK (e.getSong().patLaneIndex (5, 2, cut5) < 0 && e.getSong().patLaneIndex (1, 0, cut) >= 0);
    }
}

// v0.15 : coupure et solo font taire le son de la piste (note tenue, traîne d'écho), en douceur
// v0.18.1 : égaliseur 3 bandes de la tranche de mixage
static void testChannelEq()
{
    auto level = [] (float lo, float mid, float hi)
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (4, "pads", 0.0);
        setPlain (e, 4, "mix", "eqlow", lo); setPlain (e, 4, "mix", "eqmid", mid); setPlain (e, 4, "mix", "eqhigh", hi);
        auto& s = e.sequencer();
        s.setStep (4, 0, 0, 100); s.setNote (4, 0, 0, 48); s.setLength8 (4, 0, 0, 1024);
        s.requestPlay();
        run (e, 100);
        return run (e, 200).rms;
    };
    const auto flat = level (0.0f, 0.0f, 0.0f), cut = level (-15.0f, -15.0f, -15.0f), boost = level (0.0f, 9.0f, 0.0f);
    const auto lowOnly = level (-15.0f, 0.0f, 0.0f);
    std::printf ("  \xC3\xA9galiseur de tranche : \xC3\xA0 plat %.4f, tout \xE2\x88\x92" "15 dB %.4f, m\xC3\xA9" "diums +9 dB %.4f, graves \xE2\x88\x92" "15 dB %.4f\n", flat, cut, boost, lowOnly);
    CHECK (flat > 0.001 && cut < flat * 0.7 && boost > flat * 1.1 && lowOnly < flat * 0.9 && lowOnly > cut);
    // positions : les 4 réglages tiennent dans la zone de mixage, avant l'instrument
    Engine e;
    e.prepare (48000.0, 256);
    const auto g = e.findParam (0, "mix", "eqhigh");
    CHECK (g >= 0 && localOf (g) == mixEqHigh && mixEqHigh < instrumentBase);
}

static void testMuteSilencesAudio()
{
    for (const bool solo : { false, true })
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (4, "pads", 0.0);
        CHECK (e.setInsert (4, 0, "delay", 0.0));
        setPlain (e, 4, "fx1", "feedback", 90.0f);
        auto& s = e.sequencer();
        s.setStep (4, 0, 0, 100); s.setNote (4, 0, 0, 48); s.setLength8 (4, 0, 0, 1024);
        s.requestPlay();
        const auto before = run (e, 200).rms;
        if (solo) { e.setInstrument (5, "mono", 0.0); s.setTrackSolo (5, true); }
        else s.setTrackMute (4, true);
        run (e, 4);                                   // rampe de 6 ms
        const auto muted = run (e, 100).rms;
        if (solo) s.setTrackSolo (5, false); else s.setTrackMute (4, false);
        run (e, 4);
        const auto back = run (e, 100).rms;
        std::printf ("  coupure%s : %.4f -> %.7f -> %.4f\n", solo ? " (solo d'une autre piste)" : "", before, muted, back);
        CHECK (before > 0.001 && muted < 1.0e-6 && back > 0.001);
    }
    // réglage « laisser les traînes » : la note tenue continue, seules les nouvelles notes s'arrêtent
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (4, "pads", 0.0);
    auto& s = e.sequencer();
    s.setStep (4, 0, 0, 100); s.setNote (4, 0, 0, 48); s.setLength8 (4, 0, 0, 1024);
    s.setMuteTails (true);
    s.requestPlay();
    run (e, 200);
    s.setTrackMute (4, true);
    run (e, 4);
    const auto tails = run (e, 100).rms;
    std::printf ("  coupure en laissant les tra\xC3\xAEnes : %.4f\n", tails);
    CHECK (tails > 0.001);

    // v0.43 : coupée AU MOMENT où la note part, la note joue quand même (son coupé) et revient dès qu'on rétablit
    Engine e2;
    e2.prepare (48000.0, 256);
    e2.setInstrument (4, "pads", 0.0);
    auto& s2 = e2.sequencer();
    s2.setStep (4, 0, 0, 100); s2.setNote (4, 0, 0, 48); s2.setLength8 (4, 0, 0, 1024);
    s2.setTrackMute (4, true);
    run (e2, 4);                                      // coupure déjà en place (rampe de 6 ms faite)
    s2.requestPlay();
    const auto silent = run (e2, 100).rms;
    s2.setTrackMute (4, false);
    run (e2, 4);
    const auto revived = run (e2, 100).rms;
    std::printf ("  coup\xC3\xA9" "e au d\xC3\xA9" "part puis r\xC3\xA9" "tablie : %.7f -> %.4f\n", silent, revived);
    CHECK (silent < 1.0e-6 && revived > 0.001);
}

// v0.13.1 : inserts par pad — un écho sur la grosse caisse seule ; contourné, il se tait ; un
// autre instrument les retire
static void testPadInserts()
{
    auto tailRms = [] (bool withFx, bool bypass)
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        if (withFx)
        {
            CHECK (e.setPadInsert (0, 0, 1, "delay", 0.0));
            setPlain (e, 0, "pfx1_2", "mix", 100.0f);
            setPlain (e, 0, "pfx1_2", "feedback", 60.0f);
            setPlain (e, 0, "pfx1_2", "sync", 0.0f);
            setPlain (e, 0, "pfx1_2", "time", 300.0f);
            if (bypass) { const auto g = e.findParam (0, "padbyp", "b1_2"); CHECK (g >= 0); e.setParam (g, 1.0f); }
        }
        auto& s = e.sequencer();
        s.setStep (0, 0, 0, 110);
        s.requestPlay();
        run (e, 20);
        s.requestStop();
        run (e, 150);               // ~0,8 s : la grosse caisse s'est éteinte
        return run (e, 100).rms;    // ce qui reste : les répétitions de l'écho
    };
    const auto dry = tailRms (false, false), wet = tailRms (true, false), byp = tailRms (true, true);
    std::printf ("  insert de pad (\xC3\xA9" "cho sur la grosse caisse) : sans %.6f, avec %.5f, contourn\xC3\xA9 %.6f\n", dry, wet, byp);
    CHECK (wet > 0.002 && wet > dry * 20.0 && byp < wet * 0.05);

    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    CHECK (e.setPadInsert (0, 3, 0, "atmos", 0.0) && e.getPadInsertType (0, 3, 0) == "atmos" && e.findParam (0, "pfx4_1", "decay") >= 0);
    CHECK (! e.setPadInsert (1, 0, 0, "delay", 0.0) || e.instrumentDesc (1) != nullptr); // piste vide : refusé
    e.setInstrument (0, "mono", 0.0);
    CHECK (e.getPadInsertType (0, 3, 0).empty() && e.findParam (0, "pfx4_1", "decay") < 0);
    CHECK (! e.setPadInsert (0, 0, 0, "delay", 0.0)); // pas de pads sur un synthé
}

static void testCopyStepRange31()
{
    // v0.31 : Dupliquer ×2 recopie aussi conditions, verrous (et le pas de destination est remplacé)
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    const auto g = e.findParam (0, "inst", "p03_tune");
    e.sequencer().setStep (0, 1, 3, 100);
    e.setStepExtra (0, 1, 3, Condition::fill, 1, 2, 70, 0.0);
    e.setStepLock (0, 1, 3, g, 0.6f, 0.0);
    e.sequencer().setStep (0, 2, 20, 100);
    e.setStepExtra (0, 2, 20, Condition::notFill, 1, 2, 30, 0.0);   // ancien contenu de la destination
    e.copyStepRangeExtras (0, 0, 0, 16, 16, 0.0);
    const auto* x = e.getExtras().find (0, 0, 1, 19);
    CHECK (x != nullptr && x->cond == Condition::fill && x->prob == 70 && x->locks.size() == 1 && x->locks[0].first == g);
    CHECK (e.getExtras().find (0, 0, 2, 20) == nullptr);
    CHECK (e.getExtras().find (0, 0, 1, 3) != nullptr);                  // l'original reste
}
static void testAudioTap32()
{
    // v0.32 : la prise de son reçoit la sortie principale telle quelle, et les pistes quand on les demande
    struct Tap final : AudioTap
    {
        bool stemsWanted = false;
        long long samples = 0; double diff = 0.0, trackEnergy = 0.0; std::uint32_t mask = 0;
        float* outL = nullptr; int pos = 0;
        bool wantsStems() const noexcept override { return stemsWanted; }
        void tapBlock (const float* l, const float*, const float* const* tl, const float* const*, std::uint32_t m,
                       const float* const*, const float* const*, int n) noexcept override
        {
            for (int i = 0; i < n; ++i) diff += std::abs (l[i] - outL[pos + i]);
            if (m & 1u) for (int i = 0; i < n; ++i) trackEnergy += tl[0][i] * tl[0][i];
            mask |= m; samples += n; pos += n;
        }
    } tap;
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    for (int i = 0; i < 16; i += 4) e.sequencer().setStep (0, 0, i, 120);
    e.sequencer().requestPlay();
    e.audioTap.store (&tap);
    std::vector<float> l (256), r (256);
    Engine::BlockInfo bi; bi.bpm = 120.0;
    for (int b = 0; b < 40; ++b)
    {
        tap.outL = l.data(); tap.pos = 0; tap.stemsWanted = b >= 20;
        e.process (nullptr, nullptr, l.data(), r.data(), 256, bi, nullptr, 0);
    }
    e.audioTap.store (nullptr);
    CHECK (tap.samples == 40 * 256);
    CHECK (tap.diff < 1.0e-9);                 // la sortie principale est exactement celle écrite
    CHECK ((tap.mask & 1u) != 0 && tap.trackEnergy > 0.0); // la piste 1 arrive quand on la demande
}
static void testBusMuteSolo31()
{
    // v0.31 : coupure et solo d'un groupe, coupure d'un retour
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    e.setInstrument (1, "drumA", 0.0);
    e.setTrackOutput (0, 1);
    for (int i = 0; i < 16; i += 2) { e.sequencer().setStep (0, 0, i, 120); e.sequencer().setStep (1, 0, i, 120); }
    e.sequencer().requestPlay();
    const auto both = run (e, 100);
    // seule la piste 2 (sur le principal) : le groupe 1 coupé
    e.setBusMute (groupBase, true);
    run (e, 8);
    e.setInstrument (1, "", 0.0);
    run (e, 8);
    const auto muted = run (e, 100);
    e.setBusMute (groupBase, false);
    run (e, 8);
    const auto reopened = run (e, 100);
    std::printf ("  coupure du groupe 1 : ouvert %.3f, coup\xC3\xA9 %.6f, rouvert %.3f\n", both.peak, muted.peak, reopened.peak);
    CHECK (both.peak > 0.05f && muted.peak < 0.001f && reopened.peak > 0.05f);
    // solo du groupe 1 : une piste sur le principal se tait, celle du groupe reste
    e.setInstrument (1, "drumA", 0.0);
    for (int i = 0; i < 16; i += 2) e.sequencer().setStep (1, 0, i, 120);
    e.setTrackOutput (0, 0);
    e.setTrackOutput (2, 0);
    e.setBusSolo (groupBase, true);
    run (e, 8);
    const auto soloEmpty = run (e, 100); // aucune piste dans le groupe : tout se tait
    e.setTrackOutput (1, 1);
    run (e, 8);
    const auto soloGroup = run (e, 100); // la piste 2 passe dans le groupe : elle revient
    e.setBusSolo (groupBase, false);
    std::printf ("  solo du groupe 1 : groupe vide %.6f, avec la piste 2 %.3f\n", soloEmpty.peak, soloGroup.peak);
    CHECK (soloEmpty.peak < 0.001f && soloGroup.peak > 0.05f);
    CHECK (! e.getBusSolo (numTracks) && (e.setBusSolo (numTracks, true), ! e.getBusSolo (numTracks))); // pas de solo sur un retour
    CHECK ((e.setBusMute (masterChannel, true), ! e.getBusMute (masterChannel)));                    // ni de coupure du principal
}

static void testGroups24()
{
    // Bus de groupe : une piste envoyée au groupe 1 suit le fader du groupe ; le groupe a ses inserts
    // et ses envois ; le réglage est enregistré avec la piste
    auto build = [] (Engine& e, int out)
    {
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        e.setTrackOutput (0, out);
        for (int i = 0; i < 16; i += 2) e.sequencer().setStep (0, 0, i, 120);
        e.sequencer().requestPlay();
    };
    CHECK (isGroupChannel (groupBase) && isGroupChannel (groupBase + numGroups - 1) && ! isGroupChannel (masterChannel));
    {
        Engine e; build (e, 1);
        CHECK (e.getTrackOutput (0) == 1 && e.getTrackInfo (0).output == 1);
        const auto a = run (e, 100);
        setPlain (e, groupBase, "mix", "volume", -60.0f);
        run (e, 4);
        const auto b = run (e, 100);
        std::printf ("  bus de groupe : groupe ouvert %.3f, groupe ferm\xC3\xA9 %.6f\n", a.peak, b.peak);
        CHECK (a.finite && a.peak > 0.05f && b.peak < 0.001f);
        // une piste restée sur le principal ne dépend pas du groupe
        e.setTrackOutput (0, 0);
        run (e, 4);
        CHECK (run (e, 100).peak > 0.05f);
        e.setTrackOutput (0, 99);
        CHECK (e.getTrackOutput (0) == numGroups);
        e.setTrackOutput (0, -3);
        CHECK (e.getTrackOutput (0) == 0);
    }
    {
        // insert sur le groupe : un filtre passe-bas change le signal
        Engine e; build (e, 2);
        const auto dry = run (e, 120);
        Engine e2; build (e2, 2);
        CHECK (e2.setInsert (groupBase + 1, 0, "filter", 0.0));
        setPlain (e2, groupBase + 1, "fx1", "cutoff", 300.0f);
        const auto wet = run (e2, 120);
        std::printf ("  insert du groupe 2 : sans %.4f, avec %.4f\n", dry.rms, wet.rms);
        CHECK (wet.finite && std::abs (wet.rms - dry.rms) > dry.rms * 0.02);
    }
    {
        // envoi du groupe vers le retour A (réverbération), fader du groupe fermé, envoi avant le fader
        Engine e; build (e, 3);
        e.setInsert (numTracks + 0, 0, "reverb", 0.0);
        setPlain (e, groupBase + 2, "mix", "volume", -60.0f);
        setPlain (e, groupBase + 2, "mix", "sendA", 100.0f);
        setPlain (e, groupBase + 2, "mix", "sendPre", 1.0f);
        const auto r = run (e, 200);
        std::printf ("  envoi du groupe 3 vers le retour A : cr\xC3\xAAte %.3f\n", r.peak);
        CHECK (r.finite && r.peak > 0.005f);
    }
    {
        // le volume du groupe baisse aussi les envois de ses pistes : groupe fermé, plus de réverbération
        Engine e; build (e, 4);
        e.setInsert (numTracks + 0, 0, "reverb", 0.0);
        setPlain (e, 0, "mix", "sendA", 100.0f);
        const auto open = run (e, 120);
        setPlain (e, groupBase + 3, "mix", "volume", -60.0f);
        run (e, 400); // la traîne de la réverbération s'éteint
        const auto shut = run (e, 120);
        std::printf ("  envois d'une piste du groupe 4 : groupe ouvert %.3f, ferm\xC3\xA9 %.6f\n", open.peak, shut.peak);
        CHECK (open.peak > 0.05f && shut.peak < 0.001f);
        // piste libérée : elle revient sur le bus principal
        e.setInstrument (0, "", 0.0);
        CHECK (e.getTrackOutput (0) == 0);
    }
}

static void testPadSidechain24()
{
    // Compresseur sur le pad 1 de la piste 1, déclenché par la piste 2 (fader fermé : on n'entend que la piste 1)
    auto level = [] (bool trigger)
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        e.setInstrument (1, "drumA", 0.0);
        CHECK (e.setPadInsert (0, 0, 0, "comp", 0.0));
        setPlain (e, 0, "pfx1_1", "scsrc", 2.0f);       // « Piste 2 »
        setPlain (e, 0, "pfx1_1", "threshold", -40.0f);
        setPlain (e, 1, "mix", "volume", -60.0f);
        for (int i = 0; i < 16; ++i) e.sequencer().setStep (0, 0, i, 100);
        if (trigger) for (int i = 0; i < 16; ++i) e.sequencer().setStep (1, 0, i, 127);
        e.sequencer().requestPlay();
        run (e, 60);
        return run (e, 200).rms;
    };
    const auto free = level (false), ducked = level (true);
    std::printf ("  side-chain sur un insert de pad : seul %.4f, avec la piste 2 %.4f\n", free, ducked);
    CHECK (free > 0.005 && ducked < free * 0.8);
}

static void testSongLoopForever()
{
    // Retour d'Henrico : en boucle, le morceau s'arrêtait seul au bout d'un moment. On joue
    // longtemps à des tempos, fréquences et tailles de bloc « tordus » : il doit tourner sans fin.
    struct Case { double sr; int block; double bpm; };
    for (const auto c : { Case { 44100.0, 441, 122.0 }, Case { 48000.0, 256, 97.3 }, Case { 44100.0, 512, 133.33 }, Case { 48000.0, 480, 174.0 } })
    {
        Engine e;
        e.prepare (c.sr, c.block);
        e.setInstrument (0, "drumA", 0.0);
        auto& s = e.sequencer();
        s.setStepP (0, 0, 0, 0, 100); s.setStepP (0, 1, 0, 0, 100);
        Song sg;
        sg.songMode = true; sg.loop = true;
        SongRow a, b; a.bars = 1; b.bars = 3; a.pat[0] = 0; b.pat[0] = 1;
        sg.rows = { a, b };
        e.setSong (sg, 0.0);
        Engine::BlockInfo bi; bi.bpm = c.bpm;
        s.requestPlay();
        const int blocks = (int) (c.sr * 60.0 * 12.0 / c.block); // 12 minutes
        std::vector<float> L ((size_t) c.block), R ((size_t) c.block);
        int stoppedAt = -1;
        for (int b2 = 0; b2 < blocks && stoppedAt < 0; ++b2)
        {
            e.process (nullptr, nullptr, L.data(), R.data(), c.block, bi, nullptr, 0);
            if (b2 > 10 && (! s.isRunning() || e.getSongRow() < 0)) stoppedAt = b2;
        }
        std::printf ("  morceau en boucle %.0f Hz, blocs de %d, %.2f BPM : %s\n", c.sr, c.block, c.bpm,
                     stoppedAt < 0 ? "12 min sans arr\xC3\xAAt" : ("arr\xC3\xAAt\xC3\xA9 au bloc " + std::to_string (stoppedAt)).c_str());
        CHECK (stoppedAt < 0);
    }

    {
        // nombre de passages : intro (rangée 1) une fois, boucle (rangées 2-3) 2 fois, fin (rangée 4), arrêt
        Engine e;
        e.prepare (48000.0, 480);
        e.setInstrument (0, "drumA", 0.0);
        auto& s = e.sequencer();
        for (int p = 0; p < 4; ++p) s.setStepP (0, p, 0, 0, 100);
        Song sg;
        sg.songMode = true; sg.loop = true; sg.loopFrom = 1; sg.loopTo = 2; sg.loopCount = 2;
        SongRow r[4];
        for (int i = 0; i < 4; ++i) { r[i].bars = 1; r[i].pat[0] = i; }
        sg.rows = { r[0], r[1], r[2], r[3] };
        e.setSong (sg, 0.0);
        Engine::BlockInfo bi; bi.bpm = 120.0; // 1 mesure = 2 s = 200 blocs de 480
        s.requestPlay();
        std::vector<float> L (480), R (480);
        std::string order;
        int lastRow = -2;
        for (int b2 = 0; b2 < 200 * 9; ++b2)
        {
            e.process (nullptr, nullptr, L.data(), R.data(), 480, bi, nullptr, 0);
            const auto row = e.getSongRow();
            if (row != lastRow) { order += row < 0 ? std::string ("-") : std::to_string (row + 1); lastRow = row; }
        }
        std::printf ("  morceau, boucle 2-3 jou\xC3\xA9" "e 2 fois : rang\xC3\xA9" "es %s, lecture %s\n", order.c_str(), s.isRunning() ? "en cours" : "arr\xC3\xAAt\xC3\xA9" "e");
        CHECK (order == "12323" "4-" && ! s.isRunning());
    }
}

static void testVocoderInEngine()
{
    // Vocodeur (v0.25) sur la piste 2, la batterie de la piste 1 comme modulateur : retirée du mixage
    // ou entendue selon le réglage ; un son d'usine garde la source choisie
    auto peakOf = [] (bool removed)
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "drumA", 0.0);
        e.setInstrument (1, "voc16", 0.0);
        setPlain (e, 1, "inst", "src", 2.0f);              // piste précédente
        setPlain (e, 1, "inst", "srcmute", removed ? 1.0f : 0.0f);
        setPlain (e, 1, "mix", "volume", -60.0f);          // vocodeur coupé : on n'entend que ce qui passe à côté
        for (int i = 0; i < 16; i += 2) e.sequencer().setStep (0, 0, i, 120);
        e.sequencer().requestPlay();
        run (e, 4);
        return run (e, 200).peak;
    };
    const auto removed = peakOf (true), heard = peakOf (false);
    {
        // vocodeur en solo : sa voix (piste précédente, retirée du mixage) continue de jouer
        auto level = [] (bool solo)
        {
            Engine e;
            e.prepare (48000.0, 256);
            e.setInstrument (0, "drumA", 0.0);
            e.setInstrument (1, "voc16", 0.0);
            e.setInstrument (2, "drumA", 0.0);
            setPlain (e, 1, "inst", "src", 2.0f);
            for (int i = 0; i < 16; i += 2) { e.sequencer().setStep (0, 0, i, 120); e.sequencer().setStep (2, 0, i, 120); }
            for (int i = 0; i < 16; i += 4) { e.sequencer().setStep (1, 0, i, 110); e.sequencer().setNote (1, 0, i, 48); }
            setPlain (e, 2, "mix", "volume", -60.0f);
            if (solo) e.sequencer().setTrackSolo (1, true);
            e.sequencer().requestPlay();
            run (e, 8);
            return run (e, 200).peak;
        };
        const auto normal = level (false), soloed = level (true);
        std::printf ("  vocodeur en solo : %.3f (sans solo %.3f)\n", soloed, normal);
        CHECK (soloed > 0.3f * normal && normal > 0.01f);
    }
    {
        // une piste choisie comme sa propre voix : silence, jamais de boucle qui s'emballe
        Engine e;
        e.prepare (48000.0, 256);
        e.setInstrument (0, "vocspec", 0.0);
        setPlain (e, 0, "inst", "src", 3.0f);       // piste 1 = elle-même
        setPlain (e, 0, "inst", "dry", 100.0f);
        setPlain (e, 0, "inst", "level", 6.0f);
        for (int i = 0; i < 16; i += 4) { e.sequencer().setStep (0, 0, i, 110); e.sequencer().setNote (0, 0, i, 48); }
        e.sequencer().requestPlay();
        const auto r = run (e, 400);
        std::printf ("  vocodeur sur sa propre piste : cr\xC3\xAAte %.5f\n", r.peak);
        CHECK (r.finite && r.peak < 0.01f);
    }
    std::printf ("  vocodeur, batterie en modulateur : retir\xC3\xA9" "e %.5f, entendue %.3f\n", removed, heard);
    CHECK (removed < 1.0e-4f && heard > 0.05f);

    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "vocspec", 0.0);
    setPlain (e, 0, "inst", "src", 5.0f);                  // piste 3
    setPlain (e, 0, "inst", "srcgain", 6.0f);
    CHECK (e.applyPreset (0, -1, "Cristal"));
    const auto g = e.findParam (0, "inst", "src"), gg = e.findParam (0, "inst", "srcgain"), ga = e.findParam (0, "inst", "algo");
    CHECK (std::lround (e.describe (g)->toPlain (e.getParam (g))) == 5);
    CHECK (std::abs (e.describe (gg)->toPlain (e.getParam (gg)) - 6.0f) < 0.05f);
    CHECK (std::lround (e.describe (ga)->toPlain (e.getParam (ga))) == 6);
    // vocodeur joué au séquenceur, voix interne : il parle
    setPlain (e, 0, "inst", "src", 0.0f);
    e.sequencer().setStep (0, 0, 0, 100);
    e.sequencer().setNote (0, 0, 0, 48);
    e.sequencer().requestPlay();
    const auto r = run (e, 200);
    std::printf ("  vocodeur au s\xC3\xA9quenceur : cr\xC3\xAAte %.3f\n", r.peak);
    CHECK (r.finite && r.peak > 0.01f);
}

static void testMoveKeepsState26()
{
    // v0.26 : une piste déplacée garde son instrument (note tenue, boucles), sa position de lecture,
    // ses réglages ; la note se relâche bien à sa nouvelle place
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "mono", 0.0);
    setPlain (e, 0, "mix", "volume", -4.0f);
    auto& s = e.sequencer();
    s.setStep (0, 0, 0, 110);
    s.setNote (0, 0, 0, 48);
    s.setLength8 (0, 0, 0, 8 * 12);         // note tenue 12 pas (1,5 s à 120 BPM)
    s.requestPlay();
    run (e, 40);                            // la note sonne
    const auto* instBefore = e.instrumentDesc (0);
    std::array<int, numTracks> idx {};
    for (int t = 0; t < numTracks; ++t) idx[(size_t) t] = t;
    idx[0] = 3; idx[3] = 0;
    CHECK (e.permuteTracks (idx, 0.0));
    const auto during = run (e, 20);        // juste après le déplacement : la note continue
    CHECK (e.instrumentDesc (3) == instBefore && e.instrumentDesc (0) == nullptr);
    const auto gv = e.findParam (3, "mix", "volume");
    CHECK (gv >= 0 && std::abs (e.describe (gv)->toPlain (e.getParam (gv)) + 4.0f) < 0.05f);
    CHECK (s.getStep (3, 0, 0) == 110 && s.getStep (0, 0, 0) == 0);
    s.requestStop();
    run (e, 4);
    run (e, 600);                           // (relâchement long de la basse)
    const auto after = run (e, 300);        // relâchée, éteinte
    std::printf ("  piste d\xC3\xA9plac\xC3\xA9" "e en jouant : pendant %.3f, apr\xC3\xA8s l'arr\xC3\xAAt %.6f\n", during.peak, after.peak);
    CHECK (during.peak > 0.02f && after.peak < 0.001f);
    std::array<int, numTracks> bad {};
    CHECK (! e.permuteTracks (bad, 0.0));   // pas une permutation : refusé
}

static void testDrumAccent()
{
    // Accent des batteries : niveau de l'impulsion de déclenchement (bus d'accent) et réaction de
    // chaque circuit (attaque de la grosse caisse, part du bruit de la caisse claire)
    using namespace grv::instruments;
    struct Kit { const ModuleDesc& (*desc)(); const std::array<PadDef, 16>& (*pads)(); const char* name; };
    const Kit kits[] { { kitAnalogDesc, kitAnalogPads, "analogique" }, { kit80Desc, kit80Pads, "80" }, { kit90Desc, kit90Pads, "90" } };
    struct Hit { double peak = 0.0, energy = 0.0, hf = 0.0, early = 0.0; };
    auto play = [] (const Kit& k, int pad, float accent)
    {
        DrumKit kit (k.desc(), k.pads());
        kit.prepare (48000.0, 64);
        const auto& d = k.desc();
        std::vector<float> p (d.params.size());
        for (size_t i = 0; i < p.size(); ++i) p[i] = d.params[i].def;
        std::vector<float> L (64), R (64);
        Hit h;
        float prev = 0.0f;
        for (int b = 0; b < 24000 / 64; ++b)
        {
            if (b == 0) { kit.setNextAccent (accent); kit.noteOn (pad, 0.8f); kit.setNextAccent (0.0f); }
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            kit.render (L.data(), R.data(), 64, p.data());
            for (int i = 0; i < 64; ++i)
            {
                const double x = L[(size_t) i];
                h.peak = std::max (h.peak, std::abs (x));
                h.energy += x * x;
                h.hf += (x - prev) * (x - prev);
                if (b * 64 + i < 144) h.early += (x - prev) * (x - prev); // 3 premières ms : le front
                prev = (float) x;
            }
        }
        return h;
    };
    for (const auto& k : kits)
        for (int pad : { 0, 1, 4 })
        {
            const auto n = play (k, pad, 0.0f), a = play (k, pad, 0.5f);
            const auto db = 20.0 * std::log10 (a.peak / std::max (1.0e-9, n.peak));
            std::printf ("  accent %s, pad %d : %+.1f dB\n", k.name, pad + 1, db);
            CHECK (n.peak > 0.0 && db > 2.5 && db < 10.0);
        }
    // caisse claire : le bruit (aigus) prend plus de place sur un coup accentué
    for (const auto& k : kits)
    {
        const auto n = play (k, 1, 0.0f), a = play (k, 1, 1.0f);
        CHECK (a.hf / a.energy > n.hf / n.energy * 1.03);
    }
    // grosse caisse balayée (batterie 90) : attaque plus marquée (le front pèse plus dans le coup)
    {
        const auto n = play (kits[2], 0, 0.0f), a = play (kits[2], 0, 1.0f);
        CHECK (a.early / a.energy > n.early / n.energy * 1.05);
    }

    // Dans le moteur : un pas accentué du séquenceur sonne plus fort, le suivant revient au niveau normal
    Engine e;
    e.prepare (48000.0, 256);
    CHECK (e.setInstrument (0, "drum80", 0.0));
    auto& s = e.sequencer();
    s.setStep (0, 0, 0, 100);
    s.setStep (0, 0, 8, 100);
    s.setStep (0, 0, 16 + 8, 100);
    s.setStepFlags (0, 0, stepAccent);
    s.setAccentAmount (0, 100);
    s.requestPlay();
    const auto first = run (e, 187);  // pas 0 (accentué) : 0 à 47 872
    const auto second = run (e, 187); // pas 8 : 48 000
    std::printf ("  accent dans le moteur : %.2f contre %.2f\n", first.peak, second.peak);
    CHECK (first.finite && second.finite && first.peak > second.peak * 1.8f && first.peak < second.peak * 3.5f);
}

/** v0.46 : panoramique de la tranche, du bus principal, et Sculptor (panoramique, largeur) jusqu'à la
    sortie ; crêtes gauche / droite de la sortie. */
static void testPanning()
{
    auto energy = [] (Engine& e, int blocks, double& l, double& r, double& side)
    {
        std::vector<float> L (256), R (256);
        l = r = side = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            if (b % 50 == 0) e.pushEvent ({ InputEvent::noteOn, 0, 48, 0, 0.9f, false });
            e.process (nullptr, nullptr, L.data(), R.data(), 256, {}, nullptr, 0);
            for (int i = 0; i < 256; ++i) { l += L[(size_t) i] * L[(size_t) i]; r += R[(size_t) i] * R[(size_t) i]; side += (L[(size_t) i] - R[(size_t) i]) * (L[(size_t) i] - R[(size_t) i]); }
        }
    };
    for (int where = 0; where < 2; ++where) // 0 : tranche de la piste, 1 : bus principal
        for (float pan : { -100.0f, 100.0f })
        {
            Engine e; e.prepare (48000.0, 256); e.setInstrument (0, "mono", 0.0);
            e.setParam (globalSlot (where == 0 ? 0 : masterChannel, mixPan), (pan + 100.0f) / 200.0f);
            double l, r, sd; energy (e, 120, l, r, sd);
            CHECK (l + r > 1.0);
            if (pan < 0) CHECK (r < 1.0e-6 * l); else CHECK (l < 1.0e-6 * r);
            CHECK (pan < 0 ? e.getOutPeak (0) > 10.0f * e.getOutPeak (1) : e.getOutPeak (1) > 10.0f * e.getOutPeak (0));
        }
    for (int shaper : { 8, 9 }) // Sculptor : Panoramique, Largeur (sur un son mono)
    {
        Engine e; e.prepare (48000.0, 256); e.setInstrument (0, "mono", 0.0);
        CHECK (e.setInsert (0, 0, "sculptor", 0.0));
        auto set = [&] (const std::string& id, float plain) { const auto g = e.findParam (0, "fx1", id); CHECK (g >= 0); if (g >= 0) e.setParam (g, e.describe (g)->toNorm (plain)); };
        set ("s7on", 0);
        double l, r, sd; energy (e, 200, l, r, sd);
        CHECK (sd < 1.0e-9 * (l + r)); // tout éteint : le son mono reste au centre
        const auto pre = "s" + std::to_string (shaper);
        set (pre + "on", 1); set (pre + "amt", 100);
        energy (e, 300, l, r, sd);
        CHECK (sd > 0.01 * (l + r));
    }
}

/** v0.46 : allumer / éteindre un effet du Sculptor (et contourner un insert) ne fait pas de saut :
    sur un sinus, l'écart entre deux échantillons voisins reste celui d'un sinus. */
static void testNoClickOnToggle()
{
    const auto* d = findEffect ("sculptor");
    CHECK (d != nullptr);
    if (d == nullptr) return;
    // toggle : basculer Panoramique, Largeur et Filtre toutes les 100 blocs ; sinon, tout reste allumé
    auto runOnce = [d] (bool toggle)
    {
        auto fx = createEffect ("sculptor");
        constexpr double sr = 48000.0; constexpr int n = 32;
        fx->prepare (sr, n);
        std::vector<float> p;
        for (const auto& pd : d->params) p.push_back (pd.def);
        auto set = [&] (const char* id, float v) { const auto i = d->indexOf (id); if (i >= 0) p[(size_t) i] = v; };
        set ("s7on", 0); set ("s8amt", 100); set ("s8shape", 4); set ("s9amt", 100); set ("s4amt", 90);
        set ("s8on", 1); set ("s9on", 1); set ("s4on", 1);
        double ph = 0.0, maxStep = 0.0;
        float lastL = 0.0f, lastR = 0.0f;
        for (int b = 0; b < 3000; ++b)
        {
            if (toggle && b % 100 == 50) { const float on = b % 200 == 50 ? 0.0f : 1.0f; set ("s8on", on); set ("s9on", on); set ("s4on", on); }
            float L[n], R[n];
            for (int i = 0; i < n; ++i) { L[i] = R[i] = (float) (0.5 * std::sin (ph)); ph += 2.0 * 3.14159265358979 * 110.0 / sr; }
            fx->setPosition (b * n / sr * 2.0, true);
            fx->process (L, R, n, p.data(), 120.0);
            for (int i = 0; i < n; ++i)
            {
                if (b > 20) maxStep = std::max (maxStep, (double) std::max (std::abs (L[i] - lastL), std::abs (R[i] - lastR)));
                lastL = L[i]; lastR = R[i];
            }
        }
        return maxStep;
    };
    const auto steady = runOnce (false), toggled = runOnce (true);
    std::printf ("  Sculptor : plus grand pas entre deux \xC3\xA9" "chantillons %.4f (sans bascule %.4f)\n", toggled, steady);
    CHECK (toggled < steady * 1.25 + 0.003);
}

/** v0.46 : coupure de piste enregistrée dans le morceau (« Enr. les mouvements ») puis relue. */
static void testMuteRecording()
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    e.setRecordMoves (true);
    e.sequencer().requestPlay();
    run (e, 200);                                   // ~1 s de lecture
    e.sequencer().setTrackMute (0, true);
    e.recordSwitchUi (0, false, true);
    e.songTick (100.0);
    const auto* lane = e.getSong().laneFor (muteLane (0));
    CHECK (lane != nullptr && lane->pts.size() == 2 && lane->pts[0].norm < 0.5f && lane->pts[1].norm > 0.5f);
    e.setRecordMoves (false);
    e.sequencer().requestStop(); run (e, 2);
    e.sequencer().setTrackMute (0, false);
    e.sequencer().requestPlay();
    run (e, 60);
    const bool before = e.sequencer().getTrackMute (0); // avant le point : jouée
    run (e, 300);
    const bool after = e.sequencer().getTrackMute (0);  // après : coupée par la courbe
    std::printf ("  coupure enregistr\xC3\xA9" "e puis relue : avant %d, apr\xC3\xA8s %d\n", (int) before, (int) after);
    CHECK (! before && after);

    // crossfader des scènes : enregistré à la main, relu ensuite
    Engine f;
    f.prepare (48000.0, 256);
    f.setInstrument (0, "drumA", 0.0);
    f.setRecordMoves (true);
    f.sequencer().requestPlay();
    double ms = 0.0;
    for (int k = 0; k <= 8; ++k) { run (f, 47); f.recordFaderUi ((float) k / 8.0f); ms += 250.0; f.songTick (ms); }
    const auto* fl = f.getSong().laneFor (xfadeLane);
    CHECK (fl != nullptr && fl->pts.size() >= 2);
    f.setRecordMoves (false);
    f.sequencer().requestStop(); run (f, 2);
    f.sequencer().requestPlay();
    run (f, 300); // la main est relâchée depuis longtemps : la courbe pilote
    const auto lf = f.getLaneFader();
    std::printf ("  crossfader enregistr\xC3\xA9 puis relu : %.2f\n", lf);
    CHECK (lf > 0.5f);

    // v0.46 fix7 : solo d'un groupe et coupure d'un retour, enregistrés puis relus
    Engine g;
    g.prepare (48000.0, 256);
    g.setInstrument (0, "drumA", 0.0);
    g.setRecordMoves (true);
    g.sequencer().requestPlay();
    run (g, 200);
    const int grp = groupBase + 1, ret = numTracks + 1;
    g.setBusSolo (grp, true);  g.recordSwitchUi (grp, true, true);
    g.setBusMute (ret, true);  g.recordSwitchUi (ret, false, true);
    g.songTick (100.0);
    CHECK (g.getSong().laneFor (busSoloLane (grp)) != nullptr && g.getSong().laneFor (busMuteLane (ret)) != nullptr);
    g.setRecordMoves (false);
    g.sequencer().requestStop(); run (g, 2);
    g.setBusSolo (grp, false); g.setBusMute (ret, false);
    g.sequencer().requestPlay();
    run (g, 60);
    const bool gBefore = g.getBusSolo (grp);
    run (g, 300);
    std::printf ("  solo de groupe et coupure de retour relus : avant %d, apr\xC3\xA8s %d / %d\n", (int) gBefore, (int) g.getBusSolo (grp), (int) g.getBusMute (ret));
    CHECK (! gBefore && g.getBusSolo (grp) && g.getBusMute (ret));
}

// v0.46 fix7 : « Enr. le jeu » + « Enr. les mouvements » en une seule prise. La lecture tourne déjà
// quand on allume l'enregistrement (3 mesures) : la première rangée est le début du morceau, et
// les mouvements doivent tomber au même endroit du morceau que la musique qu'ils accompagnent.
// Puis : une rangée raccourcie emporte ses courbes (les rangées suivantes gardent les leurs).
static void testPlayAndMovesOneTake()
{
    const int barBlocks = 375; // 4 noires à 120 BPM, blocs de 256 à 48 kHz
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "drumA", 0.0);
    auto& s = e.sequencer();
    s.requestPlay();
    run (e, barBlocks * 3 + 40);                     // déjà 3 mesures (et un peu) de lecture
    e.setRecordPlay (true);
    e.setRecordMoves (true);
    double ms = 100.0;
    run (e, barBlocks);                              // la mesure 4 (index 4) est la première capturée
    e.songTick (ms += 50.0);
    run (e, barBlocks / 2);                          // milieu de la 2e mesure enregistrée
    const auto ppq = s.getPpq();
    e.recordFaderUi (0.9f);
    e.songTick (ms += 50.0);
    run (e, barBlocks);
    e.songTick (ms += 50.0);
    e.setRecordPlay (false); e.setRecordMoves (false);
    const auto& sg = e.getSong();
    const auto* fl = sg.laneFor (xfadeLane);
    const double expect = ppq - 16.0;                // 4 mesures avant la première capturée
    std::printf ("  jeu + mouvements en une prise : %d rang\xC3\xA9" "e(s), %.0f mesures, point \xC3\xA0 %.2f (attendu %.2f)\n",
                 (int) sg.rows.size(), sg.totalBeats() / 4.0, fl != nullptr && ! fl->pts.empty() ? (double) fl->pts.back().beat : -1.0, expect);
    CHECK (! sg.rows.empty());
    CHECK (fl != nullptr && ! fl->pts.empty() && std::abs ((double) fl->pts.back().beat - expect) < 0.05);
    CHECK (fl != nullptr && ! fl->pts.empty() && (double) fl->pts.back().beat < sg.totalBeats());

    // rangées éditées : la rangée 1 passe de 4 à 2 mesures, la rangée 2 garde sa courbe
    Song before;
    SongRow a, b; a.bars = 4; b.bars = 4; a.pat[0] = 0; b.pat[0] = 1;
    before.rows = { a, b };
    AutoLane l; l.global = xfadeLane;
    l.pts = { { 2.0f, 0.1f }, { 12.0f, 0.9f }, { 18.0f, 0.3f }, { 20.0f, 0.7f } }; // 2 et 12 : rangée 1 ; 18, 20 : rangée 2
    before.lanes = { l };
    Song after = before;
    after.rows[0].bars = 2;
    rippleSongLanes (before, after);
    const auto& q = after.lanes[0].pts;
    std::printf ("  rang\xC3\xA9" "e raccourcie : %d points, rang\xC3\xA9" "e 2 \xC3\xA0 %.2f et %.2f\n", (int) q.size(),
                 q.size() >= 2 ? (double) q[q.size() - 2].beat : -1.0, q.empty() ? -1.0 : (double) q.back().beat);
    CHECK (q.size() >= 3 && std::abs (q.back().beat - 12.0f) < 1.0e-4f && std::abs (q[q.size() - 2].beat - 10.0f) < 1.0e-4f);
    CHECK (std::abs (q.front().beat - 2.0f) < 1.0e-4f);
    for (const auto& p : q) CHECK (p.beat < 8.0f || p.beat >= 10.0f - 1.0e-4f); // rien entre la fin coupée et la rangée 2
    // une rangée insérée au début décale toutes les courbes
    Song ins = before;
    ins.rows.insert (ins.rows.begin(), b);
    rippleSongLanes (before, ins);
    CHECK (std::abs (ins.lanes[0].pts.front().beat - 18.0f) < 1.0e-4f);
    // une rangée supprimée emporte ses points
    Song del = before;
    del.rows.erase (del.rows.begin());
    rippleSongLanes (before, del);
    CHECK (del.lanes[0].pts.size() == 2 && std::abs (del.lanes[0].pts.front().beat - 2.0f) < 1.0e-4f);

    // ENR. suit le mode : en mode morceau, un bouton de piste va au morceau ; en boucle, au pattern
    for (const bool songMode : { true, false })
    {
        Engine m;
        m.prepare (48000.0, 256);
        m.setInstrument (0, "drumA", 0.0);
        m.sequencer().setStepP (0, 0, 0, 0, 100);
        Song sm; sm.songMode = songMode; SongRow r; r.bars = 4; r.pat[0] = 0; sm.rows = { r };
        m.setSong (sm, 0.0);
        m.sequencer().setRecording (true);
        m.sequencer().requestPlay();
        run (m, barBlocks / 2);
        const auto gl = globalSlot (0, 0);
        m.recordMoveUi (gl, 0.8f);
        m.songTick (50.0);
        const bool inSong = m.getSong().laneFor (gl) != nullptr;
        const bool inPat = m.getSong().patLaneIndex (0, 0, gl) >= 0;
        std::printf ("  ENR. %s : morceau %d, boucle %d\n", songMode ? "en mode morceau" : "en boucle", (int) inSong, (int) inPat);
        CHECK (inSong == songMode && inPat == ! songMode);
    }
}


// v0.48 : un pas retiré repart de zéro (durée, décalage, condition, verrous, accent, glissé)
static void testRemoveStepResets()
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setInstrument (0, "acid", 0.0);
    auto& s = e.sequencer();
    const auto p = s.getEditing (0);
    s.setStep (0, 0, 4, 100);
    s.setLength8 (0, 0, 4, 32);
    s.setMicro (0, 0, 4, 7);
    s.setStepFlags (0, 4, stepAccent | stepSlide | stepRec);
    s.setStepAmounts (0, 4, 60 | (40 << 8));
    e.setStepExtra (0, 0, 4, Condition::ab, 1, 2, 70, 0.0);
    CHECK (e.getExtras().find (0, p, 0, 4) != nullptr);
    e.removeStep (0, 0, 4, 0.0);
    CHECK (s.getStep (0, 0, 4) == 0);
    CHECK (s.getLength8 (0, 0, 4) == 8);
    CHECK (s.getMicro (0, 0, 4) == 0);
    CHECK (s.getStepFlags (0, 4) == stepRec);   // le départ de l'enregistreur reste
    CHECK (s.getStepAmounts (0, 4) == 0);
    CHECK (e.getExtras().find (0, p, 0, 4) == nullptr);
    // reposé : une note neuve
    s.setStep (0, 0, 4, 100);
    CHECK (s.getLength8 (0, 0, 4) == 8 && s.getMicro (0, 0, 4) == 0);
    // accord : retirer une ligne garde l'accent du pas tant qu'une autre y joue
    s.setStep (0, 1, 4, 100);
    s.setStepFlags (0, 4, stepAccent);
    e.removeStep (0, 1, 4, 0.0);
    CHECK (s.getStepFlags (0, 4) == stepAccent);
}

int main()
{
    std::printf ("Tests du moteur\n");
    testSilenceAndDrums();
    testParamsAndMixer();
    testPanning();
    testNoClickOnToggle();
    testMuteRecording();
    testPlayAndMovesOneTake();
    testScenes();
    testVectorScenes();
    testStepLocksAndConditions();
    testRemoveStepResets();
    testModulation();
    testSeqModSource();
    testSamplesAndSwap();
    testInserts();
    testPadSendsAndPreFader();
    testPadFx();
    testPatternsInEngine();
    testShiftPattern();
    testChains();
    testModSourceDefaults();
    testArpeggiator();
    testArpAndExpressionInEngine();
    testSlideInEngine();
    testOctaInEngine();
    testStopSilences();
    testMuteSilencesAudio();
    testChannelEq();
    testSong();
    testRecordHeldNotes();
    testLitNotesArpRecordAndLocks();
    testLfoDelay();
    testPadInserts();
    testEditCells();
    testUserPresetValues();
    testDistortionAlgorithms();
    testGateDeesser();
    testLfoSync();
    testPadStrip();
    testDrumAccent();
    testGroups24();
    testBusMuteSolo31();
    testAudioTap32();
    testCopyStepRange31();
    testSongLoopForever();
    testVocoderInEngine();
    testMoveKeepsState26();
    testPadSidechain24();
    testCpu();
    if (failures == 0) std::printf ("OK\n");
    return failures == 0 ? 0 : 1;
}
