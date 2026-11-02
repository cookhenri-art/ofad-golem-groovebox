/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Catalogue des instruments et des effets : description et fabrication par type.
*/

#pragma once

#include "Module.h"

#include <memory>
#include <string>
#include <vector>

namespace grv::engine
{

const std::vector<const ModuleDesc*>& instrumentCatalog();
const std::vector<const ModuleDesc*>& effectCatalog();

const ModuleDesc* findInstrument (const std::string& type);
const ModuleDesc* findEffect (const std::string& type);

/** Chaîne d'effets prête à l'emploi : jusqu'à 4 inserts (type, préréglage), pour une source donnée. */
struct ChainDesc
{
    std::string name;   // « Voix », « Bus principal »…
    std::string use;    // description de la chaîne
    std::vector<std::pair<std::string, std::string>> inserts;
};
const std::vector<ChainDesc>& chainCatalog();

std::unique_ptr<Instrument> createInstrument (const std::string& type);
std::unique_ptr<Effect> createEffect (const std::string& type);

} // namespace grv::engine
