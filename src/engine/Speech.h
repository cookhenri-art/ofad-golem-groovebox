/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Phrase tapée de la voix interne des vocodeurs (v0.33).

    Le texte (français, 160 octets au plus) est transformé une fois, hors du fil audio, en une
    suite de phonèmes : règles de lecture du français (graphèmes → sons : « eau », « ou », « an »,
    « on », « in », « ch », « gn », « qu », « tion », e muet, consonnes finales muettes…), puis
    durées (voyelle, consonne, pauses de mot, de virgule, de fin de phrase), mélodie de la phrase
    (déclinaison, montée de la question, accent final allongé) et numéro du mot (affichage).

    v0.39 — écriture expressive (demande d'Henrico : « des temps de pause, ne pas être monotone ») :
      - plusieurs espaces : un silence qui grandit avec le nombre d'espaces ; un retour à la ligne : une
        pause de virgule ;
      - deux points ou plus (« .. », « ... », « … ») : phrase suspendue (la voix reste en l'air) et
        silence qui grandit avec le nombre de points ;
      - lettre de voyelle répétée trois fois ou plus (« muuutants ») : voyelle étirée ;
      - mot en CAPITALES : appuyé (plus haut, plus long, plus fort) ;
      - « ! » : exclamation (départ plus haut, chute marquée, dernière syllabe appuyée) ; « ? » : montée ;
        doublés (« !! », « ?? ») : plus marqués ;
      - un petit accent montant à la fin de chaque mot d'un groupe (le français monte avant de
        retomber en fin de phrase) : la phrase n'est plus récitée sur une seule ligne.

    Chaque phonème porte les réglages du synthétiseur à formants de la voix interne
    (synth/Vocoder.h, DemoVoice) : trois formants et leurs gains, résonance nasale, part voisée,
    bruit (fréquence, sélectivité), occlusion et explosion des plosives.

    Rien ici n'est appelé par le fil audio : le résultat est un bloc de données fixe, publié avec le
    graphe du moteur comme les échantillons (l'ancien est retiré plus tard, jamais libéré en cours
    de lecture).
*/

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace grv::engine
{

struct SpeechPhone
{
    float f1 = 500.0f, f2 = 1500.0f, f3 = 2500.0f; // formants (Hz)
    float g1 = 1.0f, g2 = 0.5f, g3 = 0.3f;         // gains des formants
    float nasal = 0.0f;                             // résonance nasale (250 Hz)
    float voice = 1.0f;                             // part voisée (source glottale)
    float noise = 0.0f, nf = 5000.0f, nq = 1.0f;    // bruit : niveau, fréquence, sélectivité
    float dur = 0.6f;                               // durée en unités (1 unité ≈ une syllabe au débit choisi)
    float pitch = 0.0f;                             // intonation (demi-tons)
    float amp = 1.0f;                               // force (mot appuyé, exclamation : v0.39)
    std::uint8_t kind = 0;                          // 0 voyelle, 1 sonante, 2 fricative, 3 plosive, 4 silence
    std::uint8_t word = 0;                          // numéro du mot dans le texte (affichage)
};

struct SpeechScript
{
    static constexpr int maxPhones = 320;
    int count = 0;
    std::array<SpeechPhone, maxPhones> ph {};
};

namespace speech
{
    // phonèmes
    enum P : int { A, E, EH, I, O, OH, U, Y, EU, OE, SCHWA, AN, ON, IN, J, W, WY,
                   PP, TT, KK, BB, DD, GG, FF, SS, CH, VV, ZZ, JJ, MM, NN, GN, LL, RR,
                   GAP, COMMA, STOP, PAUSE,
                   // v0.39 : sons de l'anglais (voyelles, « ng », « th » sourd et sonore, « h » soufflé, « r » anglais)
                   AE, AH, AO, UH, ER, IH, NG, TH, DH, HH, RE, numP };

    inline SpeechPhone def (int p)
    {
        SpeechPhone s;
        auto v = [&s] (float f1, float f2, float f3, float g1, float g2, float g3, float nasal = 0.0f)
        { s.f1 = f1; s.f2 = f2; s.f3 = f3; s.g1 = g1; s.g2 = g2; s.g3 = g3; s.nasal = nasal; s.voice = 1.0f; s.kind = 0; s.dur = 0.62f; };
        switch (p)
        {
            case A:     v (750, 1350, 2550, 1.0f, 0.75f, 0.35f); break;
            case E:     v (400, 2000, 2600, 1.0f, 0.60f, 0.40f); break;
            case EH:    v (560, 1750, 2500, 1.0f, 0.65f, 0.35f); break;
            case I:     v (290, 2250, 3000, 1.0f, 0.45f, 0.45f); s.dur = 0.55f; break;
            case O:     v (430, 820, 2450, 1.0f, 0.60f, 0.20f); break;
            case OH:    v (560, 960, 2450, 1.0f, 0.65f, 0.25f); break;
            case U:     v (310, 760, 2300, 1.0f, 0.50f, 0.15f); s.dur = 0.55f; break;
            case Y:     v (290, 1750, 2150, 1.0f, 0.45f, 0.35f); s.dur = 0.55f; break;
            case EU:    v (380, 1550, 2300, 1.0f, 0.55f, 0.30f); break;
            case OE:    v (520, 1450, 2450, 1.0f, 0.60f, 0.30f); break;
            case SCHWA: v (480, 1450, 2450, 0.9f, 0.50f, 0.25f); s.dur = 0.38f; break;
            case AN:    v (650, 1050, 2500, 0.85f, 0.40f, 0.15f, 0.6f); s.dur = 0.70f; break;
            case ON:    v (460, 820, 2400, 0.85f, 0.35f, 0.12f, 0.6f); s.dur = 0.70f; break;
            case IN:    v (600, 1550, 2500, 0.85f, 0.45f, 0.18f, 0.6f); s.dur = 0.70f; break;
            case J:     v (280, 2250, 3000, 0.7f, 0.40f, 0.40f); s.kind = 1; s.dur = 0.16f; break;
            case W:     v (300, 750, 2300, 0.7f, 0.40f, 0.10f); s.kind = 1; s.dur = 0.16f; break;
            case WY:    v (280, 1750, 2150, 0.7f, 0.40f, 0.30f); s.kind = 1; s.dur = 0.16f; break;
            case MM:    v (280, 1100, 2300, 0.45f, 0.12f, 0.06f, 1.0f); s.kind = 1; s.dur = 0.26f; break;
            case NN:    v (280, 1700, 2600, 0.45f, 0.12f, 0.06f, 1.0f); s.kind = 1; s.dur = 0.24f; break;
            case GN:    v (280, 2100, 2800, 0.45f, 0.15f, 0.08f, 1.0f); s.kind = 1; s.dur = 0.28f; break;
            case LL:    v (360, 1300, 2700, 0.75f, 0.35f, 0.25f); s.kind = 1; s.dur = 0.22f; break;
            case RR:    v (520, 1250, 2400, 0.65f, 0.35f, 0.15f); s.kind = 1; s.dur = 0.22f; s.noise = 0.12f; s.nf = 1600.0f; s.nq = 1.2f; break;
            case FF:    v (400, 1400, 2500, 0, 0, 0); s.voice = 0.0f; s.noise = 0.30f; s.nf = 5500.0f; s.nq = 0.5f; s.kind = 2; s.dur = 0.32f; break;
            case SS:    v (400, 1700, 2600, 0, 0, 0); s.voice = 0.0f; s.noise = 0.60f; s.nf = 6500.0f; s.nq = 1.6f; s.kind = 2; s.dur = 0.34f; break;
            case CH:    v (400, 1800, 2600, 0, 0, 0); s.voice = 0.0f; s.noise = 0.55f; s.nf = 2900.0f; s.nq = 1.4f; s.kind = 2; s.dur = 0.34f; break;
            case VV:    v (300, 1200, 2400, 0.35f, 0.15f, 0.05f); s.voice = 0.45f; s.noise = 0.18f; s.nf = 5000.0f; s.nq = 0.5f; s.kind = 2; s.dur = 0.26f; break;
            case ZZ:    v (300, 1600, 2500, 0.35f, 0.15f, 0.05f); s.voice = 0.45f; s.noise = 0.40f; s.nf = 6500.0f; s.nq = 1.6f; s.kind = 2; s.dur = 0.28f; break;
            case JJ:    v (300, 1800, 2500, 0.35f, 0.15f, 0.05f); s.voice = 0.45f; s.noise = 0.35f; s.nf = 2900.0f; s.nq = 1.4f; s.kind = 2; s.dur = 0.28f; break;
            // plosives : occlusion (silence, ou murmure grave pour les sonores) puis explosion de bruit
            case PP:    v (400, 1000, 2400, 0, 0, 0); s.voice = 0.0f; s.noise = 0.45f; s.nf = 1200.0f; s.nq = 0.7f; s.kind = 3; s.dur = 0.30f; break;
            case TT:    v (400, 1800, 2600, 0, 0, 0); s.voice = 0.0f; s.noise = 0.55f; s.nf = 4200.0f; s.nq = 1.0f; s.kind = 3; s.dur = 0.30f; break;
            case KK:    v (400, 1600, 2500, 0, 0, 0); s.voice = 0.0f; s.noise = 0.55f; s.nf = 2200.0f; s.nq = 1.4f; s.kind = 3; s.dur = 0.32f; break;
            case BB:    v (250, 900, 2300, 0.30f, 0.05f, 0.02f); s.voice = 0.30f; s.noise = 0.22f; s.nf = 1200.0f; s.nq = 0.7f; s.kind = 3; s.dur = 0.26f; break;
            case DD:    v (250, 1600, 2500, 0.30f, 0.05f, 0.02f); s.voice = 0.30f; s.noise = 0.28f; s.nf = 4000.0f; s.nq = 1.0f; s.kind = 3; s.dur = 0.26f; break;
            case GG:    v (250, 1500, 2400, 0.30f, 0.05f, 0.02f); s.voice = 0.30f; s.noise = 0.28f; s.nf = 2200.0f; s.nq = 1.4f; s.kind = 3; s.dur = 0.28f; break;
            case GAP:   v (480, 1450, 2450, 0, 0, 0); s.voice = 0.0f; s.kind = 4; s.dur = 0.10f; break;
            case COMMA: v (480, 1450, 2450, 0, 0, 0); s.voice = 0.0f; s.kind = 4; s.dur = 0.70f; break;
            case PAUSE: v (480, 1450, 2450, 0, 0, 0); s.voice = 0.0f; s.kind = 4; s.dur = 0.35f; break; // durée réglée à la lecture
            case AE:    v (660, 1720, 2410, 1.0f, 0.65f, 0.35f); break;
            case AH:    v (640, 1190, 2390, 1.0f, 0.60f, 0.30f); s.dur = 0.50f; break;
            case AO:    v (570, 840, 2410, 1.0f, 0.60f, 0.22f); break;
            case UH:    v (440, 1020, 2240, 1.0f, 0.55f, 0.20f); s.dur = 0.45f; break;
            case ER:    v (490, 1350, 1690, 1.0f, 0.60f, 0.35f); s.dur = 0.60f; break;
            case IH:    v (390, 1990, 2550, 1.0f, 0.50f, 0.40f); s.dur = 0.45f; break;
            case NG:    v (280, 2300, 2750, 0.45f, 0.12f, 0.06f, 1.0f); s.kind = 1; s.dur = 0.26f; break;
            case TH:    v (400, 1400, 2500, 0, 0, 0); s.voice = 0.0f; s.noise = 0.18f; s.nf = 4500.0f; s.nq = 0.5f; s.kind = 2; s.dur = 0.30f; break;
            case DH:    v (300, 1400, 2500, 0.40f, 0.15f, 0.05f); s.voice = 0.50f; s.noise = 0.12f; s.nf = 4500.0f; s.nq = 0.5f; s.kind = 2; s.dur = 0.20f; break;
            case HH:    v (500, 1500, 2500, 0, 0, 0); s.voice = 0.0f; s.noise = 0.22f; s.nf = 1600.0f; s.nq = 0.35f; s.kind = 2; s.dur = 0.20f; break;
            case RE:    v (420, 1300, 1600, 0.70f, 0.40f, 0.25f); s.kind = 1; s.dur = 0.22f; break;
            default:    v (480, 1450, 2450, 0, 0, 0); s.voice = 0.0f; s.kind = 4; s.dur = 1.30f; break;
        }
        return s;
    }

    inline bool isVowelP (int p) noexcept { p = std::abs (p); return p <= IN || (p >= AE && p <= IH); }
    inline bool isSilenceP (int p) noexcept { p = std::abs (p); return p >= GAP && p <= PAUSE; }

    /** UTF-8 → points de code (minuscules ; les caractères invalides sont ignorés).
        upper (facultatif) : vrai pour chaque lettre écrite en capitale (v0.39 : mots appuyés). */
    inline std::u32string decode (const std::string& in, std::vector<bool>* upper = nullptr)
    {
        std::u32string out;
        for (size_t i = 0; i < in.size();)
        {
            const auto c = (unsigned char) in[i];
            char32_t cp = 0; int n = 0;
            if (c < 0x80) { cp = c; n = 1; }
            else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
            else { ++i; continue; }
            if (i + (size_t) n > in.size()) break;
            bool ok = true;
            for (int k = 1; k < n; ++k) { const auto b = (unsigned char) in[i + (size_t) k]; if ((b & 0xC0) != 0x80) ok = false; cp = (cp << 6) | (b & 0x3F); }
            i += ok ? (size_t) n : 1;
            if (! ok) continue;
            bool up = false;
            if (cp >= 'A' && cp <= 'Z') { cp += 32; up = true; }
            else if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) { cp += 32; up = true; }   // À…Þ → à…þ
            else if (cp == 0x152) { cp = 0x153; up = true; }                          // Œ → œ
            else if (cp == 0x2019 || cp == 0x2018 || cp == 0xB4 || cp == 0x60) cp = '\'';
            out.push_back (cp);
            if (upper != nullptr) upper->push_back (up);
        }
        return out;
    }

    inline bool isLetter (char32_t c) noexcept
    {
        return (c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFF && c != 0xF7) || c == 0x153;
    }
    inline bool isVowelL (char32_t c) noexcept
    {
        switch (c)
        {
            case 'a': case 'e': case 'i': case 'o': case 'u': case 'y':
            case 0xE0: case 0xE2: case 0xE4: case 0xE9: case 0xE8: case 0xEA: case 0xEB: case 0xEE: case 0xEF:
            case 0xF4: case 0xF6: case 0xFB: case 0xF9: case 0xFC: case 0x153: case 0xE6:
                return true;
            default: return false;
        }
    }
    /** e, i, y (et e accentués) : « c » et « g » s'adoucissent devant. */
    inline bool isFront (char32_t c) noexcept { return c == 'e' || c == 'i' || c == 'y' || c == 0xE9 || c == 0xE8 || c == 0xEA || c == 0xEB || c == 0xEE || c == 0xEF; }

    /** Lecture d'un mot (lettres en minuscules) → phonèmes. */
    inline void readWord (const std::u32string& w, std::vector<int>& out)
    {
        const int n = (int) w.size();
        auto at = [&] (int i) -> char32_t { return i >= 0 && i < n ? w[(size_t) i] : 0; };
        int vowels = 0;
        for (auto c : w) if (isVowelL (c)) ++vowels;
        // mots-outils courants dont la lecture par règles serait fausse
        {
            std::u32string s = w;
            auto is = [&s] (const char* t) { std::u32string x; for (const char* p = t; *p; ++p) x.push_back ((char32_t) (unsigned char) *p); return s == x; };
            if (is ("les") || is ("des") || is ("mes") || is ("tes") || is ("ses") || is ("ces"))
            { out.push_back (w[0] == 'l' ? LL : w[0] == 'd' ? DD : w[0] == 'm' ? MM : w[0] == 't' ? TT : SS); out.push_back (E); return; }
            if (is ("est")) { out.push_back (EH); return; }
            if (is ("et")) { out.push_back (E); return; }
            if (is ("monsieur")) { out.push_back (MM); out.push_back (SCHWA); out.push_back (SS); out.push_back (J); out.push_back (EU); return; }
            if (is ("femme")) { out.push_back (FF); out.push_back (A); out.push_back (MM); return; }
            if (is ("eu")) { out.push_back (Y); return; }
            if (is ("oignon")) { out.push_back (OH); out.push_back (GN); out.push_back (ON); return; }
        }
        // lettre finale muette ? (consonne finale s t d x z p g, e final, « es » final)
        auto lastSounding = [&]
        {
            int e = n;
            if (e > 2 && (w[(size_t) e - 1] == 's' || w[(size_t) e - 1] == 'x')) --e;  // pluriels
            if (e > 1 && w[(size_t) e - 1] == 'e' && vowels > 1) --e;                   // e muet final
            else if (e > 1 && vowels > 0)
            {
                const auto c = w[(size_t) e - 1];                                         // consonne finale muette
                if (c == 's' || c == 't' || c == 'd' || c == 'x' || c == 'z' || c == 'p' || c == 'g') --e;
            }
            return e;
        }();
        const int end = std::max (0, lastSounding);

        for (int i = 0; i < end;)
        {
            const auto c = at (i), c1 = at (i + 1), c2 = at (i + 2), c3 = at (i + 3);
            const bool nextIsVowel1 = isVowelL (c1);
            auto nasalAhead = [&] (int k) // « n » ou « m » en k, suivi d'une consonne ou de la fin du mot (pas doublé, pas de voyelle)
            {
                const auto m = at (k);
                if (m != 'n' && m != 'm') return false;
                const auto after = at (k + 1);
                if (after == m) return false;
                if (k + 1 >= n) return true;
                return ! isVowelL (after) && after != 'h';
            };
            // ---- voyelles composées ----
            if (c == 'e' && c1 == 'a' && c2 == 'u') { out.push_back (O); i += 3; continue; }
            if (c == 'a' && c1 == 'u') { out.push_back (O); i += 2; continue; }
            if (c == 'o' && (c1 == 'u' || c1 == 0xF9 || c1 == 0xFB)) { out.push_back (U); i += 2; continue; }
            if (c == 'o' && c1 == 'i' && nasalAhead (i + 2)) { out.push_back (W); out.push_back (IN); i += 3; continue; }
            if (c == 'o' && (c1 == 'i' || c1 == 0xEE)) { out.push_back (W); out.push_back (A); i += 2; continue; }
            if (c == 'o' && c1 == 'y' && isVowelL (c2)) { out.push_back (W); out.push_back (A); out.push_back (J); i += 2; continue; }
            if ((c == 'a' || c == 'e') && c1 == 'i' && nasalAhead (i + 2)) { out.push_back (IN); i += 3; continue; }
            if ((c == 'a' || c == 'e') && c1 == 'i' && c2 == 'l' && (c3 == 'l' || i + 3 >= n)) { out.push_back (c == 'a' ? A : EH); out.push_back (J); i += c3 == 'l' ? 4 : 3; continue; }
            if ((c == 'a' && (c1 == 'i' || c1 == 0xEE)) || (c == 'e' && c1 == 'i')) { out.push_back (EH); i += 2; continue; }
            if (c == 'a' && c1 == 'y' && isVowelL (c2)) { out.push_back (EH); out.push_back (J); i += 2; continue; }
            if ((c == 'e' && c1 == 'u') || (c == 0x153 && c1 == 'u')) { out.push_back (i + 2 >= end ? EU : OE); i += 2; continue; }
            if (c == 0x153) { out.push_back (OE); i += 1; continue; }
            if (c == 'u' && c1 == 'i' && ! nasalAhead (i + 2)) { out.push_back (WY); out.push_back (I); i += 2; continue; }
            if (c == 'i' && c1 == 'e' && nasalAhead (i + 2)) { out.push_back (J); out.push_back (IN); i += 3; continue; }
            if (c == 'i' && c1 == 'l' && c2 == 'l' && i > 0) { if (! isVowelL (at (i - 1))) out.push_back (I); out.push_back (J); i += 3; continue; }
            // ---- voyelles nasales ----
            if ((c == 'a' || c == 'e') && nasalAhead (i + 1)) { out.push_back (AN); i += 2; continue; }
            if (c == 'o' && nasalAhead (i + 1)) { out.push_back (ON); i += 2; continue; }
            if ((c == 'i' || c == 'y' || c == 'u') && nasalAhead (i + 1)) { out.push_back (IN); i += 2; continue; }
            // ---- voyelles simples ----
            if (c == 'a' || c == 0xE0 || c == 0xE2 || c == 0xE4) { out.push_back (A); ++i; continue; }
            if (c == 0xE9 || c == 0xE6) { out.push_back (E); ++i; continue; }
            if (c == 0xE8 || c == 0xEA || c == 0xEB) { out.push_back (EH); ++i; continue; }
            if (c == 'e')
            {
                // « er », « ez » en fin de mot : é ; e devant deux consonnes ou une consonne finale : è
                if ((c1 == 'r' || c1 == 'z') && i + 2 >= n && n > 2) { out.push_back (E); i += 2; continue; }
                if (i + 1 >= end) { out.push_back (vowels <= 1 ? SCHWA : EH); ++i; continue; }
                const bool twoCons = c1 != 0 && ! isVowelL (c1) && c2 != 0 && ! isVowelL (c2) && ! (c2 == 'r' || c2 == 'l') && c1 != 'h';
                const bool finalCons = c1 != 0 && ! isVowelL (c1) && i + 2 >= end;
                out.push_back (twoCons || finalCons ? EH : SCHWA);
                ++i; continue;
            }
            if (c == 'i' || c == 0xEE || c == 0xEF) { if (isVowelL (c1) && i > 0 && ! (c1 == 'e' && i + 2 >= end)) out.push_back (J); else out.push_back (I); ++i; continue; }
            if (c == 'y') { out.push_back (isVowelL (c1) && i > 0 ? J : I); ++i; continue; }
            if (c == 'o' || c == 0xF4 || c == 0xF6) { out.push_back (c == 0xF4 || i + 1 >= end || (c1 == 's' && i + 2 >= n) ? O : OH); ++i; continue; }
            if (c == 'u' || c == 0xF9 || c == 0xFB || c == 0xFC) { out.push_back (Y); ++i; continue; }
            // ---- consonnes ----
            int skip = 1;
            if (c == c1 && c != 'c') skip = 2; // consonnes doublées
            switch (c)
            {
                case 'b': out.push_back (BB); break;
                case 'c':
                    if (c1 == 'h') { out.push_back (CH); skip = 2; }
                    else if (c1 == 'c' && isFront (c2)) { out.push_back (KK); out.push_back (SS); skip = 2; }
                    else if (c1 == 'c') { out.push_back (KK); skip = 2; }
                    else out.push_back (isFront (c1) ? SS : KK);
                    break;
                case 0xE7: out.push_back (SS); break;
                case 'd': out.push_back (DD); break;
                case 'f': out.push_back (FF); break;
                case 'g':
                    if (c1 == 'n') { out.push_back (GN); skip = 2; }
                    else if (c1 == 'u' && isFront (c2)) { out.push_back (GG); skip = 2; }
                    else out.push_back (isFront (c1) ? JJ : GG);
                    break;
                case 'h': break;
                case 'j': out.push_back (JJ); break;
                case 'k': out.push_back (KK); break;
                case 'l': out.push_back (LL); break;
                case 'm': out.push_back (MM); break;
                case 'n': out.push_back (NN); break;
                case 'p': if (c1 == 'h') { out.push_back (FF); skip = 2; } else out.push_back (PP); break;
                case 'q': out.push_back (KK); if (c1 == 'u') skip = 2; break;
                case 'r': out.push_back (RR); break;
                case 's':
                    if (c1 == 'c' && isFront (c2)) { out.push_back (SS); skip = 2; }
                    else if (c1 == 'h') { out.push_back (CH); skip = 2; }
                    else if (c1 == 's') { out.push_back (SS); skip = 2; }
                    else out.push_back (i > 0 && isVowelL (at (i - 1)) && nextIsVowel1 ? ZZ : SS);
                    break;
                case 't':
                    if (c1 == 'i' && c2 == 'o' && c3 == 'n' && i > 0) { out.push_back (SS); out.push_back (J); out.push_back (ON); skip = 4; }
                    else { out.push_back (TT); if (c1 == 'h') skip = 2; }
                    break;
                case 'v': out.push_back (VV); break;
                case 'w': out.push_back (W); break;
                case 'x': if (i == 0) { out.push_back (GG); out.push_back (ZZ); } else { out.push_back (KK); out.push_back (SS); } break;
                case 'z': out.push_back (ZZ); break;
                default: break;
            }
            i += skip;
        }
    }

    inline const char* digitWord (char32_t d)
    {
        static const char* names[10] { "zéro", "un", "deux", "trois", "quatre", "cinq", "six", "sept", "huit", "neuf" };
        return names[d - '0'];
    }
}

#include "SpeechEn.h" // v0.39 : lecture de l'anglais (dans grv::engine)

/** Langue d'une phrase : « @en » en tête = anglais (v0.39), sinon français. */
inline bool speechIsEnglish (const std::string& text) noexcept { return text.rfind ("@en ", 0) == 0 || text == "@en"; }
inline std::string speechBody (const std::string& text) { return speechIsEnglish (text) ? text.substr (std::min<size_t> (4, text.size())) : text; }

/** Texte → phonèmes, durées, intonation. Retourne nullptr si le texte ne donne aucun son. */
inline std::unique_ptr<SpeechScript> compileSpeech (const std::string& text)
{
    using namespace speech;
    const bool english = speechIsEnglish (text);
    std::vector<bool> upper0;
    auto cps0 = decode (speechBody (text).substr (0, 400), &upper0);
    // chiffres lus un par un ; « … » = trois points
    std::u32string cps; std::vector<bool> upper;
    for (size_t k = 0; k < cps0.size(); ++k)
    {
        const auto c = cps0[k];
        if (c >= '0' && c <= '9') { cps.push_back (' '); upper.push_back (false); for (auto d : decode (english ? speech_en::digitWord (c) : digitWord (c))) { cps.push_back (d); upper.push_back (false); } cps.push_back (' '); upper.push_back (false); }
        else if (c == 0x2026) { for (int i = 0; i < 3; ++i) { cps.push_back ('.'); upper.push_back (false); } }
        else { cps.push_back (c); upper.push_back (upper0[k]); }
    }
    // élément de la phrase : phonème (négatif : fin de groupe en question), mot, durée et force propres
    struct Item { int p; int word; float durMul = 1.0f; float amp = 1.0f; float pitchAdd = 0.0f; bool emph = false; bool wordEnd = false; };
    enum End { endNone = 0, endComma, endStop, endQuestion, endExclaim, endSuspend };
    std::vector<Item> seq;
    std::vector<int> groupEnd; // pour chaque élément de fin de groupe : son genre (End) et sa force
    std::vector<int> groupForce;
    int wordNo = 0;
    std::u32string word; int wordUpper = 0, wordLetters = 0;
    std::vector<std::pair<int, int>> stretch; // (rang de la voyelle dans le mot, répétitions)
    bool joined = false; // élision (l', d', qu') : pas de pause entre les deux mots
    auto isSilence = [] (int p) { return isSilenceP (p); };
    auto flush = [&]
    {
        if (word.empty()) return;
        std::vector<int> ph;
        if (english) speech_en::readWord (word, ph); else readWord (word, ph);
        if (! ph.empty())
        {
            if (! seq.empty() && ! isSilence (seq.back().p) && ! joined) seq.push_back ({ GAP, wordNo });
            const bool emph = wordLetters >= 2 && wordUpper == wordLetters;
            const auto first = seq.size();
            for (auto p : ph) seq.push_back ({ p, std::min (wordNo, 255), 1.0f, emph ? 1.3f : 1.0f, 0.0f, emph });
            // voyelles étirées : la k-ième voyelle du mot (dans l'ordre d'écriture)
            for (auto [rank, reps] : stretch)
            {
                int v = -1, seen = 0;
                for (size_t q = first; q < seq.size(); ++q) if (isVowelP (seq[q].p)) { v = (int) q; if (seen++ == rank) break; }
                if (v >= 0) seq[(size_t) v].durMul *= std::min (6.0f, 1.0f + 0.75f * (float) (reps - 2));
            }
            seq.back().wordEnd = true;
        }
        word.clear(); wordUpper = wordLetters = 0; stretch.clear();
    };
    auto endGroup = [&] (int kind, int force, float pauseUnits)
    {
        if (seq.empty()) return;
        // fin de groupe placée juste après le dernier son (avant les silences d'espaces déjà posés) ;
        // si le groupe est déjà fermé, on prolonge seulement le silence
        size_t at = seq.size();
        while (at > 0 && (std::abs (seq[at - 1].p) == GAP || std::abs (seq[at - 1].p) == PAUSE)) --at;
        if (at == 0 || std::abs (seq[at - 1].p) == COMMA || std::abs (seq[at - 1].p) == STOP)
        {
            if (pauseUnits > 0.0f) seq.push_back ({ PAUSE, wordNo, pauseUnits / 0.35f });
            return;
        }
        const int code = kind == endComma || kind == endSuspend ? COMMA : STOP;
        Item e { code, seq[at - 1].word, kind == endSuspend ? pauseUnits / 0.70f : 1.0f };
        e.pitchAdd = (float) (kind * 10 + std::min (force, 3)); // genre et force, relus plus bas
        seq.insert (seq.begin() + (std::ptrdiff_t) at, e);
    };
    int textWord = 0; // numéro du mot tel qu'affiché (séparé par des espaces)
    bool inWord = false;
    for (size_t k = 0; k < cps.size(); ++k)
    {
        const auto c = cps[k];
        if (isLetter (c))
        {
            if (! inWord) inWord = true;
            ++wordLetters; if (upper[k]) ++wordUpper;
            // voyelle répétée 3 fois ou plus : une seule lettre, étirée
            if (isVowelL (c))
            {
                size_t r = k; while (r + 1 < cps.size() && cps[r + 1] == c) ++r;
                const auto reps = (int) (r - k + 1);
                if (reps >= 3)
                {
                    int rank = 0; bool inV = false;
                    for (auto x : word) { const bool v = isVowelL (x); if (v && ! inV) ++rank; inV = v; }
                    if (! word.empty() && isVowelL (word.back())) --rank; // même groupe de voyelles
                    stretch.push_back ({ std::max (0, rank), reps });
                    for (size_t q = k + 1; q <= r; ++q) { ++wordLetters; if (upper[q]) ++wordUpper; }
                    k = r;
                }
            }
            word.push_back (c); wordNo = textWord; continue;
        }
        if (c == '\'')
        {
            // anglais : l'apostrophe reste dans le mot (don't, I'm) ; français : élision (l', d', qu')
            if (english && ! word.empty() && k + 1 < cps.size() && isLetter (cps[k + 1])) { word.push_back (c); continue; }
            flush(); joined = ! english; continue;
        }
        flush();
        joined = false;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '-')
        {
            if (inWord) { inWord = false; ++textWord; }
            if (c == '\n') { endGroup (endComma, 1, 0.0f); continue; }
            // plusieurs espaces : un silence qui grandit (0,35 unité par espace en plus)
            if (c == ' ' || c == '\t')
            {
                size_t r = k; while (r + 1 < cps.size() && (cps[r + 1] == ' ' || cps[r + 1] == '\t')) ++r;
                const auto extra = (int) (r - k);
                k = r;
                if (extra > 0 && ! seq.empty()) seq.push_back ({ PAUSE, wordNo, std::min (24.0f, (float) extra) });
            }
            continue;
        }
        if (inWord) { inWord = false; ++textWord; }
        if (c == ',' || c == ';' || c == ':') { endGroup (endComma, 1, 0.0f); continue; }
        if (c == '.' || c == '!' || c == '?')
        {
            // suite de ponctuation : « ... », « !! », « ?! »…
            int dots = 0, bangs = 0, quest = 0;
            size_t r = k;
            for (; r < cps.size() && (cps[r] == '.' || cps[r] == '!' || cps[r] == '?' || cps[r] == 0x2026); ++r)
                (cps[r] == '!' ? bangs : cps[r] == '?' ? quest : dots)++;
            k = r - 1;
            if (quest > 0) endGroup (endQuestion, quest + bangs, 0.0f);
            else if (bangs > 0) endGroup (endExclaim, bangs, 0.0f);
            else if (dots >= 2) endGroup (endSuspend, dots, std::min (8.0f, 0.45f * (float) dots + 0.4f));
            else endGroup (endStop, 1, 0.0f);
        }
    }
    flush();
    while (! seq.empty() && isSilence (seq.back().p) && std::abs (seq.back().p) != STOP) seq.pop_back();
    if (seq.empty()) return nullptr;
    if (std::abs (seq.back().p) != STOP) { seq.push_back ({ STOP, wordNo }); seq.back().pitchAdd = (float) (endStop * 10 + 1); }
    if (std::abs (seq.back().p) == STOP && seq.back().pitchAdd < 1.0f) seq.back().pitchAdd = (float) (endStop * 10 + 1);

    auto s = std::make_unique<SpeechScript>();
    // groupes de souffle (jusqu'à une virgule ou une fin) : déclinaison, accents de mots, accent final
    size_t start = 0;
    int sentence = 0;
    for (size_t k = 0; k < seq.size() && s->count < SpeechScript::maxPhones; ++k)
    {
        const auto p = std::abs (seq[k].p);
        if (p != COMMA && p != STOP) continue;
        const auto code = (int) std::lround (seq[k].pitchAdd);
        const int kind = code / 10, force = std::max (1, code % 10);
        const bool question = kind == endQuestion, exclaim = kind == endExclaim, suspend = kind == endSuspend;
        // voyelles du groupe
        std::vector<size_t> vow;
        for (size_t q = start; q < k; ++q) if (isVowelP (seq[q].p)) vow.push_back (q);
        // déclinaison : de +2 à -2,5 demi-tons (exclamation : de +4 à -3 ; phrases suivantes un peu décalées)
        const double top = (exclaim ? 3.0 + force : 2.0) + (sentence % 2 == 1 ? 0.7 : 0.0), slope = exclaim ? 7.0 : 4.5;
        for (size_t q = start; q <= k && s->count < SpeechScript::maxPhones; ++q)
        {
            const auto pq = std::abs (seq[q].p);
            auto ph = def (pq);
            ph.word = (std::uint8_t) std::clamp (seq[q].word, 0, 255);
            if (pq == PAUSE) ph.dur *= seq[q].durMul;
            else if (! isSilenceP (pq)) ph.dur *= seq[q].durMul;
            else if (suspend && q == k) ph.dur *= seq[q].durMul;
            ph.amp = seq[q].amp;
            const auto rank = vow.empty() ? 0 : (std::find (vow.begin(), vow.end(), q) - vow.begin());
            const auto frac = vow.size() > 1 ? (double) rank / (double) (vow.size() - 1) : 0.0;
            float pitch = (float) (top - slope * std::clamp (frac, 0.0, 1.0));
            if (isVowelP (pq))
            {
                // dernière voyelle d'un mot (hors fin de groupe) : petit accent montant
                size_t e = q; while (e + 1 < k && ! isSilenceP (seq[e + 1].p) && ! isVowelP (seq[e + 1].p)) ++e;
                const bool lastOfWord = seq[e].wordEnd && q != vow.back();
                if (lastOfWord) { pitch += 1.3f; ph.dur *= 1.08f; }
                if (seq[q].emph) { pitch += 4.0f; ph.dur *= 1.3f; }
            }
            if (! vow.empty() && q == vow.back())
            {
                ph.dur *= 1.45f; // accent final du groupe (français) : dernière voyelle allongée
                pitch = p == COMMA && ! suspend ? 2.5f
                      : suspend ? (float) (top - slope * 0.45 + 0.5)                // voix en l'air
                      : question ? 3.5f + 1.5f * (float) force
                      : exclaim ? -2.5f - 1.0f * (float) force : -3.5f;
                if (exclaim) { ph.dur *= 1.2f; ph.amp *= 1.0f + 0.15f * (float) force; }
                if (seq[q].emph) pitch += 2.0f;
            }
            if (! isVowelP (pq)) pitch = s->count > 0 ? s->ph[(size_t) s->count - 1].pitch : pitch;
            ph.pitch = pitch;
            s->ph[(size_t) s->count++] = ph;
        }
        if (p == STOP) ++sentence;
        start = k + 1;
    }
    return s->count > 0 ? std::move (s) : nullptr;
}

} // namespace grv::engine
