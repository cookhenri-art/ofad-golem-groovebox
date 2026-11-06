/*
    Groovebox — OFAD — AGPL-3.0-or-later

    v0.49 (essai 3) : garde-fou du démarrage sur Android.

    1. Journal de plantage : un gestionnaire de signaux (SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE)
       écrit dans « plantage.txt » (dossier de l'appli) le signal, l'adresse fautive et la pile
       d'appels (décalages dans la bibliothèque : symbolisés ensuite avec la bibliothèque non
       dépouillée de la même compilation), puis rend la main au gestionnaire d'Android.
    2. Démarrage sans la séance : si le lancement précédent n'est pas allé au bout (repère
       « lancement-en-cours » encore là), la séance enregistrée n'est pas rouverte ; elle est gardée
       de côté (autosave-apres-plantage.gbx), jamais effacée.
    3. Au lancement suivant, le rapport est montré à l'écran (fenêtre d'Android).

    Seulement sur Android : ailleurs, fonctions vides.
*/

#pragma once

#include <juce_core/juce_core.h>

#if JUCE_ANDROID
 #include <csignal>
 #include <cstring>
 #include <cstdint>
 #include <dlfcn.h>
 #include <fcntl.h>
 #include <unistd.h>
 #include <sys/syscall.h>
 #include <ucontext.h>
 #include <unwind.h>
#endif

namespace grv::crashguard
{
inline juce::File folder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Groovebox");
}

/** Rapport du plantage précédent (vide s'il n'y en a pas), lu par install(). */
inline juce::String& previousReport() { static juce::String r; return r; }
/** Le lancement précédent n'est pas allé au bout : la séance n'a pas été rouverte. */
inline bool& safeStart() { static bool s = false; return s; }

#if JUCE_ANDROID
namespace detail
{
    inline int logFd = -1;
    inline struct sigaction previous[32] {};

    inline void put (const char* s) noexcept { if (logFd >= 0 && s != nullptr) { auto r = ::write (logFd, s, std::strlen (s)); (void) r; } }
    inline void putHex (std::uintptr_t v) noexcept
    {
        char b[19]; b[0] = '0'; b[1] = 'x';
        for (int i = 0; i < 16; ++i) { const int n = (int) ((v >> ((15 - i) * 4)) & 15u); b[2 + i] = (char) (n < 10 ? '0' + n : 'a' + n - 10); }
        b[18] = 0; put (b);
    }
    inline void putDec (long v) noexcept
    {
        char b[24]; int i = 23; b[i] = 0; const bool neg = v < 0; unsigned long u = neg ? (unsigned long) -v : (unsigned long) v;
        do { b[--i] = (char) ('0' + u % 10); u /= 10; } while (u != 0 && i > 1);
        if (neg) b[--i] = '-';
        put (b + i);
    }
    /** « décalage bibliothèque fonction » : le décalage se symbolise avec llvm-addr2line. */
    inline void frame (const char* label, std::uintptr_t pc) noexcept
    {
        put ("  "); put (label); put (" ");
        Dl_info info {};
        if (pc != 0 && dladdr ((void*) pc, &info) != 0 && info.dli_fbase != nullptr)
        {
            putHex (pc - (std::uintptr_t) info.dli_fbase); put (" ");
            const char* n = info.dli_fname != nullptr ? std::strrchr (info.dli_fname, '/') : nullptr;
            put (n != nullptr ? n + 1 : (info.dli_fname != nullptr ? info.dli_fname : "?"));
            if (info.dli_sname != nullptr) { put (" "); put (info.dli_sname); }
        }
        else putHex (pc);
        put ("\n");
    }
    struct Walk { int n = 0; };
    inline _Unwind_Reason_Code step (_Unwind_Context* c, void* arg) noexcept
    {
        auto* w = static_cast<Walk*> (arg);
        if (++w->n > 48) return _URC_END_OF_STACK;
        frame ("#", (std::uintptr_t) _Unwind_GetIP (c));
        return _URC_NO_REASON;
    }
    inline void handler (int sig, siginfo_t* si, void* ctx) noexcept
    {
        put ("\nsignal "); putDec (sig); put ("  code "); putDec (si != nullptr ? si->si_code : 0);
        put ("  adresse "); putHex (si != nullptr ? (std::uintptr_t) si->si_addr : 0);
        put ("  fil "); putDec ((long) syscall (SYS_gettid)); put ("\n");
        std::uintptr_t pc = 0, lr = 0;
        if (auto* uc = static_cast<ucontext_t*> (ctx))
        {
           #if defined(__aarch64__)
            pc = (std::uintptr_t) uc->uc_mcontext.pc; lr = (std::uintptr_t) uc->uc_mcontext.regs[30];
           #elif defined(__arm__)
            pc = (std::uintptr_t) uc->uc_mcontext.arm_pc; lr = (std::uintptr_t) uc->uc_mcontext.arm_lr;
           #endif
        }
        frame ("pc", pc);
        frame ("lr", lr);
        Walk w;
        _Unwind_Backtrace (step, &w);
        put ("fin\n");
        if (logFd >= 0) ::fsync (logFd);
        // la main au gestionnaire d'Android (rapport du système, fermeture de l'appli)
        if (sig > 0 && sig < 32) sigaction (sig, &previous[sig], nullptr);
        if (sig == SIGABRT || sig == SIGFPE) raise (sig);
    }
}
#endif

/** Au tout début du constructeur du processeur (application autonome Android). */
inline void install()
{
   #if JUCE_ANDROID
    static bool done = false;
    if (done) return;
    done = true;
    const auto dir = folder();
    dir.createDirectory();
    const auto log = dir.getChildFile ("plantage.txt");
    const auto boot = dir.getChildFile ("demarrage.log");
    const auto running = dir.getChildFile ("lancement-en-cours");
    if (log.existsAsFile() && log.getSize() > 0 && log.getSize() < 256 * 1024)
    {
        juce::String r = log.loadFileAsString();
        if (boot.existsAsFile() && boot.getSize() < 64 * 1024)
        {
            juce::StringArray lines; lines.addLines (boot.loadFileAsString());
            lines.removeEmptyStrings();
            r = "Dernieres etapes :\n" + lines.joinIntoString ("\n", juce::jmax (0, lines.size() - 6)) + "\n" + r;
        }
        previousReport() = r.trim();
        log.copyFileTo (dir.getChildFile ("plantage-precedent.txt"));
    }
    safeStart() = running.existsAsFile();
    running.replaceWithText ("lancement en cours\n");

    detail::logFd = ::open (log.getFullPathName().toRawUTF8(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    static char altStack[64 * 1024];
    stack_t ss {}; ss.ss_sp = altStack; ss.ss_size = sizeof (altStack); ss.ss_flags = 0;
    sigaltstack (&ss, nullptr);
    for (int sig : { SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE })
    {
        struct sigaction sa {};
        sa.sa_sigaction = detail::handler;
        sigemptyset (&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigaction (sig, &sa, &detail::previous[sig]);
    }
   #endif
}

/** Le lancement est allé au bout (quelques secondes après l'ouverture de l'interface). */
inline void launchConfirmed()
{
   #if JUCE_ANDROID
    folder().getChildFile ("lancement-en-cours").deleteFile();
   #endif
}
} // namespace grv::crashguard
