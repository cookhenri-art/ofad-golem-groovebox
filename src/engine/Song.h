/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Morceau (arrangeur, repris du principe de Sillage) :
      - une suite de RANGÉES jouées l'une après l'autre en mode morceau : chacune dure un nombre
        de mesures (4/4) et dit, pour chaque piste, le pattern à lancer (A à H), l'arrêt, ou
        « garder » ce qui joue ; une scène de réglages peut partir avec elle ;
      - des COURBES d'automatisation : pour un réglage, des points (temps en noires, valeur 0..1)
        relus en ligne droite ; elles s'enregistrent en bougeant les boutons ou le contrôleur ;
      - des COURBES DE PATTERN : enregistrées avec ENR., elles appartiennent à un pattern d'une
        piste (temps en pas depuis le début du pattern) et tournent avec lui, lisses.
    Le jeu en direct (lancements de patterns) s'enregistre aussi en rangées, mesure par mesure.

    Construit sur le fil de l'interface, publié sans verrou, lu par le fil audio (Published.h).
*/

#pragma once

#include "Params.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace grv::engine
{

constexpr int maxSongRows = 128;
constexpr int maxRowBars = 64;
constexpr int maxAutoLanes = 64;
constexpr int maxAutoPoints = 4096;
constexpr int maxPatLanes = 256;    // courbes de pattern, toutes pistes confondues
constexpr int songKeep = -2;     // la piste garde ce qu'elle joue
constexpr int songStop = -1;     // la piste s'arrête

// v0.46 : courbes d'interrupteurs de piste (coupure, solo), enregistrées avec « Enr. les mouvements »
// comme les boutons : leurs adresses suivent celles des réglages (0 ou 1, en marches d'escalier)
inline constexpr int muteLane (int track) noexcept { return numSlots + track; }
inline constexpr int soloLane (int track) noexcept { return numSlots + numTracks + track; }
inline constexpr bool isSwitchLane (int global) noexcept { return global >= numSlots && global < numSlots + 2 * numTracks; }
inline constexpr int switchTrack (int global) noexcept { return (global - numSlots) % numTracks; }
inline constexpr bool switchIsSolo (int global) noexcept { return global >= numSlots + numTracks; }
// v0.46 : courbe du crossfader des scènes A / B (0 = A, 1 = B), enregistrée et relue comme un bouton
inline constexpr int xfadeLane = numSlots + 2 * numTracks;
// v0.46 fix7 : coupure / solo des retours, du principal et des groupes (canaux après les pistes)
inline constexpr int numBusLanes = numChannels - numTracks;
inline constexpr int busLaneBase = xfadeLane + 1;
inline constexpr int busMuteLane (int channel) noexcept { return busLaneBase + (channel - numTracks); }
inline constexpr int busSoloLane (int channel) noexcept { return busLaneBase + numBusLanes + (channel - numTracks); }
inline constexpr bool isBusSwitchLane (int global) noexcept { return global >= busLaneBase && global < busLaneBase + 2 * numBusLanes; }
inline constexpr int busSwitchChannel (int global) noexcept { return numTracks + (global - busLaneBase) % numBusLanes; }
inline constexpr bool busSwitchIsSolo (int global) noexcept { return global >= busLaneBase + numBusLanes; }
inline constexpr bool isSpecialLane (int global) noexcept { return isSwitchLane (global) || global == xfadeLane || isBusSwitchLane (global); }
constexpr int numLaneAddresses = busLaneBase + 2 * numBusLanes;

struct SongRow
{
    int bars = 4;
    int scene = -1;              // scène de réglages lancée avec la rangée (-1 : aucune)
    std::array<std::int8_t, numTracks> pat {};
    SongRow() { pat.fill ((std::int8_t) songKeep); }
    bool samePatterns (const SongRow& o) const noexcept { return pat == o.pat; }
};

struct AutoPoint { float beat = 0.0f, norm = 0.0f; };

/** v0.44.1 : un point de valeur négative (-1) ouvre un « trou » : de ce point jusqu'au suivant,
    la courbe ne pilote plus le réglage (il reprend sa valeur normale). C'est ce que fait la gomme. */
constexpr float autoGap = -1.0f;
inline float autoNorm (float n) noexcept { return n < 0.0f ? autoGap : std::clamp (n, 0.0f, 1.0f); }

struct AutoLane
{
    int global = -1;
    bool enabled = true;
    std::vector<AutoPoint> pts;  // triés par temps

    /** Valeur au temps donné : en ligne droite entre deux points, la première avant, la dernière après.
        Négative (autoGap) dans un trou : le réglage n'est pas piloté. Avant un trou, la valeur tient. */
    float valueAt (double beat) const noexcept
    {
        if (pts.empty()) return autoGap;
        if (beat <= pts.front().beat) return pts.front().norm;
        if (beat >= pts.back().beat) return pts.back().norm;
        const auto it = std::upper_bound (pts.begin(), pts.end(), (float) beat, [] (float b, const AutoPoint& p) { return b < p.beat; });
        const auto& b = *it;
        const auto& a = *(it - 1);
        if (a.norm < 0.0f) return autoGap;
        if (b.norm < 0.0f) return a.norm;
        const auto span = b.beat - a.beat;
        if (span <= 1.0e-6f) return b.norm;
        return a.norm + (b.norm - a.norm) * (float) ((beat - a.beat) / span);
    }
};

/** Courbe d'un pattern : temps en pas depuis le début du pattern (boucle avec lui). */
struct PatternLane
{
    int track = -1, pattern = 0;
    AutoLane lane;
};

struct Song
{
    bool songMode = false;       // les rangées pilotent les lancements
    bool loop = true;            // en fin de boucle : on reprend (sinon arrêt en fin de morceau)
    int loopFrom = 0, loopTo = -1; // rangées bouclées (comprises) ; -1 : jusqu'à la dernière
    int loopCount = 0;           // v0.24 : nombre de passages de la boucle (0 = sans fin), puis la fin du morceau
    bool autoRead = true;        // les courbes sont relues
    std::vector<SongRow> rows;
    std::vector<AutoLane> lanes;
    std::vector<PatternLane> patLanes;

    double rowStart (int r) const noexcept
    {
        double b = 0.0;
        for (int i = 0; i < r && i < (int) rows.size(); ++i) b += 4.0 * rows[(size_t) i].bars;
        return b;
    }
    double totalBeats() const noexcept { return rowStart ((int) rows.size()); }

    /** Rangée jouée à la position donnée (en noires depuis le début du morceau), -1 au-delà. */
    int rowAt (double beat) const noexcept
    {
        if (beat < 0.0) return rows.empty() ? -1 : 0;
        double b = 0.0;
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            b += 4.0 * rows[(size_t) i].bars;
            if (beat < b - 1.0e-9) return i;
        }
        return -1;
    }

    /** Rangées bouclées, bornées par celles du morceau (première et dernière comprises). */
    int loopFirst() const noexcept { return std::clamp (loopFrom, 0, std::max (0, (int) rows.size() - 1)); }
    int loopLast() const noexcept
    {
        const auto n = (int) rows.size();
        const auto l = loopTo < 0 || loopTo >= n ? n - 1 : loopTo;
        return std::max (l, loopFirst());
    }
    double loopStartBeat() const noexcept { return rowStart (loopFirst()); }
    double loopEndBeat() const noexcept { return rows.empty() ? 0.0 : rowStart (loopLast() + 1); }

    /** Marge de calcul (en noires) : une position à moins de 1e-7 noire de la fin de la boucle
        compte déjà pour le tour suivant. Sans elle, après des centaines de tours, l'arrondi pouvait
        donner « juste avant la fin du morceau », vu comme « après la fin » : le morceau s'arrêtait
        (corrigé en v0.24). */
    static constexpr double loopEps = 1.0e-7;

    /** Tour de boucle (0 pendant le premier passage, plafonné au dernier passage si le nombre
        de passages est fixé). Même calcul que wrap(). */
    long long loopPass (double ppq) const noexcept
    {
        if (! loop || rows.empty()) return 0;
        const auto a = loopStartBeat(), len = loopEndBeat() - a;
        if (len <= 0.0 || ppq < a) return 0;
        const auto k = (long long) std::floor ((ppq - a + loopEps) / len);
        return loopCount > 0 ? std::min (k, (long long) loopCount - 1) : k;
    }

    /** Position dans le morceau : le début se joue une fois, puis la boucle se répète
        (de sa première rangée à la fin de sa dernière), sans fin ou « loopCount » fois ; après
        le dernier passage, la suite du morceau (rangées après la boucle) puis l'arrêt. */
    double wrap (double ppq) const noexcept
    {
        if (! songMode || ! loop || rows.empty()) return ppq;
        const auto a = loopStartBeat(), b = loopEndBeat(), len = b - a;
        if (len <= 0.0 || ppq < b - loopEps) return ppq;
        const auto k = (long long) std::floor ((ppq - a + loopEps) / len);
        if (loopCount > 0 && k >= loopCount) return ppq - (double) (loopCount - 1) * len; // après la boucle
        return std::clamp (ppq - (double) k * len, a, b - 2.0 * loopEps);
    }

    const AutoLane* laneFor (int global) const noexcept
    {
        for (const auto& l : lanes) if (l.global == global) return &l;
        return nullptr;
    }
    int patLaneIndex (int track, int pattern, int global) const noexcept
    {
        for (size_t i = 0; i < patLanes.size(); ++i)
            if (patLanes[i].track == track && patLanes[i].pattern == pattern && patLanes[i].lane.global == global) return (int) i;
        return -1;
    }
};

/** v0.46 fix7 : les rangées ont changé (longueur, insertion, suppression) : les courbes du morceau
    suivent leurs rangées. Les rangées identiques du début et de la fin sont repérées ; au milieu :
    - même nombre de rangées : chaque rangée garde ses points, décalés avec elle ; raccourcie, ce qui
      dépasse sa nouvelle fin est retiré (un point garde la valeur à l'endroit de la coupe) ;
    - rangées insérées : rien à reprendre ; rangées supprimées : leurs points partent avec elles ;
    - les rangées qui suivent (et ce qui dépasse la fin du morceau) glissent d'autant.
    Toutes les rangées vidées : les courbes restent telles quelles (« Vider les courbes » les retire). */
inline void rippleSongLanes (const Song& before, Song& after)
{
    const auto& o = before.rows;
    const auto& n = after.rows;
    if (n.empty() || o.empty() || after.lanes.empty()) return;
    const auto same = [] (const SongRow& x, const SongRow& y) { return x.bars == y.bars && x.scene == y.scene && x.pat == y.pat; };
    const int no = (int) o.size(), nn = (int) n.size();
    int p = 0;
    while (p < no && p < nn && same (o[(size_t) p], n[(size_t) p])) ++p;
    if (p == no && p == nn) return;
    int q = 0;
    while (q < no - p && q < nn - p && same (o[(size_t) (no - 1 - q)], n[(size_t) (nn - 1 - q)])) ++q;

    struct Seg { double from, to, shift; };       // ancien [from, to) décalé de shift ; ailleurs : retiré
    struct Cut { double oldAt, newAt; };          // rangée raccourcie : valeur gardée à la coupe
    std::vector<Seg> segs;
    std::vector<Cut> cuts;
    segs.push_back ({ -1.0e9, before.rowStart (p), 0.0 });
    if (no - p - q == nn - p - q)
        for (int i = p; i < no - q; ++i)
        {
            const auto os = before.rowStart (i), ns = after.rowStart (i);
            const auto ol = 4.0 * o[(size_t) i].bars, nl = 4.0 * n[(size_t) i].bars;
            segs.push_back ({ os, os + std::min (ol, nl), ns - os });
            if (nl < ol) cuts.push_back ({ os + nl - 1.0e-3, ns + nl - 1.0e-3 });
        }
    segs.push_back ({ before.rowStart (no - q), 1.0e12, after.rowStart (nn - q) - before.rowStart (no - q) });

    for (auto& lane : after.lanes)
    {
        std::vector<AutoPoint> pts;
        pts.reserve (lane.pts.size() + cuts.size());
        bool dropped = false;
        for (const auto& pt : lane.pts)
        {
            bool kept = false;
            for (const auto& sgm : segs)
                if ((double) pt.beat >= sgm.from - 1.0e-6 && (double) pt.beat < sgm.to - 1.0e-6)
                {
                    const auto b = (double) pt.beat + sgm.shift;
                    if (b >= 0.0) { pts.push_back ({ (float) b, pt.norm }); kept = true; }
                    break;
                }
            dropped = dropped || ! kept;
        }
        if (dropped)
            for (const auto& c : cuts)
                if (const auto v = lane.valueAt (c.oldAt); v >= 0.0f && c.newAt >= 0.0) pts.push_back ({ (float) c.newAt, v });
        std::stable_sort (pts.begin(), pts.end(), [] (const AutoPoint& a, const AutoPoint& b) { return a.beat < b.beat; });
        if (pts.size() > (size_t) maxAutoPoints) pts.resize ((size_t) maxAutoPoints);
        lane.pts = std::move (pts);
    }
    after.lanes.erase (std::remove_if (after.lanes.begin(), after.lanes.end(), [] (const AutoLane& l) { return l.pts.empty(); }), after.lanes.end());
}

/** Mesure enregistrée pendant le jeu en direct : patterns joués au début de la mesure. */
struct BarCapture
{
    long long bar = 0;
    int scene = -1;
    std::array<std::int8_t, numTracks> pat {};
};

/** Mouvement d'un réglage pendant l'enregistrement des mouvements. */
struct MoveRecord
{
    int global = -1;
    float norm = 0.0f;
    double beat = 0.0;     // morceau : noires ; pattern : pas depuis son début
    int track = -1;        // ≥ 0 : courbe du pattern joué par cette piste (ENR.)
    int pattern = 0;
};

} // namespace grv::engine
