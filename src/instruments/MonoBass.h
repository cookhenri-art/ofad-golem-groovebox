/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Basse mono » — principe des grands monophoniques analogiques à deux oscillateurs de la
    décennie 2010 (architecture reprise, ni nom, ni panneau, ni son d'origine) :
      - deux oscillateurs à forme continue (triangle → scie → carré → impulsion), sous-oscillateur
        carré une octave sous l'oscillateur 1, bruit ;
      - oscillateur 2 : octave, fréquence (demi-tons), fréquence de battement (écart constant en Hz,
        le battement reste le même sur tout le clavier), synchro dure sur l'oscillateur 1, suivi du
        clavier débrayable ; phase des oscillateurs libre ou redémarrée à chaque note ;
      - mélangeur à 5 sources dont la RÉINJECTION de la sortie du filtre dans le mélangeur (grogne) ;
      - filtre en échelle 6 / 12 / 18 / 24 dB, résonance, multidrive, suivi du clavier, quantité
        d'enveloppe ; deux enveloppes DAHDSR (retard, tenue) bouclables, calées sur le tempo,
        redéclenchées ou non, temps suivant le clavier, verrou de l'enveloppe d'ampli (bourdon) ;
      - DEUX BUS DE MODULATION : une source (triangle, carré, scie, rampe, échantillonné-bloqué,
        enveloppe de filtre ou d'ampli, sinus, bruit, constante) de 0,01 Hz à 1 kHz, redémarrée au
        clavier ou non, synchronisable ; trois quantités : hauteur (osc. 1, 2 ou les deux),
        filtre, et une destination programmable ; chaque bus est dosé par la molette, la pression
        ou rien (bus 1 : molette par défaut) ;
      - portamento à temps constant, à vitesse constante ou exponentiel, toujours ou seulement lié,
        sur l'oscillateur 1, 2 ou les deux ;
      - mono (priorité à la note basse, haute ou dernière) ou DUO paraphonique : l'oscillateur 1
        joue la note la plus basse, l'oscillateur 2 la plus haute, à travers le même filtre ;
      - arpégiateur, pitch bend, molette, pression.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace mono
{
    enum Slot
    {
        // global
        voiceMode = 0, glideMs, glideMode, retrig, tune, velAmp, drift, level,
        // oscillateurs
        o1Oct, o1Shape, o2Oct, o2Semi, o2Fine, o2Shape, sync, subShape,
        mix1, mix2, mixSub, mixNoise,
        // filtre
        cutoff, reso, slope, drive, keytrack, envAmt, velFilter,
        // enveloppes
        fA, fD, fS, fR, aA, aD, aS, aR,
        // v0.10 : architecture complète
        priority, glideType, glideOsc, o2Beat, o2Kb, oscReset, mixFb,
        fDly, fHld, fLoop, fSync, fKb, aDly, aHld, aLoop, aSync, aKb, aLatch,
        b1Src, b1Rate, b1Sync, b1Reset, b1Pitch, b1PDst, b1Filt, b1Pgm, b1GDst, b1Ctrl,
        b2Src, b2Rate, b2Sync, b2Reset, b2Pitch, b2PDst, b2Filt, b2Pgm, b2GDst, b2Ctrl,
        numSlots
    };
    constexpr int busStride = b2Src - b1Src;
    enum BusField { bSrc = 0, bRate, bSync, bReset, bPitch, bPDst, bFilt, bPgm, bGDst, bCtrl };

    enum Source { srcTri = 0, srcSquare, srcSaw, srcRamp, srcSH, srcFEnv, srcAEnv, srcSine, srcNoise, srcConst };
    enum PgmDest { pgShapes = 0, pgShape1, pgShape2, pgLevel1, pgLevelSub, pgLevel2, pgNoise, pgFeedback, pgReso, pgDrive,
                   pgOtherRate, pgVca, pgEnvTime, pgEnvAmt, pgBeat };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> sources { "Triangle", "Carr\xC3\xA9", "Scie", "Rampe", "\xC3\x89" "chantillonn\xC3\xA9",
                                                     "Env. filtre", "Env. ampli", "Sinus", "Bruit", "Constante" };
            const std::vector<std::string> pgm { "Formes 1 et 2", "Forme 1", "Forme 2", "Niveau osc. 1", "Niveau sous-osc.", "Niveau osc. 2",
                                                 "Bruit", "R\xC3\xA9injection", "R\xC3\xA9sonance", "Multidrive", "Vitesse de l'autre bus",
                                                 "Niveau VCA", "Temps des enveloppes", "Quantit\xC3\xA9 env. filtre", "Battement osc. 2" };
            const std::vector<std::string> ctrl { "Molette", "Pression", "Aucune" };
            const std::vector<std::string> pdst { "Osc. 1 et 2", "Osc. 1", "Osc. 2" };
            ModuleDesc m;
            m.type = "mono";
            m.name = "Fatsub";
            m.family = "Basses";
            m.principle = "Monophonique / duo analogique : 2 oscillateurs, r\xC3\xA9injection, filtre en \xC3\xA9" "chelle 6 \xC3\xA0 24 dB, 2 bus de modulation, DAHDSR bouclables";
            m.params = {
                C ("mode", "Voix", { "Mono", "Duo" }),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                C ("glidemode", "Portamento", { "Toujours", "Li\xC3\xA9" }, 1),
                C ("retrig", "Multi-d\xC3\xA9" "clench.", { "Non", "Oui" }),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 40),
                P ("drift", "D\xC3\xA9rive", "%", 0, 100, 15),
                P ("level", "Volume", "dB", -24, 6, -6),

                I ("o1oct", "Octave 1", "oct", -2, 2, 0),
                P ("o1shape", "Forme 1", "", 0, 4, 2),
                I ("o2oct", "Octave 2", "oct", -2, 2, 0),
                I ("o2semi", "Fr\xC3\xA9q. 2", "dt", -12, 12, 0),
                P ("o2fine", "Fin 2", "ct", -50, 50, 7),
                P ("o2shape", "Forme 2", "", 0, 4, 3),
                C ("sync", "Synchro dure", { "Non", "Oui" }),
                C ("subshape", "Sous-osc.", { "Carr\xC3\xA9", "Sinus" }),
                P ("mix1", "Osc. 1", "%", 0, 100, 80),
                P ("mix2", "Osc. 2", "%", 0, 100, 50),
                P ("mixsub", "Sous-osc.", "%", 0, 100, 40),
                P ("mixnoise", "Bruit", "%", 0, 100, 0),

                P ("cutoff", "Coupure", "Hz", 20, 16000, 700, 0.3f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 20),
                C ("slope", "Pente", { "24 dB", "18 dB", "12 dB", "6 dB" }),
                P ("drive", "Multidrive", "dB", 0, 30, 4),
                P ("keytrack", "Suivi clavier", "%", 0, 100, 30),
                P ("envamt", "Quantit\xC3\xA9 env.", "%", -100, 100, 45),
                P ("velfilter", "V\xC3\xA9l. filtre", "%", 0, 100, 25),

                P ("fa", "Attaque", "ms", 0.5f, 10000, 2, 0.3f),
                P ("fd", "D\xC3\xA9" "clin", "ms", 5, 10000, 350, 0.3f),
                P ("fs", "Niveau", "%", 0, 100, 20),
                P ("fr", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 200, 0.3f),
                P ("aa", "Attaque", "ms", 0.5f, 10000, 1, 0.3f),
                P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 600, 0.3f),
                P ("as", "Niveau", "%", 0, 100, 80),
                P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 120, 0.3f),

                C ("priority", "Priorit\xC3\xA9", { "Note basse", "Note haute", "Derni\xC3\xA8re note" }),
                C ("glidetype", "Type de glisse", { "Temps constant", "Vitesse constante", "Exponentiel" }, 2),
                C ("glideosc", "Glisse sur", pdst),
                P ("o2beat", "Battement 2", "Hz", -3.5f, 3.5f, 0),
                C ("o2kb", "Clavier osc. 2", { "Oui", "Non" }),
                C ("oscreset", "Phase des osc.", { "Libre", "Red\xC3\xA9marr\xC3\xA9" "e" }),
                P ("mixfb", "R\xC3\xA9injection", "%", 0, 100, 0),

                P ("fdly", "Retard", "ms", 0, 5000, 0, 0.3f),
                P ("fhld", "Tenue", "ms", 0, 5000, 0, 0.3f),
                C ("floop", "Boucle", { "Non", "Oui" }),
                C ("fsync", "Synchro", { "Non", "Oui" }),
                P ("fkb", "Suivi clavier", "%", 0, 100, 0),
                P ("adly", "Retard", "ms", 0, 5000, 0, 0.3f),
                P ("ahld", "Tenue", "ms", 0, 5000, 0, 0.3f),
                C ("aloop", "Boucle", { "Non", "Oui" }),
                C ("async", "Synchro", { "Non", "Oui" }),
                P ("akb", "Suivi clavier", "%", 0, 100, 0),
                C ("alatch", "Verrou", { "Non", "Oui" }),
            };
            for (int b = 1; b <= 2; ++b)
            {
                const auto k = "mb" + std::to_string (b) + "_";
                m.params.push_back (C (k + "src", "Source", sources, b == 1 ? 0 : 5));
                m.params.push_back (P (k + "rate", "Vitesse", "Hz", 0.01f, 1000.0f, b == 1 ? 5.0f : 0.5f, 0.22f));
                m.params.push_back (C (k + "sync", "Synchro", lfoSyncChoices()));
                m.params.push_back (C (k + "reset", "Red\xC3\xA9marrage clavier", { "Non", "Oui" }));
                m.params.push_back (P (k + "pitch", "Hauteur", "dt", -24, 24, 0));
                m.params.push_back (C (k + "pdst", "Hauteur de", pdst));
                m.params.push_back (P (k + "filt", "Filtre", "%", -100, 100, 0));
                m.params.push_back (P (k + "pgm", "Programmable", "%", -100, 100, 0));
                m.params.push_back (C (k + "gdst", "Destination", pgm, b == 1 ? 0 : 1));
                m.params.push_back (C (k + "ctrl", "Commande", ctrl, b == 1 ? 0 : 2));
            }
            m.sections = {
                { "global", "Global", "global", { "mode", "priority", "glide", "glidetype", "glidemode", "glideosc", "tune", "level" }, "global" },
                { "osc", "Oscillateurs", "osc", { "o1oct", "o1shape", "o2oct", "o2semi", "o2fine", "o2beat", "o2shape", "sync" }, "osc" },
                { "osc2", "Oscillateurs (suite)", "osc", { "subshape", "o2kb", "oscreset", "drift" }, "osc" },
                { "mix", "M\xC3\xA9langeur", "mix", { "mix1", "mixsub", "mix2", "mixnoise", "mixfb" }, "osc" },
                { "filter", "Filtre", "filter", { "cutoff", "reso", "drive", "slope", "keytrack", "envamt", "velfilter" }, "filter" },
                { "fenv", "Enveloppe filtre", "env", { "fdly", "fa", "fhld", "fd", "fs", "fr", "envamt", "velfilter" }, "env" },
                { "fenv2", "Enveloppe filtre (options)", "env", { "floop", "fsync", "fkb", "retrig" }, "env" },
                { "aenv", "Enveloppe ampli", "env", { "adly", "aa", "ahld", "ad", "as", "ar", "velamp", "level" }, "amp" },
                { "aenv2", "Enveloppe ampli (options)", "env", { "aloop", "async", "akb", "alatch" }, "amp" },
                { "mod1", "Bus de modulation 1", "lfo", { "mb1_src", "mb1_rate", "mb1_sync", "mb1_pitch", "mb1_pdst", "mb1_filt", "mb1_pgm", "mb1_gdst" }, "mod" },
                { "mod1b", "Bus 1 (commande)", "lfo", { "mb1_ctrl", "mb1_reset" }, "mod" },
                { "mod2", "Bus de modulation 2", "lfo", { "mb2_src", "mb2_rate", "mb2_sync", "mb2_pitch", "mb2_pdst", "mb2_filt", "mb2_pgm", "mb2_gdst" }, "mod" },
                { "mod2b", "Bus 2 (commande)", "lfo", { "mb2_ctrl", "mb2_reset" }, "mod" },
            };
            m.presets = {
                { "Sub rond", { { "o1shape", 0.6f }, { "mix2", 0 }, { "mixsub", 70 }, { "cutoff", 400 }, { "reso", 5 }, { "envamt", 20 } } },
                { "Basse grasse", {} },
                { "Grogne r\xC3\xA9inject\xC3\xA9" "e", { { "mixfb", 70 }, { "drive", 16 }, { "cutoff", 520 }, { "reso", 35 }, { "envamt", 55 }, { "fd", 420 } } },
                { "Lead hurlant", { { "o1oct", 1 }, { "o2oct", 1 }, { "o2fine", 12 }, { "cutoff", 1800 }, { "reso", 55 }, { "drive", 14 }, { "glide", 80 },
                                    { "mb1_src", 7 }, { "mb1_pitch", 0.4f }, { "as", 100 } } },
                { "Synchro balay\xC3\xA9" "e", { { "sync", 1 }, { "o2semi", 7 }, { "mix1", 0 }, { "mix2", 90 }, { "cutoff", 2500 },
                                    { "mb2_src", 5 }, { "mb2_pitch", 14 }, { "mb2_pdst", 2 }, { "mb2_ctrl", 2 } } },
                { "Duo quinte", { { "mode", 1 }, { "o2shape", 2.5f }, { "mix2", 80 }, { "cutoff", 1400 }, { "reso", 25 }, { "glide", 60 } } },
                { "Enveloppe en boucle", { { "floop", 1 }, { "fsync", 1 }, { "fa", 120 }, { "fd", 120 }, { "envamt", 70 }, { "reso", 50 }, { "cutoff", 300 } } },
                { "Battement lent", { { "o2beat", 1.2f }, { "mix2", 80 }, { "o2shape", 2 }, { "cutoff", 900 } } },
            };
            HubSpec hub;
            hub.envs = 0; hub.lfos = 0; hub.slots = 0; hub.arp = true;
            addModHub (m, hub);
            addFactoryBank (m); // banque d'usine : 50 sons par catégories (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

/** Expression partagée par l'instrument et sa voix (molette, pression). */
struct MonoExpression { float wheel = 0.0f, pressure = 0.0f; };

/** Glissement de hauteur : temps constant, vitesse constante ou exponentiel. */
class MonoGlide
{
public:
    void jump (double n) noexcept { cur = target = from = n; }
    void to (double n) noexcept { from = cur; target = n; }
    double next (int type, double ms, double sr) noexcept
    {
        if (ms < 0.5 || cur == target) { cur = target; return cur; }
        const auto samples = std::max (1.0, ms * 0.001 * sr);
        if (type == 0)      step (std::abs (target - from) / samples);          // temps constant
        else if (type == 1) step (12.0 / samples);                               // vitesse constante : ms par octave
        else                cur = target + (cur - target) * std::exp (-1.0 / (samples * 0.35));
        return cur;
    }
    double get() const noexcept { return cur; }
private:
    void step (double d) noexcept { cur = cur < target ? std::min (target, cur + d) : std::max (target, cur - d); }
    double cur = 60.0, target = 60.0, from = 60.0;
};

class MonoVoice
{
public:
    void prepare (double s)
    {
        sr = s;
        fenv.prepare (s); aenv.prepare (s);
        reset();
    }
    void reset() noexcept
    {
        o1.reset(); o2.reset(); sub.reset(); filter.reset(); fenv.reset(); aenv.reset(); dc.reset();
        busLfo[0].reset(); busLfo[1].reset (0.25);
        active = false; fb = 0.0; second = -1; pending = pendingRelease = false;
    }

    void setExpression (const MonoExpression* e) noexcept { expr = e; }
    void setSpread (int, int) noexcept {}
    /** Duo : note de l'oscillateur 2 (-1 : la même que l'oscillateur 1). */
    void setSecond (int note) noexcept { second = note; }

    void start (int note, float velocity, bool legatoStart, int) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        pendingNote = note;
        pendingLegato = legatoStart;
        pending = true;
        pendingFromNoteOn = velocity >= 0.0f;
        pendingRelease = false;
        active = true;
    }
    void release() noexcept
    {
        // Des notes relâchées ensemble (accord) passent par les notes restantes avant le relâchement
        // final : ce changement de note, encore en attente, ne doit pas rouvrir les enveloppes.
        if (pending && ! pendingFromNoteOn) pending = false;
        else if (pending) { pendingRelease = true; return; } // note jouée et relâchée dans le même bloc
        if (! latch) aenv.gate (false);
        fenv.gate (false);
    }
    bool isActive() const noexcept { return active; }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace mono;
        latch = p[aLatch] > 0.5f;
        if (pending)
        {
            pending = false;
            const bool glideAlways = p[glideMode] < 0.5f;
            const bool doGlide = p[glideMs] > 0.5f && (glideAlways || pendingLegato) && aenv.isActive();
            const auto gOsc = (int) p[glideOsc];
            if (! doGlide || gOsc == 2) g1.jump ((double) pendingNote);
            g1.to ((double) pendingNote);
            const auto n2 = second >= 0 ? second : pendingNote;
            if (! doGlide || gOsc == 1) g2.jump ((double) n2);
            g2.to ((double) n2);
            const bool retrigger = ! pendingLegato || p[retrig] > 0.5f;
            if (retrigger || ! aenv.isActive()) { fenv.gate (true, true); aenv.gate (true, true); }
            else { fenv.gate (true, false); aenv.gate (true, false); }
            if (! aenv.isActive() || p[oscReset] > 0.5f) { o1.reset(); o2.reset (0.25); sub.reset(); }
            for (int b = 0; b < 2; ++b)
                if (p[b1Reset + b * busStride] > 0.5f && ! pendingLegato) busLfo[b].reset (b == 1 ? 0.25 : 0.0);
            keyNote = pendingNote;
            if (pendingRelease) { pendingRelease = false; if (! latch) aenv.gate (false); fenv.gate (false); }
        }
        else g2.to (second >= 0 ? (double) second : keyNote);

        // Bus de modulation : valeur de la source au début du bloc (cadence de contrôle)
        const auto wheel = expr != nullptr ? (double) expr->wheel : 0.0;
        const auto press = expr != nullptr ? (double) expr->pressure : 0.0;
        double bus[2] {}, busCtl[2] {};
        for (int b = 0; b < 2; ++b)
        {
            const auto* q = p + b1Src + b * busStride;
            const auto c = (int) q[bCtrl];
            busCtl[b] = c == 0 ? wheel : c == 1 ? press : 1.0;
        }
        // vitesse : la destination programmable de l'autre bus peut la moduler
        double rateMul[2] { 1.0, 1.0 };
        for (int b = 0; b < 2; ++b)
        {
            const auto* q = p + b1Src + b * busStride;
            if ((int) q[bGDst] == pgOtherRate) rateMul[1 - b] = fastExp2 (4.0 * (q[bPgm] / 100.0) * lastBus[b] * busCtl[b]);
        }
        for (int b = 0; b < 2; ++b)
        {
            const auto* q = p + b1Src + b * busStride;
            const auto hz = lfoSyncHz ((int) q[bSync], q[bRate], bpm) * rateMul[b];
            const auto src = (int) q[bSrc];
            double v;
            switch (src)
            {
                case srcFEnv: v = fenv.getValue(); break;
                case srcAEnv: v = aenv.getValue(); break;
                case srcConst: v = 1.0; break;
                default: v = busLfo[b].advance (src, hz, n, sr); break;
            }
            bus[b] = v;
            lastBus[b] = v;
        }
        // contributions des bus
        double pitch1 = 0.0, pitch2 = 0.0, filtOct = 0.0;
        double pg[15] {};
        for (int b = 0; b < 2; ++b)
        {
            const auto* q = p + b1Src + b * busStride;
            const auto m = bus[b] * busCtl[b];
            const auto dst = (int) q[bPDst];
            const auto semis = q[bPitch] * m;
            if (dst != 2) pitch1 += semis;
            if (dst != 1) pitch2 += semis;
            filtOct += (q[bFilt] / 100.0) * 4.0 * m;
            const auto gd = std::clamp ((int) q[bGDst], 0, 14);
            if (gd != pgOtherRate) pg[gd] += (q[bPgm] / 100.0) * m;
        }

        // Enveloppes : DAHDSR, temps calés sur le tempo ou suivant le clavier
        auto times = [&] (double ms, bool syncOn, double kb)
        {
            auto t = syncOn ? syncTimeMs (ms, bpm) : ms;
            t *= fastExp2 (-(keyNote - 60.0) / 12.0 * kb) * fastExp2 (3.0 * pg[pgEnvTime]);
            return t;
        };
        const bool fs = p[fSync] > 0.5f, as = p[aSync] > 0.5f;
        const auto fk = p[fKb] / 100.0, ak = p[aKb] / 100.0;
        fenv.set (times (p[fDly], fs, fk), times (p[fA], fs, fk), times (p[fHld], fs, fk), times (p[fD], fs, fk), p[fS] / 100.0, times (p[fR], fs, fk), p[fLoop] > 0.5f);
        aenv.set (times (p[aDly], as, ak), times (p[aA], as, ak), times (p[aHld], as, ak), times (p[aD], as, ak), p[aS] / 100.0, times (p[aR], as, ak), p[aLoop] > 0.5f);

        const auto gType = (int) p[glideType];
        const auto velA = velocityGain (vel, p[velAmp] / 100.0);
        const auto gain = dbToGain (p[level]) * velA * std::clamp (1.0 + pg[pgVca], 0.0, 2.0);
        const auto drv = dbToGain (std::clamp (p[drive] + 24.0 * pg[pgDrive], 0.0, 36.0));
        const auto res = std::clamp (p[reso] / 100.0 + pg[pgReso], 0.0, 1.0);
        const auto mode = (Ladder::Mode) std::clamp ((int) p[slope], 0, 3);
        const auto envOct = std::clamp (p[envAmt] / 100.0 + pg[pgEnvAmt], -1.0, 1.0) * 7.0;
        const auto velOct = (p[velFilter] / 100.0) * 2.0 * (vel - 0.5);
        const auto driftAmt = p[drift] / 100.0;
        const auto syncOn = p[sync] > 0.5f;
        const auto subSine = p[subShape] > 0.5f;
        const auto kb2 = p[o2Kb] < 0.5f;
        auto lvl = [] (double v) { return std::clamp (v, 0.0, 1.0); };
        const auto m1 = lvl (p[mix1] / 100.0 + pg[pgLevel1]), m2 = lvl (p[mix2] / 100.0 + pg[pgLevel2]);
        const auto ms = lvl (p[mixSub] / 100.0 + pg[pgLevelSub]), mn = lvl (p[mixNoise] / 100.0 + pg[pgNoise]);
        const auto mfb = lvl (p[mixFb] / 100.0 + pg[pgFeedback]) * 1.6;
        const auto sh1 = std::clamp (p[o1Shape] + 2.0 * (pg[pgShapes] + pg[pgShape1]), 0.0, 4.0);
        const auto sh2 = std::clamp (p[o2Shape] + 2.0 * (pg[pgShapes] + pg[pgShape2]), 0.0, 4.0);
        const auto beat = p[o2Beat] + 3.5 * pg[pgBeat];
        const auto oct1 = 12.0 * std::round (p[o1Oct]);
        const auto oct2 = 12.0 * std::round (p[o2Oct]) + std::round (p[o2Semi]) + p[o2Fine] / 100.0;

        for (int i = 0; i < n; ++i)
        {
            const auto f = fenv.next();
            const auto a = aenv.next();
            // dérive lente : bruit filtré très bas
            driftState += (noise.next() - driftState) * 0.00005;
            const auto drv1 = driftState * driftAmt * 0.3;

            const auto base1 = g1.next (gType, p[glideMs], sr) + p[tune];
            const auto base2 = (kb2 ? g2.next (gType, p[glideMs], sr) : 60.0) + p[tune];
            const auto dt1 = noteHz (base1 + oct1 + pitch1 + drv1) / sr;
            const auto dt2 = (noteHz (base2 + oct2 + pitch2 - drv1) + beat) / sr;
            const auto dts = noteHz (base1 + oct1 + pitch1 - 12.0) / sr;

            // L'oscillateur 1 est maître : son bouclage synchronise l'oscillateur 2
            double wrap = -1.0;
            const auto s1 = o1.next (sh1, dt1, 0.0, -1.0, &wrap);
            const auto s2 = o2.next (sh2, std::max (1.0e-6, dt2), 0.0, syncOn ? wrap : -1.0);
            const auto ss = sub.next (subSine ? Osc::sine : Osc::pulse, dts, 0.5);

            // réinjection : la sortie du filtre revient dans le mélangeur
            const auto x = s1 * m1 + s2 * m2 + ss * ms + noise.next() * mn + fb * mfb;

            const auto octaves = envOct * f + velOct + (p[keytrack] / 100.0) * (base1 - 48.0) / 12.0 + filtOct;
            const auto fc = std::clamp ((double) p[cutoff] * fastExp2 (octaves), 16.0, 0.45 * sr);
            const auto y = filter.process (x * 0.45, fc, res, sr, mode, drv);
            fb = softClip (y * 1.4);
            const auto out = dc.process (y) * a * gain;
            L[i] += (float) out;
            R[i] += (float) out;
        }
        if (! aenv.isActive()) { active = false; fb = 0.0; }
    }

    double bpm = 120.0;

private:
    /** LFO des bus : triangle, carré, scie (descendante), rampe (montante), échantillonné-bloqué,
        sinus, bruit (aléatoire rapide). Avance par bloc. */
    struct BusLfo
    {
        double phase = 0.0, held = 0.0;
        std::uint32_t rng = 0x5EEDu;
        void reset (double p = 0.0) noexcept { phase = p; }
        double advance (int src, double hz, int n, double sr) noexcept
        {
            const auto prev = phase;
            phase += hz * (double) n / sr;
            if (phase >= 1.0) phase -= std::floor (phase);
            if (phase < prev || src == mono::srcNoise) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; held = (double) (rng >> 1) / 1073741824.0 - 1.0; }
            switch (src)
            {
                case mono::srcTri: return 1.0 - 4.0 * std::abs (phase - 0.5);
                case mono::srcSquare: return phase < 0.5 ? 1.0 : -1.0;
                case mono::srcSaw: return 1.0 - 2.0 * phase;
                case mono::srcRamp: return 2.0 * phase - 1.0;
                case mono::srcSine: return fastSin2Pi (phase);
                default: return held;
            }
        }
    };

    double sr = 48000.0, driftState = 0.0, fb = 0.0, keyNote = 60.0;
    double lastBus[2] {};
    float vel = 1.0f;
    int pendingNote = 60, second = -1;
    bool pending = false, pendingLegato = false, pendingFromNoteOn = false, pendingRelease = false, active = false, latch = false;
    const MonoExpression* expr = nullptr;
    MorphOsc o1, o2;
    Osc sub;
    Ladder filter;
    Dahdsr fenv, aenv;
    BusLfo busLfo[2];
    MonoGlide g1, g2;
    WhiteNoise noise { 0x7A11u };
    DcBlocker dc;
};

/** Une voix : monophonique (priorité basse, haute ou dernière) ou duo paraphonique. */
class MonoBass final : public SynthBase<MonoVoice, 1>
{
public:
    const ModuleDesc& desc() const noexcept override { return mono::desc(); }

    void prepare (double sampleRate, int maxBlock) override
    {
        SynthBase::prepare (sampleRate, maxBlock);
        voices[0].setExpression (&expression);
    }
    void reset() noexcept override { SynthBase::reset(); numHeld = 0; sounding = -1; }

    void setExpression (float bend, float wheel, float pressure) noexcept override
    {
        SynthBase::setExpression (bend, wheel, pressure);
        expression.wheel = wheel; expression.pressure = pressure;
    }

    void noteOn (int note, float velocity) noexcept override
    {
        note = std::clamp (note, 0, 127);
        remove (note);
        if (numHeld < (int) held.size()) held[(size_t) numHeld++] = note;
        lastVel = velocity;
        update (velocity, true);
    }

    void noteOff (int note) noexcept override
    {
        remove (note);
        if (numHeld == 0)
        {
            voices[0].release(); hub.release (0);
            sounding = -1;
            return;
        }
        update (-1.0f, false);
    }

    void allNotesOff() noexcept override { numHeld = 0; sounding = -1; SynthBase::allNotesOff(); }

    void setTempo (double b) noexcept override { SynthBase::setTempo (b); voices[0].bpm = b; }

protected:
    void configure (const float*) noexcept override { setMode (true, true, 1); }

private:
    void remove (int note) noexcept
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] == note)
            {
                for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) (j + 1)];
                --numHeld;
                return;
            }
    }

    /** Choisit la note jouée selon la priorité (duo : basse sur l'osc. 1, haute sur l'osc. 2). */
    void update (float velocity, bool fromNoteOn) noexcept
    {
        const auto* p = hubParams;
        const bool duo = p != nullptr && p[mono::voiceMode] > 0.5f;
        const auto prio = p != nullptr ? (int) p[mono::priority] : 0;
        int lo = 127, hi = 0;
        for (int i = 0; i < numHeld; ++i) { lo = std::min (lo, held[(size_t) i]); hi = std::max (hi, held[(size_t) i]); }
        const auto last = held[(size_t) (numHeld - 1)];
        const auto target = duo ? lo : prio == 0 ? lo : prio == 1 ? hi : last;
        voices[0].setSecond (duo && numHeld >= 2 ? hi : -1);
        if (target == sounding && ! fromNoteOn) return;
        const bool legatoStart = sounding >= 0 && voices[0].isActive();
        // une nouvelle note (même sans changer la note jouée) relance l'enveloppe si le mode le demande
        hub.start (0, target, velocity, legatoStart, hubParams);
        voices[0].start (target, velocity, legatoStart, sounding);
        sounding = target;
        lastNote = target;
    }

    MonoExpression expression;
    std::array<int, 16> held {};
    int numHeld = 0, sounding = -1;
    float lastVel = 1.0f;
};

} // namespace grv::instruments
