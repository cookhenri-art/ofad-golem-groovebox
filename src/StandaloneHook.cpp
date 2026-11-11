/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Compilé seulement avec l'application autonome (voir StandaloneInput.h) : donne au code partagé
    l'accès au réglage « Mute audio input » de la fenêtre autonome de JUCE.
*/

#define JUCE_CORE_INCLUDE_JNI_HELPERS 1 // v0.46 fix21 : JNI (écran gardé allumé sur Android)
#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h> // projet Projucer (Android) : réglages des modules et du greffon
#endif
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "StandaloneInput.h"
#include "engine/RemoteProtocol.h"
#include "CrashGuard.h"

#include <vector>

#if JUCE_ANDROID
namespace grv
{
// v0.46 fix21 : écran gardé allumé (drapeau FLAG_KEEP_SCREEN_ON de la fenêtre de l'activité) :
// seulement tant que l'appli est au premier plan — Android l'oublie dès qu'on la quitte —, et
// seulement quand on joue ou qu'on l'a touchée il y a moins de 10 minutes. Fil des messages
// (le fil principal d'Android).
void androidKeepScreenOn (bool on)
{
    auto* env = juce::getEnv();
    auto activity = juce::getMainActivity();
    if (env == nullptr || activity.get() == nullptr) return;
    auto actCls = env->GetObjectClass (activity.get());
    const auto getWindow = env->GetMethodID (actCls, "getWindow", "()Landroid/view/Window;");
    jobject window = getWindow != nullptr ? env->CallObjectMethod (activity.get(), getWindow) : nullptr;
    if (window != nullptr)
    {
        auto winCls = env->GetObjectClass (window);
        if (const auto mid = env->GetMethodID (winCls, on ? "addFlags" : "clearFlags", "(I)V"); mid != nullptr)
            env->CallVoidMethod (window, mid, (jint) 0x00000080); // WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        env->DeleteLocalRef (winCls);
        env->DeleteLocalRef (window);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef (actCls);
}

// v0.46.2 : toutes les vues de la fenêtre (vue web comprise) : ni effet d'étirement au bord
// (OVER_SCROLL_NEVER), ni barres de défilement. La vue web est créée après l'activité : on repasse
// de temps en temps (quelques vues : coût négligeable).
// v0.46.2 : vues gardées (références globales) pour les ramener à leur place à chaque battement
static std::vector<jobject> pinnedViews;

void androidPinViews()
{
    auto* env = juce::getEnv();
    auto activity = juce::getMainActivity();
    if (env == nullptr || activity.get() == nullptr) return;
    auto actCls = env->GetObjectClass (activity.get());
    const auto getWindow = env->GetMethodID (actCls, "getWindow", "()Landroid/view/Window;");
    jobject window = getWindow != nullptr ? env->CallObjectMethod (activity.get(), getWindow) : nullptr;
    env->DeleteLocalRef (actCls);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (window == nullptr) return;
    auto winCls = env->GetObjectClass (window);
    const auto getDecor = env->GetMethodID (winCls, "getDecorView", "()Landroid/view/View;");
    jobject decor = getDecor != nullptr ? env->CallObjectMethod (window, getDecor) : nullptr;
    env->DeleteLocalRef (winCls);
    env->DeleteLocalRef (window);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (decor == nullptr) return;

    auto viewCls = env->FindClass ("android/view/View");
    auto groupCls = env->FindClass ("android/view/ViewGroup");
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    const auto setOverScroll = env->GetMethodID (viewCls, "setOverScrollMode", "(I)V");
    const auto setVBar = env->GetMethodID (viewCls, "setVerticalScrollBarEnabled", "(Z)V");
    const auto setHBar = env->GetMethodID (viewCls, "setHorizontalScrollBarEnabled", "(Z)V");
    const auto childCount = env->GetMethodID (groupCls, "getChildCount", "()I");
    const auto childAt = env->GetMethodID (groupCls, "getChildAt", "(I)Landroid/view/View;");
    if (env->ExceptionCheck() || setOverScroll == nullptr || childCount == nullptr || childAt == nullptr)
    {
        env->ExceptionClear();
        env->DeleteLocalRef (decor); env->DeleteLocalRef (viewCls); env->DeleteLocalRef (groupCls);
        return;
    }
    // vue web : pas de zoom, pas d'agrandissement du texte par le système (la disposition reste celle prévue)
    auto webCls = env->FindClass ("android/webkit/WebView");
    if (env->ExceptionCheck()) { env->ExceptionClear(); webCls = nullptr; }
    jmethodID getSettings = webCls != nullptr ? env->GetMethodID (webCls, "getSettings", "()Landroid/webkit/WebSettings;") : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); getSettings = nullptr; }
    for (auto g : pinnedViews) env->DeleteGlobalRef (g);
    pinnedViews.clear();
    // parcours en largeur, borné (pas de récursion : pile courte)
    if (env->EnsureLocalCapacity (600) != 0) { env->ExceptionClear(); }
    std::vector<jobject> todo { decor };
    for (size_t i = 0; i < todo.size() && i < 512; ++i)
    {
        jobject v = todo[i];
        env->CallVoidMethod (v, setOverScroll, (jint) 2); // View.OVER_SCROLL_NEVER
        if (setVBar != nullptr) env->CallVoidMethod (v, setVBar, (jboolean) JNI_FALSE);
        if (setHBar != nullptr) env->CallVoidMethod (v, setHBar, (jboolean) JNI_FALSE);
        if (pinnedViews.size() < 64) pinnedViews.push_back (env->NewGlobalRef (v));
        if (webCls != nullptr && getSettings != nullptr && env->IsInstanceOf (v, webCls))
            if (jobject st = env->CallObjectMethod (v, getSettings); st != nullptr)
            {
                auto stCls = env->GetObjectClass (st);
                auto callBool = [&] (const char* name, bool on) { if (auto m = env->GetMethodID (stCls, name, "(Z)V")) env->CallVoidMethod (st, m, (jboolean) (on ? JNI_TRUE : JNI_FALSE)); if (env->ExceptionCheck()) env->ExceptionClear(); };
                callBool ("setSupportZoom", false);
                callBool ("setBuiltInZoomControls", false);
                callBool ("setDisplayZoomControls", false);
                if (auto m = env->GetMethodID (stCls, "setTextZoom", "(I)V")) env->CallVoidMethod (st, m, (jint) 100);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef (stCls);
                env->DeleteLocalRef (st);
            }
        if (env->IsInstanceOf (v, groupCls))
        {
            const auto n = env->CallIntMethod (v, childCount);
            for (jint c = 0; c < n && todo.size() < 512; ++c)
                if (jobject ch = env->CallObjectMethod (v, childAt, c); ch != nullptr) todo.push_back (ch);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    for (auto v : todo) env->DeleteLocalRef (v);
    env->DeleteLocalRef (viewCls);
    env->DeleteLocalRef (groupCls);
    if (webCls != nullptr) env->DeleteLocalRef (webCls);
}

bool androidHasWindowFocus()
{
    auto* env = juce::getEnv();
    auto activity = juce::getMainActivity();
    if (env == nullptr || activity.get() == nullptr) return true;
    auto cls = env->GetObjectClass (activity.get());
    const auto m = env->GetMethodID (cls, "hasWindowFocus", "()Z");
    const bool focus = m == nullptr || env->CallBooleanMethod (activity.get(), m) != JNI_FALSE;
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef (cls);
    return focus;
}

// v0.46.2 : à chaque battement (25 par seconde) : une vue déplacée (défilement natif de la vue web,
// panoramique du système) revient en (0, 0). L'interface ne défile jamais dans son ensemble.
void androidResetViewScroll()
{
    if (pinnedViews.empty()) return;
    auto* env = juce::getEnv();
    if (env == nullptr) return;
    auto viewCls = env->FindClass ("android/view/View");
    if (env->ExceptionCheck() || viewCls == nullptr) { env->ExceptionClear(); return; }
    const auto getX = env->GetMethodID (viewCls, "getScrollX", "()I");
    const auto getY = env->GetMethodID (viewCls, "getScrollY", "()I");
    const auto scrollTo = env->GetMethodID (viewCls, "scrollTo", "(II)V");
    if (getX != nullptr && getY != nullptr && scrollTo != nullptr)
        for (auto v : pinnedViews)
        {
            if (env->CallIntMethod (v, getX) != 0 || env->CallIntMethod (v, getY) != 0) env->CallVoidMethod (v, scrollTo, (jint) 0, (jint) 0);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef (viewCls);
}
} // namespace grv
#endif

namespace
{
const bool registered = []
{
    grv::standaloneInputHook() = [] (int op) -> int
    {
        auto* holder = juce::StandalonePluginHolder::getInstance();
        if (holder == nullptr) return -1;
        // 1.1 : télécommande : écoute des « bonjour » avec l'entrée MIDI d'où ils viennent (20)
        if (op == 20)
        {
            struct HelloListener final : juce::MidiInputCallback
            {
                void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& m) override
                {
                    if (source == nullptr) return;
                    grv::noteFromDevice (source->getName(), m.getRawData(), m.getRawDataSize()); // après la 1.1 : anti-boucle des pistes MIDI
                    grv::pushLearn (m.getRawData(), m.getRawDataSize()); // 1.1 : « Apprendre » de la télécommande (armé seulement)
                    if (! m.isSysEx()) return;
                    bool full = false;
                    const auto* d = m.getSysExData();
                    const auto n = (size_t) m.getSysExDataSize();
                    if (remote::isHello (d, n, full)) grv::remoteHello().post (source->getName(), full);
                    // côté téléphone : état et noms renvoyés par le PC
                    else if (remote::isFeedback (d, n)) grv::remoteIn().push (d, n);
                }
            };
            static HelloListener listener;
            static bool added = false;
            if (! added) { holder->deviceManager.addMidiInputDeviceCallback ({}, &listener); added = true; }
            return 20;
        }
        // 1.1 : mode télécommande : son de l'appareil en pause (10) ou repris (11)
        if (op == 10) { holder->deviceManager.closeAudioDevice(); return 10; }
        if (op == 11) { if (holder->deviceManager.getCurrentAudioDevice() == nullptr) holder->deviceManager.restartLastAudioDevice(); return 11; }
       #if JUCE_ANDROID
        // v0.46 fix16 : sur Android, le micro n'est pas « coupé » mais FERMÉ : pas de flux d'entrée
        // tant qu'on ne l'ouvre pas. Un flux d'entrée ouvert en même temps que la sortie (micro du
        // téléphone) fait craquer certains appareils et la coupure de JUCE affiche un bandeau qui
        // réduit la fenêtre. op 0 : ouvrir le micro ; 1 : le fermer ; 2 : état de départ (fermé).
        auto& dm = holder->deviceManager;
        const auto inputsOn = [&dm]
        {
            auto* d = dm.getCurrentAudioDevice();
            return d != nullptr && d->getActiveInputChannels().countNumberOfSetBits() > 0;
        };
        if (op == 0 || op == 1 || op == 2)
        {
            holder->getMuteInputValue().setValue (false); // pas de bandeau : l'entrée est fermée à la place
            const bool want = op == 0;
            if (want != inputsOn())
            {
                auto setup = dm.getAudioDeviceSetup();
                setup.useDefaultInputChannels = false;
                setup.inputChannels.clear();
                if (want) setup.inputChannels.setRange (0, 2, true);
                dm.setAudioDeviceSetup (setup, true);
            }
        }
        return inputsOn() ? 0 : 1;
       #else
        if (op == 0 || op == 1) holder->getMuteInputValue().setValue (op == 1);
        return (bool) holder->getMuteInputValue().getValue() ? 1 : 0;
       #endif
    };
    // après la 1.1 : sortie MIDI des pistes MIDI (sortie MIDI par défaut, envoyée par le lecteur de JUCE)
    // type de liaison d'un appareil, d'après son nom et son identifiant (vide si on ne peut pas le dire) :
    // « usb », « bt » (Bluetooth), « net » (réseau : rtpMIDI…), « virt » (port virtuel : loopMIDI…)
    static const auto linkKind = [] (const juce::MidiDeviceInfo& d) -> juce::String
    {
        const auto s = (d.name + " " + d.identifier).toLowerCase();
        if (s.contains ("rtpmidi") || s.contains ("network")) return "net";
        if (s.contains ("loopmidi") || s.contains ("virtual") || s.contains ("loopbe")) return "virt";
        if (s.contains ("bluetooth") || s.contains ("bthle") || s.contains ("ble-midi") || s.contains ("ble midi") || s.contains (" ble")) return "bt";
        if (s.contains ("usb")) return "usb";
        return {};
    };
    grv::standaloneMidiOutHook() = [] (const juce::var& arg) -> juce::var
    {
        auto* holder = juce::StandalonePluginHolder::getInstance();
        if (holder == nullptr) return {};
        auto& dm = holder->deviceManager;
        if (arg.isString())
        {
            const auto id = arg.toString();
            bool known = id.isEmpty();
            for (const auto& d : juce::MidiOutput::getAvailableDevices()) known = known || d.identifier == id;
            if (known)
            {
                dm.setDefaultMidiOutputDevice (id);
                holder->player.setMidiOutput (dm.getDefaultMidiOutput());
            }
        }
        // fil d'envoi : les messages partent à leur place dans le bloc (pas tous au début)
        if (auto* o = dm.getDefaultMidiOutput(); o != nullptr && ! o->isBackgroundThreadRunning()) o->startBackgroundThread();
        // anti-boucle : le nom de l'appareil joué par les pistes MIDI (ce qu'il envoie ne les rejoue pas)
        {
            juce::String name;
            for (const auto& d : juce::MidiOutput::getAvailableDevices()) if (d.identifier == dm.getDefaultMidiOutputIdentifier()) name = d.name;
            grv::setMidiOutName (name);
        }
        auto* res = new juce::DynamicObject();
        juce::Array<juce::var> list;
        for (const auto& d : juce::MidiOutput::getAvailableDevices())
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", d.identifier);
            o->setProperty ("name", d.name);
            o->setProperty ("kind", linkKind (d));
            list.add (juce::var (o));
        }
        res->setProperty ("outs", list);
        res->setProperty ("current", dm.getDefaultMidiOutputIdentifier());
        return juce::var (res);
    };
    return true;
}();
} // namespace

// ============================================================================================
// v0.46 fix6 : application autonome d'OFAD GOLEM (Windows, Linux ; pas la tablette, qui garde
// celle de JUCE). Reprend l'application autonome de JUCE (juce_audio_plugin_client_Standalone.cpp)
// avec deux ajouts :
//  - un écran de lancement : la mascotte arrive d'un coup (« tac », 0,22 s) au centre de l'écran,
//    sur fond transparent, PUIS le moteur et la fenêtre se chargent derrière elle ; elle s'efface
//    en fondu quand l'interface est prête (fonction native « appStart » → uiReadyHook), au plus
//    tard après 15 s ;
//  - la fenêtre s'ouvre agrandie (tout l'écran, barre des tâches comprise), plus en petit dans un
//    coin. Le bouton « agrandir » de la barre de titre la remet en fenêtre.
// Compilé seulement si JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP (CMakeLists.txt, application autonome).
#if JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP && ! (JUCE_ANDROID || JUCE_IOS)
#include <UiData.h>

namespace
{
juce::Image golemImage()
{
    for (int i = 0; i < UiData::namedResourceListSize; ++i)
    {
        if (juce::String (UiData::getNamedResourceOriginalFilename (UiData::namedResourceList[i])) != "golem-512.png") continue;
        int size = 0;
        if (const auto* data = UiData::getNamedResource (UiData::namedResourceList[i], size); data != nullptr && size > 0)
            return juce::ImageFileFormat::loadFrom (data, (size_t) size);
    }
    return {};
}

/** Écran de lancement : fenêtre sans cadre, transparente, toujours devant, centrée. */
class GolemSplash final : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onPopped, onDone;

    explicit GolemSplash (juce::Image img) : image (std::move (img))
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
        const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto area = display != nullptr ? display->userArea : juce::Rectangle<int> (0, 0, 1280, 800);
        const int h = juce::jlimit (300, 600, area.getHeight() * 55 / 100);
        const int w = h * 3 / 4 + 40;
        setBounds (area.withSizeKeepingCentre (w, h));
        setAlwaysOnTop (true);
        addToDesktop (0);
        setVisible (true);
        toFront (false);
        born = juce::Time::getMillisecondCounterHiRes();
        if (auto* peer = getPeer()) peer->performAnyPendingRepaintsNow();
        startTimerHz (60);
    }

    void uiReady() { ready = true; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto textH = b.getHeight() * 0.16f;
        auto imgArea = b.withTrimmedBottom (textH).reduced (4.0f);
        // arrivée « tac » : grossit en dépassant un peu, puis se pose (pivot : les pieds)
        g.saveState();
        g.addTransform (juce::AffineTransform::scale (scale, scale, imgArea.getCentreX(), imgArea.getBottom()));
        g.setOpacity (juce::jlimit (0.0f, 1.0f, scale * 1.4f - 0.4f));
        g.drawImage (image, imgArea, juce::RectanglePlacement::centred);
        g.restoreState();

        // nom et « chargement » : contour sombre puis lettres claires, lisibles sur tous les fonds d'écran
        auto text = b.withTop (b.getBottom() - textH);
        auto word = text.removeFromTop (text.getHeight() * 0.62f);
        auto shadowed = [&g] (const juce::String& s, juce::Rectangle<float> r, juce::Font font, juce::Colour c)
        {
            g.setFont (font);
            g.setColour (juce::Colour (0xc0000000));
            for (auto [dx, dy] : { std::pair { 0.0f, 2.0f }, std::pair { 1.0f, 1.0f }, std::pair { -1.0f, 1.0f } })
                g.drawText (s, r.translated (dx, dy), juce::Justification::centred);
            g.setColour (c);
            g.drawText (s, r, juce::Justification::centred);
        };
        shadowed ("OFAD GOLEM", word, juce::Font (juce::FontOptions (word.getHeight() * 0.68f, juce::Font::bold)).withExtraKerningFactor (0.28f),
                  juce::Colour (0xffeae4da));
        shadowed (juce::String::fromUTF8 ("CHARGEMENT\xE2\x80\xA6"), text, juce::Font (juce::FontOptions (text.getHeight() * 0.55f, juce::Font::bold)).withExtraKerningFactor (0.16f),
                  juce::Colour (0xfff5a623));
    }

private:
    void timerCallback() override
    {
        const double t = juce::Time::getMillisecondCounterHiRes() - born;
        if (phase == 0)
        {
            const float x = (float) juce::jlimit (0.0, 1.0, t / popMs);
            // dépassement (« back ») : 0,6 → 1,06 → 1
            const float c = 1.9f;
            const float e = 1.0f + (c + 1.0f) * std::pow (x - 1.0f, 3.0f) + c * std::pow (x - 1.0f, 2.0f);
            scale = 0.6f + 0.4f * e;
            repaint();
            if (x >= 1.0f)
            {
                phase = 1; scale = 1.0f; repaint();
                if (auto* peer = getPeer()) peer->performAnyPendingRepaintsNow();
                // la fenêtre (moteur + interface) se construit au tour suivant, sous l'écran de lancement
                juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<GolemSplash> (this)]
                                                 { if (safe != nullptr && safe->onPopped) safe->onPopped(); });
            }
        }
        else if (phase == 1)
        {
            if ((ready && t > minMs) || t > maxMs) { phase = 2; fadeFrom = t; }
        }
        else
        {
            const float a = 1.0f - (float) ((t - fadeFrom) / fadeMs);
            setAlpha (juce::jmax (0.0f, a));
            if (a <= 0.0f)
            {
                stopTimer();
                juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<GolemSplash> (this)]
                                                 { if (safe != nullptr && safe->onDone) safe->onDone(); });
            }
        }
    }

    juce::Image image;
    double born = 0.0, fadeFrom = 0.0;
    static constexpr double popMs = 220.0, minMs = 1300.0, maxMs = 90000.0, fadeMs = 280.0; // fix11 : reste jusqu'à l'interface prête
    float scale = 0.6f;
    int phase = 0;
    bool ready = false;
};

class GolemStandaloneApp final : public juce::JUCEApplication
{
public:
    GolemStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = juce::CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif
        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override           { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override        { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override                 { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String&) override
    {
        grv::crashguard::install(); // v0.49 (essai 3) : journal de plantage, démarrage sans la séance après un plantage
       #if JUCE_ANDROID
        // v0.49 (essai 3) : rapport du plantage précédent montré AVANT de créer le moteur (il reste
        // lisible même si l'appli replante ensuite au même endroit) ; on continue après « OK »
        if (const auto report = grv::crashguard::previousReport(); (report.isNotEmpty() || grv::crashguard::safeStart()) && ! reportShown)
        {
            reportShown = true;
            juce::String msg;
            if (grv::crashguard::safeStart())
                msg << juce::String::fromUTF8 ("Séance précédente gardée de côté (non rouverte).\n\n");
            msg << (report.isNotEmpty() ? report.substring (0, 3000) : juce::String::fromUTF8 ("(aucun rapport : la fermeture n'a pas été interceptée)"));
            juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                                   .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                   .withTitle (juce::String::fromUTF8 ("GOLEM s'est fermé au dernier lancement"))
                                                   .withMessage (msg)
                                                   .withButton ("OK"),
                                               [this] (int) { initialise ({}); });
            return;
        }
       #endif
        grv::startupMark ("application lancee");
        if (auto img = golemImage(); img.isValid() && ! juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            splash = std::make_unique<GolemSplash> (std::move (img));
            grv::startupMark ("ecran de lancement affiche");
            splash->onPopped = [this] { openMainWindow(); };
            splash->onDone   = [this] { splash.reset(); };
            grv::uiReadyHook() = [this] { grv::startupMark ("interface prete"); if (splash != nullptr) splash->uiReady(); };
            return;
        }
        openMainWindow();
    }

    void shutdown() override
    {
        grv::uiReadyHook() = nullptr;
        splash = nullptr;
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr) pluginHolder->savePluginState();
        if (mainWindow != nullptr) mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig (channels, juce::numElementsInArray (channels));
       #else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif
        // v0.46 fix27 : contrôleurs MIDI prêts à jouer : tout clavier ou contrôleur branché est ouvert tout
        // seul (vérifié toutes les 500 ms), sur PC comme sur Android
        return std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(), false, juce::String{}, nullptr, channelConfig, true);
    }

    void openMainWindow()
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            pluginHolder = createPluginHolder();
            if (splash != nullptr) splash->uiReady();
            return;
        }
        grv::startupMark ("moteur, carte son et derniere seance : debut");
        auto holder = createPluginHolder();
        grv::startupMark ("moteur, carte son et derniere seance : fin");
        mainWindow = std::make_unique<juce::StandaloneFilterWindow> (getApplicationName(), juce::Colour (0xff15171b), std::move (holder));
        grv::startupMark ("fenetre et interface creees");
        mainWindow->setVisible (true);
        mainWindow->setFullScreen (true); // agrandie : tout l'écran (la barre des tâches reste visible)
        if (splash != nullptr) splash->toFront (false);
    }

    juce::ApplicationProperties appProperties;
    bool reportShown = false; // v0.49 (essai 3)
    std::unique_ptr<GolemSplash> splash;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};
} // namespace

juce::JUCEApplicationBase* juce_CreateApplication() { return new GolemStandaloneApp(); }
#endif
