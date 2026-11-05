/*
    Groovebox — OFAD — AGPL-3.0-or-later
    v0.46.1 (audit F3) : garde-fou avant la lecture d'un projet ou d'un état de séance.

    Le lecteur XML de JUCE (et ValueTree::fromXml) s'appelle lui-même pour chaque balise imbriquée :
    un fichier piégé avec des centaines de milliers de balises les unes dans les autres épuise la
    pile et fait planter l'application (ou le logiciel hôte, pour le plugin). Un vrai projet GOLEM ne
    dépasse pas une dizaine de niveaux : on mesure la profondeur d'un seul passage, sans récursion,
    avant de confier le texte au lecteur.
*/
#pragma once

#include <cstddef>
#include <cstring>

namespace grv
{
/** Profondeur maximale acceptée (un projet réel en utilise moins de 10). */
constexpr int maxXmlDepth = 64;

/** Vrai si les balises de `text` (longueur `len`, sans zéro final obligatoire) ne s'imbriquent pas
    plus de `maxDepth` fois. Commentaires, CDATA, déclarations et valeurs d'attributs entre
    guillemets sont sautés. Un texte tronqué renvoie vrai : le lecteur le refusera de lui-même,
    sans descendre plus profond que ce qui a déjà été compté. */
inline bool xmlNestingWithin (const char* text, std::size_t len, int maxDepth = maxXmlDepth) noexcept
{
    if (text == nullptr) return true;
    const char* s = text;
    const char* const end = text + len;
    auto startsWith = [&] (const char* p, const char* lit) noexcept
    {
        const auto n = std::strlen (lit);
        return (std::size_t) (end - p) >= n && std::memcmp (p, lit, n) == 0;
    };
    auto find = [&] (const char* from, const char* lit) noexcept -> const char*
    {
        const auto n = std::strlen (lit);
        for (const char* p = from; (std::size_t) (end - p) >= n; ++p)
            if (std::memcmp (p, lit, n) == 0) return p;
        return nullptr;
    };

    int depth = 0;
    while (s < end)
    {
        if (*s != '<') { ++s; continue; }
        if (startsWith (s, "<!--"))      { const auto* e = find (s + 4, "-->"); if (e == nullptr) return true; s = e + 3; continue; }
        if (startsWith (s, "<![CDATA[")) { const auto* e = find (s + 9, "]]>"); if (e == nullptr) return true; s = e + 3; continue; }
        if (startsWith (s, "<?"))        { const auto* e = find (s + 2, "?>");  if (e == nullptr) return true; s = e + 2; continue; }
        if (startsWith (s, "<!"))        { const auto* e = find (s + 2, ">");   if (e == nullptr) return true; s = e + 1; continue; }
        if (startsWith (s, "</"))
        {
            if (depth > 0) --depth;
            const auto* e = find (s + 2, ">");
            if (e == nullptr) return true;
            s = e + 1;
            continue;
        }
        // balise ouvrante : jusqu'au « > » qui la ferme, en sautant les valeurs entre guillemets
        ++s;
        char quote = 0;
        char previous = 0;
        while (s < end)
        {
            const char c = *s;
            if (quote != 0) { if (c == quote) quote = 0; }
            else if (c == '"' || c == '\'') quote = c;
            else if (c == '>') break;
            previous = c;
            ++s;
        }
        if (s >= end) return true;
        ++s; // après « > »
        if (previous != '/' && ++depth > maxDepth) return false; // « <x/> » ne descend pas
    }
    return true;
}

/** Même garde-fou pour le JSON (préréglages .gbp, réglages des dossiers) : le lecteur de JUCE est
    lui aussi récursif. Compte les « [ » et « { » hors des chaînes (échappements « \" » compris). */
inline bool jsonNestingWithin (const char* text, std::size_t len, int maxDepth = maxXmlDepth) noexcept
{
    if (text == nullptr) return true;
    int depth = 0;
    bool inString = false, escaped = false;
    for (std::size_t i = 0; i < len; ++i)
    {
        const char c = text[i];
        if (inString)
        {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == '[' || c == '{') { if (++depth > maxDepth) return false; }
        else if ((c == ']' || c == '}') && depth > 0) --depth;
    }
    return true;
}
} // namespace grv
