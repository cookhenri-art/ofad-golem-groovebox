/*
    Groovebox — OFAD — AGPL-3.0-or-later

    v0.39 : lecture de l'anglais pour la voix interne des vocodeurs (demande d'Henrico : « choisir
    anglais ou français »). Inclus par Speech.h, à l'intérieur de grv::engine (pas d'en-tête propre).

    Principe : règles lettres → sons de l'anglais à contexte gauche / droit (méthode du rapport du
    Naval Research Laboratory de 1976, « Automatic translation of English text to phonetics by means
    of letter-to-sound rules », document public américain), réécrites ici sous une forme réduite, plus
    un petit lexique des mots les plus courants dont la lecture par règles serait fausse. Les sons
    sont ceux de l'anglais américain courant, rendus avec les phonèmes du synthétiseur à formants
    (les diphtongues : voyelle + glissante).

    Notation des contextes (gauche lu de droite à gauche) :
      ' ' bord du mot · '#' une voyelle ou plus · ':' zéro consonne ou plus · '*' une consonne ou plus
      '^' une consonne · '.' une consonne sonore (B D V G J L M N R W Z) · '+' E, I ou Y
      '%' suffixe (ER, E, ES, ED, ING, ELY) · '&' sifflante (S C G Z X J CH SH)
      '@' T S R D L Z N J TH CH SH (le U long devient « ou »).
*/

namespace speech_en
{
    using namespace speech;

    struct Rule { const char* left; const char* match; const char* right; const char* out; };

    // Règles par lettre, dans l'ordre d'essai (la première qui convient est prise)
    inline const std::vector<Rule>& rules()
    {
        static const std::vector<Rule> r {
            // A
            { "", "A", " ", "AX" }, { " ", "ARE", " ", "AA R" }, { " ", "AR", "O", "AX R" }, { "", "AR", "#", "EH R" },
            { "^", "AS", "#", "EY S" }, { "", "A", "WA", "AX" }, { "", "AW", "", "AO" }, { " :", "ANY", "", "EH N IY" },
            { "", "A", "^+#", "EY" }, { "#:", "ALLY", "", "AX L IY" }, { " ", "AL", "#", "AX L" }, { "", "AGAIN", "", "AX G EH N" },
            { "#:", "AG", "E", "IH JH" }, { "", "A", "^+:#", "AE" }, { " :", "A", "^+ ", "EY" }, { "", "A", "^%", "EY" },
            { " ", "ARR", "", "AX R" }, { "", "ARR", "", "AE R" }, { " :", "AR", " ", "AA R" }, { "", "AR", " ", "ER" },
            { "", "AR", "", "AA R" }, { "", "AIR", "", "EH R" }, { "", "AI", "", "EY" }, { "", "AY", "", "EY" },
            { "", "AU", "", "AO" }, { "#:", "AL", " ", "AX L" }, { "#:", "ALS", " ", "AX L Z" }, { "", "ALK", "", "AO K" },
            { "", "AL", "^", "AO L" }, { " :", "ABLE", "", "EY B AX L" }, { "", "ABLE", "", "AX B AX L" }, { "", "ANG", "+", "EY N JH" },
            { "", "A", "", "AE" },
            // B
            { " ", "BE", "^#", "B IH" }, { "", "BEING", "", "B IY IH NG" }, { " ", "BOTH", " ", "B OW TH" }, { " ", "BUS", "#", "B IH Z" },
            { "", "BUIL", "", "B IH L" }, { "", "B", "", "B" },
            // C
            { " ", "CH", "^", "K" }, { "^E", "CH", "", "K" }, { "", "CH", "", "CH" }, { " S", "CI", "#", "S AY" },
            { "", "CI", "A", "SH" }, { "", "CI", "O", "SH" }, { "", "CI", "EN", "SH" }, { "", "C", "+", "S" },
            { "", "CK", "", "K" }, { "", "COM", "%", "K AH M" }, { "", "C", "", "K" },
            // D
            { "#:", "DED", " ", "D IH D" }, { ".E", "D", " ", "D" }, { "#:^E", "D", " ", "T" }, { " ", "DE", "^#", "D IH" },
            { " ", "DO", " ", "D UW" }, { " ", "DOES", "", "D AH Z" }, { " ", "DOING", "", "D UW IH NG" }, { " ", "DOW", "", "D AW" },
            { "", "DU", "A", "JH UW" }, { "", "D", "", "D" },
            // E
            { "#:", "E", " ", "" }, { " :", "E", " ", "IY" }, { "#", "ED", " ", "D" }, { "#:", "E", "D ", "" },
            { "", "EV", "ER", "EH V" }, { "", "E", "^%", "IY" }, { "", "ERI", "#", "IY R IY" }, { "", "ERI", "", "EH R IH" },
            { "#:", "ER", "#", "ER" }, { "", "ER", "#", "EH R" }, { "", "ER", "", "ER" }, { " ", "EVEN", "", "IY V EH N" },
            { "#:", "E", "W", "" }, { "@", "EW", "", "UW" }, { "", "EW", "", "Y UW" }, { "", "E", "O", "IY" },
            { "#:&", "ES", " ", "IH Z" }, { "#:", "E", "S ", "" }, { "#:", "ELY", " ", "L IY" }, { "#:", "EMENT", "", "M EH N T" },
            { "", "EFUL", "", "F UH L" }, { "", "EE", "", "IY" }, { "", "EARN", "", "ER N" }, { " ", "EAR", "^", "ER" },
            { "", "EAD", "", "EH D" }, { "#:", "EA", " ", "IY AX" }, { "", "EA", "SU", "EH" }, { "", "EA", "", "IY" },
            { "", "EIGH", "", "EY" }, { "", "EI", "", "IY" }, { " ", "EYE", "", "AY" }, { "", "EY", "", "IY" },
            { "", "EU", "", "Y UW" }, { "", "E", "", "EH" },
            // F
            { "", "FUL", "", "F UH L" }, { "", "F", "", "F" },
            // G
            { "", "GIV", "", "G IH V" }, { " ", "G", "I^", "G" }, { "", "GE", "T", "G EH" }, { "SU", "GGES", "", "G JH EH S" },
            { "", "GG", "", "G" }, { " B#", "G", "", "G" }, { "", "G", "+", "JH" }, { "", "GREAT", "", "G R EY T" },
            { "#", "GH", "", "" }, { "", "G", "", "G" },
            // H
            { " ", "HAV", "", "HH AE V" }, { " ", "HERE", "", "HH IY R" }, { " ", "HOUR", "", "AW ER" }, { "", "HOW", "", "HH AW" },
            { "", "H", "#", "HH" }, { "", "H", "", "" },
            // I
            { "", "IEV", "", "IY V" }, { " ", "IN", "", "IH N" }, { " ", "I", " ", "AY" }, { "", "IN", "D", "AY N" }, { "", "IER", "", "IY ER" },
            { "#:R", "IED", "", "IY D" }, { "", "IED", " ", "AY D" }, { "", "IEN", "", "IY EH N" }, { "", "IE", "T", "AY EH" },
            { " :", "I", "%", "AY" }, { "", "I", "%", "IY" }, { "", "IE", "", "IY" }, { "", "I", "^+:#", "IH" },
            { "", "IR", "#", "AY R" }, { "", "IZ", "%", "AY Z" }, { "", "IS", "%", "AY Z" }, { "", "I", "D%", "AY" },
            { "+^", "I", "^+", "IH" }, { "", "I", "T%", "AY" }, { "#:^", "I", "^+", "IH" }, { "", "IR", "", "ER" },
            { "", "IGH", "", "AY" }, { "", "ILD", "", "AY L D" }, { "", "IGN", " ", "AY N" }, { "", "IGN", "^", "AY N" },
            { "", "IGN", "%", "AY N" }, { "", "IQUE", "", "IY K" }, { "", "I", "", "IH" },
            // J, K
            { "", "J", "", "JH" }, { " ", "K", "N", "" }, { "", "K", "", "K" },
            // L
            { "", "LO", "C#", "L OW" }, { "L", "L", "", "" }, { "#:^", "L", "%", "AX L" }, { "", "LEAD", "", "L IY D" }, { "", "L", "", "L" },
            // M
            { "", "MOV", "", "M UW V" }, { "", "M", "", "M" },
            // N
            { "E", "NG", "+", "N JH" }, { "", "NG", "R", "NG G" }, { "", "NG", "#", "NG G" }, { "", "NGL", "%", "NG G AX L" },
            { "", "NG", "", "NG" }, { "", "NK", "", "NG K" }, { " ", "NOW", " ", "N AW" }, { "", "N", "", "N" },
            // O
            { "", "OF", " ", "AX V" }, { "", "OROUGH", "", "ER OW" }, { "#:", "OR", " ", "ER" }, { "#:", "ORS", " ", "ER Z" },
            { "", "OR", "", "AO R" }, { " ", "ONE", "", "W AH N" }, { "", "OW", "", "OW" }, { " ", "OVER", "", "OW V ER" },
            { "", "OV", "", "AH V" }, { "", "O", "^%", "OW" }, { "", "O", "^EN", "OW" }, { "", "O", "^I#", "OW" },
            { "", "OL", "D", "OW L" }, { "", "OUGHT", "", "AO T" }, { "", "OUGH", "", "AH F" }, { " ", "OU", "", "AW" },
            { "H", "OU", "S#", "AW" }, { "", "OUS", "", "AX S" }, { "", "OUR", "", "AO R" }, { "", "OULD", "", "UH D" },
            { "^", "OU", "^L", "AH" }, { "", "OUP", "", "UW P" }, { "", "OU", "", "AW" }, { "", "OY", "", "OY" },
            { "", "OING", "", "OW IH NG" }, { "", "OI", "", "OY" }, { "", "OOR", "", "AO R" }, { "", "OOK", "", "UH K" },
            { "", "OOD", "", "UH D" }, { "", "OO", "", "UW" }, { "", "O", "E", "OW" }, { "", "O", " ", "OW" },
            { "", "OA", "", "OW" }, { " ", "ONLY", "", "OW N L IY" }, { " ", "ONCE", "", "W AH N S" }, { "C", "O", "N", "AA" },
            { "", "O", "NG", "AO" }, { " :^", "O", "N", "AH" }, { "I", "ON", "", "AX N" }, { "#:", "ON", " ", "AX N" },
            { "#^", "ON", "", "AX N" }, { "", "O", "ST ", "OW" }, { "", "OF", "^", "AO F" }, { "", "OTHER", "", "AH DH ER" },
            { "", "OSS", " ", "AO S" }, { "#:^", "OM", "", "AH M" }, { "", "O", "", "AA" },
            // P, Q, R
            { "", "PH", "", "F" }, { "", "PEOP", "", "P IY P" }, { "", "POW", "", "P AW" }, { "", "PUT", " ", "P UH T" }, { "", "P", "", "P" },
            { "", "QUAR", "", "K W AO R" }, { "", "QU", "", "K W" }, { "", "Q", "", "K" },
            { " ", "RE", "^#", "R IY" }, { "", "R", "", "R" },
            // S
            { "", "SH", "", "SH" }, { "#", "SION", "", "ZH AX N" }, { "", "SOME", "", "S AH M" }, { "#", "SUR", "#", "ZH ER" },
            { "", "SUR", "#", "SH ER" }, { "#", "SU", "#", "ZH UW" }, { "#", "SSU", "#", "SH UW" }, { "#", "SED", " ", "Z D" },
            { "#", "S", "#", "Z" }, { "", "SAID", "", "S EH D" }, { "^", "SION", "", "SH AX N" }, { "", "S", "S", "" },
            { ".", "S", " ", "Z" }, { "#:.E", "S", " ", "Z" }, { "#:^##", "S", " ", "Z" }, { "#:^#", "S", " ", "S" },
            { "U", "S", " ", "S" }, { " :#", "S", " ", "Z" }, { " ", "SCH", "", "S K" }, { "", "S", "C+", "" },
            { "#", "SM", "", "Z M" }, { "", "S", "", "S" },
            // T
            { " ", "THE", " ", "DH AX" }, { "", "TO", " ", "T UW" }, { "", "THAT", " ", "DH AE T" }, { " ", "THIS", " ", "DH IH S" },
            { " ", "THEY", "", "DH EY" }, { " ", "THERE", "", "DH EH R" }, { "", "THER", "", "DH ER" }, { "", "THEIR", "", "DH EH R" },
            { " ", "THAN", " ", "DH AE N" }, { " ", "THEM", " ", "DH EH M" }, { "", "THESE", " ", "DH IY Z" }, { " ", "THEN", "", "DH EH N" },
            { "", "THROUGH", "", "TH R UW" }, { "", "THOSE", "", "DH OW Z" }, { "", "THOUGH", " ", "DH OW" }, { " ", "THUS", "", "DH AH S" },
            { "", "TH", "", "TH" }, { "#:", "TED", " ", "T IH D" }, { "S", "TI", "#N", "CH" }, { "", "TI", "O", "SH" },
            { "", "TI", "A", "SH" }, { "", "TIEN", "", "SH AX N" }, { "", "TUR", "#", "CH ER" }, { "", "TU", "A", "CH UW" },
            { " ", "TWO", "", "T UW" }, { "", "T", "", "T" },
            // U
            { " ", "UN", "I", "Y UW N" }, { " ", "UN", "", "AH N" }, { " ", "UPON", "", "AX P AO N" }, { "@", "UR", "#", "UH R" },
            { "", "UR", "#", "Y UH R" }, { "", "UR", "*", "ER" }, { "", "U", "^ ", "AH" }, { "", "U", "^^", "AH" },
            { "", "UY", "", "AY" }, { " G", "U", "#", "" }, { "G", "U", "%", "" }, { "G", "U", "#", "W" },
            { "#N", "U", "", "Y UW" }, { "@", "U", "", "UW" }, { "", "U", "", "Y UW" },
            // V, W, X
            { "", "VIEW", "", "V Y UW" }, { "", "V", "", "V" },
            { " ", "WERE", "", "W ER" }, { "", "WA", "S", "W AA" }, { "", "WA", "T", "W AA" }, { "", "WHERE", "", "W EH R" },
            { "", "WHAT", "", "W AA T" }, { "", "WHOL", "", "HH OW L" }, { "", "WHO", "", "HH UW" }, { "", "WH", "", "W" },
            { "", "WAR", "", "W AO R" }, { "", "WOR", "^", "W ER" }, { "", "WR", "", "R" }, { "", "W", "", "W" },
            { "", "X", "", "K S" },
            // Y, Z
            { "", "YOUNG", "", "Y AH NG" }, { " ", "YOU", "", "Y UW" }, { " ", "YES", "", "Y EH S" }, { " ", "Y", "", "Y" },
            { "#:^", "Y", " ", "IY" }, { "#:^", "Y", "I", "IY" }, { " :", "Y", " ", "AY" }, { " :", "Y", "#", "AY" },
            { " :", "Y", "^+:#", "IH" }, { " :", "Y", "^#", "AY" }, { "", "Y", "", "IH" },
            { "", "Z", "", "Z" },
        };
        return r;
    }

    // Lexique : mots courants (écrits sans apostrophe) dont les règles se trompent
    inline const char* lexicon (const std::string& w)
    {
        static const std::pair<const char*, const char*> lex[] {
            { "A", "AX" }, { "THE", "DH AX" }, { "OF", "AX V" }, { "TO", "T UW" }, { "I", "AY" }, { "YOU", "Y UW" }, { "YOUR", "Y AO R" },
            { "YOURE", "Y AO R" }, { "FOR", "F AO R" }, { "OFF", "AO F" }, { "ALL", "AO L" }, { "LIFE", "L AY F" }, { "LOVE", "L AH V" },
            { "MY", "M AY" }, { "ME", "M IY" }, { "WE", "W IY" }, { "BE", "B IY" }, { "HE", "HH IY" }, { "SHE", "SH IY" }, { "NO", "N OW" },
            { "GO", "G OW" }, { "SO", "S OW" }, { "DO", "D UW" }, { "ARE", "AA R" }, { "IS", "IH Z" }, { "WAS", "W AA Z" }, { "HAVE", "HH AE V" },
            { "HAS", "HH AE Z" }, { "SAID", "S EH D" }, { "ONE", "W AH N" }, { "TWO", "T UW" }, { "EIGHT", "EY T" }, { "WHY", "W AY" },
            { "BUY", "B AY" }, { "DONT", "D OW N T" }, { "CANT", "K AE N T" }, { "WONT", "W OW N T" }, { "IM", "AY M" }, { "ITS", "IH T S" },
            { "ILL", "AY L" }, { "WHAT", "W AA T" }, { "WHO", "HH UW" }, { "WHERE", "W EH R" }, { "HERE", "HH IY R" }, { "THERE", "DH EH R" },
            { "THEY", "DH EY" }, { "COME", "K AH M" }, { "SOME", "S AH M" }, { "DONE", "D AH N" }, { "GONE", "G AO N" }, { "GIVE", "G IH V" },
            { "LIVE", "L IH V" }, { "KNOW", "N OW" }, { "PEOPLE", "P IY P AX L" }, { "WORLD", "W ER L D" }, { "WORD", "W ER D" },
            { "WORK", "W ER K" }, { "WOULD", "W UH D" }, { "COULD", "K UH D" }, { "SHOULD", "SH UH D" }, { "EYE", "AY" }, { "AGAIN", "AX G EH N" },
            { "SAYS", "S EH Z" }, { "FRIEND", "F R EH N D" }, { "WOMAN", "W UH M AX N" }, { "WOMEN", "W IH M IH N" }, { "ANY", "EH N IY" },
            { "MANY", "M EH N IY" }, { "ONLY", "OW N L IY" }, { "VERY", "V EH R IY" }, { "EVERY", "EH V R IY" }, { "MUSIC", "M Y UW Z IH K" },
            { "HELLO", "HH EH L OW" }, { "YEAH", "Y EH" }, { "OKAY", "OW K EY" }, { "OK", "OW K EY" }, { "BABY", "B EY B IY" },
            { "NIGHT", "N AY T" }, { "RIGHT", "R AY T" }, { "LIGHT", "L AY T" }, { "TIME", "T AY M" }, { "HEART", "HH AA R T" },
            { "DANCE", "D AE N S" }, { "FEEL", "F IY L" }, { "SEE", "S IY" }, { "FREE", "F R IY" }, { "GIRL", "G ER L" }, { "BOY", "B OY" },
            { "MIND", "M AY N D" }, { "ALIVE", "AX L AY V" }, { "FOREVER", "F ER EH V ER" }, { "NEVER", "N EH V ER" }, { "EVER", "EH V ER" },
            { "WANT", "W AA N T" }, { "GET", "G EH T" }, { "UP", "AH P" }, { "BUT", "B AH T" }, { "NOT", "N AA T" }, { "WITH", "W IH DH" },
            { "FROM", "F R AH M" }, { "OUR", "AW ER" }, { "AND", "AE N D" }, { "IN", "IH N" }, { "ON", "AA N" }, { "IT", "IH T" },
            { "OUT", "AW T" }, { "NOW", "N AW" }, { "HOW", "HH AW" }, { "KNOWN", "N OW N" }, { "SAY", "S EY" }, { "WAY", "W EY" },
            { "COMPUTER", "K AX M P Y UW T ER" }, { "ROBOT", "R OW B AA T" }, { "VOICE", "V OY S" }, { "AMONG", "AX M AH NG" },
            { "US", "AH S" }, { "MACHINE", "M AX SH IY N" }, { "TOGETHER", "T AX G EH DH ER" }, { "RHYTHM", "R IH DH AX M" },
            { "BELIEVE", "B IH L IY V" }, { "SOUND", "S AW N D" }, { "BASS", "B EY S" }, { "DROP", "D R AA P" }, { "MUTANTS", "M Y UW T AX N T S" }, { "MUTANT", "M Y UW T AX N T" },
        };
        for (const auto& [k, v] : lex) if (w == k) return v;
        return nullptr;
    }

    inline bool vowelC (char c) noexcept { return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y'; }
    inline bool consC (char c) noexcept { return c >= 'A' && c <= 'Z' && ! vowelC (c); }
    inline bool voicedC (char c) noexcept { return c == 'B' || c == 'D' || c == 'V' || c == 'G' || c == 'J' || c == 'L' || c == 'M' || c == 'N' || c == 'R' || c == 'W' || c == 'Z'; }
    inline bool frontC (char c) noexcept { return c == 'E' || c == 'I' || c == 'Y'; }

    /** Contexte droit à partir de la position p (mot entouré d'espaces). */
    inline bool matchRight (const char* ctx, const std::string& w, int p)
    {
        const int n = (int) w.size();
        auto at = [&] (int i) { return i >= 0 && i < n ? w[(size_t) i] : ' '; };
        for (const char* c = ctx; *c; ++c)
        {
            switch (*c)
            {
                case ' ': if (at (p) != ' ') return false; ++p; break;
                case '#': if (! vowelC (at (p))) return false; while (vowelC (at (p))) ++p; break;
                case ':': while (consC (at (p))) ++p; break;
                case '*': if (! consC (at (p))) return false; while (consC (at (p))) ++p; break;
                case '^': if (! consC (at (p))) return false; ++p; break;
                case '.': if (! voicedC (at (p))) return false; ++p; break;
                case '+': if (! frontC (at (p))) return false; ++p; break;
                case '%':
                {
                    static const char* suf[] { "ING", "ELY", "ER", "ES", "ED", "E" };
                    bool ok = false;
                    for (const char* s : suf)
                    {
                        const int l = (int) std::strlen (s);
                        if (p + l <= n && w.compare ((size_t) p, (size_t) l, s) == 0) { p += l; ok = true; break; }
                    }
                    if (! ok) return false;
                    break;
                }
                default: if (at (p) != *c) return false; ++p; break;
            }
        }
        return true;
    }

    /** Contexte gauche, lu de droite à gauche à partir de la position p (dernière lettre avant le motif). */
    inline bool matchLeft (const char* ctx, const std::string& w, int p)
    {
        const int n = (int) w.size();
        auto at = [&] (int i) { return i >= 0 && i < n ? w[(size_t) i] : ' '; };
        for (int k = (int) std::strlen (ctx) - 1; k >= 0; --k)
        {
            switch (ctx[k])
            {
                case ' ': if (at (p) != ' ') return false; --p; break;
                case '#': if (! vowelC (at (p))) return false; while (vowelC (at (p))) --p; break;
                case ':': while (consC (at (p))) --p; break;
                case '*': if (! consC (at (p))) return false; while (consC (at (p))) --p; break;
                case '^': if (! consC (at (p))) return false; --p; break;
                case '.': if (! voicedC (at (p))) return false; --p; break;
                case '+': if (! frontC (at (p))) return false; --p; break;
                case '&':
                    if ((at (p) == 'H') && (at (p - 1) == 'C' || at (p - 1) == 'S')) p -= 2;
                    else if (std::strchr ("SCGZXJ", at (p)) != nullptr && at (p) != ' ') --p;
                    else return false;
                    break;
                case '@':
                    if (at (p) == 'H' && (at (p - 1) == 'T' || at (p - 1) == 'C' || at (p - 1) == 'S')) p -= 2;
                    else if (std::strchr ("TSRDLZNJ", at (p)) != nullptr && at (p) != ' ') --p;
                    else return false;
                    break;
                default: if (at (p) != ctx[k]) return false; --p; break;
            }
        }
        return true;
    }

    /** Sons (notation ARPAbet) → phonèmes du synthétiseur. */
    inline void emit (const char* arpa, std::vector<int>& out)
    {
        std::string t;
        auto push = [&] (const std::string& x)
        {
            static const std::pair<const char*, std::array<int, 2>> map[] {
                { "IY", { speech::I, -1 } }, { "IH", { IH, -1 } }, { "EY", { E, J } }, { "EH", { EH, -1 } }, { "AE", { AE, -1 } }, { "AA", { A, -1 } },
                { "AO", { AO, -1 } }, { "OW", { O, W } }, { "UH", { UH, -1 } }, { "UW", { U, -1 } }, { "ER", { ER, -1 } }, { "AX", { SCHWA, -1 } },
                { "AH", { AH, -1 } }, { "AY", { A, J } }, { "AW", { A, W } }, { "OY", { AO, J } },
                { "P", { PP, -1 } }, { "B", { BB, -1 } }, { "T", { TT, -1 } }, { "D", { DD, -1 } }, { "K", { KK, -1 } }, { "G", { GG, -1 } },
                { "F", { FF, -1 } }, { "V", { VV, -1 } }, { "TH", { TH, -1 } }, { "DH", { DH, -1 } }, { "S", { SS, -1 } }, { "Z", { ZZ, -1 } },
                { "SH", { CH, -1 } }, { "ZH", { JJ, -1 } }, { "HH", { HH, -1 } }, { "M", { MM, -1 } }, { "N", { NN, -1 } }, { "NG", { NG, -1 } },
                { "L", { LL, -1 } }, { "W", { W, -1 } }, { "Y", { J, -1 } }, { "R", { RE, -1 } }, { "CH", { TT, CH } }, { "JH", { DD, JJ } },
            };
            for (const auto& [k, v] : map)
                if (x == k) { out.push_back (v[0]); if (v[1] >= 0) out.push_back (v[1]); return; }
        };
        for (const char* c = arpa; ; ++c)
        {
            if (*c == ' ' || *c == 0) { if (! t.empty()) push (t); t.clear(); if (*c == 0) break; }
            else t.push_back (*c);
        }
    }

    /** Lecture d'un mot anglais (lettres en minuscules, apostrophes possibles) → phonèmes. */
    inline void readWord (const std::u32string& w32, std::vector<int>& out)
    {
        std::string w;
        for (auto c : w32)
        {
            if (c >= 'a' && c <= 'z') w.push_back ((char) (c - 32));
            else if (c == 0xE9 || c == 0xE8 || c == 0xEA || c == 0xEB) w.push_back ('E'); // lettres accentuées : la lettre de base
            else if (c == 0xE0 || c == 0xE2 || c == 0xE4) w.push_back ('A');
            else if (c == 0xEE || c == 0xEF) w.push_back ('I');
            else if (c == 0xF4 || c == 0xF6) w.push_back ('O');
            else if (c == 0xF9 || c == 0xFB || c == 0xFC) w.push_back ('U');
            else if (c == 0xE7) w.push_back ('C');
        }
        if (w.empty()) return;
        if (const auto* lx = lexicon (w)) { emit (lx, out); return; }
        const std::string word = " " + w + " ";
        const auto& rs = rules();
        for (int p = 1; p < (int) word.size() - 1;)
        {
            bool done = false;
            for (const auto& r : rs)
            {
                if (r.match[0] != word[(size_t) p]) continue;
                const int l = (int) std::strlen (r.match);
                if (word.compare ((size_t) p, (size_t) l, r.match) != 0) continue;
                if (! matchLeft (r.left, word, p - 1) || ! matchRight (r.right, word, p + l)) continue;
                emit (r.out, out);
                p += l; done = true;
                break;
            }
            if (! done) ++p;
        }
    }

    inline const char* digitWord (char32_t d)
    {
        static const char* names[10] { "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine" };
        return names[d - '0'];
    }
}
