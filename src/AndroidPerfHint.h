#pragma once
// v0.46 fix16 — Android : indice de performance du fil audio (ADPF, Android 13 / API 33 et plus).
//
// Principe : sur un téléphone, le système baisse la fréquence des cœurs et déplace le fil audio
// vers les petits cœurs dès que la charge paraît faible ; au premier pic (plusieurs notes, un
// effet), le calcul d'un bloc dépasse sa durée et le son craque. On annonce au système la durée
// visée pour chaque bloc et on lui rapporte le temps réellement passé : il garde alors le fil
// audio sur des cœurs assez rapides. C'est le même mécanisme que l'option « performance hint »
// d'Oboe (que JUCE n'active pas) ; on l'appelle directement, sans modifier JUCE.
//
// Temps réel : le fil audio ne fait que lire un pointeur atomique et rapporter une durée.
// Les sessions sont créées sur le fil de l'interface (service(), appelé par le minuteur) et ne
// sont fermées qu'à la destruction, quand le son est arrêté. Sans la fonction (Android < 13),
// rien ne se passe.

#if JUCE_ANDROID
 #include <atomic>
 #include <array>
 #include <cstdint>
 #include <dlfcn.h>
 #include <unistd.h>

namespace grv
{
class AndroidPerfHint
{
public:
    ~AndroidPerfHint()
    {
        if (closeFn != nullptr)
            for (int i = 0; i < numSessions; ++i) closeFn (sessions[(size_t) i]);
    }

    // Fil audio, à la fin de chaque bloc : durée du calcul et durée du bloc (nanosecondes)
    void report (std::int64_t usedNs, std::int64_t periodNs) noexcept
    {
        const auto tid = (std::int32_t) gettid();
        audioTid.store (tid, std::memory_order_relaxed);
        audioPeriod.store (periodNs, std::memory_order_relaxed);
        if (sessionTid.load (std::memory_order_acquire) != tid) return;
        if (auto* s = current.load (std::memory_order_acquire); s != nullptr && reportFn != nullptr)
            reportFn (s, usedNs);
    }

    // Fil de l'interface (minuteur) : crée la session du fil audio, suit la durée des blocs
    void service() noexcept
    {
        const auto tid = audioTid.load (std::memory_order_relaxed);
        const auto period = audioPeriod.load (std::memory_order_relaxed);
        if (tid <= 0 || period <= 0 || ! load()) return;
        // Cible : la moitié de la durée du bloc, pour garder de la marge face aux pics
        const auto target = period / 2;
        if (tid == sessionTid.load (std::memory_order_relaxed))
        {
            if (target != sessionTarget && updateFn != nullptr)
                if (auto* s = current.load (std::memory_order_relaxed); s != nullptr && updateFn (s, target) == 0)
                    sessionTarget = target;
            return;
        }
        if (tid == failedTid || numSessions >= (int) sessions.size()) return; // refus du système, ou fil recréé trop souvent
        auto* s = createFn (getManagerFn(), &tid, 1, target);
        if (s == nullptr) { failedTid = tid; return; }
        sessions[(size_t) numSessions++] = s;
        sessionTarget = target;
        current.store (s, std::memory_order_release);
        sessionTid.store (tid, std::memory_order_release);
    }

    bool active() const noexcept { return current.load (std::memory_order_relaxed) != nullptr; }

private:
    struct Manager; struct Session;
    using GetManager = Manager* (*)();
    using Create     = Session* (*) (Manager*, const std::int32_t*, std::size_t, std::int64_t);
    using Update     = int (*) (Session*, std::int64_t);
    using Report     = int (*) (Session*, std::int64_t);
    using Close      = void (*) (Session*);

    bool load() noexcept
    {
        if (loaded) return getManagerFn != nullptr;
        loaded = true;
        if (void* h = dlopen ("libandroid.so", RTLD_NOW | RTLD_NODELETE); h != nullptr)
        {
            getManagerFn = (GetManager) dlsym (h, "APerformanceHint_getManager");
            createFn     = (Create)     dlsym (h, "APerformanceHint_createSession");
            updateFn     = (Update)     dlsym (h, "APerformanceHint_updateTargetWorkDuration");
            closeFn      = (Close)      dlsym (h, "APerformanceHint_closeSession");
            reportFn     = (Report)     dlsym (h, "APerformanceHint_reportActualWorkDuration");
            if (createFn == nullptr || reportFn == nullptr || closeFn == nullptr) getManagerFn = nullptr;
            if (getManagerFn != nullptr && getManagerFn() == nullptr) getManagerFn = nullptr;
        }
        return getManagerFn != nullptr;
    }

    bool loaded = false;
    GetManager getManagerFn = nullptr; Create createFn = nullptr; Update updateFn = nullptr;
    Report reportFn = nullptr; Close closeFn = nullptr;
    std::array<Session*, 8> sessions {};
    int numSessions = 0;
    std::int32_t failedTid = 0;
    std::int64_t sessionTarget = 0;
    std::atomic<Session*> current { nullptr };
    std::atomic<std::int32_t> audioTid { 0 }, sessionTid { 0 };
    std::atomic<std::int64_t> audioPeriod { 0 };
};
} // namespace grv
#endif
