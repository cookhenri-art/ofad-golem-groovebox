/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Micro Tonnerre » (après la 1.1) — principe des générateurs de rythmes logiciels à synthèse
    universelle : une seule architecture de son, simple, reprise pour chacun des 8 canaux, qui
    imite aussi bien une grosse caisse qu'une cymbale, un clap ou une percussion métallique
    (architecture reprise ; ni nom, ni dessin, ni son, ni motif d'origine). Par canal :
      - MÉLANGE : dosage oscillateur / bruit (au milieu, les deux à plein), égaliseur en cloche
        (fréquence, gain ±40 dB), distorsion, niveau, panoramique, groupe d'étouffement ;
      - OSCILLATEUR : sinus, triangle ou dent de scie (à bande limitée), fréquence de 20 Hz à
        20 kHz ; modulation de hauteur en déclin (balayage des grosses caisses et des toms), en
        sinus (vibrato lent ou FM à fréquence audio : métaux, cloches) ou aléatoire (grain) ;
        enveloppe attaque linéaire / déclin exponentiel ;
      - BRUIT : bruit blanc filtré (passe-bas, passe-bande ou passe-haut, résonance jusqu'au
        sifflement), en mono ou en stéréo (deux bruits différents à gauche et à droite) ;
        enveloppe exponentielle, linéaire ou « claquée » (l'attaque devient trois salves, comme
        les mains d'un clap) ;
      - VÉLOCITÉ : part de la vélocité sur l'oscillateur, le bruit et la modulation de hauteur.
    Chaque canal se relance proprement (l'ancien coup s'efface en 3 ms pendant que le nouveau part
    du début), passe par ses 2 inserts du catalogue et a ses 2 envois, comme les pads des autres
    batteries. Le morphing entre deux états du kit se fait avec les scènes et le crossfader de GOLEM.
    Les sons d'usine (kits) sont calculés par tools/presets/kits_microton.py (mt/MtKits.h).
    Aucune allocation dans le rendu ; toutes les voix sont créées d'avance.
*/

#pragma once

#include "../synth/SynthCore.h"
#include "../dsp/EqFilters.h"
#include "../engine/Module.h"

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace grv::instruments
{

using namespace grv::engine;

namespace mt
{
    constexpr int numChannels = 8;

    /** Réglages d'un canal, dans l'ordre de leur bloc « pNN_… » (positions figées). */
    enum ChParam
    {
        cMix = 0, cEqF, cEqG, cDist, cLevel, cPan, cChoke,
        cWave, cFreq, cMMode, cMAmt, cMTime, cMRate, cOAtk, cODec,
        cNFilt, cNFreq, cNQ, cNStereo, cNEnv, cNAtk, cNDec,
        cVOsc, cVNoise, cVMod,
        cSendA, cSendB,
        numCh
    };
    constexpr int volumeIndex = numChannels * numCh; // 216 : volume du kit
    constexpr int numSlots = volumeIndex + 1;

    inline const char* chIds[numCh] { "mix", "eqf", "eqg", "dist", "level", "pan", "choke",
                                      "wave", "freq", "mmode", "mamt", "mtime", "mrate", "oatk", "odec",
                                      "nfilt", "nfreq", "nq", "nstereo", "nenv", "natk", "ndec",
                                      "vosc", "vnoise", "vmod", "sendA", "sendB" };

    inline std::string prefix (int ch) { return "p" + std::string (ch < 9 ? "0" : "") + std::to_string (ch + 1) + "_"; }

    /** Un son de canal (unités des réglages). */
    struct Patch
    {
        float mix, eqf, eqg, dist, level, pan, choke;
        float wave, freq, mmode, mamt, mtime, mrate, oatk, odec;
        float nfilt, nfreq, nq, nstereo, nenv, natk, ndec;
        float vosc, vnoise, vmod;
        float sendA, sendB;
        float get (int k) const noexcept
        {
            const float v[numCh] { mix, eqf, eqg, dist, level, pan, choke, wave, freq, mmode, mamt, mtime, mrate, oatk, odec,
                                   nfilt, nfreq, nq, nstereo, nenv, natk, ndec, vosc, vnoise, vmod, sendA, sendB };
            return v[k];
        }
    };

    /** Le kit de départ (réglages par défaut des 8 canaux, son « Init »). */
    inline const std::array<Patch, numChannels>& initKit()
    {
        //                                mix  eqf    eqg  dist lvl  pan ch | wave freq  mmode mamt mtime mrate oatk odec | nfilt nfreq  nq  nst nenv natk ndec | vosc vnz vmod | A B
        static const std::array<Patch, numChannels> k {{
            { 10, 90,    3,   12, -6,   0,  0,   0, 48,   0,   34, 45,   10,   0,  480,  2, 4500,  10, 0, 0, 0,  14,   55, 70, 30,  0, 0 },  // grosse caisse
            { 58, 3200,  3,   8,  -8,   0,  0,   1, 186,  0,   10, 28,   10,   0,  150,  2, 1900,  14, 0, 0, 0,  210,  60, 70, 30,  0, 0 },  // caisse claire
            { 100, 9000, 2,   0,  -11,  8,  1,   0, 400,  0,   0,  30,   10,   0,  50,   2, 7600,  18, 1, 0, 0,  42,   50, 60, 0,   0, 0 },  // charley fermé
            { 100, 9000, 2,   0,  -12,  8,  1,   0, 400,  0,   0,  30,   10,   0,  50,   2, 7400,  18, 1, 0, 0,  420,  50, 60, 0,   0, 0 },  // charley ouvert
            { 100, 1400, 2,   0,  -9,   0,  0,   0, 400,  0,   0,  30,   10,   0,  50,   1, 1250,  24, 1, 2, 24, 300,  50, 70, 0,   0, 0 },  // clap
            { 12, 300,   0,   6,  -8, -15,  0,   0, 110,  0,   7,  130,  10,   0,  360,  0, 2200,  10, 0, 0, 0,  40,   60, 70, 20,  0, 0 },  // tom
            { 18, 2500,  0,   0,  -12, 20,  0,   0, 560,  1,   12, 30,   1040, 0,  210,  1, 3200,  30, 0, 0, 0,  60,   50, 50, 0,   0, 0 },  // percussion métallique
            { 70, 8000,  2,   0,  -13,-10,  0,   2, 3400, 1,   24, 30,   1700, 0,  700,  2, 6200,  8,  1, 0, 0,  1300, 40, 50, 0,   0, 0 },  // cymbale
        }};
        return k;
    }

    inline const ModuleDesc& desc();

    /** SVF TPT à coefficients partagés (un réglage par canal, plusieurs voix et deux côtés). */
    struct SvfCoef { double g = 0.1, k = 1.0, a1 = 0.0, a2 = 0.0, a3 = 0.0; };
    struct SvfState
    {
        double ic1 = 0.0, ic2 = 0.0;
        void reset() noexcept { ic1 = ic2 = 0.0; }
        /** mode 0 passe-bas, 1 passe-bande, 2 passe-haut */
        double process (double x, const SvfCoef& c, int mode) noexcept
        {
            const auto v3 = x - ic2;
            const auto v1 = c.a1 * ic1 + c.a2 * v3;
            const auto v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return mode == 0 ? v2 : mode == 1 ? v1 : x - c.k * v1 - v2;
        }
    };

    /** Réglages d'un canal calculés une fois par bloc (lus par ses deux voix). */
    struct ChBlock
    {
        double oscG = 1.0, noiseG = 0.0;
        int wave = 0, mmode = 0, nfilt = 0, nenv = 0;
        bool stereo = false;
        double freq = 100.0, mamt = 0.0;
        double modCoef = 0.0, modInc = 0.0, rndPeriod = 1.0;
        double oAtkInc = 1.0, oDecCoef = 0.0;
        double nAtkMs = 0.0, nAtkInc = 1.0, nDecCoef = 0.0, nLinStep = 0.0;
        double noiseNorm = 1.0;
        SvfCoef svf;
        double vosc = 0.0, vnoise = 0.0, vmod = 0.0;
    };

    /** Une voix d'un canal (deux par canal : l'ancien coup s'efface pendant que le nouveau part). */
    struct Voice
    {
        bool active = false;
        synth::Osc osc;
        double oEnv = 0.0, nEnv = 0.0, modEnv = 0.0, modPh = 0.0;
        int oStage = 0, nStage = 0; // 0 éteint, 1 attaque, 2 déclin
        double nTime = 0.0;         // secondes écoulées dans l'attaque (enveloppe claquée)
        double rndFrom = 0.0, rndTo = 0.0, rndPos = 0.0;
        double velO = 1.0, velN = 1.0, velM = 1.0, gain = 1.0;
        double fade = 1.0, fadeStep = 0.0;
        dsp::WhiteNoise nzL { 0x51A3u }, nzR { 0x7C2Fu };
        dsp::WhiteNoise rnd { 0x3D11u };
        SvfState fL, fR;

        void start (const ChBlock& b, double vel, double accent) noexcept
        {
            active = true;
            // phase de départ à zéro du signal (aucun saut) : sinus 0, triangle 1/4, scie 1/2
            osc.reset (b.wave == 1 ? 0.25 : b.wave == 2 ? 0.5 : 0.0);
            oEnv = b.oAtkInc >= 1.0 ? 1.0 : 0.0; oStage = b.oAtkInc >= 1.0 ? 2 : 1;
            nEnv = 0.0; nStage = 1; nTime = 0.0;
            if (b.nenv != 2 && b.nAtkInc >= 1.0) { nEnv = 1.0; nStage = 2; }
            modEnv = 1.0; modPh = 0.0;
            rndFrom = 0.0; rndTo = rnd.next(); rndPos = 0.0;
            fL.reset(); fR.reset();
            const auto v = std::clamp (vel, 0.0, 1.0);
            velO = 1.0 - b.vosc * (1.0 - v);
            velN = 1.0 - b.vnoise * (1.0 - v);
            velM = 1.0 - b.vmod * (1.0 - v);
            gain = 1.0 + 0.5 * std::clamp (accent, 0.0, 1.0); // accent du séquenceur : environ +3,5 dB
            fade = 1.0; fadeStep = 0.0;
        }

        void fadeOut (double sr, double ms) noexcept { if (active && fadeStep == 0.0) fadeStep = 1.0 / std::max (1.0, ms * 0.001 * sr); }

        /** Ajoute la voix à L, R (n échantillons). */
        void render (const ChBlock& b, double sr, float* L, float* R, int n) noexcept
        {
            const bool oscOn = b.oscG > 0.0, noiseOn = b.noiseG > 0.0;
            const auto inv = 1.0 / sr;
            for (int i = 0; i < n; ++i)
            {
                // enveloppe de l'oscillateur
                if (oStage == 1) { oEnv += b.oAtkInc; if (oEnv >= 1.0) { oEnv = 1.0; oStage = 2; } }
                else if (oStage == 2) { oEnv *= b.oDecCoef; if (oEnv < 1.0e-4) { oEnv = 0.0; oStage = 0; } }
                // enveloppe du bruit
                double ne = 0.0;
                if (nStage == 1)
                {
                    if (b.nenv == 2)
                    {
                        // claquée : trois salves pendant l'attaque, chacune repart à plein et retombe vite
                        const auto period = b.nAtkMs * 0.001 / 3.0;
                        if (nTime >= b.nAtkMs * 0.001 || period <= 0.0) { nStage = 2; nEnv = 1.0; ne = 1.0; }
                        else { const auto t = std::fmod (nTime, period); ne = std::exp (-t / (0.28 * period)); nTime += inv; }
                    }
                    else { nEnv += b.nAtkInc; if (nEnv >= 1.0) { nEnv = 1.0; nStage = 2; } ne = nEnv; }
                }
                else if (nStage == 2)
                {
                    if (b.nenv == 1) { nEnv -= b.nLinStep; if (nEnv <= 0.0) { nEnv = 0.0; nStage = 0; } }
                    else { nEnv *= b.nDecCoef; if (nEnv < 1.0e-4) { nEnv = 0.0; nStage = 0; } }
                    ne = nEnv;
                }
                if (oStage == 0 && nStage == 0) { active = false; return; }

                double l = 0.0, r = 0.0;
                if (oscOn && oStage != 0)
                {
                    // modulation de hauteur (demi-tons)
                    double m = 0.0;
                    if (b.mmode == 0) { m = modEnv; modEnv *= b.modCoef; }
                    else if (b.mmode == 1) { m = dsp::fastSin2Pi (modPh); modPh += b.modInc; if (modPh >= 1.0) modPh -= 1.0; }
                    else
                    {
                        m = rndFrom + (rndTo - rndFrom) * (rndPos / b.rndPeriod);
                        if ((rndPos += 1.0) >= b.rndPeriod) { rndPos = 0.0; rndFrom = rndTo; rndTo = rnd.next(); }
                    }
                    const auto f = b.freq * dsp::fastExp2 (b.mamt * velM * m / 12.0);
                    const auto w = b.wave == 0 ? synth::Osc::sine : b.wave == 1 ? synth::Osc::triangle : synth::Osc::saw;
                    const auto o = osc.next (w, f * inv, 0.5) * oEnv * b.oscG * velO;
                    l += o; r += o;
                }
                if (noiseOn && ne > 0.0)
                {
                    const auto g = ne * b.noiseG * velN * b.noiseNorm;
                    const auto a = fL.process (nzL.next(), b.svf, b.nfilt);
                    l += a * g;
                    r += (b.stereo ? fR.process (nzR.next(), b.svf, b.nfilt) : a) * g;
                }
                auto k = gain;
                if (fadeStep > 0.0) { fade -= fadeStep; if (fade <= 0.0) { active = false; return; } k *= fade; }
                L[i] += (float) (l * k);
                R[i] += (float) (r * k);
            }
        }
    };
} // namespace mt

//==============================================================================
class MicroTonnerre final : public Instrument
{
public:
    MicroTonnerre()
    {
        // deux bruits et un générateur aléatoire différents pour chaque voix
        std::uint32_t seed = 0x6D74u;
        for (auto& ch : voices)
            for (auto& v : ch)
            {
                seed = seed * 1664525u + 1013904223u; v.nzL = dsp::WhiteNoise (seed | 1u);
                seed = seed * 1664525u + 1013904223u; v.nzR = dsp::WhiteNoise (seed | 1u);
                seed = seed * 1664525u + 1013904223u; v.rnd = dsp::WhiteNoise (seed | 1u);
            }
    }

    const ModuleDesc& desc() const noexcept override { return mt::desc(); }

    void prepare (double s, int) override { sr = s > 0.0 ? s : 48000.0; reset(); }

    void reset() noexcept override
    {
        for (auto& ch : voices) for (auto& v : ch) v.active = false;
        for (auto& e : eq) e.reset();
        for (auto& d : distL) d.reset();
        for (auto& d : distR) d.reset();
        pendingMask = 0;
        nextAccent = 0.0f;
        curL.fill (0.0f); curR.fill (0.0f); curA.fill (0.0f); curB.fill (0.0f);
        silentCh.fill (true);
    }

    void setNextAccent (float amount) noexcept override { nextAccent = amount; }

    void noteOn (int note, float velocity) noexcept override
    {
        // canaux 1 à 8 ; les pads 9 à 16 rejouent les canaux 1 à 8 (contrôleurs à 16 pads)
        if (note < 0) return;
        const int ch = note & 7;
        pending[(size_t) ch] = { velocity, nextAccent };
        pendingMask |= 1u << ch;
        nextAccent = 0.0f;
    }
    void noteOff (int) noexcept override {}
    void allNotesOff() noexcept override { for (auto& ch : voices) for (auto& v : ch) v.fadeOut (sr, 4.0); }

    bool hasPadSends() const noexcept override { return true; }
    void setPadSendBuses (float* aL, float* aR, float* bL, float* bR) noexcept override { busAL = aL; busAR = aR; busBL = bL; busBR = bR; }
    void setPadInserts (const std::array<std::array<Effect*, 2>, 16>* fx, const float* params, const float* bypass, double bpm) noexcept override
    { padChain = fx; padChainParams = params; padChainBypass = bypass; padChainBpm = bpm; }
    void setSidechainSources (const float* const* l, const float* const* r) noexcept override { scSrcL = l; scSrcR = r; }

    bool isActive() const noexcept override
    {
        if (pendingMask != 0) return true;
        for (const auto& ch : voices) for (const auto& v : ch) if (v.active) return true;
        if (padChain != nullptr)
            for (int c = 0; c < mt::numChannels; ++c)
                if ((*padChain)[(size_t) c][0] != nullptr || (*padChain)[(size_t) c][1] != nullptr) return true;
        return false;
    }

    void render (float* left, float* right, int n, const float* p) noexcept override
    {
        for (int done = 0; done < n;)
        {
            const int m = std::min (maxChunk, n - done);
            renderChunk (left + done, right + done, m, p);
            done += m;
        }
    }

    static constexpr int maxChunk = 64;

private:
    void computeBlock (int c, const float* q) noexcept
    {
        using namespace mt;
        auto& b = blk[(size_t) c];
        const auto mix = std::clamp ((double) q[cMix] / 100.0, 0.0, 1.0);
        b.oscG = std::min (1.0, 2.0 * (1.0 - mix));
        b.noiseG = std::min (1.0, 2.0 * mix);
        b.wave = std::clamp ((int) std::lround (q[cWave]), 0, 2);
        b.freq = std::clamp ((double) q[cFreq], 10.0, 0.45 * sr);
        b.mmode = std::clamp ((int) std::lround (q[cMMode]), 0, 2);
        b.mamt = q[cMAmt];
        b.modCoef = dsp::decay60Coef (std::max (1.0f, q[cMTime]), sr);
        b.modInc = std::clamp ((double) q[cMRate] / sr, 0.0, 0.45);
        b.rndPeriod = std::max (1.0, sr / std::max (0.05, (double) q[cMRate]));
        b.oAtkInc = q[cOAtk] < 0.05f ? 1.0 : 1.0 / std::max (1.0, (double) q[cOAtk] * 0.001 * sr);
        b.oDecCoef = dsp::decay60Coef (std::max (1.0f, q[cODec]), sr);
        b.nfilt = std::clamp ((int) std::lround (q[cNFilt]), 0, 2);
        b.stereo = q[cNStereo] > 0.5f;
        b.nenv = std::clamp ((int) std::lround (q[cNEnv]), 0, 2);
        b.nAtkMs = std::max (0.0f, q[cNAtk]);
        b.nAtkInc = q[cNAtk] < 0.05f ? 1.0 : 1.0 / std::max (1.0, (double) q[cNAtk] * 0.001 * sr);
        b.nDecCoef = dsp::decay60Coef (std::max (1.0f, q[cNDec]), sr);
        b.nLinStep = 1.0 / std::max (1.0, (double) q[cNDec] * 0.001 * sr);
        // filtre du bruit : résonance de 0,5 à 100 ; le niveau est ramené (bande étroite = moins d'énergie)
        const auto qv = 0.5 * std::pow (200.0, std::clamp ((double) q[cNQ] / 100.0, 0.0, 1.0));
        const auto fc = std::clamp ((double) q[cNFreq], 20.0, 0.49 * sr);
        if (fc != b.svfFc || qv != b.svfQ)
        {
            b.svfFc = fc; b.svfQ = qv;
            auto& s = b.svf;
            s.g = std::tan (dsp::kPi * fc / sr);
            s.k = 1.0 / qv;
            s.a1 = 1.0 / (1.0 + s.g * (s.g + s.k));
            s.a2 = s.g * s.a1;
            s.a3 = s.g * s.a2;
        }
        b.noiseNorm = 1.0 / std::sqrt (std::max (1.0, qv)) * (b.nfilt == 1 ? 1.6 : 1.0);
        b.vosc = q[cVOsc] / 100.0; b.vnoise = q[cVNoise] / 100.0; b.vmod = q[cVMod] / 100.0;
    }

    void trigger (int c, const float* q, float vel, float acc) noexcept
    {
        using namespace mt;
        auto& pair = voices[(size_t) c];
        auto& cur = pair[(size_t) curVoice[(size_t) c]];
        const bool wasSilent = ! pair[0].active && ! pair[1].active;
        if (cur.active) { cur.fadeOut (sr, 3.0); curVoice[(size_t) c] ^= 1; }
        auto& v = pair[(size_t) curVoice[(size_t) c]];
        v.start (blk[(size_t) c], vel, acc);
        if (wasSilent) { eq[(size_t) c].reset(); distL[(size_t) c].reset(); distR[(size_t) c].reset(); }
        // groupe d'étouffement : les autres canaux du groupe s'effacent en 4 ms
        const auto g = (int) std::lround (q[cChoke]);
        if (g > 0)
            for (int o = 0; o < numChannels; ++o)
                if (o != c && (int) std::lround (lastChoke[(size_t) o]) == g)
                    for (auto& ov : voices[(size_t) o]) ov.fadeOut (sr, 4.0);
    }

    void renderChunk (float* left, float* right, int n, const float* p) noexcept
    {
        using namespace mt;
        const auto inv = 1.0f / (float) std::max (1, n);
        const auto master = (float) dsp::dbToGain (std::clamp (p[volumeIndex], -60.0f, 12.0f));
        for (int c = 0; c < numChannels; ++c) lastChoke[(size_t) c] = p[c * numCh + cChoke];
        // coups en attente : réglages du canal à jour au moment du coup
        if (pendingMask != 0)
            for (int c = 0; c < numChannels; ++c)
                if ((pendingMask >> c) & 1u)
                {
                    computeBlock (c, p + c * numCh);
                    trigger (c, p + c * numCh, pending[(size_t) c].vel, pending[(size_t) c].acc);
                }
        pendingMask = 0;

        for (int c = 0; c < numChannels; ++c)
        {
            const auto* q = p + c * numCh;
            auto& pair = voices[(size_t) c];
            const bool sounding = pair[0].active || pair[1].active;

            Effect* chainFx[2] { nullptr, nullptr };
            bool chainTrail[2] { false, false };
            if (padChain != nullptr)
                for (int k = 0; k < 2; ++k)
                    if (auto* e = (*padChain)[(size_t) c][(size_t) k]; e != nullptr)
                    {
                        if (padChainBypass[c * 2 + k] < 0.5f) chainFx[k] = e;
                        else if (e->trails (padChainParams + (c * 2 + k) * insertSlots)) { chainFx[k] = e; chainTrail[k] = true; }
                    }
            const bool chain = chainFx[0] != nullptr || chainFx[1] != nullptr;

            // niveau, panoramique et envois (passage en douceur d'un bloc à l'autre)
            const auto lvl = q[cLevel] <= -47.9f ? 0.0f : (float) dsp::dbToGain (std::min (12.0f, q[cLevel])) * master;
            const auto pan = std::clamp (q[cPan] / 100.0f, -1.0f, 1.0f);
            const auto tl = lvl * std::min (1.0f, 1.0f - pan), tr = lvl * std::min (1.0f, 1.0f + pan);
            const auto sa = std::clamp (q[cSendA] / 100.0f, 0.0f, 1.0f), sb = std::clamp (q[cSendB] / 100.0f, 0.0f, 1.0f);
            const auto ga = sa * sa, gb = sb * sb;
            const auto l0 = curL[(size_t) c], r0 = curR[(size_t) c], a0 = curA[(size_t) c], b0 = curB[(size_t) c];
            curL[(size_t) c] = tl; curR[(size_t) c] = tr; curA[(size_t) c] = ga; curB[(size_t) c] = gb;

            if (! sounding && ! chain) { silentCh[(size_t) c] = true; continue; }
            std::fill (bufL.begin(), bufL.begin() + n, 0.0f);
            std::fill (bufR.begin(), bufR.begin() + n, 0.0f);
            if (sounding)
            {
                computeBlock (c, q);
                for (auto& v : pair) if (v.active) v.render (blk[(size_t) c], sr, bufL.data(), bufR.data(), n);
                // égaliseur en cloche puis distorsion (stéréo)
                const auto eg = (double) q[cEqG];
                if (std::abs (eg) > 0.05)
                {
                    eq[(size_t) c].set (dsp::SvfEq::bell, std::clamp ((double) q[cEqF], 20.0, 0.45 * sr), 0.9, eg, sr);
                    for (int i = 0; i < n; ++i)
                    {
                        bufL[(size_t) i] = (float) eq[(size_t) c].l.process (bufL[(size_t) i]);
                        bufR[(size_t) i] = (float) eq[(size_t) c].r.process (bufR[(size_t) i]);
                    }
                }
                const auto d = std::clamp ((double) q[cDist] / 100.0, 0.0, 1.0);
                if (d > 0.005)
                {
                    // courbe quadratique (douce au début) ; la crête d'un son à plein niveau est gardée
                    const auto k = 1.0 + 40.0 * d * d, norm = 1.0 / dsp::softClip (k);
                    for (int i = 0; i < n; ++i)
                    {
                        bufL[(size_t) i] = (float) (distL[(size_t) c].process (bufL[(size_t) i] * k) * norm);
                        bufR[(size_t) i] = (float) (distR[(size_t) c].process (bufR[(size_t) i] * k) * norm);
                    }
                }
                silentCh[(size_t) c] = false;
            }
            if (chain)
                for (int k = 0; k < 2; ++k)
                    if (chainFx[k] != nullptr)
                    {
                        chainFx[k]->setSidechainSources (scSrcL, scSrcR);
                        const float* cp = padChainParams + (c * 2 + k) * insertSlots;
                        if (chainTrail[k]) chainFx[k]->processTrails (bufL.data(), bufR.data(), n, cp, padChainBpm);
                        else chainFx[k]->process (bufL.data(), bufR.data(), n, cp, padChainBpm);
                    }

            const auto dl = (tl - l0) * inv, dr = (tr - r0) * inv;
            for (int i = 0; i < n; ++i)
            {
                left[i] += bufL[(size_t) i] * (l0 + dl * (float) i);
                right[i] += bufR[(size_t) i] * (r0 + dr * (float) i);
            }
            if (busAL != nullptr && (ga > 0.0f || a0 > 0.0f))
                for (int i = 0; i < n; ++i)
                {
                    const auto k = a0 + (ga - a0) * inv * (float) i;
                    busAL[i] += bufL[(size_t) i] * k * (l0 + dl * (float) i); busAR[i] += bufR[(size_t) i] * k * (r0 + dr * (float) i);
                }
            if (busBL != nullptr && (gb > 0.0f || b0 > 0.0f))
                for (int i = 0; i < n; ++i)
                {
                    const auto k = b0 + (gb - b0) * inv * (float) i;
                    busBL[i] += bufL[(size_t) i] * k * (l0 + dl * (float) i); busBR[i] += bufR[(size_t) i] * k * (r0 + dr * (float) i);
                }
        }
    }

    struct Pending { float vel = 1.0f, acc = 0.0f; };
    struct ChBlockEx : mt::ChBlock { double svfFc = -1.0, svfQ = -1.0; };

    double sr = 48000.0;
    std::array<std::array<mt::Voice, 2>, mt::numChannels> voices {};
    std::array<int, mt::numChannels> curVoice {};
    std::array<ChBlockEx, mt::numChannels> blk {};
    std::array<dsp::SvfEqStereo, mt::numChannels> eq {};
    std::array<dsp::AdaaSoftClip, mt::numChannels> distL {}, distR {};
    std::array<Pending, mt::numChannels> pending {};
    std::array<float, mt::numChannels> lastChoke {};
    std::array<float, mt::numChannels> curL {}, curR {}, curA {}, curB {};
    std::array<bool, mt::numChannels> silentCh {};
    std::uint32_t pendingMask = 0;
    float nextAccent = 0.0f;
    std::array<float, maxChunk> bufL {}, bufR {};
    float* busAL = nullptr; float* busAR = nullptr; float* busBL = nullptr; float* busBR = nullptr;
    const std::array<std::array<Effect*, 2>, 16>* padChain = nullptr;
    const float* padChainParams = nullptr;
    const float* padChainBypass = nullptr;
    double padChainBpm = 120.0;
    const float* const* scSrcL = nullptr;
    const float* const* scSrcR = nullptr;
};

} // namespace grv::instruments

#include "mt/MtKits.h"

namespace grv::instruments::mt
{
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "microton";
            m.name = "Micro Tonnerre";
            m.family = "Batteries";
            m.principle = "G\xC3\xA9n\xC3\xA9rateur de rythmes \xC3\xA0 synth\xC3\xA8se universelle : 8 canaux, oscillateur \xC3\xA0 modulation de hauteur et bruit filtr\xC3\xA9, \xC3\xA9galiseur et distorsion";
            m.drumKit = true;
            const char* names[numChannels] { "Grosse caisse", "Caisse claire", "Charley ferm\xC3\xA9", "Charley ouvert", "Clap", "Tom", "Percussion", "Cymbale" };
            const char* kinds[numChannels] { "kick", "snare", "hatClosed", "hatOpen", "clap", "tom", "cowbell", "hatOpen" };
            const auto& init = initKit();
            const std::vector<std::string> chokeChoices { "Aucun", "Groupe 1", "Groupe 2", "Groupe 3", "Groupe 4" };
            for (int c = 0; c < numChannels; ++c)
            {
                const auto px = prefix (c);
                const auto& k = init[(size_t) c];
                m.params.push_back (P (px + "mix", "Oscillateur / bruit", "%", 0, 100, k.mix));
                m.params.push_back (P (px + "eqf", "Fr\xC3\xA9quence \xC3\xA9galiseur", "Hz", 20, 20000, k.eqf, 0.25f));
                m.params.push_back (P (px + "eqg", "Gain \xC3\xA9galiseur", "dB", -40, 40, k.eqg));
                m.params.push_back (P (px + "dist", "Distorsion", "%", 0, 100, k.dist));
                m.params.push_back (P (px + "level", "Niveau", "dB", -48, 12, k.level));
                m.params.push_back (P (px + "pan", "Panoramique", "pan", -100, 100, k.pan));
                m.params.push_back (C (px + "choke", "Groupe d'\xC3\xA9touffement", chokeChoices, (int) k.choke));
                m.params.push_back (C (px + "wave", "Forme d'onde", { "Sinus", "Triangle", "Dent de scie" }, (int) k.wave));
                m.params.push_back (P (px + "freq", "Fr\xC3\xA9quence oscillateur", "Hz", 20, 20000, k.freq, 0.25f));
                m.params.push_back (C (px + "mmode", "Modulation de hauteur", { "D\xC3\xA9" "clin", "Sinus", "Al\xC3\xA9" "atoire" }, (int) k.mmode));
                m.params.push_back (P (px + "mamt", "Quantit\xC3\xA9 de modulation", "dt", -96, 96, k.mamt));
                m.params.push_back (P (px + "mtime", "Temps de modulation", "ms", 1, 10000, k.mtime, 0.3f));
                m.params.push_back (P (px + "mrate", "Vitesse de modulation", "Hz", 0.1f, 2000, k.mrate, 0.25f));
                m.params.push_back (P (px + "oatk", "Attaque oscillateur", "ms", 0, 10000, k.oatk, 0.3f));
                m.params.push_back (P (px + "odec", "D\xC3\xA9" "clin oscillateur", "ms", 10, 10000, k.odec, 0.3f));
                m.params.push_back (C (px + "nfilt", "Filtre du bruit", { "Passe-bas", "Passe-bande", "Passe-haut" }, (int) k.nfilt));
                m.params.push_back (P (px + "nfreq", "Fr\xC3\xA9quence du bruit", "Hz", 20, 20000, k.nfreq, 0.25f));
                m.params.push_back (P (px + "nq", "R\xC3\xA9sonance du bruit", "%", 0, 100, k.nq));
                m.params.push_back (C (px + "nstereo", "Bruit st\xC3\xA9r\xC3\xA9o", { "Non", "Oui" }, (int) k.nstereo));
                m.params.push_back (C (px + "nenv", "Enveloppe du bruit", { "Exponentielle", "Lin\xC3\xA9" "aire", "Claqu\xC3\xA9" "e" }, (int) k.nenv));
                m.params.push_back (P (px + "natk", "Attaque bruit", "ms", 0, 10000, k.natk, 0.3f));
                m.params.push_back (P (px + "ndec", "D\xC3\xA9" "clin bruit", "ms", 10, 10000, k.ndec, 0.3f));
                m.params.push_back (P (px + "vosc", "V\xC3\xA9locit\xC3\xA9 \xE2\x86\x92 oscillateur", "%", 0, 100, k.vosc));
                m.params.push_back (P (px + "vnoise", "V\xC3\xA9locit\xC3\xA9 \xE2\x86\x92 bruit", "%", 0, 100, k.vnoise));
                m.params.push_back (P (px + "vmod", "V\xC3\xA9locit\xC3\xA9 \xE2\x86\x92 modulation", "%", 0, 100, k.vmod));
                m.params.push_back (P (px + "sendA", "Envoi A", "%", 0, 100, k.sendA));
                m.params.push_back (P (px + "sendB", "Envoi B", "%", 0, 100, k.sendB));
                m.padNames.push_back (names[c]);
                m.padKinds.push_back (kinds[c]);
            }
            m.params.push_back (P ("volume", "Volume du kit", "dB", -24, 6, 0));

            // Sections par canal (écran SON des batteries : un onglet par partie du son)
            for (int c = 0; c < numChannels; ++c)
            {
                const auto px = prefix (c);
                auto sec = [&] (const std::string& id, const std::string& visual, const std::string& tab, std::initializer_list<const char*> ids)
                {
                    SectionDesc s;
                    s.id = id + std::to_string (c + 1); s.name = names[c]; s.visual = visual; s.tab = tab;
                    for (auto* x : ids) s.params.push_back (px + x);
                    m.sections.push_back (s);
                };
                sec ("pad", std::string ("drumpad:") + kinds[c], "", { "freq", "odec", "wave", "mmode", "mamt", "mtime", "mrate", "oatk" });
                sec ("padnoise", "padnoise", "padnoise", { "nfilt", "nfreq", "nq", "nstereo", "nenv", "natk", "ndec", "mix" });
                sec ("padmix", "padmix", "padmix", { "mix", "eqf", "eqg", "dist", "level", "pan", "choke" });
                sec ("padamp", "padvel", "padamp", { "vosc", "vnoise", "vmod", "level" });
                sec ("padfx", "padsend", "padfx", { "sendA", "sendB" });
            }
            SectionDesc kit;
            kit.id = "kitmaster"; kit.name = "Sortie du kit"; kit.visual = "kit"; kit.tab = "kit";
            kit.params = { "volume" };
            m.sections.push_back (kit);

            // son de départ, puis les kits d'usine (calculés : mt/MtKits.h)
            Preset init0 { "Init", {}, "Init" };
            m.presets.push_back (init0);
            addFactoryKits (m);
            return m;
        }();
        return d;
    }
} // namespace grv::instruments::mt
