/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Compilé seulement avec l'application autonome (voir StandaloneInput.h) : donne au code partagé
    l'accès au réglage « Mute audio input » de la fenêtre autonome de JUCE.
*/

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h> // projet Projucer (Android) : réglages des modules et du greffon
#endif
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "StandaloneInput.h"

namespace
{
const bool registered = []
{
    grv::standaloneInputHook() = [] (int op) -> int
    {
        auto* holder = juce::StandalonePluginHolder::getInstance();
        if (holder == nullptr) return -1;
        if (op == 0 || op == 1) holder->getMuteInputValue().setValue (op == 1);
        return (bool) holder->getMuteInputValue().getValue() ? 1 : 0;
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
        return std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(), false, juce::String{}, nullptr, channelConfig, false);
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
    std::unique_ptr<GolemSplash> splash;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};
} // namespace

juce::JUCEApplicationBase* juce_CreateApplication() { return new GolemStandaloneApp(); }
#endif
