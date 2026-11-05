/*
    Groovebox — OFAD — AGPL-3.0-or-later
*/

#include "PluginEditor.h"
#include "StandaloneInput.h"
#include "engine/Bridge.h"
#include "UserPresets.h"
#include "Folders.h"
#include "PerfStats.h"

#include <unordered_map>

#include <UiData.h>

namespace grv
{

using namespace engine;

namespace
{
juce::String mimeFor (const juce::String& filename)
{
    const auto ext = filename.fromLastOccurrenceOf (".", false, false).toLowerCase();
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "js" || ext == "mjs")   return "text/javascript";
    if (ext == "css")                  return "text/css";
    if (ext == "json" || ext == "map") return "application/json";
    if (ext == "svg")                  return "image/svg+xml";
    if (ext == "png")                  return "image/png";
    if (ext == "webp")                 return "image/webp";
    if (ext == "woff2")                return "font/woff2";
    if (ext == "ico")                  return "image/vnd.microsoft.icon";
    return "application/octet-stream";
}

using Args = juce::Array<juce::var>;
using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

/** Arguments vérifiés : un nombre hors bornes ou d'un autre type est refusé (valeur de repli). */
int intArg (const Args& a, int i, int lo, int hi, int fallback)
{
    if (a.size() <= i || ! (a[i].isInt() || a[i].isInt64() || a[i].isDouble() || a[i].isBool())) return fallback;
    const auto d = (double) a[i];
    if (! std::isfinite (d)) return fallback;
    const auto v = (int) std::llround (d);
    return v < lo || v > hi ? fallback : v;
}
float unitArg (const Args& a, int i, float fallback = 0.0f)
{
    if (a.size() <= i || ! (a[i].isInt() || a[i].isInt64() || a[i].isDouble())) return fallback;
    const auto d = (float) (double) a[i];
    return std::isfinite (d) ? juce::jlimit (0.0f, 1.0f, d) : fallback;
}
float floatArg (const Args& a, int i, float lo, float hi, float fallback)
{
    if (a.size() <= i || ! (a[i].isInt() || a[i].isInt64() || a[i].isDouble())) return fallback;
    const auto d = (float) (double) a[i];
    return std::isfinite (d) ? juce::jlimit (lo, hi, d) : fallback;
}
bool boolArg (const Args& a, int i) { return a.size() > i && (bool) a[i]; }
juce::String strArg (const Args& a, int i, int maxLength)
{
    if (a.size() <= i || ! a[i].isString()) return {};
    return a[i].toString().substring (0, maxLength);
}

/** Les réponses sont des textes JSON en ASCII : l'interface les décode avec JSON.parse. */
juce::var json (const std::string& s) { return juce::var (juce::String (s)); }
} // namespace

//==============================================================================
/** v0.46 fix30 : langue choisie par l'utilisateur (données de l'application : %APPDATA% sous Windows,
    dossier privé de l'application sous Android). */
static juce::File userLangFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("OFAD GOLEM").getChildFile ("language.txt");
}
/** v0.46 fix30 : langue choisie à l'installation (Windows). */
static juce::File installLangFile()
{
    return juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
               .getChildFile ("One For All Distribution").getChildFile ("Groovebox").getChildFile ("language.txt");
}
/** v0.46 fix30f : langue à utiliser maintenant (choix de l'utilisateur, sauf installation plus récente). Relue à
    chaque chargement de l'interface (fonction « getLang ») : les données d'initialisation sont figées à l'ouverture
    de la fenêtre, un rechargement après un changement de langue y retrouvait l'ancienne (boucle sur Android). */
static juce::String currentLang()
{
    const auto inst = installLangFile().existsAsFile() ? installLangFile().loadFileAsString().trim().toLowerCase() : juce::String();
    const auto user = userLangFile().existsAsFile() ? userLangFile().loadFileAsString().trim().toLowerCase() : juce::String();
    const auto code = [] (const juce::String& v) -> juce::String { return v.startsWith ("en") ? "en" : v.startsWith ("fr") ? "fr" : ""; };
    const auto stampOf = [] (const juce::String& v) { return v.fromFirstOccurrenceOf (" ", false, false).trim(); };
    juce::String lang = code (user);
    if (code (inst).isNotEmpty() && (lang.isEmpty() || stampOf (user) != stampOf (inst)))
        lang = code (inst);
    return lang;
}

juce::WebBrowserComponent::Options GrooveboxEditor::createOptions()
{
    using Options = juce::WebBrowserComponent::Options;
    auto& proc = processor;
    auto& eng = processor.getEngine();
    auto& seq = eng.sequencer();

    auto options = Options{}
        .withBackend (Options::Backend::webview2)
        .withWinWebView2Options (Options::WinWebView2{}
                                     .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("OFAD-Groovebox-WebView"))
                                     .withStatusBarDisabled()
                                     .withBackgroundColour (juce::Colour (0xff15171b)))
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider ([this] (const auto& url) { return getResource (url); });

    // v0.46 fix30 : langue de l'interface, gardée par l'application (la WebView d'Android ne garde pas la mémoire
    // de la page) :
    //  - choix de l'utilisateur (accueil, Réglages, premier lancement) : <données de l'application>/OFAD GOLEM/language.txt,
    //    « fr » ou « en » suivi de l'installation en cours au moment du choix ;
    //  - choix de l'installateur (Windows) : ProgramData\One For All Distribution\Groovebox\language.txt,
    //    « en 20261104050100 » (langue, heure de l'installation).
    // Une nouvelle installation impose sa langue ; un choix fait ensuite dans l'application est gardé.
    // Rien de connu : l'interface pose la question au premier lancement.
    options = options.withInitialisationData ("lang", currentLang());

    auto fn = [&options] (const char* name, std::function<void (const Args&, Completion)> f)
    {
        options = options.withNativeFunction (name, [f] (const Args& a, Completion done) { f (a, std::move (done)); });
    };

    const auto T = numTracks - 1, C = numChannels - 1, L = Sequencer::numLanes - 1, S = Sequencer::maxSteps - 1;

    // --- Lecture ------------------------------------------------------------------------
    fn ("getCatalog", [] (const Args&, Completion done) { done (json (bridge::catalogJson())); });
    fn ("getTracks", [&eng] (const Args&, Completion done) { done (json (bridge::tracksJson (eng))); });
    fn ("getChannel", [&eng, C] (const Args& a, Completion done) { done (json (bridge::channelJson (eng, intArg (a, 0, 0, C, 0)))); });
    fn ("getPattern", [&eng, T] (const Args& a, Completion done) { done (json (bridge::patternJson (eng, intArg (a, 0, 0, T, 0)))); });
    fn ("getExtras", [&eng, T] (const Args& a, Completion done) { done (json (bridge::extrasJson (eng, intArg (a, 0, 0, T, 0)))); });
    fn ("getScenes", [&eng] (const Args&, Completion done) { done (json (bridge::scenesJson (eng))); });
    fn ("getSpectrum", [&eng, C] (const Args& a, Completion done)
    { done (json (bridge::spectrumJson (eng, intArg (a, 0, 0, C, 0), intArg (a, 1, 0, numInserts - 1, 0), intArg (a, 2, 8, 200, 96)))); });
    fn ("getWave", [&eng, T] (const Args& a, Completion done) { done (json (bridge::waveJson (eng, intArg (a, 0, 0, T, 0), intArg (a, 1, 0, maxSamples - 1, 0), intArg (a, 2, 16, 2048, 400)))); });
    // v0.46 fix30 : langue choisie dans l'application (gardée avec l'installation en cours)
    fn ("setLang", [] (const Args& a, Completion done)
    {
        const auto l = a.size() > 0 ? a[0].toString() : juce::String();
        if (l == "fr" || l == "en")
        {
            const auto inst = installLangFile().existsAsFile() ? installLangFile().loadFileAsString().trim().toLowerCase() : juce::String();
            const auto f = userLangFile();
            f.getParentDirectory().createDirectory();
            f.replaceWithText (l + " " + inst.fromFirstOccurrenceOf (" ", false, false).trim());
        }
        done (juce::var (true));
    });
    fn ("getLang", [] (const Args&, Completion done) { done (juce::var (currentLang())); });
    fn ("uiLog", [] (const Args& a, Completion done)
    {
        std::fprintf (stderr, "[interface] %s\n", (a.size() > 0 ? a[0].toString().substring (0, 2000) : juce::String()).toRawUTF8());
        done (juce::var());
    });

    // v0.46 fix6 : démarrage de l'interface. L'application autonome montre l'accueil (choix du
    // projet) une seule fois par lancement : l'éditeur rouvert ne le remontre pas. L'écran de
    // lancement natif (la mascotte) s'efface à ce moment-là.
    fn ("appStart", [this] (const Args&, Completion done)
    {
        static bool welcomeShown = false;
        const bool standalone = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
        const bool welcome = standalone && ! welcomeShown;
        if (standalone) welcomeShown = true;
        done (juce::var (juce::String (std::string ("{\"standalone\":") + (standalone ? "true" : "false")
                                       + ",\"welcome\":" + (welcome ? "true" : "false") + "}")));
        if (auto& hook = grv::uiReadyHook()) hook();
    });

    // v0.46 fix8 : montage audio (onglet Lancement › Audio)
    fn ("getMontage", [&proc] (const Args&, Completion done) { done (json (proc.montageJson())); });
    fn ("montageSetClips", [&proc] (const Args& a, Completion done) { proc.montageSetClips (strArg (a, 0, 4000000)); done (json (proc.montageJson())); });
    fn ("montageSetFlags", [&proc] (const Args& a, Completion done)
    {
        auto num = [&a] (int i, double def) { return a.size() > i && ! a[i].isVoid() ? (double) a[i] : def; };
        proc.montageSetFlags (boolArg (a, 0), boolArg (a, 1), num (2, 0.0), num (3, 16.0));
        done (json (proc.montageJson()));
    });
    fn ("montagePlayhead", [&proc] (const Args& a, Completion done) { proc.montageSetPlayhead (a.size() > 0 ? (double) a[0] : 0.0); done (json (proc.montageJson())); });
    fn ("montageRecord", [&proc] (const Args& a, Completion done)
    {
        const auto error = proc.montageRecord (boolArg (a, 0));
        done (json (error.isEmpty() ? proc.montageJson() : juce::JSON::toString (error).toStdString()));
    });
    fn ("montageExport", [&proc] (const Args& a, Completion done)
    {
        const auto error = proc.montageExport (intArg (a, 0, 0, 1, 0), intArg (a, 1, 0, 1, 0) == 1);
        done (json (error.isEmpty() ? proc.montageJson() : juce::JSON::toString (error).toStdString()));
    });
    fn ("montagePeaks", [&proc] (const Args& a, Completion done) { done (juce::var (proc.montagePeaks (strArg (a, 0, 80)))); });

    // v0.45.1 : l'analyseur de spectre n'est calculé que lorsqu'un écran l'affiche
    fn ("setSpectrum", [this] (const Args& a, Completion done) { spectrumOn = intArg (a, 0, 0, 1, 0); done (juce::var()); });

    fn ("setFocus", [this, &eng, C] (const Args& a, Completion done)
    {
        focusChannel = intArg (a, 0, 0, C, 0);
        if (focusChannel < numTracks) eng.focusTrack.store (focusChannel);
        done (json (bridge::channelJson (eng, focusChannel)));
    });

    // --- Réglages -------------------------------------------------------------------------
    fn ("setParam", [&eng] (const Args& a, Completion done)
    {
        const auto g = intArg (a, 0, 0, numSlots - 1, -1);
        if (g >= 0 && eng.describe (g) != nullptr)
        {
            const auto v = unitArg (a, 1, eng.getParam (g));
            eng.setParam (g, v);
            eng.recordMoveUi (g, v); // morceau : mouvement enregistré (et courbe tue pendant le geste)
        }
        done (juce::var());
    });
    fn ("resetParam", [&eng] (const Args& a, Completion done)
    {
        const auto g = intArg (a, 0, 0, numSlots - 1, -1);
        if (const auto* d = g >= 0 ? eng.describe (g) : nullptr) eng.setParam (g, d->toNorm (d->def));
        done (juce::var (g >= 0 ? (double) eng.getParam (g) : 0.0));
    });

    // --- Structure -----------------------------------------------------------------------
    fn ("setInstrument", [&eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        const auto type = strArg (a, 1, 32).toStdString();
        if (t >= 0 && eng.setInstrument (t, type, nowMs()) && type == "octa")
        {
            // v0.42 : Pieuvre 8 — égaliseur et compresseur de sortie (inserts 1 et 2 s'ils sont libres ;
            // compresseur contourné au départ : rien ne change tant qu'on ne l'active pas)
            const auto& ins = eng.getInsertTypes (t);
            if (ins[0].empty() && ins[1].empty())
            {
                eng.setInsert (t, 0, "eq", nowMs());
                eng.setInsert (t, 1, "comp", nowMs());
                if (const auto g = eng.findParam (t, "mix", "bypass2"); g >= 0) eng.setParam (g, eng.describe (g)->toNorm (1.0f));
            }
            eng.setPlayLane (t, 0);
        }
        done (json (bridge::channelJson (eng, std::max (0, t))));
    });
    // v0.42 : piste interne jouée au clavier et en MIDI (Pieuvre 8 : T1 à T8)
    fn ("setPlayLane", [&eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        if (t >= 0) eng.setPlayLane (t, intArg (a, 1, 0, 15, 0));
        done (juce::var());
    });
    fn ("setInsert", [&eng, C] (const Args& a, Completion done)
    {
        const auto ch = intArg (a, 0, 0, C, -1), s = intArg (a, 1, 0, numInserts - 1, -1);
        if (ch >= 0 && s >= 0) eng.setInsert (ch, s, strArg (a, 2, 32).toStdString(), nowMs());
        done (json (bridge::channelJson (eng, std::max (0, ch))));
    });
    fn ("setPadInsert", [&eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1), pad = intArg (a, 1, 0, numPads - 1, -1), s = intArg (a, 2, 0, padInserts - 1, -1);
        if (t >= 0 && pad >= 0 && s >= 0) eng.setPadInsert (t, pad, s, strArg (a, 3, 32).toStdString(), nowMs());
        done (json (bridge::channelJson (eng, std::max (0, t))));
    });
    fn ("moveInsert", [&eng, &proc, C] (const Args& a, Completion done)
    {
        const auto ch = intArg (a, 0, 0, C, -1);
        // v0.46 : par le processeur, pour que commandes MIDI et macros suivent l'effet
        if (ch >= 0) proc.moveInsert (ch, intArg (a, 1, 0, numInserts - 1, 0), intArg (a, 2, 0, numInserts - 1, 0));
        done (json (bridge::channelJson (eng, std::max (0, ch))));
    });
    fn ("applyPreset", [&eng, C] (const Args& a, Completion done)
    {
        const auto ch = intArg (a, 0, 0, C, -1);
        // emplacement : -1 instrument, 0..3 insert, 100 + pad × 2 + n : insert de pad
        auto slot = intArg (a, 1, -1, 100 + numPads * padInserts - 1, -1);
        if (slot >= numInserts && slot < 100) slot = -2;
        if (ch >= 0 && slot >= -1) eng.applyPreset (ch, slot, strArg (a, 2, 80).toStdString());
        done (json (bridge::channelJson (eng, std::max (0, ch))));
    });
    // --- Préréglages de l'utilisateur ------------------------------------------------------
    // Portée : "inst" (instrument de la piste), "fx" (insert), "pad" (un pad d'une batterie).
    // Le type de module est toujours celui du moteur (jamais celui qu'envoie l'interface).
    struct PresetTarget { bool ok = false; int channel = 0, slot = -1; juce::String folder, kind; std::string prefix; };
    auto presetTarget = [&eng, C] (const Args& a) -> PresetTarget
    {
        PresetTarget t;
        const auto scope = strArg (a, 0, 8);
        t.channel = intArg (a, 1, 0, C, -1);
        if (t.channel < 0) return t;
        if (scope == "fx")
        {
            t.slot = intArg (a, 2, 0, numInserts - 1, -1);
            if (const auto* d = t.slot >= 0 ? eng.effectDesc (t.channel, t.slot) : nullptr) { t.folder = d->type; t.ok = true; }
        }
        else if (const auto* d = eng.instrumentDesc (t.channel))
        {
            if (scope == "inst") { t.folder = d->type; t.ok = true; }
            else if (scope == "pad" && d->drumKit && d->type == "octa")
            {
                // v0.42 : réglages d'une piste interne de Pieuvre 8 (son bloc « t3_… »)
                const auto pad = intArg (a, 3, 0, 7, -1);
                if (pad < 0) return t;
                t.folder = "Pieuvre 8";
                t.prefix = "t" + std::to_string (pad + 1) + "_";
                t.ok = true;
            }
            else if (scope == "pad" && d->drumKit)
            {
                const auto pad = intArg (a, 3, 0, 15, -1);
                if (pad < 0) return t;
                t.folder = "Pads";
                t.kind = juce::String (pad < (int) d->padKinds.size() ? d->padKinds[(size_t) pad] : std::string());
                t.prefix = std::string ("p") + (pad < 9 ? "0" : "") + std::to_string (pad + 1) + "_";
                t.ok = true;
            }
        }
        return t;
    };
    fn ("listPresets", [&eng, presetTarget] (const Args& a, Completion done)
    {
        const auto t = presetTarget (a);
        if (! t.ok) { done (json ("null")); return; }
        std::string o = "{\"folder\":" + bridge::quote (t.folder.toStdString()) + ",\"kind\":" + bridge::quote (t.kind.toStdString()) + ",\"banks\":[";
        const auto banks = UserPresets::banks (t.folder);
        for (int i = 0; i < banks.size(); ++i) o += (i ? "," : "") + bridge::quote (banks[i].toStdString());
        o += "],\"user\":[";
        bool first = true;
        for (const auto& e : UserPresets::list (t.folder))
        {
            o += std::string (first ? "" : ",") + "[" + bridge::quote (e.bank.toStdString()) + "," + bridge::quote (e.name.toStdString()) + "," + bridge::quote (e.kind.toStdString()) + "]";
            first = false;
        }
        o += "],\"root\":" + bridge::quote (UserPresets::root().getFullPathName().toStdString()) + "}";
        juce::ignoreUnused (eng);
        done (json (o));
    });
    fn ("savePreset", [&eng, presetTarget] (const Args& a, Completion done)
    {
        const auto t = presetTarget (a);
        if (! t.ok) { done (json ("\"invalid\"")); return; }
        const auto bank = UserPresets::cleanName (strArg (a, 4, 120)), name = UserPresets::cleanName (strArg (a, 5, 120));
        if (bank.isEmpty() || name.isEmpty()) { done (json ("\"invalid\"")); return; }
        const auto r = UserPresets::save (t.folder, bank, name, t.kind, eng.moduleValues (t.channel, t.slot, t.prefix), boolArg (a, 6));
        if (r == UserPresets::SaveResult::ok && t.prefix.empty()) eng.applyValues (t.channel, t.slot, eng.moduleValues (t.channel, t.slot), {}, name.toStdString());
        done (json (r == UserPresets::SaveResult::ok ? "\"ok\"" : r == UserPresets::SaveResult::exists ? "\"exists\"" : r == UserPresets::SaveResult::invalidName ? "\"invalid\"" : "\"error\""));
    });
    fn ("loadPreset", [&eng, presetTarget] (const Args& a, Completion done)
    {
        const auto t = presetTarget (a);
        bool ok = false;
        if (t.ok)
        {
            UserPresets::Values values;
            juce::String kind;
            const auto name = UserPresets::cleanName (strArg (a, 5, 120));
            int fileVersion = 1;
            if (UserPresets::load (t.folder, strArg (a, 4, 120), name, values, kind, &fileVersion))
            {
                // préréglages d'avant la version 3 : divisions au tempo décalées de deux choix (16 et 8 mesures ajoutées)
                const auto* md = t.slot >= 0 ? eng.effectDesc (t.channel, t.slot) : eng.instrumentDesc (t.channel);
                if (fileVersion < 3 && md != nullptr)
                    for (auto& [id, val] : values)
                        for (const auto& pd : md->params)
                            if (pd.id == t.prefix + id && hasLongDivisions (pd)) { val = (float) widenDivisionIndex (pd, (int) std::lround (val)); break; }
                ok = eng.applyValues (t.channel, t.slot, values, t.prefix, t.prefix.empty() ? name.toStdString() : std::string());
            }
        }
        done (json (ok ? bridge::channelJson (eng, t.channel) : "null"));
    });
    fn ("deletePreset", [presetTarget] (const Args& a, Completion done)
    {
        const auto t = presetTarget (a);
        done (juce::var (t.ok && UserPresets::remove (t.folder, strArg (a, 4, 120), strArg (a, 5, 120))));
    });
   #if ! JUCE_ANDROID && ! JUCE_IOS
    // Ouvrir le dossier des préréglages dans l'explorateur (ordinateur seulement)
    fn ("revealPresets", [] (const Args&, Completion done)
    {
        const auto dir = UserPresets::root();
        if (dir.createDirectory()) dir.startAsProcess();
        done (juce::var());
    });
   #endif

    fn ("applyChain", [&eng, C] (const Args& a, Completion done)
    {
        const auto ch = intArg (a, 0, 0, C, -1);
        if (ch >= 0) eng.applyChain (ch, strArg (a, 1, 80).toStdString(), nowMs());
        done (json (bridge::channelJson (eng, std::max (0, ch))));
    });
    fn ("setTrackName", [&eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        if (t >= 0) eng.setTrackName (t, strArg (a, 1, 40).removeCharacters ("\r\n\t").toStdString());
        done (json (bridge::tracksJson (eng)));
    });
    // v0.33 : phrase tapée de la voix interne (vocodeurs) ; vide = phrase de démonstration
    fn ("setTrackSpeech", [&eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        if (t >= 0) eng.setTrackSpeech (t, strArg (a, 1, 164).removeCharacters ("\r\n\t").toStdString(), nowMs());
        done (json (bridge::tracksJson (eng)));
    });
    fn ("setTrackColor", [&eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        if (t >= 0) eng.setTrackColor (t, intArg (a, 1, 0, 15, 0));
        done (json (bridge::tracksJson (eng)));
    });
    // v0.31 : coupure / solo des retours et des groupes, nom d'un groupe
    fn ("busMute", [&eng, C] (const Args& a, Completion done)
    {
        const auto ch = intArg (a, 0, 0, C, -1);
        eng.setBusMute (ch, boolArg (a, 1));
        if (ch >= numTracks) eng.recordSwitchUi (ch, false, eng.getBusMute (ch)); // v0.46 fix7 : enregistrée dans le morceau
        done (json (bridge::tracksJson (eng)));
    });
    fn ("busSolo", [&eng, C] (const Args& a, Completion done)
    {
        const auto ch = intArg (a, 0, 0, C, -1);
        eng.setBusSolo (ch, boolArg (a, 1));
        if (ch >= numTracks) eng.recordSwitchUi (ch, true, eng.getBusSolo (ch));
        done (json (bridge::tracksJson (eng)));
    });
    fn ("setGroupName", [&eng] (const Args& a, Completion done)
    {
        const auto k = intArg (a, 0, 0, numGroups - 1, -1);
        if (k >= 0) eng.setGroupName (k, strArg (a, 1, 40).removeCharacters ("\r\n\t").toStdString());
        done (json (bridge::tracksJson (eng)));
    });
    // v0.24 : sortie d'une piste (0 = bus principal, 1 à 4 = bus de groupe) ; enregistrée dans l'historique
    fn ("setTrackOutput", [&eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        if (t >= 0) eng.setTrackOutput (t, intArg (a, 1, 0, numGroups, 0));
        done (json (bridge::tracksJson (eng)));
    });

    // --- Modulations ---------------------------------------------------------------------------
    fn ("addMod", [&eng, C] (const Args& a, Completion done)
    {
        const auto ch = intArg (a, 0, 0, C, -1);
        const auto target = intArg (a, 2, 0, modSourceBase - 1, -1);
        if (ch >= 0 && target >= 0 && eng.describe (globalSlot (ch, target)) != nullptr)
            eng.addModulation (ch, intArg (a, 1, 0, numModSources, 0), target, floatArg (a, 3, -1.0f, 1.0f, 0.3f), nowMs());
        done (json (bridge::channelJson (eng, std::max (0, ch))));
    });
    fn ("removeMod", [&eng, C] (const Args& a, Completion done)
    {
        const auto ch = intArg (a, 0, 0, C, -1);
        if (ch >= 0) eng.removeModulation (ch, intArg (a, 1, 0, modSourceBase - 1, -1), nowMs());
        done (json (bridge::channelJson (eng, std::max (0, ch))));
    });

    // --- Notes jouées à la main -------------------------------------------------------------
    fn ("noteOn", [&eng, &seq, T] (const Args& a, Completion done)
    {
        InputEvent e { InputEvent::noteOn, intArg (a, 0, 0, T, 0), intArg (a, 1, 0, 127, 60), 0, unitArg (a, 2, 0.8f), seq.isRecording() };
        eng.pushEvent (e);
        done (juce::var());
    });
    fn ("noteOff", [&eng, T] (const Args& a, Completion done)
    {
        InputEvent e { InputEvent::noteOff, intArg (a, 0, 0, T, 0), intArg (a, 1, 0, 127, 60), 0, 0.0f, false };
        eng.pushEvent (e);
        done (juce::var());
    });
    // Expression jouée à l'écran : 0 = pitch bend (-1..1), 1 = molette (0..1), 2 = pression (0..1), 3 = pédale
    fn ("expression", [&eng, T] (const Args& a, Completion done)
    {
        static constexpr InputEvent::Type kinds[] { InputEvent::pitchBend, InputEvent::modWheel, InputEvent::pressure, InputEvent::sustain };
        const auto k = intArg (a, 1, 0, 3, 0);
        const auto v = a.size() > 2 && (a[2].isDouble() || a[2].isInt()) ? (float) (double) a[2] : 0.0f;
        InputEvent e { kinds[k], intArg (a, 0, 0, T, 0), 0, 0, std::isfinite (v) ? juce::jlimit (-1.0f, 1.0f, v) : 0.0f, false };
        eng.pushEvent (e);
        done (juce::var());
    });

    // Enregistreur de la piste (sampler de performance) : 0 = départ / arrêt (ou cycle de la boucle), 1 = arrêter, 2 = effacer
    fn ("recordAction", [&eng, T] (const Args& a, Completion done)
    {
        InputEvent e { InputEvent::recordAction, intArg (a, 0, 0, T, 0), intArg (a, 1, 0, 2, 0), 0, 0.0f, false };
        eng.pushEvent (e);
        done (juce::var());
    });
    fn ("keepRecording", [&proc, &eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        const auto err = t >= 0 ? proc.keepRecording (t, intArg (a, 1, 0, 7, 0), intArg (a, 2, 0, maxSamples - 1, 0)) : juce::String ("Piste inconnue");
        done (err.isEmpty() ? json (bridge::channelJson (eng, t)) : json (bridge::quote (err.toStdString())));
    });

    // v0.40 : éditeur audio d'un emplacement (piste, emplacement, opération, sélection) et copie d'emplacement
    fn ("editSample", [&proc, &eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        const auto err = t >= 0 ? proc.editSample (t, intArg (a, 1, 0, maxSamples - 1, 0), intArg (a, 2, 0, 5, -1), unitArg (a, 3, 0.0f), unitArg (a, 4, 1.0f)) : juce::String ("Piste inconnue");
        done (err.isEmpty() ? json (bridge::channelJson (eng, t)) : json (bridge::quote (err.toStdString())));
    });
    // v0.41 : partie d'un échantillon (de, emplacement, a, b) copiée vers (piste, emplacement)
    fn ("copySampleRegion", [&proc, &eng, T] (const Args& a, Completion done)
    {
        const auto ft = intArg (a, 0, 0, T, -1), tt = intArg (a, 4, 0, T, -1);
        const auto err = ft >= 0 && tt >= 0 ? proc.copySampleRegion (ft, intArg (a, 1, 0, maxSamples - 1, 0), unitArg (a, 2, 0.0f), unitArg (a, 3, 1.0f), tt, intArg (a, 5, 0, maxSamples - 1, 0))
                                            : juce::String ("Piste inconnue");
        done (err.isEmpty() ? json (bridge::channelJson (eng, tt)) : json (bridge::quote (err.toStdString())));
    });
    fn ("copySample", [&proc, &eng, T] (const Args& a, Completion done)
    {
        const auto ft = intArg (a, 0, 0, T, -1), tt = intArg (a, 2, 0, T, -1);
        const auto err = ft >= 0 && tt >= 0 ? proc.copySample (ft, intArg (a, 1, 0, maxSamples - 1, 0), tt, intArg (a, 3, 0, maxSamples - 1, 0)) : juce::String ("Piste inconnue");
        done (err.isEmpty() ? json (bridge::channelJson (eng, tt)) : json (bridge::quote (err.toStdString())));
    });

    // v0.32 : enregistrement audio. Réponse : l'état (JSON) ; « error » dit pourquoi la prise n'a pas démarré
    auto recState = [&proc] (const juce::String& err)
    {
        auto st = proc.audioRecStatus();
        if (err.isNotEmpty()) st = st.dropLastCharacters (1) + ",\"startError\":" + juce::JSON::toString (juce::var (err)) + "}";
        return json (st.toStdString());
    };
    fn ("audioRecStart", [&proc, recState] (const Args& a, Completion done) { done (recState (proc.audioRecStart (intArg (a, 0, 0, 2, 0), intArg (a, 1, 0, 1, 0), intArg (a, 2, 0, 2, 0)))); });
    fn ("audioRecStop", [&proc, recState] (const Args&, Completion done) { proc.audioRecStop(); done (recState ({})); });
    fn ("audioRecStatus", [recState] (const Args&, Completion done) { done (recState ({})); });
    fn ("audioRecReveal", [&proc] (const Args&, Completion done) { done (juce::var (proc.audioRecReveal())); });

    // Annuler / rétablir et photo des réglages. Réponse : [fait, peut annuler, peut rétablir, photo prise]
    auto histJson = [&proc] (bool ok)
    {
        auto b = [] (bool v) { return std::string (v ? "true" : "false"); };
        return json ("[" + b (ok) + "," + b (proc.canUndo()) + "," + b (proc.canRedo()) + "," + b (proc.hasPhoto()) + "]");
    };
    fn ("undo", [&proc, histJson] (const Args&, Completion done) { const auto ok = proc.undo(); done (histJson (ok)); });
    fn ("redo", [&proc, histJson] (const Args&, Completion done) { const auto ok = proc.redo(); done (histJson (ok)); });
    fn ("takePhoto", [&proc, histJson] (const Args&, Completion done) { proc.takePhoto(); done (histJson (true)); });
    fn ("recallPhoto", [&proc, histJson] (const Args&, Completion done) { const auto ok = proc.recallPhoto(); done (histJson (ok)); });

    // Plein écran (application autonome) : la fenêtre prend tout l'écran, ou revient en fenêtre.
    // Réponse : "on", "off", ou "host" (dans un logiciel hôte, la fenêtre appartient à l'hôte).
    fn ("fullscreen", [this] (const Args& a, Completion done)
    {
       #if JUCE_ANDROID || JUCE_IOS
        juce::ignoreUnused (a);
        done (juce::var ("on")); // l'application occupe déjà tout l'écran
       #else
        if (processor.wrapperType != juce::AudioProcessor::wrapperType_Standalone) { done (juce::var ("host")); return; }
        auto* top = getTopLevelComponent();
        auto& desk = juce::Desktop::getInstance();
        const bool isOn = desk.getKioskModeComponent() == top;
        const bool want = a.size() > 0 && a[0].isBool() ? (bool) a[0] : ! isOn;
        if (want != isOn) desk.setKioskModeComponent (want ? top : nullptr, false);
        done (juce::var (desk.getKioskModeComponent() == top ? "on" : "off"));
       #endif
    });

    // v0.39 : entrée audio coupée par l'application autonome (sécurité anti-larsen) : lire, ouvrir, couper.
    // Réponse : 1 coupée, 0 ouverte, -1 pas d'application autonome.
    // v0.46 fix21 : l'interface signale qu'on s'en sert (au plus une fois toutes les 20 s) : sur
    // téléphone, l'écran reste allumé tant qu'on joue ou qu'on a touché l'appli il y a moins de 10 min
    fn ("userActive", [this] (const Args&, Completion done) { processor.noteUserActivity(); done (juce::var (true)); });

    // v0.46 fix22 : forme dessinée du pavé vectoriel (« x,y;x,y;… », 0..1)
    fn ("setVecPath", [this] (const Args& a, Completion done) { processor.setVecPath (strArg (a, 0, 4096)); done (juce::var (processor.getVecPath())); });
    fn ("getVecPath", [this] (const Args&, Completion done) { done (juce::var (processor.getVecPath())); });

    // v0.46 fix22 : contrôleur MIDI Bluetooth (téléphone, tablette) : arg 0 = savoir si c'est possible,
    // 1 = ouvrir la fenêtre d'appairage (demande d'abord l'autorisation Bluetooth). Réponse : 1 / 0.
    fn ("bluetoothMidi", [] (const Args& a, Completion done)
    {
        const bool open = a.size() > 0 && (int) a[0] == 1;
       #if JUCE_ANDROID || JUCE_IOS
        if (! juce::BluetoothMidiDevicePairingDialogue::isAvailable()) { done (juce::var (0)); return; }
        if (open)
            juce::RuntimePermissions::request (juce::RuntimePermissions::bluetoothMidi, [] (bool granted)
            {
                if (granted) juce::BluetoothMidiDevicePairingDialogue::open();
            });
        done (juce::var (1));
       #else
        juce::ignoreUnused (open);
        done (juce::var (0));
       #endif
    });

    fn ("inputMute", [] (const Args& a, Completion done)
    {
        const auto& hook = grv::standaloneInputHook();
        const int op = a.size() > 0 && (a[0].isInt() || a[0].isDouble()) ? juce::jlimit (-1, 1, (int) a[0]) : -1;
        done (juce::var (hook ? hook (op) : -1));
    });

    // --- Séquenceur (chaque fonction renvoie la séquence de la piste) -------------------------
    auto patternFn = [&] (const char* name, std::function<void (const Args&, int)> f)
    {
        fn (name, [f, &eng, T] (const Args& a, Completion done)
        {
            const auto t = intArg (a, 0, 0, T, -1);
            if (t >= 0) f (a, t);
            done (json (bridge::patternJson (eng, std::max (0, t))));
        });
    };
    patternFn ("seqSetStep",   [&seq, L, S] (const Args& a, int t) { seq.setStep (t, intArg (a, 1, 0, L, 0), intArg (a, 2, 0, S, 0), intArg (a, 3, 0, 127, 0)); });
    patternFn ("seqSetNote",   [&seq, L, S] (const Args& a, int t) { seq.setNote (t, intArg (a, 1, 0, L, 0), intArg (a, 2, 0, S, 0), intArg (a, 3, 0, 127, 60)); });
    patternFn ("seqSetLength8",[&seq, L, S] (const Args& a, int t) { seq.setLength8 (t, intArg (a, 1, 0, L, 0), intArg (a, 2, 0, S, 0), intArg (a, 3, 1, Sequencer::maxLength8, 8)); });
    patternFn ("seqSetMicro",  [&seq, L, S] (const Args& a, int t) { seq.setMicro (t, intArg (a, 1, 0, L, 0), intArg (a, 2, 0, S, 0), intArg (a, 3, -32, 32, 0)); });
    patternFn ("seqTrackLength", [&seq, S] (const Args& a, int t) { seq.setTrackLength (t, intArg (a, 1, 1, S + 1, 16)); });
    patternFn ("seqTrackSpeed",  [&seq] (const Args& a, int t) { seq.setTrackSpeed (t, intArg (a, 1, 0, numSpeeds - 1, 4)); });
    // v0.46 : coupure / solo enregistrés dans le morceau avec « Enr. les mouvements »
    patternFn ("seqTrackMute",   [&seq, &eng] (const Args& a, int t) { seq.setTrackMute (t, boolArg (a, 1)); eng.recordSwitchUi (t, false, boolArg (a, 1)); });
    patternFn ("seqTrackSolo",   [&seq, &eng] (const Args& a, int t) { seq.setTrackSolo (t, boolArg (a, 1)); eng.recordSwitchUi (t, true, boolArg (a, 1)); });
    patternFn ("seqLaneDir",     [&seq, L] (const Args& a, int t) { seq.setDirection (t, intArg (a, 1, 0, L, 0), (Direction) intArg (a, 2, 0, 3, 0)); });
    patternFn ("seqLaneMute",    [&seq, L] (const Args& a, int t) { seq.setLaneMute (t, intArg (a, 1, 0, L, 0), boolArg (a, 2)); });
    patternFn ("seqLaneSolo",    [&seq, L] (const Args& a, int t) { seq.setLaneSolo (t, intArg (a, 1, 0, L, 0), boolArg (a, 2)); });
    // division du pattern édité de la piste (les autres pistes et patterns gardent la leur)
    patternFn ("seqDivision",    [&seq] (const Args& a, int t) { seq.setPatternDivision (t, seq.getEditing (t), (Division) intArg (a, 1, 0, numDivisions - 1, 2)); });
    patternFn ("seqClearLane",   [&seq, L] (const Args& a, int t) { seq.clearLane (t, intArg (a, 1, 0, L, 0)); });
    patternFn ("seqClearTrack",  [&seq, &eng] (const Args&, int t) { const auto p = seq.getEditing (t); seq.clearPattern (t, p); eng.clearPatternExtras (t, p, nowMs()); });
    patternFn ("seqCopyPage",    [&seq, &eng, S] (const Args& a, int t)
    {
        // copie des pas from..from+count-1 vers to..to+count-1 sur toutes les lignes (Dupliquer ×2,
        // blocs de 4 mesures). v0.31 : avec les fonctions du pas (accent total, glissé, verrous
        // seuls, enregistrement), les conditions, répétitions, flams et verrous des réglages.
        const auto from = intArg (a, 1, 0, S, 0), to = intArg (a, 2, 0, S, 0), count = intArg (a, 3, 1, 64, 16);
        const auto n = std::min ({ count, S + 1 - from, S + 1 - to });
        if (n <= 0 || from == to) return;
        for (int l = 0; l < Sequencer::numLanes; ++l)
            for (int k = 0; k < n; ++k)
            {
                seq.setStep (t, l, to + k, seq.getStep (t, l, from + k));
                seq.setNote (t, l, to + k, seq.getNote (t, l, from + k));
                seq.setLength8 (t, l, to + k, seq.getLength8 (t, l, from + k));
                seq.setMicro (t, l, to + k, seq.getMicro (t, l, from + k));
            }
        for (int k = 0; k < n; ++k) { seq.setStepFlags (t, to + k, seq.getStepFlags (t, from + k)); seq.setStepAmounts (t, to + k, seq.getStepAmounts (t, from + k)); }
        eng.copyStepRangeExtras (t, seq.getEditing (t), from, to, n, nowMs());
    });

    // --- Patterns et lancement -------------------------------------------------------------
    const auto P = Sequencer::numPatterns - 1;
    patternFn ("seqEditPattern", [&seq, P] (const Args& a, int t) { seq.setEditing (t, intArg (a, 1, 0, P, 0)); });
    patternFn ("seqCopyPattern", [&seq, &eng, P] (const Args& a, int t)
    {
        const auto from = intArg (a, 1, 0, P, -1), to = intArg (a, 2, 0, P, -1);
        if (from < 0 || to < 0 || from == to) return;
        seq.copyPattern (t, from, to);
        eng.copyPatternExtras (t, from, to, nowMs());
    });
    // copie vers une autre piste : t = piste de destination, puis motif source, motif de destination, piste source
    patternFn ("seqCopyPatternFrom", [&seq, &eng, P] (const Args& a, int t)
    {
        const auto from = intArg (a, 1, 0, P, -1), to = intArg (a, 2, 0, P, -1), st = intArg (a, 3, 0, numTracks - 1, -1);
        if (from < 0 || to < 0 || st < 0 || (st == t && from == to)) return;
        seq.copyPattern (st, from, t, to);
        eng.copyPatternExtras (st, from, t, to, nowMs());
        eng.copyPatternLanes (st, from, t, to, nowMs());
    });
    patternFn ("seqClearPattern", [&seq, &eng, P] (const Args& a, int t)
    {
        const auto p = intArg (a, 1, 0, P, -1);
        if (p < 0) return;
        seq.clearPattern (t, p);
        eng.clearPatternExtras (t, p, nowMs());
        eng.clearPatternLanes (t, p, nowMs());
    });
    // v0.46 fix16 : décaler l'écriture du pattern édité d'un pas (dir -1 / +1), toutes les lignes (-1) ou une seule
    patternFn ("seqShift", [&seq, &eng, L] (const Args& a, int t)
    {
        const auto dir = intArg (a, 1, -1, 1, 0), lane = intArg (a, 2, -1, L, -1);
        if (dir == 0) return;
        const auto p = seq.getEditing (t);
        seq.shiftPattern (t, p, lane, dir);
        eng.shiftPatternExtras (t, p, lane, dir, nowMs());
    });
    patternFn ("seqRecQuant", [&seq] (const Args& a, int) { seq.setRecordQuantize (intArg (a, 1, 0, 100, 100)); });
    // Fonctions de pas (accent, glissé), longueur par ligne, force de l'accent
    patternFn ("seqStepFlags", [&seq, S] (const Args& a, int t) { seq.setStepFlags (t, intArg (a, 1, 0, S, 0), intArg (a, 2, 0, 15, 0)); });
    patternFn ("seqStepsFlag", [&seq, S] (const Args& a, int t)
    {
        // pas : texte JSON [s…] ; bit 1 = accent, 2 = glissé, 4 = enregistrement, 8 = sans déclenchement ; on : poser ou retirer
        auto bit = intArg (a, 2, 1, 8, 1);
        if (bit != 1 && bit != 2 && bit != 4 && bit != 8) bit = 1;
        const bool on = boolArg (a, 3);
        // le tableau doit vivre le temps de la boucle : le résultat de parse() est gardé dans une variable
        // (getArray() sur le résultat temporaire laissait un pointeur vers un objet détruit)
        const auto parsed = juce::JSON::parse (strArg (a, 1, 20000));
        if (auto* arr = parsed.getArray())
            for (int i = 0; i < std::min (arr->size(), Sequencer::maxSteps); ++i)
                if ((*arr)[i].isInt())
                {
                    const auto st = (int) (*arr)[i];
                    if (st < 0 || st > S) continue;
                    const auto f = seq.getStepFlags (t, st);
                    seq.setStepFlags (t, st, on ? (f | bit) : (f & ~bit));
                }
    });
    // v0.46 fix27 : valeur d'un pas : pas, genre (0 = force de l'accent, 1 = durée du glissé), 0 (par défaut) à 100 %
    patternFn ("seqStepAmount", [&seq, S] (const Args& a, int t)
    {
        const auto st = intArg (a, 1, 0, S, 0), which = intArg (a, 2, 0, 1, 0), v = intArg (a, 3, 0, 100, 0);
        const auto cur = seq.getStepAmounts (t, st);
        seq.setStepAmounts (t, st, which == 0 ? ((cur & 0xFF00) | v) : ((cur & 0xFF) | (v << 8)));
    });
    patternFn ("seqLaneLength", [&seq, L, S] (const Args& a, int t) { seq.setLaneLength (t, intArg (a, 1, 0, L, 0), intArg (a, 2, 0, S + 1, 0)); });
    patternFn ("seqAccent", [&seq] (const Args& a, int t) { seq.setAccentAmount (t, intArg (a, 1, 0, 100, 50)); });
    fn ("getLaunch", [this, &eng] (const Args&, Completion done) { done (json (launchJson())); });
    fn ("launchPattern", [this, &seq, T, P] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1), p = intArg (a, 1, 0, P + 1, -1); // P + 1 = arrêter la piste
        if (t >= 0 && p >= 0) seq.launch (t, p);
        done (json (launchJson()));
    });
    fn ("launchRow", [this, &proc, P] (const Args& a, Completion done)
    {
        const auto r = intArg (a, 0, 0, P, -1);
        if (r >= 0) proc.launchRow (r);
        done (json (launchJson()));
    });
    // Pistes (v0.23) : déplacer (de, vers) ou insérer une piste libre avant une autre.
    // Réponse : liste des pistes, ou un message (texte) si ce n'est pas possible.
    fn ("moveTrack", [&proc, &eng, T] (const Args& a, Completion done)
    {
        const auto from = intArg (a, 0, 0, T, -1), to = intArg (a, 1, 0, T, -1);
        const auto err = from >= 0 && to >= 0 ? proc.moveTrack (from, to) : juce::String ("Piste inconnue");
        done (err.isEmpty() ? json (bridge::tracksJson (eng)) : json (bridge::quote (err.toStdString())));
    });
    fn ("insertTrack", [&proc, &eng, T] (const Args& a, Completion done)
    {
        const auto at = intArg (a, 0, 0, T, -1);
        const auto err = at >= 0 ? proc.insertTrack (at) : juce::String ("Piste inconnue");
        done (err.isEmpty() ? json (bridge::tracksJson (eng)) : json (bridge::quote (err.toStdString())));
    });
    fn ("previewSample", [&eng, T] (const Args& a, Completion done) // v0.43 : écouter une sélection (piste, emplacement, a, b)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        if (t >= 0) eng.previewSample (t, intArg (a, 1, -1, maxSamples - 1, -1), unitArg (a, 2, 0.0f), unitArg (a, 3, 1.0f));
        done (juce::var());
    });
    fn ("instTouch", [&eng, T] (const Args& a, Completion done) // v0.47 : geste sur un écran d'instrument (piste, genre, a, b) ; valeurs bornées
    {
        const auto t = intArg (a, 0, 0, T, -1);
        if (t >= 0) eng.touchInstrument (t, intArg (a, 1, 0, 7, 0), floatArg (a, 2, -8.0f, 8.0f, 0.0f), floatArg (a, 3, -8.0f, 8.0f, 0.0f));
        done (juce::var());
    });
    fn ("resetLanes", [&proc, &eng, T] (const Args& a, Completion done) // v0.43 : une piste interne (ou les 8 : -1) remise à l'origine
    {
        const auto t = intArg (a, 0, 0, T, -1);
        const auto err = t >= 0 ? proc.resetLanes (t, intArg (a, 1, -1, 7, -2)) : juce::String ("Piste inconnue");
        done (err.isEmpty() ? json (bridge::tracksJson (eng)) : json (bridge::quote (err.toStdString())));
    });
    fn ("copyLane", [&proc, &eng, T] (const Args& a, Completion done) // v0.42 : piste interne de Pieuvre 8 → une autre
    {
        const auto t = intArg (a, 0, 0, T, -1);
        const auto err = t >= 0 ? proc.copyLane (t, intArg (a, 1, 0, 7, -1), intArg (a, 2, 0, 7, -1)) : juce::String ("Piste inconnue");
        done (err.isEmpty() ? json (bridge::tracksJson (eng)) : json (bridge::quote (err.toStdString())));
    });
    fn ("copyTrack", [&proc, &eng, T] (const Args& a, Completion done) // v0.41 : copie de piste à piste
    {
        const auto from = intArg (a, 0, 0, T, -1), to = intArg (a, 1, 0, T, -1);
        const auto err = from >= 0 && to >= 0 ? proc.copyTrack (from, to) : juce::String ("Piste inconnue");
        done (err.isEmpty() ? json (bridge::tracksJson (eng)) : json (bridge::quote (err.toStdString())));
    });
    fn ("setLaunchQuant", [this, &seq] (const Args& a, Completion done)
    {
        seq.setLaunchQuant (intArg (a, 0, 0, numLaunchQuants - 1, 2));
        done (json (launchJson()));
    });
    // Rangée entière (tous les patterns d'une lettre, sur les 16 pistes) : copier vers une autre lettre, effacer
    fn ("copyRow", [this, &seq, &eng, P] (const Args& a, Completion done)
    {
        const auto from = intArg (a, 0, 0, P, -1), to = intArg (a, 1, 0, P, -1);
        if (from >= 0 && to >= 0 && from != to)
            for (int t = 0; t < numTracks; ++t) { seq.copyPattern (t, from, to); eng.copyPatternExtras (t, from, to, nowMs()); eng.copyPatternLanes (t, from, t, to, nowMs()); }
        done (json (launchJson()));
    });
    fn ("clearRow", [this, &seq, &eng, P] (const Args& a, Completion done)
    {
        const auto p = intArg (a, 0, 0, P, -1);
        if (p >= 0)
            for (int t = 0; t < numTracks; ++t) { seq.clearPattern (t, p); eng.clearPatternExtras (t, p, nowMs()); eng.clearPatternLanes (t, p, nowMs()); }
        done (json (launchJson()));
    });
    fn ("setMuteTails", [this, &seq] (const Args& a, Completion done)
    {
        seq.setMuteTails (boolArg (a, 0));
        done (json (launchJson()));
    });
    // --- Morceau (arrangeur) -----------------------------------------------------------------
    // Rangées : [mesures, scène, [16 patterns : -2 garder, -1 arrêt, 0..7]] ; courbes : [réglage, lue,
    // nombre de points, dernier temps, 96 valeurs échantillonnées sur la longueur affichée]
    auto songJson = [&eng]
    {
        const auto& sg = eng.getSong();
        std::string o = std::string ("{\"mode\":") + (sg.songMode ? "true" : "false") + ",\"loop\":" + (sg.loop ? "true" : "false")
                      + ",\"loopFrom\":" + std::to_string (sg.loopFrom) + ",\"loopTo\":" + std::to_string (sg.loopTo) + ",\"loopCount\":" + std::to_string (sg.loopCount)
                      + ",\"read\":" + (sg.autoRead ? "true" : "false") + ",\"recPlay\":" + (eng.getRecordPlay() ? "true" : "false")
                      + ",\"recMoves\":" + (eng.getRecordMoves() ? "true" : "false") + ",\"version\":" + std::to_string (eng.getSongVersion()) + ",\"rows\":[";
        for (size_t r = 0; r < sg.rows.size(); ++r)
        {
            const auto& row = sg.rows[r];
            o += (r ? ",[" : "[") + std::to_string (row.bars) + "," + std::to_string (row.scene) + ",[";
            for (int t = 0; t < numTracks; ++t) o += (t ? "," : "") + std::to_string ((int) row.pat[(size_t) t]);
            o += "]]";
        }
        double span = std::max (16.0, sg.totalBeats());
        for (const auto& l : sg.lanes) if (! l.pts.empty()) span = std::max (span, (double) l.pts.back().beat);
        o += "],\"span\":" + std::to_string ((int) std::ceil (span)) + ",\"lanes\":[";
        for (size_t i = 0; i < sg.lanes.size(); ++i)
        {
            const auto& l = sg.lanes[i];
            o += (i ? ",[" : "[") + std::to_string (l.global) + "," + (l.enabled ? "1" : "0") + "," + std::to_string (l.pts.size()) + ","
               + std::to_string (l.pts.empty() ? 0.0 : (double) l.pts.back().beat) + ",[";
            for (int k = 0; k < 96; ++k)
            {
                char b[16]; std::snprintf (b, sizeof b, "%.3f", (double) l.valueAt (span * (k + 0.5) / 96.0));
                o += (k ? "," : "") + std::string (b);
            }
            o += "]]";
        }
        return o + "]}";
    };
    fn ("getSong", [songJson] (const Args&, Completion done) { done (json (songJson())); });

    // --- Éditeur de courbes (pattern et morceau) ---------------------------------------------------
    // Points en tableau plat [temps, valeur, temps, valeur…] (pattern : temps en pas ; morceau : en noires)
    auto pointsJson = [] (const AutoLane& l)
    {
        std::string o = "[";
        char b[40];
        for (size_t i = 0; i < l.pts.size(); ++i)
        {
            std::snprintf (b, sizeof b, "%s%.4f,%.4f", i ? "," : "", (double) l.pts[i].beat, (double) l.pts[i].norm);
            o += b;
        }
        return o + "]";
    };
    auto parsePoints = [] (const juce::String& text, float maxBeat)
    {
        std::vector<AutoPoint> pts;
        const auto parsed = juce::JSON::parse (text);
        if (auto* arr = parsed.getArray())
            for (int i = 0; i + 1 < arr->size() && (int) pts.size() < maxAutoPoints; i += 2)
            {
                const auto b = (float) (double) (*arr)[i], n = (float) (double) (*arr)[i + 1];
                if (std::isfinite (b) && std::isfinite (n) && b >= 0.0f && b <= maxBeat) pts.push_back ({ b, autoNorm (n) });
            }
        std::stable_sort (pts.begin(), pts.end(), [] (const AutoPoint& x, const AutoPoint& y) { return x.beat < y.beat; });
        return pts;
    };
    auto patternLanesJson = [&eng, &seq, pointsJson] (int t)
    {
        const auto p = seq.getEditing (t);
        const auto& sg = eng.getSong();
        std::string o = "{\"track\":" + std::to_string (t) + ",\"pattern\":" + std::to_string (p) + ",\"len\":" + std::to_string (seq.getPatternLength (t, p)) + ",\"lanes\":[";
        bool first = true;
        for (const auto& pl : sg.patLanes)
        {
            if (pl.track != t || pl.pattern != p) continue;
            o += (first ? "[" : ",[") + std::to_string (pl.lane.global) + "," + (pl.lane.enabled ? "1" : "0") + "," + pointsJson (pl.lane) + "]";
            first = false;
        }
        return o + "]}";
    };
    fn ("getPatternLanes", [patternLanesJson, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        done (t < 0 ? juce::var() : json (patternLanesJson (t)));
    });
    // Remplace les points d'une courbe du pattern édité (tableau vide : courbe effacée) ; on : lue ou non
    fn ("setPatternLane", [&eng, &seq, patternLanesJson, parsePoints, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1), g = intArg (a, 1, 0, numSlots - 1, -1);
        if (t < 0 || g < 0 || eng.describe (g) == nullptr) { done (juce::var()); return; }
        const auto p = seq.getEditing (t);
        auto pts = parsePoints (strArg (a, 2, maxAutoPoints * 40), (float) Sequencer::maxSteps);
        auto sg = eng.getSong();
        const auto i = sg.patLaneIndex (t, p, g);
        if (pts.empty()) { if (i >= 0) sg.patLanes.erase (sg.patLanes.begin() + i); }
        else if (i >= 0) { sg.patLanes[(size_t) i].lane.pts = std::move (pts); sg.patLanes[(size_t) i].lane.enabled = boolArg (a, 3); }
        else if ((int) sg.patLanes.size() < maxPatLanes)
        {
            PatternLane pl { t, p, {} };
            pl.lane.global = g; pl.lane.enabled = boolArg (a, 3); pl.lane.pts = std::move (pts);
            sg.patLanes.push_back (std::move (pl));
        }
        eng.setSong (std::move (sg), nowMs());
        done (json (patternLanesJson (t)));
    });
    fn ("getSongLane", [&eng, pointsJson] (const Args& a, Completion done)
    {
        const auto g = intArg (a, 0, 0, numLaneAddresses - 1, -1); // v0.46 : courbes de coupure / solo comprises
        const auto* l = g >= 0 ? eng.getSong().laneFor (g) : nullptr;
        done (json (l != nullptr ? pointsJson (*l) : std::string ("[]")));
    });
    fn ("setSongLane", [&eng, songJson, parsePoints] (const Args& a, Completion done)
    {
        const auto g = intArg (a, 0, 0, numLaneAddresses - 1, -1);
        if (g >= 0 && (isSpecialLane (g) || eng.describe (g) != nullptr))
        {
            auto pts = parsePoints (strArg (a, 1, maxAutoPoints * 40), 1.0e6f);
            auto sg = eng.getSong();
            AutoLane* lane = nullptr;
            for (auto& l : sg.lanes) if (l.global == g) lane = &l;
            if (pts.empty()) sg.lanes.erase (std::remove_if (sg.lanes.begin(), sg.lanes.end(), [g] (const AutoLane& l) { return l.global == g; }), sg.lanes.end());
            else if (lane != nullptr) lane->pts = std::move (pts);
            else if ((int) sg.lanes.size() < maxAutoLanes) { AutoLane l; l.global = g; l.pts = std::move (pts); sg.lanes.push_back (std::move (l)); }
            eng.setSong (std::move (sg), nowMs());
        }
        done (json (songJson()));
    });
    fn ("setSongFlags", [&eng, songJson] (const Args& a, Completion done)
    {
        auto sg = eng.getSong();
        sg.songMode = boolArg (a, 0); sg.loop = boolArg (a, 1); sg.autoRead = boolArg (a, 2);
        eng.setSong (std::move (sg), nowMs());
        done (json (songJson()));
    });
    // Rangées bouclées : première et dernière (comprises) ; dernière -1 : jusqu'à la fin du morceau
    fn ("setSongLoopRange", [&eng, songJson] (const Args& a, Completion done)
    {
        auto sg = eng.getSong();
        sg.loopFrom = intArg (a, 0, 0, maxSongRows - 1, 0);
        sg.loopTo = intArg (a, 1, -1, maxSongRows - 1, -1);
        eng.setSong (std::move (sg), nowMs());
        done (json (songJson()));
    });
    // Nombre de passages de la boucle (0 = sans fin), puis la suite du morceau et l'arrêt (v0.24)
    fn ("setSongLoopCount", [&eng, songJson] (const Args& a, Completion done)
    {
        auto sg = eng.getSong();
        sg.loopCount = intArg (a, 0, 0, 999, 0);
        eng.setSong (std::move (sg), nowMs());
        done (json (songJson()));
    });
    fn ("setSongRecord", [&eng, songJson] (const Args& a, Completion done)
    {
        eng.setRecordPlay (boolArg (a, 0));
        eng.setRecordMoves (boolArg (a, 1));
        done (json (songJson()));
    });
    // Rangées remplacées en bloc (l'interface les édite) : texte JSON [[mesures, scène, [16]]…], 128 au plus
    fn ("setSongRows", [&eng, songJson] (const Args& a, Completion done)
    {
        const auto parsed = juce::JSON::parse (strArg (a, 0, 64000));
        if (auto* arr = parsed.getArray())
        {
            auto sg = eng.getSong();
            const auto before = sg; // v0.46 fix7 : les courbes suivent leurs rangées
            sg.rows.clear();
            for (const auto& item : *arr)
            {
                if ((int) sg.rows.size() >= maxSongRows) break;
                auto* r = item.getArray();
                if (r == nullptr || r->size() < 3) continue;
                SongRow row;
                row.bars = juce::jlimit (1, maxRowBars, (int) (*r)[0]);
                row.scene = juce::jlimit (-1, numScenes - 1, (int) (*r)[1]);
                if (auto* pats = (*r)[2].getArray())
                    for (int t = 0; t < numTracks && t < pats->size(); ++t)
                        row.pat[(size_t) t] = (std::int8_t) juce::jlimit (songKeep, Sequencer::numPatterns - 1, (int) (*pats)[t]);
                sg.rows.push_back (row);
            }
            rippleSongLanes (before, sg);
            eng.setSong (std::move (sg), nowMs());
        }
        done (json (songJson()));
    });
    // Nouvelle rangée avec ce qui joue (après la rangée donnée, -1 : à la fin)
    fn ("songRowFromPlaying", [&eng, &seq, songJson] (const Args& a, Completion done)
    {
        auto sg = eng.getSong();
        if ((int) sg.rows.size() < maxSongRows)
        {
            SongRow row;
            for (int t = 0; t < numTracks; ++t)
            {
                const auto p = seq.getPlaying (t);
                row.pat[(size_t) t] = (std::int8_t) (p >= 0 && p < Sequencer::numPatterns ? p : songStop);
            }
            const auto at = intArg (a, 0, -1, (int) sg.rows.size() - 1, -1);
            const auto before = sg; // v0.46 fix7 : les courbes des rangées suivantes glissent
            sg.rows.insert (at < 0 ? sg.rows.end() : sg.rows.begin() + at + 1, row);
            rippleSongLanes (before, sg);
            eng.setSong (std::move (sg), nowMs());
        }
        done (json (songJson()));
    });
    // Courbe : 0 = lue / pas lue, 1 = effacer ; réglage absent : toutes les courbes effacées (action 2)
    fn ("songLane", [&eng, songJson] (const Args& a, Completion done)
    {
        const auto g = intArg (a, 0, -1, numLaneAddresses - 1, -1);
        const auto action = intArg (a, 1, 0, 2, 0);
        auto sg = eng.getSong();
        if (action == 2) sg.lanes.clear();
        else
            for (size_t i = 0; i < sg.lanes.size(); ++i)
                if (sg.lanes[i].global == g)
                {
                    if (action == 0) sg.lanes[i].enabled = ! sg.lanes[i].enabled;
                    else sg.lanes.erase (sg.lanes.begin() + (long) i);
                    break;
                }
        eng.setSong (std::move (sg), nowMs());
        done (json (songJson()));
    });

    fn ("setRowScene", [this, &proc, P] (const Args& a, Completion done)
    {
        const auto r = intArg (a, 0, 0, P, -1);
        if (r >= 0) proc.setRowScene (r, intArg (a, 1, -1, numScenes - 1, -1));
        done (json (launchJson()));
    });

    // --- Apprentissage MIDI ------------------------------------------------------------------
    auto midiTargetArg = [] (const Args& a, int i)
    {
        // réglages (0 et plus), commandes spéciales (-2 à -5), coupure (-100 à -115) et solo (-200 à -215) de piste
        const auto t = intArg (a, i, GrooveboxProcessor::midiLowestTarget, numSlots - 1, GrooveboxProcessor::midiNone);
        return t >= GrooveboxProcessor::midiLastSpecial || GrooveboxProcessor::isMidiTrackTarget (t) || GrooveboxProcessor::isMidiExtra (t) ? t : (int) GrooveboxProcessor::midiNone;
    };
    // v0.46 fix24 : boutons assignables (valeur, cible) et table des valeurs d'écran
    fn ("setKnob", [&proc] (const Args& a, Completion done)
    {
        if (const auto k = intArg (a, 0, 0, GrooveboxProcessor::numKnobs - 1, -1); k >= 0) proc.setKnob (k, floatArg (a, 1, 0.0f, 1.0f, 0.0f));
        done (juce::var());
    });
    fn ("setKnobTarget", [this, &proc, &eng, midiTargetArg] (const Args& a, Completion done)
    {
        const auto k = intArg (a, 0, 0, GrooveboxProcessor::numKnobs - 1, -1);
        auto t = midiTargetArg (a, 1);
        if (GrooveboxProcessor::isKnobTarget (t) || (t >= 0 && eng.describe (t) == nullptr)) t = GrooveboxProcessor::midiNone;
        if (k >= 0)
        {
            proc.setKnobTarget (k, t);
            if (a.size() > 2) proc.setKnob (k, floatArg (a, 2, 0.0f, 1.0f, 0.0f)); // position de départ : la valeur de la cible
        }
        done (json (midiJson()));
    });
    // v0.46 fix28 : LFO d'un bouton assignable (0 = aucun, 1..4) et profondeur -1..1
    fn ("setKnobLfo", [this, &proc] (const Args& a, Completion done)
    {
        const auto k = intArg (a, 0, 0, GrooveboxProcessor::numKnobs - 1, -1);
        if (k >= 0) proc.setKnobLfo (k, intArg (a, 1, 0, GrooveboxProcessor::numKnobLfos, 0), floatArg (a, 2, -1.0f, 1.0f, 0.0f));
        done (json (midiJson()));
    });
    fn ("setCtlTable", [this, &proc] (const Args& a, Completion done)
    {
        proc.setCtlTable (a.size() > 0 ? a[0].toString() : juce::String());
        done (json (midiJson()));
    });
    fn ("getMidi", [this] (const Args&, Completion done) { done (json (midiJson())); });
    fn ("resetLoudness", [this] (const Args&, Completion done) { loudness.reset(); done (juce::var()); });
    fn ("midiLearn", [this, &proc, &eng, midiTargetArg] (const Args& a, Completion done)
    {
        const auto t = midiTargetArg (a, 0);
        if (t >= 0 && eng.describe (t) == nullptr) { done (json (midiJson())); return; }
        proc.startMidiLearn (t);
        done (json (midiJson()));
    });
    fn ("setMidiRoute", [this, &proc] (const Args& a, Completion done)
    {
        // v0.46 : canal des notes (0 = tous), canal des commandes (0 = tous), commandes actives
        proc.setMidiRoute (intArg (a, 0, 0, 16, 0), intArg (a, 1, 0, 16, 0), boolArg (a, 2));
        done (json (midiJson()));
    });
    fn ("setTrackMidiChannel", [this, &proc, T] (const Args& a, Completion done)
    {
        if (const auto t = intArg (a, 0, 0, T, -1); t >= 0) proc.setTrackMidiChannel (t, intArg (a, 1, 0, 16, 0));
        done (json (midiJson()));
    });
    fn ("midiResolve", [this, &proc] (const Args& a, Completion done)
    {
        proc.resolveMidiConflict (boolArg (a, 0));
        done (json (midiJson()));
    });
    fn ("midiForget", [this, &proc, &eng, midiTargetArg] (const Args& a, Completion done)
    {
        const auto t = midiTargetArg (a, 0);
        if (t != GrooveboxProcessor::midiNone && (t < 0 || eng.describe (t) != nullptr)) proc.clearMidiTarget (t);
        done (json (midiJson()));
    });

    // --- Fichiers : uniquement par le sélecteur du système, ouvert à la demande de l'utilisateur ----
    fn ("chooseSample", [this, &proc, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1), i = intArg (a, 1, 0, maxSamples - 1, -1);
        done (juce::var());
        if (t < 0 || i < 0) return;
        chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Choisir un fichier audio"), Folders::get (Folders::samples), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, &proc, t, i] (const juce::FileChooser& fc)
        {
            const auto url = fc.getURLResult();
            if (url.isEmpty()) return;
            auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress));
            if (in == nullptr) { finishFile ("sample", "Fichier illisible"); return; }
            if (in->getTotalLength() > (juce::int64) 64 * 1024 * 1024) { finishFile ("sample", "Fichier trop lourd (64 Mo au plus)"); return; }
            juce::MemoryBlock bytes;
            in->readIntoMemoryBlock (bytes, 64 * 1024 * 1024 + 1);
            if (bytes.getSize() > (size_t) 64 * 1024 * 1024) { finishFile ("sample", "Fichier trop lourd (64 Mo au plus)"); return; }
            const auto name = juce::URL::removeEscapeChars (url.isLocalFile() ? url.getLocalFile().getFileName() : url.getFileName());
            juce::File copy;
            if (! Folders::canChoose())
            {
                // tablette : le fichier est rangé dans la bibliothèque d'échantillons de l'application,
                // pour être retrouvé ensuite sans repasser par le sélecteur (jamais d'écrasement)
                const auto clean = Folders::cleanFileName (name);
                if (clean.isNotEmpty())
                {
                    auto f = Folders::get (Folders::samples).getChildFile (clean);
                    if (! (f.existsAsFile() && f.getSize() == (juce::int64) bytes.getSize()))
                    {
                        if (f.exists()) f = f.getNonexistentSibling();
                        if (f.replaceWithData (bytes.getData(), bytes.getSize())) copy = f;
                    }
                }
            }
            const auto error = proc.loadSample (t, i, name, std::move (bytes));
            if (error.isNotEmpty() && copy != juce::File()) copy.deleteFile();   // fichier refusé : pas gardé
            finishFile ("sample", error);
        });
    });

    // v0.47.0 : importer des échantillons venus d'ailleurs (téléphone, Drive…) UNE fois : copiés dans le
    // dossier des échantillons de l'application (sous-dossier « Importés »), puis chargés depuis la
    // Bibliothèque sans repasser par le sélecteur du système. Jamais d'écrasement ; 64 Mo par fichier.
    fn ("importSamples", [this] (const Args&, Completion done)
    {
        done (juce::var());
        chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Importer des échantillons"), Folders::get (Folders::samples), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
        {
            const auto urls = fc.getURLResults();
            if (urls.isEmpty()) return;
            auto dir = Folders::get (Folders::samples).getChildFile (juce::String::fromUTF8 ("Import\xC3\xA9s"));
            if (! dir.createDirectory()) { finishFile ("import", juce::String::fromUTF8 ("Dossier des échantillons inaccessible")); return; }
            int copied = 0, refused = 0;
            for (const auto& url : urls)
            {
                if (copied + refused >= 200) break;
                const auto name = Folders::cleanFileName (juce::URL::removeEscapeChars (url.isLocalFile() ? url.getLocalFile().getFileName() : url.getFileName()));
                auto f = dir.getChildFile (name);
                if (name.isEmpty() || ! f.hasFileExtension ("wav;aif;aiff;flac;ogg")) { ++refused; continue; }
                auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress));
                if (in == nullptr) { ++refused; continue; }
                juce::MemoryBlock bytes;
                in->readIntoMemoryBlock (bytes, 64 * 1024 * 1024 + 1);
                if (bytes.getSize() == 0 || bytes.getSize() > (size_t) 64 * 1024 * 1024) { ++refused; continue; }
                if (f.exists()) f = f.getNonexistentSibling();
                if (f.replaceWithData (bytes.getData(), bytes.getSize())) ++copied; else ++refused;
            }
            finishFile ("import", copied > 0 ? juce::String() : juce::String::fromUTF8 ("Aucun échantillon importé (formats : WAV, AIFF, FLAC, OGG ; 64 Mo au plus)"));
            juce::ignoreUnused (refused);
        });
    });

    // --- Dossiers de travail (Réglages) : échantillons, projets, préréglages ---------------------
    auto foldersJson = []
    {
        std::string o = "{\"canChoose\":" + std::string (Folders::canChoose() ? "true" : "false");
        for (int k = 0; k < Folders::numKinds; ++k)
        {
            const auto kind = (Folders::Kind) k;
            o += std::string (",\"") + Folders::key (kind) + "\":{\"path\":" + bridge::quote (Folders::get (kind).getFullPathName().toStdString())
               + ",\"custom\":" + (Folders::isCustom (kind) ? "true" : "false") + "}";
        }
        return o + "}";
    };
    auto kindArg = [] (const Args& a, int i, Folders::Kind& k) { return Folders::kindFrom (strArg (a, i, 16), k); };
    fn ("getFolders", [foldersJson] (const Args&, Completion done) { done (json (foldersJson())); });
    fn ("chooseFolder", [this, kindArg] (const Args& a, Completion done)
    {
        Folders::Kind k;
        done (juce::var());
        if (! Folders::canChoose() || ! kindArg (a, 0, k)) return;
        const char* titles[] = { "Dossier des \xC3\xA9" "chantillons", "Dossier des projets", "Dossier des pr\xC3\xA9r\xC3\xA9glages" };
        chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 (titles[k]), Folders::get (k), "");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this, k] (const juce::FileChooser& fc)
        {
            const auto url = fc.getURLResult();
            if (url.isEmpty()) return;
            const bool ok = url.isLocalFile() && Folders::set (k, url.getLocalFile());
            finishFile ("folder", ok ? juce::String() : juce::String::fromUTF8 ("Ce dossier ne peut pas \xC3\xAAtre utilis\xC3\xA9"));
        });
    });
    fn ("resetFolder", [foldersJson, kindArg] (const Args& a, Completion done)
    {
        Folders::Kind k;
        if (kindArg (a, 0, k)) Folders::reset (k);
        done (json (foldersJson()));
    });
   #if ! JUCE_ANDROID && ! JUCE_IOS
    fn ("revealFolder", [kindArg] (const Args& a, Completion done)
    {
        Folders::Kind k;
        if (kindArg (a, 0, k)) { const auto d = Folders::get (k); if (d.isDirectory()) d.startAsProcess(); }
        done (juce::var());
    });
   #endif
    // Contenu d'un sous-dossier : [nom, dossier ?, octets], dossiers d'abord. Chemin relatif seulement.
    fn ("listFolder", [kindArg] (const Args& a, Completion done)
    {
        Folders::Kind k;
        if (! kindArg (a, 0, k) || k == Folders::presets) { done (json ("null")); return; }
        const auto rel = strArg (a, 1, 2048);
        juce::StringArray ext;
        if (k == Folders::samples) ext.addArray ({ "wav", "aif", "aiff", "flac", "ogg" });
        else ext.add ("gbx");
        bool ok = false;
        const auto items = Folders::list (k, rel, ext, ok);
        std::string o = std::string ("{\"ok\":") + (ok ? "true" : "false") + ",\"items\":[";
        for (size_t i = 0; i < items.size(); ++i)
            o += (i ? "," : "") + std::string ("[") + bridge::quote (items[i].name.toStdString()) + "," + (items[i].folder ? "1" : "0") + "," + std::to_string (items[i].bytes) + "]";
        done (json (o + "]}"));
    });
    fn ("loadSampleFile", [this, &proc, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1), i = intArg (a, 1, 0, maxSamples - 1, -1);
        const auto rel = strArg (a, 2, 2048);
        done (juce::var());
        juce::File f;
        if (t < 0 || i < 0 || ! Folders::resolve (Folders::samples, rel, f) || ! f.existsAsFile()) { finishFile ("sample", "Fichier introuvable"); return; }
        if (f.getSize() > (juce::int64) 64 * 1024 * 1024) { finishFile ("sample", "Fichier trop lourd (64 Mo au plus)"); return; }
        juce::MemoryBlock bytes;
        if (! f.loadFileAsData (bytes)) { finishFile ("sample", "Fichier illisible"); return; }
        finishFile ("sample", proc.loadSample (t, i, f.getFileName(), std::move (bytes)));
    });
    // Enregistrer directement dans le dossier des projets, sous un nom (sans passer par le sélecteur)
    fn ("saveProjectNamed", [&proc] (const Args& a, Completion done)
    {
        auto name = Folders::cleanFileName (strArg (a, 0, 120));
        if (name.endsWithIgnoreCase (".gbx")) name = name.dropLastCharacters (4).trim();
        if (name.isEmpty()) { done (json ("\"invalid\"")); return; }
        const auto f = Folders::get (Folders::projects).getChildFile (name + ".gbx");
        if (f.existsAsFile() && ! boolArg (a, 1)) { done (json ("\"exists\"")); return; }
        const auto tmp = f.getSiblingFile (name + ".tmp");
        bool ok = false;
        {
            juce::FileOutputStream fos (tmp);
            if (! boolArg (a, 2)) proc.setProjectFileContext (f); // v0.46 fix8 : prises du montage à côté du projet
            if (fos.openedOk()) { fos.setPosition (0); fos.truncate(); ok = proc.writeProjectFile (fos, boolArg (a, 2)) && fos.getStatus().wasOk(); } // v0.46 : 3e argument = modèle
        }
        ok = ok && tmp.moveFileTo (f);
        if (! ok) tmp.deleteFile();
        done (json (ok ? "\"ok\"" : "\"error\""));
    });
    fn ("openProjectFile", [this, &proc] (const Args& a, Completion done)
    {
        const auto rel = strArg (a, 0, 2048);
        done (juce::var());
        juce::File f;
        if (! Folders::resolve (Folders::projects, rel, f) || ! f.existsAsFile() || ! f.hasFileExtension ("gbx;xml")) { finishFile ("open", "Projet introuvable"); return; }
        juce::FileInputStream in (f);
        if (! in.openedOk()) { finishFile ("open", "Fichier illisible"); return; }
        proc.setProjectFileContext (f); // v0.46 fix8 : prises du montage relues à côté du projet
        finishFile ("open", proc.readProjectFile (in));
    });
    fn ("saveProject", [this, &proc] (const Args& a, Completion done)
    {
        done (juce::var());
        const bool tpl = boolArg (a, 0); // v0.46 : modèle (réglages sans les écritures)
        chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 (tpl ? "Enregistrer comme mod\xC3\xA8le" : "Enregistrer le projet"),
                                                       Folders::get (Folders::projects).getChildFile (juce::String::fromUTF8 (tpl ? "Mod\xC3\xA8le.gbx" : "Projet.gbx")), "*.gbx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, &proc, tpl] (const juce::FileChooser& fc)
        {
            auto url = fc.getURLResult();
            if (url.isEmpty()) return;
            if (url.isLocalFile() && ! url.getLocalFile().hasFileExtension ("gbx"))
            {
                // extension ajoutée après l'avertissement du système : ne jamais écraser un autre fichier
                const auto withExt = url.getLocalFile().withFileExtension ("gbx");
                url = juce::URL (withExt.existsAsFile() ? withExt.getNonexistentSibling() : withExt);
            }
            const auto error = juce::String::fromUTF8 ("Erreur d'\xC3\xA9" "criture");
            if (url.isLocalFile())
            {
                // fichier temporaire, puis remplacement : un échec ne détruit pas l'ancien projet
                const auto f = url.getLocalFile();
                const auto tmp = f.getSiblingFile (f.getFileNameWithoutExtension() + ".tmp");
                bool ok = false;
                {
                    juce::FileOutputStream fos (tmp);
                    if (! tpl) proc.setProjectFileContext (f); // v0.46 fix8
                    if (fos.openedOk()) { fos.setPosition (0); fos.truncate(); ok = proc.writeProjectFile (fos, tpl) && fos.getStatus().wasOk(); }
                }
                ok = ok && tmp.moveFileTo (f);
                if (! ok) tmp.deleteFile();
                finishFile ("save", ok ? juce::String() : error);
                return;
            }
            auto out = url.createOutputStream();
            if (out == nullptr) { finishFile ("save", juce::String::fromUTF8 ("Impossible d'\xC3\xA9" "crire le fichier")); return; }
            if (! tpl) proc.setProjectFileContext ({}); // fichier hors du disque (tablette) : chemins complets
            finishFile ("save", proc.writeProjectFile (*out, tpl) ? juce::String() : error);
        });
    });
    fn ("openProject", [this, &proc] (const Args&, Completion done)
    {
        done (juce::var());
        chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Ouvrir un projet"),
                                                       Folders::get (Folders::projects), "*.gbx;*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, &proc] (const juce::FileChooser& fc)
        {
            const auto url = fc.getURLResult();
            if (url.isEmpty()) return;
            auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress));
            if (in == nullptr) { finishFile ("open", "Fichier illisible"); return; }
            proc.setProjectFileContext (url.isLocalFile() ? url.getLocalFile() : juce::File()); // v0.46 fix8
            finishFile ("open", proc.readProjectFile (*in));
        });
    });

    // --- Conditions et verrous par pas --------------------------------------------------------
    auto extrasFn = [&] (const char* name, std::function<void (const Args&, int, int, int)> f)
    {
        fn (name, [f, &eng, T, L, S] (const Args& a, Completion done)
        {
            const auto t = intArg (a, 0, 0, T, -1), l = intArg (a, 1, 0, L, -1), s = intArg (a, 2, 0, S, -1);
            if (t >= 0 && l >= 0 && s >= 0) f (a, t, l, s);
            done (json (bridge::extrasJson (eng, std::max (0, t))));
        });
    };
    extrasFn ("stepCondition", [&eng] (const Args& a, int t, int l, int s)
    { eng.setStepExtra (t, l, s, (Condition) intArg (a, 3, 0, numConditions - 1, 0), intArg (a, 4, 1, 8, 1), intArg (a, 5, 1, 8, 2), intArg (a, 6, 0, 100, 100), nowMs()); });
    extrasFn ("stepLock", [&eng] (const Args& a, int t, int l, int s)
    { const auto g = intArg (a, 3, 0, numSlots - 1, -1); if (g >= 0 && eng.describe (g) != nullptr) eng.setStepLock (t, l, s, g, unitArg (a, 4, 0.0f), nowMs()); });
    extrasFn ("stepClearLock", [&eng] (const Args& a, int t, int l, int s) { eng.clearStepLock (t, l, s, intArg (a, 3, 0, numSlots - 1, -1), nowMs()); });
    extrasFn ("stepRatchet", [&eng] (const Args& a, int t, int l, int s)
    { eng.setStepRatchet (t, l, s, intArg (a, 3, 1, Sequencer::maxRatchet, 1), intArg (a, 4, -100, 100, 0), nowMs()); });
    extrasFn ("stepClear", [&eng] (const Args&, int t, int l, int s) { eng.clearStepExtras (t, l, s, nowMs()); });

    // --- Édition groupée (sélection du séquenceur) ---------------------------------------
    // Texte JSON : {"clear":[[l,s]…], "set":[[l,s,vél,note,durée8,décalage]…], "ex":[[l,s,cond,a,b,prob,[[g,v]…],répétitions,pente,flam]…]}
    // Tout est borné : 8192 pas au plus, valeurs ramenées dans leurs plages par le moteur.
    auto cellList = [L, S] (const juce::var& arr, std::vector<std::pair<int, int>>& out)
    {
        if (auto* x = arr.getArray())
            for (const auto& c : *x)
            {
                if (out.size() >= 8192) break;
                if (auto* p = c.getArray(); p != nullptr && p->size() >= 2 && (*p)[0].isInt() && (*p)[1].isInt())
                {
                    const auto l = (int) (*p)[0], st = (int) (*p)[1];
                    if (l >= 0 && l <= L && st >= 0 && st <= S) out.emplace_back (l, st);
                }
            }
    };
    fn ("seqEditCells", [&eng, T, L, S, cellList] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        const auto text = strArg (a, 1, 4000000);
        if (t < 0 || text.isEmpty()) { done (json (bridge::patternJson (eng, std::max (0, t)))); return; }
        const auto v = juce::JSON::parse (text);
        std::vector<std::pair<int, int>> clear;
        cellList (v.getProperty ("clear", {}), clear);
        auto num = [] (const juce::var& x, int lo, int hi, int def)
        {
            if (! (x.isInt() || x.isInt64() || x.isDouble())) return def;
            const auto d = (double) x;
            return std::isfinite (d) ? juce::jlimit (lo, hi, (int) std::lround (d)) : def;
        };
        std::vector<Engine::CellWrite> writes;
        if (auto* arr = v.getProperty ("set", {}).getArray())
            for (const auto& c : *arr)
            {
                if (writes.size() >= 8192) break;
                auto* p = c.getArray();
                if (p == nullptr || p->size() < 6) continue;
                Engine::CellWrite w;
                w.lane = num ((*p)[0], -1, L, -1); w.step = num ((*p)[1], -1, S, -1);
                if (w.lane < 0 || w.step < 0) continue;
                w.vel = num ((*p)[2], 0, 127, 0); w.note = num ((*p)[3], 0, 127, 60);
                w.len8 = num ((*p)[4], 1, Sequencer::maxLength8, 8); w.micro = num ((*p)[5], -32, 32, 0);
                writes.push_back (std::move (w));
            }
        std::unordered_map<int, size_t> writeAt; // (ligne, pas) → position dans writes
        for (size_t k = 0; k < writes.size(); ++k) writeAt[writes[k].lane * 4096 + writes[k].step] = k;
        if (auto* arr = v.getProperty ("ex", {}).getArray())
            for (const auto& c : *arr)
            {
                auto* p = c.getArray();
                if (p == nullptr || p->size() < 7) continue;
                const auto l = num ((*p)[0], -1, L, -1), st = num ((*p)[1], -1, S, -1);
                const auto it = writeAt.find (l * 4096 + st);
                if (l < 0 || st < 0 || it == writeAt.end()) continue;
                auto& w = writes[it->second];
                w.hasExtra = true;
                w.extra.cond = (Condition) num ((*p)[2], 0, numConditions - 1, 0);
                w.extra.a = (std::uint8_t) num ((*p)[3], 1, 8, 1);
                w.extra.b = (std::uint8_t) num ((*p)[4], 1, 8, 2);
                w.extra.prob = (std::uint8_t) num ((*p)[5], 0, 100, 100);
                if (p->size() >= 9)
                {
                    w.extra.ratchet = (std::uint8_t) num ((*p)[7], 1, Sequencer::maxRatchet, 1);
                    w.extra.ratchetSlope = (std::int8_t) num ((*p)[8], -100, 100, 0);
                }
                if (p->size() >= 10) w.extra.flam = (std::uint8_t) num ((*p)[9], 0, maxFlamMs, 0);
                if (p->size() >= 11) w.extra.direct = num ((*p)[10], 0, 1, 0) == 1;
                if (auto* locks = (*p)[6].getArray())
                    for (const auto& lk : *locks)
                        if (auto* q = lk.getArray(); q != nullptr && q->size() >= 2 && w.extra.locks.size() < 64)
                        {
                            const auto g = num ((*q)[0], -1, numSlots - 1, -1);
                            const auto val = (*q)[1].isDouble() || (*q)[1].isInt() ? (float) (double) (*q)[1] : -1.0f;
                            if (g >= 0 && std::isfinite (val) && val >= 0.0f && val <= 1.0f) w.extra.locks.push_back ({ g, val });
                        }
            }
        eng.editCells (t, clear, writes, nowMs());
        done (json (bridge::patternJson (eng, t)));
    });
    fn ("stepsLock", [&eng, T, cellList] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        std::vector<std::pair<int, int>> cells;
        cellList (juce::JSON::parse (strArg (a, 1, 200000)), cells);
        const auto g = intArg (a, 2, 0, numSlots - 1, -1);
        if (t >= 0 && g >= 0) eng.setStepLocks (t, cells, g, unitArg (a, 3, 0.0f), boolArg (a, 4), nowMs());
        done (json (bridge::extrasJson (eng, std::max (0, t))));
    });
    fn ("stepsFlam", [&eng, T, cellList] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        std::vector<std::pair<int, int>> cells;
        cellList (juce::JSON::parse (strArg (a, 1, 200000)), cells);
        if (t >= 0) eng.setStepsFlam (t, -1, cells, intArg (a, 2, 0, maxFlamMs, 0), nowMs());
        done (json (bridge::extrasJson (eng, std::max (0, t))));
    });
    fn ("stepsRatchet", [&eng, T, cellList] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        std::vector<std::pair<int, int>> cells;
        cellList (juce::JSON::parse (strArg (a, 1, 200000)), cells);
        if (t >= 0) eng.setStepsRatchet (t, cells, intArg (a, 2, 1, Sequencer::maxRatchet, 1), intArg (a, 3, -100, 100, 0), nowMs());
        done (json (bridge::extrasJson (eng, std::max (0, t))));
    });

    // --- Scènes ------------------------------------------------------------------------------
    auto scenesFn = [&] (const char* name, std::function<void (const Args&, int)> f)
    {
        fn (name, [f, &eng] (const Args& a, Completion done)
        {
            const auto s = intArg (a, 0, 0, numScenes, -1);
            if (s >= 0) f (a, s);
            done (json (bridge::scenesJson (eng)));
        });
    };
    scenesFn ("sceneSetLock", [&eng] (const Args& a, int s)
    { const auto g = intArg (a, 1, 0, numSlots - 1, -1); if (g >= 0 && eng.describe (g) != nullptr) eng.setSceneLock (s, g, unitArg (a, 2, 0.0f), nowMs()); });
    scenesFn ("sceneClearLock", [&eng] (const Args& a, int s) { eng.clearSceneLock (s, intArg (a, 1, 0, numSlots - 1, -1), nowMs()); });
    scenesFn ("sceneClear", [&eng] (const Args&, int s) { eng.clearScene (s, nowMs()); });
    scenesFn ("sceneMute", [&eng] (const Args& a, int s) { eng.setSceneMuted (s, boolArg (a, 1), nowMs()); });
    scenesFn ("sceneCopy", [&eng] (const Args& a, int s)
    {
        const auto to = intArg (a, 1, 0, numScenes, -1);
        if (to < 0 || to == s) return;
        const auto locks = eng.getScenes().scenes[(size_t) s]; // copie
        eng.clearScene (to, nowMs());
        for (const auto& l : locks) eng.setSceneLock (to, l.slot, l.norm, nowMs());
    });
    fn ("sceneLaunch", [&proc] (const Args& a, Completion done)
    {
        proc.launchScene (intArg (a, 0, 0, numScenes - 1, 0), boolArg (a, 1));
        done (juce::var());
    });

    // --- Transport et réglages globaux ---------------------------------------------------------
    fn ("play", [&seq] (const Args&, Completion done) { seq.requestPlay(); done (juce::var()); });
    fn ("stop", [&seq] (const Args&, Completion done) { seq.requestStop(); done (juce::var()); });
    fn ("record", [&seq] (const Args& a, Completion done) { seq.setRecording (boolArg (a, 0)); done (juce::var()); });
    fn ("fill", [&proc] (const Args& a, Completion done) { proc.setFill (boolArg (a, 0)); done (juce::var()); });
    fn ("breakKey", [&proc] (const Args& a, Completion done) { proc.setBreak (boolArg (a, 0)); done (juce::var()); });
    fn ("breakTimed", [&proc] (const Args& a, Completion done)
    {
        const auto beats = a.size() > 0 && (a[0].isDouble() || a[0].isInt() || a[0].isInt64()) ? (double) a[0] : 0.0;
        proc.timedBreak (std::isfinite (beats) ? beats : 0.0);
        done (juce::var());
    });
    fn ("setGlobal", [&proc] (const Args& a, Completion done)
    {
        proc.setGlobal (strArg (a, 0, 16), floatArg (a, 1, -1000.0f, 1000.0f, 0.0f));
        done (juce::var());
    });
    fn ("setPadMode", [&proc] (const Args& a, Completion done)
    {
        proc.setPadMode ((GrooveboxProcessor::PadMode) intArg (a, 0, 0, 4, 0), intArg (a, 1, 0, 1, 0),
                         intArg (a, 2, 0, Sequencer::numLanes - 1, 0), intArg (a, 3, 0, Sequencer::maxSteps / 16 - 1, 0));
        done (juce::var());
    });
    // Macros du logiciel hôte : plusieurs réglages par macro, chacun sur sa plage
    auto macrosJson = [&proc]
    {
        std::string o = "[";
        for (int m = 0; m < GrooveboxProcessor::numMacros; ++m)
        {
            char b[32];
            std::snprintf (b, sizeof b, "%.4f", (double) proc.getMacroValue (m));
            o += (m ? "," : "") + std::string ("{\"v\":") + b + ",\"rel\":" + (proc.isMacroRelative (m) ? "true" : "false") + ",\"links\":[";
            bool first = true;
            for (const auto& l : proc.getMacroLinks (m))
            {
                char x[96];
                std::snprintf (x, sizeof x, "[%d,%.4f,%.4f,%.4f,%.4f]", l.global, (double) l.lo, (double) l.hi, (double) l.base, (double) l.ref);
                o += (first ? "" : ",") + std::string (x);
                first = false;
            }
            o += "]}";
        }
        return o + "]";
    };
    fn ("getMacros", [macrosJson] (const Args&, Completion done) { done (json (macrosJson())); });
    fn ("setMacro", [&proc, macrosJson] (const Args& a, Completion done)
    {
        // ajoute le réglage à la macro, ou l'en retire s'il y est déjà
        const auto m = intArg (a, 0, 0, GrooveboxProcessor::numMacros - 1, -1);
        const auto g = intArg (a, 1, -1, numSlots - 1, -1);
        if (m >= 0 && g >= 0) proc.toggleMacroTarget (m, g);
        done (json (macrosJson()));
    });
    fn ("setMacroRange", [&proc, macrosJson] (const Args& a, Completion done)
    {
        const auto m = intArg (a, 0, 0, GrooveboxProcessor::numMacros - 1, -1);
        const auto g = intArg (a, 1, -1, numSlots - 1, -1);
        if (m >= 0 && g >= 0) proc.setMacroRange (m, g, floatArg (a, 2, 0.0f, 1.0f, 0.0f), floatArg (a, 3, 0.0f, 1.0f, 1.0f));
        done (json (macrosJson()));
    });
    fn ("setMacroMode", [&proc, macrosJson] (const Args& a, Completion done)
    {
        // v0.46 : 0 = réel (tous les réglages à la position de la macro), 1 = relatif
        if (const auto m = intArg (a, 0, 0, GrooveboxProcessor::numMacros - 1, -1); m >= 0) proc.setMacroRelative (m, intArg (a, 1, 0, 1, 1) == 1);
        done (json (macrosJson()));
    });
    fn ("clearMacro", [&proc, macrosJson] (const Args& a, Completion done)
    {
        if (const auto m = intArg (a, 0, 0, GrooveboxProcessor::numMacros - 1, -1); m >= 0) proc.clearMacro (m);
        done (json (macrosJson()));
    });
    fn ("newProject", [&proc, &eng] (const Args& a, Completion done)
    {
        proc.setProjectFileContext ({}); // v0.46 fix8 : projet sans fichier
        if (boolArg (a, 0)) proc.clearProject();
        else
        {
            // v0.46 : projet de démonstration = le projet embarqué dans l'interface (demo.gbx, un
            // vrai projet enregistré, échantillons compris) ; à défaut, la démonstration calculée
            bool loaded = false;
            for (int i = 0; i < UiData::namedResourceListSize && ! loaded; ++i)
            {
                if (juce::String (UiData::getNamedResourceOriginalFilename (UiData::namedResourceList[i])) != "demo.gbx") continue;
                int size = 0;
                if (const auto* data = UiData::getNamedResource (UiData::namedResourceList[i], size); data != nullptr && size > 0)
                {
                    juce::MemoryInputStream in (data, (size_t) size, false);
                    loaded = proc.readProjectFile (in).isEmpty();
                }
            }
            if (! loaded) proc.loadDefaultProject();
        }
        done (json (bridge::tracksJson (eng)));
    });

    // --- Échantillons --------------------------------------------------------------------------
    fn ("loadSample", [&proc, &eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1), i = intArg (a, 1, 0, maxSamples - 1, -1);
        const auto name = strArg (a, 2, 200);
        juce::String error;
        if (t < 0 || i < 0) error = "Piste inconnue";
        else if (a.size() <= 3 || ! a[3].isString() || a[3].toString().length() > 90 * 1024 * 1024) error = "Fichier trop lourd (64 Mo au plus)";
        else
        {
            juce::MemoryBlock bytes;
            if (! bytes.fromBase64Encoding (a[3].toString())) error = "Fichier illisible";
            else error = proc.loadSample (t, i, name, std::move (bytes));
        }
        done (error.isEmpty() ? json (bridge::channelJson (eng, t)) : json (bridge::quote (error.toStdString())));
    });
    fn ("clearSample", [&eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        if (t >= 0) eng.setSample (t, intArg (a, 1, 0, maxSamples - 1, 0), nullptr, nowMs());
        done (json (bridge::channelJson (eng, std::max (0, t))));
    });
    fn ("autoSlice", [&proc, &eng, T] (const Args& a, Completion done)
    {
        const auto t = intArg (a, 0, 0, T, -1);
        if (t >= 0) proc.autoSlice (t, boolArg (a, 1), intArg (a, 2, 1, 16, 16));
        done (json (bridge::channelJson (eng, std::max (0, t))));
    });

    return options;
}

//==============================================================================
GrooveboxEditor::GrooveboxEditor (GrooveboxProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      browser (createOptions())
{
    grv::Folders::setRecordingsFolder (grv::Recorder::rootFolder()); // v0.47.0 : enregistrements dans la Bibliothèque
    addAndMakeVisible (browser);
    grv::startupMark ("vue web creee, chargement de la page");

    const auto dev = devServerUrl();
    browser.goToURL (dev.isNotEmpty() ? dev : juce::WebBrowserComponent::getResourceProviderRoot());

    // L'appareil est dessiné en 16:10 et l'interface le met à l'échelle de la fenêtre en gardant
    // ses proportions : la fenêtre, elle, prend n'importe quelle taille (agrandir, plein écran,
    // écrans 4K et ultra-larges).
    setResizable (true, true);
    setResizeLimits (800, 500, 7680, 4320);
    setSize (1280, 800);

    lastProject = processor.getProjectVersion();
    lastSeq = processor.getEngine().sequencer().getVersion();
    lastStructure = processor.getEngine().getStructureVersion();
   #if JUCE_ANDROID
    startTimerHz (20); // v0.46 fix20 : téléphone et tablette — écran rafraîchi un peu moins souvent (moins de chauffe)
   #else
    startTimerHz (30);
   #endif
}

GrooveboxEditor::~GrooveboxEditor() { stopTimer(); }

juce::String GrooveboxEditor::devServerUrl()
{
   #if JUCE_DEBUG
    // Développement uniquement : interface servie par Vite sur la machine locale.
    const auto url = juce::SystemStats::getEnvironmentVariable ("GROOVEBOX_UI_DEV_URL", {});
    if (url.startsWith ("http://localhost:") || url.startsWith ("http://127.0.0.1:")) return url;
   #endif
    return {};
}

bool GrooveboxEditor::Browser::pageAboutToLoad (const juce::String& newURL)
{
    // Application d'une seule page : aucune navigation hors de l'interface embarquée.
    const auto dev = devServerUrl();
    return newURL.startsWith (juce::WebBrowserComponent::getResourceProviderRoot())
        || (dev.isNotEmpty() && newURL.startsWith (dev));
}

std::optional<juce::WebBrowserComponent::Resource> GrooveboxEditor::getResource (const juce::String& url) const
{
    const auto path = url.upToFirstOccurrenceOf ("?", false, false);
    // Le catalogue (≈ 240 ko, fixe) part comme une ressource : renvoyé par une fonction native, il
    // serait échappé caractère par caractère par la passerelle de JUCE (lent, plusieurs secondes).
    if (path == "/data/catalog.json")
    {
        static const std::string catalog = bridge::catalogJson();
        const auto* bytes = reinterpret_cast<const std::byte*> (catalog.data());
        return juce::WebBrowserComponent::Resource { std::vector<std::byte> (bytes, bytes + catalog.size()), "application/json" };
    }
    const auto filename = path == "/" || path.isEmpty() ? juce::String ("index.html") : path.fromLastOccurrenceOf ("/", false, false);
    if (static bool marked = false; ! marked && filename == "index.html") { marked = true; grv::startupMark ("page demandee par la vue web"); }
    for (int i = 0; i < UiData::namedResourceListSize; ++i)
    {
        if (filename != UiData::getNamedResourceOriginalFilename (UiData::namedResourceList[i])) continue;
        int size = 0;
        if (const auto* data = UiData::getNamedResource (UiData::namedResourceList[i], size))
        {
            const auto* bytes = reinterpret_cast<const std::byte*> (data);
            return juce::WebBrowserComponent::Resource { std::vector<std::byte> (bytes, bytes + size), mimeFor (filename) };
        }
    }
    return std::nullopt;
}

void GrooveboxEditor::parentHierarchyChanged()
{
    juce::AudioProcessorEditor::parentHierarchyChanged();
   #if ! (JUCE_ANDROID || JUCE_IOS)
    // La fenêtre de l'application autonome de JUCE n'a que « réduire » et « fermer » : on lui
    // ajoute « agrandir » (le carré), qui fait aussi agrandir d'un double-clic sur la barre de titre.
    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        if (auto* window = dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent()); window != nullptr && window != static_cast<juce::Component*> (this))
        {
            if (window->getMaximiseButton() == nullptr)
                window->setTitleBarButtonsRequired (juce::DocumentWindow::allButtons, false);
            // v0.46 : la marque dans la barre de titre (le nom du greffon, lui, ne change pas :
            // les projets des logiciels hôtes le retrouvent)
            window->setName (juce::String::fromUTF8 ("Groovebox \xE2\x80\x94 OFAD GOLEM"));
        }
   #endif
}

void GrooveboxEditor::timerCallback()
{
    auto& eng = processor.getEngine();
    const auto& seq = eng.sequencer();
    auto num = [] (double v) { char b[24]; std::snprintf (b, sizeof b, "%.4g", std::isfinite (v) ? v : 0.0); return std::string (b); };

    std::string o = "{\"t\":{\"playing\":";
    o += seq.isRunning() ? "true" : "false";
    o += ",\"rec\":"; o += seq.isRecording() ? "true" : "false";
    o += ",\"ppq\":" + num (seq.getPpq());
    o += ",\"bpm\":" + num (processor.getCurrentBpm());
    o += ",\"fill\":"; o += processor.getFill() ? "true" : "false";
    o += ",\"brk\":"; o += processor.getBreak() ? "true" : "false";
    o += ",\"tbrk\":"; o += processor.getTimedBreak() ? "true" : "false";
    {
        // v0.43 : avancée dans la boucle la plus longue (barre d'avancement de l'en-tête)
        double phase = 0.0;
        const auto loopQ = seq.longestLoop (seq.getPpq(), phase);
        o += ",\"lq\":" + num (loopQ) + ",\"lp\":" + num (phase);
    }
    // v0.46 fix8 : montage audio : mode, position (noires), prise en cours
    o += ",\"mon\":"; o += processor.montageIsOn() ? "true" : "false";
    o += ",\"mp\":" + num (processor.montagePosition());
    o += ",\"mrec\":"; o += processor.montageRecording() ? "true" : "false";
    o += "},\"g\":{\"xfade\":" + num (processor.getGlobal (ParamID::xfade)) + ",\"sceneA\":" + num (processor.getGlobal (ParamID::sceneA))
       + ",\"sceneB\":" + num (processor.getGlobal (ParamID::sceneB)) + ",\"bpm\":" + num (processor.getGlobal (ParamID::bpm))
       + ",\"swing\":" + num (processor.getGlobal (ParamID::swing));
    // v0.46 fix22 : pavé vectoriel (réglages, et position jouée avec le mouvement)
    o += ",\"vecOn\":" + num (processor.getGlobal (ParamID::vecOn)) + ",\"vecMode\":" + num (processor.getGlobal (ParamID::vecMode))
       + ",\"vecX\":" + num (processor.getGlobal (ParamID::vecX)) + ",\"vecY\":" + num (processor.getGlobal (ParamID::vecY))
       + ",\"vecOrbit\":" + num (processor.getGlobal (ParamID::vecOrbit)) + ",\"vecRate\":" + num (processor.getGlobal (ParamID::vecRate))
       + ",\"vecDepth\":" + num (processor.getGlobal (ParamID::vecDepth)) + ",\"vecRot\":" + num (processor.getGlobal (ParamID::vecRot)) + ",\"vecHeight\":" + num (processor.getGlobal (ParamID::vecHeight))
       + ",\"vecPX\":" + num (processor.getVecShownX()) + ",\"vecPY\":" + num (processor.getVecShownY());
    for (int k = 0; k < 4; ++k)
        o += ",\"" + ParamID::vecScene (k).toStdString() + "\":" + num (processor.getGlobal (ParamID::vecScene (k)))
           + ",\"" + ParamID::vecFx (k).toStdString() + "\":" + num (processor.getGlobal (ParamID::vecFx (k)));
    o += "}";

    // Notes jouées (allumage des pads) : [piste, ligne] qui ont changé
    o += ",\"hits\":[";
    bool first = true;
    for (int t = 0; t < numTracks; ++t)
        for (int l = 0; l < 16; ++l)
            if (const auto c = eng.getHitCount (t, l); c != lastHits[(size_t) t][(size_t) l])
            {
                lastHits[(size_t) t][(size_t) l] = c;
                o += (first ? "" : ",") + std::string ("[") + std::to_string (t) + "," + std::to_string (l) + "]";
                first = false;
            }
    // Notes exactes de la piste choisie : tenues (kn) et jouées depuis le dernier envoi (kh),
    // pour allumer les touches du clavier et les pads mélodiques (contrôleur, séquence, arpège)
    const auto ft = std::min (focusChannel, numTracks - 1);
    {
        const bool resync = ft != lastNoteTrack;
        lastNoteTrack = ft;
        std::string kn, kh;
        for (int n = 0; n < 128; ++n)
        {
            if (eng.isNoteDown (ft, n)) kn += (kn.empty() ? "" : ",") + std::to_string (n);
            const auto c = eng.getNoteHits (ft, n);
            if (c != lastNoteHits[(size_t) n] && ! resync) kh += (kh.empty() ? "" : ",") + std::to_string (n);
            lastNoteHits[(size_t) n] = c;
        }
        o += "],\"kn\":[" + kn + "],\"kh\":[" + kh + "],\"ph\":" + num (eng.getTrackPhase (ft));
    }
    o += ",\"peaks\":[";
    for (int ch = 0; ch < numChannels; ++ch) o += (ch ? "," : "") + num (eng.getPeak (ch));
    // v0.46 : crêtes gauche / droite de la sortie et nombre de voies de la sortie (1 = mono : le
    // panoramique ne peut pas s'entendre, l'écran de mixage le signale)
    o += "],\"outlr\":[" + num (eng.getOutPeak (0)) + "," + num (eng.getOutPeak (1));
    o += "],\"och\":" + std::to_string (processor.getMainBusNumOutputChannels());
    // Position de lecture : par ligne de la piste choisie, et ligne 1 de chaque piste
    o += ",\"pos\":[";
    for (int l = 0; l < 16; ++l) o += (l ? "," : "") + std::to_string (seq.getPosition (ft, l));
    o += "],\"tpos\":[";
    for (int t = 0; t < numTracks; ++t) o += (t ? "," : "") + std::to_string (seq.getPosition (t, 0));
    // Valeurs jouées qui diffèrent de la valeur de base (scènes, verrous, modulations) sur le canal choisi
    o += "],\"eff\":[";
    first = true;
    for (int local = 0; local < slotsPerChannel; ++local)
    {
        if (local >= modAmountBase && local < modAmountBase + numModAssigns) continue;
        const auto g = globalSlot (focusChannel, local);
        if (eng.describe (g) == nullptr) continue;
        const auto e = eng.getEffective (g);
        if (std::abs (e - eng.getParam (g)) > 1.0e-4f)
        {
            o += (first ? "" : ",") + std::string ("[") + std::to_string (local) + "," + num (e) + "]";
            first = false;
        }
    }
    o += "],\"mac\":[";
    for (int m = 0; m < GrooveboxProcessor::numMacros; ++m) o += (m ? "," : "") + num (processor.getMacroValue (m));
    o += "],\"mods\":[";
    for (int m = 0; m < numModSources; ++m)
        o += (m ? "," : "") + std::string ("[") + num (eng.getModValue (focusChannel, m)) + "," + std::to_string (eng.getModStep (focusChannel, m)) + "]";
    o += "],\"fxm\":[";
    for (int s = 0; s < numInserts; ++s)
    {
        o += (s ? "," : "") + std::string ("[");
        for (int k = 0; k < 9; ++k) o += (k ? "," : "") + num (eng.getEffectMeter (focusChannel, s, k)); // v0.31 : 8 mesures (mastering) ; v0.36 : 9 (bandes flottantes du multibande)
        o += "]";
    }
    o += "],\"pat\":[";
    for (int t = 0; t < numTracks; ++t)
        o += (t ? "," : "") + std::string ("[") + std::to_string (seq.getPlaying (t)) + "," + std::to_string (seq.getQueued (t)) + "]";
    // État de l'instrument de la piste choisie (enregistreur, position de lecture…)
    {
        float stv[48] {}; // v0.25 : jusqu'à 40 bandes d'un vocodeur
        const auto k = eng.instrumentStatus (std::min (focusChannel, numTracks - 1), stv, 48);
        o += "],\"inst\":[";
        for (int i = 0; i < k; ++i) o += (i ? "," : "") + num (stv[i]);
    }
    // v0.32.1 : molettes de la piste choisie (hauteur, modulation), y compris venant du contrôleur
    {
        const auto wt = std::min (focusChannel, numTracks - 1);
        o += "],\"whl\":[" + num (eng.getBendShown (wt)) + "," + num (eng.getWheelShown (wt));
    }
    o += "],\"hist\":[" + std::string (processor.canUndo() ? "1," : "0,") + (processor.canRedo() ? "1," : "0,") + (processor.hasPhoto() ? "1" : "0");
    // témoin de charge (v0.23) : calcul audio (moyenne, pointe, en %), mémoire de l'application et
    // part des échantillons chargés (Mo) ; la mémoire est relue une fois par seconde
    if (const auto now = juce::Time::getMillisecondCounterHiRes(); now - perfAt > 1000.0)
    {
        perfAt = now;
        memMb = (double) processMemoryBytes() / (1024.0 * 1024.0);
        if (totalMemMb <= 0.0) totalMemMb = (double) juce::SystemStats::getMemorySizeInMegabytes();
        double bytes = 0.0;
        for (int t = 0; t < numTracks; ++t)
            for (int i = 0; i < maxSamples; ++i)
                if (const auto* s = eng.getSample (t, i); s != nullptr)
                    bytes += (double) (s->data.size() + s->right.size()) * sizeof (float) + (s->original != nullptr ? (double) s->original->size() : 0.0);
        sampleMb = bytes / (1024.0 * 1024.0);
    }
    o += "],\"perf\":[" + num (processor.getCpuAverage() * 100.0f) + "," + num (processor.getCpuPeak() * 100.0f) + "," + num ((float) memMb) + "," + num ((float) sampleMb)
       + "," + num ((float) processor.getSampleRate()) + "," + num ((float) processor.getAudioBlockSize()) + "," + num (processor.getAudioPeriodMs())
       + "," + num ((float) totalMemMb) // v0.46 fix17 : mémoire de l'appareil (Mo), pour l'afficher en %
       + "," + num ((float) processor.getAndroidAudioStatus());
    o += "],\"learn\":" + std::to_string (processor.getMidiLearn());
    // v0.46 fix24 : positions des 16 boutons assignables, valeurs d'écran pilotées (MIDI, boutons)
    o += ",\"kv\":[";
    for (int k = 0; k < GrooveboxProcessor::numKnobs; ++k) o += (k ? "," : "") + num (processor.getKnob (k));
    // v0.46 fix28 : valeur jouée du réglage relié (modulation LFO…, scène, verrou), sur toutes les pistes,
    // pour que le bouton bouge avec sa modulation comme les boutons des synthés ; -1 = la valeur du bouton
    // v0.46 fix28 : valeur envoyée par chaque bouton avec son LFO (-1 : pas de LFO)
    // v0.46 fix28 : sources des 16 boutons (1 à 4 du bus principal) : [valeur, pas] pour leur dessin animé
    o += "],\"kmods\":[";
    for (int m = 0; m < GrooveboxProcessor::numKnobLfos; ++m)
        o += (m ? ",[" : "[") + num (eng.getModValue (GrooveboxProcessor::knobLfoChannel, m)) + "," + std::to_string (eng.getModStep (GrooveboxProcessor::knobLfoChannel, m)) + "]";
    o += "],\"ko\":[";
    for (int k = 0; k < GrooveboxProcessor::numKnobs; ++k) o += (k ? "," : "") + num (processor.getKnobOut (k));
    o += "],\"ke\":[";
    for (int k = 0; k < GrooveboxProcessor::numKnobs; ++k)
    {
        const auto t = processor.getKnobTarget (k);
        float e = -1.0f;
        // v0.46 fix30 : le bouton suit TOUJOURS la valeur de son réglage (tournée depuis le synthé, une scène,
        // un verrou, une courbe, une modulation…), plus seulement quand elle est modulée
        if (t >= 0 && eng.describe (t) != nullptr)
        {
            const auto v = eng.getEffective (t);
            e = std::isfinite (v) && v >= 0.0f && v <= 1.0f ? v : eng.getParam (t);
        }
        o += (k ? "," : "") + num (e);
    }
    o += "]";
    if (const auto uv = processor.getUiCtlVersion(); uv != lastUiCtlVersion)
    {
        lastUiCtlVersion = uv;
        o += ",\"uc\":[";
        bool firstUc = true;
        for (int u = 0; u < GrooveboxProcessor::numUiCtl; ++u)
            if (const auto useq = processor.getUiCtlSeq (u); useq != lastUiCtlSeq[(size_t) u])
            {
                lastUiCtlSeq[(size_t) u] = useq;
                o += (firstUc ? "[" : ",[") + std::to_string (u) + "," + num (processor.getUiCtl (u)) + "]";
                firstUc = false;
            }
        o += "]";
    }
    dsp::LoudnessBlock lb;
    while (eng.popLoudness (lb)) loudness.add (lb);
    if (spectrumOn != 0)
    {
        std::array<float, dsp::SpectrumAnalyzer::numBands> mid {}, side {};
        spectrum.compute (eng.outputScope(), eng.getSampleRate(), mid, side);
        o += ",\"spec\":[";
        for (int b = 0; b < dsp::SpectrumAnalyzer::numBands; ++b) o += (b ? "," : "") + std::to_string ((int) std::lround (mid[(size_t) b] * 10.0f));
        for (int b = 0; b < dsp::SpectrumAnalyzer::numBands; ++b) o += "," + std::to_string ((int) std::lround (side[(size_t) b] * 10.0f));
        o += "]";
    }
    o += ",\"loud\":[" + num (loudness.momentary()) + "," + num (loudness.shortTerm()) + "," + num (loudness.integrated())
       + "," + num (loudness.truePeakDb()) + "," + num (loudness.maxTruePeakDb()) + "," + num (loudness.rmsDb()) + "]";
    const auto mv = processor.getMidiVersion();
    // v0.32.1 : réglages écrits par le contrôleur MIDI ou une macro de l'hôte : [adresse, valeur]
    o += ",\"pv\":[";
    {
        bool firstPv = true;
        for (const auto g : processor.drainTouched (touchedRead))
        {
            o += (firstPv ? "" : ",") + std::string ("[") + std::to_string (g) + "," + num (eng.getParam (g)) + "]";
            firstPv = false;
        }
    }
    o += "]";
    o += ",\"midi\":" + std::string (mv != lastMidi ? "true" : "false");
    lastMidi = mv;

    // Changements faits ailleurs (enregistrement en direct, MIDI, rappel d'un projet par le logiciel hôte)
    const auto pv = processor.getProjectVersion(), sv = seq.getVersion(), st = eng.getStructureVersion();
    o += ",\"project\":" + std::string (pv != lastProject ? "true" : "false");
    std::uint64_t busMask = 0;
    for (int ch = numTracks; ch < numChannels; ++ch)
        busMask |= (eng.getBusMute (ch) ? 1ull : 0ull) << (2 * (ch - numTracks)) | (eng.getBusSolo (ch) ? 2ull : 0ull) << (2 * (ch - numTracks));
    o += ",\"seq\":" + std::string (sv != lastSeq || busMask != lastBusMask ? "true" : "false");
    lastBusMask = busMask;
    o += ",\"structure\":" + std::string (st != lastStructure ? "true" : "false");
    // morceau : rangée jouée, position (en noires), version (rangées et courbes à relire)
    o += ",\"song\":[" + std::to_string (eng.getSongRow()) + "," + num (eng.getSongBeat()) + "," + std::to_string (eng.getSongVersion())
       + "," + (eng.getRecordPlay() ? "1" : "0") + "," + (eng.getRecordMoves() ? "1" : "0") + "]}";
    lastProject = pv; lastSeq = sv; lastStructure = st;

    browser.emitEventIfBrowserIsVisible ("tick", juce::var (juce::String (o)));
}

std::string GrooveboxEditor::launchJson() const
{
    auto o = bridge::launchJson (processor.getEngine());
    o.pop_back(); // '}'
    o += std::string (",\"muteTails\":") + (processor.getEngine().sequencer().getMuteTails() ? "true" : "false");
    o += ",\"rowScenes\":[";
    for (int r = 0; r < Sequencer::numPatterns; ++r) o += (r ? "," : "") + std::to_string (processor.getRowScene (r));
    return o + "]}";
}

std::string GrooveboxEditor::midiJson() const
{
    std::string o = "{\"learn\":" + std::to_string (processor.getMidiLearn()) + ",\"maps\":[";
    bool first = true;
    for (const auto& m : processor.getMidiMappings())
    {
        o += (first ? "" : ",") + std::string ("[") + std::to_string (m.kind) + "," + std::to_string (m.channel) + ","
           + std::to_string (m.number) + "," + std::to_string (m.target) + "]";
        first = false;
    }
    o += "]";
    int kind, ch, num, oldT, newT;
    if (processor.getMidiConflict (kind, ch, num, oldT, newT))
        o += ",\"conflict\":[" + std::to_string (kind) + "," + std::to_string (ch) + "," + std::to_string (num) + "," + std::to_string (oldT) + "," + std::to_string (newT) + "]";
    // v0.46 : canaux MIDI
    o += ",\"route\":{\"note\":" + std::to_string (processor.getMidiNoteChannel()) + ",\"ctrl\":" + std::to_string (processor.getMidiCtrlChannel())
       + ",\"ctrlOn\":" + (processor.getMidiCtrlOn() ? "true" : "false") + ",\"tracks\":[";
    for (int t = 0; t < numTracks; ++t) o += (t ? "," : "") + std::to_string (processor.getTrackMidiChannel (t));
    o += "]}";
    // v0.46 fix24 : boutons assignables [cible, position] et table des valeurs d'écran (texte JSON de l'interface)
    o += ",\"knobs\":[";
    for (int k = 0; k < GrooveboxProcessor::numKnobs; ++k)
        o += (k ? ",[" : "[") + std::to_string (processor.getKnobTarget (k)) + "," + std::to_string (processor.getKnob (k))
           + "," + std::to_string (processor.getKnobLfo (k)) + "," + std::to_string (processor.getKnobDepth (k)) + "]"; // v0.46 fix28 : LFO, profondeur
    o += "],\"ctl\":" + bridge::quote (processor.getCtlTable().toStdString());
    return o + "}";
}

void GrooveboxEditor::finishFile (const juce::String& kind, const juce::String& error)
{
    const auto o = "{\"kind\":" + bridge::quote (kind.toStdString()) + ",\"error\":" + bridge::quote (error.toStdString()) + "}";
    browser.emitEventIfBrowserIsVisible ("fileDone", juce::var (juce::String (o)));
}

void GrooveboxEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff15171b)); }
void GrooveboxEditor::resized() { browser.setBounds (getLocalBounds()); }

} // namespace grv
