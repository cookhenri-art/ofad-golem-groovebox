/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Témoin de charge (v0.23) : mémoire occupée par l'application, lue auprès du système.
    Fichier à part : les en-têtes du système (Windows) ne se mélangent pas à ceux de JUCE.
*/

#pragma once

#include <cstdint>

namespace grv
{
/** Mémoire de travail du processus (octets), 0 si le système ne la donne pas. Fil de l'interface. */
std::uint64_t processMemoryBytes() noexcept;
}
