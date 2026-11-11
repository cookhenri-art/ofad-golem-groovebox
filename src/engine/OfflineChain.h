/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 (lot 1) : moteur de traitement HORS TEMPS RÉEL. Une chaîne de 4 effets quelconques du
    catalogue (Restauration comprise) traite un tampon stéréo à part du fil audio :
      - chaque effet est une instance neuve (jamais celle d'une piste), préparée à la fréquence du son ;
      - latence des effets : on la compense (le résultat commence pile au début du son d'origine) ;
      - queue (réverbération, delay) : gardée si demandé, jusqu'à ce qu'elle passe sous −90 dB
        pendant 0,3 s (au plus maxTail secondes), sinon le son garde sa longueur ;
      - les effets qui ont un mode « qualité maximale » le prennent (Effect::setOffline) ;
      - les effets calés sur la mesure reçoivent le tempo et une position qui avance ;
      - progression et annulation par atomiques (le calcul tourne dans un fil à lui).
    Aucune allocation dans la boucle de calcul hors des tampons de sortie, réservés d'avance.
*/

#pragma once

#include "Catalog.h"
#include "Module.h"
#include "Params.h"
#include "../dsp/NoDenormals.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace grv::engine
{

/** Un emplacement de la chaîne : type d'effet (vide : emplacement libre), valeurs RÉELLES des
    réglages dans l'ordre de desc().params (manquantes : valeur par défaut), contournement. */
struct ChainSlot
{
    std::string type;
    std::vector<float> plain;
    bool bypass = false;
};

struct ChainResult
{
    std::vector<float> left, right;  // right vide si le résultat est mono
    int latency = 0;                 // latence totale compensée (échantillons)
    int tail = 0;                    // échantillons de queue ajoutés
    std::string error;
};

class OfflineChain
{
public:
    /** Prépare la chaîne (hors du fil audio). Renvoie le message d'erreur, vide si tout va bien. */
    std::string prepare (const std::vector<ChainSlot>& chain, double sampleRate, double bpm, bool offlineQuality = true)
    {
        fx.clear(); params.clear(); shape.clear();
        sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
        tempo = bpm > 20.0 ? bpm : 120.0;
        for (const auto& s : chain)
        {
            if (s.type.empty() || s.bypass) continue;
            auto e = createEffect (s.type);
            if (e == nullptr) return "Effet inconnu : " + s.type;
            const auto& d = e->desc();
            std::vector<float> p (std::max<size_t> (d.params.size(), 1), 0.0f);
            for (size_t i = 0; i < d.params.size(); ++i)
            {
                const auto& pd = d.params[i];
                auto v = i < s.plain.size() ? s.plain[i] : pd.def;
                if (! std::isfinite (v)) v = pd.def;
                v = pd.toPlain (pd.toNorm (v)); // bornée et arrondie comme dans le moteur
                p[i] = v;
            }
            e->setOffline (offlineQuality);
            e->prepare (sr, block);
            e->reset();
            fx.push_back (std::move (e));
            params.push_back (std::move (p));
            shape.push_back (s.type);
        }
        staged = params;
        return {};
    }

    /** 1.1 : même suite d'effets actifs (types, dans l'ordre) : on peut changer les réglages sans
        reconstruire la chaîne (pas de remise à zéro des effets, donc pas de craquement). */
    bool sameShape (const std::vector<ChainSlot>& chain) const
    {
        size_t k = 0;
        for (const auto& s : chain)
        {
            if (s.type.empty() || s.bypass) continue;
            if (k >= shape.size() || shape[k] != s.type) return false;
            ++k;
        }
        return k == shape.size();
    }

    /** 1.1 : nouveaux réglages (fil de l'interface, après sameShape) : repris par le fil audio au
        bloc suivant, sans verrou (le fil audio saute la reprise si l'interface est en train d'écrire). */
    void stageParams (const std::vector<ChainSlot>& chain)
    {
        for (;;) // libre (0) ou réglages pas encore repris (2) : on prend la main ; 1 : le fil audio recopie
        {
            int e = stageState.load (std::memory_order_acquire);
            if (e != 1 && stageState.compare_exchange_weak (e, 1, std::memory_order_acquire)) break;
            std::this_thread::yield();
        }
        size_t k = 0;
        for (const auto& s : chain)
        {
            if (s.type.empty() || s.bypass) continue;
            if (k >= fx.size()) break;
            const auto& d = fx[k]->desc();
            for (size_t i = 0; i < d.params.size() && i < staged[k].size(); ++i)
            {
                const auto& pd = d.params[i];
                auto v = i < s.plain.size() ? s.plain[i] : pd.def;
                if (! std::isfinite (v)) v = pd.def;
                staged[k][i] = pd.toPlain (pd.toNorm (v));
            }
            ++k;
        }
        stageState.store (2, std::memory_order_release);
    }

    bool empty() const noexcept { return fx.empty(); }

    /** Latence totale (après prepare, réglages appliqués par un premier bloc silencieux). */
    int latency() noexcept
    {
        int l = 0;
        for (auto& e : fx) l += std::max (0, e->latency());
        return l;
    }

    /** Traite inL/inR (inR nul : mono) de n échantillons. keepTail : garder la queue (au plus
        maxTailSec secondes). progress (0..1) et cancel sont facultatifs. */
    ChainResult render (const float* inL, const float* inR, int n, bool keepTail, double maxTailSec = 20.0,
                        std::atomic<float>* progress = nullptr, const std::atomic<bool>* cancel = nullptr)
    {
        ChainResult res;
        if (n <= 0) { res.error = "Son vide"; return res; }
        dsp::ScopedFlushDenormals noDenormals;          // hors du fil audio : queues qui s'éteignent sans ralentir
        const bool stereoIn = inR != nullptr;

        // un premier bloc silencieux : certains effets règlent leur latence d'après leurs réglages
        float zl[block] {}, zr[block] {};
        std::vector<const float*> scL ((size_t) numTracks, zl), scR ((size_t) numTracks, zr);
        double ppq = 0.0;
        const double ppqPerSample = tempo / 60.0 / sr;
        auto runBlock = [&] (float* l, float* r, int m)
        {
            for (size_t k = 0; k < fx.size(); ++k)
            {
                fx[k]->setSidechainSources (scL.data(), scR.data());
                fx[k]->setPosition (ppq, true);
                fx[k]->process (l, r, m, params[k].data(), tempo);
            }
            ppq += ppqPerSample * m;
        };
        {
            float a[block] {}, b[block] {};
            runBlock (a, b, 1);
            for (auto& e : fx) e->reset();
            ppq = 0.0;
        }
        const int lat = latency();
        res.latency = lat;
        const int maxTail = keepTail ? (int) std::min (maxTailSec * sr, 600.0 * sr) : 0;
        const int total = n + lat + maxTail;
        res.left.reserve ((size_t) (n + maxTail)); res.right.reserve ((size_t) (n + maxTail));

        float bl[block], br[block];
        int fed = 0, skipped = 0, quietRun = 0;
        const int quietNeed = (int) (0.3 * sr);
        const float quiet = std::pow (10.0f, -90.0f / 20.0f);
        bool stereoOut = stereoIn;
        while (fed < total)
        {
            if (cancel != nullptr && cancel->load (std::memory_order_relaxed)) { res.error = "Annul\xC3\xA9"; res.left.clear(); res.right.clear(); return res; }
            const int m = std::min (block, total - fed);
            for (int i = 0; i < m; ++i)
            {
                const int s = fed + i;
                bl[i] = s < n ? inL[s] : 0.0f;
                br[i] = s < n ? (stereoIn ? inR[s] : inL[s]) : 0.0f;
            }
            runBlock (bl, br, m);
            for (int i = 0; i < m; ++i)
            {
                auto l = std::isfinite (bl[i]) ? std::clamp (bl[i], -8.0f, 8.0f) : 0.0f;
                auto r = std::isfinite (br[i]) ? std::clamp (br[i], -8.0f, 8.0f) : 0.0f;
                if (! stereoOut && std::abs (l - r) > 1.0e-7f) stereoOut = true; // effet stéréo sur un son mono
                if (skipped < lat) { ++skipped; continue; }
                const int outIndex = (int) res.left.size();
                if (outIndex >= n)
                {
                    if (! keepTail) break;
                    quietRun = std::max (std::abs (l), std::abs (r)) < quiet ? quietRun + 1 : 0;
                }
                res.left.push_back (l); res.right.push_back (r);
            }
            fed += m;
            if ((int) res.left.size() >= n && (! keepTail || quietRun >= quietNeed)) break;
            if (progress != nullptr) progress->store (std::min (1.0f, (float) fed / (float) std::max (1, n + lat)), std::memory_order_relaxed);
        }
        // queue : on retire le silence final (on garde 0,1 s de marge après la dernière valeur audible)
        if ((int) res.left.size() > n)
        {
            int last = n;
            for (int i = (int) res.left.size() - 1; i >= n; --i)
                if (std::max (std::abs (res.left[(size_t) i]), std::abs (res.right[(size_t) i])) >= quiet) { last = i + 1; break; }
            const int keep = std::min ((int) res.left.size(), last + (int) (0.1 * sr));
            res.left.resize ((size_t) keep); res.right.resize ((size_t) keep);
        }
        while ((int) res.left.size() < n) { res.left.push_back (0.0f); res.right.push_back (0.0f); }
        res.tail = (int) res.left.size() - n;
        if (! stereoOut) res.right.clear();
        if (progress != nullptr) progress->store (1.0f, std::memory_order_relaxed);
        return res;
    }

    //==========================================================================
    // 1.1 (lot 3) : au fil de l'eau (prise « pendant l'enregistrement », fil d'écriture). Le son
    // ressort recalé : streamStart() rend la latence, streamProcess() traite par blocs et ne rend
    // que le son déjà recalé, streamFinish() pousse la fin (autant de silence que de latence).
    int streamStart() noexcept
    {
        float a[block] {}, b[block] {};
        sppq = 0.0; runStream (a, b, 1);
        for (auto& e : fx) e->reset();
        sppq = 0.0; skip = latency();
        return skip;
    }
    /** Traite n échantillons en place ; renvoie le nombre d'échantillons recalés à écrire, placés au
        début de l/r (les premiers, encore « dans la latence », sont retirés). */
    int streamProcess (float* l, float* r, int n) noexcept
    {
        dsp::ScopedFlushDenormals noDenormals;          // fil d'écriture de la prise
        for (int i = 0; i < n; i += block) runStream (l + i, r + i, std::min (block, n - i));
        for (int i = 0; i < n; ++i)
        {
            l[i] = std::isfinite (l[i]) ? std::clamp (l[i], -8.0f, 8.0f) : 0.0f;
            r[i] = std::isfinite (r[i]) ? std::clamp (r[i], -8.0f, 8.0f) : 0.0f;
        }
        const int drop = std::min (skip, n);
        skip -= drop;
        if (drop > 0) { std::memmove (l, l + drop, sizeof (float) * (size_t) (n - drop)); std::memmove (r, r + drop, sizeof (float) * (size_t) (n - drop)); }
        return n - drop;
    }

    /** 1.1 (lot 3) : écoute du traitement (sortie principale) : traité en place, latence comprise. */
    void monitorProcess (float* l, float* r, int n) noexcept
    {
        if (int ready = 2; stageState.compare_exchange_strong (ready, 1, std::memory_order_acquire))
        {
            for (size_t k = 0; k < params.size(); ++k) std::copy (staged[k].begin(), staged[k].end(), params[k].begin());
            stageState.store (0, std::memory_order_release);
        }
        for (int i = 0; i < n; i += block) runStream (l + i, r + i, std::min (block, n - i));
        for (int i = 0; i < n; ++i)
        {
            l[i] = std::isfinite (l[i]) ? std::clamp (l[i], -4.0f, 4.0f) : 0.0f;
            r[i] = std::isfinite (r[i]) ? std::clamp (r[i], -4.0f, 4.0f) : 0.0f;
        }
    }

private:
    void runStream (float* l, float* r, int m) noexcept
    {
        static const float zero[block] {};
        const float* sc[numTracks];
        for (auto& p : sc) p = zero;
        for (size_t k = 0; k < fx.size(); ++k)
        {
            fx[k]->setSidechainSources (sc, sc);
            fx[k]->setPosition (sppq, true);
            fx[k]->process (l, r, m, params[k].data(), tempo);
        }
        sppq += tempo / 60.0 / sr * m;
    }
    double sppq = 0.0;
    int skip = 0;

    static constexpr int block = 256;
    double sr = 48000.0, tempo = 120.0;
    std::vector<std::unique_ptr<Effect>> fx;
    std::vector<std::vector<float>> params;
    std::vector<std::string> shape;                 // types des effets actifs (sameShape)
    std::vector<std::vector<float>> staged;         // réglages en attente (stageParams)
    std::atomic<int> stageState { 0 };              // 0 libre, 1 en cours d'écriture ou de reprise, 2 prêts
};

} // namespace grv::engine
