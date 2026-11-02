/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Sampler à pads » — principe des petits samplers de table hip-hop / lo-fi :
      - 16 pads, chacun avec son échantillon (ou une tranche d'un même échantillon) : début, fin,
        accord, niveau, panoramique, attaque, relâchement, filtre, mode (coup unique, porte,
        boucle), lecture à l'envers, calage au tempo, groupe « un pad à la fois » ;
      - chaque pad passe par un bus d'effets (bus 1, bus 2 ou direct) ; les deux bus en parallèle
        ou en série (bus 1 puis bus 2) ;
      - deux bus d'effets à trois commandes, avec les effets qui font le son de ces machines :
        vinyle (craquements, usure, pleurage), lo-fi (bits, échantillonnage, filtre), cassette
        (pleurage, saturation, souffle), filtre + saturation (passe-bas / passe-haut sur un seul
        bouton), isolateur (graves, médiums, aigus jusqu'à la coupure), boucle DJ (une tranche
        rejouée, vitesse et sens), écho à bande et réverbération.
    Le séquenceur de motifs est celui de la groovebox ; le rééchantillonnage passe par le
    sampler de performance (Pickup, pistes choisies).
    Architecture propre : ni schéma, ni nom, ni son d'origine repris.
*/

#pragma once

#include "Sampler.h"
#include "../effects/Delay.h"
#include "../effects/Reverb.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace sp
{
    constexpr int numPads = 16;
    constexpr int padParams = slicer::zoneParams + 1;          // les réglages d'une zone, puis le bus
    enum PadSlot { pBus = slicer::zoneParams };
    enum BusFx { fxNone = 0, fxVinyl, fxLofi, fxCassette, fxFilterDrive, fxIsolator, fxDjLoop, fxEcho, fxReverb, numFx };
    enum GlobalSlot { gLevel = numPads * padParams, gVelAmp, gFilterType, gChoke,
                      b1Type, b1C1, b1C2, b1C3, b2Type, b2C1, b2C2, b2C3, gRoute, gSlices, numSlots };

    inline std::string padId (int z, const char* name) { return "z" + std::string (z < 9 ? "0" : "") + std::to_string (z + 1) + "_" + name; }

    inline const std::vector<std::string>& fxNames()
    {
        static const std::vector<std::string> v { "Aucun", "Vinyle", "Lo-fi", "Cassette", "Filtre + saturation", "Isolateur", "Boucle DJ", "\xC3\x89" "cho \xC3\xA0 bande", "R\xC3\xA9verb\xC3\xA9ration" };
        return v;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "sp";
            m.name = "Poussi\xC3\xA8re de Vinyle";
            m.family = "Sampler";
            m.principle = "16 pads \xC3\xA0 \xC3\xA9" "chantillons, deux bus d'effets \xC3\xA0 trois commandes : vinyle, lo-fi, cassette, filtre + saturation, isolateur, boucle DJ, \xC3\xA9" "cho, r\xC3\xA9verb\xC3\xA9ration";
            m.drumKit = true;
            for (int z = 0; z < numPads; ++z)
            {
                m.params.push_back (P (padId (z, "start"), "D\xC3\xA9" "but", "%", 0, 100, 0));
                m.params.push_back (P (padId (z, "end"), "Fin", "%", 0, 100, 100));
                m.params.push_back (P (padId (z, "tune"), "Accord", "dt", -24, 24, 0));
                m.params.push_back (P (padId (z, "level"), "Niveau", "dB", -48, 6, 0));
                m.params.push_back (P (padId (z, "pan"), "Panoramique", "pan", -100, 100, 0));
                m.params.push_back (P (padId (z, "attack"), "Attaque", "ms", 0.1f, 2000, 0.5f, 0.3f));
                m.params.push_back (P (padId (z, "release"), "Rel\xC3\xA2" "chement", "ms", 2, 5000, 30, 0.3f));
                m.params.push_back (P (padId (z, "cutoff"), "Coupure", "Hz", 20, 20000, 20000, 0.3f));
                m.params.push_back (P (padId (z, "reso"), "R\xC3\xA9sonance", "%", 0, 100, 0));
                m.params.push_back (C (padId (z, "mode"), "Mode", { "Coup unique", "Porte", "Boucle" }));
                m.params.push_back (C (padId (z, "reverse"), "Invers\xC3\xA9", { "Non", "Oui" }));
                m.params.push_back (C (padId (z, "sync"), "Calage tempo", { "Non", "1/4", "1/2", "1 mes.", "2 mes.", "4 mes." }));
                m.params.push_back (C (padId (z, "bus"), "Bus d'effets", { "Bus 1", "Bus 2", "Direct" }));
                SectionDesc s;
                s.id = "zone" + std::to_string (z + 1);
                s.name = "Pad " + std::to_string (z + 1);
                s.visual = "zone";
                for (auto* id : { "start", "end", "tune", "level", "pan", "attack", "release", "cutoff", "reso", "mode", "reverse", "sync", "bus" })
                    s.params.push_back (padId (z, id));
                s.tab = "osc";
                m.sections.push_back (s);
                m.padNames.push_back ("Pad " + std::to_string (z + 1));
                m.padKinds.push_back ("sample");
            }
            m.params.push_back (P ("level", "Niveau", "dB", -24, 6, 0));
            m.params.push_back (P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 40));
            m.params.push_back (C ("ftype", "Filtre des pads", { "Passe-bas", "Passe-bande", "Passe-haut" }));
            m.params.push_back (C ("choke", "Pads", { "Ind\xC3\xA9pendants", "Un \xC3\xA0 la fois" }));
            for (int b = 1; b <= 2; ++b)
            {
                const auto k = "b" + std::to_string (b);
                m.params.push_back (C (k + "type", "Effet du bus " + std::to_string (b), fxNames(), b == 1 ? fxVinyl : fxNone));
                m.params.push_back (P (k + "c1", "Commande 1", "%", 0, 100, 50));
                m.params.push_back (P (k + "c2", "Commande 2", "%", 0, 100, 50));
                m.params.push_back (P (k + "c3", "Commande 3", "%", 0, 100, b == 1 ? 30 : 50));
            }
            m.params.push_back (C ("busroute", "Bus", { "Parall\xC3\xA8les", "Bus 1 puis bus 2" }));
            // un seul échantillon sur le pad 1 : les pads qui n'en ont pas jouent chacun sa part
            m.params.push_back (C ("slices", "D\xC3\xA9" "coupage", { "\xC3\x89" "chantillon entier", "16 parts \xC3\xA9" "gales", "Sur les attaques" }, 1));
            m.sections.insert (m.sections.begin(), SectionDesc { "bus2", "Bus 2", "global", { "b2type", "b2c1", "b2c2", "b2c3", "busroute" }, "global" });
            m.sections.insert (m.sections.begin(), SectionDesc { "bus1", "Bus 1", "global", { "b1type", "b1c1", "b1c2", "b1c3" }, "global" });
            m.sections.insert (m.sections.begin(), SectionDesc { "global", "Global", "global", { "level", "velamp", "ftype", "choke", "slices" }, "global" });
            m.presets = {
                { "Vinyle doux", {} },
                { "Lo-fi hip-hop", { { "b1type", fxVinyl }, { "b1c1", 60 }, { "b1c2", 55 }, { "b1c3", 35 }, { "b2type", fxLofi }, { "b2c1", 40 }, { "b2c2", 30 }, { "b2c3", 55 }, { "busroute", 1 } } },
                { "Cassette us\xC3\xA9" "e", { { "b1type", fxCassette }, { "b1c1", 55 }, { "b1c2", 45 }, { "b1c3", 40 } } },
                { "Filtre DJ", { { "b1type", fxFilterDrive }, { "b1c1", 30 }, { "b1c2", 50 }, { "b1c3", 30 } } },
                { "Isolateur", { { "b1type", fxIsolator }, { "b1c1", 50 }, { "b1c2", 50 }, { "b1c3", 50 } } },
                { "Boucle DJ", { { "b1type", fxDjLoop }, { "b1c1", 40 }, { "b1c2", 75 }, { "b1c3", 0 } } },
                { "\xC3\x89" "cho dub", { { "b1type", fxVinyl }, { "b1c1", 30 }, { "b2type", fxEcho }, { "b2c1", 55 }, { "b2c2", 60 }, { "b2c3", 35 }, { "busroute", 1 } } },
                { "Salle poussi\xC3\xA9reuse", { { "b1type", fxLofi }, { "b1c1", 30 }, { "b1c2", 20 }, { "b1c3", 60 }, { "b2type", fxReverb }, { "b2c1", 60 }, { "b2c2", 40 }, { "b2c3", 30 }, { "busroute", 1 } } },
                { "Sans effet", { { "b1type", fxNone }, { "b2type", fxNone } } },
            };
            return m;
        }();
        return d;
    }
}

/** Un bus d'effets à trois commandes (0..1). Stéréo, sur place, sans allocation au jeu. */
class SpBusFx
{
public:
    void prepare (double s)
    {
        sr = s;
        bufSize = 1;
        while (bufSize < (int) (sr * 2.5)) bufSize <<= 1;
        bufL.assign ((size_t) bufSize, 0.0f); bufR.assign ((size_t) bufSize, 0.0f);
        echo.prepare (s, 512); verb.prepare (s, 512);
        envRelease = std::exp (-1.0 / (0.4 * s));      // 400 ms
        echoP.resize (echo.desc().params.size()); verbP.resize (verb.desc().params.size());
        for (size_t i = 0; i < echoP.size(); ++i) echoP[i] = echo.desc().params[i].def;
        for (size_t i = 0; i < verbP.size(); ++i) verbP[i] = verb.desc().params[i].def;
        reset();
    }
    void reset() noexcept
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f);
        w = 0; for (auto& f : svf) f.reset(); wow = 0.0; env = 0.0; crackle = 0.0; holdN = 0; holdL = holdR = 0.0; loopOn = false; loopPos = 0.0;
        echo.reset(); verb.reset(); lastType = -1; satL.reset(); satR.reset();
    }

    void process (float* L, float* R, int n, int type, double c1, double c2, double c3, double bpm) noexcept
    {
        using namespace sp;
        if (type != lastType) { lastType = type; for (auto& f : svf) f.reset(); satL.reset(); satR.reset(); loopOn = false; }
        const auto mask = bufSize - 1;
        switch (type)
        {
            case fxVinyl:
            {
                // bande étroite selon l'usure, craquements, pleurage léger
                svf[0].set (SvfEq::highpass, 40.0 + c2 * 260.0, 0.6, 0.0, sr); svf[1].set (SvfEq::highpass, 40.0 + c2 * 260.0, 0.6, 0.0, sr);
                svf[2].set (SvfEq::lowpass, 16000.0 - c2 * 12000.0, 0.7, 0.0, sr); svf[3].set (SvfEq::lowpass, 16000.0 - c2 * 12000.0, 0.7, 0.0, sr);
                for (int i = 0; i < n; ++i)
                {
                    follow (L[i], R[i]);
                    bufL[(size_t) w] = L[i]; bufR[(size_t) w] = R[i];
                    wow += 0.55 / sr; if (wow >= 1.0) wow -= 1.0;
                    const auto d = 0.004 * sr + c3 * 0.0025 * sr * fastSin2Pi (wow);
                    double l = read (bufL, d), r = read (bufR, d);
                    l = fastTanh (svf[2].process (svf[0].process (l)) * (1.0 + c2)) / (1.0 + 0.5 * c2);
                    r = fastTanh (svf[3].process (svf[1].process (r)) * (1.0 + c2)) / (1.0 + 0.5 * c2);
                    rng = rng * 1664525u + 1013904223u;
                    const auto u = (double) (rng >> 8) / 16777216.0;
                    rng = rng * 1664525u + 1013904223u;
                    const auto v = (double) (rng >> 8) / 16777216.0 - 0.5;
                    crackle *= 0.55;                                                    // clics très courts
                    if (u < c1 * c1 * 0.0006) crackle = v * (0.15 + 0.35 * c1);          // impulsions rares, d'amplitude au hasard
                    const auto hiss = (u - 0.5) * 0.003 * c1;
                    crackle *= noiseGate();                                             // le bruit suit le son : pas de souffle à l'arrêt
                    const auto h = hiss * noiseGate();
                    L[i] = (float) (l + crackle + h); R[i] = (float) (r + crackle * 0.8 + h);
                    w = (w + 1) & mask;
                }
                break;
            }
            case fxLofi:
            {
                const auto bits = 16.0 - c1 * 12.0;
                const auto q = std::exp2 (bits - 1.0);
                const auto hold = 1 + (int) (c2 * c2 * 24.0);
                svf[0].set (SvfEq::lowpass, 18000.0 * std::pow (0.08, c3), 0.7, 0.0, sr); svf[1].set (SvfEq::lowpass, 18000.0 * std::pow (0.08, c3), 0.7, 0.0, sr);
                for (int i = 0; i < n; ++i)
                {
                    if (--holdN <= 0) { holdN = hold; holdL = L[i]; holdR = R[i]; }
                    L[i] = (float) svf[0].process (std::round (holdL * q) / q);
                    R[i] = (float) svf[1].process (std::round (holdR * q) / q);
                }
                break;
            }
            case fxCassette:
            {
                svf[0].set (SvfEq::lowpass, 14000.0 - c3 * 9000.0, 0.65, 0.0, sr); svf[1].set (SvfEq::lowpass, 14000.0 - c3 * 9000.0, 0.65, 0.0, sr);
                for (int i = 0; i < n; ++i)
                {
                    follow (L[i], R[i]);
                    bufL[(size_t) w] = L[i]; bufR[(size_t) w] = R[i];
                    wow += 0.8 / sr; if (wow >= 1.0) wow -= 1.0;
                    flutter += 9.0 / sr; if (flutter >= 1.0) flutter -= 1.0;
                    const auto d = 0.006 * sr + c1 * (0.003 * sr * fastSin2Pi (wow) + 0.0004 * sr * fastSin2Pi (flutter));
                    const auto drive = 1.0 + c2 * 5.0;
                    rng = rng * 1664525u + 1013904223u;
                    const auto hiss = ((double) (rng >> 8) / 16777216.0 - 0.5) * 0.006 * c3 * noiseGate();
                    L[i] = (float) (svf[0].process (satL.process (read (bufL, d) * drive) / std::sqrt (drive)) + hiss); // ADAA (v0.22)
                    R[i] = (float) (svf[1].process (satR.process (read (bufR, d) * drive) / std::sqrt (drive)) + hiss);
                    w = (w + 1) & mask;
                }
                break;
            }
            case fxFilterDrive:
            {
                // un seul bouton : à gauche passe-bas, au milieu ouvert, à droite passe-haut
                const auto lp = c1 < 0.48, hp = c1 > 0.52;
                const auto fc = lp ? 20000.0 * std::pow (0.004, (0.48 - c1) / 0.48) : hp ? 20.0 * std::pow (400.0, (c1 - 0.52) / 0.48) : 1000.0;
                const auto qv = 0.7 + c2 * 8.0;
                for (int k = 0; k < 2; ++k) svf[k].set (lp ? SvfEq::lowpass : SvfEq::highpass, fc, qv, 0.0, sr);
                const auto drive = 1.0 + c3 * 8.0;
                for (int i = 0; i < n; ++i)
                {
                    double l = L[i], r = R[i];
                    if (lp || hp) { l = svf[0].process (l); r = svf[1].process (r); }
                    L[i] = (float) (satL.process (l * drive) / std::sqrt (drive)); // ADAA (v0.22)
                    R[i] = (float) (satR.process (r * drive) / std::sqrt (drive));
                }
                break;
            }
            case fxIsolator:
            {
                // trois bandes (250 Hz, 2,5 kHz) : 0 = coupée, 50 % = neutre, 100 % = +6 dB
                auto g = [] (double c) { return c < 0.5 ? c * 2.0 : 1.0 + (c - 0.5) * 2.0; };
                const auto gl = g (c1), gm = g (c2), gh = g (c3);
                for (int k = 0; k < 2; ++k) { svf[k].set (SvfEq::lowpass, 250.0, 0.5, 0.0, sr); svf[2 + k].set (SvfEq::highpass, 2500.0, 0.5, 0.0, sr); }
                for (int i = 0; i < n; ++i)
                {
                    const double l = L[i], r = R[i];
                    const auto lowL = svf[0].process (l), lowR = svf[1].process (r);
                    const auto hiL = svf[2].process (l), hiR = svf[3].process (r);
                    L[i] = (float) (lowL * gl + (l - lowL - hiL) * gm + hiL * gh);
                    R[i] = (float) (lowR * gl + (r - lowR - hiR) * gm + hiR * gh);
                }
                break;
            }
            case fxDjLoop:
            {
                // c3 > 0 : la tranche (c1 : 1/32 à 1 mesure) est saisie et rejouée ; c2 : vitesse et sens
                static constexpr double quarters[] { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0 };
                const auto len = std::max (32.0, quarters[std::clamp ((int) (c1 * 5.99), 0, 5)] * 60.0 / std::max (20.0, bpm) * sr);
                static constexpr double speeds[] { -2.0, -1.0, -0.5, 0.5, 1.0, 2.0 };         // c2 : vitesse et sens (à crans)
                const auto speed = speeds[std::clamp ((int) (c2 * 5.99), 0, 5)];
                const bool on = c3 > 0.02;
                for (int i = 0; i < n; ++i)
                {
                    if (! loopOn) { bufL[(size_t) w] = L[i]; bufR[(size_t) w] = R[i]; w = (w + 1) & mask; }
                    if (on && ! loopOn) { loopOn = true; loopStart = (w - (int) len) & mask; loopPos = 0.0; }
                    if (! on && loopOn) loopOn = false;
                    if (! loopOn) continue;
                    const auto p0 = (double) loopStart + loopPos;
                    const auto l = read2 (bufL, p0), r = read2 (bufR, p0);
                    const auto edge = std::min (1.0, std::min (loopPos, len - loopPos) / 64.0);
                    L[i] = (float) (L[i] * (1.0 - c3) + l * edge * c3); R[i] = (float) (R[i] * (1.0 - c3) + r * edge * c3);
                    loopPos += speed;
                    if (loopPos >= len) loopPos -= len;
                    if (loopPos < 0.0) loopPos += len;
                }
                break;
            }
            case fxEcho:
            {
                using namespace grv::effects::echo;
                echoP[mode] = 3; echoP[sync] = 1;
                echoP[division] = (float) std::clamp (4 + (int) (c1 * 11.99), 4, 15);  // 1/16 à 1 mesure
                echoP[feedback] = (float) (c2 * 95.0); echoP[mix] = (float) (c3 * 100.0);
                echoP[saturation] = 35.0f; echoP[wobble] = 25.0f;
                echo.process (L, R, n, echoP.data(), bpm);
                break;
            }
            case fxReverb:
            {
                using namespace grv::effects::verb;
                verbP[grv::effects::verb::size] = (float) (c1 * 100.0); verbP[decay] = (float) (0.4 + c1 * c1 * 8.0);
                verbP[brightness] = (float) (1500.0 + c2 * 14000.0); verbP[mix] = (float) (c3 * 100.0);
                verb.process (L, R, n, verbP.data(), bpm);
                break;
            }
            default: break;
        }
    }

private:
    // suiveur d'enveloppe : le bruit de surface (vinyle, cassette) n'existe que quand le son passe
    void follow (float l, float r) noexcept
    {
        const auto a = (double) std::max (std::abs (l), std::abs (r));
        env = a > env ? a : env * envRelease;
    }
    double noiseGate() const noexcept { return std::min (1.0, env * 30.0); }

    double read (const std::vector<float>& b, double d) const noexcept
    {
        auto pos = (double) w - d; if (pos < 0.0) pos += bufSize;
        return read2 (b, pos);
    }
    double read2 (const std::vector<float>& b, double pos) const noexcept
    {
        while (pos < 0.0) pos += bufSize;
        const auto i0 = (int) pos; const auto f = pos - (double) i0;
        // Hermite (v0.22) : lecture modulée (pleurage, scintillement) sans perte d'aigus
        auto at = [&] (int k) { return (double) b[(size_t) (k & (bufSize - 1))]; };
        return dsp::hermite4 (at (i0 - 1), at (i0), at (i0 + 1), at (i0 + 2), f);
    }

    double env = 0.0, envRelease = 0.9999;
    double sr = 48000.0, wow = 0.0, flutter = 0.0, crackle = 0.0, holdL = 0.0, holdR = 0.0, loopPos = 0.0;
    int bufSize = 1, w = 0, holdN = 0, loopStart = 0, lastType = -1;
    bool loopOn = false;
    std::uint32_t rng = 0xC0FFEEu;
    std::vector<float> bufL, bufR;
    std::array<SvfEq, 4> svf;
    dsp::AdaaTanh satL, satR;
    grv::effects::Delay echo;
    grv::effects::Reverb verb;
    std::vector<float> echoP, verbP;
};

class SpSampler final : public Instrument
{
public:
    const ModuleDesc& desc() const noexcept override { return sp::desc(); }
    void prepare (double s, int) override { sr = s; for (auto& z : zones) z.prepare (s); for (auto& b : bus) b.prepare (s); }
    void reset() noexcept override { for (auto& z : zones) z.reset(); for (auto& b : bus) b.reset(); sliced.fill (false); }

    void setSample (int index, const SampleBuffer* s) noexcept override
    {
        if (index < 0 || index >= sp::numPads) return;
        samples[(size_t) index] = s;
        // un pad sans échantillon joue celui du pad 1 (tranches d'une même boucle)
        for (int z = 0; z < sp::numPads; ++z) zones[(size_t) z].setSample (samples[(size_t) z] != nullptr ? samples[(size_t) z] : samples[0]);
    }

    void noteOn (int pad, float velocity) noexcept override
    {
        if (pad < 0 || pad >= sp::numPads || params == nullptr) return;
        if (params[sp::gChoke] > 0.5f) for (int z = 0; z < sp::numPads; ++z) if (z != pad) zones[(size_t) z].choke();
        if (! regionFor (pad, params)) return;            // part au-delà des attaques trouvées : rien
        zones[(size_t) pad].trigger (velocity, zoneP[(size_t) pad].data(), params[sp::gVelAmp] / 100.0);
    }
    void noteOff (int pad) noexcept override
    {
        if (pad < 0 || pad >= sp::numPads || params == nullptr) return;
        zones[(size_t) pad].release (zoneP[(size_t) pad].data()); // porte et boucle : le son s'arrête au relâchement
    }
    void allNotesOff() noexcept override { for (auto& z : zones) z.choke(); }
    void setTempo (double b) noexcept override { bpm = b; }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        params = p;
        for (int done = 0; done < n; done += maxChunk)
            renderChunk (L + done, R + done, std::min (maxChunk, n - done), p);
    }

    bool isActive() const noexcept override
    {
        for (const auto& z : zones) if (z.isActive()) return true;
        if (params == nullptr) return false;
        // les bus à traîne (écho, réverbération, boucle DJ) continuent après la dernière note
        for (auto t : { (int) params[sp::b1Type], (int) params[sp::b2Type] })
            if (t == sp::fxEcho || t == sp::fxReverb || t == sp::fxDjLoop) return true;
        return false;
    }

private:
    static constexpr int maxChunk = 64;
    std::array<SlicerZone, sp::numPads> zones;
    std::array<const SampleBuffer*, sp::numPads> samples {};
    std::array<SpBusFx, 2> bus;
    std::array<float, maxChunk> b1L {}, b1R {}, b2L {}, b2R {};
    std::array<std::array<float, sp::padParams>, sp::numPads> zoneP {};
    std::array<bool, sp::numPads> sliced {};
    std::array<float, sp::numPads> partA {}, partB {};

    /** Réglages joués par un pad. Découpage : un pad sans échantillon à lui (et le pad 1) joue sa part
        de l'échantillon du pad 1 ; Début et Fin du pad se règlent dans cette part. */
    bool regionFor (int pad, const float* p) noexcept
    {
        auto& zp = zoneP[(size_t) pad];
        std::copy (p + pad * sp::padParams, p + (pad + 1) * sp::padParams, zp.begin());
        sliced[(size_t) pad] = false;
        const auto mode = std::clamp ((int) p[sp::gSlices], 0, 2);
        const auto* src = samples[0];
        if (mode == 0 || src == nullptr || (samples[(size_t) pad] != nullptr && samples[(size_t) pad] != src)) return true;
        float a = (float) pad / 16.0f, b = (float) (pad + 1) / 16.0f;
        if (mode == 2 && src->onsets.size() >= 2)
        {
            const auto count = (int) std::min<size_t> (16, src->onsets.size());
            if (pad >= count) return false;
            a = src->onsets[(size_t) pad];
            b = pad + 1 < count ? src->onsets[(size_t) pad + 1] : 1.0f;
        }
        partA[(size_t) pad] = a * 100.0f; partB[(size_t) pad] = b * 100.0f;
        sliced[(size_t) pad] = true;
        applyPart (pad);
        return true;
    }
    void applyPart (int pad) noexcept
    {
        auto& zp = zoneP[(size_t) pad];
        const auto a = partA[(size_t) pad], w = partB[(size_t) pad] - partA[(size_t) pad];
        const auto s0 = zp[slicer::zStart], e0 = zp[slicer::zEnd];
        zp[slicer::zStart] = a + w * s0 / 100.0f;
        zp[slicer::zEnd] = a + w * e0 / 100.0f;
    }

    void renderChunk (float* L, float* R, int n, const float* p) noexcept
    {
        const auto gain = dbToGain (p[sp::gLevel]);
        const auto ft = std::clamp ((int) p[sp::gFilterType], 0, 2);
        std::fill (b1L.begin(), b1L.begin() + n, 0.0f); std::fill (b1R.begin(), b1R.begin() + n, 0.0f);
        std::fill (b2L.begin(), b2L.begin() + n, 0.0f); std::fill (b2R.begin(), b2R.begin() + n, 0.0f);
        for (int z = 0; z < sp::numPads; ++z)
        {
            // réglages du bloc (la part choisie au déclenchement reste celle du pad)
            std::copy (p + z * sp::padParams, p + (z + 1) * sp::padParams, zoneP[(size_t) z].begin());
            if (sliced[(size_t) z]) applyPart (z);
            const auto* zp = zoneP[(size_t) z].data();
            switch (std::clamp ((int) zp[sp::pBus], 0, 2))
            {
                case 0:  zones[(size_t) z].render (b1L.data(), b1R.data(), n, zp, ft, gain, bpm); break;
                case 1:  zones[(size_t) z].render (b2L.data(), b2R.data(), n, zp, ft, gain, bpm); break;
                default: zones[(size_t) z].render (L, R, n, zp, ft, gain, bpm); break;
            }
        }
        const bool serial = p[sp::gRoute] > 0.5f;
        bus[0].process (b1L.data(), b1R.data(), n, (int) p[sp::b1Type], p[sp::b1C1] / 100.0, p[sp::b1C2] / 100.0, p[sp::b1C3] / 100.0, bpm);
        if (serial) for (int i = 0; i < n; ++i) { b2L[(size_t) i] += b1L[(size_t) i]; b2R[(size_t) i] += b1R[(size_t) i]; }
        bus[1].process (b2L.data(), b2R.data(), n, (int) p[sp::b2Type], p[sp::b2C1] / 100.0, p[sp::b2C2] / 100.0, p[sp::b2C3] / 100.0, bpm);
        for (int i = 0; i < n; ++i)
        {
            L[i] += b2L[(size_t) i] + (serial ? 0.0f : b1L[(size_t) i]);
            R[i] += b2R[(size_t) i] + (serial ? 0.0f : b1R[(size_t) i]);
        }
    }
    const float* params = nullptr;
    double sr = 48000.0, bpm = 120.0;
};

} // namespace grv::instruments
