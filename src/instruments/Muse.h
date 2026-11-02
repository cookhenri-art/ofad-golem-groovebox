/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « World Tour » — principe des polyphoniques analogiques récents à 8 voix héritiers des
    monophoniques à échelle (architecture reprise ; ni nom, ni façade, ni son d'origine) :
      - deux oscillateurs par voix à forme continue (triangle → scie → carré → impulsion
        étroite), pieds 32' à 2', FM linéaire de l'oscillateur 2 vers le 1, synchro dure du 2
        sur le 1, suivi du clavier débrayable sur le 2 ;
      - un oscillateur de modulation par voix (sinus, scie, scie inversée, carré, bruit), en
        plage lente ou audio (alors suivi du clavier et entendu dans le mélangeur), recalé à la
        note, unipolaire, envoyé vers la hauteur, les filtres, la forme, l'ampli et le panoramique ;
      - mélangeur qui sature (oscillateurs, oscillateur de modulation, bruit, modulation en
        anneau 1 × 2, « surcharge ») ;
      - deux filtres en échelle 24 dB par voix (1 : passe-bas ou passe-haut ; 2 : passe-bas) en
        série, en parallèle ou en stéréo (filtre 1 à gauche, filtre 2 à droite), liables ;
      - deux enveloppes ADSR (filtre, avec boucle ; ampli), vibrato dédié (et molette) ;
      - 2 LFO, 1 enveloppe et 16 routages (modulation commune), arpège ;
      - désaccord des voix (ensemble), dérive ; retard « diffusant » stéréo à plusieurs prises.
    Une piste = un timbre (le Groovebox a 16 pistes) : pas de mode bitimbral.
    Modèle : MorphOsc (polyBLEP / B-spline), Ladder TPT résolu sans délai ; coefficients toutes
    les 8 valeurs ; aucune latence ajoutée (le retard est un effet voulu).
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

#include <array>
#include <cmath>
#include <vector>

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace muse
{
    enum Slot
    {
        o1Oct = 0, o1Wave, o1Fm, o2Oct, o2Semi, o2Fine, o2Wave, o2Sync, o2Key,
        mxO1, mxO2, mxMod, mxNoise, mxRing, overload,
        moWave, moRate, moAudio, moKey, moReset, moUni, moPitch, moDest, moFilter, moWaveAmt, moVca, moPan,
        f1Mode, f1Cut, f1Res, route, f2Cut, f2Res, fLink, fEnv, fKey, fVel,
        e1A, e1D, e1S, e1R, e1Loop, e2A, e2D, e2S, e2R, velAmp,
        vibRate, vibAmt, vibWheel,
        dlyMix, dlyTime, dlySync, dlyFb, dlyDiff,
        voices, vMode, glide, spread, drift, tune, level,
        numSlots
    };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            const std::vector<std::string> feet { "32'", "16'", "8'", "4'", "2'" };
            ModuleDesc m;
            m.type = "muse";
            m.name = "World Tour";
            m.family = "Polys";
            m.principle = "Polyphonique analogique 8 voix : 2 oscillateurs \xC3\xA0 forme continue, oscillateur de modulation, 2 filtres en \xC3\xA9" "chelle (s\xC3\xA9rie, parall\xC3\xA8le, st\xC3\xA9r\xC3\xA9o), retard diffusant";
            m.params = {
                C ("o1oct", "Pied osc. 1", feet, 2),
                P ("o1wave", "Forme osc. 1", "%", 0, 100, 33),
                P ("o1fm", "FM 2 \xE2\x86\x92 1", "%", 0, 100, 0),
                C ("o2oct", "Pied osc. 2", feet, 2),
                I ("o2semi", "Demi-tons osc. 2", "dt", -12, 12, 0),
                P ("o2fine", "Fin osc. 2", "ct", -50, 50, 7),
                P ("o2wave", "Forme osc. 2", "%", 0, 100, 33),
                C ("o2sync", "Synchro 2 \xE2\x86\x92 1", onOff),
                C ("o2key", "Clavier osc. 2", onOff, 1),

                P ("mxo1", "Osc. 1", "%", 0, 100, 80),
                P ("mxo2", "Osc. 2", "%", 0, 100, 70),
                P ("mxmod", "Osc. de modulation", "%", 0, 100, 0),
                P ("mxnoise", "Bruit", "%", 0, 100, 0),
                P ("mxring", "Anneau 1 \xC3\x97 2", "%", 0, 100, 0),
                P ("overload", "Surcharge", "%", 0, 100, 15),

                C ("mowave", "Forme", { "Sinus", "Scie", "Scie invers\xC3\xA9" "e", "Carr\xC3\xA9", "Bruit" }),
                P ("morate", "Vitesse", "Hz", 0.05f, 2000, 5, 0.25f),
                C ("moaudio", "Plage", { "Lente", "Audio" }),
                C ("mokey", "Suit le clavier", onOff),
                C ("moreset", "Recal\xC3\xA9 \xC3\xA0 la note", onOff),
                C ("mouni", "Unipolaire", onOff),
                P ("mopitch", "\xE2\x86\x92 hauteur", "dt", -24, 24, 0),
                C ("modest", "Hauteur de", { "Osc. 1 et 2", "Osc. 1", "Osc. 2" }),
                P ("mofilter", "\xE2\x86\x92 filtres", "%", -100, 100, 0),
                P ("mowaveamt", "\xE2\x86\x92 formes", "%", -100, 100, 0),
                P ("movca", "\xE2\x86\x92 ampli", "%", 0, 100, 0),
                C ("mopan", "\xE2\x86\x92 panoramique", onOff),

                C ("f1mode", "Filtre 1", { "Passe-bas", "Passe-haut" }),
                P ("f1cut", "Coupure 1", "Hz", 20, 18000, 2500, 0.3f),
                P ("f1res", "R\xC3\xA9sonance 1", "%", 0, 100, 10),
                C ("route", "Routage", { "S\xC3\xA9rie", "Parall\xC3\xA8le", "St\xC3\xA9r\xC3\xA9o" }, 1),
                P ("f2cut", "Coupure 2", "Hz", 20, 18000, 2500, 0.3f),
                P ("f2res", "R\xC3\xA9sonance 2", "%", 0, 100, 10),
                C ("flink", "Coupures li\xC3\xA9" "es", onOff),
                P ("fenv", "Qt\xC3\xA9 env.", "%", -100, 100, 40),
                P ("fkey", "Clavier", "%", 0, 100, 40),
                P ("fvel", "V\xC3\xA9locit\xC3\xA9", "%", 0, 100, 20),

                P ("e1a", "Attaque", "ms", 0.5f, 10000, 4, 0.3f),
                P ("e1d", "D\xC3\xA9" "clin", "ms", 5, 10000, 700, 0.3f),
                P ("e1s", "Maintien", "%", 0, 100, 25),
                P ("e1r", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 600, 0.3f),
                C ("e1loop", "Boucle", onOff),
                P ("e2a", "Attaque", "ms", 0.5f, 10000, 3, 0.3f),
                P ("e2d", "D\xC3\xA9" "clin", "ms", 5, 10000, 900, 0.3f),
                P ("e2s", "Maintien", "%", 0, 100, 80),
                P ("e2r", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 500, 0.3f),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 35),

                P ("vibrate", "Vitesse", "Hz", 0.1f, 12, 5.5f, 0.5f),
                P ("vibamt", "Vibrato", "ct", 0, 100, 0),
                P ("vibwheel", "Molette \xE2\x86\x92 vibrato", "ct", 0, 100, 40),

                P ("dlymix", "Retard", "%", 0, 100, 0),
                P ("dlytime", "Temps", "ms", 20, 1500, 380, 0.4f),
                C ("dlysync", "Synchro", lfoSyncChoices()),
                P ("dlyfb", "R\xC3\xA9injection", "%", 0, 92, 35),
                P ("dlydiff", "Diffusion", "%", 0, 100, 45),

                I ("voices", "Voix", "", 1, 8, 8),
                C ("vmode", "Jeu", { "Poly", "Mono", "Mono legato" }),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("spread", "D\xC3\xA9saccord des voix", "%", 0, 100, 15),
                P ("drift", "D\xC3\xA9rive", "%", 0, 100, 20),
                P ("tune", "Transposition", "dt", -24, 24, 0),
                P ("level", "Volume", "dB", -30, 6, -14),
            };
            m.sections = {
                { "osc", "Oscillateurs", "osc", { "o1oct", "o1wave", "o1fm", "o2oct", "o2semi", "o2fine", "o2wave", "o2sync", "o2key" }, "osc" },
                { "mix", "M\xC3\xA9langeur", "mix", { "mxo1", "mxo2", "mxmod", "mxnoise", "mxring", "overload" }, "osc" },
                { "modosc", "Oscillateur de modulation", "mod", { "mowave", "morate", "moaudio", "mokey", "moreset", "mouni", "mopitch", "modest", "mofilter", "mowaveamt", "movca", "mopan" }, "mod" },
                { "filters", "Filtres", "filter", { "f1mode", "f1cut", "f1res", "route", "f2cut", "f2res", "flink", "fenv", "fkey", "fvel" }, "filter" },
                { "env1", "Enveloppe filtre", "env", { "e1a", "e1d", "e1s", "e1r", "e1loop", "fenv" }, "env" },
                { "env2", "Enveloppe ampli", "env", { "e2a", "e2d", "e2s", "e2r", "velamp", "level" }, "amp" },
                { "vib", "Vibrato", "lfo", { "vibrate", "vibamt", "vibwheel" }, "mod" },
                { "delay", "Retard diffusant", "fx", { "dlymix", "dlytime", "dlysync", "dlyfb", "dlydiff" }, "fx" },
                { "global", "Global", "global", { "voices", "vmode", "glide", "spread", "drift", "tune", "level" }, "global" },
            };
            using V = std::vector<std::pair<std::string, float>>;
            m.presets = {
                { "Init", V {}, "Init" },
                { "Grand voyage", V { { "o1wave", 40 }, { "o2wave", 40 }, { "o2fine", 9 }, { "f1cut", 1400 }, { "f2cut", 1400 }, { "fenv", 25 }, { "e1a", 1200 }, { "e1d", 3000 }, { "e1s", 60 },
                                      { "e2a", 900 }, { "e2r", 2600 }, { "spread", 35 }, { "dlymix", 30 }, { "dlydiff", 70 }, { "dlyfb", 55 }, { "route", 2 }, { "level", -6.0f } }, "Nappes" },
                { "Basse ladder", V { { "o1oct", 1 }, { "o2oct", 1 }, { "o1wave", 66 }, { "o2wave", 50 }, { "o2fine", 4 }, { "route", 0 }, { "f1cut", 400 }, { "f1res", 35 }, { "f2cut", 2000 },
                                      { "flink", 0 }, { "fenv", 55 }, { "e1d", 350 }, { "e1s", 10 }, { "overload", 45 }, { "vmode", 2 }, { "voices", 1 }, { "level", -0.7f } }, "Basses" },
                { "Basse en anneau", V { { "o1oct", 1 }, { "o2oct", 2 }, { "o2semi", 7 }, { "mxring", 60 }, { "mxo2", 20 }, { "f1cut", 700 }, { "fenv", 45 }, { "e1d", 260 }, { "e1s", 0 },
                                         { "vmode", 1 }, { "voices", 1 }, { "level", -2.6f } }, "Basses" },
                { "Cuivres chauds", V { { "o1wave", 40 }, { "o2wave", 40 }, { "f1cut", 700 }, { "f2cut", 700 }, { "fenv", 55 }, { "e1a", 80 }, { "e1d", 700 }, { "e1s", 45 }, { "e2a", 40 },
                                        { "spread", 25 }, { "overload", 30 }, { "level", -9.1f } }, "Cuivres" },
                { "Cordes en tourn\xC3\xA9" "e", V { { "o1wave", 36 }, { "o2wave", 36 }, { "o2fine", 12 }, { "f1mode", 1 }, { "f1cut", 180 }, { "route", 0 }, { "flink", 0 },
                                                    { "f2cut", 3500 }, { "fenv", 10 }, { "e2a", 600 }, { "e2r", 1300 }, { "spread", 55 }, { "dlymix", 20 }, { "dlydiff", 80 }, { "level", -5.1f } }, "Cordes" },
                { "Lead synchro", V { { "o2sync", 1 }, { "o2semi", 12 }, { "mxo1", 0 }, { "mxo2", 90 }, { "mopitch", 7 }, { "modest", 2 }, { "mowave", 0 }, { "morate", 0.3f },
                                      { "f1cut", 4000 }, { "f2cut", 4000 }, { "vmode", 2 }, { "voices", 1 }, { "glide", 50 }, { "vibamt", 10 }, { "dlymix", 18 }, { "level", -7.3f } }, "Leads" },
                { "Lead FM", V { { "o1fm", 45 }, { "o2oct", 3 }, { "o2wave", 0 }, { "mxo2", 0 }, { "f1cut", 5000 }, { "f2cut", 5000 }, { "vmode", 2 }, { "voices", 1 }, { "level", -6.8f } }, "Leads" },
                { "Pluck st\xC3\xA9r\xC3\xA9o", V { { "route", 2 }, { "flink", 0 }, { "f1cut", 500 }, { "f2cut", 900 }, { "f1res", 30 }, { "f2res", 30 }, { "fenv", 60 }, { "e1d", 260 }, { "e1s", 0 },
                                                   { "e2d", 500 }, { "e2s", 0 }, { "e2r", 400 }, { "dlymix", 25 }, { "level", -4.5f } }, "Plucks" },
                { "Clavier analogique", V { { "o1wave", 55 }, { "o2wave", 80 }, { "o2oct", 3 }, { "mxo2", 35 }, { "f1cut", 1800 }, { "f2cut", 1800 }, { "fenv", 30 }, { "e1d", 900 }, { "e1s", 20 },
                                            { "e2d", 1500 }, { "e2s", 40 }, { "level", -9.6f } }, "Claviers" },
                { "Battement de c\xC5\x93ur", V { { "e1loop", 1 }, { "e1a", 120 }, { "e1d", 380 }, { "e1s", 0 }, { "fenv", 50 }, { "f1cut", 500 }, { "f2cut", 500 }, { "f1res", 40 },
                                                  { "e2a", 400 }, { "e2r", 1500 }, { "level", -13.8f } }, "Nappes" },
                { "Sirène modulante", V { { "moaudio", 1 }, { "mokey", 1 }, { "morate", 261.6f }, { "mopitch", 0 }, { "mofilter", 35 }, { "mxmod", 50 }, { "mowave", 3 }, { "f1cut", 2000 },
                                          { "dlymix", 35 }, { "dlyfb", 70 }, { "level", -9.6f } }, "Effets" },
                { "Arp\xC3\xA8ge des sommets", V { { "o1wave", 66 }, { "o2wave", 90 }, { "f1cut", 900 }, { "fenv", 60 }, { "e1d", 180 }, { "e1s", 0 }, { "e2d", 300 }, { "e2s", 0 }, { "e2r", 250 },
                                                  { "dlymix", 28 }, { "dlysync", 7 }, { "arp_mode", 3 }, { "arp_oct", 2 }, { "level", -5.9f } }, "Arp\xC3\xA8ges" },
                { "Vent de sable", V { { "mxo1", 0 }, { "mxo2", 0 }, { "mxnoise", 90 }, { "f1cut", 900 }, { "f2cut", 1300 }, { "f1res", 70 }, { "f2res", 70 }, { "flink", 0 }, { "route", 2 },
                                       { "e2a", 1500 }, { "e2r", 3000 }, { "mx1_src", 6 }, { "mx1_dst@f1cut", 0 }, { "mx1_amt", 40 }, { "lf1_rate", 0.15f }, { "dlymix", 30 }, { "level", 4.1f } }, "Effets" },
            };
            HubSpec hub;
            hub.envs = 1; hub.lfos = 2; hub.firstLfo = 1; hub.slots = 16; hub.arp = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m); // v0.45.2 : banque d'usine, au moins 10 sons par catégorie (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

/** Valeurs communes du bloc : vibrato, molette. */
struct MuseShared { double vib = 0.0, wheel = 0.0; };

class MuseVoice
{
public:
    void prepare (double s) { sr = s; e1.prepare (s); e2.prepare (s); reset(); }
    void reset() noexcept
    {
        o1.reset(); o2.reset(); f1.reset(); f2.reset(); dcL.reset(); dcR.reset(); e1.reset(); e2.reset();
        moPh = 0.0; moHeld = 0.0; last2 = 0.0;
        active = pending = false;
    }
    void setShared (const MuseShared* s) noexcept { sh = s; }
    void seed (std::uint32_t s) noexcept { rng = s; noise = WhiteNoise (s ^ 0xA5A5u); }
    void setSpread (int index, int count) noexcept { uni = count > 1 ? 2.0 * index / (count - 1) - 1.0 : 0.0; }
    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; active = true;
        rng = rng * 1664525u + 1013904223u;
        noteDrift = (double) (rng >> 8) / 8388608.0 - 1.0;
        rng = rng * 1664525u + 1013904223u;
        voiceDetune = (double) (rng >> 8) / 8388608.0 - 1.0;
    }
    void release() noexcept { e1.gate (false); e2.gate (false); }
    bool isActive() const noexcept { return active; }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace muse;
        if (pending)
        {
            pending = false;
            if (p[glide] > 0.5f && from >= 0 && legatoPending) pitch.to ((double) target);
            else if (p[glide] > 0.5f && from >= 0) { pitch.jump ((double) from); pitch.to ((double) target); }
            else pitch.jump ((double) target);
            if (! legatoPending || ! e2.isActive())
            {
                e1.gate (true); e2.gate (true);
                if (p[moReset] > 0.5f) moPh = 0.0;
            }
        }
        e1.set (p[e1A], p[e1D], p[e1S] / 100.0, p[e1R]);
        e2.set (p[e2A], p[e2D], p[e2S] / 100.0, p[e2R]);
        const bool loop1 = p[e1Loop] > 0.5f;

        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-(double) n / (p[glide] * 0.001 * sr * 0.35));
        const auto dr = p[drift] / 100.0;
        const double vib = sh != nullptr ? sh->vib : 0.0, wheel = sh != nullptr ? sh->wheel : 0.0;
        const auto vibSemis = vib * (p[vibAmt] + wheel * p[vibWheel]) / 100.0;
        const auto base = pitch.next (glideCoef) + p[tune] + vibSemis + uni * 0.1
                        + (p[spread] / 100.0) * voiceDetune * 0.25 + dr * noteDrift * 0.06;
        static constexpr double footOct[5] { -2.0, -1.0, 0.0, 1.0, 2.0 };
        const auto n1 = base + 12.0 * footOct[std::clamp ((int) p[o1Oct], 0, 4)];
        const auto n2 = (p[o2Key] > 0.5f ? base : 60.0 + (base - std::round (base)))
                      + 12.0 * footOct[std::clamp ((int) p[o2Oct], 0, 4)] + std::round (p[o2Semi]) + p[o2Fine] / 100.0;

        // oscillateur de modulation (par voix)
        const bool audio = p[moAudio] > 0.5f;
        const auto moHz = std::clamp ((double) p[moRate] * (p[moKey] > 0.5f ? fastExp2 ((base - 60.0) / 12.0) : 1.0), 0.01, 0.45 * sr);
        const auto moInc = (audio ? moHz : std::min (moHz, 200.0)) / sr;
        const int moShape = std::clamp ((int) p[moWave], 0, 4);
        const bool moU = p[moUni] > 0.5f;
        const auto moPitchAmt = (double) p[moPitch];
        const int dest = std::clamp ((int) p[moDest], 0, 2);
        const auto moF = p[moFilter] / 100.0 * 4.0, moW = p[moWaveAmt] / 100.0, moV = p[moVca] / 100.0;
        const bool moP = p[moPan] > 0.5f;

        const auto w1 = p[o1Wave] / 100.0, w2 = p[o2Wave] / 100.0;
        const auto fm = p[o1Fm] / 100.0 * 0.6;
        const bool sync = p[o2Sync] > 0.5f;
        const auto m1 = p[mxO1] / 100.0, m2 = p[mxO2] / 100.0, mm = p[mxMod] / 100.0, mn = p[mxNoise] / 100.0, mr = p[mxRing] / 100.0;
        const auto ov = p[overload] / 100.0;
        const auto drv = 1.0 + ov * ov * 6.0;
        const auto mixIn = 0.45;

        const int rt = std::clamp ((int) p[route], 0, 2);
        const bool hp1 = p[f1Mode] > 0.5f, link = p[fLink] > 0.5f;
        const auto r1 = p[f1Res] / 100.0, r2 = p[f2Res] / 100.0;
        const auto keyOct = (p[fKey] / 100.0) * (base - 60.0) / 12.0;
        const auto velOct = (p[fVel] / 100.0) * 3.0 * (vel - 0.6);
        const auto envOct = (p[fEnv] / 100.0) * 7.0;
        // coupures liées : la coupure 2 règle les deux filtres (passe-haut + passe-bas : bande ou réjection)
        const auto cut1 = link ? (double) p[f2Cut] : (double) p[f1Cut];
        const auto cut2 = (double) p[f2Cut];
        const auto cutDrift = 1.0 + dr * noteDrift * 0.05;
        const auto gain = dbToGain (p[level]) * velocityGain (vel, p[velAmp] / 100.0);

        double fc1 = 1000.0, fc2 = 1000.0, panMod = 0.0, w1n = w1, w2n = w2, mo = 0.0, pOff1 = 0.0, pOff2 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            // oscillateur de modulation
            moPh += moInc;
            if (moPh >= 1.0) { moPh -= std::floor (moPh); moHeld = noise.next(); }
            switch (moShape)
            {
                case 0: mo = fastSin2Pi (moPh); break;
                case 1: mo = 2.0 * moPh - 1.0; break;
                case 2: mo = 1.0 - 2.0 * moPh; break;
                case 3: mo = moPh < 0.5 ? 1.0 : -1.0; break;
                default: mo = audio ? noise.next() : moHeld; break;
            }
            const auto moM = moU ? 0.5 + 0.5 * mo : mo;

            const auto fe = e1.next();
            if (loop1 && e1.getStage() == Adsr::sustain) e1.gate (true); // boucle : l'attaque repart
            const auto ae = e2.next();

            if ((i & 7) == 0)
            {
                const auto pm = moM * moPitchAmt;
                pOff1 = dest != 2 ? pm : 0.0;
                pOff2 = dest != 1 ? pm : 0.0;
                const auto oct = envOct * fe + keyOct + velOct + moM * moF;
                fc1 = std::clamp (cut1 * cutDrift * fastExp2 (oct), 16.0, 0.45 * sr);
                fc2 = std::clamp (cut2 * cutDrift * fastExp2 (oct), 16.0, 0.45 * sr);
                w1n = std::clamp (w1 + moM * moW, 0.0, 1.0);
                w2n = std::clamp (w2 + moM * moW, 0.0, 1.0);
                panMod = moP ? moM * 0.8 : 0.0;
                dt1 = noteHz (n1 + pOff1) / sr;
                dt2 = noteHz (n2 + pOff2) / sr;
            }
            double wrap = -1.0;
            // oscillateur 1 = maître de la synchro ; le 2 le module en FM (sortie retardée d'un échantillon)
            const auto a = o1.next (1.0 + 3.0 * w1n, dt1, fm * last2, -1.0, &wrap);
            const auto b = o2.next (1.0 + 3.0 * w2n, dt2, 0.0, sync ? wrap : -1.0);
            last2 = b;
            auto x = a * m1 + b * m2 + (audio ? mo * mm : 0.0) + noise.next() * mn * 0.8 + a * b * mr;
            // mélangeur qui sature (surcharge)
            x = fastTanh (x * mixIn * drv) / std::sqrt (drv) * 1.6;

            double yl, yr;
            if (rt == 0)
            {
                const auto s1 = hp1 ? f1.process (x, fc1, r1, sr, Ladder::hp24) : f1.process (x, fc1, r1, sr, Ladder::lp24);
                yl = yr = f2.process (s1, fc2, r2, sr, Ladder::lp24);
            }
            else
            {
                const auto s1 = hp1 ? f1.process (x, fc1, r1, sr, Ladder::hp24) : f1.process (x, fc1, r1, sr, Ladder::lp24);
                const auto s2 = f2.process (x, fc2, r2, sr, Ladder::lp24);
                if (rt == 1) yl = yr = 0.7 * (s1 + s2);
                else { yl = s1; yr = s2; }
            }
            const auto amp = ae * gain * (1.0 - moV * (1.0 - (0.5 + 0.5 * mo)));
            const auto pl = std::clamp (panMod + uni * 0.5, -1.0, 1.0);
            L[i] += (float) (dcL.process (yl) * amp * (1.0 - std::max (0.0, pl)));
            R[i] += (float) (dcR.process (yr) * amp * (1.0 + std::min (0.0, pl)));
        }
        if (! e2.isActive()) active = false;
    }

private:
    double sr = 48000.0, uni = 0.0, noteDrift = 0.0, voiceDetune = 0.0, moPh = 0.0, moHeld = 0.0, last2 = 0.0;
    double dt1 = 0.01, dt2 = 0.01;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool active = false, pending = false, legatoPending = false;
    std::uint32_t rng = 0x1357u;
    const MuseShared* sh = nullptr;
    MorphOsc o1, o2;
    Ladder f1, f2;
    Adsr e1, e2;
    Glide pitch;
    WhiteNoise noise { 0x77u };
    DcBlocker dcL, dcR;
};

/** Retard stéréo « diffusant » : une ligne par côté (prises décalées), trois passe-tout dans la
    boucle de réinjection (les échos se fondent en nuage), amorti dans les aigus. */
class DiffusionDelay
{
public:
    void prepare (double s)
    {
        sr = s;
        const auto n = (size_t) std::ceil (s * 2.2) + 8;
        bufL.assign (n, 0.0f); bufR.assign (n, 0.0f);
        for (auto& a : ap) a.buf.assign (4096, 0.0f);
        reset();
    }
    void reset() noexcept
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f);
        for (auto& a : ap) { std::fill (a.buf.begin(), a.buf.end(), 0.0f); a.pos = 0; }
        pos = 0; lpL = lpR = 0.0; smoothT = -1.0;
    }
    bool busy() const noexcept { return tail > 0; }

    void process (float* L, float* R, int n, double mix, double timeS, double fb, double diff) noexcept
    {
        if (bufL.empty()) return;
        if (mix <= 0.0001) { stale = true; tail = 0; return; } // retard coupé : ses échos partent (rien ne revient plus tard)
        if (stale) { reset(); stale = false; } // anciens échos effacés avant de reprendre
        const auto size = (int) bufL.size();
        const auto target = std::clamp (timeS * sr, 8.0, (double) size - 4.0);
        if (smoothT < 0.0) smoothT = target;
        const auto g = 0.15 + 0.55 * diff;
        static constexpr int apLen[6] { 211, 337, 547, 263, 419, 613 };
        bool loud = false;
        for (int i = 0; i < n; ++i)
        {
            smoothT += (target - smoothT) * 0.0005;
            const auto rdL = readAt (bufL, pos - smoothT);
            const auto rdR = readAt (bufR, pos - smoothT * 1.33);
            // diffusion : passe-tout en série dans la boucle
            double dl = rdL, dr = rdR;
            for (int k = 0; k < 3; ++k) { dl = ap[(size_t) k].process (dl, g, apLen[k]); dr = ap[(size_t) (k + 3)].process (dr, g, apLen[k + 3]); }
            lpL += (dl - lpL) * 0.35; lpR += (dr - lpR) * 0.35;
            const double inL = L[i], inR = R[i];
            bufL[(size_t) pos] = (float) (inL + fastTanh (lpR * fb));   // croisement gauche ↔ droite
            bufR[(size_t) pos] = (float) (inR + fastTanh (lpL * fb));
            if (++pos >= size) pos = 0;
            L[i] = (float) (inL + mix * dl);
            R[i] = (float) (inR + mix * dr);
            loud = loud || std::abs (dl) > 1.0e-5 || std::abs (dr) > 1.0e-5;
        }
        tail = loud ? 8 : std::max (0, tail - 1);
    }

private:
    struct Allpass
    {
        std::vector<float> buf;
        int pos = 0;
        double process (double x, double g, int len) noexcept
        {
            len = std::clamp (len, 1, (int) buf.size() - 1);
            auto rd = pos - len; if (rd < 0) rd += (int) buf.size();
            const auto d = (double) buf[(size_t) rd];
            const auto y = -g * x + d;
            buf[(size_t) pos] = (float) (x + g * y);
            if (++pos >= (int) buf.size()) pos = 0;
            return y;
        }
    };
    double readAt (const std::vector<float>& b, double at) const noexcept
    {
        const auto size = (double) b.size();
        while (at < 0.0) at += size;
        const auto i0 = (int) at;
        const auto f = at - i0;
        const auto i1 = i0 + 1 >= (int) b.size() ? 0 : i0 + 1;
        return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
    }
    std::vector<float> bufL, bufR;
    std::array<Allpass, 6> ap;
    int pos = 0, tail = 0;
    bool stale = false;
    double sr = 48000.0, lpL = 0.0, lpR = 0.0, smoothT = -1.0;
};

class MuseSynth final : public SynthBase<MuseVoice, 8>
{
public:
    MuseSynth() { for (int i = 0; i < 8; ++i) { voices[(size_t) i].setShared (&shared); voices[(size_t) i].seed (0x9E37u * (std::uint32_t) (i + 3)); } }
    const ModuleDesc& desc() const noexcept override { return muse::desc(); }
    void prepare (double s, int maxBlock) override { SynthBase::prepare (s, maxBlock); delay.prepare (s); }
    void reset() noexcept override { SynthBase::reset(); delay.reset(); }
    void setExpression (float bend, float wheelV, float pressure) noexcept override { wheelT = wheelV; SynthBase::setExpression (bend, wheelV, pressure); }
    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        blockN = std::min (n, voiceBlock);
        SynthBase::render (L, R, n, p);
    }
    bool isActive() const noexcept override { return SynthBase::isActive() || delay.busy(); }

protected:
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[muse::voices], 1, 8);
        switch ((int) p[muse::vMode])
        {
            case 1: setMode (true, false, v); break;
            case 2: setMode (true, true, v); break;
            default: setMode (false, true, v); break;
        }
    }
    const float* adjust (const float* p) noexcept override
    {
        using namespace muse;
        const auto nn = (double) blockN;
        vibPhase += std::clamp ((double) p[vibRate], 0.05, 20.0) * nn / sr;
        if (vibPhase >= 1.0) vibPhase -= std::floor (vibPhase);
        shared.vib = fastSin2Pi (vibPhase);
        wheel += (wheelT - wheel) * std::min (1.0, nn / (0.004 * sr));
        shared.wheel = wheel;
        return p;
    }
    void postProcess (float* L, float* R, int n, const float* p) noexcept override
    {
        using namespace muse;
        const auto t = (int) p[dlySync] > 0 ? 1.0 / std::max (0.05, lfoSyncHz ((int) p[dlySync], 1.0, bpm)) : p[dlyTime] / 1000.0;
        delay.process (L, R, n, p[dlyMix] / 100.0, std::clamp (t, 0.02, 2.0), p[dlyFb] / 100.0, p[dlyDiff] / 100.0);
    }

private:
    MuseShared shared;
    DiffusionDelay delay;
    int blockN = 32;
    double vibPhase = 0.0, wheel = 0.0, wheelT = 0.0;
};

} // namespace grv::instruments
