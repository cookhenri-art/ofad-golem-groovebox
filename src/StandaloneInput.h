/*
    Groovebox — OFAD — AGPL-3.0-or-later

    v0.39 : coupure de l'entrée audio par l'application autonome (« Mute audio input », sécurité
    anti-larsen de JUCE, cochée par défaut). Le micro marchait dans les réglages mais rien n'arrivait
    aux vocodeurs. Le code partagé (aussi utilisé par le VST3) ne connaît pas l'application
    autonome : celle-ci s'inscrit ici au démarrage (src/StandaloneHook.cpp, compilé seulement avec
    elle). Appel : -1 lire, 0 ouvrir le micro, 1 le couper. Réponse : 1 coupé, 0 ouvert,
    -1 pas d'application autonome (dans un logiciel hôte, l'entrée est celle de la piste).
    Fil des messages uniquement.
*/

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <functional>
#include <vector>
#include <juce_core/juce_core.h>

namespace grv
{
/** 1.1 : télécommande — l'application autonome note ici chaque « bonjour » reçu, avec le nom de l'entrée
    MIDI qui l'a envoyé (fil MIDI) ; le processeur le relit (fil des messages) et ouvre la sortie du même
    nom pour renvoyer l'état du PC. Op 20 de standaloneInputHook : brancher cette écoute (une fois). */
struct RemoteHelloBox
{
    juce::SpinLock lock;
    juce::String name;
    std::atomic<int> seq { 0 };
    std::atomic<bool> full { false };
    void post (const juce::String& n, bool f) { { const juce::SpinLock::ScopedLockType l (lock); name = n; } if (f) full.store (true); seq.fetch_add (1); }
    juce::String read() { const juce::SpinLock::ScopedLockType l (lock); return name; }
};
inline RemoteHelloBox& remoteHello() { static RemoteHelloBox box; return box; }

/** 1.1 : côté téléphone — messages « état » et « nom » reçus du PC (fil MIDI), relus par l'interface
    (fil des messages) qui les passe à la télécommande. 512 messages au plus en attente. */
struct RemoteInQueue
{
    juce::SpinLock lock;
    std::vector<std::vector<uint8_t>> items;
    void push (const uint8_t* d, size_t n)
    {
        const juce::SpinLock::ScopedLockType l (lock);
        if (items.size() < 512) items.emplace_back (d, d + n);
    }
    std::vector<std::vector<uint8_t>> take()
    {
        std::vector<std::vector<uint8_t>> out;
        const juce::SpinLock::ScopedLockType l (lock);
        out.swap (items);
        return out;
    }
};
inline RemoteInQueue& remoteIn() { static RemoteInQueue q; return q; }

/** 1.1 : télécommande — « Apprendre » : tant que c'est armé (bouton de l'interface), chaque CC ou note
    reçu de l'appareil branché est passé à l'interface (message local 0x0A : type 0 CC / 1 note, canal 0..15,
    numéro), qui le pose sur le réglage choisi. Désarmé : rien n'est transmis. Fil MIDI. */
inline std::atomic<bool>& remoteLearn() { static std::atomic<bool> on { false }; return on; }
inline void pushLearn (const uint8_t* raw, int size)
{
    if (! remoteLearn().load (std::memory_order_relaxed) || raw == nullptr || size < 3) return;
    const auto kind = raw[0] & 0xF0;
    const bool isCc = kind == 0xB0 && raw[1] < 120;          // pas les messages de mode (120 à 127)
    const bool isNote = kind == 0x90 && raw[2] > 0;
    if (! isCc && ! isNote) return;
    const uint8_t d[] { 0x7D, 0x47, 0x4C, 0x01, 0x0A, (uint8_t) (isNote ? 1 : 0), (uint8_t) (raw[0] & 0x0F), (uint8_t) (raw[1] & 0x7F) };
    remoteIn().push (d, sizeof d);
}

/** Après la 1.1 : anti-boucle des pistes MIDI. Ce qui arrive de l'appareil que jouent les pistes MIDI
    (même nom que la sortie choisie) ne rejoue pas les pistes : sinon ses touches, sa molette et ses
    boutons repartiraient vers lui (notes doublées ou bloquées, son qui part dans tous les sens). Le fil
    MIDI note chaque message de cet appareil ; le fil audio le reconnaît (mêmes octets, moins de 0,4 s). */
struct FromDeviceRing
{
    static constexpr int size = 128;
    std::array<std::atomic<std::uint32_t>, size> key {};
    std::array<std::atomic<double>, size> at {};
    std::atomic<int> head { 0 };
    juce::SpinLock nameLock;
    juce::String outName;                                // nom de la sortie MIDI des pistes MIDI
};
inline FromDeviceRing& fromDevice() { static FromDeviceRing r; return r; }
/** Clé d'un message de canal (fin de note normalisée : vélocité 0) ; 0 : message ignoré. */
inline std::uint32_t midiKey (const std::uint8_t* raw, int size) noexcept
{
    if (raw == nullptr || size < 2 || raw[0] < 0x80 || raw[0] >= 0xF0) return 0;
    std::uint32_t b0 = raw[0], b1 = raw[1], b2 = size > 2 ? raw[2] : 0;
    if ((b0 & 0xF0) == 0x90 && b2 == 0) b0 = 0x80 | (b0 & 0x0F);
    if ((b0 & 0xF0) == 0x80) b2 = 0;
    return (b0 << 16) | (b1 << 8) | b2 | 0x01000000u;
}
inline void setMidiOutName (const juce::String& name) { const juce::SpinLock::ScopedLockType l (fromDevice().nameLock); fromDevice().outName = name; }
/** Fil MIDI : le message vient-il de l'appareil de sortie ? (noms comparés sans casse, l'un contenant l'autre) */
inline void noteFromDevice (const juce::String& sourceName, const std::uint8_t* raw, int size)
{
    auto& r = fromDevice();
    const auto k = midiKey (raw, size);
    if (k == 0) return;
    {
        const juce::SpinLock::ScopedTryLockType l (r.nameLock);
        if (! l.isLocked() || r.outName.isEmpty()) return;
        const auto a = sourceName.toLowerCase(), b = r.outName.toLowerCase();
        if (! (a.contains (b) || b.contains (a))) return;
    }
    const int i = r.head.fetch_add (1, std::memory_order_relaxed) & (FromDeviceRing::size - 1);
    r.at[(size_t) i].store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
    r.key[(size_t) i].store (k, std::memory_order_release);
}
/** Fil audio : ce message vient-il de l'appareil de sortie ? (l'entrée trouvée est consommée) */
inline bool takeFromDevice (const std::uint8_t* raw, int size, double nowMs) noexcept
{
    auto& r = fromDevice();
    const auto k = midiKey (raw, size);
    if (k == 0) return false;
    for (int i = 0; i < FromDeviceRing::size; ++i)
        if (r.key[(size_t) i].load (std::memory_order_acquire) == k && nowMs - r.at[(size_t) i].load (std::memory_order_relaxed) < 400.0)
        {
            std::uint32_t e = k;
            if (r.key[(size_t) i].compare_exchange_strong (e, 0)) return true;
        }
    return false;
}

/** Après la 1.1 : sortie MIDI des pistes MIDI (application autonome : sortie MIDI par défaut du
    gestionnaire de périphériques, envoyée avec un fil d'envoi calé sur l'horloge). Argument : identifiant
    à choisir (chaîne vide : aucune ; var vide : seulement lire). Renvoie { outs: [{id, name}], current }.
    Fil des messages. Non posé dans un logiciel hôte (la sortie MIDI va au logiciel hôte). */
inline std::function<juce::var (const juce::var&)>& standaloneMidiOutHook()
{
    static std::function<juce::var (const juce::var&)> hook;
    return hook;
}

inline std::function<int (int)>& standaloneInputHook()
{
    static std::function<int (int)> hook;
    return hook;
}

// v0.46 fix6 : l'application autonome montre un écran de lancement (la mascotte) pendant le
// chargement ; l'éditeur l'appelle quand l'interface est prête (fonction native « appStart »), pour
// qu'il s'efface. Vide dans un logiciel hôte et sur tablette. Fil des messages uniquement.
// v0.46 fix11 : journal du démarrage (ordinateur) : chaque étape et son heure depuis le lancement,
// dans Documents/Groovebox/demarrage.log (réécrit à chaque lancement). Fil des messages uniquement.
inline void startupMark (const char* what)
{
   #if ! JUCE_IOS
    static const double t0 = juce::Time::getMillisecondCounterHiRes();
    static bool first = true;
   #if JUCE_ANDROID
    // v0.49 (essai 3) : aussi sur Android, dans le dossier de l'appli (rapport de plantage, CrashGuard.h)
    const auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Groovebox").getChildFile ("demarrage.log");
   #else
    const auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Groovebox").getChildFile ("demarrage.log");
   #endif
    if (first) { f.getParentDirectory().createDirectory(); f.replaceWithText ("OFAD GOLEM - demarrage " + juce::Time::getCurrentTime().toString (true, true) + "\n"); first = false; }
    f.appendText (juce::String ((int) std::lround (juce::Time::getMillisecondCounterHiRes() - t0)).paddedLeft (' ', 7) + " ms  " + juce::String::fromUTF8 (what) + "\n");
   #else
    juce::ignoreUnused (what);
   #endif
}

#if JUCE_ANDROID
/** v0.46 fix21 : écran gardé allumé (Android, src/StandaloneHook.cpp). Fil des messages. */
void androidKeepScreenOn (bool on);
/** v0.46.2 : aucune vue de la fenêtre ne « tire » au doigt (effet d'étirement d'Android 12+ au bord,
    défilement de la vue web) : l'écran reste fixe. Fil des messages. */
void androidPinViews();
/** v0.46.2 : remet en (0, 0) toute vue de la fenêtre qui a défilé (appelée à chaque battement). */
void androidResetViewScroll();
/** v0.46.2 : la fenêtre de l'appli a la main (faux : une autre appli ou le sélecteur de fichiers devant). */
bool androidHasWindowFocus();
#endif

inline std::function<void()>& uiReadyHook()
{
    static std::function<void()> hook;
    return hook;
}
} // namespace grv
