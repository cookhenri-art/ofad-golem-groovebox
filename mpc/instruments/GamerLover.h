/*
    OFAD GOLEM — Gamer Lover — AGPL-3.0-or-later

    Instrument « musique de jeux vidéo » des années 85-2005, un seul instrument où l'on choisit la puce :

      PUCE (sélecteur)
        8 bits ........ principe des consoles 8 bits de salon : carré à 4 largeurs (12,5 / 25 / 50 / 75 %),
                        triangle en escalier sur 4 bits (32 marches), bruit à registre long ou court ;
                        volume sur 16 niveaux (4 bits)
        Portable ...... principe des consoles portables : carré à 4 largeurs, voie d'onde libre
                        (32 pas sur 4 bits, 8 ondes calculées), bruit 15 ou 7 bits ; volume sur 16 niveaux
        16 bits FM .... FM à 4 opérateurs, 8 algorithmes (principe de la puce FM des consoles 16 bits),
                        rétroaction de l'opérateur 1, « grain » du convertisseur 9 bits de l'époque
        16 bits samples principe de la console 16 bits à échantillons : échantillons compressés sur 4 bits
                        (blocs de 16, prédiction), lecture à interpolation douce (4 points), 8 sons calculés
        Batterie ...... 16 sons de console sur les 16 pads (notes 36 à 51) : grosses caisses en triangle ou FM,
                        caisses claires et charleys en bruit, toms, cloche, pièce, saut, explosion

      TABLES (comme dans les éditeurs de musique de jeu), rafraîchies 60 ou 50 fois par seconde
        arpège (faux accords très rapides), enveloppe de volume en marches, largeur qui bouge,
        chute ou montée de hauteur au départ de la note, vibrato retardé (en marches, à la cadence des images)

      SORTIE CONSOLE (la « couleur » de la machine, pour toutes les puces)
        résolution (bits), fréquence d'échantillonnage (sans filtre : le grain d'époque), filtre de sortie
        (ouvert, salon, portable, télé), écho à filtre 8 points (principe de l'écho des consoles 16 bits),
        largeur stéréo

    Aucun échantillon ni aucune ROM d'une console : tous les sons sont calculés.
    Temps réel : aucune allocation dans render() ; échantillons calculés dans prepare().
*/

#pragma once

#include "engine/Module.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace grv::instruments
{
namespace gamer
{
using engine::ParamDesc;
using engine::P;
using engine::C;
using engine::I;

enum Slot
{
    chip = 0, voices, tune, glide, level, tick,
    wave8, duty, dutyMove, gbWave, noiseMode,
    fmAlgo, fmFb, fmMul1, fmMul2, fmMul3, fmMul4, fmLev1, fmLev2, fmLev3, fmLev4, fmDec, fmGrain,
    smpl, smplSmooth,
    arp, arpSpeed,
    vAtk, vDec, vSus, vRel,
    sweep, sweepTime, vibDepth, vibRate, vibDelay,
    drTune, drDecay, drTone,
    bits, rate, tone, echoMix, echoTime, echoFb, echoFilter, width,
    numSlots
};

enum Chip { chip8 = 0, chipGb, chipFm, chipSmp, chipDrum };

inline const std::vector<std::string>& arpNames()
{
    static const std::vector<std::string> v { "Aucun", "Octave", "Quinte", "Majeur", "Mineur", "Majeur 7", "Mineur 7",
                                              "Sus 4", "Diminué", "Montée 4", "Trille" };
    return v;
}

/** Demi-tons de chaque arpège (0 en premier) et leur nombre. */
inline const int* arpTable (int a, int& n) noexcept
{
    static const int t[][5] = { { 0 }, { 0, 12 }, { 0, 7 }, { 0, 4, 7 }, { 0, 3, 7 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 },
                                { 0, 5, 7 }, { 0, 3, 6 }, { 0, 4, 7, 12 }, { 0, 2 } };
    static const int len[] = { 1, 2, 2, 3, 3, 4, 4, 3, 3, 4, 2 };
    a = std::clamp (a, 0, 10);
    n = len[a];
    return t[a];
}

inline const std::vector<std::string>& gbWaveNames()
{
    static const std::vector<std::string> v { "Sinus", "Scie", "Carre doux", "Orgue", "Voix", "Basse", "Cloche", "Metal" };
    return v;
}

inline const std::vector<std::string>& sampleNames()
{
    static const std::vector<std::string> v { "Cordes", "Choeur", "Piano élec.", "Cuivres", "Flute", "Basse", "Guitare", "Orgue" };
    return v;
}

inline const std::vector<std::string>& padNames()
{
    static const std::vector<std::string> v { "Grosse caisse 8 bits", "Grosse caisse FM", "Caisse claire 8 bits", "Caisse claire portable",
                                              "Clap", "Charley fermé", "Charley ouvert", "Cymbale", "Tom grave", "Tom médium",
                                              "Tom aigu", "Tom FM", "Cloche", "Pièce", "Saut", "Explosion" };
    return v;
}

inline engine::ModuleDesc makeDesc()
{
    engine::ModuleDesc m;
    m.type = "gamer";
    m.name = "Gamer Lover";
    m.family = "Polys";
    m.principle = "Puces des consoles de jeu 8 et 16 bits (carré, triangle, bruit, onde 4 bits, FM 4 opérateurs, "
                  "échantillons compressés), tables à 60 Hz, batterie de console et sortie d'époque";
    auto& p = m.params;
    p.resize (numSlots);
    p[chip] = C ("chip", "Puce", { "8 bits", "Portable", "16 bits FM", "16 bits ech.", "Batterie" }, 0);
    p[voices] = I ("voices", "Voix", "", 1, 8, 4);
    p[tune] = I ("tune", "Accord", "dt", -24, 24, 0);
    p[glide] = P ("glide", "Glisse", "ms", 0, 1000, 0, 0.5f);
    p[level] = P ("level", "Niveau", "dB", -24, 6, -6);
    p[tick] = C ("tick", "Cadence", { "60 Hz", "50 Hz" }, 0);
    p[wave8] = C ("wave8", "Onde", { "Carré", "Triangle-onde", "Bruit" }, 0);
    p[duty] = C ("duty", "Largeur", { "12,5 %", "25 %", "50 %", "75 %" }, 2);
    p[dutyMove] = C ("dutymove", "Largeur animee", { "Fixe", "Montée", "Alternance", "Balayage" }, 0);
    p[gbWave] = C ("gbwave", "Onde 4 bits", gbWaveNames(), 0);
    p[noiseMode] = C ("noisemode", "Bruit", { "Long", "Court" }, 0);
    p[fmAlgo] = I ("fmalgo", "Algorithme", "", 1, 8, 5);
    p[fmFb] = I ("fmfb", "Retroaction", "", 0, 7, 3);
    for (int k = 0; k < 4; ++k)
    {
        p[fmMul1 + k] = C ("fmmul" + std::to_string (k + 1), "Op. " + std::to_string (k + 1) + " rapport",
                           { "0,5", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15" }, k == 0 ? 2 : 1);
        p[fmLev1 + k] = P ("fmlev" + std::to_string (k + 1), "Op. " + std::to_string (k + 1) + " niveau", "%", 0, 100,
                           k == 3 ? 100.0f : (k == 0 ? 55.0f : 70.0f));
    }
    p[fmDec] = P ("fmdec", "Declin modulateurs", "ms", 10, 3000, 300, 0.4f);
    p[fmGrain] = P ("fmgrain", "Grain DAC", "%", 0, 100, 50);
    p[smpl] = C ("smpl", "Echantillon", sampleNames(), 0);
    p[smplSmooth] = C ("smplsmooth", "Interpolation", { "Douce", "Dure" }, 0);
    p[arp] = C ("arp", "Arpege", arpNames(), 0);
    p[arpSpeed] = I ("arpspeed", "Vitesse arpege", "img", 1, 8, 2);
    p[vAtk] = P ("vatk", "Attaque", "ms", 0, 2000, 0, 0.4f);
    p[vDec] = P ("vdec", "Declin", "ms", 0, 3000, 250, 0.4f);
    p[vSus] = I ("vsus", "Maintien", "", 0, 15, 10);
    p[vRel] = P ("vrel", "Relachement", "ms", 0, 3000, 120, 0.4f);
    p[sweep] = I ("sweep", "Chute hauteur", "dt", -36, 36, 0);
    p[sweepTime] = P ("sweeptime", "Duree de la chute", "ms", 10, 1000, 120, 0.5f);
    p[vibDepth] = P ("vibdepth", "Vibrato", "ct", 0, 100, 0);
    p[vibRate] = P ("vibrate", "Vibrato vitesse", "Hz", 1, 12, 6);
    p[vibDelay] = P ("vibdelay", "Vibrato retard", "ms", 0, 1500, 300, 0.5f);
    p[drTune] = I ("drtune", "Batterie accord", "dt", -12, 12, 0);
    p[drDecay] = P ("drdecay", "Batterie declin", "%", 25, 200, 100);
    p[drTone] = P ("drtone", "Batterie timbre", "%", 0, 100, 50);
    p[bits] = I ("bits", "Resolution", "bits", 4, 16, 16);
    p[rate] = C ("rate", "Frequence", { "Pleine", "32 kHz", "22 kHz", "16 kHz", "11 kHz" }, 0);
    p[tone] = C ("tone", "Filtre sortie", { "Ouvert", "Salon", "Portable", "Télé" }, 0);
    p[echoMix] = P ("echomix", "Echo", "%", 0, 100, 0);
    p[echoTime] = I ("echotime", "Echo temps", "ms", 16, 240, 160);
    p[echoFb] = P ("echofb", "Echo reinjection", "%", 0, 95, 40);
    p[echoFilter] = C ("echofilter", "Echo filtre", { "Clair", "Doux", "Sombre", "Grotte" }, 1);
    p[width] = P ("width", "Largeur stereo", "%", 0, 100, 30);
    // libellés accentués (affichage GOLEM ; le greffon MPC les replie en ASCII)
    p[fmFb].name = "Rétroaction"; p[fmDec].name = "Déclin modul."; p[smpl].name = "Échantillon";
    p[arp].name = "Arpège"; p[arpSpeed].name = "Vitesse arpège"; p[vDec].name = "Déclin"; p[vRel].name = "Relâchement";
    p[drDecay].name = "Batterie déclin"; p[bits].name = "Résolution"; p[rate].name = "Fréquence";
    p[echoMix].name = "Écho"; p[echoTime].name = "Écho temps"; p[echoFb].name = "Écho retour"; p[echoFilter].name = "Écho filtre";
    p[sweepTime].name = "Durée chute"; p[width].name = "Stéréo";

    m.sections = {
        { "chip", "Puce", "global", { "chip", "voices", "tune", "glide", "tick", "level" }, "global" },
        { "pulse", "Ondes 8 bits et portable", "osc", { "wave8", "duty", "dutymove", "gbwave", "noisemode" }, "osc" },
        { "fm", "FM 16 bits", "osc", { "fmalgo", "fmfb", "fmmul1", "fmmul2", "fmmul3", "fmmul4", "fmlev1", "fmlev2", "fmlev3", "fmlev4", "fmdec", "fmgrain" }, "osc" },
        { "smp", "Échantillons 16 bits", "osc", { "smpl", "smplsmooth" }, "osc" },
        { "arp", "Tables · arpège", "mod", { "arp", "arpspeed" }, "mod" },
        { "venv", "Tables · volume", "env", { "vatk", "vdec", "vsus", "vrel" }, "amp" },
        { "pitch", "Tables · hauteur", "mod", { "sweep", "sweeptime", "vibdepth", "vibrate", "vibdelay" }, "mod" },
        { "drums", "Batterie", "global", { "drtune", "drdecay", "drtone" }, "global" },
        { "console", "Sortie console", "fx", { "bits", "rate", "tone", "width" }, "fx" },
        { "echo", "Écho", "fx", { "echomix", "echotime", "echofb", "echofilter" }, "fx" },
    };
    m.padNames = padNames();

    // sons d'usine (catégories) : seuls les réglages qui changent
    using V = std::vector<std::pair<std::string, float>>;
    auto add = [&] (const char* name, const char* cat, V v) { m.presets.push_back ({ name, std::move (v), cat }); };
    add ("Basse 8 bits", "Basses", { { "chip", 0 }, { "wave8", 1 }, { "voices", 1 }, { "vdec", 400 }, { "vsus", 12 }, { "tune", -12 } });
    add ("Basse carree 25", "Basses", { { "level", -3 }, { "chip", 0 }, { "duty", 1 }, { "voices", 1 }, { "vdec", 180 }, { "vsus", 7 }, { "tune", -12 }, { "rate", 1 } });
    add ("Basse FM dure", "Basses", { { "level", 1 }, { "chip", 2 }, { "voices", 1 }, { "fmalgo", 5 }, { "fmlev1", 80 }, { "fmlev2", 0 }, { "fmlev3", 60 }, { "fmmul1", 1 }, { "fmmul3", 2 }, { "fmdec", 180 }, { "fmgrain", 80 }, { "tune", -12 } });
    add ("Basse slap 16 bits", "Basses", { { "level", 0 }, { "chip", 3 }, { "smpl", 5 }, { "voices", 2 }, { "vsus", 11 }, { "tune", -12 }, { "rate", 1 }, { "tone", 1 } });
    add ("Basse portable", "Basses", { { "level", 2 }, { "chip", 1 }, { "wave8", 1 }, { "gbwave", 5 }, { "voices", 1 }, { "tune", -12 }, { "tone", 2 } });
    add ("Lead heros", "Leads", { { "chip", 0 }, { "duty", 2 }, { "voices", 2 }, { "vibdepth", 30 }, { "vibdelay", 250 }, { "echomix", 18 }, { "echotime", 128 } });
    add ("Lead fin 12,5", "Leads", { { "chip", 0 }, { "duty", 0 }, { "dutymove", 1 }, { "voices", 2 }, { "vibdepth", 20 } });
    add ("Lead portable", "Leads", { { "level", -5 }, { "chip", 1 }, { "duty", 1 }, { "voices", 2 }, { "vibdepth", 25 }, { "tone", 2 }, { "glide", 40 } });
    add ("Lead onde voix", "Leads", { { "level", -1 }, { "chip", 1 }, { "wave8", 1 }, { "gbwave", 4 }, { "voices", 2 }, { "vibdepth", 35 }, { "tone", 2 } });
    add ("Lead FM cuivre", "Leads", { { "level", -1 }, { "chip", 2 }, { "fmalgo", 5 }, { "fmlev1", 65 }, { "fmlev3", 45 }, { "fmmul1", 1 }, { "fmfb", 5 }, { "vibdepth", 25 }, { "echomix", 15 } });
    add ("Lead balayage", "Leads", { { "chip", 0 }, { "dutymove", 3 }, { "voices", 1 }, { "glide", 60 }, { "vibdepth", 15 } });
    add ("Arpege majeur", "Arpèges", { { "level", -5 }, { "chip", 0 }, { "duty", 1 }, { "arp", 3 }, { "arpspeed", 1 }, { "voices", 1 }, { "vsus", 9 } });
    add ("Arpege mineur", "Arpèges", { { "chip", 0 }, { "duty", 2 }, { "arp", 4 }, { "arpspeed", 1 }, { "voices", 1 }, { "vsus", 9 } });
    add ("Arpege 7e portable", "Arpèges", { { "level", -5 }, { "chip", 1 }, { "duty", 1 }, { "arp", 6 }, { "arpspeed", 2 }, { "voices", 1 }, { "tone", 2 } });
    add ("Arpege FM", "Arpèges", { { "level", -4 }, { "chip", 2 }, { "arp", 5 }, { "arpspeed", 2 }, { "voices", 1 }, { "fmalgo", 7 }, { "echomix", 25 } });
    add ("Trille", "Arpèges", { { "level", -7 }, { "chip", 0 }, { "arp", 10 }, { "arpspeed", 2 }, { "voices", 1 } });
    add ("Cordes de cartouche", "Nappes", { { "level", 0 }, { "chip", 3 }, { "smpl", 0 }, { "voices", 8 }, { "vatk", 250 }, { "vrel", 700 }, { "echomix", 35 }, { "echotime", 192 }, { "echofb", 55 }, { "rate", 1 } });
    add ("Choeur de donjon", "Nappes", { { "level", 0 }, { "chip", 3 }, { "smpl", 1 }, { "voices", 8 }, { "vatk", 400 }, { "vrel", 900 }, { "echomix", 45 }, { "echotime", 240 }, { "echofb", 60 }, { "echofilter", 3 }, { "rate", 1 } });
    add ("Orgue du chateau", "Nappes", { { "level", -9 }, { "chip", 3 }, { "smpl", 7 }, { "voices", 8 }, { "vrel", 400 }, { "echomix", 30 }, { "echofilter", 2 } });
    add ("Nappe FM douce", "Nappes", { { "level", -9 }, { "chip", 2 }, { "fmalgo", 8 }, { "voices", 6 }, { "vatk", 300 }, { "vrel", 800 }, { "fmlev1", 40 }, { "echomix", 30 } });
    add ("Piano electrique 16", "Nappes", { { "level", -8 }, { "chip", 3 }, { "smpl", 2 }, { "voices", 8 }, { "vsus", 6 }, { "vdec", 1200 }, { "echomix", 20 } });
    add ("Flute de foret", "Leads", { { "level", -7 }, { "chip", 3 }, { "smpl", 4 }, { "voices", 4 }, { "vibdepth", 20 }, { "vibdelay", 400 }, { "echomix", 30 } });
    add ("Guitare de taverne", "Leads", { { "level", 2 }, { "chip", 3 }, { "smpl", 6 }, { "voices", 6 }, { "vsus", 0 }, { "vdec", 900 }, { "echomix", 15 } });
    add ("Cloche FM", "Nappes", { { "level", -1 }, { "chip", 2 }, { "fmalgo", 5 }, { "fmmul1", 5 }, { "fmlev1", 60 }, { "fmdec", 900 }, { "vsus", 0 }, { "vdec", 1500 }, { "echomix", 25 } });
    add ("Batterie 8 bits", "Batteries", { { "chip", 4 }, { "rate", 1 } });
    add ("Batterie 16 bits", "Batteries", { { "chip", 4 }, { "drtone", 70 }, { "echomix", 12 }, { "echotime", 96 } });
    add ("Batterie portable", "Batteries", { { "chip", 4 }, { "tone", 2 }, { "bits", 8 }, { "drdecay", 80 } });
    add ("Laser", "Effets", { { "level", 1 }, { "chip", 0 }, { "duty", 1 }, { "sweep", 24 }, { "sweeptime", 180 }, { "vsus", 0 }, { "vdec", 300 } });
    add ("Chute", "Effets", { { "level", -4 }, { "chip", 0 }, { "sweep", -24 }, { "sweeptime", 600 }, { "vsus", 0 }, { "vdec", 700 } });
    add ("Vent", "Effets", { { "level", -7 }, { "chip", 0 }, { "wave8", 2 }, { "voices", 2 }, { "vatk", 500 }, { "vrel", 900 }, { "tone", 3 } });
    add ("Menu", "Effets", { { "level", 1 }, { "chip", 1 }, { "duty", 1 }, { "arp", 9 }, { "arpspeed", 1 }, { "vsus", 0 }, { "vdec", 250 } });
    return m;
}

inline const engine::ModuleDesc& desc()
{
    static const engine::ModuleDesc d = makeDesc();
    return d;
}

constexpr int maxVoices = 8;
constexpr double sampleBaseHz = 250.0;   // échantillons calculés : période de 128 pas à 32 kHz
constexpr double sampleRateSmp = 32000.0;

inline float fastSin (double ph) noexcept;

/** Échantillon « 16 bits » : déjà décodé depuis sa forme compressée 4 bits (grain compris). */
struct Smp { std::vector<float> d; int loopStart = 0; };

/** Compression 4 bits par blocs de 16 (décalage + filtre de prédiction choisis au mieux), puis décodage :
    le défaut caractéristique des échantillons des consoles 16 bits. Hors du fil audio. */
inline void crunch4 (std::vector<float>& x)
{
    static const double fa[4] = { 0.0, 0.9375, 1.90625, 1.796875 };
    static const double fb[4] = { 0.0, 0.0, -0.9375, -0.8125 };
    double p1 = 0.0, p2 = 0.0;
    for (size_t b = 0; b + 16 <= x.size(); b += 16)
    {
        double bestErr = 1e30, bp1 = 0, bp2 = 0;
        float best[16] {};
        for (int f = 0; f < 4; ++f)
            for (int sh = 0; sh <= 12; ++sh)
            {
                const double step = std::ldexp (1.0, sh) / 32768.0;
                double q1 = p1, q2 = p2, err = 0;
                float out[16];
                for (int i = 0; i < 16; ++i)
                {
                    const double pred = fa[f] * q1 + fb[f] * q2;
                    int n = (int) std::lround ((x[b + (size_t) i] - pred) / step);
                    n = std::clamp (n, -8, 7);
                    double y = std::clamp (pred + n * step, -1.0, 1.0);
                    out[i] = (float) y;
                    err += (y - x[b + (size_t) i]) * (y - x[b + (size_t) i]);
                    q2 = q1; q1 = y;
                }
                if (err < bestErr) { bestErr = err; std::copy (out, out + 16, best); bp1 = q1; bp2 = q2; }
            }
        std::copy (best, best + 16, x.begin() + (long) b);
        p1 = bp1; p2 = bp2;
    }
}

/** 8 sons calculés (32 kHz, base 250 Hz), chacun avec sa boucle (nombre entier de périodes). */
inline std::vector<Smp> buildSamples()
{
    std::vector<Smp> out (8);
    const double sr = sampleRateSmp, f0 = sampleBaseHz, tw = 2.0 * M_PI;
    const int attack = 8192, loopLen = 4096, len = attack + loopLen; // boucle : 32 périodes, partiels sur 7,8125 Hz
    const double df = sr / loopLen;
    auto partials = [&] (Smp& s, auto amp, auto env, double detune, int copies, double noise)
    {
        s.d.assign ((size_t) len, 0.0f);
        std::uint32_t r = 12345;
        for (int c = 0; c < copies; ++c)
        {
            const double dt = (c - (copies - 1) * 0.5) * detune;   // désaccord en multiples de df (boucle parfaite)
            for (int k = 1; k <= 40; ++k)
            {
                const double a = amp (k);
                if (a < 1e-4 || k * f0 > 12000.0) continue;
                const double f = k * f0 + dt * df;
                const double ph = (k * 0.37 + c * 1.3);
                for (int n = 0; n < len; ++n)
                    s.d[(size_t) n] += (float) (a * env (k, n) * fastSin (f * n / sr + ph / tw));
            }
        }
        if (noise > 0)
            for (int n = 0; n < attack; ++n)
            {
                r = r * 1664525u + 1013904223u;
                s.d[(size_t) n] += (float) (noise * ((r >> 9) / 4194304.0 - 1.0) * std::exp (-n / 1500.0));
            }
        float mx = 1e-6f;
        for (auto v : s.d) mx = std::max (mx, std::fabs (v));
        for (auto& v : s.d) v *= 0.85f / mx;
        s.loopStart = attack;
    };
    auto rise = [&] (int n, double ms) { return std::min (1.0, n / (ms * sr / 1000.0)); };
    // 0 cordes : scie douce, 3 copies désaccordées, attaque lente
    partials (out[0], [] (int k) { return 1.0 / k * (k > 18 ? 0.0 : 1.0); }, [&] (int, int n) { return rise (n, 120); }, 1.0, 3, 0.0);
    // 1 chœur : partiels mis en forme par trois formants « a »
    partials (out[1], [&] (int k) {
        const double f = k * f0;
        auto g = [&] (double c, double w) { return std::exp (-(f - c) * (f - c) / (2 * w * w)); };
        return 0.15 / k + g (700, 130) + 0.6 * g (1220, 150) + 0.3 * g (2600, 220); }, [&] (int, int n) { return rise (n, 180); }, 1.0, 3, 0.0);
    // 2 piano électrique : partiels 1, 2, 3 et 4,2 (cloche) qui retombent
    partials (out[2], [] (int k) { return k == 1 ? 1.0 : (k == 2 ? 0.35 : (k == 3 ? 0.12 : (k == 4 ? 0.08 : 0.0))); },
              [&] (int k, int n) { return std::max (k == 1 ? 0.45 : 0.08, std::exp (-n / (sr * (k == 1 ? 0.6 : 0.12)))) * rise (n, 2); }, 0.0, 1, 0.02);
    // 3 cuivres : scie qui s'ouvre
    partials (out[3], [] (int k) { return 1.0 / k; }, [&] (int k, int n) { return rise (n, 30) * std::min (1.0, 0.4 + (n / (sr * 0.08)) / k); }, 0.0, 1, 0.0);
    // 4 flûte : sinus, un peu d'harmonique 2 et 3, souffle
    partials (out[4], [] (int k) { return k == 1 ? 1.0 : (k == 2 ? 0.12 : (k == 3 ? 0.05 : 0.0)); }, [&] (int, int n) { return rise (n, 60); }, 0.0, 1, 0.12);
    // 5 basse : partiels 1-8, aigus qui retombent (claquant)
    partials (out[5], [] (int k) { return k <= 8 ? 1.0 / k : 0.0; },
              [&] (int k, int n) { return std::max (0.25, std::exp (-n * k / (sr * 0.5))) * rise (n, 1); }, 0.0, 1, 0.05);
    // 6 guitare : corde pincée (principe de la corde à retard), boucle sur la fin
    {
        auto& s = out[6];
        s.d.assign ((size_t) len, 0.0f);
        const int per = 128;
        std::vector<double> buf (per);
        std::uint32_t r = 777;
        for (auto& v : buf) { r = r * 1664525u + 1013904223u; v = (r >> 9) / 4194304.0 - 1.0; }
        double prev = 0;
        for (int n = 0; n < len; ++n)
        {
            const int i = n % per;
            const double y = 0.5 * (buf[(size_t) i] + prev) * (n < attack ? 0.9985 : 1.0);
            prev = buf[(size_t) i];
            buf[(size_t) i] = y;
            s.d[(size_t) n] = (float) y;
        }
        float mx = 1e-6f;
        for (auto v : s.d) mx = std::max (mx, std::fabs (v));
        for (auto& v : s.d) v *= 0.85f / mx;
        s.loopStart = attack;
    }
    // 7 orgue : tirettes 1, 2, 3, 4, 6, 8
    partials (out[7], [] (int k) { return (k == 1 || k == 2) ? 1.0 : ((k == 3 || k == 4) ? 0.6 : ((k == 6 || k == 8) ? 0.35 : 0.0)); },
              [&] (int, int n) { return rise (n, 8); }, 0.0, 1, 0.0);
    for (auto& s : out)
    {
        crunch4 (s.d);
        // la compression 4 bits laisse un petit saut au point de boucle (clic à chaque tour) : la fin de la
        // boucle est fondue vers ce qui précède son début, la reprise est continue
        const int len = (int) s.d.size(), n = 96;
        if (s.loopStart > n && len - s.loopStart > n)
            for (int i = 0; i < n; ++i)
            {
                const float w = (float) (i + 1) / (float) n;
                auto& v = s.d[(size_t) (len - n + i)];
                v = v * (1.0f - w) + s.d[(size_t) (s.loopStart - n + i)] * w;
            }
    }
    return out;
}

/** Onde 4 bits de la voie libre (32 pas, valeurs 0..15). */
inline float gbWaveValue (int w, int step) noexcept
{
    const double x = (step + 0.5) / 32.0, tw = 2.0 * M_PI;
    double v = 0;
    switch (w)
    {
        case 0: v = std::sin (tw * x); break;
        case 1: v = 2.0 * x - 1.0; break;
        case 2: v = std::tanh (3.0 * std::sin (tw * x)); break;
        case 3: v = (std::sin (tw * x) + 0.6 * std::sin (2 * tw * x) + 0.4 * std::sin (3 * tw * x)) / 1.6; break;
        case 4: v = (std::sin (tw * x) + 0.8 * std::sin (3 * tw * x) * std::cos (tw * x)) / 1.4; break;
        case 5: v = x < 0.5 ? std::sin (tw * x) : 0.3 * std::sin (2 * tw * x); break;
        case 6: v = (std::sin (tw * x) + 0.7 * std::sin (5 * tw * x)) / 1.5; break;
        default: v = (std::sin (tw * x) + std::sin (7 * tw * x + 1.0) * 0.8) / 1.6; break;
    }
    const int q = std::clamp ((int) std::lround ((v + 1.0) * 7.5), 0, 15);
    return (float) (q / 7.5 - 1.0);
}

inline double midiHz (double n) noexcept { return 440.0 * std::pow (2.0, (n - 69.0) / 12.0); }

inline float polyBlep (double t, double dt) noexcept
{
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

/** Sinus par table (FM) : 2048 points, interpolation linéaire. */
inline float fastSin (double ph) noexcept
{
    static const std::array<float, 2049> t = []
    {
        std::array<float, 2049> a {};
        for (int i = 0; i <= 2048; ++i) a[(size_t) i] = (float) std::sin (2.0 * M_PI * i / 2048.0);
        return a;
    }();
    ph -= std::floor (ph);
    const double x = ph * 2048.0;
    const int i = (int) x;
    const float f = (float) (x - i);
    return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
}

/** Recette d'un son de batterie de console. */
struct DrumDef
{
    int osc;            // 0 aucun, 1 triangle 4 bits, 2 carré 50 %, 3 FM
    double f0, f1;      // hauteur de départ et d'arrivée (Hz)
    double sweepMs;     // durée du glissement
    double toneMs;      // déclin de la partie tonale
    int noise;          // 0 aucun, 1 long, 2 court
    double nRate0, nRate1; // cadence du bruit (Hz) au départ et à la fin
    double noiseMs;     // déclin du bruit
    double noiseLvl, toneLvl;
    int fx;             // 0 rien, 1 pièce (deux notes), 2 claquements (clap)
    double fmRatio, fmIndex;
};

inline const DrumDef& drumDef (int pad) noexcept
{
    static const DrumDef d[16] = {
        { 1, 170, 48, 70, 200, 1, 9000, 9000, 6, 0.25, 1.3, 0, 0, 0 },        // grosse caisse 8 bits
        { 3, 140, 42, 60, 260, 0, 0, 0, 0, 0, 1.3, 0, 1.0, 2.5 },             // grosse caisse FM
        { 1, 230, 160, 30, 70, 1, 7000, 6000, 160, 0.9, 0.6, 0, 0, 0 },       // caisse claire 8 bits
        { 2, 260, 180, 25, 60, 1, 9000, 7000, 140, 0.85, 0.35, 0, 0, 0 },     // caisse claire portable
        { 0, 0, 0, 0, 0, 1, 8000, 7000, 130, 1.0, 0, 2, 0, 0 },               // clap
        { 0, 0, 0, 0, 0, 2, 14000, 14000, 45, 1.0, 0, 0, 0, 0 },              // charley fermé
        { 0, 0, 0, 0, 0, 1, 14000, 13000, 260, 0.8, 0, 0, 0, 0 },             // charley ouvert
        { 0, 0, 0, 0, 0, 1, 10000, 8000, 800, 0.75, 0, 0, 0, 0 },             // cymbale
        { 1, 150, 90, 120, 240, 0, 0, 0, 0, 0, 1.0, 0, 0, 0 },                // tom grave
        { 1, 210, 135, 110, 220, 0, 0, 0, 0, 0, 1.0, 0, 0, 0 },               // tom médium
        { 1, 290, 190, 100, 200, 0, 0, 0, 0, 0, 1.0, 0, 0, 0 },               // tom aigu
        { 3, 190, 105, 160, 280, 0, 0, 0, 0, 0, 1.0, 0, 1.5, 1.8 },           // tom FM
        { 3, 540, 540, 1, 450, 0, 0, 0, 0, 0, 0.8, 0, 1.41, 2.2 },            // cloche
        { 2, 988, 988, 1, 420, 0, 0, 0, 0, 0, 0.55, 1, 0, 0 },                // pièce
        { 2, 220, 880, 160, 230, 0, 0, 0, 0, 0, 0.5, 0, 0, 0 },               // saut
        { 0, 0, 0, 0, 0, 1, 4000, 250, 900, 1.0, 0, 0, 0, 0 },                // explosion
    };
    return d[std::clamp (pad, 0, 15)];
}
} // namespace gamer

//==============================================================================
class GamerLover : public engine::Instrument
{
public:
    using S = gamer::Slot;

    const engine::ModuleDesc& desc() const noexcept override { return gamer::desc(); }

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 0 ? sampleRate : 44100.0;
        if (samples.empty()) samples = gamer::buildSamples();
        echoL.assign ((size_t) (sr * 0.26) + 8, 0.0f);
        echoR.assign (echoL.size(), 0.0f);
        reset();
    }

    void reset() noexcept override
    {
        for (auto& v : vs) v = Voice();
        for (auto& d : drums) d = Drum();
        std::fill (echoL.begin(), echoL.end(), 0.0f);
        std::fill (echoR.begin(), echoR.end(), 0.0f);
        echoPos = 0;
        firL.fill (0.0f); firR.fill (0.0f);
        holdL = holdR = 0.0f; holdPh = 1.0;
        lp1L = lp1R = lp2L = lp2R = hpL = hpR = hpxL = hpxR = 0.0f;
        lastNote = -1;
    }

    void setExpression (float b, float, float) noexcept override { bend = b; }

    void noteOn (int note, float vel) noexcept override
    {
        pendingOn[(size_t) (numPending & 63)] = { note, vel, true };
        ++numPending;
    }
    void noteOff (int note) noexcept override
    {
        pendingOn[(size_t) (numPending & 63)] = { note, 0.0f, false };
        ++numPending;
    }
    void allNotesOff() noexcept override { for (auto& v : vs) if (v.active) v.release(); }

    void render (float* left, float* right, int n, const float* p) noexcept override
    {
        const int chipNow = std::clamp ((int) p[S::chip], 0, 4);
        applyPending (p, chipNow);
        const double tickHz = (int) p[S::tick] == 1 ? 50.0 : 60.0;
        const double spt = sr / tickHz;                 // échantillons par image
        const int nv = std::clamp ((int) p[S::voices], 1, gamer::maxVoices);
        const float wid = p[S::width] / 100.0f;
        // sortie console
        // « Pleine » : aucune réduction (avant : 44,1 kHz, qui répétait un échantillon sur douze à 48 kHz)
        static const double rates[] = { 0.0, 32000.0, 22050.0, 16000.0, 11025.0 };
        const int rateSel = std::clamp ((int) p[S::rate], 0, 4);
        const double holdStep = rateSel == 0 ? 1.0 : std::min (1.0, rates[rateSel] / sr);
        const int nb = std::clamp ((int) p[S::bits], 4, 16);
        const float q = nb >= 16 ? 0.0f : (float) std::ldexp (1.0, nb - 1);
        static const double cut[] = { 20000.0, 11000.0, 6500.0, 3800.0 };
        const int toneSel = std::clamp ((int) p[S::tone], 0, 3);
        const float lpA = (float) (1.0 - std::exp (-2.0 * M_PI * std::min (cut[toneSel], sr * 0.45) / sr));
        const float hpA = (float) std::exp (-2.0 * M_PI * (toneSel == 2 ? 140.0 : 20.0) / sr);
        const float emix = p[S::echoMix] / 100.0f, efb = p[S::echoFb] / 100.0f;
        const int edl = std::clamp ((int) (std::lround (p[S::echoTime] / 16.0) * 16 * sr / 1000.0), 1, (int) echoL.size() - 1);
        static const float firs[4][8] = { { 0.7f, 0.2f, 0.1f, 0, 0, 0, 0, 0 },
                                          { 0.35f, 0.25f, 0.18f, 0.1f, 0.06f, 0.04f, 0.02f, 0 },
                                          { 0.2f, 0.18f, 0.16f, 0.14f, 0.12f, 0.1f, 0.06f, 0.04f },
                                          { 0.5f, -0.1f, 0.3f, -0.05f, 0.2f, 0.05f, 0.06f, 0.04f } };
        const float* fir = firs[std::clamp ((int) p[S::echoFilter], 0, 3)];
        const float gain = (float) std::pow (10.0, p[S::level] / 20.0);

        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;
            if (chipNow == gamer::chipDrum)
            {
                for (auto& d : drums)
                    if (d.active)
                    {
                        const float s = d.tick (p, sr);
                        l += s * (1.0f - d.pan * wid);
                        r += s * (1.0f + d.pan * wid);
                    }
            }
            else
            {
                for (int k = 0; k < gamer::maxVoices; ++k)
                {
                    auto& v = vs[(size_t) k];
                    if (! v.active) continue;
                    if (--v.tickLeft <= 0.0) { v.tickLeft += spt; v.frame (p, chipNow, tickHz, bend); }
                    const float s = v.sample (p, chipNow, sr, samples) * 0.5f;
                    const float pan = (k & 1 ? 0.5f : -0.5f) * (k < 2 ? 0.6f : 1.0f);
                    l += s * (1.0f - pan * wid);
                    r += s * (1.0f + pan * wid);
                }
            }
            (void) nv;
            // fréquence d'échantillonnage d'époque : échantillon tenu, sans filtre (repliement voulu)
            holdPh += holdStep;
            if (holdPh >= 1.0) { holdPh -= 1.0; holdL = l; holdR = r; }
            l = holdL; r = holdR;
            if (q > 0.0f) { l = std::round (l * q) / q; r = std::round (r * q) / q; }
            // filtre de sortie : deux pôles + passe-haut (petit haut-parleur en « Portable »)
            lp1L += lpA * (l - lp1L); lp2L += lpA * (lp1L - lp2L);
            lp1R += lpA * (r - lp1R); lp2R += lpA * (lp1R - lp2R);
            const float yl = lp2L - hpxL + hpA * hpL; hpxL = lp2L; hpL = yl;
            const float yr = lp2R - hpxR + hpA * hpR; hpxR = lp2R; hpR = yr;
            l = yl; r = yr;
            // écho à filtre 8 points sur la réinjection
            if (emix > 0.0f || efb > 0.0f)
            {
                const int rd = (echoPos - edl + (int) echoL.size()) % (int) echoL.size();
                float el = echoL[(size_t) rd], er = echoR[(size_t) rd];
                for (int t = 7; t > 0; --t) { firL[(size_t) t] = firL[(size_t) t - 1]; firR[(size_t) t] = firR[(size_t) t - 1]; }
                firL[0] = el; firR[0] = er;
                float fl = 0, fr = 0;
                for (int t = 0; t < 8; ++t) { fl += fir[t] * firL[(size_t) t]; fr += fir[t] * firR[(size_t) t]; }
                float wl = std::clamp (l + fl * efb, -2.0f, 2.0f), wr = std::clamp (r + fr * efb, -2.0f, 2.0f);
                if (std::fabs (wl) < 1e-15f) wl = 0.0f;
                if (std::fabs (wr) < 1e-15f) wr = 0.0f;
                echoL[(size_t) echoPos] = wl;
                echoR[(size_t) echoPos] = wr;
                echoPos = (echoPos + 1) % (int) echoL.size();
                l += fl * emix; r += fr * emix;
            }
            l = std::tanh (l * gain); r = std::tanh (r * gain);
            left[i] += l; right[i] += r;
        }
        // pas de nombres dénormaux dans les mémoires des filtres et de l'écho au repos
        auto flush = [] (float& x) { if (std::fabs (x) < 1e-15f) x = 0.0f; };
        flush (lp1L); flush (lp1R); flush (lp2L); flush (lp2R); flush (hpL); flush (hpR); flush (hpxL); flush (hpxR);
        flush (holdL); flush (holdR);
    }

private:
    struct Pending { int note; float vel; bool on; };

    //------------------------------------------------------------------------------ voix mélodique
    struct Voice
    {
        bool active = false, gate = false;
        int note = 60; float vel = 1.0f;
        int age = 0;
        double tickLeft = 0.0;
        int frames = 0, relFrame = 0;
        float volLevel = 0.0f;          // 0..15 (marches)
        float volTgt = 0.0f, volCur = 0.0f;  // niveau visé (0..1) et niveau joué, lissé sur ~1 ms (sans clic)
        bool restartPending = false, killing = false;
        int pendNote = 60; float pendVel = 1.0f;
        float volFrom = 0.0f;           // niveau au relâchement
        double curPitch = 60.0, fromPitch = 60.0;
        double hz = 261.6;
        double ph = 0.0, ph2 = 0.0;
        int dutyIdx = 2;
        // bruit
        std::uint32_t lfsr = 1; double nph = 0.0; float nval = 1.0f;
        // FM
        double op[4] {}; float fbMem[2] {};
        // échantillon
        double spos = 0.0;

        void start (int nt, float v, bool legato)
        {
            if (active && ! legato && volCur > 1.0e-3f)
            {
                restartPending = true; killing = false; pendNote = nt; pendVel = v; note = nt;
                return;
            }
            restartPending = false; killing = false;
            const bool was = active;
            note = nt; vel = v;
            active = true; gate = true;
            if (! (legato && was))
            {
                frames = 0; volLevel = 0.0f; ph = 0.0; ph2 = 0.0; spos = 0.0;
                for (auto& o : op) o = 0.0;
                fbMem[0] = fbMem[1] = 0.0f; lfsr = 1; nph = 0.0;
                tickLeft = 0.0;
            }
            else frames = std::min (frames, 1);
        }
        void release() { if (gate) { gate = false; relFrame = frames; volFrom = volLevel; } }
        void kill() { if (active) { killing = true; gate = false; } }

        /** Une image (1/60 ou 1/50 s) : tables de volume, d'arpège, de largeur et de hauteur. */
        void frame (const float* p, int chip, double tickHz, float bend) noexcept
        {
            const double fms = 1000.0 / tickHz;
            const double t = frames * fms;
            // volume en 16 marches
            const double a = p[S::vAtk], d = p[S::vDec], s = p[S::vSus], r = p[S::vRel];
            double lvl;
            if (gate)
            {
                if (t < a) lvl = 15.0 * (t + fms) / std::max (fms, a);
                else lvl = s + (15.0 - s) * (d <= 0 ? 0.0 : std::max (0.0, 1.0 - (t - a) / d));
            }
            else
            {
                const double tr = (frames - relFrame) * fms;
                lvl = r <= 0 ? 0.0 : volFrom * std::max (0.0, 1.0 - tr / r);
                if (lvl < 0.5) lvl = 0.0;   // la voix s'éteint quand le niveau joué arrive à zéro (sample)
            }
            volLevel = (float) std::clamp (std::floor (lvl + 0.5), 0.0, 15.0);
            // 8 bits et portable : 16 marches de volume (comme les puces) ; FM et échantillons : enveloppe fine
            const bool stepped = chip == gamer::chip8 || chip == gamer::chipGb;
            if (! restartPending && ! killing)
                volTgt = stepped ? volLevel / 15.0f : (float) std::clamp (lvl / 15.0, 0.0, 1.0);
            // hauteur : glisse, arpège, chute, vibrato retardé (en marches)
            const double target = note + p[S::tune];
            const double gl = p[S::glide];
            if (gl > 0.0 && frames > 0) { const double k = std::min (1.0, fms / gl); curPitch += (target - curPitch) * k * 2.0; if (std::fabs (target - curPitch) < 0.01) curPitch = target; }
            else curPitch = target;
            int na = 1;
            const int* at = gamer::arpTable ((int) p[S::arp], na);
            const int sp = std::max (1, (int) p[S::arpSpeed]);
            const double arpOff = at[(frames / sp) % na];
            const double st = p[S::sweepTime];
            const double sw = p[S::sweep] * std::max (0.0, 1.0 - t / std::max (1.0, st));
            double vib = 0.0;
            if (p[S::vibDepth] > 0.0f && t >= p[S::vibDelay])
                vib = p[S::vibDepth] / 100.0 * std::sin (2.0 * M_PI * p[S::vibRate] * (t - p[S::vibDelay]) / 1000.0);
            hz = gamer::midiHz (curPitch + arpOff + sw + vib + bend * 2.0);
            // largeur qui bouge (8 bits / portable)
            const int base = (int) p[S::duty];
            switch ((int) p[S::dutyMove])
            {
                case 1: dutyIdx = std::min (3, base + frames / 6); break;
                case 2: dutyIdx = (frames / 4) % 2 ? (base + 2) % 4 : base; break;
                case 3: { static const int seq[6] = { 0, 1, 2, 3, 2, 1 }; dutyIdx = seq[(frames / 3 + base) % 6]; break; }
                default: dutyIdx = base; break;
            }
            ++frames;
        }

        float sample (const float* p, int chip, double sr, const std::vector<gamer::Smp>& smps) noexcept
        {
            {
                const float tgt = (restartPending || killing) ? 0.0f : volTgt;
                const float k = (restartPending || killing) ? (float) (1.0 - std::exp (-1.0 / (0.0006 * sr)))
                                                            : (float) (1.0 - std::exp (-1.0 / (0.001 * sr)));
                volCur += (tgt - volCur) * k;
                if (volCur < 1.0e-4f && tgt <= 0.0f)
                {
                    volCur = 0.0f;
                    if (restartPending) { restartPending = false; start (pendNote, pendVel, false); }
                    else if (killing || ! gate) { active = false; killing = false; return 0.0f; }
                }
            }
            const float amp = volCur * (0.35f + 0.65f * vel);
            if (amp <= 0.0f) return 0.0f;
            const double dt = hz / sr;
            float y = 0.0f;
            const int w = (int) p[S::wave8];
            if (chip == gamer::chip8 || chip == gamer::chipGb)
            {
                if (w == 0)
                {
                    static const double duties[4] = { 0.125, 0.25, 0.5, 0.75 };
                    const double du = duties[std::clamp (dutyIdx, 0, 3)];
                    float v = ph < du ? 1.0f : -1.0f;
                    v += gamer::polyBlep (ph, dt);
                    double t2 = ph - du; if (t2 < 0) t2 += 1.0;
                    v -= gamer::polyBlep (t2, dt);
                    y = v;
                }
                else if (w == 1)
                {
                    const int stp = std::min (31, (int) (ph * 32.0));
                    if (chip == gamer::chip8)
                    {
                        const int lv = stp < 16 ? 15 - stp : stp - 16;     // 15 .. 0 .. 15 : triangle en escalier
                        // le triangle n'a pas de volume : tout ou rien (porte lissée, sans clic)
                        y = (float) (lv / 7.5 - 1.0) * (0.35f + 0.65f * vel) * std::min (1.0f, volCur * 15.0f);
                        ph += dt;
                        if (ph >= 1.0) ph -= 1.0;
                        return y;
                    }
                    y = gamer::gbWaveValue ((int) p[S::gbWave], stp);
                }
                else
                {
                    // bruit : registre à décalage cadencé par la note (long 15 bits, court 93 pas / 7 bits)
                    const bool shortMode = (int) p[S::noiseMode] == 1;
                    nph += hz * 8.0 / sr;
                    while (nph >= 1.0)
                    {
                        nph -= 1.0;
                        const std::uint32_t bit = (lfsr ^ (lfsr >> (shortMode ? 6 : 1))) & 1u;
                        lfsr = (lfsr >> 1) | (bit << 14);
                        nval = (lfsr & 1u) ? 1.0f : -1.0f;
                    }
                    y = nval;
                }
                // volume 4 bits
                y *= amp;
            }
            else if (chip == gamer::chipFm)
            {
                // 4 opérateurs, rapports, niveaux ; modulateurs qui retombent ; rétroaction de l'op. 1
                static const double mul[16] = { 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
                const double tsec = frames / 60.0;
                const double modEnv = 0.35 + 0.65 * std::exp (-tsec * 1000.0 / std::max (10.0f, p[S::fmDec]));
                float o[4];
                const int alg = std::clamp ((int) p[S::fmAlgo], 1, 8) - 1;
                const float fbAmt = p[S::fmFb] > 0.0f ? (float) std::ldexp (1.0, (int) p[S::fmFb]) / 64.0f : 0.0f;
                float lv[4];
                for (int k = 0; k < 4; ++k) lv[k] = p[S::fmLev1 + k] / 100.0f;
                // indices de modulation (radians / 2π) des opérateurs qui modulent
                const float mi = 1.6f * (float) modEnv;
                auto opv = [&] (int k, float mod) {
                    op[k] += dt * mul[std::clamp ((int) p[S::fmMul1 + k], 0, 15)];
                    op[k] -= std::floor (op[k]);
                    return gamer::fastSin (op[k] + mod);
                };
                const float fbIn = fbAmt * (fbMem[0] + fbMem[1]) * 0.5f;
                o[0] = opv (0, fbIn);
                fbMem[1] = fbMem[0]; fbMem[0] = o[0];
                float out = 0.0f;
                switch (alg)
                {
                    case 0: o[1] = opv (1, o[0] * lv[0] * mi); o[2] = opv (2, o[1] * lv[1] * mi); o[3] = opv (3, o[2] * lv[2] * mi); out = o[3] * lv[3]; break;
                    case 1: o[1] = opv (1, 0); o[2] = opv (2, (o[0] * lv[0] + o[1] * lv[1]) * mi); o[3] = opv (3, o[2] * lv[2] * mi); out = o[3] * lv[3]; break;
                    case 2: o[1] = opv (1, 0); o[2] = opv (2, o[1] * lv[1] * mi); o[3] = opv (3, (o[0] * lv[0] + o[2] * lv[2]) * mi); out = o[3] * lv[3]; break;
                    case 3: o[1] = opv (1, o[0] * lv[0] * mi); o[2] = opv (2, 0); o[3] = opv (3, (o[1] * lv[1] + o[2] * lv[2]) * mi); out = o[3] * lv[3]; break;
                    case 4: o[1] = opv (1, o[0] * lv[0] * mi); o[2] = opv (2, 0); o[3] = opv (3, o[2] * lv[2] * mi); out = (o[1] * lv[1] + o[3] * lv[3]) * 0.6f; break;
                    case 5: o[1] = opv (1, o[0] * lv[0] * mi); o[2] = opv (2, o[0] * lv[0] * mi); o[3] = opv (3, o[0] * lv[0] * mi); out = (o[1] * lv[1] + o[2] * lv[2] + o[3] * lv[3]) * 0.45f; break;
                    case 6: o[1] = opv (1, o[0] * lv[0] * mi); o[2] = opv (2, 0); o[3] = opv (3, 0); out = (o[1] * lv[1] + o[2] * lv[2] + o[3] * lv[3]) * 0.45f; break;
                    default: o[1] = opv (1, 0); o[2] = opv (2, 0); o[3] = opv (3, 0); out = (o[0] * lv[0] + o[1] * lv[1] + o[2] * lv[2] + o[3] * lv[3]) * 0.35f; break;
                }
                y = out * amp;
                // convertisseur 9 bits et son défaut au passage par zéro (« grain »)
                const float g = p[S::fmGrain] / 100.0f;
                if (g > 0.0f)
                {
                    y = std::round (y * 256.0f) / 256.0f;
                    if (y != 0.0f) y += (y > 0 ? 1.0f : -1.0f) * g * 0.006f;
                }
                return y;
            }
            else if (chip == gamer::chipSmp)
            {
                const auto& s = smps[(size_t) std::clamp ((int) p[S::smpl], 0, (int) smps.size() - 1)];
                if (s.d.size() < 8) return 0.0f;
                const int len = (int) s.d.size();
                const int i0 = (int) spos;
                const float f = (float) (spos - i0);
                auto at = [&] (int k) { if (k >= len) k = s.loopStart + (k - s.loopStart) % (len - s.loopStart); return s.d[(size_t) k]; };
                if ((int) p[S::smplSmooth] == 0)
                {
                    // interpolation douce sur 4 points (l'arrondi des consoles 16 bits)
                    const float a0 = at (std::max (0, i0 - 1)), a1 = at (i0), a2 = at (i0 + 1), a3 = at (i0 + 2);
                    const float w0 = (1 - f) * (1 - f) * (1 - f) / 6.0f, w1 = (3 * f * f * f - 6 * f * f + 4) / 6.0f;
                    const float w2 = (-3 * f * f * f + 3 * f * f + 3 * f + 1) / 6.0f, w3 = f * f * f / 6.0f;
                    y = a0 * w0 + a1 * w1 + a2 * w2 + a3 * w3;
                }
                else y = at (i0);
                spos += hz / gamer::sampleBaseHz * gamer::sampleRateSmp / sr;
                if (spos >= len) spos = s.loopStart + std::fmod (spos - s.loopStart, (double) (len - s.loopStart));
                return y * amp * 1.4f;
            }
            ph += dt;
            if (ph >= 1.0) ph -= 1.0;
            return y;
        }
    };

    //------------------------------------------------------------------------------ batterie
    struct Drum
    {
        bool active = false;
        int pad = 0; float vel = 1.0f; float pan = 0.0f;
        double t = 0.0, ph = 0.0, mph = 0.0, nph = 0.0;
        std::uint32_t lfsr = 1; float nval = 1.0f;
        float eT = 0.0f, eN = 0.0f;   // marches de volume lissées (~0,5 ms) : plus de clic à chaque marche

        void start (int pd, float v) { active = true; pad = pd; vel = v; t = 0.0; ph = mph = nph = 0.0; lfsr = 1; pan = ((pd * 7) % 5 - 2) * 0.15f; }

        float tick (const float* p, double sr) noexcept
        {
            const auto& d = gamer::drumDef (pad);
            const double dec = std::max (0.25, p[S::drDecay] / 100.0);
            const double tune = std::pow (2.0, p[S::drTune] / 12.0);
            const double tone = 0.4 + 1.2 * p[S::drTone] / 100.0;
            const double ms = t * 1000.0;
            float out = 0.0f;
            bool alive = false;
            if (d.osc > 0)
            {
                const double k = d.sweepMs > 1 ? std::min (1.0, ms / d.sweepMs) : 1.0;
                double f = (d.f0 * std::pow (d.f1 / std::max (1.0, d.f0), k)) * tune;
                if (d.fx == 1 && ms > 80.0) f = 1319.0 * tune;     // pièce : deuxième note
                const double env = std::exp (-ms / (d.toneMs * dec));
                if (env > 0.002) alive = true;
                ph += f / sr; ph -= std::floor (ph);
                float v;
                if (d.osc == 1) { const int s = std::min (31, (int) (ph * 32)); const int lv = s < 16 ? 15 - s : s - 16; v = (float) (lv / 7.5 - 1.0); }
                else if (d.osc == 2) v = ph < 0.5 ? 1.0f : -1.0f;
                else { mph += f * d.fmRatio / sr; mph -= std::floor (mph); v = gamer::fastSin (ph + d.fmIndex * env * gamer::fastSin (mph) * 0.3); }
                const float e4 = std::floor ((float) env * 15.0f + 0.5f) / 15.0f;
                eT += (e4 - eT) * (float) (1.0 - std::exp (-1.0 / (0.0005 * sr)));
                out += v * eT * (float) d.toneLvl;
            }
            if (d.noise > 0)
            {
                double env = std::exp (-ms / (d.noiseMs * dec));
                if (d.fx == 2) { const double b = std::fmod (ms, 9.0); env *= ms < 27.0 ? (b < 4.0 ? 1.0 : 0.25) : 1.0; }
                if (env > 0.002) alive = true;
                const double k = std::min (1.0, ms / std::max (1.0, d.noiseMs * dec));
                const double rate = (d.nRate0 + (d.nRate1 - d.nRate0) * k) * tone;
                nph += rate / sr;
                while (nph >= 1.0)
                {
                    nph -= 1.0;
                    const std::uint32_t bit = (lfsr ^ (lfsr >> (d.noise == 2 ? 6 : 1))) & 1u;
                    lfsr = (lfsr >> 1) | (bit << 14);
                    nval = (lfsr & 1u) ? 1.0f : -1.0f;
                }
                const float e4 = std::floor ((float) env * 15.0f + 0.5f) / 15.0f;
                eN += (e4 - eN) * (float) (1.0 - std::exp (-1.0 / (0.0005 * sr)));
                out += nval * eN * (float) d.noiseLvl * 0.6f;
            }
            t += 1.0 / sr;
            if (! alive && t > 0.02 && eT < 1.0e-4f && eN < 1.0e-4f) active = false;
            return out * (0.35f + 0.65f * vel) * 0.7f;
        }
    };

    void applyPending (const float* p, int chipNow) noexcept
    {
        const int nv = std::clamp ((int) p[S::voices], 1, gamer::maxVoices);
        for (int e = 0; e < std::min (numPending, 64); ++e)
        {
            const auto& ev = pendingOn[(size_t) e];
            if (chipNow == gamer::chipDrum)
            {
                if (! ev.on) continue;
                const int pad = ev.note >= 36 && ev.note < 52 ? ev.note - 36 : ((ev.note % 16) + 16) % 16;
                // même pad : repart ; charley fermé coupe l'ouvert
                if (pad == 5) drums[6].active = false;
                drums[(size_t) pad].start (pad, ev.vel);
                continue;
            }
            if (ev.on)
            {
                if (nv == 1)
                {
                    auto& v = vs[0];
                    const bool legato = v.active && v.gate;
                    if (! (legato && p[S::glide] > 0.0f)) v.curPitch = ev.note + p[S::tune];
                    v.start (ev.note, ev.vel, legato);
                    for (int k = 1; k < gamer::maxVoices; ++k) vs[(size_t) k].kill();
                    heldStack (ev.note, true);
                    continue;
                }
                int best = -1, oldest = -1, oldAge = -1;
                for (int k = 0; k < nv; ++k)
                {
                    auto& v = vs[(size_t) k];
                    if (! v.active) { best = k; break; }
                    if (v.age > oldAge) { oldAge = v.age; oldest = k; }
                }
                if (best < 0) best = oldest;
                for (int k = 0; k < nv; ++k) ++vs[(size_t) k].age;
                auto& v = vs[(size_t) best];
                v.curPitch = ev.note + p[S::tune];
                if (p[S::glide] > 0.0f && lastNote >= 0) v.curPitch = lastNote + p[S::tune];
                v.start (ev.note, ev.vel, false);
                v.age = 0;
                lastNote = ev.note;
            }
            else
            {
                if (nv == 1)
                {
                    heldStack (ev.note, false);
                    auto& v = vs[0];
                    if (numHeld > 0) { v.start (held[(size_t) numHeld - 1], v.vel, true); }
                    else if (v.note == ev.note) v.release();
                    continue;
                }
                for (int k = 0; k < gamer::maxVoices; ++k)
                {
                    auto& v = vs[(size_t) k];
                    if (v.active && v.gate && v.note == ev.note) v.release();
                }
            }
        }
        numPending = 0;
    }

    void heldStack (int note, bool on) noexcept
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] == note) { for (int j = i; j + 1 < numHeld; ++j) held[(size_t) j] = held[(size_t) j + 1]; --numHeld; break; }
        if (on && numHeld < 16) held[(size_t) numHeld++] = note;
    }

    double sr = 44100.0;
    std::array<Voice, gamer::maxVoices> vs {};
    std::array<Drum, 16> drums {};
    std::vector<gamer::Smp> samples;
    std::array<Pending, 64> pendingOn {};
    int numPending = 0;
    std::array<int, 16> held {};
    int numHeld = 0, lastNote = -1;
    float bend = 0.0f;
    std::vector<float> echoL, echoR;
    int echoPos = 0;
    std::array<float, 8> firL {}, firR {};
    double holdPh = 1.0;
    float holdL = 0, holdR = 0;
    float lp1L = 0, lp1R = 0, lp2L = 0, lp2R = 0, hpL = 0, hpR = 0, hpxL = 0, hpxR = 0;
};

} // namespace grv::instruments
