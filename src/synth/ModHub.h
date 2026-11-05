/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Modulation commune des synthétiseurs (principe des grands synthés hybrides récents) :
      - enveloppes DAHDSR (retard, attaque, maintien, déclin, niveau, relâchement) bouclables,
        calées sur le tempo si on veut, sensibles à la vélocité ;
      - LFO à 8 formes dont un mode « pas » (jusqu'à 16 pas dessinés), par voix ou global,
        avec retard et fondu d'entrée, synchronisables au tempo ;
      - matrice de modulation : N routages « source × via → destination », profondeur ±100 %.
        Sources par voix : enveloppes et LFO ci-dessus, vélocité, note, molette, pression,
        pitch bend, aléatoire par note, constante. Destination : n'importe quel réglage continu
        de l'instrument (et la hauteur par l'accord).
      - réglages de l'arpégiateur (joué par le moteur, voir Arpeggiator.h) et plage du pitch bend.

    Les réglages sont ajoutés à la description de l'instrument par addModHub() (identifiants
    stables « me1_att », « lf3_rate », « mx1_dst »…). Le calcul se fait par voix à la cadence de
    contrôle (au plus 32 échantillons), sans allocation.
*/

#pragma once

#include "SynthCore.h"
#include "../engine/Params.h"

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace grv::synth
{

using grv::engine::ModuleDesc;
using grv::engine::ParamDesc;
using grv::engine::SectionDesc;

//==============================================================================
/** Enveloppe DAHDSR au comportement analogique (segments exponentiels). Temps en ms.
    Bouclage : tant que la note est tenue, l'enveloppe repart à l'attaque à la fin du déclin
    (principe des enveloppes bouclables des grands monophoniques). */
class Dahdsr
{
public:
    enum Stage { idle, delay, attack, hold, decay, sustain, release };

    void prepare (double sampleRate) noexcept { sr = sampleRate; }
    void reset() noexcept { stage = idle; value = 0.0; counter = 0; }

    void set (double delayMs, double attackMs, double holdMs, double decayMs, double sustainLevel, double releaseMs, bool looping) noexcept
    {
        dly = (int) std::max (0.0, delayMs * 0.001 * sr);
        hld = (int) std::max (0.0, holdMs * 0.001 * sr);
        if (attackMs != a) { a = attackMs; aCoef = coefFor (a, 0.8); }
        if (decayMs != d) { d = decayMs; dCoef = coefFor (d, 0.01); }
        if (releaseMs != r) { r = releaseMs; rCoef = coefFor (r, 0.01); }
        sus = std::clamp (sustainLevel, 0.0, 1.0);
        loop = looping;
    }

    /** retrigger faux : une note liée ne relance pas l'enveloppe (sauf si elle est au repos). */
    void gate (bool on, bool retrigger = true) noexcept
    {
        if (on)
        {
            if (retrigger || stage == idle || stage == release) { stage = dly > 0 ? delay : attack; counter = 0; }
            held = true;
        }
        else
        {
            held = false;
            if (stage != idle) stage = release;
        }
    }

    double next() noexcept
    {
        switch (stage)
        {
            case delay: if (++counter >= dly) { stage = attack; counter = 0; } break;
            case attack:
                value = 1.3 + (value - 1.3) * aCoef;
                if (value >= 1.0) { value = 1.0; stage = hld > 0 ? hold : decay; counter = 0; }
                break;
            case hold: if (++counter >= hld) { stage = decay; counter = 0; } break;
            case decay:
                value = sus + (value - sus) * dCoef;
                if (value - sus < 1.0e-3)
                {
                    if (loop && held) { stage = attack; } // boucle : on repart à l'attaque depuis le niveau atteint
                    else stage = sustain;
                }
                break;
            case sustain: value = sus; break;
            case release:
                value *= rCoef;
                if (value < 1.0e-5) { value = 0.0; stage = idle; }
                break;
            default: break;
        }
        return value;
    }

    bool isActive() const noexcept { return stage != idle; }
    double getValue() const noexcept { return value; }
    Stage getStage() const noexcept { return stage; }

private:
    double coefFor (double ms, double ratio) const noexcept
    {
        const auto samples = std::max (1.0, ms * 0.001 * sr);
        return std::exp (std::log (ratio) / samples);
    }

    double sr = 48000.0, value = 0.0, sus = 0.7;
    double a = -1, d = -1, r = -1, aCoef = 0.0, dCoef = 0.0, rCoef = 0.0;
    int dly = 0, hld = 0, counter = 0;
    bool loop = false, held = false;
    Stage stage = idle;
};

/** Temps calé sur le tempo : la valeur en ms est ramenée à la division de noire la plus proche
    (1/64 de noire à 16 noires). */
inline double syncTimeMs (double ms, double bpm) noexcept
{
    if (ms <= 0.0) return 0.0;
    const auto quarterMs = 60000.0 / std::clamp (bpm, 20.0, 400.0);
    const auto q = std::clamp (ms / quarterMs, 1.0 / 64.0, 16.0);
    const auto k = std::round (std::log2 (q) * 2.0) / 2.0; // demi-puissances de 2 : pointées comprises
    return quarterMs * std::exp2 (k);
}

//==============================================================================
/** LFO de la modulation commune : sortie -1..1 (formes bipolaires). */
class HubLfo
{
public:
    enum Shape { sine = 0, triangle, rampUp, rampDown, square, sampleHold, smoothRandom, steps, numShapes };

    void reset (double p = 0.0) noexcept { phase = p; held = target = smooth = 0.0; fade = 0.0; delayPending = true; runLeft = -1.0; started = false; }
    void restart (double p, std::uint32_t seed) noexcept { phase = p; fade = 0.0; delayPending = true; rng ^= seed * 2654435761u; runLeft = -1.0; started = true; }
    /** v0.43.5 : course limitée (« Une fois » : 1 cycle, « Demi-cycle » : 0,5) puis arrêt sur la dernière valeur. */
    void limitRun (double cycles) noexcept { runLeft = cycles; }
    /** v0.43.5 : « Libre » : la phase part du premier déclenchement, puis le LFO tourne sans être recalé. */
    bool hasStarted() const noexcept { return started; }
    /** Nouvelle note sans recaler la phase (LFO libre par voix) : le retard et le fondu repartent. */
    void restartDelay() noexcept { fade = 0.0; delayPending = true; }

    /** Avance de n échantillons ; values : les 16 pas (mode pas) ; renvoie la valeur. */
    double advance (int shape, double hz, int n, double sr, double fadeSamples, const float* values, int numSteps, double delaySamples = 0.0) noexcept
    {
        // retard : le LFO tourne mais reste muet, puis le fondu d'entrée commence
        if (delayPending) { delayPending = false; delayLeft = delaySamples; }
        const bool waiting = delayLeft > 0.0;
        if (waiting) delayLeft -= (double) n;
        const auto prev = phase;
        auto inc = hz * (double) n / sr;
        if (runLeft >= 0.0) { inc = std::min (inc, runLeft); runLeft -= inc; }
        phase += inc;
        bool wrapped = false;
        if (phase >= 1.0) { phase -= std::floor (phase); wrapped = inc > 0.0; }
        if (wrapped || phase < prev)
        {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            held = (double) (rng >> 1) / 1073741824.0 - 1.0;
        }
        fade = waiting ? 0.0 : fadeSamples <= 0.0 ? 1.0 : std::min (1.0, fade + (double) n / fadeSamples);
        double v;
        switch ((Shape) std::clamp (shape, 0, (int) numShapes - 1))
        {
            case sine: v = fastSin2Pi (phase); break;
            case triangle: v = 1.0 - 4.0 * std::abs (phase - 0.5); break;
            case rampUp: v = 2.0 * phase - 1.0; break;
            case rampDown: v = 1.0 - 2.0 * phase; break;
            case square: v = phase < 0.5 ? 1.0 : -1.0; break;
            case sampleHold: v = held; break;
            case smoothRandom:
                smooth += (held - smooth) * std::min (1.0, hz * (double) n / sr * 4.0);
                v = smooth;
                break;
            default:
            {
                const auto count = std::clamp (numSteps, 1, 16);
                const auto k = std::min (count - 1, (int) (phase * (double) count));
                v = values != nullptr ? (double) values[k] / 100.0 : 0.0;
                break;
            }
        }
        return v * fade;
    }

private:
    double phase = 0.0, held = 0.0, target = 0.0, smooth = 0.0, fade = 1.0, delayLeft = 0.0, runLeft = -1.0;
    bool delayPending = true, started = false;
    std::uint32_t rng = 0x2F6B1D3u;
};

//==============================================================================
// Description

struct HubSpec
{
    int envs = 2;            // enveloppes DAHDSR (0..4)
    int lfos = 2;            // LFO (0..4)
    int slots = 16;          // routages de la matrice (0..32)
    int firstEnv = 1;        // numéro affiché de la 1re enveloppe (« Env. mod 1 »…)
    int firstLfo = 3;        // numéro affiché du 1er LFO (les synthés ont souvent déjà LFO 1 et 2)
    bool arp = true;         // réglages de l'arpégiateur (joué par le moteur)
    bool arpSteps = false;   // arpège à pas : transposition et vélocité de 16 pas
    std::string envName = "Env. mod";
    // Enveloppes et LFO de l'instrument lui-même, offerts comme sources de la matrice (avant ceux
    // du hub) : leurs noms, dans l'ordre. La voix les fournit par modSources().
    std::vector<std::string> extEnvs, extLfos;
    int macros = 0;          // macros : boutons libres, sources de la matrice (0..8)
    // v0.47 : sources propres à l'instrument, calculées pour tout l'instrument (physique de la bille,
    // couloirs de séquence…), placées APRÈS les macros : la liste des autres instruments ne change pas
    std::vector<std::string> extraSources;
};

/** Nombre de places d'enveloppes et de LFO parmi les sources (celles de l'instrument puis celles
    du hub), de macros (sources réglées à la main). */
constexpr int hubEnvMax = 5, hubLfoMax = 5, hubMacroMax = 8, hubExtraMax = 16;

enum HubSource { srcNone = 0, srcEnv1, srcEnv2, srcEnv3, srcEnv4, srcEnv5, srcLfo1, srcLfo2, srcLfo3, srcLfo4, srcLfo5,
                 srcVelocity, srcNote, srcWheel, srcPressure, srcBend, srcRandom, srcConstant,
                 srcMacro1, srcMacro2, srcMacro3, srcMacro4, srcMacro5, srcMacro6, srcMacro7, srcMacro8, numHubSources };

inline std::vector<std::string> hubSourceNames (const HubSpec& s)
{
    std::vector<std::string> v { "Aucune" };
    const auto ne = (int) s.extEnvs.size(), nl = (int) s.extLfos.size();
    // les places sans enveloppe ni LFO restent dans la liste (numéros de source stables) : « — »
    for (int i = 0; i < hubEnvMax; ++i)
        v.push_back (i < ne ? s.extEnvs[(size_t) i] : i - ne < s.envs ? s.envName + " " + std::to_string (s.firstEnv + i - ne) : "\xE2\x80\x94");
    for (int i = 0; i < hubLfoMax; ++i)
        v.push_back (i < nl ? s.extLfos[(size_t) i] : i - nl < s.lfos ? "LFO " + std::to_string (s.firstLfo + i - nl) : "\xE2\x80\x94");
    for (const char* n : { "V\xC3\xA9locit\xC3\xA9", "Note", "Molette", "Pression", "Pitch bend", "Al\xC3\xA9" "atoire", "Constante" }) v.push_back (n);
    for (int i = 0; i < hubMacroMax; ++i) v.push_back (i < s.macros ? "Macro " + std::to_string (i + 1) : "\xE2\x80\x94");
    for (int i = 0; i < std::min ((int) s.extraSources.size(), hubExtraMax); ++i) v.push_back (s.extraSources[(size_t) i]); // v0.47
    return v;
}

inline const std::vector<std::string>& hubLfoShapes()
{
    static const std::vector<std::string> v { "Sinus", "Triangle", "Scie montante", "Scie descendante", "Carr\xC3\xA9",
                                              "\xC3\x89" "chantillonn\xC3\xA9", "Al\xC3\xA9" "atoire liss\xC3\xA9", "Pas" };
    return v;
}

inline const std::vector<std::string>& arpModes()
{
    // ordre de Arpeggiator::Mode : les nouveaux modes s'ajoutent à la fin
    static const std::vector<std::string> v { "Arr\xC3\xAAt", "Haut", "Bas", "Haut-bas", "Bas-haut", "Ordre jou\xC3\xA9", "Al\xC3\xA9" "atoire", "Accord",
                                              "Haut-bas r\xC3\xA9p\xC3\xA9t\xC3\xA9", "Bas-haut r\xC3\xA9p\xC3\xA9t\xC3\xA9",
                                              "Convergent", "Divergent", "Convergent-divergent", "P\xC3\xA9" "dale grave", "P\xC3\xA9" "dale aigu\xC3\xAB",
                                              "Sauts", "Marches haut", "Marches bas", "M\xC3\xA9lang\xC3\xA9", "Marche al\xC3\xA9" "atoire",
                                              "Jou\xC3\xA9 invers\xC3\xA9", "Haut doubl\xC3\xA9" };
    return v;
}
inline const std::vector<std::string>& arpRates()
{
    static const std::vector<std::string> v { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/8 point\xC3\xA9" "e", "1/32T" };
    return v;
}
/** Durée d'un pas d'arpège en noires. */
inline double arpRateQuarters (int i) noexcept
{
    static constexpr double q[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 0.75, 1.0 / 12.0 };
    return q[std::clamp (i, 0, 7)];
}

/** Ajoute la modulation commune à une description d'instrument. Les destinations de la matrice
    sont tous les réglages continus déjà décrits (nommés « Section · Réglage »). */
inline void addModHub (ModuleDesc& m, const HubSpec& spec)
{
    using grv::engine::P;
    using grv::engine::C;
    using grv::engine::I;

    // Destinations : réglages continus existants
    std::vector<std::string> dstNames { "Aucune" };
    m.modDests.clear();
    m.modDests.push_back (-1);
    for (size_t i = 0; i < m.params.size(); ++i)
    {
        const auto& p = m.params[i];
        if (! p.choices.empty()) continue;
        std::string section;
        for (const auto& s : m.sections)
            for (const auto& id : s.params) if (id == p.id && section.empty()) section = s.name;
        dstNames.push_back (section.empty() ? p.name : section + " \xC2\xB7 " + p.name);
        m.modDests.push_back ((int) i);
    }

    const auto sources = hubSourceNames (spec);
    const auto extE = std::clamp ((int) spec.extEnvs.size(), 0, hubEnvMax), extL = std::clamp ((int) spec.extLfos.size(), 0, hubLfoMax);
    const auto envs = std::clamp (spec.envs, 0, hubEnvMax - extE), lfos = std::clamp (spec.lfos, 0, hubLfoMax - extL), slots = std::clamp (spec.slots, 0, 32);
    const auto macros = std::clamp (spec.macros, 0, hubMacroMax);

    for (int e = 0; e < envs; ++e)
    {
        const auto k = "me" + std::to_string (e + 1) + "_";
        const auto name = spec.envName + " " + std::to_string (spec.firstEnv + e);
        m.params.push_back (P (k + "dly", "Retard", "ms", 0, 4000, 0, 0.3f));
        m.params.push_back (P (k + "att", "Attaque", "ms", 0.5f, 8000, 5, 0.3f));
        m.params.push_back (P (k + "hld", "Tenue", "ms", 0, 4000, 0, 0.3f));
        m.params.push_back (P (k + "dec", "D\xC3\xA9" "clin", "ms", 5, 12000, 400, 0.3f));
        m.params.push_back (P (k + "sus", "Niveau", "%", 0, 100, 0));
        m.params.push_back (P (k + "rel", "Rel\xC3\xA2" "chement", "ms", 5, 12000, 300, 0.3f));
        m.params.push_back (C (k + "loop", "Boucle", { "Non", "Oui" }));
        m.params.push_back (C (k + "sync", "Temps cal\xC3\xA9s", { "Libres", "Sur le tempo" }));
        m.params.push_back (P (k + "vel", "V\xC3\xA9locit\xC3\xA9", "%", 0, 100, 0));
        m.sections.push_back ({ "modenv" + std::to_string (e + 1), name, "env",
                                { k + "dly", k + "att", k + "hld", k + "dec", k + "sus", k + "rel", k + "loop", k + "sync" }, "modenv" });
    }
    for (int l = 0; l < lfos; ++l)
    {
        const auto k = "lf" + std::to_string (spec.firstLfo + l) + "_";
        const auto name = "LFO " + std::to_string (spec.firstLfo + l);
        m.params.push_back (C (k + "shape", "Forme", hubLfoShapes()));
        m.params.push_back (P (k + "rate", "Vitesse", "Hz", 0.02f, 40, 2, 0.35f));
        m.params.push_back (C (k + "sync", "Synchro", lfoSyncChoices()));
        // v0.43.5 : modes de déclenchement ajoutés à la fin (projets plus anciens convertis, format 19)
        m.params.push_back (C (k + "mode", "Mode", { "Par voix", "Global", "Par voix libre", "Une fois", "Tenu", "Demi-cycle" }));
        m.params.push_back (P (k + "fade", "Fondu d'entr\xC3\xA9" "e", "ms", 0, 5000, 0, 0.35f));
        m.params.push_back (P (k + "phase", "Phase", "\xC2\xB0", 0, 360, 0));
        m.params.push_back (I (k + "steps", "Pas", "", 2, 16, 16));
        std::vector<std::string> stepIds;
        for (int s = 0; s < 16; ++s)
        {
            m.params.push_back (P (k + "s" + std::to_string (s + 1), "Pas " + std::to_string (s + 1), "%", -100, 100, 0));
            stepIds.push_back (k + "s" + std::to_string (s + 1));
        }
        // retard à chaque note (v0.13.2), à la fin du groupe : les positions des autres réglages ne bougent pas
        m.params.push_back (P (k + "delay", "Retard", "ms", 0, 10000, 0, 0.35f));
        m.sections.push_back ({ "hublfo" + std::to_string (l + 1), name, "lfo",
                                { k + "shape", k + "sync", k + "rate", k + "mode", k + "delay", k + "fade", k + "phase", k + "steps" }, "modlfo" });
        m.sections.push_back ({ "hublfo" + std::to_string (l + 1) + "a", name + " \xC2\xB7 pas 1-8", "lfosteps",
                                std::vector<std::string> (stepIds.begin(), stepIds.begin() + 8), "modlfo" });
        m.sections.push_back ({ "hublfo" + std::to_string (l + 1) + "b", name + " \xC2\xB7 pas 9-16", "lfosteps",
                                std::vector<std::string> (stepIds.begin() + 8, stepIds.end()), "modlfo" });
    }
    if (macros > 0)
    {
        std::vector<std::string> ids;
        for (int i = 0; i < macros; ++i)
        {
            m.params.push_back (P ("macro" + std::to_string (i + 1), "Macro " + std::to_string (i + 1), "%", 0, 100, 0));
            ids.push_back ("macro" + std::to_string (i + 1));
        }
        m.sections.push_back ({ "macros", "Macros", "macros", ids, "matrix" });
    }
    for (int s = 0; s < slots; ++s)
    {
        const auto k = "mx" + std::to_string (s + 1) + "_";
        m.params.push_back (C (k + "src", "Source", sources));
        m.params.push_back (C (k + "via", "Via", sources));
        m.params.push_back (C (k + "dst", "Destination", dstNames));
        m.params.push_back (P (k + "amt", "Profondeur", "%", -100, 100, 0));
    }
    for (int s = 0; s < slots; s += 2)
    {
        std::vector<std::string> ids;
        for (int k = s; k < std::min (slots, s + 2); ++k)
            for (const char* f : { "src", "via", "dst", "amt" }) ids.push_back ("mx" + std::to_string (k + 1) + "_" + f);
        m.sections.push_back ({ "matrix" + std::to_string (s / 2 + 1), "Matrice " + std::to_string (s + 1) + "-" + std::to_string (std::min (slots, s + 2)),
                                "matrix", ids, "matrix" });
    }

    m.params.push_back (P ("pb_range", "Plage du pitch bend", "dt", 0, 24, 2));
    if (spec.arp)
    {
        m.params.push_back (C ("arp_mode", "Arp\xC3\xA8ge", arpModes()));
        m.params.push_back (I ("arp_oct", "Octaves", "", 1, 4, 1));
        m.params.push_back (C ("arp_rate", "Division", arpRates(), 3));
        m.params.push_back (P ("arp_gate", "Dur\xC3\xA9" "e", "%", 5, 100, 60));
        m.params.push_back (C ("arp_hold", "Maintien", { "Non", "Oui" }));
        m.params.push_back (P ("arp_swing", "Swing", "%", 50, 75, 50));
        std::vector<std::string> ids { "arp_mode", "arp_oct", "arp_rate", "arp_gate", "arp_hold", "arp_swing", "pb_range" };
        if (spec.arpSteps)
        {
            m.params.push_back (I ("arp_steps", "Pas", "", 0, 16, 0));
            ids.push_back ("arp_steps");
            std::vector<std::string> tr, ve;
            for (int s = 0; s < 16; ++s)
            {
                m.params.push_back (I ("arp_t" + std::to_string (s + 1), "Transp. " + std::to_string (s + 1), "dt", -24, 24, 0));
                tr.push_back ("arp_t" + std::to_string (s + 1));
            }
            for (int s = 0; s < 16; ++s)
            {
                m.params.push_back (I ("arp_v" + std::to_string (s + 1), "V\xC3\xA9l. " + std::to_string (s + 1), "", 0, 127, 100));
                ve.push_back ("arp_v" + std::to_string (s + 1));
            }
            // v0.43.6 : marche / arrêt de l'arpège, à la fin (les positions des autres réglages ne bougent pas) ;
            // le mode reste choisi quand on l'arrête. « Oui » par défaut : les anciens projets jouent pareil.
            m.params.push_back (C ("arp_on", "Arp\xC3\xA8ge en marche", { "Non", "Oui" }, 1));
            ids.insert (ids.begin(), "arp_on");
            m.sections.push_back ({ "arp", "Arp\xC3\xA8ge", "arp", ids, "arp" });
            m.sections.push_back ({ "arpt1", "Transposition 1-8", "arpsteps", std::vector<std::string> (tr.begin(), tr.begin() + 8), "arp" });
            m.sections.push_back ({ "arpt2", "Transposition 9-16", "arpsteps", std::vector<std::string> (tr.begin() + 8, tr.end()), "arp" });
            m.sections.push_back ({ "arpv1", "V\xC3\xA9locit\xC3\xA9 1-8", "arpsteps", std::vector<std::string> (ve.begin(), ve.begin() + 8), "arp" });
            m.sections.push_back ({ "arpv2", "V\xC3\xA9locit\xC3\xA9 9-16", "arpsteps", std::vector<std::string> (ve.begin() + 8, ve.end()), "arp" });
        }
        else
        {
            m.params.push_back (C ("arp_on", "Arp\xC3\xA8ge en marche", { "Non", "Oui" }, 1));
            ids.insert (ids.begin(), "arp_on");
            m.sections.push_back ({ "arp", "Arp\xC3\xA8ge", "arp", ids, "arp" });
        }
    }
    m.hubEnvs = envs; m.hubLfos = lfos; m.hubSlots = slots; m.hubExtEnvs = extE; m.hubExtLfos = extL; m.hubMacros = macros;
}

/** Dans un préréglage, une destination de la matrice s'écrit par l'identifiant du réglage visé :
    { "mx1_dst@", 0 } suivi de l'identifiant dans le nom (« mx1_dst@o1samt »). Résolu ici. */
inline void resolveHubPresetDests (ModuleDesc& m)
{
    for (auto& pr : m.presets)
        for (auto& v : pr.values)
        {
            const auto at = v.first.find ('@');
            if (at == std::string::npos) continue;
            const auto target = m.indexOf (v.first.substr (at + 1));
            v.first = v.first.substr (0, at);
            v.second = 0.0f;
            for (size_t k = 0; k < m.modDests.size(); ++k) if (m.modDests[k] == target && target >= 0) v.second = (float) k;
        }
}

//==============================================================================
/** Calcul de la modulation commune pour N voix (fil audio). */
template <int N>
class ModHub
{
public:
    /** Retrouve les réglages dans la description (hors du fil audio). */
    void bind (const ModuleDesc& d)
    {
        desc = &d;
        envs = d.hubEnvs; lfos = d.hubLfos; slots = d.hubSlots; extEnvs = d.hubExtEnvs; extLfos = d.hubExtLfos; macros = d.hubMacros;
        macroBase = macros > 0 ? d.indexOf ("macro1") : -1;
        envBase = envs > 0 ? d.indexOf ("me1_dly") : -1;
        lfoBase = -1;
        for (const auto& p : d.params) if (p.id.size() > 8 && p.id.compare (0, 2, "lf") == 0 && p.id.find ("_shape") != std::string::npos) { lfoBase = d.indexOf (p.id); break; }
        slotBase = slots > 0 ? d.indexOf ("mx1_src") : -1;
        bendRange = d.indexOf ("pb_range");
        tune = d.indexOf ("tune");
        count = std::min ((int) d.params.size(), grv::engine::instrumentSlots);
        active = slotBase >= 0 || bendRange >= 0; // sans accord, le pitch bend passe par la matrice seulement
    }

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        for (auto& v : voice) for (auto& e : v.env) e.prepare (sampleRate);
    }
    void reset() noexcept
    {
        for (auto& v : voice) { for (auto& e : v.env) e.reset(); for (auto& l : v.lfo) l.reset(); v.gateOn = false; v.holdValue.fill (0.0); v.holdPending.fill (false); }
        for (auto& l : globalLfo) l.reset();
    }

    bool enabled() const noexcept { return active; }

    void setExpression (float b, float w, float pr) noexcept { bendT = b; wheelT = w; pressureT = pr; }

    /** Une voix démarre (legato : pas de relance des enveloppes ni des LFO par voix). */
    void start (int i, int note, float velocity, bool legato, const float* p) noexcept
    {
        if (! active || i < 0 || i >= N) return;
        auto& v = voice[(size_t) i];
        v.note = note;
        if (velocity >= 0.0f) v.velocity = velocity;
        if (! legato)
        {
            v.ext.fill (0.0);
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            v.random = (double) (rng >> 1) / 1073741824.0 - 1.0;
            for (int e = 0; e < envs; ++e) v.env[(size_t) e].gate (true, true);
            for (int l = 0; l < lfos && p != nullptr; ++l)
            {
                const auto mode = (int) p[lfoBase + l * lfoStride + 3];
                auto& lf = v.lfo[(size_t) l];
                const auto ph = p[lfoBase + l * lfoStride + 5] / 360.0;
                const auto seed = (std::uint32_t) (note * 7919 + i);
                // 0 par voix : recalé à chaque note ; 2 libre : recalé à la 1re note seulement, puis il tourne ;
                // 3 une fois / 5 demi-cycle : recalé, un cycle (un demi) puis arrêt ; 4 tenu : il tourne,
                // la valeur du moment est prise à la note et gardée jusqu'à la suivante
                if (mode == 0) lf.restart (ph, seed);
                else if (mode == 2 || mode == 4) { if (! lf.hasStarted()) lf.restart (ph, seed); else lf.restartDelay(); }
                else if (mode == 3 || mode == 5) { lf.restart (ph, seed); lf.limitRun (mode == 3 ? 1.0 : 0.5); }
                if (mode == 4) v.holdPending[(size_t) l] = true;
            }
        }
        else for (int e = 0; e < envs; ++e) v.env[(size_t) e].gate (true, false);
        v.gateOn = true;
    }
    void release (int i) noexcept
    {
        if (! active || i < 0 || i >= N) return;
        for (int e = 0; e < envs; ++e) voice[(size_t) i].env[(size_t) e].gate (false);
        voice[(size_t) i].gateOn = false;
    }

    /** Valeurs des enveloppes et LFO propres à l'instrument pour la voix i (fin du bloc rendu ;
        elles servent au bloc suivant). */
    void setExternal (int i, const double* env4, const double* lfo4) noexcept // hubEnvMax et hubLfoMax valeurs
    {
        if (! active || i < 0 || i >= N) return;
        auto& v = voice[(size_t) i];
        for (int k = 0; k < extEnvs; ++k) v.ext[(size_t) k] = env4[k];
        for (int k = 0; k < extLfos; ++k) v.ext[(size_t) (hubEnvMax + k)] = lfo4[k];
    }
    bool wantsExternal() const noexcept { return active && (extEnvs > 0 || extLfos > 0); }

    /** v0.47 : valeurs des sources propres à tout l'instrument (HubSpec::extraSources), pour ce bloc. */
    void setExtra (int k, double value) noexcept { if (k >= 0 && k < hubExtraMax) extra[(size_t) k] = value; }

    /** Début de bloc : lissage des commandes d'expression, valeurs de base des destinations,
        hauteur du pitch bend. Renvoie les réglages communs (bend appliqué à l'accord). */
    const float* beginBlock (const float* p, int n, double bpm) noexcept
    {
        bpmNow = bpm; blockN = n;
        const auto k = std::min (1.0, (double) n / (0.004 * sr));
        bend += (bendT - bend) * (float) k; wheel += (wheelT - wheel) * (float) k; pressure += (pressureT - pressure) * (float) k;
        if (! active) return p;
        std::copy (p, p + count, shared.begin());
        if (tune >= 0 && bendRange >= 0) shared[(size_t) tune] += bend * p[bendRange];
        // routages actifs et valeur normalisée de leur destination
        numActive = 0;
        for (int s = 0; s < slots; ++s)
        {
            const auto b = slotBase + s * 4;
            const auto src = (int) p[b], via = (int) p[b + 1], dst = (int) p[b + 2];
            const auto amt = p[b + 3] / 100.0f;
            if (src <= srcNone || dst <= 0 || amt == 0.0f || dst >= (int) desc->modDests.size()) continue;
            const auto target = desc->modDests[(size_t) dst];
            if (target < 0 || target >= count) continue;
            auto& a = act[(size_t) numActive++];
            a.src = src; a.via = via; a.target = target; a.amount = amt;
            a.desc = &desc->params[(size_t) target];
            a.baseNorm = a.desc->toNorm (shared[(size_t) target]);
        }
        // LFO globaux
        for (int l = 0; l < lfos; ++l)
        {
            const auto b = lfoBase + l * lfoStride;
            globalValue[(size_t) l] = globalLfo[(size_t) l].advance ((int) p[b], lfoHz (p, b), n, sr, 0.0, p + b + 7, (int) p[b + 6]);
        }
        return shared.data();
    }

    /** Réglages vus par la voix i pendant ce bloc (copie modulée, ou les réglages communs). */
    const float* voiceParams (int i, const float* common) noexcept
    {
        if (! active || i < 0 || i >= N) return common;
        auto& v = voice[(size_t) i];
        double src[numHubSources + hubExtraMax] {};
        constexpr int srcCount = numHubSources + hubExtraMax;
        for (int e = 0; e < envs; ++e)
        {
            const auto b = envBase + e * envStride;
            const bool sync = common[b + 7] > 0.5f;
            auto t = [&] (int k) { return sync ? syncTimeMs (common[b + k], bpmNow) : (double) common[b + k]; };
            auto& env = v.env[(size_t) e];
            env.set (t (0), t (1), t (2), t (3), common[b + 4] / 100.0, t (5), common[b + 6] > 0.5f);
            double val = 0.0;
            for (int k = 0; k < blockN; ++k) val = env.next();
            const auto velAmt = common[b + 8] / 100.0;
            src[srcEnv1 + extEnvs + e] = val * (1.0 - velAmt + velAmt * v.velocity);
        }
        for (int l = 0; l < lfos; ++l)
        {
            const auto b = lfoBase + l * lfoStride;
            const auto mode = (int) common[b + 3];
            if (mode == 1) src[srcLfo1 + extLfos + l] = globalValue[(size_t) l];
            else
            {
                const auto val = v.lfo[(size_t) l].advance ((int) common[b], lfoHz (common, b), blockN, sr,
                                                            common[b + 4] * 0.001 * sr, common + b + 7, (int) common[b + 6], common[b + 23] * 0.001 * sr);
                if (mode == 4)
                {
                    if (v.holdPending[(size_t) l]) { v.holdValue[(size_t) l] = val; v.holdPending[(size_t) l] = false; }
                    src[srcLfo1 + extLfos + l] = v.holdValue[(size_t) l];
                }
                else src[srcLfo1 + extLfos + l] = val;
            }
        }
        for (int k = 0; k < extEnvs; ++k) src[srcEnv1 + k] = v.ext[(size_t) k];
        for (int k = 0; k < extLfos; ++k) src[srcLfo1 + k] = v.ext[(size_t) (hubEnvMax + k)];
        for (int k = 0; k < macros; ++k) src[srcMacro1 + k] = common[macroBase + k] / 100.0;
        src[srcVelocity] = v.velocity;
        src[srcNote] = std::clamp ((v.note - 60) / 48.0, -1.0, 1.0);
        src[srcWheel] = wheel;
        src[srcPressure] = pressure;
        src[srcBend] = bend;
        src[srcRandom] = v.random;
        src[srcConstant] = 1.0;
        for (int k = 0; k < hubExtraMax; ++k) src[numHubSources + k] = extra[(size_t) k];
        if (numActive == 0) return common;

        auto& out = v.params;
        std::copy (common, common + count, out.begin());
        // plusieurs routages vers la même destination s'ajoutent
        for (int k = 0; k < numActive; ++k)
        {
            const auto& a = act[(size_t) k];
            auto amt = (double) a.amount * src[std::clamp (a.src, 0, srcCount - 1)];
            if (a.via > srcNone) amt *= src[std::clamp (a.via, 0, srcCount - 1)];
            v.offsets[(size_t) k] = amt;
        }
        for (int k = 0; k < numActive; ++k)
        {
            const auto& a = act[(size_t) k];
            double sum = 0.0;
            bool first = true;
            for (int j = 0; j < numActive; ++j)
            {
                if (act[(size_t) j].target != a.target) continue;
                if (j < k) { first = false; break; }
                sum += v.offsets[(size_t) j];
            }
            if (! first) continue;
            out[(size_t) a.target] = a.desc->toPlain ((float) std::clamp (a.baseNorm + sum, 0.0, 1.0));
        }
        return out.data();
    }

private:
    static constexpr int envStride = 9, lfoStride = 24;

    double lfoHz (const float* p, int b) const noexcept { return lfoSyncHz ((int) p[b + 2], p[b + 1], bpmNow); }

    struct Active { int src = 0, via = 0, target = -1; float amount = 0.0f; float baseNorm = 0.0f; const ParamDesc* desc = nullptr; };
    struct Voice
    {
        std::array<Dahdsr, hubEnvMax> env;
        std::array<HubLfo, hubLfoMax> lfo;
        std::array<float, grv::engine::instrumentSlots> params {};
        std::array<double, 32> offsets {};
        std::array<double, hubEnvMax + hubLfoMax> ext {};
        float velocity = 1.0f;
        int note = 60;
        double random = 0.0;
        bool gateOn = false;
        std::array<double, hubLfoMax> holdValue {}; // v0.43.5 : mode « Tenu »
        std::array<bool, hubLfoMax> holdPending {};
    };

    const ModuleDesc* desc = nullptr;
    std::array<Voice, N> voice;
    std::array<HubLfo, hubLfoMax> globalLfo;
    std::array<double, hubLfoMax> globalValue {};
    std::array<double, hubExtraMax> extra {};
    std::array<float, grv::engine::instrumentSlots> shared {};
    std::array<Active, 32> act {};
    int numActive = 0, envs = 0, lfos = 0, slots = 0, extEnvs = 0, extLfos = 0, macros = 0, macroBase = -1, envBase = -1, lfoBase = -1, slotBase = -1, bendRange = -1, tune = -1, count = 0, blockN = 32;
    bool active = false;
    double sr = 48000.0, bpmNow = 120.0;
    float bend = 0.0f, wheel = 0.0f, pressure = 0.0f, bendT = 0.0f, wheelT = 0.0f, pressureT = 0.0f;
    std::uint32_t rng = 0x9E3779B9u;
};

} // namespace grv::synth
