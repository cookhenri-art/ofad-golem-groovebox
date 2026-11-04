/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Batteries : 16 pads. Chaque pad a, comme une voix de batterie analogique moderne :
      - sa voix (modèle de circuit, voir DrumVoices.h) : accord, déclin, timbre, attaque,
        2 réglages propres au circuit, saturation ;
      - un filtre analogique multimode (échelle 24 dB, 12 dB, passe-bande, passe-haut, réjection)
        avec son enveloppe (attaque, déclin, profondeur ±7 octaves) ;
      - un VCA à enveloppe attaque / maintien / déclin, niveau, panoramique, sensibilité à la
        vélocité et groupe d'étouffement (un pad du groupe coupe les autres, principe MPC) ;
      - un insert léger (saturation, filtres, compression, réduction de bits) et deux envois
        vers les retours A et B.
    Chaque pad peut recevoir un échantillon à la place de sa voix.
    Trois kits, chacun sur son propre principe :
      - « Batterie analogique » : principe des batteries analogiques modernes à machines : chaque
        pad choisit sa MACHINE (grosses caisses dure, classique, FM, plastique, soyeuse, pointue,
        acoustique ; caisses claires ; rimshots ; clap ; toms ; charleys ; cymbales ; cloche ;
        bruit, impulsion, double oscillateur), chacune avec ses 7 réglages propres (dsp/Machines.h) ;
        un échantillon se SUPERPOSE à la machine (niveau et accord propres) ou la remplace ; un LFO
        par pad (7 formes, vitesse libre ou calée sur le tempo, destination, profondeur, fondu,
        libre / redéclenché / une fois) ;
      - « Batterie 80 » : principe des boîtes analogiques du début des années 80 (circuits en T
        ponté, banc de 6 oscillateurs carrés pour les métaux, longue grosse caisse) ;
      - « Batterie 90 » : principe des boîtes hybrides de la fin des années 80 (grosse caisse à fort
        balayage, caisse claire à deux triangles + bruit, cymbales en basse résolution).
    Les réglages d'usine restent à calibrer au banc A/B.
*/

#pragma once

#include "../dsp/AnalogFilter.h"
#include "../dsp/DrumVoices.h"
#include "../dsp/ImpactVoices.h"
#include "../dsp/Machines.h"
#include "../synth/SynthCore.h"
#include "../engine/Module.h"

#include <array>
#include <functional>
#include <memory>

namespace grv::instruments
{

using namespace grv::engine;

struct PadDef
{
    std::string name;
    std::string kind; // famille (couleur dans l'interface) : kick, snare, clap, rim, hatClosed, hatOpen, tom, cowbell, sample
    std::function<std::unique_ptr<dsp::Voice>()> make; // nullptr : pad d'échantillon
    std::array<float, 9> values;                        // accord, déclin, timbre, attaque, saturation, niveau, pan, extra1, extra2 (%)
    const char* machine = nullptr;                      // batterie à machines : machine d'origine du pad
    std::array<const char*, 9> labels {};               // libellés propres à la voix (nullptr : ceux de la famille)
};

/** Libellés des 9 réglages de la voix (les 7 d'origine puis les 2 propres au circuit). */
inline std::array<std::string, 9> drumLabels (const std::string& kind)
{
    const std::string d = "D\xC3\xA9" "clin", sat = "Saturation", lv = "Niveau", pn = "Panoramique";
    if (kind == "kick")    return { "Accord", d, "Balayage", "Click", sat, lv, pn, "Temps balayage", "Forme" };
    if (kind == "snare")   return { "Accord", d, "Timbre", "Claquant", sat, lv, pn, "Couleur bruit", "Corps" };
    if (kind == "clap")    return { "Accord", d, "Timbre", "\xC3\x89" "cart", sat, lv, pn, "Salves", "R\xC3\xA9verb\xC3\xA9ration" };
    if (kind == "rim")     return { "Accord", d, "Timbre", "Claquant", sat, lv, pn, "Passe-haut", "Couleur" };
    if (kind == "hatClosed" || kind == "hatOpen") return { "Accord", d, "Timbre", "M\xC3\xA9" "tal", sat, lv, pn, "Passe-haut", "R\xC3\xA9sonance" };
    if (kind == "tom")     return { "Accord", d, "Balayage", "Bruit", sat, lv, pn, "Temps balayage", "Couleur" };
    if (kind == "cowbell") return { "Accord", d, "Timbre", "Attaque", sat, lv, pn, "\xC3\x89" "cart", "R\xC3\xA9sonance" };
    if (kind == "sample")  return { "Accord", d, "Filtre", "D\xC3\xA9" "part", sat, lv, pn, "Fin", "Attaque" };
    return { "Accord", d, "Timbre", "Attaque", sat, lv, pn, "Couleur", "Forme" };
}

/** Libellés d'un pad : ceux de sa voix s'il en a, sinon ceux de sa famille. */
inline std::array<std::string, 9> padLabels (const PadDef& pad)
{
    auto l = drumLabels (pad.kind);
    for (size_t i = 0; i < 9; ++i) if (pad.labels[i] != nullptr) l[i] = pad.labels[i];
    return l;
}

inline const char* drumSlotIds[7] { "tune", "decay", "tone", "snap", "drive", "level", "pan" };
inline const char* drumFxIds[5] { "fx", "fxAmt", "fxTone", "sendA", "sendB" };
// Réglages ajoutés en v0.7, rangés après ceux des versions précédentes (les adresses ne bougent pas)
inline const char* drumExtIds[13] { "x1", "x2", "ftype", "fcut", "freso", "fenv", "fatk", "fdec", "aatk", "ahold", "adec", "vel", "choke" };
constexpr int padParams = 7, padFxParams = 5, padFxBase = 16 * padParams;    // 112 : effets des pads
constexpr int padExtParams = 13, padExtBase = padFxBase + 16 * padFxParams;   // 192 : circuit, filtre, VCA
enum PadExt { exX1 = 0, exX2, exFType, exFCut, exFReso, exFEnv, exFAtk, exFDec, exAAtk, exAHold, exADec, exVel, exChoke };
// v0.10, batterie à machines seulement : machine, 3e réglage, couche d'échantillon, LFO du pad
constexpr int padMachParams = 12, padMachBase = padExtBase + 16 * padExtParams; // 400
enum PadMach { mcX3 = 0, mcMachine, mcSampleMode, mcSampleLevel, mcSampleTune, mcLfoShape, mcLfoRate, mcLfoSync, mcLfoDest, mcLfoDepth, mcLfoFade, mcLfoMode };
inline const char* drumMachIds[padMachParams] { "x3", "mach", "smode", "slvl", "stune", "lshape", "lrate", "lsync", "ldest", "ldepth", "lfade", "lmode" };
enum PadLfoDest { ldNone = 0, ldTune, ldDecay, ldTone, ldSnap, ldX1, ldX2, ldX3, ldCutoff, ldReso, ldLevel, ldPan, ldDrive, ldSendA, ldSendB };
constexpr float ampDecayInfinite = 8000.0f; // déclin du VCA au maximum : pas de déclin (la voix seule décide)
// v0.21, batterie à échantillons « Nerve » : chaîne de précalcul par pad (couche de synthèse,
// sous-harmonique, résolution, écrêtage) et GATE (longueur de l'enveloppe), puis réglages du kit
constexpr int padPcParams = 8;
enum PadPc { pcWave = 0, pcLevel, pcPitch, pcDecay, pcSub, pcBits, pcClip, pcGate };
inline const char* drumPcIds[padPcParams] { "pcw", "pcl", "pcp", "pcd", "sub", "bits", "clip", "gate" };

//==============================================================================
/** Insert d'un pad : un effet léger au choix, en mono, avant le panoramique et les envois. */
class PadFx
{
public:
    enum Type { none = 0, saturation, lowPass, highPass, compressor, bits, numTypes };

    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept { svf.reset(); sat.reset(); env = 0.0; held = 0.0; holdCount = 0; }

    /** amount, tone : 0..1. */
    void process (float* x, int n, int type, double amount, double tone) noexcept
    {
        switch (type)
        {
            case saturation:
            {
                const auto drive = 1.0 + amount * 24.0, makeup = 1.0 / dsp::fastTanh (drive * 0.5 + 0.5);
                const auto bright = 0.3 + 0.7 * tone;
                for (int i = 0; i < n; ++i)
                {
                    lp += (x[i] - lp) * bright;
                    x[i] = (float) (sat.process (lp * drive) * makeup * 0.7 + (x[i] - lp)); // ADAA (v0.22)
                }
                break;
            }
            case lowPass:
            case highPass:
            {
                svf.set (20.0 * std::pow (1000.0, tone), 0.6 + amount * 9.0, sr);
                for (int i = 0; i < n; ++i)
                {
                    const auto o = svf.process (x[i]);
                    x[i] = (float) (type == lowPass ? o.low : o.high);
                }
                break;
            }
            case compressor:
            {
                // seuil de 0 à -36 dB, rapport 4:1, attaque 0,1 à 30 ms (tonalité), relâchement 120 ms
                const auto thr = dsp::dbToGain (-36.0 * amount);
                const auto att = std::exp (-1.0 / (sr * (0.0001 + 0.03 * tone)));
                const auto rel = std::exp (-1.0 / (sr * 0.12));
                const auto makeup = dsp::dbToGain (12.0 * amount);
                for (int i = 0; i < n; ++i)
                {
                    const auto a = std::abs ((double) x[i]);
                    env = a > env ? a + (env - a) * att : a + (env - a) * rel;
                    const auto g = env > thr ? std::pow (thr / env, 0.75) : 1.0;
                    x[i] = (float) (x[i] * g * makeup);
                }
                break;
            }
            case bits:
            {
                // résolution de 16 à 3 bits, fréquence divisée de 1 à 32 (tonalité)
                const auto levels = std::pow (2.0, 16.0 - 13.0 * amount);
                const auto hold = 1 + (int) std::lround ((1.0 - tone) * 31.0);
                for (int i = 0; i < n; ++i)
                {
                    if (--holdCount <= 0) { held = std::round (x[i] * levels) / levels; holdCount = hold; }
                    x[i] = (float) held;
                }
                break;
            }
            default: break;
        }
    }

private:
    double sr = 48000.0, env = 0.0, held = 0.0, lp = 0.0;
    int holdCount = 0;
    dsp::Svf svf;
    dsp::AdaaTanh sat;
};

inline ModuleDesc makeKitDesc (const std::string& type, const std::string& name, const std::string& principle,
                               const std::array<PadDef, 16>& pads, bool machines = false)
{
    ModuleDesc m;
    m.type = type; m.name = name; m.family = "Batteries"; m.principle = principle; m.drumKit = true;
    if (machines)
        for (const auto& mc : dsp::machineCatalog())
        {
            m.machineNames.push_back (mc.name);
            m.machineLabels.push_back (std::vector<std::string> (mc.labels.begin(), mc.labels.end()));
        }
    for (int p = 0; p < 16; ++p)
    {
        const auto& pad = pads[(size_t) p];
        const auto labels = padLabels (pad);
        const auto prefix = "p" + std::string (p < 9 ? "0" : "") + std::to_string (p + 1) + "_";
        const auto& v = pad.values;
        m.params.push_back (P (prefix + "tune", labels[0], "dt", -24, 24, v[0]));
        m.params.push_back (P (prefix + "decay", labels[1], "ms", 10, 4000, v[1], 0.3f));
        m.params.push_back (P (prefix + "tone", labels[2], "%", 0, 100, v[2]));
        m.params.push_back (P (prefix + "snap", labels[3], "%", 0, 100, v[3]));
        m.params.push_back (P (prefix + "drive", labels[4], "dB", 0, 24, v[4]));
        m.params.push_back (P (prefix + "level", labels[5], "dB", -48, 6, v[5]));
        m.params.push_back (P (prefix + "pan", labels[6], "pan", -100, 100, v[6]));
        m.padNames.push_back (pad.name);
        m.padKinds.push_back (pad.kind);
    }
    // Insert et envois de chaque pad (onglet « Effet et envois »)
    std::vector<SectionDesc> fxSections;
    for (int p = 0; p < 16; ++p)
    {
        const auto prefix = "p" + std::string (p < 9 ? "0" : "") + std::to_string (p + 1) + "_";
        m.params.push_back (C (prefix + "fx", "Effet du pad", { "Aucun", "Saturation", "Passe-bas", "Passe-haut", "Compression", "R\xC3\xA9" "duction de bits" }));
        m.params.push_back (P (prefix + "fxAmt", "Quantit\xC3\xA9", "%", 0, 100, 50));
        m.params.push_back (P (prefix + "fxTone", "Tonalit\xC3\xA9", "%", 0, 100, 50));
        m.params.push_back (P (prefix + "sendA", "Envoi A", "%", 0, 100, 0));
        m.params.push_back (P (prefix + "sendB", "Envoi B", "%", 0, 100, 0));
        SectionDesc s;
        s.id = "padfx" + std::to_string (p + 1);
        s.name = pads[(size_t) p].name;
        s.visual = "padfx";
        s.tab = "padfx";
        for (auto* id : drumFxIds) s.params.push_back (prefix + id);
        fxSections.push_back (s);
    }
    // Circuit (2 réglages de plus), filtre analogique et VCA de chaque pad (v0.7)
    for (int p = 0; p < 16; ++p)
    {
        const auto& pad = pads[(size_t) p];
        const auto labels = padLabels (pad);
        const auto prefix = "p" + std::string (p < 9 ? "0" : "") + std::to_string (p + 1) + "_";
        m.params.push_back (P (prefix + "x1", labels[7], "%", 0, 100, pad.values[7]));
        m.params.push_back (P (prefix + "x2", labels[8], "%", 0, 100, pad.values[8]));
        m.params.push_back (C (prefix + "ftype", "Type de filtre", { "Aucun", "Passe-bas 24 dB", "Passe-bas 12 dB", "Passe-bande",
                                                                      "Passe-haut 12 dB", "Passe-haut 24 dB", "R\xC3\xA9jection" }));
        m.params.push_back (P (prefix + "fcut", "Coupure", "Hz", 20, 20000, 4000, 0.25f));
        m.params.push_back (P (prefix + "freso", "R\xC3\xA9sonance", "%", 0, 100, 20));
        m.params.push_back (P (prefix + "fenv", "Enveloppe filtre", "%", -100, 100, 0));
        m.params.push_back (P (prefix + "fatk", "Attaque filtre", "ms", 0, 2000, 0, 0.3f));
        m.params.push_back (P (prefix + "fdec", "D\xC3\xA9" "clin filtre", "ms", 5, 8000, 300, 0.3f));
        m.params.push_back (P (prefix + "aatk", "Attaque VCA", "ms", 0, 2000, 0, 0.3f));
        m.params.push_back (P (prefix + "ahold", "Maintien VCA", "ms", 0, 2000, 0, 0.3f));
        m.params.push_back (P (prefix + "adec", "D\xC3\xA9" "clin VCA", "ms", 10, ampDecayInfinite, ampDecayInfinite, 0.3f));
        m.params.push_back (P (prefix + "vel", "V\xC3\xA9locit\xC3\xA9", "%", 0, 100, 100));
        m.params.push_back (C (prefix + "choke", "Groupe d'\xC3\xA9touffement", { "Aucun", "Groupe 1", "Groupe 2", "Groupe 3", "Groupe 4" }));
    }
    // Batterie à machines (v0.10) : machine, 3e réglage, couche d'échantillon, LFO du pad
    if (machines)
        for (int p = 0; p < 16; ++p)
        {
            const auto& pad = pads[(size_t) p];
            const auto prefix = "p" + std::string (p < 9 ? "0" : "") + std::to_string (p + 1) + "_";
            const auto mi = pad.machine != nullptr ? dsp::machineIndex (pad.machine) : 0;
            const auto& lab = dsp::machineCatalog()[(size_t) mi].labels;
            m.params.push_back (P (prefix + "x3", lab[6], "%", 0, 100, 50));
            m.params.push_back (C (prefix + "mach", "Machine", m.machineNames, mi));
            m.params.push_back (C (prefix + "smode", "É" "chantillon", { "Remplace la machine", "Superposé" }, 1));
            m.params.push_back (P (prefix + "slvl", "Niveau é" "chantillon", "dB", -48, 6, 0));
            m.params.push_back (P (prefix + "stune", "Accord é" "chantillon", "dt", -24, 24, 0));
            m.params.push_back (C (prefix + "lshape", "Forme du LFO", { "Sinus", "Triangle", "Scie montante", "Scie descendante", "Carré", "Alé" "atoire", "Exponentielle" }));
            m.params.push_back (P (prefix + "lrate", "Vitesse du LFO", "Hz", 0.05f, 30, 2, 0.4f));
            m.params.push_back (C (prefix + "lsync", "Synchro du LFO", synth::lfoSyncChoices()));
            m.params.push_back (C (prefix + "ldest", "Destination du LFO", { "Aucune", "Accord", "Dé" "clin", "Réglage 3", "Réglage 4",
                                                                            "Réglage 5", "Réglage 6", "Réglage 7", "Coupure du filtre", "Résonance",
                                                                            "Niveau", "Panoramique", "Saturation", "Envoi A", "Envoi B" }));
            m.params.push_back (P (prefix + "ldepth", "Profondeur du LFO", "%", -100, 100, 0));
            m.params.push_back (P (prefix + "lfade", "Fondu du LFO", "ms", 0, 4000, 0, 0.35f));
            m.params.push_back (C (prefix + "lmode", "Mode du LFO", { "Libre", "Redé" "clenché", "Une fois" }, 1));
        }

    // Pages par pad : Synthèse (onglet Pad), Filtre, Amplitude, Effet et envois (+ Échantillon, LFO)
    for (int p = 0; p < 16; ++p)
    {
        const auto& pad = pads[(size_t) p];
        const auto prefix = "p" + std::string (p < 9 ? "0" : "") + std::to_string (p + 1) + "_";
        SectionDesc s;
        s.id = "pad" + std::to_string (p + 1);
        s.name = pad.name;
        s.visual = "drumpad:" + pad.kind;
        if (machines) for (auto* id : { "mach", "tune", "decay", "tone", "snap", "x1", "x2", "x3" }) s.params.push_back (prefix + id);
        else for (auto* id : { "tune", "decay", "tone", "snap", "x1", "x2", "drive" }) s.params.push_back (prefix + id);
        m.sections.push_back (s);
    }
    if (machines)
    {
        for (int p = 0; p < 16; ++p)
        {
            const auto prefix = "p" + std::string (p < 9 ? "0" : "") + std::to_string (p + 1) + "_";
            SectionDesc s;
            s.id = "padsmp" + std::to_string (p + 1); s.name = pads[(size_t) p].name; s.visual = "padsmp"; s.tab = "padsmp";
            for (auto* id : { "smode", "slvl", "stune" }) s.params.push_back (prefix + id);
            m.sections.push_back (s);
        }
        for (int p = 0; p < 16; ++p)
        {
            const auto prefix = "p" + std::string (p < 9 ? "0" : "") + std::to_string (p + 1) + "_";
            SectionDesc s;
            s.id = "padlfo" + std::to_string (p + 1); s.name = pads[(size_t) p].name; s.visual = "padlfo"; s.tab = "padlfo";
            for (auto* id : { "lshape", "lsync", "lrate", "ldest", "ldepth", "lfade", "lmode" }) s.params.push_back (prefix + id);
            m.sections.push_back (s);
        }
    }
    for (int p = 0; p < 16; ++p)
    {
        const auto prefix = "p" + std::string (p < 9 ? "0" : "") + std::to_string (p + 1) + "_";
        SectionDesc s;
        s.id = "padfilter" + std::to_string (p + 1);
        s.name = pads[(size_t) p].name;
        s.visual = "padfilter";
        s.tab = "padfilter";
        for (auto* id : { "ftype", "fcut", "freso", "fenv", "fatk", "fdec" }) s.params.push_back (prefix + id);
        m.sections.push_back (s);
    }
    for (int p = 0; p < 16; ++p)
    {
        const auto prefix = "p" + std::string (p < 9 ? "0" : "") + std::to_string (p + 1) + "_";
        SectionDesc s;
        s.id = "padamp" + std::to_string (p + 1);
        s.name = pads[(size_t) p].name;
        s.visual = "padamp";
        s.tab = "padamp";
        for (auto* id : { "aatk", "ahold", "adec", "level", "pan", "vel", "choke", "drive" }) s.params.push_back (prefix + id);
        m.sections.push_back (s);
    }
    for (auto& s : fxSections) m.sections.push_back (std::move (s));
    return m;
}

/** VCA et filtre d'un pad : enveloppes, étouffement. Rendu sur le fil audio, sans allocation. */
struct PadStrip
{
    dsp::AnalogFilter filter;
    double fenv = 0.0, aenv = 1.0, choke = 1.0, chokeTarget = 1.0;
    int fstage = 0, astage = 0; // filtre : 0 repos, 1 attaque, 2 déclin ; VCA : 0 ouvert, 1 attaque, 2 maintien, 3 déclin
    long holdLeft = 0;

    void prepare (double sr) noexcept { filter.prepare (sr); reset(); }
    void reset() noexcept { filter.reset(); fenv = 0.0; aenv = 1.0; choke = chokeTarget = 1.0; fstage = astage = 0; holdLeft = 0; }
    /** wasSilent : la voix ne sonnait plus, le filtre repart de zéro (pas d'état figé qui claque). */
    void trigger (bool wasSilent) noexcept
    {
        if (wasSilent) filter.reset();
        // v0.22 : un pad qui sonne encore repart de son niveau (comme un VCA analogique) au lieu
        // de retomber à zéro d'un coup
        fenv = 0.0; fstage = 1; if (wasSilent) aenv = 0.0; astage = 1; choke = chokeTarget = 1.0;
    }
};

class DrumKit final : public Instrument
{
public:
    DrumKit (const ModuleDesc& description, const std::array<PadDef, 16>& pads) : d (description)
    {
        hasMachines = ! d.machineNames.empty();
        for (int p = 0; p < 16; ++p)
        {
            if (hasMachines)
            {
                // toutes les machines de chaque pad sont créées d'avance (aucune allocation au jeu)
                const auto& cat = dsp::machineCatalog();
                for (size_t k = 0; k < cat.size(); ++k)
                    machineVoices[(size_t) p].push_back (cat[k].make ? cat[k].make (0xD000u + (std::uint32_t) (p * 64 + (int) k) * 2654435761u) : nullptr);
                curMachine[(size_t) p] = pads[(size_t) p].machine != nullptr ? dsp::machineIndex (pads[(size_t) p].machine) : 0;
            }
            if (pads[(size_t) p].make) voices[(size_t) p] = pads[(size_t) p].make();
            closedHat[(size_t) p] = pads[(size_t) p].kind == "hatClosed";
            samplePad[(size_t) p] = pads[(size_t) p].kind == "sample";
            openHat[(size_t) p] = pads[(size_t) p].kind == "hatOpen";
        }
        velSens.fill (1.0f);
        chokeGroup.fill (0);
        // réglages propres à certains kits (absents : -1)
        pcBase = d.indexOf ("p01_pcw");
        distIndex = d.indexOf ("kitdist");
        duckPadIndex = d.indexOf ("duckpad");
    }

    const ModuleDesc& desc() const noexcept override { return d; }

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        for (auto& v : voices) if (v) v->prepare (sr);
        for (auto& pad : machineVoices) for (auto& v : pad) if (v) v->prepare (sr);
        for (auto& v : samples) v.prepare (sr);
        for (auto& f : padFx) f.prepare (sr);
        for (auto& s : strips) s.prepare (sr);
        for (auto& dk : padDeclick) dk.prepare (sr);
        for (int p = 0; p < 16; ++p) if (sampleData[(size_t) p] != nullptr) applySample (p);
        distL.reset(); distR.reset(); distDc[0].prepare (sr, 10.0); distDc[1].prepare (sr, 10.0);
    }

    void reset() noexcept override
    {
        for (auto& v : voices) if (v) v->reset();
        for (auto& pad : machineVoices) for (auto& v : pad) if (v) v->reset();
        for (auto& v : samples) v.reset();
        for (auto& f : padFx) f.reset();
        for (auto& s : strips) s.reset();
        for (auto& dk : padDeclick) dk.reset();
        accCur.fill (1.0f); accTarget.fill (1.0f);
        for (auto& l : lfos) l = {};
        for (auto& pc : precalc) pc = {};
        duckEnv = 0.0; duckGain.fill (1.0f);
        distL.reset(); distR.reset(); distDc[0].reset(); distDc[1].reset();
    }

    void setTempo (double b) noexcept override { bpm = b; }
    void setNextAccent (float amount) noexcept override { nextAccent = amount; }

    void noteOn (int pad, float velocity) noexcept override
    {
        if (pad < 0 || pad >= 16) return;
        // sensibilité à la vélocité : 0 % = toujours au maximum
        const auto sens = velSens[(size_t) pad];
        const auto v = 1.0f - sens * (1.0f - std::clamp (velocity, 0.0f, 1.0f));
        bool wasSilent = true;
        auto* synthVoice = machineVoice (pad);
        const bool hasSample = sampleData[(size_t) pad] != nullptr;
        // échantillon : seul (machine « échantillon seul » ou mode Remplace), ou superposé à la machine
        const bool playSample = hasSample;
        const bool playSynth = synthVoice != nullptr && (! hasSample || layered[(size_t) pad]);
        // accent du séquenceur : comme le bus d'accent d'une boîte analogique, l'impulsion de
        // déclenchement monte (de 5 à 15 V sur les machines du début des années 80, soit jusqu'à
        // 3 fois le niveau) ; chaque voix y réagit selon son circuit (setAccent)
        const auto acc = (double) std::clamp (nextAccent, 0.0f, 1.0f);
        if (playSample) { wasSilent = ! samples[(size_t) pad].isActive(); samples[(size_t) pad].setAccent (acc); samples[(size_t) pad].trigger (v); }
        if (playSynth) { wasSilent = wasSilent && ! synthVoice->isActive(); synthVoice->setAccent (acc); synthVoice->trigger (v); }
        accTarget[(size_t) pad] = (float) (1.0 + accentLevelRange * acc);
        if (wasSilent) accCur[(size_t) pad] = accTarget[(size_t) pad]; // rien ne sonnait : le coup part directement à son niveau
        if (pcBase >= 0)
        {
            auto& pc = precalc[(size_t) pad];
            if (! pc.env.isActive()) { pc.ph = 0.0; pc.subPh = 0.0; }
            else wasSilent = false;
            pc.env.trigger (v);
        }
        hitMask |= 1u << pad; // compresseur interne : vu au début du bloc suivant (réglages à jour)
        strips[(size_t) pad].trigger (wasSilent);
        if (! wasSilent) padDeclick[(size_t) pad].arm(); // la voix relancée saute : compensé en 2 ms
        if (hasMachines && lfos[(size_t) pad].mode != 0) lfos[(size_t) pad].hit = true;

        // Le charley fermé étouffe le charley ouvert
        if (closedHat[(size_t) pad])
            for (int p = 0; p < 16; ++p)
                if (openHat[(size_t) p] && sampleData[(size_t) p] == nullptr)
                    if (auto* ov = machineVoice (p)) ov->choke();
        // Groupe d'étouffement : le pad coupe les autres pads de son groupe (fondu de 4 ms)
        if (const auto g = chokeGroup[(size_t) pad]; g > 0)
            for (int p = 0; p < 16; ++p)
                if (p != pad && chokeGroup[(size_t) p] == g)
                {
                    strips[(size_t) p].chokeTarget = 0.0;
                    if (auto* ov = machineVoice (p)) ov->choke();
                }
    }

    void noteOff (int) noexcept override {}

    void setSample (int pad, const SampleBuffer* s) noexcept override
    {
        if (pad < 0 || pad >= 16) return;
        sampleData[(size_t) pad] = s;
        applySample (pad);
    }

    bool hasPadSends() const noexcept override { return true; }
    void setPadSendBuses (float* aL, float* aR, float* bL, float* bR) noexcept override { busAL = aL; busAR = aR; busBL = bL; busBR = bR; }
    void setPadInserts (const std::array<std::array<Effect*, 2>, 16>* fx, const float* params, const float* bypass, double bpm) noexcept override
    { padChain = fx; padChainParams = params; padChainBypass = bypass; padChainBpm = bpm; }
    void setSidechainSources (const float* const* l, const float* const* r) noexcept override { scSrcL = l; scSrcR = r; }

    void render (float* left, float* right, int n, const float* p) noexcept override
    {
        const auto inv = 1.0f / (float) std::max (1, n);
        const auto chokeCoef = std::exp (-1.0 / (0.004 * sr / 5.0)); // ~4 ms jusqu'au silence
        // compresseur interne (sidechain) : le pad choisi fait baisser tous les autres
        float duckNow = 1.0f;
        if (duckPadIndex >= 0)
        {
            duckSource = (int) p[duckPadIndex] - 1;
            if (duckSource >= 0 && ((hitMask >> duckSource) & 1u) != 0) duckEnv = 1.0; // le pad choisi vient d'être frappé
            const auto amt = std::clamp (p[duckPadIndex + 1] / 100.0f, 0.0f, 1.0f);
            duckEnv *= std::exp (-(double) n / std::max (1.0, (double) p[duckPadIndex + 2] * 0.001 * sr));
            duckNow = 1.0f - amt * (float) duckEnv;
        }
        for (int pad = 0; pad < 16; ++pad)
        {
            const auto* v = p + pad * padParams;
            const auto* fx = p + padFxBase + pad * padFxParams;
            const auto* ex = p + padExtBase + pad * padExtParams;
            velSens[(size_t) pad] = std::clamp (ex[exVel] / 100.0f, 0.0f, 1.0f);
            chokeGroup[(size_t) pad] = (int) ex[exChoke];
            const bool isSample = sampleData[(size_t) pad] != nullptr;
            const auto* mc = hasMachines ? p + padMachBase + pad * padMachParams : nullptr;
            if (hasMachines) updateMachine (pad, mc);
            auto* synthVoice = machineVoice (pad);
            const bool sampleAlone = isSample && (synthVoice == nullptr || ! layered[(size_t) pad]);

            // LFO du pad (batterie à machines) : une valeur par bloc, appliquée aux réglages du bloc
            double lfo = 0.0;
            int dest = ldNone;
            if (hasMachines) { dest = (int) mc[mcLfoDest]; lfo = dest != ldNone ? advanceLfo (pad, mc, n) * (mc[mcLfoDepth] / 100.0) : 0.0; }
            auto mod01 = [&] (double x, int which) { return which == dest ? std::clamp (x + lfo, 0.0, 1.0) : x; };
            const auto tuneMod = dest == ldTune ? 24.0 * lfo : 0.0;
            const auto decayMul = dest == ldDecay ? std::exp2 (3.0 * lfo) : 1.0;
            const auto x3 = hasMachines ? mc[mcX3] / 100.0 : 0.5;
            // Échantillon seul sur un pad de synthèse (sans machine) : lu en entier, sans filtre ni fondu
            const bool wholeSample = sampleAlone && (hasMachines ? curMachine[(size_t) pad] != 0 : ! samplePad[(size_t) pad]);
            const auto vp = wholeSample
                ? dsp::VoiceParams { v[0], 4000.0, 1.0, 0.0, v[4], 1.0, 0.0 }
                : dsp::VoiceParams { v[0] + tuneMod, v[1] * decayMul, mod01 (v[2] / 100.0, ldTone), mod01 (v[3] / 100.0, ldSnap),
                                     v[4] + (dest == ldDrive ? 24.0 * lfo : 0.0),
                                     mod01 (ex[exX1] / 100.0, ldX1), mod01 (ex[exX2] / 100.0, ldX2), mod01 (x3, ldX3) };
            dsp::Voice* voice = sampleAlone ? static_cast<dsp::Voice*> (&samples[(size_t) pad]) : synthVoice;
            // couche d'échantillon superposée : son niveau et son accord
            dsp::SampleVoice* layer = isSample && ! sampleAlone ? &samples[(size_t) pad] : nullptr;

            const auto levelDb = v[5] + (dest == ldLevel ? 24.0f * (float) lfo : 0.0f);
            const auto gain = levelDb <= -47.9f ? 0.0f : std::pow (10.0f, std::min (12.0f, levelDb) / 20.0f);
            const auto pan = std::clamp (v[6] / 100.0f + (dest == ldPan ? (float) lfo : 0.0f), -1.0f, 1.0f);
            const auto duck = duckPadIndex >= 0 && pad != duckSource ? duckNow : 1.0f;
            const auto tl = gain * duck * std::min (1.0f, 1.0f - pan), tr = gain * duck * std::min (1.0f, 1.0f + pan);
            const auto l0 = curL[(size_t) pad], r0 = curR[(size_t) pad];
            curL[(size_t) pad] = tl; curR[(size_t) pad] = tr;
            const auto sa = std::clamp (fx[3] / 100.0f + (dest == ldSendA ? (float) lfo : 0.0f), 0.0f, 1.0f);
            const auto sb = std::clamp (fx[4] / 100.0f + (dest == ldSendB ? (float) lfo : 0.0f), 0.0f, 1.0f);
            const auto ga = sa * sa, gb = sb * sb;
            const auto a0 = curA[(size_t) pad], b0 = curB[(size_t) pad];
            curA[(size_t) pad] = ga; curB[(size_t) pad] = gb;

            // inserts du pad (effets du catalogue) : ils tournent même au repos (traînes d'écho, de réverbe)
            Effect* chainFx[2] { nullptr, nullptr };
            bool chainTrail[2] { false, false }; // v0.35 : contourné mais la queue continue
            if (padChain != nullptr)
                for (int k = 0; k < 2; ++k)
                    if (auto* e = (*padChain)[(size_t) pad][(size_t) k]; e != nullptr)
                    {
                        if (padChainBypass[pad * 2 + k] < 0.5f) chainFx[k] = e;
                        else if (e->trails (padChainParams + (pad * 2 + k) * insertSlots)) { chainFx[k] = e; chainTrail[k] = true; }
                    }
            const bool chain = chainFx[0] != nullptr || chainFx[1] != nullptr;

            // (les voix lisent leurs réglages au rendu, y compris ceux du coup : rien à faire au repos)
            const bool layerOn = layer != nullptr && layer->isActive();
            const float* pcp = pcBase >= 0 ? p + pcBase + pad * padPcParams : nullptr;
            const bool pcOn = pcp != nullptr && precalc[(size_t) pad].env.isActive() && (((int) pcp[pcWave] > 0 && pcp[pcLevel] > 0.0f) || pcp[pcSub] > 0.0f);
            const bool sounding = (voice != nullptr && voice->isActive()) || layerOn || pcOn;
            if (! sounding && ! chain) { padDeclick[(size_t) pad].reset(); continue; }
            std::fill (scratch.begin(), scratch.begin() + n, 0.0f);
            if (sounding)
            {
            if (voice != nullptr && voice->isActive())
            {
                voice->setParams (vp);
                voice->renderAdding (scratch.data(), n);
            }
            if (layerOn)
            {
                layer->setParams (dsp::VoiceParams { mc[mcSampleTune] + tuneMod, 4000.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.5 });
                std::fill (layerBuf.begin(), layerBuf.begin() + n, 0.0f);
                layer->renderAdding (layerBuf.data(), n);
                const auto lg = mc[mcSampleLevel] <= -47.9f ? 0.0f : std::pow (10.0f, mc[mcSampleLevel] / 20.0f);
                for (int i = 0; i < n; ++i) scratch[(size_t) i] += layerBuf[(size_t) i] * lg;
            }
            if (pcp != nullptr) processPrecalc (pad, pcp, v[0], n, pcOn);
            // niveau de l'accent, à la source (avant filtre, VCA et effet du pad, comme sur la machine) ;
            // passage en douceur d'un coup à l'autre quand le précédent sonne encore
            {
                auto& ac = accCur[(size_t) pad];
                const auto target = accTarget[(size_t) pad];
                if (ac != 1.0f || target != 1.0f)
                {
                    const auto step = (target - ac) / (float) n;
                    for (int i = 0; i < n; ++i) { ac += step; scratch[(size_t) i] *= ac; }
                    ac = target;
                }
            }
            auto& st = strips[(size_t) pad];
            // GATE (batterie à échantillons) : l'enveloppe d'amplitude est raccourcie à la proportion choisie
            float exGated[padExtParams];
            const float* exUse = ex;
            if (pcp != nullptr && pcp[pcGate] < 99.5f)
            {
                const auto g = std::clamp (pcp[pcGate] / 100.0f, 0.01f, 1.0f);
                std::copy (ex, ex + padExtParams, exGated);
                exGated[exADec] = std::max (5.0f, std::min (ex[exADec] >= ampDecayInfinite - 1.0f ? (float) v[1] : ex[exADec], (float) v[1] * 1.5f) * g);
                exGated[exAHold] = ex[exAHold] * g;
                exUse = exGated;
            }
            processStrip (st, exUse, n, chokeCoef, dest == ldCutoff ? 5.0 * lfo : 0.0, dest == ldReso ? lfo : 0.0);
            if (st.chokeTarget <= 0.0 && st.choke < 1.0e-4) { if (voice) voice->reset(); if (layer) layer->reset(); } // étouffé : la voix s'arrête
            const auto type = (int) fx[0];
            if (type != PadFx::none) padFx[(size_t) pad].process (scratch.data(), n, type, fx[1] / 100.0, fx[2] / 100.0);
            padDeclick[(size_t) pad].processMono (scratch.data(), n);
            }

            // Le pad passe en stéréo dans ses inserts ; sans insert, il reste mono jusqu'au panoramique
            const float* srcL = scratch.data();
            const float* srcR = scratch.data();
            if (chain)
            {
                std::copy (scratch.begin(), scratch.begin() + n, chainL.begin());
                std::copy (scratch.begin(), scratch.begin() + n, chainR.begin());
                for (int k = 0; k < 2; ++k)
                    if (chainFx[k] != nullptr)
                    {
                        chainFx[k]->setSidechainSources (scSrcL, scSrcR); // v0.24 : compresseur de pad déclenché par une autre piste
                        if (chainTrail[k]) chainFx[k]->processTrails (chainL.data(), chainR.data(), n, padChainParams + (pad * 2 + k) * insertSlots, padChainBpm);
                        else chainFx[k]->process (chainL.data(), chainR.data(), n, padChainParams + (pad * 2 + k) * insertSlots, padChainBpm);
                    }
                srcL = chainL.data(); srcR = chainR.data();
            }

            const auto dl = (tl - l0) * inv, dr = (tr - r0) * inv;
            for (int i = 0; i < n; ++i)
            {
                left[i] += srcL[i] * (l0 + dl * (float) i);
                right[i] += srcR[i] * (r0 + dr * (float) i);
            }
            // Envois du pad, après son niveau et son panoramique
            if (busAL != nullptr && (ga > 0.0f || a0 > 0.0f))
                for (int i = 0; i < n; ++i)
                {
                    const auto k = a0 + (ga - a0) * inv * (float) i;
                    busAL[i] += srcL[i] * k * (l0 + dl * (float) i); busAR[i] += srcR[i] * k * (r0 + dr * (float) i);
                }
            if (busBL != nullptr && (gb > 0.0f || b0 > 0.0f))
                for (int i = 0; i < n; ++i)
                {
                    const auto k = b0 + (gb - b0) * inv * (float) i;
                    busBL[i] += srcL[i] * k * (l0 + dl * (float) i); busBR[i] += srcR[i] * k * (r0 + dr * (float) i);
                }
        }
        hitMask = 0;
        // distorsion de sortie du kit (boîte « Impact ») : courbe quadratique, anti-repliement (ADAA)
        if (distIndex >= 0 && p[distIndex] > 0.2f)
        {
            const auto a = std::clamp ((double) p[distIndex] / 100.0, 0.0, 1.0);
            const auto k = 1.0 + 30.0 * a * a;
            const auto out = (1.0 - 0.35 * a) / dsp::softClip (k);
            for (int i = 0; i < n; ++i)
            {
                left[i] = (float) distDc[0].process (distL.process (left[i] * k) * out);
                right[i] = (float) distDc[1].process (distR.process (right[i] * k) * out);
            }
        }
    }

    static constexpr int maxChunk = 64;

private:
    /** Précalcul d'un pad (batterie à échantillons) sur scratch[0..n) : couche de synthèse (sinus,
        carré, scie, triangle) et sous-harmonique sous la même enveloppe, puis résolution et écrêtage. */
    void processPrecalc (int pad, const float* pc, double tune, int n, bool synthOn) noexcept
    {
        auto& st = precalc[(size_t) pad];
        if (synthOn)
        {
            st.env.setDecay (std::max (5.0f, pc[pcDecay]), sr);
            const auto wave = (int) pc[pcWave];
            const auto lvl = wave > 0 ? pc[pcLevel] / 100.0 : 0.0, subLvl = pc[pcSub] / 100.0;
            const auto f = 55.0 * dsp::fastExp2 ((tune + pc[pcPitch]) / 12.0);
            const auto dt = std::min (0.45, f / sr), dtSub = dt * 0.5;
            for (int i = 0; i < n; ++i)
            {
                const auto e = st.env.next();
                st.ph += dt; st.ph -= std::floor (st.ph);
                st.subPh += dtSub; st.subPh -= std::floor (st.subPh);
                double o = 0.0;
                switch (wave)
                {
                    case 1: o = dsp::fastSin2Pi (st.ph); break;
                    case 2: o = (st.ph < 0.5 ? 1.0 : -1.0) + synth::polyBlep (st.ph, dt) - synth::polyBlep (std::fmod (st.ph + 0.5, 1.0), dt); break;
                    case 3: o = 2.0 * st.ph - 1.0 - synth::polyBlep (st.ph, dt); break;
                    case 4: o = 1.0 - 4.0 * std::abs (st.ph - 0.5); break;
                    default: break;
                }
                scratch[(size_t) i] += (float) ((o * lvl * 0.7 + dsp::fastSin2Pi (st.subPh) * subLvl * 0.8) * e);
            }
        }
        if (pc[pcBits] < 15.9f)
        {
            const auto levels = std::pow (2.0, std::max (1.0f, pc[pcBits]) - 1.0);
            for (int i = 0; i < n; ++i) scratch[(size_t) i] = (float) (std::round (scratch[(size_t) i] * levels) / levels);
        }
        if (pc[pcClip] > 0.5f)
        {
            const auto c = pc[pcClip] / 100.0, k = 1.0 + 12.0 * c * c, norm = 1.0 / dsp::fastTanh (k);
            for (int i = 0; i < n; ++i)
            {
                const auto x = (double) scratch[(size_t) i];
                scratch[(size_t) i] = (float) (x + (st.clip.process (x * k) * norm - x) * c); // ADAA (v0.22)
            }
        }
    }

    struct PrecalcState { double ph = 0.0, subPh = 0.0; dsp::DecayEnv env; dsp::AdaaTanh clip; };
    std::array<PrecalcState, 16> precalc {};
    int pcBase = -1, distIndex = -1, duckPadIndex = -1, duckSource = -1;
    double duckEnv = 0.0;
    std::uint32_t hitMask = 0;
    std::array<float, 16> duckGain {};
    dsp::AdaaSoftClip distL, distR;
    std::array<dsp::DcBlocker, 2> distDc;

    /** Filtre analogique puis VCA du pad, sur scratch[0..n). */
    void processStrip (PadStrip& st, const float* ex, int n, double chokeCoef, double cutOct = 0.0, double resoMod = 0.0) noexcept
    {
        const auto ftype = (int) ex[exFType];
        const auto aDec = ex[exADec];
        const bool ampIdle = st.astage == 0 || (st.astage >= 2 && aDec >= ampDecayInfinite - 1.0f && st.aenv >= 1.0);
        if (ftype == 0 && ampIdle && st.choke >= 1.0 && st.chokeTarget >= 1.0) return;

        const auto samplesOf = [this] (float ms) { return std::max (1.0, (double) ms * 0.001 * sr); };
        const auto fAtkInc = ex[exFAtk] <= 0.05f ? 1.0 : 1.0 / samplesOf (ex[exFAtk]);
        const auto fDecCoef = dsp::decay60Coef (ex[exFDec], sr);
        const auto fOct = ex[exFEnv] / 100.0 * 7.0;
        const auto cut = (double) ex[exFCut] * (cutOct != 0.0 ? std::exp2 (cutOct) : 1.0);
        const auto reso = std::clamp (ex[exFReso] / 100.0 + resoMod, 0.0, 1.0);
        const auto aAtkInc = ex[exAAtk] <= 0.05f ? 1.0 : 1.0 / samplesOf (ex[exAAtk]);
        const auto holdSamples = (long) std::lround ((double) ex[exAHold] * 0.001 * sr);
        const auto aDecCoef = aDec >= ampDecayInfinite - 1.0f ? 1.0 : dsp::decay60Coef (aDec, sr);

        for (int i = 0; i < n; ++i)
        {
            double x = scratch[(size_t) i];
            if (ftype != 0)
            {
                if (st.fstage == 1) { st.fenv += fAtkInc; if (st.fenv >= 1.0) { st.fenv = 1.0; st.fstage = 2; } }
                else if (st.fstage == 2) st.fenv *= fDecCoef;
                const auto c = fOct != 0.0 ? cut * dsp::fastExp2 (fOct * st.fenv) : cut;
                x = st.filter.process (x, ftype, c, reso);
            }
            switch (st.astage)
            {
                case 1: st.aenv += aAtkInc; if (st.aenv >= 1.0) { st.aenv = 1.0; st.astage = 2; st.holdLeft = holdSamples; } break;
                case 2: if (--st.holdLeft <= 0) st.astage = 3; break;
                case 3: st.aenv *= aDecCoef; break;
                default: break;
            }
            st.choke = st.chokeTarget + (st.choke - st.chokeTarget) * chokeCoef;
            scratch[(size_t) i] = (float) (x * st.aenv * st.choke);
        }
    }

    /** Voix de synthèse du pad : sa machine (batterie à machines) ou son circuit fixe. */
    dsp::Voice* machineVoice (int pad) noexcept
    {
        if (! hasMachines) return voices[(size_t) pad].get();
        const auto m = curMachine[(size_t) pad];
        auto& list = machineVoices[(size_t) pad];
        return m > 0 && m < (int) list.size() ? list[(size_t) m].get() : nullptr;
    }

    /** Machine et couche d'échantillon du pad, lues à chaque bloc. Changer de machine coupe l'ancienne. */
    void updateMachine (int pad, const float* mc) noexcept
    {
        const auto m = std::clamp ((int) mc[mcMachine], 0, (int) machineVoices[(size_t) pad].size() - 1);
        if (m != curMachine[(size_t) pad])
        {
            if (auto* old = machineVoice (pad)) old->reset();
            curMachine[(size_t) pad] = m;
        }
        layered[(size_t) pad] = mc[mcSampleMode] > 0.5f && m > 0;
        lfos[(size_t) pad].mode = (int) mc[mcLfoMode];
    }

    /** LFO du pad : renvoie sa valeur (-1..1) pour ce bloc. */
    double advanceLfo (int pad, const float* mc, int n) noexcept
    {
        auto& l = lfos[(size_t) pad];
        const auto hz = synth::lfoSyncHz ((int) mc[mcLfoSync], mc[mcLfoRate], bpm);
        if (l.hit)
        {
            l.hit = false;
            if (l.mode != 0) { l.phase = 0.0; l.done = false; l.fade = 0.0; }
        }
        if (! (l.mode == 2 && l.done))
        {
            l.phase += hz * (double) n / sr;
            if (l.phase >= 1.0)
            {
                if (l.mode == 2) { l.phase = 1.0; l.done = true; }
                else
                {
                    l.phase -= std::floor (l.phase);
                    l.rng ^= l.rng << 13; l.rng ^= l.rng >> 17; l.rng ^= l.rng << 5;
                    l.held = (double) (l.rng >> 1) / 1073741824.0 - 1.0;
                }
            }
        }
        const auto fadeMs = (double) mc[mcLfoFade];
        l.fade = fadeMs <= 0.5 ? 1.0 : std::min (1.0, l.fade + (double) n / (fadeMs * 0.001 * sr));
        const auto ph = l.phase;
        double v;
        switch ((int) mc[mcLfoShape])
        {
            case 0: v = dsp::fastSin2Pi (ph); break;
            case 1: v = 1.0 - 4.0 * std::abs (ph - 0.5); break;
            case 2: v = 2.0 * ph - 1.0; break;
            case 3: v = 1.0 - 2.0 * ph; break;
            case 4: v = ph < 0.5 ? 1.0 : -1.0; break;
            case 5: v = l.held; break;
            default: v = 2.0 * std::exp (-5.0 * ph) - 1.0; break; // exponentielle descendante
        }
        return v * l.fade;
    }

    void applySample (int pad) noexcept
    {
        const auto* s = sampleData[(size_t) pad];
        samples[(size_t) pad].setSample (s ? s->data.data() : nullptr, s ? (int) s->data.size() : 0, s ? s->sampleRate : 44100.0);
    }

    const ModuleDesc& d;
    double sr = 48000.0;
    std::array<std::unique_ptr<dsp::Voice>, 16> voices;
    std::array<dsp::SampleVoice, 16> samples;
    std::array<const SampleBuffer*, 16> sampleData {};
    std::array<float, 16> curL {}, curR {}, curA {}, curB {};
    std::array<PadFx, 16> padFx;
    std::array<PadStrip, 16> strips;
    std::array<synth::Declick, 16> padDeclick;
    std::array<float, 16> velSens {};
    std::array<float, 16> accCur { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, accTarget { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    float nextAccent = 0.0f;
    /** Niveau ajouté par un accent à 100 % : ×2,4 (+7,6 dB) ; avec la vélocité accentuée, un coup
        de vélocité 100 gagne environ 9 dB, l'écart de 5 à 15 V du bus d'accent des machines. */
    static constexpr double accentLevelRange = 1.4;
    std::array<int, 16> chokeGroup {};
    float* busAL = nullptr; float* busAR = nullptr; float* busBL = nullptr; float* busBR = nullptr;
    std::array<bool, 16> closedHat {}, openHat {}, samplePad {};
    std::array<float, maxChunk> scratch {}, layerBuf {};
    std::array<float, maxChunk> chainL {}, chainR {};   // pad passé en stéréo dans ses inserts
    const std::array<std::array<Effect*, 2>, 16>* padChain = nullptr;
    const float* padChainParams = nullptr;
    const float* padChainBypass = nullptr;
    double padChainBpm = 120.0;
    const float* const* scSrcL = nullptr;
    const float* const* scSrcR = nullptr;
    // batterie à machines
    struct PadLfo { double phase = 0.0, held = 0.0, fade = 1.0; bool hit = false, done = false; int mode = 1; std::uint32_t rng = 0x1F0u; };
    bool hasMachines = false;
    std::array<std::vector<std::unique_ptr<dsp::Voice>>, 16> machineVoices;
    std::array<int, 16> curMachine {};
    std::array<bool, 16> layered {};
    std::array<PadLfo, 16> lfos {};
    double bpm = 120.0;
};

//==============================================================================
// Définition des trois kits

using V = std::unique_ptr<dsp::Voice>;
template <typename T, typename... A> std::function<V()> mk (A... a) { return [=] { return std::make_unique<T> (a...); }; }

// Valeurs : accord, déclin (ms), timbre, attaque, saturation (dB), niveau (dB), pan, extra1, extra2 (%).
// Chaque voix a sa propre graine de dérive : deux pads identiques ne sonnent pas exactement pareil.

inline const std::array<PadDef, 16>& kitAnalogPads()
{
    using namespace dsp;
    // Batterie analogique moderne : grosse caisse à oscillateur balayé, caisses claires à
    // résonateurs + bruit, toms en T ponté à balayage marqué, métaux à 6 carrés.
    static const std::array<PadDef, 16> pads {{
        { "Grosse caisse", "kick", mk<KickDrum> (50.0, 48.0, 22.0, 0xA101u), { 0, 450, 50, 35, 6, -6, 0, 50, 15 }, "GC dure" },
        { "Caisse claire", "snare", mk<SnareDrum> (180.0, 330.0, 1400.0, 0.6, 0xA102u), { 0, 220, 55, 60, 3, -6, 0, 50, 50 }, "CC dure" },
        { "Clap", "clap", mk<ClapDrum> (0xA103u), { 0, 300, 50, 50, 0, -6, 0, 50, 63 }, "Clap classique" },
        { "Rimshot", "rim", mk<RimDrum> (1700.0, 480.0, 0xA104u), { 0, 60, 50, 60, 0, -6, 0, 55, 20 }, "Rim dur" },
        { "Charley ferm\xC3\xA9", "hatClosed", mk<HatDrum> (0, 6500.0, 0xA105u), { 0, 60, 60, 70, 0, -10, 0, 50, 15 }, "Charley classique" },
        { "Charley ouvert", "hatOpen", mk<HatDrum> (0, 6500.0, 0xA106u), { 0, 480, 60, 70, 0, -12, 0, 50, 15 }, "Charley classique" },
        { "Tom grave", "tom", mk<TomDrum> (110.0, 12.0, 0xA107u), { -5, 420, 40, 20, 0, -6, -20, 50, 10 }, "Tom classique" },
        { "Tom aigu", "tom", mk<TomDrum> (110.0, 12.0, 0xA108u), { 5, 320, 40, 20, 0, -6, 20, 50, 10 }, "Tom classique" },
        { "Cloche", "cowbell", mk<CowbellDrum> (0xA109u), { 0, 260, 50, 50, 0, -12, 0, 50, 35 }, "Cloche classique" },
        { "Basse longue", "kick", mk<KickDrum> (50.0, 48.0, 22.0, 0xA10Au), { -5, 1500, 25, 10, 9, -6, 0, 60, 30 }, "GC classique" },
        { "Tom m\xC3\xA9" "dium", "tom", mk<TomDrum> (110.0, 12.0, 0xA10Bu), { 0, 360, 40, 20, 0, -6, 0, 50, 10 }, "Tom classique" },
        { "Caisse claire 2", "snare", mk<SnareDrum> (180.0, 330.0, 1400.0, 0.6, 0xA10Cu), { 4, 160, 35, 80, 6, -6, 0, 65, 35 }, "CC classique" },
        { "\xC3\x89" "chantillon 1", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 }, "\xC3\x89" "chantillon seul" },
        { "\xC3\x89" "chantillon 2", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 }, "\xC3\x89" "chantillon seul" },
        { "\xC3\x89" "chantillon 3", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 }, "\xC3\x89" "chantillon seul" },
        { "\xC3\x89" "chantillon 4", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 }, "\xC3\x89" "chantillon seul" },
    }};
    return pads;
}

inline const std::array<PadDef, 16>& kit80Pads()
{
    using namespace dsp;
    // Principe de la boîte analogique du début des années 80 : grosse caisse, toms, conga en
    // T ponté (faible balayage, long déclin), caisse claire à deux T pontés 238 / 476 Hz + bruit,
    // rimshot et claves en T pontés aigus, métaux sur 6 oscillateurs carrés.
    static const std::array<PadDef, 16> pads {{
        { "Grosse caisse", "kick", mk<BridgedTDrum> (48.0, 6.0, 0.0, 0xB101u), { 0, 900, 40, 15, 2, -6, 0, 35, 10 } },
        { "Caisse claire", "snare", mk<SnareDrum> (238.0, 476.0, 2200.0, 0.15, 0xB102u), { 0, 180, 45, 55, 0, -7, 0, 50, 50 } },
        { "Clap", "clap", mk<ClapDrum> (0xB103u), { 0, 260, 45, 45, 0, -7, 0, 50, 63 } },
        { "Rimshot", "rim", mk<RimDrum> (1667.0, 455.0, 0xB104u), { 0, 40, 55, 70, 0, -9, 0, 55, 20 } },
        { "Charley ferm\xC3\xA9", "hatClosed", mk<HatDrum> (0, 6500.0, 0xB105u), { 0, 50, 70, 85, 0, -11, 10, 50, 15 } },
        { "Charley ouvert", "hatOpen", mk<HatDrum> (0, 6500.0, 0xB106u), { 0, 600, 70, 85, 0, -12, 10, 50, 15 } },
        { "Tom grave", "tom", mk<TomDrum> (90.0, 4.0, 0xB107u), { 0, 500, 30, 10, 0, -7, -25, 50, 5 } },
        { "Tom m\xC3\xA9" "dium", "tom", mk<TomDrum> (130.0, 4.0, 0xB108u), { 0, 420, 30, 10, 0, -7, 0, 50, 5 } },
        { "Tom aigu", "tom", mk<TomDrum> (180.0, 4.0, 0xB109u), { 0, 350, 30, 10, 0, -7, 25, 50, 5 } },
        { "Conga", "tom", mk<TomDrum> (330.0, 2.0, 0xB10Au), { 0, 200, 20, 5, 0, -9, 15, 30, 0 } },
        { "Cloche", "cowbell", mk<CowbellDrum> (0xB10Bu), { 0, 300, 45, 50, 0, -12, 0, 50, 35 } },
        { "Cymbale", "hatOpen", mk<HatDrum> (0, 5200.0, 0xB10Cu), { -2, 2500, 45, 90, 0, -14, -15, 40, 10 } },
        { "Maracas", "hatClosed", mk<HatDrum> (0, 8000.0, 0xB10Du), { 0, 40, 80, 0, 0, -14, 20, 70, 15 } },
        { "Claves", "rim", mk<RimDrum> (2500.0, 2500.0, 0xB10Eu), { 0, 35, 100, 20, 0, -11, 0, 55, 0 } },
        { "\xC3\x89" "chantillon 1", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
        { "\xC3\x89" "chantillon 2", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
    }};
    return pads;
}

inline const std::array<PadDef, 16>& kit90Pads()
{
    using namespace dsp;
    // Principe de la boîte hybride de la fin des années 80 : grosse caisse à oscillateur
    // fortement balayé et mis en forme, caisse claire à 2 résonateurs glissés + bruit, toms
    // balayés, cymbales en basse résolution (6 bits).
    static const std::array<PadDef, 16> pads {{
        { "Grosse caisse", "kick", mk<KickDrum> (52.0, 36.0, 14.0, 0xC101u), { 0, 520, 60, 55, 4, -6, 0, 50, 30 } },
        { "Caisse claire", "snare", mk<SnareDrum> (160.0, 220.0, 1000.0, 0.6, 0xC102u), { 0, 200, 60, 65, 2, -7, 0, 50, 50 } },
        { "Clap", "clap", mk<ClapDrum> (0xC103u), { 0, 320, 55, 40, 0, -7, 0, 50, 63 } },
        { "Rimshot", "rim", mk<RimDrum> (1900.0, 520.0, 0xC104u), { 0, 45, 60, 75, 2, -9, 0, 55, 25 } },
        { "Charley ferm\xC3\xA9", "hatClosed", mk<HatDrum> (6, 7000.0, 0xC105u), { 0, 70, 65, 80, 0, -11, 8, 50, 15 } },
        { "Charley ouvert", "hatOpen", mk<HatDrum> (6, 7000.0, 0xC106u), { 0, 520, 65, 80, 0, -12, 8, 50, 15 } },
        { "Tom grave", "tom", mk<TomDrum> (95.0, 10.0, 0xC107u), { 0, 450, 45, 15, 0, -7, -25, 50, 15 } },
        { "Tom m\xC3\xA9" "dium", "tom", mk<TomDrum> (140.0, 10.0, 0xC108u), { 0, 380, 45, 15, 0, -7, 0, 50, 15 } },
        { "Tom aigu", "tom", mk<TomDrum> (200.0, 10.0, 0xC109u), { 0, 320, 45, 15, 0, -7, 25, 50, 15 } },
        { "Crash", "hatOpen", mk<HatDrum> (6, 4800.0, 0xC10Au), { -3, 2200, 40, 70, 0, -14, -20, 40, 10 } },
        { "Ride", "hatOpen", mk<HatDrum> (6, 6000.0, 0xC10Bu), { 2, 1800, 55, 95, 0, -15, 20, 45, 30 } },
        { "Basse longue", "kick", mk<KickDrum> (44.0, 24.0, 25.0, 0xC10Cu), { -2, 1400, 40, 30, 8, -7, 0, 60, 35 } },
        { "\xC3\x89" "chantillon 1", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
        { "\xC3\x89" "chantillon 2", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
        { "\xC3\x89" "chantillon 3", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
        { "\xC3\x89" "chantillon 4", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
    }};
    return pads;
}

inline const ModuleDesc& kitAnalogDesc()
{
    static const ModuleDesc d = makeKitDesc ("drumA", "Machine \xC3\xA9lectronique",
        "Batterie analogique \xC3\xA0 machines : 30 mod\xC3\xA8les de voix au choix par pad, chacun avec ses r\xC3\xA9glages, \xC3\xA9" "chantillon superpos\xC3\xA9, LFO, filtre analogique et VCA par pad",
        kitAnalogPads(), true);
    return d;
}
inline const ModuleDesc& kit80Desc()
{
    static const ModuleDesc d = makeKitDesc ("drum80", "Boum 80",
        "Bo\xC3\xAE" "te analogique du d\xC3\xA9" "but des ann\xC3\xA9" "es 80 : circuits en T pont\xC3\xA9, 6 oscillateurs pour les m\xC3\xA9taux", kit80Pads());
    return d;
}
inline const ModuleDesc& kit90Desc()
{
    static const ModuleDesc d = makeKitDesc ("drum90", "Club 90",
        "Bo\xC3\xAE" "te hybride de la fin des ann\xC3\xA9" "es 80 : grosse caisse balay\xC3\xA9" "e, cymbales en basse r\xC3\xA9solution", kit90Pads());
    return d;
}

//==============================================================================
// v0.21 : boîte « Impact » et batterie à échantillons « Nerve »

inline const std::array<PadDef, 16>& kitImpactPads()
{
    using namespace dsp;
    // Principe des boîtes analogiques compactes et agressives : dix voix (grosse caisse, deux caisses
    // claires, deux toms, cymbale, cloche, charleys, tambour FM), chacune avec sa COULEUR (dernier
    // réglage) ; trois doubles déjà colorés, trois pads d'échantillon.
    using L9 = std::array<const char*, 9>;
    const char* D = "D\xC3\xA9" "clin";
    static const L9 kick { "Accord", D, "Balayage", "Attaque", "Saturation", "Niveau", "Panoramique", "Temps balayage", "Couleur" };
    static const L9 sn1 { "Accord", D, "Timbre", "Claquant", "Saturation", "Niveau", "Panoramique", "Corps", "Couleur" };
    static const L9 sn2 { "Accord", D, "Filtre", "Nettet\xC3\xA9", "Saturation", "Niveau", "Panoramique", "Salves", "Couleur" };
    static const L9 tom { "Accord", D, "Balayage", "Baguette", "Saturation", "Niveau", "Panoramique", "Temps balayage", "Couleur" };
    static const L9 metal { "Accord", D, "Timbre", "M\xC3\xA9tal", "Saturation", "Niveau", "Panoramique", "Passe-haut", "Couleur" };
    static const L9 fm { "Porteuse", D, "Quantit\xC3\xA9 FM", "Claquant", "Saturation", "Niveau", "Panoramique", "Modulateur", "Couleur" };
    static const std::array<PadDef, 16> pads {{
        { "Grosse caisse", "kick", mk<ImpactKick> (46.0, 0xD101u), { 0, 520, 55, 45, 0, -6, 0, 30, 0 }, nullptr, kick },
        { "Caisse claire 1", "snare", mk<ImpactSnare> (185.0, 0xD102u), { 0, 260, 50, 55, 0, -7, 0, 55, 0 }, nullptr, sn1 },
        { "Caisse claire 2", "clap", mk<ImpactSnare2> (0xD103u), { 0, 220, 45, 60, 0, -8, 0, 40, 0 }, nullptr, sn2 },
        { "Tom aigu", "tom", mk<ImpactTom> (190.0, 0xD104u), { 0, 380, 45, 35, 0, -8, 20, 35, 0 }, nullptr, tom },
        { "Tom grave", "tom", mk<ImpactTom> (95.0, 0xD105u), { 0, 520, 45, 35, 0, -7, -20, 35, 0 }, nullptr, tom },
        { "Cymbale", "cymbal", mk<ImpactMetal> (ImpactMetal::cymbal, 0xD106u), { 0, 1800, 50, 60, 0, -14, -15, 50, 0 }, nullptr, metal },
        { "Cloche", "cowbell", mk<CowbellDrum> (0xD107u), { 0, 280, 50, 50, 0, -12, 10, 50, 35 } },
        { "Charley ferm\xC3\xA9", "hatClosed", mk<ImpactMetal> (ImpactMetal::closedHat, 0xD108u), { 0, 70, 55, 55, 0, -11, 8, 50, 0 }, nullptr, metal },
        { "Charley ouvert", "hatOpen", mk<ImpactMetal> (ImpactMetal::openHat, 0xD109u), { 0, 520, 55, 55, 0, -12, 8, 50, 0 }, nullptr, metal },
        { "Tambour FM", "fm", mk<ImpactFm> (110.0, 0xD10Au), { 0, 400, 45, 40, 0, -9, 0, 40, 0 }, nullptr, fm },
        { "Grosse caisse color\xC3\xA9" "e", "kick", mk<ImpactKick> (46.0, 0xD10Bu), { -2, 650, 60, 55, 3, -7, 0, 35, 100 }, nullptr, kick },
        { "Caisse claire color\xC3\xA9" "e", "snare", mk<ImpactSnare> (185.0, 0xD10Cu), { 1, 240, 55, 65, 0, -8, 0, 50, 100 }, nullptr, sn1 },
        { "FM color\xC3\xA9", "fm", mk<ImpactFm> (110.0, 0xD10Du), { 5, 600, 60, 30, 0, -10, 0, 60, 100 }, nullptr, fm },
        { "\xC3\x89" "chantillon 1", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
        { "\xC3\x89" "chantillon 2", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
        { "\xC3\x89" "chantillon 3", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
    }};
    return pads;
}

inline const ModuleDesc& kitImpactDesc()
{
    static const ModuleDesc d = []
    {
        auto m = makeKitDesc ("drumImp", "Dragon Fist",
            "Bo\xC3\xAE" "te analogique compacte et agressive : dix voix, une couleur par voix, distorsion de sortie", kitImpactPads());
        m.params.push_back (P ("kitdist", "Distorsion", "%", 0, 100, 0));
        SectionDesc s;
        s.id = "kitmaster"; s.name = "Sortie du kit"; s.visual = "kit"; s.tab = "kit";
        s.params = { "kitdist" };
        m.sections.push_back (s);
        return m;
    }();
    return d;
}

inline const std::array<PadDef, 16>& kitNervePads()
{
    using namespace dsp;
    // Principe des batteries à échantillons logicielles : chaque pad lit l'échantillon qu'on y dépose ;
    // sans échantillon, une voix de synthèse tient lieu de son de départ. Chaque pad passe par la
    // chaîne de précalcul (onglet Précalcul) puis par son filtre et son enveloppe.
    static const std::array<PadDef, 16> pads {{
        { "Grosse caisse", "kick", mk<KickDrum> (52.0, 40.0, 16.0, 0xE101u), { 0, 480, 55, 50, 4, -6, 0, 45, 25 } },
        { "Grosse caisse 2", "kick", mk<BridgedTDrum> (50.0, 6.0, 0.0, 0xE102u), { 0, 800, 40, 20, 3, -6, 0, 40, 15 } },
        { "Caisse claire", "snare", mk<SnareDrum> (190.0, 340.0, 1600.0, 0.6, 0xE103u), { 0, 220, 55, 65, 3, -7, 0, 50, 50 } },
        { "Clap", "clap", mk<ClapDrum> (0xE104u), { 0, 300, 50, 50, 0, -7, 0, 50, 63 } },
        { "Rimshot", "rim", mk<RimDrum> (1750.0, 500.0, 0xE105u), { 0, 50, 55, 65, 0, -9, 0, 55, 20 } },
        { "Charley ferm\xC3\xA9", "hatClosed", mk<HatDrum> (0, 7000.0, 0xE106u, 1), { 0, 55, 60, 70, 0, -11, 10, 50, 15 } },
        { "Charley ouvert", "hatOpen", mk<HatDrum> (0, 7000.0, 0xE107u, 1), { 0, 450, 60, 70, 0, -12, 10, 50, 15 } },
        { "Shaker", "hatClosed", mk<HatDrum> (0, 9000.0, 0xE108u), { 0, 80, 85, 10, 0, -14, -15, 70, 10 } },
        { "Tom grave", "tom", mk<TomDrum> (100.0, 10.0, 0xE109u), { 0, 420, 40, 20, 0, -7, -20, 50, 10 } },
        { "Tom aigu", "tom", mk<TomDrum> (170.0, 10.0, 0xE10Au), { 0, 320, 40, 20, 0, -7, 20, 50, 10 } },
        { "Percussion", "fm", mk<FmDrum> (220.0, false, 0xE10Bu), { 0, 260, 45, 30, 0, -10, 0, 40, 30 } },
        { "Cloche", "cowbell", mk<CowbellDrum> (0xE10Cu), { 0, 260, 50, 50, 0, -12, 0, 50, 35 } },
        { "\xC3\x89" "chantillon 1", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
        { "\xC3\x89" "chantillon 2", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
        { "\xC3\x89" "chantillon 3", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
        { "\xC3\x89" "chantillon 4", "sample", nullptr, { 0, 4000, 100, 0, 0, -6, 0, 100, 0 } },
    }};
    return pads;
}

inline const ModuleDesc& kitNerveDesc()
{
    static const ModuleDesc d = []
    {
        auto m = makeKitDesc ("drumNrv", "Cut of the Beat",
            "Batterie \xC3\xA0 \xC3\xA9" "chantillons : pads \xC3\xA0 d\xC3\xA9poser, pr\xC3\xA9" "calcul par pad (synth\xC3\xA8se ajout\xC3\xA9" "e, sous-harmonique, r\xC3\xA9solution, \xC3\xA9" "cr\xC3\xAA" "tage), GATE, compresseur interne",
            kitNervePads());
        const auto& pads = kitNervePads();
        for (int p = 0; p < 16; ++p)
        {
            const auto prefix = "p" + std::string (p < 9 ? "0" : "") + std::to_string (p + 1) + "_";
            const auto kind = pads[(size_t) p].kind;
            // départ : grosse caisse avec sous-harmonique, charleys un peu écrêtés
            const auto sub0 = kind == "kick" ? 25.0f : 0.0f;
            m.params.push_back (C (prefix + "pcw", "Synth\xC3\xA8se ajout\xC3\xA9" "e", { "Aucune", "Sinus", "Carr\xC3\xA9", "Scie", "Triangle" }));
            m.params.push_back (P (prefix + "pcl", "Niveau synth\xC3\xA8se", "%", 0, 100, 50));
            m.params.push_back (P (prefix + "pcp", "Hauteur synth\xC3\xA8se", "dt", -24, 48, kind == "kick" ? 0.0f : 24.0f));
            m.params.push_back (P (prefix + "pcd", "D\xC3\xA9" "clin synth\xC3\xA8se", "ms", 5, 4000, 300, 0.3f));
            m.params.push_back (P (prefix + "sub", "Sous-harmonique", "%", 0, 100, sub0));
            m.params.push_back (P (prefix + "bits", "R\xC3\xA9solution", "bits", 2, 16, 16));
            m.params.push_back (P (prefix + "clip", "\xC3\x89" "cr\xC3\xAA" "tage doux", "%", 0, 100, 0));
            m.params.push_back (P (prefix + "gate", "Gate", "%", 5, 100, 100));
            SectionDesc s;
            s.id = "padpc" + std::to_string (p + 1); s.name = pads[(size_t) p].name; s.visual = "padpc"; s.tab = "padpc";
            for (auto* id : drumPcIds) s.params.push_back (prefix + id);
            m.sections.push_back (s);
        }
        std::vector<std::string> sources { "Aucun" };
        for (int p = 0; p < 16; ++p) sources.push_back (std::to_string (p + 1) + ". " + pads[(size_t) p].name);
        m.params.push_back (C ("duckpad", "Compresseur : pad d\xC3\xA9" "clencheur", sources, 1));
        m.params.push_back (P ("duckamt", "Compresseur : profondeur", "%", 0, 100, 0));
        m.params.push_back (P ("duckrel", "Compresseur : rel\xC3\xA2" "chement", "ms", 20, 1000, 180, 0.4f));
        SectionDesc s;
        s.id = "kitduck"; s.name = "Compresseur interne"; s.visual = "kit"; s.tab = "kit";
        s.params = { "duckpad", "duckamt", "duckrel" };
        m.sections.push_back (s);
        return m;
    }();
    return d;
}

} // namespace grv::instruments
