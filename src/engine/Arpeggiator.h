/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Arpégiateur de piste (principe des arpégiateurs des polyphoniques des années 80 et des
    synthés logiciels récents) : 20 modes — haut, bas, haut-bas et bas-haut (avec ou sans
    répétition des extrêmes), ordre joué (à l'endroit ou à l'envers), aléatoire, mélangé, marche
    aléatoire, accord, convergent, divergent, pédale grave ou aiguë, sauts, marches, notes doublées ; 1 à 4 octaves ; division calée sur le tempo (ternaire et pointée comprises) ;
    durée des notes ; maintien (les notes restent après le relâchement, jusqu'au prochain accord) ;
    swing ; arpège à pas (jusqu'à 16 pas : transposition et vélocité, vélocité 0 = silence).

    Joué par le moteur, à la cadence des blocs de contrôle (au plus 32 échantillons, < 1 ms).
    Calé sur la mesure quand le séquenceur joue, sinon parti de la première note. Sans allocation.
*/

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace grv::engine
{

class Arpeggiator
{
public:
    struct Settings
    {
        int mode = 0;            // 0 = arrêt
        int octaves = 1;
        double stepQuarters = 0.25;
        double gate = 0.6;       // part du pas
        bool hold = false;
        double swing = 0.5;      // 0,5 = droit
        int steps = 0;           // arpège à pas : 0 = sans
        const float* transpose = nullptr; // 16 valeurs (demi-tons)
        const float* velocity = nullptr;  // 16 valeurs (0..127)
    };

    // Les modes s'ajoutent toujours à la fin (les projets enregistrés gardent leur choix).
    enum Mode
    {
        off = 0, up, down, upDown, downUp, played, random, chord,
        upDownIncl,      // haut-bas, notes extrêmes répétées
        downUpIncl,      // bas-haut, notes extrêmes répétées
        converge,        // de l'extérieur vers l'intérieur : grave, aiguë, 2e grave, 2e aiguë…
        diverge,         // du milieu vers l'extérieur
        convergeDiverge, // l'un puis l'autre
        pedalLow,        // note grave en pédale (pouce) : 1-2-1-3-1-4…
        pedalHigh,       // note aiguë en pédale (auriculaire) : 4-1-4-2-4-3…
        skipUp,          // par sauts : 1-3-2-4-3-5…
        stairsUp,        // marches de trois : 1-2-3 2-3-4 3-4-5…
        stairsDown,      // marches de trois en descendant
        shuffle,         // aléatoire sans répétition : chaque note une fois par tour
        randomWalk,      // marche aléatoire : un pas vers le haut ou vers le bas
        playedReverse,   // ordre joué, à l'envers
        repeatUp,        // haut, chaque note deux fois
        numModes
    };

    void clear() noexcept { numHeld = 0; numLatched = 0; physical = 0; index = 0; stepCount = 0; nextPpq = -1.0; started = false; walk = 0; shuffleLen = 0; }

    void noteOn (int note, float velocity, bool hold) noexcept
    {
        // maintien : une nouvelle note après avoir tout relâché remplace l'accord retenu
        if (hold && physical == 0) numLatched = 0;
        ++physical;
        add (held, numHeld, note, velocity);
        if (hold) add (latched, numLatched, note, velocity);
        if (! started) { started = true; immediate = true; }
    }

    void noteOff (int note) noexcept
    {
        if (remove (held, numHeld, note)) physical = std::max (0, physical - 1);
    }

    /** Notes à arpéger : celles tenues, ou l'accord retenu en maintien. */
    int activeCount (bool hold) const noexcept { return hold ? std::max (numHeld, numLatched) : numHeld; }

    /** Avance d'un bloc. ppq : position en noires au début du bloc (séquenceur en marche) ;
        n échantillons ; onNote (note, vel) et offNote (note) jouent l'instrument. */
    template <typename OnNote, typename OffNote>
    void process (const Settings& s, double ppq, bool transportRunning, double ppqPerSample, int n, OnNote&& onNote, OffNote&& offNote) noexcept
    {
        const auto span = ppqPerSample * (double) n;
        const auto now = transportRunning ? ppq : clock;
        const auto end = now + span;
        if (! transportRunning) clock += span;

        // fin des notes jouées
        if (numSounding > 0 && offAt >= 0.0 && offAt < end) { releaseAll (offNote); offAt = -1.0; }

        const bool hold = s.hold;
        const auto count = activeCount (hold);
        if (count == 0)
        {
            if (numSounding > 0) { releaseAll (offNote); offAt = -1.0; }
            started = false; index = 0; stepCount = 0; nextPpq = -1.0; walk = 0; shuffleLen = 0;
            return;
        }

        const auto stepLen = std::max (1.0e-3, s.stepQuarters);
        auto boundary = [&] (double t)
        {
            const auto k = std::ceil (t / stepLen - 1.0e-9);
            return k * stepLen + (((long long) k & 1) ? (s.swing - 0.5) * 2.0 * stepLen : 0.0);
        };

        if (immediate)
        {
            // première note : tout de suite, puis sur la grille (sauf si la grille tombe trop près)
            immediate = false;
            playStep (s, now, stepLen, onNote, offNote);
            nextPpq = boundary (now + stepLen * 0.5);
        }
        // pas de rattrapage après un saut de la tête de lecture (ou un changement d'horloge)
        if (nextPpq < 0.0 || nextPpq < now - stepLen || nextPpq > end + 4.0 * stepLen + 16.0) nextPpq = boundary (now);
        while (nextPpq < end)
        {
            playStep (s, nextPpq, stepLen, onNote, offNote);
            nextPpq = boundary (nextPpq + stepLen * 0.5);
        }
    }

    /** Coupe les notes jouées (arpège arrêté, piste changée). */
    template <typename OffNote>
    void flush (OffNote&& offNote) noexcept { releaseAll (offNote); offAt = -1.0; clear(); }

private:
    struct Held { int note; float vel; };
    static constexpr int maxHeld = 16;

    static void add (std::array<Held, maxHeld>& list, int& count, int note, float vel) noexcept
    {
        for (int i = 0; i < count; ++i) if (list[(size_t) i].note == note) { list[(size_t) i].vel = vel; return; }
        if (count < maxHeld) list[(size_t) count++] = { note, vel };
    }
    static bool remove (std::array<Held, maxHeld>& list, int& count, int note) noexcept
    {
        for (int i = 0; i < count; ++i)
            if (list[(size_t) i].note == note)
            {
                for (int j = i; j < count - 1; ++j) list[(size_t) j] = list[(size_t) (j + 1)];
                --count;
                return true;
            }
        return false;
    }

    template <typename OffNote>
    void releaseAll (OffNote& offNote) noexcept
    {
        for (int i = 0; i < numSounding; ++i) offNote (sounding[(size_t) i]);
        numSounding = 0;
    }

    template <typename OnNote, typename OffNote>
    void playStep (const Settings& s, double at, double stepLen, OnNote& onNote, OffNote& offNote) noexcept
    {
        releaseAll (offNote);
        // notes sources, triées par hauteur (ou dans l'ordre joué)
        const auto& src = (s.hold && numLatched > numHeld) ? latched : held;
        const auto count = (s.hold && numLatched > numHeld) ? numLatched : numHeld;
        std::array<Held, maxHeld> notes {};
        std::copy (src.begin(), src.begin() + count, notes.begin());
        if (s.mode != played && s.mode != playedReverse)
            std::sort (notes.begin(), notes.begin() + count, [] (const Held& a, const Held& b) { return a.note < b.note; });

        // arpège à pas : transposition et vélocité du pas (vélocité 0 = silence)
        int transpose = 0;
        float stepVel = -1.0f;
        if (s.steps > 0 && s.transpose != nullptr && s.velocity != nullptr)
        {
            const auto k = stepCount % std::clamp (s.steps, 1, 16);
            transpose = (int) std::lround (s.transpose[k]);
            stepVel = s.velocity[k] / 127.0f;
        }
        ++stepCount;
        offAt = at + stepLen * std::clamp (s.gate, 0.02, 1.0) - 1.0e-6;
        if (stepVel == 0.0f) return;

        const auto octaves = std::clamp (s.octaves, 1, 4);
        if (s.mode == chord)
        {
            for (int i = 0; i < count; ++i) emit (notes[(size_t) i].note + transpose, stepVel >= 0.0f ? stepVel : notes[(size_t) i].vel, onNote);
            return;
        }
        const auto len = count * octaves;
        const auto pos = std::clamp (position (s.mode, len), 0, len - 1);
        ++index;
        const auto& h = notes[(size_t) (pos % count)];
        emit (h.note + 12 * (pos / count) + transpose, stepVel >= 0.0f ? stepVel : h.vel, onNote);
    }

    std::uint32_t nextRandom() noexcept { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

    /** Place de la note jouée dans la suite triée (len = notes × octaves), d'après le pas « index ». */
    int position (int mode, int len) noexcept
    {
        if (len <= 1) return 0;
        const auto k = index;
        // ordre « convergent » : 0, len-1, 1, len-2…
        auto convergeAt = [len] (int i) { return (i & 1) == 0 ? i / 2 : len - 1 - i / 2; };
        switch (mode)
        {
            case up: case played: return k % len;
            case down: case playedReverse: return len - 1 - k % len;
            case upDown: case downUp:
            {
                const auto period = 2 * len - 2;
                const auto m = k % period;
                const auto p = m < len ? m : period - m;
                return mode == downUp ? len - 1 - p : p;
            }
            case upDownIncl: case downUpIncl:
            {
                const auto m = k % (2 * len);
                const auto p = m < len ? m : 2 * len - 1 - m;
                return mode == downUpIncl ? len - 1 - p : p;
            }
            case random: return (int) (nextRandom() % (std::uint32_t) len);
            case converge: return convergeAt (k % len);
            case diverge: return convergeAt (len - 1 - k % len);
            case convergeDiverge:
            {
                const auto m = k % (2 * len);
                return m < len ? convergeAt (m) : convergeAt (2 * len - 1 - m);
            }
            case pedalLow:
            {
                const auto m = k % (2 * (len - 1));
                return (m & 1) == 0 ? 0 : 1 + m / 2;
            }
            case pedalHigh:
            {
                const auto m = k % (2 * (len - 1));
                return (m & 1) == 0 ? len - 1 : m / 2;
            }
            case skipUp: return (k / 2 + ((k & 1) ? 2 : 0)) % len;
            case stairsUp: return (k / 3 + k % 3) % len;
            case stairsDown: return len - 1 - (k / 3 + k % 3) % len;
            case shuffle:
            {
                // un nouveau tirage à chaque tour (et quand le nombre de notes change)
                const auto m = k % len;
                if (m == 0 || shuffleLen != len)
                {
                    shuffleLen = std::min (len, (int) order.size());
                    for (int i = 0; i < shuffleLen; ++i) order[(size_t) i] = (std::uint8_t) i;
                    for (int i = shuffleLen - 1; i > 0; --i) std::swap (order[(size_t) i], order[(size_t) (nextRandom() % (std::uint32_t) (i + 1))]);
                }
                return order[(size_t) (m % shuffleLen)];
            }
            case randomWalk:
            {
                if (k == 0) { walk = 0; return 0; }
                walk += (nextRandom() & 1) ? 1 : -1;
                if (walk < 0) walk = 1;
                if (walk >= len) walk = len - 2;
                return walk;
            }
            case repeatUp: return (k / 2) % len;
            default: return k % len;
        }
    }

    template <typename OnNote>
    void emit (int note, float vel, OnNote& onNote) noexcept
    {
        if (note < 0 || note > 127 || numSounding >= maxHeld) return;
        onNote (note, vel);
        sounding[(size_t) numSounding++] = note;
    }

    std::array<Held, maxHeld> held {}, latched {};
    std::array<int, maxHeld> sounding {};
    int numHeld = 0, numLatched = 0, numSounding = 0, physical = 0, index = 0, stepCount = 0;
    double nextPpq = -1.0, offAt = -1.0, clock = 0.0;
    bool started = false, immediate = false;
    std::uint32_t rng = 0x1234567u;
    std::array<std::uint8_t, maxHeld * 4> order {}; // tirage du mode « mélangé » (16 notes × 4 octaves)
    int shuffleLen = 0, walk = 0;
};

} // namespace grv::engine
