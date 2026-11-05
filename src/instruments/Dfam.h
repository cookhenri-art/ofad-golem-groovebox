/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Percussion à séquenceur » — principe des synthétiseurs de percussion analogiques semi-modulaires
    à séquenceur 8 pas (architecture reprise, ni nom, ni dessin, ni son d'origine) :
      - deux VCO triangle / carré, FM linéaire de l'oscillateur 1 vers le 2, synchro forcée ;
      - mélangeur : VCO 1, bruit blanc, VCO 2 ;
      - filtre en échelle 4 pôles commutable passe-bas / passe-haut, résonance, modulation par le bruit ;
      - trois générateurs d'enveloppe à déclin seul : hauteur des VCO (quantités bipolaires, une
        par oscillateur, déclin commun), filtre (quantité bipolaire), VCA (attaque rapide ou lente) ;
        un coup plus faible ne coupe pas la queue d'un coup plus fort (chaque enveloppe repart du
        plus haut des deux niveaux, comme le condensateur de la machine) ;
      - SÉQUENCEUR 8 PAS : une rangée de hauteurs (vers VCO 1 et 2, VCO 2 seul ou rien) et une
        rangée de vélocités, qui dosent les trois enveloppes. Il avance d'un pas à chaque note
        reçue (séquence de la groovebox, pads, clavier), comme l'entrée d'avance de la machine ;
        il repart du premier pas au lancement de la lecture. Mode « Note jouée » : la hauteur suit
        la note, la vélocité suit le jeu.
    Monophonique, sans allocation dans le rendu.
*/

#pragma once

#include "../synth/SynthCore.h"
#include "../dsp/DrumVoices.h"
#include "../engine/Module.h"
#include "FactoryBanks.h"

#include <atomic>
#include <map>

namespace grv::instruments
{

using namespace grv::engine;

namespace dfam
{
    enum Slot
    {
        seqMode = 0, seqLen, pitchRange, pitchTo,
        vcoDecay, vco1Eg, vco1Freq, vco1Wave, fmAmt, vco2Eg, vco2Freq, vco2Wave, sync,
        mix1, mixNoise, mix2,
        vcfMode, cutoff, reso, vcfEg, vcfDecay, noiseVcf,
        vcaAttack, vcaDecay, volume, velSens, drift,
        stepPitch,                 // 8 hauteurs (-100..100 %)
        stepVel = stepPitch + 8,   // 8 vélocités (0..100 %)
        numSlots = stepVel + 8
    };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> waves { "Triangle", "Carr\xC3\xA9" };
            ModuleDesc m;
            m.type = "dfam";
            m.name = "Kick Killer";
            m.family = "Batteries";
            m.principle = "Percussion analogique semi-modulaire : 2 VCO, bruit, filtre en \xC3\xA9" "chelle, 3 enveloppes \xC3\xA0 d\xC3\xA9" "clin, s\xC3\xA9quenceur 8 pas hauteur et v\xC3\xA9locit\xC3\xA9";
            m.params = {
                C ("seqmode", "S\xC3\xA9quence", { "8 pas (avance \xC3\xA0 chaque note)", "Note jou\xC3\xA9" "e" }),
                I ("seqlen", "Longueur", "pas", 1, 8, 8),
                P ("prange", "\xC3\x89tendue des hauteurs", "dt", 0, 48, 24),
                C ("pitchto", "Hauteurs vers", { "VCO 1 et 2", "Arr\xC3\xAAt", "VCO 2" }),
                P ("vcodecay", "D\xC3\xA9" "clin VCO", "ms", 5, 4000, 120, 0.3f),
                P ("vco1eg", "VCO 1 env.", "%", -100, 100, 30),
                P ("vco1freq", "VCO 1 fr\xC3\xA9quence", "Hz", 15, 5000, 110, 0.3f),
                C ("vco1wave", "VCO 1 forme", waves),
                P ("fm", "FM 1 \xE2\x86\x92 2", "%", 0, 100, 0),
                P ("vco2eg", "VCO 2 env.", "%", -100, 100, 0),
                P ("vco2freq", "VCO 2 fr\xC3\xA9quence", "Hz", 15, 5000, 220, 0.3f),
                C ("vco2wave", "VCO 2 forme", waves, 1),
                C ("sync", "Synchro 2 sur 1", { "Non", "Oui" }),
                P ("mix1", "Niveau VCO 1", "%", 0, 100, 80),
                P ("mixnoise", "Niveau bruit", "%", 0, 100, 0),
                P ("mix2", "Niveau VCO 2", "%", 0, 100, 0),
                C ("vcfmode", "Filtre", { "Passe-bas", "Passe-haut" }),
                P ("cutoff", "Coupure", "Hz", 20, 20000, 1200, 0.25f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 15),
                P ("vcfeg", "VCF env.", "%", -100, 100, 40),
                P ("vcfdecay", "D\xC3\xA9" "clin VCF", "ms", 5, 4000, 180, 0.3f),
                P ("noisevcf", "Bruit \xE2\x86\x92 VCF", "%", 0, 100, 0),
                C ("vcaattack", "Attaque VCA", { "Rapide", "Lente" }),
                P ("vcadecay", "D\xC3\xA9" "clin VCA", "ms", 5, 4000, 350, 0.3f),
                P ("volume", "Volume", "dB", -24, 6, -6),
                P ("velsens", "Sensibilit\xC3\xA9 au jeu", "%", 0, 100, 30),
                P ("drift", "D\xC3\xA9rive", "%", 0, 100, 20),
            };
            for (int k = 0; k < 8; ++k) m.params.push_back (P ("p" + std::to_string (k + 1), "Hauteur " + std::to_string (k + 1), "%", -100, 100, 0));
            for (int k = 0; k < 8; ++k) m.params.push_back (P ("v" + std::to_string (k + 1), "V\xC3\xA9locit\xC3\xA9 " + std::to_string (k + 1), "%", 0, 100, 100));
            m.sections = {
                { "vco", "Oscillateurs", "osc", { "vco1freq", "vco1wave", "vco1eg", "fm", "vco2freq", "vco2wave", "vco2eg", "sync" }, "osc" },
                { "mix", "M\xC3\xA9langeur", "global", { "mix1", "mixnoise", "mix2", "vcodecay", "volume", "velsens", "drift" }, "global" },
                { "vcf", "Filtre", "filter", { "vcfmode", "cutoff", "reso", "vcfeg", "vcfdecay", "noisevcf" }, "filter" },
                { "vca", "VCA", "amp", { "vcaattack", "vcadecay", "volume" }, "amp" },
                { "seqp", "S\xC3\xA9quenceur : hauteurs", "global", { "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8" }, "global" },
                { "seqv", "S\xC3\xA9quenceur : v\xC3\xA9locit\xC3\xA9s", "global", { "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8" }, "global" },
                { "seq", "S\xC3\xA9quence", "global", { "seqmode", "seqlen", "prange", "pitchto" }, "global" },
            };
            auto steps = [] (std::initializer_list<float> pitch, std::initializer_list<float> vel)
            {
                std::vector<std::pair<std::string, float>> out;
                int k = 0;
                for (auto x : pitch) out.push_back ({ "p" + std::to_string (++k), x });
                k = 0;
                for (auto x : vel) out.push_back ({ "v" + std::to_string (++k), x });
                return out;
            };
            auto with = [] (std::vector<std::pair<std::string, float>> a, std::initializer_list<std::pair<const char*, float>> b)
            {
                std::map<std::string, float> v (a.begin(), a.end());
                for (const auto& [id, x] : b) v[id] = x;
                return std::vector<std::pair<std::string, float>> (v.begin(), v.end());
            };
            const std::string PC = "Percussions", SQ = "S\xC3\xA9quences", B = "Basses", FX = "Effets";
            m.presets = {
                { "Init", with (steps ({ 0, 0, 0, 0, 0, 0, 0, 0 }, { 100, 100, 100, 100, 100, 100, 100, 100 }), { { "volume", 1.8f }, { "volume", 1.3f } }), "Init" },
                { "Grosse caisse profonde", with (steps ({ 0, 0, 0, 0, 0, 0, 0, 0 }, { 100, 25, 60, 25, 100, 25, 70, 40 }),
                    { { "volume", 0.8f }, { "vco1freq", 45 }, { "vco1eg", 45 }, { "vcodecay", 55 }, { "mix1", 100 }, { "cutoff", 500 }, { "reso", 10 }, { "vcfeg", 30 },
                      { "vcfdecay", 120 }, { "vcadecay", 520 } }), PC },
                { "Tom qui chante", with (steps ({ 0, 30, -20, 50, 0, 70, -40, 20 }, { 100, 70, 85, 60, 100, 75, 90, 65 }),
                    { { "volume", 1.4f }, { "vco1freq", 120 }, { "vco1eg", 25 }, { "vcodecay", 110 }, { "prange", 12 }, { "cutoff", 2000 }, { "vcfeg", 25 }, { "vcadecay", 380 } }), PC },
                { "Caisse claire bruit\xC3\xA9" "e", with (steps ({ 0, 0, 0, 0, 0, 0, 0, 0 }, { 60, 40, 100, 40, 60, 40, 100, 55 }),
                    { { "volume", 5.2f }, { "vco1freq", 190 }, { "vco1eg", 20 }, { "vcodecay", 40 }, { "mix1", 60 }, { "mixnoise", 100 }, { "cutoff", 5000 }, { "vcfeg", 35 },
                      { "vcfdecay", 120 }, { "vcadecay", 190 } }), PC },
                { "Charleys m\xC3\xA9talliques", with (steps ({ 0, 10, 0, 20, 0, 10, 0, 30 }, { 100, 40, 70, 40, 100, 40, 70, 55 }),
                    { { "volume", 6.0f }, { "vco1freq", 2800 }, { "vco1wave", 1 }, { "vco2freq", 4300 }, { "vco2wave", 1 }, { "fm", 65 }, { "mix1", 50 }, { "mix2", 50 }, { "mixnoise", 45 },
                      { "vcfmode", 1 }, { "cutoff", 6000 }, { "reso", 30 }, { "vcfeg", 0 }, { "vcaattack", 0 }, { "vcadecay", 70 }, { "vco1eg", 0 }, { "prange", 6 } }), PC },
                { "Basse s\xC3\xA9quenc\xC3\xA9" "e", with (steps ({ 0, 0, 25, 0, -25, 0, 58, 33 }, { 100, 60, 90, 50, 100, 70, 85, 60 }),
                    { { "volume", -3.5f }, { "vco1freq", 55 }, { "vco1wave", 1 }, { "vco2freq", 55.4f }, { "mix2", 70 }, { "vco1eg", 0 }, { "cutoff", 260 }, { "reso", 60 },
                      { "vcfeg", 60 }, { "vcfdecay", 200 }, { "vcadecay", 260 }, { "prange", 12 } }), B },
                { "Bulles FM", with (steps ({ 80, -30, 40, -60, 20, 90, -10, 50 }, { 100, 60, 80, 50, 90, 70, 100, 60 }),
                    { { "volume", 0.0f }, { "vco1freq", 300 }, { "vco2freq", 800 }, { "fm", 80 }, { "vco2eg", 50 }, { "sync", 1 }, { "mix1", 0 }, { "mix2", 90 },
                      { "cutoff", 3000 }, { "reso", 35 }, { "vcfeg", 20 }, { "vcadecay", 180 }, { "vcodecay", 160 } }), SQ },
                { "Percussion techno", with (steps ({ 0, 40, 0, 70, 0, 40, 100, 70 }, { 100, 50, 80, 50, 100, 50, 90, 70 }),
                    { { "volume", 5.0f }, { "vco1freq", 160 }, { "vco1eg", 35 }, { "vcodecay", 70 }, { "mixnoise", 20 }, { "cutoff", 1500 }, { "reso", 45 }, { "vcfeg", 45 },
                      { "vcfdecay", 90 }, { "vcadecay", 160 }, { "prange", 18 } }), SQ },
                { "R\xC3\xA9sonance qui crie", with (steps ({ 0, 50, 20, 80, 0, 50, 30, 100 }, { 100, 80, 90, 70, 100, 80, 90, 75 }),
                    { { "volume", 4.7f }, { "vco1freq", 70 }, { "mixnoise", 25 }, { "cutoff", 180 }, { "reso", 90 }, { "vcfeg", 75 }, { "vcfdecay", 260 }, { "vcadecay", 300 },
                      { "prange", 12 }, { "pitchto", 1 } }), SQ },
                { "Bips", with (steps ({ 0, 100, 50, 25, 75, 0, 100, 50 }, { 100, 70, 90, 60, 100, 80, 90, 70 }),
                    { { "volume", 1.4f }, { "vco1freq", 700 }, { "vco1wave", 1 }, { "vco1eg", 0 }, { "cutoff", 5000 }, { "vcfeg", 0 }, { "vcadecay", 70 }, { "prange", 36 } }), SQ },
                { "Souffle lent", with (steps ({ 0, 0, 0, 0, 0, 0, 0, 0 }, { 100, 50, 80, 50, 100, 50, 80, 50 }),
                    { { "volume", 0.5f }, { "vcaattack", 1 }, { "vcadecay", 1600 }, { "mixnoise", 70 }, { "mix1", 20 }, { "cutoff", 700 }, { "reso", 55 }, { "vcfeg", 45 },
                      { "vcfdecay", 900 }, { "vco1eg", 0 } }), FX },
                { "Tom accord\xC3\xA9 (note jou\xC3\xA9" "e)", with (steps ({ 0, 0, 0, 0, 0, 0, 0, 0 }, { 100, 100, 100, 100, 100, 100, 100, 100 }),
                    { { "volume", 1.6f }, { "seqmode", 1 }, { "vco1freq", 130 }, { "vco1eg", 20 }, { "vcodecay", 90 }, { "cutoff", 2500 }, { "vcfeg", 20 }, { "vcadecay", 420 }, { "velsens", 80 } }), PC },
            };
            addFactoryBank (m); // v0.47 (maj. 5 nov.) : banque d'usine (au moins 90 sons par synthé)
            return m;
        }();
        return d;
    }
}

class DfamSynth final : public Instrument
{
public:
    DfamSynth() : noise (0xDFA3u), drift (0xDFA1u) {}
    const ModuleDesc& desc() const noexcept override { return dfam::desc(); }

    void prepare (double s, int) override { sr = s > 0.0 ? s : 48000.0; dc.prepare (sr, 10.0); reset(); }
    void reset() noexcept override
    {
        o1.reset(); o2.reset (0.25); ladder.reset(); dc.reset(); outClip.reset();
        eVco = eVcf = eVca = 0.0; vcaTarget = 0.0; attacking = false; step = -1;
        shownStep.store (-1, std::memory_order_relaxed);
    }
    void transportStarted() noexcept override { step = -1; }   // la séquence repart du pas 1
    void transportStopped() noexcept override { step = -1; }

    void noteOn (int note, float velocity) noexcept override
    {
        pendingNote = note; pendingVel = velocity; pending = true;
    }
    void noteOff (int) noexcept override {}
    void allNotesOff() noexcept override { eVca *= 0.0; eVco = eVcf = 0.0; attacking = false; }
    bool isActive() const noexcept override { return eVca > 1.0e-5 || attacking || pending; }

    /** Écran : pas joué (-1 au repos). */
    int status (float* out, int max) const noexcept override
    {
        if (max < 1) return 0;
        out[0] = (float) shownStep.load (std::memory_order_relaxed);
        return 1;
    }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        using namespace dfam;
        if (pending) { pending = false; trigger (p, pendingNote, pendingVel); }
        if (! isActive()) return;
        const auto vcoCoef = dsp::decay60Coef (p[vcoDecay], sr), vcfCoef = dsp::decay60Coef (p[vcfDecay], sr);
        const auto vcaCoef = dsp::decay60Coef (p[vcaDecay], sr);
        const auto atkInc = 1.0 / std::max (1.0, (p[vcaAttack] > 0.5f ? 0.035 : 0.0006) * sr);
        const auto w1 = p[vco1Wave] > 0.5f ? synth::Osc::pulse : synth::Osc::triangle;
        const auto w2 = p[vco2Wave] > 0.5f ? synth::Osc::pulse : synth::Osc::triangle;
        const bool doSync = p[sync] > 0.5f;
        const auto to = (int) p[pitchTo];
        const auto off1 = to == 0 ? pitchOffset : 0.0, off2 = to != 1 ? pitchOffset : 0.0;
        const auto dr = drift.next (p[dfam::drift] / 100.0);
        const auto f1 = (double) p[vco1Freq] * dsp::fastExp2 ((off1 + dr) / 12.0), f2 = (double) p[vco2Freq] * dsp::fastExp2 ((off2 - 0.7 * dr) / 12.0);
        const auto eg1 = p[vco1Eg] / 100.0 * 48.0, eg2 = p[vco2Eg] / 100.0 * 48.0;
        const auto fm = p[fmAmt] / 100.0;
        const auto m1 = p[mix1] / 100.0, mn = p[mixNoise] / 100.0, m2 = p[mix2] / 100.0;
        const auto mode = p[vcfMode] > 0.5f ? synth::Ladder::hp24 : synth::Ladder::lp24;
        const auto res = p[reso] / 100.0;
        const auto vcfOct = p[vcfEg] / 100.0 * 7.0;
        const auto noiseOct = p[noiseVcf] / 100.0 * 3.0;
        const auto gain = dsp::dbToGain (p[volume] + 9.0); // réserve : les enveloppes à déclin seul et l'échelle donnent un niveau bas
        for (int i = 0; i < n; ++i)
        {
            const auto ev = eVco, ef = eVcf;
            eVco *= vcoCoef; eVcf *= vcfCoef;
            if (attacking) { eVca += atkInc * vcaTarget; if (eVca >= vcaTarget) { eVca = vcaTarget; attacking = false; } }
            else eVca *= vcaCoef;
            double wrap = -1.0;
            const auto dt1 = std::min (0.45, f1 * dsp::fastExp2 (eg1 * ev / 12.0) / sr);
            const auto a = o1.next (w1, dt1, 0.5, -1.0, &wrap);
            const auto dt2 = std::clamp (f2 * dsp::fastExp2 (eg2 * ev / 12.0) * (1.0 + 2.5 * fm * a) / sr, 1.0e-6, 0.45);
            const auto b = o2.next (w2, dt2, 0.5, doSync ? wrap : -1.0);
            const auto nz = noise.next();
            const auto x = a * m1 + b * m2 + nz * mn * 0.8;
            const auto fc = std::clamp ((double) p[cutoff] * dsp::fastExp2 (vcfOct * ef + noiseOct * nz), 20.0, 0.45 * sr);
            const auto y = ladder.process (x * 0.7, fc, res, sr, mode, 1.0);
            // étage de sortie : saturation douce (transparente aux niveaux normaux, crêtes tenues sous 2)
            const auto out = (float) (2.0 * outClip.process (dc.process (y) * eVca * gain * 0.5));
            L[i] += out; R[i] += out;
        }
    }

private:
    /** Un coup : avance du séquenceur, niveaux des enveloppes (le plus haut de l'ancien et du nouveau). */
    void trigger (const float* p, int note, float velocity) noexcept
    {
        using namespace dfam;
        const auto sens = p[velSens] / 100.0;
        const auto play = 1.0 - sens * (1.0 - std::clamp ((double) velocity, 0.0, 1.0));
        double vel = play;
        if (p[seqMode] < 0.5f)
        {
            const auto len = std::clamp ((int) std::lround (p[seqLen]), 1, 8);
            step = (step + 1) % len;
            pitchOffset = p[stepPitch + step] / 100.0 * p[pitchRange];
            vel *= p[stepVel + step] / 100.0;
            shownStep.store (step, std::memory_order_relaxed);
        }
        else
        {
            pitchOffset = (double) (note - 60);
            shownStep.store (-1, std::memory_order_relaxed);
        }
        // v0.30 : quand le coup précédent s'est éteint, les deux VCO repartent du même point —
        // l'attaque est la même à chaque coup (sur la machine d'origine ils tournent librement,
        // d'où un clic d'attaque qui change d'un coup à l'autre) ; sur un son encore fort, ils continuent
        if (eVca < 0.03 && ! attacking) { o1.reset(); o2.reset(); }
        eVco = std::max (eVco, vel);
        eVcf = std::max (eVcf, vel);
        if (vel > eVca) { vcaTarget = vel; attacking = true; }
    }

    /** Dérive lente des oscillateurs (marche aléatoire bornée, en demi-tons). */
    struct Drift
    {
        explicit Drift (std::uint32_t seed) : rng (seed) {}
        double next (double amount) noexcept
        {
            rng = rng * 1664525u + 1013904223u;
            walk = std::clamp (walk * 0.995 + 0.02 * ((double) (rng >> 8) / 8388608.0 - 1.0), -1.0, 1.0);
            return walk * amount * 0.15;
        }
        std::uint32_t rng;
        double walk = 0.0;
    };

    double sr = 48000.0, eVco = 0.0, eVcf = 0.0, eVca = 0.0, vcaTarget = 0.0, pitchOffset = 0.0;
    bool attacking = false, pending = false;
    int step = -1, pendingNote = 60;
    float pendingVel = 1.0f;
    std::atomic<int> shownStep { -1 };
    synth::Osc o1, o2;
    synth::Ladder ladder;
    dsp::WhiteNoise noise;
    dsp::DcBlocker dc;
    dsp::AdaaSoftClip outClip;
    Drift drift;
};

} // namespace grv::instruments
