/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Porte et dé-esseur » : deux traitements de dynamique sans anticipation (latence nulle).
      - Porte : détection crête stéréo liée après un filtre coupe-bas (la grosse caisse ne rouvre
        pas la porte d'une charleston), seuil avec hystérésis (pas de claquements autour du seuil),
        maintien, attaque et relâchement en dB, atténuation réglable (une porte à 12 dB laisse
        respirer la pièce). Seuil au minimum : porte toujours ouverte.
      - Dé-esseur : détection sur la bande des sifflantes ; en mode « bande haute », seule la part
        au-dessus de la fréquence est atténuée (séparation complémentaire : grave + aigu = signal
        d'origine à l'identique quand rien n'est atténué) ; en mode « large », tout le signal
        baisse. « Écoute » fait entendre la bande détectée pour régler la fréquence.
    Mesures : 0 réduction totale, 1 réduction de la porte, 2 réduction du dé-esseur (dB).
    Idée reprise de la tranche de Sillage Studio (même auteur), réécrite pour Groovebox.
*/

#pragma once

#include "../dsp/Common.h"
#include "../dsp/EqFilters.h"
#include "../engine/Module.h"

#include <algorithm>
#include <cmath>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace gate
{
    enum Slot { gThresh = 0, gRange, gAttack, gHold, gRelease, gHyst, gHp,
                dsFreq, dsThresh, dsAmount, dsMode, dsListen, numSlots };

    constexpr float gateOffDb = -80.0f;

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "gate"; m.name = "Sentinel"; m.role = "Porte et d\xC3\xA9-esseur"; m.family = "Compresseurs et dynamique"; m.form = "rack";
            m.principle = "Porte de bruit \xC3\xA0 hyst\xC3\xA9r\xC3\xA9sis et maintien, filtre de d\xC3\xA9tection ; d\xC3\xA9-esseur sur la bande des sifflantes, sans latence";
            m.params = {
                P ("gThresh", "Seuil", "dB", gateOffDb, 0, -50),
                P ("gRange", "Att\xC3\xA9nuation", "dB", 0, 80, 40),
                P ("gAttack", "Attaque", "ms", 0.05f, 50, 0.5f, 0.35f),
                P ("gHold", "Maintien", "ms", 0, 500, 30, 0.5f),
                P ("gRelease", "Rel\xC3\xA2" "chement", "ms", 5, 2000, 120, 0.35f),
                P ("gHyst", "Hyst\xC3\xA9r\xC3\xA9sis", "dB", 0, 12, 4),
                P ("gHp", "Filtre de d\xC3\xA9tection", "Hz", 20, 2000, 20, 0.3f),
                P ("dsFreq", "Fr\xC3\xA9quence", "Hz", 2000, 12000, 6500, 0.5f),
                P ("dsThresh", "Seuil", "dB", -60, 0, -30),
                P ("dsAmount", "R\xC3\xA9" "duction max", "dB", 0, 24, 0),
                C ("dsMode", "Mode", { "Bande haute", "Large" }),
                C ("dsListen", "\xC3\x89" "coute", { "Non", "Oui" }),
            };
            m.sections = {
                { "gate", "Porte", "gate", { "gThresh", "gRange", "gAttack", "gHold", "gRelease", "gHyst", "gHp" }, "main" },
                { "deess", "D\xC3\xA9-esseur", "deess", { "dsFreq", "dsThresh", "dsAmount", "dsMode", "dsListen" }, "main" },
            };
            m.presets = {
                { "Porte douce", {} },
                { "Batterie serr\xC3\xA9" "e", { { "gThresh", -32 }, { "gRange", 80 }, { "gAttack", 0.1f }, { "gHold", 12 }, { "gRelease", 60 }, { "gHp", 120 } } },
                { "Caisse claire et toms", { { "gThresh", -28 }, { "gRange", 30 }, { "gAttack", 0.2f }, { "gHold", 40 }, { "gRelease", 180 }, { "gHp", 90 } } },
                { "Souffle de guitare", { { "gThresh", -55 }, { "gRange", 30 }, { "gAttack", 2 }, { "gHold", 60 }, { "gRelease", 300 }, { "gHyst", 6 } } },
                { "Porte rythmique (coupure franche)", { { "gThresh", -20 }, { "gRange", 80 }, { "gAttack", 0.05f }, { "gHold", 0 }, { "gRelease", 15 }, { "gHyst", 0 } } },
                { "Voix : d\xC3\xA9-esseur", { { "gThresh", gateOffDb }, { "dsFreq", 6500 }, { "dsThresh", -28 }, { "dsAmount", 8 } } },
                { "Voix : porte et d\xC3\xA9-esseur", { { "gThresh", -48 }, { "gRange", 18 }, { "gAttack", 1 }, { "gHold", 80 }, { "gRelease", 250 }, { "gHp", 100 },
                                                         { "dsFreq", 6000 }, { "dsThresh", -30 }, { "dsAmount", 8 } } },
                { "Charleston adoucie", { { "gThresh", gateOffDb }, { "dsFreq", 9000 }, { "dsThresh", -26 }, { "dsAmount", 6 } } },
                { "Sifflantes en \xC3\xA9" "coute", { { "gThresh", gateOffDb }, { "dsAmount", 8 }, { "dsListen", 1 } } },
            };
            return m;
        }();
        return d;
    }
}

class GateDeesser final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return gate::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override
    {
        scHpL.reset(); scHpR.reset(); splitL.reset(); splitR.reset(); dsL.reset(); dsR.reset();
        peak = 0.0; gateDb = 0.0; holdLeft = 0; open = false; dsEnv = 0.0; gateWasOn = dsWasOn = false;
        gateGr = dsGr = 0.0f;
    }
    float meter (int i) const noexcept override
    {
        return i == 0 ? std::max (gateGr, dsGr) : (i == 1 ? gateGr : (i == 2 ? dsGr : 0.0f));
    }

    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace gate;
        const bool gateOn = p[gThresh] > gateOffDb + 0.05f && p[gRange] > 0.01f;
        const bool dsOn = p[dsAmount] > 0.01f || p[dsListen] > 0.5f;

        // Porte : constantes du bloc
        const auto thrOpen = (double) p[gThresh];
        const auto thrClose = thrOpen - (double) p[gHyst];
        const auto range = (double) p[gRange];
        const auto attC = std::exp (-1.0 / std::max (1.0, p[gAttack] * 0.001 * sr));
        const auto relC = std::exp (-1.0 / std::max (1.0, p[gRelease] * 0.001 * sr));
        const auto holdN = (int) (p[gHold] * 0.001 * sr);
        const auto pkRel = std::exp (-1.0 / (0.010 * sr)); // détecteur crête : relâchement 10 ms
        if (gateOn && ! gateWasOn) { scHpL.reset(); scHpR.reset(); peak = 0.0; }
        if (dsOn && ! dsWasOn) { splitL.reset(); splitR.reset(); dsL.reset(); dsR.reset(); dsEnv = 0.0; }
        gateWasOn = gateOn; dsWasOn = dsOn;
        if (gateOn)
        {
            scHpL.set (SvfEq::highpass, p[gHp], 0.707, 0.0, sr);
            scHpR.set (SvfEq::highpass, p[gHp], 0.707, 0.0, sr);
        }

        // Dé-esseur : séparation complémentaire et détection
        const auto dsF = (double) p[dsFreq];
        const auto dsT = (double) p[dsThresh];
        const auto dsMax = (double) p[dsAmount];
        const bool wide = p[dsMode] > 0.5f;
        const bool listen = p[dsListen] > 0.5f;
        const auto dsAtt = std::exp (-1.0 / (0.0005 * sr));
        const auto dsRel = std::exp (-1.0 / (0.060 * sr));
        if (dsOn)
        {
            splitL.set (SvfEq::lowpass, dsF, 0.707, 0.0, sr);
            splitR.set (SvfEq::lowpass, dsF, 0.707, 0.0, sr);
            // détection resserrée sur la bande des sifflantes (passe-bande centré au-dessus de la coupure)
            dsL.set (SvfEq::bandpass, dsF * 1.25, 0.9, 0.0, sr);
            dsR.set (SvfEq::bandpass, dsF * 1.25, 0.9, 0.0, sr);
        }

        double maxGateGr = 0.0, maxDsGr = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double l = L[i], r = R[i];

            if (dsOn)
            {
                const auto lowL = splitL.process (l), lowR = splitR.process (r);
                const auto hiL = l - lowL, hiR = r - lowR;
                const auto det = std::max (std::abs (dsL.process (l)), std::abs (dsR.process (r)));
                dsEnv = det > dsEnv ? det + (dsEnv - det) * dsAtt : det + (dsEnv - det) * dsRel;
                const auto over = gainToDb (dsEnv) - dsT;
                // genou doux de 6 dB, rapport 4:1, plafonné à la réduction maximale
                double red = 0.0;
                if (over > 3.0) red = (over) * 0.75;
                else if (over > -3.0) red = 0.75 * (over + 3.0) * (over + 3.0) / 12.0;
                red = std::min (red, dsMax);
                const auto g = dbToGain (-red);
                if (listen) { l = hiL; r = hiR; }
                else if (wide) { l *= g; r *= g; }
                else { l = lowL + hiL * g; r = lowR + hiR * g; }
                maxDsGr = std::max (maxDsGr, red);
            }

            if (gateOn)
            {
                const auto sc = std::max (std::abs (scHpL.process (l)), std::abs (scHpR.process (r)));
                peak = sc > peak ? sc : sc + (peak - sc) * pkRel;
                const auto lvl = gainToDb (peak);
                if (lvl >= thrOpen) { open = true; holdLeft = holdN; }
                else if (lvl < thrClose)
                {
                    if (holdLeft > 0) --holdLeft;
                    else open = false;
                }
                const auto target = open ? 0.0 : -range;
                gateDb = target > gateDb ? target + (gateDb - target) * attC : target + (gateDb - target) * relC;
                const auto g = dbToGain (gateDb);
                l *= g; r *= g;
                maxGateGr = std::max (maxGateGr, -gateDb);
            }

            L[i] = (float) l; R[i] = (float) r;
        }

        if (! gateOn) { gateDb = 0.0; open = false; holdLeft = 0; }
        gateGr = (float) maxGateGr;
        dsGr = (float) maxDsGr;
    }

private:
    double sr = 48000.0;
    SvfEq scHpL, scHpR, splitL, splitR, dsL, dsR;
    double peak = 0.0, gateDb = 0.0, dsEnv = 0.0;
    int holdLeft = 0;
    bool open = false, gateWasOn = false, dsWasOn = false;
    float gateGr = 0.0f, dsGr = 0.0f;
};

} // namespace grv::effects
