/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Tests de tous les instruments et effets du catalogue : rendu valide, niveau raisonnable,
    silence après relâchement, coût de calcul. Écrit un WAV par module si un dossier est donné.
*/

#include "instruments/HybridPoly.h"
#include "instruments/WaveMorph.h"
#include "instruments/Pads.h"
#include "instruments/Octa.h"
#include "instruments/MS20.h"
#include "instruments/SH101.h"
#include "instruments/SpSampler.h"
#include "instruments/Blofeld.h"
#include "instruments/Ob6.h"
#include "instruments/Orbit.h"
#include "instruments/Fm6.h"
#include "instruments/Fm4.h"
#include "instruments/Muse.h"
#include "instruments/FmX.h"
#include "instruments/Dfam.h"
#include "instruments/Vocoders.h"
#include "engine/Catalog.h"
#include "effects/StepFx.h"
#include "StepFxV038.h"
#include "engine/ModSources.h"

#include <array>
#include <chrono>
#include <complex>
#include <cmath>
#include <functional>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace grv::engine;

static constexpr double kPi = 3.14159265358979323846;

static int failures = 0;
#define CHECK(cond, what) do { if (! (cond)) { std::printf ("  ECHEC %s : %s\n", name.c_str(), what); ++failures; } } while (0)

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const std::uint32_t n = (std::uint32_t) L.size();
    const std::uint32_t dataBytes = n * 2 * 2;
    auto u32 = [&] (std::uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (std::uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 ((std::uint32_t) sr); u32 ((std::uint32_t) sr * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (std::uint32_t i = 0; i < n; ++i)
    {
        const std::int16_t l = (std::int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * 32767.0f);
        const std::int16_t r = (std::int16_t) std::lround (std::clamp (R[i], -1.0f, 1.0f) * 32767.0f);
        std::fwrite (&l, 2, 1, f); std::fwrite (&r, 2, 1, f);
    }
    std::fclose (f);
}

static std::vector<float> defaults (const ModuleDesc& d)
{
    std::vector<float> p ((size_t) grv::engine::instrumentSlots, 0.0f);
    for (size_t i = 0; i < d.params.size(); ++i) p[i] = d.params[i].def;
    return p;
}

static void applyPreset (const ModuleDesc& d, std::vector<float>& p, const Preset& pr)
{
    p = defaults (d);
    for (const auto& [id, v] : pr.values) { const auto i = d.indexOf (id); if (i >= 0) p[(size_t) i] = v; }
}

static void testInstrument (const ModuleDesc& d, const char* outDir)
{
    const std::string name = d.type;
    constexpr int sr = 48000, block = 32;
    auto inst = createInstrument (d.type);
    CHECK (inst != nullptr, "fabrique");
    if (! inst) return;
    CHECK (d.params.size() <= (size_t) grv::engine::instrumentSlots, "trop de r\xC3\xA9glages");
    for (const auto& s : d.sections) for (const auto& id : s.params) CHECK (d.indexOf (id) >= 0, ("section " + s.id + " : " + id).c_str());
    for (const auto& pr : d.presets) for (const auto& [id, v] : pr.values) CHECK (d.indexOf (id) >= 0, ("pr\xC3\xA9r\xC3\xA9glage " + pr.name + " : " + id).c_str());

    inst->prepare (sr, block);
    auto p = defaults (d);

    // Échantillon d'essai (samplers) : 2 s, accord de sinus avec attaques
    static SampleBuffer testSample = []
    {
        SampleBuffer b;
        b.sampleRate = 44100.0;
        for (int i = 0; i < 88200; ++i)
        {
            const auto t = (double) i / 44100.0, beat = std::fmod (t, 0.25);
            const auto v = (std::sin (2 * kPi * 220 * t) + 0.5 * std::sin (2 * kPi * 330 * t)) * 0.4 * (0.4 + 0.6 * std::exp (-beat * 20));
            b.data.push_back ((float) v);
            b.right.push_back ((float) (v * 0.9));
        }
        return b;
    }();
    inst->setSample (0, &testSample);
    const bool isInput = d.type == "input";
    std::vector<float> inL (block), inR (block);
    if (isInput) { p[(size_t) d.indexOf ("monitor")] = 1.0f; inst->setInput (inL.data(), inR.data()); }

    // Phrase : 4 mesures à 120 BPM, notes liées et accords, puis 3 s de silence
    const int total = sr * 11;
    std::vector<float> L ((size_t) total, 0.0f), R ((size_t) total, 0.0f);
    struct Ev { int at; int note; float vel; bool on; };
    std::vector<Ev> evs;
    const int step = sr / 8; // double croche à 120 BPM
    const int notes[16] { 36, 36, 48, 36, 39, 36, 43, 41, 36, 48, 36, 46, 36, 39, 41, 43 };
    for (int bar = 0; bar < 4; ++bar)
        for (int s = 0; s < 16; ++s)
        {
            const auto at = (bar * 16 + s) * step;
            const auto note = d.drumKit ? s % 16 : notes[s] + (bar == 3 ? 12 : 0);
            const auto vel = (s % 4 == 0) ? 1.0f : 0.7f;
            const auto len = (s % 8 == 3) ? step * 2 : step / 2; // une note sur huit liée à la suivante
            evs.push_back ({ at, note, vel, true });
            evs.push_back ({ at + len, note, 0.0f, false });
            if (! d.drumKit && bar >= 2 && s % 8 == 0) // accords
                for (int k : { 4, 7, 11 }) { evs.push_back ({ at, note + 12 + k, 0.8f, true }); evs.push_back ({ at + step * 4, note + 12 + k, 0.0f, false }); }
        }
    std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });

    size_t ei = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < total; pos += block)
    {
        const auto presetIndex = (pos / (sr * 2)) % (int) std::max<size_t> (1, d.presets.size());
        if (! d.presets.empty() && pos % (sr * 2) == 0) applyPreset (d, p, d.presets[(size_t) presetIndex]);
        while (ei < evs.size() && evs[ei].at < pos + block)
        {
            if (evs[ei].on) inst->noteOn (evs[ei].note, evs[ei].vel); else inst->noteOff (evs[ei].note);
            ++ei;
        }
        for (int k = 0; k < block; ++k) { const auto on = pos < sr * 8; inL[(size_t) k] = on ? (float) (0.3 * std::sin (0.05 * (pos + k))) : 0.0f; inR[(size_t) k] = inL[(size_t) k]; }
        inst->setTempo (120.0);
        inst->render (L.data() + pos, R.data() + pos, std::min (block, total - pos), p.data());
    }
    const auto sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();

    float peak = 0.0f, tail = 0.0f;
    bool finite = true;
    for (int i = 0; i < total; ++i)
    {
        finite = finite && std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]);
        peak = std::max (peak, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
        if (i > total - sr) tail = std::max (tail, std::abs (L[(size_t) i]));
    }
    std::printf ("  %-10s cr\xC3\xAA" "te %5.2f  fin %.5f  calcul %5.2f %% d'un c\xC5\x93ur\n", name.c_str(), peak, tail, sec / 11.0 * 100.0);
    CHECK (finite, "valeurs invalides");
    CHECK (peak > 0.05f, "trop faible");
    CHECK (peak < 2.5f, "trop fort");
    CHECK (tail < 0.002f, "ne s'\xC3\xA9teint pas");
    if (outDir) writeWav (std::string (outDir) + "/" + name + ".wav", L, R, sr);
}

static void testEffect (const ModuleDesc& d, const char* outDir)
{
    const std::string name = d.type;
    constexpr int sr = 48000, block = 32;
    auto fx = createEffect (d.type);
    CHECK (fx != nullptr, "fabrique");
    if (! fx) return;
    CHECK (d.params.size() <= (size_t) grv::engine::insertSlots, "trop de r\xC3\xA9glages");
    for (const auto& s : d.sections) for (const auto& id : s.params) CHECK (d.indexOf (id) >= 0, ("section " + s.id + " : " + id).c_str());
    for (const auto& pr : d.presets) for (const auto& [id, v] : pr.values) CHECK (d.indexOf (id) >= 0, ("pr\xC3\xA9r\xC3\xA9glage " + pr.name + " : " + id).c_str());
    fx->prepare (sr, block);
    CHECK (fx->latency() == 0, "latence non nulle");

    // Signal : impulsions de batterie synthétiques + nappe, 6 s, puis 6 s de silence
    const int total = sr * 12;
    std::vector<float> L ((size_t) total, 0.0f), R ((size_t) total, 0.0f);
    std::uint32_t rng = 1;
    for (int i = 0; i < sr * 6; ++i)
    {
        const auto t = (double) (i % (sr / 2)) / sr;
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        const auto noise = (double) rng / 2147483648.0 - 1.0;
        const auto hit = std::sin (2 * kPi * 55 * t) * std::exp (-t * 12) + noise * std::exp (-t * 40) * 0.3;
        const auto pad = 0.15 * std::sin (2 * kPi * 220 * i / (double) sr) + 0.1 * std::sin (2 * kPi * 331 * i / (double) sr);
        L[(size_t) i] = (float) (0.6 * hit + pad);
        R[(size_t) i] = (float) (0.6 * hit + pad * 0.8);
    }
    auto p = defaults (d);
    const auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < total; pos += block)
    {
        if (! d.presets.empty() && pos % (sr * 3) == 0 && pos < sr * 6)
            applyPreset (d, p, d.presets[(size_t) ((pos / (sr * 3)) % (int) d.presets.size())]);
        fx->process (L.data() + pos, R.data() + pos, block, p.data(), 120.0);
    }
    const auto sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    float peak = 0.0f, tail = 0.0f;
    bool finite = true;
    for (int i = 0; i < total; ++i)
    {
        finite = finite && std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]);
        peak = std::max (peak, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
        if (i > total - sr / 2) tail = std::max (tail, std::abs (L[(size_t) i]));
    }
    std::printf ("  %-10s cr\xC3\xAA" "te %5.2f  fin %.5f  calcul %5.2f %% d'un c\xC5\x93ur\n", name.c_str(), peak, tail, sec / 12.0 * 100.0);
    CHECK (finite, "valeurs invalides");
    CHECK (peak < 4.0f, "trop fort");
    CHECK (tail < 0.01f, "ne s'\xC3\xA9teint pas");
    if (outDir) writeWav (std::string (outDir) + "/fx-" + name + ".wav", L, R, sr);
}

/** LFO synchronisés au tempo (v0.9) : chaque instrument qui en a reste stable, toutes divisions. */
static void testLfoSyncInstruments()
{
    for (const auto* d : instrumentCatalog())
    {
        std::vector<int> syncs;
        for (size_t i = 0; i < d->params.size(); ++i)
            if (d->params[i].id.size() > 4 && d->params[i].id.compare (d->params[i].id.size() - 4, 4, "sync") == 0 && ! d->params[i].choices.empty()) syncs.push_back ((int) i);
        if (syncs.empty()) continue;
        const std::string name = d->type;
        auto inst = createInstrument (d->type);
        inst->prepare (48000.0, 256);
        inst->setTempo (128.0);
        auto p = defaults (*d);
        bool ok = true;
        std::vector<float> L (256), R (256);
        for (int div = 0; div < 18; ++div)
        {
            for (auto k : syncs) p[(size_t) k] = (float) div;
            for (size_t i = 0; i < d->params.size(); ++i) // profondeurs de LFO au maximum
                if (d->params[i].name.find ("LFO") != std::string::npos && d->params[i].choices.empty() && d->params[i].unit != "Hz") p[i] = d->params[i].max;
            inst->noteOn (48, 0.9f);
            for (int b = 0; b < 40; ++b)
            {
                std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
                inst->render (L.data(), R.data(), 256, p.data());
                for (int i = 0; i < 256; ++i) ok = ok && std::isfinite (L[(size_t) i]) && std::abs (L[(size_t) i]) < 8.0f;
            }
            inst->noteOff (48);
        }
        std::printf ("  %-8s LFO synchronis\xC3\xA9s (%zu) : %s\n", d->type.c_str(), syncs.size(), ok ? "ok" : "\xC3\x89" "CHEC");
        CHECK (ok, "LFO synchronis\xC3\xA9");
    }
}

/** Batterie à machines (v0.10) : chaque machine sonne sur un pad, reste bornée et s'éteint ;
    le LFO du pad module ; un échantillon se superpose à la machine. */
static void testMachines()
{
    const std::string name = "machines";
    const auto* d = [] { for (const auto* x : instrumentCatalog()) if (x->type == "drumA") return x; return (const ModuleDesc*) nullptr; }();
    CHECK (d != nullptr && ! d->machineNames.empty() && d->machineLabels.size() == d->machineNames.size(), "description");
    if (d == nullptr) return;
    CHECK (d->params.size() <= (size_t) instrumentSlots, "trop de r\xC3\xA9glages");
    auto inst = createInstrument ("drumA");
    inst->prepare (48000.0, 32);
    inst->setTempo (120.0);
    auto p = defaults (*d);
    const auto mach = d->indexOf ("p01_mach");
    std::vector<float> L (32), R (32);
    int silent = 0, loud = 0;
    for (size_t m = 1; m < d->machineNames.size(); ++m)
    {
        p[(size_t) mach] = (float) m;
        inst->render (L.data(), R.data(), 32, p.data()); // le bloc lit la machine
        inst->noteOn (0, 0.9f);
        float peak = 0.0f, tail = 0.0f;
        bool finite = true;
        for (int b = 0; b < 48000 * 3 / 32; ++b)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 32, p.data());
            for (int i = 0; i < 32; ++i)
            {
                finite = finite && std::isfinite (L[(size_t) i]);
                peak = std::max (peak, std::abs (L[(size_t) i]));
                if (b > 48000 * 3 / 32 - 300) tail = std::max (tail, std::abs (L[(size_t) i]));
            }
        }
        std::printf ("  machine %-22s cr\xC3\xAAte %.2f fin %.5f\n", d->machineNames[m].c_str(), peak, tail);
        if (! finite || peak > 3.0f) ++loud;
        if (peak < 0.02f) ++silent;
        CHECK (tail < 0.01f, ("ne s'\xC3\xA9teint pas : " + d->machineNames[m]).c_str());
    }
    CHECK (silent == 0, "machine muette");
    CHECK (loud == 0, "machine trop forte ou invalide");

    // LFO du pad vers l'accord : deux coups au même instant du cycle donnent le même son,
    // à un autre instant, un son différent
    p = defaults (*d);
    p[(size_t) d->indexOf ("p01_ldest")] = 1.0f;   // Accord
    p[(size_t) d->indexOf ("p01_ldepth")] = 100.0f;
    p[(size_t) d->indexOf ("p01_lmode")] = 0.0f;   // libre
    p[(size_t) d->indexOf ("p01_lrate")] = 1.0f;
    auto energyAfterHit = [&] (int waitBlocks)
    {
        for (int b = 0; b < waitBlocks; ++b) inst->render (L.data(), R.data(), 32, p.data());
        inst->noteOn (0, 1.0f);
        double zc = 0.0; float prev = 0.0f;
        for (int b = 0; b < 300; ++b)
        {
            std::fill (L.begin(), L.end(), 0.0f);
            inst->render (L.data(), R.data(), 32, p.data());
            for (auto x : L) { if (prev <= 0.0f && x > 0.0f) zc += 1.0; prev = x; }
        }
        return zc;
    };
    inst->reset();
    const auto a = energyAfterHit (10), b = energyAfterHit (750);
    std::printf ("  LFO du pad vers l'accord : %.0f / %.0f passages par z\xC3\xA9ro\n", a, b);
    CHECK (std::abs (a - b) > 3.0, "LFO du pad sans effet");
}


// v0.10 : Wheel-Mod du poly analogique, maintien et LFO manuel du poly chorus
static double zeroCrossRate (Instrument& inst, std::vector<float>& p, int blocks)
{
    std::vector<float> L (64), R (64);
    int zc = 0; float prev = 0.0f; double energy = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
        inst.render (L.data(), R.data(), 64, p.data());
        for (float x : L) { if ((x > 0.0f) != (prev > 0.0f)) ++zc; prev = x; energy += (double) x * x; }
    }
    return energy < 1.0e-9 ? -1.0 : (double) zc;
}

static void testPolyExpression()
{
    const std::string name = "expression des polys";
    // Prophet : la molette ouvre le Wheel-Mod (LFO carré vers la fréquence de A) -> la hauteur varie
    {
        const auto& d = *[] { for (const auto* x : instrumentCatalog()) if (x->type == "poly5") return x; return (const ModuleDesc*) nullptr; }();
        auto inst = createInstrument ("poly5");
        inst->prepare (48000.0, 64);
        auto p = defaults (d);
        p[(size_t) d.indexOf ("mixb")] = 0.0f;
        p[(size_t) d.indexOf ("ltri")] = 0.0f; p[(size_t) d.indexOf ("lsquare")] = 1.0f;
        p[(size_t) d.indexOf ("lforate")] = 2.0f;
        p[(size_t) d.indexOf ("cutoff")] = 18000.0f;
        inst->noteOn (81, 0.9f);
        zeroCrossRate (*inst, p, 200);
        // écart entre fenêtres courtes (~50 ms) sur une seconde : la hauteur bouge-t-elle ?
        auto spreadOf = [&] { double lo = 1e9, hi = 0.0; for (int w = 0; w < 20; ++w) { const auto z = zeroCrossRate (*inst, p, 37); lo = std::min (lo, z); hi = std::max (hi, z); } return std::make_pair (lo, hi); };
        const auto a = spreadOf();
        inst->setExpression (0.0f, 1.0f, 0.0f);
        zeroCrossRate (*inst, p, 20);
        const auto b = spreadOf();
        std::printf ("  poly analogique : passages par fen\xC3\xAAtre %.0f..%.0f sans molette, %.0f..%.0f avec\n", a.first, a.second, b.first, b.second);
        CHECK (a.first > 0 && a.second - a.first <= 0.04 * a.first + 2, "stable sans molette");
        CHECK (b.second - b.first > 0.12 * a.first, "la molette module la hauteur");
        inst->setExpression (0.0f, 0.0f, 0.0f);
        zeroCrossRate (*inst, p, 40);
        const auto a1 = zeroCrossRate (*inst, p, 375), a2 = zeroCrossRate (*inst, p, 375);
        // pitch bend : +2 demi-tons -> ~12 % de passages en plus
        inst->setExpression (1.0f, 0.0f, 0.0f);
        zeroCrossRate (*inst, p, 40);
        const auto c = zeroCrossRate (*inst, p, 750);
        CHECK (c > (a1 + a2) * 1.08 && c < (a1 + a2) * 1.17, "pitch bend de 2 demi-tons");
    }
    // Poly chorus : maintien quand l'arpège est arrêté
    {
        const auto& d = *[] { for (const auto* x : instrumentCatalog()) if (x->type == "polyj") return x; return (const ModuleDesc*) nullptr; }();
        auto inst = createInstrument ("polyj");
        inst->prepare (48000.0, 64);
        auto p = defaults (d);
        p[(size_t) d.indexOf ("er")] = 20.0f;
        p[(size_t) d.indexOf ("chorus")] = 0.0f;
        p[(size_t) d.indexOf ("arp_hold")] = 1.0f;
        zeroCrossRate (*inst, p, 1);
        inst->noteOn (60, 0.9f); inst->noteOn (64, 0.9f);
        zeroCrossRate (*inst, p, 10);
        inst->noteOff (60); inst->noteOff (64);
        CHECK (zeroCrossRate (*inst, p, 300) > 0.0, "les notes restent maintenues");
        inst->noteOn (67, 0.9f); inst->noteOff (67); // nouvelle note : l'accord retenu part, la nouvelle reste
        zeroCrossRate (*inst, p, 300);
        p[(size_t) d.indexOf ("arp_hold")] = 0.0f; // maintien coupé : tout se relâche
        zeroCrossRate (*inst, p, 300);
        CHECK (! inst->isActive(), "maintien coupé : silence");
    }
}

// v0.10 : matrice du poly hybride — une source propre à la voix (LFO 1) et un LFO de la matrice (LFO 3)
static void testHybridMatrix()
{
    const std::string name = "matrice du poly hybride";
    const auto& d = *[] { for (const auto* x : instrumentCatalog()) if (x->type == "hybrid") return x; return (const ModuleDesc*) nullptr; }();
    CHECK (d.hubSlots == 16 && d.hubExtEnvs == 3 && d.hubExtLfos == 2 && d.hubLfos == 2, "description de la matrice");
    auto dstOf = [&] (const char* id) { const auto t = d.indexOf (id); for (size_t k = 0; k < d.modDests.size(); ++k) if (d.modDests[k] == t) return (float) k; return 0.0f; };
    auto run = [&] (int src, bool route) {
        auto inst = createInstrument ("hybrid");
        inst->prepare (48000.0, 32);
        auto p = defaults (d);
        p[(size_t) d.indexOf ("o1shape")] = 2.0f; p[(size_t) d.indexOf ("mix2")] = 0.0f;
        p[(size_t) d.indexOf ("cutoff")] = 18000.0f; p[(size_t) d.indexOf ("envamt")] = 0.0f;
        p[(size_t) d.indexOf ("l1shape")] = 3.0f; p[(size_t) d.indexOf ("l1rate")] = 3.0f; // LFO 1 carré
        p[(size_t) d.indexOf ("lf3_shape")] = 4.0f; p[(size_t) d.indexOf ("lf3_rate")] = 3.0f;
        if (route) { p[(size_t) d.indexOf ("mx1_src")] = (float) src; p[(size_t) d.indexOf ("mx1_dst")] = dstOf ("tune"); p[(size_t) d.indexOf ("mx1_amt")] = 20.0f; }
        std::vector<float> L (32), R (32);
        inst->render (L.data(), R.data(), 32, p.data());
        inst->noteOn (69, 0.9f);
        for (int b = 0; b < 300; ++b) inst->render (L.data(), R.data(), 32, p.data());
        double lo = 1e9, hi = 0.0;
        for (int w = 0; w < 24; ++w)
        {
            int zc = 0; float prev = 0.0f;
            for (int b = 0; b < 50; ++b)
            {
                std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
                inst->render (L.data(), R.data(), 32, p.data());
                for (float x : L) { if ((x > 0.0f) != (prev > 0.0f)) ++zc; prev = x; }
            }
            lo = std::min (lo, (double) zc); hi = std::max (hi, (double) zc);
        }
        return std::make_pair (lo, hi);
    };
    const auto& srcNames = d.params[(size_t) d.indexOf ("mx1_src")].choices;
    auto srcOf = [&] (const char* n) { for (size_t k = 0; k < srcNames.size(); ++k) if (srcNames[k] == n) return (int) k; return 0; };
    const auto none = run (0, false), lfo1 = run (srcOf ("LFO 1"), true), lfo3 = run (srcOf ("LFO 3"), true);
    std::printf ("  poly hybride : passages par fen\xC3\xAAtre %.0f..%.0f sans routage, LFO 1 %.0f..%.0f, LFO 3 %.0f..%.0f\n",
                 none.first, none.second, lfo1.first, lfo1.second, lfo3.first, lfo3.second);
    CHECK (none.second - none.first <= 0.05 * none.first + 2, "stable sans routage");
    CHECK (lfo1.second - lfo1.first > 0.3 * none.first, "LFO de la voix vers l'accord");
    CHECK (lfo3.second - lfo3.first > 0.3 * none.first, "LFO de la matrice vers l'accord");
    // densité et synchro dure : la forme change, le niveau reste raisonnable
    for (int fx = 0; fx < 2; ++fx)
    {
        auto inst = createInstrument ("hybrid");
        inst->prepare (48000.0, 32);
        auto p = defaults (d);
        p[(size_t) d.indexOf ("o1smod")] = (float) fx; p[(size_t) d.indexOf ("o1samt")] = 80.0f;
        p[(size_t) d.indexOf ("o1tbl")] = 1.0f;
        std::vector<float> L (32), R (32);
        inst->noteOn (45, 0.9f);
        float peak = 0.0f; bool finite = true;
        for (int b = 0; b < 3000; ++b)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 32, p.data());
            for (float x : L) { finite = finite && std::isfinite (x); peak = std::max (peak, std::abs (x)); }
        }
        CHECK (finite && peak > 0.05f && peak < 2.0f, fx == 0 ? "synchro dure (table)" : "densit\xC3\xA9 (table)");
    }
}

// v0.10 : wave morph — chaque mutant (A et B) et chaque type de filtre 1 sonnent sans exploser
static void testMorphModes()
{
    const std::string name = "wave morph : mutants et filtres";
    const auto& d = *[] { for (const auto* x : instrumentCatalog()) if (x->type == "morph") return x; return (const ModuleDesc*) nullptr; }();
    CHECK (d.hubSlots == 32 && d.hubMacros == 8 && d.hubEnvs == 2 && d.hubLfos == 3, "description");
    auto run = [&] (const char* a, float va, const char* b, float vb) {
        auto inst = createInstrument ("morph");
        inst->prepare (48000.0, 32);
        auto p = defaults (d);
        p[(size_t) d.indexOf (a)] = va;
        if (b) p[(size_t) d.indexOf (b)] = vb;
        std::vector<float> L (32), R (32);
        inst->noteOn (48, 1.0f); inst->noteOn (60, 1.0f);
        float peak = 0.0f; bool finite = true;
        for (int k = 0; k < 2000; ++k)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 32, p.data());
            for (float x : L) { finite = finite && std::isfinite (x); peak = std::max (peak, std::abs (x)); }
        }
        return finite && peak > 0.01f && peak < 2.5f;
    };
    for (int m = 1; m < 8; ++m)
    {
        CHECK (run ("w1mut", (float) m, "w1mutamt", 80.0f), "mutant A");
        CHECK (run ("w1mut2", (float) m, "w1mut2amt", 80.0f), "mutant B");
    }
    for (int f = 0; f < 10; ++f) CHECK (run ("f1mode", (float) f, "f1reso", 85.0f), "filtre 1");
    CHECK (run ("vmode", 2.0f, "univoices", 8.0f), "unisson 8 voix");
    CHECK (run ("mix3", 80.0f, nullptr, 0.0f), "oscillateur 3");

    // Tables d'ondes choisies : 64 ondes ; liste de 2 ondes (sinus, carré) balayée par la position
    CHECK (d.params[(size_t) d.indexOf ("w1s1")].choices.size() == 64 && d.waveShapes.size() == 64, "bibliot\xC3\xA8que de 64 ondes");
    auto brightness = [&] (float pos)
    {
        auto inst = createInstrument ("morph");
        inst->prepare (48000.0, 32);
        auto p = defaults (d);
        p[(size_t) d.indexOf ("w1n")] = 1.0f;   // 2 ondes
        p[(size_t) d.indexOf ("w1s1")] = 0.0f;  // sinus
        p[(size_t) d.indexOf ("w1s2")] = 3.0f;  // carré
        p[(size_t) d.indexOf ("w1pos")] = pos;
        p[(size_t) d.indexOf ("mix2")] = 0.0f;
        p[(size_t) d.indexOf ("f1cutoff")] = 18000.0f; p[(size_t) d.indexOf ("env1f1")] = 0.0f;
        std::vector<float> L (32), R (32);
        inst->noteOn (48, 1.0f);
        double hf = 0.0, all = 0.0; float prev = 0.0f;
        for (int k = 0; k < 1500; ++k)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 32, p.data());
            if (k > 300) for (float x : L) { hf += (double) (x - prev) * (x - prev); all += (double) x * x; prev = x; }
        }
        return all > 0.0 ? hf / all : 0.0;
    };
    const auto bSine = brightness (0.0f), bSquare = brightness (15.0f), bHalf = brightness (7.5f);
    std::printf ("  tables choisies : sinus %.4f, mi-chemin %.4f, carr\xC3\xA9 %.4f\n", bSine, bHalf, bSquare);
    CHECK (bSquare > bSine * 3.0 && bHalf > bSine && bHalf < bSquare, "position dans la liste choisie");
}

// v0.11 : sampler de performance (machines, découpes, étirement calé au tempo, enregistreur, boucle)
static void testOcta()
{
    const std::string name = "sampler de performance";
    const auto& d = *[] { for (const auto* x : instrumentCatalog()) if (x->type == "octa") return x; return (const ModuleDesc*) nullptr; }();
    // v0.42 : réglages de la piste interne T1 (« t1_… ») ; état : piste jouée puis 5 valeurs par piste
    auto idx = [&] (const char* id) { return (size_t) d.indexOf (std::string ("t1_") + id); };
    constexpr double sr = 48000.0;
    auto block = [&] (Instrument& inst, std::vector<float>& p, std::vector<float>& L, std::vector<float>& R, int n = 32) {
        std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
        inst.render (L.data(), R.data(), n, p.data());
    };

    // 1. Découpes jouées au clavier : 8 parts d'amplitudes 0,1 à 0,8
    {
        static SampleBuffer sb;
        sb.sampleRate = sr; sb.data.resize (8 * 4800);
        for (size_t i = 0; i < sb.data.size(); ++i) sb.data[i] = (float) ((double) (i / 4800 + 1) * 0.1 * std::sin ((double) i * 0.2));
        auto inst = createInstrument ("octa");
        inst->prepare (sr, 32); inst->setTempo (120.0); inst->setSample (0, &sb);
        auto p = defaults (d);
        p[idx ("slices")] = 3.0f; p[idx ("keymode")] = 1.0f; p[idx ("velamp")] = 0.0f;
        std::vector<float> L (32), R (32);
        block (*inst, p, L, R);
        bool ok = true;
        for (int k = 0; k < 8; ++k)
        {
            inst->noteOn (60 + k, 1.0f);
            float peak = 0.0f;
            for (int b = 0; b < 100; ++b) { block (*inst, p, L, R); if (b > 4) for (float x : L) peak = std::max (peak, std::abs (x)); }
            ok = ok && std::abs (peak - 0.1f * (float) (k + 1)) < 0.03f;
            inst->noteOff (60 + k);
        }
        CHECK (ok, "d\xC3\xA9" "coupe choisie par la note");
    }

    // 1a. Clavier « Emplacements » : Do3 joue l'échantillon 1, Ré3 l'échantillon 3 (amplitudes 0,2 et 0,6)
    {
        static SampleBuffer s1, s3;
        for (auto* x : { &s1, &s3 })
        {
            x->sampleRate = sr; x->data.resize (48000);
            const auto amp = x == &s1 ? 0.2 : 0.6;
            for (size_t i = 0; i < x->data.size(); ++i) x->data[i] = (float) (amp * std::sin ((double) i * 0.2));
        }
        auto inst = createInstrument ("octa");
        inst->prepare (sr, 32); inst->setTempo (120.0); inst->setSample (0, &s1); inst->setSample (2, &s3);
        auto p = defaults (d);
        p[idx ("keymode")] = 2.0f; p[idx ("velamp")] = 0.0f; p[idx ("stretch")] = 0.0f;
        std::vector<float> L (32), R (32);
        block (*inst, p, L, R);
        float peaks[2] {};
        int k = 0;
        for (const int note : { 60, 62 })
        {
            inst->noteOn (note, 1.0f);
            for (int b = 0; b < 100; ++b) { block (*inst, p, L, R); if (b > 4) for (float x : L) peaks[k] = std::max (peaks[k], std::abs (x)); }
            inst->noteOff (note);
            for (int b = 0; b < 200; ++b) block (*inst, p, L, R);
            ++k;
        }
        std::printf ("  emplacements par note : %.3f / %.3f\n", peaks[0], peaks[1]);
        CHECK (std::abs (peaks[0] - 0.2f) < 0.05f && std::abs (peaks[1] - 0.6f) < 0.12f, "emplacement choisi par la note");
    }

    // 1b. v0.16 : arrêt du séquenceur — une boucle à tenue infinie et la boucle Pickup se taisent,
    // la boucle Pickup repart avec le séquenceur
    {
        static SampleBuffer sl;
        sl.sampleRate = sr; sl.data.resize (24000);
        for (size_t i = 0; i < sl.data.size(); ++i) sl.data[i] = (float) (0.5 * std::sin ((double) i * 0.1));
        auto inst = createInstrument ("octa");
        inst->prepare (sr, 32); inst->setTempo (120.0); inst->setSample (0, &sl);
        auto p = defaults (d);
        p[idx ("loop")] = 1.0f; p[idx ("holdmode")] = 2.0f; p[idx ("stretch")] = 0.0f;
        std::vector<float> L (32), R (32);
        auto level = [&] (int blocks) { float pk = 0.0f; for (int b = 0; b < blocks; ++b) { block (*inst, p, L, R); for (float x : L) pk = std::max (pk, std::abs (x)); } return pk; };
        block (*inst, p, L, R);
        inst->noteOn (60, 1.0f);
        level (10);
        inst->noteOff (60);
        const auto held = level (3000);              // 2 s : la boucle tient sans fin
        inst->transportStopped();
        level (200);
        const auto stopped = level (100);
        std::printf ("  boucle infinie : %.3f, apr\xC3\xA8s arr\xC3\xAAt : %.5f\n", held, stopped);
        CHECK (held > 0.1f && stopped < 1.0e-3f, "arr\xC3\xAAt : boucle Flex tue");

        auto pk = createInstrument ("octa");
        pk->prepare (sr, 32); pk->setTempo (120.0);
        auto q = defaults (d);
        q[idx ("machine")] = 1.0f; q[idx ("recsrc")] = 0.0f; q[idx ("picklen")] = 0.0f;
        std::vector<float> inL (32), inR (32);
        for (int i = 0; i < 32; ++i) inL[(size_t) i] = inR[(size_t) i] = (float) (0.4 * std::sin ((double) i * 0.3));
        pk->setInput (inL.data(), inR.data());
        auto lv = [&] (int blocks) { float m = 0.0f; for (int b = 0; b < blocks; ++b) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); pk->render (L.data(), R.data(), 32, q.data()); for (float x : L) m = std::max (m, std::abs (x)); } return m; };
        pk->recordAction (0); lv (300);              // prise de 0,2 s
        pk->recordAction (0); lv (2);                // boucle fermée
        pk->setInput (nullptr, nullptr);
        const auto playing = lv (300);
        pk->transportStopped();
        lv (8);                                      // fondu d'arrêt de 3 ms (v0.22)
        const auto quiet = lv (292);
        pk->transportStarted();
        const auto again = lv (300);
        std::printf ("  boucle Pickup : %.3f, arr\xC3\xAAt %.5f, reprise %.3f\n", playing, quiet, again);
        CHECK (playing > 0.1f && quiet < 1.0e-6f && again > 0.1f, "arr\xC3\xAAt : boucle Pickup tue puis reprise");
    }

    // 1b. Filtre et lo-fi de la piste : un passe-bas 24 dB à 500 Hz étouffe un 8 kHz ; 4 bits -> paliers de 1/8
    {
        static SampleBuffer sb;
        sb.sampleRate = sr; sb.data.resize (96000);
        for (size_t i = 0; i < sb.data.size(); ++i) sb.data[i] = (float) (0.5 * std::sin (2.0 * 3.14159265 * 8000.0 * (double) i / sr));
        auto run = [&] (float ftype, float cut, float bits, bool& steps) {
            auto inst = createInstrument ("octa");
            inst->prepare (sr, 32); inst->setTempo (120.0); inst->setSample (0, &sb);
            auto p = defaults (d);
            p[idx ("velamp")] = 0.0f; p[idx ("ftype")] = ftype; p[idx ("fcut")] = cut; p[idx ("bits")] = bits;
            std::vector<float> L (32), R (32);
            block (*inst, p, L, R);
            inst->noteOn (60, 1.0f);
            double e = 0.0; steps = true;
            for (int b = 0; b < 300; ++b)
            {
                block (*inst, p, L, R);
                if (b > 50) for (float x : L) { e += (double) x * x; steps = steps && std::abs (x * 8.0f - std::round (x * 8.0f)) < 1.0e-3f; }
            }
            return e;
        };
        bool st = false, st2 = false;
        const auto open = run (0.0f, 4000.0f, 16.0f, st);
        const auto closed = run (1.0f, 500.0f, 16.0f, st);
        const auto high = run (1.0f, 18000.0f, 16.0f, st);
        run (0.0f, 4000.0f, 4.0f, st2);
        std::printf ("  filtre de piste : ouvert %.2f, 18 kHz %.2f, 500 Hz %.6f\n", open, high, closed);
        CHECK (open > 1.0 && closed < open * 0.01 && high > open * 0.5, "passe-bas de la piste");
        CHECK (st2, "r\xC3\xA9" "duction \xC3\xA0 4 bits");
    }

    // 2. Étirement « Beat » : une mesure de 4 clics à 120 BPM, jouée à 60 BPM -> clics toutes les secondes
    {
        static SampleBuffer sb;
        sb.sampleRate = sr; sb.data.assign (96000, 0.0f);
        for (int q = 0; q < 4; ++q) for (int i = 0; i < 200; ++i) sb.data[(size_t) (q * 24000 + i)] = (float) (0.8 * std::exp (-i / 40.0) * std::sin (i * 0.9));
        for (double tempo : { 120.0, 60.0 })
        {
            auto inst = createInstrument ("octa");
            inst->prepare (sr, 32); inst->setTempo (tempo); inst->setSample (0, &sb);
            auto p = defaults (d);
            p[idx ("stretch")] = 2.0f; p[idx ("velamp")] = 0.0f;
            std::vector<float> L (32), R (32);
            block (*inst, p, L, R);
            inst->noteOn (60, 1.0f);
            int second = -1;
            for (int b = 0; b < 2500 && second < 0; ++b)
            {
                block (*inst, p, L, R);
                for (int i = 0; i < 32; ++i) if (b * 32 + i > 6000 && std::abs (L[(size_t) i]) > 0.2f) { second = b * 32 + i; break; }
            }
            const auto expected = 24000.0 * 120.0 / tempo;
            std::printf ("  \xC3\xA9tirement Beat \xC3\xA0 %.0f BPM : 2e clic \xC3\xA0 %d (attendu %.0f)\n", tempo, second, expected);
            CHECK (second > 0 && std::abs (second - expected) < 0.03 * expected, "clic cal\xC3\xA9 au tempo");
        }
    }

    // 2b. Étirement « Normal » : à 60 BPM la hauteur ne bouge pas ; +12 demi-tons la double
    {
        static SampleBuffer sb;
        sb.sampleRate = sr; sb.data.resize (96000);
        for (size_t i = 0; i < sb.data.size(); ++i) sb.data[i] = (float) (0.5 * std::sin (2.0 * 3.14159265 * 440.0 * (double) i / sr));
        auto rateAt = [&] (double tempo, float semis) {
            auto inst = createInstrument ("octa");
            inst->prepare (sr, 32); inst->setTempo (tempo); inst->setSample (0, &sb);
            auto p = defaults (d);
            p[idx ("stretch")] = 1.0f; p[idx ("tune")] = semis;
            std::vector<float> L (32), R (32);
            block (*inst, p, L, R);
            inst->noteOn (60, 1.0f);
            int zc = 0; float prev = 0.0f;
            for (int b = 0; b < 1500; ++b) { block (*inst, p, L, R); if (b > 100) for (float x : L) { if ((x > 0.0f) != (prev > 0.0f)) ++zc; prev = x; } }
            return zc / (1400.0 * 32.0 / sr) / 2.0; // Hz
        };
        const auto f60 = rateAt (60.0, 0.0f), fUp = rateAt (120.0, 12.0f);
        std::printf ("  \xC3\xA9tirement Normal : %.0f Hz \xC3\xA0 60 BPM, %.0f Hz \xC3\xA0 +12 dt\n", f60, fUp);
        CHECK (std::abs (f60 - 440.0) < 15.0 && std::abs (fUp - 880.0) < 30.0, "hauteur ind\xC3\xA9pendante du tempo");
    }

    // 3. Enregistreur : 4 pas de l'entrée, puis lecture du tampon
    {
        auto inst = createInstrument ("octa");
        inst->prepare (sr, 32); inst->setTempo (120.0);
        auto p = defaults (d);
        p[idx ("reclen")] = 2.0f; // 4 pas = 0,5 s
        std::vector<float> in (32), L (32), R (32);
        inst->setInput (in.data(), in.data());
        inst->recordAction (0);
        int t = 0;
        for (int b = 0; b < 1000; ++b) { for (int i = 0; i < 32; ++i, ++t) in[(size_t) i] = (float) (0.5 * std::sin (t * 0.03)); block (*inst, p, L, R); }
        float st[48] {};
        inst->status (st, 48);
        std::printf ("  enregistreur : \xC3\xA9tat %.0f, %.3f s\n", st[1 + 0], st[1 + 1]);
        CHECK (st[1 + 0] == 2.0f && std::abs (st[1 + 1] - 0.5f) < 0.01f, "4 pas enregistr\xC3\xA9s");
        std::fill (in.begin(), in.end(), 0.0f);
        p[idx ("slot")] = 0.0f; p[idx ("velamp")] = 0.0f;
        inst->noteOn (60, 1.0f);
        float peak = 0.0f;
        for (int b = 0; b < 300; ++b) { block (*inst, p, L, R); for (float x : L) peak = std::max (peak, std::abs (x)); }
        CHECK (peak > 0.4f && peak < 0.6f, "lecture du tampon enregistr\xC3\xA9");
    }

    // 4. Pickup : enregistrer, fermer la boucle (calée sur 1 mesure), elle tourne seule ; surcouche
    {
        auto inst = createInstrument ("octa");
        inst->prepare (sr, 32); inst->setTempo (120.0);
        auto p = defaults (d);
        p[idx ("machine")] = 1.0f; // Pickup
        std::vector<float> in (32), L (32), R (32);
        inst->setInput (in.data(), in.data());
        block (*inst, p, L, R);
        inst->recordAction (0);
        int t = 0;
        for (int b = 0; b < 2800; ++b) { for (int i = 0; i < 32; ++i, ++t) in[(size_t) i] = (float) (0.4 * std::sin (t * 0.02)); block (*inst, p, L, R); } // 1,87 s
        inst->recordAction (0); // ferme : arrondie à 1 mesure (2 s)
        std::fill (in.begin(), in.end(), 0.0f);
        block (*inst, p, L, R);
        float st[48] {};
        inst->status (st, 48);
        float peak = 0.0f;
        for (int b = 0; b < 1500; ++b) { block (*inst, p, L, R); for (float x : L) peak = std::max (peak, std::abs (x)); }
        std::printf ("  boucle : \xC3\xA9tat %.0f, %.3f s, cr\xC3\xAA" "te %.2f\n", st[1 + 3], st[1 + 1], peak);
        CHECK (st[1 + 3] == 2.0f && std::abs (st[1 + 1] - 2.0f) < 0.01f && peak > 0.3f, "boucle ferm\xC3\xA9" "e et jou\xC3\xA9" "e");
        inst->recordAction (0);
        block (*inst, p, L, R);
        inst->status (st, 48);
        CHECK (st[1 + 3] == 3.0f, "surcouche");
    }
}

// v0.13 : effets « pédales » — atmosphère (tous préréglages stables, gel qui tient), échos
// multiples (temps juste, chaque machine stable), effets à pas (calés sur le tempo), boucleur
static void testPedalEffects()
{
    const std::string name = "effets p\xC3\xA9" "dales";
    constexpr double sr = 48000.0;
    const auto& cat = effectCatalog();
    auto descOf = [&] (const char* t) -> const ModuleDesc& { for (const auto* d : cat) if (d->type == t) return *d; return *cat[0]; };
    auto rms = [] (const std::vector<float>& v, size_t a, size_t b) { double s = 0.0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) std::max<size_t> (1, b - a)); };

    // 1. Atmosphère : chaque préréglage reste fini et borné ; le gel garde la queue, la salle s'éteint
    {
        const auto& d = descOf ("atmos");
        for (const auto& pr : d.presets)
        {
            auto fx = createEffect ("atmos");
            fx->prepare (sr, 64);
            auto p = defaults (d); applyPreset (d, p, pr);
            const bool frozen = p[(size_t) d.indexOf ("freeze")] > 0.5f;
            if (frozen) p[(size_t) d.indexOf ("freeze")] = 0.0f;   // on remplit avant de geler
            const int total = (int) (sr * 14);
            std::vector<float> L ((size_t) total, 0.0f), R ((size_t) total, 0.0f);
            for (int i = 0; i < (int) sr / 2; ++i) { const auto v = 0.4f * (float) std::sin (i * 0.05) * (float) std::exp (-i / 8000.0); L[(size_t) i] = v; R[(size_t) i] = v; }
            bool ok = true; float peak = 0.0f;
            for (int pos = 0; pos < total; pos += 64)
            {
                if (frozen && pos >= (int) sr) p[(size_t) d.indexOf ("freeze")] = 1.0f;
                fx->process (L.data() + pos, R.data() + pos, 64, p.data(), 120.0);
            }
            for (float x : L) { ok = ok && std::isfinite (x); peak = std::max (peak, std::abs (x)); }
            const auto late = rms (L, (size_t) (sr * 12), (size_t) total);
            std::printf ("  atmosph\xC3\xA8re %-26s cr\xC3\xAAte %.2f, apr\xC3\xA8s 12 s %.5f\n", pr.name.c_str(), peak, late);
            CHECK (ok && peak < 3.0f, ("atmosph\xC3\xA8re born\xC3\xA9" "e : " + pr.name).c_str());
            if (frozen) CHECK (late > 0.002, "gel : la queue tient");
            if (pr.name == "Grande salle") CHECK (late < 1.0e-3, "salle : la queue s'\xC3\xA9teint");
        }
    }

    // 2. Échos multiples : premier écho au bon temps (numérique, 1/4 à 120 BPM = 24000) ; chaque machine bornée
    {
        const auto& d = descOf ("mdelay");
        for (int md = 0; md < 8; ++md)
        {
            auto fx = createEffect ("mdelay");
            fx->prepare (sr, 64);
            auto p = defaults (d);
            p[(size_t) d.indexOf ("mode")] = (float) md; p[(size_t) d.indexOf ("division")] = 10.0f; // 1/4
            p[(size_t) d.indexOf ("mix")] = 100.0f; p[(size_t) d.indexOf ("moddepth")] = 0.0f; p[(size_t) d.indexOf ("grit")] = 0.0f;
            p[(size_t) d.indexOf ("feedback")] = md == 0 ? 0.0f : 105.0f;
            const int total = (int) (sr * 8);
            std::vector<float> L ((size_t) total, 0.0f), R ((size_t) total, 0.0f);
            for (int i = 0; i < 200; ++i) { L[(size_t) i] = R[(size_t) i] = 0.8f * (float) std::sin (i * 0.3); }
            for (int pos = 0; pos < total; pos += 64) fx->process (L.data() + pos, R.data() + pos, 64, p.data(), 120.0);
            bool ok = true; float peak = 0.0f; int first = -1;
            for (int i = 0; i < total; ++i) { ok = ok && std::isfinite (L[(size_t) i]); peak = std::max (peak, std::abs (L[(size_t) i])); if (first < 0 && i > 2000 && std::abs (L[(size_t) i]) > 0.1f) first = i; }
            std::printf ("  \xC3\xA9" "chos multiples, machine %d : 1er \xC3\xA9" "cho \xC3\xA0 %d, cr\xC3\xAAte %.2f\n", md, first, peak);
            CHECK (ok && peak < 4.0f, "\xC3\xA9" "chos born\xC3\xA9" "s");
            if (md == 0) CHECK (first >= 23990 && first <= 24100, "temps de l'\xC3\xA9" "cho num\xC3\xA9rique");
            if (md == 4) CHECK (first > 0, "\xC3\xA9" "cho invers\xC3\xA9 audible");
        }
    }

    // 3. Effets à pas : grille vide = signal intact ; « Répète » sur tous les pas = tranche de 1/32 rejouée
    {
        const auto& d = descOf ("stepfx");
        auto run = [&] (int fxKind, bool playing) {
            auto fx = createEffect ("stepfx");
            fx->prepare (sr, 64);
            auto p = defaults (d);
            for (int s = 0; s < 16; ++s) p[(size_t) s] = (float) fxKind;
            const int total = (int) sr;
            std::vector<float> L ((size_t) total), R ((size_t) total), in ((size_t) total);
            for (int i = 0; i < total; ++i) in[(size_t) i] = L[(size_t) i] = R[(size_t) i] = (float) (0.5 * std::sin (i * 2.0 * kPi * 3.0 / sr)); // 3 Hz : lent
            double ppq = 0.0;
            for (int pos = 0; pos < total; pos += 64) { fx->setPosition (ppq, playing); fx->process (L.data() + pos, R.data() + pos, 64, p.data(), 120.0); ppq += 64 * 2.0 / sr; }
            double diff = 0.0; for (int i = 0; i < total; ++i) diff = std::max (diff, (double) std::abs (L[(size_t) i] - in[(size_t) i]));
            return diff;
        };
        const auto none = run (0, true), rep = run (1, true), stopped = run (1, false);
        std::printf ("  effets \xC3\xA0 pas : \xC3\xA9" "cart vide %.5f, r\xC3\xA9p\xC3\xA8te %.3f, arr\xC3\xAAt\xC3\xA9 %.5f\n", none, rep, stopped);
        CHECK (none < 1.0e-4 && stopped < 1.0e-4 && rep > 0.05, "effets \xC3\xA0 pas");
    }

    // 4. Boucleur : enregistrer 1 s, lire (la boucle sonne sans entrée), effacer (silence), surcouche
    {
        const auto& d = descOf ("looper");
        auto fx = createEffect ("looper");
        fx->prepare (sr, 64);
        auto p = defaults (d);
        const auto act = (size_t) d.indexOf ("action");
        std::vector<float> L (64), R (64);
        int t = 0;
        auto blocks = [&] (int count, bool withInput) {
            double e = 0.0;
            for (int b = 0; b < count; ++b)
            {
                for (int i = 0; i < 64; ++i, ++t) L[(size_t) i] = R[(size_t) i] = withInput ? (float) (0.4 * std::sin (t * 0.01)) : 0.0f;
                fx->process (L.data(), R.data(), 64, p.data(), 120.0);
                for (float x : L) e += (double) x * x;
            }
            return std::sqrt (e / (count * 64.0));
        };
        blocks (2, false);
        p[act] = 1.0f; blocks (750, true);          // 1 s
        p[act] = 2.0f; const auto played = blocks (750, false);
        const auto len = fx->meter (3);
        p[act] = 3.0f; blocks (750, true);          // surcouche
        p[act] = 2.0f; const auto louder = blocks (750, false);
        p[act] = 4.0f; const auto erased = blocks (750, false);
        std::printf ("  boucleur : %.3f s, lecture %.3f, apr\xC3\xA8s surcouche %.3f, effac\xC3\xA9 %.5f\n", len, played, louder, erased);
        CHECK (std::abs (len - 1.0f) < 0.01f && played > 0.2 && louder > played * 1.3 && erased < 1.0e-6, "boucleur");
    }
}

// v0.13.2 : nouveaux réglages à leur place (identifiant ↔ position dans le moteur)
// v0.15 : sampler à découpes — découpes égales choisies par le nombre et la page, redéclenchement
static void testSlicerSlices()
{
    const std::string name = "sampler \xC3\xA0 d\xC3\xA9" "coupes";
    const auto* d = grv::engine::findInstrument ("sampler");
    CHECK (d != nullptr, "catalogue");
    if (d == nullptr) return;
    static SampleBuffer sb = []
    {
        SampleBuffer b;
        b.sampleRate = 48000.0;
        b.data.resize (32 * 4800);  // 32 parts de 0,1 s, amplitude croissante (part k : 0,02 × (k + 1))
        for (size_t i = 0; i < b.data.size(); ++i) b.data[i] = (float) (0.02 * (double) (i / 4800 + 1) * std::sin ((double) i * 0.2));
        return b;
    }();
    auto peakOf = [&] (int count, int page, int pad, int retrig, int* bursts)
    {
        auto inst = createInstrument ("sampler");
        inst->prepare (48000.0, 32); inst->setTempo (120.0); inst->setSample (0, &sb);
        auto p = defaults (*d);
        p[(size_t) d->indexOf ("slicecnt")] = (float) count;
        p[(size_t) d->indexOf ("slicepage")] = (float) page;
        p[(size_t) d->indexOf ("retrig")] = (float) retrig;
        p[(size_t) d->indexOf ("rtim")] = 3.0f;                        // 1/16 : 0,125 s à 120 BPM
        p[(size_t) d->indexOf ("velamp")] = 0.0f;
        const auto zEnd = d->indexOf (pad < 9 ? ("z0" + std::to_string (pad + 1) + "_release").c_str() : ("z" + std::to_string (pad + 1) + "_release").c_str());
        if (zEnd >= 0) p[(size_t) zEnd] = 2.0f;
        std::vector<float> L (32), R (32);
        inst->render (L.data(), R.data(), 32, p.data());
        inst->noteOn (pad, 1.0f);
        float peak = 0.0f; int onsets = 0; bool loud = false;
        for (int b = 0; b < 48000 / 32; ++b)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 32, p.data());
            float bp = 0.0f; for (float x : L) bp = std::max (bp, std::abs (x));
            peak = std::max (peak, bp);
            if (bp > 0.005f && ! loud) ++onsets;
            loud = bp > 0.005f;
        }
        if (bursts) *bursts = onsets;
        return peak;
    };
    // 32 parts (choix 5), page 2 (choix 1), pad 3 : part 19 → 0,02 × 20 = 0,4
    const auto a = peakOf (5, 1, 3, 0, nullptr);
    // 8 parts (choix 3), pad 1 : 4 parts d'origine → la plus forte est la 8e, 0,16
    const auto b = peakOf (3, 0, 1, 0, nullptr);
    // 8 parts, pad 9 : au-delà → silence
    const auto c = peakOf (3, 0, 9, 0, nullptr);
    int bursts = 0;
    peakOf (5, 0, 0, 3, &bursts);                                         // 4 coups à la double croche
    std::printf ("  d\xC3\xA9" "coupes : %.3f %.3f %.4f, redéclenchement : %d coups\n", a, b, c, bursts);
    CHECK (std::abs (a - 0.4f) < 0.05f, "part choisie par la page");
    CHECK (std::abs (b - 0.16f) < 0.03f, "part sur 8");
    CHECK (c < 1.0e-4f, "part inexistante muette");
    CHECK (bursts == 4, "redéclenchement");
}

// v0.16 : sampler à pads — un échantillon sur le pad 1 : chaque pad joue sa part (16 parts égales) ;
// en mode Porte, le son s'arrête au relâchement du pad
static void testSpSlices()
{
    const std::string name = "sampler \xC3\xA0 pads : d\xC3\xA9" "coupage";
    const auto* d = grv::engine::findInstrument ("sp");
    if (d == nullptr) { CHECK (false, "catalogue"); return; }
    static SampleBuffer sb = []
    {
        SampleBuffer b; b.sampleRate = 48000.0; b.data.resize (16 * 6000);
        for (size_t i = 0; i < b.data.size(); ++i) b.data[i] = (float) (0.03 * (double) (i / 6000 + 1) * std::sin ((double) i * 0.2));
        return b;
    }();
    auto play = [&] (int pad, bool gate, int holdBlocks)
    {
        auto inst = createInstrument ("sp");
        inst->prepare (48000.0, 32); inst->setTempo (120.0); inst->setSample (0, &sb);
        auto p = defaults (*d);
        p[(size_t) d->indexOf ("b1type")] = 0.0f;                 // bus sans effet
        p[(size_t) d->indexOf ("velamp")] = 0.0f;
        if (gate) p[(size_t) d->indexOf (("z" + std::string (pad < 9 ? "0" : "") + std::to_string (pad + 1) + "_mode").c_str())] = 1.0f;
        std::vector<float> L (32), R (32);
        inst->render (L.data(), R.data(), 32, p.data());
        inst->noteOn (pad, 1.0f);
        float peak = 0.0f, after = 0.0f;
        for (int b = 0; b < 200; ++b)
        {
            if (b == holdBlocks) inst->noteOff (pad);
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 32, p.data());
            for (float x : L) { peak = std::max (peak, std::abs (x)); if (b > holdBlocks + 80) after = std::max (after, std::abs (x)); }
        }
        return std::pair<float, float> { peak, after };
    };
    const auto [p5, unused] = play (5, false, 500);              // pad 6 : part 6 → 0,03 × 6 = 0,18
    const auto [pg, gateAfter] = play (2, true, 40);            // porte : relâché après 40 blocs (27 ms)
    std::printf ("  pad 6 : %.3f, porte : %.3f puis %.5f\n", p5, pg, gateAfter);
    CHECK (std::abs (p5 - 0.18f) < 0.03f, "part du pad");
    CHECK (pg > 0.05f && gateAfter < 1.0e-3f, "porte");
    (void) unused;
}

// v0.15 : sampler chromatique joué « en une fois » : la fin de la note ne coupe pas l'échantillon
static void testKeysOneShot()
{
    const std::string name = "sampler chromatique";
    const auto* d = grv::engine::findInstrument ("keys");
    if (d == nullptr) { CHECK (false, "catalogue"); return; }
    static SampleBuffer sb = [] { SampleBuffer b; b.sampleRate = 48000.0; for (int i = 0; i < 48000; ++i) b.data.push_back ((float) (0.5 * std::sin (i * 0.05))); return b; }();
    auto levelAfterRelease = [&] (float mode)
    {
        auto inst = createInstrument ("keys");
        inst->prepare (48000.0, 32); inst->setSample (0, &sb);
        auto p = defaults (*d);
        p[(size_t) d->indexOf ("trigmode")] = mode;
        p[(size_t) d->indexOf ("ar")] = 10.0f;
        std::vector<float> L (32), R (32);
        inst->noteOn (60, 1.0f);
        for (int b = 0; b < 100; ++b) inst->render (L.data(), R.data(), 32, p.data());
        inst->noteOff (60);
        float peak = 0.0f;
        for (int b = 0; b < 400; ++b) { std::fill (L.begin(), L.end(), 0.0f); inst->render (L.data(), R.data(), 32, p.data()); if (b > 300) for (float x : L) peak = std::max (peak, std::abs (x)); }
        return peak;
    };
    const auto gate = levelAfterRelease (0.0f), once = levelAfterRelease (1.0f);
    std::printf ("  chromatique : tenue %.4f, en une fois %.4f\n", gate, once);
    CHECK (gate < 1.0e-3f && once > 0.1f, "lecture en une fois");
}

// v0.15 : sampler à pads — chaque effet de bus sonne, reste borné et s'éteint ; bus en série ; direct
static void testSpSampler()
{
    const std::string name = "sampler \xC3\xA0 pads";
    const auto* d = grv::engine::findInstrument ("sp");
    CHECK (d != nullptr, "catalogue");
    if (d == nullptr) return;
    static SampleBuffer sb = []
    {
        SampleBuffer b;
        b.sampleRate = 48000.0;
        for (int i = 0; i < 24000; ++i) { const auto v = (float) (0.5 * std::sin (2 * kPi * 180 * i / 48000.0) * std::exp (-i / 9000.0)); b.data.push_back (v); }
        return b;
    }();
    auto renderWith = [&] (int type, double c1, double c2, double c3, int route, int bus, float& peak, float& tail, bool& finite)
    {
        auto inst = createInstrument ("sp");
        inst->prepare (48000.0, 32);
        inst->setSample (0, &sb);
        auto p = defaults (*d);
        p[(size_t) d->indexOf ("b1type")] = (float) type;
        p[(size_t) d->indexOf ("b1c1")] = (float) (c1 * 100); p[(size_t) d->indexOf ("b1c2")] = (float) (c2 * 100); p[(size_t) d->indexOf ("b1c3")] = (float) (c3 * 100);
        p[(size_t) d->indexOf ("b2type")] = route ? (float) grv::instruments::sp::fxReverb : 0.0f;
        p[(size_t) d->indexOf ("busroute")] = (float) route;
        p[(size_t) d->indexOf ("z01_bus")] = (float) bus;
        const int total = 48000 * 8;
        std::vector<float> L ((size_t) total), R ((size_t) total);
        peak = 0.0f; tail = 0.0f; finite = true;
        for (int pos = 0; pos < total; pos += 32)
        {
            if (pos % 24000 == 0 && pos < 48000 * 2) inst->noteOn (0, 1.0f);
            if (pos == 48000 * 2 && type == grv::instruments::sp::fxDjLoop) p[(size_t) d->indexOf ("b1c3")] = 0.0f;  // boucle DJ relâchée
            inst->setTempo (120.0);
            inst->render (L.data() + pos, R.data() + pos, 32, p.data());
        }
        for (int i = 0; i < total; ++i)
        {
            finite = finite && std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]);
            peak = std::max (peak, std::abs (L[(size_t) i]));
            if (i > total - 48000) tail = std::max (tail, std::abs (L[(size_t) i]));
        }
    };
    for (int type = 0; type < grv::instruments::sp::numFx; ++type)
    {
        float peak, tail; bool finite;
        renderWith (type, 0.5, 0.5, 0.6, 0, 0, peak, tail, finite);
        std::printf ("  bus %-22s cr\xC3\xAAte %.3f fin %.6f\n", grv::instruments::sp::fxNames()[(size_t) type].c_str(), peak, tail);
        CHECK (finite, "valeurs invalides");
        CHECK (peak > 0.05f && peak < 2.0f, ("niveau : " + grv::instruments::sp::fxNames()[(size_t) type]).c_str());
        CHECK (tail < 0.002f, ("ne s'\xC3\xA9teint pas : " + grv::instruments::sp::fxNames()[(size_t) type]).c_str());
    }
    // bus en série (lo-fi puis réverbération) et pad direct
    float peakSerial, tailSerial, peakDirect, tailDirect; bool f1, f2;
    renderWith (grv::instruments::sp::fxLofi, 0.5, 0.3, 0.3, 1, 0, peakSerial, tailSerial, f1);
    renderWith (grv::instruments::sp::fxLofi, 0.5, 0.3, 0.3, 0, 2, peakDirect, tailDirect, f2);
    CHECK (f1 && f2 && peakSerial > 0.05f && peakDirect > 0.05f && tailSerial < 0.002f && tailDirect < 0.002f, "s\xC3\xA9rie / direct");
}

// v0.18 : trois polys de nappes (tables d'ondes, filtre d'état, synthèse vectorielle à orbite)
static void testPadPolys()
{
    const std::string name = "nappes v0.18";
    auto descOf = [] (const char* type) { for (const auto* x : instrumentCatalog()) if (x->type == type) return x; return (const ModuleDesc*) nullptr; };
    // son fini, présent, sans excès, réglages donnés (au plus deux)
    auto play = [&] (const char* type, std::initializer_list<std::pair<const char*, float>> set, int blocks = 1500)
    {
        const auto& d = *descOf (type);
        auto inst = createInstrument (type);
        inst->prepare (48000.0, 64);
        inst->setTempo (120.0);
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); if (i >= 0) p[(size_t) i] = v; else std::printf ("  ECHEC r\xC3\xA9glage inconnu %s\n", id); }
        std::vector<float> L (64), R (64);
        inst->noteOn (48, 1.0f); inst->noteOn (60, 0.8f); inst->noteOn (67, 0.6f);
        float peak = 0.0f; bool finite = true;
        for (int k = 0; k < blocks; ++k)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 64, p.data());
            for (int i = 0; i < 64; ++i) { finite = finite && std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]); peak = std::max (peak, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i]))); }
        }
        return finite && peak > 0.005f && peak < 3.0f;
    };
    CHECK (descOf ("blo") && descOf ("ob6") && descOf ("orbit"), "au catalogue");
    if (! descOf ("blo") || ! descOf ("ob6") || ! descOf ("orbit")) return;
    for (const auto* t : { "blo", "ob6", "orbit" }) CHECK (descOf (t)->family == "Nappes", "famille Nappes");

    // positions des réglages (sauvegardes)
    auto at = [&] (const char* type, const char* id) { return descOf (type)->indexOf (id); };
    using namespace grv::instruments;
    CHECK (at ("blo", "e1mode") == blo::envBase && at ("blo", "l3delay") == blo::lfoBase + 11 && at ("blo", "fbalance") == blo::fBalance, "tables d'ondes : positions");
    CHECK (at ("ob6", "atlfo") == ob6::atLfo && at ("ob6", "fmode") == ob6::fMode && at ("ob6", "xmode") == ob6::xMode, "filtre d'\xC3\xA9tat : positions");
    CHECK (at ("orbit", "ag1") == orbit::cornerBase && at ("orbit", "dreso") == orbit::cornerBase + 31 && at ("orbit", "chorusdepth") == orbit::chorusDepth
           && at ("orbit", "gres") == orbit::globalReso && at ("orbit", "dw2") == orbit::waveBase + 7 && (int) descOf ("orbit")->params.size() >= orbit::numSlots, "orbiteur : positions");
    // position W : chaque générateur joue l'onde choisie dans la bibliothèque
    CHECK (descOf ("orbit")->params[(size_t) at ("orbit", "ag1")].choices.size() == 8 && descOf ("orbit")->params[(size_t) at ("orbit", "aw1")].choices.size() == 64, "orbiteur : W et 64 ondes");
    for (int w : { 0, 8, 35, 63 }) CHECK (play ("orbit", { { "ag1", 7.0f }, { "aw1", (float) w }, { "ablend", 60.0f } }, 600), "onde W du coin A");

    // tables d'ondes : formes et tables, filtres, enveloppes
    for (int sh = 0; sh < 16; ++sh) CHECK (play ("blo", { { "o1shape", (float) sh }, { "o1pos", 60.0f } }, 600), "forme ou table de l'oscillateur 1");
    for (int f = 0; f < 11; ++f) CHECK (play ("blo", { { "f1type", (float) f }, { "f1reso", 90.0f } }, 600), "type du filtre 1, r\xC3\xA9sonance forte");
    for (int c = 0; c < 4; ++c) CHECK (play ("blo", { { "f1drive", 80.0f }, { "f1curve", (float) c } }, 600), "courbe de saturation");
    for (int m = 0; m < 5; ++m) CHECK (play ("blo", { { "e2mode", (float) m } }, 600), "mode de l'enveloppe d'ampli");
    CHECK (play ("blo", { { "routing", 1.0f }, { "f2type", 5.0f } }), "filtres en parall\xC3\xA8le");
    CHECK (play ("blo", { { "vmode", 2.0f }, { "univoices", 8.0f } }), "unisson 8 voix");
    CHECK (play ("blo", { { "o1fm", 90.0f }, { "o2sync", 1.0f } }), "FM et synchro");

    // filtre d'état : mode continu, passe-bande, X-Mod, unisson
    for (float m : { 0.0f, 50.0f, 100.0f }) CHECK (play ("ob6", { { "fmode", m }, { "reso", 85.0f } }, 600), "mode du filtre");
    CHECK (play ("ob6", { { "bandpass", 1.0f } }), "passe-bande");
    CHECK (play ("ob6", { { "xoscb", 80.0f }, { "xfreqa", 1.0f } }), "X-Mod osc. 2 → fr\xC3\xA9quence 1");
    CHECK (play ("ob6", { { "unison", 1.0f }, { "univoices", 8.0f } }), "unisson");
    CHECK (play ("ob6", { { "lfoamt", 80.0f }, { "lfoamp", 1.0f } }), "tr\xC3\xA9molo du LFO");

    // orbiteur : trajectoires, modulations des coins, filtres, pile mono, bruit
    for (int sh = 0; sh < 7; ++sh) CHECK (play ("orbit", { { "shape", (float) sh }, { "speed", 7.0f } }, 600), "trajectoire");
    for (int m = 0; m < 5; ++m) CHECK (play ("orbit", { { "ablend", 80.0f }, { "abmode", (float) m } }, 600), "modulation du coin A");
    for (int f = 0; f < 6; ++f) CHECK (play ("orbit", { { "aftype", (float) f }, { "areso", 95.0f } }, 600), "filtre du coin A");
    CHECK (play ("orbit", { { "vmode", 1.0f }, { "stack", 8.0f } }), "pile mono 8 voix");
    CHECK (play ("orbit", { { "ag1", 6.0f }, { "cx", 0.0f } }), "bruit dans le coin A");
    CHECK (play ("orbit", { { "drive", 90.0f }, { "tremdepth", 80.0f } }), "saturation et tr\xC3\xA9molo");

    // l'orbe bouge (Lissajous rapide) et l'écran le reçoit ; la position mélange les coins
    {
        const auto& d = *descOf ("orbit");
        auto inst = createInstrument ("orbit");
        inst->prepare (48000.0, 64);
        inst->setTempo (120.0);
        auto p = defaults (d);
        p[(size_t) d.indexOf ("shape")] = 2.0f; p[(size_t) d.indexOf ("speed")] = 7.0f; p[(size_t) d.indexOf ("size")] = 100.0f;
        std::vector<float> L (64), R (64);
        inst->noteOn (60, 1.0f);
        float st[8] {}, minX = 1.0f, maxX = 0.0f;
        int n = 0;
        for (int k = 0; k < 800; ++k)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 64, p.data());
            n = inst->status (st, 8);
            minX = std::min (minX, st[0]); maxX = std::max (maxX, st[0]);
        }
        std::printf ("  orbiteur : orbe bleu x de %.2f \xC3\xA0 %.2f, %d valeurs d'\xC3\xA9tat, joue %.0f\n", minX, maxX, n, st[4]);
        CHECK (n == 5 && st[4] > 0.5f && maxX - minX > 0.8f, "l'orbe parcourt la trajectoire");
        inst->allNotesOff();
        for (int k = 0; k < 3000; ++k) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); inst->render (L.data(), R.data(), 64, p.data()); }
        inst->status (st, 8);
        CHECK (st[4] < 0.5f, "plus aucune voix apr\xC3\xA8s rel\xC3\xA2" "chement");
    }
}

// v0.21 : boîte Impact (couleur, distorsion de sortie), batterie Nerve (précalcul, GATE, compresseur
// interne), percussion à séquenceur (avance à chaque note, longueur, mode note jouée)
static void testDrums21()
{
    const std::string name = "batteries v0.21";
    auto descOf = [] (const char* type) { for (const auto* x : instrumentCatalog()) if (x->type == type) return x; return (const ModuleDesc*) nullptr; };
    CHECK (descOf ("drumImp") && descOf ("drumNrv") && descOf ("dfam"), "au catalogue");
    if (! descOf ("drumImp") || ! descOf ("drumNrv") || ! descOf ("dfam")) return;
    // énergie d'un rendu : pads frappés au départ (et pad « extra » à 50 ms), réglages donnés
    auto energy = [&] (const char* type, std::initializer_list<int> hits, std::initializer_list<std::pair<const char*, float>> set, int extra = -1, int blocks = 750)
    {
        const auto& d = *descOf (type);
        auto inst = createInstrument (type);
        inst->prepare (48000.0, 64);
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); if (i >= 0) p[(size_t) i] = v; else std::printf ("  ECHEC r\xC3\xA9glage inconnu %s\n", id); }
        std::vector<float> L (64), R (64);
        for (int h : hits) inst->noteOn (h, 1.0f);
        double e = 0.0; bool finite = true;
        for (int k = 0; k < blocks; ++k)
        {
            if (k == 37 && extra >= 0) inst->noteOn (extra, 1.0f);
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 64, p.data());
            for (int i = 0; i < 64; ++i) { finite = finite && std::isfinite (L[(size_t) i]); e += (double) L[(size_t) i] * L[(size_t) i] + (double) R[(size_t) i] * R[(size_t) i]; }
        }
        return finite ? e : -1.0;
    };
    // Impact : chaque voix sonne, avec et sans couleur ; la couleur change le son ; la distorsion aussi
    for (int pad = 0; pad < 13; ++pad)
    {
        const auto pre = "p" + std::string (pad < 9 ? "0" : "") + std::to_string (pad + 1) + "_x2";
        const auto plain = energy ("drumImp", { pad }, { { pre.c_str(), 0.0f } }), col = energy ("drumImp", { pad }, { { pre.c_str(), 100.0f } });
        std::printf ("  Impact pad %2d : sans couleur %.3f, avec %.3f\n", pad + 1, plain, col);
        CHECK (plain > 1.0e-3 && col > 1.0e-3, "voix de la bo\xC3\xAEte Impact");
        // la cloche n'a pas de couleur ; celle du tambour FM change la hauteur (énergie voisine)
        if (pad != 6 && pad != 9 && pad != 12) CHECK (std::abs (col - plain) > 0.05 * plain && col < plain * 6.0, "couleur qui change le son sans l'\xC3\xA9" "craser");
    }
    const auto dry = energy ("drumImp", { 0, 1, 7 }, {}), dist = energy ("drumImp", { 0, 1, 7 }, { { "kitdist", 80.0f } });
    std::printf ("  Impact : distorsion 0 %% %.2f, 80 %% %.2f\n", dry, dist);
    CHECK (dist > dry * 1.3, "distorsion de sortie");
    // Nerve : synthèse ajoutée, sous-harmonique, GATE, compresseur interne
    const auto base = energy ("drumNrv", { 2 }, {}), synth = energy ("drumNrv", { 2 }, { { "p03_pcw", 1.0f }, { "p03_pcl", 80.0f } });
    const auto gated = energy ("drumNrv", { 2 }, { { "p03_gate", 15.0f } });
    const auto crushed = energy ("drumNrv", { 2 }, { { "p03_bits", 3.0f }, { "p03_clip", 80.0f } });
    // le pad 16 (sans échantillon : muet) déclenche le compresseur en même temps que la caisse claire
    const auto open = energy ("drumNrv", { 15, 2 }, {}), ducked = energy ("drumNrv", { 15, 2 }, { { "duckpad", 16.0f }, { "duckamt", 100.0f } });
    std::printf ("  Nerve : caisse claire %.2f, + sinus %.2f, gate 15 %% %.2f, 3 bits + \xC3\xA9" "cr\xC3\xAA" "tage %.2f, compresseur %.2f -> %.2f\n", base, synth, gated, crushed, open, ducked);
    CHECK (base > 1.0e-3 && synth > base * 1.2 && gated < base * 0.6 && crushed > 0.0 && std::abs (crushed - base) > 0.05 * base && ducked < open * 0.9, "pr\xC3\xA9" "calcul, gate et compresseur");
    // percussion à séquenceur : le pas avance à chaque note, boucle sur la longueur, mode note jouée
    {
        const auto& d = *descOf ("dfam");
        CHECK (d.indexOf ("v8") == grv::instruments::dfam::stepVel + 7 && d.indexOf ("drift") == grv::instruments::dfam::drift && d.indexOf ("p1") == grv::instruments::dfam::stepPitch, "percussion : positions");
        auto inst = createInstrument ("dfam");
        inst->prepare (48000.0, 64);
        auto p = defaults (d);
        p[(size_t) d.indexOf ("seqlen")] = 3.0f;
        std::vector<float> L (64), R (64);
        float st[4] {};
        std::vector<int> seen;
        double peak = 0.0;
        for (int k = 0; k < 5; ++k)
        {
            inst->noteOn (60, 1.0f);
            for (int b = 0; b < 40; ++b)
            {
                std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
                inst->render (L.data(), R.data(), 64, p.data());
                for (float x : L) peak = std::max (peak, (double) std::abs (x));
            }
            inst->status (st, 4);
            seen.push_back ((int) st[0]);
        }
        std::printf ("  percussion : pas %d %d %d %d %d, cr\xC3\xAA" "te %.2f\n", seen[0], seen[1], seen[2], seen[3], seen[4], peak);
        CHECK (seen == std::vector<int> ({ 0, 1, 2, 0, 1 }) && peak > 0.01 && peak < 3.0, "s\xC3\xA9quenceur 8 pas");
        inst->transportStarted();
        inst->noteOn (60, 1.0f);
        std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
        inst->render (L.data(), R.data(), 64, p.data());
        inst->status (st, 4);
        CHECK ((int) st[0] == 0, "s\xC3\xA9quence relanc\xC3\xA9" "e au d\xC3\xA9part de la lecture");
        p[(size_t) d.indexOf ("seqmode")] = 1.0f;
        inst->noteOn (72, 1.0f);
        inst->render (L.data(), R.data(), 64, p.data());
        inst->status (st, 4);
        CHECK ((int) st[0] == -1, "mode note jou\xC3\xA9" "e");
        for (int f = 0; f < 2; ++f)
            for (int w = 0; w < 2; ++w)
            {
                auto q = defaults (d);
                q[(size_t) d.indexOf ("vcfmode")] = (float) f; q[(size_t) d.indexOf ("vco1wave")] = (float) w; q[(size_t) d.indexOf ("reso")] = 100.0f;
                q[(size_t) d.indexOf ("sync")] = 1.0f; q[(size_t) d.indexOf ("fm")] = 100.0f; q[(size_t) d.indexOf ("mixnoise")] = 100.0f; q[(size_t) d.indexOf ("noisevcf")] = 100.0f;
                auto x = createInstrument ("dfam");
                x->prepare (48000.0, 64);
                x->noteOn (60, 1.0f);
                bool finite = true; double pk = 0.0;
                for (int b = 0; b < 300; ++b)
                {
                    std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
                    x->render (L.data(), R.data(), 64, q.data());
                    for (float v : L) { finite = finite && std::isfinite (v); pk = std::max (pk, (double) std::abs (v)); }
                }
                CHECK (finite && pk > 0.005 && pk < 4.0, "percussion : filtre, formes, FM, synchro, r\xC3\xA9sonance max");
            }
    }
    // orbiteur : la phase d'un générateur change le mélange de deux générateurs (A + C en somme)
    {
        const auto& d = *descOf ("orbit");
        CHECK (d.indexOf ("dph2") == grv::instruments::orbit::phaseBase + 7, "orbiteur : positions des phases");
    }
}

static void testSlotIds()
{
    const std::string name = "positions";
    auto at = [] (const char* type, const char* id) { for (const auto* d : instrumentCatalog()) if (d->type == type) return d->indexOf (id); return -2; };
    CHECK (at ("hybrid", "l1delay") == grv::instruments::hybrid::l1Delay && at ("hybrid", "l2delay") == grv::instruments::hybrid::l2Delay, "hybride : retards des LFO");
    CHECK (at ("morph", "l1delay") == grv::instruments::morph::l1Delay && at ("morph", "l2delay") == grv::instruments::morph::l2Delay, "wave morph : retards des LFO");
    CHECK (at ("pads", "l1delay") == grv::instruments::pads::l1Delay && at ("pads", "l2delay") == grv::instruments::pads::l2Delay, "nappes : retards des LFO");
    CHECK (at ("ms20", "mgdelay") == grv::instruments::ms20::mgDelay && at ("ms20", "level") == grv::instruments::ms20::level, "mono semi-modulaire");
    CHECK (at ("sh101", "lfodelay") == grv::instruments::sh101::lfoDelay && at ("sh101", "level") == grv::instruments::sh101::level, "mono \xC3\xA0 m\xC3\xA9langeur");
    CHECK (at ("sp", "busroute") == grv::instruments::sp::gRoute && at ("sp", "z16_bus") == 15 * grv::instruments::sp::padParams + grv::instruments::sp::pBus, "sampler \xC3\xA0 pads");
    CHECK (at ("octa", "t1_srr") == grv::instruments::octa::srr && at ("octa", "t1_rt16") == grv::instruments::octa::recTrack1 + 15
           && at ("octa", "t3_tune") == grv::instruments::octa::unitParam (2, grv::instruments::octa::tune)
           && at ("octa", "e17st") == grv::instruments::octa::attrSlot (16, grv::instruments::octa::aStart), "sampler de performance");
}


// v0.22 : qualité du son — anti-clics, anti-repliement, lissages, gel borné (sans changer les fonctions)
static double maxD2 (const std::vector<float>& x, int from, int to)
{
    double m = 0.0;
    for (int i = std::max (2, from); i < std::min ((int) x.size(), to); ++i) m = std::max (m, std::abs ((double) x[(size_t) i] - 2.0 * x[(size_t) i - 1] + x[(size_t) i - 2]));
    return m;
}

/** Part de l'énergie hors des harmoniques de f0 (repliement), en dB. */
static double inharmonicDb (const std::vector<float>& x, int start, double f0, double sr)
{
    constexpr int N = 16384;
    std::vector<std::complex<double>> b ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const auto t = (double) i / (N - 1);
        const auto w = 0.35875 - 0.48829 * std::cos (2 * kPi * t) + 0.14128 * std::cos (4 * kPi * t) - 0.01168 * std::cos (6 * kPi * t);
        b[(size_t) i] = (double) x[(size_t) (start + i)] * w;
    }
    grv::synth::fft (b, false);
    double harm = 0.0, other = 0.0;
    for (int k = 4; k < N / 2; ++k)
    {
        const auto f = (double) k * sr / N;
        const auto e = std::norm (b[(size_t) k]);
        const auto h = f / f0, near = std::abs (h - std::round (h)) * f0 / (sr / N);
        if (std::round (h) >= 1.0 && near <= 4.0) harm += e; else other += e;
    }
    return 10.0 * std::log10 ((other + 1.0e-30) / (harm + 1.0e-30));
}

static void testSound22()
{
    const std::string name = "qualit\xC3\xA9 du son v0.22";
    constexpr double sr = 48000.0;

    // 1. Note rejouée pendant qu'elle sonne : chromatique (l'échantillon repart du début)
    {
        const auto* d = grv::engine::findInstrument ("keys");
        static SampleBuffer sb = [] { SampleBuffer b; b.sampleRate = 48000.0; for (int i = 0; i < 96000; ++i) b.data.push_back ((float) (0.5 * std::sin (2 * kPi * 220.0 * i / 48000.0))); return b; }();
        auto inst = createInstrument ("keys");
        inst->prepare (sr, 32); inst->setSample (0, &sb);
        auto p = defaults (*d);
        std::vector<float> out, L (32), R (32);
        inst->noteOn (57, 1.0f);
        for (int blk = 0; blk < 600; ++blk)
        {
            if (blk == 307) inst->noteOn (57, 1.0f);
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 32, p.data());
            out.insert (out.end(), L.begin(), L.end());
        }
        const auto steady = maxD2 (out, 4000, 9800), jump = maxD2 (out, 307 * 32 - 2, 307 * 32 + 96);
        std::printf ("  note rejou\xC3\xA9" "e (chromatique) : saut %.4f, r\xC3\xA9gime %.4f\n", jump, steady);
        CHECK (jump < 0.06, "note rejou\xC3\xA9" "e sans clic (chromatique)");
    }

    // 2. Pad de batterie relancé pendant qu'il sonne : plus de saut (au plus un coude de pente)
    {
        auto inst = createInstrument ("drumImp");
        const auto* d = grv::engine::findInstrument ("drumImp");
        inst->prepare (sr, 32);
        auto p = defaults (*d);
        std::vector<float> out, L (32), R (32);
        inst->noteOn (3, 1.0f);
        for (int blk = 0; blk < 120; ++blk)
        {
            if (blk == 47) inst->noteOn (3, 1.0f);
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), 32, p.data());
            out.insert (out.end(), L.begin(), L.end());
        }
        const auto first = maxD2 (out, 0, 96), again = maxD2 (out, 47 * 32 - 2, 47 * 32 + 96);
        std::printf ("  pad relanc\xC3\xA9 : 1re frappe %.4f, relance %.4f\n", first, again);
        // sans l'anti-clic : 0,12 (saut franc) ; il reste un simple coude de pente
        CHECK (again < 0.04, "pad relanc\xC3\xA9 sans clic");
    }

    // 3. Tête de lecture : début déplacé et fin coupée en fondu
    {
        static SampleBuffer dc = [] { SampleBuffer b; b.sampleRate = 48000.0; b.data.assign (48000, 0.5f); return b; }();
        grv::synth::SamplePlayer pl;
        pl.setSample (&dc);
        grv::synth::SamplePlayer::Region r; r.start = 0.3; r.end = 0.5;
        pl.start (r);
        double l, rr, first = 0.0, last = 1.0;
        pl.next (1.0, first, rr);
        while (pl.isActive()) { pl.next (1.0, l, rr); if (pl.isActive()) last = l; }
        std::printf ("  t\xC3\xAA" "te de lecture : 1er %.4f, dernier %.4f\n", first, last);
        CHECK (first < 0.02 && last < 0.02, "fondus de d\xC3\xA9" "but et de fin");
    }

    // 4. Tables d'ondes : pas de saut de timbre au passage d'une octave
    {
        grv::synth::WaveBank wb;
        wb.addWave ([] (int h) { return std::complex<double> (0.0, -1.0 / h); });
        const auto dtB = 0.5 / 256.0; // frontière entre deux niveaux
        double diff = 0.0, before = 0.0;
        for (int k = 0; k < 64; ++k)
        {
            const auto ph = (k + 0.5) / 64.0;
            diff = std::max (diff, (double) std::abs (wb.read (0, grv::synth::WaveBank::mipFor (dtB * 0.999), ph) - wb.read (0, grv::synth::WaveBank::mipFor (dtB * 1.001), ph)));
            before = std::max (before, (double) std::abs (wb.read (0, grv::synth::WaveBank::levelFor (dtB * 0.9 * 0.999), ph) - wb.read (0, grv::synth::WaveBank::levelFor (dtB * 0.9 * 1.001), ph)));
        }
        std::printf ("  tables : \xC3\xA9" "cart \xC3\xA0 la fronti\xC3\xA8re %.5f (niveau seul : %.5f)\n", diff, before);
        CHECK (diff < 0.01, "fondu entre niveaux de table");
    }

    // 5. Distorsion poussée (×4 depuis la v0.22) : repliement mesuré sur un sinus de 5 kHz
    //    (mesuré à ×2 : lampe −22,5 dB, fuzz −18,4 dB ; à ×4 : −36,8 et −27,1 dB)
    {
        const auto* d = grv::engine::findEffect ("dist");
        auto measure = [&] (float type) {
            auto fx = createEffect ("dist");
            fx->prepare (sr, 64);
            auto p = defaults (*d);
            p[(size_t) d->indexOf ("typeA")] = type; p[(size_t) d->indexOf ("driveA")] = 80.0f;
            const int total = 32768;
            std::vector<float> L ((size_t) total), R ((size_t) total);
            for (int i = 0; i < total; ++i) L[(size_t) i] = R[(size_t) i] = (float) (0.4 * std::sin (2 * kPi * 4987.0 * i / sr));
            for (int pos = 0; pos < total; pos += 64) fx->process (L.data() + pos, R.data() + pos, 64, p.data(), 120.0);
            return inharmonicDb (L, 12000, 4987.0, sr);
        };
        const auto lamp = measure (1.0f), fuzz = measure (6.0f);
        std::printf ("  distorsion \xC3\xA0 80 %%, 5 kHz : repliement lampe %.1f dB, fuzz %.1f dB\n", lamp, fuzz);
        CHECK (lamp < -33.0 && fuzz < -25.0, "distorsion : repliement contenu");
    }

    // 6. Sous-oscillateur et scie (mono à mélangeur) à une note aiguë
    {
        const auto* d = grv::engine::findInstrument ("sh101");
        auto inst = createInstrument ("sh101");
        inst->prepare (sr, 32);
        auto p = defaults (*d);
        auto set = [&] (const char* id, float v) { p[(size_t) d->indexOf (id)] = v; };
        set ("saw", 100); set ("pulse", 0); set ("sub", 100); set ("subtype", 0); set ("noise", 0);
        set ("cutoff", 20000); set ("reso", 0); set ("envamt", 0); set ("sustain", 100); set ("attack", 1); set ("vcolfo", 0); set ("lfoamt", 0);
        std::vector<float> out, L (32), R (32);
        inst->noteOn (96, 1.0f);
        for (int blk = 0; blk < 1100; ++blk) { std::fill (L.begin(), L.end(), 0.0f); inst->render (L.data(), R.data(), 32, p.data()); out.insert (out.end(), L.begin(), L.end()); }
        const auto f0 = 440.0 * std::pow (2.0, (96.0 - 69.0) / 12.0) * 0.5; // le sous-oscillateur divise par 2
        const auto db = inharmonicDb (out, 12000, f0, sr);
        std::printf ("  mono \xC3\xA0 m\xC3\xA9langeur, note 96 : repliement %.1f dB\n", db);
        CHECK (db < -35.0, "scie et sous-oscillateur : repliement contenu");
    }

    // 7. Mono semi-modulaire, modulation en anneau
    {
        const auto* d = grv::engine::findInstrument ("ms20");
        auto inst = createInstrument ("ms20");
        inst->prepare (sr, 32);
        auto p = defaults (*d);
        auto set = [&] (const char* id, float v) { p[(size_t) d->indexOf (id)] = v; };
        set ("v1level", 0); set ("v2level", 100); set ("v2wave", 3); set ("v1wave", 1);
        set ("lpcut", 20000); set ("hpcut", 20); set ("lppeak", 0); set ("hppeak", 0); set ("e2sustain", 100); set ("e2attack", 1); set ("drive", 0);
        std::vector<float> out, L (32), R (32);
        inst->noteOn (84, 1.0f);
        for (int blk = 0; blk < 1100; ++blk) { std::fill (L.begin(), L.end(), 0.0f); inst->render (L.data(), R.data(), 32, p.data()); out.insert (out.end(), L.begin(), L.end()); }
        const auto f0 = 440.0 * std::pow (2.0, (84.0 - 69.0) / 12.0);
        const auto db = inharmonicDb (out, 12000, f0, sr);
        std::printf ("  anneau, note 84 : repliement %.1f dB\n", db);
        CHECK (db < -30.0, "anneau : repliement contenu");
    }

    // 8. Ambiance gelée avec chatoiement : le niveau ne monte plus
    {
        const auto* d = grv::engine::findEffect ("atmos");
        auto fx = createEffect ("atmos");
        fx->prepare (sr, 64);
        auto p = defaults (*d);
        p[(size_t) d->indexOf ("mode")] = 2.0f; p[(size_t) d->indexOf ("shimmer")] = 80.0f; p[(size_t) d->indexOf ("mix")] = 100.0f;
        std::vector<float> L (64), R (64);
        std::uint32_t rng = 1;
        auto run = [&] (int blocks, bool input) { float pk = 0.0f; for (int b = 0; b < blocks; ++b) { for (int i = 0; i < 64; ++i) { rng = rng * 1664525u + 1013904223u; L[(size_t) i] = R[(size_t) i] = input ? (float) ((double) (rng >> 8) / 16777216.0 - 0.5) * 0.5f : 0.0f; } fx->process (L.data(), R.data(), 64, p.data(), 120.0); for (float x : L) pk = std::max (pk, std::abs (x)); } return pk; };
        run (750, true);
        p[(size_t) d->indexOf ("freeze")] = 1.0f;
        const auto a = run (750, false);
        run (6000, false);
        const auto b = run (750, false);
        std::printf ("  ambiance gel\xC3\xA9" "e : 1re seconde %.3f, apr\xC3\xA8s 9 s %.3f\n", a, b);
        CHECK (std::isfinite (b) && b < 1.5 * a + 0.01 && b < 2.0f, "gel born\xC3\xA9");
    }

    // 9. Filtre à diodes : borné à résonance maximale sur toute la plage
    {
        grv::synth::DiodeLadder f;
        f.prepare (sr);
        double pk = 0.0; bool finite = true;
        for (double fc : { 60.0, 300.0, 1000.0, 4000.0, 12000.0 })
        {
            f.reset();
            double ph = 0.0;
            for (int i = 0; i < 24000; ++i) { ph += 110.0 / sr; ph -= std::floor (ph); const auto y = f.process (2.0 * ph - 1.0, fc, 1.0); finite = finite && std::isfinite (y); pk = std::max (pk, std::abs (y)); }
        }
        std::printf ("  filtre \xC3\xA0 diodes, r\xC3\xA9sonance max : cr\xC3\xAA" "te %.2f\n", pk);
        CHECK (finite && pk < 4.0, "filtre \xC3\xA0 diodes born\xC3\xA9");
    }
}


// v0.23 : side-chain — compresseur piloté par une autre piste ; suiveur sur l'audio ou les notes
static void testSidechain23()
{
    const std::string name = "side-chain v0.23";
    constexpr double sr = 48000.0;
    for (const char* type : { "comp", "bus", "fet", "opto" })
    {
        const auto* d = grv::engine::findEffect (type);
        if (d == nullptr) { CHECK (false, "catalogue"); continue; }
        auto run = [&] (bool kick) {
            auto fx = createEffect (type);
            fx->prepare (sr, 32);
            auto p = defaults (*d);
            p[(size_t) d->indexOf ("scsrc")] = 3.0f; // « Piste 3 »
            if (d->indexOf ("threshold") >= 0) p[(size_t) d->indexOf ("threshold")] = -30.0f;
            std::array<std::array<float, 32>, 16> scl {}, scr {};
            const float* ptrL[16]; const float* ptrR[16];
            for (int k = 0; k < 16; ++k) { ptrL[k] = scl[(size_t) k].data(); ptrR[k] = scr[(size_t) k].data(); }
            double inE = 0.0, outE = 0.0;
            for (int b = 0; b < 600; ++b)
            {
                float L[32], R[32];
                for (int i = 0; i < 32; ++i)
                {
                    const auto t = b * 32 + i;
                    L[i] = R[i] = (float) (0.05 * std::sin (2 * kPi * 110.0 * t / sr)); // basse douce
                    scl[2][(size_t) i] = scr[2][(size_t) i] = kick ? (float) (0.9 * std::sin (2 * kPi * 55.0 * t / sr)) : 0.0f; // grosse caisse sur la piste 3
                }
                fx->setSidechainSources (ptrL, ptrR);
                fx->process (L, R, 32, p.data(), 120.0);
                if (b > 300) for (int i = 0; i < 32; ++i) { inE += 0.0025 * 0.5; outE += (double) L[i] * L[i]; }
            }
            return std::sqrt (outE / (299.0 * 32.0));
        };
        const auto quiet = run (false), ducked = run (true);
        std::printf ("  side-chain %-5s : sans la piste 3 %.4f, avec %.4f\n", type, quiet, ducked);
        CHECK (ducked < quiet * 0.7, (std::string ("compresseur ") + type + " pilot\xC3\xA9 par la piste 3").c_str());
    }
    // suiveur : notes d'une autre piste
    {
        grv::engine::ModSource m;
        std::vector<float> p (grv::engine::modSourceSlots, 0.0f);
        const auto& d = grv::engine::modSourceDesc();
        for (size_t i = 0; i < d.params.size(); ++i) p[i] = d.params[i].def;
        p[grv::engine::msType] = 2.0f;
        p[grv::engine::msFollowSrc] = (float) (grv::engine::numTracks + 1 + 4); // notes de la piste 5
        p[grv::engine::msAttack] = 2.0f; p[grv::engine::msRelease] = 100.0f;
        const auto dt = 32.0 / sr;
        float before = m.advance (p.data(), dt, 120.0, 0.0, true, 0.0f);
        m.sideNote (1.0f);
        float peak = 0.0f;
        for (int k = 0; k < 20; ++k) peak = std::max (peak, m.advance (p.data(), dt, 120.0, 0.0, true, 0.0f));
        float later = 0.0f;
        for (int k = 0; k < 1500; ++k) later = m.advance (p.data(), dt, 120.0, 0.0, true, 0.0f);
        std::printf ("  suiveur sur les notes : %.3f -> %.3f -> %.4f\n", before, peak, later);
        CHECK (before < 0.01f && peak > 0.8f && later < 0.05f, "suiveur sur les notes d'une piste");
    }
}

/** Vocodeurs (v0.25) : chaque variante parle, sans écart de niveau excessif ; sources du modulateur,
    gel, silence sans note, coût. */
static void testVocoders25()
{
    const std::string name = "vocodeurs v0.25";
    constexpr int sr = 48000, block = 32;
    struct Out { double rmsDb = -200.0; bool finite = true; double sec = 0.0; };
    // joue un accord pendant 'seconds', la voix interne ou le signal 'mod' (entrée) comme modulateur
    auto play = [&] (const char* type, const std::vector<std::pair<std::string, float>>& set, double seconds, bool notes,
                     const std::vector<float>* mod = nullptr, int modVia = 0)
    {
        Out o;
        auto inst = createInstrument (type);
        const auto& d = inst->desc();
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); CHECK (i >= 0, id.c_str()); if (i >= 0) p[(size_t) i] = v; }
        inst->prepare (sr, block);
        if (notes) for (int nn : { 48, 52, 55, 60 }) inst->noteOn (nn, 0.9f);
        const int total = (int) (seconds * sr);
        std::vector<float> L ((size_t) block), R ((size_t) block), mL ((size_t) block);
        std::array<const float*, 16> ptrs {};
        double sum = 0.0; long cnt = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int pos = 0; pos < total; pos += block)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            for (int k = 0; k < block; ++k) mL[(size_t) k] = mod != nullptr ? (*mod)[(size_t) ((pos + k) % (int) mod->size())] : 0.0f;
            if (modVia == 0) inst->setInput (mL.data(), mL.data());
            if (modVia == 1) inst->setNeighbor (mL.data(), mL.data());
            if (modVia == 2) { ptrs.fill (nullptr); for (auto& q : ptrs) q = mL.data(); inst->setSidechainSources (ptrs.data(), ptrs.data()); }
            inst->render (L.data(), R.data(), block, p.data());
            for (int k = 0; k < block; ++k)
            {
                o.finite = o.finite && std::isfinite (L[(size_t) k]) && std::isfinite (R[(size_t) k]);
                if (pos >= total / 2) { sum += (double) L[(size_t) k] * L[(size_t) k] + (double) R[(size_t) k] * R[(size_t) k]; ++cnt; }
            }
        }
        o.sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / seconds;
        o.rmsDb = 10.0 * std::log10 (sum / (2.0 * std::max (1L, cnt)) + 1.0e-30);
        return o;
    };

    struct Variant { const char* type; const char* label; std::vector<std::pair<std::string, float>> set; };
    std::vector<Variant> vars {
        { "voc16", "16 bandes", {} },
        { "vocstr", "cordes 16", { { "bands", 0 } } }, { "vocstr", "cordes 24", { { "bands", 1 } } },
        { "vocstr", "cordes 32", { { "bands", 2 } } }, { "vocstr", "cordes 40", { { "bands", 3 } } },
        { "vocsyn", "vocodeur", { { "mvoc", 100 }, { "mpoly", 0 } } },
        { "vocsyn", "ordinateur", { { "mvoc", 0 }, { "mcomp", 100 }, { "mpoly", 0 } } },
        { "vocsyn", "bo\xC3\xAEte", { { "mvoc", 0 }, { "mtalk", 100 }, { "mpoly", 0 } } },
        { "vocsyn", "voix mod.", { { "mvoc", 0 }, { "mbio", 100 }, { "mpoly", 0 } } },
    };
    for (int a = 0; a < 8; ++a) vars.push_back ({ "vocspec", grv::instruments::voc::algos[a].name, { { "algo", (float) a } } });
    double worst = 0.0;
    std::string lastType;
    double lo = 0.0, hi = -200.0;
    auto closeGroup = [&]
    {
        if (lastType.empty()) return;
        std::printf ("  %-8s : \xC3\xA9" "cart entre variantes %.1f dB\n", lastType.c_str(), hi - lo);
        CHECK (hi - lo < 4.0, (lastType + " : variantes de niveaux trop diff\xC3\xA9rents").c_str());
    };
    for (const auto& v : vars)
    {
        if (lastType != v.type) { closeGroup(); lastType = v.type; lo = 0.0; hi = -200.0; }
        const auto o = play (v.type, v.set, 4.0, true);
        worst = std::max (worst, o.sec);
        lo = std::min (lo, o.rmsDb); hi = std::max (hi, o.rmsDb);
        std::printf ("  %-8s %-24s %6.1f dB RMS  calcul %4.1f %%\n", v.type, v.label, o.rmsDb, o.sec * 100.0);
        CHECK (o.finite, "valeurs invalides");
        CHECK (o.rmsDb > -40.0 && o.rmsDb < -6.0, (std::string (v.type) + " " + v.label + " : niveau").c_str());
    }
    closeGroup();
    std::printf ("  vocodeurs : calcul le plus lourd %.1f %% d'un c\xC5\x93ur\n", worst * 100.0);
    CHECK (worst < 0.12, "vocodeur trop co\xC3\xBBteux");

    // sans note : silence (la voix interne parle mais rien ne sonne)
    for (const char* t : { "voc16", "vocstr", "vocspec", "vocsyn" })
    {
        const auto o = play (t, { { "dry", 50 } }, 2.0, false);
        CHECK (o.rmsDb < -80.0, (std::string (t) + " : sonne sans note").c_str());
    }

    // sources : entrée audio, piste précédente, piste 5 (une « voix » : impulsions filtrées)
    std::vector<float> voice ((size_t) sr);
    {
        grv::synth::DemoVoice dv; dv.prepare (sr);
        for (auto& x : voice) x = (float) dv.next (3.0, 140.0);
    }
    const auto viaInput = play ("voc16", { { "src", 1 } }, 2.0, true, &voice, 0);
    const auto viaPrev = play ("voc16", { { "src", 2 } }, 2.0, true, &voice, 1);
    const auto viaTrack = play ("voc16", { { "src", 3 + 4 } }, 2.0, true, &voice, 2);
    const auto noMod = play ("voc16", { { "src", 1 } }, 2.0, true, nullptr, 0);
    std::printf ("  sources : entr\xC3\xA9" "e %.1f dB, piste pr\xC3\xA9" "c\xC3\xA9" "dente %.1f dB, piste 5 %.1f dB, sans voix %.1f dB\n", viaInput.rmsDb, viaPrev.rmsDb, viaTrack.rmsDb, noMod.rmsDb);
    CHECK (viaInput.rmsDb > -40.0 && viaPrev.rmsDb > -40.0 && viaTrack.rmsDb > -40.0, "sources du modulateur");
    CHECK (noMod.rmsDb < -70.0, "porteuse seule : silence");
    {
        auto inst = createInstrument ("voc16");
        auto p = defaults (inst->desc());
        p[(size_t) inst->desc().indexOf ("src")] = 2.0f;
        CHECK (inst->absorbsNeighbor (p.data()), "piste pr\xC3\xA9" "c\xC3\xA9" "dente retir\xC3\xA9" "e du mixage");
        p[(size_t) inst->desc().indexOf ("srcmute")] = 0.0f;
        CHECK (! inst->absorbsNeighbor (p.data()), "piste pr\xC3\xA9" "c\xC3\xA9" "dente entendue");
    }
    // gel : la voix se tait (entrée muette) mais le timbre figé continue tant que les notes tiennent
    {
        auto inst = createInstrument ("vocspec");
        const auto& d = inst->desc();
        auto p = defaults (d);
        inst->prepare (sr, block);
        for (int nn : { 48, 55, 60 }) inst->noteOn (nn, 0.9f);
        std::vector<float> L ((size_t) block), R ((size_t) block), z ((size_t) block, 0.0f);
        double after = 0.0;
        for (int pos = 0; pos < sr * 3; pos += block)
        {
            if (pos == sr * 3 / 2) { p[(size_t) d.indexOf ("freeze")] = 1.0f; p[(size_t) d.indexOf ("src")] = 1.0f; } // gel, puis voix coupée
            inst->setInput (z.data(), z.data());
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), block, p.data());
            if (pos > sr * 5 / 2) for (int k = 0; k < block; ++k) after += (double) L[(size_t) k] * L[(size_t) k];
        }
        const auto db = 10.0 * std::log10 (after / (sr / 2.0) + 1.0e-30);
        std::printf ("  gel : %.1f dB une seconde apr\xC3\xA8s le silence de la voix\n", db);
        CHECK (db > -45.0, "gel");
    }
}

/** v0.33 : phrase tapée de la voix interne, prise de son des vocodeurs (armée, enregistrée, en boucle). */
static void testVoice33()
{
    const std::string name = "voix v0.33";
    constexpr int sr = 48000, block = 32;
    using grv::engine::compileSpeech;
    // texte → phonèmes
    {
        const auto sc = compileSpeech ("Bonjour, je suis le robot. \xC3\x87" "a va ?");
        CHECK (sc != nullptr && sc->count > 15 && sc->count < 60, "phrase compil\xC3\xA9" "e");
        CHECK (compileSpeech ("  ,,; ...") == nullptr, "texte sans lettre");
        CHECK (compileSpeech (std::string (2000, 'a')) != nullptr, "texte tr\xC3\xA8s long born\xC3\xA9");
        CHECK (compileSpeech ("\xFF\xFE\xC3") == nullptr, "UTF-8 invalide ignor\xC3\xA9");
        // niveau de la voix interne : phrase tapée comparable à la démonstration
        auto level = [&] (const grv::engine::SpeechScript* s, int* maxWord)
        {
            grv::synth::DemoVoice dv; dv.prepare (sr); dv.setScript (s);
            double sum = 0.0; bool fin = true;
            for (int i = 0; i < sr * 6; ++i)
            {
                const auto x = dv.next (2.5, 125.0);
                fin = fin && std::isfinite (x) && std::abs (x) < 4.0;
                sum += x * x;
                if (maxWord != nullptr) *maxWord = std::max (*maxWord, dv.currentWord());
            }
            CHECK (fin, "voix interne : valeurs invalides");
            return 10.0 * std::log10 (sum / (sr * 6.0) + 1.0e-30);
        };
        int maxWord = -1;
        const auto demoDb = level (nullptr, nullptr), textDb = level (sc.get(), &maxWord);
        std::printf ("  voix interne : d\xC3\xA9monstration %.1f dB, phrase tap\xC3\xA9" "e %.1f dB (%d phon\xC3\xA8mes, mot %d atteint)\n", demoDb, textDb, sc->count, maxWord);
        CHECK (std::abs (textDb - demoDb) < 6.0, "phrase tap\xC3\xA9" "e : niveau");
        CHECK (maxWord >= 4, "mots prononc\xC3\xA9s");
    }

    // prise de son : silence, voix de 1,2 s, silence → la prise s'arrête seule et tourne
    auto inst = createInstrument ("voc16");
    auto* voc = dynamic_cast<grv::instruments::VocoderSynth*> (inst.get());
    CHECK (voc != nullptr, "vocodeur");
    if (voc == nullptr) return;
    const auto& d = inst->desc();
    auto p = defaults (d);
    p[(size_t) d.indexOf ("src")] = (float) grv::instruments::voc::srcCapture;
    CHECK (d.params[(size_t) d.indexOf ("src")].choices.back() == "Prise de son", "choix \xC2\xAB Prise de son \xC2\xBB \xC3\xA0 la fin");
    inst->prepare (sr, block);
    for (int nn : { 48, 55, 60 }) inst->noteOn (nn, 0.9f);
    std::vector<float> voice ((size_t) sr * 6, 0.0f);
    {
        grv::synth::DemoVoice dv; dv.prepare (sr);
        for (int i = (int) (0.5 * sr); i < (int) (1.7 * sr); ++i) voice[(size_t) i] = (float) (dv.next (3.0, 140.0) * 0.5);
    }
    std::vector<float> L ((size_t) block), R ((size_t) block), z ((size_t) block, 0.0f);
    float st[48] {};
    auto capState = [&] { const auto k = inst->status (st, 48); return k > 8 ? (int) st[k - 6] : -1; };
    auto capSec = [&] { const auto k = inst->status (st, 48); return k > 8 ? st[k - 5] : 0.0f; };
    inst->recordAction (0); // armée
    int stArmed = -1, stRec = -1;
    double heardAfter = 0.0;
    for (int pos = 0; pos < sr * 5; pos += block)
    {
        inst->setInput (voice.data() + pos, voice.data() + pos);
        std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
        inst->render (L.data(), R.data(), block, p.data());
        if (pos == sr / 4) stArmed = capState();
        if (pos == sr) stRec = capState();
        if (pos > sr * 4) for (int k = 0; k < block; ++k) heardAfter += (double) L[(size_t) k] * L[(size_t) k];
    }
    const auto len = capSec();
    const auto loopDb = 10.0 * std::log10 (heardAfter / (double) sr + 1.0e-30);
    std::printf ("  prise de son : arm\xC3\xA9" "e %d, en cours %d, puis %d ; %.2f s gard\xC3\xA9" "es ; boucle %.1f dB\n", stArmed, stRec, capState(), len, loopDb);
    CHECK (stArmed == 1 && stRec == 2 && capState() == 3, "\xC3\xA9tats de la prise");
    CHECK (len > 1.2f && len < 1.6f, "longueur de la prise (silence final retir\xC3\xA9)");
    CHECK (loopDb > -45.0, "la prise tourne en boucle");
    // copie (fil de l'interface) puis échantillon gardé : la lecture continue depuis l'échantillon
    std::vector<float> copy; double rate = 0.0;
    CHECK (voc->copyCapture (copy, rate) && rate == sr && copy.size() > (size_t) sr, "copie de la prise");
    CHECK (! voc->captureToKeep(), "prise copi\xC3\xA9" "e une seule fois");
    grv::engine::SampleBuffer smp; smp.data = copy; smp.sampleRate = 44100.0; // autre fréquence : lue au bon débit
    inst->setSample (0, &smp);
    CHECK (capState() == 0, "\xC3\xA9" "chantillon re\xC3\xA7u");
    // calée sur une mesure : la phrase repart à chaque mesure du séquenceur
    p[(size_t) d.indexOf ("ploop")] = 1.0f;
    double first = 0.0, again = 0.0;
    const auto beat = 60.0 / 120.0 * sr;
    for (int pos = 0; pos < (int) (beat * 8); pos += block)
    {
        inst->setTempo (120.0);
        inst->setPosition ((double) pos / beat, true);
        inst->setInput (z.data(), z.data());
        std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
        inst->render (L.data(), R.data(), block, p.data());
        double e = 0.0; for (int k = 0; k < block; ++k) e += (double) L[(size_t) k] * L[(size_t) k];
        if (pos >= (int) (beat * 0.5) && pos < (int) (beat * 1.5)) first += e;
        if (pos >= (int) (beat * 4.5) && pos < (int) (beat * 5.5)) again += e;
    }
    const auto diffDb = 10.0 * std::log10 ((first + 1.0e-20) / (again + 1.0e-20));
    std::printf ("  prise calée sur la mesure : écart d'une mesure à l'autre %.2f dB\n", diffDb);
    CHECK (first > 1.0e-6 && std::abs (diffDb) < 1.5, "prise cal\xC3\xA9" "e sur la mesure");
    inst->setSample (0, nullptr);
    // effacer, armer puis annuler : rien ne reste
    inst->recordAction (0); CHECK (capState() == 1, "armer");
    inst->recordAction (0); CHECK (capState() == 0, "annuler");
}

/** v0.34 : multibande (somme plate, compression dans sa bande, bandes flottantes), compresseur à lampes
    (le rapport monte avec le niveau), égaliseurs à lampes et de console (courbes mesurées). */
static void testStudio34()
{
    const std::string name = "studio v0.34";
    constexpr int sr = 48000, block = 32;
    // gain (dB) d'un effet sur un sinus : 0,6 s jouées, mesure sur les 0,2 dernières secondes
    auto gainAt = [&] (const char* type, const std::vector<std::pair<std::string, float>>& set, double hz, double ampDb, float* meter0 = nullptr)
    {
        auto fx = createEffect (type);
        const auto& d = fx->desc();
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); CHECK (i >= 0, id.c_str()); if (i >= 0) p[(size_t) i] = v; }
        fx->prepare (sr, block);
        const auto a = grv::dsp::dbToGain (ampDb);
        const int total = (int) (0.6 * sr);
        std::vector<float> L ((size_t) block), R ((size_t) block);
        double ein = 0.0, eout = 0.0;
        for (int pos = 0; pos < total; pos += block)
        {
            for (int k = 0; k < block; ++k) L[(size_t) k] = R[(size_t) k] = (float) (a * std::sin (2.0 * kPi * hz * (pos + k) / sr));
            if (pos > total - sr / 5) for (int k = 0; k < block; ++k) ein += (double) L[(size_t) k] * L[(size_t) k];
            fx->process (L.data(), R.data(), block, p.data(), 120.0);
            if (pos > total - sr / 5) for (int k = 0; k < block; ++k) eout += (double) L[(size_t) k] * L[(size_t) k];
        }
        if (meter0 != nullptr) *meter0 = fx->meter (0);
        return 10.0 * std::log10 ((eout + 1.0e-20) / (ein + 1.0e-20));
    };
    // multibande neutre : somme plate
    double worst = 0.0;
    for (double hz : { 40.0, 150.0, 400.0, 1000.0, 2500.0, 5000.0, 9000.0, 15000.0 }) worst = std::max (worst, std::abs (gainAt ("mband", {}, hz, -12)));
    std::printf ("  multibande neutre : \xC3\xA9" "cart maximal %.2f dB de 40 Hz \xC3\xA0 15 kHz\n", worst);
    CHECK (worst < 0.3, "multibande : somme non plate");
    const auto lowComp = gainAt ("mband", { { "b1rat", 6 }, { "b1thr", -30 }, { "b1att", 1 } }, 60, -6);
    const auto highFree = gainAt ("mband", { { "b1rat", 6 }, { "b1thr", -30 }, { "b1att", 1 } }, 3000, -6);
    const auto floatCut = gainAt ("mband", { { "f1on", 1 }, { "f1freq", 1000 }, { "f1thr", -40 }, { "f1range", -9 } }, 1000, -6);
    std::printf ("  multibande : grave compress\xC3\xA9 %.1f dB, aigu %.2f dB, bande flottante %.1f dB\n", lowComp, highFree, floatCut);
    CHECK (lowComp < -6.0 && std::abs (highFree) < 0.5, "multibande : compression hors de sa bande");
    CHECK (floatCut < -6.0, "multibande : bande flottante");
    // compresseur à lampes : la réduction monte plus vite que le niveau (rapport qui grandit)
    float g1 = 0, g2 = 0, g3 = 0;
    gainAt ("varimu", { { "tube", 0 }, { "input", 0 }, { "threshold", -20 } }, 200, -30, &g1);
    gainAt ("varimu", { { "tube", 0 }, { "input", 0 }, { "threshold", -20 } }, 200, -14, &g2);
    gainAt ("varimu", { { "tube", 0 }, { "input", 0 }, { "threshold", -20 } }, 200, 0, &g3);
    std::printf ("  compresseur \xC3\xA0 lampes : r\xC3\xA9" "duction %.1f / %.1f / %.1f dB (entr\xC3\xA9" "e -30 / -14 / 0 dB)\n", g1, g2, g3);
    CHECK (g1 < 0.5f && g2 > 1.0f && g3 - g2 > (g2 - g1) * 1.2f, "compresseur \xC3\xA0 lampes : rapport fixe");
    // égaliseur à lampes : bosse sous la fréquence et creux au-dessus quand on accentue ET atténue
    const auto boost = gainAt ("tubeeq", { { "tube", 0 }, { "lowfreq", 2 }, { "lowboost", 10 } }, 40, -18);
    const auto bump = gainAt ("tubeeq", { { "tube", 0 }, { "lowfreq", 2 }, { "lowboost", 6 }, { "lowatten", 6 } }, 50, -18);
    const auto dip = gainAt ("tubeeq", { { "tube", 0 }, { "lowfreq", 2 }, { "lowboost", 6 }, { "lowatten", 6 } }, 120, -18);
    const auto air = gainAt ("tubeeq", { { "tube", 0 }, { "hifreq", 4 }, { "hiboost", 10 } }, 10000, -18);
    std::printf ("  \xC3\xA9galiseur \xC3\xA0 lampes : grave +%.1f dB ; bosse %.1f dB \xC3\xA0 50 Hz, creux %.1f dB \xC3\xA0 120 Hz ; aigu +%.1f dB\n", boost, bump, dip, air);
    CHECK (boost > 8.0 && bump > 4.0 && dip < -1.0 && air > 12.0, "\xC3\xA9galiseur \xC3\xA0 lampes : courbes");
    // égaliseur de console : cloche à sa fréquence, passe-haut 18 dB/oct
    const auto bell = gainAt ("consoleeq", { { "lmfgain", 12 }, { "lmffreq", 1000 } }, 1000, -18);
    const auto hpOct = gainAt ("consoleeq", { { "hpf", 200 } }, 100, -18);
    const auto hp2Oct = gainAt ("consoleeq", { { "hpf", 200 } }, 50, -18);
    std::printf ("  \xC3\xA9galiseur de console : cloche %.1f dB, passe-haut %.1f dB (1 octave) %.1f dB (2 octaves)\n", bell, hpOct, hp2Oct);
    CHECK (std::abs (bell - 12.0) < 1.0, "\xC3\xA9galiseur de console : cloche");
    CHECK (hpOct < -12.0 && hp2Oct - hpOct < -15.0, "\xC3\xA9galiseur de console : pente du passe-haut");
}

static void testSpace35()
{
    const std::string name = "espaces v0.35";
    constexpr int sr = 48000, block = 32;
    // réponse impulsionnelle (canal gauche, mono en entrée) sur `seconds` secondes
    auto impulse = [&] (const char* type, const std::vector<std::pair<std::string, float>>& set, double seconds, std::vector<float>& outL, std::vector<float>* outR = nullptr)
    {
        auto fx = createEffect (type);
        const auto& d = fx->desc();
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); CHECK (i >= 0, id.c_str()); if (i >= 0) p[(size_t) i] = v; }
        fx->prepare (sr, block);
        const int total = (int) (seconds * sr) / block * block;
        outL.assign ((size_t) total, 0.0f);
        if (outR != nullptr) outR->assign ((size_t) total, 0.0f);
        std::vector<float> L ((size_t) block), R ((size_t) block);
        for (int pos = 0; pos < total; pos += block)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            if (pos == 0) L[0] = R[0] = 1.0f;
            fx->process (L.data(), R.data(), block, p.data(), 120.0);
            for (int k = 0; k < block; ++k)
            {
                outL[(size_t) (pos + k)] = L[(size_t) k];
                if (outR != nullptr) (*outR)[(size_t) (pos + k)] = R[(size_t) k];
            }
        }
    };
    auto finite = [] (const std::vector<float>& v) { for (auto x : v) if (! std::isfinite (x) || std::abs (x) > 8.0f) return false; return true; };
    auto energyDb = [&] (const std::vector<float>& v, double t0, double t1)
    {
        double e = 0.0;
        for (int i = (int) (t0 * sr); i < std::min ((int) v.size(), (int) (t1 * sr)); ++i) e += (double) v[(size_t) i] * v[(size_t) i];
        return 10.0 * std::log10 (e / std::max (1.0, (t1 - t0) * sr) + 1.0e-30);
    };
    std::vector<float> l, r;
    // réverbération vintage : chaque mode et chaque couleur, queue présente, finie, déclin respecté
    for (int mode = 0; mode < 6; ++mode)
        for (int col = 0; col < 3; ++col)
        {
            impulse ("vintverb", { { "mode", (float) mode }, { "color", (float) col }, { "decay", 2.0f }, { "mix", 100 }, { "predelay", 0 } }, 6.0, l, &r);
            const auto early = energyDb (l, 0.05, 0.25), late = energyDb (l, 2.05, 2.25), end = energyDb (l, 5.5, 6.0);
            CHECK (finite (l) && finite (r), "r\xC3\xA9verb\xC3\xA9ration vintage : valeurs");
            CHECK (early > -75.0, "r\xC3\xA9verb\xC3\xA9ration vintage : queue absente");
            // RT60 = 2 s : environ −60 dB au bout de 2 s (tolérance large : amortissement des aigus)
            CHECK (early - late > 35.0 && early - late < 95.0, "r\xC3\xA9verb\xC3\xA9ration vintage : d\xC3\xA9" "clin");
            CHECK (end < early - 60.0, "r\xC3\xA9verb\xC3\xA9ration vintage : queue sans fin");
            if (mode == 0 && col == 1)
            {
                double c = 0.0, el = 0.0, er = 0.0;
                for (size_t i = (size_t) (0.05 * sr); i < (size_t) (1.0 * sr); ++i) { c += (double) l[i] * r[i]; el += (double) l[i] * l[i]; er += (double) r[i] * r[i]; }
                const auto corr = c / std::sqrt (el * er + 1.0e-30);
                std::printf ("  r\xC3\xA9verb\xC3\xA9ration vintage (salle, 1980, 2 s) : d\xC3\xA9" "but %.1f dB, \xC3\xA0 2 s %.1f dB, corr\xC3\xA9lation G/D %.2f\n", early, late, corr);
                CHECK (std::abs (corr) < 0.5, "r\xC3\xA9verb\xC3\xA9ration vintage : pas assez large");
            }
        }
    // couleur 1970 plus sombre que l'actuelle : part d'aigus dans la queue
    auto hiShare = [&] (const std::vector<float>& v)
    {
        double hi = 0.0, all = 0.0, prev = 0.0;
        for (size_t i = (size_t) (0.05 * sr); i < (size_t) (0.8 * sr); ++i) { const auto d = v[i] - prev; prev = v[i]; hi += d * d; all += (double) v[i] * v[i]; }
        return hi / (all + 1.0e-30);
    };
    impulse ("vintverb", { { "color", 0 }, { "mix", 100 } }, 1.0, l);
    const auto dark = hiShare (l);
    impulse ("vintverb", { { "color", 2 }, { "mix", 100 } }, 1.0, l);
    const auto bright = hiShare (l);
    CHECK (bright > dark * 1.5, "r\xC3\xA9verb\xC3\xA9ration vintage : couleurs identiques");
    // déclin long : la queue tient
    impulse ("vintverb", { { "mix", 100 }, { "decay", 20 } }, 4.0, l);
    CHECK (energyDb (l, 3.5, 4.0) > energyDb (l, 0.1, 0.5) - 25.0, "r\xC3\xA9verb\xC3\xA9ration vintage : d\xC3\xA9" "clin long");
    // plaque : dense et finie, déclin réglé
    impulse ("plate", { { "decay", 1.0f }, { "mix", 100 }, { "predelay", 0 } }, 4.0, l);
    const auto pe = energyDb (l, 0.05, 0.25), pl = energyDb (l, 1.05, 1.25), pend = energyDb (l, 3.5, 4.0);
    std::printf ("  plaque (1 s) : d\xC3\xA9" "but %.1f dB, \xC3\xA0 1 s %.1f dB, fin %.1f dB\n", pe, pl, pend);
    CHECK (finite (l) && pe - pl > 35.0 && pend < pe - 70.0, "plaque : d\xC3\xA9" "clin");
    // ressort : 1 à 3 ressorts, dispersion (les aigus arrivent avant les graves dans le premier retour)
    for (int n = 1; n <= 3; ++n)
    {
        impulse ("spring", { { "springs", (float) n }, { "mix", 100 }, { "decay", 60 } }, 6.0, l, &r);
        CHECK (finite (l) && finite (r), "ressort : valeurs");
        CHECK (energyDb (l, 0.02, 0.3) > -80.0, "ressort : silence");
        CHECK (energyDb (l, 5.5, 6.0) < energyDb (l, 0.02, 0.3) - 60.0, "ressort : queue sans fin");
    }
    impulse ("spring", { { "springs", 1 }, { "mix", 100 }, { "drip", 0 } }, 0.2, l);
    {
        // centre de gravité temporel des aigus (dérivée) et des graves (lissage) dans le premier retour
        double th = 0.0, eh = 0.0, tl = 0.0, el = 0.0, prev = 0.0, lp = 0.0;
        for (size_t i = 0; i < l.size(); ++i)
        {
            const auto d = l[i] - prev; prev = l[i];
            lp += (l[i] - lp) * 0.02;
            th += i * d * d; eh += d * d; tl += i * lp * lp; el += lp * lp;
        }
        const auto tHi = th / (eh + 1.0e-30) / sr * 1000.0, tLo = tl / (el + 1.0e-30) / sr * 1000.0;
        std::printf ("  ressort : aigus \xC3\xA0 %.1f ms, graves \xC3\xA0 %.1f ms (dispersion)\n", tHi, tLo);
        CHECK (tLo > tHi + 2.0, "ressort : pas de dispersion");
    }
    // écho à bande : têtes aux bons temps, auto-oscillation bornée, chaque combinaison jouée
    impulse ("tapeecho", { { "heads", 6 }, { "speed", 100 }, { "intensity", 0 }, { "wear", 0 }, { "echo", 100 }, { "mix", 100 } }, 0.5, l);
    {
        auto peakNear = [&] (double ms) { float m = 0.0f; for (int i = (int) ((ms - 6) * sr / 1000); i < (int) ((ms + 6) * sr / 1000); ++i) m = std::max (m, std::abs (l[(size_t) i])); return m; };
        auto quietNear = [&] (double ms) { float m = 0.0f; for (int i = (int) ((ms - 20) * sr / 1000); i < (int) ((ms + 20) * sr / 1000); ++i) m = std::max (m, std::abs (l[(size_t) i])); return m; };
        const auto h1 = peakNear (100), h2 = peakNear (200), h3 = peakNear (300), gap = quietNear (150);
        std::printf ("  \xC3\xA9" "cho \xC3\xA0 bande : t\xC3\xAA" "tes %.3f / %.3f / %.3f, entre deux %.4f\n", h1, h2, h3, gap);
        CHECK (h1 > 0.05f && h2 > 0.05f && h3 > 0.05f && gap < h1 * 0.1f, "\xC3\xA9" "cho \xC3\xA0 bande : t\xC3\xAA" "tes");
    }
    for (int h = 0; h < 7; ++h)
    {
        impulse ("tapeecho", { { "heads", (float) h }, { "intensity", 108 }, { "wear", 80 }, { "drive", 80 }, { "spring", 50 } }, 8.0, l, &r);
        CHECK (finite (l) && finite (r), "\xC3\xA9" "cho \xC3\xA0 bande : auto-oscillation non born\xC3\xA9" "e");
    }
    impulse ("tapeecho", { { "intensity", 40 } }, 12.0, l);
    CHECK (energyDb (l, 11.0, 12.0) < -90.0, "\xC3\xA9" "cho \xC3\xA0 bande : r\xC3\xA9" "p\xC3\xA9titions sans fin");
    // Atmosphère (principe des pédales d'ambiance à 12 algorithmes) : chaque algorithme sonne, reste fini, s'éteint
    for (int m = 0; m < 12; ++m)
    {
        impulse ("atmos", { { "mode", (float) m }, { "decay", 2.0f }, { "mix", 100 }, { "predelay", 0 } }, 9.0, l, &r);
        const auto e0 = energyDb (l, 0.0, 0.5) , eEnd = energyDb (l, 8.5, 9.0);
        CHECK (finite (l) && finite (r), "atmosph\xC3\xA8re : valeurs");
        if (e0 <= -80.0) std::printf ("  algorithme %d : %.1f dB\n", m, e0);
        CHECK (e0 > -80.0, "atmosph\xC3\xA8re : algorithme muet");
        CHECK (eEnd < e0 - 60.0, "atmosph\xC3\xA8re : queue sans fin");
    }
    {
        // gonflement : la réverbération monte (rapport du début à la suite, sous un son tenu)
        auto fx = createEffect ("atmos");
        const auto& d = fx->desc();
        auto p = defaults (d);
        p[(size_t) d.indexOf ("mode")] = 7; p[(size_t) d.indexOf ("rise")] = 1000; p[(size_t) d.indexOf ("mix")] = 100; p[(size_t) d.indexOf ("decay")] = 3;
        fx->prepare (sr, block);
        std::vector<float> L ((size_t) block), R ((size_t) block);
        double e1 = 0.0, e2 = 0.0;
        for (int pos = 0; pos < (int) (1.6 * sr); pos += block)
        {
            for (int k = 0; k < block; ++k) L[(size_t) k] = R[(size_t) k] = (float) (0.3 * std::sin (2.0 * kPi * 220.0 * (pos + k) / sr));
            fx->process (L.data(), R.data(), block, p.data(), 120.0);
            for (int k = 0; k < block; ++k)
            {
                const auto t = pos + k;
                if (t < sr / 5) e1 += (double) L[(size_t) k] * L[(size_t) k];
                else if (t > (int) (1.2 * sr) && t < (int) (1.4 * sr)) e2 += (double) L[(size_t) k] * L[(size_t) k];
            }
        }
        std::printf ("  atmosph\xC3\xA8re, gonflement : %.1f dB entre la premi\xC3\xA8re et la 6e dizaine de centi\xC3\xA8mes\n", 10.0 * std::log10 ((e2 + 1e-30) / (e1 + 1e-30)));
        CHECK (e2 > e1 * 10.0, "atmosph\xC3\xA8re : gonflement");
    }
    // non linéaire, porte : la queue tombe après sa durée
    impulse ("atmos", { { "mode", 10 }, { "decay", 0.3f }, { "shape", 0 }, { "mix", 100 }, { "predelay", 0 } }, 1.0, l);
    {
        const auto in = energyDb (l, 0.05, 0.25), after = energyDb (l, 0.4, 0.8);
        std::printf ("  atmosph\xC3\xA8re, porte : %.1f dB pendant, %.1f dB apr\xC3\xA8s\n", in, after);
        CHECK (in - after > 40.0, "atmosph\xC3\xA8re : porte");
    }
    // inversée : la fin est plus forte que le début
    impulse ("atmos", { { "mode", 10 }, { "decay", 0.6f }, { "shape", 1 }, { "mix", 100 }, { "predelay", 0 } }, 1.0, l);
    CHECK (energyDb (l, 0.45, 0.58) > energyDb (l, 0.02, 0.15) + 6.0, "atmosph\xC3\xA8re : inversion");
    {
        // traîne : contourné après l'impulsion, la queue continue ; sans traîne, rien ne sort que le direct (silence)
        for (int persist = 0; persist < 2; ++persist)
        {
            auto fx = createEffect ("atmos");
            const auto& d = fx->desc();
            auto p = defaults (d);
            p[(size_t) d.indexOf ("persist")] = (float) persist; p[(size_t) d.indexOf ("mix")] = 100; p[(size_t) d.indexOf ("decay")] = 3;
            fx->prepare (sr, block);
            std::vector<float> L ((size_t) block, 0.0f), R ((size_t) block, 0.0f);
            L[0] = R[0] = 1.0f;
            fx->process (L.data(), R.data(), block, p.data(), 120.0);
            double e = 0.0;
            for (int pos = block; pos < sr; pos += block)
            {
                std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
                CHECK (fx->trails (p.data()) == (persist == 1), "atmosph\xC3\xA8re : tra\xC3\xAEne");
                if (fx->trails (p.data())) fx->processTrails (L.data(), R.data(), block, p.data(), 120.0);
                for (auto v : L) e += (double) v * v;
            }
            CHECK (persist == 1 ? e > 1.0e-4 : e == 0.0, "atmosph\xC3\xA8re : tra\xC3\xAEne au contournement");
        }
    }
    impulse ("tapeecho", { { "sync", 1 }, { "division", 5 } }, 0.6, l); // 1/8 à 120 = 250 ms
    {
        float m = 0.0f; int at = 0;
        for (int i = (int) (0.05 * sr); i < (int) (0.4 * sr); ++i) if (std::abs (l[(size_t) i]) > m) { m = std::abs (l[(size_t) i]); at = i; }
        std::printf ("  \xC3\xA9" "cho \xC3\xA0 bande cal\xC3\xA9 au tempo : premier retour \xC3\xA0 %.1f ms\n", at * 1000.0 / sr);
        CHECK (std::abs (at * 1000.0 / sr - 250.0) < 8.0, "\xC3\xA9" "cho \xC3\xA0 bande : tempo");
    }
}

/** v0.36 : Flanger, Phaser double, Dimension ; styles du compresseur moderne ; couleurs, lo-fi, groove et gel de
    l'écho ; dynamique spectrale de l'égaliseur. */
static void testFx36()
{
    const std::string name = "effets v0.36";
    using grv::dsp::kTwoPi;
    constexpr int sr = 48000, block = 32;
    using Set = std::vector<std::pair<std::string, float>>;
    // rend `seconds` secondes du signal gen(t, canal) à travers l'effet ; sorties gauche / droite
    auto run = [&] (const char* type, const Set& set, double seconds, const std::function<double (int, int)>& gen,
                    std::vector<float>& outL, std::vector<float>& outR, const Set& later = {}, double laterAt = 1.0e9)
    {
        auto fx = createEffect (type);
        const auto& d = fx->desc();
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); CHECK (i >= 0, id.c_str()); if (i >= 0) p[(size_t) i] = v; }
        fx->prepare (sr, block);
        const int total = (int) (seconds * sr) / block * block;
        outL.assign ((size_t) total, 0.0f); outR.assign ((size_t) total, 0.0f);
        std::vector<float> L ((size_t) block), R ((size_t) block);
        bool switched = false;
        for (int pos = 0; pos < total; pos += block)
        {
            if (! switched && pos >= laterAt * sr)
            {
                switched = true;
                for (const auto& [id, v] : later) { const auto i = d.indexOf (id); if (i >= 0) p[(size_t) i] = v; }
            }
            for (int k = 0; k < block; ++k) { L[(size_t) k] = (float) gen (pos + k, 0); R[(size_t) k] = (float) gen (pos + k, 1); }
            fx->process (L.data(), R.data(), block, p.data(), 120.0);
            for (int k = 0; k < block; ++k) { outL[(size_t) (pos + k)] = L[(size_t) k]; outR[(size_t) (pos + k)] = R[(size_t) k]; }
        }
    };
    auto finite = [] (const std::vector<float>& v) { for (auto x : v) if (! std::isfinite (x) || std::abs (x) > 8.0f) return false; return true; };
    auto rmsDb = [&] (const std::vector<float>& v, double t0, double t1)
    {
        double e = 0.0; int c = 0;
        for (size_t i = (size_t) (t0 * sr); i < (size_t) (t1 * sr) && i < v.size(); ++i) { e += (double) v[i] * v[i]; ++c; }
        return 10.0 * std::log10 (e / std::max (1, c) + 1.0e-30);
    };
    // amplitude d'une fréquence (Goertzel) entre t0 et t1
    auto toneDb = [&] (const std::vector<float>& v, double hz, double t0, double t1)
    {
        const auto w = kTwoPi * hz / sr, c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0; int nn = 0;
        for (size_t i = (size_t) (t0 * sr); i < (size_t) (t1 * sr); ++i) { const auto s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; ++nn; }
        const auto mag = std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / std::max (1, nn);
        return 20.0 * std::log10 (mag + 1.0e-12);
    };
    std::uint32_t seed = 7u;
    auto noise = [&] (int, int) { seed = seed * 1664525u + 1013904223u; return ((double) (seed >> 9) / 4194304.0 - 1.0) * 0.25; };
    auto sine = [] (double hz, double amp) { return [hz, amp] (int i, int) { return amp * std::sin (kTwoPi * hz * i / 48000.0); }; };
    std::vector<float> l, r, l2, r2;

    // Flanger : chaque mode fini et non muet ; réinjection maximale bornée ; matrice = peigne figé ; inversé = creux
    for (int m = 0; m < 3; ++m)
        for (float fb : { -95.0f, 0.0f, 95.0f })
        {
            run ("flanger", { { "mode", (float) m }, { "feedback", fb }, { "mix", 100 } }, 3.0, noise, l, r);
            CHECK (finite (l) && finite (r), "flanger : valeurs");
            CHECK (rmsDb (l, 1.0, 3.0) > -40.0, "flanger : silence");
        }
    run ("flanger", { { "mode", 2 }, { "rate", 2 }, { "mix", 100 }, { "feedback", 0 } }, 2.0, sine (1000, 0.3), l, r);
    const auto fixedVar = std::abs (rmsDb (l, 0.5, 0.75) - rmsDb (l, 1.5, 1.75));
    run ("flanger", { { "mode", 0 }, { "rate", 1 }, { "mix", 100 }, { "feedback", 0 }, { "depth", 100 } }, 2.0, sine (1000, 0.3), l, r);
    double lo = 1.0e9, hi = -1.0e9;
    for (int k = 0; k < 20; ++k) { const auto e = rmsDb (l, 0.5 + 0.05 * k, 0.55 + 0.05 * k); lo = std::min (lo, e); hi = std::max (hi, e); }
    std::printf ("  flanger : variation en matrice %.2f dB, en balayage %.1f dB\n", fixedVar, hi - lo);
    CHECK (fixedVar < 0.5 && hi - lo > 6.0, "flanger : matrice ou balayage");
    run ("flanger", { { "mode", 0 }, { "depth", 0 }, { "manual", 0.3f }, { "mix", 100 }, { "feedback", 0 } }, 1.0, sine (60, 0.3), l, r);
    const auto sumN = rmsDb (l, 0.3, 1.0);
    run ("flanger", { { "mode", 1 }, { "depth", 0 }, { "manual", 0.3f }, { "mix", 100 }, { "feedback", 0 } }, 1.0, sine (60, 0.3), l, r);
    const auto sumI = rmsDb (l, 0.3, 1.0);
    CHECK (sumI < sumN - 12.0, "flanger : mode invers\xC3\xA9 sans creux dans le grave");

    // Phaser double : 3 routages ; en stéréo, gauche et droite différents
    for (int rt = 0; rt < 3; ++rt)
    {
        run ("phaser2", { { "routing", (float) rt }, { "mix", 100 }, { "feedbackA", 95 }, { "feedbackB", -95 } }, 3.0, noise, l, r);
        CHECK (finite (l) && finite (r) && rmsDb (l, 1.0, 3.0) > -40.0, "phaser double : routage");
    }
    run ("phaser2", { { "routing", 2 }, { "mix", 100 } }, 2.0, [] (int i, int) { return 0.2 * std::sin (kTwoPi * 440.0 * i / 48000.0); }, l, r);
    double dlr = 0.0; for (size_t i = (size_t) sr; i < l.size(); ++i) dlr += std::abs (l[i] - r[i]);
    CHECK (dlr > 100.0, "phaser double : st\xC3\xA9r\xC3\xA9o identique");

    // Dimension : large (corrélation G/D < 0,95) et compatible mono (somme > -6 dB du direct)
    for (int m = 0; m < 4; ++m)
    {
        seed = 7u;
        run ("dimension", { { "mode", (float) m } }, 2.0, [&] (int i, int c) { (void) c; return 0.0; }, l, r); // silence : reste muet
        CHECK (rmsDb (l, 0.5, 2.0) < -120.0, "dimension : bruit propre");
        std::vector<float> in;
        seed = 11u;
        std::vector<double> mono (2 * sr);
        for (auto& x : mono) x = noise (0, 0);
        run ("dimension", { { "mode", (float) m } }, 2.0, [&] (int i, int) { return mono[(size_t) i]; }, l, r);
        double c = 0.0, el = 0.0, er = 0.0, es = 0.0, ed = 0.0;
        for (size_t i = (size_t) (0.5 * sr); i < l.size(); ++i)
        { c += (double) l[i] * r[i]; el += (double) l[i] * l[i]; er += (double) r[i] * r[i]; const auto sm = 0.5 * (l[i] + r[i]); es += sm * sm; ed += mono[i] * mono[i]; }
        const auto corr = c / std::sqrt (el * er + 1.0e-30), monoDb = 10.0 * std::log10 (es / ed);
        if (m == 1) std::printf ("  dimension (r\xC3\xA9glage 2) : corr\xC3\xA9lation %.2f, somme mono %.1f dB\n", corr, monoDb);
        CHECK (finite (l) && corr < 0.97 && monoDb > -6.0, "dimension : largeur ou mono");
    }

    // Compresseur moderne : chaque style comprime (le montant remonte un signal faible), fini
    for (int st = 0; st < 10; ++st)
    {
        const bool up = st == 9;
        run ("comp", { { "style", (float) st }, { "threshold", up ? -20.0f : -24.0f }, { "ratio", 4 }, { "autogain", 0 }, { "makeup", 0 } }, 1.5,
             sine (220, up ? 0.01 : 0.5), l, r);
        const auto inDb = 20.0 * std::log10 ((up ? 0.01 : 0.5) / std::sqrt (2.0));
        const auto outDb = rmsDb (l, 1.0, 1.5);
        CHECK (finite (l), "compresseur : valeurs");
        if (up) CHECK (outDb > inDb + 3.0, "compresseur montant : ne remonte pas");
        else CHECK (outDb < inDb - 3.0, "compresseur : style sans compression");
    }
    // seuil automatique : même réduction à -6 et -26 dB d'entrée (à 3 dB près) ; sans : très différente
    auto grAt = [&] (float autoThr, double amp)
    {
        run ("comp", { { "autothr", autoThr }, { "threshold", -24 }, { "ratio", 4 }, { "autogain", 0 }, { "detection", 1 } }, 6.0, sine (220, amp), l, r);
        return 20.0 * std::log10 (amp / std::sqrt (2.0)) - rmsDb (l, 5.0, 6.0);
    };
    const auto a1 = grAt (1, 0.7), a2 = grAt (1, 0.07), b1 = grAt (0, 0.7), b2 = grAt (0, 0.07);
    std::printf ("  seuil automatique : r\xC3\xA9" "duction %.1f / %.1f dB (sans : %.1f / %.1f dB)\n", a1, a2, b1, b2);
    CHECK (std::abs (a1 - a2) < 3.0 && std::abs (b1 - b2) > 6.0, "compresseur : seuil automatique");
    // caractère lampe : ajoute des harmoniques
    run ("comp", { { "character", 0 }, { "threshold", -30 } }, 1.0, sine (200, 0.5), l, r);
    const auto h0 = toneDb (l, 400, 0.5, 1.0) - toneDb (l, 200, 0.5, 1.0);
    run ("comp", { { "character", 1 }, { "colour", 80 }, { "threshold", -30 } }, 1.0, sine (200, 0.5), l, r);
    const auto h1 = toneDb (l, 400, 0.5, 1.0) - toneDb (l, 200, 0.5, 1.0);
    CHECK (h1 > h0 + 20.0, "compresseur : caract\xC3\xA8re lampe sans harmonique");

    // Écho : couleurs et lo-fi finis ; gel qui tient ; groove qui décale le premier retour
    for (int c = 0; c < 6; ++c)
    {
        run ("delay", { { "echocolour", (float) c }, { "lofi", 60 }, { "feedback", 90 } }, 3.0, noise, l, r);
        CHECK (finite (l) && finite (r) && rmsDb (l, 1.0, 3.0) > -40.0, "\xC3\xA9" "cho : couleur");
    }
    auto burst = [] (int i, int) { return i < 2400 ? 0.4 * std::sin (kTwoPi * 330.0 * i / 48000.0) : 0.0; };
    run ("delay", { { "feedback", 50 }, { "mix", 100 } }, 4.0, burst, l, r, { { "freeze", 1 } }, 0.5);
    auto peakWin = [&] (double t0, double t1) { double m = -300.0; for (double t = t0; t < t1; t += 0.05) m = std::max (m, rmsDb (l, t, t + 0.05)); return m; };
    const auto fz1 = peakWin (0.75, 1.75), fz2 = peakWin (3.0, 4.0);
    std::printf ("  \xC3\xA9" "cho gel\xC3\xA9 : %.1f dB puis %.1f dB\n", fz1, fz2);
    CHECK (fz1 > -60.0 && fz2 > fz1 - 3.0, "\xC3\xA9" "cho : le gel ne tient pas");
    auto firstEcho = [&] (float groove)
    {
        run ("delay", { { "groove", groove }, { "mode", 0 }, { "feedback", 0 }, { "mix", 100 }, { "sync", 1 }, { "division", 7 } }, 0.6,
             [] (int i, int) { return i == 0 ? 1.0 : 0.0; }, l, r);
        for (size_t i = 100; i < l.size(); ++i) if (std::abs (l[i]) > 0.05f) return (double) i / sr * 1000.0;
        return 0.0;
    };
    const auto e0 = firstEcho (0), e1 = firstEcho (50);
    std::printf ("  \xC3\xA9" "cho : premier retour %.1f ms, avec groove +50 %% %.1f ms\n", e0, e1);
    CHECK (std::abs (e0 - 250.0) < 2.0 && std::abs (e1 - 265.6) < 2.0, "\xC3\xA9" "cho : groove");

    // Égaliseur spectral : une cloche large (Q 0,35) à 2,7 kHz, plage -12 dB ; une composante forte à 1,5 kHz
    // et une faible à 5 kHz : en spectral la faible est épargnée, en dynamique simple elle baisse avec la forte
    auto mix2 = [] (int i, int) { return 0.5 * std::sin (kTwoPi * 1500.0 * i / 48000.0) + 0.005 * std::sin (kTwoPi * 5000.0 * i / 48000.0); };
    auto drops = [&] (float spec)
    {
        run ("eq", { { "b5_freq", 2700 }, { "b5_q", 0.35f }, { "b5_range", -12 }, { "b5_thr", -30 }, { "spectral", spec } }, 1.0, mix2, l, r);
        const auto loud = toneDb (l, 1500, 0.5, 1.0) - 20.0 * std::log10 (0.5), quiet = toneDb (l, 5000, 0.5, 1.0) - 20.0 * std::log10 (0.005);
        return std::pair<double, double> { loud, quiet };
    };
    const auto [sl, sq] = drops (1);
    const auto [dl, dq] = drops (0);
    std::printf ("  \xC3\xA9galiseur : spectral %.1f / %.1f dB, simple %.1f / %.1f dB (forte / faible)\n", sl, sq, dl, dq);
    CHECK (finite (l), "\xC3\xA9galiseur spectral : valeurs");
    CHECK (sl < -4.0 && sq > sl + 4.0 && sq > -3.0 && dq < -4.0, "\xC3\xA9galiseur : dynamique spectrale");
}

/** v0.36 : Furnace (filtres sur le son distordu seulement, modulateurs internes) et Grinder (multibande). */
static void testDist36()
{
    const std::string name = "distorsions v0.36";
    using grv::dsp::kTwoPi;
    constexpr int sr = 48000, block = 32;
    using Set = std::vector<std::pair<std::string, float>>;
    auto run = [&] (const char* type, const Set& set, double seconds, const std::function<double (int)>& gen, std::vector<float>& out)
    {
        auto fx = createEffect (type);
        const auto& d = fx->desc();
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); CHECK (i >= 0, id.c_str()); if (i >= 0) p[(size_t) i] = v; }
        fx->prepare (sr, block);
        const int total = (int) (seconds * sr) / block * block;
        out.assign ((size_t) total, 0.0f);
        std::vector<float> L ((size_t) block), R ((size_t) block);
        for (int pos = 0; pos < total; pos += block)
        {
            for (int k = 0; k < block; ++k) L[(size_t) k] = R[(size_t) k] = (float) gen (pos + k);
            fx->setPosition (pos / (double) sr * 2.0, true);
            fx->process (L.data(), R.data(), block, p.data(), 120.0);
            for (int k = 0; k < block; ++k) out[(size_t) (pos + k)] = L[(size_t) k];
        }
    };
    auto finite = [] (const std::vector<float>& v) { for (auto x : v) if (! std::isfinite (x) || std::abs (x) > 8.0f) return false; return true; };
    auto rmsDb = [&] (const std::vector<float>& v, double t0, double t1)
    {
        double e = 0.0; int c = 0;
        for (size_t i = (size_t) (t0 * sr); i < (size_t) (t1 * sr) && i < v.size(); ++i) { e += (double) v[i] * v[i]; ++c; }
        return 10.0 * std::log10 (e / std::max (1, c) + 1.0e-30);
    };
    auto toneDb = [&] (const std::vector<float>& v, double hz, double t0, double t1)
    {
        const auto w = kTwoPi * hz / sr, c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0; int nn = 0;
        for (size_t i = (size_t) (t0 * sr); i < (size_t) (t1 * sr); ++i) { const auto s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; ++nn; }
        return 20.0 * std::log10 (std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / std::max (1, nn) + 1.0e-12);
    };
    std::vector<float> o;
    auto sine = [] (double hz, double a) { return [hz, a] (int i) { return a * std::sin (kTwoPi * hz * i / 48000.0); }; };

    // Furnace : filtre d'entrée passe-bas à 200 Hz, moteur A à 50 % : la part non distordue garde ses aigus
    run ("dist", { { "mixA", 50 }, { "driveA", 10 }, { "typeA", 0 }, { "preType", 1 }, { "preFreq", 200 } }, 0.5, sine (5000, 0.2), o);
    const auto keep = toneDb (o, 5000, 0.2, 0.5) - 20.0 * std::log10 (0.2);
    std::printf ("  furnace : 5 kHz avec filtre d'entr\xC3\xA9" "e \xC3\xA0 200 Hz et m\xC3\xA9lange A 50 %% : %.1f dB\n", keep);
    CHECK (keep > -9.0 && keep < -3.0, "furnace : le filtre touche la part non distordue");
    // passe-bas de sortie du moteur A : les harmoniques du son distordu baissent
    run ("dist", { { "driveA", 70 }, { "typeA", 6 } }, 0.5, sine (300, 0.3), o);
    const auto h0 = toneDb (o, 2100, 0.2, 0.5);
    run ("dist", { { "driveA", 70 }, { "typeA", 6 }, { "postLpA", 800 } }, 0.5, sine (300, 0.3), o);
    const auto h1 = toneDb (o, 2100, 0.2, 0.5);
    CHECK (h1 < h0 - 15.0, "furnace : passe-bas de sortie du moteur A");
    // modulateur LFO sur le drive A : le niveau bouge ; à l'arrêt il ne bouge pas
    auto spread = [&] (const Set& s)
    {
        run ("dist", s, 2.0, sine (220, 0.05), o);
        double lo = 1.0e9, hi = -1.0e9;
        for (int k = 0; k < 30; ++k) { const auto e = rmsDb (o, 0.5 + 0.05 * k, 0.55 + 0.05 * k); lo = std::min (lo, e); hi = std::max (hi, e); }
        return hi - lo;
    };
    const auto still = spread ({ { "driveA", 50 }, { "mix", 100 } });
    const auto moving = spread ({ { "driveA", 50 }, { "m1type", 1 }, { "m1rate", 1 }, { "m1amt", 100 }, { "m1dest", 0 } });
    const auto seq = spread ({ { "driveA", 50 }, { "m1type", 4 }, { "m1div", 7 }, { "m1shape", 5 }, { "m1amt", 100 }, { "m1dest", 0 } });
    std::printf ("  furnace : \xC3\xA9" "cart de niveau sans modulation %.2f dB, LFO sur le drive %.1f dB, s\xC3\xA9quenceur %.1f dB\n", still, moving, seq);
    CHECK (still < 0.5 && moving > 4.0 && seq > 3.0, "furnace : modulateurs");
    // tous les types de modulateurs, toutes les destinations : fini
    for (int t = 1; t <= 4; ++t)
        for (int dst = 0; dst < 21; dst += 4)
        {
            run ("dist", { { "m1type", (float) t }, { "m1dest", (float) dst }, { "m1amt", 100 }, { "m2type", (float) (5 - t) }, { "m2dest", (float) (20 - dst) }, { "m2amt", -100 },
                           { "feedback", 60 }, { "mixB", 100 } }, 0.6, [] (int i) { return (i / 4800) % 2 ? 0.3 * std::sin (i * 0.05) : 0.0; }, o);
            CHECK (finite (o), "furnace : modulation non finie");
        }

    // Grinder : sans distorsion, la somme des bandes reste plate (2, 3 et 4 bandes)
    for (int nb = 0; nb < 3; ++nb)
    {
        std::uint32_t seed = 3u;
        auto noise = [&seed] (int) { seed = seed * 1664525u + 1013904223u; return ((double) (seed >> 9) / 4194304.0 - 1.0) * 0.2; };
        Set s { { "bands", (float) nb } };
        for (int b = 1; b <= 4; ++b) { s.push_back ({ "b" + std::to_string (b) + "type", 0 }); s.push_back ({ "b" + std::to_string (b) + "drive", 0 }); }
        run ("grinder", s, 1.0, noise, o);
        seed = 3u;
        std::vector<float> ref ((size_t) o.size());
        for (size_t i = 0; i < ref.size(); ++i) ref[i] = (float) noise ((int) i);
        const auto diff = rmsDb (o, 0.3, 1.0) - rmsDb (ref, 0.3, 1.0);
        if (nb == 2) std::printf ("  grinder neutre (4 bandes) : \xC3\xA9" "cart de niveau %.2f dB\n", diff);
        CHECK (std::abs (diff) < 0.5, "grinder : somme des bandes");
    }
    // 16 algorithmes : finis et non muets
    for (int a = 0; a < 16; ++a)
    {
        run ("grinder", { { "b2type", (float) a }, { "b2drive", 70 }, { "b2bias", 30 }, { "b2fb", 60 } }, 0.5, sine (400, 0.3), o);
        CHECK (finite (o) && rmsDb (o, 0.2, 0.5) > -40.0, "grinder : algorithme");
    }
    // porte : une bande faible est coupée
    run ("grinder", { { "b2gate", -20 } }, 0.5, sine (400, 0.01), o);
    const auto gated = rmsDb (o, 0.2, 0.5);
    run ("grinder", { { "b2gate", -80 } }, 0.5, sine (400, 0.01), o);
    const auto open = rmsDb (o, 0.2, 0.5);
    CHECK (gated < open - 20.0, "grinder : porte");
    // dynamique montante : un signal faible remonte
    run ("grinder", { { "b2dyn", -100 }, { "b2type", 0 }, { "b2drive", 0 } }, 1.0, sine (400, 0.01), o);
    const auto up = rmsDb (o, 0.5, 1.0);
    run ("grinder", { { "b2dyn", 0 }, { "b2type", 0 }, { "b2drive", 0 } }, 1.0, sine (400, 0.01), o);
    const auto flat = rmsDb (o, 0.5, 1.0);
    std::printf ("  grinder : dynamique montante %+.1f dB\n", up - flat);
    CHECK (up > flat + 6.0, "grinder : dynamique");
    // générateur de réinjection : une impulsion fait sonner la bande à la fréquence choisie, et s'éteint
    run ("grinder", { { "b2fb", 95 }, { "b2fbfreq", 440 }, { "b2type", 1 }, { "b2drive", 20 } }, 3.0, [] (int i) { return i < 64 ? 0.5 : 0.0; }, o);
    const auto ring = toneDb (o, 440, 0.1, 0.4) - toneDb (o, 300, 0.1, 0.4);
    CHECK (finite (o) && ring > 6.0, "grinder : r\xC3\xA9injection accord\xC3\xA9" "e");
}

/** v0.37 : Sculptor ; v0.38 : saturations bande et lampe, compresseur à distorsion, transitoires, chorus à trois voix, réverbération de prestige. */
static void testFx3738()
{
    const std::string name = "effets v0.37-v0.38";
    using grv::dsp::kTwoPi;
    constexpr int sr = 48000, block = 32;
    using Set = std::vector<std::pair<std::string, float>>;
    auto run = [&] (const char* type, const Set& set, double seconds, const std::function<double (int, int)>& gen, std::vector<float>& l, std::vector<float>& r, bool playing = true)
    {
        auto fx = createEffect (type);
        const auto& d = fx->desc();
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); CHECK (i >= 0, id.c_str()); if (i >= 0) p[(size_t) i] = v; }
        fx->prepare (sr, block);
        const int total = (int) (seconds * sr) / block * block;
        l.assign ((size_t) total, 0.0f); r.assign ((size_t) total, 0.0f);
        std::vector<float> L ((size_t) block), R ((size_t) block);
        for (int pos = 0; pos < total; pos += block)
        {
            for (int k = 0; k < block; ++k) { L[(size_t) k] = (float) gen (pos + k, 0); R[(size_t) k] = (float) gen (pos + k, 1); }
            fx->setPosition (pos / (double) sr * 2.0, playing);
            fx->process (L.data(), R.data(), block, p.data(), 120.0);
            for (int k = 0; k < block; ++k) { l[(size_t) (pos + k)] = L[(size_t) k]; r[(size_t) (pos + k)] = R[(size_t) k]; }
        }
    };
    auto finite = [] (const std::vector<float>& v) { for (auto x : v) if (! std::isfinite (x) || std::abs (x) > 8.0f) return false; return true; };
    auto rmsDb = [&] (const std::vector<float>& v, double t0, double t1)
    {
        double e = 0.0; int c = 0;
        for (size_t i = (size_t) (t0 * sr); i < (size_t) (t1 * sr) && i < v.size(); ++i) { e += (double) v[i] * v[i]; ++c; }
        return 10.0 * std::log10 (e / std::max (1, c) + 1.0e-30);
    };
    auto toneDb = [&] (const std::vector<float>& v, double hz, double t0, double t1)
    {
        const auto w = kTwoPi * hz / sr, c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0; int nn = 0;
        for (size_t i = (size_t) (t0 * sr); i < (size_t) (t1 * sr); ++i) { const auto s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; ++nn; }
        return 20.0 * std::log10 (std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / std::max (1, nn) + 1.0e-12);
    };
    std::uint32_t seed = 5u;
    auto noise = [&] (int, int) { seed = seed * 1664525u + 1013904223u; return ((double) (seed >> 9) / 4194304.0 - 1.0) * 0.2; };
    auto sine = [] (double hz, double a) { return [hz, a] (int i, int) { return a * std::sin (kTwoPi * hz * i / 48000.0); }; };
    std::vector<float> l, r;

    // Sculptor : chaque effet seul, chaque bande, 14 formes : fini et non muet ; la pompe module le niveau au rythme
    for (int s = 1; s <= 9; ++s)
        for (int b = 0; b < 4; ++b)
        {
            const auto pre = "s" + std::to_string (s);
            run ("sculptor", { { "s7on", 0 }, { pre + "on", 1 }, { pre + "band", (float) b }, { pre + "amt", 100 }, { pre + "shape", (float) ((s * 3 + b) % 14) } }, 1.0, noise, l, r);
            CHECK (finite (l) && finite (r) && rmsDb (l, 0.3, 1.0) > -45.0, ("sculptor : " + pre).c_str());
        }
    run ("sculptor", {}, 2.0, sine (300, 0.3), l, r);
    double lo = 1.0e9, hi = -1.0e9;
    for (int k = 0; k < 40; ++k) { const auto e = rmsDb (l, 1.0 + 0.025 * k, 1.025 + 0.025 * k); lo = std::min (lo, e); hi = std::max (hi, e); }
    std::printf ("  sculptor : pompe 1/4 (80 %%) : \xC3\xA9" "cart de niveau %.1f dB\n", hi - lo);
    CHECK (hi - lo > 8.0, "sculptor : la pompe ne pompe pas");
    // temps : arrêt de bande sur la mesure → le son est relu en retard (pas de silence, pas d'explosion)
    run ("sculptor", { { "s7on", 0 }, { "s1on", 1 }, { "s1shape", 13 }, { "s1len", 4 }, { "s1amt", 100 }, { "trange", 3 } }, 4.0, sine (440, 0.3), l, r);
    CHECK (finite (l) && rmsDb (l, 0.5, 4.0) > -30.0, "sculptor : temps");
    // v0.45 : panoramique et largeur sur un son mono (gauche = droite) : il en sort de la stéréo
    auto sideDb = [&] () { double e = 0.0, m = 0.0; for (size_t i = l.size() / 4; i < l.size(); ++i) { const double d = l[i] - r[i], c = l[i] + r[i]; e += d * d; m += c * c; } return 10.0 * std::log10 ((e + 1e-12) / (m + 1e-12)); };
    run ("sculptor", { { "s7on", 0 } }, 2.0, sine (300, 0.3), l, r);
    const auto sideOff = sideDb();
    run ("sculptor", { { "s7on", 0 }, { "s8on", 1 }, { "s8amt", 100 }, { "s8shape", 3 } }, 2.0, sine (300, 0.3), l, r);
    const auto sidePan = sideDb();
    run ("sculptor", { { "s7on", 0 }, { "s9on", 1 }, { "s9amt", 100 }, { "s9shape", 12 }, { "s9len", 1 } }, 2.0, sine (300, 0.3), l, r);
    const auto sideWide = sideDb();
    std::printf ("  sculptor sur un son mono : c\xC3\xB4t\xC3\xA9s %.1f dB ; panoramique %.1f dB ; largeur %.1f dB\n", sideOff, sidePan, sideWide);
    CHECK (sidePan > sideOff + 20.0 && sideWide > sideOff + 20.0 && finite (l) && finite (r), "sculptor : panoramique et largeur sur un son mono");
    // départ à chaque attaque : coups de 0,1 s toutes les 0,5 s, transport arrêté
    run ("sculptor", { { "trigger", 1 }, { "s7shape", 10 }, { "s7len", 1 } }, 2.0, [] (int i, int) { return (i % 24000) < 4800 ? 0.3 * std::sin (i * 0.05) : 0.0; }, l, r, false);
    CHECK (finite (l), "sculptor : d\xC3\xA9part \xC3\xA0 l'attaque");

    // Saturation à bande : harmoniques, bosse du grave, vitesse ; finie
    run ("tapesat", { { "drive", 18 }, { "wow", 0 } }, 0.6, sine (200, 0.3), l, r);
    const auto h3 = toneDb (l, 600, 0.2, 0.6) - toneDb (l, 200, 0.2, 0.6);
    run ("tapesat", { { "drive", 18 }, { "bias", 60 }, { "wow", 0 } }, 0.6, sine (200, 0.3), l, r);
    const auto h2 = toneDb (l, 400, 0.2, 0.6) - toneDb (l, 200, 0.2, 0.6);
    std::printf ("  saturation \xC3\xA0 bande : H3 %.1f dB ; polaris\xC3\xA9" "e H2 %.1f dB\n", h3, h2);
    CHECK (finite (l) && h3 > -45.0 && h2 > -45.0, "saturation \xC3\xA0 bande : harmoniques");
    for (int sp = 0; sp < 3; ++sp) { run ("tapesat", { { "speed", (float) sp }, { "wow", 100 }, { "hiss", 100 }, { "drive", 24 } }, 0.5, noise, l, r); CHECK (finite (l), "saturation \xC3\xA0 bande : vitesse"); }
    // Saturation à lampe : harmoniques paires
    run ("tubesat", { { "drive", 70 }, { "bias", 50 } }, 0.6, sine (200, 0.3), l, r);
    const auto t2 = toneDb (l, 400, 0.2, 0.6) - toneDb (l, 200, 0.2, 0.6);
    std::printf ("  saturation \xC3\xA0 lampe : H2 %.1f dB\n", t2);
    CHECK (finite (l) && t2 > -30.0, "saturation \xC3\xA0 lampe : pas d'harmoniques paires");
    // Compresseur à distorsion : chaque rapport réduit (sauf 1:1), Nuke le plus fort ; fini
    double gr4 = 0.0, grN = 0.0;
    for (int ri = 0; ri < 8; ++ri)
        for (int dm = 0; dm < 3; ++dm)
        {
            run ("distcomp", { { "ratio", (float) ri }, { "audio", (float) dm }, { "output", 0 }, { "input", 18 } }, 0.8, sine (220, 0.25), l, r);
            CHECK (finite (l), "compresseur \xC3\xA0 distorsion : valeurs");
            if (dm == 0 && ri == 3) gr4 = 18.0 + 20.0 * std::log10 (0.25 / std::sqrt (2.0)) - rmsDb (l, 0.4, 0.8);
            if (dm == 0 && ri == 7) grN = 18.0 + 20.0 * std::log10 (0.25 / std::sqrt (2.0)) - rmsDb (l, 0.4, 0.8);
        }
    std::printf ("  compresseur \xC3\xA0 distorsion : r\xC3\xA9" "duction 4:1 %.1f dB, Nuke %.1f dB\n", gr4, grN);
    CHECK (gr4 > 6.0 && grN > gr4 + 3.0, "compresseur \xC3\xA0 distorsion : r\xC3\xA9" "duction");
    // Transitoires : attaque +100 % relève le début des coups, maintien -100 % creuse la queue
    auto hit = [] (int i, int) { const auto t = (i % 24000) / 48000.0; return 0.3 * std::exp (-t * 6.0) * std::sin (i * 0.06); };
    run ("transient", { { "attack", 0 }, { "sustain", 0 } }, 1.0, hit, l, r);
    const auto a0 = rmsDb (l, 0.5, 0.505), s0 = rmsDb (l, 0.75, 0.95);
    run ("transient", { { "attack", 100 }, { "sustain", -100 } }, 1.0, hit, l, r);
    const auto a1 = rmsDb (l, 0.5, 0.505), s1 = rmsDb (l, 0.75, 0.95);
    std::printf ("  transitoires : attaque %+.1f dB, queue %+.1f dB\n", a1 - a0, s1 - s0);
    CHECK (finite (l) && a1 > a0 + 3.0 && s1 < s0 - 3.0, "transitoires");
    // Chorus à trois voix : 3 modes, large (G ≠ D)
    for (int md = 0; md < 3; ++md)
    {
        run ("chorus3", { { "mode", (float) md } }, 1.0, noise, l, r);
        double d = 0.0; for (size_t i = 24000; i < l.size(); ++i) d += std::abs (l[i] - r[i]);
        CHECK (finite (l) && d > 50.0, "chorus \xC3\xA0 trois voix");
    }
    // Réverbération de prestige : déclin réglé, aigus qui meurent avant les graves, gel qui tient, queue finie
    for (int md = 0; md < 6; ++md)
    {
        run ("prestige", { { "mode", (float) md }, { "mix", 100 }, { "decay", 1.0f }, { "predelay", 0 } }, 5.0, [] (int i, int) { return i == 0 ? 1.0 : 0.0; }, l, r);
        const auto e0 = rmsDb (l, 0.05, 0.3), e1 = rmsDb (l, 1.05, 1.3), end = rmsDb (l, 4.5, 5.0);
        if (md == 0) std::printf ("  prestige (salle, 1 s) : d\xC3\xA9" "but %.1f dB, \xC3\xA0 1 s %.1f dB, fin %.1f dB\n", e0, e1, end);
        CHECK (finite (l) && e0 - e1 > 30.0 && e0 - e1 < 80.0 && end < e0 - 80.0, "prestige : d\xC3\xA9" "clin");
    }
    run ("prestige", { { "mix", 100 }, { "decay", 3 }, { "freeze", 0 } }, 1.0, noise, l, r);
    CHECK (finite (l), "prestige : bruit");
}

// v0.39 : Hopscotch à 64 pas, un effet et son réglage par pas ; le son « de base » est celui de la v0.38
static void testHop39()
{
    const std::string name = "Hopscotch v0.39";
    std::printf ("%s\n", name.c_str());
    namespace stepfx = grv::effects::stepfx;
    using grv::effects::StepFx;
    const double sr = 48000.0;
    const auto& d = stepfx::desc();
    CHECK ((int) d.params.size() == stepfx::numSlots && d.params.size() <= (size_t) insertSlots, "nombre de r\xC3\xA9glages");
    CHECK (d.indexOf ("s17") == stepfx::step17 && d.indexOf ("s64") == stepfx::stepSlot (63) && d.indexOf ("p1type") == stepfx::persoBase, "positions des r\xC3\xA9glages");
    const auto& names = stepfx::entryNames();
    const char* old[8] { "\xE2\x80\x94", "R\xC3\xA9p\xC3\xA8te", "Inverse", "Filtre", "\xC3\x89" "crase", "Arr\xC3\xAAt bande", "Hachure", "\xC3\x89" "cho" };
    bool same = true; for (int i = 0; i < 8; ++i) same = same && names[(size_t) i] == old[i];
    CHECK (same && names.size() == 51, "choix d'avant la v0.39 en t\xC3\xAAte de liste");

    auto input = [] (int i) { return (float) (0.4 * std::sin (i * 2.0 * ::kPi * 220.0 / 48000.0) * (0.6 + 0.4 * std::sin (i * 2.0 * ::kPi * 1.3 / 48000.0))
                                             + 0.15 * std::sin (i * 2.0 * ::kPi * 3100.0 / 48000.0)); };
    const int total = (int) sr * 2;
    auto runNew = [&] (const std::vector<float>& p, std::vector<float>& L, int stepsFor = 0) {
        StepFx fx; fx.prepare (sr, 64);
        L.assign ((size_t) total, 0.0f); std::vector<float> R ((size_t) total);
        for (int i = 0; i < total; ++i) L[(size_t) i] = R[(size_t) i] = input (i);
        double ppq = 0.0; int maxStep = 0;
        for (int pos = 0; pos < total; pos += 64) { fx.setPosition (ppq, true); fx.process (L.data() + pos, R.data() + pos, 64, p.data(), 120.0); ppq += 64 * 2.0 / sr; maxStep = std::max (maxStep, (int) fx.meter (0)); }
        (void) stepsFor; return maxStep;
    };
    // 1. son de base identique à la v0.38 (chaque effet seul, puis une grille mélangée)
    {
        const auto& od = grv::legacy38::stepfx::desc();
        double worst = 0.0;
        for (int kind = 0; kind <= 8; ++kind)
        {
            auto po = defaults (od); auto pn = defaults (d);
            static constexpr int mixed[16] { 0, 1, 0, 2, 3, 0, 6, 1, 0, 5, 0, 4, 7, 2, 1, 5 };
            for (int s = 0; s < 16; ++s) { po[(size_t) s] = pn[(size_t) s] = (float) (kind < 8 ? kind : mixed[s]); }
            std::vector<float> a, b ((size_t) total), br ((size_t) total);
            runNew (pn, a);
            grv::legacy38::StepFx ofx; ofx.prepare (sr, 64);
            for (int i = 0; i < total; ++i) b[(size_t) i] = br[(size_t) i] = input (i);
            double ppq = 0.0;
            for (int pos = 0; pos < total; pos += 64) { ofx.setPosition (ppq, true); ofx.process (b.data() + pos, br.data() + pos, 64, po.data(), 120.0); ppq += 64 * 2.0 / sr; }
            double diff = 0.0; for (int i = 0; i < total; ++i) diff = std::max (diff, (double) std::abs (a[(size_t) i] - b[(size_t) i]));
            worst = std::max (worst, diff);
            if (diff > 1.0e-4) std::printf ("  effet de base %d : \xC3\xA9" "cart %.6f\n", kind, diff);
        }
        std::printf ("  son de base compar\xC3\xA9 \xC3\xA0 la v0.38 : \xC3\xA9" "cart maximal %.7f\n", worst);
        CHECK (worst < 1.0e-4, "son de base inchang\xC3\xA9");
    }
    // 2. chacun des 51 choix sur tous les pas : son fini et borné, pas de silence complet (sauf « silence »)
    {
        int bad = 0;
        for (int e = 1; e < (int) names.size(); ++e)
        {
            auto p = defaults (d);
            for (int s = 0; s < 16; ++s) p[(size_t) s] = (float) e;
            std::vector<float> L; runNew (p, L);
            double peak = 0.0, rms = 0.0; bool fin = true;
            for (float x : L) { fin = fin && std::isfinite (x); peak = std::max (peak, (double) std::abs (x)); rms += (double) x * x; }
            rms = std::sqrt (rms / total);
            const bool silenceWanted = names[(size_t) e] == "Volume : silence";
            if (! fin || peak > 3.0 || (! silenceWanted && rms < 0.01)) { ++bad; std::printf ("  %s : crete %.2f, efficace %.4f\n", names[(size_t) e].c_str(), peak, rms); }
        }
        CHECK (bad == 0, "les 51 choix de pas");
    }
    // 3. 64 pas (4 mesures de 1/16) : seul le pas 50 est un silence
    {
        auto p = defaults (d);
        p[(size_t) d.indexOf ("steps")] = 64.0f;
        const auto silence = (int) (std::find (names.begin(), names.end(), std::string ("Volume : silence")) - names.begin());
        p[(size_t) stepfx::stepSlot (49)] = (float) silence;
        StepFx fx; fx.prepare (sr, 64);
        const int stepS = (int) (sr * 0.125); // 1/16 à 120 BPM
        const int n = stepS * 64;
        std::vector<float> L ((size_t) n), R ((size_t) n);
        for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = input (i);
        double ppq = 0.0; int maxStep = 0;
        for (int pos = 0; pos + 64 <= n; pos += 64) { fx.setPosition (ppq, true); fx.process (L.data() + pos, R.data() + pos, 64, p.data(), 120.0); ppq += 64 * 2.0 / sr; maxStep = std::max (maxStep, (int) fx.meter (0)); }
        auto rmsIn = [&] (int a, int b) { double e = 0.0; for (int i = a; i < b; ++i) e += (double) L[(size_t) i] * L[(size_t) i]; return std::sqrt (e / (b - a)); };
        const auto in50 = rmsIn (49 * stepS + 600, 50 * stepS - 200), in20 = rmsIn (19 * stepS + 600, 20 * stepS - 200);
        std::printf ("  64 pas : pas 50 %.5f, pas 20 %.3f, dernier pas lu %d\n", in50, in20, maxStep);
        CHECK (in50 < 1.0e-3 && in20 > 0.1 && maxStep == 64, "64 pas");
    }
    // 4. réglage perso : un passe-bas à 80 Hz sur tous les pas éteint un son à 3,1 kHz
    {
        auto p = defaults (d);
        const auto perso1 = (int) (std::find (names.begin(), names.end(), std::string ("Perso 1")) - names.begin());
        for (int s = 0; s < 16; ++s) p[(size_t) s] = (float) perso1;
        p[(size_t) d.indexOf ("p1type")] = (float) stepfx::filterFx;
        p[(size_t) d.indexOf ("p1a")] = 0.0f; p[(size_t) d.indexOf ("p1b")] = 0.0f; p[(size_t) d.indexOf ("p1c")] = 0.0f; p[(size_t) d.indexOf ("p1d")] = 50.0f;
        StepFx fx; fx.prepare (sr, 64);
        std::vector<float> L ((size_t) 48000), R ((size_t) 48000);
        for (int i = 0; i < 48000; ++i) L[(size_t) i] = R[(size_t) i] = (float) (0.5 * std::sin (i * 2.0 * ::kPi * 3100.0 / sr));
        double ppq = 0.0;
        for (int pos = 0; pos < 48000; pos += 64) { fx.setPosition (ppq, true); fx.process (L.data() + pos, R.data() + pos, 64, p.data(), 120.0); ppq += 64 * 2.0 / sr; }
        double e = 0.0; for (int i = 24000; i < 48000; ++i) e += (double) L[(size_t) i] * L[(size_t) i];
        const auto db = 20.0 * std::log10 (std::sqrt (e / 24000.0) / (0.5 / std::sqrt (2.0)));
        std::printf ("  perso 1 (passe-bas 80 Hz) : %.1f dB sur 3,1 kHz\n", db);
        CHECK (db < -40.0, "r\xC3\xA9glage perso");
    }
}

/** v0.39 : écriture expressive de la voix interne (pauses, étirements, capitales, ! et ?). */
static void testSpeech39()
{
    const std::string name = "voix v0.39";
    std::printf ("%s\n", name.c_str());
    using grv::engine::compileSpeech;
    auto total = [] (const char* t) { const auto sc = compileSpeech (t); double d = 0.0; if (sc) for (int i = 0; i < sc->count; ++i) d += sc->ph[(size_t) i].dur; return d; };
    auto range = [] (const char* t) { const auto sc = compileSpeech (t); float lo = 99.0f, hi = -99.0f; if (sc) for (int i = 0; i < sc->count; ++i) if (sc->ph[(size_t) i].kind == 0) { lo = std::min (lo, sc->ph[(size_t) i].pitch); hi = std::max (hi, sc->ph[(size_t) i].pitch); } return std::pair<float, float> { lo, hi }; };
    const auto base = total ("les mutants sont parmi nous"), spaced = total ("les mutants sont          parmi nous"),
               dots = total ("les mutants sont... parmi nous"), stretched = total ("les muuuutants sont parmi nous");
    std::printf ("  dur\xC3\xA9" "es : normale %.2f, espaces %.2f, points de suspension %.2f, voyelle \xC3\xA9tir\xC3\xA9" "e %.2f\n", base, spaced, dots, stretched);
    CHECK (spaced > base + 2.5 && dots > base + 1.2 && stretched > base + 0.8, "pauses et \xC3\xA9tirements");
    const auto q = range ("les mutants sont parmi nous ?"), ex = range ("les mutants sont parmi nous !"), st = range ("les mutants sont parmi nous.");
    const auto caps = range ("les MUTANTS sont parmi nous.");
    std::printf ("  hauteurs : point %.1f..%.1f, question ..%.1f, exclamation %.1f..%.1f, capitales ..%.1f\n", st.first, st.second, q.second, ex.first, ex.second, caps.second);
    CHECK (q.second > st.second + 1.0f && ex.second > st.second + 1.0f && caps.second > st.second + 1.5f, "intonation");
    // la phrase reste dite dans le même nombre de mots, et la voix ne sort rien d'invalide
    const auto sc = compileSpeech ("Oh NON !!   les muuutants...\nsont parmi nous ?!");
    CHECK (sc != nullptr && sc->count < grv::engine::SpeechScript::maxPhones, "phrase expressive");
    if (sc)
    {
        grv::synth::DemoVoice dv; dv.prepare (48000.0); dv.setScript (sc.get());
        bool fin = true; double peak = 0.0;
        for (int i = 0; i < 48000 * 6; ++i) { const auto x = dv.next (2.5, 125.0, 2.0); fin = fin && std::isfinite (x); peak = std::max (peak, std::abs (x)); }
        CHECK (fin && peak < 4.0, "voix expressive born\xC3\xA9" "e");
    }
    // anglais : règles lettres → sons et lexique (« @en » en tête)
    {
        using namespace grv::engine::speech;
        auto phones = [] (const char* t) { std::vector<int> v; const auto sc = compileSpeech (t); if (sc) for (int i = 0; i < sc->count; ++i) v.push_back ((int) sc->ph[(size_t) i].kind * 1000 + (int) (sc->ph[(size_t) i].f3)); return v; };
        auto has = [] (const char* t, int ph) { std::vector<int> out; grv::engine::speech_en::readWord ([] (const char* x) { std::u32string u; for (; *x; ++x) u.push_back ((char32_t) *x); return u; } (t), out);
                                                return std::find (out.begin(), out.end(), ph) != out.end(); };
        const bool th = has ("think", TH) && has ("this", DH) && has ("sing", NG) && has ("cat", AE) && has ("house", HH) && has ("bird", ER) && has ("red", RE);
        const bool fr = ! grv::engine::speechIsEnglish ("les mutants") && grv::engine::speechIsEnglish ("@en hello");
        const auto en = compileSpeech ("@en Hello world, the mutants are among us! Don't you think so?");
        std::printf ("  anglais : %d phon\xC3\xA8mes, sons propres %s\n", en ? en->count : 0, th ? "oui" : "non");
        CHECK (th && fr && en != nullptr && en->count > 30 && phones ("@en the") != phones ("le"), "voix anglaise");
    }
}

// v0.40 : Pieuvre 8 — réserve commune, attributs par emplacement (fondus, rognage, note), tours de boucle
/** v0.41 : Pieuvre 8 — Début suivi pendant la lecture (LFO sur Début). Échantillon en rampe : la
    sortie dit où lit la tête. */
static void testOcta41()
{
    const std::string name = "Pieuvre 8 v0.41 (d\xC3\xA9" "but suivi)";
    std::printf ("%s\n", name.c_str());
    namespace oc = grv::instruments::octa;
    const auto& d = oc::desc();
    auto idx = [&] (const char* id) { return (size_t) d.indexOf (std::string ("t1_") + id); };
    CHECK (d.indexOf ("t1_startlive") == oc::startLive, "position du r\xC3\xA9glage");
    constexpr double sr = 48000.0;
    static SampleBuffer ramp;
    ramp.sampleRate = sr; ramp.data.resize (96000);
    for (size_t i = 0; i < ramp.data.size(); ++i) ramp.data[i] = (float) (0.8 * (double) i / (double) ramp.data.size());
    auto run = [&] (bool live, float& jump, float& lastValue, bool& finite)
    {
        auto inst = createInstrument ("octa");
        inst->prepare (sr, 32); inst->setTempo (120.0); inst->setSample (0, &ramp);
        auto p = defaults (d);
        p[idx ("velamp")] = 0.0f; p[idx ("startlive")] = live ? 1.0f : 0.0f;
        std::vector<float> L (32), R (32);
        auto block = [&] { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); inst->render (L.data(), R.data(), 32, p.data()); };
        block();
        inst->noteOn (60, 1.0f);
        for (int b = 0; b < 20; ++b) block();
        float prev = L[31];
        jump = 0.0f; finite = true;
        p[idx ("start")] = 50.0f; // Début déplacé pendant la lecture (comme par un LFO)
        for (int b = 0; b < 4; ++b)
        {
            block();
            for (float x : L) { finite = finite && std::isfinite (x); jump = std::max (jump, std::abs (x - prev)); prev = x; }
        }
        lastValue = L[31];
    };
    float jOff = 0, vOff = 0, jOn = 0, vOn = 0; bool fOff = true, fOn = true;
    run (false, jOff, vOff, fOff);
    run (true, jOn, vOn, fOn);
    std::printf ("  au d\xC3\xA9part : lecture \xC3\xA0 %.3f ; en continu : %.3f (plus grand \xC3\xA9" "cart entre deux images %.4f)\n", vOff, vOn, jOn);
    CHECK (fOff && fOn, "valeurs finies");
    CHECK (vOff < 0.05f, "au d\xC3\xA9part : D\xC3\xA9" "but lu au d\xC3\xA9" "clenchement seulement");
    CHECK (vOn > 0.38f && vOn < 0.45f, "en continu : la t\xC3\xAA" "te suit D\xC3\xA9" "but");
    CHECK (jOn < 0.02f, "en continu : glissement sans saut (pas de clic)");
}

static void testOcta42()
{
    const std::string name = "Pieuvre 8 v0.42 (8 pistes internes)";
    std::printf ("%s\n", name.c_str());
    namespace oc = grv::instruments::octa;
    const auto& d = oc::desc();
    auto idx = [&] (const std::string& id) { return (size_t) d.indexOf (id); };
    CHECK (d.indexOf ("e1bars") == oc::attrSlot (0, oc::aBars) && d.indexOf ("e128note") == oc::attrSlot (127, oc::aNote)
           && d.indexOf ("t1_loops") == oc::loops && d.indexOf ("t8_startlive") == oc::unitParam (7, oc::startLive)
           && (int) d.params.size() <= grv::engine::instrumentSlots && d.drumKit && d.laneNotes && d.padNames.size() == 8, "positions des r\xC3\xA9glages");
    CHECK ((int) d.params[idx ("t1_slot")].choices.size() == oc::numSlotChoices && d.params[idx ("t1_slot")].choices[17] == "T1 \xC2\xB7 1", "choix d'emplacement");
    constexpr double sr = 48000.0;
    static SampleBuffer tone;
    tone.sampleRate = sr; tone.data.resize (24000);
    for (size_t i = 0; i < tone.data.size(); ++i) tone.data[i] = (float) (0.5 * std::sin ((double) i * 0.05));
    auto peakOver = [&] (Instrument& inst, std::vector<float>& p, int blocks, std::vector<float>* env = nullptr) {
        std::vector<float> L (32), R (32);
        float peak = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst.render (L.data(), R.data(), 32, p.data());
            float bp = 0.0f; for (float x : L) bp = std::max (bp, std::abs (x));
            peak = std::max (peak, bp);
            if (env) env->push_back (bp);
        }
        return peak;
    };
    // 1. réserve commune : T1 joue l'emplacement 3 de T2 (T2 · 3 = réserve 18), avec l'attribut de gain de cet emplacement
    {
        auto a = createInstrument ("octa");
        a->prepare (sr, 32); a->setTempo (120.0);
        a->setSample (18, &tone);
        auto p = defaults (d);
        p[idx ("t1_slot")] = (float) (oc::poolBase + 16 + 2); p[idx ("t1_velamp")] = 0.0f;
        peakOver (*a, p, 2);
        a->noteOn (0, 1.0f); // pad 1 = T1
        const auto fromT2 = peakOver (*a, p, 100);
        p[oc::attrSlot (18, oc::aGain)] = -12.0f;
        a->noteOn (0, 1.0f);
        const auto quiet = peakOver (*a, p, 100);
        std::printf ("  r\xC3\xA9serve : T2\xC2\xB7" "3 jou\xC3\xA9 par T1 %.3f, gain de l'emplacement -12 dB %.3f\n", fromT2, quiet);
        CHECK (fromT2 > 0.3f && std::abs (quiet / fromT2 - 0.251f) < 0.05f, "r\xC3\xA9serve commune et attributs");
        a->noteOn (0, 1.0f); peakOver (*a, p, 4);
        a->setSample (18, nullptr);
        CHECK (peakOver (*a, p, 20) < 1.0e-6f, "t\xC3\xAAte arr\xC3\xAAt\xC3\xA9" "e quand l'emplacement change");
    }
    // 2. lignes : un pas de la ligne 3 (note encodée) joue T3, la coupure de sa ligne le tait ; clavier sur T2
    {
        static SampleBuffer s3, s2;
        for (auto* x : { &s3, &s2 }) { x->sampleRate = sr; x->data.resize (48000); for (size_t i = 0; i < x->data.size(); ++i) x->data[i] = (float) ((x == &s3 ? 0.6 : 0.2) * std::sin ((double) i * 0.07)); }
        auto inst = createInstrument ("octa");
        inst->prepare (sr, 32); inst->setTempo (120.0);
        inst->setSample (32, &s3); // T3 · 1
        inst->setSample (16, &s2); // T2 · 1
        auto p = defaults (d);
        for (int k = 1; k <= 8; ++k) { p[idx ("t" + std::to_string (k) + "_velamp")] = 0.0f; p[idx ("t" + std::to_string (k) + "_holdmode")] = 2.0f; p[idx ("t" + std::to_string (k) + "_loop")] = 1.0f; }
        peakOver (*inst, p, 2);
        inst->noteOn (128 + 2 * 128 + 60, 1.0f);
        const auto t3 = peakOver (*inst, p, 200);
        inst->setLaneMutes (1u << 2);
        peakOver (*inst, p, 10); // 5 ms de fondu
        const auto muted = peakOver (*inst, p, 50);
        inst->setLaneMutes (0);
        const auto back = peakOver (*inst, p, 50);
        inst->allNotesOff(); peakOver (*inst, p, 200);
        inst->setPlayLane (1);
        inst->noteOn (60, 1.0f);
        const auto t2 = peakOver (*inst, p, 200);
        std::printf ("  ligne 3 : %.3f, coup\xC3\xA9" "e %.5f, rendue %.3f ; clavier sur T2 : %.3f\n", t3, muted, back, t2);
        CHECK (std::abs (t3 - 0.6f) < 0.1f && muted < 1.0e-4f && back > 0.5f, "ligne jou\xC3\xA9" "e, coup\xC3\xA9" "e, rendue");
        CHECK (std::abs (t2 - 0.2f) < 0.06f, "clavier : piste interne jou\xC3\xA9" "e");
    }
    // 2b. v0.43 : clavier en zones — T1 sur Do0-Si0 (24-35), T2 sur Do1-Si1 (36-47) : chaque note joue la piste de sa zone
    {
        static SampleBuffer z1, z2;
        for (auto* x : { &z1, &z2 }) { x->sampleRate = sr; x->data.resize (48000); for (size_t i = 0; i < x->data.size(); ++i) x->data[i] = (float) ((x == &z1 ? 0.2 : 0.6) * std::sin ((double) i * 0.07)); }
        auto inst = createInstrument ("octa");
        inst->prepare (sr, 32); inst->setTempo (120.0);
        inst->setSample (0, &z1); inst->setSample (16, &z2);
        auto p = defaults (d);
        for (int k = 1; k <= 2; ++k) { p[idx ("t" + std::to_string (k) + "_velamp")] = 0.0f; p[idx ("t" + std::to_string (k) + "_holdmode")] = 2.0f; p[idx ("t" + std::to_string (k) + "_loop")] = 1.0f; }
        p[(size_t) oc::kbModeIndex()] = 1.0f;
        peakOver (*inst, p, 2);
        int lane = -1, stepNote = -1;
        const bool mapped = inst->mapNote (40, lane, stepNote);
        inst->noteOn (40, 1.0f);
        const auto z2peak = peakOver (*inst, p, 200);
        inst->allNotesOff(); peakOver (*inst, p, 200);
        inst->noteOn (30, 1.0f);
        const auto z1peak = peakOver (*inst, p, 200);
        std::printf ("  zones : Mi1 \xE2\x86\x92 T%d (note du pas %d) %.3f ; Fa#0 \xE2\x86\x92 %.3f\n", lane + 1, stepNote, z2peak, z1peak);
        CHECK (mapped && lane == 1 && stepNote == 64 && std::abs (z2peak - 0.6f) < 0.1f && std::abs (z1peak - 0.2f) < 0.06f, "clavier en zones");
        CHECK (d.params[idx ("t3_zlo")].def == 48.0f && d.params[idx ("t3_zhi")].def == 59.0f, "zones d'origine");
    }
    // 3. fondus d'entrée et de sortie, rognage, tours de boucle, note de l'emplacement (T1)
    {
        auto inst = createInstrument ("octa");
        inst->prepare (sr, 32); inst->setTempo (120.0); inst->setSample (0, &tone);
        auto p = defaults (d);
        p[idx ("t1_velamp")] = 0.0f;
        p[oc::attrSlot (0, oc::aFadeIn)] = 200.0f; p[oc::attrSlot (0, oc::aFadeOut)] = 200.0f;
        std::vector<float> env;
        peakOver (*inst, p, 2);
        inst->noteOn (60, 1.0f);
        peakOver (*inst, p, 760, &env); // 0,5 s : tout l'échantillon
        const auto early = env[30], mid = env[370], late = env[740];
        std::printf ("  fondus : 20 ms %.3f, milieu %.3f, 10 ms avant la fin %.3f\n", early, mid, late);
        CHECK (early < mid * 0.5f && late < mid * 0.3f && mid > 0.4f, "fondus de l'emplacement");
        p[oc::attrSlot (0, oc::aFadeIn)] = 0.0f; p[oc::attrSlot (0, oc::aFadeOut)] = 0.0f;
        p[oc::attrSlot (0, oc::aEnd)] = 50.0f; p[idx ("t1_loop")] = 1.0f; p[idx ("t1_loops")] = 2.0f; p[idx ("t1_holdmode")] = 2.0f;
        env.clear();
        inst->noteOn (60, 1.0f);
        peakOver (*inst, p, 1500, &env); // 1 s
        int lastLoud = 0; for (int i = 0; i < (int) env.size(); ++i) if (env[(size_t) i] > 0.05f) lastLoud = i;
        const auto heardS = lastLoud * 32.0 / sr;
        std::printf ("  boucle de 2 tours sur 0,25 s : son jusqu'\xC3\xA0 %.2f s\n", heardS);
        CHECK (heardS > 0.45 && heardS < 0.6, "tours de boucle");
        p[idx ("t1_loop")] = 0.0f; p[idx ("t1_keymode")] = 2.0f; p[oc::attrSlot (0, oc::aNote)] = 52.0f;
        inst->noteOn (52, 1.0f);
        CHECK (peakOver (*inst, p, 100) > 0.3f, "note propre \xC3\xA0 l'emplacement");
    }
}

// v0.44 : Old Metal — FM à 6 opérateurs
static void testMuse()
{
    const std::string name = "World Tour (poly analogique 8 voix)";
    std::printf ("%s\n", name.c_str());
    namespace f = grv::instruments::muse;
    const auto& d = f::desc();
    CHECK (d.indexOf ("o1oct") == f::o1Oct && d.indexOf ("level") == f::level && d.indexOf ("dlydiff") == f::dlyDiff && d.indexOf ("tune") == f::tune
           && (int) d.params.size() <= grv::engine::instrumentSlots && d.family == "Polys", "positions des r\xC3\xA9glages");
    constexpr double sr = 48000.0;
    struct Out { float peak = 0.0f; bool finite = true; double rough = 0.0; double rmsL = 0.0, rmsR = 0.0; bool activeAfter = true; double corr = 0.0; };
    auto run = [&] (std::initializer_list<std::pair<const char*, float>> set, int note = 60, float vel = 1.0f, int blocks = 300, bool releaseAfter = false, int chord = 0)
    {
        Out o;
        grv::instruments::MuseSynth inst;
        inst.prepare (sr, 64);
        inst.setTempo (120.0);
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); if (i >= 0) p[(size_t) i] = v; else std::printf ("  ECHEC r\xC3\xA9glage inconnu %s\n", id); }
        std::vector<float> L (64), R (64);
        inst.noteOn (note, vel);
        for (int c = 1; c <= chord; ++c) inst.noteOn (note + c * 4, vel);
        double prev = 0.0, sumAbs = 0.0, sumDiff = 0.0, lr = 0.0;
        for (int k = 0; k < blocks; ++k)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst.render (L.data(), R.data(), 64, p.data());
            for (int i = 0; i < 64; ++i)
            {
                const auto x = (double) L[(size_t) i], y = (double) R[(size_t) i];
                o.finite = o.finite && std::isfinite (x) && std::isfinite (y);
                o.peak = std::max (o.peak, (float) std::max (std::abs (x), std::abs (y)));
                if (k > 20) { sumAbs += std::abs (x); sumDiff += std::abs (x - prev); o.rmsL += x * x; o.rmsR += y * y; lr += x * y; }
                prev = x;
            }
        }
        o.rough = sumAbs > 0.0 ? sumDiff / sumAbs : 0.0;
        o.corr = lr / std::sqrt (std::max (1e-12, o.rmsL * o.rmsR));
        if (releaseAfter)
        {
            inst.noteOff (note);
            for (int c = 1; c <= chord; ++c) inst.noteOff (note + c * 4);
            for (int k = 0; k < (int) (16.0 * sr / 64); ++k) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); inst.render (L.data(), R.data(), 64, p.data()); }
            o.activeAfter = inst.isActive();
        }
        return o;
    };
    const auto def = run ({}, 48, 1.0f, 300, true, 3);
    std::printf ("  accord : cr\xC3\xAAte %.3f, \xC3\xA9teint apr\xC3\xA8s 16 s : %s\n", def.peak, def.activeAfter ? "non" : "oui");
    CHECK (def.finite && def.peak > 0.02f && def.peak < 4.0f && ! def.activeAfter, "joue et s'\xC3\xA9teint");
    // filtres : chaque routage et chaque mode, résonance maximale : finis, bornés
    bool routes = true;
    for (float r : { 0.0f, 1.0f, 2.0f }) for (float hp : { 0.0f, 1.0f }) for (float lk : { 0.0f, 1.0f })
    {
        const auto o = run ({ { "route", r }, { "f1mode", hp }, { "flink", lk }, { "f1res", 100 }, { "f2res", 100 }, { "overload", 100 } }, 36, 1.0f, 150);
        if (! o.finite || o.peak > 4.0f) { routes = false; std::printf ("  routage %.0f / %.0f / %.0f : cr\xC3\xAAte %.3f\n", r, hp, lk, o.peak); }
    }
    CHECK (routes, "routages s\xC3\xA9rie / parall\xC3\xA8le / st\xC3\xA9r\xC3\xA9o, r\xC3\xA9sonance maximale");
    // stéréo : filtres différents à gauche et à droite
    const auto st = run ({ { "route", 2 }, { "f1cut", 300 }, { "f2cut", 6000 }, { "spread", 0 }, { "fenv", 0 } }, 48);
    const auto par = run ({ { "route", 1 }, { "f1cut", 300 }, { "f2cut", 6000 }, { "spread", 0 }, { "fenv", 0 } }, 48);
    std::printf ("  st\xC3\xA9r\xC3\xA9o : corr\xC3\xA9lation G/D %.2f ; parall\xC3\xA8le %.2f\n", st.corr, par.corr);
    CHECK (st.corr < 0.9 && par.corr > 0.99, "routage st\xC3\xA9r\xC3\xA9o");
    // synchro et FM : plus riches
    const auto plain = run ({ { "mxo1", 0 }, { "f1cut", 18000 }, { "f2cut", 18000 }, { "fenv", 0 } }, 48);
    const auto sync = run ({ { "mxo1", 0 }, { "o2sync", 1 }, { "o2semi", 7 }, { "f1cut", 18000 }, { "f2cut", 18000 }, { "fenv", 0 } }, 48);
    const auto fmo = run ({ { "mxo2", 0 }, { "o1fm", 80 }, { "o1wave", 0 }, { "f1cut", 18000 }, { "f2cut", 18000 }, { "fenv", 0 } }, 48);
    const auto sine = run ({ { "mxo2", 0 }, { "o1wave", 0 }, { "f1cut", 18000 }, { "f2cut", 18000 }, { "fenv", 0 } }, 48);
    std::printf ("  synchro %.3f (sans %.3f) ; FM %.3f (sans %.3f)\n", sync.rough, plain.rough, fmo.rough, sine.rough);
    CHECK (sync.rough > plain.rough * 1.1 && fmo.rough > sine.rough * 1.3, "synchro 2 \xE2\x86\x92 1, FM 2 \xE2\x86\x92 1");
    // oscillateur de modulation audio entendu ; vers le filtre
    const auto mo = run ({ { "mxo1", 0 }, { "mxo2", 0 }, { "mxmod", 100 }, { "moaudio", 1 }, { "mokey", 1 }, { "morate", 261.6f } }, 60);
    std::printf ("  oscillateur de modulation seul (audio) : cr\xC3\xAAte %.3f\n", mo.peak);
    CHECK (mo.peak > 0.02f, "oscillateur de modulation en plage audio");
    // retard diffusant : la traîne reste après la note, puis s'éteint
    const auto dly = run ({ { "dlymix", 60 }, { "dlyfb", 60 }, { "e2r", 20 } }, 60, 1.0f, 40, true);
    std::printf ("  retard : actif apr\xC3\xA8s 16 s : %s\n", dly.activeAfter ? "oui" : "non");
    CHECK (dly.finite && ! dly.activeAfter, "retard diffusant s'\xC3\xA9teint");
    // enveloppe en boucle : le filtre bat (variation lente du niveau)
    const auto loopE = run ({ { "e1loop", 1 }, { "e1a", 50 }, { "e1d", 80 }, { "e1s", 0 }, { "fenv", 80 }, { "f1cut", 200 }, { "f2cut", 200 } }, 48, 1.0f, 600);
    CHECK (loopE.finite && loopE.peak > 0.01f, "enveloppe en boucle");
}

/** v0.45.1 : un préréglage qui change le mode de jeu (poly ↔ mono legato) ne doit pas rendre le synthé muet :
    ni les notes jouées dans le même bloc, ni les suivantes (relâchement resté en attente sur une voix muette). */
/** v0.46 : famille Drones — Hot Sauce, Octo Lyma, Komodo Dragon : chaque mode sonne, reste fini,
    et s'éteint après le relâchement. */
static void testDrones()
{
    struct Case { const char* type; std::vector<std::pair<const char*, float>> set; const char* what; };
    const std::vector<Case> cases {
        { "hots", { { "atype", 0 } }, "analogique" }, { "hots", { { "atype", 1 }, { "ap1", 100 }, { "ap2", 60 } }, "table d'ondes" },
        { "hots", { { "atype", 2 }, { "ap2", 50 } }, "harmonique" }, { "hots", { { "atype", 3 }, { "ap1", 60 }, { "ap2", 80 } }, "modal" },
        { "hots", { { "btype", 1 }, { "blvl", 80 }, { "fmab", 90 }, { "route", 100 }, { "f2drive", 90 }, { "f2res", 90 } }, "FM A vers B, parall\xC3\xA8le" },
        { "hots", { { "sub", 100 }, { "noise", 100 } }, "utilitaire" },
        { "lyra", {}, "plaques" }, { "lyra", { { "play", 1 } }, "clavier" },
        { "lyra", { { "p1mod", 90 }, { "p2mod", 90 }, { "p3mod", 90 }, { "p4mod", 90 } }, "FM crois\xC3\xA9" "e" },
        { "lyra", { { "p1mod", 80 }, { "p1src", 1 }, { "p2mod", 80 }, { "p2src", 2 }, { "lfomode", 1 }, { "lfolink", 1 } }, "LFO et retour" },
        { "lyra", { { "q1hold", 100 }, { "q2hold", 100 }, { "dmod1", 100 }, { "dmod2", 100 }, { "dmodsrc", 1 }, { "dfb", 60 }, { "drive", 100 } }, "tenue, retard, distorsion" },
        { "lyra", { { "fcut", 400 }, { "freso", 100 }, { "flfo", 100 } }, "filtre passe-bas r\xC3\xA9sonant, LFO" },
        { "lyra", { { "fmode", 1 }, { "fcut", 900 }, { "freso", 80 } }, "filtre passe-bande" },
        { "lyra", { { "fmode", 2 }, { "fcut", 2000 }, { "freso", 60 }, { "drive", 100 } }, "filtre passe-haut" },
        { "lyra", { { "lfosync", 1 }, { "lfoadiv", 10 }, { "flfo", 80 }, { "fcut", 1500 }, { "dsync", 1 }, { "ddiv1", 10 }, { "ddiv2", 0 }, { "dmix", 60 }, { "dfb", 80 } }, "LFO et retard au tempo" },
        { "bitm", {}, "sinus" }, { "bitm", { { "o1a", 85 }, { "res", 5 }, { "mmode", 1 } }, "256 segments, inverse" },
        { "bitm", { { "o1a", 15 }, { "o1b", 240 }, { "denv", 100 }, { "dloop", 1 }, { "da", 50 }, { "dd", 80 }, { "mmode", 2 } }, "delta en boucle, redresse" },
        { "bitm", { { "mix2", 100 }, { "o2semi", 7 }, { "noise", 100 }, { "noisehz", 300 }, { "ftype", 3 }, { "pamt", 24 } }, "osc. 2, bruit, r\xC3\xA9jection" },
    };
    for (const auto& c : cases)
    {
        const std::string name = std::string (c.type) + " \xC2\xB7 " + c.what;
        const auto* d = findInstrument (c.type);
        auto inst = createInstrument (c.type);
        if (d == nullptr || ! inst) { CHECK (false, "fabrique"); continue; }
        CHECK (d->family == "Drones" && (int) d->params.size() <= grv::engine::instrumentSlots, "famille, r\xC3\xA9glages");
        constexpr int sr = 48000, block = 64;
        inst->prepare (sr, block);
        auto p = defaults (*d);
        for (const auto& [id, v] : c.set) { const auto i = d->indexOf (id); CHECK (i >= 0, id); if (i >= 0) p[(size_t) i] = v; }
        std::vector<float> L (block), R (block);
        float peak = 0.0f, tail = 0.0f;
        bool finite = true;
        for (int n : { 48, 52, 55, 59 }) inst->noteOn (n, 0.9f);
        for (int b = 0; b < sr * 2 / block; ++b)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), block, p.data());
            for (int i = 0; i < block; ++i) { finite = finite && std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]); peak = std::max (peak, std::abs (L[(size_t) i])); }
        }
        for (int n : { 48, 52, 55, 59 }) inst->noteOff (n);
        for (int b = 0; b < sr * 14 / block; ++b)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), block, p.data());
            if (b > sr * 13 / block) for (int i = 0; i < block; ++i) tail = std::max (tail, std::abs (L[(size_t) i]));
        }
        std::printf ("  %-40s cr\xC3\xAA" "te %5.2f  fin %.6f\n", name.c_str(), peak, tail);
        CHECK (finite, "valeurs invalides");
        CHECK (peak > 0.01f, "trop faible");
        CHECK (peak < 4.0f, "trop fort");
        CHECK (tail < 0.001f, "ne s'\xC3\xA9teint pas");
    }
}

static void testModeSwitch()
{
    for (const char* type : { "fm4", "fm6", "fmx", "ob6", "poly5", "muse" })
    {
        const std::string name = type;
        const auto* d = findInstrument (type);
        auto inst = createInstrument (type);
        if (d == nullptr || ! inst) { CHECK (false, type); continue; }
        const auto vm = d->indexOf ("vmode");
        if (vm < 0) continue;
        constexpr int sr = 48000, block = 32;
        inst->prepare (sr, block);
        auto p = defaults (*d);
        std::vector<float> L (block), R (block);
        auto play = [&] (int blocks, bool chord) {
            float pk = 0.0f;
            for (int b = 0; b < blocks; ++b)
            {
                if (chord && b == 0) for (int n : { 48, 55, 60 }) inst->noteOn (n, 0.8f);
                if (chord && b == blocks / 2) for (int n : { 48, 55, 60 }) inst->noteOff (n);
                std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
                inst->render (L.data(), R.data(), block, p.data());
                for (int i = 0; i < block; ++i) pk = std::max (pk, std::abs (L[(size_t) i]));
            }
            return pk;
        };
        play (sr / block / 2, true);
        const float modes[] { 2.0f, 0.0f, 1.0f, 0.0f };
        for (const auto m : modes)
        {
            p[(size_t) vm] = m;               // changement de mode et accord dans le même bloc
            const auto same = play (sr / block / 2, true);
            const auto next = play (sr / block / 2, true); // accord suivant
            CHECK (same > 0.01f && next > 0.01f, (std::string (type) + " : muet apr\xC3\xA8s changement de mode").c_str());
        }
    }
}

static void testFmx()
{
    const std::string name = "Komodo Fuel (FM \xC3\xA0 algorithme libre)";
    std::printf ("%s\n", name.c_str());
    namespace f = grv::instruments::fmx;
    const auto& d = f::desc();
    CHECK (d.indexOf ("o1_wave") == f::opParam (0, f::oWave) && d.indexOf ("o6_out") == f::opParam (5, f::oOut) && d.indexOf ("m66") == f::matParam (5, 5)
           && d.indexOf ("m21") == f::matParam (1, 0) && (int) d.params.size() <= grv::engine::instrumentSlots && d.family == "Synth\xC3\xA9s FM", "positions des r\xC3\xA9glages");
    constexpr double sr = 48000.0;
    struct Out { float peak = 0.0f; bool finite = true; double rough = 0.0; int crossings = 0; bool activeAfter = true; };
    auto run = [&] (std::initializer_list<std::pair<const char*, float>> set, int note = 69, int blocks = 300, bool releaseAfter = false)
    {
        Out o;
        grv::instruments::FmxSynth inst;
        inst.prepare (sr, 64);
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); if (i >= 0) p[(size_t) i] = v; else std::printf ("  ECHEC r\xC3\xA9glage inconnu %s\n", id); }
        std::vector<float> L (64), R (64);
        inst.noteOn (note, 1.0f);
        double prev = 0.0, sumAbs = 0.0, sumDiff = 0.0;
        for (int k = 0; k < blocks; ++k)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst.render (L.data(), R.data(), 64, p.data());
            for (int i = 0; i < 64; ++i)
            {
                const auto x = (double) L[(size_t) i];
                o.finite = o.finite && std::isfinite (x);
                o.peak = std::max (o.peak, std::abs ((float) x));
                if (k > 20) { sumAbs += std::abs (x); sumDiff += std::abs (x - prev); if ((x >= 0.0) != (prev >= 0.0)) ++o.crossings; }
                prev = x;
            }
        }
        o.rough = sumAbs > 0.0 ? sumDiff / sumAbs : 0.0;
        if (releaseAfter)
        {
            inst.noteOff (note);
            for (int k = 0; k < (int) (8.0 * sr / 64); ++k) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); inst.render (L.data(), R.data(), 64, p.data()); }
            o.activeAfter = inst.isActive();
        }
        return o;
    };
    const auto sine = run ({ { "m21", 0 }, { "o1_sus", 100 } });
    const auto expected = 2.0 * 440.0 * (279 * 64) / sr;
    const auto fm = run ({ { "m21", 60 }, { "o1_sus", 100 }, { "o2_sus", 100 } });
    std::printf ("  sinus : %d passages (attendu ~%.0f) ; 2\xE2\x86\x92" "1 : rugosit\xC3\xA9 %.3f (sinus %.3f)\n", sine.crossings, expected, fm.rough, sine.rough);
    CHECK (std::abs (sine.crossings - expected) < expected * 0.03 && fm.rough > sine.rough * 1.5, "fr\xC3\xA9quence juste, modulation 2 \xE2\x86\x92 1");
    // toutes les formes jouent ; toute la matrice à fond (boucles comprises) : fini, borné
    bool wavesOk = true;
    for (int w = 0; w < f::numWaves; ++w) { const auto o = run ({ { "o1_wave", (float) w }, { "m21", 0 } }, 60, 60); wavesOk = wavesOk && o.finite && o.peak > 0.01f && o.peak < 4.0f; }
    CHECK (wavesOk, "les 12 formes");
    grv::instruments::FmxSynth inst;
    inst.prepare (sr, 64);
    auto p = defaults (d);
    for (int j = 0; j < 6; ++j) { p[(size_t) f::opParam (j, f::oOut)] = 50; p[(size_t) f::opParam (j, f::oSus)] = 100; for (int k = 0; k < 6; ++k) p[(size_t) f::matParam (j, k)] = 100; }
    std::vector<float> L (64), R (64);
    inst.noteOn (60, 1.0f);
    bool fin = true; float pk = 0.0f;
    for (int k = 0; k < 400; ++k) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); inst.render (L.data(), R.data(), 64, p.data()); for (auto x : L) { fin = fin && std::isfinite (x); pk = std::max (pk, std::abs (x)); } }
    std::printf ("  matrice pleine (boucles) : cr\xC3\xAAte %.3f\n", pk);
    CHECK (fin && pk < 4.0f, "matrice pleine, boucles");
    // fréquence fixe, rétroaction, extinction, boucle d'enveloppe
    const auto fixed = run ({ { "m21", 0 }, { "o1_fixed", 1 }, { "o1_fixhz", 100 }, { "o1_sus", 100 } }, 30);
    const auto fbk = run ({ { "m21", 0 }, { "m11", 70 }, { "o1_sus", 100 } });
    const auto rel = run ({}, 60, 100, true);
    std::printf ("  fixe 100 Hz : %d passages ; r\xC3\xA9troaction %.3f\n", fixed.crossings, fbk.rough);
    CHECK (std::abs (fixed.crossings - 2.0 * 100.0 * (279 * 64) / sr) < 8 && fbk.rough > sine.rough * 1.5 && ! rel.activeAfter, "fr\xC3\xA9quence fixe, r\xC3\xA9troaction, extinction");
}

static void testFm4()
{
    const std::string name = "New Metal (FM 4 op\xC3\xA9rateurs)";
    std::printf ("%s\n", name.c_str());
    namespace f = grv::instruments::fm4;
    const auto& d = f::desc();
    CHECK (d.indexOf ("algo") == f::algo && d.indexOf ("level") == f::level && d.indexOf ("width") == f::width && d.indexOf ("tune") == f::tune
           && (int) d.params.size() <= grv::engine::instrumentSlots && d.family == "Synth\xC3\xA9s FM", "positions des r\xC3\xA9glages, famille FM");
    bool order = true;
    for (const auto& a : f::algorithms())
        for (int k = 0; k < 4; ++k) for (int j = 0; j <= k; ++j) order = order && ((a.mods[k] >> j) & 1) == 0;
    CHECK (order && f::ratiosAB().size() == 34 && std::abs (f::ratiosAB()[33] - 16.0) < 1e-9, "8 algorithmes (B2 → B1 → A → C), rapports");

    constexpr double sr = 48000.0;
    struct Out { float peak = 0.0f; bool finite = true; double rough = 0.0; int crossings = 0; bool activeAfter = true; double rmsL = 0.0, rmsR = 0.0; };
    auto run = [&] (std::initializer_list<std::pair<const char*, float>> set, int note = 60, float vel = 1.0f, int blocks = 300, bool releaseAfter = false)
    {
        Out o;
        grv::instruments::Fm4Synth inst;
        inst.prepare (sr, 64);
        inst.setTempo (120.0);
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); if (i >= 0) p[(size_t) i] = v; else std::printf ("  ECHEC r\xC3\xA9glage inconnu %s\n", id); }
        std::vector<float> L (64), R (64);
        inst.noteOn (note, vel);
        double prev = 0.0, sumAbs = 0.0, sumDiff = 0.0;
        for (int k = 0; k < blocks; ++k)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst.render (L.data(), R.data(), 64, p.data());
            for (int i = 0; i < 64; ++i)
            {
                const auto x = (double) L[(size_t) i];
                o.finite = o.finite && std::isfinite (x) && std::isfinite (R[(size_t) i]);
                o.peak = std::max (o.peak, std::max (std::abs ((float) x), std::abs (R[(size_t) i])));
                if (k > 20) { sumAbs += std::abs (x); sumDiff += std::abs (x - prev); if ((x >= 0.0) != (prev >= 0.0)) ++o.crossings; o.rmsL += x * x; o.rmsR += (double) R[(size_t) i] * R[(size_t) i]; }
                prev = x;
            }
        }
        o.rough = sumAbs > 0.0 ? sumDiff / sumAbs : 0.0;
        if (releaseAfter)
        {
            inst.noteOff (note);
            for (int k = 0; k < (int) (8.0 * sr / 64); ++k) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); inst.render (L.data(), R.data(), 64, p.data()); }
            o.activeAfter = inst.isActive();
        }
        return o;
    };
    bool allAlgos = true;
    for (int a = 1; a <= 8; ++a)
        for (float mixv : { -63.0f, 0.0f, 63.0f })
        {
            const auto o = run ({ { "algo", (float) a }, { "leva", 100 }, { "levb", 100 }, { "feedback", 60 }, { "mix", mixv }, { "adec", 127 }, { "bdec", 127 }, { "aend", 127 }, { "bend", 127 } }, 60, 1.0f, 120);
            if (! o.finite || o.peak < 0.005f || o.peak > 4.0f) { allAlgos = false; std::printf ("  algorithme %d m\xC3\xA9lange %.0f : cr\xC3\xAAte %.3f\n", a, mixv, o.peak); }
        }
    CHECK (allAlgos, "les 8 algorithmes jouent (X, X+Y, Y)");
    // sinus pur (aucune modulation) : passages par zéro ≈ 2 × fréquence
    const auto sine = run ({ { "leva", 0 }, { "levb", 0 }, { "mix", -63 } }, 69);
    const auto expected = 2.0 * 440.0 * (279 * 64) / sr;
    const auto rich = run ({ { "leva", 110 }, { "aend", 110 }, { "mix", -63 } }, 69);
    std::printf ("  sinus : %d passages (attendu ~%.0f), rugosit\xC3\xA9 %.3f ; A\xE2\x86\x92" "C : %.3f\n", sine.crossings, expected, sine.rough, rich.rough);
    CHECK (std::abs (sine.crossings - expected) < expected * 0.03 && rich.rough > sine.rough * 1.5, "fr\xC3\xA9quence juste, modulation A \xE2\x86\x92 C");
    const auto harmC = run ({ { "leva", 0 }, { "levb", 0 }, { "mix", -63 }, { "harm", -12 } }, 69);
    const auto nofb = run ({ { "algo", 2 }, { "leva", 0 }, { "levb", 40 }, { "bdec", 127 }, { "bend", 127 }, { "mix", 63 } }, 57);
    const auto fb = run ({ { "algo", 2 }, { "leva", 0 }, { "levb", 40 }, { "bdec", 127 }, { "bend", 127 }, { "mix", 63 }, { "feedback", 120 } }, 57);
    std::printf ("  harmoniques C : %.3f ; B2\xE2\x86\x92" "B1 sans / avec r\xC3\xA9troaction : %.3f / %.3f\n", harmC.rough, nofb.rough, fb.rough);
    CHECK (harmC.rough > sine.rough * 1.3 && fb.rough > nofb.rough * 1.15, "harmoniques sur C, r\xC3\xA9troaction");
    // filtre fermé : plus doux ; passe-bas 24 dB stable à résonance max
    const auto bright = run ({ { "leva", 110 }, { "aend", 110 }, { "mix", -63 } }, 48);
    const auto dark = run ({ { "leva", 110 }, { "aend", 110 }, { "mix", -63 }, { "ffreq", 300 } }, 48);
    const auto lad = run ({ { "leva", 110 }, { "ftype", 1 }, { "ffreq", 800 }, { "fres", 100 } }, 48);
    const auto bw = run ({ { "leva", 110 }, { "aend", 110 }, { "mix", -63 }, { "base", 2000 }, { "width", 1 } }, 48);
    std::printf ("  filtre : ouvert %.3f, ferm\xC3\xA9 %.3f ; 24 dB r\xC3\xA9s. max cr\xC3\xAAte %.3f ; base/largeur %.3f\n", bright.rough, dark.rough, lad.peak, bw.peak);
    CHECK (dark.rough < bright.rough * 0.6 && lad.finite && lad.peak < 4.0f && bw.finite && bw.peak < bright.peak, "filtres");
    // extinction, stéréo, vélocité
    const auto rel = run ({}, 60, 1.0f, 100, true);
    const auto st = run ({ { "spread", 100 } }, 60);
    const auto soft = run ({ { "velamp", 100 } }, 60, 0.2f), loud = run ({ { "velamp", 100 } }, 60, 1.0f);
    CHECK (! rel.activeAfter && std::abs (st.rmsL - st.rmsR) > 0.05 * (st.rmsL + st.rmsR) && soft.peak < loud.peak * 0.6, "extinction, place st\xC3\xA9r\xC3\xA9o, v\xC3\xA9locit\xC3\xA9");
    // enveloppe A déclenchée : la modulation s'éteint (son qui s'adoucit) ; à la touche : tenue
    const auto trig = run ({ { "leva", 120 }, { "adec", 30 }, { "aend", 0 }, { "mix", -63 } }, 60, 1.0f, 600);
    const auto held = run ({ { "leva", 120 }, { "adec", 30 }, { "aend", 0 }, { "amode", 1 }, { "mix", -63 } }, 60, 1.0f, 600);
    std::printf ("  enveloppe A : d\xC3\xA9" "clench\xC3\xA9" "e %.3f, \xC3\xA0 la touche %.3f\n", trig.rough, held.rough);
    CHECK (held.rough > trig.rough * 1.3, "enveloppe de modulation d\xC3\xA9" "clench\xC3\xA9" "e / \xC3\xA0 la touche");
}

static void testFm6()
{
    const std::string name = "Old Metal (FM 6 op\xC3\xA9rateurs)";
    std::printf ("%s\n", name.c_str());
    namespace f = grv::instruments::fm6;
    const auto& d = f::desc();
    CHECK (d.indexOf ("o1_mode") == f::opBase && d.indexOf ("o6_rcurve") == f::opParam (5, f::oRCurve) && d.indexOf ("pressam") == f::pressAm
           && (int) d.params.size() <= grv::engine::instrumentSlots && d.indexOf ("tune") == f::tune, "positions des r\xC3\xA9glages");
    // algorithmes : un modulateur a toujours un numéro plus grand que sa cible ; porteurs
    bool order = true;
    for (const auto& a : f::algorithms())
        for (int k = 0; k < 6; ++k) for (int j = 0; j <= k; ++j) order = order && ((a.mods[k] >> j) & 1) == 0;
    auto carriers = [] (int n) { int c = 0; for (int k = 0; k < 6; ++k) c += (f::algorithms()[(size_t) n - 1].carriers >> k) & 1; return c; };
    CHECK (order && carriers (1) == 2 && carriers (5) == 3 && carriers (16) == 1 && carriers (24) == 5 && carriers (32) == 6, "32 algorithmes");

    constexpr double sr = 48000.0;
    struct Out { float peak = 0.0f; bool finite = true; double rough = 0.0; int crossings = 0; bool activeAfter = true; };
    auto run = [&] (std::initializer_list<std::pair<const char*, float>> set, int note = 60, float vel = 1.0f, int blocks = 300, bool releaseAfter = false)
    {
        Out o;
        grv::instruments::Fm6Synth inst;
        inst.prepare (sr, 64);
        inst.setTempo (120.0);
        auto p = defaults (d);
        for (const auto& [id, v] : set) { const auto i = d.indexOf (id); if (i >= 0) p[(size_t) i] = v; else std::printf ("  ECHEC r\xC3\xA9glage inconnu %s\n", id); }
        std::vector<float> L (64), R (64);
        inst.noteOn (note, vel);
        double prev = 0.0, sumAbs = 0.0, sumDiff = 0.0;
        for (int k = 0; k < blocks; ++k)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst.render (L.data(), R.data(), 64, p.data());
            for (int i = 0; i < 64; ++i)
            {
                const auto x = (double) L[(size_t) i];
                o.finite = o.finite && std::isfinite (x);
                o.peak = std::max (o.peak, std::abs ((float) x));
                if (k > 20) { sumAbs += std::abs (x); sumDiff += std::abs (x - prev); if ((x >= 0.0) != (prev >= 0.0)) ++o.crossings; }
                prev = x;
            }
        }
        o.rough = sumAbs > 0.0 ? sumDiff / sumAbs : 0.0;
        if (releaseAfter)
        {
            inst.noteOff (note);
            for (int k = 0; k < (int) (8.0 * sr / 64); ++k) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); inst.render (L.data(), R.data(), 64, p.data()); }
            o.activeAfter = inst.isActive();
        }
        return o;
    };

    // chaque algorithme, tous les opérateurs à 80 : fini, présent, sans excès
    bool allAlgos = true;
    for (int a = 1; a <= 32; ++a)
    {
        const auto o = run ({ { "algo", (float) a }, { "o2_level", 80 }, { "o3_level", 80 }, { "o4_level", 80 }, { "o5_level", 80 }, { "o6_level", 80 }, { "feedback", 5 } }, 60, 1.0f, 120);
        if (! o.finite || o.peak < 0.005f || o.peak > 4.0f) { allAlgos = false; std::printf ("  algorithme %d : cr\xC3\xAAte %.3f\n", a, o.peak); }
    }
    CHECK (allAlgos, "les 32 algorithmes jouent");
    // rétroaction : un sinus devient riche (plus « rugueux ») ; algorithme 32, opérateur 6 seul
    const auto sine = run ({ { "algo", 32 }, { "o1_level", 0 }, { "o6_level", 99 } });
    const auto saw = run ({ { "algo", 32 }, { "o1_level", 0 }, { "o6_level", 99 }, { "feedback", 7 } });
    std::printf ("  r\xC3\xA9troaction : rugosit\xC3\xA9 %.4f \xE2\x86\x92 %.4f\n", sine.rough, saw.rough);
    CHECK (sine.finite && saw.finite && saw.rough > sine.rough * 1.5, "r\xC3\xA9troaction");
    // modulation : un modulateur enrichit le porteur (algorithme 1 : 2 → 1)
    const auto fmOn = run ({ { "o2_level", 90 }, { "o2_coarse", 3 } });
    CHECK (fmOn.rough > sine.rough * 1.5, "modulation 2 \xE2\x86\x92 1");
    // fréquence fixe : 100 Hz quelle que soit la note (≈ 200 passages par zéro par seconde)
    const auto fixedLow = run ({ { "o1_mode", 1 }, { "o1_coarse", 2 } }, 40), fixedHigh = run ({ { "o1_mode", 1 }, { "o1_coarse", 2 } }, 80);
    const auto secs = (300 - 21) * 64 / sr;
    std::printf ("  fr\xC3\xA9quence fixe : %.0f et %.0f passages / s\n", fixedLow.crossings / secs, fixedHigh.crossings / secs);
    CHECK (std::abs (fixedLow.crossings / secs - 200.0) < 10.0 && std::abs (fixedHigh.crossings / secs - 200.0) < 10.0, "fr\xC3\xA9quence fixe");
    // rapport 0,5 : une octave sous la note
    const auto half = run ({ { "o1_coarse", 0 } }, 69);
    CHECK (std::abs (half.crossings / secs - 440.0) < 15.0, "rapport 0,5");
    // vélocité : sensibilité 7, note douce bien plus faible
    const auto soft = run ({ { "o1_vel", 7 } }, 60, 0.2f), hard = run ({ { "o1_vel", 7 } }, 60, 1.0f);
    CHECK (soft.peak < hard.peak * 0.5f && soft.peak > 0.0f, "v\xC3\xA9locit\xC3\xA9");
    // relâchement : la voix s'éteint ; caractère « Années 80 » fini
    CHECK (! run ({}, 60, 1.0f, 100, true).activeAfter, "rel\xC3\xA2" "chement : voix \xC3\xA9teinte");
    const auto retro = run ({ { "character", 1 }, { "o2_level", 80 } });
    CHECK (retro.finite && retro.peak > 0.01f, "caract\xC3\xA8re Ann\xC3\xA9" "es 80");
    // enveloppe de hauteur et LFO
    CHECK (run ({ { "pl1", 80 }, { "pr1", 40 }, { "lfopmd", 99 }, { "pms", 7 }, { "lfoamd", 99 }, { "o1_ams", 3 } }).finite, "enveloppe de hauteur et LFO");
}

// v0.43.5 : modes de déclenchement des LFO de la modulation commune
static void testLfoModes()
{
    const std::string name = "LFO : une fois, demi-cycle, libre";
    std::printf ("%s\n", name.c_str());
    using grv::synth::HubLfo;
    const double sr = 48000.0;
    // une fois : un cycle (2 Hz : 0,5 s) puis arrêt — la valeur ne bouge plus
    HubLfo one;
    one.restart (0.0, 1); one.limitRun (1.0);
    double v = 0.0;
    for (int b = 0; b < 200; ++b) v = one.advance (HubLfo::rampUp, 2.0, 256, sr, 0.0, nullptr, 16);
    const auto after = one.advance (HubLfo::rampUp, 2.0, 256, sr, 0.0, nullptr, 16);
    CHECK (std::abs (after - v) < 1.0e-9 && std::abs (v + 1.0) < 1.0e-6, "une fois : arr\xC3\xAAt au bout d'un cycle");
    // demi-cycle : arrêt au milieu (scie montante : 0)
    HubLfo half;
    half.restart (0.0, 1); half.limitRun (0.5);
    for (int b = 0; b < 200; ++b) v = half.advance (HubLfo::rampUp, 2.0, 256, sr, 0.0, nullptr, 16);
    CHECK (std::abs (v) < 1.0e-6, "demi-cycle : arr\xC3\xAAt au milieu");
    // libre : la phase continue (pas recalée), le LFO non démarré l'indique
    HubLfo fr;
    CHECK (! fr.hasStarted(), "libre : pas encore d\xC3\xA9marr\xC3\xA9");
    fr.restart (0.25, 1);
    CHECK (fr.hasStarted(), "libre : d\xC3\xA9marr\xC3\xA9 au 1er d\xC3\xA9" "clenchement");
    // le mode « Mode » a 6 choix ; les anciens projets sont convertis (format 19)
    const auto& d = grv::instruments::octa::desc();
    const auto i = d.indexOf ("t1_lf1_mode");
    CHECK (i >= 0 && d.params[(size_t) i].choices.size() == 6 && d.params[(size_t) i].choices[3] == "Une fois", "6 modes");
}

int main (int argc, char** argv)
{
    const char* outDir = argc > 1 ? argv[1] : nullptr;
    std::printf ("Instruments (%zu)\n", instrumentCatalog().size());
    for (const auto* d : instrumentCatalog()) testInstrument (*d, outDir);
    std::printf ("Effets (%zu)\n", effectCatalog().size());
    for (const auto* d : effectCatalog()) testEffect (*d, outDir);
    testLfoSyncInstruments();
    testMachines();
    testPolyExpression();
    testHybridMatrix();
    testMorphModes();
    testOcta();
    testPedalEffects();
    testSpSampler();
    testSlicerSlices();
    testKeysOneShot();
    testSpSlices();
    testSlotIds();
    testPadPolys();
    testDrums21();
    testSound22();
    testSidechain23();
    testVocoders25();
    testVoice33();
    testStudio34();
    testSpace35();
    testFx36();
    testDist36();
    testFx3738();
    testHop39();
    testSpeech39();
    testOcta42();
    testLfoModes();
    testFm6();
    testFm4();
    testMuse();
    testFmx();
    testModeSwitch();
    testDrones();
    testOcta41();
    std::printf (failures == 0 ? "OK\n" : "%d \xC3\xA9" "chec(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
