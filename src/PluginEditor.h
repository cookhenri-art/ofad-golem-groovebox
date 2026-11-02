/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Éditeur : une seule interface React affichée dans la WebView de JUCE 8, identique dans le
    plugin VST3 et dans l'application autonome.

    Contrat avec l'interface : voir ui/src/engine/protocol.ts (fonctions natives et événement « tick »).
    Sécurité : la page ne peut charger que l'interface embarquée ; chaque argument reçu de la page
    est vérifié (type, bornes, taille) avant d'atteindre le moteur ; aucun accès au réseau. Les fichiers ne
    sont lus ou écrits que par le sélecteur de fichiers du système, ouvert par l'utilisateur.
*/

#pragma once

#include "PluginProcessor.h"
#include "dsp/Spectrum.h"

#include <array>
#include <optional>

namespace grv
{

class GrooveboxEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit GrooveboxEditor (GrooveboxProcessor&);
    ~GrooveboxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    /** Application autonome : ajoute le bouton « agrandir » (carré) à la barre de titre de la fenêtre. */
    void parentHierarchyChanged() override;

private:
    void timerCallback() override;
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url) const;
    juce::WebBrowserComponent::Options createOptions();
    static juce::String devServerUrl();
    std::string midiJson() const;
    std::string launchJson() const;
    void finishFile (const juce::String& kind, const juce::String& error);

    struct Browser final : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override;
    };

    GrooveboxProcessor& processor;
    Browser browser;

    std::unique_ptr<juce::FileChooser> chooser;
    dsp::LoudnessReader loudness;
    dsp::SpectrumAnalyzer spectrum;          // v0.45.1 : spectre du bus principal (calculé ici, à 30 images/s)
    int spectrumOn = 0;                       // demandé par l'interface (0 : rien n'est calculé)
    int focusChannel = 0;
    std::array<std::array<std::uint32_t, 16>, engine::numTracks> lastHits {};
    std::array<std::uint32_t, 128> lastNoteHits {};   // notes jouées de la piste choisie (clavier, pads mélodiques)
    int lastNoteTrack = -1;
    std::uint32_t lastProject = 0, lastSeq = 0, lastStructure = 0, lastMidi = 0;
    std::uint64_t lastBusMask = 0; // v0.46 : coupures / solos des bus (changés par le contrôleur MIDI)
    std::uint32_t touchedRead = 0; // v0.32.1 : réglages écrits par le contrôleur MIDI ou une macro
    double perfAt = 0.0, memMb = 0.0, sampleMb = 0.0; // témoin de charge (v0.23), relu chaque seconde

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveboxEditor)
};

} // namespace grv
