/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Piste MIDI » (après la 1.1) : une piste qui ne fait pas de son, elle joue un appareil MIDI
    externe (synthé, boîte à rythmes) branché au PC ou au téléphone. Principe des pistes MIDI des
    stations de production et grooveboxes du marché : tout ce qu'a une piste classique (séquenceur,
    verrous, scènes, modulations, lancement, morceau) agit ici sur des messages MIDI.
      - Réception : canal MIDI de la piste (réglage existant de la piste, 1 à 16).
      - Émission : canal 1 à 16, réglable à part (on peut émettre sur un autre canal).
      - 32 boutons envoyés en CC (4 sections de 8, renommables dans l'interface) : envoyés seulement
        quand leur valeur change (geste, verrou, scène, modulation), jamais au chargement (le son de
        l'appareil n'est pas écrasé) ; « Envoyer au départ » les renvoie à la lecture.
      - Le numéro de CC et le canal de chaque bouton (et de chaque pad) sont FIGÉS : réglés une fois
        (Apprendre ou liste), jamais par une scène, un verrou, une modulation ou une courbe.
      - 16 pads : CC maintenu (gate), CC marche / arrêt (toggle), CC par pas (chaque appui passe à la
        valeur suivante, 2 à 8 pas), ou note.
      - Son (programme) et banque, envoyés quand ils changent.
      - Pitch bend, molette, pression de la piste renvoyés à l'appareil.
      - Effets MIDI : arpégiateur du moteur (le même que celui des synthés), accord (avec étalement),
        gamme (les notes tombent dans la gamme), écho de notes (répétitions qui s'éteignent, avec
        transposition), hasard (probabilité de jouer, vélocité variée), transposition, vélocité fixe.
      - Coupure / solo de la piste : les notes en cours s'arrêtent, rien n'est joué tant qu'elle est coupée.
    Ordre : arpège (moteur) → hasard → gamme → transposition → accord → vélocité → écho → sortie.
    Temps réel : aucune allocation ; file de 256 messages à venir (écho, étalement).
*/

#pragma once

#include "../engine/Module.h"
#include "../synth/ModHub.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace grv::instruments
{

using namespace grv::engine;

namespace midit
{
    // positions figées (projets, préréglages) : les nouveaux réglages s'ajoutent à la fin
    enum Slot
    {
        outCh = 0, transpose, velMode, velFixed, program, bank, sendOnPlay,
        k1,                       // 32 boutons (valeur envoyée en CC) : k1 … k1 + 31
        n1 = k1 + 32,             // numéro de CC de chaque bouton (figé) : n1 … n1 + 31
        c1 = n1 + 32,             // canal de chaque bouton (figé ; 0 : canal d'émission de la piste)
        chord = c1 + 32, strum, scale, root, echoN, echoDiv, echoFb, echoTr, chance, velRand,
        arpMode, // arp_mode, arp_oct, arp_rate, arp_gate, arp_hold, arp_swing, arp_on (l'arpège du moteur, sans pas)
        pad1 = arpMode + 7,       // 16 pads appuyés (0 / 1)
        padNum1 = pad1 + 16,      // numéro de CC ou note de chaque pad (figé)
        padCh1 = padNum1 + 16,    // canal de chaque pad (figé ; 0 : canal d'émission)
        padMode1 = padCh1 + 16,   // mode de chaque pad (figé) : CC maintenu, CC marche / arrêt, CC par pas, note
        padSteps1 = padMode1 + 16 // nombre de pas (2 à 8) du mode « CC par pas » (figé)
    };
    constexpr int numKnobs = 32, numPads = 16;
    inline constexpr int defaultCc[numKnobs] { 74, 71, 73, 72, 75, 76, 77, 78, 91, 93, 70, 79, 16, 17, 18, 19,
                                               20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 85, 86, 87, 89 };
    enum PadMode { padGate = 0, padToggle, padStep, padNote };

    inline const std::vector<std::string>& chordNames()
    {
        static const std::vector<std::string> v { "Aucun", "Octave", "Quinte", "Quinte + octave", "Majeur", "Mineur", "Sus2", "Sus4",
                                                  "7e", "Majeur 7", "Mineur 7", "Mineur 9", "Diminu\xC3\xA9", "Augment\xC3\xA9" };
        return v;
    }
    /** Intervalles de chaque accord (demi-tons au-dessus de la note jouée), 4 au plus ; -1 : fin. */
    inline const int (&chordIntervals (int c))[4]
    {
        static const int t[][4] { { -1, -1, -1, -1 }, { 12, -1, -1, -1 }, { 7, -1, -1, -1 }, { 7, 12, -1, -1 }, { 4, 7, -1, -1 }, { 3, 7, -1, -1 },
                                  { 2, 7, -1, -1 }, { 5, 7, -1, -1 }, { 4, 7, 10, -1 }, { 4, 7, 11, -1 }, { 3, 7, 10, -1 }, { 3, 7, 10, 14 },
                                  { 3, 6, -1, -1 }, { 4, 8, -1, -1 } };
        return t[std::clamp (c, 0, 13)];
    }
    inline const std::vector<std::string>& scaleNames()
    {
        static const std::vector<std::string> v { "Libre", "Majeure", "Mineure naturelle", "Mineure harmonique", "Dorien", "Phrygien",
                                                  "Lydien", "Mixolydien", "Pentatonique majeure", "Pentatonique mineure", "Blues" };
        return v;
    }
    /** Masque des 12 degrés (bit 0 = tonique). */
    inline std::uint16_t scaleMask (int s) noexcept
    {
        static constexpr std::uint16_t m[] { 0xFFF, 0xAB5, 0x5AD, 0x9AD, 0x6AD, 0x5AB, 0xAD5, 0x6B5, 0x295, 0x4A9, 0x4E9 };
        return m[std::clamp (s, 0, 10)];
    }
    inline const std::vector<std::string>& noteNames()
    {
        static const std::vector<std::string> v { "Do", "Do#", "R\xC3\xA9", "R\xC3\xA9#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
        return v;
    }
    inline const std::vector<std::string>& echoDivs()
    {
        static const std::vector<std::string> v { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/8 point\xC3\xA9" "e", "1/4", "1/2" };
        return v;
    }
    inline double echoQuarters (int i) noexcept
    {
        static constexpr double q[] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 2.0 };
        return q[std::clamp (i, 0, 7)];
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            using grv::synth::arpModes;
            using grv::synth::arpRates;
            ModuleDesc m;
            m.type = "midiext";
            m.name = "Piste MIDI";
            m.family = "MIDI externe";
            m.principle = "Joue un appareil MIDI externe : notes, CC, programme, avec le s\xC3\xA9quenceur, les verrous et les effets MIDI";
            m.params = {
                I ("outch", "Canal d'\xC3\xA9mission", "", 1, 16, 1),
                I ("transpose", "Transposition", "dt", -48, 48, 0),
                C ("velmode", "V\xC3\xA9locit\xC3\xA9", { "Jou\xC3\xA9" "e", "Fixe" }),
                I ("velfixed", "V\xC3\xA9locit\xC3\xA9 fixe", "", 1, 127, 100),
                I ("program", "Son (programme)", "", 0, 128, 0),
                I ("bank", "Banque", "", 0, 128, 0),
                C ("sendonplay", "Envoyer au d\xC3\xA9part", { "Non", "Oui" }),
            };
            for (int k = 0; k < numKnobs; ++k)
                m.params.push_back (I ("cc" + std::to_string (k + 1), "Bouton " + std::to_string (k + 1), "", 0, 127, 64));
            auto fixedP = [] (ParamDesc d) { d.fixed = true; return d; };
            for (int k = 0; k < numKnobs; ++k)
                m.params.push_back (fixedP (I ("ccnum" + std::to_string (k + 1), "CC du bouton " + std::to_string (k + 1), "", 0, 119, defaultCc[k]))); // 120 à 127 : ordres de l'appareil, interdits
            for (int k = 0; k < numKnobs; ++k)
                m.params.push_back (fixedP (I ("ccch" + std::to_string (k + 1), "Canal du bouton " + std::to_string (k + 1), "", 0, 16, 0)));
            m.params.push_back (C ("chord", "Accord", chordNames()));
            m.params.push_back (I ("strum", "\xC3\x89" "talement", "ms", 0, 120, 0));
            m.params.push_back (C ("scale", "Gamme", scaleNames()));
            m.params.push_back (C ("root", "Tonique", noteNames()));
            m.params.push_back (I ("echon", "\xC3\x89" "chos", "", 0, 8, 0));
            m.params.push_back (C ("echodiv", "Temps de l'\xC3\xA9" "cho", echoDivs(), 4));
            m.params.push_back (P ("echofb", "Extinction", "%", 10, 100, 70));
            m.params.push_back (I ("echotr", "Transposition de l'\xC3\xA9" "cho", "dt", -12, 12, 0));
            m.params.push_back (P ("chance", "Probabilit\xC3\xA9", "%", 0, 100, 100));
            m.params.push_back (P ("velrand", "V\xC3\xA9locit\xC3\xA9 au hasard", "%", 0, 50, 0));
            // arpégiateur du moteur : même bloc que les synthés (arp_mode … arp_swing, arp_on), sans pas (les notes se règlent dans la séquence)
            m.params.push_back (C ("arp_mode", "Arp\xC3\xA8ge", arpModes()));
            m.params.push_back (I ("arp_oct", "Octaves", "", 1, 4, 1));
            m.params.push_back (C ("arp_rate", "Division", arpRates(), 3));
            m.params.push_back (P ("arp_gate", "Dur\xC3\xA9" "e", "%", 5, 100, 60));
            m.params.push_back (C ("arp_hold", "Maintien", { "Non", "Oui" }));
            m.params.push_back (P ("arp_swing", "Swing", "%", 50, 75, 50));
            m.params.push_back (C ("arp_on", "Arp\xC3\xA8ge en marche", { "Non", "Oui" }, 1));
            // 16 pads : appui (0 / 1, maintenu), puis leur réglage figé
            for (int k = 0; k < numPads; ++k)
            {
                auto d = C ("pad" + std::to_string (k + 1), "Pad " + std::to_string (k + 1), { "Rel\xC3\xA2" "ch\xC3\xA9", "Appuy\xC3\xA9" });
                d.momentary = true;
                m.params.push_back (d);
            }
            for (int k = 0; k < numPads; ++k)
                m.params.push_back (fixedP (I ("padnum" + std::to_string (k + 1), "CC ou note du pad " + std::to_string (k + 1), "", 0, 127, k < 8 ? 80 + k : 102 + (k - 8))));
            for (int k = 0; k < numPads; ++k)
                m.params.push_back (fixedP (I ("padch" + std::to_string (k + 1), "Canal du pad " + std::to_string (k + 1), "", 0, 16, 0)));
            for (int k = 0; k < numPads; ++k)
                m.params.push_back (fixedP (C ("padmode" + std::to_string (k + 1), "Mode du pad " + std::to_string (k + 1),
                                               { "CC maintenu", "CC marche / arr\xC3\xAAt", "CC par pas", "Note" })));
            for (int k = 0; k < numPads; ++k)
                m.params.push_back (fixedP (I ("padsteps" + std::to_string (k + 1), "Pas du pad " + std::to_string (k + 1), "", 2, 8, 4)));
            for (auto& d : m.params)
            {
                if (d.id == "outch") d.fixed = true;                           // canal d'émission : posé une fois pour toutes
                if (d.id == "program" || d.id == "bank") d.noMod = true;       // son et banque : verrouillables par pas, jamais modulés
            }

            std::vector<std::string> knobs, cfg, pads;
            for (int k = 0; k < numKnobs; ++k) knobs.push_back ("cc" + std::to_string (k + 1));
            for (int k = 0; k < numKnobs; ++k) { cfg.push_back ("ccnum" + std::to_string (k + 1)); cfg.push_back ("ccch" + std::to_string (k + 1)); }
            for (int k = 0; k < numPads; ++k)
                for (const char* f : { "pad", "padnum", "padch", "padmode", "padsteps" }) pads.push_back (f + std::to_string (k + 1));
            auto part = [&knobs] (int s) { return std::vector<std::string> (knobs.begin() + s * 8, knobs.begin() + s * 8 + 8); };
            // « midicfg » : réglés par les listes et l'apprentissage du bandeau de la façade, pas par des boutons
            m.sections = {
                { "out", "Appareil", "global", { "outch", "program", "bank" }, "midicfg" },
                { "play", "Jeu", "global", { "transpose", "velmode", "velfixed", "sendonplay" }, "global" },
                // les 32 boutons et les 16 pads sont dessinés par le bandeau de la façade (sections renommables)
                { "knobs1", "Boutons CC 1 \xC3\xA0 8", "macros", part (0), "midicfg" },
                { "knobs2", "Boutons CC 9 \xC3\xA0 16", "macros", part (1), "midicfg" },
                { "knobs3", "Boutons CC 17 \xC3\xA0 24", "macros", part (2), "midicfg" },
                { "knobs4", "Boutons CC 25 \xC3\xA0 32", "macros", part (3), "midicfg" },
                { "pads", "Pads", "global", pads, "midicfg" },
                { "ccnums", "Num\xC3\xA9ros et canaux des CC", "global", cfg, "midicfg" },
                { "chord", "Accord et gamme", "global", { "chord", "strum", "scale", "root", "chance", "velrand" }, "global" },
                { "echo", "\xC3\x89" "cho de notes", "global", { "echon", "echodiv", "echofb", "echotr" }, "global" },
                { "arp", "Arp\xC3\xA8ge", "arp", { "arp_on", "arp_mode", "arp_oct", "arp_rate", "arp_gate", "arp_hold", "arp_swing" }, "arp" },
            };
            // l'arpège ne joue pas tant qu'on ne choisit pas un mode (Arrêt par défaut)
            return m;
        }();
        return d;
    }
}

class MidiTrack final : public Instrument
{
public:
    const ModuleDesc& desc() const noexcept override { return midit::desc(); }
    void prepare (double s, int) override { sr = s > 1000.0 ? s : 48000.0; reset(); }
    void reset() noexcept override
    {
        numSched = 0; clock = 0.0; started = false;
        for (auto& h : held) h = Held {};
        sounding.fill (0);
    }
    void setMidiOut (MidiOutQueue* q) noexcept override { out = q; }
    void setTempo (double b) noexcept override { bpm = b > 20.0 ? b : 120.0; }

    void noteOn (int note, float velocity) noexcept override
    {
        if (silent || note < 0 || note > 127) return;
        const auto* p = last;
        if (p == nullptr) return;
        auto& h = held[(size_t) note];
        if (h.count > 0) releaseHeld (h, 0); // la même touche rejouée : la précédente s'arrête
        h = Held {};
        // hasard : probabilité de jouer
        if (rnd() * 100.0 >= p[midit::chance]) return;
        int base = note;
        base = quantize (base, (int) p[midit::scale], (int) p[midit::root]);
        base += (int) p[midit::transpose];
        if (base < 0 || base > 127) return;
        int v = p[midit::velMode] >= 0.5f ? (int) p[midit::velFixed] : (int) std::lround (std::clamp (velocity, 0.0f, 1.0f) * 127.0f);
        if (p[midit::velRand] > 0.0f) v = (int) std::lround (v * (1.0 + (rnd() * 2.0 - 1.0) * p[midit::velRand] / 100.0));
        v = std::clamp (v, 1, 127);
        h.ch = std::clamp ((int) p[midit::outCh], 1, 16) - 1;
        // accord, avec étalement (chaque note suivante un peu plus tard)
        const auto& iv = midit::chordIntervals ((int) p[midit::chord]);
        const double strumS = p[midit::strum] / 1000.0 * sr;
        h.notes[0] = (std::int8_t) base; h.count = 1;
        for (int k = 0; k < 4 && iv[k] >= 0 && h.count < 5; ++k)
            if (const int nn = base + iv[k]; nn <= 127) h.notes[(size_t) h.count++] = (std::int8_t) nn;
        for (int k = 0; k < h.count; ++k)
        {
            if (k == 0 || strumS < 1.0) sendNoteOn (h.ch, h.notes[(size_t) k], v, 0.0);
            else schedule (clock + strumS * k, 0x90 | h.ch, h.notes[(size_t) k], v);
        }
        // écho de notes : répétitions qui s'éteignent (durée : la moitié de l'intervalle)
        const int echoes = (int) p[midit::echoN];
        if (echoes > 0)
        {
            const double gap = midit::echoQuarters ((int) p[midit::echoDiv]) * 60.0 / bpm * sr;
            double ev = v;
            for (int e = 1; e <= echoes; ++e)
            {
                ev *= p[midit::echoFb] / 100.0;
                const int vv = (int) std::lround (ev);
                if (vv < 1) break;
                const int nn = base + (int) p[midit::echoTr] * e;
                if (nn < 0 || nn > 127) break;
                schedule (clock + gap * e, 0x90 | h.ch, nn, vv);
                schedule (clock + gap * e + gap * 0.5, 0x80 | h.ch, nn, 0);
            }
        }
    }

    void noteOff (int note) noexcept override
    {
        if (note < 0 || note > 127) return;
        auto& h = held[(size_t) note];
        if (h.count > 0) releaseHeld (h, 0);
        h = Held {};
    }

    void allNotesOff() noexcept override { stopAll(); }
    void transportStopped() noexcept override { stopAll(); }
    void transportStarted() noexcept override
    {
        if (last != nullptr && last[midit::sendOnPlay] >= 0.5f) sendAllState (last);
    }

    void setTrackSilent (bool s) noexcept override
    {
        if (s && ! silent) stopAll();
        silent = s;
    }

    void setExpression (float bend, float wheel, float pressure) noexcept override
    {
        if (last == nullptr || out == nullptr) return;
        const int ch = std::clamp ((int) last[midit::outCh], 1, 16) - 1;
        const int b = std::clamp ((int) std::lround ((bend + 1.0f) * 8191.5f), 0, 16383);
        if (b != lastBend) { lastBend = b; out->push (0, (std::uint8_t) (0xE0 | ch), (std::uint8_t) (b & 0x7F), (std::uint8_t) (b >> 7)); }
        const int w = std::clamp ((int) std::lround (wheel * 127.0f), 0, 127);
        if (w != lastWheel) { lastWheel = w; out->push (0, (std::uint8_t) (0xB0 | ch), 1, (std::uint8_t) w); }
        const int pr = std::clamp ((int) std::lround (pressure * 127.0f), 0, 127);
        if (pr != lastPressure) { lastPressure = pr; out->push (0, (std::uint8_t) (0xD0 | ch), (std::uint8_t) pr, 0, 2); }
    }

    void render (float*, float*, int n, const float* p) noexcept override
    {
        last = p;
        const int ch = std::clamp ((int) p[midit::outCh], 1, 16) - 1;
        if (! started)
        {
            // premier bloc : on note l'état sans rien envoyer (le son de l'appareil n'est pas écrasé)
            started = true;
            for (int k = 0; k < midit::numKnobs; ++k)
            {
                sentVal[(size_t) k] = (int) std::lround (p[midit::k1 + k]); sentNum[(size_t) k] = (int) p[midit::n1 + k];
                const int kc = (int) p[midit::c1 + k]; sentCh[(size_t) k] = kc >= 1 && kc <= 16 ? kc - 1 : ch;
            }
            sentProg = (int) p[midit::program]; sentBank = (int) p[midit::bank];
            for (int k = 0; k < midit::numPads; ++k) padPrev[(size_t) k] = p[midit::pad1 + k] >= 0.5f;
        }
        else if (out != nullptr && ! silent)
        {
            // CC : seulement quand la valeur (ou le numéro) change ; au plus un par bouton et par bloc
            for (int k = 0; k < midit::numKnobs; ++k)
            {
                // SÉCURITÉ : changer le numéro (ou le canal) d'un bouton n'envoie RIEN — sinon chaque numéro
                // traversé recevrait la valeur (et les CC 120 à 127 sont des ordres de l'appareil : tout couper,
                // remise à zéro, local, mono / poly). Seule la valeur qui bouge part, jamais vers 120 à 127.
                const int v = std::clamp ((int) std::lround (p[midit::k1 + k]), 0, 127), num = std::clamp ((int) p[midit::n1 + k], 0, 127);
                const int kc = (int) p[midit::c1 + k], kch = kc >= 1 && kc <= 16 ? kc - 1 : ch;
                if (num != sentNum[(size_t) k] || kch != sentCh[(size_t) k]) { sentNum[(size_t) k] = num; sentCh[(size_t) k] = kch; sentVal[(size_t) k] = v; }
                else if (v != sentVal[(size_t) k])
                {
                    sentVal[(size_t) k] = v;
                    if (num < 120) out->push (0, (std::uint8_t) (0xB0 | kch), (std::uint8_t) num, (std::uint8_t) v);
                }
            }
            const int bank = (int) p[midit::bank], prog = (int) p[midit::program];
            if (bank != sentBank || prog != sentProg)
            {
                sentBank = bank; sentProg = prog;
                // comme la télécommande : CC 0 = banque, CC 32 = 0, puis le changement de programme
                if (bank > 0) { out->push (0, (std::uint8_t) (0xB0 | ch), 0, (std::uint8_t) (bank - 1)); out->push (0, (std::uint8_t) (0xB0 | ch), 32, 0); }
                if (prog > 0) out->push (0, (std::uint8_t) (0xC0 | ch), (std::uint8_t) (prog - 1), 0, 2);
            }
        }
        if (started && out != nullptr) renderPads (p, ch);
        // messages à venir (écho, étalement)
        const double end = clock + n;
        for (int i = 0; i < numSched;)
        {
            auto& s = sched[(size_t) i];
            if (s.at < end)
            {
                const int off = std::clamp ((int) (s.at - clock), 0, n - 1);
                if (! silent)
                {
                    if ((s.status & 0xF0) == 0x90) sendNoteOn (s.status & 0x0F, s.d1, s.d2, off);
                    else sendNoteOff (s.status & 0x0F, s.d1, off);
                }
                sched[(size_t) i] = sched[(size_t) --numSched];
            }
            else ++i;
        }
        clock = end;
    }

    /** Pour les tests : nombre de notes qui sonnent sur l'appareil. */
    int soundingCount() const noexcept { int c = 0; for (auto v : sounding) c += v; return c; }

private:
    struct Held { std::array<std::int8_t, 5> notes {}; int count = 0; int ch = 0; };
    struct Sched { double at = 0.0; int status = 0, d1 = 0, d2 = 0; };

    double rnd() noexcept { seed = seed * 1664525u + 1013904223u; return (double) (seed >> 8) / 16777216.0; }

    static int quantize (int note, int scale, int root) noexcept
    {
        if (scale <= 0) return note;
        const auto mask = midit::scaleMask (scale);
        for (int d = 0; d < 12; ++d) // la note de la gamme la plus proche (vers le bas d'abord)
        {
            const int down = note - d, up = note + d;
            if (down >= 0 && (mask >> (((down - root) % 12 + 12) % 12) & 1)) return down;
            if (up <= 127 && (mask >> (((up - root) % 12 + 12) % 12) & 1)) return up;
        }
        return note;
    }

    void schedule (double at, int status, int d1, int d2) noexcept
    {
        if (numSched < (int) sched.size()) sched[(size_t) numSched++] = { at, status, d1, d2 };
    }
    void sendNoteOn (int ch, int note, int vel, double off) noexcept
    {
        if (out == nullptr) return;
        auto& s = sounding[(size_t) (ch * 128 + note)];
        if (s > 0) out->push ((int) off, (std::uint8_t) (0x80 | ch), (std::uint8_t) note, 0); // rejouée : on relâche d'abord
        out->push ((int) off, (std::uint8_t) (0x90 | ch), (std::uint8_t) note, (std::uint8_t) vel);
        s = 1;
    }
    void sendNoteOff (int ch, int note, int off) noexcept
    {
        auto& s = sounding[(size_t) (ch * 128 + note)];
        if (s == 0 || out == nullptr) return;
        out->push (off, (std::uint8_t) (0x80 | ch), (std::uint8_t) note, 0);
        s = 0;
    }
    void releaseHeld (const Held& h, int off) noexcept
    {
        for (int k = 0; k < h.count; ++k)
        {
            const int nn = h.notes[(size_t) k];
            // une note de l'accord encore à venir (étalement) ne doit plus partir
            for (int i = 0; i < numSched;)
                if ((sched[(size_t) i].status & 0xF0) == 0x90 && (sched[(size_t) i].status & 0x0F) == h.ch && sched[(size_t) i].d1 == nn && k > 0)
                    sched[(size_t) i] = sched[(size_t) --numSched];
                else ++i;
            sendNoteOff (h.ch, nn, off);
        }
    }
    /** Pads : un front (appui / relâchement) déclenche selon le mode ; le numéro et le canal sont figés. */
    void renderPads (const float* p, int ch) noexcept
    {
        for (int k = 0; k < midit::numPads; ++k)
        {
            const bool on = p[midit::pad1 + k] >= 0.5f;
            if (on == padPrev[(size_t) k]) continue;
            padPrev[(size_t) k] = on;
            if (silent) continue;
            const int num = std::clamp ((int) p[midit::padNum1 + k], 0, 127);
            const int pc = (int) p[midit::padCh1 + k], pch = pc >= 1 && pc <= 16 ? pc - 1 : ch;
            const int mode = std::clamp ((int) p[midit::padMode1 + k], 0, 3);
            auto cc = [&] (int v) { if (num < 120) out->push (0, (std::uint8_t) (0xB0 | pch), (std::uint8_t) num, (std::uint8_t) std::clamp (v, 0, 127)); };
            if (mode == midit::padNote)
            {
                if (on) { const int v = p[midit::velMode] >= 0.5f ? (int) p[midit::velFixed] : 100; sendNoteOn (pch, num, std::clamp (v, 1, 127), 0.0); padNoteCh[(size_t) k] = (std::int8_t) pch; padNoteNum[(size_t) k] = (std::int8_t) num; }
                else if (padNoteNum[(size_t) k] >= 0) { sendNoteOff (padNoteCh[(size_t) k], padNoteNum[(size_t) k], 0); padNoteNum[(size_t) k] = -1; }
            }
            else if (mode == midit::padGate) cc (on ? 127 : 0);
            else if (! on) continue;
            else if (mode == midit::padToggle) { padState[(size_t) k] = padState[(size_t) k] ? 0 : 1; cc (padState[(size_t) k] ? 127 : 0); }
            else
            {
                const int steps = std::clamp ((int) p[midit::padSteps1 + k], 2, 8);
                padState[(size_t) k] = (padState[(size_t) k] + 1) % steps;
                cc ((int) std::lround (127.0 * padState[(size_t) k] / (steps - 1)));
            }
        }
    }

    void stopAll() noexcept
    {
        padNoteNum.fill (-1);
        numSched = 0;
        for (auto& h : held) h = Held {};
        for (int i = 0; i < 16 * 128; ++i)
            if (sounding[(size_t) i] > 0) sendNoteOff (i / 128, i % 128, 0);
    }
    void sendAllState (const float* p) noexcept
    {
        if (out == nullptr) return;
        const int ch = std::clamp ((int) p[midit::outCh], 1, 16) - 1;
        if (p[midit::bank] > 0.5f) { out->push (0, (std::uint8_t) (0xB0 | ch), 0, (std::uint8_t) ((int) p[midit::bank] - 1)); out->push (0, (std::uint8_t) (0xB0 | ch), 32, 0); }
        if (p[midit::program] > 0.5f) out->push (0, (std::uint8_t) (0xC0 | ch), (std::uint8_t) ((int) p[midit::program] - 1), 0, 2);
        for (int k = 0; k < midit::numKnobs; ++k)
        {
            const int kc = (int) p[midit::c1 + k], kch = kc >= 1 && kc <= 16 ? kc - 1 : ch;
            if (const int num = std::clamp ((int) p[midit::n1 + k], 0, 127); num < 120)
                out->push (0, (std::uint8_t) (0xB0 | kch), (std::uint8_t) num, (std::uint8_t) std::clamp ((int) std::lround (p[midit::k1 + k]), 0, 127));
        }
    }

    MidiOutQueue* out = nullptr;
    const float* last = nullptr;          // réglages du dernier bloc (les notes arrivent entre deux blocs)
    double sr = 48000.0, bpm = 120.0, clock = 0.0;
    bool started = false, silent = false;
    std::array<Held, 128> held {};
    std::array<std::uint8_t, 16 * 128> sounding {};
    std::array<Sched, 256> sched {};
    int numSched = 0;
    std::array<int, midit::numKnobs> sentVal {}, sentNum {}, sentCh {};
    std::array<bool, midit::numPads> padPrev {};
    std::array<int, midit::numPads> padState {};
    std::array<std::int8_t, midit::numPads> padNoteCh {}, padNoteNum { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    int sentProg = 0, sentBank = 0, lastBend = 8192, lastWheel = 0, lastPressure = 0;
    std::uint32_t seed = 0x2545F491u;
};

} // namespace grv::instruments
