/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Octo Lyma » — principe des synthés « organismiques » à 8 voix pour drones (architecture
    reprise ; ni nom, ni façade, ni son d'origine) :
      - 8 voix accordées une à une, rangées en 4 paires (1-2, 3-4, 5-6, 7-8) et 2 groupes de 4 ;
      - chaque voix : triangle qu'on durcit jusqu'au carré (« Durcir », par paire), enveloppe
        lente ou rapide (attaque, déclin par groupe) ;
      - modulation par paire : sa profondeur et sa source — la paire voisine (FM dont l'effet
        suit l'enveloppe des voix qui modulent), le LFO, ou le retour de la sortie ;
      - « Tenue » par groupe : tant qu'une note du groupe est tenue, toutes ses voix sonnent au
        moins à ce niveau (le bourdon) ; transposition par groupe ;
      - LFO « hyper » : deux LFO carrés combinés en ET ou en OU, le premier pouvant moduler la
        vitesse du second (« Lier ») ;
      - retard modulé : deux lignes (gauche, droite) à réinjection croisée, temps modulés par le
        LFO ou par leur propre sortie ; distorsion en fin de chaîne.
      - v0.46 (ajout, absent de la machine d'origine qui n'a pas de filtre) : filtre d'état 12 dB
        (passe-bas, passe-bande, passe-haut) sur les voix avant le retard, coupure modulable par
        le LFO hyper ; grand ouvert par défaut (passe-bas à 20 kHz : contourné), les sons d'usine
        ne changent pas.
      - v0.46 (ajout) : LFO A et B et temps du retard calés sur le tempo (divisions), au choix.
    Jeu : « Plaques » — une note joue la voix (note modulo 8) à sa hauteur réglée ; « Clavier » —
    les 8 voix jouent les notes reçues.
    Modèle : phase accumulée, triangle mis en forme (tanh), FM par la phase ; retards à
    interpolation linéaire ; aucune latence ajoutée.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

#include <array>
#include <cstdint>
#include <cmath>
#include <vector>

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace lyra
{
    constexpr int numVoices = 8;
    // réglages globaux, puis 8 accords de voix, puis 4 paires (durcir, modulation, source)
    enum Global
    {
        play = 0, base, q1Pitch, q2Pitch, q1Hold, q2Hold, q1Att, q2Att, q1Dec, q2Dec,
        lfoA, lfoB, lfoMode, lfoLink,
        dTime1, dTime2, dMod1, dMod2, dModSrc, dFb, dMix,
        drive, velAmp, level,
        numGlobal
    };
    constexpr int tuneBase = numGlobal;            // t1..t8
    constexpr int pairBase = tuneBase + numVoices; // p{k}sharp, p{k}mod, p{k}src
    constexpr int pairStride = 3;
    inline constexpr int pairParam (int pair, int field) noexcept { return pairBase + pair * pairStride + field; }
    // v0.46 : filtre, ajouté après les paires (les positions des réglages existants ne bougent pas)
    enum Filter { fCut = pairBase + 4 * pairStride, fReso, fMode, fLfo,
                  lfoSync, lfoADiv, lfoBDiv, dSync, dDiv1, dDiv2, numParams }; // v0.46 : synchro au tempo
    inline const std::vector<std::string>& lfoDivisions()
    {
        static const std::vector<std::string> d { "4 mes.", "2 mes.", "1 mes.", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T" };
        return d;
    }
    inline double lfoQuarters (int i) noexcept
    {
        static constexpr double q[11] { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
        return q[std::clamp (i, 0, 10)];
    }
    inline const std::vector<std::string>& delayDivisions()
    {
        static const std::vector<std::string> d { "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/4.", "1/2" };
        return d;
    }
    inline double delayQuarters (int i) noexcept
    {
        static constexpr double q[11] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0 };
        return q[std::clamp (i, 0, 10)];
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            ModuleDesc m;
            m.type = "lyra";
            m.name = "Octo Lyma";
            m.family = "Drones";
            m.principle = "8 voix en paires et en groupes, FM crois\xC3\xA9" "e entre paires, tenue en bourdon, LFO hyper (ET / OU), retard modul\xC3\xA9 \xC3\xA0 deux lignes et distorsion";
            m.params = {
                C ("play", "Jeu", { "Plaques", "Clavier" }),
                I ("base", "Note de base", "note", 24, 72, 48),
                P ("q1pitch", "Hauteur 1-4", "dt", -24, 24, 0),
                P ("q2pitch", "Hauteur 5-8", "dt", -24, 24, 0),
                P ("q1hold", "Tenue 1-4", "%", 0, 100, 0),
                P ("q2hold", "Tenue 5-8", "%", 0, 100, 0),
                P ("q1att", "Attaque 1-4", "ms", 2, 6000, 40, 0.3f),
                P ("q2att", "Attaque 5-8", "ms", 2, 6000, 40, 0.3f),
                P ("q1dec", "D\xC3\xA9" "clin 1-4", "ms", 20, 12000, 900, 0.3f),
                P ("q2dec", "D\xC3\xA9" "clin 5-8", "ms", 20, 12000, 900, 0.3f),
                P ("lfoa", "LFO A", "Hz", 0.05f, 60, 2, 0.3f),
                P ("lfob", "LFO B", "Hz", 0.05f, 60, 3, 0.3f),
                C ("lfomode", "Combinaison", { "ET", "OU" }),
                C ("lfolink", "Lier A \xE2\x86\x92 B", onOff),
                P ("dtime1", "Temps 1", "ms", 1, 1000, 260, 0.4f),
                P ("dtime2", "Temps 2", "ms", 1, 1000, 390, 0.4f),
                P ("dmod1", "Modulation 1", "%", 0, 100, 0),
                P ("dmod2", "Modulation 2", "%", 0, 100, 0),
                C ("dmodsrc", "Source", { "LFO", "Soi-m\xC3\xAAme" }),
                P ("dfb", "R\xC3\xA9injection", "%", 0, 95, 40),
                P ("dmix", "Retard", "%", 0, 100, 25),
                P ("drive", "Distorsion", "%", 0, 100, 0),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 30),
                P ("level", "Volume", "dB", -30, 6, -14),
            };
            // accords de départ : deux accords ouverts (quintes, octaves)
            const float tunes[numVoices] { 0, 7, 12, 19, -5, 2, 10, 14 };
            for (int v = 0; v < numVoices; ++v)
                m.params.push_back (P ("t" + std::to_string (v + 1), "Voix " + std::to_string (v + 1), "dt", -24, 24, tunes[v]));
            static const char* pn[4] { "1-2", "3-4", "5-6", "7-8" };
            for (int k = 0; k < 4; ++k)
            {
                const auto o = "p" + std::to_string (k + 1);
                m.params.push_back (P (o + "sharp", std::string ("Forme (triangle \xE2\x86\x92 carr\xC3\xA9) ") + pn[k], "%", 0, 100, 10));
                m.params.push_back (P (o + "mod", std::string ("Modulation ") + pn[k], "%", 0, 100, 0));
                m.params.push_back (C (o + "src", std::string ("Source ") + pn[k], { "Paire voisine", "LFO", "Retour" }));
            }
            m.params.push_back (P ("fcut", "Coupure", "Hz", 20, 20000, 20000, 0.3f));
            m.params.push_back (P ("freso", "R\xC3\xA9sonance", "%", 0, 100, 0));
            m.params.push_back (C ("fmode", "Type de filtre", { "Passe-bas", "Passe-bande", "Passe-haut" }));
            m.params.push_back (P ("flfo", "LFO \xE2\x86\x92 coupure", "%", -100, 100, 0));
            m.params.push_back (C ("lfosync", "LFO au tempo", { "Libre", "Tempo" }));
            m.params.push_back (C ("lfoadiv", "LFO A \xC2\xB7 division", lfoDivisions(), 5));
            m.params.push_back (C ("lfobdiv", "LFO B \xC2\xB7 division", lfoDivisions(), 8));
            m.params.push_back (C ("dsync", "Retard au tempo", { "Libre", "Tempo" }));
            m.params.push_back (C ("ddiv1", "Temps 1 \xC2\xB7 division", delayDivisions(), 7));
            m.params.push_back (C ("ddiv2", "Temps 2 \xC2\xB7 division", delayDivisions(), 8));
            m.sections = {
                { "voices", "Voix", "osc", { "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8" }, "osc" },
                { "pairs", "Paires", "osc", { "p1sharp", "p1mod", "p1src", "p2sharp", "p2mod", "p2src", "p3sharp", "p3mod", "p3src", "p4sharp", "p4mod", "p4src" }, "osc" },
                { "groups", "Groupes", "env", { "q1pitch", "q1hold", "q1att", "q1dec", "q2pitch", "q2hold", "q2att", "q2dec" }, "env" },
                { "lfo", "LFO hyper", "lfo", { "lfoa", "lfob", "lfomode", "lfolink", "lfosync", "lfoadiv", "lfobdiv" }, "mod" },
                { "filter", "Filtre", "filter", { "fcut", "freso", "fmode", "flfo" }, "filter" },
                { "delay", "Retard modul\xC3\xA9", "fx", { "dtime1", "dtime2", "dmod1", "dmod2", "dmodsrc", "dfb", "dmix", "dsync", "ddiv1", "ddiv2" }, "fx" },
                { "global", "Global", "global", { "play", "base", "drive", "velamp", "level" }, "global" },
            };
            using V = std::vector<std::pair<std::string, float>>;
            m.presets = {
                { "Init", V {}, "Init" },
                { "Bourdon d'orgue", V { { "q1hold", 55 }, { "q2hold", 40 }, { "q1att", 900 }, { "q2att", 1400 }, { "q1dec", 4000 }, { "q2dec", 5000 }, { "p1mod", 15 }, { "p2mod", 10 },
                                         { "dmix", 35 }, { "dfb", 55 }, { "level", -9.8f } }, "Nappes" },
                { "Essaim", V { { "p1mod", 60 }, { "p2mod", 45 }, { "p3mod", 55 }, { "p4mod", 40 }, { "p1src", 1 }, { "p3src", 1 }, { "lfoa", 5.5f }, { "lfob", 7.1f }, { "lfolink", 1 },
                                { "dmod1", 40 }, { "dmod2", 30 }, { "dmix", 40 }, { "drive", 30 }, { "level", -6.8f } }, "Effets" },
            };
            addFactoryBank (m); // banque d'usine, au moins 10 sons par catégorie (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

/** Ligne de retard à interpolation linéaire (taille fixée à la préparation). */
class LyraDelay
{
public:
    void prepare (double sr) { buf.assign ((size_t) (sr * 2.2) + 4, 0.0f); w = 0; } // 2 s : la blanche à 60 BPM
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void write (double x) noexcept { buf[(size_t) w] = (float) x; if (++w >= (int) buf.size()) w = 0; }
    double read (double samples) const noexcept
    {
        const auto n = (int) buf.size();
        samples = std::clamp (samples, 1.0, (double) n - 3.0);
        auto pos = (double) w - samples;
        while (pos < 0.0) pos += n;
        const auto i = (int) pos;
        const auto f = pos - i;
        const auto a = buf[(size_t) i], b = buf[(size_t) ((i + 1) % n)];
        return a + f * (b - a);
    }
private:
    std::vector<float> buf;
    int w = 0;
};

class LyraSynth final : public Instrument
{
public:
    const ModuleDesc& desc() const noexcept override { return lyra::desc(); }

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate;
        d1.prepare (sr); d2.prepare (sr);
        reset();
    }
    void reset() noexcept override
    {
        for (auto& v : vs) v = Voice {};
        d1.reset(); d2.reset();
        lfoPhA = lfoPhB = 0.0; lfoOut = 0.0; fb = 0.0; y1 = y2 = 0.0; fz1 = fz2 = 0.0; fG = fK = -1.0; dcL = dcR = 0.0; dcXL = dcXR = 0.0;
        held.fill (0); voiceHeld.fill (0); noteVoice.fill (-1); nextVoice = 0;
    }

    void noteOn (int note, float velocity) noexcept override
    {
        note = std::clamp (note, 0, 127);
        if (noteVoice[(size_t) note] >= 0) noteOff (note); // note rejouée sans relâchement
        int v = 0;
        if (keyboard) { v = nextVoice; nextVoice = (nextVoice + 1) % lyra::numVoices; }
        else v = note % lyra::numVoices;
        auto& x = vs[(size_t) v];
        x.note = note; x.gate = true; x.vel = velocity;
        noteVoice[(size_t) note] = (std::int8_t) v;
        ++voiceHeld[(size_t) v];
        ++held[(size_t) (v / 4)];
    }
    void noteOff (int note) noexcept override
    {
        note = std::clamp (note, 0, 127);
        const int v = noteVoice[(size_t) note];
        if (v < 0) return;
        noteVoice[(size_t) note] = -1;
        if (voiceHeld[(size_t) v] > 0 && --voiceHeld[(size_t) v] == 0) vs[(size_t) v].gate = false;
        if (held[(size_t) (v / 4)] > 0) --held[(size_t) (v / 4)];
    }
    void allNotesOff() noexcept override
    {
        for (auto& x : vs) x.gate = false;
        held.fill (0); voiceHeld.fill (0); noteVoice.fill (-1);
    }

    void setTempo (double b) noexcept override { bpm = b; }

    bool isActive() const noexcept override
    {
        for (const auto& x : vs) if (x.env > 1.0e-5 || x.hold > 1.0e-5) return true;
        return std::abs (y1) > 1.0e-5 || std::abs (y2) > 1.0e-5 || std::abs (fz1) > 1.0e-5 || std::abs (fz2) > 1.0e-5;
    }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        using namespace lyra;
        keyboard = p[play] > 0.5f;
        // par voix : hauteur, incrément, dureté ; par groupe : tenue, attaque, déclin
        std::array<double, numVoices> inc {}, k {};
        std::array<double, 2> holdT {}, aC {}, dC {};
        for (int q = 0; q < 2; ++q)
        {
            holdT[(size_t) q] = held[(size_t) q] > 0 ? p[q == 0 ? q1Hold : q2Hold] / 100.0 : 0.0;
            aC[(size_t) q] = 1.0 / std::max (1.0, p[q == 0 ? q1Att : q2Att] * 0.001 * sr);
            dC[(size_t) q] = std::exp (std::log (0.001) / std::max (1.0, p[q == 0 ? q1Dec : q2Dec] * 0.001 * sr));
        }
        for (int v = 0; v < numVoices; ++v)
        {
            const auto& x = vs[(size_t) v];
            const auto qp = p[v < 4 ? q1Pitch : q2Pitch];
            const auto nt = keyboard ? (double) x.note + qp : (double) p[base] + qp + p[tuneBase + v];
            inc[(size_t) v] = std::clamp (noteHz (nt) / sr, 0.0, 0.45);
            const auto s = p[pairParam (v / 2, 0)] / 100.0;
            k[(size_t) v] = 1.0 + s * s * 24.0;
        }
        std::array<double, 4> modAmt {};
        std::array<int, 4> modSrc {};
        for (int pr = 0; pr < 4; ++pr)
        {
            const auto a = p[pairParam (pr, 1)] / 100.0;
            modAmt[(size_t) pr] = a * a;
            modSrc[(size_t) pr] = std::clamp ((int) std::lround (p[pairParam (pr, 2)]), 0, 2);
        }
        // v0.46 : au tempo, la vitesse vient de la division (une période = la durée de la division)
        const bool lSync = p[lfoSync] > 0.5f, dSyncOn = p[dSync] > 0.5f;
        const auto qps = std::max (20.0, bpm) / 60.0; // noires par seconde
        const auto incA = (lSync ? qps / lfoQuarters ((int) std::lround (p[lfoADiv])) : (double) p[lfoA]) / sr;
        const auto incBb = (lSync ? qps / lfoQuarters ((int) std::lround (p[lfoBDiv])) : (double) p[lfoB]) / sr;
        const bool orMode = p[lfoMode] > 0.5f, link = p[lfoLink] > 0.5f;
        const auto t1 = dSyncOn ? delayQuarters ((int) std::lround (p[dDiv1])) / qps * sr : p[dTime1] * 0.001 * sr;
        const auto t2 = dSyncOn ? delayQuarters ((int) std::lround (p[dDiv2])) / qps * sr : p[dTime2] * 0.001 * sr;
        const auto m1 = p[dMod1] / 100.0, m2 = p[dMod2] / 100.0;
        const bool selfMod = p[dModSrc] > 0.5f;
        const auto fbk = p[dFb] / 100.0, mix = p[dMix] / 100.0;
        const auto dr = p[drive] / 100.0, drv = 1.0 + dr * dr * 20.0, drvComp = 1.0 / std::sqrt (drv);
        const auto gain = dbToGain (p[level]);
        const auto velS = p[velAmp] / 100.0;
        const auto lfoSmooth = 1.0 - std::exp (-1.0 / (0.002 * sr));
        // filtre d'état (TPT) : coupure en octaves, modulée par le LFO (±4 octaves), recalculée tous
        // les 16 échantillons et lissée ; passe-bas grand ouvert sans modulation : contourné
        const int fType = std::clamp ((int) std::lround (p[fMode]), 0, 2);
        const auto cutOct = std::log2 (std::clamp ((double) p[fCut], 20.0, 20000.0));
        const auto lfoDepth = p[fLfo] / 100.0 * 4.0;
        const auto rs = p[fReso] / 100.0;
        const auto fKTarget = 2.0 - 1.96 * rs * rs; // 2 = sans résonance (Q 0,5) … 0,04 = au bord de l'auto-oscillation
        const bool fBypass = fType == 0 && p[fCut] >= 19000.0f && std::abs (lfoDepth) < 1.0e-4;
        if (fBypass) { fz1 = fz2 = 0.0; fG = -1.0; }
        const auto fSmooth = 1.0 - std::exp (-16.0 / (0.004 * sr));

        for (int i = 0; i < n; ++i)
        {
            // LFO hyper : deux carrés combinés (adoucis sur 2 ms)
            lfoPhA += incA; if (lfoPhA >= 1.0) lfoPhA -= 1.0;
            const double a = lfoPhA < 0.5 ? 1.0 : 0.0;
            lfoPhB += incBb * (link ? (0.5 + a) : 1.0); if (lfoPhB >= 1.0) lfoPhB -= 1.0;
            const double b = lfoPhB < 0.5 ? 1.0 : 0.0;
            const auto target = orMode ? std::max (a, b) : a * b;
            lfoOut += (target - lfoOut) * lfoSmooth;
            const auto lfoBi = lfoOut * 2.0 - 1.0;

            // sortie de chaque voix au pas précédent, pour la FM croisée entre paires
            std::array<double, 4> pairOut {};
            for (int pr = 0; pr < 4; ++pr) pairOut[(size_t) pr] = 0.5 * (vs[(size_t) (2 * pr)].last + vs[(size_t) (2 * pr + 1)].last);
            double sum = 0.0;
            for (int v = 0; v < numVoices; ++v)
            {
                auto& x = vs[(size_t) v];
                const int q = v / 4, pr = v / 2;
                // enveloppe : monte tant que la note est tenue, redescend sinon ; tenue du groupe
                if (x.gate) x.env = std::min (1.0, x.env + aC[(size_t) q]);
                else x.env *= dC[(size_t) q];
                x.hold += (holdT[(size_t) q] - x.hold) * (holdT[(size_t) q] > x.hold ? aC[(size_t) q] : (1.0 - dC[(size_t) q]));
                const auto amp = std::max (x.env * (1.0 - velS * (1.0 - x.vel)), x.hold);
                if (amp < 1.0e-6 && x.env < 1.0e-6) { x.last = 0.0; continue; }
                double mod = 0.0;
                switch (modSrc[(size_t) pr])
                {
                    case 0: mod = pairOut[(size_t) (pr ^ 1)] * 1.5; break; // paire voisine (déjà pondérée par son enveloppe)
                    case 1: mod = lfoBi * 0.12; break;                     // LFO : vibrato
                    default: mod = fb * 1.2; break;                        // retour de la sortie
                }
                x.ph += inc[(size_t) v] * (1.0 + (modSrc[(size_t) pr] == 1 ? modAmt[(size_t) pr] * mod : 0.0));
                if (x.ph >= 1.0) x.ph -= 1.0;
                auto ph = x.ph + (modSrc[(size_t) pr] == 1 ? 0.0 : modAmt[(size_t) pr] * mod);
                ph -= std::floor (ph);
                const auto tri = ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph;
                const auto kk = k[(size_t) v];
                const auto w = fastTanh (tri * kk) / fastTanh (kk);
                x.last = w * amp;
                sum += x.last;
            }
            auto dry = sum * 0.3;
            if (! fBypass)
            {
                if ((i & 15) == 0)
                {
                    const auto hz = std::clamp (std::exp2 (cutOct + lfoDepth * lfoBi), 20.0, 0.45 * sr);
                    const auto g = std::tan (kPi * hz / sr);
                    if (fG < 0.0) { fG = g; fK = fKTarget; }
                    fG += (g - fG) * fSmooth; fK += (fKTarget - fK) * fSmooth;
                }
                // une forte résonance sature doucement (jamais d'emballement)
                const auto hp = (dry - (fK + fG) * fz1 - fz2) / (1.0 + fK * fG + fG * fG);
                const auto bp = fastTanh (fG * hp + fz1);
                const auto lp = fG * bp + fz2;
                fz1 = 2.0 * bp - fz1; fz2 = 2.0 * lp - fz2;
                fz1 = std::clamp (fz1, -8.0, 8.0); fz2 = std::clamp (fz2, -8.0, 8.0);
                dry = fType == 0 ? lp : fType == 1 ? bp * fK : hp;
            }

            // retard : deux lignes, réinjection croisée, temps modulés par le LFO ou par elles-mêmes
            const auto src1 = selfMod ? y1 : lfoBi, src2 = selfMod ? y2 : lfoBi;
            const auto r1 = d1.read (t1 * (1.0 + 0.5 * m1 * src1));
            const auto r2 = d2.read (t2 * (1.0 + 0.5 * m2 * src2));
            y1 = r1; y2 = r2;
            d1.write (fastTanh (dry + fbk * r2));
            d2.write (fastTanh (dry + fbk * r1));
            auto l = dry + mix * r1, r = dry + mix * r2;
            fb = 0.5 * (l + r);
            if (dr > 0.0) { l = fastTanh (l * drv) * drvComp; r = fastTanh (r * drv) * drvComp; }
            // bloqueur de continu (la distorsion et le retour peuvent en créer)
            dcL = l - dcXL + 0.995 * dcL; dcXL = l;
            dcR = r - dcXR + 0.995 * dcR; dcXR = r;
            L[i] += (float) (dcL * gain);
            R[i] += (float) (dcR * gain);
        }
        if (std::abs (fz1) < 1.0e-12) fz1 = 0.0;
        if (std::abs (fz2) < 1.0e-12) fz2 = 0.0;
        if (std::abs (y1) < 1.0e-7) y1 = 0.0;
        if (std::abs (y2) < 1.0e-7) y2 = 0.0;
    }

private:
    struct Voice
    {
        double ph = 0.0, env = 0.0, hold = 0.0, last = 0.0;
        float vel = 1.0f;
        int note = 60;
        bool gate = false;
    };
    std::array<Voice, lyra::numVoices> vs {};
    std::array<int, 2> held {};                 // notes tenues par groupe (tenue du bourdon)
    std::array<int, lyra::numVoices> voiceHeld {}; // notes tenues par voix
    std::array<std::int8_t, 128> noteVoice = [] { std::array<std::int8_t, 128> a {}; a.fill (-1); return a; }();
    LyraDelay d1, d2;
    double sr = 48000.0, lfoPhA = 0.0, lfoPhB = 0.0, lfoOut = 0.0, fb = 0.0, y1 = 0.0, y2 = 0.0, dcL = 0.0, dcR = 0.0, dcXL = 0.0, dcXR = 0.0;
    double fz1 = 0.0, fz2 = 0.0, fG = -1.0, fK = 2.0; // filtre d'état (v0.46)
    double bpm = 120.0;
    int nextVoice = 0;
    bool keyboard = false;
};

} // namespace grv::instruments
