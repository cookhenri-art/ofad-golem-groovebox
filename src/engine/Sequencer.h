/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Séquenceur : 16 pistes × 16 lignes × 512 pas (32 mesures en double croches).
      - Piste batterie : une ligne par pad. Piste mélodique : les lignes portent les notes d'un
        accord (ligne 1 = note principale).
      - Chaque pas : vélocité, note, durée (1/8 de pas), micro-décalage (1/64 de pas).
      - 8 patterns par piste (clips), lancés seuls ou en rangée, calés sur la quantification de lancement.
      - Par pattern : longueur (1..512), vitesse (×1/8 … ×2). Par piste : coupure, solo.
      - Par ligne : sens de lecture (avant, arrière, aller-retour, aléatoire), coupure, solo.
      - Divisions 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T, 1/64 ; swing 50..75 % ; horloge DAW ou interne ;
        enregistrement en direct à quantification réglable.
      - Conditions de déclenchement (principe Elektron) : probabilité, A:B, FILL, PRÉ, VOISIN, 1ER
        et leurs contraires ; verrous de réglages par pas ; répétitions dans le pas ; flam (deux coups
        rapprochés, principe des boîtes à rythmes des années 80). Ces « extras » vivent dans un objet
        publié (édité par l'interface) ; les notes vivent dans des atomiques (enregistrement direct).
      - Fonctions de pas de la piste (principe des séquenceurs de basse et de batterie des années 80) :
        accent (renforce tous les sons du pas, dosé par piste) et glissé (la note tient jusqu'au pas
        suivant : un instrument monophonique glisse vers la note suivante, ou la lie si c'est la même).
      - Longueur par ligne (polymétrie) : une ligne peut boucler sur moins ou plus de pas que le pattern.

    Pur C++, sans allocation sur le fil audio.
*/

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace grv::engine
{

// L'ordre des valeurs est celui des projets enregistrés : les nouvelles divisions sont à la fin.
enum class Division : int { eighth = 0, eighthTriplet, sixteenth, sixteenthTriplet, thirtySecond,
                            quarter, thirtySecondTriplet, sixtyFourth };
constexpr int numDivisions = 8;

constexpr int stepsPerBar (Division d) noexcept
{
    switch (d)
    {
        case Division::quarter:             return 4;
        case Division::eighth:              return 8;
        case Division::eighthTriplet:       return 12;
        case Division::sixteenth:           return 16;
        case Division::sixteenthTriplet:    return 24;
        case Division::thirtySecond:        return 32;
        case Division::thirtySecondTriplet: return 48;
        case Division::sixtyFourth:         return 64;
    }
    return 16;
}

inline const char* divisionName (Division d) noexcept
{
    static constexpr const char* names[] { "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/4", "1/32T", "1/64" };
    return names[std::clamp ((int) d, 0, numDivisions - 1)];
}

/** Quantification du lancement des patterns : immédiat, 1 temps, 1, 2 ou 4 mesures. */
constexpr int numLaunchQuants = 5;
inline double launchQuantQuarters (int q) noexcept
{
    static constexpr double v[] { 0.0, 1.0, 4.0, 8.0, 16.0 };
    return v[std::clamp (q, 0, numLaunchQuants - 1)];
}

enum class Direction : int { forward = 0, reverse, pingPong, random };

/** Vitesses de piste (principe Elektron) : ×1/8, ×1/4, ×1/2, ×3/4, ×1, ×3/2, ×2. */
constexpr int numSpeeds = 7;
inline double speedFactor (int s) noexcept
{
    static constexpr double f[] { 0.125, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    return f[std::clamp (s, 0, numSpeeds - 1)];
}

inline int positionFor (Direction dir, long long k, int length, std::uint32_t& rng) noexcept
{
    if (length <= 1) return 0;
    switch (dir)
    {
        case Direction::forward: return (int) (k % length);
        case Direction::reverse: return length - 1 - (int) (k % length);
        case Direction::pingPong:
        {
            const auto period = 2LL * (length - 1);
            const auto m = (int) (k % period);
            return m < length ? m : (int) (period - m);
        }
        case Direction::random:
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            return (int) (rng % (std::uint32_t) length);
    }
    return 0;
}

enum class Condition : std::uint8_t { none = 0, ab, fill, notFill, pre, notPre, nei, notNei, first, notFirst };
constexpr int numConditions = 10;

struct StepExtra
{
    Condition cond = Condition::none;
    std::uint8_t a = 1, b = 2;     // A:B
    std::uint8_t prob = 100;       // probabilité en %
    std::uint8_t ratchet = 1;      // répétitions dans le pas (1 à 8)
    std::int8_t ratchetSlope = 0;  // pente de vélocité des répétitions (-100 à 100 %)
    std::uint8_t flam = 0;         // flam : écart entre le coup léger et le coup principal (ms, 0 = sans)
    bool direct = false;           // note sortie de l'arpégiateur (enregistrée) : rejouée sans repasser par lui
    std::vector<std::pair<int, float>> locks; // verrous : (adresse globale du réglage, valeur normalisée)
};

/** Conditions et verrous, publiés par l'interface. Clé : piste, pattern, ligne, pas. */
struct PatternExtras
{
    std::unordered_map<std::uint32_t, StepExtra> steps;
    static std::uint32_t key (int track, int pattern, int lane, int step) noexcept
    {
        return ((std::uint32_t) track << 24) | ((std::uint32_t) (pattern & 0xF) << 20) | ((std::uint32_t) (lane & 0xFF) << 12) | (std::uint32_t) (step & 0xFFF);
    }
    /** Pattern 1 (projets enregistrés avant les patterns, tests). */
    static std::uint32_t key (int track, int lane, int step) noexcept { return key (track, 0, lane, step); }
    static int keyTrack (std::uint32_t k) noexcept { return (int) (k >> 24); }
    static int keyPattern (std::uint32_t k) noexcept { return (int) ((k >> 20) & 0xF); }
    static int keyLane (std::uint32_t k) noexcept { return (int) ((k >> 12) & 0xFF); }
    static int keyStep (std::uint32_t k) noexcept { return (int) (k & 0xFFF); }
    const StepExtra* find (int track, int pattern, int lane, int step) const noexcept
    {
        const auto it = steps.find (key (track, pattern, lane, step));
        return it == steps.end() ? nullptr : &it->second;
    }
};

struct HostTime
{
    bool playing = false;
    double ppq = 0.0;
};

/** Fonctions de pas de la piste (un octet par pas du pattern). */
/** Fonctions d'un pas (toutes les lignes du pas) : accent ; glissé (la note tient jusqu'au pas
    suivant, et les verrous du pas glissent depuis les valeurs du moment) ; enregistrement (le
    pas lance l'enregistreur de la piste) ; sans déclenchement (le pas pose ses verrous sans jouer). */
enum StepFlag : std::uint8_t { stepAccent = 1, stepSlide = 2, stepRec = 4, stepTrigless = 8, stepAllFlags = 15 };
constexpr int maxFlamMs = 60;

struct SeqEvent
{
    int track, lane, offset;   // lane = -1 : fonction du pas seule (départ de l'enregistreur)
    float velocity;
    int note;
    double lengthSamples;
    const StepExtra* extra;
    std::uint8_t flags = 0;    // fonctions du pas (StepFlag)
    double stepSamples = 0.0;  // durée du pas (glissé des verrous)
    bool repeat = false;       // répétition (ou coup principal d'un flam) d'un pas déjà déclenché
    float accent = 0.0f;       // pas accentué : force de l'accent de la piste (0..1), 0 sinon
};

/**
    Chaque piste a 8 patterns (principe des clips MPC / Force). Un pattern porte ses notes, sa
    longueur et sa vitesse. L'interface édite le « pattern édité » de chaque piste ; le fil audio
    joue le « pattern joué ». Lancer un pattern le met en attente : il remplace le pattern joué au
    prochain multiple de la quantification de lancement (1 mesure par défaut) et repart de son début.
*/
class Sequencer
{
public:
    static constexpr int numTracks = 16;
    static constexpr int numLanes = 16;
    static constexpr int maxSteps = 512; // 32 mesures en double croches
    static constexpr int numPatterns = 8;
    static constexpr int stopPattern = numPatterns; // valeur d'attente : arrêter la piste
    static constexpr int maxRatchet = 8;
    static constexpr int maxLength8 = 1024; // durée d'une note : 128 pas au plus (en huitièmes de pas)

    Sequencer() : tracks (std::make_unique<std::array<Track, numTracks>>())
    {
        for (auto& t : *tracks)
        {
            for (auto& pat : t.pat)
            {
                for (auto& lane : pat.vel) for (auto& s : lane) s.store (0);
                for (auto& lane : pat.note) for (auto& s : lane) s.store (60);
                for (auto& lane : pat.len) for (auto& s : lane) s.store (8);
                for (auto& lane : pat.micro) for (auto& s : lane) s.store (0);
                for (auto& f : pat.flags) f.store (0);
                for (auto& l : pat.laneLen) l.store (0);
                pat.length.store (16); pat.speed.store (4);
            }
            t.accent.store (50);
            for (auto& d : t.direction) d.store (0);
            for (auto& m : t.laneMute) m.store (false);
            for (auto& m : t.laneSolo) m.store (false);
            for (auto& p : t.position) p.store (-1);
            t.mute.store (false); t.solo.store (false);
            t.playing.store (0); t.queued.store (-1); t.editing.store (0);
            t.skip.fill (-1); t.ratchetK.fill (-1);
            t.lastResult.fill (false);
        }
    }

    //==========================================================================
    // Fonctions de pas, longueur par ligne, accent

    /** Fonctions d'un pas du pattern (StepFlag). */
    void setStepFlagsP (int t, int p, int step, int flags) noexcept
    {
        if (! validT (t) || ! validP (p) || step < 0 || step >= maxSteps) return;
        pat (t, p).flags[(size_t) step].store ((std::uint8_t) (flags & stepAllFlags), std::memory_order_relaxed);
        bump();
    }
    int getStepFlagsP (int t, int p, int step) const noexcept
    {
        return validT (t) && validP (p) && step >= 0 && step < maxSteps ? pat (t, p).flags[(size_t) step].load (std::memory_order_relaxed) : 0;
    }
    void setStepFlags (int t, int step, int flags) noexcept { setStepFlagsP (t, getEditing (t), step, flags); }
    int getStepFlags (int t, int step) const noexcept { return getStepFlagsP (t, getEditing (t), step); }

    /** Longueur propre d'une ligne (0 = celle du pattern). */
    void setLaneLengthP (int t, int p, int lane, int n) noexcept
    {
        if (! validTL (t, lane) || ! validP (p)) return;
        pat (t, p).laneLen[(size_t) lane].store (std::clamp (n, 0, maxSteps), std::memory_order_relaxed);
        bump();
    }
    int getLaneLengthP (int t, int p, int lane) const noexcept
    {
        return validTL (t, lane) && validP (p) ? pat (t, p).laneLen[(size_t) lane].load (std::memory_order_relaxed) : 0;
    }
    void setLaneLength (int t, int lane, int n) noexcept { setLaneLengthP (t, getEditing (t), lane, n); }
    int getLaneLength (int t, int lane) const noexcept { return getLaneLengthP (t, getEditing (t), lane); }

    /** Force de l'accent de la piste (0..100 %) : la vélocité d'un pas accentué monte d'autant vers le maximum. */
    void setAccentAmount (int t, int percent) noexcept { if (validT (t)) { trk (t).accent.store (std::clamp (percent, 0, 100)); bump(); } }
    int getAccentAmount (int t) const noexcept { return validT (t) ? trk (t).accent.load() : 50; }

    //==========================================================================
    // Édition (tout fil). Sans numéro de pattern : le pattern édité de la piste.

    int getEditing (int t) const noexcept { return validT (t) ? trk (t).editing.load (std::memory_order_relaxed) : 0; }
    void setEditing (int t, int p) noexcept { if (validT (t) && validP (p)) { trk (t).editing.store (p); bump(); } }

    void setStep (int t, int lane, int step, int velocity) noexcept    { setStepP (t, getEditing (t), lane, step, velocity); }
    void setNote (int t, int lane, int step, int note) noexcept        { setNoteP (t, getEditing (t), lane, step, note); }
    void setLength8 (int t, int lane, int step, int eighths) noexcept  { setLength8P (t, getEditing (t), lane, step, eighths); }
    void setMicro (int t, int lane, int step, int micro) noexcept      { setMicroP (t, getEditing (t), lane, step, micro); }
    int getStep (int t, int lane, int step) const noexcept     { return getStepP (t, getEditing (t), lane, step); }
    int getNote (int t, int lane, int step) const noexcept     { return getNoteP (t, getEditing (t), lane, step); }
    int getLength8 (int t, int lane, int step) const noexcept  { return getLength8P (t, getEditing (t), lane, step); }
    int getMicro (int t, int lane, int step) const noexcept    { return getMicroP (t, getEditing (t), lane, step); }

    void setStepP (int t, int p, int lane, int step, int velocity) noexcept
    {
        if (! valid (t, p, lane, step)) return;
        pat (t, p).vel[(size_t) lane][(size_t) step].store ((std::uint8_t) std::clamp (velocity, 0, 127), std::memory_order_relaxed);
        bump();
    }
    void setNoteP (int t, int p, int lane, int step, int note) noexcept
    {
        if (! valid (t, p, lane, step)) return;
        pat (t, p).note[(size_t) lane][(size_t) step].store ((std::uint8_t) std::clamp (note, 0, 127), std::memory_order_relaxed);
        bump();
    }
    void setLength8P (int t, int p, int lane, int step, int eighths) noexcept
    {
        if (! valid (t, p, lane, step)) return;
        pat (t, p).len[(size_t) lane][(size_t) step].store ((std::uint16_t) std::clamp (eighths, 1, maxLength8), std::memory_order_relaxed);
        bump();
    }
    void setMicroP (int t, int p, int lane, int step, int micro) noexcept
    {
        if (! valid (t, p, lane, step)) return;
        pat (t, p).micro[(size_t) lane][(size_t) step].store ((std::int8_t) std::clamp (micro, -32, 32), std::memory_order_relaxed);
        bump();
    }
    int getStepP (int t, int p, int lane, int step) const noexcept    { return valid (t, p, lane, step) ? pat (t, p).vel[(size_t) lane][(size_t) step].load (std::memory_order_relaxed) : 0; }
    int getNoteP (int t, int p, int lane, int step) const noexcept    { return valid (t, p, lane, step) ? pat (t, p).note[(size_t) lane][(size_t) step].load (std::memory_order_relaxed) : 60; }
    int getLength8P (int t, int p, int lane, int step) const noexcept { return valid (t, p, lane, step) ? pat (t, p).len[(size_t) lane][(size_t) step].load (std::memory_order_relaxed) : 8; }
    int getMicroP (int t, int p, int lane, int step) const noexcept   { return valid (t, p, lane, step) ? pat (t, p).micro[(size_t) lane][(size_t) step].load (std::memory_order_relaxed) : 0; }

    /** Vrai si le pattern contient au moins une note. */
    bool hasNotes (int t, int p) const noexcept
    {
        if (! validT (t) || ! validP (p)) return false;
        for (auto& lane : pat (t, p).vel)
            for (auto& s : lane) if (s.load (std::memory_order_relaxed) > 0) return true;
        return false;
    }

    void clearLane (int t, int lane) noexcept { clearLaneP (t, getEditing (t), lane); }
    void clearLaneP (int t, int p, int lane) noexcept
    {
        if (! validTL (t, lane) || ! validP (p)) return;
        for (auto& s : pat (t, p).vel[(size_t) lane]) s.store (0, std::memory_order_relaxed);
        bump();
    }
    void clearTrack (int t) noexcept { clearPattern (t, getEditing (t)); }
    void clearPattern (int t, int p) noexcept
    {
        for (int l = 0; l < numLanes; ++l) clearLaneP (t, p, l);
        if (validT (t) && validP (p)) for (auto& f : pat (t, p).flags) f.store (0, std::memory_order_relaxed);
    }
    /** Remet toute la piste à zéro : notes, hauteurs, durées, décalages, longueurs, vitesses. */
    void resetTrack (int t) noexcept
    {
        if (! validT (t)) return;
        resetContent (t);
        trk (t).editing.store (0); trk (t).playing.store (0); trk (t).queued.store (-1);
        bump();
    }
    /** Comme resetTrack, sans toucher au pattern joué, en attente ou édité (retour en arrière en jeu). */
    void resetContent (int t) noexcept
    {
        if (! validT (t)) return;
        for (auto& pt : trk (t).pat)
        {
            for (auto& lane : pt.vel) for (auto& s : lane) s.store (0, std::memory_order_relaxed);
            for (auto& lane : pt.note) for (auto& s : lane) s.store (60, std::memory_order_relaxed);
            for (auto& lane : pt.len) for (auto& s : lane) s.store (8, std::memory_order_relaxed);
            for (auto& lane : pt.micro) for (auto& s : lane) s.store (0, std::memory_order_relaxed);
            for (auto& f : pt.flags) f.store (0, std::memory_order_relaxed);
            for (auto& l : pt.laneLen) l.store (0, std::memory_order_relaxed);
            pt.length.store (16); pt.speed.store (4); pt.division.store ((int) Division::sixteenth);
        }
        trk (t).accent.store (50);
        bump();
    }
    void clearAll() noexcept { for (int t = 0; t < numTracks; ++t) for (int p = 0; p < numPatterns; ++p) clearPattern (t, p); }
    /** v0.46 fix16 : décale l'écriture d'un pas vers la droite (dir > 0) ou vers la gauche (dir < 0),
        en boucle sur la longueur de chaque ligne : le pas qui sort d'un côté revient de l'autre.
        Notes, hauteurs, durées et décalages suivent ; lane = -1 : toutes les lignes, et aussi les
        fonctions des pas (accent, glissé, enregistrement, sans déclenchement). */
    void shiftPattern (int t, int p, int lane, int dir) noexcept
    {
        if (! validT (t) || ! validP (p) || dir == 0 || lane >= numLanes) return;
        auto& pt = pat (t, p);
        const int length = std::clamp (pt.length.load (std::memory_order_relaxed), 1, maxSteps);
        const auto rotate = [dir] (auto& cells, int n)
        {
            if (n < 2) return;
            using V = decltype (cells[0].load());
            std::array<V, maxSteps> tmp {};
            const int d = ((dir % n) + n) % n;
            for (int i = 0; i < n; ++i) tmp[(size_t) i] = cells[(size_t) i].load (std::memory_order_relaxed);
            for (int i = 0; i < n; ++i) cells[(size_t) ((i + d) % n)].store (tmp[(size_t) i], std::memory_order_relaxed);
        };
        for (int l = (lane < 0 ? 0 : lane); l < (lane < 0 ? numLanes : lane + 1); ++l)
        {
            const auto own = pt.laneLen[(size_t) l].load (std::memory_order_relaxed);
            const int n = own > 0 ? std::min (own, maxSteps) : length;
            rotate (pt.vel[(size_t) l], n); rotate (pt.note[(size_t) l], n);
            rotate (pt.len[(size_t) l], n); rotate (pt.micro[(size_t) l], n);
        }
        if (lane < 0) rotate (pt.flags, length);
        bump();
    }

    /** Copie un pattern entier (notes, longueur, vitesse) d'un emplacement à un autre de la même piste. */
    /** v0.26 : pistes déplacées (fil de l'interface) — la piste t passe en newIndex[t] avec ses
        patterns, ses lignes coupées / en solo, sa coupure, son solo, son accent et les patterns
        joué, en attente et édité. La position de lecture suit à part (permuteRuntime, fil audio). */
    void permuteData (const std::array<int, numTracks>& newIndex) noexcept
    {
        // copie en valeurs simples puis écriture : chaque pas est relu une seule fois
        struct Plain
        {
            std::array<std::array<std::array<std::uint8_t, maxSteps>, numLanes>, numPatterns> vel, note;
            std::array<std::array<std::array<std::uint16_t, maxSteps>, numLanes>, numPatterns> len;
            std::array<std::array<std::array<std::int8_t, maxSteps>, numLanes>, numPatterns> micro;
            std::array<std::array<std::uint8_t, maxSteps>, numPatterns> flags;
            std::array<std::array<int, numLanes>, numPatterns> laneLen;
            std::array<int, numPatterns> length, speed, division;
            std::array<bool, numLanes> laneMute, laneSolo;
            bool mute, solo;
            int playing, queued, editing, accent;
        };
        auto plain = std::make_unique<std::array<Plain, numTracks>>(); // ~5 Mo, fil de l'interface
        for (int t = 0; t < numTracks; ++t)
        {
            auto& d = (*plain)[(size_t) newIndex[(size_t) t]];
            auto& tr = trk (t);
            for (int p = 0; p < numPatterns; ++p)
            {
                auto& a = tr.pat[(size_t) p];
                for (int l = 0; l < numLanes; ++l)
                {
                    for (int st = 0; st < maxSteps; ++st)
                    {
                        d.vel[(size_t) p][(size_t) l][(size_t) st] = a.vel[(size_t) l][(size_t) st].load();
                        d.note[(size_t) p][(size_t) l][(size_t) st] = a.note[(size_t) l][(size_t) st].load();
                        d.len[(size_t) p][(size_t) l][(size_t) st] = a.len[(size_t) l][(size_t) st].load();
                        d.micro[(size_t) p][(size_t) l][(size_t) st] = a.micro[(size_t) l][(size_t) st].load();
                    }
                    d.laneLen[(size_t) p][(size_t) l] = a.laneLen[(size_t) l].load();
                }
                for (int st = 0; st < maxSteps; ++st) d.flags[(size_t) p][(size_t) st] = a.flags[(size_t) st].load();
                d.length[(size_t) p] = a.length.load(); d.speed[(size_t) p] = a.speed.load(); d.division[(size_t) p] = a.division.load();
            }
            for (int l = 0; l < numLanes; ++l) { d.laneMute[(size_t) l] = tr.laneMute[(size_t) l].load(); d.laneSolo[(size_t) l] = tr.laneSolo[(size_t) l].load(); }
            d.mute = tr.mute.load(); d.solo = tr.solo.load();
            d.playing = tr.playing.load(); d.queued = tr.queued.load(); d.editing = tr.editing.load(); d.accent = tr.accent.load();
        }
        for (int t = 0; t < numTracks; ++t)
        {
            const auto& d = (*plain)[(size_t) t];
            auto& tr = trk (t);
            for (int p = 0; p < numPatterns; ++p)
            {
                auto& a = tr.pat[(size_t) p];
                for (int l = 0; l < numLanes; ++l)
                {
                    for (int st = 0; st < maxSteps; ++st)
                    {
                        a.vel[(size_t) l][(size_t) st].store (d.vel[(size_t) p][(size_t) l][(size_t) st]);
                        a.note[(size_t) l][(size_t) st].store (d.note[(size_t) p][(size_t) l][(size_t) st]);
                        a.len[(size_t) l][(size_t) st].store (d.len[(size_t) p][(size_t) l][(size_t) st]);
                        a.micro[(size_t) l][(size_t) st].store (d.micro[(size_t) p][(size_t) l][(size_t) st]);
                    }
                    a.laneLen[(size_t) l].store (d.laneLen[(size_t) p][(size_t) l]);
                }
                for (int st = 0; st < maxSteps; ++st) a.flags[(size_t) st].store (d.flags[(size_t) p][(size_t) st]);
                a.length.store (d.length[(size_t) p]); a.speed.store (d.speed[(size_t) p]); a.division.store (d.division[(size_t) p]);
            }
            for (int l = 0; l < numLanes; ++l) { tr.laneMute[(size_t) l].store (d.laneMute[(size_t) l]); tr.laneSolo[(size_t) l].store (d.laneSolo[(size_t) l]); }
            tr.mute.store (d.mute); tr.solo.store (d.solo);
            tr.playing.store (d.playing); tr.queued.store (d.queued); tr.editing.store (d.editing); tr.accent.store (d.accent);
        }
        bump();
    }

    /** v0.26 (fil audio) : la position de lecture de chaque piste (pas en cours, sens, répétitions,
        début du pattern, bascule attendue) suit la piste déplacée — la boucle ne repart pas. */
    void permuteRuntime (const std::array<int, numTracks>& newIndex) noexcept
    {
        std::array<bool, numTracks> done {};
        for (int start = 0; start < numTracks; ++start)
        {
            // cycles de la permutation : t → newIndex[t]
            int t = start;
            while (! done[(size_t) t] && newIndex[(size_t) t] != start)
            {
                done[(size_t) t] = true;
                swapRuntime (start, newIndex[(size_t) t]);
                t = newIndex[(size_t) t];
            }
            done[(size_t) t] = true;
        }
    }

    void copyPattern (int t, int from, int to) noexcept { copyPattern (t, from, t, to); }
    /** Copie vers une autre piste (même motif sur un autre instrument). */
    void copyPattern (int st, int from, int t, int to) noexcept
    {
        if (! validT (st) || ! validT (t) || ! validP (from) || ! validP (to) || (st == t && from == to)) return;
        auto& a = pat (st, from); auto& b = pat (t, to);
        for (int l = 0; l < numLanes; ++l)
            for (int s = 0; s < maxSteps; ++s)
            {
                b.vel[(size_t) l][(size_t) s].store (a.vel[(size_t) l][(size_t) s].load());
                b.note[(size_t) l][(size_t) s].store (a.note[(size_t) l][(size_t) s].load());
                b.len[(size_t) l][(size_t) s].store (a.len[(size_t) l][(size_t) s].load());
                b.micro[(size_t) l][(size_t) s].store (a.micro[(size_t) l][(size_t) s].load());
            }
        for (int s = 0; s < maxSteps; ++s) b.flags[(size_t) s].store (a.flags[(size_t) s].load());
        for (int l = 0; l < numLanes; ++l) b.laneLen[(size_t) l].store (a.laneLen[(size_t) l].load());
        b.length.store (a.length.load()); b.speed.store (a.speed.load()); b.division.store (a.division.load());
        bump();
    }

    void setTrackLength (int t, int n) noexcept { setPatternLength (t, getEditing (t), n); }
    int getTrackLength (int t) const noexcept { return getPatternLength (t, getEditing (t)); }
    void setTrackSpeed (int t, int s) noexcept { setPatternSpeed (t, getEditing (t), s); }
    int getTrackSpeed (int t) const noexcept { return getPatternSpeed (t, getEditing (t)); }
    void setPatternLength (int t, int p, int n) noexcept { if (validT (t) && validP (p)) { pat (t, p).length.store (std::clamp (n, 1, maxSteps)); bump(); } }
    int getPatternLength (int t, int p) const noexcept { return validT (t) && validP (p) ? pat (t, p).length.load() : 16; }
    void setPatternSpeed (int t, int p, int s) noexcept { if (validT (t) && validP (p)) { pat (t, p).speed.store (std::clamp (s, 0, numSpeeds - 1)); bump(); } }
    int getPatternSpeed (int t, int p) const noexcept { return validT (t) && validP (p) ? pat (t, p).speed.load() : 4; }

    void setTrackMute (int t, bool m) noexcept { if (validT (t)) { trk (t).mute.store (m); bump(); } }
    /** v0.43 : l'instrument coupe lui-même le son de ses lignes (Pieuvre 8) : une ligne coupée continue
        de jouer ses notes (seul le son est coupé), pour qu'une note longue revienne quand on rétablit. */
    void setLaneAudioMute (int t, bool on) noexcept { if (validT (t)) trk (t).laneAudioMute.store (on, std::memory_order_relaxed); }
    bool getTrackMute (int t) const noexcept { return validT (t) && trk (t).mute.load(); }
    void setTrackSolo (int t, bool s) noexcept { if (validT (t)) { trk (t).solo.store (s); bump(); } }
    bool getTrackSolo (int t) const noexcept { return validT (t) && trk (t).solo.load(); }
    /** v0.25 : la piste t est la voix de la piste t + 1 (vocodeur) : elle joue quand t + 1 est en solo. */
    void setSoloFollow (int t, bool f) noexcept { if (validT (t)) trk (t).soloFollow.store (f, std::memory_order_relaxed); }

    void setDirection (int t, int lane, Direction d) noexcept { if (validTL (t, lane)) { trk (t).direction[(size_t) lane].store ((int) d); bump(); } }
    Direction getDirection (int t, int lane) const noexcept { return validTL (t, lane) ? (Direction) trk (t).direction[(size_t) lane].load() : Direction::forward; }
    void setLaneMute (int t, int lane, bool m) noexcept { if (validTL (t, lane)) { trk (t).laneMute[(size_t) lane].store (m); bump(); } }
    bool getLaneMute (int t, int lane) const noexcept { return validTL (t, lane) && trk (t).laneMute[(size_t) lane].load(); }
    void setLaneSolo (int t, int lane, bool s) noexcept { if (validTL (t, lane)) { trk (t).laneSolo[(size_t) lane].store (s); bump(); } }
    bool getLaneSolo (int t, int lane) const noexcept { return validTL (t, lane) && trk (t).laneSolo[(size_t) lane].load(); }

    /** Division de TOUS les patterns de toutes les pistes (projets d'avant la division par pattern). */
    void setDivision (Division d) noexcept
    {
        const auto v = std::clamp ((int) d, 0, numDivisions - 1);
        division.store (v);
        for (int t = 0; t < numTracks; ++t) for (auto& pt : trk (t).pat) pt.division.store (v);
        bump();
    }
    Division getDivision() const noexcept { return (Division) division.load(); }
    /** Division d'un pattern d'une piste : seule la piste et le pattern écrits en changent. */
    void setPatternDivision (int t, int p, Division d) noexcept
    {
        if (validT (t) && validP (p)) { pat (t, p).division.store (std::clamp ((int) d, 0, numDivisions - 1)); bump(); }
    }
    Division getPatternDivision (int t, int p) const noexcept
    {
        return validT (t) && validP (p) ? (Division) pat (t, p).division.load (std::memory_order_relaxed) : Division::sixteenth;
    }
    Division getTrackDivision (int t) const noexcept { return getPatternDivision (t, getEditing (t)); }

    /** Quantification de l'enregistrement en direct : 100 % = sur le pas, 0 % = jeu conservé (décalage fin). */
    void setRecordQuantize (int percent) noexcept { recQuant.store (std::clamp (percent, 0, 100)); bump(); }
    int getRecordQuantize() const noexcept { return recQuant.load(); }

    //==========================================================================
    // Lancement des patterns (tout fil)

    /** Met un pattern en attente sur une piste (stopPattern = arrêter la piste). */
    void launch (int t, int p) noexcept { if (validT (t) && (validP (p) || p == stopPattern)) { trk (t).queued.store (p); bump(); } }
    /** Rangée de lancement (principe Force) : le même pattern sur toutes les pistes. v0.23 : une
        piste dont la case de la rangée est vide (aucune note) s'arrête, comme avec son bouton
        d'arrêt (boucles comprises) — elle ne continue plus son ancien pattern. */
    void launchRow (int p) noexcept { for (int t = 0; t < numTracks; ++t) launch (t, validP (p) && ! hasNotes (t, p) ? stopPattern : p); }
    int getPlaying (int t) const noexcept { return validT (t) ? trk (t).playing.load (std::memory_order_relaxed) : 0; }
    int getQueued (int t) const noexcept { return validT (t) ? trk (t).queued.load (std::memory_order_relaxed) : -1; }
    /** Remet directement un pattern joué (chargement d'un projet, séquenceur arrêté). */
    void setPlaying (int t, int p) noexcept { if (validT (t) && (validP (p) || p == -1)) { trk (t).playing.store (p); trk (t).queued.store (-1); bump(); } }
    void setLaunchQuant (int q) noexcept { launchQuant.store (std::clamp (q, 0, numLaunchQuants - 1)); bump(); }
    /** Coupure et solo : faux (par défaut) = le son s'arrête tout de suite ; vrai = seules les notes
        s'arrêtent, les sons tenus et les traînes (écho, réverbération) finissent de sonner. */
    void setMuteTails (bool on) noexcept { muteTails.store (on); bump(); }
    bool getMuteTails() const noexcept { return muteTails.load (std::memory_order_relaxed); }
    int getLaunchQuant() const noexcept { return launchQuant.load(); }

    std::uint32_t getVersion() const noexcept { return version.load (std::memory_order_acquire); }
    void bump() noexcept { version.fetch_add (1, std::memory_order_release); }

    //==========================================================================
    // Transport (tout fil)

    void requestPlay() noexcept { startRequested.store (true); stopRequested.store (false); }
    void requestStop() noexcept { stopRequested.store (true); startRequested.store (false); }
    void setRecording (bool r) noexcept { recording.store (r); }
    bool isRecording() const noexcept { return recording.load(); }
    bool isRunning() const noexcept { return running.load (std::memory_order_relaxed); }
    void setFill (bool f) noexcept { fill.store (f); }
    bool getFill() const noexcept { return fill.load(); }

    int getPosition (int t, int lane) const noexcept { return validTL (t, lane) ? trk (t).position[(size_t) lane].load (std::memory_order_relaxed) : -1; }
    /** Position en noires du début du dernier bloc (pour l'affichage mesure.temps). */
    double getPpq() const noexcept { return ppqDisplay.load (std::memory_order_relaxed); }

    //==========================================================================
    // Fil audio

    void beginBlock (int numSamples, double sampleRate, double bpm, double swingPercent, const HostTime& host) noexcept
    {
        blockSamples = numSamples;
        sampleRateNow = std::max (1.0, sampleRate);
        ppqPerSample = std::max (1.0, bpm) / 60.0 / std::max (1.0, sampleRate);
        swing = std::clamp (swingPercent, 50.0, 75.0) / 100.0;

        if (startRequested.exchange (false))
        {
            internalRunning = true; internalPpq = 0.0;
            restartAll (0.0);
        }
        if (stopRequested.exchange (false)) internalRunning = false;

        if (host.playing)
        {
            if (! followingHost) restartAll (0.0);
            else if (std::abs (host.ppq - lastBlockEndPpq) > 1.0e-3)
                hostJumped (host.ppq); // boucle ou saut de la tête de lecture du logiciel hôte
            blockStartPpq = host.ppq;
            followingHost = true;
            running.store (true, std::memory_order_relaxed);
        }
        else
        {
            if (followingHost) { followingHost = false; internalRunning = false; }
            blockStartPpq = internalPpq;
            running.store (internalRunning, std::memory_order_relaxed);
        }

        // Séquenceur arrêté : un pattern lancé remplace tout de suite le pattern joué
        if (! running.load (std::memory_order_relaxed))
            for (auto& t : *tracks)
            {
                const auto q = t.queued.load (std::memory_order_relaxed);
                if (q < 0) continue;
                commitSwitch (t, q, 0.0);
            }

        ppqDisplay.store (running.load() ? blockStartPpq : -1.0, std::memory_order_relaxed);
        if (! running.load (std::memory_order_relaxed))
            for (auto& t : *tracks) for (auto& p : t.position) p.store (-1, std::memory_order_relaxed);
    }

    double ppqAt (int offset) const noexcept { return blockStartPpq + ppqPerSample * (double) offset; }
    /** Fil audio : position dans le pattern joué par la piste, en pas (avec la fraction), dans
        sa boucle ; -1 à l'arrêt ou sans pattern. */
    double stepPhase (int ti, double ppq) const noexcept
    {
        if (! validT (ti) || ! running.load (std::memory_order_relaxed)) return -1.0;
        const auto& t = trk (ti);
        const auto p = t.playing.load (std::memory_order_relaxed);
        if (! validP (p)) return -1.0;
        const auto& pt = pat (ti, p);
        const auto stepLen = 4.0 / (double) stepsPerBar ((Division) pt.division.load (std::memory_order_relaxed)) / speedFactor (pt.speed.load (std::memory_order_relaxed));
        const auto len = (double) std::max (1, pt.length.load (std::memory_order_relaxed));
        const auto s = std::max (0.0, (ppq - t.originPpq) / stepLen);
        return s - std::floor (s / len) * len;
    }
    /** v0.43 (fil de l'interface) : boucle la plus longue parmi les patterns joués : durée en noires et
        avancée (0 à 1) à la position ppq ; durée 0 si rien ne joue. */
    double longestLoop (double ppq, double& phase) const noexcept
    {
        phase = 0.0;
        if (! running.load (std::memory_order_relaxed)) return 0.0;
        double best = 0.0;
        for (int ti = 0; ti < numTracks; ++ti)
        {
            const auto& t = trk (ti);
            const auto p = t.playing.load (std::memory_order_relaxed);
            if (! validP (p)) continue;
            const auto& pt = pat (ti, p);
            const auto stepLen = 4.0 / (double) stepsPerBar ((Division) pt.division.load (std::memory_order_relaxed)) / speedFactor (pt.speed.load (std::memory_order_relaxed));
            const auto dur = (double) std::max (1, pt.length.load (std::memory_order_relaxed)) * stepLen;
            if (dur <= best + 1.0e-9) continue;
            best = dur;
            const auto x = std::max (0.0, ppq - t.originPpq) / dur;
            phase = x - std::floor (x);
        }
        return best;
    }
    double getBlockStartPpq() const noexcept { return blockStartPpq; }
    double getBlockEndPpq() const noexcept { return blockStartPpq + ppqPerSample * (double) blockSamples; }
    /** Fil audio, entre beginBlock et collect : bascule imposée (mode morceau) à l'instant donné du bloc. */
    void forceSwitch (int t, int p, double atPpq) noexcept
    {
        if (! validT (t) || ! (validP (p) || p == stopPattern)) return;
        trk (t).forcedFor = p; trk (t).forcedAt = atPpq;
    }
    double getPpqPerSample() const noexcept { return ppqPerSample; }
    bool isPlayingInBlock() const noexcept { return running.load (std::memory_order_relaxed); }

    /** Appelle onEvent (SeqEvent) pour chaque pas déclenché dans le bloc, piste par piste. */
    template <typename OnEvent>
    void collect (const PatternExtras* extras, OnEvent&& onEvent) noexcept
    {
        if (! running.load (std::memory_order_relaxed)) return;

        const auto p0 = blockStartPpq;
        const auto p1 = p0 + ppqPerSample * (double) blockSamples;

        bool anyTrackSolo = false;
        for (auto& t : *tracks) anyTrackSolo = anyTrackSolo || t.solo.load (std::memory_order_relaxed);

        for (int ti = 0; ti < numTracks; ++ti)
        {
            auto& t = (*tracks)[(size_t) ti];

            // Pattern en attente : instant de bascule calé sur la quantification de lancement
            const auto q = t.queued.load (std::memory_order_relaxed);
            if (q >= 0 && q != t.switchFor)
            {
                const auto Q = launchQuantQuarters (launchQuant.load (std::memory_order_relaxed));
                t.switchFor = q;
                t.switchAt = Q <= 0.0 ? p0 : std::ceil (p0 / Q - 1.0e-9) * Q;
            }
            else if (q < 0) t.switchFor = -1;
            // mode morceau : la rangée suivante bascule exactement à sa mesure (prime sur l'attente)
            if (t.forcedFor >= 0)
            {
                t.switchFor = t.forcedFor; t.switchAt = std::max (p0, t.forcedAt);
                t.forcedFor = -1;
                int expected = t.queued.load (std::memory_order_relaxed);
                if (expected >= 0) t.queued.compare_exchange_strong (expected, -1);
            }

            const bool switching = t.switchFor >= 0 && t.switchAt < p1;
            const auto cut = switching ? std::max (p0, t.switchAt) : p1;

            if (cut > p0) collectSegment (ti, t, t.playing.load (std::memory_order_relaxed), t.originPpq, p0, cut, extras, anyTrackSolo, onEvent);
            if (switching)
            {
                const auto target = t.switchFor;
                commitSwitch (t, target, std::max (p0, t.switchAt));
                if (cut < p1) collectSegment (ti, t, t.playing.load (std::memory_order_relaxed), t.originPpq, cut, p1, extras, anyTrackSolo, onEvent);
            }
        }
    }

    /** Enregistrement en direct. Renvoie la ligne utilisée (-1 si rien n'est enregistré). */
    int recordHit (int ti, int lane, int note, float velocity, int offset, bool melodic, int* outPattern = nullptr, int* outPos = nullptr) noexcept
    {
        if (! validT (ti) || ! recording.load() || ! running.load (std::memory_order_relaxed)) return -1;
        auto& t = trk (ti);
        const auto p = t.playing.load();
        if (! validP (p)) return -1;
        auto& pt = pat (ti, p);
        const auto stepLen = 4.0 / (double) stepsPerBar ((Division) pt.division.load()) / speedFactor (pt.speed.load());
        const auto s = (ppqAt (offset) - t.originPpq) / stepLen;
        const auto k = std::max (0LL, (long long) std::llround (s));
        const auto patLen = pt.length.load();
        std::uint32_t dummy = 1;

        if (melodic)
        {
            // Première ligne libre à ce pas (accords), sinon la ligne 1
            lane = 0;
            for (int l = 0; l < numLanes; ++l)
            {
                const auto pp = positionFor (Direction::forward, k, laneLength (pt, l, patLen), dummy);
                if (pt.vel[(size_t) l][(size_t) pp].load() == 0 || pt.note[(size_t) l][(size_t) pp].load() == note) { lane = l; break; }
            }
        }
        if (! validTL (ti, lane)) return -1;
        auto dir = (Direction) t.direction[(size_t) lane].load();
        if (dir == Direction::random) dir = Direction::forward;
        const auto pos = positionFor (dir, k, laneLength (pt, lane, patLen), dummy);
        if (melodic) setNoteP (ti, p, lane, pos, note);
        // Quantification partielle : l'écart au pas est gardé en décalage fin (1/64 de pas)
        const auto keep = 1.0 - (double) recQuant.load() / 100.0;
        setMicroP (ti, p, lane, pos, (int) std::lround ((s - (double) k) * 64.0 * keep));
        setStepP (ti, p, lane, pos, std::max (1, (int) std::lround (velocity * 127.0f)));
        if ((double) k > s) t.skip[(size_t) lane] = k;
        // note tenue : sa durée s'écrira au relâchement (recordRelease)
        OpenRec* slot = nullptr;
        for (auto& o : openRecs) if (o.track == ti && o.note == note) slot = &o;
        if (slot == nullptr) for (auto& o : openRecs) if (o.track < 0) { slot = &o; break; }
        if (slot == nullptr) slot = &openRecs[(size_t) (openNext++ % openRecs.size())];
        *slot = { ti, note, lane, p, pos, ppqAt (offset), stepLen };
        if (outPattern != nullptr) *outPattern = p;
        if (outPos != nullptr) *outPos = pos;
        return lane;
    }

    /** Enregistrement en direct : la note relâchée prend la durée tenue (en huitièmes de pas, 1 à 128 pas). */
    void recordRelease (int ti, int note, int offset) noexcept
    {
        for (auto& o : openRecs)
        {
            if (o.track != ti || o.note != note) continue;
            if (running.load (std::memory_order_relaxed) && o.stepLen > 0.0)
            {
                const auto held = (ppqAt (offset) - o.start) / o.stepLen;
                const auto len8 = std::clamp ((int) std::lround (held * 8.0), 1, maxLength8);
                setLength8P (o.track, o.pattern, o.lane, o.pos, len8);
            }
            o.track = -1;
        }
    }

    void endBlock() noexcept
    {
        lastBlockEndPpq = blockStartPpq + ppqPerSample * (double) blockSamples;
        if (! followingHost && internalRunning)
            internalPpq += ppqPerSample * (double) blockSamples;
    }

    /** Évaluation d'une condition (publique pour les tests). */
    bool evaluate (int ti, int lane, const StepExtra& e, long long loop) noexcept
    {
        auto& t = trk (ti);
        bool result = true;
        bool recordsResult = true;
        switch (e.cond)
        {
            case Condition::none: recordsResult = e.prob < 100; break;
            case Condition::ab: result = e.b > 0 && (loop % e.b) == (long long) std::max (0, e.a - 1); break;
            case Condition::fill: result = fill.load(); break;
            case Condition::notFill: result = ! fill.load(); break;
            case Condition::pre: result = t.lastResult[(size_t) lane]; recordsResult = false; break;
            case Condition::notPre: result = ! t.lastResult[(size_t) lane]; recordsResult = false; break;
            case Condition::nei: result = neighbourResult (ti, lane); recordsResult = false; break;
            case Condition::notNei: result = ! neighbourResult (ti, lane); recordsResult = false; break;
            case Condition::first: result = loop == 0; break;
            case Condition::notFirst: result = loop != 0; break;
        }
        if (result && e.prob < 100)
        {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            result = (int) (rng % 100u) < (int) e.prob;
        }
        if (recordsResult) t.lastResult[(size_t) lane] = result;
        return result;
    }

private:
    struct PatternData
    {
        std::array<std::array<std::atomic<std::uint8_t>, maxSteps>, numLanes> vel, note;
        std::array<std::array<std::atomic<std::uint16_t>, maxSteps>, numLanes> len; // en huitièmes de pas (jusqu'à 128 pas)
        std::array<std::array<std::atomic<std::int8_t>, maxSteps>, numLanes> micro;
        std::array<std::atomic<std::uint8_t>, maxSteps> flags;   // fonctions de pas (accent, glissé)
        std::array<std::atomic<int>, numLanes> laneLen;          // longueur par ligne (0 = celle du pattern)
        std::atomic<int> length { 16 }, speed { 4 };
        std::atomic<int> division { (int) Division::sixteenth }; // division propre au pattern (binaire, ternaire)
    };

    static int laneLength (const PatternData& pt, int lane, int patternLength) noexcept
    {
        const auto l = pt.laneLen[(size_t) lane].load (std::memory_order_relaxed);
        return l > 0 ? l : patternLength;
    }

    // notes enregistrées en direct, encore tenues (fil audio)
    struct OpenRec { int track = -1, note = 0, lane = 0, pattern = 0, pos = 0; double start = 0.0, stepLen = 0.0; };
    std::array<OpenRec, 32> openRecs {};
    unsigned openNext = 0;

    struct Track
    {
        std::array<PatternData, numPatterns> pat;
        std::array<std::atomic<int>, numLanes> direction, position;
        std::array<std::atomic<bool>, numLanes> laneMute, laneSolo;
        std::atomic<bool> mute { false }, solo { false }, soloFollow { false };
        std::atomic<bool> laneAudioMute { false }; // v0.43 : coupure des lignes faite par l'instrument (son seul)
        std::atomic<int> playing { 0 }, queued { -1 }, editing { 0 };
        std::atomic<int> accent { 50 };
        // Fil audio
        std::array<long long, numLanes> skip {};
        std::array<bool, numLanes> lastResult {};
        std::array<long long, numLanes> ratchetK {};  // pas dont les répétitions restent à jouer
        std::array<int, numLanes> ratchetDone {}, ratchetPos {}, ratchetReps {}; // répétitions jouées, position du pas, nombre de coups
        std::array<float, numLanes> ratchetVel {};             // vélocité du pas (accent compris), 0..1
        std::array<bool, numLanes> ratchetFlam {};             // vrai : flam (le 2e coup est le coup principal)
        std::array<float, numLanes> ratchetAcc {};             // accent du pas (gardé par ses répétitions)
        std::array<double, numLanes> ratchetWhen {}, ratchetGap {}; // premier coup et écart entre coups (noires)
        double originPpq = 0.0;   // début du pattern joué (en noires)
        double switchAt = -1.0;   // instant de bascule vers le pattern en attente
        int switchFor = -1;       // pattern pour lequel switchAt a été calculé
        int forcedFor = -1;       // mode morceau : bascule imposée dans ce bloc (pattern ou stopPattern), -1 sinon
        double forcedAt = 0.0;
    };

    Track& trk (int t) noexcept { return (*tracks)[(size_t) t]; }
    const Track& trk (int t) const noexcept { return (*tracks)[(size_t) t]; }
    PatternData& pat (int t, int p) noexcept { return trk (t).pat[(size_t) p]; }
    const PatternData& pat (int t, int p) const noexcept { return trk (t).pat[(size_t) p]; }

    /** Échange l'état de lecture de deux pistes (fil audio). */
    void swapRuntime (int a, int b) noexcept
    {
        if (a == b) return;
        auto& x = trk (a); auto& y = trk (b);
        for (int l = 0; l < numLanes; ++l)
        {
            const auto d = x.direction[(size_t) l].load(); x.direction[(size_t) l].store (y.direction[(size_t) l].load()); y.direction[(size_t) l].store (d);
            const auto q = x.position[(size_t) l].load(); x.position[(size_t) l].store (y.position[(size_t) l].load()); y.position[(size_t) l].store (q);
        }
        std::swap (x.skip, y.skip); std::swap (x.lastResult, y.lastResult);
        std::swap (x.ratchetK, y.ratchetK); std::swap (x.ratchetDone, y.ratchetDone); std::swap (x.ratchetPos, y.ratchetPos);
        std::swap (x.ratchetReps, y.ratchetReps); std::swap (x.ratchetVel, y.ratchetVel); std::swap (x.ratchetFlam, y.ratchetFlam);
        std::swap (x.ratchetAcc, y.ratchetAcc); std::swap (x.ratchetWhen, y.ratchetWhen); std::swap (x.ratchetGap, y.ratchetGap);
        std::swap (x.originPpq, y.originPpq); std::swap (x.switchAt, y.switchAt); std::swap (x.switchFor, y.switchFor);
        std::swap (x.forcedFor, y.forcedFor); std::swap (x.forcedAt, y.forcedAt);
        for (auto& o : openRecs) { if (o.track == a) o.track = b; else if (o.track == b) o.track = a; }
    }

    void restartAll (double origin) noexcept
    {
        for (auto& t : *tracks)
        {
            t.skip.fill (-1); t.lastResult.fill (false); t.ratchetK.fill (-1);
            t.originPpq = origin; t.switchFor = -1; t.switchAt = -1.0;
        }
    }

    /** Saut de la tête de lecture : chaque pattern repart au début de la mesure où l'on arrive,
        et un pattern en attente part tout de suite (une boucle de l'hôte plus courte que la
        quantification ne doit pas le bloquer). */
    void hostJumped (double ppq) noexcept
    {
        const auto bar = std::floor (ppq / 4.0 + 1.0e-9) * 4.0;
        for (auto& t : *tracks)
        {
            t.originPpq = bar;
            t.skip.fill (-1); t.ratchetK.fill (-1);
            const auto q = t.queued.load (std::memory_order_relaxed);
            if (q >= 0) { t.switchFor = q; t.switchAt = ppq; }
            else { t.switchFor = -1; t.switchAt = -1.0; }
        }
    }

    void commitSwitch (Track& t, int target, double at) noexcept
    {
        t.playing.store (target == stopPattern ? -1 : target, std::memory_order_relaxed);
        int expected = target;
        t.queued.compare_exchange_strong (expected, -1);
        t.originPpq = at; t.switchFor = -1; t.switchAt = -1.0;
        t.skip.fill (-1); t.lastResult.fill (false); t.ratchetK.fill (-1);
        for (auto& p : t.position) p.store (-1, std::memory_order_relaxed);
        bump();
    }

    template <typename OnEvent>
    void collectSegment (int ti, Track& t, int p, double origin, double from, double to,
                         const PatternExtras* extras, bool anyTrackSolo, OnEvent& onEvent) noexcept
    {
        if (! validP (p)) return;
        auto& pt = t.pat[(size_t) p];
        const auto p0 = blockStartPpq;
        const auto stepLen = 4.0 / (double) stepsPerBar ((Division) pt.division.load (std::memory_order_relaxed)) / speedFactor (pt.speed.load (std::memory_order_relaxed));
        const auto swingOffset = (swing - 0.5) * 2.0 * stepLen;
        const auto len = pt.length.load (std::memory_order_relaxed);
        const bool followed = t.soloFollow.load (std::memory_order_relaxed) && ti + 1 < numTracks && (*tracks)[(size_t) (ti + 1)].solo.load (std::memory_order_relaxed);
        const bool trackSilent = t.mute.load (std::memory_order_relaxed) || (anyTrackSolo && ! t.solo.load (std::memory_order_relaxed) && ! followed);
        /* v0.43 : la coupure (et le solo) coupe le SON, pas le jeu des notes : les notes continuent d'être
           jouées, le moteur fond la piste à zéro en 6 ms ; une note longue (4 mesures…) revient dès qu'on
           rétablit. Seul le réglage « laisser finir les sons et les traînes » saute encore les notes
           (le son reste ouvert pour que les traînes finissent). */
        const bool skipTrack = trackSilent && muteTails.load (std::memory_order_relaxed);
        const bool laneAudio = t.laneAudioMute.load (std::memory_order_relaxed);

        bool anyLaneSolo = false;
        if (! laneAudio)
            for (auto& s : t.laneSolo) anyLaneSolo = anyLaneSolo || s.load (std::memory_order_relaxed);

        // Répétitions en cours, commencées dans un bloc précédent
        for (int lane = 0; lane < numLanes; ++lane)
            if (t.ratchetK[(size_t) lane] >= 0) collectRatchets (ti, t, pt, p, lane, stepLen, from, to, extras, skipTrack, anyLaneSolo, onEvent);

        const auto kFirst = std::max (0LL, (long long) std::floor ((from - origin) / stepLen) - 1);
        const auto kLast = (long long) std::ceil ((to - origin) / stepLen) + 1;
        constexpr double eps = 1.0e-6;

        const auto accentAmt = (double) t.accent.load (std::memory_order_relaxed) / 100.0;
        const auto flamUnit = 0.001 * sampleRateNow * ppqPerSample; // noires par milliseconde

        for (auto k = kFirst; k <= kLast; ++k)
        {
            // le swing suit la grille absolue (pas pairs / impairs depuis le début du morceau)
            const auto kAbs = k + (long long) std::llround (origin / stepLen);
            const auto stepTime = origin + (double) k * stepLen + ((kAbs & 1) ? swingOffset : 0.0);
            const auto nextTime = origin + (double) (k + 1) * stepLen + (((kAbs + 1) & 1) ? swingOffset : 0.0);
            // fonctions de pas : elles suivent la grille du pattern (toutes les lignes du pas)
            const auto flags = pt.flags[(size_t) (k % std::max (1, len))].load (std::memory_order_relaxed);
            if ((flags & stepRec) != 0 && ! trackSilent && stepTime >= from - eps * ppqPerSample && stepTime < to - eps * ppqPerSample)
            {
                const auto at = (stepTime - p0) / ppqPerSample;
                if (at >= -eps && at < (double) blockSamples - eps)
                    onEvent (SeqEvent { ti, -1, std::clamp ((int) std::floor (at + eps), 0, std::max (0, blockSamples - 1)), 0.0f, -1, 0.0, nullptr, flags, 0.0 });
            }

            for (int lane = 0; lane < numLanes; ++lane)
            {
                const auto llen = laneLength (pt, lane, len);
                const auto loop = k / std::max (1, llen);
                const auto pos = positionFor ((Direction) t.direction[(size_t) lane].load (std::memory_order_relaxed), k, llen, rng);
                const auto micro = (double) pt.micro[(size_t) lane][(size_t) pos].load (std::memory_order_relaxed) / 64.0 * stepLen;
                const auto when = stepTime + micro;
                if (when < from - eps * ppqPerSample || when >= to - eps * ppqPerSample) continue;
                const auto at = (when - p0) / ppqPerSample;
                if (at < -eps || at >= (double) blockSamples - eps) continue;

                t.position[(size_t) lane].store (pos, std::memory_order_relaxed);
                const auto vel = pt.vel[(size_t) lane][(size_t) pos].load (std::memory_order_relaxed);
                if (vel == 0) continue;
                if (t.skip[(size_t) lane] == k) { t.skip[(size_t) lane] = -1; continue; }

                const StepExtra* extra = extras != nullptr ? extras->find (ti, p, lane, pos) : nullptr;
                if (extra != nullptr && ! evaluate (ti, lane, *extra, loop)) continue;
                if (skipTrack || (! laneAudio && t.laneMute[(size_t) lane].load (std::memory_order_relaxed))) continue;
                if (anyLaneSolo && ! t.laneSolo[(size_t) lane].load (std::memory_order_relaxed)) continue;

                const auto offset = std::clamp ((int) std::floor (at + eps), 0, std::max (0, blockSamples - 1));
                auto lenPpq = (double) pt.len[(size_t) lane][(size_t) pos].load (std::memory_order_relaxed) / 8.0 * stepLen;
                auto v = (double) vel / 127.0;
                if (flags & stepAccent) v += (1.0 - v) * accentAmt;
                // glissé : la note tient jusqu'au pas suivant (un peu au-delà, pour que les notes se chevauchent)
                if (flags & stepSlide) lenPpq = std::max (lenPpq, nextTime - when + 0.12 * stepLen);

                const bool trigless = (flags & stepTrigless) != 0; // verrous seuls : ni répétitions ni flam
                const int reps = extra != nullptr && ! trigless ? std::clamp ((int) extra->ratchet, 1, maxRatchet) : 1;
                const int flamMs = extra != nullptr && reps <= 1 && ! trigless ? std::min ((int) extra->flam, maxFlamMs) : 0;
                t.ratchetK[(size_t) lane] = -1; // un nouveau pas arrête les répétitions du précédent
                auto firstVel = v;
                if (reps > 1 || flamMs > 0)
                {
                    // Répétitions réparties jusqu'au pas suivant (swing compris), ou flam : un coup léger
                    // puis le coup principal flamMs plus tard. Chaque coup dure au plus 85 % de son écart ;
                    // les suivants sont joués par collectRatchets.
                    const auto gap = flamMs > 0 ? (double) flamMs * flamUnit
                                                : std::max (stepLen * 0.25, nextTime - when) / (double) reps;
                    if (flamMs > 0) firstVel = v * 0.55;
                    else lenPpq = std::min (lenPpq, gap * 0.85);
                    t.ratchetK[(size_t) lane] = k;
                    t.ratchetDone[(size_t) lane] = 1;
                    t.ratchetPos[(size_t) lane] = pos;
                    t.ratchetReps[(size_t) lane] = flamMs > 0 ? 2 : reps;
                    t.ratchetVel[(size_t) lane] = (float) v;
                    t.ratchetFlam[(size_t) lane] = flamMs > 0;
                    t.ratchetAcc[(size_t) lane] = (flags & stepAccent) ? (float) accentAmt : 0.0f;
                    t.ratchetWhen[(size_t) lane] = when;
                    t.ratchetGap[(size_t) lane] = gap;
                }
                onEvent (SeqEvent { ti, lane, offset, (float) (flamMs > 0 ? firstVel : v),
                                    (int) pt.note[(size_t) lane][(size_t) pos].load (std::memory_order_relaxed),
                                    (flamMs > 0 ? std::min (lenPpq, t.ratchetGap[(size_t) lane] * 0.85) : lenPpq) / ppqPerSample, extra,
                                    flags, (nextTime - when) / ppqPerSample, false,
                                    (flags & stepAccent) ? (float) accentAmt : 0.0f });
                if (reps > 1 || flamMs > 0) collectRatchets (ti, t, pt, p, lane, stepLen, from, to, extras, skipTrack, anyLaneSolo, onEvent);
            }
        }
    }

    /** Répétitions (ou coup principal d'un flam) d'un pas déjà déclenché (sa condition a été
        évaluée une seule fois) qui tombent dans [from, to). Elles suivent la coupure et le solo du moment. */
    template <typename OnEvent>
    void collectRatchets (int ti, Track& t, const PatternData& pt, int p, int lane, double stepLen,
                          double from, double to, const PatternExtras* extras, bool trackSilent, bool anyLaneSolo, OnEvent& onEvent) noexcept
    {
        constexpr double eps = 1.0e-6;
        const auto pos = t.ratchetPos[(size_t) lane];
        const auto when = t.ratchetWhen[(size_t) lane];
        const auto interval = t.ratchetGap[(size_t) lane];
        const StepExtra* extra = extras != nullptr ? extras->find (ti, p, lane, pos) : nullptr;
        const auto vel = pt.vel[(size_t) lane][(size_t) pos].load (std::memory_order_relaxed);
        const int reps = t.ratchetReps[(size_t) lane];
        const auto baseVel = (double) t.ratchetVel[(size_t) lane];
        const bool flam = t.ratchetFlam[(size_t) lane];
        auto& done = t.ratchetDone[(size_t) lane];
        if (vel == 0 || extra == nullptr || reps <= 1 || done >= reps) { t.ratchetK[(size_t) lane] = -1; return; }
        const auto stepLenPpq = (double) pt.len[(size_t) lane][(size_t) pos].load (std::memory_order_relaxed) / 8.0 * stepLen;
        const auto lenPpq = flam ? stepLenPpq : std::min (stepLenPpq, interval * 0.85);
        const bool silent = trackSilent || (! t.laneAudioMute.load (std::memory_order_relaxed) && t.laneMute[(size_t) lane].load (std::memory_order_relaxed))
                         || (anyLaneSolo && ! t.laneSolo[(size_t) lane].load (std::memory_order_relaxed));
        const auto slope = (double) extra->ratchetSlope / 100.0;
        while (done < reps)
        {
            const auto rt = when + interval * (double) done;
            if (rt >= to - eps * ppqPerSample) return;         // plus tard : bloc suivant
            const auto at = (rt - blockStartPpq) / ppqPerSample;
            const int j = done++;
            if (rt < from - eps * ppqPerSample || at < -eps) continue; // déjà passé (saut de lecture)
            if (silent) continue;
            const auto v = flam ? baseVel : std::clamp (baseVel * (1.0 + slope * (double) j / (double) (reps - 1)), 1.0 / 127.0, 1.0);
            const auto offset = std::clamp ((int) std::floor (at + eps), 0, std::max (0, blockSamples - 1));
            onEvent (SeqEvent { ti, lane, offset, (float) v,
                                (int) pt.note[(size_t) lane][(size_t) pos].load (std::memory_order_relaxed),
                                lenPpq / ppqPerSample, extra, 0, 0.0, true, t.ratchetAcc[(size_t) lane] });
        }
        t.ratchetK[(size_t) lane] = -1;
    }

    bool neighbourResult (int ti, int lane) const noexcept
    {
        if (lane > 0) return trk (ti).lastResult[(size_t) (lane - 1)];
        return ti > 0 ? trk (ti - 1).lastResult[0] : false;
    }

    static bool validT (int t) noexcept { return t >= 0 && t < numTracks; }
    static bool validP (int p) noexcept { return p >= 0 && p < numPatterns; }
    static bool validTL (int t, int l) noexcept { return validT (t) && l >= 0 && l < numLanes; }
    static bool valid (int t, int p, int l, int s) noexcept { return validTL (t, l) && validP (p) && s >= 0 && s < maxSteps; }

    std::unique_ptr<std::array<Track, numTracks>> tracks;
    std::atomic<int> division { (int) Division::sixteenth };
    std::atomic<int> recQuant { 100 }, launchQuant { 2 };
    std::atomic<bool> muteTails { false };
    std::atomic<std::uint32_t> version { 0 };
    std::atomic<bool> startRequested { false }, stopRequested { false }, recording { false }, running { false }, fill { false };
    std::atomic<double> ppqDisplay { -1.0 };

    bool internalRunning = false, followingHost = false;
    double internalPpq = 0.0, blockStartPpq = 0.0, ppqPerSample = 0.0, swing = 0.5, lastBlockEndPpq = 0.0, sampleRateNow = 48000.0;
    int blockSamples = 0;
    std::uint32_t rng = 0x6C8E9CF5u;
};

} // namespace grv::engine
