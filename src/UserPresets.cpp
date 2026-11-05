/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Préréglages de l'utilisateur (voir UserPresets.h).
*/

#include "UserPresets.h"
#include "Folders.h"
#include "engine/Catalog.h"
#include "ParseGuard.h" // v0.46.1 (audit F3)

#include <algorithm>
#include <cmath>

namespace grv
{

namespace
{
const char* const extension = ".gbp";

/** v0.46.1 (audit F3) : un préréglage peut venir d'un tiers : lecture refusée si le JSON s'imbrique
    sans fin (le lecteur de JUCE est récursif). */
juce::var parsePresetFile (const juce::File& f)
{
    const auto text = f.loadFileAsString();
    if (! jsonNestingWithin (text.toRawUTF8(), text.getNumBytesAsUTF8())) return {};
    return juce::JSON::parse (text);
}

juce::File presetFile (const juce::String& moduleType, const juce::String& bank, const juce::String& name)
{
    const auto b = UserPresets::cleanName (bank), n = UserPresets::cleanName (name);
    if (b.isEmpty() || n.isEmpty()) return {};
    return UserPresets::folder (moduleType).getChildFile (b).getChildFile (n + extension);
}
} // namespace

juce::File UserPresets::root()
{
    return Folders::get (Folders::presets); // choisi dans les Réglages (par défaut <Documents>/Groovebox/Préréglages)
}

juce::String UserPresets::cleanName (const juce::String& raw)
{
    auto s = raw.substring (0, 200);
    juce::String out;
    for (auto c : s)
    {
        if (c < 0x20 || c == 0x7F) continue;                            // caractères de contrôle
        if (juce::String ("<>:\"/\\|?*").containsChar (c)) { out << ' '; continue; }
        out << c;
    }
    out = out.trim();
    while (out.endsWithChar ('.') || out.startsWithChar ('.')) out = out.trimCharactersAtEnd (".").trimCharactersAtStart (".").trim();
    while (out.contains ("  ")) out = out.replace ("  ", " ");
    out = out.substring (0, 60).trim();
    // noms réservés de Windows (« CON », « COM1 », aussi suivis d'une extension : « CON.txt »)
    const auto stem = out.upToFirstOccurrenceOf (".", false, false).trim().toUpperCase();
    const bool reserved = stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL"
                  || ((stem.startsWith ("COM") || stem.startsWith ("LPT")) && stem.length() == 4 && juce::CharacterFunctions::isDigit (stem[3]));
    return reserved ? "_" + out : out;
}

juce::String UserPresets::formerName (const juce::String& moduleType)
{
    // noms affichés jusqu'à la v0.31.2 (dossiers de préréglages de l'utilisateur à reprendre)
    static const std::pair<const char*, const char*> names[] = {
        { "drumA", "Batterie analogique" }, { "drum80", "Batterie 80" }, { "drum90", "Batterie 90" },
        { "drumImp", "Batterie Impact" }, { "drumNrv", "Batterie Nerve" }, { "dfam", "Percussion \xC3\xA0 s\xC3\xA9quenceur" },
        { "acid", "Basse acide" }, { "mono", "Basse mono" }, { "ms20", "Mono semi-modulaire" }, { "sh101", "Mono \xC3\xA0 m\xC3\xA9langeur" },
        { "poly5", "Poly analogique" }, { "polyj", "Poly chorus" }, { "hybrid", "Poly hybride" }, { "morph", "Wave morph" },
        { "pads", "Nappes" }, { "blo", "Poly \xC3\xA0 tables d'ondes" }, { "ob6", "Poly \xC3\xA0 filtre d'\xC3\xA9tat" },
        { "sampler", "Sampler \xC3\xA0 d\xC3\xA9" "coupes" }, { "keys", "Sampler chromatique" }, { "sp", "Sampler \xC3\xA0 pads" },
        { "octa", "Sampler de performance" }, { "voc16", "Vocodeur 16 bandes" }, { "vocstr", "Vocodeur \xC3\xA0 cordes" },
        { "vocspec", "Vocodeur spectral" }, { "vocsyn", "Voix de synth\xC3\xA8se" },
    };
    for (const auto& [type, name] : names)
        if (moduleType == type) return juce::String::fromUTF8 (name);
    return {};
}

juce::String UserPresets::folderName (const juce::String& moduleType)
{
    if (moduleType == "Pads") return "Pads";
    const auto type = moduleType.toStdString();
    const auto* d = engine::findInstrument (type);
    if (d == nullptr) d = engine::findEffect (type);
    auto t = cleanName (d != nullptr ? juce::String::fromUTF8 (d->name.c_str()) : moduleType);
    return t.isEmpty() ? juce::String ("Divers") : t;
}

void UserPresets::adoptLegacy (const juce::String& moduleType, const juce::File& dir)
{
    // avant la v0.15, un dossier par identifiant de type (« ms20 », « eq »…) ; maintenant par nom du
    // synthé ou de l'effet. Les préréglages sont déplacés un par un, seulement ceux de ce type
    // (Windows ne distingue pas « pads » de « Pads »), sans jamais écraser un fichier.
    static juce::CriticalSection lock;
    static juce::StringArray done;
    const juce::ScopedLock sl (lock);
    const auto doneKey = dir.getParentDirectory().getFullPathName() + "|" + moduleType;
    if (done.contains (doneKey)) return;
    done.add (doneKey);
    const auto t = cleanName (moduleType);
    if (t.isEmpty()) return;
    // v0.31.3 : les synthés ont changé de nom affiché ; les dossiers à l'ancien nom sont repris aussi
    juce::Array<juce::File> sources { root().getChildFile (t) };
    if (const auto former = cleanName (formerName (moduleType)); former.isNotEmpty()) sources.add (root().getChildFile (former));
    int moved = 0;
    for (const auto& legacy : sources)
    {
    if (! legacy.isDirectory() || legacy == dir) continue;
    for (const auto& bank : legacy.findChildFiles (juce::File::findDirectories, false))
        for (const auto& f : bank.findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension))
        {
            if (moved >= 4000 || f.getSize() > maxFileBytes) continue;
            const auto v = parsePresetFile (f);
            if (! v.isObject() || v.getProperty ("module", {}).toString() != moduleType) continue;
            const auto target = dir.getChildFile (bank.getFileName()).getChildFile (f.getFileName());
            if (target.exists() || ! target.getParentDirectory().createDirectory()) continue;
            if (f.moveFileTo (target)) ++moved;
        }
    }
}

juce::File UserPresets::folder (const juce::String& moduleType)
{
    const auto dir = root().getChildFile (folderName (moduleType));
    adoptLegacy (moduleType, dir);
    return dir;
}

juce::StringArray UserPresets::banks (const juce::String& moduleType)
{
    juce::StringArray out;
    for (const auto& d : folder (moduleType).findChildFiles (juce::File::findDirectories, false))
        if (out.size() < 200 && cleanName (d.getFileName()) == d.getFileName()) out.add (d.getFileName()); // nom « propre » seulement
    out.sortNatural();
    return out;
}

std::vector<UserPresets::Entry> UserPresets::list (const juce::String& moduleType)
{
    std::vector<Entry> out;
    for (const auto& bank : banks (moduleType))
    {
        auto files = folder (moduleType).getChildFile (bank).findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        for (const auto& f : files)
        {
            if (out.size() >= 2000) return out;
            if (cleanName (f.getFileNameWithoutExtension()) != f.getFileNameWithoutExtension()) continue; // renommé à la main : illisible ici
            Entry e { bank, f.getFileNameWithoutExtension(), {} };
            // famille du pad, lue dans le fichier (petit) seulement pour les pads
            if (moduleType == "Pads" && f.getSize() <= maxFileBytes)
                if (const auto v = parsePresetFile (f); v.isObject())
                    e.kind = v.getProperty ("kind", {}).toString().substring (0, 20);
            out.push_back (std::move (e));
        }
    }
    return out;
}

UserPresets::SaveResult UserPresets::save (const juce::String& moduleType, const juce::String& bank, const juce::String& name,
                                           const juce::String& kind, const Values& values, bool overwrite)
{
    const auto file = presetFile (moduleType, bank, name);
    if (file == juce::File()) return SaveResult::invalidName;
    if (file.existsAsFile() && ! overwrite) return SaveResult::exists;
    if (! file.getParentDirectory().createDirectory()) return SaveResult::writeError;

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("format", "groovebox-preset");
    obj->setProperty ("version", 5); // 2 : réserve de 6 dB du poly à chorus et du wave morph ; 3 : divisions jusqu'à 16 mesures ; 4 : machines du sampler de performance ; 5 : Static retirée
    obj->setProperty ("module", moduleType);
    obj->setProperty ("kind", kind);
    obj->setProperty ("name", cleanName (name));
    auto* vals = new juce::DynamicObject();
    for (const auto& [id, v] : values)
        if (std::isfinite (v)) vals->setProperty (juce::Identifier (juce::String (id)), std::round ((double) v * 10000.0) / 10000.0); // 4 décimales : lisible
    obj->setProperty ("values", juce::var (vals));
    const auto text = juce::JSON::toString (juce::var (obj));

    // fichier temporaire à côté, puis remplacement : jamais de préréglage à moitié écrit
    const auto temp = file.getSiblingFile (file.getFileNameWithoutExtension() + ".tmp");
    if (! temp.replaceWithText (text, false, false, "\n")) return SaveResult::writeError;
    if (! temp.moveFileTo (file)) { temp.deleteFile(); return SaveResult::writeError; }
    return SaveResult::ok;
}

bool UserPresets::load (const juce::String& moduleType, const juce::String& bank, const juce::String& name, Values& values, juce::String& kind, int* fileVersion)
{
    values.clear();
    const auto file = presetFile (moduleType, bank, name);
    if (! file.existsAsFile() || file.getSize() > maxFileBytes) return false;
    const auto v = parsePresetFile (file);
    if (! v.isObject() || v.getProperty ("format", {}).toString() != "groovebox-preset") return false;
    kind = v.getProperty ("kind", {}).toString().substring (0, 20);
    if (fileVersion != nullptr) *fileVersion = (int) v.getProperty ("version", 1);
    if (auto* obj = v.getProperty ("values", {}).getDynamicObject())
        for (const auto& p : obj->getProperties())
        {
            if (values.size() >= 1024) break;
            const auto& x = p.value;
            if (! (x.isDouble() || x.isInt() || x.isInt64())) continue;
            const auto f = (float) (double) x;
            if (std::isfinite (f)) values.emplace_back (p.name.toString().substring (0, 64).toStdString(), f);
        }
    // version 1 : le poly à chorus et le wave morph n'avaient pas leur réserve de 6 dB (même son qu'avant)
    if ((int) v.getProperty ("version", 1) < 2 && (moduleType == "polyj" || moduleType == "morph"))
        for (auto& [id, val] : values)
            if (id == "level") val = std::max (-24.0f, val - 6.0f);
    // version 3 et avant : 5 machines (Flex, Static, Thru, Neighbor, Pickup) ; version 4 : 3 (Flex, Static, Pickup) ;
    // depuis la version 5 : Flex, Pickup
    if (const auto ver = (int) v.getProperty ("version", 1); ver < 5 && moduleType == "octa")
        for (auto& [id, val] : values)
            if (id == "machine") val = (ver < 4 ? val >= 3.5f : val >= 1.5f) ? 1.0f : 0.0f;
    return true;
}

bool UserPresets::remove (const juce::String& moduleType, const juce::String& bank, const juce::String& name)
{
    const auto file = presetFile (moduleType, bank, name);
    if (! file.existsAsFile()) return false;
    const bool ok = file.moveToTrash() || file.deleteFile();
    // banque vide : on retire aussi son dossier (s'il est vide)
    const auto dir = file.getParentDirectory();
    if (ok && dir.findChildFiles (juce::File::findFilesAndDirectories, false).isEmpty()) dir.deleteFile();
    return ok;
}

} // namespace grv
