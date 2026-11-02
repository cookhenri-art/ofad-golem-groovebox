/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Sampler de performance » — principe des samplers de performance à 8 pistes (architecture
    reprise, ni nom, ni panneau, ni son d'origine). Chaque piste qui le prend choisit sa MACHINE :
      - Flex : un échantillon en mémoire, ou le tampon de l'enregistreur de la piste ;
        (Static retirée en v0.15 : sur ordinateur, tout est en mémoire, elle jouait comme Flex) ;
      - Thru : l'entrée audio passe dans la piste, ouverte par les pas (enveloppe d'ampli) ;
      - Neighbor : la sortie de la piste précédente passe dans celle-ci (elle quitte le mixage) :
        on enchaîne ainsi plus de 4 effets ;
      - Pickup : boucle enregistrée en direct (enregistrer, fermer la boucle, surcouches),
        longueur calée sur la mesure.
    Lecture (pages Lecture et Réglages de lecture) : hauteur, début, longueur, vitesse (négative =
    à l'envers), redéclenchements (nombre, écart), étirement Non / Normal / Beat calé sur le tempo
    de l'échantillon, boucle, DÉCOUPES (grille de 2 à 64 ou attaques détectées) jouées au clavier
    ou choisies par « Début », gain. Ampli : attaque, tenue (durée, note, infinie), relâchement,
    volume, balance. Trois LFO à pas dessinables et leurs destinations (matrice), un enregistreur par
    piste (entrée, piste précédente ou mixage principal, longueur en pas, pas d'enregistrement du
    séquenceur). L'emplacement d'échantillon (16 + l'enregistreur) se verrouille par pas comme tout
    réglage : c'est le verrou d'échantillon. Les deux effets de la piste sont ses inserts 1 et 2.

    v0.40 (demande d'Henrico : « fidèle aux samplers de performance ») :
      - réserve commune : chaque piste Pieuvre (8 au plus) joue les 16 emplacements et
        l'enregistreur de toutes les autres pistes Pieuvre (128 emplacements, 8 enregistreurs) ;
      - réglages propres à chaque emplacement (« attributs ») : tempo en mesures, début et fin
        (rognage), point de boucle, fondus d'entrée et de sortie, fondu de boucle, gain, note du
        clavier « Emplacements » ; ils suivent l'emplacement, quelle que soit la piste qui le joue ;
      - nombre de tours de boucle, puis lecture jusqu'à la fin.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "../synth/Stretcher.h"
#include "../dsp/AnalogFilter.h"

#include <atomic>

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace octa
{
    /** Réglages d'une piste interne (T1 à T8), dans cet ordre ; les LFO et la matrice suivent (unitDesc). */
    enum Slot
    {
        machine = 0, slot,
        tune, fine, start, length, rate, retrig, retrigTime,
        stretch, loop, slices, keyMode, bars, gain,
        attack, hold, holdMode, release, volume, balance, velAmp,
        recSource, recLength, thruGain,
        pickGain, pickDir, pickLen, pickKeep,
        fType, fCut, fReso, fEnv, fAtk, fDec,   // filtre multimode à enveloppe (FX1 d'origine de l'Octatrack)
        drive, bits, srr,                       // Lo-fi : saturation, réduction de bits et d'échantillonnage
        recTrack1,                              // 16 interrupteurs : pistes du mixeur enregistrées (source « Pistes choisies »)
        loops = recTrack1 + 16,                 // v0.40 : tours de boucle
        startLive,                              // v0.41 : début suivi pendant la lecture (LFO sur Début)
        zoneLo, zoneHi,                         // v0.43 : zone du clavier (mode « Zones ») : notes basse et haute
        unitOwn
    };
    // attributs d'un emplacement (décalage dans son bloc de 9)
    enum Attr { aBars = 0, aStart, aEnd, aLoop, aFadeIn, aFadeOut, aXfade, aGain, aNote, attrCount };
    constexpr int attrStride = attrCount;
    // v0.42 : 8 pistes internes, réserve de 128 emplacements (16 par piste) partagée par les 8
    constexpr int numUnits = 8, bankSlots = 16, poolSize = numUnits * bankSlots;
    // choix d'emplacement d'une piste interne : son enregistreur, ses 16 emplacements, la réserve
    // (8 × 16, toutes les pistes internes) et les 8 enregistreurs
    constexpr int ownSlots = 16, poolBase = 17, poolSlots = poolSize, recBase = poolBase + poolSlots, numSlotChoices = recBase + 8;
    // Thru et Neighbor retirés en v0.12.7, Static en v0.15 (identique à Flex) : les numéros internes restent
    enum Machine { flex = 0, staticM, thru, neighbor, pickup };
    /** Machine choisie : Flex ou Pickup (rangées 0, 1 dans le réglage). */
    inline int machineOf (float v) noexcept { return std::clamp ((int) v, 0, 1) == 1 ? pickup : flex; }

    inline const std::vector<std::string>& retrigTimes()
    {
        static const std::vector<std::string> v { "1/32", "1/16", "1/8", "1/4", "1/2", "1 mes." };
        return v;
    }
    inline double retrigQuarters (int i) noexcept
    {
        static constexpr double q[] { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0 };
        return q[std::clamp (i, 0, 5)];
    }

    /** Une piste interne : ses réglages (identifiants sans préfixe), LFO et matrice compris. */
    inline const ModuleDesc& unitDesc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "octa_t";
            m.name = "Pieuvre 8 \xC2\xB7 piste";
            m.family = "Sampler";
            std::vector<std::string> slots { "Enregistreur" };
            for (int i = 1; i <= 16; ++i) slots.push_back ("\xC3\x89" "chantillon " + std::to_string (i));
            for (int t = 1; t <= 8; ++t) for (int i = 1; i <= 16; ++i) slots.push_back ("T" + std::to_string (t) + " \xC2\xB7 " + std::to_string (i));
            for (int t = 1; t <= 8; ++t) slots.push_back ("Enreg. T" + std::to_string (t));
            m.params = {
                C ("machine", "Machine", { "Flex", "Pickup" }),
                C ("slot", "Emplacement", slots, 1),

                P ("tune", "Hauteur", "dt", -24, 24, 0),
                P ("fine", "Fin", "ct", -50, 50, 0),
                P ("start", "D\xC3\xA9" "but", "%", 0, 100, 0),
                P ("length", "Longueur", "%", 0, 100, 100),
                P ("rate", "Vitesse", "%", -200, 200, 100),
                I ("retrig", "Redécl.", "", 1, 16, 1),
                C ("rtim", "\xC3\x89" "cart redécl.", retrigTimes(), 1),

                C ("stretch", "\xC3\x89tirement", { "Non", "Normal", "Beat" }),
                C ("loop", "Boucle", { "Non", "Oui", "Aller-retour" }),
                C ("slices", "D\xC3\xA9" "coupes", { "Non", "2", "4", "8", "16", "32", "64", "Attaques" }),
                C ("keymode", "Clavier", { "Chromatique", "D\xC3\xA9" "coupes", "Emplacements" }),
                C ("bars", "Mesures", { "Auto", "1/4", "1/2", "1", "2", "4", "8", "16" }),
                P ("gain", "Gain", "dB", -24, 24, 0),

                P ("attack", "Attaque", "ms", 0, 2000, 0.5f, 0.3f),
                P ("hold", "Tenue", "ms", 0, 5000, 200, 0.3f),
                // v0.43 : nom explicite. Durée réglée (Tenue en ms) / tant qu'on appuie / jusqu'au bout (un appui suffit)
                C ("holdmode", "D\xC3\xA9" "clenchement", { "Dur\xC3\xA9" "e r\xC3\xA9" "gl\xC3\xA9" "e", "Tant qu'appuy\xC3\xA9", "Jusqu'au bout" }, 2),
                P ("release", "Rel\xC3\xA2" "chement", "ms", 2, 8000, 40, 0.3f),
                P ("volume", "Volume", "dB", -48, 12, 0),
                P ("balance", "Balance", "pan", -100, 100, 0),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 50),

                C ("recsrc", "Source", { "Entr\xC3\xA9" "e", "Pistes choisies", "Mixage principal" }),
                C ("reclen", "Dur\xC3\xA9" "e", { "1 pas", "2 pas", "4 pas", "8 pas", "16 pas", "32 pas", "64 pas", "Maximum" }, 4),
                P ("thrugain", "Niveau d'entr\xC3\xA9" "e", "dB", -24, 12, 0),

                P ("pickgain", "Niveau ajout\xC3\xA9", "dB", -24, 6, 0),
                C ("pickdir", "Sens", { "Avant", "Arri\xC3\xA8re", "Aller-retour" }),
                C ("picklen", "Boucle cal\xC3\xA9" "e", { "Libre", "1 mes.", "2 mes.", "4 mes.", "8 mes." }, 1),
                P ("pickkeep", "Garde aux surcouches", "%", 0, 100, 100),

                C ("ftype", "Type de filtre", { "Arr\xC3\xAAt", "Passe-bas 24 dB", "Passe-bas 12 dB", "Passe-bande",
                                                "Passe-haut 12 dB", "Passe-haut 24 dB", "R\xC3\xA9jection" }),
                P ("fcut", "Coupure", "Hz", 20, 20000, 4000, 0.25f),
                P ("freso", "R\xC3\xA9sonance", "%", 0, 100, 15),
                P ("fenv", "Enveloppe filtre", "%", -100, 100, 0),
                P ("fatk", "Attaque filtre", "ms", 0, 2000, 0, 0.3f),
                P ("fdec", "D\xC3\xA9" "clin filtre", "ms", 5, 8000, 300, 0.3f),

                P ("drive", "Saturation", "%", 0, 100, 0),
                I ("bits", "Bits", "", 4, 16, 16),
                I ("srr", "R\xC3\xA9" "duction d'\xC3\xA9" "chantillonnage", "x", 1, 32, 1),
            };
            // pistes du mixeur enregistrées quand la source est « Pistes choisies » (la piste elle-même est toujours exclue)
            for (int t = 1; t <= 16; ++t)
                m.params.push_back (C ("rt" + std::to_string (t), "Piste " + std::to_string (t), { "Non", "Oui" }, 1));
            {
                std::vector<std::string> tours { "Sans fin" };
                for (int i = 1; i <= 16; ++i) tours.push_back (std::to_string (i) + (i == 1 ? " tour" : " tours"));
                m.params.push_back (C ("loops", "Tours de boucle", tours));
            }
            // v0.41 : « Au départ » (principe d'origine : Début lu à chaque déclenchement) ou « En continu » :
            // Début modulé (LFO, modulations) fait glisser la fenêtre de lecture pendant qu'elle joue
            m.params.push_back (C ("startlive", "D\xC3\xA9" "but suivi", { "Au d\xC3\xA9part", "En continu" }));
            // v0.43 : zone du clavier de la piste (mode « Zones » : chaque piste interne a ses notes, la note
            // basse joue l'échantillon à sa hauteur d'origine) ; gardée par les préréglages
            m.params.push_back (I ("zlo", "Zone : note basse", "", 0, 127, 36));
            m.params.push_back (I ("zhi", "Zone : note haute", "", 0, 127, 47));
            m.params[m.params.size() - 1].keepOnPreset = m.params[m.params.size() - 2].keepOnPreset = true;
            m.presets = {
                { "Flex : un coup", {} },
                { "Boucle au tempo", { { "stretch", 1 }, { "loop", 1 }, { "holdmode", 2 } } },
                { "Boucle de batterie d\xC3\xA9" "coup\xC3\xA9" "e", { { "stretch", 2 }, { "slices", 4 }, { "keymode", 1 }, { "holdmode", 1 }, { "release", 20 } } },
                { "Pickup : boucle d'une mesure", { { "machine", 1 }, { "picklen", 1 } } },
                { "Pickup : boucle de la groovebox", { { "machine", 1 }, { "recsrc", 1 }, { "picklen", 2 } } },
                { "Glitch \xC3\xA0 l'envers", { { "rate", -100 }, { "retrig", 4 }, { "rtim", 0 }, { "holdmode", 0 }, { "hold", 120 } } },
                { "Boucle filtr\xC3\xA9" "e qui s'ouvre", { { "stretch", 1 }, { "loop", 1 }, { "holdmode", 2 }, { "ftype", 1 }, { "fcut", 300 }, { "freso", 45 }, { "fenv", 60 }, { "fatk", 400 }, { "fdec", 2500 } } },
                { "Lo-fi 12 bits", { { "stretch", 1 }, { "loop", 1 }, { "holdmode", 2 }, { "bits", 12 }, { "srr", 3 }, { "drive", 20 }, { "ftype", 2 }, { "fcut", 9000 } } },
                { "D\xC3\xA9" "coupes grill\xC3\xA9" "es", { { "stretch", 2 }, { "slices", 4 }, { "keymode", 1 }, { "holdmode", 1 }, { "release", 20 }, { "bits", 6 }, { "srr", 8 }, { "drive", 45 }, { "ftype", 4 }, { "fcut", 250 } } },
                { "D\xC3\xA9" "but frott\xC3\xA9 au LFO", { { "stretch", 0 }, { "loop", 1 }, { "holdmode", 2 }, { "startlive", 1 }, { "lf1_rate", 0.5f }, { "mx1_dst@start", 0 }, { "mx1_amt", 20 } } },
            };
            HubSpec hub;
            hub.envs = 0; hub.lfos = 3; hub.firstLfo = 1; hub.slots = 6; hub.arp = false;
            addModHub (m, hub);
            // les trois premiers routages partent des trois LFO (une destination et une profondeur par LFO)
            for (int k = 1; k <= 3; ++k)
                if (const auto i = m.indexOf ("mx" + std::to_string (k) + "_src"); i >= 0)
                {
                    const auto& ch = m.params[(size_t) i].choices;
                    for (size_t c = 0; c < ch.size(); ++c) if (ch[c] == "LFO " + std::to_string (k)) m.params[(size_t) i].def = (float) c;
                }
            resolveHubPresetDests (m);
            return m;
        }();
        return d;
    }
    /** Nombre de réglages d'une piste interne (bloc de chaque piste dans les réglages de l'instrument). */
    inline int unitStride() noexcept { static const int n = (int) unitDesc().params.size(); return n; }
    inline int unitParam (int unit, int index) noexcept { return unit * unitStride() + index; }
    /** Attributs des 128 emplacements, après les 8 blocs de piste. */
    inline int attrBase() noexcept { return numUnits * unitStride(); }
    inline int attrSlot (int slot, int a) noexcept { return attrBase() + slot * attrStride + a; }
    inline std::string unitPrefix (int unit) { return "t" + std::to_string (unit + 1) + "_"; }
    /** v0.43 : réglage commun après les attributs — clavier : piste choisie ou zones. */
    inline int kbModeIndex() noexcept { return attrBase() + poolSize * attrStride; }

    /** L'instrument : 8 pistes internes (préfixes t1_ à t8_) puis les attributs des 128 emplacements
        (e1 à e128 : e1 à e16 = emplacements de T1, e17 à e32 = ceux de T2…). */
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "octa";
            m.name = "Pieuvre 8";
            m.family = "Sampler";
            m.principle = "8 pistes internes (machines Flex et Pickup), r\xC3\xA9serve de 128 emplacements, d\xC3\xA9" "coupes, \xC3\xA9tirement, enregistreurs, LFO \xC3\xA0 pas, 2 effets par piste";
            m.drumKit = true; // 8 lignes de séquence (une par piste interne), comme les pads d'une batterie
            m.laneNotes = true; // chaque pas joue aussi sa note (clavier enregistré, anciens projets)
            for (int k = 1; k <= numUnits; ++k) { m.padNames.push_back ("T" + std::to_string (k)); m.padKinds.push_back ("octa"); }
            const auto& u = unitDesc();
            for (int k = 0; k < numUnits; ++k)
                for (const auto& p : u.params)
                {
                    auto q = p;
                    q.id = unitPrefix (k) + p.id;
                    // zones du clavier d'origine : une octave par piste, T1 de Do1 à Si1, T2 de Do2 à Si2…
                    if (p.id == "zlo") q.def = (float) std::min (127, 24 + 12 * k);
                    if (p.id == "zhi") q.def = (float) std::min (127, 35 + 12 * k);
                    q.name = "T" + std::to_string (k + 1) + " \xC2\xB7 " + p.name;
                    m.params.push_back (std::move (q));
                }
            for (int k = 1; k <= poolSize; ++k)
            {
                const auto e = "e" + std::to_string (k);
                const auto n = "T" + std::to_string ((k - 1) / 16 + 1) + " \xC2\xB7 " + std::to_string ((k - 1) % 16 + 1) + " \xC2\xB7 ";
                m.params.push_back (C (e + "bars", n + "tempo", { "Auto", "1/4 mes.", "1/2 mes.", "1 mes.", "2 mes.", "4 mes.", "8 mes.", "16 mes." }));
                m.params.push_back (P (e + "st", n + "d\xC3\xA9" "but", "%", 0, 100, 0));
                m.params.push_back (P (e + "en", n + "fin", "%", 0, 100, 100));
                m.params.push_back (P (e + "lp", n + "point de boucle", "%", 0, 100, 0));
                m.params.push_back (P (e + "fi", n + "fondu d'entr\xC3\xA9" "e", "ms", 0, 4000, 0, 0.35f));
                m.params.push_back (P (e + "fo", n + "fondu de sortie", "ms", 0, 4000, 0, 0.35f));
                m.params.push_back (P (e + "xf", n + "fondu de boucle", "ms", 0, 500, 5, 0.4f));
                m.params.push_back (P (e + "gain", n + "gain", "dB", -24, 24, 0));
                m.params.push_back (I (e + "note", n + "note", "", 0, 127, 59 + ((k - 1) % 16) + 1)); // Do3 = emplacement 1 de la piste
            }
            // v0.43 : le clavier (et le MIDI) joue la piste interne choisie, ou chaque piste dans sa zone
            m.params.push_back (C ("kbmode", "Clavier", { "Piste choisie", "Zones" }));
            // sections : celles de T1 (les écrans génériques) ; la façade a ses propres pages par piste
            m.sections = {
                { "src", "Lecture", "sample", { "t1_tune", "t1_start", "t1_length", "t1_rate", "t1_retrig", "t1_rtim", "t1_fine", "t1_slot" }, "osc" },
                { "setup", "R\xC3\xA9glages de lecture", "osc", { "t1_stretch", "t1_loop", "t1_slices", "t1_keymode", "t1_bars", "t1_gain", "t1_machine" }, "osc" },
                { "amp", "Ampli", "env", { "t1_attack", "t1_hold", "t1_holdmode", "t1_release", "t1_volume", "t1_balance", "t1_velamp" }, "amp" },
                { "filter", "Filtre", "filter", { "t1_ftype", "t1_fcut", "t1_freso", "t1_fenv", "t1_fatk", "t1_fdec" }, "filter" },
                { "lofi", "Lo-fi", "global", { "t1_drive", "t1_bits", "t1_srr" }, "global" },
            };
            // préréglages : ceux d'une piste interne ; le moteur les applique à la piste interne jouée
            // (Engine::applyPreset) ; ici ils visent T1 (contrôles, outils hors moteur)
            m.presets = u.presets;
            for (auto& pr : m.presets) for (auto& v : pr.values) v.first = unitPrefix (0) + v.first;
            return m;
        }();
        return d;
    }
}

class OctaUnit;

/** v0.42 : ce que les 8 pistes internes partagent — la réserve de 128 emplacements, leurs attributs
    (réglages de l'instrument, valeurs jouées du bloc) et les pistes elles-mêmes (enregistreurs). */
struct OctaShared
{
    std::array<const SampleBuffer*, octa::poolSize> pool {};
    std::array<const OctaUnit*, octa::numUnits> units {};
    const float* attrs = nullptr; // attributs de l'emplacement 0 (puis 9 par emplacement)
};

/** Une piste interne de Pieuvre 8 (T1 à T8) : machine, têtes de lecture, enregistreur, boucle Pickup,
    filtre, lo-fi et LFO. Ses réglages : le bloc de la piste (octa::Slot, puis LFO et matrice). */
class OctaUnit
{
public:
    static constexpr double maxRecordSeconds = 16.0;

    void prepare (double s, int unitIndex, OctaShared* shared)
    {
        (void) lfoModeIdx(); // hors du fil audio
        sr = s;
        bank = unitIndex;
        sh = shared;
        for (auto& h : heads) h.st.prepare (s);
        for (auto& f : filters) f.prepare (s);
        hub.bind (octa::unitDesc());
        hub.prepare (s);
        // tampon de l'enregistreur (et de la boucle Pickup), alloué hors du fil audio
        const auto frames = (size_t) (maxRecordSeconds * s);
        recL.assign (frames, 0.0f);
        recR.assign (frames, 0.0f);
        reset();
    }

    void reset() noexcept
    {
        for (auto& h : heads) { h.st.stop(); h.env = 0.0; h.stage = idle; }
        for (auto& f : filters) f.reset();
        fenv = 0.0; fenvStage = 0; holdCount = 0; holdL = holdR = 0.0; cutSmooth = -1.0;
        driveL.reset(); driveR.reset(); pickAmp = 0.0; pickupPaused = false;
        hub.reset();
        recState.store (0); recLen.store (0); recPos = 0; recTarget = 0;
        loopState.store (0); loopLen = 0; loopPos = 0.0; loopDir = 1.0;
        pendingRetrigs = 0;
    }

    /** Un emplacement de la réserve change : une tête qui lisait l'ancien tampon s'arrête net
        (le moteur va le libérer). */
    void dropSource (const SampleBuffer* s) noexcept
    {
        if (s == nullptr) return;
        for (auto& h : heads) if (h.source == s) { h.st.stop(); h.stage = idle; h.source = nullptr; }
    }
    void setTempo (double b) noexcept { bpm = b; }
    void setInput (const float* l, const float* r) noexcept { inL = l; inR = r; }
    /** Une tête qui lit l'enregistreur d'une autre piste interne : arrêtée quand il est effacé. */
    bool recorderView (const float*& l, const float*& r, int& length, double& rate) const noexcept
    {
        const auto len = recLen.load (std::memory_order_relaxed);
        if (len < 16) return false;
        l = recL.data(); r = recR.data(); length = len; rate = sr;
        return true;
    }
    void setExpression (float b, float w, float pr) noexcept { hub.setExpression (b, w, pr); }
    bool wantsMainMix() const noexcept { return recSourceNow >= 1 && (recState.load (std::memory_order_relaxed) == 1 || machineNow.load (std::memory_order_relaxed) == octa::pickup); }
    /** Source « Pistes choisies » : masque des pistes (le moteur les additionne) ; -1 = mixage principal. */
    int capturedTracks() const noexcept { return recSourceNow == 1 ? trackMask : -1; }
    int recSource() const noexcept { return recSourceNow; }

    // Les notes sont jouées au début du bloc suivant : les verrous du pas (emplacement, début,
    // découpes…) sont alors en place — un verrou d'échantillon vaut pour son propre pas.
    void noteOn (int note, float velocity) noexcept { queue (note, velocity, true); }
    void noteOff (int note) noexcept { queue (note, 0.0f, false); }

    void allNotesOff() noexcept
    {
        numQueued = 0;
        pendingRetrigs = 0;
        for (auto& h : heads) if (h.stage != idle) { h.stage = releasing; h.fast = true; }
        hub.release (0);
    }

    void recordAction (int action) noexcept { pendingAction = action; }

    /** Arrêt du séquenceur (comme la touche STOP de la machine) : les têtes Flex finissent sur
        leur relâchement (déclenchement « Jusqu'au bout » et boucles comprises), la boucle Pickup se tait ;
        elle repart du début avec le séquenceur. Une prise Pickup en cours est refermée. */
    void transportStopped() noexcept
    {
        numQueued = 0;
        pendingRetrigs = 0;
        for (auto& h : heads) if (h.stage != idle) h.stage = releasing;
        const auto s = loopState.load();
        if (s == 1 && params != nullptr) closeLoop (params);
        else if (s == 3) loopState.store (2);
        if (loopState.load() >= 2) pickupPaused = true;
    }
    void transportStarted() noexcept
    {
        if (! pickupPaused) return;
        pickupPaused = false;
        loopDir = 1.0;
        loopPos = loopDirForward() ? 0.0 : (double) std::max (0, loopLen - 1);
    }

    /** Calcul d'un bloc, ajouté dans L / R (p : réglages joués de la piste interne). */
    void render (float* L, float* R, int n, const float* p) noexcept
    {
        params = p;
        machineNow.store (octa::machineOf (p[octa::machine]), std::memory_order_relaxed);
        recSourceNow = std::clamp ((int) p[octa::recSource], 0, 2);
        trackMask = 0;
        for (int t = 0; t < 16; ++t) if (p[octa::recTrack1 + t] > 0.5f) trackMask |= 1 << t;
        if (pendingAction >= 0) { const auto a = pendingAction; pendingAction = -1; if (! doAction (a, p)) pendingAction = a; }
        for (int q = 0; q < numQueued; ++q)
        {
            const auto& e = queued[(size_t) q];
            if (e.on) start (e.note, e.velocity, p);
            else stopNote (e.note, p);
        }
        numQueued = 0;

        const auto* common = hub.beginBlock (p, n, bpm);
        const auto* vp = hub.voiceParams (0, common);
        const auto lvl = dbToGain (vp[octa::volume]);
        const auto pan = vp[octa::balance] / 100.0;
        const auto gl = lvl * std::min (1.0, 1.0 - pan), gr = lvl * std::min (1.0, 1.0 + pan);
        const auto atk = 1.0 - std::exp (-1.0 / std::max (1.0, vp[octa::attack] * 0.001 * sr));
        const auto rel = std::exp (std::log (0.001) / std::max (1.0, vp[octa::release] * 0.001 * sr));
        const auto fastRel = std::exp (std::log (0.001) / (0.004 * sr));
        const auto holdMode = (int) vp[octa::holdMode];
        const auto holdSamples = (int) (vp[octa::hold] * 0.001 * sr);
        const auto inGain = dbToGain (vp[octa::thruGain]);
        const auto mach = machineNow.load (std::memory_order_relaxed);
        const bool monitorInput = recSourceNow == 0; // les pistes et le mixage enregistrés ne sont pas réécoutés (larsen)

        // v0.41 : Début suivi — l'écart entre Début joué (LFO, modulations) et celui de la tête fait glisser
        // sa région, réparti sur les images du bloc (pas de saut, donc pas de clic)
        const bool startLive = vp[octa::startLive] > 0.5f;
        std::array<double, 2> slide {};
        if (startLive && n > 0)
            for (size_t k = 0; k < heads.size(); ++k)
                if (auto& h = heads[k]; h.stage != idle && h.framesPerPct > 0.0)
                {
                    const auto d = (vp[octa::start] - h.startPct) * h.framesPerPct;
                    if (h.age < 1.0) h.st.shift (d); // tête qui n'a encore rien joué : elle part directement de là
                    else slide[k] = d / (double) n;
                    h.startPct = vp[octa::start];
                }

        // vitesse de la position et hauteur des grains
        const auto rateF = vp[octa::rate] / 100.0;
        const auto pitchSemis = vp[octa::tune] + vp[octa::fine] / 100.0 + noteOffset;
        const auto pitchF = fastExp2 (pitchSemis / 12.0);

        // filtre et lo-fi de la piste (après l'ampli, avant volume et balance)
        Post post;
        post.mode = std::clamp ((int) vp[octa::fType], 0, (int) dsp::AnalogFilter::numModes - 1);
        post.cutLog = std::log2 (std::clamp ((double) vp[octa::fCut], 20.0, 20000.0));
        post.reso = std::clamp (vp[octa::fReso] / 100.0, 0.0, 1.0);
        post.envOct = std::clamp (vp[octa::fEnv] / 100.0, -1.0, 1.0) * 7.0; // ± 7 octaves
        post.atk = 1.0 - std::exp (-1.0 / std::max (1.0, vp[octa::fAtk] * 0.001 * sr));
        post.dec = std::exp (std::log (0.001) / std::max (1.0, vp[octa::fDec] * 0.001 * sr));
        post.drive = std::clamp (vp[octa::drive] / 100.0, 0.0, 1.0);
        post.driveGain = 1.0 + post.drive * 15.0;
        post.driveNorm = 1.0 / std::sqrt (post.driveGain);
        const auto bitsN = std::clamp ((int) std::lround (vp[octa::bits]), 4, 16);
        post.quant = bitsN < 16 ? std::ldexp (1.0, bitsN - 1) : 0.0;
        post.hold = std::clamp ((int) std::lround (vp[octa::srr]), 1, 32);
        if (cutSmooth < 0.0) cutSmooth = post.cutLog;

        for (int i = 0; i < n; ++i)
        {
            // redéclenchements programmés
            if (pendingRetrigs > 0 && --retrigCountdown <= 0.0)
            {
                retrigCountdown += retrigEvery;
                --pendingRetrigs;
                trigger (retrigNote, retrigVel, vp);
            }

            // source de l'enregistreur : entrée, piste précédente, ou mixage principal du bloc précédent
            double srcL = 0.0, srcR = 0.0;
            if (recSourceNow == 0 && inL != nullptr) { srcL = inL[i] * inGain; srcR = (inR != nullptr ? inR[i] : inL[i]) * inGain; }
            else if (recSourceNow >= 1 && mach == octa::pickup && fifoRead != fifoWrite)
            {
                // mixage principal : file d'un bloc de contrôle de retard, sans trou entre les blocs
                srcL = fifoL[(size_t) fifoRead]; srcR = fifoR[(size_t) fifoRead];
                fifoRead = (fifoRead + 1) & (fifoSize - 1);
            }
            if (recSourceNow == 0) recordFrame (srcL, srcR);

            double l = 0.0, r = 0.0;
            if (mach == octa::pickup)
            {
                const auto g = dbToGain (vp[octa::pickGain]);
                loopFrame (srcL * g, srcR * g, vp, l, r, monitorInput);
                process (post, l, r);
                L[i] += (float) (l * gl);
                R[i] += (float) (r * gr);
                continue;
            }
            for (size_t k = 0; k < heads.size(); ++k)
            {
                auto& h = heads[k];
                if (h.stage == idle) continue;
                double hl = 0.0, hr = 0.0;
                {
                    if (! h.st.isActive()) { h.stage = idle; continue; }
                    if (slide[k] != 0.0) h.st.shift (slide[k]);
                    // vitesse relative au sens de départ (une vitesse négative au départ lit à l'envers)
                    const auto inc = h.fileStep * (h.stretched ? rateF * h.tempoRatio : rateF) * (h.reversed ? -1.0 : 1.0);
                    h.st.next (inc, pitchF, hl, hr);
                    auto g = h.gain;
                    // v0.40 : fondu d'entrée (depuis le départ) et de sortie (avant la fin de la lecture)
                    if (h.fadeIn > 1.0 && h.age < h.fadeIn) g *= h.age / h.fadeIn;
                    if (h.fadeOutFrames > 1.0) { const auto te = h.st.toEnd(); if (te < h.fadeOutFrames) g *= std::max (0.0, te / h.fadeOutFrames); }
                    h.age += 1.0;
                    hl *= g; hr *= g;
                }
                // enveloppe d'ampli : attaque, tenue, relâchement
                switch (h.stage)
                {
                    case attackS: h.env += (1.0 - h.env) * atk; if (h.env > 0.999) { h.env = 1.0; h.stage = holdS; h.held = 0; } break;
                    case holdS:
                        if (holdMode == 0 && ++h.held >= holdSamples) h.stage = releasing;
                        break;
                    case releasing: h.env *= h.fast ? fastRel : rel; if (h.env < 1.0e-4) { h.stage = idle; h.st.stop(); } break;
                    default: break;
                }
                l += hl * h.env * h.vel;
                r += hr * h.env * h.vel;
            }
            process (post, l, r);
            L[i] += (float) (l * gl);
            R[i] += (float) (r * gr);
        }
        if (mach != octa::pickup || recSourceNow == 0) fifoRead = fifoWrite = 0;
        playPos.store ((float) heads[0].st.position(), std::memory_order_relaxed);
        if (heads[0].stage == idle && heads[1].stage == idle) hub.release (0);
    }

    void captureMain (const float* l, const float* r, int n) noexcept
    {
        if (params == nullptr) return;
        if (machineNow.load (std::memory_order_relaxed) == octa::pickup)
        {
            // la boucle lit le mixage dans une petite file (un bloc de contrôle de retard)
            for (int i = 0; i < n; ++i)
            {
                const auto next = (fifoWrite + 1) & (fifoSize - 1);
                if (next == fifoRead) break; // file pleine : l'excédent est perdu
                fifoL[(size_t) fifoWrite] = l[i]; fifoR[(size_t) fifoWrite] = r[i];
                fifoWrite = next;
            }
            return;
        }
        for (int i = 0; i < n; ++i) recordFrame (l[i], r[i]);
    }

    int status (float* out, int max) const noexcept
    {
        if (max < 5) return 0;
        out[0] = (float) recState.load (std::memory_order_relaxed);               // 0 vide, 1 enregistre, 2 plein
        out[1] = (float) ((double) recLen.load (std::memory_order_relaxed) / std::max (1.0, sr)); // secondes enregistrées
        out[2] = playPos.load (std::memory_order_relaxed);                        // position de lecture (fraction)
        out[3] = (float) loopState.load (std::memory_order_relaxed);              // Pickup : 0 vide, 1 enr., 2 lecture, 3 surcouche
        out[4] = (float) machineNow.load (std::memory_order_relaxed);
        return 5;
    }

    /** Quelque chose à calculer (tête, enregistrement, boucle, note en attente) : sinon la piste est sautée. */
    /** Positions des 3 réglages « Mode » des LFO d'une piste interne (calculées une fois, à la préparation). */
    static const std::array<int, 3>& lfoModeIdx()
    {
        static const std::array<int, 3> r = []
        {
            std::array<int, 3> x {};
            for (int l = 0; l < 3; ++l) x[(size_t) l] = octa::unitDesc().indexOf ("lf" + std::to_string (l + 1) + "_mode");
            return x;
        }();
        return r;
    }

    /** v0.43.5 : piste silencieuse — les LFO libres (« Libre », « Tenu ») continuent de tourner,
        comme sur la machine : la phase n'attend pas le son suivant. */
    void idleTick (int n, const float* p) noexcept
    {
        bool freeLfo = false;
        for (const auto i : lfoModeIdx()) if (i >= 0) { const auto m = (int) p[i]; freeLfo = freeLfo || m == 2 || m == 4; }
        if (! freeLfo) return;
        const auto* common = hub.beginBlock (p, n, bpm);
        hub.voiceParams (0, common);
    }

    bool busy() const noexcept
    {
        return numQueued > 0 || pendingAction >= 0 || pendingRetrigs > 0 || isActive();
    }
    bool isActive() const noexcept
    {
        return heads[0].stage != idle || heads[1].stage != idle || recState.load (std::memory_order_relaxed) == 1
            || loopState.load (std::memory_order_relaxed) == 1 || (loopState.load (std::memory_order_relaxed) >= 2 && ! pickupPaused);
    }

    /** Copie du tampon de l'enregistreur (fil de l'interface, enregistreur à l'arrêt). */
    bool copyRecording (std::vector<float>& l, std::vector<float>& r, double& rate) const
    {
        // l'état « copie en cours » (4) interdit au fil audio de réécrire le tampon pendant la copie
        int expected = 2;
        if (! recState.compare_exchange_strong (expected, 4)) return false;
        const auto len = (size_t) recLen.load();
        const bool ok = len >= 16 && len <= recL.size();
        if (ok)
        {
            l.assign (recL.begin(), recL.begin() + (long) len);
            r.assign (recR.begin(), recR.begin() + (long) len);
            rate = sr;
        }
        recState.store (2);
        return ok;
    }

private:
    /** Réglages du filtre et du lo-fi, calculés une fois par bloc. */
    struct Post
    {
        int mode = 0, hold = 1;
        double cutLog = 12.0, reso = 0.0, envOct = 0.0, atk = 1.0, dec = 0.0;
        double drive = 0.0, driveGain = 1.0, driveNorm = 1.0, quant = 0.0;
    };

    /** Saturation, réduction d'échantillonnage et de bits, puis filtre multimode à enveloppe. */
    void process (const Post& q, double& l, double& r) noexcept
    {
        if (q.drive > 0.0)
        {
            // v0.22 : saturation anti-repliée (ADAA)
            l = driveL.process (l * q.driveGain) * q.driveNorm;
            r = driveR.process (r * q.driveGain) * q.driveNorm;
        }
        else { driveL.reset(); driveR.reset(); }
        if (q.hold > 1)
        {
            if (holdCount <= 0) { holdL = l; holdR = r; holdCount = q.hold; }
            --holdCount;
            l = holdL; r = holdR;
        }
        if (q.quant > 0.0)
        {
            l = std::round (std::clamp (l, -1.5, 1.5) * q.quant) / q.quant;
            r = std::round (std::clamp (r, -1.5, 1.5) * q.quant) / q.quant;
        }
        // enveloppe attaque / déclin du filtre, relancée à chaque note
        if (fenvStage == 1) { fenv += (1.02 - fenv) * q.atk; if (fenv >= 1.0) { fenv = 1.0; fenvStage = 2; } }
        else if (fenvStage == 2) { fenv *= q.dec; if (fenv < 1.0e-5) { fenv = 0.0; fenvStage = 0; } }
        if (q.mode == 0) return;
        cutSmooth += (q.cutLog - cutSmooth) * 0.002; // coupure lissée : pas de saut au bouton
        const auto hz = fastExp2 (cutSmooth + q.envOct * fenv);
        l = filters[0].process (l, q.mode, hz, q.reso);
        r = filters[1].process (r, q.mode, hz, q.reso);
    }

    enum Stage { idle = 0, attackS, holdS, releasing };
    struct Head
    {
        Stretcher st;
        double env = 0.0, vel = 1.0, gain = 1.0, fileStep = 1.0, tempoRatio = 1.0;
        int held = 0;
        Stage stage = idle;
        bool fast = false, stretched = false, reversed = false;
        const SampleBuffer* source = nullptr; // échantillon lu (nul : tampon de l'enregistreur)
        const OctaUnit* peer = nullptr;       // piste interne dont on lit l'enregistreur
        double fadeIn = 0.0, fadeOutFrames = 0.0, age = 0.0; // v0.40 : fondus de l'emplacement
        // v0.41 : début suivi — valeur de Début (%) déjà appliquée, images par % (0 : sans, découpes)
        double startPct = 0.0, framesPerPct = 0.0;
    };

    bool loopRecording() const noexcept { const auto s = loopState.load (std::memory_order_relaxed); return s == 1 || s == 3; }


    /** Ce qu'un choix d'emplacement fait jouer (v0.40) : la vue audio, l'échantillon (pour les
        attaques et la validité), la piste Pieuvre qui le porte (enregistreur d'une autre piste), et
        les attributs de l'emplacement (nul : valeurs d'origine). */
    struct SlotRef
    {
        AudioView view;
        const SampleBuffer* sb = nullptr;
        const OctaUnit* peer = nullptr;
        const float* attrs = nullptr;
    };
    SlotRef resolve (int choice) const noexcept
    {
        using namespace octa;
        SlotRef r;
        choice = std::clamp (choice, 0, numSlotChoices - 1);
        if (choice == 0)
        {
            const auto len = recLen.load (std::memory_order_relaxed);
            if (len >= 16) { r.view.left = recL.data(); r.view.right = recR.data(); r.view.length = len; r.view.sampleRate = sr; }
            return r;
        }
        if (sh == nullptr) return r;
        if (choice < recBase)
        {
            // ses 16 emplacements (sa rangée de la réserve), ou un emplacement de la réserve
            const auto index = choice <= ownSlots ? bank * bankSlots + choice - 1 : choice - poolBase;
            r.sb = sh->pool[(size_t) std::clamp (index, 0, poolSize - 1)];
            r.view = AudioView::of (r.sb);
            if (sh->attrs != nullptr) r.attrs = sh->attrs + index * attrStride;
            return r;
        }
        const auto t = choice - recBase;
        if (t == bank) return resolve (0);
        const auto* other = sh->units[(size_t) t];
        const float* l = nullptr; const float* rr = nullptr; int len = 0; double rate = sr;
        if (other != nullptr && other->recorderView (l, rr, len, rate)) { r.view.left = l; r.view.right = rr; r.view.length = len; r.view.sampleRate = rate; r.peer = other; }
        return r;
    }

    void trigger (int note, float velocity, const float* p) noexcept
    {
        currentNote = note;
        hub.start (0, note, velocity, false, p);
        fenvStage = 1; // l'enveloppe du filtre repart de sa valeur (pas de clic)
        const auto mach = octa::machineOf (p[octa::machine]);
        if (mach == octa::pickup)
        {
            // un pas relance la boucle depuis son début (synchronisation)
            if (loopState.load() >= 2) { loopPos = loopDirForward() ? 0.0 : (double) std::max (0, loopLen - 1); }
            return;
        }
        // l'ancienne tête s'efface en 4 ms : pas de clic
        heads[1] = heads[0];
        if (heads[1].stage != idle) { heads[1].stage = releasing; heads[1].fast = true; }
        auto& h = heads[0];
        h.vel = velocityGain (velocity, p[octa::velAmp] / 100.0);
        h.env = 0.0; h.stage = attackS; h.fast = false; h.held = 0;

        // clavier « Emplacements » : chaque note joue un des 16 échantillons de la piste — celui dont la
        // note (attribut, v0.40) est jouée, sinon Do3 = 1, Do#3 = 2… (même principe sur les pads et dans
        // le séquenceur), avec les mêmes réglages de lecture
        const bool keySlots = (int) p[octa::keyMode] == 2;
        int slotNow = (int) p[octa::slot];
        if (keySlots)
        {
            slotNow = (((note - 60) % 16) + 16) % 16 + 1;
            if (sh != nullptr && sh->attrs != nullptr)
                for (int k = 0; k < 16; ++k)
                    if ((int) std::lround (sh->attrs[(bank * octa::bankSlots + k) * octa::attrStride + octa::aNote]) == note) { slotNow = k + 1; break; }
        }
        const auto ref = resolve (slotNow);
        const auto& view = ref.view;
        if (! view.valid()) { h.stage = idle; return; }
        const auto* at = ref.attrs;
        auto attr = [at] (int a, float def) { return at != nullptr ? at[a] : def; };

        // rognage de l'emplacement (attributs début / fin) : la lecture, les découpes et la boucle s'y tiennent
        auto trimA = std::clamp ((double) attr (octa::aStart, 0.0f) / 100.0, 0.0, 1.0), trimB = std::clamp ((double) attr (octa::aEnd, 100.0f) / 100.0, 0.0, 1.0);
        if (trimB < trimA) std::swap (trimA, trimB);
        if (trimB - trimA < 0.001) { trimA = 0.0; trimB = 1.0; }
        const auto span = trimB - trimA;

        // découpes : grille ou attaques ; « Début » choisit la découpe (et la note en mode Découpes)
        const auto sliceMode = (int) p[octa::slices];
        const bool keySlices = (int) p[octa::keyMode] == 1;
        double a = p[octa::start] / 100.0, b = 1.0;
        int count = 0;
        static constexpr int grid[8] { 0, 2, 4, 8, 16, 32, 64, 0 };
        const SampleBuffer* sb = ref.sb;
        if (sliceMode >= 1 && sliceMode <= 6) count = grid[sliceMode];
        else if (sliceMode == 7 && sb != nullptr && ! sb->onsets.empty()) count = (int) sb->onsets.size();
        bool absolute = false; // les attaques sont des positions dans tout l'échantillon
        if (count > 0)
        {
            auto idx = std::min (count - 1, (int) std::floor (p[octa::start] / 100.0 * count));
            if (keySlices) idx += note - 60;
            idx = ((idx % count) + count) % count;
            if (sliceMode == 7) { a = sb->onsets[(size_t) idx]; b = idx + 1 < count ? sb->onsets[(size_t) idx + 1] : 1.0; absolute = true; }
            else { a = (double) idx / count; b = (double) (idx + 1) / count; }
        }
        if (! absolute) { a = trimA + a * span; b = trimA + b * span; }
        noteOffset = keySlices || keySlots ? 0.0 : (double) (note - 60);
        const auto len = std::clamp (p[octa::length] / 100.0, 0.0, 1.0);
        const auto end = a + (b - a) * std::max (len, 0.001);

        // tempo de l'échantillon : la région rognée vaut un nombre de mesures (réglage de la piste, sinon
        // attribut de l'emplacement, sinon automatique : 85 à 170 BPM)
        const auto seconds = (double) view.length / view.sampleRate * span;
        static constexpr double barChoices[8] { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
        auto barsN = barChoices[std::clamp ((int) p[octa::bars], 0, 7)];
        if (barsN <= 0.0) barsN = barChoices[std::clamp ((int) attr (octa::aBars, 0.0f), 0, 7)];
        if (barsN <= 0.0)
        {
            barsN = 1.0;
            for (const auto c : { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 }) { const auto t = c * 4.0 * 60.0 / seconds; if (t >= 85.0 && t < 170.0) { barsN = c; break; } }
        }
        const auto sampleBpm = barsN * 4.0 * 60.0 / std::max (1.0e-3, seconds);

        Stretcher::Setup s;
        s.start = a; s.end = end;
        const auto lp = (int) p[octa::loop];
        s.loop = lp == 1 ? Stretcher::loopForward : lp == 2 ? Stretcher::loopPingPong : Stretcher::noLoop;
        s.mode = (Stretcher::Mode) std::clamp ((int) p[octa::stretch], 0, 2);
        s.sliceSamples = 60.0 / sampleBpm / 4.0 * view.sampleRate;
        // v0.40 : point de boucle, fondu de boucle (attributs), tours (piste)
        const auto lpt = std::clamp ((double) attr (octa::aLoop, 0.0f) / 100.0, 0.0, 1.0);
        s.loopStart = lpt > a && lpt < end ? lpt : -1.0;
        s.xfade = std::clamp ((double) attr (octa::aXfade, 5.0f), 0.0, 500.0) * 0.001;
        s.loopCount = std::clamp ((int) p[octa::loops], 0, 16);
        // vitesse négative au départ : lecture depuis la fin
        h.reversed = p[octa::rate] < 0.0f;
        s.reverse = h.reversed;
        h.source = sb;
        h.peer = ref.peer;
        h.st.start (view, s);
        h.stretched = s.mode != Stretcher::off;
        h.fileStep = view.sampleRate / sr;
        h.tempoRatio = h.stretched ? bpm / std::max (1.0, sampleBpm) : 1.0;
        h.gain = dbToGain (p[octa::gain] + attr (octa::aGain, 0.0f));
        // v0.40 : fondus d'entrée et de sortie de l'emplacement
        h.fadeIn = std::max (0.0, (double) attr (octa::aFadeIn, 0.0f)) * 0.001 * sr;
        h.fadeOutFrames = std::max (0.0, (double) attr (octa::aFadeOut, 0.0f)) * 0.001 * view.sampleRate;
        h.age = 0.0;
        // v0.41 : Début suivi (sans découpes : avec elles, Début choisit une découpe)
        h.startPct = p[octa::start];
        h.framesPerPct = count > 0 ? 0.0 : span / 100.0 * (double) std::max (0, view.length - 1);
    }

    // --- enregistreur -----------------------------------------------------------------------
    /** 0 = départ / arrêt (cycle de la boucle), 1 = arrêter, 2 = effacer, 3 = (re)partir (pas
        d'enregistrement). Renvoie faux si l'action doit attendre (tampon en cours de copie). */
    bool doAction (int a, const float* p) noexcept
    {
        if (recState.load() == 4) return false;
        if (machineNow.load (std::memory_order_relaxed) == octa::pickup)
        {
            pickupPaused = false; // une commande de la boucle la remet en route (même séquenceur arrêté)
            const auto s = loopState.load();
            if (a == 2) { loopState.store (0); loopLen = 0; loopPos = 0.0; return true; }
            if (a == 1) { if (s == 1) closeLoop (p); else if (s == 3) loopState.store (2); return true; }
            if (s == 0)
            {
                // la boucle écrit dans le tampon de l'enregistreur : il n'est plus disponible
                int expected = recState.load();
                if (expected == 4 || ! recState.compare_exchange_strong (expected, 0)) return false;
                loopState.store (1); loopLen = 0; loopPos = 0.0;
            }
            else if (s == 1) closeLoop (p);
            else loopState.store (s == 2 ? 3 : 2);
            return true;
        }
        if (a == 2) { recState.store (0); recLen.store (0); return true; }
        if (a == 1 || (a == 0 && recState.load() == 1)) { if (recState.load() == 1) recState.store (recLen.load() > 16 ? 2 : 0); return true; }
        static constexpr int stepsFor[8] { 1, 2, 4, 8, 16, 32, 64, 0 };
        const auto choice = std::clamp ((int) p[octa::recLength], 0, 7);
        const auto stepSamples = 60.0 / std::max (20.0, bpm) / 4.0 * sr;
        recTarget = stepsFor[choice] == 0 ? (int) recL.size() : std::min ((int) recL.size(), std::max (16, (int) (stepsFor[choice] * stepSamples)));
        recPos = 0;
        recLen.store (0);
        recState.store (1);
        return true;
    }

    void queue (int note, float velocity, bool on) noexcept
    {
        if (numQueued < (int) queued.size()) queued[(size_t) numQueued++] = { note, velocity, on };
    }

    void start (int note, float velocity, const float* p) noexcept
    {
        trigger (note, velocity, p);
        // redéclenchements : le même départ, à intervalles réguliers
        pendingRetrigs = std::clamp ((int) std::lround (p[octa::retrig]), 1, 16) - 1;
        retrigEvery = std::max (1.0, octa::retrigQuarters ((int) p[octa::retrigTime]) * 60.0 / std::max (20.0, bpm) * sr);
        retrigCountdown = retrigEvery;
        retrigNote = note; retrigVel = velocity;
    }

    void stopNote (int note, const float* p) noexcept
    {
        if (note != currentNote) return;
        pendingRetrigs = 0;
        if ((int) p[octa::holdMode] == 1) heads[0].stage = heads[0].stage == idle ? idle : releasing; // tenue « Note »
        hub.release (0);
    }

    void recordFrame (double l, double r) noexcept
    {
        if (machineNow.load (std::memory_order_relaxed) == octa::pickup || recState.load (std::memory_order_relaxed) != 1) return;
        if (recPos >= recTarget || recPos >= (int) recL.size()) { recState.store (2); return; }
        recL[(size_t) recPos] = (float) l;
        recR[(size_t) recPos] = (float) r;
        ++recPos;
        recLen.store (recPos, std::memory_order_relaxed);
        if (recPos >= recTarget) recState.store (2);
    }

    // --- boucle Pickup ------------------------------------------------------------------------
    bool loopDirForward() const noexcept { return params == nullptr || (int) params[octa::pickDir] != 1; }

    void closeLoop (const float* p) noexcept
    {
        const auto total = (int) recL.size();
        auto len = std::min ((int) loopPos, total);
        const auto recorded = len; // matière réellement enregistrée (peut dépasser la boucle arrondie)
        // moins de 50 ms : appui manqué, la boucle est abandonnée
        if (len < (int) (0.05 * sr)) { loopState.store (0); loopLen = 0; loopPos = 0.0; recLen.store (0); return; }
        static constexpr int barsFor[5] { 0, 1, 2, 4, 8 };
        const auto q = barsFor[std::clamp ((int) p[octa::pickLen], 0, 4)];
        if (q > 0)
        {
            // longueur arrondie au nombre de mesures le plus proche (au moins une période, au plus le tampon)
            const auto unit = std::min ((double) total, (double) q * 4.0 * 60.0 / std::max (20.0, bpm) * sr);
            auto rounded = std::round ((double) len / unit) * unit;
            rounded = std::max (unit, std::min (rounded, (double) total));
            const auto newLen = std::min (total, (int) rounded);
            for (int i = len; i < newLen; ++i) { recL[(size_t) i] = 0.0f; recR[(size_t) i] = 0.0f; }
            len = newLen;
        }
        loopLen = std::max (16, len);
        smoothSeam (loopLen, recorded);
        loopPos = 0.0; loopDir = 1.0;
        recLen.store (loopLen);
        loopState.store (2);
    }

    /** v0.22 : jointure de la boucle sans clic. Si la prise dépasse la boucle (longueur arrondie
        vers le bas), la fin dépassante est fondue dans le début (fondu à puissance constante) ;
        sinon la fin et le début reçoivent un court fondu (5 ms). Fil audio, sans allocation. */
    void smoothSeam (int len, int recorded) noexcept
    {
        const auto F = std::min ((int) (0.005 * sr), len / 8);
        if (F < 2) return;
        if (recorded >= len + F && len + F <= (int) recL.size())
        {
            for (int i = 0; i < F; ++i)
            {
                const auto t = ((double) i + 0.5) / (double) F;
                const auto a = std::sin (0.5 * dsp::kPi * t), b = std::cos (0.5 * dsp::kPi * t);
                recL[(size_t) i] = (float) (recL[(size_t) i] * a + recL[(size_t) (len + i)] * b);
                recR[(size_t) i] = (float) (recR[(size_t) i] * a + recR[(size_t) (len + i)] * b);
            }
            return;
        }
        // fin réelle de la prise : la boucle arrondie vers le haut se termine par du silence
        const auto end = std::clamp (recorded, F, len);
        for (int i = 0; i < F; ++i)
        {
            const auto g = ((double) i + 0.5) / (double) F;
            recL[(size_t) i] *= (float) g; recR[(size_t) i] *= (float) g;
            recL[(size_t) (end - 1 - i)] *= (float) g; recR[(size_t) (end - 1 - i)] *= (float) g;
        }
    }

    void loopFrame (double inLx, double inRx, const float* p, double& outL, double& outR, bool monitor) noexcept
    {
        const auto s = loopState.load (std::memory_order_relaxed);
        outL = outR = 0.0;
        if (s == 1)
        {
            // premier passage : on enregistre, et on écoute ce qui entre
            const auto i = (int) loopPos;
            if (i >= (int) recL.size()) { closeLoop (p); return; }
            recL[(size_t) i] = (float) inLx; recR[(size_t) i] = (float) inRx;
            loopPos += 1.0;
            recLen.store (i + 1, std::memory_order_relaxed);
            if (monitor) { outL = inLx; outR = inRx; }
            return;
        }
        if (s < 2 || loopLen < 16) { pickAmp = 0.0; return; }
        // v0.22 : la boucle s'arrête et repart en 3 ms (plus de coupure sèche à l'arrêt du séquenceur)
        if (pickupPaused && pickAmp <= 0.0) return;
        const auto ampStep = 1.0 / (0.003 * sr);
        pickAmp = pickupPaused ? std::max (0.0, pickAmp - ampStep) : std::min (1.0, pickAmp + ampStep);
        const auto i = std::clamp ((int) loopPos, 0, loopLen - 1);
        outL = recL[(size_t) i] * pickAmp; outR = recR[(size_t) i] * pickAmp;
        if (s == 3 && ! pickupPaused)
        {
            const auto keep = p[octa::pickKeep] / 100.0;
            recL[(size_t) i] = (float) (recL[(size_t) i] * keep + inLx);
            recR[(size_t) i] = (float) (recR[(size_t) i] * keep + inRx);
            if (monitor) { outL += inLx; outR += inRx; }
        }
        const auto dirMode = (int) p[octa::pickDir];
        const auto step = dirMode == 1 ? -1.0 : loopDir;
        loopPos += step;
        if (dirMode == 2)
        {
            if (loopPos >= loopLen) { loopPos = loopLen - 2; loopDir = -1.0; }
            else if (loopPos < 0.0) { loopPos = 1.0; loopDir = 1.0; }
        }
        else
        {
            if (loopPos >= loopLen) loopPos -= loopLen;
            if (loopPos < 0.0) loopPos += loopLen;
        }
    }

    std::array<Head, 2> heads;
    ModHub<1> hub;
    OctaShared* sh = nullptr; // réserve commune et autres pistes internes
    int bank = 0;             // numéro de la piste interne (sa rangée de 16 dans la réserve)
    std::vector<float> recL, recR;
    mutable std::atomic<int> recState { 0 }; // 0 vide, 1 enregistre, 2 plein, 4 copie en cours (interface)
    std::atomic<int> recLen { 0 }, loopState { 0 };
    std::atomic<float> playPos { 0.0f };
    int recPos = 0, recTarget = 0, loopLen = 0;
    double loopPos = 0.0, loopDir = 1.0;
    static constexpr int fifoSize = 1024;
    std::array<float, fifoSize> fifoL {}, fifoR {};
    int fifoRead = 0, fifoWrite = 0;
    struct QueuedNote { int note; float velocity; bool on; };
    std::array<QueuedNote, 32> queued {};
    int numQueued = 0;
    const float* params = nullptr;
    const float* inL = nullptr;
    const float* inR = nullptr;
    double sr = 48000.0, bpm = 120.0, noteOffset = 0.0;
    std::array<dsp::AnalogFilter, 2> filters;
    double fenv = 0.0, cutSmooth = -1.0, holdL = 0.0, holdR = 0.0;
    int fenvStage = 0, holdCount = 0;
    double retrigEvery = 1.0, retrigCountdown = 0.0;
    int pendingRetrigs = 0, retrigNote = 60, currentNote = -1, pendingAction = -1;
    float retrigVel = 1.0f;
    std::atomic<int> machineNow { 0 };
    int recSourceNow = 0, trackMask = 0;
    bool pickupPaused = false; // boucle Pickup tue : séquenceur arrêté
    double pickAmp = 0.0;
    dsp::AdaaTanh driveL, driveR;      // gain de la boucle Pickup (fondu de 3 ms à l'arrêt et à la reprise)
};

/** Pieuvre 8 (v0.42) : UNE piste du mixeur, 8 pistes internes T1 à T8 (lignes 0 à 7 du séquenceur,
    comme les pads d'une batterie). Chaque piste interne a ses réglages, ses LFO, son enregistreur et
    ses 2 effets (inserts de pad 1 à 8) ; la réserve de 128 emplacements et leurs attributs sont
    communs. Notes : 0 à 7 = la piste interne (hauteur de base) ; 16 et plus = la piste jouée au
    clavier (setPlayLane), en chromatique. Les lignes coupées (coupure, solo) se taisent en 5 ms. */
class OctaSampler final : public Instrument
{
public:
    const ModuleDesc& desc() const noexcept override { return octa::desc(); }

    void prepare (double s, int maxBlock) override
    {
        sr = s;
        for (int k = 0; k < octa::numUnits; ++k) { units[(size_t) k].prepare (s, k, &shared); shared.units[(size_t) k] = &units[(size_t) k]; }
        const auto n = (size_t) std::max (32, maxBlock);
        bufL.assign (n, 0.0f); bufR.assign (n, 0.0f);
        chainL.assign (n, 0.0f); chainR.assign (n, 0.0f);
        reset();
    }
    void reset() noexcept override
    {
        for (auto& u : units) u.reset();
        for (auto& g : muteGain) g = 1.0f;
        for (auto& c : chainBusy) c = false;
    }

    void setSample (int index, const SampleBuffer* s) noexcept override
    {
        if (index < 0 || index >= octa::poolSize) return;
        auto& slot = shared.pool[(size_t) index];
        if (slot == s) return;
        for (auto& u : units) u.dropSource (slot);
        if (prevSource != nullptr && prevSource == slot) { prevOn = false; prevSt.stop(); prevSource = nullptr; }
        slot = s;
    }
    void setTempo (double b) noexcept override { for (auto& u : units) u.setTempo (b); }
    void setInput (const float* l, const float* r) noexcept override { for (auto& u : units) u.setInput (l, r); }
    void setExpression (float b, float w, float pr) noexcept override { units[(size_t) playLane].setExpression (b, w, pr); }
    void setPlayLane (int lane) noexcept override { playLane = std::clamp (lane, 0, octa::numUnits - 1); }
    void setLaneMutes (std::uint32_t mask) noexcept override { laneMutes = mask; }
    void setPadInserts (const std::array<std::array<Effect*, 2>, 16>* fx, const float* params, const float* bypass, double bpm) noexcept override
    { padChain = fx; padChainParams = params; padChainBypass = bypass; padChainBpm = bpm; }
    void preview (int index, float a, float b) noexcept override
    {
        prevIndex.store (index, std::memory_order_relaxed);
        prevA.store (a, std::memory_order_relaxed); prevB.store (b, std::memory_order_relaxed);
        prevSeq.fetch_add (1, std::memory_order_release);
    }

    // notes 0 à 7 : la piste interne (pads) ; 16 à 127 : le clavier — la piste jouée en chromatique, ou
    // (mode « Zones ») chaque piste dans sa zone ; 128 + ligne × 128 + note : un pas du séquenceur
    void noteOn (int note, float velocity) noexcept override { key (note, velocity, true); }
    void noteOff (int note) noexcept override { key (note, 0.0f, false); }
    bool mapNote (int note, int& lane, int& stepNote) const noexcept override
    {
        if (note < 16 || note > 127) return false;
        lane = playLane; stepNote = note;
        if (! zonesOn()) return true;
        for (int k = 0; k < octa::numUnits; ++k)
        {
            int lo = 0, hi = 0;
            zone (k, lo, hi);
            if (note >= lo && note <= hi) { lane = k; stepNote = std::clamp (note - lo + 60, 0, 127); return true; }
        }
        return true;
    }
    void allNotesOff() noexcept override { for (auto& u : units) u.allNotesOff(); }
    void recordAction (int action) noexcept override { units[(size_t) playLane].recordAction (action); }
    void transportStopped() noexcept override { for (auto& u : units) u.transportStopped(); }
    void transportStarted() noexcept override { for (auto& u : units) u.transportStarted(); }

    bool wantsMainMix() const noexcept override
    {
        for (const auto& u : units) if (u.wantsMainMix()) return true;
        return false;
    }
    /** Une piste interne sur « Mixage principal » l'emporte ; sinon les pistes du mixeur choisies par
        toutes celles qui enregistrent « Pistes choisies ». */
    int capturedTracks() const noexcept override
    {
        int mask = 0;
        for (const auto& u : units)
            if (u.wantsMainMix())
            {
                if (u.recSource() == 2) return -1;
                mask |= std::max (0, u.capturedTracks());
            }
        return mask != 0 ? mask : -1;
    }
    void captureMain (const float* l, const float* r, int n) noexcept override { for (auto& u : units) if (u.wantsMainMix()) u.captureMain (l, r, n); }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        lastP = p;
        n = std::min (n, (int) bufL.size());
        renderPreview (L, R, n);
        shared.attrs = p + octa::attrBase();
        const auto stride = octa::unitStride();
        const auto fade = 1.0f / (float) std::max (1.0, 0.005 * sr); // coupure d'une ligne : 5 ms
        for (int k = 0; k < octa::numUnits; ++k)
        {
            auto& u = units[(size_t) k];
            // les 2 effets de la piste interne (inserts de pad), avec leurs traînes
            Effect* fx[2] { nullptr, nullptr };
            bool trail[2] { false, false };
            if (padChain != nullptr)
                for (int e = 0; e < 2; ++e)
                    if (auto* x = (*padChain)[(size_t) k][(size_t) e]; x != nullptr)
                    {
                        const auto* fp = padChainParams + (k * 2 + e) * insertSlots;
                        if (padChainBypass[k * 2 + e] < 0.5f) fx[e] = x;
                        else if (x->trails (fp)) { fx[e] = x; trail[e] = true; }
                    }
            const bool chain = fx[0] != nullptr || fx[1] != nullptr;
            const bool sounding = u.busy();
            const auto target = ((laneMutes >> k) & 1u) != 0 ? 0.0f : 1.0f;
            if (! sounding) u.idleTick (n, p + k * stride);
            if (! sounding && ! (chain && chainBusy[(size_t) k])) { muteGain[(size_t) k] = target; continue; }
            std::fill (bufL.begin(), bufL.begin() + n, 0.0f);
            std::fill (bufR.begin(), bufR.begin() + n, 0.0f);
            if (sounding) u.render (bufL.data(), bufR.data(), n, p + k * stride);
            if (chain)
            {
                for (int e = 0; e < 2; ++e)
                    if (fx[e] != nullptr)
                    {
                        const auto* fp = padChainParams + (k * 2 + e) * insertSlots;
                        if (trail[e]) fx[e]->processTrails (bufL.data(), bufR.data(), n, fp, padChainBpm);
                        else fx[e]->process (bufL.data(), bufR.data(), n, fp, padChainBpm);
                    }
                // traînes : la chaîne reste calculée tant qu'elle sonne
                float peak = 0.0f;
                for (int i = 0; i < n; ++i) peak = std::max (peak, std::max (std::abs (bufL[(size_t) i]), std::abs (bufR[(size_t) i])));
                chainBusy[(size_t) k] = sounding || peak > 1.0e-5f;
            }
            auto g = muteGain[(size_t) k];
            for (int i = 0; i < n; ++i)
            {
                g += std::clamp (target - g, -fade, fade);
                L[i] += bufL[(size_t) i] * g;
                R[i] += bufR[(size_t) i] * g;
            }
            muteGain[(size_t) k] = g;
        }
    }

    /** État pour l'interface : piste jouée, puis 5 valeurs par piste interne (enregistreur : état,
        secondes ; position de lecture ; boucle Pickup ; machine). */
    int status (float* out, int max) const noexcept override
    {
        if (max < 2 + 5 * octa::numUnits) return 0;
        out[0] = (float) playLane;
        for (int k = 0; k < octa::numUnits; ++k) units[(size_t) k].status (out + 1 + 5 * k, 5);
        out[1 + 5 * octa::numUnits] = prevPos.load (std::memory_order_relaxed); // v0.43 : écoute de l'éditeur (-1 : aucune)
        return 2 + 5 * octa::numUnits;
    }
    bool isActive() const noexcept override
    {
        if (prevOn || prevSeq.load (std::memory_order_relaxed) != prevSeen) return true;
        for (int k = 0; k < octa::numUnits; ++k) if (units[(size_t) k].busy() || chainBusy[(size_t) k]) return true;
        return false;
    }
    bool copyRecording (int lane, std::vector<float>& l, std::vector<float>& r, double& rate) const override
    {
        return lane >= 0 && lane < octa::numUnits && units[(size_t) lane].copyRecording (l, r, rate);
    }
    const OctaUnit& unit (int k) const noexcept { return units[(size_t) std::clamp (k, 0, octa::numUnits - 1)]; }

private:
    /** v0.43 : écoute d'une sélection de l'éditeur — lue une fois à sa hauteur d'origine, fondus de 3 ms,
        directement à la sortie de l'instrument (sans les effets des pistes internes). */
    void renderPreview (float* L, float* R, int n) noexcept
    {
        if (const auto seq = prevSeq.load (std::memory_order_acquire); seq != prevSeen)
        {
            prevSeen = seq;
            const auto index = prevIndex.load (std::memory_order_relaxed);
            prevSource = index >= 0 && index < octa::poolSize ? shared.pool[(size_t) index] : nullptr;
            prevOn = false;
            if (prevSource != nullptr)
            {
                Stretcher::Setup su;
                su.start = std::clamp ((double) prevA.load(), 0.0, 1.0); su.end = std::clamp ((double) prevB.load(), 0.0, 1.0);
                if (su.end - su.start < 1.0e-4) { su.start = 0.0; su.end = 1.0; }
                prevSt.prepare (sr);
                prevSt.start (AudioView::of (prevSource), su);
                prevOn = prevSt.isActive();
                prevAge = 0.0;
                prevStep = prevSource->sampleRate / sr;
            }
        }
        if (! prevOn) { prevPos.store (-1.0f, std::memory_order_relaxed); return; }
        const auto fade = 0.003 * sr;
        for (int i = 0; i < n; ++i)
        {
            double l = 0.0, r = 0.0;
            const auto te = prevSt.toEnd() / std::max (1.0e-9, prevStep);
            prevSt.next (prevStep, 1.0, l, r);
            if (! prevSt.isActive()) { prevOn = false; prevPos.store (-1.0f, std::memory_order_relaxed); return; }
            const auto g = std::min ({ 1.0, prevAge / fade, te / fade });
            prevAge += 1.0;
            L[i] += (float) (l * g); R[i] += (float) (r * g);
        }
        prevPos.store ((float) prevSt.position(), std::memory_order_relaxed);
    }
    bool zonesOn() const noexcept { return lastP != nullptr && lastP[octa::kbModeIndex()] > 0.5f; }
    void zone (int k, int& lo, int& hi) const noexcept
    {
        lo = (int) std::lround (lastP[octa::unitParam (k, octa::zoneLo)]);
        hi = (int) std::lround (lastP[octa::unitParam (k, octa::zoneHi)]);
        if (hi < lo) std::swap (lo, hi);
    }
    void key (int note, float velocity, bool on) noexcept
    {
        auto send = [&] (int k, int n) { if (on) units[(size_t) k].noteOn (n, velocity); else units[(size_t) k].noteOff (n); };
        if (note >= 0 && note < octa::numUnits) send (note, 60);
        else if (note >= 128) { if ((note - 128) / 128 < octa::numUnits) send ((note - 128) / 128, (note - 128) % 128); }
        else if (note >= 16)
        {
            if (! zonesOn()) { send (playLane, note); return; }
            for (int k = 0; k < octa::numUnits; ++k)
            {
                int lo = 0, hi = 0;
                zone (k, lo, hi);
                if (note >= lo && note <= hi) send (k, std::clamp (note - lo + 60, 0, 127));
            }
        }
    }

    std::array<OctaUnit, octa::numUnits> units;
    OctaShared shared;
    const float* lastP = nullptr; // réglages joués du dernier bloc (zones du clavier)
    std::atomic<int> prevIndex { -1 }, prevSeq { 0 };
    std::atomic<float> prevA { 0.0f }, prevB { 1.0f }, prevPos { -1.0f };
    int prevSeen = 0;
    bool prevOn = false;
    Stretcher prevSt;
    const SampleBuffer* prevSource = nullptr;
    double prevAge = 0.0, prevStep = 1.0;
    std::array<float, octa::numUnits> muteGain {};
    std::array<bool, octa::numUnits> chainBusy {};
    std::vector<float> bufL, bufR, chainL, chainR;
    int playLane = 0;
    std::uint32_t laneMutes = 0;
    double sr = 48000.0;
    const std::array<std::array<Effect*, 2>, 16>* padChain = nullptr;
    const float* padChainParams = nullptr;
    const float* padChainBypass = nullptr;
    double padChainBpm = 120.0;
};

} // namespace grv::instruments
