/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Dossiers de travail (voir Folders.h).
*/

#include "Folders.h"
#include "UserPresets.h"

#include <algorithm>
#include <array>

namespace grv
{

namespace
{
juce::CriticalSection lock;
bool loaded = false;
std::array<juce::String, Folders::numKinds> chosen; // chemins absolus choisis (vide : par défaut)

juce::File base()
{
   #if JUCE_ANDROID || JUCE_IOS
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Groovebox");
   #else
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Groovebox");
   #endif
}

// un seul élément de chemin, sans caractère de contrôle ni séparateur (les noms existants sont gardés tels quels)
bool safeComponent (const juce::String& n)
{
    if (n.isEmpty() || n.length() > 255 || n == "." || n == "..") return false;
    for (auto c : n)
        if (c < 0x20 || c == 0x7F || c == '/' || c == '\\' || c == ':') return false;
    return true;
}

bool usable (const juce::File& d)
{
    if (d == juce::File() || ! juce::File::isAbsolutePath (d.getFullPathName())) return false;
    if (d.getParentDirectory() == d) return false;               // racine d'un disque : refusée
    if (d.isSymbolicLink()) return false;
    return d.isDirectory();
}
} // namespace

const char* Folders::key (Kind k) noexcept
{
    switch (k)
    {
        case samples:  return "samples";
        case projects: return "projects";
        case presets:  return "presets";
        default:       return "";
    }
}

bool Folders::kindFrom (const juce::String& s, Kind& out) noexcept
{
    for (int i = 0; i < numKinds; ++i)
        if (s == key ((Kind) i)) { out = (Kind) i; return true; }
    return false;
}

juce::File Folders::settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Groovebox").getChildFile ("dossiers.json");
}

juce::File Folders::defaultFor (Kind k)
{
    switch (k)
    {
        case samples:  return base().getChildFile (juce::String::fromUTF8 ("\xC3\x89" "chantillons"));
        case projects: return base().getChildFile ("Projets");
        case presets:  return base().getChildFile (juce::String::fromUTF8 ("Pr\xC3\xA9r\xC3\xA9glages"));
        default:       return base();
    }
}

static void loadSettings()
{
    if (loaded) return;
    loaded = true;
    const auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Groovebox").getChildFile ("dossiers.json");
    if (! f.existsAsFile() || f.getSize() > 16 * 1024) return;
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject()) return;
    for (int i = 0; i < Folders::numKinds; ++i)
    {
        const auto s = v.getProperty (Folders::key ((Folders::Kind) i), {}).toString().substring (0, 1024);
        if (s.isNotEmpty() && juce::File::isAbsolutePath (s)) chosen[(size_t) i] = s;
    }
}

bool Folders::isCustom (Kind k)
{
    if (! canChoose() || k < 0 || k >= numKinds) return false;
    const juce::ScopedLock sl (lock);
    loadSettings();
    return chosen[(size_t) k].isNotEmpty();
}

juce::File Folders::get (Kind k)
{
    if (k < 0 || k >= numKinds) return {};
    juce::File d;
    {
        const juce::ScopedLock sl (lock);
        loadSettings();
        if (canChoose() && chosen[(size_t) k].isNotEmpty()) d = juce::File (chosen[(size_t) k]);
    }
    // dossier choisi disparu (disque débranché…) : on revient au dossier par défaut sans rien effacer du réglage
    if (d == juce::File() || ! usable (d)) d = defaultFor (k);
    d.createDirectory();
    return d;
}

bool Folders::set (Kind k, const juce::File& dir)
{
    if (! canChoose() || k < 0 || k >= numKinds || ! usable (dir)) return false;
    const juce::ScopedLock sl (lock);
    loadSettings();
    chosen[(size_t) k] = dir.getFullPathName();
    auto* obj = new juce::DynamicObject();
    for (int i = 0; i < numKinds; ++i)
        if (chosen[(size_t) i].isNotEmpty()) obj->setProperty (key ((Kind) i), chosen[(size_t) i]);
    const auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    const auto tmp = f.getSiblingFile ("dossiers.tmp");
    if (! tmp.replaceWithText (juce::JSON::toString (juce::var (obj)), false, false, "\n")) return false;
    return tmp.moveFileTo (f);
}

void Folders::reset (Kind k)
{
    if (k < 0 || k >= numKinds) return;
    const juce::ScopedLock sl (lock);
    loadSettings();
    chosen[(size_t) k].clear();
    auto* obj = new juce::DynamicObject();
    for (int i = 0; i < numKinds; ++i)
        if (chosen[(size_t) i].isNotEmpty()) obj->setProperty (key ((Kind) i), chosen[(size_t) i]);
    const auto f = settingsFile();
    if (f.getParentDirectory().createDirectory())
    {
        const auto tmp = f.getSiblingFile ("dossiers.tmp");
        if (tmp.replaceWithText (juce::JSON::toString (juce::var (obj)), false, false, "\n")) tmp.moveFileTo (f);
    }
}

juce::String Folders::cleanFileName (const juce::String& raw)
{
    return UserPresets::cleanName (raw);
}

bool Folders::resolve (Kind k, const juce::String& relative, juce::File& out)
{
    const auto root = get (k);
    auto f = root;
    juce::StringArray parts;
    parts.addTokens (relative.substring (0, 2048).replaceCharacter ('\\', '/'), "/", "");
    parts.removeEmptyStrings();
    if (parts.size() > maxDepth) return false;
    for (const auto& p : parts)
    {
        // chaque élément doit être un nom simple et déjà propre : pas de « .. », pas de lecteur, pas de caractère interdit
        if (! safeComponent (p)) return false;
        f = f.getChildFile (p);
        if (f.isSymbolicLink()) return false;
    }
    if (f != root && ! f.isAChildOf (root)) return false;
    out = f;
    return true;
}

std::vector<Folders::Item> Folders::list (Kind k, const juce::String& relative, const juce::StringArray& extensions, bool& ok)
{
    std::vector<Item> out;
    juce::File dir;
    ok = resolve (k, relative, dir) && dir.isDirectory();
    if (! ok) return out;
    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles))
    {
        if (out.size() >= (size_t) maxItems) break;
        const auto& f = e.getFile();
        const auto name = f.getFileName();
        if (! safeComponent (name) || f.isSymbolicLink()) continue;    // nom inutilisable ici, ou lien : ignoré
        if (e.isDirectory()) { out.push_back ({ name, true, 0 }); continue; }
        bool match = extensions.isEmpty();
        for (const auto& x : extensions) if (f.hasFileExtension (x)) { match = true; break; }
        if (match) out.push_back ({ name, false, e.getFileSize() });
    }
    std::sort (out.begin(), out.end(), [] (const Item& a, const Item& b)
    {
        if (a.folder != b.folder) return a.folder;
        return a.name.compareNatural (b.name) < 0;
    });
    return out;
}

} // namespace grv
