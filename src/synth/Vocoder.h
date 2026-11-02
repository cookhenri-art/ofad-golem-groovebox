/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Cœur commun des vocodeurs (v0.25) : banc de filtres d'analyse et de synthèse, suiveurs
    d'enveloppe, détection voisé / non voisé, voix interne de démonstration.

    Principe (vocodeur à bancs de filtres, depuis les machines de laboratoire des années 30) : le
    MODULATEUR (une voix) passe dans N filtres passe-bande ; un suiveur mesure l'énergie de chaque
    bande. La PORTEUSE (un synthé joué au clavier) passe dans N filtres semblables ; chaque bande
    est ouverte par l'enveloppe mesurée. Le timbre de la voix est ainsi imprimé sur le synthé.

    Choix du modèle (latence nulle, coût maîtrisé) :
      - filtres à variables d'état TPT (stables, réglables sans clic), sortie passe-bande
        normalisée (gain 1 au centre), 1 ou 2 étages en cascade (12 ou 24 dB/oct) ;
      - synthèse en mono sur la somme de la porteuse : la stéréo vient de la répartition des
        bandes (gauche / droite) et des effets ; le coût est divisé par deux ;
      - suiveurs à attaque et relâchement séparés (redresseur double alternance + un pôle) ;
      - pas de transformée de Fourier : aucune latence (un vocodeur à FFT en ajouterait 10 à 40 ms).
    Écarté volontairement : modèles spectraux à FFT, analyse par prédiction linéaire (LPC).

    Aucune allocation dans le rendu : 40 bandes au plus, tableaux fixes.
*/

#pragma once

#include "SynthCore.h"
#include "../engine/Speech.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace grv::synth
{

/** Coefficients d'un passe-bande TPT à gain 1 au centre (partagés entre étages). */
struct BandCoefs
{
    double a1 = 0.0, a2 = 0.0, a3 = 0.0, k = 1.0;

    void set (double fc, double q, double sr) noexcept
    {
        fc = std::clamp (fc, 20.0, 0.45 * sr);
        const auto g = std::tan (kPi * fc / sr);
        k = 1.0 / std::max (0.1, q);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

struct BandState
{
    double ic1 = 0.0, ic2 = 0.0;
    void reset() noexcept { ic1 = ic2 = 0.0; }
    double bp (double x, const BandCoefs& c) noexcept
    {
        const auto v3 = x - ic2;
        const auto v1 = c.a1 * ic1 + c.a2 * v3;
        const auto v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return c.k * v1;
    }
};

/** Banc de filtres d'un vocodeur. */
class VocoderBank
{
public:
    static constexpr int maxBands = 40;

    struct Config
    {
        int bands = 16;
        double fLo = 50.0, fHi = 5080.0;
        int order = 2;               // étages par filtre (1 : 12 dB/oct, 2 : 24 dB/oct)
        double qScale = 1.0;         // < 1 : bandes larges ; > 1 : bandes étroites (sonorité « cristal »)
        double trimDb = 0.0;         // correction de niveau mesurée (variantes au même niveau)
        double formant = 0.0;        // décalage des fréquences de synthèse (demi-tons)
        int bandShift = 0;           // la bande k de synthèse reçoit l'enveloppe de la bande k - décalage
        bool invert = false;         // grave ↔ aigu
        double attackMs = 2.0, releaseMs = 40.0;
        double expansion = 0.0;      // -0,5..2 : écarte (> 0) ou rapproche (< 0) les bandes fortes et faibles
        double traces = 0.0;         // 0..1 : traîne longue des bandes (sonorité réverbérante)
        double stereo = 0.0;         // 0..1 : bandes paires à gauche, impaires à droite
        bool freeze = false;         // enveloppes figées (maintien)

        bool sameFilters (const Config& o) const noexcept
        {
            return bands == o.bands && fLo == o.fLo && fHi == o.fHi && order == o.order && qScale == o.qScale && trimDb == o.trimDb;
        }
    };

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        peakRel = std::exp (-1.0 / (0.3 * sr));
        xfStep = 1.0 / (0.003 * sr); // fondu de 3 ms quand les filtres changent
        cfg.bands = 0; // force le calcul
        hasPending = false; xf = 1.0; xfDir = 0;
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : an) s.reset();
        for (auto& s : an2) s.reset();
        for (auto& s : sy) s.reset();
        for (auto& s : sy2) s.reset();
        env.fill (0.0); slow.fill (0.0); used.fill (0.0); shown.fill (0.0);
        peak = 0.0;
    }

    /** Filtres de synthèse à zéro (synthèse arrêtée puis reprise). */
    void resetSynth() noexcept { for (auto& st : sy) st.reset(); for (auto& st : sy2) st.reset(); }

    /** À chaque bloc : ne recalcule que ce qui a changé. */
    void setConfig (const Config& c) noexcept
    {
        Config next = c;
        next.bands = std::clamp (next.bands, 2, maxBands);
        next.order = std::clamp (next.order, 1, 2);
        next.fHi = std::min (next.fHi, 0.42 * sr);
        next.fLo = std::clamp (next.fLo, 20.0, next.fHi * 0.5);
        if (cfg.bands != 0 && ! next.sameFilters (hasPending ? pending : cfg))
        {
            // le banc change en cours de jeu (nombre de bandes, algorithme) :
            // fondu de sortie de 3 ms, changement, puis fondu d'entrée (aucun clic)
            pending = next;
            hasPending = true;
            xfDir = -1;
        }
        if (hasPending)
        {
            // en attendant le changement, les réglages sans filtres s'appliquent à l'ancien banc
            Config keep = cfg;
            keep.bandShift = next.bandShift; keep.invert = next.invert; keep.attackMs = next.attackMs; keep.releaseMs = next.releaseMs;
            keep.expansion = next.expansion; keep.traces = next.traces; keep.stereo = next.stereo; keep.freeze = next.freeze;
            keep.formant = next.formant;
            pending = next;
            next = keep;
        }
        apply (next);
    }

    /** Fondu en cours ou changement de filtres en attente. */
    bool busy() const noexcept { return xfDir != 0; }

private:
    void apply (const Config& next) noexcept
    {
        if (! next.sameFilters (cfg))
        {
            const auto n = next.bands;
            const auto ratio = std::pow (next.fHi / next.fLo, 1.0 / (double) (n - 1));
            // largeur d'une bande = espacement : les bandes voisines se croisent vers -3 dB
            const auto bw = std::log2 (ratio);
            const auto q0 = std::sqrt (std::exp2 (bw)) / (std::exp2 (bw) - 1.0);
            const auto q = q0 * (next.order == 2 ? 0.72 : 1.0) * next.qScale;
            const auto shift = std::exp2 (std::clamp (next.formant, -24.0, 24.0) / 12.0);
            const auto trim = std::pow (10.0, next.trimDb / 20.0);
            for (int k = 0; k < n; ++k)
            {
                const auto fc = next.fLo * std::pow (ratio, (double) k);
                freq[(size_t) k] = fc;
                ac[(size_t) k].set (fc, q, sr);
                sc[(size_t) k].set (fc * shift, q, sr);
            }
            // gain de compensation : plus de bandes étroites = moins d'énergie par bande
            // bandes plus étroites (plus nombreuses, Q plus fort) : chaque bande porte moins d'énergie
            // dans l'analyse comme dans la synthèse ; la somme suit √n × Q (puis correction mesurée)
            makeup = 2.2 * std::sqrt ((double) n / 16.0) * next.qScale * (next.order == 2 ? 1.25 : 0.55) * trim;
            if (cfg.bands == 0) reset();
            else remapEnvelopes (next);
        }
        else if (next.formant != cfg.formant)
        {
            // formants seuls (bouton tourné, LFO) : les filtres de synthèse glissent, sans fondu
            const auto n = next.bands;
            const auto ratio = std::pow (next.fHi / next.fLo, 1.0 / (double) (n - 1));
            const auto bw = std::log2 (ratio);
            const auto q = std::sqrt (std::exp2 (bw)) / (std::exp2 (bw) - 1.0) * (next.order == 2 ? 0.72 : 1.0) * next.qScale;
            const auto shift = std::exp2 (std::clamp (next.formant, -24.0, 24.0) / 12.0);
            for (int k = 0; k < n; ++k) sc[(size_t) k].set (freq[(size_t) k] * shift, q, sr);
        }
        if (next.attackMs != cfg.attackMs || next.releaseMs != cfg.releaseMs || next.traces != cfg.traces || cfg.bands == 0)
        {
            att = std::exp (-1.0 / (std::max (0.1, next.attackMs) * 0.001 * sr));
            rel = std::exp (-1.0 / (std::max (1.0, next.releaseMs) * 0.001 * sr));
            slowRel = std::exp (-1.0 / ((300.0 + 3700.0 * std::clamp (next.traces, 0.0, 1.0)) * 0.001 * sr));
        }
        cfg = next;
        for (int k = 0; k < cfg.bands; ++k)
        {
            const auto side = (k & 1) ? 1.0 : -1.0;
            const auto pan = side * std::clamp (cfg.stereo, 0.0, 1.0);
            gl[(size_t) k] = std::min (1.0, 1.0 - pan);
            gr[(size_t) k] = std::min (1.0, 1.0 + pan);
        }
    }

public:
    const Config& config() const noexcept { return cfg; }
    int bands() const noexcept { return cfg.bands; }
    double bandFreq (int k) const noexcept { return freq[(size_t) std::clamp (k, 0, maxBands - 1)]; }

    /** Analyse d'un échantillon du modulateur (met à jour les enveloppes). */
    void analyse (double x) noexcept
    {
        const auto n = cfg.bands;
        if (cfg.freeze) return;
        double pk = 0.0;
        for (int k = 0; k < n; ++k)
        {
            auto y = an[(size_t) k].bp (x, ac[(size_t) k]);
            if (cfg.order == 2) y = an2[(size_t) k].bp (y, ac[(size_t) k]);
            const auto r = std::abs (y);
            auto& e = env[(size_t) k];
            e = r > e ? r + (e - r) * att : r + (e - r) * rel;
            pk = std::max (pk, e);
        }
        // crête de toutes les bandes (expansion) : monte vite, redescend en 300 ms environ
        peak = pk > peak ? pk : peak * peakRel;
    }

    /** Enveloppes à appliquer (décalage, inversion, expansion, traînes), une fois par échantillon. */
    void shape() noexcept
    {
        const auto n = cfg.bands;
        const auto x = std::clamp (cfg.expansion, -0.5, 2.0);
        const auto tr = std::clamp (cfg.traces, 0.0, 1.0);
        const auto ref = 1.0 / (peak + 1.0e-6);
        for (int k = 0; k < n; ++k)
        {
            int src = cfg.invert ? n - 1 - k : k;
            src -= cfg.bandShift;
            double e = src >= 0 && src < n ? env[(size_t) src] : 0.0;
            if (x != 0.0 && e > 1.0e-9) e *= fastExp2 (x * fastLog2 (std::min (1.0, e * ref))); // (e / crête) ^ x
            if (tr > 0.0)
            {
                auto& s = slow[(size_t) k];
                s = e > s ? e : s * slowRel;
                e = std::max (e, tr * s);
            }
            used[(size_t) k] = e;
        }
    }

    /** Synthèse d'un échantillon de porteuse (mono) : sortie stéréo ajoutée à l, r. */
    void synth (double car, double& l, double& r) noexcept
    {
        const auto n = cfg.bands;
        double sl = 0.0, srr = 0.0;
        for (int k = 0; k < n; ++k)
        {
            auto y = sy[(size_t) k].bp (car, sc[(size_t) k]);
            if (cfg.order == 2) y = sy2[(size_t) k].bp (y, sc[(size_t) k]);
            const auto v = y * used[(size_t) k];
            sl += v * gl[(size_t) k];
            srr += v * gr[(size_t) k];
        }
        l += sl * makeup * xf;
        r += srr * makeup * xf;
        if (xfDir != 0)
        {
            xf += xfDir * xfStep;
            if (xf <= 0.0)
            {
                xf = 0.0;
                if (hasPending)
                {
                    hasPending = false;
                    apply (pending);
                    for (auto& st : an) st.reset();
                    for (auto& st : an2) st.reset();
                    resetSynth();
                }
                xfDir = 1;
            }
            else if (xf >= 1.0) { xf = 1.0; xfDir = 0; }
        }
    }

    /** Sans synthèse (porteuse muette) : le changement de filtres se fait sans attendre. */
    void settle() noexcept
    {
        if (! hasPending) return;
        hasPending = false;
        apply (pending);
        for (auto& st : an) st.reset();
        for (auto& st : an2) st.reset();
        resetSynth();
        xf = 1.0; xfDir = 0;
    }

    /** Niveau d'une bande pour l'affichage (0..1, échelle en dB de -60 à 0), mis à jour par bloc. */
    void updateShown() noexcept
    {
        for (int k = 0; k < cfg.bands; ++k)
        {
            const auto db = 20.0 * std::log10 (used[(size_t) k] * 4.0 + 1.0e-9);
            const auto v = std::clamp ((db + 60.0) / 60.0, 0.0, 1.0);
            shown[(size_t) k] = std::max (v, shown[(size_t) k] * 0.85);
        }
    }
    float shownLevel (int k) const noexcept { return (float) shown[(size_t) std::clamp (k, 0, maxBands - 1)]; }
    double envelope (int k) const noexcept { return env[(size_t) std::clamp (k, 0, maxBands - 1)]; }

private:
    /** Enveloppes reportées sur les nouvelles bandes par fréquence (gel conservé, pas de
        nouvelle attaque des bandes) quand le banc change. */
    void remapEnvelopes (const Config& next) noexcept
    {
        const auto oldN = cfg.bands;
        if (oldN < 2) return;
        std::array<double, maxBands> e = env, sl = slow;
        const auto oldLo = std::log2 (cfg.fLo), oldSpan = std::log2 (cfg.fHi) - oldLo;
        const auto ratio = std::pow (next.fHi / next.fLo, 1.0 / (double) (next.bands - 1));
        for (int k = 0; k < next.bands; ++k)
        {
            const auto pos = std::clamp ((std::log2 (next.fLo * std::pow (ratio, (double) k)) - oldLo) / oldSpan * (oldN - 1), 0.0, (double) (oldN - 1));
            const auto i0 = std::min (oldN - 2, (int) pos);
            const auto f = pos - i0;
            env[(size_t) k] = e[(size_t) i0] + (e[(size_t) i0 + 1] - e[(size_t) i0]) * f;
            slow[(size_t) k] = sl[(size_t) i0] + (sl[(size_t) i0 + 1] - sl[(size_t) i0]) * f;
        }
        for (int k = next.bands; k < maxBands; ++k) { env[(size_t) k] = 0.0; slow[(size_t) k] = 0.0; }
    }

    double sr = 48000.0, att = 0.9, rel = 0.999, slowRel = 0.9999, peak = 0.0, makeup = 2.0;
    double peakRel = 0.99995, xf = 1.0, xfStep = 0.007;
    int xfDir = 0;
    bool hasPending = false;
    Config cfg, pending;
    std::array<BandCoefs, maxBands> ac {}, sc {};
    std::array<BandState, maxBands> an {}, an2 {}, sy {}, sy2 {};
    std::array<double, maxBands> env {}, slow {}, used {}, freq {}, gl {}, gr {}, shown {};
};

/** Détecteur voisé / non voisé : part de l'énergie au-dessus de 3,5 kHz. 0 = voyelle, 1 = « s », « ch », « f ». */
class UnvoicedDetector
{
public:
    void prepare (double s) noexcept
    {
        sr = s;
        att = std::exp (-1.0 / (0.002 * sr));
        rel = std::exp (-1.0 / (0.030 * sr));
        smooth = 1.0 - std::exp (-1.0 / (0.002 * sr));
        reset();
    }
    void reset() noexcept { hp1.reset(); hp2.reset(); lp1.reset(); eh = el = u = 0.0; }
    double process (double x) noexcept
    {
        const auto h = hp2.highpass (hp1.highpass (x, 3500.0, sr), 3500.0, sr);
        const auto l = lp1.lowpass (x, 1500.0, sr);
        follow (eh, std::abs (h));
        follow (el, std::abs (l));
        const auto target = eh / (eh + el + 1.0e-6);
        u += (target - u) * smooth;
        return u;
    }
    double value() const noexcept { return u; }

private:
    void follow (double& e, double r) noexcept { e = r > e ? r + (e - r) * att : r + (e - r) * rel; }
    double sr = 48000.0, att = 0.9, rel = 0.99, eh = 0.0, el = 0.0, u = 0.0, smooth = 0.01;
    OnePole hp1, hp2, lp1;
};

/** Suiveur global du modulateur et porte de bruit (le souffle d'un micro ne fait pas parler la porteuse). */
class ModGate
{
public:
    void prepare (double s) noexcept
    {
        sr = s;
        att = std::exp (-1.0 / (0.001 * sr));
        rel = std::exp (-1.0 / (0.060 * sr));
        gcoef = std::exp (-1.0 / (0.005 * sr));
        reset();
    }
    void reset() noexcept { env = 0.0; gain = 0.0; }
    /** threshold : gain linéaire (0 = porte ouverte). */
    double process (double x, double threshold) noexcept
    {
        const auto r = std::abs (x);
        env = r > env ? r + (env - r) * att : r + (env - r) * rel;
        const auto target = threshold <= 0.0 || env >= threshold ? 1.0 : 0.0;
        gain = target + (gain - target) * gcoef;
        return x * gain;
    }
    double level() const noexcept { return env; }

private:
    double sr = 48000.0, att = 0.9, rel = 0.99, gcoef = 0.99, env = 0.0, gain = 0.0;
};

/** Voix interne de démonstration : une phrase de syllabes synthétisées (source glottale filtrée par
    trois formants qui glissent d'une voyelle à l'autre, consonnes sifflantes, plosives et nasales).
    Elle fait parler le vocodeur sans micro ; débit et hauteur réglables. */
class DemoVoice
{
public:
    void prepare (double s) noexcept { sr = s; reset(); }
    void reset() noexcept
    {
        osc.reset(); for (auto& f : form) f.reset(); hs.reset(); hs2.reset(); sibF.reset();
        phase = 0.0; syl = 0; f1 = 730.0; f2 = 1090.0; f3 = 2440.0; amp = 0.0; vib = 0.0;
        restartScript();
    }

    /** v0.33 : phrase tapée (nullptr : phrase de démonstration). Fil audio ; la phrase repart du début. */
    void setScript (const grv::engine::SpeechScript* s) noexcept
    {
        if (s == script) return;
        script = s != nullptr && s->count > 0 ? s : nullptr;
        restartScript();
    }
    bool hasScript() const noexcept { return script != nullptr; }
    /** Mot prononcé (phrase tapée), -1 pendant les silences ou sans phrase tapée. */
    int currentWord() const noexcept
    {
        if (script == nullptr || ph < 0 || ph >= script->count) return -1;
        const auto& p = script->ph[(size_t) ph];
        return p.kind == 4 ? -1 : (int) p.word;
    }
    /** Le début de la phrase (départ du séquenceur). */
    void restartScript() noexcept
    {
        ph = 0; phT = 0.0; ctl = 0;
        sv = sn = sb = 0.0; sNasal = 0.0; sg1 = sg2 = sg3 = 0.0; sp = 0.0;
        if (script != nullptr) { const auto& p = script->ph[0]; sf1 = p.f1; sf2 = p.f2; sf3 = p.f3; sNf = p.nf; }
        for (auto& f : sform) f.reset();
        nasalF.reset(); noiseF.reset();
    }

    /** rate : syllabes par seconde ; pitchHz : hauteur de la voix ; expr : ampleur de l'intonation
        de la phrase tapée (1 = normale, 0 = recto tono, 2 = très chantante ; v0.39). */
    double next (double rate, double pitchHz, double expr = 1.0) noexcept
    {
        if (script != nullptr) return nextScript (rate, pitchHz, expr);
        static constexpr double vowels[5][3] {
            { 730.0, 1090.0, 2440.0 },   // a
            { 530.0, 1840.0, 2480.0 },   // è
            { 270.0, 2290.0, 3010.0 },   // i
            { 570.0, 840.0, 2410.0 },    // o
            { 300.0, 870.0, 2240.0 },    // ou
        };
        // phrase de 8 syllabes : voyelle, consonne (0 aucune, 1 « s », 2 « t », 3 « m »), intonation
        static constexpr int seqV[8] { 0, 3, 1, 4, 2, 0, 3, 1 };
        static constexpr int seqC[8] { 1, 2, 3, 0, 1, 2, 0, 3 };
        static constexpr double seqP[8] { 0.0, 2.0, -1.0, 3.0, 1.0, 0.0, -2.0, -3.0 };

        phase += std::clamp (rate, 0.2, 12.0) / sr;
        if (phase >= 1.0) { phase -= 1.0; syl = (syl + 1) & 7; }
        const auto v = seqV[syl], c = seqC[syl];
        // formants : glissement de 25 ms vers la voyelle
        const auto g = 1.0 - std::exp (-1.0 / (0.025 * sr));
        f1 += (vowels[v][0] - f1) * g; f2 += (vowels[v][1] - f2) * g; f3 += (vowels[v][2] - f3) * g;

        // enveloppe de la syllabe : consonne (18 %), voyelle, petite pause en fin
        const bool cons = phase < 0.18;
        const bool pause = phase > 0.9;
        const auto targetAmp = pause ? 0.0 : (cons ? (c == 3 ? 0.5 : 0.15) : 1.0);
        amp += (targetAmp - amp) * (1.0 - std::exp (-1.0 / (0.008 * sr)));

        // source glottale : scie adoucie, léger vibrato, intonation de la phrase
        vib += 5.5 / sr; if (vib >= 1.0) vib -= 1.0;
        const auto hz = std::clamp (pitchHz, 50.0, 500.0) * fastExp2 ((seqP[syl] + 0.15 * fastSin2Pi (vib)) / 12.0);
        const auto glott = osc.next (Osc::saw, hz / sr, 0.5);
        form[0].set (f1, 8.0, sr); form[1].set (f2, 10.0, sr); form[2].set (f3, 12.0, sr);
        // sorties passe-bande ramenées à un gain de 1 au centre (division par Q)
        auto y = form[0].process (glott).band / 8.0 + form[1].process (glott).band * 0.55 / 10.0 + form[2].process (glott).band * 0.3 / 12.0;
        y *= amp;
        // consonnes : « s » (souffle aigu), « t » (salve brève), « m » (déjà dans l'amplitude, voix fermée)
        // v0.40 (retour d'Henrico : « une sorte de charleston, toujours au même endroit ») : le « s » est
        // un souffle en bande (5 à 9 kHz) qui monte et descend en douceur, le « t » une explosion de
        // 8 ms seulement, plus bas — plus de bruit blanc large et sec qui sonnait comme une cymbale
        if (cons && (c == 1 || c == 2))
        {
            if ((sibCtl++ & 15) == 0) sibF.set (c == 1 ? 6200.0 : 3800.0, c == 1 ? 1.6 : 1.1, sr);
            const auto secs = phase / std::max (0.2, std::clamp (rate, 0.2, 12.0)); // temps depuis le début de la syllabe
            const auto env = c == 2 ? std::exp (-secs / 0.008) * 0.55 : std::pow (std::sin (kPi * phase / 0.18), 2.0) * 0.38;
            y += sibF.process (noise.next()).band / (c == 1 ? 1.6 : 1.1) * env;
        }
        return y * 1.6;
    }

    int syllable() const noexcept { return syl; }

private:
    /** Phrase tapée : synthèse à formants phonème par phonème (v0.33).
        Source glottale (scie adoucie) dans trois résonances qui glissent d'un son à l'autre
        (coarticulation de 20 ms), résonance nasale à 250 Hz, bruit filtré (fricatives, « r »),
        plosives en deux temps : occlusion (silence ou murmure grave) puis explosion qui retombe.
        Les coefficients des filtres sont recalculés tous les 16 échantillons (0,3 ms). */
    double nextScript (double rate, double pitchHz, double expr) noexcept
    {
        const auto& sc = *script;
        // une unité ≈ une syllabe au débit choisi (un peu plus vif que la démonstration)
        phT += std::clamp (rate, 0.2, 12.0) * 1.3 / sr;
        while (phT >= (double) sc.ph[(size_t) ph].dur)
        {
            phT -= (double) sc.ph[(size_t) ph].dur;
            if (++ph >= sc.count) ph = 0; // la phrase tourne en boucle (le point final fait la pause)
        }
        const auto& p = sc.ph[(size_t) ph];
        const auto x = phT / std::max (0.01, (double) p.dur); // avancement dans le phonème (0..1)

        // cibles : voix, bruit, explosion
        double tv = p.voice * p.amp, tn = p.noise * p.amp, burst = 0.0;
        if (p.kind == 3)
        {
            // plosive : 55 % d'occlusion (murmure seul pour b, d, g), puis explosion qui retombe
            if (x < 0.55) { tn = 0.0; tv = p.voice * p.amp * 0.6; }
            else
            {
                // v0.40 : explosion brève (constante de 7 ms, quel que soit le débit) — une explosion
                // longue sonnait comme un coup de charleston à chaque « t », « k », « p »
                const auto secs = (x - 0.55) * (double) p.dur / (std::clamp (rate, 0.2, 12.0) * 1.3);
                burst = p.noise * p.amp * 0.85 * std::exp (-secs / 0.007); tn = 0.0; tv = p.voice * p.amp;
            }
        }
        const auto ga = 1.0 - std::exp (-1.0 / (0.006 * sr)); // amplitudes : 6 ms
        sv += (tv - sv) * ga;
        sn += (tn - sn) * ga;
        sb = burst; // explosion : immédiate

        if ((ctl++ & 15) == 0)
        {
            // formants : glissement de 20 ms (calculé pour 16 échantillons)
            const auto gf = 1.0 - std::exp (-16.0 / (0.020 * sr));
            if (p.kind != 4 && p.kind != 3 && p.voice > 0.0f)
            {
                sf1 += ((double) p.f1 - sf1) * gf; sf2 += ((double) p.f2 - sf2) * gf; sf3 += ((double) p.f3 - sf3) * gf;
            }
            else if (p.kind == 3) { sf1 += ((double) p.f1 - sf1) * gf * 0.5; sf2 += ((double) p.f2 - sf2) * gf * 0.5; }
            const auto gg = 1.0 - std::exp (-16.0 / (0.012 * sr));
            sg1 += ((double) p.g1 - sg1) * gg; sg2 += ((double) p.g2 - sg2) * gg; sg3 += ((double) p.g3 - sg3) * gg;
            sNasal += ((double) p.nasal - sNasal) * gg;
            sNf += ((double) p.nf - sNf) * gg * 2.0;
            const auto gp = 1.0 - std::exp (-16.0 / (0.040 * sr));
            sp += ((double) p.pitch * std::clamp (expr, 0.0, 2.0) - sp) * gp;
            const auto nyq = 0.45 * sr;
            sform[0].set (std::min (sf1, nyq), 8.0, sr); sform[1].set (std::min (sf2, nyq), 10.0, sr); sform[2].set (std::min (sf3, nyq), 12.0, sr);
            nasalF.set (250.0, 4.0, sr);
            noiseF.set (std::min (sNf, nyq), std::max (0.4, (double) p.nq), sr);
        }

        // source glottale : scie, léger vibrato, intonation de la phrase
        vib += 5.0 / sr; if (vib >= 1.0) vib -= 1.0;
        const auto hz = std::clamp (pitchHz, 50.0, 500.0) * fastExp2 ((sp + 0.12 * fastSin2Pi (vib)) / 12.0);
        const auto glott = osc.next (Osc::saw, hz / sr, 0.5) * sv;
        auto y = sform[0].process (glott).band / 8.0 * sg1 + sform[1].process (glott).band / 10.0 * sg2 + sform[2].process (glott).band / 12.0 * sg3;
        y += nasalF.process (glott).band / 4.0 * sNasal * 0.8;
        // bruit : fricatives (continu) et explosions des plosives
        const auto nAmt = sn + sb;
        if (nAmt > 1.0e-4) y += noiseF.process (noise.next()).band * nAmt * 0.9;
        else noiseF.process (0.0);
        return y * 1.6;
    }

    const grv::engine::SpeechScript* script = nullptr;
    int ph = 0;
    unsigned ctl = 0;
    double phT = 0.0, sv = 0.0, sn = 0.0, sb = 0.0, sNasal = 0.0, sg1 = 0.0, sg2 = 0.0, sg3 = 0.0, sp = 0.0;
    double sf1 = 500.0, sf2 = 1500.0, sf3 = 2500.0, sNf = 5000.0;
    std::array<grv::dsp::Svf, 3> sform;
    grv::dsp::Svf nasalF, noiseF;

    double sr = 48000.0, phase = 0.0, f1 = 730.0, f2 = 1090.0, f3 = 2440.0, amp = 0.0, vib = 0.0;
    int syl = 0;
    Osc osc;
    std::array<grv::dsp::Svf, 3> form;
    OnePole hs, hs2;
    grv::dsp::Svf sibF; // v0.40 : souffle des consonnes de la phrase de démonstration
    unsigned sibCtl = 0;
    WhiteNoise noise { 0x7A1C3u };
};

} // namespace grv::synth
