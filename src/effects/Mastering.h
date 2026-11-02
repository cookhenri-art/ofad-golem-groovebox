/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Mastering » (v0.31) : chaîne de mastering tout-en-un pour le bus principal, sur le principe
    des suites de mastering logicielles (égaliseur, dynamique multibande, image stéréo,
    excitation, maximiseur) — architecture propre, sans latence ajoutée.

      1. Égaliseur : coupe-bas (20/30/40 Hz), graves (étagère 90 Hz), médiums (cloche réglable),
         air (étagère 10 kHz).
      2. Dynamique multibande : trois bandes séparées par des filtres de Linkwitz-Riley d'ordre 4
         (somme plate en amplitude, phase d'un passe-tout ; aucune anticipation), un compresseur
         par bande (seuil, rapport, niveau), attaque et relâchement communs, genou doux, détection
         stéréo liée.
      3. Image stéréo : largeur par bande (milieu / côtés). Bande grave à 0 % = basses en mono.
      4. Excitation : « éclat » (harmoniques ajoutées à la bande aiguë, en parallèle) et
         « chaleur » (saturation douce de type bande magnétique sur tout le signal), ADAA.
      5. Maximiseur : gain, écrêtage doux des transitoires (ADAA) puis limiteur sans anticipation
         à deux relâchements — rapide après une crête isolée (avec maintien de 8 ms, sous la
         demi-période des graves : pas de distorsion de modulation), lent quand la réduction dure
         (le reste du mix ne « pompe » plus à chaque coup de grosse caisse) ; détection en crête
         vraie (×2) au choix, plafond garanti.

    Mesures : 0 réduction du maximiseur (dB), 1 crête d'entrée, 2 crête de sortie, 3 à 5
    réduction des bandes grave / médium / aigu (dB), 6 sonie à court terme approchée (LUFS),
    7 écrêtage (dB pris par l'écrêteur).
*/

#pragma once

#include "Dynamics.h"

namespace grv::effects
{

namespace master
{
    enum Slot
    {
        eqOn = 0, lowCut, lowGain, midFreq, midGain, airGain,
        dynOn, xLow, xHigh, thrLow, thrMid, thrHigh, ratLow, ratMid, ratHigh, gainLow, gainMid, gainHigh, attack, release,
        imgOn, widthLow, widthMid, widthHigh,
        excOn, shine, warmth,
        maxOn, drive, ceiling, character, clipAmt, truePeak, splitLow,
        numSlots
    };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "master"; m.name = "Varnish"; m.role = "Cha\xC3\xAEne de mastering"; m.family = "Mastering"; m.form = "module";
            m.principle = "Cha\xC3\xAEne de mastering tout-en-un : \xC3\xA9galiseur, dynamique 3 bandes, image st\xC3\xA9r\xC3\xA9o par bande, excitation, maximiseur \xC3\xA0 deux rel\xC3\xA2" "chements (sans pompage), cr\xC3\xAA" "te vraie, latence nulle";
            const std::vector<std::string> onOff { "Non", "Oui" };
            m.params = {
                C ("eqOn", "\xC3\x89galiseur", onOff, 1),
                C ("lowCut", "Coupe-bas", { "Non", "20 Hz", "30 Hz", "40 Hz" }, 2),
                P ("lowGain", "Graves", "dB", -6, 6, 0),
                P ("midFreq", "Fr\xC3\xA9q. m\xC3\xA9" "diums", "Hz", 200, 5000, 1000, 0.4f),
                P ("midGain", "M\xC3\xA9" "diums", "dB", -6, 6, 0),
                P ("airGain", "Air", "dB", -6, 6, 0),
                C ("dynOn", "Dynamique", onOff, 1),
                P ("xLow", "S\xC3\xA9paration grave", "Hz", 60, 300, 120, 0.5f),
                P ("xHigh", "S\xC3\xA9paration aigu", "Hz", 1000, 8000, 2500, 0.5f),
                P ("thrLow", "Seuil grave", "dB", -36, 0, -12),
                P ("thrMid", "Seuil m\xC3\xA9" "dium", "dB", -36, 0, -12),
                P ("thrHigh", "Seuil aigu", "dB", -36, 0, -12),
                P ("ratLow", "Rapport grave", ":1", 1, 8, 2, 0.5f),
                P ("ratMid", "Rapport m\xC3\xA9" "dium", ":1", 1, 8, 1.5f, 0.5f),
                P ("ratHigh", "Rapport aigu", ":1", 1, 8, 1.5f, 0.5f),
                P ("gainLow", "Niveau grave", "dB", -6, 6, 0),
                P ("gainMid", "Niveau m\xC3\xA9" "dium", "dB", -6, 6, 0),
                P ("gainHigh", "Niveau aigu", "dB", -6, 6, 0),
                P ("attack", "Attaque", "ms", 1, 100, 20, 0.5f),
                P ("release", "Rel\xC3\xA2" "chement", "ms", 20, 1000, 200, 0.4f),
                C ("imgOn", "Image", onOff, 1),
                P ("widthLow", "Largeur grave", "%", 0, 200, 0),
                P ("widthMid", "Largeur m\xC3\xA9" "dium", "%", 0, 200, 100),
                P ("widthHigh", "Largeur aigu", "%", 0, 200, 110),
                C ("excOn", "Excitation", onOff, 1),
                P ("shine", "\xC3\x89" "clat", "%", 0, 100, 15),
                P ("warmth", "Chaleur", "%", 0, 100, 15),
                C ("maxOn", "Maximiseur", onOff, 1),
                P ("drive", "Gain", "dB", 0, 24, 4),
                P ("ceiling", "Plafond", "dB", -3, 0, -1),
                P ("character", "Caract\xC3\xA8re", "%", 0, 100, 40),
                P ("clipAmt", "\xC3\x89" "cr\xC3\xAA" "tage", "%", 0, 100, 30),
                C ("truePeak", "Cr\xC3\xAA" "te vraie", onOff, 1),
                C ("splitLow", "Graves \xC3\xA0 part", onOff, 1),
            };
            m.sections = {
                { "eq", "\xC3\x89galiseur", "global", { "eqOn", "lowCut", "lowGain", "midFreq", "midGain", "airGain" }, "main" },
                { "dyn", "Dynamique 3 bandes", "global", { "dynOn", "xLow", "xHigh", "thrLow", "thrMid", "thrHigh", "ratLow", "ratMid", "ratHigh", "gainLow", "gainMid", "gainHigh", "attack", "release" }, "main" },
                { "img", "Image st\xC3\xA9r\xC3\xA9o", "global", { "imgOn", "widthLow", "widthMid", "widthHigh" }, "main" },
                { "exc", "Excitation", "global", { "excOn", "shine", "warmth" }, "main" },
                { "max", "Maximiseur", "master", { "maxOn", "drive", "ceiling", "character", "clipAmt", "truePeak", "splitLow" }, "main" },
            };
            m.presets = {
                { "Master CD (-9 LUFS)", { { "drive", 7 }, { "ceiling", -0.3f }, { "character", 40 }, { "clipAmt", 40 }, { "truePeak", 0 } }, "Mastering" },
                { "Streaming (-14 LUFS, -1 dBTP)", { { "drive", 3 }, { "ceiling", -1 }, { "character", 55 }, { "clipAmt", 20 }, { "truePeak", 1 },
                                                     { "ratLow", 1.6f }, { "ratMid", 1.3f }, { "ratHigh", 1.3f } }, "Mastering" },
                { "Club fort (-7 LUFS)", { { "drive", 10 }, { "ceiling", -0.3f }, { "character", 25 }, { "clipAmt", 60 }, { "truePeak", 0 },
                                           { "lowGain", 1 }, { "ratLow", 3 }, { "thrLow", -15 }, { "shine", 25 }, { "warmth", 25 } }, "Mastering" },
                { "Doux et ouvert", { { "drive", 3 }, { "ceiling", -1 }, { "character", 70 }, { "clipAmt", 10 }, { "truePeak", 1 },
                                      { "airGain", 1.5f }, { "ratLow", 1.5f }, { "ratMid", 1.2f }, { "ratHigh", 1.2f }, { "widthHigh", 125 }, { "warmth", 10 } }, "Mastering" },
                { "Basses serr\xC3\xA9" "es", { { "drive", 5 }, { "ceiling", -1 }, { "lowCut", 2 }, { "ratLow", 4 }, { "thrLow", -16 }, { "attack", 10 }, { "release", 120 },
                                                    { "widthLow", 0 }, { "truePeak", 1 } }, "Mastering" },
                { "Brillant", { { "drive", 5 }, { "ceiling", -1 }, { "airGain", 2.5f }, { "midGain", -1 }, { "midFreq", 400 }, { "shine", 35 }, { "widthHigh", 130 }, { "truePeak", 1 } }, "Mastering" },
                { "S\xC3\xA9" "curit\xC3\xA9 seulement", { { "eqOn", 0 }, { "dynOn", 0 }, { "imgOn", 0 }, { "excOn", 0 }, { "drive", 0 }, { "ceiling", -1 }, { "clipAmt", 0 }, { "truePeak", 1 } }, "Mastering" },
            };
            return m;
        }();
        return d;
    }
}

/** Filtre de Linkwitz-Riley d'ordre 4 (deux Butterworth d'ordre 2 en cascade). */
struct LR4
{
    SvfEq a, b;
    void set (SvfEq::Type t, double fc, double sr) noexcept { a.set (t, fc, 0.70710678, 0.0, sr); b.set (t, fc, 0.70710678, 0.0, sr); }
    double process (double x) noexcept { return b.process (a.process (x)); }
    void reset() noexcept { a.reset(); b.reset(); }
};

/** Conversions rapides (dynamique : la précision de 0,01 dB suffit). */
inline double fdb (double x) noexcept { return 6.0205999 * fastLog2 (x); }
inline double fgain (double db) noexcept { return fastExp2 (db * 0.16609640); }

class Mastering final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return master::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override
    {
        for (auto& c : ch)
        {
            c.hp.reset(); c.low.reset(); c.mid.reset(); c.air.reset();
            c.lp1.reset(); c.hp1.reset(); c.lp2.reset(); c.hp2.reset(); c.apLp.reset(); c.apHp.reset(); c.mxLp.reset(); c.mxHp.reset();
            c.os.reset(); c.clip.reset(); c.warm.reset(); c.shineSat.reset();
            c.kHs.reset(); c.kHp.reset();
        }
        for (auto& e : bandEnv) e = 0.0;
        for (auto& g : bandGr) g = 0.0;
        fastGr = slowGr = lowFast = lowSlow = 0.0; hold = lowHold = 0; clipDb = 0.0; grDb = 0.0; kPower = 0.0; inMeter = outMeter = 0.0f;
    }
    float meter (int i) const noexcept override
    {
        switch (i)
        {
            case 0: return (float) grDb;
            case 1: return inMeter;
            case 2: return outMeter;
            case 3: case 4: case 5: return (float) bandGr[(size_t) (i - 3)];
            case 6: return (float) (kPower > 1.0e-12 ? -0.691 + 10.0 * std::log10 (kPower) : -100.0);
            case 7: return (float) clipDb;
            default: return 0.0f;
        }
    }

    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace master;
        float pin = 0.0f;
        for (int i = 0; i < n; ++i) pin = std::max (pin, std::max (std::abs (L[i]), std::abs (R[i])));
        inMeter = std::max (pin, inMeter * 0.9f);

        const bool eq = p[eqOn] > 0.5f, dyn = p[dynOn] > 0.5f, img = p[imgOn] > 0.5f, exc = p[excOn] > 0.5f, mx = p[maxOn] > 0.5f;
        // réglages des filtres (recalculés seulement s'ils changent : SvfEq garde ses coefficients)
        const auto cut = (int) std::lround (p[lowCut]);
        for (auto& c : ch)
        {
            c.hp.set (SvfEq::highpass, cut == 1 ? 20.0 : cut == 2 ? 30.0 : 40.0, 0.70710678, 0.0, sr);
            c.low.set (SvfEq::lowShelf, 90.0, 0.7, p[lowGain], sr);
            c.mid.set (SvfEq::bell, p[midFreq], 0.8, p[midGain], sr);
            c.air.set (SvfEq::highShelf, 10000.0, 0.7, p[airGain], sr);
            const auto f1 = (double) p[xLow], f2 = std::max ((double) p[xHigh], f1 * 2.0);
            c.lp1.set (SvfEq::lowpass, f1, sr); c.hp1.set (SvfEq::highpass, f1, sr);
            c.lp2.set (SvfEq::lowpass, f2, sr); c.hp2.set (SvfEq::highpass, f2, sr);
            c.apLp.set (SvfEq::lowpass, f2, sr); c.apHp.set (SvfEq::highpass, f2, sr);
            c.kHs.set (SvfEq::highShelf, 1681.0, 0.71, 4.0, sr); // pondération K approchée (sonie affichée)
            c.kHp.set (SvfEq::highpass, 38.0, 0.5, 0.0, sr);
        }
        const auto att = msCoef (p[attack], sr), rel = msCoef (p[release], sr);
        const double thr[3] { p[thrLow], p[thrMid], p[thrHigh] }, rat[3] { p[ratLow], p[ratMid], p[ratHigh] };
        const double bandGain[3] { dbToGain (p[gainLow]), dbToGain (p[gainMid]), dbToGain (p[gainHigh]) };
        const double width[3] { p[widthLow] / 100.0, p[widthMid] / 100.0, p[widthHigh] / 100.0 };
        const auto shineAmt = p[shine] / 100.0, warmAmt = p[warmth] / 100.0;
        const auto inG = mx ? dbToGain (p[drive]) : 1.0;
        const auto tp = p[truePeak] > 0.5f;
        const auto ceil = dbToGain (p[ceiling]) * (tp ? 0.944 : 1.0); // marge de 0,5 dB pour les crêtes entre échantillons
        const auto clipLevel = ceil * 1.12;                             // l'écrêteur prend ce qui dépasse le plafond de ~1 dB
        const auto clipMix = p[clipAmt] / 100.0;
        const auto chr = p[character] / 100.0;
        const auto relFast = msCoef (10.0 + 140.0 * chr, sr), relSlow = msCoef (300.0 + 1200.0 * chr, sr);
        const auto attSlow = msCoef (60.0, sr);
        const int holdLen = (int) (0.008 * sr);
        const bool split = p[splitLow] > 0.5f;
        const auto lowCeil = ceil * 0.8;
        for (auto& c : ch) { c.mxLp.set (SvfEq::lowpass, p[xLow], sr); c.mxHp.set (SvfEq::highpass, p[xLow], sr); }
        const auto kCoef = msCoef (1500.0, sr); // ≈ fenêtre de 3 s
        double maxBand[3] {}, maxClip = 0.0;

        for (int i = 0; i < n; ++i)
        {
            double x[2] { L[i], R[i] };
            // 1. égaliseur
            if (eq)
                for (int c = 0; c < 2; ++c)
                {
                    auto v = x[c];
                    if (cut > 0) v = ch[c].hp.process (v);
                    v = ch[c].air.process (ch[c].mid.process (ch[c].low.process (v)));
                    x[c] = v;
                }
            // 2 à 4. bandes : séparation, compression, largeur, éclat
            if (dyn || img || (exc && shineAmt > 0.0))
            {
                double b[3][2];
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = ch[c];
                    const auto lo = s.lp1.process (x[c]);
                    const auto rest = s.hp1.process (x[c]);
                    // la bande grave passe par le passe-tout de la 2e séparation : somme plate
                    b[0][c] = s.apLp.process (lo) + s.apHp.process (lo);
                    b[1][c] = s.lp2.process (rest);
                    b[2][c] = s.hp2.process (rest);
                }
                for (int k = 0; k < 3; ++k)
                {
                    if (dyn)
                    {
                        const auto lvl = std::max (std::abs (b[k][0]), std::abs (b[k][1]));
                        bandEnv[k] = lvl > bandEnv[k] ? lvl + (bandEnv[k] - lvl) * att : lvl + (bandEnv[k] - lvl) * rel;
                        const auto gr = -compressDb (fdb (std::max (1.0e-9, bandEnv[k])), thr[k], rat[k], 6.0);
                        maxBand[k] = std::max (maxBand[k], gr);
                        const auto g = fgain (-gr) * bandGain[k];
                        b[k][0] *= g; b[k][1] *= g;
                    }
                    if (img && std::abs (width[k] - 1.0) > 1.0e-3)
                    {
                        const auto mid = 0.5 * (b[k][0] + b[k][1]), side = 0.5 * (b[k][0] - b[k][1]) * width[k];
                        b[k][0] = mid + side; b[k][1] = mid - side;
                    }
                }
                if (exc && shineAmt > 0.0)
                    for (int c = 0; c < 2; ++c)
                        b[2][c] += shineAmt * 0.6 * (ch[c].shineSat2 (b[2][c]) - b[2][c]);
                for (int c = 0; c < 2; ++c) x[c] = b[0][c] + b[1][c] + b[2][c];
            }
            // chaleur : saturation douce, crête conservée
            if (exc && warmAmt > 0.0)
                for (int c = 0; c < 2; ++c)
                {
                    const auto k = 1.0 + 3.0 * warmAmt;
                    x[c] = x[c] + (ch[c].warm.process (x[c] * k) / k - x[c]) * warmAmt;
                }
            // 5. maximiseur
            if (mx)
            {
                double y[2] { x[0] * inG, x[1] * inG };
                if (clipMix > 0.0)
                    for (int c = 0; c < 2; ++c)
                    {
                        const auto cl = clipLevel * ch[c].clip.process (y[c] / clipLevel);
                        const auto out = y[c] + (cl - y[c]) * clipMix;
                        if (std::abs (y[c]) > clipLevel * 0.5) maxClip = std::max (maxClip, fdb (std::abs (y[c]) / std::max (1.0e-9, std::abs (out))));
                        y[c] = out;
                    }
                // graves à part : la bande sous la séparation grave est limitée seule (sous 80 % du
                // plafond) — la grosse caisse ne fait plus baisser la nappe ni la voix
                if (split)
                {
                    double lo[2], hi[2];
                    for (int c = 0; c < 2; ++c) { lo[c] = ch[c].mxLp.process (y[c]); hi[c] = ch[c].mxHp.process (y[c]); }
                    const auto lpk = std::max (std::abs (lo[0]), std::abs (lo[1]));
                    const auto lneed = lpk > lowCeil ? fdb (lpk / lowCeil) : 0.0;
                    if (lneed >= lowFast) { lowFast = lneed; lowHold = holdLen * 2; }
                    else if (lowHold > 0) --lowHold;
                    else lowFast *= relFast;
                    lowSlow = lneed > lowSlow ? lneed + (lowSlow - lneed) * attSlow : lowSlow * relSlow;
                    const auto lg = fgain (-std::max (lowFast, lowSlow));
                    y[0] = lo[0] * lg + hi[0]; y[1] = lo[1] * lg + hi[1];
                }
                auto pk = std::max (std::abs (y[0]), std::abs (y[1]));
                if (tp)
                {
                    double a, bb, c2, d;
                    ch[0].os.up (y[0], a, bb); ch[1].os.up (y[1], c2, d);
                    pk = std::max ({ pk, std::abs (a), std::abs (bb), std::abs (c2), std::abs (d) });
                }
                const auto need = pk > ceil ? fdb (pk / ceil) : 0.0; // réduction nécessaire maintenant (dB)
                // étage rapide : attaque immédiate, maintien, relâchement court
                if (need >= fastGr) { fastGr = need; hold = holdLen; }
                else if (hold > 0) --hold;
                else fastGr *= relFast;
                // étage lent : suit la réduction qui dure, relâche lentement (pas de pompage)
                slowGr = need > slowGr ? need + (slowGr - need) * attSlow : slowGr * relSlow;
                const auto gr = std::max (fastGr, slowGr);
                const auto g = fgain (-gr);
                x[0] = std::clamp (y[0] * g, -ceil, ceil);
                x[1] = std::clamp (y[1] * g, -ceil, ceil);
                grDb = gr;
            }
            else grDb = 0.0;
            // sonie approchée (sortie)
            {
                const auto kl = ch[0].kHs.process (ch[0].kHp.process (x[0])), kr = ch[1].kHs.process (ch[1].kHp.process (x[1]));
                kPower = (kl * kl + kr * kr) + (kPower - (kl * kl + kr * kr)) * kCoef;
            }
            L[i] = (float) (std::isfinite (x[0]) ? x[0] : 0.0);
            R[i] = (float) (std::isfinite (x[1]) ? x[1] : 0.0);
        }
        for (int k = 0; k < 3; ++k) bandGr[(size_t) k] = dyn ? std::max (maxBand[k], bandGr[(size_t) k] * 0.8) : 0.0;
        clipDb = std::max (maxClip, clipDb * 0.8);
        float pout = 0.0f;
        for (int i = 0; i < n; ++i) pout = std::max (pout, std::max (std::abs (L[i]), std::abs (R[i])));
        outMeter = std::max (pout, outMeter * 0.9f);
    }

private:
    struct Channel
    {
        SvfEq hp, low, mid, air, kHs, kHp;
        LR4 lp1, hp1, lp2, hp2, apLp, apHp, mxLp, mxHp;
        dsp::Oversampler2x os;
        AdaaTanh clip, shineSat;
        AdaaSoftClip warm;
        double shPrev = 0.0;
        /** Harmoniques de la bande aiguë : redressement doux (paires) + saturation (impaires). */
        double shineSat2 (double v) noexcept
        {
            const auto odd = shineSat.process (v * 2.5) / 2.5;
            const auto even = 0.35 * (std::abs (v) - shPrev);
            shPrev = shPrev + (std::abs (v) - shPrev) * 0.02; // retire le continu du redressement
            return odd + even;
        }
    };
    double sr = 48000.0;
    std::array<Channel, 2> ch;
    std::array<double, 3> bandEnv {}, bandGr {};
    double fastGr = 0.0, slowGr = 0.0, lowFast = 0.0, lowSlow = 0.0, grDb = 0.0, clipDb = 0.0, kPower = 0.0;
    int hold = 0, lowHold = 0;
    float inMeter = 0.0f, outMeter = 0.0f;
};

} // namespace grv::effects
