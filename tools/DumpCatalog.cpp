/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Écrit le catalogue des modules (JSON) : sert à l'interface de développement (moteur simulé)
    et à vérifier que l'interface et le moteur décrivent les mêmes réglages.
    Usage : groovebox_catalog > ui/src/engine/catalog.json
*/

#include "engine/Bridge.h"

#include <cstdio>

int main()
{
    std::fputs (grv::engine::bridge::catalogJson().c_str(), stdout);
    std::fputs ("\n", stdout);
    return 0;
}
