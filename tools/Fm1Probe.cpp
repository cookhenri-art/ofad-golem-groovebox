// Groovebox — OFAD — AGPL-3.0-or-later
// v0.49 : essai d'un firmware FM-1 dans le FM Player, au rythme réel : démarrage, vitesse de l'émulation,
// canaux MIDI qui font sonner une note (pour choisir le canal par défaut de chaque firmware).
//   groovebox_fm1probe fichier.fwsc [secondes de démarrage]
#include "instruments/FmPlayer.h"
#include "engine/Catalog.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

using namespace grv::engine;
namespace fmp = grv::instruments::fmplayer;

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage : groovebox_fm1probe fichier.fwsc [secondes]\n"); return 2; }
    std::vector<std::uint8_t> pkg;
    if (FILE* fp = std::fopen (argv[1], "rb")) { std::uint8_t b[65536]; size_t n; while ((n = std::fread (b, 1, sizeof b, fp)) > 0) pkg.insert (pkg.end(), b, b + n); std::fclose (fp); }
    SampleBuffer fw; fw.firmware = true;
    const auto err = fmp::start (pkg, {}, fw.runtime, argv[1]);
    if (! err.empty()) { std::printf ("REFUS %s\n", err.c_str()); return 1; }
    auto inst = createInstrument ("fmplayer");
    const auto* d = findInstrument ("fmplayer");
    const int sr = 48000, block = 256;
    inst->prepare (sr, block);
    inst->setSample (0, &fw);
    std::vector<float> p (d->params.size());
    for (size_t i = 0; i < p.size(); ++i) p[i] = d->params[i].def;
    p[(size_t) d->indexOf ("clock")] = 0.0f;
    if (const char* l = std::getenv ("FM1_LEAD_MS")) p[(size_t) d->indexOf ("lead")] = (float) std::atof (l);
    std::vector<float> L (block), R (block);
    const auto t0 = std::chrono::steady_clock::now();
    long long blocks = 0;
    auto run = [&] (double sec, int note, int chan, bool on, double* rms)
    {
        double acc = 0; long cnt = 0;
        p[(size_t) d->indexOf ("channel")] = (float) (chan + 1);
        const auto nb = (long long) (sec * sr / block);
        for (long long b = 0; b < nb; ++b)
        {
            if (b == 0 && on) inst->noteOn (note, 0.8f);
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), block, p.data());
            for (int i = 0; i < block; ++i) { acc += L[(size_t) i] * L[(size_t) i] + R[(size_t) i] * R[(size_t) i]; ++cnt; }
            ++blocks;
            std::this_thread::sleep_until (t0 + std::chrono::microseconds (blocks * block * 1000000 / sr));
        }
        if (on) inst->noteOff (note);
        if (rms) *rms = std::sqrt (acc / std::max (1L, cnt));
    };
    run (argc > 2 ? std::atof (argv[2]) : 5.0, 0, 0, false, nullptr); // démarrage
    double base = 0; run (1.0, 0, 0, false, &base);                    // bruit de fond (séquenceur qui joue tout seul)
    // GOLEM 1.0 : notes en rafale qui se chevauchent (jeu lié, accords rapides), puis plus rien :
    // le son doit retomber au niveau du fond (FM1_STUCK=pas en blocs entre deux notes, durée tenue en blocs)
    if (const char* st = std::getenv ("FM1_STUCK"))
    {
        int step = 2, hold = 6, count = 32;
        std::sscanf (st, "%d,%d,%d", &step, &hold, &count);
        p[(size_t) d->indexOf ("channel")] = 1.0f;
        const int total = count * step + hold + 4;
        int held[128] {};
        for (int b = 0; b < total; ++b)
        {
            for (int k = 0; k < count; ++k)
            {
                const int nt = 48 + (k * 7) % 25;
                if (b == k * step + hold) { inst->noteOff (nt); }
                if (b == k * step) { inst->noteOn (nt, 0.8f); ++held[nt]; }
            }
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            inst->render (L.data(), R.data(), block, p.data());
            ++blocks;
            std::this_thread::sleep_until (t0 + std::chrono::microseconds (blocks * block * 1000000 / sr));
        }
        double r1 = 0, r2 = 0, r3 = 0;
        run (1.0, 0, 0, false, &r1); run (1.0, 0, 0, false, &r2); run (1.0, 0, 0, false, &r3);
        std::printf ("rafale pas %d tenue %d x%d : après 1 s %.4f, 2 s %.4f, 3 s %.4f (fond %.4f)%s\n", step, hold, count, r1, r2, r3, base,
                     r3 > base * 3.0 + 0.003 ? "  ← NOTES BLOQUÉES" : "  ok");
        inst->setSample (0, nullptr);
        return 0;
    }
    std::printf ("fond %.4f ; canaux :", base);
    for (int c = 0; c < 16; ++c)
    {
        double r = 0; run (0.5, 60, c, true, &r);
        run (0.35, 0, c, false, nullptr);
        if (r > base * 2.0 + 0.002) std::printf (" %d(%.3f)", c + 1, r);
    }
    if (const char* shot = std::getenv ("FM1_SHOT")) // écran en fin d'essai (PPM)
        if (auto* fpl = dynamic_cast<grv::instruments::FmPlayer*> (inst.get()); fpl != nullptr && fpl->machine() != nullptr)
        {
            std::vector<std::uint16_t> lcd (240 * 240);
            fm1_view (fpl->machine(), lcd.data(), nullptr);
            if (FILE* o = std::fopen (shot, "wb"))
            {
                std::fprintf (o, "P6 240 240 255\n");
                for (auto v : lcd) { const unsigned char px[3] { (unsigned char) (((v >> 11) & 31) * 255 / 31), (unsigned char) (((v >> 5) & 63) * 255 / 63), (unsigned char) ((v & 31) * 255 / 31) }; std::fwrite (px, 1, 3, o); }
                std::fclose (o);
            }
        }
    // GOLEM 1.0 : bouton du boîtier piloté par son réglage (FM1_KNOBTEST=knob1|select|…) : l'écran doit
    // changer plus que sans geste
    if (const char* k = std::getenv ("FM1_KNOBTEST"))
        if (auto* fpl = dynamic_cast<grv::instruments::FmPlayer*> (inst.get()); fpl != nullptr && fpl->machine() != nullptr)
        {
            std::vector<std::uint16_t> a (240 * 240), b (240 * 240), c (240 * 240);
            auto diff = [] (const std::vector<std::uint16_t>& x, const std::vector<std::uint16_t>& y) { long n = 0; for (size_t i = 0; i < x.size(); ++i) n += x[i] != y[i]; return n; };
            fm1_view (fpl->machine(), a.data(), nullptr); run (0.8, 0, 0, false, nullptr);
            fm1_view (fpl->machine(), b.data(), nullptr);
            p[(size_t) d->indexOf (k)] += 6.0f; run (0.8, 0, 0, false, nullptr);
            fm1_view (fpl->machine(), c.data(), nullptr);
            std::printf ("\n%s : sans geste %ld pixels changés, +6 crans %ld\n", k, diff (a, b), diff (b, c));
        }
    if (std::getenv ("FM1_KEYTEST")) // touche du boîtier (Do du milieu) au lieu du MIDI
    {
        double r = 0;
        inst->touch (fmp::touchKey, 26.0f, 1.0f); run (0.6, 0, 0, false, &r); inst->touch (fmp::touchKey, 26.0f, 0.0f);
        std::printf ("touche du boîtier : %.4f\n", r);
    }
    float st[8] {};
    inst->status (st, 8);
    std::printf ("\ndémarré %d arrêté %d vitesse %.2f trous %.0f\n", (int) st[1], (int) st[2], st[3], st[4]);
    inst->setSample (0, nullptr);
    return 0;
}
