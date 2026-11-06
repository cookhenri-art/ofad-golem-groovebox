/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Interfaces des modules : instruments et effets.

    Règles temps réel (fil audio) :
      - render() / process() n'allouent pas, ne verrouillent pas, n'appellent aucune API système ;
      - les réglages arrivent déjà convertis en unités réelles dans le tableau p[]
        (p[i] = valeur du i-ème réglage de la description du module) ;
      - prepare() est appelé hors du fil audio, avant que le module ne soit publié.
*/

#pragma once

#include "Params.h"

#include <cstdint>
#include <memory>
#include <array>
#include <vector>

namespace grv::engine
{

/** Échantillon audio mono possédé par le moteur ; les modules n'en gardent qu'un pointeur. */
struct SampleBuffer
{
    std::vector<float> data;            // canal gauche (ou mono)
    std::vector<float> right;           // canal droit (vide si mono)
    double sampleRate = 44100.0;
    std::string name;
    std::shared_ptr<const std::vector<std::uint8_t>> original; // fichier d'origine (sauvegarde du projet, historique)
    std::vector<float> onsets;          // attaques détectées au chargement (fractions, 64 au plus)
    // v0.49 : firmware du lecteur FM-1 (pas de son : data vide) et sa machine émulée, créée au chargement
    // hors du fil audio et détruite avec l'échantillon (corbeille du moteur)
    bool firmware = false;
    std::shared_ptr<void> runtime;
};

class Effect;
class Instrument;
struct SpeechScript; // Speech.h : phrase tapée de la voix interne (v0.33)

class Instrument
{
public:
    virtual ~Instrument() = default;
    virtual const ModuleDesc& desc() const noexcept = 0;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() noexcept = 0;

    /** note = numéro MIDI (instruments mélodiques) ou numéro de pad 0..15 (batteries). */
    virtual void noteOn (int note, float velocity) noexcept = 0;
    virtual void noteOff (int note) noexcept = 0;
    virtual void allNotesOff() noexcept {}
    /** Le séquenceur s'arrête (fil audio) : les sons qui tiendraient sans fin (boucles à tenue
        infinie, boucles Pickup) s'arrêtent comme sur la machine, avec leur relâchement. */
    virtual void transportStopped() noexcept {}
    /** Le séquenceur repart (fil audio). */
    virtual void transportStarted() noexcept {}
    /** Force de l'accent (0..1) de la prochaine note, donnée par le séquenceur juste avant
        noteOn() puis remise à 0. Les synthés l'ignorent (leur accent passe par la vélocité). */
    virtual void setNextAccent (float /*amount*/) noexcept {}

    /** Ajoute le rendu stéréo dans left/right. */
    virtual void render (float* left, float* right, int numSamples, const float* p) noexcept = 0;

    /** Échantillons (batteries : un par pad ; sampler : index 0). Fil audio. */
    virtual void setSample (int /*index*/, const SampleBuffer* /*sample*/) noexcept {}

    /** Tempo en cours (instruments synchronisés), appelé à chaque bloc. */
    virtual void setTempo (double /*bpm*/) noexcept {}
    /** v0.33 : position du séquenceur au début du bloc (noires) et lecture en cours. */
    virtual void setPosition (double /*ppq*/, bool /*playing*/) noexcept {}
    /** v0.33 : phrase tapée de la piste (voix interne des vocodeurs), nullptr sans phrase. */
    virtual void setSpeech (const SpeechScript* /*script*/) noexcept {}

    /** Commandes d'expression de la piste, appelées à chaque bloc : pitch bend -1..1, molette
        de modulation 0..1, pression (aftertouch de canal) 0..1. */
    virtual void setExpression (float /*bend*/, float /*wheel*/, float /*pressure*/) noexcept {}

    /** v0.49 : commande MIDI (CC) d'un contrôleur que GOLEM n'a pas apprise (n° 0..127, valeur 0..1). Fil audio. */
    virtual void controller (int /*cc*/, float /*value*/) noexcept {}

    /** Entrée audio (piste « Entrée audio »). */
    virtual void setInput (const float* /*left*/, const float* /*right*/) noexcept {}

    /** Envois par pad (batteries) : le moteur fournit, avant chaque render(), deux bus stéréo
        (retours A et B) où l'instrument ajoute ses envois. Pointeurs nuls : pas d'envoi. */
    virtual bool hasPadSends() const noexcept { return false; }
    virtual void setPadSendBuses (float* /*aL*/, float* /*aR*/, float* /*bL*/, float* /*bR*/) noexcept {}

    /** Enregistreur de la piste (sampler de performance) : 0 = départ / arrêt de
        l'enregistrement (ou cycle de la boucle), 1 = arrêter, 2 = effacer, 3 = (re)partir
        (pas d'enregistrement du séquenceur). Fil audio. */
    virtual void recordAction (int /*action*/) noexcept {}

    /** Sortie de la piste précédente pour ce bloc (après ses effets, avant son volume et sa coupure ; v0.25). */
    virtual void setNeighbor (const float* /*left*/, const float* /*right*/) noexcept {}
    /** Vrai si l'instrument (réglages p de ce bloc) prend la piste précédente à son compte : elle
        n'est plus envoyée au mixage. */
    virtual bool absorbsNeighbor (const float* /*p*/) const noexcept { return false; }
    /** Inserts par pad (batteries) : effets du graphe (nullptr = libre), leurs réglages (bloc de
        64 emplacements par insert, pad × 2 + n) et interrupteurs (pad × 2 + n, > 0,5 = contourné).
        Appelé avant chaque render. */
    virtual void setPadInserts (const std::array<std::array<Effect*, 2>, 16>* /*fx*/, const float* /*params*/, const float* /*bypass*/, double /*bpm*/) noexcept {}
    /** v0.24 : sources de side-chain (sorties des pistes) transmises aux inserts par pad. */
    virtual void setSidechainSources (const float* const* /*left*/, const float* const* /*right*/) noexcept {}
    /** Mixage principal du bloc qui vient d'être rendu (rééchantillonnage). */
    virtual bool wantsMainMix() const noexcept { return false; }
    virtual void captureMain (const float* /*left*/, const float* /*right*/, int /*numSamples*/) noexcept {}
    /** Pistes à recevoir dans captureMain à la place du mixage principal (masque de bits), -1 = mixage. */
    virtual int capturedTracks() const noexcept { return -1; }

    /** v0.42 : instrument à pistes internes (Pieuvre 8, lignes 0 à 7 du séquenceur) — ligne jouée par
        le clavier et le MIDI (notes 16 et plus), et lignes coupées (coupure, solo des lignes) : le son
        d'une ligne coupée se tait aussi. Fil audio, avant chaque render. */
    virtual void setPlayLane (int /*lane*/) noexcept {}
    /** v0.43 : note jouée au clavier (16 à 127) → ligne qui la reçoit et note à écrire dans le pas
        (zones du clavier de Pieuvre 8). Faux : pas de correspondance propre. Fil audio ou interface. */
    virtual bool mapNote (int /*note*/, int& /*lane*/, int& /*stepNote*/) const noexcept { return false; }
    virtual void setLaneMutes (std::uint32_t /*mask*/) noexcept {}
    /** v0.43 : écouter une partie (fractions a..b) d'un échantillon chargé (fil de l'interface : valeurs
        atomiques lues par le fil audio). index < 0 : arrêter l'écoute. */
    virtual void preview (int /*index*/, float /*a*/, float /*b*/) noexcept {}
    /** v0.47 : geste sur un écran de l'instrument (pavé de la physique : tenir, lâcher, lancer) ;
        fil de l'interface, valeurs atomiques lues par le fil audio. */
    virtual void touch (int /*kind*/, float /*a*/, float /*b*/) noexcept {}
    /** v0.42 : copie du tampon de l'enregistreur d'une piste interne (fil de l'interface). */
    virtual bool copyRecording (int /*lane*/, std::vector<float>& /*l*/, std::vector<float>& /*r*/, double& /*rate*/) const { return false; }

    /** v0.49 : l'instrument lit des firmwares (FM Player) ; les autres ne reçoivent jamais un firmware
        à la place d'un échantillon (ni l'inverse). */
    virtual bool acceptsFirmware() const noexcept { return false; }
    /** v0.49 : retard propre de l'instrument (échantillons) : le moteur retarde d'autant les autres pistes
        pour que tout tombe ensemble (FM Player : avance de l'émulation). Fil audio. */
    virtual int latencySamples() const noexcept { return 0; }

    /** État à montrer dans l'interface (fil de l'interface, lit des valeurs atomiques) ;
        renvoie le nombre de valeurs écrites. */
    virtual int status (float* /*out*/, int /*max*/) const noexcept { return 0; }

    /** Compteur d'activité par pad ou note, pour allumer l'interface. */
    virtual bool isActive() const noexcept { return true; }
};

class Effect
{
public:
    virtual ~Effect() = default;
    virtual const ModuleDesc& desc() const noexcept = 0;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() noexcept = 0;

    /** Traite le bloc sur place. bpm sert aux effets synchronisés. */
    virtual void process (float* left, float* right, int numSamples, const float* p, double bpm) noexcept = 0;

    /** Position de lecture en noires (effets calés sur la mesure), appelée avant process(). */
    virtual void setPosition (double /*ppq*/, bool /*playing*/) noexcept {}

    /** Side-chain (v0.23), appelé avant process() : sortie de chaque piste (après ses inserts,
        avant son volume et sa coupure ; numTracks tampons de numSamples). Un effet qui a un
        réglage « Side-chain » y lit la piste choisie au lieu de son entrée. */
    virtual void setSidechainSources (const float* const* /*left*/, const float* const* /*right*/) noexcept {}

    /** Mesures pour l'interface (réduction de gain, niveaux…). */
    virtual float meter (int /*index*/) const noexcept { return 0.0f; }

    /** Analyseur : copie les derniers échantillons de sortie (mono) ; renvoie leur nombre (0 : pas d'analyseur). */
    virtual int analysis (float* /*dst*/, int /*count*/) const noexcept { return 0; }

    /** Latence ajoutée, en échantillons (0 pour presque tous les effets). */
    virtual int latency() const noexcept { return 0; }

    /** Traîne (v0.35) : vrai si, contourné, l'effet doit encore faire sonner sa queue. */
    virtual bool trails (const float* /*p*/) const noexcept { return false; }
    /** Contourné avec traîne : le direct passe entier, la queue déjà engagée s'y ajoute (aucune entrée nouvelle). */
    virtual void processTrails (float* /*left*/, float* /*right*/, int /*numSamples*/, const float* /*p*/, double /*bpm*/) noexcept {}
};

} // namespace grv::engine
