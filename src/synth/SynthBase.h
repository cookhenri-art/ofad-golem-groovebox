/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Base commune des instruments mélodiques : allocation des voix (polyphonie, monophonie avec
    legato et portamento), rendu par voix. Chaque instrument fournit sa description et sa voix.

    Contrat d'une voix V :
        void prepare (double sampleRate);
        void reset();
        void start (int note, float velocity, bool legato, int prevNote);
                                    // legato : pas de redéclenchement ; velocity < 0 : garder la vélocité ;
                                    // prevNote : dernière note jouée (portamento polyphonique), -1 sinon
        void setSpread (int index, int count);                 // place de la voix dans un unisson
        void release();
        bool isActive() const;
        void render (float* L, float* R, int n, const float* p);  // ajoute au bus
*/

#pragma once

#include "SynthCore.h"
#include "ModHub.h"
#include "../engine/Module.h"

#include <initializer_list>

namespace grv::synth
{

using grv::engine::Instrument;
using grv::engine::ModuleDesc;

template <typename V, int N>
class SynthBase : public Instrument
{
public:
    void prepare (double sampleRate, int) override
    {
        sr = sampleRate;
        for (auto& v : voices) v.prepare (sampleRate);
        for (auto& d : declick) d.prepare (sampleRate);
        hub.bind (desc());
        hub.prepare (sampleRate);
        reset();
    }

    void reset() noexcept override
    {
        for (auto& v : voices) v.reset();
        for (auto& d : declick) d.reset();
        hub.reset();
        alloc.clear();
        stack.clear();
    }

    void noteOn (int note, float velocity) noexcept override
    {
        note = std::clamp (note, 0, 127);
        if (velocity >= 0.0f) lastVelocity = velocity;
        const auto prev = lastNote;
        lastNote = note;
        if (mono)
        {
            const bool wasHeld = ! stack.empty();
            stack.push (note);
            // monophonie : une voix ; unisson : toutes les voix sur la même note
            const auto count = std::max (1, unison);
            for (int i = 0; i < count; ++i)
            {
                auto& v = voices[(size_t) i];
                v.setSpread (i, count);
                const bool lg = legato && wasHeld && v.isActive();
                if (! lg && v.isActive()) declick[(size_t) i].arm();
                hub.start (i, note, velocity, lg, hubParams);
                v.start (note, velocity, lg, prev);
            }
            lastVoice = 0;
            return;
        }
        const auto i = alloc.noteOn (note, polyphony, [this] (int k) { return voices[(size_t) k].isActive(); });
        voices[(size_t) i].setSpread (0, 1);
        if (voices[(size_t) i].isActive()) declick[(size_t) i].arm(); // voix volée ou note rejouée
        hub.start (i, note, velocity, false, hubParams);
        voices[(size_t) i].start (note, velocity, false, prev);
        lastVoice = i;
    }

    void noteOff (int note) noexcept override
    {
        if (mono)
        {
            stack.remove (note);
            const auto count = std::max (1, unison);
            for (int i = 0; i < count; ++i)
            {
                if (stack.empty()) { voices[(size_t) i].release(); hub.release (i); }
                else if (legato) { hub.start (i, stack.top(), -1.0f, true, hubParams); voices[(size_t) i].start (stack.top(), -1.0f, true, note); } // revient à la note tenue
            }
            return;
        }
        const auto i = alloc.noteOff (note);
        if (i >= 0) { voices[(size_t) i].release(); hub.release (i); }
    }

    void allNotesOff() noexcept override
    {
        for (auto& v : voices) v.release();
        for (int i = 0; i < N; ++i) hub.release (i);
        alloc.clear();
        stack.clear();
    }

    void setTempo (double b) noexcept override { bpm = b; }
    void setExpression (float bend, float wheel, float pressure) noexcept override { hub.setExpression (bend, wheel, pressure); }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        // les voix sont rendues dans un tampon à part (anti-clic par voix) : blocs de 64 au plus
        // (le moteur travaille par 32)
        if (n > voiceBlock)
        {
            for (int o = 0; o < n; o += voiceBlock) render (L + o, R + o, std::min (voiceBlock, n - o), p);
            return;
        }
        p = adjust (p);
        configure (p);
        hubParams = p;
        // modulation commune : pitch bend sur l'accord, puis réglages modulés voix par voix
        const auto* common = hub.beginBlock (p, n, bpm);
        blockStart (common, n); // v0.47 : calculs propres à tout l'instrument (sources de la matrice)
        const auto count = mono ? std::max (1, unison) : polyphony;
        for (int i = 0; i < N; ++i)
        {
            auto& v = voices[(size_t) i];
            if (! v.isActive()) continue;
            if (i >= count) { v.release(); hub.release (i); } // polyphonie réduite pendant le jeu
            float vL[voiceBlock] {}, vR[voiceBlock] {};
            const auto* vp = hub.voiceParams (i, common);
            v.render (vL, vR, n, vp);
            voiceDone (i, vp);
            declick[(size_t) i].process (vL, vR, n);
            for (int k = 0; k < n; ++k) { L[k] += vL[k]; R[k] += vR[k]; }
            // enveloppes et LFO de la voix offerts à la matrice (bloc suivant)
            if constexpr (requires (V& x, double* a) { x.modSources (a, a); })
                if (hub.wantsExternal()) { double e[hubEnvMax] {}, l[hubLfoMax] {}; v.modSources (e, l); hub.setExternal (i, e, l); }
        }
        postProcess (L, R, n, common);
    }

    bool isActive() const noexcept override
    {
        for (const auto& v : voices) if (v.isActive()) return true;
        return false;
    }

protected:
    /** Lit le mode de jeu dans les réglages (appelé à chaque bloc). */
    virtual void configure (const float* p) noexcept = 0;
    /** Réglages vus par les voix : par défaut ceux de la piste ; un instrument peut en remplacer
        quelques-uns pour ce bloc (vitesse des LFO calée sur le tempo), sans allocation. */
    virtual const float* adjust (const float* p) noexcept { return p; }
    const float* syncRates (const float* p, int count, std::initializer_list<std::array<int, 2>> rateAndSync) noexcept
    {
        // tous les réglages sont recopiés (la modulation commune lit ceux qui suivent l'instrument)
        const auto all = std::min ((int) desc().params.size(), (int) scratch.size());
        count = std::min (count, all);
        std::copy (p, p + all, scratch.begin());
        for (const auto& rs : rateAndSync)
            if (rs[0] < count && rs[1] < count)
                scratch[(size_t) rs[0]] = (float) lfoSyncHz ((int) p[rs[1]], p[rs[0]], bpm);
        return scratch.data();
    }

    /** v0.47 : début de bloc, après la modulation commune et avant les voix (ex. physique d'une bille
        offerte à la matrice par hub.setExtra). */
    virtual void blockStart (const float* /*common*/, int /*n*/) noexcept {}
    /** v0.47 : la voix i vient d'être rendue avec ces réglages (modulés). */
    virtual void voiceDone (int /*i*/, const float* /*vp*/) noexcept {}

    /** Effet propre à l'instrument (chorus intégré…), sur le rendu de toutes les voix. */
    virtual void postProcess (float*, float*, int, const float*) noexcept {}

    /** isMono : une note à la fois ; unisonVoices > 1 : toutes ces voix jouent la note (mono). */
    void setMode (bool isMono, bool isLegato, int voicesWanted, int unisonVoices = 1) noexcept
    {
        unisonVoices = std::clamp (unisonVoices, 1, N);
        // le premier réglage du mode (premier bloc) ne coupe pas une note jouée juste avant
        if (configured && (isMono != mono || (isMono && unisonVoices != unison)))
        {
            // v0.45.1 : les notes tenues (ou reçues dans ce bloc, avant le changement de mode : préréglage
            // choisi puis note jouée) sont rejouées dans le nouveau mode au lieu d'être coupées
            std::array<int, 32> held {};
            int nh = 0;
            if (mono) for (int i = 0; i < stack.size() && nh < (int) held.size(); ++i) held[(size_t) nh++] = stack.at (i);
            else for (int i = 0; i < N && nh < (int) held.size(); ++i) if (alloc.slot (i).held && alloc.slot (i).note >= 0) held[(size_t) nh++] = alloc.slot (i).note;
            for (int i = 0; i < N; ++i) { voices[(size_t) i].release(); hub.release (i); }
            alloc.clear(); stack.clear();
            mono = isMono; legato = isLegato; unison = isMono ? unisonVoices : 1; polyphony = std::clamp (voicesWanted, 1, N);
            for (int k = 0; k < nh; ++k) noteOn (held[(size_t) k], lastVelocity);
            return;
        }
        if (! configured && isMono && ! mono)
        {
            // notes reçues avant le premier bloc (pas encore jouées) : rejouées dans la monophonie.
            // v0.47 (maj. 5 nov.) : les voix données par la polyphonie sont remises à zéro, sinon un unisson plus
            // large que l'accord gardait des voix sur d'autres notes (note sans fin au relâchement).
            // Un instrument qui gère lui-même ses notes (basse mono) n'a rien dans l'allocateur : rien à rejouer.
            std::array<int, 32> held {};
            int nh = 0;
            for (int i = 0; i < N && nh < (int) held.size(); ++i) if (alloc.slot (i).held && alloc.slot (i).note >= 0) held[(size_t) nh++] = alloc.slot (i).note;
            if (nh > 0)
            {
                for (int i = 0; i < N; ++i) { voices[(size_t) i].reset(); hub.release (i); }
                alloc.clear(); stack.clear();
                configured = true;
                mono = isMono; legato = isLegato; unison = unisonVoices; polyphony = std::clamp (voicesWanted, 1, N);
                for (int k = 0; k < nh; ++k) noteOn (held[(size_t) k], lastVelocity);
                return;
            }
        }
        configured = true;
        mono = isMono;
        legato = isLegato;
        unison = isMono ? unisonVoices : 1;
        polyphony = std::clamp (voicesWanted, 1, N);
    }

    static constexpr int voiceBlock = 64;
    std::array<V, N> voices;
    std::array<Declick, N> declick;
    ModHub<N> hub;
    const float* hubParams = nullptr; // réglages du dernier bloc (redéclenchement des LFO par voix)
    VoiceAllocator<N> alloc;
    NoteStack stack;
    double sr = 48000.0, bpm = 120.0;
    std::array<float, grv::engine::instrumentSlots> scratch {};
    bool mono = false, legato = true, configured = false;
    int polyphony = N, unison = 1, lastNote = -1, lastVoice = 0; // lastVoice : voix de la dernière note (v0.47)
    float lastVelocity = 0.8f;
};

} // namespace grv::synth
