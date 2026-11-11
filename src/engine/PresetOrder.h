/*
    Groovebox — OFAD — AGPL-3.0-or-later
    1.1 : numérotation des préréglages d'usine (changement de programme, listes, télécommande).
    Ordre logique : « Init » toujours n° 1, puis les banques dans l'ordre affiché (Acide, Basses, Leads,
    Nappes, Plucks, Claviers, Cordes, Cuivres, Arpèges, Effets, Classiques) ; dans une banque, l'ordre
    de la liste d'origine. Les préréglages de l'utilisateur viennent à la suite (n° = sons d'usine + 1…).
    Le même ordre est envoyé à l'interface : l'indice d'un son dans le catalogue EST son numéro - 1.
*/

#pragma once

#include "Params.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

namespace grv::engine
{

/** Rang de la banque d'un préréglage d'usine (le son « Init » passe toujours en premier). */
inline int presetBankRank (const Preset& p) noexcept
{
    if (p.name == "Init") return -1;
    static const char* const order[] = { "Init", "Acide", "Basses", "Leads", "Nappes", "Plucks", "Claviers",
                                         "Cordes", "Cuivres", "Arp\xC3\xA8ges", "Effets", "Classiques" };
    const std::string& c = p.category.empty() ? std::string ("Classiques") : p.category;
    for (int i = 0; i < (int) (sizeof (order) / sizeof (order[0])); ++i)
        if (c == order[i]) return i;
    return 100;
}

/** Indices des préréglages d'usine dans l'ordre des numéros : order[n - 1] = indice du son n° n.
    Banques inconnues de la liste (« Percussions », « Voix »…) : après les connues, dans l'ordre où elles
    apparaissent (comme le panneau des préréglages). */
inline std::vector<size_t> presetProgramOrder (const ModuleDesc& d)
{
    std::vector<int> rank (d.presets.size());
    std::vector<std::string> others;
    for (size_t i = 0; i < d.presets.size(); ++i)
    {
        rank[i] = presetBankRank (d.presets[i]);
        if (rank[i] == 100)
        {
            const auto& c = d.presets[i].category;
            auto it = std::find (others.begin(), others.end(), c);
            if (it == others.end()) { others.push_back (c); it = others.end() - 1; }
            rank[i] = 100 + (int) (it - others.begin());
        }
    }
    std::vector<size_t> o (d.presets.size());
    std::iota (o.begin(), o.end(), size_t { 0 });
    std::stable_sort (o.begin(), o.end(), [&rank] (size_t a, size_t b) { return rank[a] < rank[b]; });
    return o;
}

/** Numéro (0 = n° 1) d'un préréglage d'usine d'après son nom, -1 s'il n'en est pas un. */
inline int presetProgramOf (const ModuleDesc& d, const std::string& name)
{
    if (name.empty()) return -1;
    const auto o = presetProgramOrder (d);
    for (size_t n = 0; n < o.size(); ++n)
        if (d.presets[o[n]].name == name) return (int) n;
    return -1;
}

} // namespace grv::engine
