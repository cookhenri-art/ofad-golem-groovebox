/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « FM Player » (v0.49) — lecteur de firmwares du boîtier de synthèse FM-1 : le processeur du boîtier
    est émulé (third_party/fm1emu : fm1-emu de Joakim Eriksson, licence MIT) et lit le firmware tel quel
    (fichier .fwsc, d'origine ou fait par la communauté). Chaque firmware devient ainsi un instrument de
    GOLEM : joué par le séquenceur, le clavier et le MIDI, avec son écran et ses commandes sur la façade.

    Le firmware arrive comme un échantillon (emplacement 0 de la piste) : ses octets sont gardés dans le
    projet comme ceux d'un fichier audio, et la machine émulée est créée au chargement, hors du fil audio
    (SampleBuffer::runtime). Le fil audio ne fait que lire l'anneau de sortie du FM-1 (sans verrou ni
    allocation), rééchantillonner de 44,1 kHz vers la fréquence de GOLEM (interpolation d'Hermite) et
    envoyer les notes en MIDI, horodatées.

    Aucun firmware n'est fourni avec GOLEM : chacun charge le sien.
*/

#pragma once

#include "../engine/Module.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if GROOVEBOX_FM1
 #include "fm1_bridge.h"
#endif

namespace grv::instruments
{

using namespace grv::engine;

namespace fmplayer
{
    enum Slot { level = 0, transpose, lead, channel, clock, notes,
                // GOLEM 1.0 : les 8 commandes du boîtier en réglages (contrôleur MIDI, 8 encodeurs, automation)
                knob1, knob2, knob3, knob4, select, presets, algorithm, master,
                // GOLEM 1.0 : les 12 touches de menu et OCT − / OCT + (appui long → contrôleur MIDI)
                keyFirst, numKeys = 14, travelFirst = keyFirst + numKeys, numSlots = travelFirst + 7 };
    /** GOLEM 1.0 : crans envoyés au firmware pour la course complète de chaque bouton (0 à 100 %) :
        peu de crans pour un choix court (4 pour une piste 1 à 4 : un quart de tour par cran). */
    inline constexpr int numTravels = 8;
    inline constexpr int travelClicks[numTravels] = { 4, 8, 16, 32, 64, 128, 256, 512 };
    /** Touche du boîtier (numéro de l'émulateur) de chaque réglage de touche, et son nom. */
    inline constexpr int keyOfSlot[14] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1 };
    inline constexpr const char* keyIds[14] = { "key_fx", "key_scl", "key_env", "key_lfo", "key_edit", "key_glo",
                                                "key_home", "key_save", "key_arp", "key_seq", "key_play", "key_rec", "key_octdn", "key_octup" };
    inline constexpr const char* keyNames[14] = { "FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT -", "OCT +" };
    /** Bouton rotatif du boîtier piloté par chaque réglage Knob 1 à Algorithm (numéros de l'émulateur). */
    inline constexpr int encOfSlot[7] = { 2, 3, 4, 5, 0, 6, 1 };
    /** v0.49 : pistes internes T1 à T4 (comme Pieuvre 8) : chacune joue sur son canal MIDI (canal de T1,
        puis les suivants) — les firmwares à plusieurs pistes (SLOOP, sloopDX, JIANT…) les jouent toutes. */
    inline constexpr int numLanes = 4;

    /** Gestes de la façade (Instrument::touch) : kind = touche, bouton rotatif, potentiomètre, force des touches. */
    enum Touch { touchKey = 1, touchEncoder = 2, touchAnalog = 3, touchVelocity = 4 };

    /** Avance gardée par le FM-1 (ms) : plus elle est courte, plus le jeu au clavier est direct, mais
        un ordinateur lent peut alors faire des trous dans le son. */
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "fmplayer";
            m.name = "FM Player";
            m.family = "Polys";
            // v0.49 : 4 lignes de séquence T1 à T4 (une par canal MIDI), notes par pas, comme Pieuvre 8
            m.drumKit = true;
            m.laneNotes = true;
            m.laneLowKeys = true; // GOLEM 1.0 : le clavier entier joue la piste T choisie (pas de pads en bas du clavier)
            for (int k = 1; k <= numLanes; ++k) { m.padNames.push_back ("T" + std::to_string (k)); m.padKinds.push_back ("fm1"); }
            m.principle = "Lecteur de firmwares du bo\xC3\xAEtier de synth\xC3\xA8se FM-1 (processeur \xC3\xA9mul\xC3\xA9) : "
                          "chaque firmware .fwsc devient un instrument";
            m.params = {
                P ("level", "Volume", "dB", -24, 12, 0),
                P ("transpose", "Transposition", "dt", -24, 24, 0),
                P ("lead", "Avance", "ms", 10, 120, 40),
                // chaque firmware écoute son canal (celui d'origine : 2 ; Felucca : 1 à 4) ; Auto : celui du firmware chargé
                C ("channel", "Canal de T1 (T2 \xC3\xA0 T4 : les suivants)", { "Auto", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" }),
                // horloge MIDI de GOLEM (départ, arrêt, 24 tops par noire) : les séquenceurs des firmwares suivent
                C ("clock", "Horloge MIDI", { "Non", "Oui" }, 1),
                // notes de GOLEM : par la prise MIDI du boîtier, ou sur ses touches (firmwares qui ne lisent pas le MIDI)
                C ("notes", "Notes jou\xC3\xA9" "es par", { "Auto", "MIDI", "Touches du bo\xC3\xAEtier" }),
                // GOLEM 1.0 : les commandes du boîtier. Sur le vrai boîtier, des encodeurs sans fin ; ici bornés
                // (0 à 100 %) : la course complète envoie « Course des boutons » crans au firmware, assez pour
                // aller de son minimum à son maximum (il s'arrête de lui-même à ses limites).
                P ("knob1", "Knob 1", "%", 0, 100, 50),
                P ("knob2", "Knob 2", "%", 0, 100, 50),
                P ("knob3", "Knob 3", "%", 0, 100, 50),
                P ("knob4", "Knob 4", "%", 0, 100, 50),
                P ("select", "Select", "%", 0, 100, 50),
                P ("presets", "Presets", "%", 0, 100, 50),
                P ("algorithm", "Algorithm", "%", 0, 100, 50),
                P ("master", "Master", "%", 0, 100, 100),
            };
            // GOLEM 1.0 : touches du boîtier (appuyée / relâchée), une note ou un pad du contrôleur les tient
            for (int k = 0; k < fmplayer::numKeys; ++k)
            {
                auto c = C (fmplayer::keyIds[k], std::string ("Touche ") + fmplayer::keyNames[k], { "Rel\xC3\xA2" "ch\xC3\xA9" "e", "Appuy\xC3\xA9" "e" });
                c.momentary = true;
                m.params.push_back (c);
            }
            // après les touches (positions figées : enum Slot) : la course de chacun des 7 boutons
            {
                const char* ids[7] = { "travel_k1", "travel_k2", "travel_k3", "travel_k4", "travel_select", "travel_presets", "travel_algorithm" };
                const char* nm[7] = { "Knob 1", "Knob 2", "Knob 3", "Knob 4", "Select", "Presets", "Algorithm" };
                for (int i = 0; i < 7; ++i)
                    m.params.push_back (C (ids[i], std::string ("Course ") + nm[i],
                                           { "4 crans", "8 crans", "16 crans", "32 crans", "64 crans", "128 crans", "256 crans", "512 crans" },
                                           i == 6 ? 0 : i < 4 ? 5 : 6)); // Algorithm : 4 crans (un quart de tour par algorithme) ;
                                                                          // Knob 1 à 4 : 128 (la plupart des réglages des firmwares)
            }
            // les commandes du boîtier d'abord : elles tombent sous les 8 encodeurs de GOLEM
            m.sections = { { "box", "Bo\xC3\xAEtier", "global", { "knob1", "knob2", "knob3", "knob4", "select", "presets", "algorithm", "master" }, "global" },
                           { "global", "Global", "global", { "level", "transpose", "lead", "channel", "clock", "notes" }, "global" },
                           { "travel", "Courses", "global", { "travel_k1", "travel_k2", "travel_k3", "travel_k4", "travel_select", "travel_presets", "travel_algorithm" }, "global" } };
            return m;
        }();
        return d;
    }

    /** Un firmware .fwsc (en-tête « FW » du fabricant ou paquet SDK) ? Contrôle léger, avant l'émulation. */
    inline bool looksLikeFirmware (const std::string& name, const std::uint8_t* bytes, size_t size) noexcept
    {
        if (bytes == nullptr || size < 4096 || size > (size_t) 8 * 1024 * 1024) return false;
        if (name.size() < 5) return false;
        auto ext = name.substr (name.size() - 5);
        for (auto& c : ext) c = (char) std::tolower ((unsigned char) c);
        return ext == ".fwsc";
    }

#if GROOVEBOX_FM1
    /** La machine émulée d'un firmware chargé (créée hors du fil audio, détruite par la corbeille du moteur),
        avec ce que « Auto » choisit pour lui : canal MIDI (0..15) et notes sur les touches du boîtier. */
    struct Machine
    {
        Fm1* fm = nullptr;
        int channel = 0;
        bool keys = false;
        ~Machine() { if (fm != nullptr) fm1_free (fm); }
    };

    /** Réglages « Auto » des firmwares essayés dans GOLEM (canal qui fait sonner les notes, mesuré). */
    inline void autoProfile (const std::string& fileName, Machine& m)
    {
        std::string n;
        for (auto c : fileName) n += (char) std::tolower ((unsigned char) c);
        auto has = [&] (const char* k) { return n.find (k) != std::string::npos; };
        m.channel = 0; m.keys = false;
        if (has ("fm-1_0") || has ("fm-1.fwsc") || has ("fm-1 ")) m.channel = 1; // firmware d'origine : canal 2
        else if (has ("x0x")) m.channel = 2;                                        // X0X : ses basses sur les canaux 3 et 4
        else if (has ("b1") && (has ("b1.") || has ("b1-") || has ("b1 ") || has ("drawbar"))) m.keys = true; // orgue B1 : touches du boîtier
    }

    /** Crée la machine d'un firmware (fil de l'interface) ; message d'erreur sinon. */
    inline std::string start (const std::vector<std::uint8_t>& pkg, const std::vector<std::uint8_t>& sectors,
                              std::shared_ptr<void>& out, const std::string& fileName = {})
    {
        auto* f = fm1_new (pkg.data(), pkg.size(), sectors.empty() ? nullptr : sectors.data(), sectors.size(), 240);
        if (f == nullptr)
        {
            std::uint8_t msg[256] {};
            const auto n = fm1_last_error (msg, sizeof (msg));
            return n > 0 ? std::string ((const char*) msg, std::min (n, sizeof (msg))) : std::string ("Firmware refus\xC3\xA9");
        }
        fm1_analog (f, 4, 1023); // potentiomètre MASTER au maximum (la façade le montre ainsi ; certains firmwares s'en servent de volume)
        auto m = std::make_shared<Machine>();
        m->fm = f;
        autoProfile (fileName, *m);
        out = std::move (m);
        return {};
    }
#endif
}

class FmPlayer final : public Instrument
{
public:
    const ModuleDesc& desc() const noexcept override { return fmplayer::desc(); }
    bool acceptsFirmware() const noexcept override { return true; }
    /** Avance de l'émulation (le son sort « Avance » après la note) : le moteur retarde d'autant les autres pistes. */
    int latencySamples() const noexcept override { return machineOn ? latency : 0; }

    void prepare (double s, int maxBlock) override
    {
        sr = s > 0.0 ? s : 48000.0;
        step = 44100.0 / sr;
        // images à 44,1 kHz pour un bloc, plus la marge de l'interpolation
        buf.assign ((size_t) (2 * ((int) std::ceil (std::max (maxBlock, 64) * step) + 16)), 0.0f);
        reset();
    }

    void reset() noexcept override { avail = 0; pos = 1.0; lastBend = -2.0f; lastWheel = -1.0f; lastPressure = -1.0f; }

    void setSample (int index, const SampleBuffer* s) noexcept override
    {
        if (index != 0) return;
       #if GROOVEBOX_FM1
        auto* m = s != nullptr ? static_cast<fmplayer::Machine*> (s->runtime.get()) : nullptr;
        fm.store (m != nullptr ? m->fm : nullptr);
        autoChannel = m != nullptr ? m->channel : 0;
        autoKeys = m != nullptr && m->keys;
       #else
        (void) s;
       #endif
        avail = 0; pos = 1.0;
        machineOn = m != nullptr;
        idle = 0; paused = false;
        for (auto& k : lastKnob) k = -1; // nouvelle machine : positions reprises sans geste,
        lastMaster = -1;                 // MASTER renvoyé à sa valeur réglée
        for (auto& k : lastKey) k = -1;
    }

    /** v0.49 : note du moteur → piste T et note : pas de séquence (128 + T × 128 + note), pad (0 à 15 :
        la T, Do3), clavier (16 à 127 : la T choisie). */
    void decode (int note, int& lane, int& n) const noexcept
    {
        if (note >= 128) { lane = std::clamp ((note - 128) / 128, 0, fmplayer::numLanes - 1); n = (note - 128) % 128; }
        else { lane = playLane; n = std::clamp (note, 0, 127); } // GOLEM 1.0 : toutes les notes sont des touches
    }
    /** Case de la note tenue (pour la relâcher sur le canal et la touche où elle a été jouée). */
    static int slotOf (int note) noexcept
    {
        if (note >= 128) return std::clamp (note - 128, 0, 2047);
        return 2048 + std::clamp (note, 0, 127);
    }

    void noteOn (int note, float velocity) noexcept override
    {
        wake();
        int lane = 0, n0 = 60;
        decode (note, lane, n0);
        if ((laneMutes >> lane) & 1u) return; // ligne coupée (coupure, solo)
        const auto slot = (size_t) slotOf (note);
        if (sent[slot] >= 0) noteOff (note); // même case encore tenue : relâchée d'abord
        const auto n = std::clamp (n0 + transposeNow, 0, 127);
        const auto v = std::clamp ((int) std::lround (velocity * 127.0f), 1, 127);
        if (useKeys)
        {
            // touches du boîtier : Fa3 (note 53) à Sol5, les autres octaves ramenées dans ces 27 touches
            auto idx = n - 53;
            while (idx < 0) idx += 12;
            while (idx > 26) idx -= 12;
            sent[slot] = (std::int16_t) (4096 + 14 + idx);
           #if GROOVEBOX_FM1
            if (auto* f = fm.load()) fm1_key_at (f, (std::uint8_t) (14 + idx), true, (std::uint8_t) v);
           #endif
            return;
        }
        const auto ch = (chan + lane) & 15;
        sent[slot] = (std::int16_t) (ch * 128 + n);
        midiOn (0x90, ch, (std::uint8_t) n, (std::uint8_t) v);
    }

    void noteOff (int note) noexcept override
    {
        const auto slot = (size_t) slotOf (note);
        const auto s = sent[slot];
        if (s < 0) return;
        sent[slot] = -1;
        if (s >= 4096)
        {
           #if GROOVEBOX_FM1
            if (auto* f = fm.load()) fm1_key_at (f, (std::uint8_t) (s - 4096), false, 0);
           #endif
            return;
        }
        midiOn (0x80, s / 128, (std::uint8_t) (s % 128), 0); // le canal et la note joués, même si la T ou la transposition ont changé
    }

    void setPlayLane (int lane) noexcept override { playLane = std::clamp (lane, 0, fmplayer::numLanes - 1); }
    void setLaneMutes (std::uint32_t mask) noexcept override { laneMutes = mask; }

    /** CC d'un contrôleur branché (non appris par GOLEM) : tel quel au firmware, qui s'en sert à sa façon. */
    void controller (int cc, float value) noexcept override
    {
        wake();
        midi (0xB0, (std::uint8_t) std::clamp (cc, 0, 127), (std::uint8_t) std::clamp ((int) std::lround (value * 127.0f), 0, 127));
    }

    /** Position du séquenceur (noires) au début du bloc : horloge MIDI pour les firmwares à séquenceur. */
    void setPosition (double ppq, bool playing) noexcept override
    {
        if (! clockOn) { wasPlaying = false; return; }
        if (playing && ! wasPlaying) { midi (ppq < 0.01 ? 0xFA : 0xFB, 0, 0, 1); lastTick = (long long) std::floor (ppq * 24.0) - 1; }
        else if (! playing && wasPlaying) midi (0xFC, 0, 0, 1);
        wasPlaying = playing;
        if (! playing) return;
        const auto tick = (long long) std::floor (ppq * 24.0);
        if (tick < lastTick || tick - lastTick > 96) lastTick = tick - 1; // saut (boucle, déplacement) : pas de rafale
        for (; lastTick < tick; ++lastTick) midi (0xF8, 0, 0, 1);
    }

    void allNotesOff() noexcept override
    {
        sent.fill (-1);
        for (int k = 0; k < fmplayer::numLanes; ++k) { midiOn (0xB0, (chan + k) & 15, 123, 0); midiOn (0xB0, (chan + k) & 15, 120, 0); }
    }
    void transportStopped() noexcept override
    {
        for (int k = 0; k < fmplayer::numLanes; ++k) midiOn (0xB0, (chan + k) & 15, 123, 0);
    }

    void setExpression (float bend, float wheel, float pressure) noexcept override
    {
        if (std::abs (bend - lastBend) > 1.0e-4f)
        {
            lastBend = bend;
            const auto v = std::clamp ((int) std::lround (8192.0f + bend * 8191.0f), 0, 16383);
            midi (0xE0, (std::uint8_t) (v & 0x7F), (std::uint8_t) (v >> 7));
        }
        if (std::abs (wheel - lastWheel) > 1.0e-3f)
        {
            lastWheel = wheel;
            midi (0xB0, 1, (std::uint8_t) std::clamp ((int) std::lround (wheel * 127.0f), 0, 127));
        }
        if (std::abs (pressure - lastPressure) > 1.0e-3f)
        {
            lastPressure = pressure;
            midi (0xD0, (std::uint8_t) std::clamp ((int) std::lround (pressure * 127.0f), 0, 127), 0, 2);
        }
    }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        transposeNow = (int) std::lround (p[fmplayer::transpose]);
        clockOn = p[fmplayer::clock] > 0.5f;
        const auto chSel = (int) std::lround (p[fmplayer::channel]);
        const auto ch = std::clamp (chSel <= 0 ? autoChannel : chSel - 1, 0, 15);
        const auto ns = (int) std::lround (p[fmplayer::notes]);
        useKeys = ns == 0 ? autoKeys : ns == 2;
        if (ch != chan) { midi (0xB0, 123, 0); chan = ch; } // nouveau canal : les notes de l'ancien s'arrêtent
       #if GROOVEBOX_FM1
        auto* f = fm.load();
        if (f == nullptr || n <= 0 || buf.empty()) return;
        const auto leadFrames = (std::uint32_t) std::clamp (p[fmplayer::lead] * 0.001 * 44100.0, 256.0, 16000.0);
        if (leadFrames != lastLead) { fm1_set_lead (f, leadFrames); lastLead = leadFrames; }
        // GOLEM 1.0 : commandes du boîtier pilotées par leurs réglages (contrôleur MIDI, encodeurs, automation)
        // course complète (0 à 100 %) = « Course des boutons » crans ; les crans déjà envoyés sont comptés
        for (int i = 0; i < 7; ++i)
        {
            const auto full = (double) fmplayer::travelClicks[std::clamp ((int) std::lround (p[fmplayer::travelFirst + i]), 0, fmplayer::numTravels - 1)];
            const auto want = (double) p[fmplayer::knob1 + i] / 100.0 * full; // position en crans
            if (lastKnob[i] < -0.5 || full != lastFull[i]) { lastKnob[i] = want; lastFull[i] = full; continue; } // départ ou course changée : sans geste
            const auto clicks = (int) std::lround (want - lastKnob[i]);
            if (clicks == 0) continue;
            // 1.0 (6 nov.) : butées recalées — en arrivant à 0 % ou à 100 %, une course de plus dans le même sens :
            // le firmware s'arrête à sa butée, et 0 % / 100 % redeviennent son minimum / maximum (même s'il a été
            // poussé au-delà avant, ou si sa course est plus longue que celle réglée)
            const bool atEnd = i < 4 && (want <= 0.0 || want >= full); // Knob 1 à 4 seulement (Select, Presets : listes qui bouclent)
            const auto extra = atEnd ? (want <= 0.0 ? -(int) full : (int) full) : 0;
            for (int left = clicks + extra; left != 0;) // au plus 64 crans par message
            {
                const auto k = std::clamp (left, -64, 64);
                fm1_encoder_at (f, (std::uint8_t) fmplayer::encOfSlot[i], k);
                left -= k;
            }
            lastKnob[i] += clicks;
            idle = 0; wakeRequest.store (true, std::memory_order_relaxed);
        }
        if (const auto m = (int) std::lround (p[fmplayer::master] * 10.23); m != lastMaster)
        {
            fm1_analog_at (f, 4, (std::uint16_t) std::clamp (m, 0, 1023));
            if (lastMaster >= 0) { idle = 0; wakeRequest.store (true, std::memory_order_relaxed); }
            lastMaster = m;
        }
        // GOLEM 1.0 : touches du boîtier tenues par leur réglage (pad ou note du contrôleur)
        for (int k = 0; k < fmplayer::numKeys; ++k)
        {
            const int down = p[fmplayer::keyFirst + k] >= 0.5f ? 1 : 0;
            if (lastKey[k] < 0) { lastKey[k] = down; continue; } // départ : position reprise, sans appui
            if (down == lastKey[k]) continue;
            lastKey[k] = down;
            fm1_key_at (f, (std::uint8_t) fmplayer::keyOfSlot[k], down != 0, 100);
            idle = 0; wakeRequest.store (true, std::memory_order_relaxed);
        }
        latency = (int) std::lround (leadFrames / step) + 2; // en images de GOLEM (+ l'interpolation)
        // pause : rien joué, rien entendu depuis 20 s (et pas d'horloge qui tourne) → l'émulation dort
        if (wakeRequest.exchange (false, std::memory_order_relaxed)) wake();
        if (clockOn && wasPlaying) idle = 0;
        if (paused) return;
        if (idle > (long long) (20.0 * sr)) { paused = true; fm1_set_paused (f, true); avail = 0; pos = 1.0; return; }
        idle += n;
        const auto cap = (int) (buf.size() / 2);
        // images de 44,1 kHz nécessaires pour ce bloc (point courant + 2 suivants pour l'interpolation)
        const auto need = std::min (cap, (int) std::ceil (pos + n * step) + 3);
        if (avail < need)
        {
            fm1_pull (f, buf.data() + 2 * avail, (std::uint32_t) (need - avail));
            avail = need;
        }
        const auto g = (float) std::pow (10.0, p[fmplayer::level] / 20.0);
        for (int i = 0; i < n; ++i)
        {
            auto k = (int) pos;
            if (k + 2 >= avail) break; // ne doit pas arriver (bloc plus long que prévu)
            const auto t = (float) (pos - k);
            const auto km = std::max (0, k - 1);
            const auto l = hermite (buf[(size_t) (2 * km)], buf[(size_t) (2 * k)], buf[(size_t) (2 * k + 2)], buf[(size_t) (2 * k + 4)], t);
            const auto r = hermite (buf[(size_t) (2 * km + 1)], buf[(size_t) (2 * k + 1)], buf[(size_t) (2 * k + 3)], buf[(size_t) (2 * k + 5)], t);
            if (std::abs (l) + std::abs (r) > 1.0e-4f) idle = 0; // la machine joue (séquenceur, son génératif)
            L[i] += g * l;
            R[i] += g * r;
            pos += step;
        }
        // garder une image avant le point courant (interpolation) et tout ce qui suit
        const auto drop = std::max (0, (int) pos - 1);
        if (drop > 0)
        {
            std::copy (buf.begin() + 2 * drop, buf.begin() + 2 * avail, buf.begin());
            avail -= drop;
            pos -= drop;
        }
       #else
        (void) L; (void) R; (void) n;
       #endif
    }

    /** Gestes de la façade (fil de l'interface) : touches, boutons rotatifs, potentiomètre. */
    void touch (int kind, float a, float b) noexcept override
    {
       #if GROOVEBOX_FM1
        auto* f = fm.load();
        if (f == nullptr) return;
        wakeRequest.store (true, std::memory_order_relaxed); // geste sur la façade : la machine se réveille
        fm1_set_paused (f, false);
        if (kind == fmplayer::touchKey) fm1_key (f, (std::uint8_t) std::clamp ((int) a, 0, 40), b > 0.5f);
        else if (kind == fmplayer::touchEncoder) fm1_encoder (f, (std::uint8_t) std::clamp ((int) a, 0, 7), (std::int32_t) std::clamp ((int) b, -64, 64));
        else if (kind == fmplayer::touchAnalog) fm1_analog (f, (std::uint8_t) std::clamp ((int) a, 0, 22), (std::uint16_t) std::clamp ((int) b, 0, 1023));
        else if (kind == fmplayer::touchVelocity) fm1_key_velocity (f, (std::uint8_t) std::clamp ((int) a, 1, 127));
       #else
        (void) kind; (void) a; (void) b;
       #endif
    }

    /** État pour l'interface : [0] 1 = firmware chargé, [1] démarré, [2] arrêté sur une faute,
        [3] vitesse de l'émulation (× temps réel), [4] images manquantes depuis le chargement. */
    int status (float* out, int max) const noexcept override
    {
        if (max < 5) return 0;
       #if GROOVEBOX_FM1
        auto* f = fm.load();
        if (f == nullptr) { out[0] = 0.0f; out[1] = out[2] = out[3] = out[4] = 0.0f; return 5; }
        const auto v = fm1_view (f, nullptr, nullptr);
        out[0] = 1.0f;
        out[1] = (v & 1u) ? 1.0f : 0.0f;
        out[2] = (v & 2u) ? 1.0f : 0.0f;
        out[3] = fm1_speed (f);
        out[4] = (float) fm1_starved (f);
        if (max >= 6) { out[5] = paused ? 1.0f : 0.0f; return 6; }
        return 5;
       #else
        out[0] = -1.0f; out[1] = out[2] = out[3] = out[4] = 0.0f; // -1 : lecteur absent de cette version
        return 5;
       #endif
    }

   #if GROOVEBOX_FM1
    /** L'émulateur (fil de l'interface : écran, flash). */
    Fm1* machine() const noexcept { return fm.load(); }
   #endif

private:
    static float hermite (float y0, float y1, float y2, float y3, float t) noexcept
    {
        const auto c1 = 0.5f * (y2 - y0);
        const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    /** Une note, un CC, un geste : la machine en pause repart (fil audio). */
    void wake() noexcept
    {
        idle = 0;
        if (! paused) return;
        paused = false;
       #if GROOVEBOX_FM1
        if (auto* f = fm.load()) fm1_set_paused (f, false);
       #endif
    }

    /** Message de canal sur un canal donné (piste T). */
    void midiOn (std::uint8_t status, int ch, std::uint8_t a, std::uint8_t b) noexcept
    {
       #if GROOVEBOX_FM1
        if (auto* f = fm.load()) fm1_midi (f, (std::uint8_t) (status | (ch & 15)), a, b, 3);
       #else
        (void) status; (void) ch; (void) a; (void) b;
       #endif
    }

    void midi (std::uint8_t status, std::uint8_t a, std::uint8_t b, std::uint8_t len = 3) noexcept
    {
       #if GROOVEBOX_FM1
        if (auto* f = fm.load()) fm1_midi (f, (std::uint8_t) (status | (status < 0xF0 ? chan : 0)), a, b, len);
       #else
        (void) status; (void) a; (void) b; (void) len;
       #endif
    }

   #if GROOVEBOX_FM1
    std::atomic<Fm1*> fm { nullptr };
   #endif
    std::vector<float> buf;
    int avail = 0;
    double pos = 1.0, sr = 48000.0, step = 44100.0 / 48000.0;
    std::uint32_t lastLead = 0;
    int latency = 0;
    double lastKnob[7] = { -1, -1, -1, -1, -1, -1, -1 };
    double lastFull[7] = {};
    int lastMaster = -1;
    int lastKey[fmplayer::numKeys] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    bool machineOn = false, paused = false;
    long long idle = 0;
    std::atomic<bool> wakeRequest { false };
    int transposeNow = 0, chan = 0, autoChannel = 0;
    bool useKeys = false, autoKeys = false;
    /** Notes tenues par case (slotOf) : canal × 128 + note, 4096 + touche du boîtier, -1 : rien. */
    std::array<std::int16_t, 2176> sent = [] { std::array<std::int16_t, 2176> a {}; a.fill (-1); return a; }();
    int playLane = 0;
    std::uint32_t laneMutes = 0;
    bool clockOn = true, wasPlaying = false;
    long long lastTick = 0;
    float lastBend = -2.0f, lastWheel = -1.0f, lastPressure = -1.0f;
};

} // namespace grv::instruments
