/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Préréglages de l'utilisateur : un son d'instrument, un réglage d'effet ou le son d'un pad de
    batterie, enregistrés dans des banques que l'utilisateur nomme lui-même. Ils servent dans
    tous les projets.

    Rangement (un fichier texte par préréglage, lisible et facile à partager) :
        <dossier des préréglages>/<nom du synthé ou de l'effet>/<banque>/<nom>.gbp
        <dossier des préréglages>/Pads/<banque>/<nom>.gbp
    Dossier des préréglages : choisi dans les Réglages (voir Folders.h) ; par défaut
    <Documents>/Groovebox/Préréglages, et sur Android le dossier privé de l'application.

    Valeurs rangées par identifiant de réglage et en unités lisibles (Hz, ms, dB…) : un préréglage
    reste valable si la plage d'un réglage change. Noms nettoyés (aucun chemin, aucun caractère
    interdit), fichiers lus avec une taille plafonnée, écriture dans un fichier temporaire puis
    remplacement (jamais de fichier à moitié écrit).
*/

#pragma once

#include <juce_core/juce_core.h>

#include <string>
#include <utility>
#include <vector>

namespace grv
{

class UserPresets
{
public:
    struct Entry { juce::String bank, name, kind; };
    using Values = std::vector<std::pair<std::string, float>>;

    /** Dossier racine des préréglages (créé à la demande). */
    static juce::File root();

    /** Nom sûr pour un fichier (1 à 60 caractères), ou vide si rien d'utilisable. */
    static juce::String cleanName (const juce::String& raw);

    /** Dossier d'un type de module ("drumA", "eq"…) ou des pads ("Pads") : nommé d'après le synthé
        ou l'effet (« Mono semi-modulaire », « Égaliseur »…). Les préréglages d'avant (rangés par
        identifiant de type) y sont déplacés au premier passage. */
    static juce::File folder (const juce::String& moduleType);
    static juce::String folderName (const juce::String& moduleType);

    /** Préréglages d'un dossier, triés par banque puis par nom (au plus 2000). */
    static std::vector<Entry> list (const juce::String& moduleType);

    /** Banques existantes d'un dossier. */
    static juce::StringArray banks (const juce::String& moduleType);

    enum class SaveResult { ok, exists, invalidName, writeError };
    static SaveResult save (const juce::String& moduleType, const juce::String& bank, const juce::String& name,
                            const juce::String& kind, const Values& values, bool overwrite);

    /** Lit un préréglage. kind : famille du pad (vide pour un module). */
    static bool load (const juce::String& moduleType, const juce::String& bank, const juce::String& name, Values& values, juce::String& kind, int* fileVersion = nullptr);

    /** Met le fichier à la corbeille du système (ou le supprime si la corbeille n'existe pas). */
    static bool remove (const juce::String& moduleType, const juce::String& bank, const juce::String& name);

    static constexpr int maxFileBytes = 512 * 1024;

private:
    static void adoptLegacy (const juce::String& moduleType, const juce::File& dir);
    static juce::String formerName (const juce::String& moduleType);
};

} // namespace grv
