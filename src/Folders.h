/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Dossiers de travail choisis dans les Réglages : échantillons, projets, préréglages.
    Ils servent dans tous les projets et sont rangés à part, dans le dossier de l'application :
        <données de l'application>/Groovebox/dossiers.json

    Par défaut :
        <Documents>/Groovebox/Échantillons, …/Projets, …/Préréglages (ordinateur)
        dossier privé de l'application (Android : le système ne donne pas de chemin de fichier
        pour un dossier choisi ; les échantillons importés y sont copiés pour être retrouvés).

    Sécurité : un dossier n'est changé que par le sélecteur du système, à la demande de
    l'utilisateur. Les listes et les lectures restent DANS le dossier choisi : chemin relatif
    nettoyé, ni « .. », ni chemin absolu, ni lien symbolique ; nombres d'éléments, profondeur et
    tailles plafonnés. L'interface ne transmet jamais de chemin absolu.
*/

#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace grv
{

class Folders
{
public:
    enum Kind { samples = 0, projects, presets, numKinds };

    static const char* key (Kind k) noexcept;
    static bool kindFrom (const juce::String& s, Kind& out) noexcept;

    /** Dossier en service (celui choisi, ou celui par défaut). Créé à la demande. */
    static juce::File get (Kind k);
    static juce::File defaultFor (Kind k);
    static bool isCustom (Kind k);

    /** Le dossier peut-il être changé ici ? (non sur Android et iOS) */
    static constexpr bool canChoose() noexcept
    {
       #if JUCE_ANDROID || JUCE_IOS
        return false;
       #else
        return true;
       #endif
    }

    /** Retient un dossier choisi par l'utilisateur ; faux si ce n'est pas un dossier utilisable. */
    static bool set (Kind k, const juce::File& dir);
    static void reset (Kind k);

    /** Fichier ou dossier désigné par un chemin relatif, à l'intérieur du dossier ; faux sinon. */
    static bool resolve (Kind k, const juce::String& relative, juce::File& out);

    struct Item { juce::String name; bool folder = false; juce::int64 bytes = 0; };
    /** Contenu d'un sous-dossier (dossiers puis fichiers des extensions données), trié, plafonné. */
    static std::vector<Item> list (Kind k, const juce::String& relative, const juce::StringArray& extensions, bool& ok);

    /** v0.47.0 : les enregistrements audio (dossier de l'enregistreur) apparaissent dans la Bibliothèque des
        échantillons comme un dossier « Mes enregistrements » (lecture seule, chemins vérifiés comme les
        autres) : une boucle enregistrée se charge sans quitter l'application (le sélecteur du système
        faisait planter l'application sur Android). À régler au démarrage. */
    static void setRecordingsFolder (const juce::File& dir);
    static juce::String recordingsName();

    /** Nom de fichier sûr (sans chemin, sans caractère interdit), ou vide. */
    static juce::String cleanFileName (const juce::String& raw);

    static constexpr int maxItems = 1000;
    static constexpr int maxDepth = 12;

private:
    static juce::File settingsFile();
};

} // namespace grv
