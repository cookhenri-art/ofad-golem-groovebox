/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Poly hybride » — principe des polyphoniques hybrides récents à oscillateurs numériques et
    chemin de filtre analogique (architecture reprise, ni nom, ni panneau, ni son d'origine) :
      - trois oscillateurs par voix : formes analogiques morphables (sinus → triangle → scie →
        carré → impulsion) OU table d'ondes de 16 formes, avec une commande « Shape » dont l'effet
        se choisit — synchro dure (balayage de l'oscillateur esclave) ou densité (trois copies
        désaccordées de la forme) ;
      - modulation de fréquence entre oscillateurs (3→1, 1→2, 2→3), anneau 1×2, bruit ;
      - saturation avant filtre, filtre multimode 12/24 dB (passe-bas, bande, haut), distorsion
        après filtre ;
      - trois enveloppes (ampli, mod 1, mod 2), deux LFO par voix + deux LFO de la matrice ;
      - matrice de modulation de 16 routages à deux sources (source × via) ;
      - modes de voix : poly, mono, mono legato, mono à 2 voix désaccordées ; arpège. 8 voix.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"
#include "WaveMorph.h" // banque de formes partagée (table d'ondes)

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace hybrid
{
    enum Slot
    {
        voices = 0, glide, level, velAmp, tune, drift, spreadAmt,
        o1Shape, o1Semi, o1Fine, o2Shape, o2Semi, o2Fine, o3Shape, o3Semi, o3Fine,
        mix1, mix2, mix3, mixRing, mixNoise, fm31,
        preDrive, fMode, cutoff, reso, keytrack, envAmt, velFilter, postDist,
        aA, aD, aS, aR, m1A, m1D, m1S, m1R, m2A, m2D, m2S, m2R,
        m2Pitch, m2Shape,
        l1Rate, l1Shape, l1Pitch, l1Shapes, l2Rate, l2Shape, l2Cutoff, l2Amp,
        l1Sync, l2Sync, // v0.9 : synchro au tempo
        // v0.10 : tables d'ondes, Shape (synchro dure / densité), FM croisée, modes de voix
        o1Tbl, o2Tbl, o3Tbl, o1SMod, o2SMod, o3SMod, o1SAmt, o2SAmt, o3SAmt,
        fm12, fm23, vMode, uniDetune,
        l1Delay, l2Delay, // v0.13.2 : retard des LFO à chaque note
        numSlots
    };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> lfoShapes { "Sinus", "Triangle", "Scie", "Carr\xC3\xA9", "Al\xC3\xA9" "atoire" };
            const std::vector<std::string> waveSrc { "Formes analogiques", "Table d'ondes" };
            const std::vector<std::string> shapeFx { "Synchro dure", "Densit\xC3\xA9" };
            ModuleDesc m;
            m.type = "hybrid";
            m.name = "Micropointe";
            m.family = "Polys";
            m.principle = "Polyphonique hybride : 3 oscillateurs num\xC3\xA9riques, filtre multimode, saturation avant et distorsion apr\xC3\xA8s filtre";
            m.params = {
                I ("voices", "Voix", "", 1, 8, 8),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("level", "Niveau", "dB", -24, 6, -4),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 30),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("drift", "D\xC3\xA9rive", "%", 0, 100, 10),
                P ("spread", "\xC3\x89" "talement", "%", 0, 100, 30),

                P ("o1shape", "Forme 1", "", 0, 4, 2), I ("o1semi", "Demi-tons 1", "dt", -24, 24, 0), P ("o1fine", "Fin 1", "ct", -50, 50, 0),
                P ("o2shape", "Forme 2", "", 0, 4, 2), I ("o2semi", "Demi-tons 2", "dt", -24, 24, 0), P ("o2fine", "Fin 2", "ct", -50, 50, 8),
                P ("o3shape", "Forme 3", "", 0, 4, 0), I ("o3semi", "Demi-tons 3", "dt", -24, 24, 12), P ("o3fine", "Fin 3", "ct", -50, 50, 0),
                P ("mix1", "Niveau 1", "%", 0, 100, 70),
                P ("mix2", "Niveau 2", "%", 0, 100, 60),
                P ("mix3", "Niveau 3", "%", 0, 100, 0),
                P ("mixring", "Anneau 1\xC3\x97" "2", "%", 0, 100, 0),
                P ("mixnoise", "Bruit", "%", 0, 100, 0),
                P ("fm31", "FM 3\xE2\x86\x92" "1", "%", 0, 100, 0),

                P ("predrive", "Saturation", "dB", 0, 24, 2),
                C ("fmode", "Type", { "Passe-bas 24", "Passe-bas 12", "Passe-bande", "Passe-haut 12", "Passe-haut 24" }),
                P ("cutoff", "Coupure", "Hz", 20, 18000, 2500, 0.3f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 15),
                P ("keytrack", "Suivi clavier", "%", 0, 100, 50),
                P ("envamt", "Env. 2 \xE2\x86\x92 coupure", "%", -100, 100, 35),
                P ("velfilter", "V\xC3\xA9l. filtre", "%", 0, 100, 20),
                P ("postdist", "Distorsion", "%", 0, 100, 0),

                P ("aa", "Attaque", "ms", 0.5f, 10000, 2, 0.3f), P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 800, 0.3f),
                P ("as", "Maintien", "%", 0, 100, 80), P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 500, 0.3f),
                P ("m1a", "Attaque", "ms", 0.5f, 10000, 2, 0.3f), P ("m1d", "D\xC3\xA9" "clin", "ms", 5, 10000, 500, 0.3f),
                P ("m1s", "Maintien", "%", 0, 100, 30), P ("m1r", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 400, 0.3f),
                P ("m2a", "Attaque", "ms", 0.5f, 10000, 2, 0.3f), P ("m2d", "D\xC3\xA9" "clin", "ms", 5, 10000, 200, 0.3f),
                P ("m2s", "Maintien", "%", 0, 100, 0), P ("m2r", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 200, 0.3f),
                P ("m2pitch", "Env. 3 \xE2\x86\x92 hauteur", "dt", -24, 24, 0),
                P ("m2shape", "Env. 3 \xE2\x86\x92 formes", "%", -100, 100, 0),

                P ("l1rate", "LFO 1 vitesse", "Hz", 0.05f, 30, 5, 0.4f), C ("l1shape", "LFO 1 forme", lfoShapes),
                P ("l1pitch", "LFO 1 \xE2\x86\x92 hauteur", "ct", 0, 100, 0), P ("l1shapes", "LFO 1 \xE2\x86\x92 formes", "%", 0, 100, 0),
                P ("l2rate", "LFO 2 vitesse", "Hz", 0.05f, 30, 0.5f, 0.4f), C ("l2shape", "LFO 2 forme", lfoShapes, 1),
                P ("l2cutoff", "LFO 2 \xE2\x86\x92 coupure", "%", 0, 100, 0), P ("l2amp", "LFO 2 \xE2\x86\x92 ampli", "%", 0, 100, 0),
                C ("l1sync", "LFO 1 synchro", lfoSyncChoices()), C ("l2sync", "LFO 2 synchro", lfoSyncChoices()),

                C ("o1tbl", "Source 1", waveSrc), C ("o2tbl", "Source 2", waveSrc), C ("o3tbl", "Source 3", waveSrc),
                C ("o1smod", "Effet Shape 1", shapeFx), C ("o2smod", "Effet Shape 2", shapeFx), C ("o3smod", "Effet Shape 3", shapeFx),
                P ("o1samt", "Shape 1", "%", 0, 100, 0), P ("o2samt", "Shape 2", "%", 0, 100, 0), P ("o3samt", "Shape 3", "%", 0, 100, 0),
                P ("fm12", "FM 1\xE2\x86\x92" "2", "%", 0, 100, 0),
                P ("fm23", "FM 2\xE2\x86\x92" "3", "%", 0, 100, 0),
                C ("vmode", "Mode de voix", { "Poly", "Mono", "Mono legato", "Mono 2 voix" }),
                P ("unidetune", "\xC3\x89" "cart des 2 voix", "ct", 0, 50, 10),
                C ("l1delay", "LFO 1 retard", lfoDelayChoices()), C ("l2delay", "LFO 2 retard", lfoDelayChoices()),
            };
            m.sections = {
                { "global", "Global", "global", { "vmode", "voices", "glide", "tune", "drift", "spread", "unidetune", "velamp", "level" }, "global" },
                { "osc1", "Oscillateur 1", "osc", { "o1tbl", "o1shape", "o1smod", "o1samt", "o1semi", "o1fine", "fm31" }, "osc" },
                { "osc2", "Oscillateur 2", "osc", { "o2tbl", "o2shape", "o2smod", "o2samt", "o2semi", "o2fine", "fm12" }, "osc" },
                { "osc3", "Oscillateur 3", "osc", { "o3tbl", "o3shape", "o3smod", "o3samt", "o3semi", "o3fine", "fm23" }, "osc" },
                { "mixer", "M\xC3\xA9langeur", "mix", { "mix1", "mix2", "mix3", "mixring", "mixnoise" }, "osc" },
                { "filter", "Filtre", "filter", { "predrive", "fmode", "cutoff", "reso", "keytrack", "envamt", "velfilter", "postdist" }, "filter" },
                { "aenv", "Enveloppe 1 (ampli)", "env", { "aa", "ad", "as", "ar", "velamp", "level" }, "amp" },
                { "menv", "Enveloppes 2 et 3", "env", { "m1a", "m1d", "m1s", "m1r", "m2a", "m2d", "m2s", "m2r" }, "env" },
                { "envmod", "Destinations des enveloppes", "mod", { "envamt", "m2pitch", "m2shape" }, "env" },
                { "lfo1", "LFO 1", "lfo", { "l1sync", "l1rate", "l1shape", "l1pitch", "l1shapes", "l1delay" }, "mod" },
                { "lfo2", "LFO 2", "lfo", { "l2sync", "l2rate", "l2shape", "l2cutoff", "l2amp", "l2delay" }, "mod" },
            };
            m.presets = {
                { "Poly brillant", {} },
                { "Basse FM", { { "o1shape", 0 }, { "o3shape", 0 }, { "o3semi", 12 }, { "fm31", 45 }, { "mix2", 0 }, { "cutoff", 1800 }, { "m1d", 250 }, { "m1s", 0 }, { "predrive", 8 } } },
                { "Lead d\xC3\xA9" "chir\xC3\xA9", { { "o1shape", 3 }, { "o2shape", 2.5f }, { "predrive", 18 }, { "postdist", 40 }, { "reso", 35 }, { "glide", 60 }, { "voices", 1 } } },
                { "Nappe mouvante", { { "o1shape", 1.5f }, { "o2shape", 2.2f }, { "l1shapes", 45 }, { "l1rate", 0.3f }, { "aa", 1200 }, { "ar", 2500 }, { "l2cutoff", 25 } } },
                { "Pluck num\xC3\xA9rique", { { "o1shape", 4 }, { "mixring", 40 }, { "as", 0 }, { "ad", 400 }, { "ar", 400 }, { "envamt", 60 }, { "m1d", 180 }, { "m1s", 0 } } },
                { "Scies denses", { { "o1smod", 1 }, { "o1samt", 70 }, { "o2smod", 1 }, { "o2samt", 55 }, { "o2semi", 12 }, { "cutoff", 4200 }, { "aa", 20 }, { "ar", 900 } } },
                { "Synchro balay\xC3\xA9" "e", { { "o1samt", 30 }, { "mix2", 0 }, { "reso", 25 }, { "mx1_src", 2 }, { "mx1_dst@o1samt", 0 }, { "mx1_amt", 60 } } },
                { "Table vivante", { { "o1tbl", 1 }, { "o2tbl", 1 }, { "o1shape", 1.5f }, { "o2shape", 3 }, { "l1shapes", 30 }, { "l1rate", 0.4f }, { "aa", 600 }, { "ar", 1800 } } },
                { "Basse mono 2", { { "vmode", 3 }, { "unidetune", 14 }, { "o1semi", -12 }, { "o2semi", -12 }, { "cutoff", 700 }, { "envamt", 55 }, { "m1d", 260 }, { "m1s", 10 }, { "predrive", 9 } } },
            };
            HubSpec hub;
            hub.extEnvs = { "Env. 1 (ampli)", "Env. 2", "Env. 3" };
            hub.extLfos = { "LFO 1", "LFO 2" };
            hub.envs = 0; hub.lfos = 2; hub.slots = 16; hub.firstLfo = 3; hub.arp = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m); // banque d'usine : 50 sons par catégories (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

/** Un oscillateur du poly hybride : formes analogiques (MorphOsc) ou table d'ondes, avec l'effet
    « Shape » — synchro dure (esclave accéléré jusqu'à 3 octaves, remis en phase par un maître
    au pas de la note) ou densité (deux copies désaccordées ajoutées). */
struct HybridOsc
{
    MorphOsc main, d1, d2;
    BlepLine bl; // tables : sortie retardée d'un échantillon comme MorphOsc, correction de la synchro
    double master = 0.0, tph = 0.0, tph1 = 0.0, tph2 = 0.0;

    void reset (double p) noexcept { main.reset (p); d1.reset (p + 0.31); d2.reset (p + 0.67); bl.reset(); master = 0.0; tph = p; tph1 = p + 0.31; tph2 = p + 0.67; }

    double next (bool table, double shape, double dt, double fm, int fx, double amt, const WaveBank& wb) noexcept
    {
        dt = std::clamp (dt, 1.0e-7, 0.45);
        amt = std::clamp (amt, 0.0, 1.0);
        if (fx == 0 && amt > 0.0)
        {
            // synchro dure : le maître tourne à la fréquence de la note
            const auto ratio = fastExp2 (amt * 3.0);
            const auto slaveDt = std::min (0.45, dt * ratio);
            if (! table)
            {
                double syncAt = -1.0;
                master += dt;
                if (master >= 1.0) { master -= 1.0; syncAt = std::clamp (1.0 - master / dt, 1.0e-6, 1.0); }
                return main.next (shape, slaveDt, fm, syncAt);
            }
            // table : valeur à l'instant m (avant d'avancer), saut au retour du maître corrigé par
            // BLEP (v0.22) ; la fenêtre adoucit toujours le saut comme avant
            auto val = [&] (double ms) { return tableRead (wb, shape, wrap (ms * ratio + fm), slaveDt) * (1.0 - 0.5 * amt * ms); };
            const auto y = val (master);
            master += dt;
            if (master >= 1.0)
            {
                master -= 1.0;
                const auto syncAt = std::clamp (1.0 - master / dt, 1.0e-6, 1.0);
                bl.step (val (0.0) - val (1.0), syncAt);
            }
            return bl.push (y);
        }
        master += dt; master -= std::floor (master);
        if (fx == 1 && amt > 0.0)
        {
            // densité : ±(4..30) centièmes, copies mêlées à mesure que la commande monte
            const auto c = (4.0 + 26.0 * amt) / 1200.0;
            const auto up = fastExp2 (c), down = fastExp2 (-c);
            const auto g = 1.0 / (1.0 + 1.2 * amt);
            if (! table) return (main.next (shape, dt, fm) + amt * (d1.next (shape, dt * up, fm) + d2.next (shape, dt * down, fm))) * g;
            const auto a = advance (tph, dt), b = advance (tph1, dt * up), cc = advance (tph2, dt * down);
            return bl.push ((tableRead (wb, shape, wrap (a + fm), dt) + amt * (tableRead (wb, shape, wrap (b + fm), dt) + tableRead (wb, shape, wrap (cc + fm), dt))) * g);
        }
        if (! table) return main.next (shape, dt, fm);
        return bl.push (tableRead (wb, shape, wrap (advance (tph, dt) + fm), dt));
    }

private:
    static double wrap (double x) noexcept { return x - std::floor (x); }
    static double advance (double& ph, double dt) noexcept { const auto r = ph; ph += dt; ph -= std::floor (ph); return r; }
    static double tableRead (const WaveBank& wb, double shape, double ph, double dt) noexcept
    {
        // la commande de forme (0..4) parcourt les 16 formes de la table
        const auto pos = std::clamp (shape * 3.75, 0.0, (double) (wb.size() - 1));
        const auto w0 = (int) pos;
        const auto w1 = std::min (w0 + 1, wb.size() - 1);
        const auto f = pos - (double) w0;
        const auto level = WaveBank::mipFor (dt);
        return wb.read (w0, level, ph) * (1.0 - f) + wb.read (w1, level, ph) * f;
    }
};

class HybridVoice
{
public:
    void prepare (double s) { sr = s; aenv.prepare (s); m1.prepare (s); m2.prepare (s); reset(); }
    void reset() noexcept
    {
        o1.reset (0.0); o2.reset (0.3); o3.reset (0.6); ladder.reset(); svf.reset(); aenv.reset(); m1.reset(); m2.reset(); dc.reset();
        active = false; last2 = 0.0;
    }
    void setSpread (int index, int count) noexcept { spread = count > 1 ? (2.0 * index / (count - 1) - 1.0) : 0.0; }
    void seed (std::uint32_t s) noexcept { rng = s; }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; active = true;
        if (! legatoStart) lfoAge = 0.0;
        rng = rng * 1664525u + 1013904223u;
        drift = (double) (rng >> 8) / 8388608.0 - 1.0;
        rng = rng * 1664525u + 1013904223u;
        pan = (double) (rng >> 8) / 8388608.0 - 1.0;
    }
    void release() noexcept { aenv.gate (false); m1.gate (false); m2.gate (false); }
    bool isActive() const noexcept { return active; }

    /** Enveloppes et LFO de la voix, sources de la matrice. */
    void modSources (double* env, double* lfo) const noexcept
    {
        env[0] = lastA; env[1] = lastE2; env[2] = lastE3;
        lfo[0] = lastL1; lfo[1] = lastL2;
    }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace hybrid;
        if (pending)
        {
            pending = false;
            pitch.jump ((double) (p[glide] > 0.5f && from >= 0 ? from : target));
            pitch.to ((double) target);
            if (! legatoPending || ! aenv.isActive()) { aenv.gate (true); m1.gate (true); m2.gate (true); }
        }
        const auto& wb = grv::instruments::morph::bank();
        aenv.set (p[aA], p[aD], p[aS] / 100.0, p[aR]);
        m1.set (p[m1A], p[m1D], p[m1S] / 100.0, p[m1R]);
        m2.set (p[m2A], p[m2D], p[m2S] / 100.0, p[m2R]);
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto gain = dbToGain (p[level]) * velocityGain (vel, p[velAmp] / 100.0);
        const auto drv = dbToGain (p[preDrive]);
        const auto res = p[reso] / 100.0;
        const auto fm = (int) p[fMode];
        const auto dist = p[postDist] / 100.0;
        const auto distGain = 1.0 + dist * 20.0;
        const auto velOct = (p[velFilter] / 100.0) * 2.0 * (vel - 0.5);
        const auto dr = drift * p[hybrid::drift] / 100.0 * 0.12 + spread * p[uniDetune] / 100.0;
        const auto pn = spread != 0.0 ? spread * std::max (0.5, p[spreadAmt] / 100.0) : pan * p[spreadAmt] / 100.0;
        const auto gl = 1.0 - std::max (0.0, pn) * 0.6, gr = 1.0 + std::min (0.0, pn) * 0.6;
        const auto fmAmt = p[fm31] / 100.0, fmA12 = p[fm12] / 100.0, fmA23 = p[fm23] / 100.0;
        const auto mixR = p[mixRing] / 100.0, mixN = p[mixNoise] / 100.0;
        const auto off1 = std::round (p[o1Semi]) + p[o1Fine] / 100.0;
        const auto off2 = std::round (p[o2Semi]) + p[o2Fine] / 100.0 - dr * 0.5;
        const auto off3 = std::round (p[o3Semi]) + p[o3Fine] / 100.0;
        const auto mx1 = p[mix1] / 100.0, mx2 = p[mix2] / 100.0, mx3 = p[mix3] / 100.0;
        const auto ktAmt = p[keytrack] / 100.0, envOct = (p[envAmt] / 100.0) * 7.0, l2Oct = p[l2Cutoff] / 100.0 * 3.0;
        const auto tremolo = 0.5 * p[l2Amp] / 100.0;
        const bool t1 = p[o1Tbl] > 0.5f, t2 = p[o2Tbl] > 0.5f, t3 = p[o3Tbl] > 0.5f;
        const int fx1 = (int) p[o1SMod], fx2 = (int) p[o2SMod], fx3 = (int) p[o3SMod];
        const auto sa1 = p[o1SAmt] / 100.0, sa2 = p[o2SAmt] / 100.0, sa3 = p[o3SAmt] / 100.0;
        const bool need3 = mx3 > 0.0 || fmAmt > 0.0, need2 = mx2 > 0.0 || mixR > 0.0 || fmA23 > 0.0;
        double a = 0.0, e2 = 0.0, e3 = 0.0, l1 = 0.0, l2 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            a = aenv.next(); e2 = m1.next(); e3 = m2.next();
            lfoAge += 1.0 / sr;
            l1 = lfo1.next ((int) p[l1Shape], p[l1Rate], sr) * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) p[l1Delay]));
            l2 = lfo2.next ((int) p[l2Shape], p[l2Rate], sr) * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) p[l2Delay]));
            const auto note = pitch.next (glideCoef) + p[tune] + dr + l1 * p[l1Pitch] / 100.0 + e3 * p[m2Pitch];
            const auto shapeMod = l1 * p[l1Shapes] / 25.0 + e3 * p[m2Shape] / 25.0;

            // FM croisée : 2→3 lit la sortie de 2 de l'échantillon précédent
            const auto s3 = need3 ? o3.next (t3, std::clamp (p[o3Shape] + shapeMod, 0.0, 4.0), noteHz (note + off3) / sr, last2 * fmA23 * 0.5, fx3, sa3, wb) : 0.0;
            const auto s1 = o1.next (t1, std::clamp (p[o1Shape] + shapeMod, 0.0, 4.0), noteHz (note + off1) / sr, s3 * fmAmt * 0.5, fx1, sa1, wb);
            const auto s2 = need2 ? o2.next (t2, std::clamp (p[o2Shape] + shapeMod, 0.0, 4.0), noteHz (note + off2) / sr, s1 * fmA12 * 0.5, fx2, sa2, wb) : 0.0;
            last2 = s2;

            const auto x = s1 * mx1 + s2 * mx2 + s3 * mx3 + s1 * s2 * mixR + (mixN > 0.0 ? noise.next() * mixN : 0.0);

            const auto octaves = envOct * e2 + l2 * l2Oct + velOct + ktAmt * (note - 60.0) / 12.0;
            const auto fc = std::clamp ((double) p[cutoff] * fastExp2 (octaves), 16.0, 0.45 * sr);
            double y;
            switch (fm)
            {
                case 0: y = ladder.process (x * 0.35, fc, res, sr, Ladder::lp24, drv); break;
                case 1: y = ladder.process (x * 0.35, fc, res, sr, Ladder::lp12, drv); break;
                case 2: y = svf.process (softClip (x * 0.35 * drv), fc, res, sr).band * 1.5; break;
                case 3: y = ladder.process (x * 0.35, fc, res, sr, Ladder::hp12, drv); break;
                default: y = ladder.process (x * 0.35, fc, res, sr, Ladder::hp24, drv); break;
            }
            if (dist > 0.0)
            {
                // distorsion numérique : repli doux puis écrêtage
                const auto z = y * distGain;
                const auto folded = fastSin2Pi (0.25 * softClip (z * 0.5) * (1.0 + dist));
                y = (y * (1.0 - dist) + folded * 0.5 * dist);
            }
            const auto out = dc.process (y) * a * gain * (1.0 - tremolo * (1.0 + l2));
            L[i] += (float) (out * gl);
            R[i] += (float) (out * gr);
        }
        lastA = a; lastE2 = e2; lastE3 = e3; lastL1 = l1; lastL2 = l2;
        if (! aenv.isActive()) active = false;
    }

private:
    double sr = 48000.0, drift = 0.0, pan = 0.0, spread = 0.0, last2 = 0.0;
    double lastA = 0.0, lastE2 = 0.0, lastE3 = 0.0, lastL1 = 0.0, lastL2 = 0.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false;
    std::uint32_t rng = 7;
    HybridOsc o1, o2, o3;
    Ladder ladder;
    StateFilter svf;
    Adsr aenv, m1, m2;
    SimpleLfo lfo1, lfo2;
    double lfoAge = 1.0e9;
    Glide pitch;
    WhiteNoise noise { 0xCAFEu };
    DcBlocker dc;
};

class HybridPoly final : public SynthBase<HybridVoice, 8>
{
public:
    HybridPoly()
    {
        (void) morph::bank(); // tables calculées à la création (fil de l'interface), jamais sur le fil audio
        for (int i = 0; i < 8; ++i) voices[(size_t) i].seed (0xA5A5u * (std::uint32_t) (i + 1));
    }
    const ModuleDesc& desc() const noexcept override { return hybrid::desc(); }
protected:
    const float* adjust (const float* p) noexcept override
    { return syncRates (p, hybrid::numSlots, { { hybrid::l1Rate, hybrid::l1Sync }, { hybrid::l2Rate, hybrid::l2Sync } }); }
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[hybrid::voices], 1, 8);
        switch ((int) p[hybrid::vMode])
        {
            case 1: setMode (true, false, v); break;        // mono, chaque note relance les enveloppes
            case 2: setMode (true, true, v); break;         // mono legato
            case 3: setMode (true, true, v, 2); break;      // mono, deux voix désaccordées
            default: setMode (v == 1, true, v); break;      // poly (1 voix : mono legato, comme avant)
        }
    }
};

} // namespace grv::instruments
