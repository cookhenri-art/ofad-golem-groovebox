/*
    Groovebox — OFAD — AGPL-3.0-or-later
*/

#include "PluginProcessor.h"
#include "engine/RemoteProtocol.h"
#include "StandaloneInput.h" // v0.46 fix11 : journal du démarrage
#include "CrashGuard.h"      // v0.49 (essai 3) : journal de plantage (Android)
#include "ParseGuard.h"       // v0.46.1 (audit F3)

#include <set>
#include <map>
#include "engine/Catalog.h"
#include "engine/PresetOrder.h"
#include "UserPresets.h"
#include "instruments/Sampler.h"
#include "instruments/Octa.h"
#include "instruments/Vocoders.h"
#include "instruments/FmPlayer.h"
#include "effects/Distortion.h"
#if ! GROOVEBOX_NO_EDITOR
 #include "PluginEditor.h"
#endif

namespace grv
{

using namespace engine;
static void busesFromTree (Engine& engine, const juce::ValueTree& tree); // v0.31

namespace
{
double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

/** Canal d'un INSERT enregistré : hors de la disposition actuelle (projet d'une version plus récente),
    il est ignoré au lieu d'être ramené sur le dernier canal (v0.24). */
bool validChannel (const juce::ValueTree& c)
{
    const auto v = c.getProperty ("c");
    return ! v.isVoid() && (int) v >= 0 && (int) v < numChannels;
}

juce::String toHex (const std::vector<int>& values)
{
    juce::String hex;
    hex.preallocateBytes (values.size() * 2 + 1);
    for (auto v : values) hex << juce::String::toHexString (v & 0xFF).paddedLeft ('0', 2);
    return hex;
}

std::vector<int> fromHex (const juce::String& hex, int count)
{
    std::vector<int> out ((size_t) count, -1);
    for (int i = 0; i < count && i * 2 + 1 < hex.length(); ++i)
        out[(size_t) i] = hex.substring (i * 2, i * 2 + 2).getHexValue32();
    return out;
}

/** Valeurs sur quatre chiffres (durées au-delà de 255 huitièmes). */
juce::String toHex16 (const std::vector<int>& values)
{
    juce::String hex;
    hex.preallocateBytes (values.size() * 4 + 1);
    for (auto v : values) hex << juce::String::toHexString (v & 0xFFFF).paddedLeft ('0', 4);
    return hex;
}

std::vector<int> fromHex16 (const juce::String& hex, int count)
{
    std::vector<int> out ((size_t) count, -1);
    for (int i = 0; i < count && i * 4 + 3 < hex.length(); ++i)
        out[(size_t) i] = hex.substring (i * 4, i * 4 + 4).getHexValue32();
    return out;
}

/** Durées d'une voie : « len16 » (quatre chiffres) si présent, sinon « len » (anciens fichiers). */
std::vector<int> readLengths (const juce::ValueTree& lane, int count)
{
    if (lane.hasProperty ("len16")) return fromHex16 (lane.getProperty ("len16").toString(), count);
    return fromHex (lane.getProperty ("len").toString(), count);
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout GrooveboxProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using Attr = juce::AudioParameterFloatAttributes;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::bpm, 1 }, "Tempo",
                                                             juce::NormalisableRange<float> (40.0f, 240.0f, 0.1f), 122.0f, Attr().withLabel ("BPM")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::swing, 1 }, "Swing",
                                                             juce::NormalisableRange<float> (50.0f, 75.0f, 0.1f), 54.0f, Attr().withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::xfade, 1 }, "Crossfader",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ParamID::sceneA, 1 }, "Scene A", 1, numScenes, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ParamID::sceneB, 1 }, "Scene B", 1, numScenes, 9));
    for (int m = 0; m < numMacros; ++m)
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::macro (m), 1 }, "Macro " + juce::String (m + 1),
                                                                 juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f));
    // v0.46 fix22 : pavé vectoriel — X et Y s'automatisent dans le logiciel hôte (sur ordinateur)
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ParamID::vecOn, 1 }, "Vecteur", 0, 1, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ParamID::vecMode, 1 }, "Vecteur mode", 0, 1, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::vecX, 1 }, "Vecteur X", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::vecY, 1 }, "Vecteur Y", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ParamID::vecOrbit, 1 }, "Vecteur mouvement", 0, 8, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ParamID::vecRate, 1 }, "Vecteur vitesse", 0, 8, 2));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::vecDepth, 1 }, "Vecteur rayon", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.35f));
    // v0.46 fix28 : rotation de la forme (les projets plus anciens la reçoivent à 0 par fillMissingParams)
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::vecRot, 1 }, "Vecteur rotation", juce::NormalisableRange<float> (0.0f, 360.0f, 0.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::vecHeight, 1 }, "Vecteur hauteur", juce::NormalisableRange<float> (0.05f, 2.0f, 0.0f), 1.0f));
    for (int k = 0; k < 4; ++k)
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ParamID::vecScene (k), 1 }, "Vecteur scene " + juce::String::charToString ((juce::juce_wchar) ('A' + k)), 1, numScenes, k + 1));
    for (int k = 0; k < 4; ++k)
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ParamID::vecFx (k), 1 }, "Vecteur effet " + juce::String::charToString ((juce::juce_wchar) ('A' + k)), 0, 4, k + 1));
    return layout;
}

GrooveboxProcessor::GrooveboxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Entree", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Sortie", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "GROOVEBOX", createLayout())
{
    formats.registerBasicFormats();
    pBpm = state.getRawParameterValue (ParamID::bpm);
    pSwing = state.getRawParameterValue (ParamID::swing);
    pXfade = state.getRawParameterValue (ParamID::xfade);
    pSceneA = state.getRawParameterValue (ParamID::sceneA);
    pSceneB = state.getRawParameterValue (ParamID::sceneB);
    pVecOn = state.getRawParameterValue (ParamID::vecOn); pVecMode = state.getRawParameterValue (ParamID::vecMode);
    pVecX = state.getRawParameterValue (ParamID::vecX); pVecY = state.getRawParameterValue (ParamID::vecY);
    pVecOrbit = state.getRawParameterValue (ParamID::vecOrbit); pVecRate = state.getRawParameterValue (ParamID::vecRate);
    pVecDepth = state.getRawParameterValue (ParamID::vecDepth); pVecRot = state.getRawParameterValue (ParamID::vecRot); pVecHeight = state.getRawParameterValue (ParamID::vecHeight);
    for (int k = 0; k < 4; ++k) { pVecScene[(size_t) k] = state.getRawParameterValue (ParamID::vecScene (k)); pVecFx[(size_t) k] = state.getRawParameterValue (ParamID::vecFx (k)); }
    for (int h = 0; h < numHostCtl; ++h) hostParams[(size_t) h] = state.getParameter (hostCtlId (h)); // v0.46 fix24
    jassert (hostCtlId (numHostCtl - 1).isNotEmpty() && hostCtlId (numHostCtl).isEmpty());
    for (int m = 0; m < numMacros; ++m)
    {
        pMacro[(size_t) m] = state.getRawParameterValue (ParamID::macro (m));
        for (auto& l : macroSlots[(size_t) m]) l.global.store (-1);
        lastMacro[(size_t) m].store (-1.0f);
        macroRel[(size_t) m].store (true); // v0.46 : relatif par défaut
    }
    currentBpm.store (pBpm->load());
    for (auto& m : ccMap) m.store (midiNone);
    // v0.46 fix24 : boutons assignables libres, rien en attente
    for (auto& k : knobTarget) k.store (midiNone);
    for (auto& k : knobPending) k.store (-1.0f);
    for (auto& k : knobOut) k.store (-1.0f);
    for (auto& h : hostPending) h.store (-1.0f);
    for (auto& m : noteMap) m.store (midiNone);
    for (auto& r : rowScene) r.store (-1);
    grv::startupMark ("moteur : creation");
    engine.prepare (48000.0, 512);
    engine.audioTap.store (&recorder); // v0.32 : la prise ne fait rien tant qu'elle n'est pas armée
    engine.montageFeed.store (&montagePlayer); // v0.46 fix8 : ne joue rien tant que le mode montage est éteint
    loadDefaultProject();
    grv::startupMark ("moteur : projet de depart charge");
   #if JUCE_ANDROID
    constructedAt = nowMs();
    grv::crashguard::install();
    if (grv::crashguard::safeStart())
    {
        // v0.49 (essai 3) : le lancement précédent a planté : séance gardée de côté, pas rouverte
        // (ni l'enregistrement automatique, ni l'état gardé par l'application : voir setStateInformation)
        if (auto f = autosaveFile(); f.existsAsFile())
            f.moveFileTo (f.getSiblingFile ("autosave-apres-plantage.gbx"));
        restoredFromAutosave = true;
        grv::startupMark ("lancement precedent interrompu : seance gardee de cote, non rouverte");
    }
    else if (auto f = autosaveFile(); f.existsAsFile())
    {
        grv::startupMark ("derniere seance (enregistrement auto) : lecture");
        if (auto in = f.createInputStream())
            restoredFromAutosave = readProjectFile (*in).isEmpty();
        grv::startupMark ("derniere seance (enregistrement auto) : relue");
    }
   #endif
    wifi = std::make_unique<grv::WifiServer> (remoteId); // 1.1 : télécommande Wi-Fi (écoute coupée tant qu'on ne la demande pas)
    startTimer (40);
}

// 1.1 : TÉLÉCOMMANDE — retour du PC vers le téléphone (application autonome seulement). Le téléphone dit
// « bonjour » ; l'application note l'entrée MIDI d'où il vient (StandaloneHook.cpp) ; on ouvre la sortie
// MIDI du même nom et on renvoie, toutes les 80 ms, ce qui a changé : volumes, coupures, solos, boutons
// assignables, crossfader, pavé vectoriel, lecture ; les noms des pistes toutes les 0,8 s.
void GrooveboxProcessor::remoteFeedbackTick()
{
    if (wrapperType != wrapperType_Standalone) return;
    const auto& hook = grv::standaloneInputHook();
    if (! hook) return;
    if (! remoteListening) { remoteListening = true; hook (20); }
    auto& box = grv::remoteHello();
    const auto seq = box.seq.load();
    const auto full = box.full.exchange (false);
    if (seq != remoteHelloSeen)
    {
        remoteHelloSeen = seq;
        const auto name = box.read();
        if (remoteBack == nullptr || name != remoteBackName)
        {
            remoteBack.reset();
            remoteBackName = name;
            const auto outs = juce::MidiOutput::getAvailableDevices();
            juce::String id;
            for (const auto& d : outs) if (d.name == name) { id = d.identifier; break; }
            if (id.isEmpty()) for (const auto& d : outs) if (name.isNotEmpty() && (d.name.containsIgnoreCase (name) || name.containsIgnoreCase (d.name))) { id = d.identifier; break; }
            if (id.isNotEmpty()) remoteBack = juce::MidiOutput::openDevice (id);
            remoteSent.clear();
            for (auto& n : remoteNames) n.clear();
            for (auto& n : remoteKnobLabels) n.clear(); for (auto& n : remotePresets) n.clear();
            remoteLevels.fill (255);
        }
    }
    if (full) { remoteSent.clear(); for (auto& n : remoteNames) n.clear(); for (auto& n : remoteKnobLabels) n.clear(); for (auto& n : remotePresets) n.clear(); remoteLevels.fill (255); }
    // Wi-Fi : nouvelle télécommande ou demande de tout l'état
    const bool wifiClient = wifi != nullptr && wifi->hasClient();
    if (wifi != nullptr)
    {
        const auto ws = wifi->helloSeq.load();
        if (wifi->fullRequest.exchange (false) || (ws != wifiHelloSeen && remoteSent.empty())) { remoteSent.clear(); for (auto& n : remoteNames) n.clear(); for (auto& n : remoteKnobLabels) n.clear(); for (auto& n : remotePresets) n.clear(); remoteLevels.fill (255); }
        wifiHelloSeen = ws;
    }
    if (remoteBack == nullptr && ! wifiClient) return;
    // 1.1 : position jouée du pavé (avec le mouvement) à chaque passage (40 ms) : l'orbe de la télécommande
    // suit la forme comme sur le PC
    {
        if (remoteSent.empty()) remoteVecShown = { -1, -1 }; // tout l'état est redemandé
        const int sx = (int) std::lround (juce::jlimit (0.0f, 1.0f, getVecShownX()) * 16383.0f);
        const int sy = (int) std::lround (juce::jlimit (0.0f, 1.0f, getVecShownY()) * 16383.0f);
        if (sx != remoteVecShown[0] || sy != remoteVecShown[1])
        {
            remoteVecShown = { sx, sy };
            std::vector<uint8_t> m { remote::manufacturer, remote::id0, remote::id1, remote::version, remote::cmdState };
            uint8_t g[10];
            remote::putTarget (g, remote::vecShownX, getVecShownX());
            remote::putTarget (g + 5, remote::vecShownY, getVecShownY());
            m.insert (m.end(), g, g + 10);
            remoteSendAll (m);
        }
    }
    if ((++remoteTicks & 1) != 0) return;

    // cibles suivies (mêmes numéros que l'apprentissage MIDI) et leur valeur actuelle
    std::vector<std::pair<int, float>> now;
    now.reserve (96);
    auto& seq_ = engine.sequencer();
    for (int t = 0; t < engine::numTracks; ++t)
    {
        now.emplace_back (engine::globalSlot (t, engine::mixerBase), engine.getParam (engine::globalSlot (t, engine::mixerBase)));
        now.emplace_back (midiMuteBase - t, seq_.getTrackMute (t) ? 1.0f : 0.0f);
        now.emplace_back (midiSoloBase - t, seq_.getTrackSolo (t) ? 1.0f : 0.0f);
    }
    now.emplace_back (engine::globalSlot (engine::masterChannel, engine::mixerBase), engine.getParam (engine::globalSlot (engine::masterChannel, engine::mixerBase)));
    // retours A-D et groupes : volume, coupure (et solo des groupes)
    for (int ch = engine::numTracks; ch < engine::numChannels; ++ch)
    {
        if (ch == engine::masterChannel) continue;
        now.emplace_back (engine::globalSlot (ch, engine::mixerBase), engine.getParam (engine::globalSlot (ch, engine::mixerBase)));
        now.emplace_back (midiBusMuteBase - (ch - engine::numTracks), engine.getBusMute (ch) ? 1.0f : 0.0f);
        if (engine::isGroupChannel (ch)) now.emplace_back (midiBusSoloBase - (ch - engine::numTracks), engine.getBusSolo (ch) ? 1.0f : 0.0f);
    }
    for (int k = 0; k < numKnobs; ++k) now.emplace_back (midiKnobBase - k, knobVal[(size_t) k].load());
    for (const int h : { 4, 5, 6, 7, 8, 9, 10, 19, 20, 21, 22, 27, 28 }) // 1.1 : + durée, taille, rotation, hauteur du mouvement
        if (auto* p = hostParams[(size_t) h]) now.emplace_back (midiHostBase - h, p->getValue());
    if (auto* x = state.getParameter (ParamID::xfade)) now.emplace_back ((int) midiXfade, x->getValue());
    now.emplace_back ((int) midiPlay, seq_.isRunning() ? 1.0f : 0.0f);

    if (remoteSent.size() != now.size()) remoteSent.assign (now.size(), -1);
    std::vector<uint8_t> msg { remote::manufacturer, remote::id0, remote::id1, remote::version, remote::cmdState };
    auto flush = [&]
    {
        if (msg.size() > 5) remoteSendAll (msg);
        msg.resize (5);
    };
    for (size_t i = 0; i < now.size(); ++i)
    {
        const auto v14 = (int) std::lround (juce::jlimit (0.0f, 1.0f, now[i].second) * 16383.0f);
        if (v14 == remoteSent[i]) continue;
        remoteSent[i] = v14;
        uint8_t g[5];
        remote::putTarget (g, now[i].first, now[i].second);
        msg.insert (msg.end(), g, g + 5);
        if (msg.size() >= 5 + 48 * 5) flush();
    }
    flush();

    // niveaux (crêtes) des 16 pistes et du principal, 0..127 (racine : lecture plus parlante)
    {
        std::vector<uint8_t> lv { remote::manufacturer, remote::id0, remote::id1, remote::version, remote::cmdLevels };
        bool changed = false;
        // ordre : 16 pistes, principal, puis retours A-D et groupes 1-7 (canaux 16-19 et 21-27)
        for (int i = 0; i < engine::numChannels; ++i)
        {
            const auto ch = i < engine::numTracks ? i : i == engine::numTracks ? engine::masterChannel : (i <= engine::masterChannel ? i - 1 : i);
            const auto v = (uint8_t) juce::jlimit (0, 127, (int) std::lround (std::sqrt (juce::jlimit (0.0f, 1.0f, engine.getPeak (ch))) * 127.0f));
            if (v != remoteLevels[(size_t) i]) { remoteLevels[(size_t) i] = v; changed = true; }
            lv.push_back (v);
        }
        if (changed) remoteSendAll (lv);
    }
    auto nibbles = [] (std::vector<uint8_t>& m, const std::string& txt)
    {
        for (size_t i = 0; i < txt.size() && i < 40; ++i) { const auto c = (uint8_t) txt[i]; m.push_back ((uint8_t) (c >> 4)); m.push_back ((uint8_t) (c & 15)); }
    };
    if (remoteTicks % 10 == 0 || remoteNames[0].empty())
    {
        // noms et couleurs des pistes
        for (int t = 0; t < engine::numTracks; ++t)
        {
            const auto& info = engine.getTrackInfo (t);
            const auto key = (info.name.empty() ? std::string (" ") : info.name) + "#" + std::to_string (info.color) + "#" + info.instrumentType;
            if (key == remoteNames[(size_t) t]) continue;
            remoteNames[(size_t) t] = key;
            std::vector<uint8_t> m { remote::manufacturer, remote::id0, remote::id1, remote::version, remote::cmdName, (uint8_t) t,
                                     (uint8_t) (info.instrumentType.empty() ? 127 : juce::jlimit (0, 126, info.color)) };
            nibbles (m, info.name);
            remoteSendAll (m);
        }
        // noms des groupes (canal 21 à 27 ; vide = « Groupe n » sur le téléphone)
        for (int g = 0; g < engine::numGroups; ++g)
        {
            const auto ch = engine::groupBase + g;
            const auto& n = engine.getGroupName (g);
            const auto key = "G" + n;
            if (key == remoteNames[(size_t) ch]) continue;
            remoteNames[(size_t) ch] = key;
            std::vector<uint8_t> m { remote::manufacturer, remote::id0, remote::id1, remote::version, remote::cmdName, (uint8_t) ch, 127 };
            nibbles (m, n);
            remoteSendAll (m);
        }
        // 1.1 : préréglage de chaque piste (instrument puis 4 inserts) avec son numéro d'usine, pour que la
        // télécommande affiche « Son 12 · nom » et suive les changements faits sur le PC.
        // [cmdPreset, piste, canal MIDI de la piste (0 = aucun), 1 si piste choisie, canal des notes (0 = tous),
        //  commandes MIDI actives (0/1), canal des commandes (0 = tous) — un changement de programme y lance une scène,
        //  puis 5 fois : numéro sur 14 bits (2 octets, sons d'usine puis ceux de l'utilisateur, 16383 = aucun),
        //  nombre de sons sur 14 bits, nom en demi-octets, 0x7F, type du module, 0x7F]
        {
            const auto focusT = juce::jlimit (0, engine::numTracks - 1, engine.focusTrack.load (std::memory_order_relaxed));
            const auto noteChan = juce::jlimit (0, 16, midiNoteChan.load (std::memory_order_relaxed));
            const auto ctrlChan = juce::jlimit (0, 16, midiCtrlChan.load (std::memory_order_relaxed));
            const auto ctrlActive = midiCtrlOn.load (std::memory_order_relaxed);
            for (int t = 0; t < engine::numTracks; ++t)
            {
                const auto& info = engine.getTrackInfo (t);
                const auto ch = juce::jlimit (0, 16, trackMidiChan[(size_t) t].load (std::memory_order_relaxed));
                std::vector<uint8_t> m { remote::manufacturer, remote::id0, remote::id1, remote::version, remote::cmdPreset, (uint8_t) t,
                                         (uint8_t) ch, (uint8_t) (t == focusT ? 1 : 0), (uint8_t) noteChan,
                                         (uint8_t) (ctrlActive ? 1 : 0), (uint8_t) ctrlChan };
                // clé bon marché d'abord : on ne cherche les numéros (et la liste des sons de l'utilisateur,
                // sur le disque) que si quelque chose a changé
                std::string key (m.begin() + 5, m.end());
                for (int s = -1; s < engine::numInserts; ++s)
                    key += "|" + (s < 0 ? info.instrumentType + "#" + info.preset : info.insertTypes[(size_t) s] + "#" + engine.getInsertPresets (t)[(size_t) s]);
                if (key == remotePresets[(size_t) t]) continue;
                remotePresets[(size_t) t] = key;
                for (int s = -1; s < engine::numInserts; ++s)
                {
                    const auto* d = s < 0 ? engine.instrumentDesc (t) : engine.effectDesc (t, s);
                    const std::string name = s < 0 ? info.preset : engine.getInsertPresets (t)[(size_t) s];
                    int prog = d != nullptr ? engine::presetProgramOf (*d, name) : -1;
                    int total = 0;
                    if (d != nullptr)
                    {
                        const auto users = UserPresets::list (juce::String (d->type));
                        total = (int) (d->presets.size() + users.size());
                        if (prog < 0 && ! name.empty())
                            for (size_t i = 0; i < users.size(); ++i)
                                if (users[i].name.toStdString() == name) { prog = (int) (d->presets.size() + i); break; }
                    }
                    if (prog < 0 || prog >= 16383) prog = 16383; // aucun
                    total = juce::jlimit (0, 16383, total);
                    m.push_back ((uint8_t) (prog >> 7)); m.push_back ((uint8_t) (prog & 127));
                    m.push_back ((uint8_t) (total >> 7)); m.push_back ((uint8_t) (total & 127));
                    nibbles (m, name);
                    m.push_back (0x7F);
                    nibbles (m, s < 0 ? info.instrumentType : info.insertTypes[(size_t) s]); // type du module : liste des sons sur le téléphone
                    m.push_back (0x7F);
                }
                remoteSendAll (m);
            }
        }
        // libellés des 16 boutons assignables (ce qu'ils pilotent sur le PC)
        for (int k = 0; k < numKnobs; ++k)
        {
            juce::String sub; int color = 127;
            const auto label = remoteTargetLabel (getKnobTarget (k), sub, color);
            const auto key = (label + "|" + sub + "|" + juce::String (color)).toStdString();
            if (key == remoteKnobLabels[(size_t) k]) continue;
            remoteKnobLabels[(size_t) k] = key;
            std::vector<uint8_t> m { remote::manufacturer, remote::id0, remote::id1, remote::version, remote::cmdKnob, (uint8_t) k, (uint8_t) color };
            nibbles (m, label.toStdString());
            m.push_back (0x7F);
            nibbles (m, sub.toStdString());
            remoteSendAll (m);
        }
    }
}

juce::String GrooveboxProcessor::remoteTargetLabel (int t, juce::String& sub, int& color) const
{
    color = 127;
    sub = "Global";
    if (t == midiNone) { sub = juce::String::fromUTF8 ("\xC3\xA0 affecter"); return juce::String::fromUTF8 ("\xE2\x80\x94"); }
    if (t >= 0 && t < engine::numSlots)
    {
        const auto ch = engine::channelOf (t), local = engine::localOf (t);
        if (ch < engine::numTracks)
        {
            const auto& info = engine.getTrackInfo (ch);
            sub = "P" + juce::String (ch + 1) + " " + juce::String::fromUTF8 (info.name.c_str());
            color = juce::jlimit (0, 126, info.color);
        }
        else if (ch < engine::masterChannel) sub = "Retour " + juce::String::charToString ((juce::juce_wchar) ('A' + ch - engine::numTracks));
        else if (ch == engine::masterChannel) sub = "Principal";
        else sub = "Groupe " + juce::String (ch - engine::groupBase + 1);
        if (local < engine::instrumentBase)
        {
            static const char* const mix[] { "Volume", "Panoramique", "Envoi A", "Envoi B", "Envoi C", "Envoi D" };
            return local < 6 ? juce::String (mix[local]) : juce::String ("Mixage");
        }
        if (local < engine::insertBase)
        {
            if (ch < engine::numTracks)
                if (const auto* d = engine::findInstrument (engine.getTrackInfo (ch).instrumentType))
                    if (local - engine::instrumentBase < (int) d->params.size()) return juce::String::fromUTF8 (d->params[(size_t) (local - engine::instrumentBase)].name.c_str());
            return "Instrument";
        }
        if (local < engine::modSourceBase)
        {
            const auto k = (local - engine::insertBase) / engine::insertSlots, i = (local - engine::insertBase) % engine::insertSlots;
            if (const auto* d = engine::findEffect (engine.getInsertTypes (ch)[(size_t) k]))
                if (i < (int) d->params.size()) return juce::String::fromUTF8 (d->params[(size_t) i].name.c_str());
            return "Effet " + juce::String (k + 1);
        }
        return "Modulation";
    }
    if (isHostTarget (t))
    {
        static const char* const host[] { "Tempo", "Swing", "Sc\xC3\xA8ne A", "Sc\xC3\xA8ne B", "Pav\xC3\xA9", "Pav\xC3\xA9 : mode", "Pav\xC3\xA9 X", "Pav\xC3\xA9 Y",
                                          "Orbite", "Orbite : vitesse", "Orbite : taille", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6",
                                          "Macro 7", "Macro 8", "Coin A", "Coin B", "Coin C", "Coin D", "Effet A", "Effet B", "Effet C", "Effet D", "Rotation", "Hauteur" };
        const auto h = midiHostBase - t;
        return h >= 0 && h < (int) std::size (host) ? juce::String::fromUTF8 (host[h]) : juce::String ("Global");
    }
    if (t <= midiMuteBase && t > midiMuteBase - engine::numTracks) { sub = "P" + juce::String (midiMuteBase - t + 1); return "Coupure"; }
    if (t <= midiSoloBase && t > midiSoloBase - engine::numTracks) { sub = "P" + juce::String (midiSoloBase - t + 1); return "Solo"; }
    if (t == midiXfade) return "Crossfader";
    if (t == midiFill) return "Fill";
    if (t == midiBreak) return "Break";
    if (t == midiPlay) return "Lecture";
    return juce::String::fromUTF8 ("R\xC3\xA9glage");
}

void GrooveboxProcessor::presetRequestsTick()
{
    for (int t = 0; t < engine::numTracks; ++t)
    {
        const auto v = pendingPreset[(size_t) t].exchange (0, std::memory_order_relaxed) - 1;
        if (v < 0) continue;
        // [cible + 1] << 16 | numéro (0 = n° 1) ; cible : -1 instrument, 0..3 inserts
        const auto slot = (v >> 16) - 1, prog = v & 0xFFFF;
        if (slot < -1 || slot >= engine::numInserts) continue;
        const auto* d = slot < 0 ? engine.instrumentDesc (t) : engine.effectDesc (t, slot);
        if (d == nullptr) continue;
        // numéros : « Init » = 1, sons d'usine banque par banque, puis les sons de l'utilisateur
        const auto order = engine::presetProgramOrder (*d);
        if (prog < (int) order.size()) { engine.applyPreset (t, slot, d->presets[order[(size_t) prog]].name); continue; }
        const auto users = UserPresets::list (juce::String (d->type));
        const auto k = (size_t) (prog - (int) order.size());
        if (k >= users.size()) continue;
        UserPresets::Values values;
        juce::String kind;
        int fileVersion = 1;
        if (! UserPresets::load (juce::String (d->type), users[k].bank, users[k].name, values, kind, &fileVersion)) continue;
        if (fileVersion < 3) // préréglages d'avant la version 3 : divisions au tempo décalées de deux choix
            for (auto& [id, val] : values)
                for (const auto& pd : d->params)
                    if (pd.id == id && engine::hasLongDivisions (pd)) { val = (float) engine::widenDivisionIndex (pd, (int) std::lround (val)); break; }
        engine.applyValues (t, slot, values, {}, users[k].name.toStdString());
    }
}

bool GrooveboxProcessor::presetByNumber (const ModuleDesc& d, int prog, std::vector<std::pair<std::string, float>>& values, juce::String& name)
{
    values.clear();
    if (prog < 0) return false;
    const auto order = engine::presetProgramOrder (d);
    if (prog < (int) order.size())
    {
        const auto& pr = d.presets[order[(size_t) prog]];
        for (const auto& [id, v] : pr.values) values.emplace_back (id, v);
        name = juce::String::fromUTF8 (pr.name.c_str());
        return true;
    }
    const auto users = UserPresets::list (juce::String (d.type));
    const auto k = (size_t) (prog - (int) order.size());
    if (k >= users.size()) return false;
    UserPresets::Values uv;
    juce::String kind;
    int fileVersion = 1;
    if (! UserPresets::load (juce::String (d.type), users[k].bank, users[k].name, uv, kind, &fileVersion)) return false;
    if (fileVersion < 3)
        for (auto& [id, val] : uv)
            for (const auto& pd : d.params)
                if (pd.id == id && engine::hasLongDivisions (pd)) { val = (float) engine::widenDivisionIndex (pd, (int) std::lround (val)); break; }
    for (const auto& [id, v] : uv) values.emplace_back (id, v);
    name = users[k].name;
    return true;
}

void GrooveboxProcessor::soundPoolTick()
{
    // après la 1.1 : numéros de son verrouillés (pas, scènes) → réglages complets publiés pour le fil audio
    const auto now = nowMs();
    if (now - lastSoundPoolMs < 250.0) return;
    lastSoundPoolMs = now;
    std::array<std::vector<int>, numTracks> want;
    auto note = [&want] (int g, float v)
    {
        const auto ch = channelOf (g);
        if (ch < 0 || ch >= numTracks || localOf (g) != trackSoundSlot) return;
        if (const int n = (int) std::lround (v * trackSoundMax); n > 0) want[(size_t) ch].push_back (n);
    };
    for (const auto& [key, x] : engine.getExtras().steps) for (const auto& [g, v] : x.locks) note (g, v);
    for (const auto& sc : engine.getScenes().scenes) for (const auto& l : sc) note (l.slot, l.norm);
    std::string sig = std::to_string (userPresetGen.load());
    for (int t = 0; t < numTracks; ++t)
    {
        auto& w = want[(size_t) t];
        std::sort (w.begin(), w.end());
        w.erase (std::unique (w.begin(), w.end()), w.end());
        const auto* d = engine.instrumentDesc (t);
        if (d == nullptr || w.empty()) continue;
        sig += "|" + std::to_string (t) + ":" + d->type;
        for (const auto n : w) sig += "," + std::to_string (n);
    }
    if (sig == lastSoundSig) return;
    lastSoundSig = sig;
    auto pool = std::make_unique<SoundPool>();
    for (int t = 0; t < numTracks; ++t)
    {
        const auto* d = engine.instrumentDesc (t);
        if (d == nullptr || d->type == "octa" || d->type == "midiext") continue; // Pieuvre : emplacements ; piste MIDI : programme
        for (const auto n : want[(size_t) t])
        {
            std::vector<std::pair<std::string, float>> vals;
            juce::String name;
            if (! presetByNumber (*d, n - 1, vals, name)) continue;
            SoundPool::Entry e;
            e.track = t; e.number = n; e.desc = d;
            // un préréglage part des réglages d'origine ; les réglages de branchement restent ceux de la piste
            for (size_t i = 0; i < d->params.size() && (int) i < instrumentSlots; ++i)
            {
                const auto& p = d->params[i];
                if (p.keepOnPreset || p.fixed) continue;
                auto v = p.def;
                for (const auto& [id, value] : vals)
                    if (id == p.id && std::isfinite (value)) { v = std::clamp (value, std::min (p.min, p.max), std::max (p.min, p.max)); break; }
                e.values.emplace_back ((int) i, p.toNorm (v));
            }
            pool->entries.push_back (std::move (e));
        }
    }
    engine.publishSoundPool (std::move (pool), now);
}

void GrooveboxProcessor::remoteSendAll (const std::vector<uint8_t>& m)
{
    if (remoteBack != nullptr) remoteBack->sendMessageNow (juce::MidiMessage::createSysExMessage (m.data(), (int) m.size()));
    if (wifi != nullptr) wifi->send (m);
}

bool GrooveboxProcessor::setWifiRemote (bool on)
{
    // l'objet vit autant que le processeur (le fil audio le lit) : on n'arrête que l'écoute
    if (wrapperType != wrapperType_Standalone || wifi == nullptr) return false;
    if (! on) { wifi->stop(); return false; }
    return wifi->start();
}

GrooveboxProcessor::~GrooveboxProcessor()
{
    stopTimer();
    proc->cancel.store (true);                       // 1.1 : traitement hors temps réel en cours
    delete monitorChain.exchange (nullptr);          // 1.1 : écoute du traitement (le fil audio est arrêté)
    if (procThread.joinable()) procThread.join();
    engine.setAudition (nullptr);
    remoteBack.reset();
    wifi.reset();
    recorder.stop();
    engine.audioTap.store (nullptr);
    engine.montageFeed.store (nullptr);
    if (montageJob != nullptr) montageJob->stopThread (10000);
    montagePlayer.stopReading();
   #if JUCE_ANDROID
    // laisse finir un enregistrement automatique en cours (5 s au plus)
    for (int i = 0; i < 500 && autosaveBusy->load(); ++i) juce::Thread::sleep (10);
   #endif
}

#if JUCE_ANDROID
#endif

// v0.46 fix22 : forme dessinée du pavé vectoriel — « x,y;x,y;… » (0..1), 128 points au plus ;
// gardée dans le projet (propriété « vecPath » de l'état) ; le fil audio lit l'autre moitié du
// double tampon (aucun verrou). Fil des messages.
void GrooveboxProcessor::setVecPath (const juce::String& text, bool store)
{
    const auto next = (vecPathIndex.load() + 1) & 1;
    auto& p = vecPaths[(size_t) next];
    p.count = 0;
    for (const auto& pt : juce::StringArray::fromTokens (text, ";", ""))
    {
        if (p.count >= (int) p.x.size()) break;
        const auto xy = juce::StringArray::fromTokens (pt, ",", "");
        if (xy.size() != 2) continue;
        p.x[(size_t) p.count] = juce::jlimit (0.0f, 1.0f, xy[0].getFloatValue());
        p.y[(size_t) p.count] = juce::jlimit (0.0f, 1.0f, xy[1].getFloatValue());
        ++p.count;
    }
    vecPathIndex.store (next, std::memory_order_release);
    if (store) state.state.setProperty ("vecPath", text, nullptr);
}

juce::String GrooveboxProcessor::getVecPath() const
{
    return state.state.getProperty ("vecPath").toString();
}

void GrooveboxProcessor::noteUserActivity() noexcept
{
   #if JUCE_ANDROID
    lastUserActivity = nowMs();
   #endif
}

#if JUCE_ANDROID
juce::File GrooveboxProcessor::autosaveFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Groovebox").getChildFile ("autosave.gbx");
}

void GrooveboxProcessor::autosaveIfChanged()
{
    const auto stamp = changeStamp();
    if (stamp == lastAutosaveStamp || autosaveBusy->load()) return;
    lastAutosaveStamp = stamp;

    // Le projet est mis en texte ici (fil de l'interface) ; l'écriture se fait sur un fil à part,
    // dans un fichier temporaire qui ne remplace l'ancien que si tout a été écrit.
    auto tree = state.copyState();
    tree.removeChild (tree.getChildWithName ("PROJECT"), nullptr);
    tree.appendChild (projectToTree(), nullptr);
    auto xml = tree.createXml();
    if (xml == nullptr) return;
    auto text = std::make_shared<juce::String> (xml->toString());
    const auto f = autosaveFile();
    auto busy = autosaveBusy;
    busy->store (true);
    juce::Thread::launch ([text, f, busy]
    {
        f.getParentDirectory().createDirectory();
        const auto tmp = f.getSiblingFile ("autosave.tmp");
        bool ok = false;
        {
            juce::FileOutputStream out (tmp);
            if (out.openedOk())
            {
                out.setPosition (0); out.truncate();
                out.writeText (*text, false, false, nullptr);
                out.flush();
                ok = out.getStatus().wasOk();
            }
        }
        if (ok) tmp.moveFileTo (f); else tmp.deleteFile();
        busy->store (false);
    });
}
#endif

std::uint64_t GrooveboxProcessor::changeStamp() const
{
    std::uint64_t stamp = projectVersion.load() + ((std::uint64_t) engine.sequencer().getVersion() << 20) + ((std::uint64_t) engine.getStructureVersion() << 40)
                        + ((std::uint64_t) midiVersion.load() << 8) + ((std::uint64_t) rowVersion.load() << 28);
    for (int ch = 0; ch < numChannels; ++ch) stamp += (std::uint64_t) engine.getParamVersion (ch) * (std::uint64_t) (ch + 7);
    stamp += (std::uint64_t) engine.getSceneVersion() * 0x9E3779B97F4A7C15ull;
    stamp += (std::uint64_t) engine.getSongVersion() * 0xC2B2AE3D27D4EB4Full;
    // noms, couleurs, préréglages affichés et macros ne passent pas par un compteur : empreinte
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&h] (std::uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    auto mixText = [&mix] (const std::string& text) { for (const auto c : text) mix ((std::uint8_t) c); mix (0xFF); };
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& info = engine.getTrackInfo (t);
        mixText (info.name); mixText (info.preset); mixText (info.speech); mixText (info.labels); mix ((std::uint64_t) info.color); mix ((std::uint64_t) engine.getTrackOutput (t) + 977u);
    }
    for (int ch = 0; ch < numChannels; ++ch)
        for (const auto& name : engine.getInsertPresets (ch)) mixText (name);
    for (int m = 0; m < numMacros; ++m)
        for (const auto& l : getMacroLinks (m))
            mix ((std::uint64_t) (l.global + 1) * 0x10000ull + (std::uint64_t) std::lround (l.lo * 1000.0f) * 0x100ull + (std::uint64_t) std::lround (l.hi * 1000.0f));
    for (int m = 0; m < numMacros; ++m) mix (macroRel[(size_t) m].load() ? 0xA5ull + (std::uint64_t) m : 0x5Aull);
    return stamp ^ h;
}

void GrooveboxProcessor::timerCallback()
{
   #if JUCE_ANDROID
    // v0.49 (essai 3) : lancement allé au bout (12 s sans plantage)
    if (! launchOk && nowMs() - constructedAt > 12000.0) { launchOk = true; grv::crashguard::launchConfirmed(); grv::startupMark ("lancement confirme (12 s)"); }
   #endif
    // v0.49 : greffon — retard compensé (FM Player) annoncé au logiciel hôte, qui recale GOLEM sur le temps
    if (wrapperType != wrapperType_Standalone)
        if (const auto c = engine.compensationSamples(); c != getLatencySamples()) setLatencySamples (c);
    engine.collectGarbage (nowMs());
    engine.songTick (nowMs());                          // morceau : mesures et mouvements enregistrés
    if (const auto sc = engine.takeSongScene(); sc >= 0) launchScene (sc, false);
    keepVoiceCaptures(); // v0.33 : prises de son des vocodeurs gardées avec le projet
    montageTick();       // v0.46 fix8 : prise du montage rangée, export terminé
    audioRecTick();      // v0.46.2 : prise calée finie d'elle-même
    historyTick();
    remoteFeedbackTick(); // 1.1 : retour vers la télécommande
    presetRequestsTick(); // 1.1 : préréglages appelés par changement de programme
    soundPoolTick();      // après la 1.1 : sons du pas
   #if JUCE_ANDROID
    perfHint.service();
    {
        if (lastUserActivity <= 0.0) lastUserActivity = nowMs();
        const bool keep = engine.sequencer().isRunning() || recorder.isRecording() || nowMs() - lastUserActivity < 10.0 * 60.0 * 1000.0;
        if (keep != screenKeptOn) { screenKeptOn = keep; androidKeepScreenOn (keep); }
    }
    // v0.46 fix16 : une seconde après le démarrage, le micro est fermé (pas de flux d'entrée)
    if (inputStartTicks >= 0 && ++inputStartTicks >= 25)
    {
        inputStartTicks = -1;
        if (const auto& hook = standaloneInputHook()) hook (2);
    }
    if (++autosaveTicks >= 375) { autosaveTicks = 0; autosaveIfChanged(); } // 15 s
    // v0.46.2 : écran fixe (pas d'étirement au bord ni de défilement des vues Android) : 1 s, 2 s, 5 s,
    // puis toutes les 10 s (la vue web peut être recréée)
    {
        static int pinTicks = 0;
        ++pinTicks;
        if (pinTicks == 25 || pinTicks == 50 || pinTicks == 125 || pinTicks % 250 == 0) androidPinViews();
        androidResetViewScroll();
        // v0.46.4 : plus de suspension du calcul quand la fenêtre perd la main : une liste déroulante du
        // système (choix de la piste…) la fait perdre aussi, et le son se coupait en plein jeu
    }
   #endif

    // v0.46 fix24 : réglages globaux pilotés (MIDI, boutons assignables) : écrits ici, sur le fil de l'interface
    for (int h = 0; h < numHostCtl; ++h)
        if (const auto v = hostPending[(size_t) h].exchange (-1.0f); v >= 0.0f)
            if (auto* p = hostParams[(size_t) h]) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    // Crossfader piloté en MIDI : le réglage du logiciel hôte suit (affichage, sauvegarde)
    if (const auto x = xfadeFromMidi.exchange (-1.0f); x >= 0.0f)
        if (auto* p = state.getParameter (ParamID::xfade)) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, x));
    // v0.46 : crossfader relu depuis sa courbe du morceau : le réglage (et l'écran) le suivent
    if (const auto lf = engine.getLaneFader(); lf >= 0.0f)
        if (auto* p = state.getParameter (ParamID::xfade); p != nullptr && std::abs (p->getValue() - lf) > 0.002f) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, lf));

    if (const auto sc = pendingSceneLaunch.exchange (-1); sc >= 0) launchScene (sc, false);

    if (songModeToggles.exchange (0) % 2 == 1)
    {
        auto sg = engine.getSong();
        sg.songMode = ! sg.songMode;
        engine.setSong (std::move (sg), nowMs());
    }
    // v0.46 : photo et rappel demandés par le contrôleur MIDI
    if (photoRequests.exchange (0) > 0) { takePhoto(); projectVersion.fetch_add (1); }
    if (recallRequests.exchange (0) > 0 && recallPhoto()) projectVersion.fetch_add (1);
    for (auto n = playToggleRequests.exchange (0); n > 0; --n)
    {
        auto& seq = engine.sequencer();
        if (seq.isRunning()) seq.requestStop(); else seq.requestPlay();
    }

    // Rangée lancée : la scène de réglages liée part quand les patterns ont basculé
    if (const auto sc = pendingRowScene.load(); sc >= 0)
    {
        bool waiting = false;
        for (int t = 0; t < numTracks; ++t) waiting = waiting || engine.sequencer().getQueued (t) >= 0;
        if (! waiting) { pendingRowScene.store (-1); launchScene (sc, false); }
    }
}

void GrooveboxProcessor::launchRow (int row)
{
    if (row < 0 || row >= Sequencer::numPatterns) return;
    engine.sequencer().launchRow (row);
    pendingRowScene.store (rowScene[(size_t) row].load());
    lastRow.store (row, std::memory_order_relaxed);
}

//==============================================================================
// Déplacer et insérer des pistes (v0.23)

namespace
{
    std::array<juce::String, engine::numTracks> instrumentTypes (const juce::ValueTree& project)
    {
        std::array<juce::String, engine::numTracks> out;
        for (const auto& c : project)
            if (c.hasType ("TRACK"))
                if (const auto i = (int) c.getProperty ("i", -1); i >= 0 && i < engine::numTracks) out[(size_t) i] = c.getProperty ("instrument").toString();
        return out;
    }

    /** v0.42 : projet d'avant les pistes internes de Pieuvre 8 (format 17 et avant), où chaque piste
        Pieuvre était une piste du mixeur. Les 8 premières (dans l'ordre des pistes) deviennent les
        pistes internes T1 à T8 de la première : réglages renommés (« tune » → « t3_tune »),
        emplacements rangés dans la réserve (16 par piste : échantillons et attributs), séquences
        (ligne 1 de chaque piste → ligne de sa T, notes comprises), verrous de pas et de scène,
        inserts 1 et 2 → les 2 effets de la T, coupure et solo → ceux de sa ligne ; volume et
        panoramique du mixeur ajoutés à ceux de la T. Les pistes du mixeur absorbées sont libérées.
        Une 9e piste Pieuvre (et suivantes) devient une Pieuvre à elle seule (sa T1). La piste qui
        accueille garde son mixage et ses inserts 3 et 4 ; ses inserts 1 et 2 libres reçoivent
        l'égaliseur et le compresseur de sortie (compresseur contourné : le son ne change pas). */
    void mergeOctaTracks (juce::ValueTree project)
    {
        using engine::numTracks;
        for (auto c : project)
            if (c.hasType ("PHOTO")) for (auto ph : c) if (ph.hasType ("PROJECT")) mergeOctaTracks (ph);
        if ((int) project.getProperty ("version", 1) >= 18) return;
        std::vector<int> octs;
        for (const auto& c : project)
            if (c.hasType ("TRACK") && c.getProperty ("instrument").toString() == "octa")
                if (const auto i = (int) c.getProperty ("i", -1); i >= 0 && i < numTracks) octs.push_back (i);
        std::sort (octs.begin(), octs.end());
        octs.erase (std::unique (octs.begin(), octs.end()), octs.end());
        if (octs.empty()) return;
        std::array<int, numTracks> destOf, unitOf {};
        destOf.fill (-1);
        for (size_t j = 0; j < octs.size(); ++j)
        {
            destOf[(size_t) octs[j]] = j < 8 ? octs[0] : octs[j];
            unitOf[(size_t) octs[j]] = j < 8 ? (int) j : 0;
        }
        auto merged = [&] (int t) { return t >= 0 && t < numTracks && destOf[(size_t) t] >= 0; };
        auto newInstId = [] (const juce::String& id, int k) -> juce::String
        {
            if (id.length() > 1 && id[0] == 'e' && juce::CharacterFunctions::isDigit (id[1]))
            {
                int n = 0, q = 1;
                while (q < id.length() && juce::CharacterFunctions::isDigit (id[q])) n = n * 10 + (id[q++] - '0');
                if (n >= 1 && n <= 16) return "e" + juce::String (k * 16 + n) + id.substring (q);
            }
            return "t" + juce::String (k + 1) + "_" + id;
        };
        // une référence de réglage (canal, zone, identifiant) ; faux : à retirer
        auto convertRef = [&] (juce::ValueTree v, const char* key) -> bool
        {
            const auto c = (int) v.getProperty (key, -1);
            if (! merged (c)) return true;
            const auto d = destOf[(size_t) c], k = unitOf[(size_t) c];
            const auto area = v.getProperty ("a").toString(), id = v.getProperty ("i").toString();
            if (area == "inst") { v.setProperty ("i", newInstId (id, k), nullptr); v.setProperty (key, d, nullptr); return true; }
            if (area == "fx1" || area == "fx2")
            {
                v.setProperty ("a", "pfx" + juce::String (k + 1) + "_" + area.substring (2), nullptr); v.setProperty (key, d, nullptr);
                return true;
            }
            if (area == "mix" && (id == "bypass1" || id == "bypass2"))
            {
                v.setProperty ("a", "padbyp", nullptr); v.setProperty ("i", "b" + juce::String (k + 1) + "_" + id.substring (6), nullptr);
                v.setProperty (key, d, nullptr);
                return true;
            }
            return c == d; // la piste qui accueille garde son mixage, ses inserts 3 et 4 ; les autres les perdent
        };

        // 1. volume et panoramique des pistes absorbées : ajoutés au volume et à la balance de leur T
        auto params = project.getChildWithName ("PARAMS");
        auto findP = [&] (int c, const char* area, const char* id) -> juce::ValueTree
        {
            for (auto pp : params) if ((int) pp.getProperty ("c", -1) == c && pp.getProperty ("a").toString() == area && pp.getProperty ("i").toString() == id) return pp;
            return {};
        };
        auto plainOf = [&] (int c, const char* area, const char* id, const engine::ModuleDesc& d) -> float
        {
            const auto i = d.indexOf (id);
            if (i < 0) return 0.0f;
            const auto pp = findP (c, area, id);
            return pp.isValid() ? d.params[(size_t) i].toPlain ((float) (double) pp.getProperty ("v", 0.0)) : d.params[(size_t) i].def;
        };
        auto setPlain = [&] (int c, const char* area, const char* id, const engine::ModuleDesc& d, float plain)
        {
            const auto i = d.indexOf (id);
            if (i < 0 || ! params.isValid()) return;
            auto pp = findP (c, area, id);
            if (! pp.isValid()) { pp = juce::ValueTree ("P"); pp.setProperty ("c", c, nullptr); pp.setProperty ("a", area, nullptr); pp.setProperty ("i", id, nullptr); params.appendChild (pp, nullptr); }
            pp.setProperty ("v", (double) d.params[(size_t) i].toNorm (plain), nullptr);
        };
        if (params.isValid())
        {
            const auto& u = instruments::octa::unitDesc();
            for (const auto t : octs)
            {
                const auto d = destOf[(size_t) t];
                if (d == t) continue;
                const auto& mt = engine::mixerDesc (t);
                const auto& md = engine::mixerDesc (d);
                const auto vol = plainOf (t, "inst", "volume", u) + plainOf (t, "mix", "volume", mt) - plainOf (d, "mix", "volume", md);
                const auto bal = plainOf (t, "inst", "balance", u) + plainOf (t, "mix", "pan", mt) - plainOf (d, "mix", "pan", md);
                setPlain (t, "inst", "volume", u, juce::jlimit (-48.0f, 12.0f, vol));
                setPlain (t, "inst", "balance", u, juce::jlimit (-100.0f, 100.0f, bal));
            }
        }

        // 2. séquences : ligne 1 de chaque piste Pieuvre → ligne de sa T (notes, durées, décalages)
        if (auto seqT = project.getChildWithName ("SEQ"); seqT.isValid())
        {
            std::array<juce::ValueTree, numTracks> src;
            for (auto tr : seqT) if (tr.hasType ("T")) if (const auto i = (int) tr.getProperty ("i", -1); merged (i)) src[(size_t) i] = tr.createCopy();
            for (const auto d : octs)
            {
                if (destOf[(size_t) d] != d) continue; // une piste qui accueille
                juce::ValueTree out ("T");
                out.setProperty ("i", d, nullptr);
                if (src[(size_t) d].isValid())
                    for (const auto* id : { "play", "edit", "accent" })
                        if (src[(size_t) d].hasProperty (id)) out.setProperty (id, src[(size_t) d].getProperty (id), nullptr);
                std::map<int, juce::ValueTree> pats;
                std::map<int, std::array<int, 16>> lens;
                for (const auto t : octs)
                {
                    if (destOf[(size_t) t] != d) continue;
                    const auto k = unitOf[(size_t) t];
                    const auto& st = src[(size_t) t];
                    if (! st.isValid()) continue;
                    // coupure et solo de la piste, sens de sa ligne 1 → ceux de la ligne de sa T
                    juce::ValueTree laneSet ("LANE");
                    laneSet.setProperty ("l", k, nullptr);
                    for (const auto& c : st) if (c.hasType ("LANE") && (int) c.getProperty ("l", -1) == 0) laneSet.setProperty ("dir", c.getProperty ("dir", 0), nullptr);
                    bool laneMute = (bool) st.getProperty ("mute", false), laneSolo = (bool) st.getProperty ("solo", false);
                    for (const auto& c : st) if (c.hasType ("LANE") && (int) c.getProperty ("l", -1) == 0) { laneMute = laneMute || (bool) c.getProperty ("mute", false); laneSolo = laneSolo || (bool) c.getProperty ("solo", false); }
                    laneSet.setProperty ("mute", laneMute, nullptr); laneSet.setProperty ("solo", laneSolo, nullptr);
                    if (laneMute || laneSolo || (int) laneSet.getProperty ("dir", 0) != 0) { if (! laneSet.hasProperty ("dir")) laneSet.setProperty ("dir", 0, nullptr); out.appendChild (laneSet, nullptr); }
                    for (const auto& pt : st)
                    {
                        if (! pt.hasType ("PAT")) continue;
                        const auto p = (int) pt.getProperty ("p", 0);
                        auto& np = pats[p];
                        const auto len = (int) pt.getProperty ("length", 16);
                        if (! np.isValid())
                        {
                            np = juce::ValueTree ("PAT");
                            np.setProperty ("p", p, nullptr);
                            for (const auto* id : { "length", "speed", "division", "flags" }) if (pt.hasProperty (id)) np.setProperty (id, pt.getProperty (id), nullptr);
                            lens[p].fill (0);
                        }
                        np.setProperty ("length", std::max ((int) np.getProperty ("length", 16), len), nullptr);
                        // longueur propre de la ligne 1 (sinon celle du pattern)
                        int own = len;
                        juce::StringArray ll;
                        ll.addTokens (pt.getProperty ("laneLen").toString(), ",", "");
                        if (ll.size() > 0 && ll[0].getIntValue() > 0) own = ll[0].getIntValue();
                        lens[p][(size_t) k] = own;
                        for (const auto& ln : pt)
                            if (ln.hasType ("LANE") && (int) ln.getProperty ("l", -1) == 0)
                            {
                                auto c = ln.createCopy();
                                c.setProperty ("l", k, nullptr);
                                np.appendChild (c, nullptr);
                            }
                    }
                }
                for (auto& [p, np] : pats)
                {
                    const auto L = (int) np.getProperty ("length", 16);
                    juce::String txt;
                    bool any = false;
                    for (int l = 0; l < 16; ++l)
                    {
                        const auto v = lens[p][(size_t) l] > 0 && lens[p][(size_t) l] != L ? lens[p][(size_t) l] : 0;
                        txt << (l ? "," : "") << v;
                        any = any || v > 0;
                    }
                    if (any) np.setProperty ("laneLen", txt, nullptr); else np.removeProperty ("laneLen", nullptr);
                    out.appendChild (np, nullptr);
                }
                // remplace les séquences de toutes les pistes regroupées dans celle-ci
                for (int k = seqT.getNumChildren(); --k >= 0;)
                    if (const auto i = (int) seqT.getChild (k).getProperty ("i", -1); seqT.getChild (k).hasType ("T") && merged (i) && destOf[(size_t) i] == d)
                        seqT.removeChild (k, nullptr);
                seqT.appendChild (out, nullptr);
            }
        }

        // 3. tout le reste, élément par élément
        std::vector<juce::ValueTree> addPadFx;
        std::array<bool, numTracks> destInsert01 {};
        for (int n = project.getNumChildren(); --n >= 0;)
        {
            auto c = project.getChild (n);
            const auto type = c.getType().toString();
            if (type == "TRACK")
            {
                const auto i = (int) c.getProperty ("i", -1);
                if (! merged (i)) continue;
                c.setProperty ("preset", "", nullptr);
                if (destOf[(size_t) i] != i)
                {
                    c.setProperty ("instrument", "", nullptr);
                    c.setProperty ("name", "Piste " + juce::String (i + 1), nullptr);
                    c.removeProperty ("speech", nullptr);
                    c.removeProperty ("labels", nullptr);
                    c.removeProperty ("out", nullptr);
                }
            }
            else if (type == "INSERT")
            {
                const auto ch = (int) c.getProperty ("c", -1), sl = (int) c.getProperty ("s", 0);
                if (! merged (ch)) continue;
                if (sl < 2)
                {
                    juce::ValueTree pf ("PADFX");
                    pf.setProperty ("t", destOf[(size_t) ch], nullptr); pf.setProperty ("p", unitOf[(size_t) ch], nullptr); pf.setProperty ("s", sl, nullptr);
                    pf.setProperty ("type", c.getProperty ("type"), nullptr);
                    addPadFx.push_back (pf);
                    project.removeChild (n, nullptr);
                }
                else if (destOf[(size_t) ch] != ch) project.removeChild (n, nullptr);
            }
            else if (type == "MOD")
            {
                const auto ch = (int) c.getProperty ("c", -1);
                if (! merged (ch)) continue;
                if (destOf[(size_t) ch] != ch || ! convertRef (c, "c")) project.removeChild (n, nullptr);
            }
            else if (type == "SAMPLE")
            {
                const auto t = (int) c.getProperty ("t", -1);
                if (! merged (t)) continue;
                c.setProperty ("i", unitOf[(size_t) t] * 16 + juce::jlimit (0, 15, (int) c.getProperty ("i", 0)), nullptr);
                c.setProperty ("t", destOf[(size_t) t], nullptr);
            }
            else if (type == "EXTRA")
            {
                const auto t = (int) c.getProperty ("t", -1);
                if (merged (t))
                {
                    if ((int) c.getProperty ("l", 0) != 0) { project.removeChild (n, nullptr); continue; } // accords : la ligne 1 seulement
                    c.setProperty ("l", unitOf[(size_t) t], nullptr);
                    c.setProperty ("t", destOf[(size_t) t], nullptr);
                }
                for (int j = c.getNumChildren(); --j >= 0;) if (! convertRef (c.getChild (j), "c")) c.removeChild (j, nullptr);
            }
            else if (type == "PARAMS" || type == "SCENE")
            {
                for (int j = c.getNumChildren(); --j >= 0;) if (! convertRef (c.getChild (j), "c")) c.removeChild (j, nullptr);
            }
            else if (type == "MACRO" || type == "MIDI" || type == "KNOB")
            {
                if (c.hasProperty ("c") && ! convertRef (c, "c")) project.removeChild (n, nullptr);
            }
            else if (type == "SONG")
            {
                for (int j = c.getNumChildren(); --j >= 0;)
                {
                    auto l = c.getChild (j);
                    if (l.hasType ("AUTO")) { if (! l.hasProperty ("sw") && ! convertRef (l, "c")) c.removeChild (j, nullptr); }
                    else if (l.hasType ("PAUTO"))
                    {
                        const auto t = (int) l.getProperty ("t", -1);
                        if (! convertRef (l, "c")) { c.removeChild (j, nullptr); continue; }
                        if (merged (t)) l.setProperty ("t", destOf[(size_t) t], nullptr);
                    }
                }
            }
        }
        for (auto& pf : addPadFx) project.appendChild (pf, nullptr);
        // égaliseur et compresseur de sortie sur les inserts 1 et 2 libres de chaque Pieuvre
        for (const auto& c : project)
            if (c.hasType ("INSERT"))
                if (const auto ch = (int) c.getProperty ("c", -1); merged (ch) && (int) c.getProperty ("s", 0) < 2) destInsert01[(size_t) ch] = true;
        for (const auto t : octs)
        {
            if (destOf[(size_t) t] != t || destInsert01[(size_t) t]) continue;
            for (int sl = 0; sl < 2; ++sl)
            {
                juce::ValueTree ins ("INSERT");
                ins.setProperty ("c", t, nullptr); ins.setProperty ("s", sl, nullptr);
                ins.setProperty ("type", sl == 0 ? "eq" : "comp", nullptr); ins.setProperty ("preset", "", nullptr);
                project.appendChild (ins, nullptr);
            }
            setPlain (t, "mix", "bypass2", engine::mixerDesc (t), 1.0f);
        }
    }

    /** Renumérote les pistes d'un projet (ou d'une copie d'historique, ou de la photo) :
        newIndex[ancienne] = nouvelle. Tout ce qui est rangé par piste suit : instrument, nom,
        couleur, inserts, inserts de pad, réglages, modulations, verrous de scène et de pas,
        séquences, échantillons, macros, apprentissage MIDI, morceau (rangées et courbes), et les
        réglages qui désignent une piste (side-chain, source du suiveur, pistes enregistrées par le
        sampler de performance). Les retours et le bus principal ne bougent pas. */
    void renumberTracks (juce::ValueTree node, const std::array<int, engine::numTracks>& to,
                         const std::array<juce::String, engine::numTracks>& instType)
    {
        using engine::numTracks;
        auto mapTrack = [&] (int t) { return t >= 0 && t < numTracks ? to[(size_t) t] : t; };
        auto mapProp = [&] (juce::ValueTree& v, const juce::Identifier& id)
        {
            if (v.hasProperty (id)) v.setProperty (id, mapTrack ((int) v.getProperty (id)), nullptr);
        };
        // valeur rangée normalisée d'un réglage à crans : rang k parmi n
        auto remapChoice = [] (juce::ValueTree& v, int n, auto&& f)
        {
            const auto x = (double) v.getProperty ("v", 0.0);
            const auto k = juce::jlimit (0, n - 1, (int) std::floor (x * n));
            const auto k2 = f (k);
            if (k2 != k) v.setProperty ("v", ((double) k2 + 0.5) / (double) n, nullptr);
        };
        auto remapValue = [&] (juce::ValueTree& v, int oldChannel)
        {
            const auto area = v.getProperty ("a").toString(), id = v.getProperty ("i").toString();
            if (id == "scsrc" && (area.startsWith ("fx") || area.startsWith ("pfx"))) // inserts de canal et de pad (v0.24)
                remapChoice (v, numTracks + 1, [&] (int k) { return k >= 1 ? mapTrack (k - 1) + 1 : k; });
            else if (id == "fsrc" && area.startsWith ("mod"))
                remapChoice (v, 2 * numTracks + 1, [&] (int k) {
                    if (k >= 1 && k <= numTracks) return mapTrack (k - 1) + 1;
                    if (k > numTracks) return mapTrack (k - numTracks - 1) + numTracks + 1;
                    return k; });
            else if (area == "inst" && oldChannel >= 0 && oldChannel < numTracks && instType[(size_t) oldChannel] == "octa"
                     && id.length() > 5 && id[0] == 't' && id.substring (2, 5) == "_rt")
            {
                // pistes du mixeur enregistrées par une piste interne (« t3_rt5 »)
                const auto k = id.substring (5).getIntValue();
                if (k >= 1 && k <= numTracks) v.setProperty ("i", id.substring (0, 5) + juce::String (mapTrack (k - 1) + 1), nullptr);
            }
        };

        for (auto c : node)
        {
            const auto type = c.getType().toString();
            if (type == "TRACK")
            {
                const auto oldI = (int) c.getProperty ("i");
                const auto newI = mapTrack (oldI);
                c.setProperty ("i", newI, nullptr);
                // nom d'origine (« Piste 7 ») : il suit la nouvelle place
                if (c.getProperty ("name").toString() == "Piste " + juce::String (oldI + 1))
                    c.setProperty ("name", "Piste " + juce::String (newI + 1), nullptr);
            }
            else if (type == "SEQ") { for (auto tr : c) if (tr.hasType ("T")) mapProp (tr, "i"); }
            else if (type == "INSERT" || type == "MOD" || type == "MACRO" || type == "MIDI" || type == "KNOB") mapProp (c, "c");
            else if (type == "PADFX" || type == "SAMPLE") mapProp (c, "t");
            else if (type == "PARAMS")
            {
                for (auto pp : c) { const auto oldC = (int) pp.getProperty ("c", -1); remapValue (pp, oldC); mapProp (pp, "c"); }
            }
            else if (type == "SCENE" || type == "EXTRA")
            {
                if (type == "EXTRA") mapProp (c, "t");
                for (auto l : c) { const auto oldC = (int) l.getProperty ("c", -1); remapValue (l, oldC); mapProp (l, "c"); }
            }
            else if (type == "SONG")
            {
                for (auto r : c)
                {
                    if (r.hasType ("ROW"))
                    {
                        juce::StringArray pats;
                        pats.addTokens (r.getProperty ("pat").toString(), ",", "");
                        juce::StringArray out;
                        for (int t = 0; t < numTracks; ++t) out.add ("-2"); // songKeep : la piste garde ce qu'elle joue
                        for (int t = 0; t < numTracks && t < pats.size(); ++t) out.set (mapTrack (t), pats[t]);
                        r.setProperty ("pat", out.joinIntoString (","), nullptr);
                    }
                    else if (r.hasType ("AUTO")) { if (! r.hasProperty ("sw")) mapProp (r, "c"); else if (r.hasProperty ("t")) mapProp (r, "t"); } // v0.46 : coupure / solo : sa piste
                    else if (r.hasType ("PAUTO")) { mapProp (r, "t"); mapProp (r, "c"); }
                }
            }
            else if (type == "PHOTO") { for (auto ph : c) if (ph.hasType ("PROJECT")) renumberTracks (ph, to, instrumentTypes (ph)); }
        }
    }

}

void GrooveboxProcessor::prepareTrackPermutation (const std::array<int, numTracks>& newIndex)
{
    // échantillons chargés : recopiés plutôt que décodés de nouveau (voir restoreSnapshot)
    for (int t = 0; t < numTracks; ++t)
        for (int i = 0; i < maxSamples; ++i)
            if (const auto* s = engine.getSample (t, i); s != nullptr && s->original != nullptr) reuseSamples.push_back (s);
    // v0.26 : les pistes changent de place avec tout leur état (instrument, boucles en cours,
    // effets et leurs traînes, position de lecture des patterns) : rien n'est recréé ; le projet
    // renuméroté qui suit ne fait que confirmer les réglages
    engine.permuteTracks (newIndex, nowMs());
}

void GrooveboxProcessor::applyTrackPermutationExtras (const std::array<int, numTracks>& newIndex)
{
    // (patterns joués et édités : déplacés par le moteur avec les pistes, v0.26)
    // photo des réglages : renumérotée elle aussi
    {
        const juce::SpinLock::ScopedLockType lock (photoLock);
        if (photo.isValid()) { auto ph = photo.createCopy(); renumberTracks (ph, newIndex, instrumentTypes (ph)); photo = ph; }
    }
    // commandes MIDI apprises : réglages, coupure et solo d'une piste suivent leur piste
    auto remapTarget = [&] (int target)
    {
        if (target >= 0 && target < numSlots)
        {
            const auto ch = channelOf (target);
            return ch < numTracks ? globalSlot (newIndex[(size_t) ch], localOf (target)) : target;
        }
        if (target <= midiMuteBase && target > midiMuteBase - numTracks) return midiMuteBase - newIndex[(size_t) (midiMuteBase - target)];
        if (target <= midiSoloBase && target > midiSoloBase - numTracks) return midiSoloBase - newIndex[(size_t) (midiSoloBase - target)];
        return target;
    };
    for (auto* map : { &ccMap, &noteMap })
        for (auto& m : *map) m.store (remapTarget (m.load()));
    // v0.46 : le canal MIDI propre à une piste la suit
    {
        std::array<int, numTracks> ch {};
        for (int t = 0; t < numTracks; ++t) ch[(size_t) newIndex[(size_t) t]] = trackMidiChan[(size_t) t].load();
        for (int t = 0; t < numTracks; ++t) trackMidiChan[(size_t) t].store (ch[(size_t) t]);
        // GOLEM 1.0 : les canaux de ses pistes internes T aussi
        std::array<std::array<int, maxLaneChans>, numTracks> lc {};
        for (int t = 0; t < numTracks; ++t) for (int l = 0; l < maxLaneChans; ++l) lc[(size_t) newIndex[(size_t) t]][(size_t) l] = laneMidiChan[(size_t) t][(size_t) l].load();
        for (int t = 0; t < numTracks; ++t) for (int l = 0; l < maxLaneChans; ++l) laneMidiChan[(size_t) t][(size_t) l].store (lc[(size_t) t][(size_t) l]);
    }
    midiVersion.fetch_add (1);
}

juce::String GrooveboxProcessor::moveTrack (int from, int to)
{
    if (from < 0 || from >= numTracks || to < 0 || to >= numTracks) return "Piste inconnue";
    if (from == to) return {};
    commitHistory(); // ce qui précède reste une étape à part

    // nouvel ordre : la piste « from » vient se placer en « to », les autres glissent
    std::vector<int> order;
    for (int t = 0; t < numTracks; ++t) if (t != from) order.push_back (t);
    order.insert (order.begin() + to, from);
    std::array<int, numTracks> newIndex {};
    for (int k = 0; k < numTracks; ++k) newIndex[(size_t) order[(size_t) k]] = k;

    auto snap = snapshotForHistory();
    renumberTracks (snap, newIndex, instrumentTypes (snap));
    prepareTrackPermutation (newIndex);
    restoreSnapshot (snap);
    applyTrackPermutationExtras (newIndex);

    // une étape d'historique ; Ctrl+Z remet aussi le jeu, la photo et les commandes MIDI
    const auto prev = committed;
    commitHistory();
    if (! undoStack.empty() && undoStack.back() == prev)
    {
        moveSteps.push_back ({ prev, {}, newIndex });
        if (moveSteps.size() > maxHistory) moveSteps.erase (moveSteps.begin());
    }
    projectVersion.fetch_add (1);
    return {};
}

juce::String GrooveboxProcessor::insertTrack (int at)
{
    if (at < 0 || at >= numTracks) return "Piste inconnue";
    // une piste libre (sans instrument), la plus éloignée après « at » d'abord, vient s'y placer
    int free = -1;
    for (int t = numTracks - 1; t >= 0 && free < 0; --t)
        if (engine.getTrackInfo (t).instrumentType.empty() && t >= at) free = t;
    for (int t = at - 1; t >= 0 && free < 0; --t)
        if (engine.getTrackInfo (t).instrumentType.empty()) free = t;
    if (free < 0) return "Les 16 pistes sont prises : lib\xC3\xA9rez-en une pour ins\xC3\xA9rer";
    // une piste libre prise avant : elle se place juste avant la piste visée (qui recule d'un cran)
    return moveTrack (free, free < at ? at - 1 : at);
}

juce::String GrooveboxProcessor::copyTrack (int from, int to)
{
    if (from < 0 || from >= numTracks || to < 0 || to >= numTracks) return "Piste inconnue";
    if (from == to) return {};
    const auto srcType = engine.getTrackInfo (from).instrumentType;
    if (srcType.empty()) return juce::String::fromUTF8 ("La piste copi\xC3\xA9" "e est libre : rien \xC3\xA0 copier");
    commitHistory(); // ce qui précède reste une étape à part

    auto snap = snapshotForHistory();
    auto isOf = [] (const juce::ValueTree& v, const char* key, int t) { return (int) v.getProperty (key, -1) == t; };
    // copie d'un élément rangé par piste : renuméroté vers « to » (et ses verrous de la piste source)
    auto moved = [&] (const juce::ValueTree& v, const char* key)
    {
        auto c = v.createCopy();
        c.setProperty (key, to, nullptr);
        for (auto l : c) if (isOf (l, "c", from)) l.setProperty ("c", to, nullptr);
        return c;
    };
    std::vector<juce::ValueTree> added;
    juce::ValueTree oldSeq;
    for (int k = snap.getNumChildren(); --k >= 0;)
    {
        auto c = snap.getChild (k);
        const auto type = c.getType().toString();
        const char* key = type == "TRACK" ? "i" : (type == "INSERT" || type == "MOD") ? "c"
                        : (type == "PADFX" || type == "SAMPLE" || type == "EXTRA") ? "t" : nullptr;
        if (key != nullptr)
        {
            // pour MOD et INSERT, « c » est le canal ; les verrous d'un pas (EXTRA) suivent leur piste
            if (isOf (c, key, from)) added.push_back (moved (c, key));
            if (isOf (c, key, to)) snap.removeChild (k, nullptr);
        }
        else if (type == "PARAMS" || type == "SCENE")
        {
            // réglages et verrous de scène : ceux de la piste d'arrivée sont remplacés par ceux de la source
            for (int j = c.getNumChildren(); --j >= 0;) if (isOf (c.getChild (j), "c", to)) c.removeChild (j, nullptr);
            const auto n = c.getNumChildren();
            for (int j = 0; j < n; ++j)
                if (auto l = c.getChild (j); isOf (l, "c", from)) { auto cp = l.createCopy(); cp.setProperty ("c", to, nullptr); c.appendChild (cp, nullptr); }
        }
        else if (type == "SEQ")
        {
            // séquences (patterns, voies, longueurs, accent) ; la piste d'arrivée garde sa coupure et son solo
            juce::ValueTree src, dst;
            for (auto tr : c) { if (isOf (tr, "i", from)) src = tr; if (isOf (tr, "i", to)) dst = tr; }
            if (src.isValid())
            {
                auto cp = src.createCopy();
                cp.setProperty ("i", to, nullptr);
                for (const auto* id : { "mute", "solo" })
                {
                    if (dst.isValid() && dst.hasProperty (id)) cp.setProperty (id, dst.getProperty (id), nullptr);
                    else cp.removeProperty (id, nullptr);
                }
                if (dst.isValid()) c.removeChild (dst, nullptr);
                c.appendChild (cp, nullptr);
            }
        }
    }
    for (auto& a : added)
    {
        if (a.hasType ("TRACK") && a.getProperty ("name").toString() == "Piste " + juce::String (from + 1))
            a.setProperty ("name", "Piste " + juce::String (to + 1), nullptr); // nom d'origine : celui de sa place
        snap.appendChild (a, nullptr);
    }
    restoreSnapshot (snap);
    commitHistory();
    projectVersion.fetch_add (1);
    return {};
}

static bool encodeWav24 (const std::vector<float>& l, const std::vector<float>& r, bool stereo, double sampleRate, juce::MemoryBlock& wav);

juce::String GrooveboxProcessor::copyLane (int track, int from, int to)
{
    using namespace instruments;
    if (track < 0 || track >= numTracks) return "Piste inconnue";
    if (engine.getTrackInfo (track).instrumentType != "octa") return juce::String::fromUTF8 ("La piste n'a pas Pieuvre 8");
    if (from < 0 || from >= octa::numUnits || to < 0 || to >= octa::numUnits) return juce::String::fromUTF8 ("Piste interne inconnue");
    if (from == to) return {};
    commitHistory();
    auto snap = snapshotForHistory();
    const juce::String pf = "t" + juce::String (from + 1) + "_", pt = "t" + juce::String (to + 1) + "_";
    const auto& slotDesc = octa::unitDesc().params[(size_t) octa::slot];

    /* v0.43.2 : la T d'arrivée reçoit ses PROPRES copies des sons — rien n'est partagé avec la T copiée
       (avant : elle jouait les emplacements de l'autre T ; vider ou changer la note de l'une touchait
       l'autre). Les 16 emplacements de la T copiée (sons et attributs) remplacent ceux de la T
       d'arrivée ; un son pris ailleurs (réserve d'une autre T, enregistreur) est recopié dans un
       emplacement libre de la T d'arrivée. Tout passe par l'historique (Ctrl+Z). */
    const int fromBank = from * octa::bankSlots, toBank = to * octa::bankSlots;
    std::map<int, juce::var> refAt; // échantillons de la piste avant la copie (index → réf.)
    for (int n = snap.getNumChildren(); --n >= 0;)
    {
        const auto c = snap.getChild (n);
        if (! c.hasType ("SAMPLE") || (int) c.getProperty ("t", -1) != track) continue;
        const int i = (int) c.getProperty ("i", -1);
        refAt[i] = c.getProperty ("ref");
        if (i >= toBank && i < toBank + octa::bankSlots) snap.removeChild (n, nullptr);
    }
    std::array<bool, octa::bankSlots> used {};
    std::vector<std::pair<int, int>> attrCopies; // attributs d'emplacement : (index source, index d'arrivée)
    auto putSample = [&] (int dst, const juce::var& ref)
    {
        juce::ValueTree smp ("SAMPLE");
        smp.setProperty ("t", track, nullptr); smp.setProperty ("i", dst, nullptr); smp.setProperty ("ref", ref, nullptr);
        snap.appendChild (smp, nullptr);
        used[(size_t) (dst - toBank)] = true;
    };
    for (int k = 0; k < octa::bankSlots; ++k)
    {
        attrCopies.push_back ({ fromBank + k, toBank + k });
        if (auto it = refAt.find (fromBank + k); it != refAt.end()) putSample (toBank + k, it->second);
    }
    const auto* inst = engine.instrumentAt (track);
    std::map<int, int> ownCopies; // choix d'emplacement d'origine → emplacement propre (1 à 16), -1 si impossible
    auto ownCopy = [&] (int choice, int poolIdx, int recLane) -> int
    {
        if (auto it = ownCopies.find (choice); it != ownCopies.end()) return it->second;
        int free = -1;
        for (int k = 0; k < octa::bankSlots && free < 0; ++k) if (! used[(size_t) k]) free = k;
        int result = -1;
        if (free >= 0 && poolIdx >= 0)
        {
            if (auto it = refAt.find (poolIdx); it != refAt.end())
            {
                putSample (toBank + free, it->second);
                attrCopies.push_back ({ poolIdx, toBank + free });
                result = free + 1;
            }
        }
        else if (free >= 0 && inst != nullptr)
        {
            std::vector<float> l, r;
            double rate = 48000.0;
            juce::MemoryBlock wav;
            if (inst->copyRecording (recLane, l, r, rate) && encodeWav24 (l, r, r.size() == l.size(), rate, wav))
            {
                const auto* b = (const std::uint8_t*) wav.getData();
                auto bytes = std::make_shared<const std::vector<std::uint8_t>> (b, b + wav.getSize());
                putSample (toBank + free, juce::var (new SampleRef ("Enregistrement T" + std::to_string (recLane + 1) + ".wav", std::move (bytes))));
                result = free + 1;
            }
        }
        ownCopies[choice] = result;
        return result;
    };
    // choix d'emplacement de la T copiée → choix pour la T d'arrivée (toujours un son à elle si possible)
    auto mapSlot = [&] (int k) -> int
    {
        if (k >= 1 && k <= octa::ownSlots) return k;
        if (k >= octa::poolBase && k < octa::recBase)
        {
            const int p = k - octa::poolBase;
            if (p >= fromBank && p < fromBank + octa::bankSlots) return 1 + p - fromBank;
            const int c = ownCopy (k, p, -1);
            return c > 0 ? c : k;
        }
        const int r = k == 0 ? from : juce::jlimit (0, octa::numUnits - 1, k - octa::recBase);
        const int c = ownCopy (octa::recBase + r, -1, r);
        return c > 0 ? c : (r == from || r == to ? 0 : k);
    };

    // réglage d'une piste interne : renommé vers l'autre ; l'emplacement joué devient une copie à elle
    auto moveRef = [&] (juce::ValueTree v) -> bool
    {
        if ((int) v.getProperty ("c", -1) != track) return false;
        const auto area = v.getProperty ("a").toString(), id = v.getProperty ("i").toString();
        if (area == "inst" && id.startsWith (pf))
        {
            v.setProperty ("i", pt + id.substring (pf.length()), nullptr);
            if (id == pf + "slot")
            {
                const auto k = (int) slotDesc.toPlain ((float) (double) v.getProperty ("v", 0.0));
                const auto k2 = mapSlot (k);
                v.setProperty ("v", (double) slotDesc.toNorm ((float) k2), nullptr);
            }
            return true;
        }
        const auto pfx = "pfx" + juce::String (from + 1) + "_";
        if (area.startsWith (pfx)) { v.setProperty ("a", "pfx" + juce::String (to + 1) + "_" + area.substring (pfx.length()), nullptr); return true; }
        if (area == "padbyp" && id.startsWith ("b" + juce::String (from + 1) + "_")) { v.setProperty ("i", "b" + juce::String (to + 1) + "_" + id.fromFirstOccurrenceOf ("_", false, false), nullptr); return true; }
        return false;
    };
    auto isTo = [&] (const juce::ValueTree& v)
    {
        if ((int) v.getProperty ("c", -1) != track) return false;
        const auto area = v.getProperty ("a").toString(), id = v.getProperty ("i").toString();
        return (area == "inst" && id.startsWith (pt)) || area.startsWith ("pfx" + juce::String (to + 1) + "_")
            || (area == "padbyp" && id.startsWith ("b" + juce::String (to + 1) + "_"));
    };
    std::vector<juce::ValueTree> added;
    for (int n = snap.getNumChildren(); --n >= 0;)
    {
        auto c = snap.getChild (n);
        if (c.hasType ("PARAMS") || c.hasType ("SCENE"))
        {
            for (int j = c.getNumChildren(); --j >= 0;) if (isTo (c.getChild (j))) c.removeChild (j, nullptr);
            const auto cnt = c.getNumChildren();
            for (int j = 0; j < cnt; ++j) { auto cp = c.getChild (j).createCopy(); if (moveRef (cp)) c.appendChild (cp, nullptr); }
        }
        else if (c.hasType ("PADFX") && (int) c.getProperty ("t", -1) == track)
        {
            if ((int) c.getProperty ("p", -1) == to) snap.removeChild (n, nullptr);
            else if ((int) c.getProperty ("p", -1) == from) { auto cp = c.createCopy(); cp.setProperty ("p", to, nullptr); added.push_back (cp); }
        }
        else if (c.hasType ("EXTRA") && (int) c.getProperty ("t", -1) == track)
        {
            if ((int) c.getProperty ("l", -1) == to) snap.removeChild (n, nullptr);
            else if ((int) c.getProperty ("l", -1) == from)
            {
                auto cp = c.createCopy();
                cp.setProperty ("l", to, nullptr);
                for (auto l : cp) moveRef (l);
                added.push_back (cp);
            }
        }
        else if (c.hasType ("SEQ"))
            for (auto tr : c)
            {
                if (! tr.hasType ("T") || (int) tr.getProperty ("i", -1) != track) continue;
                // lignes (pas, notes, durées) de chaque pattern, et réglages de la ligne (sens, coupure, solo)
                auto copyLanes = [&] (juce::ValueTree parent)
                {
                    juce::ValueTree src;
                    for (int j = parent.getNumChildren(); --j >= 0;)
                    {
                        const auto ch = parent.getChild (j);
                        if (! ch.hasType ("LANE")) continue;
                        if ((int) ch.getProperty ("l", -1) == to) parent.removeChild (j, nullptr);
                        else if ((int) ch.getProperty ("l", -1) == from) src = ch;
                    }
                    if (src.isValid()) { auto cp = src.createCopy(); cp.setProperty ("l", to, nullptr); parent.appendChild (cp, nullptr); }
                };
                copyLanes (tr);
                for (auto pat : tr)
                    if (pat.hasType ("PAT"))
                    {
                        copyLanes (pat);
                        juce::StringArray ll;
                        ll.addTokens (pat.getProperty ("laneLen").toString(), ",", "");
                        if (ll.size() == 16) { ll.set (to, ll[from]); pat.setProperty ("laneLen", ll.joinIntoString (","), nullptr); }
                    }
            }
    }
    for (auto& a : added) snap.appendChild (a, nullptr);
    // l'emplacement joué, même à sa valeur d'origine (non écrite) : celui de la piste copiée
    if (auto params = snap.getChildWithName ("PARAMS"); params.isValid())
    {
        const auto gFrom = engine.findParam (track, "inst", (pf + "slot").toStdString());
        if (gFrom >= 0)
        {
            const auto k = (int) slotDesc.toPlain (engine.getParam (gFrom));
            const auto k2 = mapSlot (k);
            for (int j = params.getNumChildren(); --j >= 0;)
                if (const auto c = params.getChild (j); (int) c.getProperty ("c", -1) == track && c.getProperty ("a").toString() == "inst" && c.getProperty ("i").toString() == pt + "slot")
                    params.removeChild (j, nullptr);
            juce::ValueTree v ("P");
            v.setProperty ("c", track, nullptr); v.setProperty ("a", "inst", nullptr); v.setProperty ("i", pt + "slot", nullptr);
            v.setProperty ("v", (double) slotDesc.toNorm ((float) k2), nullptr);
            params.appendChild (v, nullptr);
        }
    }
    // attributs des emplacements recopiés (début, fin, boucle, fondus, note…) : ceux de la T d'arrivée
    // sont remplacés par ceux des sons qu'elle reçoit
    auto attrIndex = [&] (const juce::ValueTree& v, juce::String& rest) -> int
    {
        if ((int) v.getProperty ("c", -1) != track || v.getProperty ("a").toString() != "inst") return -1;
        const auto id = v.getProperty ("i").toString();
        if (! id.startsWithChar ('e')) return -1;
        int n = 0, pos = 1;
        while (pos < id.length() && juce::CharacterFunctions::isDigit (id[pos])) n = n * 10 + (id[pos++] - '0');
        if (pos == 1 || pos >= id.length() || n < 1 || n > octa::poolSize) return -1;
        rest = id.substring (pos);
        return n - 1;
    };
    for (auto parent : snap)
    {
        if (! (parent.hasType ("PARAMS") || parent.hasType ("SCENE"))) continue;
        std::vector<std::pair<int, juce::ValueTree>> attrs;
        for (int j = parent.getNumChildren(); --j >= 0;)
        {
            juce::String rest;
            const auto c = parent.getChild (j);
            const int idx = attrIndex (c, rest);
            if (idx < 0) continue;
            attrs.push_back ({ idx, c.createCopy() });
            if (idx >= toBank && idx < toBank + octa::bankSlots) parent.removeChild (j, nullptr);
        }
        for (const auto& [src, dst] : attrCopies)
            for (const auto& [idx, c] : attrs)
                if (idx == src)
                {
                    juce::String rest;
                    attrIndex (c, rest);
                    auto cp = c.createCopy();
                    cp.setProperty ("i", "e" + juce::String (dst + 1) + rest, nullptr);
                    parent.appendChild (cp, nullptr);
                }
    }
    restoreSnapshot (snap);
    commitHistory();
    projectVersion.fetch_add (1);
    return {};
}

juce::String GrooveboxProcessor::resetLanes (int track, int lane)
{
    using namespace instruments;
    if (track < 0 || track >= numTracks) return "Piste inconnue";
    if (engine.getTrackInfo (track).instrumentType != "octa") return juce::String::fromUTF8 ("La piste n'a pas Pieuvre 8");
    if (lane < -1 || lane >= octa::numUnits) return juce::String::fromUTF8 ("Piste interne inconnue");
    commitHistory();
    auto snap = snapshotForHistory();
    // réglages d'une piste interne (et ses 2 effets) remis à leur valeur d'origine : retirés de la copie
    auto of = [&] (const juce::ValueTree& v) -> bool
    {
        if ((int) v.getProperty ("c", -1) != track) return false;
        const auto area = v.getProperty ("a").toString(), id = v.getProperty ("i").toString();
        for (int k = 0; k < octa::numUnits; ++k)
        {
            if (lane >= 0 && k != lane) continue;
            if ((area == "inst" && id.startsWith ("t" + juce::String (k + 1) + "_")) || area.startsWith ("pfx" + juce::String (k + 1) + "_")
                || (area == "padbyp" && id.startsWith ("b" + juce::String (k + 1) + "_")))
                return true;
            if (lane < 0 && area == "inst" && id == "kbmode") return true;
            // v0.46 : attributs de ses 16 emplacements (tempo, début, fin, boucle, fondus, gain, note)
            if (area == "inst" && id.startsWith ("e"))
            {
                const auto num = id.substring (1).initialSectionContainingOnly ("0123456789");
                if (num.isNotEmpty() && (num.getIntValue() - 1) / octa::bankSlots == k) return true;
            }
        }
        return false;
    };
    // v0.46 : « remis à l'origine » veut dire tout : réglages, verrous de pas, verrous de scène,
    // courbes et mouvements enregistrés, modulations, effets de la piste interne — rien ne reste
    std::function<void (juce::ValueTree)> prune = [&] (juce::ValueTree node)
    {
        for (int j = node.getNumChildren(); --j >= 0;)
        {
            auto c = node.getChild (j);
            if ((c.hasType ("P") || c.hasType ("L") || c.hasType ("AUTO") || c.hasType ("PAUTO") || c.hasType ("MOD")) && of (c)) { node.removeChild (j, nullptr); continue; }
            if (c.hasType ("PADFX") && (int) c.getProperty ("t", -1) == track && (lane < 0 || (int) c.getProperty ("p", -1) == lane)) { node.removeChild (j, nullptr); continue; }
            prune (c);
            // verrou de pas resté sans rien : retiré aussi s'il n'a ni condition, ni probabilité, ni répétition
            if (c.hasType ("EXTRA") && c.getNumChildren() == 0 && (int) c.getProperty ("cond", 0) == 0 && (int) c.getProperty ("prob", 100) >= 100
                && ! c.hasProperty ("rat") && ! c.hasProperty ("flam") && ! c.hasProperty ("arp"))
                node.removeChild (j, nullptr);
        }
    };
    prune (snap);
    restoreSnapshot (snap);
    commitHistory();
    projectVersion.fetch_add (1);
    return {};
}

//==============================================================================
// Apprentissage MIDI

void GrooveboxProcessor::clearMidiTarget (int target) noexcept
{
    for (auto& m : ccMap) { int e = target; m.compare_exchange_strong (e, midiNone); }
    for (auto& m : noteMap) { int e = target; m.compare_exchange_strong (e, midiNone); }
    midiVersion.fetch_add (1);
}

void GrooveboxProcessor::setMidiMapping (int kind, int channel, int number, int target) noexcept
{
    if (channel < 0 || channel > 15 || number < 0 || number > 127) return;
    if (! isMidiSpecial (target) && ! isMidiTrackTarget (target) && ! isMidiExtra (target) && (target < 0 || target >= numSlots)) target = midiNone;
    (kind == 0 ? ccMap : noteMap)[(size_t) (channel * 128 + number)].store (target);
    midiVersion.fetch_add (1);
}

bool GrooveboxProcessor::getMidiConflict (int& kind, int& channel, int& number, int& oldTarget, int& newTarget) const noexcept
{
    const auto key = conflictKey.load();
    if (key < 0) return false;
    kind = key / 2048; channel = (key % 2048) / 128; number = key % 128;
    oldTarget = conflictOld.load(); newTarget = conflictNew.load();
    return true;
}

void GrooveboxProcessor::resolveMidiConflict (bool replace) noexcept
{
    const auto key = conflictKey.exchange (-1);
    if (key < 0) return;
    if (replace)
    {
        const auto target = conflictNew.load();
        clearMidiTarget (target); // une cible n'a qu'une commande
        setMidiMapping (key / 2048, (key % 2048) / 128, key % 128, target);
    }
    midiVersion.fetch_add (1);
}

/** Commandes de transport et du morceau apprises au contrôleur (fil audio : rien que des atomiques). */
void GrooveboxProcessor::transportCommand (int target) noexcept
{
    auto& seq = engine.sequencer();
    switch (target)
    {
        case midiStart:    seq.requestPlay(); break;
        case midiStop:     seq.requestStop(); break;
        case midiRecord:   seq.setRecording (! seq.isRecording()); break;
        case midiRecMoves: engine.setRecordMoves (! engine.getRecordMoves()); break;
        case midiSongMode: songModeToggles.fetch_add (1); break; // le morceau se republie sur le fil de l'interface
        // v0.46 : rangées au contrôleur ; sans rangée lancée, on part de celle que joue la première piste
        case midiRowPrev:
        case midiRowNext:
        {
            auto cur = lastRow.load (std::memory_order_relaxed);
            if (cur < 0)
                for (int t = 0; t < numTracks && cur < 0; ++t)
                    if (const auto p = seq.getPlaying (t); p >= 0 && p < Sequencer::numPatterns) cur = p;
            const auto n = Sequencer::numPatterns;
            // v0.46 fix29 : 16 rangées (A à P) ; on saute les rangées vides (aucune piste n'a de notes) :
            // la rangée suivante de H est I si elle est écrite, sinon on revient à A
            auto used = [&seq, n] (int r) { for (int t = 0; t < numTracks; ++t) if (seq.hasNotesInLength (t, r)) return true; return false; };
            int next = cur < 0 ? 0 : cur;
            if (cur >= 0)
                for (int k = 1; k <= n; ++k)
                {
                    const auto r = (cur + (target == midiRowNext ? k : n - k)) % n;
                    if (used (r) || k == n) { next = r; break; }
                }
            launchRow (next);
            break;
        }
        case midiPhoto:  photoRequests.fetch_add (1); break;
        case midiRecall: recallRequests.fetch_add (1); break;
        case midiRowStop:
            for (int t = 0; t < numTracks; ++t) seq.launch (t, Sequencer::stopPattern);
            break;
        default:
            if (target <= midiRowBase && target > midiRowBase - Sequencer::numPatterns) launchRow (midiRowBase - target);
            break;
    }
}

std::vector<GrooveboxProcessor::MidiMapping> GrooveboxProcessor::getMidiMappings() const
{
    std::vector<MidiMapping> out;
    for (int k = 0; k < numMidiKeys; ++k)
    {
        if (const auto t = ccMap[(size_t) k].load(); t != midiNone) out.push_back ({ 0, k / 128, k % 128, t });
        if (const auto t = noteMap[(size_t) k].load(); t != midiNone) out.push_back ({ 1, k / 128, k % 128, t });
    }
    return out;
}

bool GrooveboxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    const bool inOk = in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return outOk && inOk;
}

void GrooveboxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    midiOutBuf.ensureSize (3 * 4 * engine::MidiOutQueue::capacity); // après la 1.1 : pas d'allocation dans le fil audio
    montagePlayer.setDoc (montageDoc, sampleRate); // v0.46 fix8 : positions des clips à la fréquence de la carte
    const auto size = (size_t) std::max (samplesPerBlock, 4096);
    silentIn.assign (size * 2, 0.0f);
    monoOut.assign (size, 0.0f);
}

//==============================================================================
void GrooveboxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto n = buffer.getNumSamples();
    if (n <= 0) return;
    // v0.46.2 (Android) : l'application est passée derrière une autre (Ouvrir, Exporter, autre appli) :
    // le système y ralentit le calcul, d'où craquements et ralentis. On rend du silence sans rien
    // calculer (la lecture reprend où elle était au retour), sauf pendant un enregistrement audio.
    if (inBackground.load (std::memory_order_relaxed) && ! recorder.isRecording())
    {
        buffer.clear();
        midi.clear();
        return;
    }
    // Charge du calcul (v0.46 fix16) : temps passé dans cette fonction / temps réellement écoulé
    // entre deux appels, compté par fenêtres d'un quart de seconde. Mesurer contre l'horloge
    // (et non contre « échantillons / fréquence ») donne la vraie part du fil audio occupée,
    // même si la fréquence annoncée ou la taille des blocs varie d'un appareil à l'autre.
    struct CpuMeter
    {
        GrooveboxProcessor& p; int frames; double blockSeconds; juce::int64 t0 = juce::Time::getHighResolutionTicks();
        ~CpuMeter()
        {
            const auto used = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            auto& m = p.loadMeter;
            if (m.last != 0)
            {
                const auto gap = juce::Time::highResolutionTicksToSeconds (t0 - m.last);
                if (gap > 0.5 || gap <= 0.0) m = {}; // son arrêté ou appareil changé : on repart à zéro
                else { m.used += used; m.wall += gap; m.maxUsed = std::max (m.maxUsed, used); ++m.count; }
            }
            m.last = t0;
            if (m.wall >= 0.25 && m.count > 0)
            {
                const auto period = m.wall / m.count;
                p.cpuAvg.store ((float) (m.used / m.wall), std::memory_order_relaxed);
                p.cpuPeak.store (std::max ((float) (m.maxUsed / period), p.cpuPeak.load (std::memory_order_relaxed) * 0.6f), std::memory_order_relaxed);
                p.audioPeriodMs.store ((float) (period * 1000.0), std::memory_order_relaxed);
                p.audioBlockSize.store (frames, std::memory_order_relaxed);
                m.used = m.wall = m.maxUsed = 0.0; m.count = 0;
            }
           #if JUCE_ANDROID
            p.perfHint.report ((std::int64_t) (used * 1.0e9), (std::int64_t) (blockSeconds * 1.0e9));
           #else
            juce::ignoreUnused (blockSeconds);
           #endif
        }
    } cpuMeter { *this, n, (double) n / std::max (1.0, getSampleRate()) };

    // Macros : une macro automatisée par le logiciel hôte écrit dans les réglages qui lui sont
    // attribués, chacun sur sa plage
    for (int m = 0; m < numMacros; ++m)
    {
        const auto v = pMacro[(size_t) m]->load (std::memory_order_relaxed);
        const auto last = lastMacro[(size_t) m].load (std::memory_order_relaxed);
        const bool rel = macroRel[(size_t) m].load (std::memory_order_relaxed);
        if (std::abs (v - last) > 1.0e-6f)
            for (auto& l : macroSlots[(size_t) m])
                if (const auto g = l.global.load (std::memory_order_relaxed); g >= 0)
                {
                    const auto lo = l.lo.load (std::memory_order_relaxed), hi = l.hi.load (std::memory_order_relaxed);
                    float target;
                    if (rel)
                    {
                        // v0.46 : relatif ; réglage retouché à la main depuis la dernière écriture :
                        // il est réancré à sa nouvelle valeur, à la position précédente de la macro
                        auto base = l.base.load (std::memory_order_relaxed), ref = l.ref.load (std::memory_order_relaxed);
                        const auto cur = engine.getParam (g), w = l.written.load (std::memory_order_relaxed);
                        if (base < 0.0f || (w >= 0.0f && std::abs (cur - w) > 1.0e-4f))
                        {
                            base = cur; ref = last >= 0.0f ? last : v;
                            l.base.store (base, std::memory_order_relaxed); l.ref.store (ref, std::memory_order_relaxed);
                        }
                        target = relativeMacro (v, lo, hi, base, ref);
                    }
                    else target = lo + (hi - lo) * v;
                    engine.setParam (g, std::clamp (target, 0.0f, 1.0f));
                    l.written.store (engine.getParam (g), std::memory_order_relaxed);
                    markTouched (g);
                }
        lastMacro[(size_t) m].store (v, std::memory_order_relaxed);
    }

    Engine::BlockInfo bi;
    bi.bpm = pBpm->load();
    if (auto* hostPlayHead = getPlayHead())
        if (auto pos = hostPlayHead->getPosition())
        {
            if (auto hostBpm = pos->getBpm(); hostBpm.hasValue() && *hostBpm > 0.0) bi.bpm = *hostBpm;
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition()) { bi.host.playing = true; bi.host.ppq = *ppq; }
        }
    bi.bpm = juce::jlimit (20.0, 400.0, bi.bpm);
    currentBpm.store (bi.bpm, std::memory_order_relaxed);
    bi.swing = pSwing->load();
    bi.fader = juce::jlimit (0.0f, 1.0f, pXfade->load());
    bi.sceneA = juce::jlimit (0, numScenes - 1, (int) pSceneA->load() - 1);
    bi.sceneB = juce::jlimit (0, numScenes - 1, (int) pSceneB->load() - 1);
    // v0.46 fix22 : pavé vectoriel — position au doigt (ou automatisée), plus le mouvement qui tourne
    // autour d'elle (calé sur la mesure en lecture) ; poids bilinéaires des coins A (haut gauche),
    // B (haut droite), C (bas gauche), D (bas droite)
    bi.vecOn = pVecOn->load() >= 0.5f;
    bi.vecMode = pVecMode->load() >= 0.5f ? 1 : 0;
    for (int k = 0; k < 4; ++k)
    {
        bi.vecScene[(size_t) k] = juce::jlimit (0, numScenes - 1, (int) std::lround (pVecScene[(size_t) k]->load()) - 1);
        bi.vecFx[(size_t) k] = juce::jlimit (0, 4, (int) std::lround (pVecFx[(size_t) k]->load()));
    }
    {
        static constexpr double barsPerTurn[] { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0 };
        const auto turnBeats = 4.0 * barsPerTurn[juce::jlimit (0, 8, (int) std::lround (pVecRate->load()))];
        const auto orbit = juce::jlimit (0, 8, (int) std::lround (pVecOrbit->load()));
        const auto prev = vecPhase;
        if (engine.sequencer().isRunning()) vecPhase = std::fmod (std::max (0.0, engine.sequencer().getPpq()) / turnBeats, 1.0);
        else vecPhase = std::fmod (vecPhase + (double) n / std::max (1.0, getSampleRate()) * bi.bpm / 60.0 / turnBeats, 1.0);
        const auto r = (double) juce::jlimit (0.0f, 1.0f, pVecDepth->load()) * 0.5;
        const auto tw = juce::MathConstants<double>::twoPi * vecPhase;
        double ox = 0.0, oy = 0.0;
        bool absolute = false; // forme dessinée : position exacte sur le pavé (ni centre, ni taille)
        // parcours d'une ligne brisée fermée, à vitesse régulière (sommets en -1..1 ou 0..1)
        const auto along = [] (const float* px, const float* py, int count, double t, double& outX, double& outY)
        {
            if (count < 2) { outX = count > 0 ? px[0] : 0.0; outY = count > 0 ? py[0] : 0.0; return; }
            double total = 0.0;
            for (int i = 0; i < count; ++i) { const int j = (i + 1) % count; total += std::hypot (px[j] - px[i], py[j] - py[i]); }
            if (total <= 1.0e-9) { outX = px[0]; outY = py[0]; return; }
            double d = t * total;
            for (int i = 0; i < count; ++i)
            {
                const int j = (i + 1) % count;
                const double seg = std::hypot (px[j] - px[i], py[j] - py[i]);
                if (d <= seg || i == count - 1)
                {
                    const auto f = seg > 1.0e-9 ? std::min (1.0, d / seg) : 0.0;
                    outX = px[i] + (px[j] - px[i]) * f; outY = py[i] + (py[j] - py[i]) * f; return;
                }
                d -= seg;
            }
        };
        switch (orbit)
        {
            case 1: ox = std::cos (tw); oy = std::sin (tw); break;                           // cercle
            case 2: ox = std::sin (tw); oy = std::sin (2.0 * tw) * 0.5; break;                // infini (∞)
            case 3: { static const float vx[] { 0, 1, 0, -1 }, vy[] { -1, 0, 1, 0 };          // losange
                      along (vx, vy, 4, vecPhase, ox, oy); break; }
            case 4: { static const float vx[] { -1, 1, 1, -1 }, vy[] { -1, -1, 1, 1 };        // carré
                      along (vx, vy, 4, vecPhase, ox, oy); break; }
            case 5: { static const float vx[] { 0.0f, 0.87f, -0.87f }, vy[] { -1.0f, 0.5f, 0.5f }; // triangle
                      along (vx, vy, 3, vecPhase, ox, oy); break; }
            case 6: { static const float vx[] { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, -0.5f }, // ricochet (aller-retour en zigzag)
                                             vy[] { -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f };
                      along (vx, vy, 8, vecPhase, ox, oy); break; }
            case 7: { // hasard : un nouveau point tiré à chaque quart de tour, rejoint en glissant
                      const auto q = vecPhase * 4.0, f = q - std::floor (q);
                      if (std::floor (q) != std::floor (prev * 4.0) || prev > vecPhase)
                      {
                          vecRand[0] = vecRand[2]; vecRand[1] = vecRand[3];
                          auto rnd = [this] { vecSeed = vecSeed * 1664525u + 1013904223u; return (double) (vecSeed >> 8) / 16777216.0; };
                          vecRand[2] = rnd() * 2.0 - 1.0; vecRand[3] = rnd() * 2.0 - 1.0;
                      }
                      const auto e = f * f * (3.0 - 2.0 * f);
                      ox = vecRand[0] + (vecRand[2] - vecRand[0]) * e; oy = vecRand[1] + (vecRand[3] - vecRand[1]) * e; break; }
            case 8: { // forme dessinée au doigt (positions 0..1 sur le pavé)
                      const auto& path = vecPaths[(size_t) (vecPathIndex.load (std::memory_order_acquire) & 1)];
                      if (path.count >= 2) { along (path.x.data(), path.y.data(), path.count, vecPhase, ox, oy); absolute = true; }
                      break; }
            default: break;
        }
        // v0.46 fix28 : rotation de la forme autour de son centre (pas la forme dessinée, placée telle quelle)
        if (! absolute)
        {
            oy *= (double) juce::jlimit (0.05f, 2.0f, pVecHeight->load()); // aplatie ou étirée, avant de tourner
            const auto a = juce::degreesToRadians ((double) juce::jlimit (0.0f, 360.0f, pVecRot->load()));
            const auto ca = std::cos (a), sa = std::sin (a), rx = ox * ca - oy * sa;
            oy = ox * sa + oy * ca; ox = rx;
        }
        const auto x = juce::jlimit (0.0, 1.0, absolute ? ox : (double) pVecX->load() + ox * r);
        const auto y = juce::jlimit (0.0, 1.0, absolute ? oy : (double) pVecY->load() + oy * r);
        vecShownX.store ((float) x, std::memory_order_relaxed); vecShownY.store ((float) y, std::memory_order_relaxed);
        bi.vecW = { (float) ((1.0 - x) * (1.0 - y)), (float) (x * (1.0 - y)), (float) ((1.0 - x) * y), (float) (x * y) };
    }
    // Break temporisé : demandé par l'interface, compté ici en temps (suit les changements de tempo)
    const auto beatsThisBlock = (double) n / getSampleRate() * bi.bpm / 60.0;
    if (const auto req = timedBreakRequest.exchange (0.0); req != 0.0)
    {
        if (req < 0.0) timedBreakBeats = 0.0;
        else
        {
            auto beats = req;
            // position au début de ce bloc : celle de l'hôte, sinon le début du bloc précédent + sa durée
            const auto prev = engine.sequencer().getPpq();
            const auto ppq = bi.host.playing ? bi.host.ppq : (prev >= 0.0 ? prev + lastBlockBeats : -1.0);
            if (ppq >= 0.0) beats = std::max (0.25, std::round (ppq + req) - ppq); // fin calée sur un temps
            timedBreakBeats = beats;
        }
    }
    lastBlockBeats = beatsThisBlock;
    const bool timed = timedBreakBeats > 0.0;
    if (timed) timedBreakBeats -= beatsThisBlock;
    timedBreakOn.store (timed, std::memory_order_relaxed);
    bi.breakActive = breakHeld.load (std::memory_order_relaxed) || timed;

    // MIDI : notes vers la piste choisie dans l'interface, changement de programme = scène
    const auto focus = juce::jlimit (0, numTracks - 1, engine.focusTrack.load (std::memory_order_relaxed));
    const bool drums = engine.isPadNoteTrack (focus); // v0.46 : Pieuvre 8 se joue comme un clavier
    const bool rec = engine.sequencer().isRecording();
    const auto noteCh = midiNoteChan.load (std::memory_order_relaxed), ctrlCh = midiCtrlChan.load (std::memory_order_relaxed);
    const bool ctrlOn = midiCtrlOn.load (std::memory_order_relaxed);
    std::array<int, numTracks> trackCh {};
    for (int t = 0; t < numTracks; ++t) trackCh[(size_t) t] = trackMidiChan[(size_t) t].load (std::memory_order_relaxed);
    // GOLEM 1.0 : canaux des pistes internes T (FM Player, Pieuvre 8), lus une fois par bloc
    std::array<std::array<int, maxLaneChans>, numTracks> laneCh {};
    std::array<int, numTracks> laneCount {};
    for (int t = 0; t < numTracks; ++t)
    {
        laneCount[(size_t) t] = engine.laneNoteCount (t);
        for (int l = 0; l < laneCount[(size_t) t]; ++l) laneCh[(size_t) t][(size_t) l] = laneMidiChan[(size_t) t][(size_t) l].load (std::memory_order_relaxed);
    }
    // v0.46 fix24 : une cible pilotée par une commande MIDI ou un bouton assignable (valeur 0..1).
    // Les commandes (lecture, arrêt, rangées…) partent à 0,5 et plus ; coupure, solo, Fill, Break suivent la moitié.
    const auto applyTarget = [&] (int target, float v)
    {
        if (target >= 0) { engine.setParam (target, v); engine.recordMoveAudio (target, v); markTouched (target); }
        else if (isHostTarget (target)) hostPending[(size_t) (midiHostBase - target)].store (v, std::memory_order_relaxed);
        else if (isUiTarget (target))
        {
            const auto u = (size_t) (midiUiBase - target);
            uiCtlVal[u].store (v, std::memory_order_relaxed); uiCtlSeq[u].fetch_add (1); uiCtlVersion.fetch_add (1);
        }
        else if (target == midiXfade) { bi.fader = v; xfadeFromMidi.store (v); engine.recordFaderAudio (v); }
        else if (target == midiBreak) { setBreak (v >= 0.5f); bi.breakActive = v >= 0.5f; }
        else if (target == midiFill) setFill (v >= 0.5f);
        else if (target == midiPlay && v >= 0.5f) playToggleRequests.fetch_add (1);
        else if (target <= midiStart && target >= midiLastSpecial) { if (v >= 0.5f) transportCommand (target); }
        // coupure / solo de piste : bouton à bascule (127 = coupé, 0 = joué) ou maintenu
        else if (target <= midiBusSoloBase && target > midiBusSoloBase - numBusTargets) { const auto ch = numTracks + midiBusSoloBase - target; engine.setBusSolo (ch, v >= 0.5f); engine.recordSwitchAudio (ch, true, engine.getBusSolo (ch)); }
        else if (target <= midiBusMuteBase && target > midiBusMuteBase - numBusTargets) { const auto ch = numTracks + midiBusMuteBase - target; engine.setBusMute (ch, v >= 0.5f); engine.recordSwitchAudio (ch, false, engine.getBusMute (ch)); }
        else if (target <= midiSoloBase && target > midiSoloBase - numTracks) { engine.sequencer().setTrackSolo (midiSoloBase - target, v >= 0.5f); engine.recordSwitchAudio (midiSoloBase - target, true, v >= 0.5f); }
        else if (target <= midiMuteBase && target > midiMuteBase - numTracks) { engine.sequencer().setTrackMute (midiMuteBase - target, v >= 0.5f); engine.recordSwitchAudio (midiMuteBase - target, false, v >= 0.5f); }
    };
    // bouton assignable k : sa cible suit ; une commande ne part qu'au passage de la moitié
    const auto driveKnob = [&] (int k, float v)
    {
        const auto t = knobTarget[(size_t) k].load (std::memory_order_relaxed);
        const auto last = knobLastAudio[(size_t) k];
        knobLastAudio[(size_t) k] = v;
        if (t == midiNone) return;
        if ((isMidiSpecial (t) && t != midiXfade) || isMidiTrackTarget (t)) { if ((last >= 0.5f) == (v >= 0.5f)) return; }
        applyTarget (t, v);
    };
    for (int k = 0; k < numKnobs; ++k)
        if (const auto p = knobPending[(size_t) k].exchange (-1.0f, std::memory_order_relaxed); p >= 0.0f) driveKnob (k, p);
    // v0.46 fix28 : LFO des boutons : la cible suit position + profondeur × source (sources 1 à 4 du bus
    // principal, valeurs du bloc précédent). Ni les commandes ni le tempo. Les réglages de son sont écrits
    // sans enregistrer de mouvement ; LFO retiré : la cible revient à la position du bouton.
    {
        const bool mark = (++knobLfoBlocks & 7) == 0; // l'interface relit les réglages ~20 fois par seconde
        for (int k = 0; k < numKnobs; ++k)
        {
            const auto t = knobTarget[(size_t) k].load (std::memory_order_relaxed);
            const auto src = knobLfo[(size_t) k].load (std::memory_order_relaxed);
            const auto depth = knobDepth[(size_t) k].load (std::memory_order_relaxed);
            const bool command = (isMidiSpecial (t) && t != midiXfade) || isMidiTrackTarget (t) || (isHostTarget (t) && midiHostBase - t == 0);
            const bool on = src > 0 && t != midiNone && ! command && std::abs (depth) > 1.0e-4f;
            const auto base = knobVal[(size_t) k].load (std::memory_order_relaxed);
            auto sendTo = [&] (int tt, float v, bool touch)
            {
                if (tt >= 0) { engine.setParam (tt, v); if (touch) markTouched (tt); }
                else applyTarget (tt, v);
            };
            auto send = [&] (float v, bool touch) { sendTo (t, v, touch); };
            // bouton libéré ou relié ailleurs pendant sa modulation : l'ancienne cible revient à la position du bouton
            if (knobLfoActive[(size_t) k] && knobLfoTarget[(size_t) k] != t)
            {
                if (const auto old = knobLfoTarget[(size_t) k]; old != midiNone) sendTo (old, knobLfoBase[(size_t) k], true);
                knobLfoActive[(size_t) k] = false;
                knobOut[(size_t) k].store (-1.0f, std::memory_order_relaxed);
            }
            if (on)
            {
                const auto m = engine.getModValue (knobLfoChannel, src - 1);
                const auto v = juce::jlimit (0.0f, 1.0f, base + depth * (std::isfinite (m) ? m : 0.0f));
                if (! knobLfoActive[(size_t) k] || std::abs (v - knobLfoSent[(size_t) k]) > 2.0e-4f) { send (v, mark); knobLfoSent[(size_t) k] = v; }
                else if (mark && t >= 0) markTouched (t);
                knobLfoActive[(size_t) k] = true;
                knobLfoTarget[(size_t) k] = t;
                knobLfoBase[(size_t) k] = base;
                knobOut[(size_t) k].store (v, std::memory_order_relaxed);
            }
            else if (knobLfoActive[(size_t) k])
            {
                knobLfoActive[(size_t) k] = false;
                if (t != midiNone && ! command) send (base, true);
                knobOut[(size_t) k].store (-1.0f, std::memory_order_relaxed);
            }
        }
    }

    // 1.1 : télécommande : réglages en valeur absolue (câble : SysEx de n'importe quel canal ; Wi-Fi : file)
    const auto applyRemote = [&] (int target, float v)
    {
        if (target < midiLowestTarget) return;
        if (isKnobTarget (target)) { const auto k = midiKnobBase - target; knobVal[(size_t) k].store (v, std::memory_order_relaxed); driveKnob (k, v); }
        else if (target != midiNone) applyTarget (target, juce::jlimit (0.0f, 1.0f, v));
    };
    if (wifi != nullptr)
        wifi->drain ([&] (const uint8_t* d, size_t len)
        {
            remoteSeen.store (true, std::memory_order_relaxed);
            uint8_t raw[3];
            // notes, CC et molettes du clavier du téléphone : comme s'ils venaient d'un appareil MIDI
            if (const int m = remote::decodeMidi (d, len, raw); m > 0) midi.addEvent (raw, m, 0);
            else remote::forEachSet (d, len, applyRemote);
        });

    int numEvents = 0;
    const double nowMs = juce::Time::getMillisecondCounterHiRes(); // après la 1.1 : anti-boucle des pistes MIDI
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        // après la 1.1 : message venu de l'appareil joué par les pistes MIDI (ou écho exact de ce qu'on vient
        // de lui envoyer) : il ne rejoue pas les pistes (pas de boucle) ; Apprendre et les commandes apprises restent
        bool fromDev = false;
        if (! msg.isSysEx())
        {
            fromDev = grv::takeFromDevice (msg.getRawData(), msg.getRawDataSize(), nowMs);
            if (const auto k = grv::midiKey (msg.getRawData(), msg.getRawDataSize()); ! fromDev && k != 0)
                for (int i = 0; i < (int) sentKey.size(); ++i)
                    if (sentKey[(size_t) i] == k && nowMs - sentAt[(size_t) i] < 400.0) { sentKey[(size_t) i] = 0; fromDev = true; break; }
        }
        if (msg.isSysEx())
        {
            const auto* d = msg.getSysExData();
            const auto len = (size_t) msg.getSysExDataSize();
            if (remote::isRemote (d, len))
            {
                remoteSeen.store (true, std::memory_order_relaxed);
                remote::forEachSet (d, len, applyRemote);
            }
            continue;
        }
        // après la 1.1 : dernier CC reçu (« Apprendre » le numéro de CC d'un bouton de piste MIDI)
        if (msg.isController())
        {
            lastCcIn.store ((msg.getChannel() << 8) | msg.getControllerNumber(), std::memory_order_relaxed);
            lastCcSeq.fetch_add (1, std::memory_order_release);
        }
        const auto offset = juce::jlimit (0, n - 1, meta.samplePosition);
        const auto chan = juce::jlimit (0, 15, msg.getChannel() - 1);
        // v0.46 : canaux MIDI (comme Ableton ou Fruity) — commandes apprises seulement sur le canal
        // des commandes (ou tous), et seulement si elles sont actives ; notes et expression : vers les
        // pistes réglées sur ce canal, sinon vers la piste choisie (si elle n'a pas son propre canal
        // et si le canal des notes l'accepte)
        const bool ctrlChan = ctrlCh == 0 || ctrlCh == chan + 1;
        const bool ctrlMsg = ctrlOn && ctrlChan;
        int dest[numTracks];
        int numDest = 0;
        // GOLEM 1.0 : une piste interne T avec son canal joue ce canal (elle passe avant le canal de sa piste)
        int laneDest[numTracks * maxLaneChans][2];
        int numLaneDest = 0;
        bool viaLane[numTracks] = {};
        for (int t = 0; t < numTracks; ++t)
            for (int l = 0; l < laneCount[(size_t) t]; ++l)
                if (laneCh[(size_t) t][(size_t) l] == chan + 1) { laneDest[numLaneDest][0] = t; laneDest[numLaneDest][1] = l; ++numLaneDest; viaLane[t] = true; }
        for (int t = 0; t < numTracks; ++t) if (trackCh[(size_t) t] == chan + 1 && ! viaLane[t]) dest[numDest++] = t;
        const bool toFocus = numDest == 0 && numLaneDest == 0 && (noteCh == 0 || noteCh == chan + 1) && trackCh[(size_t) focus] == 0;
        if (toFocus) dest[numDest++] = focus;
        // expression et commandes du canal d'une T : vers sa piste (une fois)
        if (! msg.isNoteOnOrOff())
            for (int t = 0; t < numTracks; ++t) if (viaLane[t]) dest[numDest++] = t;

        // Apprentissage : le premier CC (ou la première note) reçu prend la cible demandée
        if (const auto learn = learnTarget.load (std::memory_order_relaxed); learn != midiNone && ctrlChan && (msg.isController() || msg.isNoteOn()))
        {
            const bool cc = msg.isController();
            const auto num = cc ? msg.getControllerNumber() : msg.getNoteNumber();
            // commande déjà affectée à autre chose : on demande avant de remplacer (rien n'est changé ici)
            if (const auto old = (cc ? ccMap : noteMap)[(size_t) (chan * 128 + num)].load(); old != midiNone && old != learn)
            {
                conflictOld.store (old); conflictNew.store (learn);
                conflictKey.store ((cc ? 0 : 1) * 2048 + chan * 128 + num);
                learnTarget.store (midiNone);
                midiVersion.fetch_add (1);
                continue;
            }
            // une cible n'a qu'une commande : l'ancienne est oubliée
            for (auto& m : ccMap) { int e = learn; m.compare_exchange_strong (e, midiNone); }
            for (auto& m : noteMap) { int e = learn; m.compare_exchange_strong (e, midiNone); }
            (cc ? ccMap : noteMap)[(size_t) (chan * 128 + num)].store (learn);
            learnTarget.store (midiNone);
            midiVersion.fetch_add (1);
            continue;
        }

        // Expression des pistes jouées : pitch bend, molette (CC 1), pression, pédale (CC 64)
        auto expression = [&] (InputEvent::Type type, float value)
        {
            for (int d = 0; d < numDest; ++d)
            {
                if (numEvents >= (int) midiEvents.size()) return;
                InputEvent e;
                e.type = type; e.track = dest[d]; e.note = 0; e.offset = offset; e.velocity = value; e.record = false;
                midiEvents[(size_t) numEvents++] = e;
            }
        };
        if (fromDev && (msg.isPitchWheel() || msg.isChannelPressure() || msg.isAftertouch())) continue;
        if (msg.isPitchWheel()) { expression (InputEvent::pitchBend, juce::jlimit (-1.0f, 1.0f, (float) (msg.getPitchWheelValue() - 8192) / 8191.0f)); continue; }
        if (msg.isChannelPressure()) { expression (InputEvent::pressure, (float) msg.getChannelPressureValue() / 127.0f); continue; }

        if (msg.isController())
        {
            // 1.1 : choix de banque standard (CC 0 et CC 32) pour le changement de programme qui suit
            if (msg.getControllerNumber() == 0) bankMsb[(size_t) chan] = msg.getControllerValue();
            if (msg.getControllerNumber() == 32) bankLsb[(size_t) chan] = msg.getControllerValue();
            const auto target = ctrlMsg ? ccMap[(size_t) (chan * 128 + msg.getControllerNumber())].load (std::memory_order_relaxed) : (int) midiNone;
            if (fromDev && target == midiNone) continue; // après la 1.1 : pas de boucle avec l'appareil des pistes MIDI
            // 1.1 : CC 102 à 105 (non définis par la norme) = préréglage de l'effet des inserts 1 à 4
            // (valeur 0 = n° 1) pour les pistes réglées sur ce canal, sauf si la commande est apprise
            if (target == midiNone && msg.getControllerNumber() >= 102 && msg.getControllerNumber() <= 105)
            {
                const int slot = msg.getControllerNumber() - 102, n = msg.getControllerValue();
                bool any = false;
                for (int t = 0; t < numTracks; ++t)
                    if (trackCh[(size_t) t] == chan + 1) { pendingPreset[(size_t) t].store ((((slot + 1) << 16) | n) + 1, std::memory_order_relaxed); any = true; }
                if (any) continue; // sans piste sur ce canal : commande libre, comme les autres
            }
            const auto v = (float) msg.getControllerValue() / 127.0f;
            if (target == midiNone && msg.getControllerNumber() == 1) expression (InputEvent::modWheel, v);
            else if (target == midiNone && msg.getControllerNumber() == 64) expression (InputEvent::sustain, v >= 0.5f ? 1.0f : 0.0f);
            else if (isKnobTarget (target)) { const auto k = midiKnobBase - target; knobVal[(size_t) k].store (v, std::memory_order_relaxed); driveKnob (k, v); }
            else if (target != midiNone) applyTarget (target, v);
            else if (msg.getControllerNumber() != 1 && msg.getControllerNumber() != 64)
                for (int d = 0; d < numDest && numEvents < (int) midiEvents.size(); ++d) // v0.49 : CC libres → instrument (FM Player)
                {
                    InputEvent e;
                    e.type = InputEvent::controller; e.track = dest[d]; e.note = msg.getControllerNumber(); e.offset = offset; e.velocity = v; e.record = false;
                    midiEvents[(size_t) numEvents++] = e;
                }
            continue;
        }
        if (msg.isNoteOnOrOff() && ctrlMsg)
            if (const auto target = noteMap[(size_t) (chan * 128 + msg.getNoteNumber())].load (std::memory_order_relaxed); target != midiNone)
            {
                const bool on = msg.isNoteOn();
                if (target == midiBreak) { setBreak (on); bi.breakActive = on; }
                else if (target == midiFill) setFill (on);
                else if (target == midiPlay && on) playToggleRequests.fetch_add (1);
                else if (target <= midiStart && target >= midiLastSpecial) { if (on) transportCommand (target); }
                else if (target == midiXfade && on) { bi.fader = bi.fader < 0.5f ? 1.0f : 0.0f; xfadeFromMidi.store (bi.fader); engine.recordFaderAudio (bi.fader); }
                // GOLEM 1.0 : bouton maintenu (touches du FM Player) : appuyé tant que la note est tenue
                else if (target >= 0 && engine.isMomentary (target))
                { engine.setParam (target, on ? 1.0f : 0.0f); markTouched (target); }
                else if (target >= 0 && on) { engine.setParam (target, engine.getParam (target) < 0.5f ? 1.0f : 0.0f); markTouched (target); }
                // v0.46 fix24 : bouton assignable, réglage global, valeur d'écran : chaque appui bascule 0 / 1
                else if (isKnobTarget (target) && on)
                {
                    const int k = midiKnobBase - target; const float nv = knobVal[(size_t) k].load() < 0.5f ? 1.0f : 0.0f;
                    knobVal[(size_t) k].store (nv); driveKnob (k, nv);
                }
                else if (isHostTarget (target) && on)
                {
                    const auto* p = hostParams[(size_t) (midiHostBase - target)];
                    applyTarget (target, p != nullptr && p->getValue() < 0.5f ? 1.0f : 0.0f);
                }
                else if (isUiTarget (target) && on) applyTarget (target, uiCtlVal[(size_t) (midiUiBase - target)].load() < 0.5f ? 1.0f : 0.0f);
                // pad du contrôleur : chaque appui bascule la coupure ou le solo de la piste (ou du bus)
                else if (target <= midiBusSoloBase && on) { const auto ch = numTracks + midiBusSoloBase - target; engine.setBusSolo (ch, ! engine.getBusSolo (ch)); engine.recordSwitchAudio (ch, true, engine.getBusSolo (ch)); }
                else if (target <= midiBusMuteBase && on) { const auto ch = numTracks + midiBusMuteBase - target; engine.setBusMute (ch, ! engine.getBusMute (ch)); engine.recordSwitchAudio (ch, false, engine.getBusMute (ch)); }
                else if (target <= midiSoloBase && on) { auto& s = engine.sequencer(); const auto t = midiSoloBase - target; s.setTrackSolo (t, ! s.getTrackSolo (t)); engine.recordSwitchAudio (t, true, s.getTrackSolo (t)); }
                else if (target <= midiMuteBase && on) { auto& s = engine.sequencer(); const auto t = midiMuteBase - target; s.setTrackMute (t, ! s.getTrackMute (t)); engine.recordSwitchAudio (t, false, s.getTrackMute (t)); }
                continue;
            }

        if (fromDev && msg.isNoteOnOrOff()) continue; // après la 1.1 : pas de boucle avec l'appareil des pistes MIDI
        // Pads du contrôleur en mode Scènes ou Couper (notes 36 à 51), comme les pads de l'interface
        // (seulement pour les notes qui iraient à la piste choisie)
        if (msg.isNoteOnOrOff() && toFocus)
        {
            const auto mode = (PadMode) padMode.load (std::memory_order_relaxed);
            const auto pad = msg.getNoteNumber() - firstPadNote;
            if (mode != PadMode::play)
            {
                if (msg.isNoteOn() && pad >= 0 && pad < 16)
                {
                    if (mode == PadMode::scenes)
                    {
                        const auto scene = padBank.load (std::memory_order_relaxed) * 16 + pad;
                        pendingSceneLaunch.store (scene);
                    }
                    else if (mode == PadMode::mute)
                    {
                        engine.sequencer().setTrackMute (pad, ! engine.sequencer().getTrackMute (pad));
                        engine.recordSwitchAudio (pad, false, engine.sequencer().getTrackMute (pad)); // v0.46
                    }
                    else if (mode == PadMode::solo) // 1.0 : solo des 16 pistes
                    {
                        engine.sequencer().setTrackSolo (pad, ! engine.sequencer().getTrackSolo (pad));
                        engine.recordSwitchAudio (pad, true, engine.sequencer().getTrackSolo (pad));
                    }
                    else if (mode == PadMode::steps) // focus est déjà borné à 0..15
                    {
                        // Pose ou retire le pas (mêmes valeurs atomiques que l'écran ; l'interface se met à jour au tick suivant)
                        auto& sq = engine.sequencer();
                        const auto lane = padLane.load (std::memory_order_relaxed);
                        const auto step = padPage.load (std::memory_order_relaxed) * 16 + pad;
                        const auto p = sq.getEditing (focus);
                        const auto vel = sq.getStepP (focus, p, lane, step);
                        sq.setStepP (focus, p, lane, step, vel > 0 ? 0 : juce::jlimit (1, 127, (int) std::lround (msg.getFloatVelocity() * 127.0f)));
                    }
                }
                continue;
            }
        }

        if (msg.isNoteOnOrOff())
        {
            for (int d = 0; d < numDest && numEvents < (int) midiEvents.size(); ++d)
            {
                const auto t = dest[d];
                auto note = msg.getNoteNumber();
                if (t == focus ? drums : engine.isPadNoteTrack (t)) { note -= firstPadNote; if (note < 0 || note > 15) continue; }
                InputEvent e;
                e.type = msg.isNoteOn() ? InputEvent::noteOn : InputEvent::noteOff;
                e.track = t; e.note = note; e.offset = offset;
                e.velocity = msg.isNoteOn() ? msg.getFloatVelocity() : 0.0f;
                e.record = rec && t == focus; // l'enregistrement en direct suit la piste choisie
                midiEvents[(size_t) numEvents++] = e;
            }
            // GOLEM 1.0 : note vers une piste interne T (adressée comme le séquenceur : 128 + T × 128 + note)
            for (int d = 0; d < numLaneDest && numEvents < (int) midiEvents.size(); ++d)
            {
                InputEvent e;
                e.type = msg.isNoteOn() ? InputEvent::noteOn : InputEvent::noteOff;
                e.track = laneDest[d][0];
                e.note = 128 + laneDest[d][1] * 128 + msg.getNoteNumber();
                e.offset = offset;
                e.velocity = msg.isNoteOn() ? msg.getFloatVelocity() : 0.0f;
                e.record = rec && e.track == focus;
                midiEvents[(size_t) numEvents++] = e;
            }
        }
        else if (msg.isProgramChange())
        {
            // 1.1 : pistes réglées sur ce canal → son n° (banque × 128 + programme + 1) de l'instrument ;
            // sinon, sur le canal des commandes : lancement d'une scène (comme avant)
            const auto prog = msg.getProgramChangeNumber();
            const int msb = bankMsb[(size_t) chan], lsb = bankLsb[(size_t) chan];
            // banque sur 14 bits ; un appareil qui n'envoie que le CC 0 (ou que le CC 32) donne la page directement
            const int page = msb > 0 && lsb > 0 ? msb * 128 + lsb : juce::jmax (msb, lsb);
            const int number = juce::jmin (0xFFFF, page * 128 + prog);
            bool toTrack = false;
            for (int t = 0; t < numTracks; ++t)
                if (trackCh[(size_t) t] == chan + 1)
                {
                    pendingPreset[(size_t) t].store (number + 1, std::memory_order_relaxed); // instrument : cible 0 dans les bits hauts
                    toTrack = true;
                }
            if (! toTrack && ctrlMsg && prog < numScenes) pendingSceneLaunch.store (prog); // lancée par le fil de l'interface (pas d'allocation ici)
        }
    }

    // Fill du séquenceur : seul le fil audio l'écrit (Fill, Break tenu, Break temporisé, MIDI compris)
    engine.sequencer().setFill (fillHeld.load (std::memory_order_relaxed) || breakHeld.load (std::memory_order_relaxed) || timed);
    bi.breakActive = breakHeld.load (std::memory_order_relaxed) || timed;

    // Entrée audio (piste « Entrée audio ») : copie, car le tampon sert aussi de sortie
    const auto numIn = getTotalNumInputChannels();
    const auto chunkMax = (int) monoOut.size();
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int pos = 0; pos < n; pos += chunkMax)
    {
        const auto len = std::min (chunkMax, n - pos);
        float* inL = silentIn.data();
        float* inR = silentIn.data() + chunkMax;
        if (numIn > 0)
        {
            std::copy (buffer.getReadPointer (0) + pos, buffer.getReadPointer (0) + pos + len, inL);
            const auto* srcR = buffer.getReadPointer (numIn > 1 ? 1 : 0) + pos;
            std::copy (srcR, srcR + len, inR);
        }
        else { std::fill (inL, inL + len, 0.0f); std::fill (inR, inR + len, 0.0f); }

        // événements de ce morceau du bloc
        int first = 0, count = 0;
        for (int i = 0; i < numEvents; ++i)
            if (midiEvents[(size_t) i].offset >= pos && midiEvents[(size_t) i].offset < pos + len) { if (count == 0) first = i; ++count; }
        for (int i = first; i < first + count; ++i) midiEvents[(size_t) i].offset -= pos;

        float* dstR = outR != nullptr ? outR + pos : monoOut.data();
        const double nowMsOut = juce::Time::getMillisecondCounterHiRes();
        engine.process (inL, inR, outL + pos, dstR, len, bi, count > 0 ? midiEvents.data() + first : nullptr, count);
        // après la 1.1 : messages des pistes MIDI de ce morceau, placés à leur échantillon dans le bloc
        {
            const auto& q = engine.midiOut;
            for (int k = 0; k < q.count; ++k)
            {
                const auto& e = q.ev[(size_t) k];
                const juce::uint8 raw[3] { e.b0, e.b1, e.b2 };
                midiOutBuf.addEvent (raw, juce::jlimit (1, 3, (int) e.len), juce::jlimit (0, n - 1, pos + e.offset));
                // écho exact de ce message renvoyé par l'appareil : reconnu à l'entrée (anti-boucle)
                if (const auto k = grv::midiKey (raw, juce::jlimit (1, 3, (int) e.len)); k != 0)
                {
                    sentKey[(size_t) sentHead] = k; sentAt[(size_t) sentHead] = nowMsOut;
                    sentHead = (sentHead + 1) % (int) sentKey.size();
                }
            }
        }
        if (outR == nullptr)
            for (int i = 0; i < len; ++i) outL[pos + i] = 0.5f * (outL[pos + i] + monoOut[(size_t) i]);
        // après le premier morceau, la position du logiciel hôte avance avec le séquenceur
        if (bi.host.playing) bi.host.ppq += (double) len * bi.bpm / 60.0 / getSampleRate();
    }
    // 1.1 (lot 3) : écoute du traitement de l'écran AUDIO (après la prise : celle-ci garde le son d'origine)
    monitorBusy.fetch_add (1);
    if (auto* mc = monitorChain.load(); mc != nullptr && outR != nullptr) mc->monitorProcess (outL, outR, n);
    monitorBusy.fetch_sub (1);
    // après la 1.1 : la sortie MIDI du bloc = les messages des pistes MIDI (jamais l'entrée renvoyée telle quelle)
    midi.clear();
    if (! midiOutBuf.isEmpty()) { midi.addEvents (midiOutBuf, 0, n, 0); midiOutBuf.clear(); }
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

double GrooveboxProcessor::setMonitorChain (const juce::var& chainVar)
{
    const auto chain = parseChain (chainVar);
    std::unique_ptr<engine::OfflineChain> next;
    double ms = 0.0;
    bool any = false;
    for (const auto& c : chain) any = any || (! c.type.empty() && ! c.bypass);
    // 1.1 : un bouton tourné (mêmes effets, dans le même ordre) : seuls les réglages changent, la chaîne
    // garde son état (queues, filtres) ; la reconstruire à chaque mouvement faisait craquer le son
    if (auto* cur = monitorChain.load(); any && cur != nullptr && cur->sameShape (chain))
    {
        cur->stageParams (chain);
        return monitorMs;
    }
    if (any)
    {
        next = std::make_unique<engine::OfflineChain>();
        const auto sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        if (! next->prepare (chain, sr, currentBpm.load(), false).empty()) next.reset();
        else ms = 1000.0 * next->latency() / sr;
    }
    auto* old = monitorChain.exchange (next.release());
    while (monitorBusy.load() > 0) juce::Thread::yield(); // le fil audio a lâché l'ancienne
    delete old;
    monitorMs = ms;
    return ms;
}

void GrooveboxProcessor::setMonitorSources (const juce::var& v)
{
    std::uint32_t tracks = 0, returns = 0;
    if (const auto* a = v.getProperty ("tracks", juce::var()).getArray())
        for (const auto& t : *a) if ((int) t >= 0 && (int) t < engine::numTracks) tracks |= 1u << (int) t;
    if (const auto* a = v.getProperty ("returns", juce::var()).getArray())
        for (const auto& r : *a) if ((int) r >= 0 && (int) r < engine::numReturns) returns |= 1u << (int) r;
    const bool input = (bool) v.getProperty ("input", false);
    const bool on = (bool) v.getProperty ("on", false) && (tracks != 0 || returns != 0 || input);
    engine.setMonitorSources (tracks, returns, input, on);
}

//==============================================================================
void GrooveboxProcessor::launchScene (int scene, bool otherSlot) noexcept
{
    if (scene < 0 || scene >= numScenes) return;
    const bool toB = (pXfade->load() > 0.5f) != otherSlot;
    auto* p = state.getParameter (toB ? ParamID::sceneB : ParamID::sceneA);
    p->setValueNotifyingHost (p->convertTo0to1 ((float) (scene + 1)));
}

void GrooveboxProcessor::setGlobal (const juce::String& id, float plainValue)
{
    if (id != ParamID::bpm && id != ParamID::swing && id != ParamID::xfade && id != ParamID::sceneA && id != ParamID::sceneB
        && ! id.startsWith ("macro") && ! id.startsWith ("vec")) return;
    if (auto* p = state.getParameter (id))
        if (std::isfinite (plainValue))
        {
            const auto norm = p->convertTo0to1 (juce::jlimit (p->getNormalisableRange().start, p->getNormalisableRange().end, plainValue));
            p->setValueNotifyingHost (norm);
            if (id == ParamID::xfade) engine.recordFaderUi (norm); // v0.46 : crossfader enregistré dans le morceau
        }
}

float GrooveboxProcessor::getGlobal (const juce::String& id) const
{
    if (auto* v = state.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

std::vector<GrooveboxProcessor::MacroLink> GrooveboxProcessor::getMacroLinks (int macro) const
{
    std::vector<MacroLink> out;
    if (macro < 0 || macro >= numMacros) return out;
    for (const auto& l : macroSlots[(size_t) macro])
        if (const auto g = l.global.load(); g >= 0) out.push_back ({ g, l.lo.load(), l.hi.load(), l.base.load(), l.ref.load() });
    return out;
}

void GrooveboxProcessor::setMacroLinks (int macro, const std::vector<MacroLink>& links, bool pushValue) noexcept
{
    if (macro < 0 || macro >= numMacros) return;
    auto& slots = macroSlots[(size_t) macro];
    const auto mv = pMacro[(size_t) macro]->load (std::memory_order_relaxed);
    const bool rel = macroRel[(size_t) macro].load();
    size_t k = 0;
    for (const auto& l : links)
    {
        if (k >= slots.size()) break;
        if (l.global < 0 || l.global >= numSlots || engine.describe (l.global) == nullptr) continue;
        const auto lo = std::isfinite (l.lo) ? std::clamp (l.lo, 0.0f, 1.0f) : 0.0f, hi = std::isfinite (l.hi) ? std::clamp (l.hi, 0.0f, 1.0f) : 1.0f;
        // ancrage du mode relatif : celui donné, sinon la valeur actuelle du réglage et de la macro
        const bool anchored = std::isfinite (l.base) && l.base >= 0.0f && std::isfinite (l.ref) && l.ref >= 0.0f;
        const auto base = anchored ? std::clamp (l.base, 0.0f, 1.0f) : engine.getParam (l.global);
        const auto ref = anchored ? std::clamp (l.ref, 0.0f, 1.0f) : mv;
        slots[k].global.store (-1); // pas de lecture d'un emplacement à moitié écrit par le fil audio
        slots[k].lo.store (lo); slots[k].hi.store (hi); slots[k].base.store (base); slots[k].ref.store (ref);
        slots[k].written.store (engine.getParam (l.global));
        slots[k].global.store (l.global);
        ++k;
    }
    for (; k < slots.size(); ++k) slots[k].global.store (-1);
    // relatif : jamais d'écriture immédiate (le réglage garde sa valeur)
    lastMacro[(size_t) macro].store (pushValue && ! rel ? -1.0f : mv);
}

void GrooveboxProcessor::moveInsert (int channel, int from, int to)
{
    if (channel < 0 || channel >= numChannels || from < 0 || to < 0 || from >= numInserts || to >= numInserts || from == to) return;
    engine.moveInsert (channel, from, to, nowMs());
    const auto remap = engine::Engine::insertSwapMap (channel, from, to);
    bool midiChanged = false;
    for (auto* map : { &ccMap, &noteMap })
        for (auto& m : *map)
            if (const auto t = m.load(); t >= 0)
                if (const auto n = remap (t); n != t) { m.store (n); midiChanged = true; }
    if (midiChanged) midiVersion.fetch_add (1);
    for (int m = 0; m < numMacros; ++m)
    {
        auto links = getMacroLinks (m);
        bool changed = false;
        for (auto& l : links) if (const auto n = remap (l.global); n != l.global) { l.global = n; changed = true; }
        if (changed) setMacroLinks (m, links, false);
    }
}

void GrooveboxProcessor::setMacroRelative (int macro, bool relative) noexcept
{
    if (macro < 0 || macro >= numMacros) return;
    macroRel[(size_t) macro].store (relative);
    // nouvel ancrage : chaque réglage part de sa valeur actuelle, à la position actuelle de la macro ;
    // en absolu, les réglages rejoignent leur plage au prochain mouvement de la macro
    auto links = getMacroLinks (macro);
    for (auto& l : links) l.base = l.ref = -1.0f;
    setMacroLinks (macro, links, false);
}

bool GrooveboxProcessor::toggleMacroTarget (int macro, int global) noexcept
{
    if (macro < 0 || macro >= numMacros || global < 0 || global >= numSlots) return false;
    auto links = getMacroLinks (macro);
    const auto it = std::find_if (links.begin(), links.end(), [global] (const MacroLink& l) { return l.global == global; });
    if (it != links.end()) { links.erase (it); setMacroLinks (macro, links, false); return false; }
    if ((int) links.size() >= macroLinks) return false;
    links.push_back ({ global, 0.0f, 1.0f }); // relatif : ancré à sa valeur actuelle (base = -1)
    setMacroLinks (macro, links, true);
    return true;
}

void GrooveboxProcessor::setMacroRange (int macro, int global, float lo, float hi) noexcept
{
    auto links = getMacroLinks (macro);
    for (auto& l : links) if (l.global == global) { l.lo = lo; l.hi = hi; }
    setMacroLinks (macro, links, true);
}

void GrooveboxProcessor::clearMacro (int macro) noexcept { setMacroLinks (macro, {}, false); }

//==============================================================================
// Adresses stables

GrooveboxProcessor::Address GrooveboxProcessor::addressOf (int global) const
{
    Address a;
    if (global < 0 || global >= numSlots) return a;
    a.channel = channelOf (global);
    const auto local = localOf (global);
    const ModuleDesc* d = nullptr;
    int index = 0;
    if (local < instrumentBase) { a.area = "mix"; d = &mixerDesc (a.channel); index = local - mixerBase; }
    else if (local < insertBase) { a.area = "inst"; d = engine.instrumentDesc (a.channel); index = local - instrumentBase; }
    else if (local < modSourceBase)
    {
        const auto slot = (local - insertBase) / insertSlots;
        a.area = "fx" + juce::String (slot + 1); d = engine.effectDesc (a.channel, slot); index = (local - insertBase) % insertSlots;
    }
    else if (local < modSourceBase + numModSources * modSourceSlots)
    {
        const auto src = (local - modSourceBase) / modSourceSlots;
        a.area = "mod" + juce::String (src + 1); d = &modSourceDesc(); index = (local - modSourceBase) % modSourceSlots;
    }
    else if (local >= padBypassBase && local < padBypassBase + numPads * padInserts)
    {
        // interrupteur d'un insert de pad : seulement s'il y a un effet
        const auto k = local - padBypassBase;
        if (a.channel >= numTracks || engine.padEffectDesc (a.channel, k / padInserts, k % padInserts) == nullptr) { a.channel = -1; return a; }
        a.area = "padbyp"; a.id = "b" + juce::String (k / padInserts + 1) + "_" + juce::String (k % padInserts + 1);
        return a;
    }
    else if (local == trackSoundSlot) // après la 1.1 : son du pas
    {
        if (a.channel >= numTracks || engine.instrumentDesc (a.channel) == nullptr) { a.channel = -1; return a; }
        a.area = "son"; a.id = "n";
        return a;
    }
    else if (local >= padInsertBase && a.channel < numTracks)
    {
        const auto k = (local - padInsertBase) / insertSlots;
        a.area = "pfx" + juce::String (k / padInserts + 1) + "_" + juce::String (k % padInserts + 1);
        d = engine.padEffectDesc (a.channel, k / padInserts, k % padInserts); index = (local - padInsertBase) % insertSlots;
    }
    if (d == nullptr || index < 0 || index >= (int) d->params.size()) { a.channel = -1; return a; }
    a.id = juce::String::fromUTF8 (d->params[(size_t) index].id.c_str());
    return a;
}

int GrooveboxProcessor::globalOf (const Address& a) const
{
    if (a.channel < 0 || a.channel >= numChannels) return -1;
    return engine.findParam (a.channel, a.area.toStdString(), a.id.toStdString());
}

//==============================================================================
// Projet de départ

void GrooveboxProcessor::clearProject()
{
    // v0.46 fix8 : nouveau projet : montage vide (les fichiers des prises restent sur le disque)
    montageDoc = {};
    montagePlayer.setActive (false);
    montagePlayer.setDoc (montageDoc, getSampleRate() > 0.0 ? getSampleRate() : 48000.0);
    const auto t = nowMs();
    engine.beginBatch();
    for (int tr = 0; tr < numTracks; ++tr)
    {
        engine.setInstrument (tr, "", t);
        engine.setTrackName (tr, "Piste " + std::to_string (tr + 1));
        engine.setTrackColor (tr, tr % 8);
        engine.setTrackOutput (tr, 0);
        engine.setTrackSpeech (tr, {}, t); // v0.33
        engine.clearTrackExtras (tr, t);
    }
    busesFromTree (engine, juce::ValueTree ("PROJECT")); // v0.31 : retours et groupes rouverts, noms d'origine
    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int s = 0; s < numInserts; ++s) engine.setInsert (ch, s, "", t);
        for (const auto& m : engine.getModulations (ch)) engine.removeModulation (ch, m.target, t);
        engine.applyDefaults (ch, mixerBase, mixerDesc (ch));
        engine.applyModSourceDefaults (ch);
    }
    for (int s = 0; s <= numScenes; ++s) { engine.clearScene (s, t); engine.setSceneMuted (s, false, t); }
    auto& seq = engine.sequencer();
    for (int tr = 0; tr < numTracks; ++tr)
    {
        seq.resetTrack (tr);
        seq.setTrackMute (tr, false); seq.setTrackSolo (tr, false);
        for (int l = 0; l < Sequencer::numLanes; ++l)
        {
            seq.setDirection (tr, l, Direction::forward); seq.setLaneMute (tr, l, false); seq.setLaneSolo (tr, l, false); seq.setLaneLockGlide (tr, l, false); seq.setLaneLockMute (tr, l, false);
        }
    }
    seq.setDivision (Division::sixteenth);
    seq.setRecordQuantize (100);
    seq.setLaunchQuant (2);
    seq.setMuteTails (false);
    engine.setSong ({}, t);
    engine.setRecordPlay (false); engine.setRecordMoves (false);
    for (auto& r : rowScene) r.store (-1);
    for (int m = 0; m < numMacros; ++m) { clearMacro (m); macroRel[(size_t) m].store (true); } // v0.46 : relatif par défaut
    engine.endBatch (t);
    projectVersion.fetch_add (1);
}

void GrooveboxProcessor::loadDefaultProject()
{
    clearProject();
    const auto t = nowMs();
    auto& seq = engine.sequencer();
    engine.beginBatch();

    engine.setInstrument (0, "drumA", t);  engine.setTrackName (0, "Batterie"); engine.setTrackColor (0, 0);
    engine.setInstrument (1, "acid", t);   engine.setTrackName (1, "Basse acide"); engine.setTrackColor (1, 1);
    engine.setInstrument (2, "polyj", t);  engine.setTrackName (2, "Accords"); engine.setTrackColor (2, 2);
    engine.setInstrument (3, "pads", t);   engine.setTrackName (3, "Nappe"); engine.setTrackColor (3, 3);
    engine.applyPreset (2, -1, "Accords house");

    // Retours : A réverbération, B écho ; bus principal : limiteur de sécurité
    engine.setInsert (numTracks + 0, 0, "reverb", t);
    engine.setInsert (numTracks + 1, 0, "delay", t);
    engine.setInsert (masterChannel, 0, "limiter", t);

    auto setPlain = [this] (int ch, const char* area, const char* id, float plain)
    {
        const auto g = engine.findParam (ch, area, id);
        if (const auto* d = engine.describe (g)) engine.setParam (g, d->toNorm (plain));
    };
    setPlain (2, "mix", "sendA", 25); setPlain (2, "mix", "sendB", 18);
    setPlain (3, "mix", "sendA", 40); setPlain (3, "mix", "volume", -4);
    setPlain (0, "mix", "sendA", 6);
    setPlain (1, "mix", "sendB", 10);

    // Batterie : grosse caisse, caisse claire, charleys
    for (int s : { 0, 4, 8, 12 }) seq.setStep (0, 0, s, 118);
    for (int s : { 4, 12 }) seq.setStep (0, 1, s, 105);
    for (int s = 2; s < 16; s += 4) seq.setStep (0, 4, s, 78);
    for (int s : { 1, 3, 5, 7, 9, 11, 13, 15 }) seq.setStep (0, 4, s, 40);
    seq.setStep (0, 5, 14, 70);
    seq.setStep (0, 2, 15, 60);
    engine.setStepExtra (0, 2, 15, Condition::ab, 2, 2, 100, t); // clap sur la 2e mesure de chaque paire

    // Basse acide : ligne avec glissés et accents
    static constexpr int bass[16] { 36, 0, 36, 48, 0, 36, 39, 0, 36, 0, 46, 36, 0, 43, 41, 0 };
    static constexpr int vel[16]  { 120, 0, 80, 110, 0, 85, 100, 0, 120, 0, 105, 80, 0, 110, 90, 0 };
    for (int s = 0; s < 16; ++s)
        if (bass[s] > 0)
        {
            seq.setStep (1, 0, s, vel[s]); seq.setNote (1, 0, s, bass[s]);
            seq.setLength8 (1, 0, s, (s == 3 || s == 13) ? 12 : 5); // deux notes liées : glissé
        }

    // Accords : la mineur 7 / ré mineur 7
    static constexpr int chordA[3] { 57, 60, 64 }, chordB[3] { 62, 65, 69 };
    for (int k = 0; k < 3; ++k)
    {
        for (int s : { 0, 3 }) { seq.setStep (2, k, s, 95); seq.setNote (2, k, s, chordA[k]); seq.setLength8 (2, k, s, 6); }
        for (int s : { 8, 11 }) { seq.setStep (2, k, s, 95); seq.setNote (2, k, s, chordB[k]); seq.setLength8 (2, k, s, 6); }
        // Nappe : une note longue par mesure
        seq.setStep (3, k, 0, 90); seq.setNote (3, k, 0, chordA[k] - 12); seq.setLength8 (3, k, 0, 120);
    }

    // Scènes de départ : 9 = filtre ouvert de la basse et réverbération plus large ; Break = batterie coupée
    if (auto g = engine.findParam (1, "inst", "cutoff"); g >= 0) engine.setSceneLock (8, g, engine.describe (g)->toNorm (2400.0f), t);
    if (auto g = engine.findParam (1, "inst", "envmod"); g >= 0) engine.setSceneLock (8, g, engine.describe (g)->toNorm (85.0f), t);
    if (auto g = engine.findParam (3, "mix", "sendA"); g >= 0) engine.setSceneLock (8, g, engine.describe (g)->toNorm (80.0f), t);
    if (auto g = engine.findParam (0, "mix", "volume"); g >= 0) engine.setSceneLock (breakScene, g, 0.0f, t);

    engine.endBatch (t);
    projectVersion.fetch_add (1);
}

//==============================================================================
// Échantillons

void GrooveboxProcessor::openSampleDecay (int track, int index)
{
    // Échantillon posé sur un pad de SYNTHÈSE d'une batterie sans machines : son déclin part au maximum,
    // l'échantillon se lit en entier, on le raccourcit ensuite au bouton Déclin (1.1). Les pads
    // « Échantillon » et les batteries à machines (réglages d'échantillon propres) ne changent pas.
    const auto* d = engine.instrumentDesc (track);
    if (d == nullptr || ! d->drumKit || index < 0 || index >= (int) d->padKinds.size() || d->padKinds[(size_t) index] == "sample") return;
    const auto prefix = "p" + std::string (index < 9 ? "0" : "") + std::to_string (index + 1) + "_";
    int decay = -1;
    for (size_t k = 0; k < d->params.size(); ++k)
    {
        if (d->params[k].id == prefix + "mach") return;
        if (d->params[k].id == prefix + "decay") decay = (int) k;
    }
    if (decay >= 0 && decay < instrumentSlots) engine.setParam (globalSlot (track, instrumentBase + decay), 1.0f);
}

juce::String GrooveboxProcessor::loadSample (int track, int index, const juce::String& fileName, juce::MemoryBlock fileBytes)
{
    if (track < 0 || track >= numTracks || index < 0 || index >= maxSamples) return "Piste inconnue";
    if (fileBytes.getSize() == 0) return "Fichier vide";
    if (fileBytes.getSize() > (size_t) 64 * 1024 * 1024) return "Fichier trop lourd (64 Mo au plus)";
    const auto* first = (const std::uint8_t*) fileBytes.getData();
    return loadSampleBytes (track, index, fileName, std::make_shared<const std::vector<std::uint8_t>> (first, first + fileBytes.getSize()));
}

juce::String GrooveboxProcessor::loadSampleBytes (int track, int index, const juce::String& fileName, std::shared_ptr<const std::vector<std::uint8_t>> bytes)
{
    if (track < 0 || track >= numTracks || index < 0 || index >= maxSamples) return "Piste inconnue";
    if (bytes == nullptr || bytes->empty()) return "Fichier vide";
    // v0.49 : firmware du FM-1 (.fwsc) — pas un son : la machine émulée est créée ici, hors du fil audio
    if (instruments::fmplayer::looksLikeFirmware (fileName.toStdString(), bytes->data(), bytes->size())
        || fileName.endsWithIgnoreCase (".fwsc"))
        return loadFirmwareBytes (track, index, fileName, std::move (bytes));
    std::unique_ptr<juce::AudioFormatReader> reader (
        formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (bytes->data(), bytes->size(), false)));
    if (reader == nullptr || reader->sampleRate < 8000.0 || reader->sampleRate > 384000.0)
        return juce::String::fromUTF8 ("Format non reconnu : WAV, AIFF, FLAC, OGG ou MP3");

    // 3 minutes au plus, et 10 millions d'images au plus (mémoire bornée quelle que soit la fréquence)
    const auto maxLength = std::min ((juce::int64) (reader->sampleRate * 180.0), (juce::int64) 10'000'000);
    const auto length = (int) std::min (reader->lengthInSamples, maxLength);
    if (length <= 4) return "Fichier audio vide";

    const auto channels = (int) juce::jlimit (1u, 2u, reader->numChannels);
    juce::AudioBuffer<float> temp (channels, length);
    reader->read (&temp, 0, length, 0, true, channels > 1);

    auto s = std::make_unique<SampleBuffer>();
    s->sampleRate = reader->sampleRate;
    s->data.assign (temp.getReadPointer (0), temp.getReadPointer (0) + length);
    if (channels > 1) s->right.assign (temp.getReadPointer (1), temp.getReadPointer (1) + length);
    for (auto& v : s->data) if (! std::isfinite (v)) v = 0.0f;
    for (auto& v : s->right) if (! std::isfinite (v)) v = 0.0f;
    // nom : sans chemin ni caractères de contrôle, 80 caractères au plus
    auto name = fileName.fromLastOccurrenceOf ("/", false, false).fromLastOccurrenceOf ("\\", false, false)
                    .removeCharacters ("\r\n\t\"<>").substring (0, 80);
    s->name = name.toStdString();
    s->original = std::move (bytes); // partagé avec l'historique : pas de copie du fichier
    s->onsets = synth::computeOnsets (*s, 64); // découpes « attaques » du sampler de performance
    engine.setSample (track, index, std::move (s), nowMs());
    projectVersion.fetch_add (1);
    return {};
}

juce::String GrooveboxProcessor::loadFirmwareBytes (int track, int index, const juce::String& fileName, std::shared_ptr<const std::vector<std::uint8_t>> bytes)
{
    if (index != 0) return juce::String::fromUTF8 ("Un firmware se charge dans l'emplacement 1");
    const auto* inst = engine.instrumentAt (track);
    if (inst == nullptr || ! inst->acceptsFirmware())
        return juce::String::fromUTF8 ("Un firmware .fwsc se charge sur une piste FM Player");
    if (! instruments::fmplayer::looksLikeFirmware (fileName.toStdString(), bytes->data(), bytes->size()))
        return juce::String::fromUTF8 ("Firmware non reconnu (fichier .fwsc de 4 Ko \xC3\xA0 8 Mo)");
   #if GROOVEBOX_FM1
    auto s = std::make_unique<SampleBuffer>();
    s->firmware = true;
    s->sampleRate = 44100.0;
    std::vector<std::uint8_t> flash;
    if (auto& pf = pendingFlash[(size_t) track]; pf != nullptr) { flash = *pf; pf.reset(); } // mémoire flash du projet
    grv::startupMark (("FM Player : demarrage du firmware " + fileName.fromLastOccurrenceOf ("/", false, false).substring (0, 60) + " (flash " + juce::String ((int) flash.size()) + " o)").toRawUTF8());
    const auto err = instruments::fmplayer::start (*bytes, flash, s->runtime, fileName.toStdString()); // nom : réglages « Auto »
    grv::startupMark (err.empty() ? "FM Player : firmware demarre" : "FM Player : firmware refuse");
    if (! err.empty()) return juce::String::fromUTF8 (err.c_str());
    auto name = fileName.fromLastOccurrenceOf ("/", false, false).fromLastOccurrenceOf ("\\", false, false)
                    .removeCharacters ("\r\n\t\"<>").substring (0, 80);
    s->name = name.toStdString();
    s->original = std::move (bytes);
    engine.setSample (track, index, std::move (s), nowMs());
    projectVersion.fetch_add (1);
    return {};
   #else
    juce::ignoreUnused (track, bytes);
    return juce::String::fromUTF8 ("Lecteur de firmwares absent de cette version (compil\xC3\xA9" "e sans Rust)");
   #endif
}

void GrooveboxProcessor::keepVoiceCaptures()
{
    for (int t = 0; t < numTracks; ++t)
    {
        const auto* voc = dynamic_cast<const instruments::VocoderSynth*> (engine.instrumentAt (t));
        if (voc == nullptr || ! voc->captureToKeep()) continue;
        std::vector<float> data;
        double rate = 48000.0;
        if (! voc->copyCapture (data, rate)) continue;
        // WAV 24 bits mono, emplacement 1 de la piste : la phrase reste avec le projet (et l'historique)
        juce::MemoryBlock wav;
        {
            juce::AudioBuffer<float> buf (1, (int) data.size());
            buf.copyFrom (0, 0, data.data(), (int) data.size());
            auto* stream = new juce::MemoryOutputStream (wav, false);
            juce::WavAudioFormat fmt;
            std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (stream, rate, 1, 24, {}, 0));
            if (w == nullptr) { delete stream; voc->captureKeepFailed(); continue; }
            w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
        }
        const auto err = loadSample (t, 0, juce::String::fromUTF8 ("Prise de son piste ") + juce::String (t + 1) + ".wav", std::move (wav));
        if (err.isNotEmpty()) voc->captureKeepFailed();
    }
}

juce::String GrooveboxProcessor::keepRecording (int track, int lane, int index)
{
    const auto* d = engine.instrumentDesc (track);
    const auto* inst = engine.instrumentAt (track);
    if (d == nullptr || d->type != "octa" || inst == nullptr) return juce::String::fromUTF8 ("La piste n'a pas de sampler de performance");
    if (index < 0 || index >= maxSamples) return juce::String::fromUTF8 ("Emplacement inconnu");
    std::vector<float> l, r;
    double rate = 48000.0;
    if (! inst->copyRecording (lane, l, r, rate)) return juce::String::fromUTF8 ("Rien d'enregistré (ou enregistrement en cours)");

    // fichier WAV 24 bits : l'enregistrement est gardé avec le projet comme un échantillon chargé
    juce::MemoryBlock wav;
    {
        juce::AudioBuffer<float> buf (2, (int) l.size());
        buf.copyFrom (0, 0, l.data(), (int) l.size());
        buf.copyFrom (1, 0, r.data(), (int) r.size());
        auto* stream = new juce::MemoryOutputStream (wav, false);
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (stream, rate, 2, 24, {}, 0));
        if (w == nullptr) { delete stream; return juce::String::fromUTF8 ("Écriture impossible"); }
        w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }
    const auto name = juce::String::fromUTF8 ("Enregistrement T") + juce::String (lane + 1) + ".wav";
    return loadSample (track, index, name, std::move (wav));
}

/** WAV 24 bits en mémoire (échantillon modifié ou extrait, gardé avec le projet comme un fichier chargé). */
static bool encodeWav24 (const std::vector<float>& l, const std::vector<float>& r, bool stereo, double sampleRate, juce::MemoryBlock& wav)
{
    const auto len = (int) l.size(), ch = stereo ? 2 : 1;
    juce::AudioBuffer<float> buf (ch, len);
    buf.copyFrom (0, 0, l.data(), len);
    if (stereo) buf.copyFrom (1, 0, r.data(), len);
    auto* stream = new juce::MemoryOutputStream (wav, false);
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (stream, sampleRate, (unsigned) ch, 24, {}, 0));
    if (w == nullptr) { delete stream; return false; }
    w->writeFromAudioSampleBuffer (buf, 0, len);
    return true;
}

juce::String GrooveboxProcessor::editSample (int track, int index, int op, double a, double b)
{
    if (track < 0 || track >= numTracks || index < 0 || index >= maxSamples) return juce::String::fromUTF8 ("Emplacement inconnu");
    const auto* src = engine.getSample (track, index);
    if (src == nullptr || src->data.size() < 16) return juce::String::fromUTF8 ("Emplacement vide");
    const auto n = (int) src->data.size();
    const bool stereo = src->right.size() == src->data.size();
    std::vector<float> l = src->data, r = stereo ? src->right : std::vector<float> {};
    a = juce::jlimit (0.0, 1.0, a); b = juce::jlimit (0.0, 1.0, b);
    if (b < a) std::swap (a, b);
    if (b - a < 1.0e-4) { a = 0.0; b = 1.0; }
    const auto i0 = juce::jlimit (0, n - 1, (int) std::floor (a * n)), i1 = juce::jlimit (i0 + 1, n, (int) std::ceil (b * n));
    auto each = [&] (auto&& f) { f (l); if (stereo) f (r); };
    switch (op)
    {
        case 0: each ([&] (std::vector<float>& v) { std::reverse (v.begin() + i0, v.begin() + i1); }); break;
        case 1:
        {
            float peak = 0.0f;
            each ([&] (std::vector<float>& v) { for (int i = i0; i < i1; ++i) peak = std::max (peak, std::abs (v[(size_t) i])); });
            if (peak < 1.0e-6f) return juce::String::fromUTF8 ("Sélection silencieuse");
            const auto g = 0.966f / peak; // -0,3 dB crête
            each ([&] (std::vector<float>& v) { for (int i = i0; i < i1; ++i) v[(size_t) i] *= g; });
            break;
        }
        case 2: case 3:
        {
            const auto len = std::max (1, i1 - i0);
            each ([&] (std::vector<float>& v)
            {
                for (int i = i0; i < i1; ++i)
                {
                    const auto t = (double) (i - i0) / (double) len;
                    const auto g = op == 2 ? std::sin (0.5 * juce::MathConstants<double>::pi * t) : std::cos (0.5 * juce::MathConstants<double>::pi * t);
                    v[(size_t) i] = (float) (v[(size_t) i] * g * g);
                }
                if (op == 2) for (int i = 0; i < i0; ++i) v[(size_t) i] = 0.0f;   // avant le fondu d'entrée : silence
                else for (int i = i1; i < n; ++i) v[(size_t) i] = 0.0f;           // après le fondu de sortie : silence
            });
            break;
        }
        case 4:
            if (i1 - i0 < 16) return juce::String::fromUTF8 ("Sélection trop courte");
            each ([&] (std::vector<float>& v) { v = std::vector<float> (v.begin() + i0, v.begin() + i1); });
            break;
        case 5: each ([&] (std::vector<float>& v) { std::fill (v.begin() + i0, v.begin() + i1, 0.0f); }); break;
        default: return juce::String::fromUTF8 ("Opération inconnue");
    }
    // WAV 24 bits : l'échantillon modifié est gardé avec le projet comme un fichier chargé
    juce::MemoryBlock wav;
    if (! encodeWav24 (l, r, stereo, src->sampleRate, wav)) return juce::String::fromUTF8 ("Écriture impossible");
    const auto name = juce::String::fromUTF8 (src->name.c_str());
    pushSampleVersion (track, index); // 1.1 : Annuler (8 versions)
    return loadSample (track, index, name, std::move (wav));
}

//==============================================================================
// 1.1 (lots 1 et 2) : TRAITER un échantillon par une chaîne d'effets, hors temps réel
juce::File GrooveboxProcessor::processedFolder()
{
   #if JUCE_ANDROID
    auto dir = Recorder::rootFolder().getChildFile (juce::String::fromUTF8 ("Son traité"));
   #else
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Groovebox").getChildFile (juce::String::fromUTF8 ("Son traité"));
   #endif
    dir.createDirectory();
    return dir;
}

std::vector<engine::ChainSlot> GrooveboxProcessor::parseChain (const juce::var& chain)
{
    std::vector<engine::ChainSlot> out;
    if (const auto* arr = chain.getArray())
        for (const auto& v : *arr)
        {
            if ((int) out.size() >= 4) break;
            engine::ChainSlot s;
            s.type = v.getProperty ("type", "").toString().substring (0, 40).toStdString();
            s.bypass = (bool) v.getProperty ("bypass", false);
            if (const auto* vals = v.getProperty ("values", juce::var()).getArray())
                for (const auto& x : *vals)
                {
                    if (s.plain.size() >= 256) break;
                    const auto f = (float) (double) x;
                    s.plain.push_back (std::isfinite (f) ? f : 0.0f);
                }
            if (! s.type.empty() && engine::findEffect (s.type) == nullptr) continue; // type inconnu : ignoré
            out.push_back (std::move (s));
        }
    return out;
}

void GrooveboxProcessor::pushSampleVersion (int track, int index)
{
    const auto* src = engine.getSample (track, index);
    if (src == nullptr || src->original == nullptr) return;
    auto& h = sampleHistory[track * maxSamples + index];
    h.push_back ({ juce::String::fromUTF8 (src->name.c_str()), src->original });
    while (h.size() > 8) h.pop_front();
}

int GrooveboxProcessor::sampleUndoCount (int track, int index) const
{
    const auto it = sampleHistory.find (track * maxSamples + index);
    return it == sampleHistory.end() ? 0 : (int) it->second.size();
}

juce::String GrooveboxProcessor::sampleUndo (int track, int index)
{
    const auto it = sampleHistory.find (track * maxSamples + index);
    if (it == sampleHistory.end() || it->second.empty()) return juce::String::fromUTF8 ("Rien à annuler");
    auto v = it->second.back();
    it->second.pop_back();
    return loadSampleBytes (track, index, v.name, v.bytes);
}

juce::String GrooveboxProcessor::procStart (int track, int index, double a, double b, const juce::var& chainVar, bool keepTail, bool preview, bool keepOriginal)
{
    if (proc->busy.load()) return juce::String::fromUTF8 ("Un traitement est déjà en cours");
    if (track < 0 || track >= numTracks || index < 0 || index >= maxSamples) return juce::String::fromUTF8 ("Emplacement inconnu");
    const auto* src = engine.getSample (track, index);
    if (src == nullptr || src->data.size() < 16) return juce::String::fromUTF8 ("Emplacement vide");
    auto chain = parseChain (chainVar);
    bool any = false;
    for (const auto& c : chain) any = any || (! c.type.empty() && ! c.bypass);
    if (! any) return juce::String::fromUTF8 ("Choisissez au moins un effet");
    if (procThread.joinable()) procThread.join();

    const auto n = (int) src->data.size();
    const bool stereo = src->right.size() == src->data.size();
    a = juce::jlimit (0.0, 1.0, a); b = juce::jlimit (0.0, 1.0, b);
    if (b < a) std::swap (a, b);
    if (b - a < 1.0e-4) { a = 0.0; b = 1.0; }
    const auto i0 = juce::jlimit (0, n - 1, (int) std::floor (a * n)), i1 = juce::jlimit (i0 + 1, n, (int) std::ceil (b * n));
    if (i1 - i0 < 16) return juce::String::fromUTF8 ("Sélection trop courte");

    proc = std::make_unique<ProcJob>();
    auto* job = proc.get();
    job->track = track; job->index = index; job->preview = preview; job->keepOriginal = keepOriginal;
    job->name = juce::String::fromUTF8 (src->name.c_str());
    job->rate = src->sampleRate;
    job->busy.store (true);
    std::vector<float> L = src->data, R = stereo ? src->right : std::vector<float> {};
    const double bpm = currentBpm.load();
    procThread = std::thread ([job, chain = std::move (chain), L = std::move (L), R = std::move (R), stereo, n, i0, i1, keepTail, bpm]() mutable
    {
        engine::OfflineChain oc;
        auto err = oc.prepare (chain, job->rate, bpm);
        if (err.empty())
        {
            const int m = i1 - i0;
            auto res = oc.render (L.data() + i0, stereo ? R.data() + i0 : nullptr, m, keepTail, 20.0, &job->progress, &job->cancel);
            if (! res.error.empty()) err = res.error;
            else
            {
                const bool outStereo = stereo || ! res.right.empty();
                const auto& pl = res.left; const auto& pr = res.right.empty() ? res.left : res.right;
                const int len = (int) pl.size();
                // son entier : début intact, sélection traitée (bords fondus sur 5 ms), queue mêlée à la suite
                const int total = std::max (n, i0 + len);
                std::vector<float> ol ((size_t) total, 0.0f), orr (outStereo ? (size_t) total : 0, 0.0f);
                for (int i = 0; i < n; ++i) { ol[(size_t) i] = L[(size_t) i]; if (outStereo) orr[(size_t) i] = stereo ? R[(size_t) i] : L[(size_t) i]; }
                const bool whole = i0 == 0 && i1 == n;
                const int fade = whole ? 0 : std::min (m / 4, (int) (0.005 * job->rate));
                for (int i = 0; i < len; ++i)
                {
                    const int s = i0 + i;
                    float w = 1.0f; // poids du traité dans la sélection
                    if (fade > 0 && i < fade) w = (float) i / (float) fade;
                    if (fade > 0 && i >= m - fade && i < m) w = (float) (m - i) / (float) fade;
                    if (i < m)
                    {
                        ol[(size_t) s] = ol[(size_t) s] * (1.0f - w) + pl[(size_t) i] * w;
                        if (outStereo) orr[(size_t) s] = orr[(size_t) s] * (1.0f - w) + pr[(size_t) i] * w;
                    }
                    else
                    {   // queue : s'ajoute à ce qui suit la sélection
                        ol[(size_t) s] += pl[(size_t) i];
                        if (outStereo) orr[(size_t) s] += pr[(size_t) i];
                    }
                }
                job->l = std::move (ol); job->r = std::move (orr);
                job->latency = res.latency; job->tail = res.tail;
            }
        }
        job->error = juce::String::fromUTF8 (err.c_str());
        job->ready.store (err.empty());
        job->busy.store (false);
    });
    return {};
}

void GrooveboxProcessor::procCancel()
{
    proc->cancel.store (true);
}

void GrooveboxProcessor::procAudition (bool play)
{
    if (! play || ! proc->ready.load() || proc->busy.load() || proc->l.empty()) { engine.setAudition (nullptr); return; }
    auto b = std::make_shared<engine::SampleBuffer>();
    b->data = proc->l; b->right = proc->r; b->sampleRate = proc->rate; b->name = proc->name.toStdString();
    engine.setAudition (b.get());
    auditionPrev = std::move (auditionHold); // l'ancien reste vivant le temps que le moteur le lâche
    auditionHold = std::move (b);
}

juce::var GrooveboxProcessor::procStatus()
{
    auto* o = new juce::DynamicObject();
    auto* job = proc.get();
    const bool busy = job->busy.load();
    if (! busy && procThread.joinable()) procThread.join();
    // résultat prêt : pré-écoute, ou remplacement de l'échantillon (une seule fois)
    if (! busy && job->ready.load() && ! job->applied)
    {
        job->applied = true;
        if (job->preview) procAudition (true);
        else
        {
            const auto* src = engine.getSample (job->track, job->index);
            if (job->keepOriginal && src != nullptr && src->original != nullptr)
            {
                auto dir = processedFolder().getChildFile ("Originaux");
                dir.createDirectory();
                auto name = juce::File::createLegalFileName (job->name.isEmpty() ? juce::String ("son") : job->name);
                auto f = dir.getNonexistentChildFile (juce::File (name).getFileNameWithoutExtension() + juce::Time::getCurrentTime().formatted (" %Y-%m-%d %Hh%M"),
                                                      juce::File (name).getFileExtension().isEmpty() ? ".wav" : juce::File (name).getFileExtension(), false);
                f.replaceWithData (src->original->data(), src->original->size());
            }
            pushSampleVersion (job->track, job->index);
            juce::MemoryBlock wav;
            if (! encodeWav24 (job->l, job->r, job->r.size() == job->l.size() && ! job->r.empty(), job->rate, wav)) job->error = juce::String::fromUTF8 ("Écriture impossible");
            else if (auto e = loadSample (job->track, job->index, job->name, std::move (wav)); e.isNotEmpty()) job->error = e;
        }
    }
    o->setProperty ("busy", busy);
    o->setProperty ("progress", (double) job->progress.load());
    o->setProperty ("ready", job->ready.load());
    o->setProperty ("preview", job->preview);
    o->setProperty ("error", job->error);
    o->setProperty ("track", job->track);
    o->setProperty ("index", job->index);
    o->setProperty ("seconds", job->l.empty() ? 0.0 : (double) job->l.size() / job->rate);
    o->setProperty ("latencyMs", job->rate > 0 ? 1000.0 * job->latency / job->rate : 0.0);
    o->setProperty ("tailSeconds", job->rate > 0 ? (double) job->tail / job->rate : 0.0);
    o->setProperty ("auditioning", engine.auditionPlaying());
    o->setProperty ("undo", job->track >= 0 ? sampleUndoCount (job->track, job->index) : 0);
    return juce::var (o);
}

juce::String GrooveboxProcessor::copySample (int fromTrack, int fromIndex, int toTrack, int toIndex)
{
    if (fromTrack < 0 || fromTrack >= numTracks || toTrack < 0 || toTrack >= numTracks || fromIndex < 0 || fromIndex >= maxSamples || toIndex < 0 || toIndex >= maxSamples)
        return juce::String::fromUTF8 ("Emplacement inconnu");
    if (fromTrack == toTrack && fromIndex == toIndex) return {};
    const auto* src = engine.getSample (fromTrack, fromIndex);
    if (src == nullptr || src->original == nullptr) return juce::String::fromUTF8 ("Emplacement vide");
    return loadSampleBytes (toTrack, toIndex, juce::String::fromUTF8 (src->name.c_str()), src->original);
}

juce::String GrooveboxProcessor::copySampleRegion (int fromTrack, int fromIndex, double a, double b, int toTrack, int toIndex)
{
    if (fromTrack < 0 || fromTrack >= numTracks || toTrack < 0 || toTrack >= numTracks || fromIndex < 0 || fromIndex >= maxSamples || toIndex < 0 || toIndex >= maxSamples)
        return juce::String::fromUTF8 ("Emplacement inconnu");
    const auto* src = engine.getSample (fromTrack, fromIndex);
    if (src == nullptr || src->data.size() < 16) return juce::String::fromUTF8 ("Emplacement vide");
    const auto n = (int) src->data.size();
    const bool stereo = src->right.size() == src->data.size();
    a = juce::jlimit (0.0, 1.0, a); b = juce::jlimit (0.0, 1.0, b);
    if (b < a) std::swap (a, b);
    if (b - a < 1.0e-4) { a = 0.0; b = 1.0; }
    const auto i0 = juce::jlimit (0, n - 1, (int) std::floor (a * n)), i1 = juce::jlimit (i0 + 1, n, (int) std::ceil (b * n));
    if (i1 - i0 < 16) return juce::String::fromUTF8 ("Sélection trop courte");
    std::vector<float> l (src->data.begin() + i0, src->data.begin() + i1), r;
    if (stereo) r.assign (src->right.begin() + i0, src->right.begin() + i1);
    juce::MemoryBlock wav;
    if (! encodeWav24 (l, r, stereo, src->sampleRate, wav)) return juce::String::fromUTF8 ("Écriture impossible");
    auto name = juce::String::fromUTF8 (src->name.c_str());
    if (name.containsChar ('.')) name = name.upToLastOccurrenceOf (".", false, false);
    return loadSample (toTrack, toIndex, name + " (extrait).wav", std::move (wav));
}

int GrooveboxProcessor::autoSlice (int track, bool onTransients, int count)
{
    const auto* d = engine.instrumentDesc (track);
    const auto* s = engine.getSample (track, 0);
    if (d == nullptr || d->type != "sampler" || s == nullptr) return 0;
    const auto slices = synth::computeSlices (*s, onTransients, count);
    for (int z = 0; z < instruments::slicer::numZones; ++z)
    {
        const auto a = z < (int) slices.size() ? slices[(size_t) z].first * 100.0f : 0.0f;
        const auto b = z < (int) slices.size() ? slices[(size_t) z].second * 100.0f : 0.0f;
        const auto gs = engine.findParam (track, "inst", instruments::slicer::zoneId (z, "start"));
        const auto ge = engine.findParam (track, "inst", instruments::slicer::zoneId (z, "end"));
        if (gs >= 0) engine.setParam (gs, engine.describe (gs)->toNorm (a));
        if (ge >= 0) engine.setParam (ge, engine.describe (ge)->toNorm (b));
    }
    return (int) slices.size();
}

//==============================================================================
// Sauvegarde : tout est rangé par identifiants stables (canal, zone, réglage), pas par position.

juce::ValueTree GrooveboxProcessor::projectToTree (bool withSamples) const
{
    juce::ValueTree project ("PROJECT");
    // version 2 : 12 types de bande dans l'égaliseur (8 en version 1)
    // version 3 : 14 algorithmes dans la distorsion (11 avant)
    // version 4 : 20 modes d'arpège (8 avant) ; version 5 : réserve de 6 dB (poly à chorus, wave morph) ;
    // version 6 : division par pattern ; version 7 : divisions au tempo jusqu'à 16 mesures ;
    // version 8 : 64 ondes pour l'oscillateur 3 du wave morph
    project.setProperty ("version", 20, nullptr); // 20 : déclin des pads de synthèse appliqué à leur échantillon ; 19 : 3 modes de LFO ajoutés (une fois, tenu, demi-cycle) ; 18 : Pieuvre 8 à 8 pistes internes (anciennes pistes Pieuvre regroupées) ; 17 : réserve commune de Pieuvre 8 (153 choix d'emplacement) ; 16 : Hopscotch à 64 pas (51 choix par pas) ; 15 : 12 algorithmes dans l'Atmosphère (5 avant) ; 12 : position W (onde choisie) dans les générateurs de l'orbiteur ; 13 : bus de groupe (canaux 21 à 24) ; 14 : « Prise de son » ajoutée aux sources des vocodeurs

    for (int t = 0; t < numTracks; ++t)
    {
        const auto& info = engine.getTrackInfo (t);
        juce::ValueTree tr ("TRACK");
        tr.setProperty ("i", t, nullptr);
        tr.setProperty ("name", juce::String::fromUTF8 (info.name.c_str()), nullptr);
        tr.setProperty ("color", info.color, nullptr);
        if (const auto out = engine.getTrackOutput (t); out != 0) tr.setProperty ("out", out, nullptr); // v0.24 : bus de groupe
        if (! info.speech.empty()) tr.setProperty ("speech", juce::String::fromUTF8 (info.speech.c_str()), nullptr); // v0.33 : phrase tapée
        if (! info.labels.empty()) tr.setProperty ("labels", juce::String::fromUTF8 (info.labels.c_str()), nullptr); // après la 1.1 : sections de la piste MIDI
        tr.setProperty ("instrument", juce::String (info.instrumentType), nullptr);
        tr.setProperty ("preset", juce::String::fromUTF8 (info.preset.c_str()), nullptr);
       #if GROOVEBOX_FM1
        // v0.49 : FM Player — ce que le firmware a écrit dans sa mémoire flash (ses sons, ses réglages)
        if (withSamples)
            if (const auto* fp = dynamic_cast<const instruments::FmPlayer*> (engine.instrumentAt (t)); fp != nullptr && fp->machine() != nullptr)
                if (const auto need = fm1_sectors (fp->machine(), nullptr, 0); need > 0 && need <= (size_t) 16 * 1024 * 1024)
                {
                    juce::MemoryBlock mb (need);
                    if (fm1_sectors (fp->machine(), (std::uint8_t*) mb.getData(), mb.getSize()) == need)
                        tr.setProperty ("fm1flash", mb.toBase64Encoding(), nullptr);
                }
       #endif
        project.appendChild (tr, nullptr);
    }
    // v0.31 : retours et groupes — coupure, solo, nom
    for (int ch = numTracks; ch < numChannels; ++ch)
    {
        const auto name = ch >= groupBase ? engine.getGroupName (ch - groupBase) : std::string();
        if (! engine.getBusMute (ch) && ! engine.getBusSolo (ch) && name.empty()) continue;
        juce::ValueTree b ("BUS");
        b.setProperty ("c", ch, nullptr);
        if (engine.getBusMute (ch)) b.setProperty ("mute", 1, nullptr);
        if (engine.getBusSolo (ch)) b.setProperty ("solo", 1, nullptr);
        if (! name.empty()) b.setProperty ("name", juce::String::fromUTF8 (name.c_str()), nullptr);
        project.appendChild (b, nullptr);
    }
    for (int ch = 0; ch < numChannels; ++ch)
        for (int s = 0; s < numInserts; ++s)
            if (const auto& type = engine.getInsertTypes (ch)[(size_t) s]; ! type.empty())
            {
                juce::ValueTree ins ("INSERT");
                ins.setProperty ("c", ch, nullptr); ins.setProperty ("s", s, nullptr);
                ins.setProperty ("type", juce::String (type), nullptr);
                ins.setProperty ("preset", juce::String::fromUTF8 (engine.getInsertPresets (ch)[(size_t) s].c_str()), nullptr);
                project.appendChild (ins, nullptr);
            }
    // inserts des pads de batterie
    for (int tr = 0; tr < numTracks; ++tr)
        for (int pad = 0; pad < numPads; ++pad)
            for (int k = 0; k < padInserts; ++k)
                if (const auto& type = engine.getPadInsertType (tr, pad, k); ! type.empty())
                {
                    juce::ValueTree ins ("PADFX");
                    ins.setProperty ("t", tr, nullptr); ins.setProperty ("p", pad, nullptr); ins.setProperty ("s", k, nullptr);
                    ins.setProperty ("type", juce::String (type), nullptr);
                    project.appendChild (ins, nullptr);
                }

    // Réglages : seulement ceux qui diffèrent de la valeur d'origine
    juce::ValueTree params ("PARAMS");
    for (int g = 0; g < numSlots; ++g)
    {
        const auto* d = engine.describe (g);
        if (d == nullptr || (localOf (g) >= modAmountBase && localOf (g) < modAmountBase + numModAssigns)) continue; // profondeurs : avec les modulations
        const auto v = engine.getParam (g);
        if (std::abs (v - d->toNorm (d->def)) < 1.0e-6f) continue;
        const auto a = addressOf (g);
        if (a.channel < 0) continue;
        juce::ValueTree p ("P");
        p.setProperty ("c", a.channel, nullptr); p.setProperty ("a", a.area, nullptr); p.setProperty ("i", a.id, nullptr);
        p.setProperty ("v", (double) v, nullptr);
        params.appendChild (p, nullptr);
    }
    project.appendChild (params, nullptr);

    for (int ch = 0; ch < numChannels; ++ch)
        for (const auto& m : engine.getModulations (ch))
        {
            const auto a = addressOf (globalSlot (ch, m.target));
            if (a.channel < 0) continue;
            juce::ValueTree mod ("MOD");
            mod.setProperty ("c", ch, nullptr); mod.setProperty ("src", m.source, nullptr);
            mod.setProperty ("a", a.area, nullptr); mod.setProperty ("i", a.id, nullptr);
            mod.setProperty ("amount", (double) (engine.getParam (globalSlot (ch, modAmountBase + m.amountIndex)) * 2.0f - 1.0f), nullptr);
            project.appendChild (mod, nullptr);
        }

    const auto& sc = engine.getScenes();
    for (int s = 0; s <= numScenes; ++s)
    {
        if (sc.scenes[(size_t) s].empty() && ! sc.muted[(size_t) s]) continue;
        juce::ValueTree scene ("SCENE");
        scene.setProperty ("n", s, nullptr);
        scene.setProperty ("muted", sc.muted[(size_t) s], nullptr);
        for (const auto& l : sc.scenes[(size_t) s])
        {
            const auto a = addressOf (l.slot);
            if (a.channel < 0) continue;
            juce::ValueTree lock ("L");
            lock.setProperty ("c", a.channel, nullptr); lock.setProperty ("a", a.area, nullptr); lock.setProperty ("i", a.id, nullptr);
            lock.setProperty ("v", (double) l.norm, nullptr);
            scene.appendChild (lock, nullptr);
        }
        project.appendChild (scene, nullptr);
    }

    const auto& seq = engine.sequencer();
    juce::ValueTree st ("SEQ");
    // division par pattern (PAT) depuis la version 6 ; la valeur globale reste pour les anciennes versions
    st.setProperty ("division", (int) Division::sixteenth, nullptr);
    st.setProperty ("recQuant", seq.getRecordQuantize(), nullptr);
    st.setProperty ("launchQuant", seq.getLaunchQuant(), nullptr);
    st.setProperty ("muteTails", seq.getMuteTails(), nullptr);
    for (int t = 0; t < numTracks; ++t)
    {
        juce::ValueTree tr ("T");
        tr.setProperty ("i", t, nullptr);
        tr.setProperty ("mute", seq.getTrackMute (t), nullptr);
        tr.setProperty ("solo", seq.getTrackSolo (t), nullptr);
        tr.setProperty ("play", seq.getPlaying (t), nullptr);
        tr.setProperty ("edit", seq.getEditing (t), nullptr);
        if (seq.getAccentAmount (t) != 50) tr.setProperty ("accent", seq.getAccentAmount (t), nullptr);
        for (int l = 0; l < Sequencer::numLanes; ++l)
        {
            const bool settings = seq.getDirection (t, l) != Direction::forward || seq.getLaneMute (t, l) || seq.getLaneSolo (t, l)
                                  || seq.getLaneLockGlide (t, l) || seq.getLaneLockMute (t, l);
            if (! settings) continue;
            juce::ValueTree lane ("LANE");
            lane.setProperty ("l", l, nullptr);
            lane.setProperty ("dir", (int) seq.getDirection (t, l), nullptr);
            lane.setProperty ("mute", seq.getLaneMute (t, l), nullptr);
            lane.setProperty ("solo", seq.getLaneSolo (t, l), nullptr);
            if (seq.getLaneLockGlide (t, l)) lane.setProperty ("lglide", true, nullptr); // 1.0 : verrous glissés
            if (seq.getLaneLockMute (t, l)) lane.setProperty ("lmute", true, nullptr);   // 1.0 : verrous coupés
            tr.appendChild (lane, nullptr);
        }
        for (int p = 0; p < Sequencer::numPatterns; ++p)
        {
            juce::ValueTree pt ("PAT");
            pt.setProperty ("p", p, nullptr);
            pt.setProperty ("length", seq.getPatternLength (t, p), nullptr);
            pt.setProperty ("speed", seq.getPatternSpeed (t, p), nullptr);
            if (seq.getPatternDivision (t, p) != Division::sixteenth) pt.setProperty ("division", (int) seq.getPatternDivision (t, p), nullptr);
            {
                std::vector<int> flags;
                bool anyFlag = false;
                for (int s = 0; s < Sequencer::maxSteps; ++s) { flags.push_back (seq.getStepFlagsP (t, p, s)); anyFlag = anyFlag || flags.back() != 0; }
                if (anyFlag) pt.setProperty ("flags", toHex (flags), nullptr);
                // v0.46 fix27 : valeurs des pas (force de l'accent + 256 × durée du glissé)
                std::vector<int> amts;
                bool anyAmt = false;
                for (int st = 0; st < Sequencer::maxSteps; ++st) { amts.push_back (seq.getStepAmountsP (t, p, st)); anyAmt = anyAmt || amts.back() != 0; }
                if (anyAmt) pt.setProperty ("amt", toHex16 (amts), nullptr);
                juce::String lens;
                bool anyLen = false;
                for (int l = 0; l < Sequencer::numLanes; ++l)
                {
                    lens << (l ? "," : "") << seq.getLaneLengthP (t, p, l);
                    anyLen = anyLen || seq.getLaneLengthP (t, p, l) > 0;
                }
                if (anyLen) pt.setProperty ("laneLen", lens, nullptr);
            }
            for (int l = 0; l < Sequencer::numLanes; ++l)
            {
                // une voie sans note n'est pas écrite : on la repère d'abord (copie d'historique rapide)
                bool any = false;
                for (int s = 0; s < Sequencer::maxSteps && ! any; ++s) any = seq.getStepP (t, p, l, s) > 0;
                if (! any) continue;
                std::vector<int> vel, note, len, micro;
                vel.reserve (Sequencer::maxSteps); note.reserve (Sequencer::maxSteps); len.reserve (Sequencer::maxSteps); micro.reserve (Sequencer::maxSteps);
                for (int s = 0; s < Sequencer::maxSteps; ++s)
                {
                    vel.push_back (seq.getStepP (t, p, l, s)); note.push_back (seq.getNoteP (t, p, l, s));
                    len.push_back (seq.getLength8P (t, p, l, s)); micro.push_back (seq.getMicroP (t, p, l, s) & 0xFF);
                }
                juce::ValueTree lane ("LANE");
                lane.setProperty ("l", l, nullptr);
                lane.setProperty ("vel", toHex (vel), nullptr);
                lane.setProperty ("note", toHex (note), nullptr);
                // « len » reste lisible par les versions précédentes (borné à 255) ; « len16 » garde les longues notes
                std::vector<int> len8 (len.size());
                bool longNotes = false;
                for (size_t i = 0; i < len.size(); ++i) { len8[i] = std::min (255, len[i]); longNotes = longNotes || len[i] > 255; }
                lane.setProperty ("len", toHex (len8), nullptr);
                if (longNotes) lane.setProperty ("len16", toHex16 (len), nullptr);
                lane.setProperty ("micro", toHex (micro), nullptr);
                pt.appendChild (lane, nullptr);
            }
            const bool defaults = pt.getNumChildren() == 0 && seq.getPatternLength (t, p) == 16 && seq.getPatternSpeed (t, p) == 4
                               && ! pt.hasProperty ("flags") && ! pt.hasProperty ("laneLen") && ! pt.hasProperty ("division");
            if (! defaults) tr.appendChild (pt, nullptr);
        }
        st.appendChild (tr, nullptr);
    }
    project.appendChild (st, nullptr);

    juce::ValueTree rows ("ROWS");
    for (int r = 0; r < Sequencer::numPatterns; ++r)
        rows.setProperty ("scene" + juce::String (r), rowScene[(size_t) r].load(), nullptr);
    project.appendChild (rows, nullptr);
    if (auto sgTree = songToTree(); sgTree.isValid()) project.appendChild (sgTree, nullptr);

    for (const auto& [key, x] : engine.getExtras().steps)
    {
        juce::ValueTree e ("EXTRA");
        e.setProperty ("t", PatternExtras::keyTrack (key), nullptr); e.setProperty ("p", PatternExtras::keyPattern (key), nullptr);
        e.setProperty ("l", PatternExtras::keyLane (key), nullptr); e.setProperty ("s", PatternExtras::keyStep (key), nullptr);
        e.setProperty ("cond", (int) x.cond, nullptr); e.setProperty ("a", (int) x.a, nullptr); e.setProperty ("b", (int) x.b, nullptr);
        e.setProperty ("prob", (int) x.prob, nullptr);
        if (x.ratchet > 1) { e.setProperty ("rat", (int) x.ratchet, nullptr); e.setProperty ("rs", (int) x.ratchetSlope, nullptr); }
        if (x.flam > 0) e.setProperty ("flam", (int) x.flam, nullptr);
        if (x.direct) e.setProperty ("arp", 1, nullptr); // note sortie de l'arpégiateur
        for (const auto& [g, v] : x.locks)
        {
            const auto a = addressOf (g);
            if (a.channel < 0) continue;
            juce::ValueTree lock ("L");
            lock.setProperty ("c", a.channel, nullptr); lock.setProperty ("a", a.area, nullptr); lock.setProperty ("i", a.id, nullptr);
            lock.setProperty ("v", (double) v, nullptr);
            e.appendChild (lock, nullptr);
        }
        project.appendChild (e, nullptr);
    }

    for (int t = 0; t < numTracks && withSamples; ++t)
        for (int i = 0; i < maxSamples; ++i)
            if (const auto* s = engine.getSample (t, i); s != nullptr && s->original != nullptr)
            {
                juce::ValueTree smp ("SAMPLE");
                smp.setProperty ("t", t, nullptr); smp.setProperty ("i", i, nullptr);
                smp.setProperty ("name", juce::String::fromUTF8 (s->name.c_str()), nullptr);
                juce::MemoryBlock mb (s->original->data(), s->original->size());
                smp.setProperty ("data", mb.toBase64Encoding(), nullptr);
                project.appendChild (smp, nullptr);
            }

    {
        // v0.46 : canaux MIDI (notes, commandes, commandes actives, canal propre à chaque piste)
        juce::ValueTree mr ("MIDIROUTE");
        mr.setProperty ("note", midiNoteChan.load(), nullptr); mr.setProperty ("ctrl", midiCtrlChan.load(), nullptr);
        mr.setProperty ("ctrlOn", midiCtrlOn.load(), nullptr);
        juce::String tc;
        for (int t = 0; t < numTracks; ++t) tc << (t ? "," : "") << trackMidiChan[(size_t) t].load();
        mr.setProperty ("tracks", tc, nullptr);
        // GOLEM 1.0 : canaux des pistes internes T, piste par piste (« 1.0.4.0… » ; vide si aucun)
        juce::String lc; bool anyLane = false;
        for (int t = 0; t < numTracks; ++t)
        {
            lc << (t ? "," : "");
            for (int l = 0; l < maxLaneChans; ++l) { const auto c = laneMidiChan[(size_t) t][(size_t) l].load(); anyLane |= c != 0; lc << (l ? "." : "") << c; }
        }
        if (anyLane) mr.setProperty ("lanes", lc, nullptr);
        project.appendChild (mr, nullptr);
    }
    for (const auto& mm : getMidiMappings())
    {
        juce::ValueTree mt ("MIDI");
        mt.setProperty ("kind", mm.kind, nullptr); mt.setProperty ("ch", mm.channel, nullptr); mt.setProperty ("num", mm.number, nullptr);
        if (mm.target >= 0)
        {
            const auto a = addressOf (mm.target);
            if (a.channel < 0) continue;
            mt.setProperty ("c", a.channel, nullptr); mt.setProperty ("a", a.area, nullptr); mt.setProperty ("i", a.id, nullptr);
        }
        else mt.setProperty ("special", mm.target, nullptr);
        project.appendChild (mt, nullptr);
    }
    // v0.46 fix24 : boutons assignables (cible : adresse d'un réglage, ou commande / réglage global / valeur d'écran)
    for (int k = 0; k < numKnobs; ++k)
    {
        const auto t = knobTarget[(size_t) k].load();
        juce::ValueTree kn ("KNOB");
        kn.setProperty ("k", k, nullptr); kn.setProperty ("v", knobVal[(size_t) k].load(), nullptr);
        if (const auto l = knobLfo[(size_t) k].load(); l > 0) { kn.setProperty ("lfo", l, nullptr); kn.setProperty ("depth", knobDepth[(size_t) k].load(), nullptr); }
        if (t >= 0)
        {
            const auto a = addressOf (t);
            if (a.channel >= 0) { kn.setProperty ("c", a.channel, nullptr); kn.setProperty ("a", a.area, nullptr); kn.setProperty ("i", a.id, nullptr); }
        }
        else if (t != midiNone) kn.setProperty ("special", t, nullptr);
        project.appendChild (kn, nullptr);
    }
    if (ctlTable.isNotEmpty())
    {
        juce::ValueTree ct ("CTLTABLE");
        ct.setProperty ("text", ctlTable, nullptr);
        project.appendChild (ct, nullptr);
    }

    juce::ValueTree photoNow;
    {
        const juce::SpinLock::ScopedLockType lock (photoLock); // la sauvegarde peut venir du fil du logiciel hôte
        photoNow = photo;
    }
    if (withSamples && photoNow.isValid())
    {
        // les échantillons de la photo sont des liens en mémoire : ils ne s'écrivent pas dans le fichier
        auto copy = photoNow.createCopy();
        copy.removeProperty ("samples", nullptr);
        for (int k = copy.getNumChildren(); --k >= 0;)
            if (copy.getChild (k).hasType ("SAMPLE")) copy.removeChild (k, nullptr);
        juce::ValueTree ph ("PHOTO");
        ph.appendChild (copy, nullptr);
        project.appendChild (ph, nullptr);
    }

    {
        // v0.46 : modes des macros (bit m : relatif), avant les affectations
        int rel = 0;
        for (int m = 0; m < numMacros; ++m) if (macroRel[(size_t) m].load()) rel |= 1 << m;
        juce::ValueTree mm ("MACROMODE");
        mm.setProperty ("rel", rel, nullptr);
        project.appendChild (mm, nullptr);
    }
    for (int m = 0; m < numMacros; ++m)
        for (const auto& l : getMacroLinks (m))
        {
            const auto a = addressOf (l.global);
            if (a.channel < 0) continue;
            juce::ValueTree mac ("MACRO");
            mac.setProperty ("m", m, nullptr); mac.setProperty ("c", a.channel, nullptr);
            mac.setProperty ("a", a.area, nullptr); mac.setProperty ("i", a.id, nullptr);
            mac.setProperty ("lo", l.lo, nullptr); mac.setProperty ("hi", l.hi, nullptr);
            if (l.base >= 0.0f) { mac.setProperty ("base", l.base, nullptr); mac.setProperty ("ref", l.ref, nullptr); }
            project.appendChild (mac, nullptr);
        }
    // v0.46 fix8 : montage audio (les fichiers des prises sont à côté du projet)
    if (! montageDoc.clips.empty() || ! montageDoc.takes.empty()) project.appendChild (montage::toTree (montageDoc, projectContext), nullptr);
    return project;
}

/** v0.31 : coupure, solo et noms des retours et des groupes (éléments BUS ; absents = remis à zéro). */
static void busesFromTree (Engine& engine, const juce::ValueTree& tree)
{
    for (int ch = numTracks; ch < numChannels; ++ch) { engine.setBusMute (ch, false); engine.setBusSolo (ch, false); }
    for (int k = 0; k < numGroups; ++k) engine.setGroupName (k, {});
    for (const auto& c : tree)
        if (c.hasType ("BUS"))
        {
            const auto ch = (int) c.getProperty ("c", -1);
            if (! Engine::isBus (ch)) continue;
            engine.setBusMute (ch, (int) c.getProperty ("mute", 0) != 0);
            engine.setBusSolo (ch, (int) c.getProperty ("solo", 0) != 0);
            if (ch >= groupBase) engine.setGroupName (ch - groupBase, c.getProperty ("name").toString().substring (0, 40).toStdString());
        }
}

void GrooveboxProcessor::projectFromTree (const juce::ValueTree& projectIn)
{
    if (! projectIn.isValid()) return;
    // v0.42 : anciennes pistes Pieuvre regroupées en une Pieuvre à 8 pistes internes
    auto project = projectIn.createCopy();
    mergeOctaTracks (project);
    clearProject();
    // v0.46 fix8 : montage audio
    if (const auto m = project.getChildWithName ("MONTAGE"); m.isValid())
    {
        montageDoc = montage::fromTree (m, projectContext);
        montagePlayer.setDoc (montageDoc, getSampleRate() > 0.0 ? getSampleRate() : 48000.0);
    }
    const auto t = nowMs();
    engine.beginBatch();
    auto clampInt = [] (const juce::var& v, int lo, int hi, int def) { return v.isVoid() ? def : juce::jlimit (lo, hi, (int) v); };

    for (const auto& c : project)
        if (c.hasType ("TRACK"))
        {
            const auto i = clampInt (c.getProperty ("i"), 0, numTracks - 1, 0);
            engine.setInstrument (i, c.getProperty ("instrument").toString().substring (0, 32).toStdString(), t);
            engine.setTrackName (i, c.getProperty ("name").toString().substring (0, 40).toStdString());
            engine.setTrackColor (i, clampInt (c.getProperty ("color"), 0, 15, 0));
            engine.setTrackOutput (i, clampInt (c.getProperty ("out"), 0, numGroups, 0));
            engine.setTrackSpeech (i, c.getProperty ("speech").toString().toStdString(), t); // v0.33
            engine.setTrackLabels (i, c.getProperty ("labels").toString().substring (0, 200).toStdString());
            // v0.49 : FM Player — mémoire flash du firmware, reprise au chargement du firmware (SAMPLE qui suit)
            pendingFlash[(size_t) i].reset();
            if (c.hasProperty ("fm1flash"))
            {
                juce::MemoryBlock mb;
                if (mb.fromBase64Encoding (c.getProperty ("fm1flash").toString()) && mb.getSize() <= (size_t) 16 * 1024 * 1024)
                    pendingFlash[(size_t) i] = std::make_shared<std::vector<std::uint8_t>> ((const std::uint8_t*) mb.getData(), (const std::uint8_t*) mb.getData() + mb.getSize());
            }
        }
        else if (c.hasType ("INSERT") && validChannel (c))
            engine.setInsert (clampInt (c.getProperty ("c"), 0, numChannels - 1, 0), clampInt (c.getProperty ("s"), 0, numInserts - 1, 0),
                              c.getProperty ("type").toString().substring (0, 32).toStdString(), t);
    // inserts de pad : une fois les instruments en place
    for (const auto& c : project)
        if (c.hasType ("PADFX"))
            engine.setPadInsert (clampInt (c.getProperty ("t"), 0, numTracks - 1, 0), clampInt (c.getProperty ("p"), 0, numPads - 1, 0),
                                 clampInt (c.getProperty ("s"), 0, padInserts - 1, 0), c.getProperty ("type").toString().substring (0, 32).toStdString(), t);

    busesFromTree (engine, project);
    applyProjectContent (project, true, t);
    // la photo des réglages revient avec le projet ; l'historique repart de zéro
    juce::ValueTree loadedPhoto;
    if (const auto ph = project.getChildWithName ("PHOTO").getChild (0); ph.hasType ("PROJECT"))
    {
        loadedPhoto = ph.createCopy();
        loadedPhoto.removeProperty ("samples", nullptr);
        for (int k = loadedPhoto.getNumChildren(); --k >= 0;)
            if (loadedPhoto.getChild (k).hasType ("SAMPLE") || loadedPhoto.getChild (k).hasType ("PHOTO")) loadedPhoto.removeChild (k, nullptr);
    }
    {
        const juce::SpinLock::ScopedLockType lock (photoLock);
        photo = loadedPhoto;
    }
    undoStack.clear(); redoStack.clear(); committed = {};
    engine.endBatch (nowMs()); // le chargement des échantillons a pu être long
    projectVersion.fetch_add (1);
}

float GrooveboxProcessor::migrateValue (int version, int g, float v) const
{
    // Réglages rangés par une version plus ancienne du format de projet (projet, photo ou scène).
    if (version >= 20 || g < 0) return v;
    const auto* d = engine.describe (g);
    if (d == nullptr) return v;
    // version 20 : modes « Un cycle », « Demi-cycle », « Maintien » ajoutés à la fin du redéclenchement
    // des sources de modulation de piste (3 choix avant : libre, note, mesure)
    if (const auto local = localOf (g); local >= modSourceBase && local < modSourceBase + numModSources * modSourceSlots
        && (local - modSourceBase) % modSourceSlots == msRetrig)
        return d->toNorm ((float) juce::jlimit (0, 2, (int) std::floor (v * 3.0f)));
    if (version >= 19) return v;
    // version 19 : modes des LFO de la modulation commune — « Une fois », « Tenu », « Demi-cycle » ajoutés
    // à la fin (3 choix avant : par voix, global, par voix libre), dans tous les instruments
    if (localOf (g) >= instrumentBase && localOf (g) < insertBase && d->choices.size() == 6 && d->id.size() >= 8
        && d->id.compare (d->id.size() - 5, 5, "_mode") == 0)
    {
        const auto k = d->id.size() - 8; // « lfN_mode » à la fin (avec ou sans « tN_ » devant)
        if (d->id.compare (k, 2, "lf") == 0 && std::isdigit ((unsigned char) d->id[k + 2]) && (k == 0 || d->id[k - 1] == '_'))
            return d->toNorm ((float) juce::jlimit (0, 2, (int) std::floor (v * 3.0f)));
    }
    if (version >= 17) return v;
    // v0.42 : réglage d'une piste interne de Pieuvre 8 (« t3_slot ») — identifiant sans le préfixe
    std::string octaId;
    if (localOf (g) >= instrumentBase && localOf (g) < insertBase)
        if (const auto* inst = engine.instrumentDesc (channelOf (g)); inst != nullptr && inst->type == "octa"
            && d->id.size() > 3 && d->id[0] == 't' && std::isdigit ((unsigned char) d->id[1]) && d->id[2] == '_')
            octaId = d->id.substr (3);
    // version 17 : Pieuvre 8 — emplacements de la réserve commune ajoutés à la fin (17 choix avant)
    if (octaId == "slot")
        return d->toNorm ((float) juce::jlimit (0, 16, (int) std::floor (v * 17.0f)));
    if (version >= 16) return v;
    // effet d'un insert de canal ou de pad qui porte ce réglage
    auto effectAt = [this] (int global) -> const ModuleDesc*
    {
        const auto local = localOf (global), ch = channelOf (global);
        if (local >= insertBase && local < insertBase + numInserts * insertSlots)
            return engine.effectDesc (ch, (local - insertBase) / insertSlots);
        if (local >= padInsertBase && local < padInsertBase + numPads * padInserts * insertSlots)
        {
            const auto k = (local - padInsertBase) / insertSlots;
            return engine.padEffectDesc (ch, k / padInserts, k % padInserts);
        }
        return nullptr;
    };
    // version 16 : Hopscotch à 64 pas — chaque pas a 51 choix (8 avant : les mêmes en tête de liste),
    // longueur de 1 à 64 pas (1 à 16 avant)
    if (const auto* fx = effectAt (g); fx != nullptr && fx->type == "stepfx")
    {
        if (d->id == "steps") return d->toNorm (1.0f + std::round (juce::jlimit (0.0f, 1.0f, v) * 15.0f));
        if (d->id.size() >= 2 && d->id[0] == 's' && std::isdigit ((unsigned char) d->id[1]) && std::atoi (d->id.c_str() + 1) <= 16)
            return d->toNorm ((float) juce::jlimit (0, 7, (int) std::floor (v * 8.0f)));
    }
    if (version >= 15) return v;
    // version 15 : 7 algorithmes ajoutés à la fin de l'Atmosphère (5 avant), dans un insert de canal ou de pad
    if (d->id == "mode" && d->choices.size() == 12)
    {
        const auto* fx = effectAt (g);
        if (fx != nullptr && fx->type == "atmos")
            return d->toNorm ((float) juce::jlimit (0, 4, (int) std::floor (v * 5.0f)));
    }
    if (version >= 14) return v;
    // version 14 : « Prise de son » ajoutée à la fin des sources des vocodeurs (19 choix avant)
    if (d->id == "src" && localOf (g) >= instrumentBase && localOf (g) < insertBase)
        if (const auto* inst = engine.instrumentDesc (channelOf (g)); inst != nullptr && inst->family == "Vocodeurs")
            return d->toNorm ((float) juce::jlimit (0, 18, (int) std::floor (v * 19.0f)));
    if (version >= 12) return v;
    // version 12 : générateurs de l'orbiteur — 7 choix (A à F, bruit), puis W (onde choisie dans la bibliothèque)
    if (d->id.size() == 3 && d->id[0] >= 'a' && d->id[0] <= 'd' && d->id[1] == 'g' && (d->id[2] == '1' || d->id[2] == '2')
        && localOf (g) >= instrumentBase && localOf (g) < insertBase)
        if (const auto* inst = engine.instrumentDesc (channelOf (g)); inst != nullptr && inst->type == "orbit")
            return d->toNorm ((float) juce::jlimit (0, 6, (int) std::floor (v * 7.0f)));
    if (version >= 11) return v;
    // version 10 : machines Thru et Neighbor retirées du sampler de performance (Flex, Static, Pickup) ;
    // version 11 : Static retirée (elle jouait comme Flex) : Flex, Pickup
    if (octaId == "machine")
        {
            const bool pickup = version < 10 ? juce::jlimit (0, 4, (int) std::floor (v * 5.0f)) == 4
                                             : juce::jlimit (0, 2, (int) std::floor (v * 3.0f)) == 2;
            return d->toNorm (pickup ? 1.0f : 0.0f);
        }
    // version 11 : clavier « Emplacements » ajouté au sampler de performance (3 choix au lieu de 2)
    if (octaId == "keymode")
            return d->toNorm ((float) juce::jlimit (0, 1, (int) std::floor (v * 2.0f)));
    if (version >= 10) return v;
    if (version >= 9) return v;
    // version 9 : filtre et lo-fi du sampler de performance — huit destinations de modulation en plus
    if (octaId.size() > 4 && octaId.compare (0, 2, "mx") == 0 && octaId.compare (octaId.size() - 4, 4, "_dst") == 0)
        {
            const auto oldCount = (int) d->choices.size() - 8;
            const auto oldIndex = juce::jlimit (0, oldCount - 1, (int) std::floor (v * (float) oldCount));
            return d->toNorm ((float) oldIndex);
        }
    if (version >= 8) return v;
    // version 8 : bibliothèque de 64 ondes du wave morph (16 avant) pour l'oscillateur 3
    if (d->id == "w3pos" && localOf (g) >= instrumentBase && localOf (g) < insertBase)
        if (const auto* inst = engine.instrumentDesc (channelOf (g)); inst != nullptr && inst->type == "morph")
        {
            const auto oldIndex = juce::jlimit (0, 15, (int) std::floor (v * 16.0f));
            return d->toNorm ((float) oldIndex);
        }
    if (version >= 7) return v;
    // version 7 : divisions au tempo jusqu'à 16 mesures (deux choix insérés avant « 4 mes. »)
    if (hasLongDivisions (*d))
    {
        const auto oldCount = (int) d->choices.size() - 2;
        const auto oldIndex = juce::jlimit (0, oldCount - 1, (int) std::floor (v * (float) oldCount));
        return d->toNorm ((float) widenDivisionIndex (*d, oldIndex));
    }
    if (version >= 5) return v;
    const auto local = localOf (g);
    // version 5 : 6 dB de réserve dans le poly à chorus et le wave morph (même son qu'avant)
    if (local >= instrumentBase && local < insertBase && d->id == "level")
        if (const auto* inst = engine.instrumentDesc (channelOf (g)); inst != nullptr && (inst->type == "polyj" || inst->type == "morph"))
            return d->toNorm (std::max (d->min, d->toPlain (v) - 6.0f));
    if (version >= 4) return v;
    // Une valeur à crans est rangée normalisée : si la liste de choix a grandi, on la
    // recalcule pour garder le même choix.
    auto remap = [&] (int oldCount) { const auto oldIndex = juce::jlimit (0, oldCount - 1, (int) std::floor (v * (float) oldCount)); return d->toNorm ((float) oldIndex); };
    // version 4 : 20 modes d'arpège (8 avant)
    if (local >= instrumentBase && local < insertBase && d->id == "arp_mode") return remap (8);
    if (version >= 3 || local < insertBase || local >= modSourceBase) return v;
    const auto* fx = engine.effectDesc (channelOf (g), (local - insertBase) / insertSlots);
    if (fx == nullptr) return v;
    if (version < 2 && fx->type == "eq" && d->id.size() >= 6 && d->id.compare (d->id.size() - 5, 5, "_type") == 0) return remap (8);
    if (fx->type == "dist" && (d->id == "typeA" || d->id == "typeB")) return remap (effects::dist::algorithmsV1);
    return v;
}

void GrooveboxProcessor::applyProjectContent (const juce::ValueTree& project, bool withSamplesAndMidi, double t)
{
    auto addr = [] (const juce::ValueTree& v) { return Address { (int) v.getProperty ("c", -1), v.getProperty ("a").toString(), v.getProperty ("i").toString() }; };
    auto clampInt = [] (const juce::var& v, int lo, int hi, int def) { return v.isVoid() ? def : juce::jlimit (lo, hi, (int) v); };
    auto unit = [] (const juce::var& v) { const auto x = (float) (double) v; return std::isfinite (x) ? juce::jlimit (0.0f, 1.0f, x) : 0.0f; };

    // Projets de version 1 : la liste des types de bande de l'égaliseur avait 8 choix (12 aujourd'hui).
    // Projets de version 1 et 2 : la distorsion avait 11 algorithmes (14 aujourd'hui).
    const auto version = (int) project.getProperty ("version", 1);
    auto fix = [this, version] (int g, float v) { return migrateValue (version, g, v); };
    std::vector<std::pair<int, int>> wholeSamplePads;

    // Les commandes MIDI apprises restent d'un projet à l'autre, sauf si le projet chargé a les siennes
    if (withSamplesAndMidi && project.getChildWithName ("MIDI").isValid())
    {
        for (auto& m : ccMap) m.store (midiNone);
        for (auto& m : noteMap) m.store (midiNone);
        midiVersion.fetch_add (1);
    }

    // v0.46 fix24 : boutons assignables et table des valeurs d'écran : comme les commandes MIDI, ils restent
    // d'un projet à l'autre, sauf si le projet chargé a les siens
    if (withSamplesAndMidi && (project.getChildWithName ("KNOB").isValid() || project.getChildWithName ("CTLTABLE").isValid()))
    {
        for (int k = 0; k < numKnobs; ++k) { knobTarget[(size_t) k].store (midiNone); knobVal[(size_t) k].store (0.0f); knobLfo[(size_t) k].store (0); knobDepth[(size_t) k].store (0.0f); }
        ctlTable = project.getChildWithName ("CTLTABLE").getProperty ("text").toString().substring (0, 20000);
        midiVersion.fetch_add (1);
    }

    bool sawMacroMode = false;
    auto macroLinkOf = [&unit] (const juce::ValueTree& c, int g)
    {
        return MacroLink { g, c.hasProperty ("lo") ? unit (c.getProperty ("lo")) : 0.0f, c.hasProperty ("hi") ? unit (c.getProperty ("hi")) : 1.0f,
                           c.hasProperty ("base") ? unit (c.getProperty ("base")) : -1.0f, c.hasProperty ("ref") ? unit (c.getProperty ("ref")) : -1.0f };
    };
    for (const auto& c : project)
    {
        if (c.hasType ("PARAMS"))
        {
            for (const auto& p : c)
                if (const auto g = globalOf (addr (p)); g >= 0) engine.setParam (g, fix (g, unit (p.getProperty ("v"))));
        }
        else if (c.hasType ("MOD"))
        {
            const auto a = addr (c);
            if (const auto g = globalOf (a); g >= 0)
                engine.addModulation (a.channel, clampInt (c.getProperty ("src"), 0, numModSources, 0), localOf (g),
                                      juce::jlimit (-1.0f, 1.0f, (float) (double) c.getProperty ("amount", 0.0)), t);
        }
        else if (c.hasType ("SCENE"))
        {
            const auto s = clampInt (c.getProperty ("n"), 0, numScenes, 0);
            engine.setSceneMuted (s, (bool) c.getProperty ("muted", false), t);
            for (const auto& l : c)
                if (const auto g = globalOf (addr (l)); g >= 0) engine.setSceneLock (s, g, fix (g, unit (l.getProperty ("v"))), t);
        }
        else if (c.hasType ("SEQ"))
        {
            auto& seq = engine.sequencer();
            seq.setDivision ((Division) clampInt (c.getProperty ("division"), 0, numDivisions - 1, 2));
            seq.setRecordQuantize (clampInt (c.getProperty ("recQuant"), 0, 100, 100));
            seq.setLaunchQuant (clampInt (c.getProperty ("launchQuant"), 0, numLaunchQuants - 1, 2));
            seq.setMuteTails ((bool) c.getProperty ("muteTails", false));
            auto readLane = [&] (const juce::ValueTree& lane, int ti, int p)
            {
                const auto l = clampInt (lane.getProperty ("l"), 0, Sequencer::numLanes - 1, 0);
                const auto vel = fromHex (lane.getProperty ("vel").toString(), Sequencer::maxSteps);
                const auto note = fromHex (lane.getProperty ("note").toString(), Sequencer::maxSteps);
                const auto len = readLengths (lane, Sequencer::maxSteps);
                const auto micro = fromHex (lane.getProperty ("micro").toString(), Sequencer::maxSteps);
                for (int s = 0; s < Sequencer::maxSteps; ++s)
                {
                    if (vel[(size_t) s] >= 0) seq.setStepP (ti, p, l, s, vel[(size_t) s]);
                    if (note[(size_t) s] >= 0) seq.setNoteP (ti, p, l, s, note[(size_t) s]);
                    if (len[(size_t) s] > 0) seq.setLength8P (ti, p, l, s, len[(size_t) s]);
                    if (micro[(size_t) s] >= 0) seq.setMicroP (ti, p, l, s, (int) (std::int8_t) (std::uint8_t) micro[(size_t) s]);
                }
            };
            for (const auto& tr : c)
            {
                const auto ti = clampInt (tr.getProperty ("i"), 0, numTracks - 1, 0);
                // projets d'avant les patterns : longueur, vitesse et notes vont au pattern 1
                if (tr.hasProperty ("length")) seq.setPatternLength (ti, 0, clampInt (tr.getProperty ("length"), 1, Sequencer::maxSteps, 16));
                if (tr.hasProperty ("speed")) seq.setPatternSpeed (ti, 0, clampInt (tr.getProperty ("speed"), 0, numSpeeds - 1, 4));
                seq.setTrackMute (ti, (bool) tr.getProperty ("mute", false));
                seq.setTrackSolo (ti, (bool) tr.getProperty ("solo", false));
                seq.setAccentAmount (ti, clampInt (tr.getProperty ("accent"), 0, 100, 50));
                for (const auto& child : tr)
                {
                    if (child.hasType ("LANE"))
                    {
                        const auto l = clampInt (child.getProperty ("l"), 0, Sequencer::numLanes - 1, 0);
                        seq.setDirection (ti, l, (Direction) clampInt (child.getProperty ("dir"), 0, 3, 0));
                        seq.setLaneMute (ti, l, (bool) child.getProperty ("mute", false));
                        seq.setLaneSolo (ti, l, (bool) child.getProperty ("solo", false));
                        seq.setLaneLockGlide (ti, l, (bool) child.getProperty ("lglide", false));
                        seq.setLaneLockMute (ti, l, (bool) child.getProperty ("lmute", false));
                        if (child.hasProperty ("vel")) readLane (child, ti, 0);
                    }
                    else if (child.hasType ("PAT"))
                    {
                        const auto p = clampInt (child.getProperty ("p"), 0, Sequencer::numPatterns - 1, 0);
                        seq.setPatternLength (ti, p, clampInt (child.getProperty ("length"), 1, Sequencer::maxSteps, 16));
                        seq.setPatternSpeed (ti, p, clampInt (child.getProperty ("speed"), 0, numSpeeds - 1, 4));
                        if (child.hasProperty ("division"))
                            seq.setPatternDivision (ti, p, (Division) clampInt (child.getProperty ("division"), 0, numDivisions - 1, 2));
                        if (child.hasProperty ("flags"))
                        {
                            const auto flags = fromHex (child.getProperty ("flags").toString(), Sequencer::maxSteps);
                            for (int s = 0; s < Sequencer::maxSteps; ++s)
                                if (flags[(size_t) s] > 0) seq.setStepFlagsP (ti, p, s, flags[(size_t) s]);
                        }
                        if (child.hasProperty ("amt")) // v0.46 fix27
                        {
                            const auto amts = fromHex16 (child.getProperty ("amt").toString(), Sequencer::maxSteps);
                            for (int s = 0; s < Sequencer::maxSteps; ++s)
                                if (amts[(size_t) s] > 0) seq.setStepAmountsP (ti, p, s, amts[(size_t) s]);
                        }
                        if (child.hasProperty ("laneLen"))
                        {
                            juce::StringArray parts;
                            parts.addTokens (child.getProperty ("laneLen").toString().substring (0, 200), ",", "");
                            for (int l = 0; l < std::min (parts.size(), Sequencer::numLanes); ++l)
                                seq.setLaneLengthP (ti, p, l, juce::jlimit (0, Sequencer::maxSteps, parts[l].getIntValue()));
                        }
                        for (const auto& lane : child)
                            if (lane.hasType ("LANE")) readLane (lane, ti, p);
                    }
                }
                if (tr.hasProperty ("play")) seq.setPlaying (ti, clampInt (tr.getProperty ("play"), -1, Sequencer::numPatterns - 1, 0));
                if (tr.hasProperty ("edit")) seq.setEditing (ti, clampInt (tr.getProperty ("edit"), 0, Sequencer::numPatterns - 1, 0));
            }
        }
        else if (c.hasType ("ROWS"))
        {
            for (int r = 0; r < Sequencer::numPatterns; ++r)
                rowScene[(size_t) r].store (clampInt (c.getProperty ("scene" + juce::String (r)), -1, numScenes - 1, -1));
        }
        else if (c.hasType ("SONG")) songFromTree (c, t);
        else if (c.hasType ("EXTRA"))
        {
            const auto ti = clampInt (c.getProperty ("t"), 0, numTracks - 1, 0);
            const auto pt = clampInt (c.getProperty ("p"), 0, Sequencer::numPatterns - 1, 0);
            const auto l = clampInt (c.getProperty ("l"), 0, Sequencer::numLanes - 1, 0);
            const auto s = clampInt (c.getProperty ("s"), 0, Sequencer::maxSteps - 1, 0);
            engine.setStepExtraP (ti, pt, l, s, (Condition) clampInt (c.getProperty ("cond"), 0, numConditions - 1, 0),
                                  clampInt (c.getProperty ("a"), 1, 8, 1), clampInt (c.getProperty ("b"), 1, 8, 2), clampInt (c.getProperty ("prob"), 0, 100, 100), t);
            if (c.hasProperty ("rat"))
                engine.setStepRatchetP (ti, pt, l, s, clampInt (c.getProperty ("rat"), 1, Sequencer::maxRatchet, 1), clampInt (c.getProperty ("rs"), -100, 100, 0), t);
            if (c.hasProperty ("flam"))
                engine.setStepsFlam (ti, pt, { { l, s } }, clampInt (c.getProperty ("flam"), 0, maxFlamMs, 0), t);
            if ((int) c.getProperty ("arp", 0) == 1) engine.setStepDirectP (ti, pt, l, s, true, t);
            for (const auto& lock : c)
                if (const auto g = globalOf (addr (lock)); g >= 0) engine.setStepLockP (ti, pt, l, s, g, fix (g, unit (lock.getProperty ("v"))), t);
        }
        else if (c.hasType ("SAMPLE") && withSamplesAndMidi)
        {
            juce::MemoryBlock bytes;
            const auto st = clampInt (c.getProperty ("t"), 0, numTracks - 1, 0), si = clampInt (c.getProperty ("i"), 0, maxSamples - 1, 0);
            if (bytes.fromBase64Encoding (c.getProperty ("data").toString()))
                if (loadSample (st, si, c.getProperty ("name").toString(), std::move (bytes)).isEmpty() && version < 20)
                    wholeSamplePads.push_back ({ st, si }); // projet d'avant la version 20 : lecture entière gardée
        }
        else if (c.hasType ("MACROMODE"))
        {
            sawMacroMode = true;
            const auto rel = (int) c.getProperty ("rel", 0);
            for (int m = 0; m < numMacros; ++m) macroRel[(size_t) m].store (((rel >> m) & 1) != 0);
        }
        else if (c.hasType ("MACRO"))
        {
            // projet d'avant la v0.46 (sans modes) : ses macros étaient absolues
            if (! sawMacroMode) { sawMacroMode = true; for (auto& r : macroRel) r.store (false); }
            // une macro peut piloter plusieurs réglages (une entrée par réglage) ; sans plage : pleine plage
            const auto m = clampInt (c.getProperty ("m"), 0, numMacros - 1, 0);
            auto links = getMacroLinks (m);
            if (const auto g = globalOf (addr (c)); g >= 0 && (int) links.size() < macroLinks)
            {
                links.push_back (macroLinkOf (c, g));
                setMacroLinks (m, links, true);
            }
        }
        else if (c.hasType ("MIDIROUTE") && withSamplesAndMidi)
        {
            midiNoteChan.store (clampInt (c.getProperty ("note"), 0, 16, 0));
            midiCtrlChan.store (clampInt (c.getProperty ("ctrl"), 0, 16, 0));
            midiCtrlOn.store ((bool) c.getProperty ("ctrlOn", true));
            const auto parts = juce::StringArray::fromTokens (c.getProperty ("tracks").toString(), ",", "");
            for (int t = 0; t < numTracks; ++t) trackMidiChan[(size_t) t].store (t < parts.size() ? juce::jlimit (0, 16, parts[t].getIntValue()) : 0);
            const auto lanes = juce::StringArray::fromTokens (c.getProperty ("lanes").toString(), ",", "");
            for (int t = 0; t < numTracks; ++t)
            {
                const auto ls = t < lanes.size() ? juce::StringArray::fromTokens (lanes[t], ".", "") : juce::StringArray();
                for (int l = 0; l < maxLaneChans; ++l) laneMidiChan[(size_t) t][(size_t) l].store (l < ls.size() ? juce::jlimit (0, 16, ls[l].getIntValue()) : 0);
            }
            midiVersion.fetch_add (1);
        }
        else if (c.hasType ("KNOB") && withSamplesAndMidi)
        {
            const auto k = clampInt (c.getProperty ("k"), 0, numKnobs - 1, 0);
            knobVal[(size_t) k].store (unit (c.getProperty ("v")));
            knobLfo[(size_t) k].store (clampInt (c.getProperty ("lfo"), 0, numKnobLfos, 0));
            { const auto d = (float) (double) c.getProperty ("depth", 0.0); knobDepth[(size_t) k].store (std::isfinite (d) ? juce::jlimit (-1.0f, 1.0f, d) : 0.0f); }
            if (c.hasProperty ("special")) setKnobTarget (k, clampInt (c.getProperty ("special"), midiLowestTarget, midiXfade, midiNone));
            else if (c.hasProperty ("c")) setKnobTarget (k, globalOf (addr (c)));
        }
        else if (c.hasType ("MIDI") && withSamplesAndMidi)
        {
            auto target = c.hasProperty ("special") ? clampInt (c.getProperty ("special"), midiLowestTarget, midiXfade, midiNone) : globalOf (addr (c));
            if (c.hasProperty ("special") && ! isMidiSpecial (target) && ! isMidiTrackTarget (target) && ! isMidiExtra (target)) target = midiNone;
            if (target != midiNone)
                setMidiMapping (clampInt (c.getProperty ("kind"), 0, 1, 0), clampInt (c.getProperty ("ch"), 0, 15, 0), clampInt (c.getProperty ("num"), 0, 127, 0), target);
        }
    }
    // version 20 : le déclin d'un pad de synthèse agit aussi sur l'échantillon qu'on y dépose ; un projet
    // plus ancien lisait ces échantillons en entier : leur déclin est mis au maximum (son inchangé)
    for (const auto& [st, si] : wholeSamplePads) openSampleDecay (st, si);
    // les préréglages affichés reviennent après les réglages
    for (const auto& c : project)
        if (c.hasType ("INSERT") && validChannel (c))
            engine.setInsertPresetName (clampInt (c.getProperty ("c"), 0, numChannels - 1, 0), clampInt (c.getProperty ("s"), 0, numInserts - 1, 0),
                                        c.getProperty ("preset").toString().substring (0, 60).toStdString());
    for (const auto& c : project)
        if (c.hasType ("TRACK"))
            engine.setTrackPresetName (clampInt (c.getProperty ("i"), 0, numTracks - 1, 0), c.getProperty ("preset").toString().substring (0, 60).toStdString());
}



//==============================================================================
// Annuler / rétablir, photo des réglages
//
// Chaque étape est une copie du projet sans les fichiers d'échantillons (ceux-ci sont gardés par
// lien partagé). Le rappel n'écrit que ce qui diffère de l'état courant : les instruments et effets
// qui n'ont pas changé ne sont pas recréés (pas de coupure du son), les réglages vont directement
// à leur valeur, et une piste dont la séquence n'a pas changé n'est pas réécrite.

juce::ValueTree GrooveboxProcessor::snapshotForHistory() const
{
    auto snap = projectToTree (false);
    snap.setProperty ("samples", true, nullptr); // les échantillons de cette copie font foi
    for (int k = snap.getNumChildren(); --k >= 0;)
    {
        auto c = snap.getChild (k);
        if (c.hasType ("MIDI") || c.hasType ("MIDIROUTE") || c.hasType ("PHOTO") || c.hasType ("MONTAGE") || c.hasType ("KNOB") || c.hasType ("CTLTABLE")) snap.removeChild (k, nullptr); // montage : annuler à part
        else if (c.hasType ("SEQ"))
            for (auto tr : c) { tr.removeProperty ("play", nullptr); tr.removeProperty ("edit", nullptr); } // le jeu en cours n'est pas un réglage
    }
    for (int t = 0; t < numTracks; ++t)
        for (int i = 0; i < maxSamples; ++i)
        {
            const auto* s = engine.getSample (t, i);
            auto& ref = sampleRefs[(size_t) t][(size_t) i];
            if (s == nullptr || s->original == nullptr) { ref = nullptr; continue; }
            if (ref == nullptr || ref->bytes != s->original || ref->name != s->name) ref = new SampleRef (s->name, s->original);
            juce::ValueTree smp ("SAMPLE");
            smp.setProperty ("t", t, nullptr); smp.setProperty ("i", i, nullptr);
            smp.setProperty ("ref", juce::var (ref.get()), nullptr);
            snap.appendChild (smp, nullptr);
        }
    return snap;
}

void GrooveboxProcessor::restoreSnapshot (const juce::ValueTree& snap)
{
    if (! snap.hasType ("PROJECT")) return;
    auto& seq = engine.sequencer();
    auto addr = [] (const juce::ValueTree& v) { return Address { (int) v.getProperty ("c", -1), v.getProperty ("a").toString(), v.getProperty ("i").toString() }; };
    auto clampInt = [] (const juce::var& v, int lo, int hi, int def) { return v.isVoid() ? def : juce::jlimit (lo, hi, (int) v); };
    auto unit = [] (const juce::var& v) { const auto x = (float) (double) v; return std::isfinite (x) ? juce::jlimit (0.0f, 1.0f, x) : 0.0f; };
    auto collect = [] (const juce::ValueTree& tree, const char* type, auto keep)
    {
        std::vector<juce::ValueTree> out;
        for (const auto& c : tree) if (c.hasType (type) && keep (c)) out.push_back (c);
        return out;
    };
    auto same = [] (const std::vector<juce::ValueTree>& x, const std::vector<juce::ValueTree>& y)
    {
        if (x.size() != y.size()) return false;
        for (size_t k = 0; k < x.size(); ++k) if (! x[k].isEquivalentTo (y[k])) return false;
        return true;
    };
    auto all = [] (const juce::ValueTree&) { return true; };
    const bool snapHasSamples = (bool) snap.getProperty ("samples", false);
    const int snapVersion = (int) snap.getProperty ("version", 1); // photo relue d'un ancien projet : réglages à convertir

    // Ce que veut la copie, une entrée par piste et par emplacement (la dernière l'emporte)
    std::array<juce::ValueTree, numTracks> wantedTracks;
    for (const auto& c : snap)
        if (c.hasType ("TRACK")) wantedTracks[(size_t) clampInt (c.getProperty ("i"), 0, numTracks - 1, 0)] = c;
    std::array<std::array<std::array<std::string, padInserts>, numPads>, numTracks> wantedPadFx {};
    for (const auto& c : snap)
        if (c.hasType ("PADFX"))
            wantedPadFx[(size_t) clampInt (c.getProperty ("t"), 0, numTracks - 1, 0)][(size_t) clampInt (c.getProperty ("p"), 0, numPads - 1, 0)]
                       [(size_t) clampInt (c.getProperty ("s"), 0, padInserts - 1, 0)] = c.getProperty ("type").toString().substring (0, 32).toStdString();
    std::array<std::array<std::string, numInserts>, numChannels> wantedInserts {};
    for (const auto& c : snap)
        if (c.hasType ("INSERT") && validChannel (c))
            wantedInserts[(size_t) clampInt (c.getProperty ("c"), 0, numChannels - 1, 0)][(size_t) clampInt (c.getProperty ("s"), 0, numInserts - 1, 0)]
                = c.getProperty ("type").toString().substring (0, 32).toStdString();

    // Échantillons voulus. Une photo relue d'un fichier n'en porte pas : ceux de la piste restent.
    std::array<std::array<SampleRef::Ptr, maxSamples>, numTracks> wantedSamples;
    if (snapHasSamples)
    {
        for (const auto& c : snap)
            if (c.hasType ("SAMPLE"))
                if (auto* r = dynamic_cast<SampleRef*> (c.getProperty ("ref").getObject()); r != nullptr && r->bytes != nullptr)
                    wantedSamples[(size_t) clampInt (c.getProperty ("t"), 0, numTracks - 1, 0)][(size_t) clampInt (c.getProperty ("i"), 0, maxSamples - 1, 0)] = r;
    }
    else
    {
        for (int tr = 0; tr < numTracks; ++tr)
            for (int i = 0; i < maxSamples; ++i)
                if (const auto* s = engine.getSample (tr, i); s != nullptr && s->original != nullptr)
                    wantedSamples[(size_t) tr][(size_t) i] = new SampleRef (s->name, s->original);
    }

    // échantillons déjà décodés : un échantillon voulu qui y figure est recopié, pas décodé (v0.23)
    for (int tr = 0; tr < numTracks; ++tr)
        for (int i = 0; i < maxSamples; ++i)
            if (const auto* s = engine.getSample (tr, i); s != nullptr && s->original != nullptr) reuseSamples.push_back (s);

    engine.beginBatch();
    const auto t = nowMs();

    // 1. Structure : un instrument ou un effet n'est recréé que si son type a changé
    bool structure = false;
    for (int tr = 0; tr < numTracks; ++tr)
    {
        const auto& c = wantedTracks[(size_t) tr];
        if (! c.isValid()) continue;
        const auto type = c.getProperty ("instrument").toString().substring (0, 32).toStdString();
        if (engine.getTrackInfo (tr).instrumentType != type) { engine.setInstrument (tr, type, t); structure = true; }
        engine.setTrackName (tr, c.getProperty ("name").toString().substring (0, 40).toStdString());
        engine.setTrackColor (tr, clampInt (c.getProperty ("color"), 0, 15, 0));
        engine.setTrackOutput (tr, clampInt (c.getProperty ("out"), 0, numGroups, 0));
        engine.setTrackSpeech (tr, c.getProperty ("speech").toString().toStdString(), t); // v0.33
        engine.setTrackLabels (tr, c.getProperty ("labels").toString().substring (0, 200).toStdString());
    }
    busesFromTree (engine, snap); // v0.31
    for (int ch = 0; ch < numChannels; ++ch)
        for (int s = 0; s < numInserts; ++s)
            if (engine.getInsertTypes (ch)[(size_t) s] != wantedInserts[(size_t) ch][(size_t) s])
            {
                engine.setInsert (ch, s, wantedInserts[(size_t) ch][(size_t) s], t);
                structure = true;
            }
    for (int tr = 0; tr < numTracks; ++tr)
        for (int pad = 0; pad < numPads; ++pad)
            for (int k = 0; k < padInserts; ++k)
                if (engine.getPadInsertType (tr, pad, k) != wantedPadFx[(size_t) tr][(size_t) pad][(size_t) k])
                {
                    engine.setPadInsert (tr, pad, k, wantedPadFx[(size_t) tr][(size_t) pad][(size_t) k], t);
                    structure = true;
                }
    // état réel après la structure (un instrument recréé a perdu ses verrous et modulations)
    const auto now = snapshotForHistory();

    // 2. Réglages : chacun va directement à sa valeur, sans passer par la valeur d'origine
    std::vector<float> target ((size_t) numSlots, -1.0f);
    for (const auto& c : snap)
        if (c.hasType ("PARAMS"))
            for (const auto& p : c)
                if (const auto g = globalOf (addr (p)); g >= 0) target[(size_t) g] = migrateValue (snapVersion, g, unit (p.getProperty ("v")));
    for (int g = 0; g < numSlots; ++g)
    {
        const auto* d = engine.describe (g);
        if (d == nullptr || (localOf (g) >= modAmountBase && localOf (g) < modAmountBase + numModAssigns)) continue; // profondeurs : avec les modulations
        const auto v = target[(size_t) g] >= 0.0f ? target[(size_t) g] : d->toNorm (d->def);
        if (std::abs (engine.getParam (g) - v) >= 1.0e-6f) engine.setParam (g, v);
    }

    // 3. Modulations : réécrites seulement sur les canaux où elles diffèrent
    juce::ValueTree part ("PROJECT");
    part.setProperty ("version", 17, nullptr);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto onChannel = [ch] (const juce::ValueTree& c) { return (int) c.getProperty ("c", -1) == ch; };
        const auto wanted = collect (snap, "MOD", onChannel);
        if (same (wanted, collect (now, "MOD", onChannel))) continue;
        for (const auto& m : engine.getModulations (ch)) engine.removeModulation (ch, m.target, t);
        for (const auto& m : wanted) part.appendChild (m.createCopy(), nullptr);
    }
    for (int tr = 0; tr < numTracks; ++tr)
        if (wantedTracks[(size_t) tr].isValid()) part.appendChild (wantedTracks[(size_t) tr].createCopy(), nullptr); // préréglages affichés
    for (const auto& c : snap)
        if (c.hasType ("INSERT") || c.hasType ("PADFX")) part.appendChild (c.createCopy(), nullptr);

    // 4. Scènes : reconstruites d'un bloc et publiées une seule fois
    if (const auto wanted = collect (snap, "SCENE", all); ! same (wanted, collect (now, "SCENE", all)))
    {
        auto set = std::make_unique<SceneSet>();
        for (const auto& c : wanted)
        {
            const auto sc = clampInt (c.getProperty ("n"), 0, numScenes, 0);
            set->muted[(size_t) sc] = (bool) c.getProperty ("muted", false);
            for (const auto& l : c)
                if (const auto g = globalOf (addr (l)); g >= 0 && set->scenes[(size_t) sc].size() < (size_t) numSlots)
                    set->scenes[(size_t) sc].push_back ({ g, migrateValue (snapVersion, g, unit (l.getProperty ("v"))) });
        }
        engine.replaceScenes (std::move (set), t);
    }

    // 5. Séquences : chaque valeur va directement à sa cible (pas de remise à zéro pendant le jeu)
    const auto seqWanted = snap.getChildWithName ("SEQ"), seqNow = now.getChildWithName ("SEQ");
    const auto globalDivision = clampInt (seqWanted.getProperty ("division"), 0, numDivisions - 1, 2);
    seq.setRecordQuantize (clampInt (seqWanted.getProperty ("recQuant"), 0, 100, 100));
    seq.setLaunchQuant (clampInt (seqWanted.getProperty ("launchQuant"), 0, numLaunchQuants - 1, 2));
    seq.setMuteTails ((bool) seqWanted.getProperty ("muteTails", false));
    auto trackOf = [] (const juce::ValueTree& seqTree, int tr)
    {
        juce::ValueTree found;
        for (const auto& c : seqTree) if ((int) c.getProperty ("i", -1) == tr) found = c;
        return found;
    };
    auto writeTrack = [&] (int tr, const juce::ValueTree& w)
    {
        if (seq.getAccentAmount (tr) != clampInt (w.getProperty ("accent"), 0, 100, 50)) seq.setAccentAmount (tr, clampInt (w.getProperty ("accent"), 0, 100, 50));
        std::array<juce::ValueTree, Sequencer::numLanes> laneSet;
        std::array<juce::ValueTree, Sequencer::numPatterns> pats;
        for (const auto& c : w)
        {
            if (c.hasType ("LANE")) laneSet[(size_t) clampInt (c.getProperty ("l"), 0, Sequencer::numLanes - 1, 0)] = c;
            else if (c.hasType ("PAT")) pats[(size_t) clampInt (c.getProperty ("p"), 0, Sequencer::numPatterns - 1, 0)] = c;
        }
        for (int l = 0; l < Sequencer::numLanes; ++l)
        {
            const auto& c = laneSet[(size_t) l];
            const auto dir = (Direction) clampInt (c.getProperty ("dir"), 0, 3, 0);
            if (seq.getDirection (tr, l) != dir) seq.setDirection (tr, l, dir);
            if (seq.getLaneMute (tr, l) != (bool) c.getProperty ("mute", false)) seq.setLaneMute (tr, l, (bool) c.getProperty ("mute", false));
            if (seq.getLaneSolo (tr, l) != (bool) c.getProperty ("solo", false)) seq.setLaneSolo (tr, l, (bool) c.getProperty ("solo", false));
            if (seq.getLaneLockGlide (tr, l) != (bool) c.getProperty ("lglide", false)) seq.setLaneLockGlide (tr, l, (bool) c.getProperty ("lglide", false));
            if (seq.getLaneLockMute (tr, l) != (bool) c.getProperty ("lmute", false)) seq.setLaneLockMute (tr, l, (bool) c.getProperty ("lmute", false));
        }
        for (int p = 0; p < Sequencer::numPatterns; ++p)
        {
            const auto& pt = pats[(size_t) p];
            const auto length = clampInt (pt.getProperty ("length"), 1, Sequencer::maxSteps, 16);
            const auto speed = clampInt (pt.getProperty ("speed"), 0, numSpeeds - 1, 4);
            if (seq.getPatternLength (tr, p) != length) seq.setPatternLength (tr, p, length);
            if (seq.getPatternSpeed (tr, p) != speed) seq.setPatternSpeed (tr, p, speed);
            const auto div = (Division) clampInt (pt.getProperty ("division"), 0, numDivisions - 1, globalDivision);
            if (seq.getPatternDivision (tr, p) != div) seq.setPatternDivision (tr, p, div);
            const auto flags = fromHex (pt.getProperty ("flags").toString(), Sequencer::maxSteps);
            for (int s = 0; s < Sequencer::maxSteps; ++s)
            {
                const auto f = std::max (0, flags[(size_t) s]);
                if (seq.getStepFlagsP (tr, p, s) != f) seq.setStepFlagsP (tr, p, s, f);
            }
            {
                const auto amts = fromHex16 (pt.getProperty ("amt").toString(), Sequencer::maxSteps); // v0.46 fix27
                for (int s = 0; s < Sequencer::maxSteps; ++s)
                {
                    const auto v = std::max (0, amts[(size_t) s]);
                    if (seq.getStepAmountsP (tr, p, s) != v) seq.setStepAmountsP (tr, p, s, v);
                }
            }
            juce::StringArray lens;
            lens.addTokens (pt.getProperty ("laneLen").toString().substring (0, 200), ",", "");
            std::array<juce::ValueTree, Sequencer::numLanes> lanes;
            for (const auto& lane : pt)
                if (lane.hasType ("LANE")) lanes[(size_t) clampInt (lane.getProperty ("l"), 0, Sequencer::numLanes - 1, 0)] = lane;
            for (int l = 0; l < Sequencer::numLanes; ++l)
            {
                const auto ll = l < lens.size() ? juce::jlimit (0, Sequencer::maxSteps, lens[l].getIntValue()) : 0;
                if (seq.getLaneLengthP (tr, p, l) != ll) seq.setLaneLengthP (tr, p, l, ll);
                const auto& lane = lanes[(size_t) l];
                if (! lane.isValid())
                {
                    // voie vide : seules les notes présentes sont effacées (le reste n'est pas joué)
                    for (int s = 0; s < Sequencer::maxSteps; ++s)
                        if (seq.getStepP (tr, p, l, s) != 0) seq.setStepP (tr, p, l, s, 0);
                    continue;
                }
                const auto vel = fromHex (lane.getProperty ("vel").toString(), Sequencer::maxSteps);
                const auto note = fromHex (lane.getProperty ("note").toString(), Sequencer::maxSteps);
                const auto len = readLengths (lane, Sequencer::maxSteps);
                const auto micro = fromHex (lane.getProperty ("micro").toString(), Sequencer::maxSteps);
                for (int s = 0; s < Sequencer::maxSteps; ++s)
                {
                    // hauteur, durée et décalage d'abord : la note ne part jamais avec les anciens
                    const auto n = note[(size_t) s] >= 0 ? note[(size_t) s] : 60;
                    const auto d = len[(size_t) s] > 0 ? len[(size_t) s] : 8;
                    const auto m = micro[(size_t) s] >= 0 ? (int) (std::int8_t) (std::uint8_t) micro[(size_t) s] : 0;
                    const auto v = std::max (0, vel[(size_t) s]);
                    if (seq.getNoteP (tr, p, l, s) != n) seq.setNoteP (tr, p, l, s, n);
                    if (seq.getLength8P (tr, p, l, s) != d) seq.setLength8P (tr, p, l, s, d);
                    if (seq.getMicroP (tr, p, l, s) != m) seq.setMicroP (tr, p, l, s, m);
                    if (seq.getStepP (tr, p, l, s) != v) seq.setStepP (tr, p, l, s, v);
                }
            }
        }
    };
    auto content = [] (const juce::ValueTree& v)
    {
        auto copy = v.createCopy();
        copy.removeProperty ("mute", nullptr); copy.removeProperty ("solo", nullptr);
        return copy;
    };
    std::unique_ptr<PatternExtras> extras;
    for (int tr = 0; tr < numTracks; ++tr)
    {
        const auto wanted = trackOf (seqWanted, tr), current = trackOf (seqNow, tr);
        seq.setTrackMute (tr, (bool) wanted.getProperty ("mute", false));
        seq.setTrackSolo (tr, (bool) wanted.getProperty ("solo", false));
        if (! (wanted.isValid() && current.isValid() && content (wanted).isEquivalentTo (content (current))))
            writeTrack (tr, wanted);

        // conditions, répétitions, flams et verrous de la piste : rebâtis d'un bloc s'ils diffèrent
        auto onTrack = [tr] (const juce::ValueTree& c) { return (int) c.getProperty ("t", -1) == tr; };
        const auto extrasWanted = collect (snap, "EXTRA", onTrack);
        if (same (extrasWanted, collect (now, "EXTRA", onTrack))) continue;
        if (extras == nullptr) extras = std::make_unique<PatternExtras> (engine.getExtras());
        for (auto it = extras->steps.begin(); it != extras->steps.end();)
            it = PatternExtras::keyTrack (it->first) == tr ? extras->steps.erase (it) : std::next (it);
        for (const auto& c : extrasWanted)
        {
            const auto pt = clampInt (c.getProperty ("p"), 0, Sequencer::numPatterns - 1, 0);
            const auto l = clampInt (c.getProperty ("l"), 0, Sequencer::numLanes - 1, 0);
            const auto st = clampInt (c.getProperty ("s"), 0, Sequencer::maxSteps - 1, 0);
            auto& x = extras->steps[PatternExtras::key (tr, pt, l, st)];
            x.cond = (Condition) clampInt (c.getProperty ("cond"), 0, numConditions - 1, 0);
            x.a = (std::uint8_t) clampInt (c.getProperty ("a"), 1, 8, 1);
            x.b = (std::uint8_t) clampInt (c.getProperty ("b"), 1, 8, 2);
            x.prob = (std::uint8_t) clampInt (c.getProperty ("prob"), 0, 100, 100);
            x.ratchet = (std::uint8_t) clampInt (c.getProperty ("rat"), 1, Sequencer::maxRatchet, 1);
            x.ratchetSlope = (std::int8_t) clampInt (c.getProperty ("rs"), -100, 100, 0);
            x.flam = (std::uint8_t) clampInt (c.getProperty ("flam"), 0, maxFlamMs, 0);
            x.direct = (int) c.getProperty ("arp", 0) == 1;
            x.locks.clear();
            for (const auto& lock : c)
                if (const auto g = globalOf (addr (lock)); g >= 0 && x.locks.size() < 64)
                    x.locks.push_back ({ g, migrateValue (snapVersion, g, unit (lock.getProperty ("v"))) });
        }
    }
    if (extras != nullptr) engine.replaceExtras (std::move (extras), t);

    // rangées et macros : écrites seulement si elles changent ; une macro réattribuée ne
    // repousse pas sa valeur sur le réglage rétabli
    // morceau : remplacé seulement s'il a changé (rangées, courbes, modes)
    {
        const auto wanted = snap.getChildWithName ("SONG");
        const auto songNow = songToTree();
        const bool songSame = wanted.isValid() ? (songNow.isValid() && wanted.isEquivalentTo (songNow)) : ! songNow.isValid();
        if (! songSame) songFromTree (wanted, t);
    }
    const auto rows = snap.getChildWithName ("ROWS");
    for (int r = 0; r < Sequencer::numPatterns; ++r)
    {
        const auto v = clampInt (rows.getProperty ("scene" + juce::String (r)), -1, numScenes - 1, -1);
        if (rowScene[(size_t) r].load() != v) { rowScene[(size_t) r].store (v); rowVersion.fetch_add (1); }
    }
    std::array<std::vector<MacroLink>, numMacros> macros;
    for (const auto& c : snap)
    {
        if (c.hasType ("MACROMODE"))
        {
            const auto rel = (int) c.getProperty ("rel", 0);
            for (int m = 0; m < numMacros; ++m) macroRel[(size_t) m].store (((rel >> m) & 1) != 0);
        }
        if (c.hasType ("MACRO"))
            if (const auto g = globalOf (addr (c)); g >= 0)
                macros[(size_t) clampInt (c.getProperty ("m"), 0, numMacros - 1, 0)].push_back (
                    { g, c.hasProperty ("lo") ? unit (c.getProperty ("lo")) : 0.0f, c.hasProperty ("hi") ? unit (c.getProperty ("hi")) : 1.0f,
                      c.hasProperty ("base") ? unit (c.getProperty ("base")) : -1.0f, c.hasProperty ("ref") ? unit (c.getProperty ("ref")) : -1.0f });
    }
    for (int m = 0; m < numMacros; ++m)
    {
        const auto cur = getMacroLinks (m);
        const bool macroSame = cur.size() == macros[(size_t) m].size()
            && std::equal (cur.begin(), cur.end(), macros[(size_t) m].begin(), [] (const MacroLink& x, const MacroLink& y)
                           { return x.global == y.global && std::abs (x.lo - y.lo) < 1.0e-4f && std::abs (x.hi - y.hi) < 1.0e-4f; });
        if (! macroSame) setMacroLinks (m, macros[(size_t) m], false);
    }

    applyProjectContent (part, false, t);
    engine.endBatch (nowMs());

    // 6. Échantillons, hors du lot (le décodage peut être long) : rechargés depuis la copie
    //    partagée seulement s'ils ont changé
    for (int tr = 0; tr < numTracks; ++tr)
        for (int i = 0; i < maxSamples; ++i)
        {
            const auto& w = wantedSamples[(size_t) tr][(size_t) i];
            const auto* s = engine.getSample (tr, i);
            if (w == nullptr)
            {
                if (s != nullptr) engine.setSample (tr, i, nullptr, nowMs());
                continue;
            }
            if (s != nullptr && s->original == w->bytes && s->name == w->name) continue;
            // déjà décodé ailleurs (piste déplacée, étape d'historique) : simple copie, bien plus rapide
            // (les échantillons retirés restent en vie dans la corbeille du moteur pendant 3 s)
            const SampleBuffer* known = nullptr;
            for (const auto* r : reuseSamples) if (r->original == w->bytes && r->name == w->name && ! r->firmware) { known = r; break; } // v0.49 : un firmware a sa propre machine
            if (known != nullptr)
            {
                engine.setSample (tr, i, std::make_unique<SampleBuffer> (*known), nowMs());
                sampleRefs[(size_t) tr][(size_t) i] = w;
                continue;
            }
            if (loadSampleBytes (tr, i, juce::String::fromUTF8 (w->name.c_str()), w->bytes).isEmpty())
                sampleRefs[(size_t) tr][(size_t) i] = w; // même lien : l'étape suivante reste identique
        }
        reuseSamples.clear();

    projectVersion.fetch_add (1);
}

bool GrooveboxProcessor::commitHistory()
{
    auto snap = snapshotForHistory();
    if (committed.isValid() && snap.isEquivalentTo (committed)) return false;
    if (committed.isValid())
    {
        undoStack.push_back (committed);
        if (undoStack.size() > maxHistory) undoStack.erase (undoStack.begin());
        redoStack.clear();
    }
    committed = snap;
    trimHistory();
    return true;
}

void GrooveboxProcessor::trimHistory()
{
    // Les échantillons que seul l'historique garde en mémoire ont un budget : au-delà, les étapes
    // les plus anciennes sont oubliées (une tablette n'a pas la mémoire d'un ordinateur).
   #if JUCE_ANDROID || JUCE_IOS
    constexpr size_t budget = (size_t) 192 * 1024 * 1024;
   #else
    constexpr size_t budget = (size_t) 1024 * 1024 * 1024;
   #endif
    std::set<const void*> live;
    auto refsOf = [] (const juce::ValueTree& tree, auto f)
    {
        for (const auto& c : tree)
            if (c.hasType ("SAMPLE"))
                if (auto* r = dynamic_cast<SampleRef*> (c.getProperty ("ref").getObject()); r != nullptr && r->bytes != nullptr) f (*r);
    };
    refsOf (committed, [&] (const SampleRef& r) { live.insert (r.bytes.get()); });
    refsOf (photo, [&] (const SampleRef& r) { live.insert (r.bytes.get()); });
    auto held = [&]
    {
        std::set<const void*> seen;
        size_t total = 0;
        auto add = [&] (const SampleRef& r) { if (! live.count (r.bytes.get()) && seen.insert (r.bytes.get()).second) total += r.bytes->size(); };
        for (const auto& s : undoStack) refsOf (s, add);
        for (const auto& s : redoStack) refsOf (s, add);
        return total;
    };
    while (! undoStack.empty() && held() > budget) undoStack.erase (undoStack.begin());
    while (! redoStack.empty() && held() > budget) redoStack.erase (redoStack.begin());
}

void GrooveboxProcessor::historyTick()
{
    // une étape est rangée quand le projet a changé puis n'a plus bougé pendant 0,4 s
    // (un geste sur un bouton donne une seule étape) ; au repos, rien n'est copié
    const auto stamp = changeStamp();
    if (stamp != historyStamp) { historyStamp = stamp; historyCountdown = 10; return; }
    if (historyCountdown > 0) { if (--historyCountdown > 0) return; }
    else if (committed.isValid()) return;
    commitHistory();
}

bool GrooveboxProcessor::undo()
{
    commitHistory(); // un réglage tout juste fait compte comme une étape
    if (undoStack.empty()) return false;
    redoStack.push_back (committed);
    const auto target = undoStack.back();
    undoStack.pop_back();
    // retour avant un déplacement de pistes : l'ordre inverse remet aussi le jeu, la photo et le MIDI
    MoveStep* crossing = nullptr;
    for (auto& m : moveSteps) if (m.before == target) crossing = &m;
    std::array<int, numTracks> inverse {};
    if (crossing != nullptr)
    {
        for (int t = 0; t < numTracks; ++t) inverse[(size_t) crossing->newIndex[(size_t) t]] = t;
        crossing->redoMarker = redoStack.back();
        prepareTrackPermutation (inverse);
    }
    restoreSnapshot (target);
    if (crossing != nullptr) applyTrackPermutationExtras (inverse);
    committed = snapshotForHistory();
    historyStamp = changeStamp(); historyCountdown = 0;
    return true;
}

bool GrooveboxProcessor::redo()
{
    if (commitHistory()) return false; // un changement après le retour en arrière efface la suite
    if (redoStack.empty()) return false;
    undoStack.push_back (committed);
    const auto target = redoStack.back();
    redoStack.pop_back();
    MoveStep* crossing = nullptr;
    for (auto& m : moveSteps) if (m.redoMarker.isValid() && m.redoMarker == target) crossing = &m;
    if (crossing != nullptr) { crossing->before = undoStack.back(); prepareTrackPermutation (crossing->newIndex); }
    restoreSnapshot (target);
    if (crossing != nullptr) applyTrackPermutationExtras (crossing->newIndex);
    committed = snapshotForHistory();
    historyStamp = changeStamp(); historyCountdown = 0;
    return true;
}

void GrooveboxProcessor::takePhoto()
{
    commitHistory();
    const juce::SpinLock::ScopedLockType lock (photoLock);
    photo = committed;
}

bool GrooveboxProcessor::recallPhoto()
{
    if (! photo.isValid()) return false;
    commitHistory();
    // v0.46 : la photo rappelle les réglages, pas le morceau : rangées, courbes et enregistrement
    // en cours restent tels quels (avant, le morceau de la photo remplaçait celui qu'on enregistrait
    // et la lecture repartait à 0)
    auto snap = photo.createCopy();
    if (const auto old = snap.getChildWithName ("SONG"); old.isValid()) snap.removeChild (old, nullptr);
    if (const auto songNow = songToTree(); songNow.isValid()) snap.appendChild (songNow.createCopy(), nullptr);
    // enregistrement des mouvements : le rappel s'enregistre comme un geste (rejoué au même endroit)
    const bool recMoves = engine.isSongRecording(); // v0.46 fix7 : ENR. en mode morceau aussi
    std::vector<float> before;
    if (recMoves) { before.resize ((size_t) numSlots); for (int g = 0; g < numSlots; ++g) before[(size_t) g] = engine.getParam (g); }
    // le rappel de la photo est lui-même une étape : Ctrl+Z revient à l'état d'avant
    restoreSnapshot (snap);
    if (recMoves)
        for (int g = 0; g < numSlots; ++g)
            if (const auto v = engine.getParam (g); std::abs (v - before[(size_t) g]) > 1.0e-6f && engine.describe (g) != nullptr) engine.recordMoveUi (g, v);
    commitHistory();
    historyStamp = changeStamp(); historyCountdown = 0;
    return true;
}

void GrooveboxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    tree.removeChild (tree.getChildWithName ("PROJECT"), nullptr);
    tree.appendChild (projectToTree(), nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

/** v0.46 fix26 : un état enregistré par une version plus ancienne n'a pas les réglages ajoutés depuis
    (pavé vectoriel…) : ils sont ajoutés à leur valeur d'origine, sinon ils garderaient celle de la séance
    en cours (pavé décentré en ouvrant la démonstration, par exemple). */
void GrooveboxProcessor::fillMissingParams (juce::ValueTree& tree) const
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            bool found = false;
            for (const auto& c : tree) if (c.hasType ("PARAM") && c.getProperty ("id").toString() == id) { found = true; break; }
            if (found) continue;
            juce::ValueTree pt ("PARAM");
            pt.setProperty ("id", id, nullptr);
            pt.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
            tree.appendChild (pt, nullptr);
        }
}

void GrooveboxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
   #if JUCE_ANDROID
    // Au démarrage, l'enregistrement automatique est plus récent que l'état gardé par l'application
    if (restoredFromAutosave && nowMs() - constructedAt < 10000.0) { restoredFromAutosave = false; return; }
   #endif
    grv::startupMark ("derniere seance : lecture");
    // v0.46.1 (audit F3) : état venu d'une session d'un logiciel hôte (peut avoir été échangée) :
    // profondeur des balises vérifiée avant la lecture (format de JUCE : « VC2! », taille, texte)
    if (data == nullptr || sizeInBytes <= 8) return;
    if (! xmlNestingWithin (static_cast<const char*> (data) + 8, (std::size_t) sizeInBytes - 8)) return;
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    const auto project = tree.getChildWithName ("PROJECT");
    tree.removeChild (project, nullptr);
    fillMissingParams (tree); // v0.46 fix26
    state.replaceState (tree);
    setVecPath (state.state.getProperty ("vecPath").toString(), false); // v0.46 fix22 : forme dessinée du pavé
    if (project.isValid()) projectFromTree (project);
    grv::startupMark ("derniere seance : relue");
}

bool GrooveboxProcessor::writeProjectFile (juce::OutputStream& out, bool asTemplate)
{
    auto tree = state.copyState();
    tree.removeChild (tree.getChildWithName ("PROJECT"), nullptr);
    // v0.46 fix8 : projet écrit dans un fichier : les prises du montage rejoignent « <nom> - audio »
    if (! asTemplate && projectContext != juce::File())
    {
        montageDoc.pruneTakes();
        montage::bindToProject (montageDoc, projectContext);
        montagePlayer.setDoc (montageDoc, getSampleRate() > 0.0 ? getSampleRate() : 48000.0);
    }
    auto project = projectToTree();
    if (asTemplate)
    {
        // v0.46 : modèle — tous les réglages (sons, échantillons, effets, mixeur, scènes, macros,
        // MIDI) restent ; tout ce qui est écrit part : patterns et notes, verrous de pas, morceau
        // (rangées, courbes), rangées de lancement, photo ; pistes ni coupées ni en solo
        for (int n = project.getNumChildren(); --n >= 0;)
        {
            auto c = project.getChild (n);
            if (c.hasType ("EXTRA") || c.hasType ("SONG") || c.hasType ("PHOTO") || c.hasType ("ROWS") || c.hasType ("MONTAGE")) project.removeChild (n, nullptr);
            else if (c.hasType ("SEQ"))
                for (auto tr : c)
                {
                    tr.removeAllChildren (nullptr);
                    tr.setProperty ("play", 0, nullptr); tr.setProperty ("edit", 0, nullptr);
                    tr.setProperty ("mute", false, nullptr); tr.setProperty ("solo", false, nullptr);
                }
        }
    }
    tree.appendChild (project, nullptr);
    auto xml = tree.createXml();
    if (xml == nullptr) return false;
    xml->writeTo (out);
    out.flush();
    return true;
}

juce::String GrooveboxProcessor::readProjectFile (juce::InputStream& in)
{
    // 600 Mo au plus : 16 pistes d'échantillons encodés en base 64.
    // v0.46.1 (audit F3) : 256 Mo sur téléphone et tablette (le fichier est copié plusieurs fois en
    // mémoire pendant la lecture : au-delà, l'application serait arrêtée par le système)
   #if JUCE_ANDROID || JUCE_IOS
    constexpr size_t limit = (size_t) 256 * 1024 * 1024;
   #else
    constexpr size_t limit = (size_t) 600 * 1024 * 1024;
   #endif
    const auto size = in.getTotalLength();
    if (size > (juce::int64) limit) return "Projet trop lourd";
    // lecture bornée, même si la taille du flux est inconnue
    juce::MemoryBlock raw;
    in.readIntoMemoryBlock (raw, (int) (limit + 1)); // 600 Mo tiennent dans un int
    if (raw.getSize() > limit) return "Projet trop lourd";
    // v0.46.1 (audit F3) : balises imbriquées sans fin (fichier piégé) : refusé avant la lecture
    if (! xmlNestingWithin (static_cast<const char*> (raw.getData()), raw.getSize()))
        return juce::String::fromUTF8 ("Ce fichier n'est pas un projet Groovebox");
    const auto text = raw.toString();
    raw.reset();
    if (text.isEmpty()) return "Fichier vide";
    auto xml = juce::parseXML (text);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType())) return juce::String::fromUTF8 ("Ce fichier n'est pas un projet Groovebox");
    auto tree = juce::ValueTree::fromXml (*xml);
    const auto project = tree.getChildWithName ("PROJECT");
    if (! project.isValid()) return juce::String::fromUTF8 ("Projet incomplet");
    tree.removeChild (project, nullptr);
    fillMissingParams (tree); // v0.46 fix26 : projet plus ancien (sans pavé…) : ses réglages absents repartent de leur valeur d'origine
    state.replaceState (tree);
    setVecPath (state.state.getProperty ("vecPath").toString(), false); // v0.46 fix22
    projectFromTree (project);
    return {};
}

juce::AudioProcessorEditor* GrooveboxProcessor::createEditor()
{
   #if GROOVEBOX_NO_EDITOR
    return nullptr; // outil de rendu hors temps réel
   #else
    return new GrooveboxEditor (*this);
   #endif
}

} // namespace grv

#if ! GROOVEBOX_NO_EDITOR
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new grv::GrooveboxProcessor();
}
#endif

namespace grv
{

using namespace engine;

//==============================================================================
// Morceau : rangées et courbes, rangées par adresse stable (comme les réglages)

juce::ValueTree GrooveboxProcessor::songToTree() const
{
    const auto& sg = engine.getSong();
    if (sg.rows.empty() && sg.lanes.empty() && sg.patLanes.empty() && ! sg.songMode) return {};
    juce::ValueTree tree ("SONG");
    tree.setProperty ("mode", sg.songMode, nullptr);
    tree.setProperty ("loop", sg.loop, nullptr);
    if (sg.loopFrom != 0) tree.setProperty ("loopFrom", sg.loopFrom, nullptr);
    if (sg.loopTo != -1) tree.setProperty ("loopTo", sg.loopTo, nullptr);
    if (sg.loopCount != 0) tree.setProperty ("loopCount", sg.loopCount, nullptr);
    tree.setProperty ("read", sg.autoRead, nullptr);
    for (const auto& row : sg.rows)
    {
        juce::ValueTree r ("ROW");
        r.setProperty ("bars", row.bars, nullptr);
        if (row.scene >= 0) r.setProperty ("scene", row.scene, nullptr);
        juce::String pats;
        for (int t = 0; t < numTracks; ++t) pats << (t ? "," : "") << (int) row.pat[(size_t) t];
        r.setProperty ("pat", pats, nullptr);
        tree.appendChild (r, nullptr);
    }
    for (const auto& lane : sg.lanes)
    {
        if (lane.pts.empty()) continue;
        juce::ValueTree l ("AUTO");
        if (lane.global == xfadeLane) l.setProperty ("sw", "xfade", nullptr); // v0.46 : crossfader des scènes
        else if (isSwitchLane (lane.global))
        {
            // v0.46 : courbe de coupure / solo d'une piste
            l.setProperty ("sw", switchIsSolo (lane.global) ? "solo" : "mute", nullptr);
            l.setProperty ("t", switchTrack (lane.global), nullptr);
        }
        else if (isBusSwitchLane (lane.global))
        {
            // v0.46 fix7 : coupure / solo d'un retour, du principal ou d'un groupe (numéro de canal)
            l.setProperty ("sw", busSwitchIsSolo (lane.global) ? "bsolo" : "bmute", nullptr);
            l.setProperty ("ch", busSwitchChannel (lane.global), nullptr);
        }
        else
        {
            const auto a = addressOf (lane.global);
            if (a.channel < 0) continue;
            l.setProperty ("c", a.channel, nullptr); l.setProperty ("a", a.area, nullptr); l.setProperty ("i", a.id, nullptr);
        }
        if (! lane.enabled) l.setProperty ("off", true, nullptr);
        // « temps:valeur » séparés par des espaces : 4 décimales, lisible
        juce::String pts;
        pts.preallocateBytes (lane.pts.size() * 16);
        for (const auto& p : lane.pts) pts << juce::String (p.beat, 4) << ":" << juce::String (p.norm, 4) << " ";
        l.setProperty ("pts", pts.trimEnd(), nullptr);
        tree.appendChild (l, nullptr);
    }
    // courbes de pattern : piste, pattern, réglage, points « pas:valeur »
    for (const auto& pl : sg.patLanes)
    {
        const auto a = addressOf (pl.lane.global);
        if (a.channel < 0 || pl.lane.pts.empty()) continue;
        juce::ValueTree l ("PAUTO");
        l.setProperty ("t", pl.track, nullptr); l.setProperty ("p", pl.pattern, nullptr);
        l.setProperty ("c", a.channel, nullptr); l.setProperty ("a", a.area, nullptr); l.setProperty ("i", a.id, nullptr);
        if (! pl.lane.enabled) l.setProperty ("off", true, nullptr);
        juce::String pts;
        pts.preallocateBytes (pl.lane.pts.size() * 16);
        for (const auto& p : pl.lane.pts) pts << juce::String (p.beat, 4) << ":" << juce::String (p.norm, 4) << " ";
        l.setProperty ("pts", pts.trimEnd(), nullptr);
        tree.appendChild (l, nullptr);
    }
    return tree;
}

void GrooveboxProcessor::songFromTree (const juce::ValueTree& tree, double t)
{
    Song sg;
    if (tree.isValid() && tree.hasType ("SONG"))
    {
        sg.songMode = (bool) tree.getProperty ("mode", false);
        sg.loop = (bool) tree.getProperty ("loop", true);
        sg.loopFrom = (int) tree.getProperty ("loopFrom", 0);
        sg.loopTo = (int) tree.getProperty ("loopTo", -1);
        sg.loopCount = juce::jlimit (0, 999, (int) tree.getProperty ("loopCount", 0));
        sg.autoRead = (bool) tree.getProperty ("read", true);
        for (const auto& c : tree)
        {
            if (c.hasType ("ROW") && (int) sg.rows.size() < maxSongRows)
            {
                SongRow row;
                row.bars = juce::jlimit (1, maxRowBars, (int) c.getProperty ("bars", 4));
                row.scene = juce::jlimit (-1, numScenes - 1, (int) c.getProperty ("scene", -1));
                juce::StringArray pats;
                pats.addTokens (c.getProperty ("pat").toString().substring (0, 200), ",", "");
                for (int k = 0; k < numTracks && k < pats.size(); ++k)
                    row.pat[(size_t) k] = (std::int8_t) juce::jlimit (songKeep, Sequencer::numPatterns - 1, pats[k].getIntValue());
                sg.rows.push_back (row);
            }
            else if (c.hasType ("AUTO") && (int) sg.lanes.size() < maxAutoLanes)
            {
                auto g = globalOf ({ (int) c.getProperty ("c", -1), c.getProperty ("a").toString(), c.getProperty ("i").toString() });
                const auto sw = c.getProperty ("sw").toString();
                if (sw == "xfade") g = xfadeLane; // v0.46 : crossfader des scènes
                else if (sw == "bmute" || sw == "bsolo") // v0.46 fix7 : coupure / solo d'un bus
                {
                    const auto ch = (int) c.getProperty ("ch", -1);
                    g = ch >= numTracks && ch < numChannels && (sw == "bmute" || isGroupChannel (ch)) ? (sw == "bsolo" ? busSoloLane (ch) : busMuteLane (ch)) : -1;
                }
                else if (c.hasProperty ("sw")) // v0.46 : coupure / solo d'une piste
                {
                    const auto tr = (int) c.getProperty ("t", -1);
                    g = tr >= 0 && tr < numTracks ? (c.getProperty ("sw").toString() == "solo" ? soloLane (tr) : muteLane (tr)) : -1;
                }
                if (g < 0) continue;
                AutoLane lane;
                lane.global = g;
                lane.enabled = ! (bool) c.getProperty ("off", false);
                juce::StringArray pts;
                pts.addTokens (c.getProperty ("pts").toString().substring (0, maxAutoPoints * 24), " ", "");
                for (const auto& p : pts)
                {
                    if ((int) lane.pts.size() >= maxAutoPoints) break;
                    const auto beat = p.upToFirstOccurrenceOf (":", false, false).getFloatValue();
                    const auto norm = p.fromFirstOccurrenceOf (":", false, false).getFloatValue();
                    if (std::isfinite (beat) && std::isfinite (norm) && beat >= 0.0f) lane.pts.push_back ({ beat, autoNorm (norm) });
                }
                if (! lane.pts.empty()) sg.lanes.push_back (std::move (lane));
            }
            else if (c.hasType ("PAUTO") && (int) sg.patLanes.size() < maxPatLanes)
            {
                const auto g = globalOf ({ (int) c.getProperty ("c", -1), c.getProperty ("a").toString(), c.getProperty ("i").toString() });
                if (g < 0) continue;
                PatternLane pl;
                pl.track = juce::jlimit (-1, numTracks - 1, (int) c.getProperty ("t", -1));
                pl.pattern = juce::jlimit (0, Sequencer::numPatterns - 1, (int) c.getProperty ("p", 0));
                if (pl.track < 0) continue;
                pl.lane.global = g;
                pl.lane.enabled = ! (bool) c.getProperty ("off", false);
                juce::StringArray pts;
                pts.addTokens (c.getProperty ("pts").toString().substring (0, maxAutoPoints * 24), " ", "");
                for (const auto& p : pts)
                {
                    if ((int) pl.lane.pts.size() >= maxAutoPoints) break;
                    const auto beat = p.upToFirstOccurrenceOf (":", false, false).getFloatValue();
                    const auto norm = p.fromFirstOccurrenceOf (":", false, false).getFloatValue();
                    if (std::isfinite (beat) && std::isfinite (norm) && beat >= 0.0f) pl.lane.pts.push_back ({ beat, autoNorm (norm) });
                }
                if (! pl.lane.pts.empty()) sg.patLanes.push_back (std::move (pl));
            }
        }
    }
    engine.setSong (std::move (sg), t);
}


//==============================================================================
// v0.32 : enregistrement audio
juce::String GrooveboxProcessor::audioRecStart (int mode, int format, int follow)
{
    std::vector<Recorder::TrackInfo> tracks;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& info = engine.getTrackInfo (t);
        if (! info.instrumentType.empty()) tracks.push_back ({ t, juce::String::fromUTF8 (info.name.c_str()) });
    }
    std::array<bool, numReturns> returnsUsed {};
    for (int r = 0; r < numReturns; ++r)
        for (const auto& type : engine.getInsertTypes (numTracks + r))
            if (! type.empty()) returnsUsed[(size_t) r] = true;
    juce::String error;
    const auto sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    if (montageTakeOn || montageExportOn) return juce::String::fromUTF8 ("Un enregistrement est déjà en cours");
    // v0.46.2 : prise calée sur la lecture. Lecture en cours : départ au début du prochain tour de
    // la boucle la plus longue (ou de la prochaine mesure si rien ne boucle)
    if (follow == 2)
    {
        // v0.46.2 : boucle parfaite : un tour d'élan, puis exactement un tour (début donné par audioRecTick)
        recorder.setFollowPlayback (true, -1.0);
        recorder.setLoopTake (true);
    }
    else if (follow == 1)
    {
        auto& seq = engine.sequencer();
        double startPpq = -1.0;
        if (seq.isRunning())
        {
            const auto now = seq.getPpq();
            double phase = 0.0;
            const auto loopQ = seq.longestLoop (now, phase);
            startPpq = loopQ > 1.0e-6 ? now + (1.0 - phase) * loopQ : std::ceil (now / 4.0 + 1.0e-7) * 4.0;
        }
        recorder.setFollowPlayback (true, startPpq);
    }
    if (! recorder.start ((Recorder::Mode) juce::jlimit (0, 2, mode), format == 1 ? Recorder::Format::mp3 : Recorder::Format::wav,
                          sr, tracks, returnsUsed, error))
        recorder.setFollowPlayback (false);
    return error;
}

juce::String GrooveboxProcessor::audioRecStartEx (const juce::var& o)
{
    Recorder::TakeOptions t;
    t.custom = true;
    t.master = (bool) o.getProperty ("master", true);
    t.masterTreated = (bool) o.getProperty ("masterTreated", false);
    t.trackMask = (std::uint32_t) ((juce::int64) o.getProperty ("tracks", 0) & 0xFFFF);
    t.sumTracks = (bool) o.getProperty ("sum", false);
    t.returnMask = (std::uint32_t) ((int) o.getProperty ("returns", 0) & 0xF);
    t.input = (bool) o.getProperty ("input", false);
    t.chain = parseChain (o.getProperty ("chain", juce::var()));
    t.during = (bool) o.getProperty ("during", false);
    t.dest = juce::jlimit (0, 3, (int) o.getProperty ("dest", 0));
    const int format = juce::jlimit (0, 2, (int) o.getProperty ("format", 0));
    t.float32 = format == 1;
    int follow = juce::jlimit (0, 3, (int) o.getProperty ("follow", 1));
    if (follow == 3) t.barsQuarters = 4.0 * juce::jlimit (1, 64, (int) o.getProperty ("bars", 4));
    t.treatedRoot = processedFolder();
    t.project = o.getProperty ("project", "").toString().substring (0, 60);
    t.bpm = currentBpm.load();
    if (! t.master && ! t.masterTreated && t.trackMask == 0 && t.returnMask == 0 && ! t.input) return juce::String::fromUTF8 ("Cochez au moins une source");
    recorder.setTakeOptions (std::move (t));
    const auto e = audioRecStart (0, format == 2 ? 1 : 0, follow == 3 ? 1 : follow);
    if (e.isNotEmpty()) recorder.setTakeOptions ({});
    return e;
}

juce::String GrooveboxProcessor::audioRecToPad (int track, int index)
{
    if (recorder.isRecording() || recorder.isProcessing()) return juce::String::fromUTF8 ("La prise n'est pas encore prête");
    const auto f = recorder.resultFile();
    if (! f.existsAsFile()) return juce::String::fromUTF8 ("Aucune prise à charger");
    if (f.getSize() > 512ll * 1024 * 1024) return juce::String::fromUTF8 ("Prise trop lourde pour un pad");
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) return juce::String::fromUTF8 ("Prise illisible");
    pushSampleVersion (track, index); // annulable, comme l'éditeur d'échantillon
    return loadSample (track, index, f.getFileName(), std::move (mb));
}

void GrooveboxProcessor::audioRecTick()
{
    // boucle parfaite : lecture en cours, on donne le début du prochain tour de la boucle la plus
    // longue (le tour en cours sert d'élan) ; rien ne boucle : la mesure suivante
    if (recorder.wantsLoopStart())
    {
        auto& seq = engine.sequencer();
        if (seq.isRunning())
        {
            const auto now = seq.getPpq();
            double phase = 0.0;
            auto loopQ = seq.longestLoop (now, phase);
            if (loopQ <= 1.0e-6) { loopQ = 4.0; phase = now / 4.0 - std::floor (now / 4.0); }
            recorder.setLoopStart (now + (1.0 - phase) * loopQ, loopQ);
        }
    }
    // prise calée finie d'elle-même (fin du morceau, ARRÊT, fin du tour de boucle)
    if (! montageTakeOn && ! montageExportOn && recorder.isRecording() && recorder.followsPlayback() && recorder.takeFull())
        recorder.stop();
}

void GrooveboxProcessor::audioRecStop()
{
    // v0.46.2 : prise calée, en lecture : elle s'arrête à la fin du tour de boucle en cours (à
    // l'échantillon près, rangée par audioRecTick) ; sinon tout de suite
    // (un 2e appui pendant l'attente de la fin du tour : arrêt tout de suite)
    if (recorder.isRecording() && recorder.followsPlayback() && ! recorder.takeFull() && ! recorder.stopPending())
    {
        auto& seq = engine.sequencer();
        double phase = 0.0;
        const auto loopQ = seq.isRunning() ? seq.longestLoop (seq.getPpq(), phase) : 0.0;
        if (recorder.requestStopAtCycle (loopQ)) return;
    }
    recorder.stop();
}

bool GrooveboxProcessor::audioRecReveal()
{
   #if JUCE_ANDROID || JUCE_IOS
    return false;
   #else
    const auto dir = recorder.lastFolder();
    return dir.isDirectory() && dir.startAsProcess();
   #endif
}

} // namespace grv


//==============================================================================
// v0.46 fix8 : montage audio
namespace grv
{
std::string GrooveboxProcessor::montageJson() const
{
    juce::String status = montageStatus;
    if (montageTakeOn) status = juce::String::fromUTF8 ("Prise en cours…");
    else if (montageJob != nullptr) status = juce::String::fromUTF8 ("Découpage de la prise en clips…");
    else if (montageExportOn) status = juce::String::fromUTF8 ("Export en cours…");
    return montage::toJson (montageDoc, montagePlayer.isActive(), montagePlayer.positionBeats(), status);
}

void GrooveboxProcessor::montageSetClips (const juce::String& json)
{
    if (montage::clipsFromJson (json, montageDoc))
        montagePlayer.setDoc (montageDoc, getSampleRate() > 0.0 ? getSampleRate() : 48000.0);
}

void GrooveboxProcessor::montageSetFlags (bool on, bool loop, double a, double b)
{
    montageDoc.loop = loop;
    montageDoc.loopA = std::clamp (a, 0.0, 1.0e6);
    montageDoc.loopB = std::clamp (std::max (b, montageDoc.loopA + 0.25), 0.0, 1.0e6);
    montagePlayer.setDoc (montageDoc, getSampleRate() > 0.0 ? getSampleRate() : 48000.0);
    if (! montageTakeOn) montagePlayer.setActive (on);
}

void GrooveboxProcessor::montageSetPlayhead (double beat)
{
    montageDoc.playhead = std::clamp (beat, 0.0, 1.0e6);
    montagePlayer.setPlayhead (montageDoc.playhead);
}

juce::String GrooveboxProcessor::montageRecord (bool start)
{
    if (! start)
    {
        if (! montageTakeOn) return {};
        montageTakeOn = false;
        montageTakeBpm = getCurrentBpm() > 0.0 ? getCurrentBpm() : montageTakeBpm; // tempo joué pendant la prise
        const auto startPpq = recorder.takeStartPpq();
        recorder.stop();
        engine.sequencer().requestStop();
        if (startPpq < 0.0) { montageStatus = juce::String::fromUTF8 ("Prise vide : la lecture n'a pas tourné"); montageTakeFolder.deleteRecursively(); return {}; }
        // découpage en clips dans un fil à part (relit les fichiers)
        auto result = std::make_shared<montage::TakeResult>();
        auto done = std::make_shared<std::atomic<bool>> (false);
        const auto folder = montageTakeFolder;
        const auto takeBpm = montageTakeBpm;
        const auto docBpm = montageDoc.clips.empty() ? takeBpm : montageDoc.bpm;
        if (montageDoc.clips.empty()) montageDoc.bpm = takeBpm;
        const auto prefix = folder.getFileName().replaceCharacters (" ", "_");
        const auto placeAt = montageTakeAt;
        const auto wholeSpan = montageTakeLoop; // prise de boucle : un clip par piste, de toute la boucle
        struct Job final : juce::Thread
        {
            Job (std::function<void()> f) : juce::Thread ("Groovebox · clips de la prise"), fn (std::move (f)) {}
            void run() override { fn(); }
            std::function<void()> fn;
        };
        montageJobResult = result;
        montageJobDone = done;
        montageJob = std::make_unique<Job> ([=] { *result = montage::analyseTake (folder, placeAt, takeBpm, docBpm, prefix, wholeSpan); done->store (true); });
        montageJob->startThread (juce::Thread::Priority::low);
        return {};
    }
    if (montageTakeOn || recorder.isRecording()) return juce::String::fromUTF8 ("Un enregistrement est déjà en cours");
    if (montageJob != nullptr) return juce::String::fromUTF8 ("La prise précédente est encore en cours de découpage");
    std::vector<Recorder::TrackInfo> tracks;
    for (int t = 0; t < numTracks; ++t)
        if (! engine.getTrackInfo (t).instrumentType.empty()) tracks.push_back ({ t, {} });
    std::array<bool, numReturns> returnsUsed {};
    for (int r = 0; r < numReturns; ++r)
        for (const auto& type : engine.getInsertTypes (numTracks + r))
            if (! type.empty()) returnsUsed[(size_t) r] = true;
    // dossier de la prise : à côté du projet s'il a un fichier, sinon dossier de la séance
    const auto base = projectContext != juce::File() ? montage::audioFolderFor (projectContext) : montage::sessionFolder();
    auto folder = base.getChildFile ("Prise " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %Hh%Mm%S"));
    if (folder.exists()) folder = folder.getNonexistentSibling (false);
    juce::String error;
    const auto rate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const auto bpm = getCurrentBpm() > 0.0 ? getCurrentBpm() : 120.0;
    // v0.46 fix12 : Boucle allumée → la prise dure exactement la boucle (arrêt seul, à l'échantillon
    // près) et se pose au début de la boucle : les clips tombent pile sur la boucle
    const bool loopTake = montageDoc.loop && montageDoc.loopB - montageDoc.loopA > 1.0e-6;
    recorder.setTakeLimit (loopTake ? (std::int64_t) std::llround ((montageDoc.loopB - montageDoc.loopA) * 60.0 / bpm * rate) : -1);
    montagePlayer.setActive (false); // on joue les instruments pendant la prise
    if (! recorder.startMontageTake (folder, rate, tracks, returnsUsed, error)) return error;
    montageTakeFolder = folder;
    montageTakeBpm = bpm;
    montageTakeLoop = loopTake;
    // la prise se pose au point de départ du montage, ramené à la mesure la plus proche (ou au début de
    // la boucle) ; elle commence au début de la mesure suivante de la lecture (ou tout de suite si la
    // lecture part de zéro)
    montageTakeAt = loopTake ? std::max (0.0, montageDoc.loopA) : std::round (std::max (0.0, montageDoc.playhead) / 4.0) * 4.0;
    montageTakeOn = true;
    montageStatus.clear();
    return {};
}

juce::String GrooveboxProcessor::montageExport (int format, bool loopOnly)
{
    if (montageDoc.clips.empty()) return juce::String::fromUTF8 ("Le montage est vide");
    if (recorder.isRecording() || montageTakeOn) return juce::String::fromUTF8 ("Un enregistrement est déjà en cours");
    std::vector<Recorder::TrackInfo> none;
    std::array<bool, numReturns> noReturns {};
    juce::String error;
    engine.sequencer().requestStop();
    montagePlayer.setActive (true);
    montagePlayer.setExporting (true);
    // v0.46.2 : l'export part au premier échantillon joué et finit pile : à la fin de la mesure qui
    // contient la fin du dernier clip (tout le montage), ou à la fin de la boucle (boucle seule)
    const auto rate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const auto bpm = getCurrentBpm() > 0.0 ? getCurrentBpm() : 120.0;
    const auto barFrames = 4.0 * 60.0 / bpm * rate;
    const bool loop = loopOnly && montageDoc.loopB - montageDoc.loopA > 1.0e-6;
    montagePlayer.setPlayhead (loop ? montageDoc.loopA : 0.0);
    const auto frames = loop ? (montageDoc.loopB - montageDoc.loopA) * 60.0 / bpm * rate
                             : std::max (1.0, std::ceil ((double) montagePlayer.endFrames() / barFrames - 1.0e-6)) * barFrames;
    recorder.setFollowPlayback (true, -2.0);
    recorder.setTakeLimit ((std::int64_t) std::llround (frames));
    if (! recorder.start (Recorder::Mode::master, format == 1 ? Recorder::Format::mp3 : Recorder::Format::wav,
                          rate, none, noReturns, error))
    {
        recorder.setFollowPlayback (false);
        recorder.setTakeLimit (-1);
        montagePlayer.setExporting (false);
        return error;
    }
    montageExportOn = true;
    montageExportWait = 0; // la lecture part quand le début est prêt (montageTick) : l'attaque n'est pas manquée
    return {};
}

juce::String GrooveboxProcessor::montagePeaks (const juce::String& id) const
{
    if (const auto* t = montageDoc.takeFor (id)) return montage::standardPeaks (t->peaks);
    return {};
}

void GrooveboxProcessor::montageTick()
{
    // prise arrêtée par la lecture (ARRÊT) : on range la prise
    if (montageTakeOn && ! engine.sequencer().isRunning() && recorder.takeStartPpq() >= 0.0) montageRecord (false);
    // v0.46 fix12 : prise de boucle complète : rangée et lecture arrêtée, pile à la fin de la boucle
    if (montageTakeOn && recorder.takeFull()) montageRecord (false);
    if (montageJob != nullptr && montageJobDone != nullptr && montageJobDone->load())
    {
        montageJob->stopThread (1000);
        montageJob.reset();
        const auto& r = *montageJobResult;
        if (r.clips.empty()) montageStatus = juce::String::fromUTF8 ("Prise sans son : aucun clip");
        else
        {
            montage::placeTake (montageDoc, r);
            montageStatus = juce::String (r.clips.size()) + juce::String::fromUTF8 (" clips ajoutés");
            ++montageTakeCount;
        }
        montageDoc.playhead = r.startBeat;
        montagePlayer.setDoc (montageDoc, getSampleRate() > 0.0 ? getSampleRate() : 48000.0);
        montagePlayer.setPlayhead (montageDoc.playhead);
        montagePlayer.setActive (true); // on écoute le montage tout de suite
        montageJobResult.reset();
        projectVersion.fetch_add (1);
    }
    // v0.46.2 : export : départ quand le fil de lecture a préparé le début (au plus 1 s d'attente)
    if (montageExportOn && montageExportWait >= 0)
    {
        if (montagePlayer.primed() || ++montageExportWait > 25) { montageExportWait = -1; engine.sequencer().requestPlay(); }
        return;
    }
    if (montageExportOn && (recorder.takeFull() || montagePlayer.exportFinished() || ! engine.sequencer().isRunning()))
    {
        montageExportOn = false;
        engine.sequencer().requestStop();
        recorder.stop();
        montagePlayer.setExporting (false);
        montagePlayer.setPlayhead (montageDoc.playhead);
        montageStatus = juce::String::fromUTF8 ("Montage exporté dans ") + recorder.lastFolderShown();
    }
}
} // namespace grv
