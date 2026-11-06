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
