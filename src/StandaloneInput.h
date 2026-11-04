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

#include <cmath>
#include <functional>
#include <juce_core/juce_core.h>

namespace grv
{
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
   #if ! (JUCE_ANDROID || JUCE_IOS)
    static const double t0 = juce::Time::getMillisecondCounterHiRes();
    static bool first = true;
    const auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Groovebox").getChildFile ("demarrage.log");
    if (first) { f.getParentDirectory().createDirectory(); f.replaceWithText ("OFAD GOLEM - demarrage " + juce::Time::getCurrentTime().toString (true, true) + "\n"); first = false; }
    f.appendText (juce::String ((int) std::lround (juce::Time::getMillisecondCounterHiRes() - t0)).paddedLeft (' ', 7) + " ms  " + juce::String::fromUTF8 (what) + "\n");
   #else
    juce::ignoreUnused (what);
   #endif
}

#if JUCE_ANDROID
/** v0.46 fix21 : écran gardé allumé (Android, src/StandaloneHook.cpp). Fil des messages. */
void androidKeepScreenOn (bool on);
#endif

inline std::function<void()>& uiReadyHook()
{
    static std::function<void()> hook;
    return hook;
}
} // namespace grv
