/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Échantillonneurs, repris du moteur de StemSampler Pro (même auteur) et adaptés au groovebox :
      - « Sampler à découpes » : 16 zones sur les 16 pads. Chaque zone lit une partie d'un
        échantillon (ou son propre échantillon) avec accord, niveau, panoramique, enveloppe,
        filtre, mode (coup unique, porte, boucle), lecture inversée et calage au tempo (la zone
        dure exactement 1/4, 1/2, 1, 2 ou 4 mesures). Découpage automatique égal ou sur les transitoires.
      - « Sampler chromatique » : un échantillon joué sur tout le clavier, 8 voix, boucle avant
        ou aller-retour, enveloppes d'ampli et de filtre.
    La séparation de pistes par IA de StemSampler Pro n'est pas reprise ici (modèle externe lourd).
*/

#pragma once

#include "../synth/SynthBase.h"
#include "../synth/SamplePlayer.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

//==============================================================================
namespace slicer
{
    constexpr int numZones = 16;
    constexpr int zoneParams = 12;
    enum ZoneSlot { zStart = 0, zEnd, zTune, zLevel, zPan, zAttack, zRelease, zCutoff, zReso, zMode, zReverse, zSync };
    enum GlobalSlot { gLevel = numZones * zoneParams, gVelAmp, gFilterType, gChoke,
                      gSliceCount, gSlicePage, gRetrig, gRetrigTime, numSlots };

    /** Découpes égales (v0.15) : 0 = zones libres (Début / Fin de chaque zone), sinon 2 à 128 parts. */
    inline int sliceCountOf (float v) noexcept { static constexpr int c[8] { 0, 2, 4, 8, 16, 32, 64, 128 }; return c[std::clamp ((int) v, 0, 7)]; }
    /** Redéclenchement : nombre de coups et écart (en noires). */
    inline int retrigCountOf (float v) noexcept { static constexpr int c[8] { 1, 2, 3, 4, 6, 8, 12, 16 }; return c[std::clamp ((int) v, 0, 7)]; }
    inline double retrigQuarters (float v) noexcept { static constexpr double q[6] { 1.0 / 16, 1.0 / 8, 1.0 / 6, 1.0 / 4, 1.0 / 2, 1.0 }; return q[std::clamp ((int) v, 0, 5)]; }

    inline std::string zoneId (int z, const char* name) { return "z" + std::string (z < 9 ? "0" : "") + std::to_string (z + 1) + "_" + name; }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "sampler";
            m.name = "Cutsample";
            m.family = "Sampler";
            m.principle = "16 zones sur les pads, d\xC3\xA9" "coupage automatique, calage au tempo (moteur StemSampler Pro)";
            m.drumKit = true;
            for (int z = 0; z < numZones; ++z)
            {
                const auto a = (float) z / numZones * 100.0f, b = (float) (z + 1) / numZones * 100.0f;
                m.params.push_back (P (zoneId (z, "start"), "D\xC3\xA9" "but", "%", 0, 100, a));
                m.params.push_back (P (zoneId (z, "end"), "Fin", "%", 0, 100, b));
                m.params.push_back (P (zoneId (z, "tune"), "Accord", "dt", -24, 24, 0));
                m.params.push_back (P (zoneId (z, "level"), "Niveau", "dB", -48, 6, 0));
                m.params.push_back (P (zoneId (z, "pan"), "Panoramique", "pan", -100, 100, 0));
                m.params.push_back (P (zoneId (z, "attack"), "Attaque", "ms", 0.1f, 2000, 0.5f, 0.3f));
                m.params.push_back (P (zoneId (z, "release"), "Rel\xC3\xA2" "chement", "ms", 2, 5000, 30, 0.3f));
                m.params.push_back (P (zoneId (z, "cutoff"), "Coupure", "Hz", 20, 20000, 20000, 0.3f));
                m.params.push_back (P (zoneId (z, "reso"), "R\xC3\xA9sonance", "%", 0, 100, 0));
                m.params.push_back (C (zoneId (z, "mode"), "Mode", { "Coup unique", "Porte", "Boucle" }));
                m.params.push_back (C (zoneId (z, "reverse"), "Invers\xC3\xA9", { "Non", "Oui" }));
                m.params.push_back (C (zoneId (z, "sync"), "Calage tempo", { "Non", "1/4", "1/2", "1 mes.", "2 mes.", "4 mes." }));
                SectionDesc s;
                s.id = "zone" + std::to_string (z + 1);
                s.name = "Zone " + std::to_string (z + 1);
                s.visual = "zone";
                for (auto* id : { "start", "end", "tune", "level", "pan", "attack", "release", "cutoff", "reso", "mode", "reverse", "sync" })
                    s.params.push_back (zoneId (z, id));
                s.tab = "osc";
                m.sections.push_back (s);
                m.padNames.push_back ("Zone " + std::to_string (z + 1));
                m.padKinds.push_back ("sample");
            }
            m.params.push_back (P ("level", "Niveau", "dB", -24, 6, 0));
            m.params.push_back (P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 60));
            m.params.push_back (C ("ftype", "Filtre", { "Passe-bas", "Passe-bande", "Passe-haut" }));
            m.params.push_back (C ("choke", "Pads", { "Ind\xC3\xA9pendants", "Un \xC3\xA0 la fois" }));
            // découpes égales : les 16 pads jouent les parts 1 à 16, 17 à 32… selon la page (verrouillable par pas)
            m.params.push_back (C ("slicecnt", "D\xC3\xA9" "coupes", { "Zones libres", "2", "4", "8", "16", "32", "64", "128" }));
            m.params.push_back (C ("slicepage", "Page", { "1-16", "17-32", "33-48", "49-64", "65-80", "81-96", "97-112", "113-128" }));
            m.params.push_back (C ("retrig", "Red\xC3\xA9" "cl.", { "Non", "2", "3", "4", "6", "8", "12", "16" }));
            m.params.push_back (C ("rtim", "\xC3\x89" "cart red\xC3\xA9" "cl.", { "1/64", "1/32", "1/16T", "1/16", "1/8", "1/4" }, 1));
            m.sections.insert (m.sections.begin(), SectionDesc { "slices", "D\xC3\xA9" "coupes et red\xC3\xA9" "clenchement", "global", { "slicecnt", "slicepage", "retrig", "rtim" }, "global" });
            m.sections.insert (m.sections.begin(), SectionDesc { "global", "Global", "global", { "level", "velamp", "ftype", "choke" }, "global" });
            return m;
        }();
        return d;
    }
}

class SlicerZone
{
public:
    void prepare (double s) noexcept { sr = s; reset(); }
    void reset() noexcept { for (auto& h : heads) { h.player.stop(); h.env = 0.0; } fl.reset(); fr.reset(); }
    void setSample (const SampleBuffer* s) noexcept { for (auto& h : heads) h.player.setSample (s); }
    bool hasSample() const noexcept { return heads[0].player.hasSample(); }
    bool isActive() const noexcept { return heads[0].player.isActive() || heads[1].player.isActive(); }

    void trigger (float velocity, const float* z, double velSens) noexcept
    {
        if (! hasSample()) return;
        // la tête en cours s'efface en 3 ms : pas de clic au redéclenchement
        heads[1] = heads[0];
        heads[1].releasing = true;
        heads[1].fast = true;
        auto& h = heads[0];
        SamplePlayer::Region r;
        r.start = z[slicer::zStart] / 100.0;
        r.end = z[slicer::zEnd] / 100.0;
        r.loopStart = r.start; r.loopEnd = r.end;
        r.loop = (int) z[slicer::zMode] == 2 ? SamplePlayer::loopForward : SamplePlayer::noLoop;
        r.reverse = z[slicer::zReverse] > 0.5f;
        h.player.start (r);
        h.env = 0.0;
        h.releasing = false;
        h.fast = false;
        h.gain = velocityGain (velocity, velSens);
    }

    void release (const float* z) noexcept
    {
        if ((int) z[slicer::zMode] != 0) heads[0].releasing = true; // porte et boucle : relâchement à la fin de la note
    }

    void choke() noexcept { for (auto& h : heads) { h.releasing = true; h.fast = true; } }

    void render (float* L, float* R, int n, const float* z, int filterType, double gain, double bpm) noexcept
    {
        if (! isActive()) return;
        const auto lenFrac = std::max (1.0e-4, std::abs (z[slicer::zEnd] - z[slicer::zStart]) / 100.0);
        auto rate = heads[0].player.fileRate() / sr * fastExp2 (z[slicer::zTune] / 12.0);
        static constexpr double beats[6] { 0.0, 1.0, 2.0, 4.0, 8.0, 16.0 };
        const auto sync = std::clamp ((int) z[slicer::zSync], 0, 5);
        if (sync > 0 && bpm > 0.0)
        {
            // la zone dure exactement le nombre de temps choisi (rééchantillonnage)
            const auto zoneSamples = lenFrac * (double) heads[0].player.length();
            const auto targetSamples = beats[sync] * 60.0 / bpm * sr;
            rate = zoneSamples / std::max (1.0, targetSamples);
        }
        const auto atk = 1.0 - std::exp (-1.0 / std::max (1.0, z[slicer::zAttack] * 0.001 * sr));
        const auto rel = std::exp (std::log (0.001) / std::max (1.0, z[slicer::zRelease] * 0.001 * sr));
        const auto fastRel = std::exp (std::log (0.001) / (0.003 * sr));
        const auto lvl = gain * (z[slicer::zLevel] <= -47.9f ? 0.0 : dbToGain (z[slicer::zLevel]));
        const auto pan = z[slicer::zPan] / 100.0;
        const auto gl = lvl * std::min (1.0, 1.0 - pan), gr = lvl * std::min (1.0, 1.0 + pan);
        const bool filtered = z[slicer::zCutoff] < 19900.0f || z[slicer::zReso] > 0.5f || filterType != 0;
        SvfCoefs c;
        if (filtered) c.set (z[slicer::zCutoff], z[slicer::zReso] / 100.0, sr);

        for (int i = 0; i < n; ++i)
        {
            double sl = 0.0, sr2 = 0.0;
            for (auto& h : heads)
            {
                if (! h.player.isActive()) continue;
                double l, r;
                h.player.next (rate, l, r);
                if (h.releasing) h.env *= h.fast ? fastRel : rel;
                else h.env += (1.0 - h.env) * atk;
                sl += l * h.env * h.gain;
                sr2 += r * h.env * h.gain;
                if (h.releasing && h.env < 1.0e-4) h.player.stop();
            }
            if (filtered)
            {
                const auto ol = fl.process (sl, c), orr = fr.process (sr2, c);
                sl = filterType == 0 ? ol.low : (filterType == 1 ? ol.band : ol.high);
                sr2 = filterType == 0 ? orr.low : (filterType == 1 ? orr.band : orr.high);
            }
            L[i] += (float) (sl * gl);
            R[i] += (float) (sr2 * gr);
        }
    }

private:
    struct Head { SamplePlayer player; double env = 0.0, gain = 1.0; bool releasing = false, fast = false; };
    std::array<Head, 2> heads;
    StateFilter fl, fr;
    double sr = 48000.0;
};

class SlicerSampler final : public Instrument
{
public:
    const ModuleDesc& desc() const noexcept override { return slicer::desc(); }
    void prepare (double s, int) override { sr = s; for (auto& z : zones) z.prepare (s); }
    void reset() noexcept override { for (auto& z : zones) z.reset(); retrigLeft.fill (0); sliced.fill (false); }

    void setSample (int index, const SampleBuffer* s) noexcept override
    {
        if (index < 0 || index >= slicer::numZones) return;
        samples[(size_t) index] = s;
        for (int z = 0; z < slicer::numZones; ++z)
        {
            const auto* src = samples[(size_t) z] != nullptr ? samples[(size_t) z] : samples[0];
            zones[(size_t) z].setSample (src);
        }
    }

    void noteOn (int pad, float velocity) noexcept override
    {
        if (pad < 0 || pad >= slicer::numZones || params == nullptr) return;
        if (params[slicer::gChoke] > 0.5f) for (int z = 0; z < slicer::numZones; ++z) if (z != pad) { zones[(size_t) z].choke(); retrigLeft[(size_t) z] = 0; }
        if (! regionFor (pad, params)) return;              // part au-delà du nombre de découpes : rien
        zones[(size_t) pad].trigger (velocity, zoneP[(size_t) pad].data(), params[slicer::gVelAmp] / 100.0);
        // redéclenchement : la même part rejouée N fois, à l'écart choisi (calé sur le tempo)
        const auto count = slicer::retrigCountOf (params[slicer::gRetrig]);
        retrigLeft[(size_t) pad] = count - 1;
        retrigVel[(size_t) pad] = velocity;
        retrigWait[(size_t) pad] = retrigInterval (params);
    }
    void noteOff (int pad) noexcept override
    {
        if (pad < 0 || pad >= slicer::numZones || params == nullptr) return;
        zones[(size_t) pad].release (zoneP[(size_t) pad].data());
    }
    void allNotesOff() noexcept override { for (auto& z : zones) z.choke(); retrigLeft.fill (0); }
    void setTempo (double b) noexcept override { bpm = b; }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        params = p;
        const auto gain = dbToGain (p[slicer::gLevel]);
        const auto ft = std::clamp ((int) p[slicer::gFilterType], 0, 2);
        // réglages des zones (la part choisie au déclenchement remplace Début / Fin en mode découpes)
        for (int z = 0; z < slicer::numZones; ++z)
        {
            auto& zp = zoneP[(size_t) z];
            std::copy (p + z * slicer::zoneParams, p + (z + 1) * slicer::zoneParams, zp.begin());
            if (sliced[(size_t) z]) { zp[slicer::zStart] = partStart[(size_t) z]; zp[slicer::zEnd] = partEnd[(size_t) z]; }
        }
        // le bloc est coupé aux redéclenchements (au sample près)
        int pos = 0;
        while (pos < n)
        {
            int len = n - pos;
            for (int z = 0; z < slicer::numZones; ++z)
                if (retrigLeft[(size_t) z] > 0) len = std::min (len, std::max (1, (int) std::ceil (retrigWait[(size_t) z])));
            for (int z = 0; z < slicer::numZones; ++z)
                zones[(size_t) z].render (L + pos, R + pos, len, zoneP[(size_t) z].data(), ft, gain, bpm);
            pos += len;
            for (int z = 0; z < slicer::numZones; ++z)
            {
                if (retrigLeft[(size_t) z] <= 0) continue;
                retrigWait[(size_t) z] -= len;
                if (retrigWait[(size_t) z] > 0.5) continue;
                --retrigLeft[(size_t) z];
                retrigWait[(size_t) z] += retrigInterval (p);
                zones[(size_t) z].trigger (retrigVel[(size_t) z], zoneP[(size_t) z].data(), p[slicer::gVelAmp] / 100.0);
            }
        }
    }

    bool isActive() const noexcept override
    {
        for (const auto& z : zones) if (z.isActive()) return true;
        for (auto r : retrigLeft) if (r > 0) return true;
        return false;
    }

private:
    /** Part jouée par un pad. Zones libres : Début / Fin de la zone ; découpes : part (page × 16 + pad) sur N. */
    bool regionFor (int pad, const float* p) noexcept
    {
        auto& zp = zoneP[(size_t) pad];
        std::copy (p + pad * slicer::zoneParams, p + (pad + 1) * slicer::zoneParams, zp.begin());
        const auto count = slicer::sliceCountOf (p[slicer::gSliceCount]);
        sliced[(size_t) pad] = count > 0;
        if (count == 0) return true;
        const auto idx = std::clamp ((int) p[slicer::gSlicePage], 0, 7) * 16 + pad;
        if (idx >= count) { sliced[(size_t) pad] = false; return false; }
        partStart[(size_t) pad] = (float) idx * 100.0f / (float) count;
        partEnd[(size_t) pad] = (float) (idx + 1) * 100.0f / (float) count;
        zp[slicer::zStart] = partStart[(size_t) pad]; zp[slicer::zEnd] = partEnd[(size_t) pad];
        return true;
    }
    double retrigInterval (const float* p) const noexcept
    {
        return std::max (16.0, slicer::retrigQuarters (p[slicer::gRetrigTime]) * 60.0 / std::max (20.0, bpm) * sr);
    }

    std::array<SlicerZone, slicer::numZones> zones;
    std::array<const SampleBuffer*, slicer::numZones> samples {};
    std::array<std::array<float, slicer::zoneParams>, slicer::numZones> zoneP {};
    std::array<bool, slicer::numZones> sliced {};
    std::array<float, slicer::numZones> partStart {}, partEnd {}, retrigVel {};
    std::array<int, slicer::numZones> retrigLeft {};
    std::array<double, slicer::numZones> retrigWait {};
    const float* params = nullptr; // réglages du dernier bloc (le moteur appelle render avant les notes suivantes)
    double sr = 48000.0, bpm = 120.0;
};

//==============================================================================
namespace keys
{
    enum Slot
    {
        voices = 0, glide, level, velAmp, pan, tune, fine, root,
        start, end, loopMode, loopStart, loopEnd, reverse,
        aA, aD, aS, aR,
        fType, cutoff, reso, envAmt, keytrack, velFilter, fA, fD, fS, fR,
        trigMode,                       // v0.15 : note tenue (porte) ou en une fois (coup unique)
        numSlots
    };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "keys";
            m.name = "Chromasample";
            m.family = "Sampler";
            m.principle = "Un \xC3\xA9" "chantillon jou\xC3\xA9 sur tout le clavier, boucles, enveloppes d'ampli et de filtre (moteur StemSampler Pro)";
            m.params = {
                I ("voices", "Voix", "", 1, 8, 8),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("level", "Niveau", "dB", -24, 6, 0),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 50),
                P ("pan", "Panoramique", "pan", -100, 100, 0),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("fine", "Fin", "ct", -50, 50, 0),
                I ("root", "Note de base", "note", 24, 96, 60),
                P ("start", "D\xC3\xA9" "but", "%", 0, 100, 0),
                P ("end", "Fin", "%", 0, 100, 100),
                C ("loop", "Boucle", { "Non", "Avant", "Aller-retour" }),
                P ("loopstart", "D\xC3\xA9" "but boucle", "%", 0, 100, 0),
                P ("loopend", "Fin boucle", "%", 0, 100, 100),
                C ("reverse", "Invers\xC3\xA9", { "Non", "Oui" }),
                P ("aa", "Attaque", "ms", 0.5f, 10000, 1, 0.3f), P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 1000, 0.3f),
                P ("as", "Maintien", "%", 0, 100, 100), P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 200, 0.3f),
                C ("ftype", "Filtre", { "Passe-bas", "Passe-bande", "Passe-haut" }),
                P ("cutoff", "Coupure", "Hz", 20, 20000, 20000, 0.3f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 0),
                P ("envamt", "Enveloppe", "%", -100, 100, 0),
                P ("keytrack", "Suivi clavier", "%", 0, 100, 0),
                P ("velfilter", "V\xC3\xA9l. filtre", "%", 0, 100, 0),
                P ("fa", "Attaque", "ms", 0.5f, 10000, 2, 0.3f), P ("fd", "D\xC3\xA9" "clin", "ms", 5, 10000, 500, 0.3f),
                P ("fs", "Maintien", "%", 0, 100, 0), P ("fr", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 300, 0.3f),
                C ("trigmode", "Lecture", { "Tant que tenue", "En une fois" }),
            };
            m.sections = {
                { "global", "Global", "global", { "voices", "glide", "tune", "fine", "root", "velamp", "pan", "level" }, "global" },
                { "sample", "\xC3\x89" "chantillon", "sample", { "start", "end", "loop", "loopstart", "loopend", "reverse", "trigmode" }, "osc" },
                { "filter", "Filtre", "filter", { "ftype", "cutoff", "reso", "envamt", "keytrack", "velfilter" }, "filter" },
                { "fenv", "Enveloppe filtre", "env", { "fa", "fd", "fs", "fr", "envamt" }, "env" },
                { "aenv", "Enveloppe ampli", "env", { "aa", "ad", "as", "ar", "velamp", "level" }, "amp" },
            };
            m.presets = {
                { "Naturel", {} },
                { "Nappe en boucle", { { "loop", 2 }, { "loopstart", 30 }, { "loopend", 70 }, { "aa", 600 }, { "ar", 1500 } } },
                { "Pluck filtr\xC3\xA9", { { "cutoff", 800 }, { "envamt", 70 }, { "fd", 250 }, { "as", 0 }, { "ad", 400 }, { "ar", 300 } } },
            };
            return m;
        }();
        return d;
    }
}

class KeysVoice
{
public:
    void prepare (double s) { sr = s; aenv.prepare (s); fenv.prepare (s); reset(); }
    void reset() noexcept { player.stop(); aenv.reset(); fenv.reset(); fl.reset(); fr.reset(); active = false; }
    void setSpread (int, int) noexcept {}
    void setSample (const SampleBuffer* s) noexcept { player.setSample (s); active = false; }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true;
        active = player.hasSample();
    }
    // en une fois : la fin de la note ne coupe pas le son, l'échantillon joue jusqu'à sa fin
    void release() noexcept { if (! oneShot) { aenv.gate (false); fenv.gate (false); } }
    bool isActive() const noexcept { return active; }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace keys;
        oneShot = (int) p[trigMode] == 1;
        if (pending)
        {
            pending = false;
            pitch.jump ((double) (p[glide] > 0.5f && from >= 0 ? from : target));
            pitch.to ((double) target);
            if (! legatoPending || ! player.isActive())
            {
                SamplePlayer::Region r;
                r.start = p[keys::start] / 100.0; r.end = p[keys::end] / 100.0;
                r.loopStart = p[loopStart] / 100.0; r.loopEnd = p[loopEnd] / 100.0;
                r.loop = oneShot ? SamplePlayer::noLoop : (SamplePlayer::LoopMode) std::clamp ((int) p[loopMode], 0, 2); // en une fois : pas de boucle sans fin
                r.reverse = p[reverse] > 0.5f;
                player.start (r);
                aenv.gate (true); fenv.gate (true);
            }
        }
        if (! player.isActive()) { active = false; return; }
        aenv.set (p[aA], p[aD], p[aS] / 100.0, p[aR]);
        fenv.set (p[fA], p[fD], p[fS] / 100.0, p[fR]);
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto gain = dbToGain (p[level]) * velocityGain (vel, p[velAmp] / 100.0);
        const auto pn = p[pan] / 100.0;
        const auto gl = gain * std::min (1.0, 1.0 - pn), gr = gain * std::min (1.0, 1.0 + pn);
        const auto ft = std::clamp ((int) p[fType], 0, 2);
        const bool filtered = p[cutoff] < 19900.0f || p[reso] > 0.5f || ft != 0 || p[envAmt] != 0.0f;
        const auto baseRate = player.fileRate() / sr;
        const auto offset = p[tune] + p[fine] / 100.0 - std::round (p[root]);
        const auto velOct = (p[velFilter] / 100.0) * 2.0 * (vel - 0.5);

        for (int i = 0; i < n; ++i)
        {
            const auto a = aenv.next(), fe = fenv.next();
            const auto note = pitch.next (glideCoef);
            double l, r;
            player.next (baseRate * fastExp2 ((note + offset) / 12.0), l, r);
            if (filtered)
            {
                const auto oct = (p[envAmt] / 100.0) * 7.0 * fe + velOct + (p[keytrack] / 100.0) * (note - 60.0) / 12.0;
                SvfCoefs c;
                c.set (std::clamp ((double) p[cutoff] * fastExp2 (oct), 16.0, 0.45 * sr), p[reso] / 100.0, sr);
                const auto ol = fl.process (l, c), orr = fr.process (r, c);
                l = ft == 0 ? ol.low : (ft == 1 ? ol.band : ol.high);
                r = ft == 0 ? orr.low : (ft == 1 ? orr.band : orr.high);
            }
            L[i] += (float) (l * a * gl);
            R[i] += (float) (r * a * gr);
            if (! player.isActive()) { aenv.reset(); break; }
        }
        if (! aenv.isActive()) { active = false; player.stop(); }
    }

private:
    double sr = 48000.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false, oneShot = false;
    SamplePlayer player;
    Adsr aenv, fenv;
    StateFilter fl, fr;
    Glide pitch;
};

class KeysSampler final : public SynthBase<KeysVoice, 8>
{
public:
    const ModuleDesc& desc() const noexcept override { return keys::desc(); }
    void setSample (int index, const SampleBuffer* s) noexcept override
    {
        if (index != 0) return;
        for (auto& v : voices) v.setSample (s);
    }
protected:
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[keys::voices], 1, 8);
        setMode (v == 1, true, v);
    }
};

//==============================================================================
namespace input
{
    enum Slot { gain = 0, monitor, channel, gate, width, numSlots };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "input";
            m.name = "Entr\xC3\xA9" "e audio";
            m.family = "Entr\xC3\xA9" "e";
            m.principle = "Entr\xC3\xA9" "e de la carte son ou de la piste du logiciel h\xC3\xB4te, trait\xC3\xA9" "e par les effets de la piste";
            m.params = {
                P ("gain", "Gain", "dB", -24, 24, 0),
                C ("monitor", "\xC3\x89" "coute", { "Non", "Oui" }),
                C ("channel", "Canaux", { "St\xC3\xA9r\xC3\xA9o", "Gauche", "Droite", "Mono" }),
                P ("gate", "Porte de bruit", "dB", -90, -20, -90),
                P ("width", "Largeur", "%", 0, 200, 100),
            };
            m.sections = { { "global", "Global", "global", { "gain", "monitor", "channel", "gate", "width" }, "global" } };
            return m;
        }();
        return d;
    }
}

/** Entrée audio : l'écoute est coupée par défaut (évite tout larsen micro → enceintes). */
class AudioInput final : public Instrument
{
public:
    const ModuleDesc& desc() const noexcept override { return input::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override { env = 0.0; gateGain = 0.0; }
    void noteOn (int, float) noexcept override {}
    void noteOff (int) noexcept override {}
    void setInput (const float* l, const float* r) noexcept override { inL = l; inR = r; }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        if (p[input::monitor] < 0.5f || inL == nullptr) return;
        const auto g = dbToGain (p[input::gain]);
        const auto thr = p[input::gate] <= -89.9f ? 0.0 : dbToGain (p[input::gate]);
        const auto ch = (int) p[input::channel];
        const auto w = p[input::width] / 100.0;
        const auto envC = std::exp (-1.0 / (0.01 * sr)), gateC = std::exp (-1.0 / (0.005 * sr));
        for (int i = 0; i < n; ++i)
        {
            double l = inL[i], r = inR != nullptr ? inR[i] : inL[i];
            if (ch == 1) r = l; else if (ch == 2) l = r; else if (ch == 3) l = r = 0.5 * (l + r);
            const auto lvl = std::max (std::abs (l), std::abs (r));
            env = std::max (lvl, env * envC);
            const auto target = env >= thr ? 1.0 : 0.0;
            gateGain = target + (gateGain - target) * gateC;
            const auto m = 0.5 * (l + r), s = 0.5 * (l - r) * w;
            L[i] += (float) ((m + s) * g * gateGain);
            R[i] += (float) ((m - s) * g * gateGain);
        }
    }

private:
    const float* inL = nullptr;
    const float* inR = nullptr;
    double sr = 48000.0, env = 0.0, gateGain = 0.0;
};

} // namespace grv::instruments
