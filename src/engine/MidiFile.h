/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 : export d'un projet en fichier MIDI standard (SMF type 1, 480 tics par noire).

      - une piste MIDI par piste GOLEM qui a des notes (nom de la piste, son canal) ;
      - batteries sur le canal 10, pads rangés selon la convention General MIDI d'après leur nom
        (grosse caisse 36, caisse claire 38, clap 39, charley fermé 42, ouvert 46…) ;
      - avec le morceau (rangées) : chaque rangée joue, pendant ses mesures, le pattern qu'elle donne
        à chaque piste (garder = le pattern continue, arrêt = silence) ; sans morceau : le pattern
        joué de chaque piste, sur la longueur du plus long ;
      - par pas : vélocité (accent : +20), durée, micro-décalage, répétitions (ratchets) ; glissé :
        la note se lie à la suivante ; longueur de ligne propre (polyrythmie), vitesse et division du
        pattern ; conditions A:B, première fois, Fill (Fill joué = non), probabilité (jouée à 50 % et
        plus) ; les verrous de réglages ne sont pas exportés (ce ne sont pas des notes).

    Même table de correspondance pour le futur import (drumNoteForPad / padForDrumNote).
*/

#pragma once

#include "Engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace grv::engine::midifile
{

constexpr int ticksPerQuarter = 480;

/** Note General MIDI d'un pad de batterie d'après son nom (et son rang si le nom ne dit rien). */
inline int drumNoteForPad (const std::string& name, int pad) noexcept
{
    auto has = [&name] (const char* s) { return name.find (s) != std::string::npos; };
    if (has ("Grosse caisse")) return has ("2") ? 35 : 36;
    if (has ("Basse longue")) return 35;
    if (has ("Caisse claire")) return has ("2") ? 40 : 38;
    if (has ("Clap")) return 39;
    if (has ("Rimshot") || has ("Rim")) return 37;
    if (has ("Charley ferm")) return 42;
    if (has ("Charley ouvert")) return 46;
    if (has ("Charley p")) return 44;
    if (has ("Tom grave")) return 41;
    if (has ("Tom m")) return 45;
    if (has ("Tom aigu")) return 48;
    if (has ("Crash")) return 49;
    if (has ("Ride")) return 51;
    if (has ("Cloche")) return 56;
    if (has ("Conga")) return 63;
    if (has ("Maracas") || has ("Shaker")) return 70;
    if (has ("Claves")) return 75;
    return 60 + std::clamp (pad, 0, 15); // échantillons et pads sans équivalent : à partir du do 4
}

namespace detail
{
    inline void putVar (std::vector<std::uint8_t>& o, std::uint32_t v)
    {
        std::uint8_t buf[5]; int n = 0;
        buf[n++] = (std::uint8_t) (v & 0x7F);
        while ((v >>= 7) != 0) buf[n++] = (std::uint8_t) ((v & 0x7F) | 0x80);
        while (n > 0) o.push_back (buf[--n]);
    }
    inline void put32 (std::vector<std::uint8_t>& o, std::uint32_t v)
    { o.push_back ((std::uint8_t) (v >> 24)); o.push_back ((std::uint8_t) (v >> 16)); o.push_back ((std::uint8_t) (v >> 8)); o.push_back ((std::uint8_t) v); }
    inline void put16 (std::vector<std::uint8_t>& o, int v) { o.push_back ((std::uint8_t) (v >> 8)); o.push_back ((std::uint8_t) v); }

    struct Ev { std::int64_t tick; int order; std::uint8_t b0, b1, b2; }; // order : note off avant note on au même tic

    inline void metaText (std::vector<std::uint8_t>& o, int type, const std::string& s)
    {
        putVar (o, 0); o.push_back (0xFF); o.push_back ((std::uint8_t) type);
        const auto n = std::min<size_t> (s.size(), 127);
        putVar (o, (std::uint32_t) n);
        o.insert (o.end(), s.begin(), s.begin() + (std::ptrdiff_t) n);
    }

    inline std::vector<std::uint8_t> chunk (const std::vector<std::uint8_t>& body)
    {
        std::vector<std::uint8_t> o { 'M', 'T', 'r', 'k' };
        put32 (o, (std::uint32_t) body.size());
        o.insert (o.end(), body.begin(), body.end());
        return o;
    }

    /** Une condition de pas, rejouée sans hasard ni Fill (export reproductible). */
    inline bool conditionPlays (const StepExtra* x, long long loop) noexcept
    {
        if (x == nullptr) return true;
        if (x->prob < 50) return false;
        switch (x->cond)
        {
            case Condition::ab:       return x->b > 0 && (int) (loop % x->b) + 1 == x->a;
            case Condition::fill:     return false;
            case Condition::first:    return loop == 0;
            case Condition::notFirst: return loop > 0;
            default:                  return true; // sans Fill, voisin et précédent : on joue
        }
    }
}

struct ExportOptions
{
    bool useSong = true;       // morceau (rangées) s'il y en a
    int barsWithoutSong = 0;   // sans morceau : nombre de mesures (0 = longueur du plus long pattern)
};

/** Le projet en fichier MIDI (octets d'un .mid). Fil de l'interface. */
inline std::vector<std::uint8_t> exportProject (const Engine& e, double bpm, const ExportOptions& opt = {})
{
    using namespace detail;
    const auto& seq = e.sequencer();
    const auto& song = e.getSong();
    const auto& extras = e.getExtras();
    const bool withSong = opt.useSong && ! song.rows.empty();

    // Segments de chaque piste : (début en noires, fin en noires, pattern, début du pattern en noires)
    struct Seg { double from, to; int pattern; double origin; };
    std::vector<std::vector<Seg>> segs ((size_t) numTracks);
    double totalBeats = 0.0;
    if (withSong)
    {
        std::array<int, numTracks> cur; cur.fill (-1);
        std::array<double, numTracks> origin {};
        double b = 0.0;
        for (const auto& row : song.rows)
        {
            const double len = 4.0 * std::max (1, row.bars);
            for (int t = 0; t < numTracks; ++t)
            {
                const int p = row.pat[(size_t) t];
                if (p == songStop) cur[(size_t) t] = -1;
                else if (p >= 0 && p < Sequencer::numPatterns && p != cur[(size_t) t]) { cur[(size_t) t] = p; origin[(size_t) t] = b; }
                if (cur[(size_t) t] >= 0) segs[(size_t) t].push_back ({ b, b + len, cur[(size_t) t], origin[(size_t) t] });
            }
            b += len;
        }
        totalBeats = b;
    }
    else
    {
        double longest = 4.0;
        for (int t = 0; t < numTracks; ++t)
        {
            const int p = std::clamp (seq.getPlaying (t), 0, Sequencer::numPatterns - 1);
            const double stepB = 4.0 / stepsPerBar (seq.getPatternDivision (t, p)) / speedFactor (seq.getPatternSpeed (t, p));
            longest = std::max (longest, stepB * seq.getPatternLength (t, p));
        }
        totalBeats = opt.barsWithoutSong > 0 ? 4.0 * opt.barsWithoutSong : std::ceil (longest / 4.0) * 4.0;
        for (int t = 0; t < numTracks; ++t)
            segs[(size_t) t].push_back ({ 0.0, totalBeats, std::clamp (seq.getPlaying (t), 0, Sequencer::numPatterns - 1), 0.0 });
    }

    std::vector<std::vector<std::uint8_t>> chunks;
    {
        std::vector<std::uint8_t> c0;
        metaText (c0, 0x03, "OFAD GOLEM");
        const auto us = (std::uint32_t) std::lround (60'000'000.0 / std::clamp (bpm, 20.0, 400.0));
        putVar (c0, 0); c0.push_back (0xFF); c0.push_back (0x51); c0.push_back (3);
        c0.push_back ((std::uint8_t) (us >> 16)); c0.push_back ((std::uint8_t) (us >> 8)); c0.push_back ((std::uint8_t) us);
        putVar (c0, 0); c0.push_back (0xFF); c0.push_back (0x58); c0.push_back (4); c0.push_back (4); c0.push_back (2); c0.push_back (24); c0.push_back (8);
        putVar (c0, (std::uint32_t) std::lround (totalBeats * ticksPerQuarter)); c0.push_back (0xFF); c0.push_back (0x2F); c0.push_back (0);
        chunks.push_back (chunk (c0));
    }

    int nextChannel = 0;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto* d = e.instrumentDesc (t);
        if (d == nullptr || segs[(size_t) t].empty()) continue;
        const bool drums = d->drumKit && ! d->laneNotes;
        int channel = 9;
        if (! drums) { if (nextChannel == 9) ++nextChannel; channel = nextChannel++ & 15; }

        std::vector<Ev> evs;
        auto addNote = [&] (double startB, double lenB, int note, int vel)
        {
            if (vel <= 0 || note < 0 || note > 127 || startB >= totalBeats) return;
            const auto on = (std::int64_t) std::llround (startB * ticksPerQuarter);
            const auto off = std::max (on + 1, (std::int64_t) std::llround (std::min (totalBeats, startB + lenB) * ticksPerQuarter));
            evs.push_back ({ on, 1, (std::uint8_t) (0x90 | channel), (std::uint8_t) note, (std::uint8_t) std::clamp (vel, 1, 127) });
            evs.push_back ({ off, 0, (std::uint8_t) (0x80 | channel), (std::uint8_t) note, 0 });
        };

        for (const auto& s : segs[(size_t) t])
        {
            const int p = s.pattern;
            const double stepB = 4.0 / stepsPerBar (seq.getPatternDivision (t, p)) / speedFactor (seq.getPatternSpeed (t, p));
            const int patLen = std::max (1, seq.getPatternLength (t, p));
            for (int lane = 0; lane < Sequencer::numLanes; ++lane)
            {
                const int laneLen = seq.getLaneLengthP (t, p, lane) > 0 ? seq.getLaneLengthP (t, p, lane) : patLen;
                // premier pas de la ligne dans le segment
                long long k = (long long) std::ceil ((s.from - s.origin) / stepB - 1.0e-9);
                for (;; ++k)
                {
                    const double at = s.origin + (double) k * stepB;
                    if (at >= s.to - 1.0e-9) break;
                    const int st = (int) (k % laneLen);
                    const long long loop = k / laneLen;
                    const int vel = seq.getStepP (t, p, lane, st);
                    if (vel <= 0) continue;
                    const auto* x = extras.find (t, p, lane, st);
                    if (! conditionPlays (x, loop)) continue;
                    const int flags = seq.getStepFlagsP (t, p, st);
                    const int v = std::min (127, vel + ((flags & stepAccent) != 0 ? 20 : 0));
                    const int note = drums ? drumNoteForPad (lane < (int) d->padNames.size() ? d->padNames[(size_t) lane] : std::string(), lane)
                                           : seq.getNoteP (t, p, lane, st);
                    const double micro = seq.getMicroP (t, p, lane, st) / 64.0 * stepB;
                    double len = seq.getLength8P (t, p, lane, st) / 8.0 * stepB;
                    if ((flags & stepSlide) != 0) len = std::max (len, stepB * 1.05); // glissé : liée à la suivante
                    if (drums) len = std::min (len, stepB * 0.9);
                    const int reps = x != nullptr ? std::clamp ((int) x->ratchet, 1, Sequencer::maxRatchet) : 1;
                    for (int r = 0; r < reps; ++r)
                    {
                        const double sub = stepB / reps;
                        const int rv = reps > 1 && x != nullptr ? std::clamp ((int) std::lround (v * (1.0 + x->ratchetSlope / 100.0 * r / std::max (1, reps - 1))), 1, 127) : v;
                        addNote (at + micro + r * sub, reps > 1 ? sub * 0.9 : len, note, rv);
                    }
                }
            }
        }
        if (evs.empty()) continue;
        std::sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : a.order < b.order; });

        std::vector<std::uint8_t> c;
        const auto& info = e.getTrackInfo (t);
        metaText (c, 0x03, info.name.empty() ? d->name : info.name);
        metaText (c, 0x04, d->name);
        if (! drums) { putVar (c, 0); c.push_back ((std::uint8_t) (0xC0 | channel)); c.push_back (0); }
        std::int64_t last = 0;
        for (const auto& ev : evs)
        {
            putVar (c, (std::uint32_t) std::max<std::int64_t> (0, ev.tick - last));
            last = std::max (last, ev.tick);
            c.push_back (ev.b0); c.push_back (ev.b1); c.push_back (ev.b2);
        }
        putVar (c, 0); c.push_back (0xFF); c.push_back (0x2F); c.push_back (0);
        chunks.push_back (chunk (c));
    }

    std::vector<std::uint8_t> out { 'M', 'T', 'h', 'd' };
    put32 (out, 6); put16 (out, 1); put16 (out, (int) chunks.size()); put16 (out, ticksPerQuarter);
    for (const auto& c : chunks) out.insert (out.end(), c.begin(), c.end());
    return out;
}

} // namespace grv::engine::midifile
