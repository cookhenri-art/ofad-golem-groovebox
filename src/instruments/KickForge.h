/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Kick Forge » (1.2) — principe des constructeurs de kicks par couches (architecture reprise ;
    ni nom, ni dessin, ni sons d'origine) :
      - jusqu'à 8 couches empilées, chacune avec son morceau (bibliothèque de morceaux CALCULÉS :
        punchs, crunchs, rumbles, clics, percs, tops, renforts, corps, reverse bass — ou le sample
        glissé sur la couche, accordé sur la tonalité par détection), son volume, sa hauteur, son
        étirement, son décalage, sa longueur, ses fondus à courbe, sa tranche (filtres, égaliseur,
        transitoires, distorsion), muet / solo / polarité / inversion / verrou ;
      - un rack de 9 modules dans l'ordre choisi (égaliseur, égaliseur M/S, distorsion, façonneur
        de kick, transitoires, convolution, finaliseur, multibande, filtres) ;
      - styles (règles de tirage au hasard, tenues par l'interface), tonalité, gain automatique ;
      - le kick suit la tonalité réglée ou la note jouée.
    Le kick est CALCULÉ par un fil de calcul à chaque changement de réglage (qualité maximale, sans
    limite de charge) puis rejoué tel quel : ce qu'on entend est exactement ce qu'on voit. Le fil
    audio n'alloue rien, ne bloque jamais.
    1.2 (régularité sous LFO) : les kicks calculés sont GARDÉS (24 sur ordinateur, 10 sur téléphone),
    rangés avec leurs réglages. À chaque coup, le fil audio joue le kick gardé des réglages de cet
    instant (au 1/128 de la course près pour les réglages continus, exactement pour les crans) ; s'il
    n'existe pas encore, il joue le plus proche et le demande en priorité au fil de calcul. Un LFO
    repassant par les mêmes valeurs donne donc toujours le même kick. Volume, vélocité et hauteur
    jouée sont appliqués en direct (ils ne relancent aucun calcul).
*/

#pragma once

#include "kf/KfStyles.h"

#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <mutex>
#include <thread>
#if defined (__APPLE__)
 #include <TargetConditionals.h>
#endif

namespace grv::instruments
{

using namespace grv::engine;

namespace kf
{
    inline const ModuleDesc& desc();

    inline const std::vector<std::string>& styleNames()
    {
        static const std::vector<std::string> s { "Construction libre", "Rumble", "Reverse bass", "Industriel", "Psy", "Schranz", "Rumble propre",
                                                  "Newstyle", "Rawstyle", "Hard techno", "Gabber", "Frenchcore", "Hardstyle", "Techno" };
        return s;
    }
    inline std::vector<std::string> keyNames()
    {
        static const char* fr[12] { "Do", "Do#", "R\xC3\xA9", "R\xC3\xA9#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
        std::vector<std::string> k;
        for (int m = 24; m < 48; ++m) k.push_back (std::string (fr[m % 12]) + " " + std::to_string (m / 12 - 1));
        return k;
    }
}

//==============================================================================
class KickForge final : public Instrument
{
public:
    KickForge() = default;
    ~KickForge() override { stopWorker(); }

    const ModuleDesc& desc() const noexcept override { return kf::desc(); }

    void prepare (double s, int) override
    {
        stopWorker();
        sr = s > 0.0 ? s : 48000.0;
        const auto cap = (size_t) (kf::maxSeconds * sr * 1.05) + 64;
        const auto ns = (size_t) kf::numSlots();
        for (int k = 0; k < numCache; ++k)
        {
            auto& b = bufs[(size_t) k];
            // le dernier emplacement sert au tout premier coup (calcul dans le fil audio) : réservé d'avance
            if (k == numCache - 1) { b.L.assign (cap, 0.0f); b.R.assign (cap, 0.0f); }
            else { b.L.clear(); b.R.clear(); }
            b.length.store (0); b.gen.store (0); b.played.store (0); b.written = 0;
            b.q.assign (ns, 0); b.bpm = 0.0; b.samples = {};
        }
        published.store (-1);
        usedMask.store (0);
        for (auto& v : voices) v = {};
        req.p.assign (ns, 0.0f); preq.p.assign (ns, 0.0f);
        qNow.assign (ns, 0); qWork.assign (ns, 0);
        stepped.assign (ns, 0);
        realtime.assign (ns, 0);
        const auto& d = kf::desc();
        for (size_t i = 0; i < ns && i < d.params.size(); ++i) stepped[i] = d.params[i].stepped() ? 1 : 0;
        for (const int i : { (int) kf::gVolume, (int) kf::gVelSens, (int) kf::gPitchMode }) realtime[(size_t) i] = 1;
        lastHash = 0; lastBpm = -1.0; hitClock = 0; writeClock = 0;
        rcache = std::make_unique<kf::RenderCache>();
        startWorker();
    }

    void reset() noexcept override { for (auto& v : voices) v.active = false; }

    void setTempo (double b) noexcept override { bpm = std::clamp (b, 20.0, 400.0); }
    void setSample (int index, const SampleBuffer* s) noexcept override
    {
        if (index >= 0 && index < kf::numLayers) samples[(size_t) index] = s;
    }

    void noteOn (int note, float velocity) noexcept override { pendingNote = note; pendingVel = velocity; pending = true; }
    void noteOff (int) noexcept override {}
    void allNotesOff() noexcept override { for (auto& v : voices) if (v.active) v.fade = std::min (v.fade, 1.0f), v.fadeStep = 1.0f / (float) std::max (1.0, 0.004 * sr); }

    bool isActive() const noexcept override { return pending || voices[0].active || voices[1].active; }

    void render (float* left, float* right, int n, const float* p) noexcept override
    {
        requestIfChanged (p);
        if (pending)
        {
            pending = false;
            startVoice (p);
        }
        const auto gainVol = (float) dsp::dbToGain (std::clamp (p[kf::gVolume], -60.0f, 12.0f));
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const auto& b = bufs[(size_t) v.buf];
            if (b.L.empty()) { v.active = false; continue; }
            const auto len = v.len;
            for (int i = 0; i < n; ++i)
            {
                const auto pos = v.pos;
                const auto i0 = (int) pos;
                if (i0 + 1 >= len) { v.active = false; break; }
                const auto f = (float) (pos - i0);
                auto l = b.L[(size_t) i0] + (b.L[(size_t) i0 + 1] - b.L[(size_t) i0]) * f;
                auto r = b.R[(size_t) i0] + (b.R[(size_t) i0 + 1] - b.R[(size_t) i0]) * f;
                auto g = v.gain * gainVol;
                if (v.fadeStep > 0.0f) { v.fade -= v.fadeStep; if (v.fade <= 0.0f) { v.active = false; break; } g *= v.fade; }
                left[i] += l * g; right[i] += r * g;
                v.pos += v.rate;
            }
            if (! v.active) usedMask.fetch_and (~(1u << v.buf));
        }
        // tampons encore lus par une voix (le fil de calcul ne les réécrit pas)
        unsigned m = 0;
        for (auto& v : voices) if (v.active) m |= 1u << v.buf;
        usedMask.store (m);
    }

    /** Fil de l'interface : copie des vues du dernier calcul (afficheur). */
    std::string viewJson (unsigned knownVersion = 0xffffffffu) const;
    /** Fil de l'interface : numéro du dernier calcul publié. */
    unsigned viewVersion() const noexcept { return viewVer.load(); }
    /** Tests : attend que le kick des réglages en cours soit calculé (au plus ms millisecondes). */
    bool waitReady (int ms) const
    {
        for (int t = 0; t < ms; t += 2)
        {
            if (doneSeq.load() == reqSeq.load() && published.load() >= 0) return true;
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
        }
        return false;
    }

    /** Tests : attend que le fil de calcul n'ait plus rien à faire (au plus ms millisecondes). */
    bool waitIdle (int ms) const
    {
        for (int t = 0; t < ms; t += 2)
        {
            if (doneSeq.load() == reqSeq.load() && pdoneSeq.load() == preqSeq.load()) return true;
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
        }
        return false;
    }

private:
    /** Kick gardé. Écrit par le fil de calcul quand gen est impair ; lu par le fil audio quand gen
        est pair et n'a pas bougé pendant la lecture ; un kick joué est épinglé (usedMask). */
    struct Buffer
    {
        std::vector<float> L, R;
        std::atomic<int> length { 0 };
        std::atomic<unsigned> gen { 0 };
        std::atomic<unsigned> played { 0 };          // dernier coup qui l'a joué (0 : jamais)
        unsigned written = 0;                         // ordre de calcul (fil de calcul seulement)
        std::vector<std::int16_t> q;                  // réglages quantifiés
        double bpm = 0.0;
        std::array<const SampleBuffer*, kf::numLayers> samples {};
    };
    struct Request { std::vector<float> p; double bpm = 120.0; std::array<const SampleBuffer*, kf::numLayers> samples {}; };
#if defined (__ANDROID__) || (defined (TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || defined (GRV_LOW_MEMORY) // GRV_LOW_MEMORY : greffon MPC
    static constexpr int numCache = 10;
#else
    static constexpr int numCache = 24;
#endif
    static constexpr int tolerance = 16; // 1/128 de la course (quantification au 1/2048)

    /** Réglages quantifiés : crans tels quels, continus au 1/2048 de leur course (forme normalisée) ;
        volume, vélocité et hauteur jouée à 0 (appliqués en direct). Pur : appelé par les deux fils. */
    void quantize (const float* p, std::int16_t* out) const noexcept
    {
        const auto& d = kf::desc();
        const auto ns = kf::numSlots();
        for (int i = 0; i < ns; ++i)
        {
            if (realtime[(size_t) i] != 0) { out[i] = 0; continue; }
            if (stepped[(size_t) i] != 0) { out[i] = (std::int16_t) std::clamp ((int) std::lround (p[i]), -32768, 32767); continue; }
            out[i] = (std::int16_t) std::lround (d.params[(size_t) i].toNorm (p[i]) * 2048.0f);
        }
    }
    /** Écart entre deux jeux de réglages quantifiés : -1 si un cran diffère, sinon le plus grand écart. */
    int distance (const std::int16_t* a, const std::int16_t* b) const noexcept
    {
        const auto ns = kf::numSlots();
        int dmax = 0;
        for (int i = 0; i < ns; ++i)
        {
            const auto dd = std::abs ((int) a[i] - (int) b[i]);
            if (dd == 0) continue;
            if (stepped[(size_t) i] != 0) return -1;
            dmax = std::max (dmax, dd);
        }
        return dmax;
    }

    /** Fil audio : épingle l'emplacement k (lu à la génération g) ; échoue si le fil de calcul l'a pris. */
    bool pin (int k, unsigned g) noexcept
    {
        usedMask.fetch_or (1u << k);
        if (bufs[(size_t) k].gen.load() != g) { usedMask.fetch_and (~(1u << k)); return false; }
        return true;
    }

    /** Fil audio : le kick gardé le plus proche des réglages q (même tempo, mêmes samples), épinglé. */
    int findCached (const std::int16_t* q, int& dist) noexcept
    {
        // dans la tolérance, le kick déjà joué le plus récemment l'emporte (un même endroit du LFO garde
        // son kick, même si un kick encore plus proche a été calculé depuis) ; sinon le plus proche
        int best = -1, bestD = INT_MAX;
        unsigned bestGen = 0, bestPlayed = 0;
        for (int k = 0; k < numCache; ++k)
        {
            auto& b = bufs[(size_t) k];
            const auto g = b.gen.load (std::memory_order_acquire);
            if ((g & 1u) != 0 || b.length.load (std::memory_order_relaxed) <= 0) continue;
            if (b.bpm != bpm || b.samples != samples) continue;
            const auto dd = distance (q, b.q.data());
            const auto pl = b.played.load (std::memory_order_relaxed);
            std::atomic_thread_fence (std::memory_order_acquire);
            if (b.gen.load (std::memory_order_relaxed) != g || dd < 0) continue; // réécrit pendant la lecture, ou autre cran
            const bool in = dd <= tolerance, bestIn = bestD <= tolerance;
            const bool better = best < 0 ? true
                              : in && ! bestIn ? true
                              : ! in && bestIn ? false
                              : in ? (pl != bestPlayed ? pl > bestPlayed : dd < bestD)
                              : dd < bestD;
            if (better) { best = k; bestD = dd; bestGen = g; bestPlayed = pl; }
        }
        if (best < 0 || ! pin (best, bestGen)) return -1;
        dist = bestD;
        return best;
    }
    struct Voice { bool active = false; int buf = 0, len = 0; double pos = 0.0, rate = 1.0; float gain = 1.0f, fade = 1.0f, fadeStep = 0.0f; };

    void startVoice (const float* p) noexcept
    {
        quantize (p, qNow.data());
        ++hitClock;
        int dist = INT_MAX;
        auto pub = findCached (qNow.data(), dist);
        if (pub < 0 || dist > tolerance)
        {
            // pas encore de kick pour ces réglages : demandé en priorité au fil de calcul
            postPriority (p);
            if (pub < 0)
            {
                // rien de proche : le dernier publié (épinglé), sinon le tout premier calcul
                const auto last = published.load (std::memory_order_acquire);
                if (last >= 0)
                {
                    const auto g = bufs[(size_t) last].gen.load();
                    if ((g & 1u) == 0 && pin (last, g)) pub = last;
                }
            }
        }
        if (pub < 0 && published.load (std::memory_order_acquire) < 0)
        {
            // rien de calculé encore (premier coup juste après l'ouverture, rendu hors temps réel) :
            // calcul immédiat, une seule fois dans la vie de l'instrument, dans le dernier emplacement
            // (réservé d'avance ; le fil de calcul, qui n'a rien publié, prend d'abord les premiers)
            kf::Input in;
            in.p.assign (p, p + kf::numSlots());
            in.bpm = bpm; in.samples = samples; in.sr = sr;
            const auto res = kf::render (in);
            const int k = numCache - 1;
            auto& b = bufs[(size_t) k];
            const auto n = std::min ((int) res.L.size(), (int) b.L.size() - 2);
            if (n <= 0) return;
            b.gen.fetch_add (1);
            std::copy (res.L.begin(), res.L.begin() + n, b.L.begin());
            std::copy (res.R.begin(), res.R.begin() + n, b.R.begin());
            b.L[(size_t) n] = b.R[(size_t) n] = 0.0f;
            std::copy (qNow.begin(), qNow.end(), b.q.begin());
            b.bpm = bpm; b.samples = samples;
            b.length.store (n);
            b.gen.fetch_add (1);
            int expected = -1;
            published.compare_exchange_strong (expected, k);
            if (! pin (k, b.gen.load())) return;
            pub = k;
        }
        if (pub < 0) return;
        bufs[(size_t) pub].played.store (hitClock, std::memory_order_relaxed);
        // l'ancien coup s'efface en 3 ms pendant que le nouveau part
        auto& old = voices[(size_t) cur];
        if (old.active && old.fadeStep == 0.0f) old.fadeStep = 1.0f / (float) std::max (1.0, 0.003 * sr);
        cur ^= 1;
        auto& v = voices[(size_t) cur];
        v.active = true; v.buf = pub; v.len = bufs[(size_t) pub].length.load(); v.pos = 0.0; v.fade = 1.0f; v.fadeStep = 0.0f;
        // hauteur : fixe (tonalité réglée) ou suit la note jouée (classe de note, à moins d'une demi-octave)
        v.rate = 1.0;
        if (p[kf::gPitchMode] > 0.5f)
        {
            const auto key = 24 + std::clamp ((int) std::lround (p[kf::gKey]), 0, 23);
            auto d = ((pendingNote - key) % 12 + 12) % 12;
            if (d > 6) d -= 12;
            v.rate = std::pow (2.0, d / 12.0);
        }
        const auto sens = std::clamp (p[kf::gVelSens] / 100.0f, 0.0f, 1.0f);
        v.gain = 1.0f - sens * (1.0f - std::clamp (pendingVel, 0.0f, 1.0f));
    }

    std::uint64_t hashOf (const float* p, int n, double bpm, const std::array<const SampleBuffer*, kf::numLayers>& s) const noexcept
    {
        std::uint64_t h = 1469598103934665603ull;
        auto mix = [&] (std::uint64_t v) { h ^= v; h *= 1099511628211ull; };
        for (int i = 0; i < n; ++i) { if (realtime[(size_t) i] != 0) continue; std::uint32_t u; std::memcpy (&u, p + i, 4); mix (u); }
        std::uint64_t b; std::memcpy (&b, &bpm, 8); mix (b);
        for (auto* x : s) mix ((std::uint64_t) (std::uintptr_t) x);
        return h;
    }

    /** Fil audio : si les réglages, le tempo ou les samples ont changé, dépose une demande (sans verrou). */
    void requestIfChanged (const float* p) noexcept
    {
        const auto n = kf::numSlots();
        const auto h = hashOf (p, n, bpm, samples);
        if (h == lastHash) return;
        lastHash = h;
        const auto s = reqSeq.load (std::memory_order_relaxed) + 1; // impair : écriture en cours
        reqSeq.store (s, std::memory_order_release);
        std::atomic_thread_fence (std::memory_order_release);
        std::memcpy (req.p.data(), p, sizeof (float) * (size_t) n);
        req.bpm = bpm; req.samples = samples;
        std::atomic_thread_fence (std::memory_order_release);
        reqSeq.store (s + 1, std::memory_order_release);    // pair : demande complète
    }

    /** Fil audio : demande prioritaire (le kick des réglages d'un coup joué), sans verrou. */
    void postPriority (const float* p) noexcept
    {
        const auto n = kf::numSlots();
        const auto s = preqSeq.load (std::memory_order_relaxed) + 1;
        preqSeq.store (s, std::memory_order_release);
        std::atomic_thread_fence (std::memory_order_release);
        std::memcpy (preq.p.data(), p, sizeof (float) * (size_t) n);
        preq.bpm = bpm; preq.samples = samples;
        std::atomic_thread_fence (std::memory_order_release);
        preqSeq.store (s + 1, std::memory_order_release);
    }

    void startWorker()
    {
        quit.store (false);
        worker = std::thread ([this] { workLoop(); });
    }
    void stopWorker()
    {
        quit.store (true);
        if (worker.joinable()) worker.join();
    }

    /** Fil de calcul : copie une demande (seqlock) ; faux si elle a été réécrite pendant la copie. */
    static bool readRequest (const Request& r, const std::atomic<unsigned>& seq, unsigned s1, kf::Input& in) noexcept
    {
        std::memcpy (in.p.data(), r.p.data(), sizeof (float) * in.p.size());
        in.bpm = r.bpm; in.samples = r.samples;
        std::atomic_thread_fence (std::memory_order_acquire);
        return seq.load (std::memory_order_acquire) == s1;
    }

    /** Fil de calcul : emplacement à réécrire (jamais le publié ni un kick en lecture) : un vide, sinon
        le plus ancien jamais joué, sinon le joué le plus anciennement. Rendu impair (pris), ou -1. */
    int claimSlot() noexcept
    {
        const auto pub = published.load();
        std::array<int, numCache> order {};
        for (int k = 0; k < numCache; ++k) order[(size_t) k] = k;
        auto score = [&] (int k) -> std::uint64_t
        {
            const auto& b = bufs[(size_t) k];
            if (b.length.load() <= 0) return 0;
            const auto pl = b.played.load (std::memory_order_relaxed);
            return pl == 0 ? (1ull << 32) + b.written : (2ull << 32) + pl;
        };
        std::sort (order.begin(), order.end(), [&] (int a, int b) { return score (a) < score (b); });
        for (const auto k : order)
        {
            if (k == pub) continue;
            if (published.load() < 0 && k == numCache - 1) continue; // réservé au tout premier coup
            auto& b = bufs[(size_t) k];
            b.gen.fetch_add (1);                                   // impair : pris
            if ((usedMask.load() >> k) & 1u) { b.gen.fetch_add (1); continue; } // en lecture : on le laisse
            return k;
        }
        return -1;
    }

    void workLoop()
    {
        kf::Input in;
        in.p.resize ((size_t) kf::numSlots());
        in.cache = rcache.get();
        while (! quit.load())
        {
            // 1. demande prioritaire (un coup joué sans son kick), 2. demande ordinaire (réglages changés)
            bool prio = false;
            unsigned s1 = 0;
            const auto ps = preqSeq.load (std::memory_order_acquire);
            if ((ps & 1u) == 0 && ps != pdoneSeq.load())
            {
                if (! readRequest (preq, preqSeq, ps, in)) continue;
                prio = true; s1 = ps;
            }
            else
            {
                s1 = reqSeq.load (std::memory_order_acquire);
                if ((s1 & 1u) != 0 || s1 == doneSeq.load())
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (2));
                    continue;
                }
                if (! readRequest (req, reqSeq, s1, in)) continue;
            }
            in.sr = sr;
            quantize (in.p.data(), qWork.data());
            if (prio)
            {
                // déjà gardé exactement (calculé entre-temps par une demande ordinaire) : rien à refaire
                bool have = false;
                for (auto& b : bufs)
                    if ((b.gen.load() & 1u) == 0 && b.length.load() > 0 && b.bpm == in.bpm && b.samples == in.samples && distance (qWork.data(), b.q.data()) == 0) { have = true; break; }
                if (have) { pdoneSeq.store (s1); continue; }
            }
            auto res = kf::render (in);
            const auto slot = claimSlot();
            if (slot < 0) { std::this_thread::sleep_for (std::chrono::milliseconds (2)); continue; }
            auto& b = bufs[(size_t) slot];
            const auto n = std::min ((int) res.L.size(), (int) (kf::maxSeconds * sr * 1.05));
            b.L.assign (res.L.begin(), res.L.begin() + n); b.L.push_back (0.0f); b.L.push_back (0.0f);
            b.R.assign (res.R.begin(), res.R.begin() + n); b.R.push_back (0.0f); b.R.push_back (0.0f);
            std::copy (qWork.begin(), qWork.end(), b.q.begin());
            b.bpm = in.bpm; b.samples = in.samples;
            b.written = ++writeClock;
            b.played.store (0, std::memory_order_relaxed);
            b.length.store (n);
            b.gen.fetch_add (1);                                       // pair : prêt
            published.store (slot, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lock (viewMutex);
                view = std::move (res);
                view.L.clear(); view.R.clear();
            }
            viewVer.fetch_add (1);
            if (prio) pdoneSeq.store (s1); else doneSeq.store (s1);
        }
    }

    double sr = 48000.0, bpm = 120.0, lastBpm = -1.0;
    std::array<Buffer, numCache> bufs;
    std::atomic<int> published { -1 };
    std::atomic<unsigned> usedMask { 0 };
    std::array<Voice, 2> voices {};
    int cur = 0;
    bool pending = false;
    int pendingNote = 60;
    float pendingVel = 1.0f;
    std::array<const SampleBuffer*, kf::numLayers> samples {};

    Request req, preq;
    std::atomic<unsigned> reqSeq { 0 }, doneSeq { 0 }, preqSeq { 0 }, pdoneSeq { 0 };
    std::uint64_t lastHash = 0;
    std::vector<std::int16_t> qNow, qWork;               // réglages quantifiés (fil audio / fil de calcul)
    std::vector<std::uint8_t> stepped, realtime;         // réglage à crans ; joué en direct
    unsigned hitClock = 0, writeClock = 0;
    std::unique_ptr<kf::RenderCache> rcache;

    std::thread worker;
    std::atomic<bool> quit { false };
    mutable std::mutex viewMutex;
    kf::Result view;
    std::atomic<unsigned> viewVer { 0 };
};

//==============================================================================
inline std::string KickForge::viewJson (unsigned knownVersion) const
{
    if (knownVersion == viewVer.load()) return "{\"v\":" + std::to_string (knownVersion) + "}"; // rien de neuf
    // formes d'onde réduites à 1200 points (la plus forte valeur de chaque tranche, signe gardé),
    // codées en entiers 16 bits petit-boutistes, en base64
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto b64 = [] (const std::vector<std::int16_t>& v)
    {
        std::string s;
        const auto* bytes = reinterpret_cast<const unsigned char*> (v.data());
        const auto n = v.size() * 2;
        s.reserve ((n + 2) / 3 * 4);
        for (size_t i = 0; i < n; i += 3)
        {
            const unsigned a = bytes[i], b = i + 1 < n ? bytes[i + 1] : 0u, c = i + 2 < n ? bytes[i + 2] : 0u;
            s += tbl[a >> 2]; s += tbl[((a & 3) << 4) | (b >> 4)];
            s += i + 1 < n ? tbl[((b & 15) << 2) | (c >> 6)] : '=';
            s += i + 2 < n ? tbl[c & 63] : '=';
        }
        return s;
    };
    std::lock_guard<std::mutex> lock (viewMutex);
    const auto& r = view;
    constexpr int cols = 1200;
    const auto total = std::max ({ r.length, (int) r.sum.size(), 1 });
    auto reduce = [&] (const std::vector<float>& w, int start, double scale)
    {
        std::vector<std::int16_t> out ((size_t) cols, 0);
        for (int c = 0; c < cols; ++c)
        {
            const auto a = (int) ((double) c / cols * total) - start, b = (int) ((double) (c + 1) / cols * total) - start;
            float best = 0.0f;
            for (int i = std::max (0, a); i < std::min ((int) w.size(), std::max (a + 1, b)); ++i) if (std::abs (w[(size_t) i]) > std::abs (best)) best = w[(size_t) i];
            out[(size_t) c] = (std::int16_t) std::lround (std::clamp (best * scale, -1.0, 1.0) * 32767.0);
        }
        return out;
    };
    auto esc = [] (const std::string& s) { std::string o; for (auto ch : s) { if (ch == '"' || ch == '\\') o += '\\'; if ((unsigned char) ch >= 0x20) o += ch; } return o; };
    std::string j = "{\"v\":" + std::to_string (viewVer.load()) + ",\"sr\":" + std::to_string ((int) sr) + ",\"len\":" + std::to_string (total)
                  + ",\"bpm\":" + std::to_string (r.bpm) + ",\"key\":" + std::to_string (r.keyHz) + ",\"peak\":" + std::to_string (r.peakDb)
                  + ",\"gain\":" + std::to_string (r.gain) + ",\"layers\":[";
    for (int l = 0; l < kf::numLayers; ++l)
    {
        const auto& lv = r.layers[(size_t) l];
        if (l) j += ",";
        if (! lv.on) { j += "null"; continue; }
        j += "{\"name\":\"" + esc (lv.name) + "\",\"start\":" + std::to_string (lv.start) + ",\"n\":" + std::to_string (lv.len)
           + ",\"hz\":" + std::to_string (r.detectedHz[(size_t) l]) + ",\"w\":\"" + b64 (reduce (lv.wave, lv.start, r.gain)) + "\"}";
    }
    std::vector<std::int16_t> grv ((size_t) cols, 0);
    for (int c = 0; c < cols; ++c)
    {
        const auto a = (int) ((double) c / cols * total), b = std::max (a + 1, (int) ((double) (c + 1) / cols * total));
        float m = 0.0f;
        for (int i = a; i < std::min (b, (int) r.gr.size()); ++i) m = std::min (m, r.gr[(size_t) i]);
        grv[(size_t) c] = (std::int16_t) std::lround (std::clamp (m / 24.0f, -1.0f, 0.0f) * 32767.0f);
    }
    j += "],\"sum\":\"" + b64 (reduce (r.sum, 0, r.gain)) + "\",\"post\":\"" + b64 (reduce (r.post, 0, 1.0)) + "\",\"gr\":\"" + b64 (grv) + "\"}";
    return j;
}

//==============================================================================
namespace kf
{
    /** Liste de la bibliothèque (familles, noms des morceaux, réponses impulsionnelles) pour l'interface. */
    inline std::string libraryJson()
    {
        auto esc = [] (const std::string& s) { std::string o; for (auto ch : s) { if (ch == '"' || ch == '\\') o += '\\'; o += ch; } return o; };
        std::string j = "{\"families\":[";
        for (int f = 0; f < numFamilies; ++f)
        {
            const auto& fi = families()[(size_t) f];
            if (f) j += ",";
            j += "{\"name\":\"" + esc (fi.name) + "\",\"tag\":\"" + esc (fi.tag) + "\",\"per\":" + std::to_string (fi.perFlavor) + ",\"flavors\":[";
            for (size_t k = 0; k < fi.flavors.size(); ++k) j += (k ? ",\"" : "\"") + esc (fi.flavors[k]) + "\"";
            j += "],\"parts\":[";
            for (int i = 0; i < partCount (f); ++i) j += (i ? ",\"" : "\"") + esc (partName (f, i)) + "\"";
            j += "]}";
        }
        j += "],\"irs\":[";
        for (size_t i = 0; i < irList().size(); ++i) j += (i ? ",[\"" : "[\"") + esc (irList()[i].first) + "\",\"" + esc (irList()[i].second) + "\"]";
        return j + "]}";
    }

    /** Réglages et sections (sans les sons d'usine, tirés ensuite de ces réglages). */
    inline const ModuleDesc& paramsDesc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "kickforge";
            m.name = "Kick Forge";
            m.family = "Batteries";
            m.principle = "Constructeur de kicks par couches : morceaux calcul\xC3\xA9s ou samples, fondus \xC3\xA0 la souris, rack de 9 effets, styles et tirage au hasard";
            std::vector<std::string> famNames;
            for (const auto& f : families()) famNames.push_back (f.name);
            const int defFam[numLayers] { fPunch, fClick, fBody, fRumble, fCrunch, fPunchTop, fEnhReverb, fReverse };
            const int defPart[numLayers] { 16, 0, 0, 0, 0, 0, 0, 0 };
            const float defVol[numLayers] { 0.0f, -9.0f, 0.0f, -12.0f, -6.0f, -9.0f, -12.0f, -8.0f };
            for (int l = 0; l < numLayers; ++l)
            {
                const auto px = layerPrefix (l);
                const auto L = "Couche " + std::to_string (l + 1) + " : ";
                m.params.push_back (C (px + "on", L + "marche", { "Non", "Oui" }, l < 3 ? 1 : 0));
                m.params.push_back (C (px + "fam", L + "famille", famNames, defFam[l]));
                m.params.push_back (I (px + "part", L + "morceau", "", 0, 199, defPart[l]));
                m.params.push_back (P (px + "vol", L + "volume", "dB", -48, 12, defVol[l]));
                m.params.push_back (P (px + "pitch", L + "hauteur", "dt", -24, 24, 0));
                m.params.push_back (P (px + "fine", L + "hauteur fine", "ct", -100, 100, 0));
                m.params.push_back (P (px + "stretch", L + "\xC3\xA9tirement", "\xC3\x97", 0.25f, 4.0f, 1.0f, 0.5f));
                m.params.push_back (P (px + "offset", L + "d\xC3\xA9" "calage", "ms", -20, 1000, 0, 0.5f));
                m.params.push_back (P (px + "len", L + "longueur", "ms", 5, 4000, 4000, 0.4f));
                m.params.push_back (P (px + "filen", L + "fondu d'entr\xC3\xA9" "e", "ms", 0, 1000, 0, 0.4f));
                m.params.push_back (P (px + "ficurve", L + "courbe d'entr\xC3\xA9" "e", "%", -100, 100, 0));
                m.params.push_back (P (px + "folen", L + "fondu de sortie", "ms", 0, 2000, 5, 0.4f));
                m.params.push_back (P (px + "focurve", L + "courbe de sortie", "%", -100, 100, 0));
                m.params.push_back (C (px + "mute", L + "muet", { "Non", "Oui" }, 0));
                m.params.push_back (C (px + "solo", L + "solo", { "Non", "Oui" }, 0));
                m.params.push_back (C (px + "phase", L + "polarit\xC3\xA9 invers\xC3\xA9" "e", { "Non", "Oui" }, 0));
                m.params.push_back (C (px + "rev", L + "\xC3\xA0 l'envers", { "Non", "Oui" }, 0));
                m.params.push_back (C (px + "lock", L + "verrou (hasard)", { "Non", "Oui" }, 0));
                m.params.push_back (C (px + "keytrk", L + "sample accord\xC3\xA9", { "Non", "Oui" }, 1));
                m.params.push_back (P (px + "hp", L + "passe-haut", "Hz", 20, 2000, 20, 0.3f));
                m.params.push_back (P (px + "lp", L + "passe-bas", "Hz", 200, 20000, 20000, 0.3f));
                m.params.push_back (P (px + "eqlo", L + "grave", "dB", -18, 18, 0));
                m.params.push_back (P (px + "eqmidf", L + "fr\xC3\xA9quence m\xC3\xA9" "dium", "Hz", 60, 12000, 1000, 0.3f));
                m.params.push_back (P (px + "eqmidg", L + "m\xC3\xA9" "dium", "dB", -18, 18, 0));
                m.params.push_back (P (px + "eqhi", L + "aigu", "dB", -18, 18, 0));
                m.params.push_back (P (px + "tratk", L + "attaque", "%", -100, 100, 0));
                m.params.push_back (P (px + "trsus", L + "maintien", "%", -100, 100, 0));
                m.params.push_back (C (px + "drvtype", L + "type de distorsion", gd2::Gd2Dist::names(), 1));
                m.params.push_back (P (px + "drv", L + "pouss\xC3\xA9" "e", "dB", 0, 36, 0));
                m.params.push_back (P (px + "drvmix", L + "m\xC3\xA9lange distorsion", "%", 0, 100, 100));
            }
            m.params.push_back (C ("style", "Style", styleNames(), 0));
            m.params.push_back (C ("key", "Tonalit\xC3\xA9", keyNames(), 5)); // fa 1
            m.params.push_back (C ("pitchmode", "Hauteur jou\xC3\xA9" "e", { "Fixe (tonalit\xC3\xA9)", "Suit la note" }, 0));
            m.params.push_back (P ("velsens", "V\xC3\xA9locit\xC3\xA9", "%", 0, 100, 50));
            m.params.push_back (P ("volume", "Volume", "dB", -24, 6, 0));
            m.params.push_back (C ("autogain", "Gain automatique", { "Non", "Oui" }, 1));
            // rack : marche, rang, puis réglages de chaque module
            std::vector<std::string> irs;
            for (const auto& [cat, name] : irList()) irs.push_back (std::string (cat) + " \xC2\xB7 " + name);
            for (int mo = 0; mo < numModules; ++mo)
            {
                const std::string px = std::string (modulePrefix[mo]) + "_";
                const std::string N = std::string (moduleNames[mo]) + " : ";
                m.params.push_back (C (px + "on", N + "marche", { "Non", "Oui" }, 0));
                m.params.push_back (I (px + "ord", N + "rang", "", 1, numModules, mo + 1));
                auto add = [&] (ParamDesc pd) { m.params.push_back (std::move (pd)); };
                switch (mo)
                {
                    case mEq:
                        add (P (px + "lsf", N + "fr\xC3\xA9q. plateau grave", "Hz", 20, 500, 80, 0.4f)); add (P (px + "lsg", N + "plateau grave", "dB", -18, 18, 0));
                        add (P (px + "b1f", N + "fr\xC3\xA9q. cloche 1", "Hz", 30, 16000, 250, 0.3f)); add (P (px + "b1g", N + "cloche 1", "dB", -18, 18, 0)); add (P (px + "b1q", N + "largeur 1", "", 0.3f, 8.0f, 1.0f, 0.5f));
                        add (P (px + "b2f", N + "fr\xC3\xA9q. cloche 2", "Hz", 30, 16000, 2500, 0.3f)); add (P (px + "b2g", N + "cloche 2", "dB", -18, 18, 0)); add (P (px + "b2q", N + "largeur 2", "", 0.3f, 8.0f, 1.0f, 0.5f));
                        add (P (px + "hsf", N + "fr\xC3\xA9q. plateau aigu", "Hz", 1000, 16000, 8000, 0.4f)); add (P (px + "hsg", N + "plateau aigu", "dB", -18, 18, 0));
                        break;
                    case mMs:
                        add (P (px + "midlo", N + "milieu grave", "dB", -12, 12, 0)); add (P (px + "midhi", N + "milieu aigu", "dB", -12, 12, 0));
                        add (P (px + "sidehp", N + "c\xC3\xB4t\xC3\xA9s passe-haut", "Hz", 20, 2000, 150, 0.3f)); add (P (px + "sideg", N + "c\xC3\xB4t\xC3\xA9s", "dB", -24, 12, 0));
                        add (P (px + "width", N + "largeur", "%", 0, 200, 100));
                        break;
                    case mDist:
                        add (C (px + "type", N + "type", gd2::Gd2Dist::names(), 0)); add (P (px + "drive", N + "pouss\xC3\xA9" "e", "dB", 0, 36, 12));
                        add (C (px + "pre", N + "pr\xC3\xA9" "filtre", { "Aucun", "Passe-bas", "Passe-bande", "Passe-haut" }, 0)); add (P (px + "pref", N + "fr\xC3\xA9q. pr\xC3\xA9" "filtre", "Hz", 40, 10000, 800, 0.3f));
                        add (P (px + "tone", N + "ton", "Hz", 500, 20000, 20000, 0.3f));
                        break;
                    case mShaper:
                        add (P (px + "amount", N + "quantit\xC3\xA9", "%", 0, 100, 100));
                        for (int k = 1; k <= 8; ++k) add (P (px + "n" + std::to_string (k), N + "point " + std::to_string (k), "%", 0, 100, 100));
                        break;
                    case mTrans:
                        add (P (px + "attack", N + "attaque", "%", -100, 100, 30)); add (P (px + "duck", N + "creux", "%", 0, 100, 0));
                        add (C (px + "speed", N + "vitesse", { "1/16", "1/32", "1/48", "1/64" }, 2));
                        break;
                    case mConv:
                        add (C (px + "ir", N + "r\xC3\xA9ponse", irs, 8)); add (P (px + "wet", N + "mouill\xC3\xA9", "%", 0, 100, 30));
                        add (P (px + "stereo", N + "st\xC3\xA9r\xC3\xA9o", "%", 0, 100, 50)); add (P (px + "tone", N + "ton", "%", -100, 100, 0));
                        add (P (px + "speed", N + "vitesse", "%", -100, 100, 0));
                        add (P (px + "c1", N + "courbe : d\xC3\xA9part", "%", 0, 100, 100)); add (P (px + "c2t", N + "courbe : milieu (temps)", "%", 1, 99, 30));
                        add (P (px + "c2", N + "courbe : milieu", "%", 0, 100, 60)); add (P (px + "c3", N + "courbe : fin", "%", 0, 100, 0));
                        break;
                    case mFinal:
                        add (P (px + "boostf", N + "fr\xC3\xA9q. grave", "Hz", 30, 200, 60, 0.5f)); add (P (px + "boost", N + "grave relev\xC3\xA9", "dB", 0, 12, 0));
                        add (P (px + "mono", N + "grave en mono sous", "Hz", 20, 500, 120, 0.4f)); add (P (px + "clip", N + "\xC3\xA9" "cr\xC3\xAA" "tage", "%", 0, 100, 0));
                        break;
                    case mMulti:
                        add (P (px + "depth", N + "profondeur", "%", 0, 100, 40)); add (P (px + "time", N + "temps", "%", 10, 1000, 100, 0.4f));
                        add (P (px + "up", N + "vers le haut", "%", 0, 100, 50)); add (P (px + "down", N + "vers le bas", "%", 0, 100, 100));
                        add (P (px + "low", N + "grave", "dB", -12, 12, 0)); add (P (px + "mid", N + "m\xC3\xA9" "dium", "dB", -12, 12, 0)); add (P (px + "high", N + "aigu", "dB", -12, 12, 0));
                        break;
                    default:
                        add (P (px + "hp", N + "passe-haut", "Hz", 20, 2000, 20, 0.3f)); add (P (px + "hpres", N + "r\xC3\xA9sonance passe-haut", "%", 0, 100, 0));
                        add (P (px + "lp", N + "passe-bas", "Hz", 200, 20000, 20000, 0.3f)); add (P (px + "lpres", N + "r\xC3\xA9sonance passe-bas", "%", 0, 100, 0));
                        break;
                }
                add (P (px + "mix", N + "m\xC3\xA9lange", "%", 0, 100, 100));
            }
            // après le rack (positions figées à la suite) : alignement de phase du grave des couches
            m.params.push_back (C ("phasealign", "Alignement de phase", { "Non", "Oui" }, 1));
            // sections (écran SON générique) : une par couche, le kit, le rack
            for (int l = 0; l < numLayers; ++l)
            {
                SectionDesc s;
                s.id = "layer" + std::to_string (l + 1); s.name = "Couche " + std::to_string (l + 1); s.visual = "kflayer";
                for (auto* id : { "on", "fam", "part", "vol", "pitch", "stretch", "offset", "len", "filen", "folen", "hp", "lp", "drv" }) s.params.push_back (layerPrefix (l) + id);
                m.sections.push_back (s);
            }
            {
                SectionDesc s; s.id = "kfmain"; s.name = "Kick"; s.visual = "kfmain";
                s.params = { "style", "key", "pitchmode", "velsens", "volume", "autogain", "phasealign" };
                m.sections.push_back (s);
            }
            for (int mo = 0; mo < numModules; ++mo)
            {
                SectionDesc s; s.id = std::string ("kf") + modulePrefix[mo]; s.name = moduleNames[mo]; s.visual = "kfmod"; s.tab = "kffx";
                const std::string px = std::string (modulePrefix[mo]) + "_";
                s.params.push_back (px + "on");
                for (auto* id : moduleParams()[(size_t) mo]) s.params.push_back (px + id);
                m.sections.push_back (s);
            }
            return m;
        }();
        return d;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m = paramsDesc();
            m.presets.push_back ({ "Init", {}, "Init" });
            // sons d'usine : six tirages fixes de chaque style (à 150 BPM pour les décalages au tempo)
            for (int st = 1; st < (int) styleNames().size(); ++st)
                for (int k = 0; k < 6; ++k)
                {
                    auto p = defaults();
                    p[gStyle] = (float) st;
                    randomize (p, 0, -1, (std::uint32_t) (st * 101 + k * 7 + 1), 150.0, false);
                    Preset pr;
                    pr.name = styleNames()[(size_t) st] + " " + (k < 9 ? "0" : "") + std::to_string (k + 1);
                    pr.category = styleNames()[(size_t) st];
                    const auto def = defaults();
                    for (size_t i = 0; i < p.size(); ++i)
                        if (p[i] != def[i]) pr.values.push_back ({ m.params[i].id, p[i] });
                    m.presets.push_back (std::move (pr));
                }
            return m;
        }();
        return d;
    }
} // namespace kf

inline std::vector<float> kf::defaults()
{
    const auto& d = paramsDesc();
    std::vector<float> v (d.params.size());
    for (size_t i = 0; i < v.size(); ++i) v[i] = d.params[i].def;
    return v;
}

} // namespace grv::instruments
