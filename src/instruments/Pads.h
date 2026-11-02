/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Nappes » — principe des synthétiseurs virtuels-analogiques à parties A/B très utilisés pour
    les nappes et les leads de musique électronique (architecture reprise, ni nom, ni panneau, ni
    son d'origine) :
      - deux parties A et B de deux oscillateurs chacune (A1, A2, B1, B2) : forme (dont impulsions
        étroites et tri-scie), octave, note, fin, unisson jusqu'à 8 voix désaccordées et étalées,
        phase libre ou fixée, inversion, panoramique, volume ;
      - un filtre par partie (passe-bas 12/24, bande, haut, contourné), saturation, et une commande
        globale de coupure / résonance qui agit sur les deux, « warm drive » ;
      - une enveloppe d'ampli par partie, deux enveloppes de modulation, deux LFO ;
      - matrice de modulation (16 routages), arpège à pas (transposition, vélocité) ;
      - chorus et largeur stéréo intégrés. 8 notes de polyphonie.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace pads
{
    enum Slot
    {
        voices = 0, glide, level, velAmp, tune, width, chorusMix, chorusRate,
        aWave, aOct, aSemi, aFine, aUnison, aDetune, aSpread, aLevel,
        bWave, bOct, bSemi, bFine, bUnison, bDetune, bSpread, bLevel,
        faType, faCutoff, faReso, faDrive, fbType, fbCutoff, fbReso, keytrack,
        aA, aD, aS, aR, mA, mD, mS, mR, modCutoffA, modCutoffB, modPitch,
        l1Rate, l1Shape, l1Pitch, l1Pan, l2Rate, l2Shape, l2Cutoff,
        l1Sync, l2Sync, // v0.9 : synchro au tempo
        // v0.10 : phase, inversion, panoramique ; oscillateurs A2 et B2 ; ampli B ; env. mod 2 ;
        //         commande globale des filtres ; mode de voix
        aPhase, aInv, aPan, aRetrig, bPhase, bInv, bPan, bRetrig,
        a2Wave, a2Oct, a2Semi, a2Fine, a2Unison, a2Detune, a2Spread, a2Level, a2Phase, a2Inv, a2Pan, a2Retrig,
        b2Wave, b2Oct, b2Semi, b2Fine, b2Unison, b2Detune, b2Spread, b2Level, b2Phase, b2Inv, b2Pan, b2Retrig,
        bA, bD, bS, bR, m2A, m2D, m2S, m2R,
        fbDrive, fcCutoff, fcReso, warmDrive, vMode, bEnvOwn,
        l1Delay, l2Delay, // v0.13.2 : retard des LFO (depuis la première note d'une phrase)
        numSlots
    };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> waves { "Sinus", "Triangle", "Scie", "Carr\xC3\xA9", "Impulsion", "Bruit", "Impulsion \xC3\xA9troite", "Tri-scie" };
            const std::vector<std::string> ftypes { "Passe-bas 12", "Passe-bas 24", "Passe-bande", "Passe-haut", "Contourn\xC3\xA9" };
            const std::vector<std::string> onOff { "Non", "Oui" };
            const std::vector<std::string> retrig { "Libre", "Fix\xC3\xA9" "e" };
            const std::vector<std::string> lfoShapes { "Sinus", "Triangle", "Scie", "Carr\xC3\xA9", "Al\xC3\xA9" "atoire" };
            ModuleDesc m;
            m.type = "pads";
            m.name = "Bignappe One";
            m.family = "Nappes";
            m.principle = "Virtuel-analogique \xC3\xA0 deux parties, unisson \xC3\xA9pais jusqu'\xC3\xA0 8 voix, filtres par partie, chorus int\xC3\xA9gr\xC3\xA9";
            m.params = {
                I ("voices", "Polyphonie", "", 1, 8, 8),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("level", "Niveau", "dB", -24, 6, -10),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 20),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("width", "Largeur", "%", 0, 200, 120),
                P ("chorusmix", "Chorus", "%", 0, 100, 35),
                P ("chorusrate", "Vitesse chorus", "Hz", 0.05f, 5, 0.4f, 0.5f),

                C ("awave", "Forme A", waves, 2), I ("aoct", "Octave A", "oct", -3, 3, 0), I ("asemi", "Demi-tons A", "dt", -12, 12, 0),
                P ("afine", "Fin A", "ct", -50, 50, 0), I ("aunison", "Unisson A", "", 1, 8, 5), P ("adetune", "D\xC3\xA9saccord A", "%", 0, 100, 30),
                P ("aspread", "\xC3\x89" "talement A", "%", 0, 100, 80), P ("alevel", "Niveau A", "%", 0, 100, 80),
                C ("bwave", "Forme B", waves, 4), I ("boct", "Octave B", "oct", -3, 3, -1), I ("bsemi", "Demi-tons B", "dt", -12, 12, 0),
                P ("bfine", "Fin B", "ct", -50, 50, 4), I ("bunison", "Unisson B", "", 1, 8, 3), P ("bdetune", "D\xC3\xA9saccord B", "%", 0, 100, 20),
                P ("bspread", "\xC3\x89" "talement B", "%", 0, 100, 60), P ("blevel", "Niveau B", "%", 0, 100, 50),

                C ("fatype", "Filtre A", ftypes, 1), P ("facutoff", "Coupure A", "Hz", 20, 18000, 3500, 0.3f),
                P ("fareso", "R\xC3\xA9sonance A", "%", 0, 100, 10), P ("fadrive", "Saturation", "dB", 0, 18, 0),
                C ("fbtype", "Filtre B", ftypes, 1), P ("fbcutoff", "Coupure B", "Hz", 20, 18000, 2500, 0.3f),
                P ("fbreso", "R\xC3\xA9sonance B", "%", 0, 100, 10), P ("keytrack", "Suivi clavier", "%", 0, 100, 30),

                P ("aa", "Attaque", "ms", 0.5f, 10000, 600, 0.3f), P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 1500, 0.3f),
                P ("as", "Maintien", "%", 0, 100, 85), P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 15000, 2500, 0.3f),
                P ("ma", "Attaque", "ms", 0.5f, 10000, 900, 0.3f), P ("md", "D\xC3\xA9" "clin", "ms", 5, 10000, 2500, 0.3f),
                P ("ms", "Maintien", "%", 0, 100, 40), P ("mr", "Rel\xC3\xA2" "chement", "ms", 5, 15000, 2000, 0.3f),
                P ("modcutoffa", "Env. mod. \xE2\x86\x92 coupure A", "%", -100, 100, 25),
                P ("modcutoffb", "Env. mod. \xE2\x86\x92 coupure B", "%", -100, 100, 25),
                P ("modpitch", "Env. mod. \xE2\x86\x92 hauteur", "dt", -12, 12, 0),

                P ("l1rate", "LFO 1 vitesse", "Hz", 0.02f, 20, 4.5f, 0.4f), C ("l1shape", "LFO 1 forme", lfoShapes),
                P ("l1pitch", "LFO 1 \xE2\x86\x92 vibrato", "ct", 0, 100, 0), P ("l1pan", "LFO 1 \xE2\x86\x92 panoramique", "%", 0, 100, 0),
                P ("l2rate", "LFO 2 vitesse", "Hz", 0.02f, 20, 0.15f, 0.4f), C ("l2shape", "LFO 2 forme", lfoShapes, 1),
                P ("l2cutoff", "LFO 2 \xE2\x86\x92 coupures", "%", 0, 100, 15),
                C ("l1sync", "LFO 1 synchro", lfoSyncChoices()), C ("l2sync", "LFO 2 synchro", lfoSyncChoices()),

                P ("aphase", "Phase A1", "\xC2\xB0", 0, 360, 0), C ("ainv", "Inversion A1", onOff), P ("apan", "Panoramique A1", "%", -100, 100, 0), C ("aretrig", "Phase A1", retrig),
                P ("bphase", "Phase B1", "\xC2\xB0", 0, 360, 0), C ("binv", "Inversion B1", onOff), P ("bpan", "Panoramique B1", "%", -100, 100, 0), C ("bretrig", "Phase B1", retrig),
                C ("a2wave", "Forme A2", waves, 2), I ("a2oct", "Octave A2", "oct", -3, 3, 0), I ("a2semi", "Demi-tons A2", "dt", -12, 12, 7),
                P ("a2fine", "Fin A2", "ct", -50, 50, 0), I ("a2unison", "Unisson A2", "", 1, 8, 1), P ("a2detune", "D\xC3\xA9saccord A2", "%", 0, 100, 20),
                P ("a2spread", "\xC3\x89" "talement A2", "%", 0, 100, 60), P ("a2level", "Niveau A2", "%", 0, 100, 0),
                P ("a2phase", "Phase A2", "\xC2\xB0", 0, 360, 0), C ("a2inv", "Inversion A2", onOff), P ("a2pan", "Panoramique A2", "%", -100, 100, 0), C ("a2retrig", "Phase A2", retrig),
                C ("b2wave", "Forme B2", waves, 2), I ("b2oct", "Octave B2", "oct", -3, 3, 0), I ("b2semi", "Demi-tons B2", "dt", -12, 12, 0),
                P ("b2fine", "Fin B2", "ct", -50, 50, -6), I ("b2unison", "Unisson B2", "", 1, 8, 1), P ("b2detune", "D\xC3\xA9saccord B2", "%", 0, 100, 20),
                P ("b2spread", "\xC3\x89" "talement B2", "%", 0, 100, 60), P ("b2level", "Niveau B2", "%", 0, 100, 0),
                P ("b2phase", "Phase B2", "\xC2\xB0", 0, 360, 0), C ("b2inv", "Inversion B2", onOff), P ("b2pan", "Panoramique B2", "%", -100, 100, 0), C ("b2retrig", "Phase B2", retrig),
                P ("ba", "Attaque", "ms", 0.5f, 10000, 600, 0.3f), P ("bd", "D\xC3\xA9" "clin", "ms", 5, 10000, 1500, 0.3f),
                P ("bs", "Maintien", "%", 0, 100, 85), P ("br", "Rel\xC3\xA2" "chement", "ms", 5, 15000, 2500, 0.3f),
                P ("m2a", "Attaque", "ms", 0.5f, 10000, 5, 0.3f), P ("m2d", "D\xC3\xA9" "clin", "ms", 5, 10000, 600, 0.3f),
                P ("m2s", "Maintien", "%", 0, 100, 0), P ("m2r", "Rel\xC3\xA2" "chement", "ms", 5, 15000, 600, 0.3f),
                P ("fbdrive", "Saturation B", "dB", 0, 18, 0),
                P ("fccutoff", "Coupure globale", "%", -100, 100, 0), P ("fcreso", "R\xC3\xA9sonance globale", "%", -100, 100, 0),
                C ("warmdrive", "Warm drive", onOff),
                C ("vmode", "Mode de voix", { "Poly", "Mono", "Mono legato" }),
                C ("benvown", "Ampli B", { "Comme l'ampli A", "Propre" }),
                C ("l1delay", "LFO 1 retard", lfoDelayChoices()), C ("l2delay", "LFO 2 retard", lfoDelayChoices()),
            };
            m.sections = {
                { "global", "Global", "global", { "vmode", "voices", "glide", "tune", "velamp", "width", "chorusmix", "chorusrate", "level" }, "global" },
                { "parta", "Oscillateur A1", "osc", { "awave", "aoct", "asemi", "afine", "aunison", "adetune", "aspread", "alevel" }, "osc" },
                { "parta1x", "A1 : phase et place", "osc", { "aretrig", "aphase", "ainv", "apan" }, "osc" },
                { "parta2", "Oscillateur A2", "osc", { "a2wave", "a2oct", "a2semi", "a2fine", "a2unison", "a2detune", "a2spread", "a2level" }, "osc" },
                { "parta2x", "A2 : phase et place", "osc", { "a2retrig", "a2phase", "a2inv", "a2pan" }, "osc" },
                { "partb", "Oscillateur B1", "osc", { "bwave", "boct", "bsemi", "bfine", "bunison", "bdetune", "bspread", "blevel" }, "osc" },
                { "partb1x", "B1 : phase et place", "osc", { "bretrig", "bphase", "binv", "bpan" }, "osc" },
                { "partb2", "Oscillateur B2", "osc", { "b2wave", "b2oct", "b2semi", "b2fine", "b2unison", "b2detune", "b2spread", "b2level" }, "osc" },
                { "partb2x", "B2 : phase et place", "osc", { "b2retrig", "b2phase", "b2inv", "b2pan" }, "osc" },
                { "filters", "Filtres", "filter", { "fatype", "facutoff", "fareso", "fadrive", "fbtype", "fbcutoff", "fbreso", "fbdrive" }, "filter" },
                { "fctrl", "Commande des filtres", "filter", { "fccutoff", "fcreso", "keytrack", "warmdrive" }, "filter" },
                { "aenv", "Ampli A", "env", { "aa", "ad", "as", "ar", "velamp", "level" }, "amp" },
                { "benv", "Ampli B", "env", { "benvown", "ba", "bd", "bs", "br" }, "amp" },
                { "menv", "Enveloppe de modulation 1", "env", { "ma", "md", "ms", "mr", "modcutoffa", "modcutoffb", "modpitch" }, "env" },
                { "menv2", "Enveloppe de modulation 2", "env", { "m2a", "m2d", "m2s", "m2r" }, "env" },
                { "lfo1", "LFO 1", "lfo", { "l1sync", "l1rate", "l1shape", "l1pitch", "l1pan", "l1delay" }, "mod" },
                { "lfo2", "LFO 2", "lfo", { "l2sync", "l2rate", "l2shape", "l2cutoff", "l2delay" }, "mod" },
            };
            m.presets = {
                { "Nappe large", {} },
                { "Supersaw", { { "aunison", 8 }, { "adetune", 45 }, { "bwave", 2 }, { "bunison", 8 }, { "bdetune", 35 }, { "boct", 0 }, { "aa", 20 }, { "ar", 800 }, { "facutoff", 9000 } } },
                { "Pluck trance", { { "aunison", 6 }, { "aa", 1 }, { "ad", 300 }, { "as", 0 }, { "ar", 400 }, { "ma", 1 }, { "md", 250 }, { "ms", 0 }, { "modcutoffa", 70 }, { "facutoff", 600 }, { "chorusmix", 20 } } },
                { "Choeur doux", { { "awave", 1 }, { "bwave", 0 }, { "aunison", 4 }, { "adetune", 15 }, { "facutoff", 2200 }, { "aa", 1500 }, { "l1pitch", 8 }, { "chorusmix", 60 } } },
                { "Lead \xC3\xA9pais", { { "voices", 1 }, { "glide", 70 }, { "aunison", 7 }, { "adetune", 25 }, { "bwave", 3 }, { "boct", 0 }, { "aa", 2 }, { "as", 100 }, { "ar", 300 }, { "facutoff", 5000 }, { "fareso", 25 } } },
                { "Quatre oscillateurs", { { "a2level", 60 }, { "a2unison", 4 }, { "b2level", 45 }, { "benvown", 1 }, { "b2wave", 7 }, { "b2oct", 1 }, { "aa", 400 }, { "ba", 900 }, { "br", 1800 } } },
                { "Pluck \xC3\xA0 la phase", { { "aretrig", 1 }, { "bretrig", 1 }, { "aunison", 3 }, { "level", -16 }, { "aa", 1 }, { "ad", 250 }, { "as", 0 }, { "ar", 300 },
                                               { "mx1_src", 4 }, { "mx1_dst@fccutoff", 0 }, { "mx1_amt", 60 }, { "m2d", 220 }, { "fccutoff", -40 }, { "chorusmix", 15 } } },
                { "Basse arp\xC3\xA8ge", { { "vmode", 2 }, { "awave", 3 }, { "aunison", 1 }, { "boct", -1 }, { "bwave", 2 }, { "facutoff", 900 }, { "modcutoffa", 55 }, { "ma", 1 }, { "md", 180 }, { "ms", 0 },
                                         { "arp_mode", 1 }, { "arp_rate", 3 }, { "arp_steps", 8 }, { "arp_t3", 12 }, { "arp_t6", 7 }, { "arp_v2", 70 }, { "arp_v4", 60 }, { "chorusmix", 0 } } },
            };
            HubSpec hub;
            hub.extEnvs = { "Ampli A", "Ampli B", "Env. mod 1", "Env. mod 2" };
            hub.extLfos = { "LFO 1", "LFO 2" };
            hub.envs = 0; hub.lfos = 0; hub.slots = 16; hub.arp = true; hub.arpSteps = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m); // banque d'usine : 50 sons par catégories (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

/** LFO communs à toutes les voix (calculés une fois par bloc). */
struct PadsShared { double lfo1 = 0.0, lfo2 = 0.0; };

class PadsVoice
{
public:
    static constexpr int maxUnison = 8;

    void prepare (double s) { sr = s; aenvA.prepare (s); aenvB.prepare (s); menv.prepare (s); menv2.prepare (s); reset(); }
    void reset() noexcept
    {
        for (auto& u : units) { for (auto& o : u.osc) o.reset(); for (auto& o : u.aux) o.reset(); }
        for (auto& f : fA) f.reset();
        for (auto& f : fB) f.reset();
        aenvA.reset(); aenvB.reset(); menv.reset(); menv2.reset(); dcL.reset(); dcR.reset();
        active = false;
    }
    void setSpread (int, int) noexcept {}
    void seed (std::uint32_t s) noexcept { rng = s; }
    void share (const PadsShared* s) noexcept { shared = s; }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; active = true;
    }
    void release() noexcept { aenvA.gate (false); aenvB.gate (false); menv.gate (false); menv2.gate (false); }
    bool isActive() const noexcept { return active; }

    void modSources (double* env, double* lfo) const noexcept
    {
        env[0] = lastA; env[1] = lastB; env[2] = lastM1; env[3] = lastM2;
        lfo[0] = shared ? shared->lfo1 : 0.0; lfo[1] = shared ? shared->lfo2 : 0.0;
    }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace pads;
        const auto lfo1 = shared ? shared->lfo1 : 0.0, lfo2 = shared ? shared->lfo2 : 0.0;
        if (pending)
        {
            pending = false;
            pitch.jump ((double) (p[glide] > 0.5f && from >= 0 ? from : target));
            pitch.to ((double) target);
            if (! legatoPending || ! (aenvA.isActive() || aenvB.isActive()))
            {
                // phase libre : différente à chaque note (l'unisson ne « phase » pas) ; fixée : la même
                static constexpr int retrigSlot[4] { aRetrig, a2Retrig, bRetrig, b2Retrig };
                static constexpr int phaseSlot[4] { aPhase, a2Phase, bPhase, b2Phase };
                for (int u = 0; u < 4; ++u)
                {
                    const bool fixed = p[retrigSlot[u]] > 0.5f;
                    const auto ph0 = p[phaseSlot[u]] / 360.0;
                    for (int k = 0; k < maxUnison; ++k)
                    {
                        rng = rng * 1664525u + 1013904223u;
                        const auto ph = fixed ? ph0 : (double) (rng >> 8) / 16777216.0;
                        units[(size_t) u].osc[(size_t) k].reset (ph - std::floor (ph));
                        units[(size_t) u].aux[(size_t) k].reset (ph - std::floor (ph));
                    }
                }
                aenvA.gate (true); aenvB.gate (true); menv.gate (true); menv2.gate (true);
            }
        }
        aenvA.set (p[aA], p[aD], p[aS] / 100.0, p[aR]);
        // ampli B : le sien, ou le même réglage que l'ampli A
        if (p[bEnvOwn] > 0.5f) aenvB.set (p[bA], p[bD], p[bS] / 100.0, p[bR]);
        else aenvB.set (p[aA], p[aD], p[aS] / 100.0, p[aR]);
        menv.set (p[mA], p[mD], p[mS] / 100.0, p[mR]);
        menv2.set (p[m2A], p[m2D], p[m2S] / 100.0, p[m2R]);
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto gain = dbToGain (p[level]) * velocityGain (vel, p[velAmp] / 100.0);
        const auto vib = lfo1 * p[l1Pitch] / 100.0;
        const auto panMod = lfo1 * p[l1Pan] / 100.0;

        auto part = [&] (int w, int o, int s, int f, int u, int dtn, int spr, int lvl, int inv, int pan)
        {
            Part pt { (int) p[w], std::clamp ((int) p[u], 1, maxUnison), 12.0 * std::round (p[o]) + std::round (p[s]) + p[f] / 100.0,
                      p[dtn] / 100.0, p[spr] / 100.0, p[lvl] / 100.0, {}, {}, {} };
            pt.norm = (p[inv] > 0.5f ? -1.0 : 1.0) / std::sqrt ((double) pt.count);
            const auto centre = p[pan] / 100.0;
            for (int k = 0; k < pt.count; ++k)
            {
                // désaccords symétriques, plus serrés au centre (principe des « super » scies)
                const auto pos = pt.count > 1 ? (2.0 * k / (pt.count - 1) - 1.0) : 0.0;
                const auto cents = pt.detune * 60.0 * pos * (0.6 + 0.4 * std::abs (pos));
                pt.ratio[(size_t) k] = fastExp2 ((pt.offset + cents / 100.0) / 12.0);
                const auto pn = std::clamp (centre + pos * pt.spread, -1.0, 1.0);
                pt.gl[(size_t) k] = pt.norm * (1.0 - std::max (0.0, pn));
                pt.gr[(size_t) k] = pt.norm * (1.0 + std::min (0.0, pn));
            }
            return pt;
        };
        const Part parts[4] {
            part (aWave, aOct, aSemi, aFine, aUnison, aDetune, aSpread, aLevel, aInv, aPan),
            part (a2Wave, a2Oct, a2Semi, a2Fine, a2Unison, a2Detune, a2Spread, a2Level, a2Inv, a2Pan),
            part (bWave, bOct, bSemi, bFine, bUnison, bDetune, bSpread, bLevel, bInv, bPan),
            part (b2Wave, b2Oct, b2Semi, b2Fine, b2Unison, b2Detune, b2Spread, b2Level, b2Inv, b2Pan) };
        const bool useA = parts[0].level > 0.0 || parts[1].level > 0.0, useB = parts[2].level > 0.0 || parts[3].level > 0.0;
        // commande globale : décale les deux coupures (±5 octaves) et les deux résonances
        const auto ktOct = (p[keytrack] / 100.0) * (pitch.get() - 60.0) / 12.0 + lfo2 * p[l2Cutoff] / 100.0 * 2.0 + p[fcCutoff] / 100.0 * 5.0;
        const auto resA = std::clamp ((p[faReso] + p[fcReso]) / 100.0, 0.0, 1.0), resB = std::clamp ((p[fbReso] + p[fcReso]) / 100.0, 0.0, 1.0);
        const bool warm = p[warmDrive] > 0.5f;
        const auto drvA = dbToGain (p[faDrive]) * (warm ? 1.8 : 1.0), drvB = dbToGain (p[fbDrive]) * (warm ? 1.8 : 1.0);
        const int typeA = (int) p[faType], typeB = (int) p[fbType];
        double a = 0.0, b = 0.0, me = 0.0, me2 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            a = aenvA.next(); b = aenvB.next(); me = menv.next(); me2 = menv2.next();
            const auto note = pitch.next (glideCoef) + p[tune] + vib + me * p[modPitch];

            double al = 0.0, ar = 0.0, bl = 0.0, br = 0.0;
            for (int u = 0; u < 2; ++u) if (parts[u].level > 0.0) unison (units[(size_t) u], parts[u], note, al, ar, parts[u].level);
            for (int u = 2; u < 4; ++u) if (parts[u].level > 0.0) unison (units[(size_t) u], parts[u], note, bl, br, parts[u].level);

            std::pair<double, double> ya {}, yb {};
            if (useA)
            {
                const auto fcA = std::clamp ((double) p[faCutoff] * fastExp2 (ktOct + (p[modCutoffA] / 100.0) * 6.0 * me), 16.0, 0.45 * sr);
                ya = filt (fA, typeA, al * drvA, ar * drvA, fcA, resA, drvA > 1.0);
            }
            if (useB)
            {
                const auto fcB = std::clamp ((double) p[fbCutoff] * fastExp2 (ktOct + (p[modCutoffB] / 100.0) * 6.0 * me), 16.0, 0.45 * sr);
                yb = filt (fB, typeB, bl * drvB, br * drvB, fcB, resB, drvB > 1.0);
            }
            const auto outL = dcL.process (ya.first * a + yb.first * b) * gain;
            const auto outR = dcR.process (ya.second * a + yb.second * b) * gain;
            L[i] += (float) (outL * (1.0 - std::max (0.0, panMod)));
            R[i] += (float) (outR * (1.0 + std::min (0.0, panMod)));
        }
        lastA = a; lastB = b; lastM1 = me; lastM2 = me2;
        if (! aenvA.isActive() && ! aenvB.isActive()) active = false;
    }

private:
    struct Part
    {
        int wave, count;
        double offset, detune, spread, level;
        std::array<double, maxUnison> ratio, gl, gr;
        double norm = 1.0;
    };
    struct Unit { std::array<Osc, maxUnison> osc, aux; };

    void unison (Unit& un, const Part& pt, double note, double& l, double& r, double lvl) noexcept
    {
        const auto baseDt = noteHz (note) / sr;
        static constexpr Osc::Wave map[8] { Osc::sine, Osc::triangle, Osc::saw, Osc::pulse, Osc::pulse, Osc::saw, Osc::pulse, Osc::saw };
        const auto wv = std::clamp (pt.wave, 0, 7);
        const auto w = map[wv];
        const auto pw = wv == 4 ? 0.25 : wv == 6 ? 0.125 : 0.5;
        for (int k = 0; k < pt.count; ++k)
        {
            double y;
            if (wv == 5) y = noise.next() * 0.6;
            else if (wv == 7) // tri-scie : moitié triangle, moitié scie (même fréquence, même phase de départ)
            {
                const auto d = baseDt * pt.ratio[(size_t) k];
                y = 0.6 * un.osc[(size_t) k].next (Osc::triangle, d, 0.5) + 0.5 * un.aux[(size_t) k].next (Osc::saw, d, 0.5);
            }
            else y = un.osc[(size_t) k].next (w, baseDt * pt.ratio[(size_t) k], pw);
            l += y * pt.gl[(size_t) k] * lvl;
            r += y * pt.gr[(size_t) k] * lvl;
        }
    }

    std::pair<double, double> filt (std::array<StateFilter, 4>& f, int type, double l, double r, double fc, double res, bool saturate) noexcept
    {
        if (saturate) { l = fastTanh (l); r = fastTanh (r); }
        if (type == 4) return { l, r }; // contourné
        SvfCoefs c;
        c.set (fc, type == 1 ? res * 0.5 : res, sr); // un seul calcul de coefficients pour les deux canaux
        if (type == 1) // 24 dB : deux étages
        {
            const auto l1 = f[0].process (l, c).low, r1 = f[1].process (r, c).low;
            return { f[2].process (l1, c).low, f[3].process (r1, c).low };
        }
        const auto ol = f[0].process (l, c), orr = f[1].process (r, c);
        switch (type)
        {
            case 0: return { ol.low, orr.low };
            case 2: return { ol.band * 1.4, orr.band * 1.4 };
            default: return { ol.high, orr.high };
        }
    }

    double sr = 48000.0, lastA = 0.0, lastB = 0.0, lastM1 = 0.0, lastM2 = 0.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false;
    std::uint32_t rng = 3;
    const PadsShared* shared = nullptr;
    std::array<Unit, 4> units;
    std::array<StateFilter, 4> fA, fB;
    Adsr aenvA, aenvB, menv, menv2;
    Glide pitch;
    WhiteNoise noise { 0xD00Du };
    DcBlocker dcL, dcR;
};

class Pads final : public SynthBase<PadsVoice, 8>
{
public:
    Pads()
    {
        for (int i = 0; i < 8; ++i) { voices[(size_t) i].seed (0x2545F491u * (std::uint32_t) (i + 1)); voices[(size_t) i].share (&lfos); }
    }

    const ModuleDesc& desc() const noexcept override { return pads::desc(); }
    void prepare (double s, int block) override { SynthBase::prepare (s, block); chorus.prepare (s); chorus.reset(); }
    void reset() noexcept override { SynthBase::reset(); chorus.reset(); }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        using namespace pads;
        // LFO communs (une valeur par bloc de commande)
        const auto r1 = lfoSyncHz ((int) p[l1Sync], p[l1Rate], bpm), r2 = lfoSyncHz ((int) p[l2Sync], p[l2Rate], bpm);
        double l1 = 0.0, l2 = 0.0;
        for (int i = 0; i < n; ++i) { l1 = lfo1.next ((int) p[l1Shape], r1, sr); l2 = lfo2.next ((int) p[l2Shape], r2, sr); }
        lfoAge += (double) n / sr;
        lfos.lfo1 = l1 * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) p[l1Delay]));
        lfos.lfo2 = l2 * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) p[l2Delay]));
        SynthBase::render (L, R, n, p);
    }

protected:
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[pads::voices], 1, 8);
        switch ((int) p[pads::vMode])
        {
            case 1: setMode (true, false, v); break;
            case 2: setMode (true, true, v); break;
            default: setMode (v == 1, true, v); break;
        }
    }
    void postProcess (float* L, float* R, int n, const float* p) noexcept override
    {
        using namespace pads;
        const auto mix = p[chorusMix] / 100.0;
        if (mix > 0.0) chorus.process (L, R, n, p[chorusRate], 2.2, 8.0, mix, 12000.0);
        // largeur stéréo (mid/side)
        const auto w = p[width] / 100.0;
        if (std::abs (w - 1.0) < 1.0e-6) return;
        for (int i = 0; i < n; ++i)
        {
            const auto m = 0.5 * ((double) L[i] + R[i]), s = 0.5 * ((double) L[i] - R[i]) * w;
            L[i] = (float) (m + s);
            R[i] = (float) (m - s);
        }
    }

private:
    PadsShared lfos;
    double lfoAge = 1.0e9;
public:
    /** Première note d'une phrase (aucune voix ne sonne) : le retard des LFO repart. */
    void noteOn (int note, float velocity) noexcept override { if (! isActive()) lfoAge = 0.0; SynthBase::noteOn (note, velocity); }
private:
    SimpleLfo lfo1, lfo2;
    BbdChorus chorus;
};

} // namespace grv::instruments
