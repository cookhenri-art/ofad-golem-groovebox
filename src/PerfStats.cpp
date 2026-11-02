/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Mémoire du processus : Windows (ensemble de travail), Linux et Android (/proc/self/statm),
    macOS (informations de tâche). Lecture seule, sans allocation durable.
*/

#include "PerfStats.h"

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef PSAPI_VERSION
  #define PSAPI_VERSION 2 // K32GetProcessMemoryInfo dans kernel32 (Windows 7 et plus), sans psapi.lib
 #endif
 #include <windows.h>
 #include <psapi.h>
#elif defined (__APPLE__)
 #include <mach/mach.h>
#else
 #include <cstdio>
 #include <unistd.h>
#endif

namespace grv
{

std::uint64_t processMemoryBytes() noexcept
{
#if defined (_WIN32)
    PROCESS_MEMORY_COUNTERS pmc {};
    pmc.cb = sizeof (pmc);
    // K32GetProcessMemoryInfo est dans kernel32 (Windows 7 et plus) : aucune bibliothèque en plus
    if (K32GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc))) return (std::uint64_t) pmc.WorkingSetSize;
    return 0;
#elif defined (__APPLE__)
    mach_task_basic_info info {};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info (mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) == KERN_SUCCESS) return (std::uint64_t) info.resident_size;
    return 0;
#else
    unsigned long size = 0, resident = 0;
    if (FILE* f = std::fopen ("/proc/self/statm", "r"))
    {
        if (std::fscanf (f, "%lu %lu", &size, &resident) != 2) resident = 0;
        std::fclose (f);
    }
    const auto page = sysconf (_SC_PAGESIZE);
    return (std::uint64_t) resident * (std::uint64_t) (page > 0 ? page : 4096);
#endif
}

} // namespace grv
