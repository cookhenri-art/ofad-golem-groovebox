/*
    Groovebox — OFAD — AGPL-3.0-or-later
*/

#include "Engine.h"

#include <functional>
#include "Catalog.h"
#include "../instruments/Octa.h" // v0.42 : préréglages de Pieuvre 8 (piste interne jouée)

#include <cstdio>
#include <cstring>
#include <limits>

namespace grv::engine
{

//==============================================================================
// Réglages de mixage d'un canal et profondeurs de modulation

const ModuleDesc& mixerDesc (int channel)
{
    auto build = [] (bool track, float volume)
    {
        ModuleDesc m;
        m.type = "mixer";
        m.name = "Mixage";
        m.params = {
            P ("volume", "Volume", "dB", -60, 6, volume),
            P ("pan", "Panoramique", "pan", -100, 100, 0),
            P ("sendA", "Envoi A", "%", 0, 100, 0),
            P ("sendB", "Envoi B", "%", 0, 100, 0),
            P ("sendC", "Envoi C", "%", 0, 100, 0),
            P ("sendD", "Envoi D", "%", 0, 100, 0),
            C ("bypass1", "Insert 1", { "Actif", "Contourn\xC3\xA9" }),
            C ("bypass2", "Insert 2", { "Actif", "Contourn\xC3\xA9" }),
            C ("bypass3", "Insert 3", { "Actif", "Contourn\xC3\xA9" }),
            C ("bypass4", "Insert 4", { "Actif", "Contourn\xC3\xA9" }),
            P ("width", "Largeur", "%", 0, 200, 100),
            C ("sendPre", "Envois", { "Apr\xC3\xA8s le fader", "Avant le fader" }),
            // égaliseur de la tranche : plateau grave, cloche médium à fréquence réglable, plateau aigu
            P ("eqlow", "Graves", "dB", -15, 15, 0),
            P ("eqmid", "M\xC3\xA9" "diums", "dB", -15, 15, 0),
            P ("eqmidfreq", "Fr\xC3\xA9quence des m\xC3\xA9" "diums", "Hz", 200, 8000, 1000, 0.3f),
            P ("eqhigh", "Aigus", "dB", -15, 15, 0),
        };
        if (! track) for (int i = mixSendA; i <= mixSendD; ++i) m.params[(size_t) i].name = "\xE2\x80\x94";
        return m;
    };
    static const ModuleDesc trackMix = build (true, 0.0f);
    static const ModuleDesc returnMix = build (false, 0.0f);
    static const ModuleDesc masterMix = build (false, -3.0f);
    if (channel == masterChannel) return masterMix;
    if (isGroupChannel (channel)) return trackMix; // un groupe a ses envois vers les retours, comme une piste
    return channel >= numTracks ? returnMix : trackMix;
}

static const ParamDesc& modAmountDesc()
{
    static const ParamDesc d = P ("amount", "Profondeur", "%", -100, 100, 0);
    return d;
}

/** Après la 1.1 : son du pas (numéro de préréglage, 0 = aucun) : verrouillable, jamais modulé. */
const ParamDesc& trackSoundDesc()
{
    static const ParamDesc d = [] { auto p = I ("sound", "Son du pas", "", 0, trackSoundMax, 0); p.noMod = true; return p; }();
    return d;
}

/** Interrupteur d'un insert de pad (contourné ou non). */
static const ParamDesc& padBypassDesc()
{
    static const ParamDesc d = C ("bypass", "Contourn\xC3\xA9", { "Non", "Oui" });
    return d;
}

//==============================================================================

Engine::Engine()
    : seq (std::make_unique<Sequencer>()),
      base (new std::atomic<float>[numSlots]),
      effective (new std::atomic<float>[numSlots]),
      state (std::make_unique<std::array<ChannelState, numChannels>>())
{
    for (int i = 0; i < numSlots; ++i) { base[i].store (0.0f); effective[i].store (0.0f); }
    for (int t = 0; t < numTracks; ++t) slotOwner[(size_t) t] = audioOwner[(size_t) t] = (std::uint8_t) t; // jetons des pistes (v0.26)

    for (int ch = 0; ch < numChannels; ++ch)
    {
        applyDefaults (ch, mixerBase, mixerDesc (ch));
        applyModSourceDefaults (ch);
        for (int a = 0; a < numModAssigns; ++a) storeBase (globalSlot (ch, modAmountBase + a), 0.5f);
    }

    for (int t = 0; t < numTracks; ++t) info[(size_t) t].name = "Piste " + std::to_string (t + 1);

    scenes.publish (std::make_unique<SceneSet>(), bin, 0.0);
    extras.publish (std::make_unique<PatternExtras>(), bin, 0.0);
    soundPool.publish (std::make_unique<SoundPool>(), bin, 0.0); // après la 1.1 : sons du pas
    song.publish (std::make_unique<Song>(), bin, 0.0);
    for (auto& h : heldGlobal) h.store (-1);
    for (auto& p : trackPhase) p.store (-1.0f);
    publishGraph (0.0);
}

Engine::~Engine() = default;

void Engine::prepare (double sr, int maxBlock)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    maxBlockSize = std::max (32, maxBlock);
    for (auto& i : instruments) if (i) { i->prepare (sampleRate, controlBlock); i->reset(); i->setMidiOut (&midiOut); }
    for (auto& ch : effects) for (auto& e : ch) if (e) { e->prepare (sampleRate, controlBlock); e->reset(); }
    for (auto& c : *state) { c.seenVersion = 0xFFFFFFFFu; for (auto& m : c.mods) m.reset(); }
    numPendingOffs = 0;
    loudness.prepare (sampleRate);
    const auto compLen = (size_t) std::min (maxCompensation * 4, (int) (sampleRate * 0.25) + controlBlock + 1);
    for (int t = 0; t < numTracks; ++t) { compL[(size_t) t].assign (compLen, 0.0f); compR[(size_t) t].assign (compLen, 0.0f); compWrite[(size_t) t] = 0; }
    for (auto& b : busDly) b.prepare ((int) compLen); // 1.1 (lot 6)
    groupLat.fill (0); returnLat.fill (0); busComp = 0;
    compDelay = 0;
    compReported.store (0);
    trackLat.fill (0); compPrev.fill (0); compOld.fill (0); compFadeLeft.fill (0);
}

void Engine::applyDefaults (int channel, int baseSlot, const ModuleDesc& d) noexcept
{
    for (size_t i = 0; i < d.params.size(); ++i)
    {
        const auto g = (size_t) globalSlot (channel, baseSlot + (int) i);
        const auto v = d.params[i].toNorm (d.params[i].def);
        base[g].store (v);
        effective[g].store (v);
    }
    paramVersion[(size_t) channel].fetch_add (1);
}

void Engine::applyModSourceDefaults (int channel) noexcept
{
    static constexpr float types[numModSources] { 0, 0, 1, 1, 2, 3 }; // LFO, LFO, fonction, fonction, suiveur, séquenceur
    const auto& d = modSourceDesc();
    for (int s = 0; s < numModSources; ++s)
    {
        applyDefaults (channel, modSourceBase + s * modSourceSlots, d);
        const auto g = (size_t) globalSlot (channel, modSourceBase + s * modSourceSlots + msType);
        const auto v = d.params[(size_t) msType].toNorm (types[s]);
        base[g].store (v); effective[g].store (v);
        if (types[s] == 1)
        {
            // fonctions : redéclenchées à chaque note (enveloppe de modulation)
            const auto gr = (size_t) globalSlot (channel, modSourceBase + s * modSourceSlots + msRetrig);
            const auto r = d.params[(size_t) msRetrig].toNorm (1.0f);
            base[gr].store (r); effective[gr].store (r);
        }
    }
    paramVersion[(size_t) channel].fetch_add (1);
}

void Engine::resetParams (int channel, int from, int count) noexcept
{
    for (int i = 0; i < count; ++i) { base[(size_t) globalSlot (channel, from + i)].store (0.0f); effective[(size_t) globalSlot (channel, from + i)].store (0.0f); }
    paramVersion[(size_t) channel].fetch_add (1);
}

//==============================================================================
// Structure

bool Engine::setInstrument (int track, const std::string& type, double nowMs)
{
    if (track < 0 || track >= numTracks) return false;
    std::unique_ptr<Instrument> next;
    if (! type.empty())
    {
        next = createInstrument (type);
        if (next == nullptr) return false;
        next->prepare (sampleRate, controlBlock);
        next->reset();
        next->setMidiOut (&midiOut); // après la 1.1 : pistes MIDI
    }

    if (type.empty()) setTrackOutput (track, 0); // piste libérée : elle revient sur le bus principal (v0.24)
    resetParams (track, instrumentBase, instrumentSlots);
    if (next) applyDefaults (track, instrumentBase, next->desc());

    bin.retire (std::move (instruments[(size_t) track]), nowMs);
    instruments[(size_t) track] = std::move (next);
    // les inserts de pad appartenaient à l'ancien instrument
    for (int pad = 0; pad < numPads; ++pad)
        for (int k = 0; k < padInserts; ++k)
        {
            bin.retire (std::move (padEffects[(size_t) track][(size_t) pad][(size_t) k]), nowMs);
            padInsertTypes[(size_t) track][(size_t) pad][(size_t) k].clear();
        }
    resetParams (track, padBypassBase, numPads * padInserts);
    resetParams (track, padInsertBase, numPads * padInserts * insertSlots);
    for (auto& smp : samples[(size_t) track]) bin.retire (std::move (smp), nowMs);

    auto& ti = info[(size_t) track];
    // 1.0 (7 nov.) : la piste prend le nom de la machine tant qu'on ne l'a pas renommée : nom par défaut
    // (« Piste N ») ou nom de la machine précédente → nom de la nouvelle ; piste libérée → « Piste N »
    const auto* old = findInstrument (ti.instrumentType);
    const bool autoName = ti.name.rfind ("Piste ", 0) == 0 || (old != nullptr && ti.name == old->name);
    ti.instrumentType = type;
    ti.preset.clear();
    if (autoName)
    {
        if (const auto* d = findInstrument (type)) ti.name = d->name;
        else if (type.empty()) ti.name = "Piste " + std::to_string (track + 1);
    }

    // Les verrous de pas et assignations qui visaient l'ancien instrument n'ont plus de sens
    auto& mods = modAssigns[(size_t) track];
    mods.erase (std::remove_if (mods.begin(), mods.end(), [] (const ModAssign& m)
        { return (m.target >= instrumentBase && m.target < instrumentBase + instrumentSlots) || m.target >= padInsertBase; }), mods.end());

    publishGraph (nowMs);
    // courbes du morceau qui visaient l'ancien instrument (et ses inserts de pad) : retirées
    remapSongLanes ([track] (int g)
    {
        const auto l = localOf (g);
        const bool gone = channelOf (g) == track && ((l >= instrumentBase && l < instrumentBase + instrumentSlots) || l >= padBypassBase);
        return gone ? -1 : g;
    }, nowMs);
    return true;
}

bool Engine::permuteTracks (const std::array<int, numTracks>& newIndex, double nowMs)
{
    std::array<bool, numTracks> seen {};
    bool moves = false;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto n = newIndex[(size_t) t];
        if (n < 0 || n >= numTracks || seen[(size_t) n]) return false;
        seen[(size_t) n] = true;
        moves = moves || n != t;
    }
    if (! moves) return true;
    // une table par membre : la piste t va en newIndex[t]
    auto move = [&newIndex] (auto& arr)
    {
        auto tmp = std::make_unique<std::array<std::remove_reference_t<decltype (arr[0])>, numTracks>>();
        for (int t = 0; t < numTracks; ++t) (*tmp)[(size_t) newIndex[(size_t) t]] = std::move (arr[(size_t) t]);
        for (int t = 0; t < numTracks; ++t) arr[(size_t) t] = std::move ((*tmp)[(size_t) t]);
    };
    move (instruments);
    move (padEffects);
    move (padInsertTypes);
    move (samples);
    move (speechScripts); // v0.33
    move (info);
    move (slotOwner);
    // tables communes à tous les canaux : seules les 16 pistes bougent
    {
        auto tmpFx = std::make_unique<std::array<std::array<std::unique_ptr<Effect>, numInserts>, numTracks>>();
        std::array<std::array<std::string, numInserts>, numTracks> types, presets;
        std::array<std::vector<ModAssign>, numTracks> mods;
        for (int t = 0; t < numTracks; ++t)
        {
            const auto n = (size_t) newIndex[(size_t) t];
            (*tmpFx)[n] = std::move (effects[(size_t) t]);
            types[n] = insertTypes[(size_t) t];
            presets[n] = insertPresetNames[(size_t) t];
            mods[n] = std::move (modAssigns[(size_t) t]);
        }
        for (int t = 0; t < numTracks; ++t)
        {
            effects[(size_t) t] = std::move ((*tmpFx)[(size_t) t]);
            insertTypes[(size_t) t] = types[(size_t) t];
            insertPresetNames[(size_t) t] = presets[(size_t) t];
            modAssigns[(size_t) t] = std::move (mods[(size_t) t]);
        }
    }
    std::array<int, numTracks> outs {};
    for (int t = 0; t < numTracks; ++t) outs[(size_t) newIndex[(size_t) t]] = trackOutput[(size_t) t].load (std::memory_order_relaxed);
    for (int t = 0; t < numTracks; ++t) trackOutput[(size_t) t].store (outs[(size_t) t], std::memory_order_relaxed);
    // réglages (mixage, instrument, inserts, modulations, inserts de pad) de chaque piste
    {
        std::vector<float> b ((size_t) (numTracks * slotsPerChannel)), e ((size_t) (numTracks * slotsPerChannel));
        for (int t = 0; t < numTracks; ++t)
            for (int sl = 0; sl < slotsPerChannel; ++sl)
            {
                const auto dst = (size_t) (newIndex[(size_t) t] * slotsPerChannel + sl);
                b[dst] = base[(size_t) globalSlot (t, sl)].load (std::memory_order_relaxed);
                e[dst] = effective[(size_t) globalSlot (t, sl)].load (std::memory_order_relaxed);
            }
        for (int t = 0; t < numTracks; ++t)
        {
            for (int sl = 0; sl < slotsPerChannel; ++sl)
            {
                base[(size_t) globalSlot (t, sl)].store (b[(size_t) (t * slotsPerChannel + sl)], std::memory_order_relaxed);
                effective[(size_t) globalSlot (t, sl)].store (e[(size_t) (t * slotsPerChannel + sl)], std::memory_order_relaxed);
            }
            paramVersion[(size_t) t].fetch_add (1);
        }
    }
    seq->permuteData (newIndex);
    publishGraph (nowMs); // le fil audio déplace l'état des pistes en adoptant ce graphe
    return true;
}

void Engine::adoptTrackOrder (const Graph& g) noexcept
{
    if (g.owner == audioOwner) return;
    // où est passé ce que le fil audio avait en t
    std::array<int, numTracks> newIndex {};
    for (int t = 0; t < numTracks; ++t)
    {
        newIndex[(size_t) t] = t;
        for (int p = 0; p < numTracks; ++p) if (g.owner[(size_t) p] == audioOwner[(size_t) t]) { newIndex[(size_t) t] = p; break; }
    }
    auto& st = *state;
    std::array<bool, numTracks> done {};
    for (int start = 0; start < numTracks; ++start)
    {
        int t = start;
        while (! done[(size_t) t] && newIndex[(size_t) t] != start)
        {
            done[(size_t) t] = true;
            const auto n = (size_t) newIndex[(size_t) t];
            std::swap (st[(size_t) start], st[n]);
            std::swap (scCurL[(size_t) start], scCurL[n]); std::swap (scCurR[(size_t) start], scCurR[n]);
            std::swap (scPrevL[(size_t) start], scPrevL[n]); std::swap (scPrevR[(size_t) start], scPrevR[n]);
            std::swap (scCurValid[(size_t) start], scCurValid[n]); std::swap (scPrevValid[(size_t) start], scPrevValid[n]);
            std::swap (absorbedNow[(size_t) start], absorbedNow[n]);
            std::swap (trackWasPlaying[(size_t) start], trackWasPlaying[n]);
            std::swap (arpRecording[(size_t) start], arpRecording[n]);
            t = (int) n;
        }
        done[(size_t) t] = true;
    }
    for (int i = 0; i < numPendingOffs; ++i)
    {
        auto& o = pendingOffs[(size_t) i];
        if (o.track >= 0 && o.track < numTracks) o.track = newIndex[(size_t) o.track];
    }
    seq->permuteRuntime (newIndex);
    audioOwner = g.owner;
}

bool Engine::setPadInsert (int track, int pad, int slot, const std::string& type, double nowMs)
{
    if (track < 0 || track >= numTracks || pad < 0 || pad >= numPads || slot < 0 || slot >= padInserts) return false;
    if (instruments[(size_t) track] == nullptr || ! instruments[(size_t) track]->desc().drumKit) return false;
    std::unique_ptr<Effect> next;
    if (! type.empty())
    {
        next = createEffect (type);
        if (next == nullptr) return false;
        next->prepare (sampleRate, controlBlock);
        next->reset();
    }
    const auto baseSlot = padInsertSlot (pad, slot);
    resetParams (track, baseSlot, insertSlots);
    if (next) applyDefaults (track, baseSlot, next->desc());
    storeBase (globalSlot (track, padBypassBase + pad * padInserts + slot), 0.0f);
    bin.retire (std::move (padEffects[(size_t) track][(size_t) pad][(size_t) slot]), nowMs);
    padEffects[(size_t) track][(size_t) pad][(size_t) slot] = std::move (next);
    padInsertTypes[(size_t) track][(size_t) pad][(size_t) slot] = type;
    auto& mods = modAssigns[(size_t) track];
    mods.erase (std::remove_if (mods.begin(), mods.end(), [baseSlot] (const ModAssign& m)
        { return m.target >= baseSlot && m.target < baseSlot + insertSlots; }), mods.end());
    publishGraph (nowMs);
    remapSongLanes ([track, baseSlot] (int g) { return channelOf (g) == track && localOf (g) >= baseSlot && localOf (g) < baseSlot + insertSlots ? -1 : g; }, nowMs);
    return true;
}

const ModuleDesc* Engine::moduleAt (int channel, int slot, int& baseSlot) const noexcept
{
    if (slot < 0) { baseSlot = instrumentBase; return instrumentDesc (channel); }
    if (slot < numInserts) { baseSlot = insertBase + slot * insertSlots; return effectDesc (channel, slot); }
    const auto k = slot - 100;
    if (k < 0 || k >= numPads * padInserts) return nullptr;
    baseSlot = padInsertSlot (k / padInserts, k % padInserts);
    return padEffectDesc (channel, k / padInserts, k % padInserts);
}

bool Engine::setInsert (int channel, int slot, const std::string& type, double nowMs)
{
    if (channel < 0 || channel >= numChannels || slot < 0 || slot >= numInserts) return false;
    std::unique_ptr<Effect> next;
    if (! type.empty())
    {
        next = createEffect (type);
        if (next == nullptr) return false;
        next->prepare (sampleRate, controlBlock);
        next->reset();
    }
    const auto baseSlot = insertBase + slot * insertSlots;
    resetParams (channel, baseSlot, insertSlots);
    if (next) applyDefaults (channel, baseSlot, next->desc());
    storeBase (globalSlot (channel, mixBypass1 + slot), mixerDesc (channel).params[(size_t) (mixBypass1 + slot)].toNorm (0));

    bin.retire (std::move (effects[(size_t) channel][(size_t) slot]), nowMs);
    effects[(size_t) channel][(size_t) slot] = std::move (next);
    insertTypes[(size_t) channel][(size_t) slot] = type;
    insertPresetNames[(size_t) channel][(size_t) slot].clear();
    if (channel < numTracks) { info[(size_t) channel].insertTypes[(size_t) slot] = type; info[(size_t) channel].insertPresets[(size_t) slot].clear(); }

    auto& mods = modAssigns[(size_t) channel];
    mods.erase (std::remove_if (mods.begin(), mods.end(), [baseSlot] (const ModAssign& m)
        { return m.target >= baseSlot && m.target < baseSlot + insertSlots; }), mods.end());

    publishGraph (nowMs);
    remapSongLanes ([channel, baseSlot] (int g) { return channelOf (g) == channel && localOf (g) >= baseSlot && localOf (g) < baseSlot + insertSlots ? -1 : g; }, nowMs);
    return true;
}

void Engine::moveInsert (int channel, int from, int to, double nowMs)
{
    if (channel < 0 || channel >= numChannels || from < 0 || to < 0 || from >= numInserts || to >= numInserts || from == to) return;
    auto& fx = effects[(size_t) channel];
    std::swap (fx[(size_t) from], fx[(size_t) to]);
    std::swap (insertTypes[(size_t) channel][(size_t) from], insertTypes[(size_t) channel][(size_t) to]);
    std::swap (insertPresetNames[(size_t) channel][(size_t) from], insertPresetNames[(size_t) channel][(size_t) to]);
    if (channel < numTracks)
    {
        std::swap (info[(size_t) channel].insertTypes[(size_t) from], info[(size_t) channel].insertTypes[(size_t) to]);
        std::swap (info[(size_t) channel].insertPresets[(size_t) from], info[(size_t) channel].insertPresets[(size_t) to]);
    }
    // Les réglages suivent l'effet
    for (int i = 0; i < insertSlots; ++i)
    {
        auto& a = base[(size_t) globalSlot (channel, insertBase + from * insertSlots + i)];
        auto& b = base[(size_t) globalSlot (channel, insertBase + to * insertSlots + i)];
        const auto va = a.load(); a.store (b.load()); b.store (va);
    }
    // v0.46 : l'interrupteur « contourné » suit aussi l'effet
    {
        auto& a = base[(size_t) globalSlot (channel, mixBypass1 + from)];
        auto& b = base[(size_t) globalSlot (channel, mixBypass1 + to)];
        const auto va = a.load(); a.store (b.load()); b.store (va);
    }
    const auto remap = insertSwapMap (channel, from, to);
    // v0.46 : modulations du canal et scènes suivent l'effet déplacé
    for (auto& m : modAssigns[(size_t) channel])
        m.target = localOf (remap (globalSlot (channel, m.target)));
    {
        auto sc = std::make_unique<SceneSet> (*scenes.editable());
        bool changed = false;
        for (auto& list : sc->scenes)
        {
            for (auto& lk : list) if (const auto g = remap (lk.slot); g != lk.slot) { lk.slot = g; changed = true; }
            std::sort (list.begin(), list.end(), [] (const SceneLock& x, const SceneLock& y) { return x.slot < y.slot; });
        }
        if (changed) publishScenes (std::move (sc), nowMs);
    }
    paramVersion[(size_t) channel].fetch_add (1);
    publishGraph (nowMs);
    // les courbes suivent aussi l'effet déplacé
    remapSongLanes (remap, nowMs);
}

std::function<int (int)> Engine::insertSwapMap (int channel, int from, int to)
{
    return [channel, from, to] (int g)
    {
        if (g < 0 || channelOf (g) != channel) return g;
        const auto l = localOf (g);
        const auto a = insertBase + from * insertSlots, b = insertBase + to * insertSlots;
        if (l >= a && l < a + insertSlots) return globalSlot (channel, l - a + b);
        if (l >= b && l < b + insertSlots) return globalSlot (channel, l - b + a);
        if (l == mixBypass1 + from) return globalSlot (channel, mixBypass1 + to);
        if (l == mixBypass1 + to) return globalSlot (channel, mixBypass1 + from);
        return g;
    };
}

bool Engine::applyPreset (int channel, int slot, const std::string& presetName)
{
    if (channel < 0 || channel >= numChannels) return false;
    int baseSlot = 0;
    const ModuleDesc* d = moduleAt (channel, slot, baseSlot);
    if (d == nullptr) return false;
    // v0.42 : Pieuvre 8 — un préréglage règle la piste interne jouée (son bloc de réglages), pas les 8
    if (slot < 0 && channel < numTracks && d->type == "octa")
    {
        const auto& u = instruments::octa::unitDesc();
        const auto lane = std::clamp (playLane[(size_t) channel].load (std::memory_order_relaxed), 0, instruments::octa::numUnits - 1);
        const auto from = baseSlot + instruments::octa::unitParam (lane, 0);
        for (const auto& pr : u.presets)
        {
            if (pr.name != presetName) continue;
            // réglages gardés (zone du clavier de la piste)
            std::array<std::pair<int, float>, 8> kept {};
            int nk = 0;
            for (size_t i = 0; i < u.params.size() && nk < (int) kept.size(); ++i)
                if (u.params[i].keepOnPreset) kept[(size_t) nk++] = { (int) i, base[(size_t) globalSlot (channel, from + (int) i)].load (std::memory_order_relaxed) };
            applyDefaults (channel, from, u);
            for (int k = 0; k < nk; ++k) storeBase (globalSlot (channel, from + kept[(size_t) k].first), kept[(size_t) k].second);
            for (const auto& [id, value] : pr.values)
                if (const auto i = u.indexOf (id); i >= 0) storeBase (globalSlot (channel, from + i), u.params[(size_t) i].toNorm (value));
            paramVersion[(size_t) channel].fetch_add (1);
            info[(size_t) channel].preset = presetName;
            return true;
        }
        return false;
    }
    for (const auto& pr : d->presets)
    {
        if (pr.name != presetName) continue;
        // un préréglage part des réglages d'origine ; les réglages de branchement (source du
        // modulateur d'un vocodeur…) restent ceux de la piste (v0.25)
        std::array<float, 16> kept {};
        int nk = 0;
        for (size_t i = 0; i < d->params.size() && nk < (int) kept.size(); ++i)
            if (d->params[i].keepOnPreset) kept[(size_t) nk++] = base[(size_t) globalSlot (channel, baseSlot + (int) i)].load (std::memory_order_relaxed);
        applyDefaults (channel, baseSlot, *d);
        nk = 0;
        for (size_t i = 0; i < d->params.size() && nk < (int) kept.size(); ++i)
            if (d->params[i].keepOnPreset) storeBase (globalSlot (channel, baseSlot + (int) i), kept[(size_t) nk++]);
        for (const auto& [id, value] : pr.values)
        {
            const auto i = d->indexOf (id);
            if (i >= 0) storeBase (globalSlot (channel, baseSlot + i), d->params[(size_t) i].toNorm (value));
        }
        paramVersion[(size_t) channel].fetch_add (1);
        if (slot >= 0 && slot < numInserts) insertPresetNames[(size_t) channel][(size_t) slot] = presetName;
        if (channel < numTracks)
        {
            if (slot < 0) info[(size_t) channel].preset = presetName;
            else if (slot < numInserts) info[(size_t) channel].insertPresets[(size_t) slot] = presetName;
        }
        return true;
    }
    return false;
}

std::vector<std::pair<std::string, float>> Engine::moduleValues (int channel, int slot, const std::string& prefix) const
{
    std::vector<std::pair<std::string, float>> out;
    int baseSlot = 0;
    const auto* d = moduleAt (channel, slot, baseSlot);
    if (d == nullptr) return out;
    for (size_t i = 0; i < d->params.size(); ++i)
    {
        const auto& p = d->params[i];
        if (! prefix.empty() && p.id.compare (0, prefix.size(), prefix) != 0) continue;
        const auto norm = base[(size_t) globalSlot (channel, baseSlot + (int) i)].load (std::memory_order_relaxed);
        out.emplace_back (p.id.substr (prefix.size()), p.toPlain (norm));
    }
    return out;
}

bool Engine::applyValues (int channel, int slot, const std::vector<std::pair<std::string, float>>& values,
                          const std::string& prefix, const std::string& presetName)
{
    int baseSlot = 0;
    const auto* d = moduleAt (channel, slot, baseSlot);
    if (d == nullptr) return false;
    for (size_t i = 0; i < d->params.size(); ++i)
    {
        const auto& p = d->params[i];
        if (! prefix.empty() && p.id.compare (0, prefix.size(), prefix) != 0) continue;
        if (p.keepOnPreset) continue; // réglage de branchement : celui de la piste reste (v0.25)
        auto v = p.def;
        const auto key = p.id.substr (prefix.size());
        for (const auto& [id, value] : values)
            if (id == key && std::isfinite (value)) { v = std::clamp (value, std::min (p.min, p.max), std::max (p.min, p.max)); break; }
        storeBase (globalSlot (channel, baseSlot + (int) i), p.toNorm (v));
    }
    paramVersion[(size_t) channel].fetch_add (1);
    if (! presetName.empty() && prefix.empty())
    {
        if (slot >= 0 && slot < numInserts) insertPresetNames[(size_t) channel][(size_t) slot] = presetName;
        if (channel < numTracks)
        {
            if (slot < 0) info[(size_t) channel].preset = presetName;
            else if (slot < numInserts) info[(size_t) channel].insertPresets[(size_t) slot] = presetName;
        }
    }
    return true;
}

void Engine::publishGraph (double nowMs)
{
    if (batchDepth > 0) { batchDirty = true; return; }
    auto g = std::make_unique<Graph>();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& c = g->ch[(size_t) ch];
        const bool isTrack = ch < numTracks;
        c.instrument = isTrack ? instruments[(size_t) ch].get() : nullptr;
        c.used = isTrack ? c.instrument != nullptr : true;
        for (int i = 0; i < numInserts; ++i) c.inserts[(size_t) i] = effects[(size_t) ch][(size_t) i].get();
        c.mods = modAssigns[(size_t) ch];
        if (isTrack) for (int i = 0; i < maxSamples; ++i) c.samples[(size_t) i] = samples[(size_t) ch][(size_t) i].get();
        if (isTrack) c.speech = speechScripts[(size_t) ch].get();

        const auto& mix = mixerDesc (ch);
        for (size_t i = 0; i < mix.params.size(); ++i) c.desc[(size_t) mixerBase + i] = &mix.params[i];
        c.numRanges = 0;
        c.ranges[(size_t) c.numRanges++] = { mixerBase, instrumentBase };
        if (c.instrument)
        {
            const auto& d = c.instrument->desc();
            for (size_t i = 0; i < d.params.size() && (int) i < instrumentSlots; ++i) c.desc[(size_t) instrumentBase + i] = &d.params[i];
            c.ranges[(size_t) (c.numRanges - 1)].second = instrumentBase + std::min ((int) d.params.size(), instrumentSlots);
            const auto a = d.indexOf ("arp_mode");
            c.arpParam = a >= 0 && a < instrumentSlots ? a : -1;
            const auto ao = d.indexOf ("arp_on");
            c.arpOnParam = ao >= 0 && ao < instrumentSlots ? ao : -1;
            auto gl = d.indexOf ("glide");
            if (gl < 0) gl = d.indexOf ("porta");
            c.glideParam = gl >= 0 && gl < instrumentSlots ? gl : -1;
        }
        else c.glideParam = -1;
        for (int s = 0; s < numInserts; ++s)
            if (auto* e = c.inserts[(size_t) s])
            {
                const auto& d = e->desc();
                for (size_t i = 0; i < d.params.size() && (int) i < insertSlots; ++i)
                    c.desc[(size_t) (insertBase + s * insertSlots) + i] = &d.params[i];
                c.ranges[(size_t) c.numRanges++] = { insertBase + s * insertSlots, insertBase + s * insertSlots + std::min ((int) d.params.size(), insertSlots) };
            }
        c.anyPadFx = false;
        if (isTrack)
            for (int pad = 0; pad < numPads; ++pad)
                for (int k = 0; k < padInserts; ++k)
                    if (auto* e = padEffects[(size_t) ch][(size_t) pad][(size_t) k].get())
                    {
                        c.padFx[(size_t) pad][(size_t) k] = e;
                        c.anyPadFx = true;
                        const auto b = padInsertSlot (pad, k);
                        const auto& d = e->desc();
                        for (size_t i = 0; i < d.params.size() && (int) i < insertSlots; ++i) c.desc[(size_t) b + i] = &d.params[i];
                        c.ranges[(size_t) c.numRanges++] = { b, b + std::min ((int) d.params.size(), insertSlots) };
                    }
        for (int i = 0; i < numPads * padInserts; ++i) c.desc[(size_t) (padBypassBase + i)] = &padBypassDesc();
        if (isTrack && c.instrument != nullptr) c.desc[(size_t) trackSoundSlot] = &trackSoundDesc(); // après la 1.1 : son du pas
        for (int s = 0; s < numModSources; ++s)
            for (int i = 0; i < modSourceSlots; ++i)
                c.desc[(size_t) (modSourceBase + s * modSourceSlots + i)] = &modSourceDesc().params[(size_t) i];
        for (int a = 0; a < numModAssigns; ++a) c.desc[(size_t) (modAmountBase + a)] = &modAmountDesc();
        c.ranges[(size_t) c.numRanges++] = { modSourceBase, padBypassBase + numPads * padInserts };
        c.usedSlots = padBypassBase + numPads * padInserts;
    }
    g->owner = slotOwner;
    graph.publish (std::move (g), bin, nowMs);
    structureVersion.fetch_add (1);
    for (auto& v : paramVersion) v.fetch_add (1);
}

//==============================================================================
// Réglages

void Engine::setParam (int global, float norm) noexcept
{
    if (global < 0 || global >= numSlots || ! std::isfinite (norm)) return;
    base[(size_t) global].store (std::clamp (norm, 0.0f, 1.0f), std::memory_order_relaxed);
    effective[(size_t) global].store (std::clamp (norm, 0.0f, 1.0f), std::memory_order_relaxed); // repris par le fil audio si le réglage est modulé
    paramVersion[(size_t) channelOf (global)].fetch_add (1, std::memory_order_release);
}

float Engine::getParam (int global) const noexcept
{
    return global >= 0 && global < numSlots ? base[(size_t) global].load (std::memory_order_relaxed) : 0.0f;
}

float Engine::getEffective (int global) const noexcept
{
    return global >= 0 && global < numSlots ? effective[(size_t) global].load (std::memory_order_relaxed) : 0.0f;
}

const ParamDesc* Engine::describe (int global) const noexcept
{
    // Résolu d'après les modules en place (fil de l'interface), pas d'après le graphe publié :
    // juste après un changement de structure, la description est déjà la bonne.
    if (global < 0 || global >= numSlots) return nullptr;
    const auto ch = channelOf (global), local = localOf (global);
    auto at = [] (const ModuleDesc* d, int i) -> const ParamDesc* { return d != nullptr && i >= 0 && i < (int) d->params.size() ? &d->params[(size_t) i] : nullptr; };
    if (local < instrumentBase) return at (&mixerDesc (ch), local - mixerBase);
    if (local < insertBase) return ch < numTracks ? at (instrumentDesc (ch), local - instrumentBase) : nullptr;
    if (local < modSourceBase) return at (effectDesc (ch, (local - insertBase) / insertSlots), (local - insertBase) % insertSlots);
    if (local < modSourceBase + numModSources * modSourceSlots) return at (&modSourceDesc(), (local - modSourceBase) % modSourceSlots);
    if (local >= modAmountBase && local < modAmountBase + numModAssigns) return &modAmountDesc();
    if (local >= padBypassBase && local < padBypassBase + numPads * padInserts) return &padBypassDesc();
    if (local == trackSoundSlot) return ch < numTracks && instrumentDesc (ch) != nullptr ? &trackSoundDesc() : nullptr; // après la 1.1
    if (local >= padInsertBase && ch < numTracks)
    {
        const auto k = (local - padInsertBase) / insertSlots;
        return at (padEffectDesc (ch, k / padInserts, k % padInserts), (local - padInsertBase) % insertSlots);
    }
    return nullptr;
}

int Engine::findParam (int channel, const std::string& area, const std::string& id) const
{
    if (channel < 0 || channel >= numChannels) return -1;
    const ModuleDesc* d = nullptr;
    int b = 0;
    if (area == "mix") { d = &mixerDesc (channel); b = mixerBase; }
    else if (area == "inst") { if (channel < numTracks && instruments[(size_t) channel]) { d = &instruments[(size_t) channel]->desc(); b = instrumentBase; } }
    else if (area.size() == 3 && area.rfind ("fx", 0) == 0)
    {
        const auto s = area[2] - '1';
        if (s >= 0 && s < numInserts && effects[(size_t) channel][(size_t) s]) { d = &effects[(size_t) channel][(size_t) s]->desc(); b = insertBase + s * insertSlots; }
    }
    else if (area.size() == 4 && area.rfind ("mod", 0) == 0)
    {
        const auto s = area[3] - '1';
        if (s >= 0 && s < numModSources) { d = &modSourceDesc(); b = modSourceBase + s * modSourceSlots; }
    }
    else if (area == "son") // après la 1.1 : son du pas
        return channel < numTracks && id == "n" ? globalSlot (channel, trackSoundSlot) : -1;
    else if (area == "padbyp")
    {
        // interrupteurs des inserts de pad : « b<pad>_<n> » (1..16, 1..2)
        int pad = 0, k = 0;
        if (std::sscanf (id.c_str(), "b%d_%d", &pad, &k) == 2 && pad >= 1 && pad <= numPads && k >= 1 && k <= padInserts)
            return globalSlot (channel, padBypassBase + (pad - 1) * padInserts + (k - 1));
        return -1;
    }
    else if (area.rfind ("pfx", 0) == 0 && channel < numTracks)
    {
        // insert de pad : « pfx<pad>_<n> » (1..16, 1..2)
        int pad = 0, k = 0;
        if (std::sscanf (area.c_str(), "pfx%d_%d", &pad, &k) == 2 && pad >= 1 && pad <= numPads && k >= 1 && k <= padInserts)
            if (const auto& e = padEffects[(size_t) channel][(size_t) (pad - 1)][(size_t) (k - 1)]) { d = &e->desc(); b = padInsertSlot (pad - 1, k - 1); }
    }
    if (d == nullptr) return -1;
    const auto i = d->indexOf (id);
    return i < 0 ? -1 : globalSlot (channel, b + i);
}

//==============================================================================
// Modulations

int Engine::addModulation (int channel, int source, int target, float amount, double nowMs)
{
    if (channel < 0 || channel >= numChannels || source < 0 || source > numModSources || target < 0 || target >= modSourceBase) return -1;
    auto& mods = modAssigns[(size_t) channel];
    for (auto& m : mods)
        if (m.target == target)
        {
            m.source = source;
            setParam (globalSlot (channel, modAmountBase + m.amountIndex), (std::clamp (amount, -1.0f, 1.0f) + 1.0f) * 0.5f);
            publishGraph (nowMs);
            return m.amountIndex;
        }
    std::array<bool, numModAssigns> taken {};
    for (auto& m : mods) taken[(size_t) m.amountIndex] = true;
    int idx = -1;
    for (int i = 0; i < numModAssigns; ++i) if (! taken[(size_t) i]) { idx = i; break; }
    if (idx < 0) return -1;
    mods.push_back ({ source, target, idx });
    setParam (globalSlot (channel, modAmountBase + idx), (std::clamp (amount, -1.0f, 1.0f) + 1.0f) * 0.5f);
    publishGraph (nowMs);
    return idx;
}

void Engine::removeModulation (int channel, int target, double nowMs)
{
    if (channel < 0 || channel >= numChannels) return;
    auto& mods = modAssigns[(size_t) channel];
    mods.erase (std::remove_if (mods.begin(), mods.end(), [target] (const ModAssign& m) { return m.target == target; }), mods.end());
    publishGraph (nowMs);
}

std::vector<ModAssign> Engine::getModulations (int channel) const
{
    return channel >= 0 && channel < numChannels ? modAssigns[(size_t) channel] : std::vector<ModAssign>{};
}

//==============================================================================
// Scènes

void Engine::publishScenes (std::unique_ptr<SceneSet> s, double nowMs) { scenes.publish (std::move (s), bin, nowMs); sceneVersion.fetch_add (1); }

void Engine::replaceScenes (std::unique_ptr<SceneSet> s, double nowMs)
{
    if (s == nullptr) return;
    for (auto& v : s->scenes)
    {
        v.erase (std::remove_if (v.begin(), v.end(), [] (const SceneLock& l) { return l.slot < 0 || l.slot >= numSlots || ! std::isfinite (l.norm); }), v.end());
        for (auto& l : v) l.norm = std::clamp (l.norm, 0.0f, 1.0f);
        std::sort (v.begin(), v.end(), [] (const SceneLock& a, const SceneLock& b) { return a.slot < b.slot; });
        v.erase (std::unique (v.begin(), v.end(), [] (const SceneLock& a, const SceneLock& b) { return a.slot == b.slot; }), v.end());
    }
    publishScenes (std::move (s), nowMs);
}

static bool isEmptyExtra (const StepExtra& e);

void Engine::replaceExtras (std::unique_ptr<PatternExtras> e, double nowMs)
{
    if (e == nullptr) return;
    for (auto it = e->steps.begin(); it != e->steps.end();)
        it = isEmptyExtra (it->second) ? e->steps.erase (it) : std::next (it);
    publishExtras (std::move (e), nowMs);
}

void Engine::setSceneLock (int scene, int global, float norm, double nowMs)
{
    if (scene < 0 || scene > numScenes || global < 0 || global >= numSlots) return;
    auto s = std::make_unique<SceneSet> (*scenes.editable());
    auto& v = s->scenes[(size_t) scene];
    auto it = std::lower_bound (v.begin(), v.end(), global, [] (const SceneLock& l, int g) { return l.slot < g; });
    if (it != v.end() && it->slot == global) it->norm = std::clamp (norm, 0.0f, 1.0f);
    else v.insert (it, { global, std::clamp (norm, 0.0f, 1.0f) });
    publishScenes (std::move (s), nowMs);
}

void Engine::clearSceneLock (int scene, int global, double nowMs)
{
    if (scene < 0 || scene > numScenes) return;
    auto s = std::make_unique<SceneSet> (*scenes.editable());
    auto& v = s->scenes[(size_t) scene];
    v.erase (std::remove_if (v.begin(), v.end(), [global] (const SceneLock& l) { return l.slot == global; }), v.end());
    publishScenes (std::move (s), nowMs);
}

void Engine::clearScene (int scene, double nowMs)
{
    if (scene < 0 || scene > numScenes) return;
    auto s = std::make_unique<SceneSet> (*scenes.editable());
    s->scenes[(size_t) scene].clear();
    publishScenes (std::move (s), nowMs);
}

void Engine::setSceneMuted (int scene, bool muted, double nowMs)
{
    if (scene < 0 || scene > numScenes) return;
    auto s = std::make_unique<SceneSet> (*scenes.editable());
    s->muted[(size_t) scene] = muted;
    publishScenes (std::move (s), nowMs);
}

bool Engine::applyChain (int channel, const std::string& chainName, double nowMs)
{
    if (channel < 0 || channel >= numChannels) return false;
    for (const auto& c : chainCatalog())
    {
        if (c.name != chainName) continue;
        beginBatch();
        for (int s = 0; s < numInserts; ++s)
        {
            const auto has = s < (int) c.inserts.size();
            setInsert (channel, s, has ? c.inserts[(size_t) s].first : std::string(), nowMs);
            setParam (globalSlot (channel, mixBypass1 + s), 0.0f);
        }
        endBatch (nowMs);
        for (int s = 0; s < (int) c.inserts.size() && s < numInserts; ++s) applyPreset (channel, s, c.inserts[(size_t) s].second);
        return true;
    }
    return false;
}

//==============================================================================
// Conditions et verrous par pas

void Engine::publishExtras (std::unique_ptr<PatternExtras> e, double nowMs) { extras.publish (std::move (e), bin, nowMs); seq->bump(); }

/** Durée d'un pas d'arpège en noires (mêmes choix que synth::arpRates()). */
static double synth_arpRateQuarters (int i) noexcept
{
    static constexpr double q[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 0.75, 1.0 / 12.0 };
    return q[std::clamp (i, 0, 7)];
}

static bool isEmptyExtra (const StepExtra& e) { return e.cond == Condition::none && e.prob >= 100 && e.ratchet <= 1 && e.flam == 0 && ! e.direct && e.locks.empty(); }

void Engine::setStepExtra (int track, int lane, int step, Condition c, int a, int b, int prob, double nowMs)
{ setStepExtraP (track, seq->getEditing (track), lane, step, c, a, b, prob, nowMs); }
void Engine::setStepLock (int track, int lane, int step, int global, float norm, double nowMs)
{ setStepLockP (track, seq->getEditing (track), lane, step, global, norm, nowMs); }
void Engine::clearStepLock (int track, int lane, int step, int global, double nowMs)
{ clearStepLockP (track, seq->getEditing (track), lane, step, global, nowMs); }
void Engine::clearStepExtras (int track, int lane, int step, double nowMs)
{ clearStepExtrasP (track, seq->getEditing (track), lane, step, nowMs); }

static bool validStep (int track, int pattern, int lane, int step)
{
    return track >= 0 && track < numTracks && pattern >= 0 && pattern < Sequencer::numPatterns
        && lane >= 0 && lane < Sequencer::numLanes && step >= 0 && step < Sequencer::maxSteps;
}

void Engine::setStepExtraP (int track, int pattern, int lane, int step, Condition c, int a, int b, int prob, double nowMs)
{
    if (! validStep (track, pattern, lane, step)) return;
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    const auto k = PatternExtras::key (track, pattern, lane, step);
    auto& x = e->steps[k];
    x.cond = (Condition) std::clamp ((int) c, 0, numConditions - 1);
    x.a = (std::uint8_t) std::clamp (a, 1, 8);
    x.b = (std::uint8_t) std::clamp (b, 1, 8);
    x.prob = (std::uint8_t) std::clamp (prob, 0, 100);
    if (isEmptyExtra (x)) e->steps.erase (k);
    publishExtras (std::move (e), nowMs);
}

void Engine::setStepRatchetP (int track, int pattern, int lane, int step, int count, int slope, double nowMs)
{
    if (! validStep (track, pattern, lane, step)) return;
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    const auto k = PatternExtras::key (track, pattern, lane, step);
    auto& x = e->steps[k];
    x.ratchet = (std::uint8_t) std::clamp (count, 1, Sequencer::maxRatchet);
    x.ratchetSlope = (std::int8_t) std::clamp (slope, -100, 100);
    if (isEmptyExtra (x)) e->steps.erase (k);
    publishExtras (std::move (e), nowMs);
}

void Engine::setStepsRatchet (int track, const std::vector<std::pair<int, int>>& cells, int count, int slope, double nowMs)
{
    if (track < 0 || track >= numTracks) return;
    const auto p = seq->getEditing (track);
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    for (const auto& [l, st] : cells)
    {
        if (! validStep (track, p, l, st)) continue;
        const auto k = PatternExtras::key (track, p, l, st);
        auto& x = e->steps[k];
        x.ratchet = (std::uint8_t) std::clamp (count, 1, Sequencer::maxRatchet);
        x.ratchetSlope = (std::int8_t) std::clamp (slope, -100, 100);
        if (isEmptyExtra (x)) e->steps.erase (k);
    }
    publishExtras (std::move (e), nowMs);
}

void Engine::setStepsFlam (int track, int pattern, const std::vector<std::pair<int, int>>& cells, int ms, double nowMs)
{
    if (track < 0 || track >= numTracks) return;
    const auto p = pattern < 0 ? seq->getEditing (track) : pattern;
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    for (const auto& [l, st] : cells)
    {
        if (! validStep (track, p, l, st)) continue;
        const auto k = PatternExtras::key (track, p, l, st);
        auto& x = e->steps[k];
        x.flam = (std::uint8_t) std::clamp (ms, 0, maxFlamMs);
        if (isEmptyExtra (x)) e->steps.erase (k);
    }
    publishExtras (std::move (e), nowMs);
}

void Engine::setStepDirectP (int track, int pattern, int lane, int step, bool direct, double nowMs)
{
    if (! validStep (track, pattern, lane, step)) return;
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    const auto k = PatternExtras::key (track, pattern, lane, step);
    e->steps[k].direct = direct;
    if (isEmptyExtra (e->steps[k])) e->steps.erase (k);
    publishExtras (std::move (e), nowMs);
}

void Engine::setStepLockP (int track, int pattern, int lane, int step, int global, float norm, double nowMs)
{
    if (! validStep (track, pattern, lane, step)) return;
    if (global < 0 || global >= numSlots) return;
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    auto& locks = e->steps[PatternExtras::key (track, pattern, lane, step)].locks;
    for (auto& l : locks) if (l.first == global) { l.second = std::clamp (norm, 0.0f, 1.0f); publishExtras (std::move (e), nowMs); return; }
    if (locks.size() < 64) locks.push_back ({ global, std::clamp (norm, 0.0f, 1.0f) });
    publishExtras (std::move (e), nowMs);
}

void Engine::editCells (int track, const std::vector<std::pair<int, int>>& clear, const std::vector<CellWrite>& writes, double nowMs)
{
    if (track < 0 || track >= numTracks) return;
    const auto p = seq->getEditing (track);
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    bool extrasChanged = false;
    for (const auto& [l, s] : clear)
    {
        if (! validStep (track, p, l, s)) continue;
        seq->setStepP (track, p, l, s, 0);
        extrasChanged = e->steps.erase (PatternExtras::key (track, p, l, s)) > 0 || extrasChanged;
    }
    for (const auto& w : writes)
    {
        if (! validStep (track, p, w.lane, w.step)) continue;
        seq->setNoteP (track, p, w.lane, w.step, w.note);
        seq->setLength8P (track, p, w.lane, w.step, w.len8);
        seq->setMicroP (track, p, w.lane, w.step, w.micro);
        seq->setStepP (track, p, w.lane, w.step, w.vel);
        const auto k = PatternExtras::key (track, p, w.lane, w.step);
        extrasChanged = e->steps.erase (k) > 0 || extrasChanged;
        if (! w.hasExtra) continue;
        StepExtra x;
        x.cond = (Condition) std::clamp ((int) w.extra.cond, 0, numConditions - 1);
        x.a = (std::uint8_t) std::clamp ((int) w.extra.a, 1, 8);
        x.b = (std::uint8_t) std::clamp ((int) w.extra.b, 1, 8);
        x.prob = (std::uint8_t) std::clamp ((int) w.extra.prob, 0, 100);
        x.ratchet = (std::uint8_t) std::clamp ((int) w.extra.ratchet, 1, Sequencer::maxRatchet);
        x.ratchetSlope = (std::int8_t) std::clamp ((int) w.extra.ratchetSlope, -100, 100);
        x.flam = (std::uint8_t) std::clamp ((int) w.extra.flam, 0, maxFlamMs);
        x.direct = w.extra.direct;
        for (const auto& [g, v] : w.extra.locks)
            if (x.locks.size() < 64 && g >= 0 && g < numSlots && describe (g) != nullptr && std::isfinite (v))
                x.locks.push_back ({ g, std::clamp (v, 0.0f, 1.0f) });
        if (! isEmptyExtra (x)) { e->steps[k] = std::move (x); extrasChanged = true; }
    }
    if (extrasChanged) publishExtras (std::move (e), nowMs);
    else seq->bump();
}

void Engine::setStepLocks (int track, const std::vector<std::pair<int, int>>& cells, int global, float norm, bool clear, double nowMs)
{
    if (track < 0 || track >= numTracks || global < 0 || global >= numSlots || describe (global) == nullptr) return;
    const auto p = seq->getEditing (track);
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    for (const auto& [l, s] : cells)
    {
        if (! validStep (track, p, l, s)) continue;
        const auto k = PatternExtras::key (track, p, l, s);
        if (clear)
        {
            auto it = e->steps.find (k);
            if (it == e->steps.end()) continue;
            auto& locks = it->second.locks;
            locks.erase (std::remove_if (locks.begin(), locks.end(), [global] (auto& x) { return x.first == global; }), locks.end());
            if (isEmptyExtra (it->second)) e->steps.erase (it);
            continue;
        }
        auto& locks = e->steps[k].locks;
        bool found = false;
        for (auto& x : locks) if (x.first == global) { x.second = std::clamp (norm, 0.0f, 1.0f); found = true; }
        if (! found && locks.size() < 64) locks.push_back ({ global, std::clamp (norm, 0.0f, 1.0f) });
    }
    publishExtras (std::move (e), nowMs);
}

void Engine::clearStepLockP (int track, int pattern, int lane, int step, int global, double nowMs)
{
    if (! validStep (track, pattern, lane, step)) return;
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    const auto k = PatternExtras::key (track, pattern, lane, step);
    auto it = e->steps.find (k);
    if (it == e->steps.end()) return;
    auto& locks = it->second.locks;
    locks.erase (std::remove_if (locks.begin(), locks.end(), [global] (auto& l) { return l.first == global; }), locks.end());
    if (isEmptyExtra (it->second)) e->steps.erase (it);
    publishExtras (std::move (e), nowMs);
}

void Engine::clearStepExtrasP (int track, int pattern, int lane, int step, double nowMs)
{
    if (! validStep (track, pattern, lane, step)) return;
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    e->steps.erase (PatternExtras::key (track, pattern, lane, step));
    publishExtras (std::move (e), nowMs);
}

void Engine::removeStep (int track, int lane, int step, double nowMs)
{
    if (track < 0 || track >= numTracks) return;
    const auto p = seq->getEditing (track);
    if (! validStep (track, p, lane, step)) return;
    seq->setStepP (track, p, lane, step, 0);
    seq->setLength8P (track, p, lane, step, 8);
    seq->setMicroP (track, p, lane, step, 0);
    bool empty = true;
    for (int l = 0; l < Sequencer::numLanes && empty; ++l)
        if (seq->getStepP (track, p, l, step) > 0) empty = false;
    if (empty)
    {
        seq->setStepFlagsP (track, p, step, seq->getStepFlagsP (track, p, step) & stepRec); // le départ de l'enregistreur reste
        seq->setStepAmountsP (track, p, step, 0);
    }
    clearStepExtrasP (track, p, lane, step, nowMs);
}

void Engine::clearTrackExtras (int track, double nowMs)
{
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    for (auto it = e->steps.begin(); it != e->steps.end();)
        it = (PatternExtras::keyTrack (it->first) == track) ? e->steps.erase (it) : std::next (it);
    publishExtras (std::move (e), nowMs);
}

void Engine::clearPatternExtras (int track, int pattern, double nowMs)
{
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    for (auto it = e->steps.begin(); it != e->steps.end();)
        it = (PatternExtras::keyTrack (it->first) == track && PatternExtras::keyPattern (it->first) == pattern) ? e->steps.erase (it) : std::next (it);
    publishExtras (std::move (e), nowMs);
}

void Engine::copyStepRangeExtras (int track, int pattern, int from, int to, int count, double nowMs)
{
    if (track < 0 || track >= numTracks || count <= 0 || from == to) return;
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    const auto inDst = [&] (std::uint32_t k)
    {
        const auto s = PatternExtras::keyStep (k);
        return PatternExtras::keyTrack (k) == track && PatternExtras::keyPattern (k) == pattern && s >= to && s < to + count;
    };
    std::vector<std::pair<std::uint32_t, StepExtra>> copies;
    for (const auto& [k, x] : e->steps)
    {
        const auto s = PatternExtras::keyStep (k);
        if (PatternExtras::keyTrack (k) == track && PatternExtras::keyPattern (k) == pattern && s >= from && s < from + count
            && to + (s - from) < Sequencer::maxSteps && ! isEmptyExtra (x))
            copies.push_back ({ PatternExtras::key (track, pattern, PatternExtras::keyLane (k), to + (s - from)), x });
    }
    for (auto it = e->steps.begin(); it != e->steps.end();) it = inDst (it->first) ? e->steps.erase (it) : std::next (it);
    for (auto& c : copies) e->steps[c.first] = std::move (c.second);
    publishExtras (std::move (e), nowMs);
}

void Engine::shiftPatternExtras (int track, int pattern, int lane, int dir, double nowMs)
{
    if (track < 0 || track >= numTracks || dir == 0) return;
    const auto& sq = sequencer();
    const int length = std::clamp (sq.getPatternLength (track, pattern), 1, Sequencer::maxSteps);
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    std::vector<std::pair<std::uint32_t, StepExtra>> moved;
    for (auto it = e->steps.begin(); it != e->steps.end();)
    {
        const auto k = it->first;
        const auto l = PatternExtras::keyLane (k), st = PatternExtras::keyStep (k);
        const auto own = sq.getLaneLengthP (track, pattern, l);
        const int n = own > 0 ? std::min (own, Sequencer::maxSteps) : length;
        if (PatternExtras::keyTrack (k) == track && PatternExtras::keyPattern (k) == pattern && (lane < 0 || l == lane) && st < n)
        {
            const int to = (st + ((dir % n) + n) % n) % n;
            moved.push_back ({ PatternExtras::key (track, pattern, l, to), std::move (it->second) });
            it = e->steps.erase (it);
        }
        else ++it;
    }
    for (auto& m : moved) e->steps[m.first] = std::move (m.second);
    publishExtras (std::move (e), nowMs);
}

void Engine::copyPatternExtras (int srcTrack, int from, int track, int to, double nowMs)
{
    if ((srcTrack == track && from == to) || srcTrack < 0 || srcTrack >= numTracks || track < 0 || track >= numTracks) return;
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    std::vector<std::pair<std::uint32_t, StepExtra>> copies;
    for (const auto& [k, x] : e->steps)
        if (PatternExtras::keyTrack (k) == srcTrack && PatternExtras::keyPattern (k) == from)
        {
            auto y = x;
            if (srcTrack != track)
            {
                // verrous de la piste source : vers le même réglage de la piste de destination
                std::vector<std::pair<int, float>> kept;
                for (const auto& [g, v] : y.locks)
                {
                    if (channelOf (g) != srcTrack) { kept.push_back ({ g, v }); continue; }
                    const auto ng = globalSlot (track, g % slotsPerChannel);
                    const auto* a = describe (g); const auto* b = describe (ng);
                    if (a != nullptr && b != nullptr && a->id == b->id) kept.push_back ({ ng, v });
                }
                y.locks = std::move (kept);
            }
            if (! isEmptyExtra (y)) copies.push_back ({ PatternExtras::key (track, to, PatternExtras::keyLane (k), PatternExtras::keyStep (k)), std::move (y) });
        }
    for (auto it = e->steps.begin(); it != e->steps.end();)
        it = (PatternExtras::keyTrack (it->first) == track && PatternExtras::keyPattern (it->first) == to) ? e->steps.erase (it) : std::next (it);
    for (auto& c : copies) e->steps[c.first] = std::move (c.second);
    publishExtras (std::move (e), nowMs);
}

//==============================================================================
// Échantillons

void Engine::setSample (int track, int index, std::unique_ptr<SampleBuffer> sample, double nowMs)
{
    if (track < 0 || track >= numTracks || index < 0 || index >= maxSamples) return;
    auto& slot = samples[(size_t) track][(size_t) index];
    // L'ancien échantillon reste valide le temps que le fil audio voie le nouveau graphe
    bin.retire (std::move (slot), nowMs);
    slot = std::move (sample);
    publishGraph (nowMs);
}

void Engine::setTrackSpeech (int track, const std::string& text, double nowMs)
{
    if (track < 0 || track >= numTracks) return;
    // 160 octets au plus (plus « @en » pour l'anglais, v0.39), sans couper un caractère UTF-8 en deux
    auto t = text;
    if (t.size() > 164) { size_t cut = 164; while (cut > 0 && ((unsigned char) t[cut] & 0xC0) == 0x80) --cut; t.resize (cut); }
    if (info[(size_t) track].speech == t) return;
    info[(size_t) track].speech = t;
    auto& slot = speechScripts[(size_t) track];
    bin.retire (std::move (slot), nowMs); // l'ancienne phrase reste valide tant que le fil audio peut la lire
    slot = t.empty() ? nullptr : compileSpeech (t);
    publishGraph (nowMs);
}

const SampleBuffer* Engine::getSample (int track, int index) const noexcept
{
    if (track < 0 || track >= numTracks || index < 0 || index >= maxSamples) return nullptr;
    return samples[(size_t) track][(size_t) index].get();
}

float Engine::getEffectMeter (int channel, int slot, int index) const noexcept
{
    if (channel < 0 || channel >= numChannels || slot < 0 || slot >= numInserts) return 0.0f;
    const auto& e = effects[(size_t) channel][(size_t) slot];
    return e ? e->meter (index) : 0.0f;
}

//==============================================================================
// Fil audio

void Engine::touch (ChannelState& c, int local, float norm) noexcept
{
    if (! c.touched[(size_t) local])
    {
        c.touched[(size_t) local] = 1;
        c.touchedList[(size_t) c.numTouched++] = local;
    }
    c.overrideNorm[(size_t) local] = std::clamp (norm, 0.0f, 1.0f);
}

void Engine::updateParams (const Graph& g, const SceneSet& s, const BlockInfo& bi, float fader, int n, double ppq) noexcept
{
    auto& st = *state;
    const auto dt = (double) n / sampleRate;

    // 1. Valeurs de base (recalculées seulement si un réglage a changé)
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto& cg = g.ch[(size_t) ch];
        if (! cg.used) continue;
        auto& c = st[(size_t) ch];
        const auto ver = paramVersion[(size_t) ch].load (std::memory_order_acquire);
        if (ver != c.seenVersion || cg.instrument != c.seenInstrument)
        {
            if (cg.instrument != c.seenInstrument) { c.numPlocks.fill (0); c.anyPlocks = 0; }
            c.seenVersion = ver;
            c.seenInstrument = cg.instrument;
            for (int r = 0; r < cg.numRanges; ++r)
                for (int i = cg.ranges[(size_t) r].first; i < cg.ranges[(size_t) r].second; ++i)
                    if (const auto* d = cg.desc[(size_t) i])
                        c.plain[(size_t) i] = d->toPlain (base[(size_t) globalSlot (ch, i)].load (std::memory_order_relaxed));
        }
        for (int r = 0; r < cg.numRanges; ++r)
        {
            const auto [a, b] = cg.ranges[(size_t) r];
            std::memcpy (c.eff.data() + a, c.plain.data() + a, sizeof (float) * (size_t) (b - a));
        }
        // le précédent jeu de réglages modifiés sert à rétablir l'affichage
        std::memcpy (c.prevTouched.data(), c.touchedList.data(), sizeof (int) * (size_t) c.numTouched);
        c.numPrevTouched = c.numTouched;
        for (int i = 0; i < c.numTouched; ++i) c.touched[(size_t) c.touchedList[(size_t) i]] = 0;
        c.numTouched = 0;
    }

    // valeur de départ d'un réglage : sa courbe d'automatisation si elle joue, sinon sa valeur de base
    auto baseNorm = [this, &st] (int global)
    {
        const auto& c = st[(size_t) channelOf (global)];
        const auto local = (size_t) localOf (global);
        return c.touched[local] ? c.overrideNorm[local] : base[(size_t) global].load (std::memory_order_relaxed);
    };
    auto apply = [&] (int global, float norm)
    {
        const auto ch = channelOf (global), local = localOf (global);
        if (ch < 0 || ch >= numChannels || ! g.ch[(size_t) ch].used || g.ch[(size_t) ch].desc[(size_t) local] == nullptr) return;
        if (g.ch[(size_t) ch].desc[(size_t) local]->fixed) return; // après la 1.1 : réglage figé (numéro de CC, canal)
        touch (st[(size_t) ch], local, norm);
    };

    // 1b. Courbes d'automatisation du morceau (avant les scènes, les verrous et les modulations,
    //     comme si le bouton bougeait) ; un réglage tenu à la main n'est pas relu
    // position de chaque piste dans son pattern (courbes de pattern, enregistrement à l'écran)
    for (int t = 0; t < numTracks; ++t) trackPhase[(size_t) t].store ((float) seq->stepPhase (t, ppq), std::memory_order_relaxed);
    if (const auto* sg = song.get(); sg != nullptr && seq->isPlayingInBlock())
    {
        if (sg->autoRead && ! sg->lanes.empty())
        {
            const auto beat = sg->wrap (ppq);
            for (const auto& lane : sg->lanes)
            {
                if (! lane.enabled || lane.pts.empty() || isHeld (lane.global)) continue;
                if (isSwitchLane (lane.global))
                {
                    // v0.46 : coupure / solo de piste relus (un changement seulement si l'état diffère)
                    if (const auto v = lane.valueAt (beat); v >= 0.0f)
                    {
                        const auto t = switchTrack (lane.global);
                        const bool on = v >= 0.5f;
                        if (switchIsSolo (lane.global)) { if (seq->getTrackSolo (t) != on) seq->setTrackSolo (t, on); }
                        else if (seq->getTrackMute (t) != on) seq->setTrackMute (t, on);
                    }
                    continue;
                }
                if (isBusSwitchLane (lane.global))
                {
                    // v0.46 fix7 : coupure / solo des retours et des groupes relus
                    if (const auto v = lane.valueAt (beat); v >= 0.0f)
                    {
                        const auto ch = busSwitchChannel (lane.global);
                        const bool on = v >= 0.5f;
                        if (busSwitchIsSolo (lane.global)) { if (getBusSolo (ch) != on) setBusSolo (ch, on); }
                        else if (getBusMute (ch) != on) setBusMute (ch, on);
                    }
                    continue;
                }
                if (lane.global >= 0 && lane.global < numSlots)
                    if (const auto v = lane.valueAt (beat); v >= 0.0f) apply (lane.global, v);
            }
        }
        // courbes des patterns joués (après celles du morceau : le pattern a le dernier mot)
        for (const auto& pl : sg->patLanes)
        {
            const auto& lane = pl.lane;
            if (! lane.enabled || lane.pts.empty() || lane.global < 0 || lane.global >= numSlots) continue;
            if (seq->getPlaying (pl.track) != pl.pattern || isHeld (lane.global)) continue;
            const auto ph = (double) trackPhase[(size_t) pl.track].load (std::memory_order_relaxed);
            if (ph >= 0.0) if (const auto v = lane.valueAt (ph); v >= 0.0f) apply (lane.global, v);
        }
    }

    // 2. Scènes A/B au crossfader — ou, pavé vectoriel en mode scènes (v0.46 fix22), quatre scènes
    //    mêlées selon les poids des coins : valeur = somme des poids × (verrou de la scène, ou base)
    static const std::vector<SceneLock> empty;
    if (vecScenesOn)
    {
        std::array<const std::vector<SceneLock>*, 4> lists {};
        std::array<size_t, 4> idx {};
        for (size_t k = 0; k < 4; ++k)
        {
            const auto sc = std::clamp (bi.vecScene[k], 0, numScenes - 1);
            lists[k] = s.muted[(size_t) sc] ? &empty : &s.scenes[(size_t) sc];
        }
        for (;;)
        {
            int slot = std::numeric_limits<int>::max();
            for (size_t k = 0; k < 4; ++k)
                if (idx[k] < lists[k]->size()) slot = std::min (slot, (*lists[k])[idx[k]].slot);
            if (slot == std::numeric_limits<int>::max()) break;
            const auto base = baseNorm (slot);
            const auto* d = g.ch[(size_t) channelOf (slot)].desc[(size_t) localOf (slot)];
            const bool stepped = d != nullptr && d->stepped();
            float v = 0.0f, best = -1.0f, bestV = base;
            for (size_t k = 0; k < 4; ++k)
            {
                float nk = base;
                if (idx[k] < lists[k]->size() && (*lists[k])[idx[k]].slot == slot) { nk = (*lists[k])[idx[k]].norm; ++idx[k]; }
                v += smoothedVec[k] * nk;
                if (smoothedVec[k] > best) { best = smoothedVec[k]; bestV = nk; }
            }
            apply (slot, stepped ? bestV : v);
        }
    }
    else
    {
    const auto sa = std::clamp (bi.sceneA, 0, numScenes - 1), sb = std::clamp (bi.sceneB, 0, numScenes - 1);
    const auto& la = s.muted[(size_t) sa] ? empty : s.scenes[(size_t) sa];
    const auto& lb = s.muted[(size_t) sb] ? empty : s.scenes[(size_t) sb];
    size_t ia = 0, ib = 0;
    while (ia < la.size() || ib < lb.size())
    {
        int slot;
        float na, nb;
        if (ib >= lb.size() || (ia < la.size() && la[ia].slot < lb[ib].slot)) { slot = la[ia].slot; na = la[ia].norm; nb = baseNorm (slot); ++ia; }
        else if (ia >= la.size() || lb[ib].slot < la[ia].slot) { slot = lb[ib].slot; nb = lb[ib].norm; na = baseNorm (slot); ++ib; }
        else { slot = la[ia].slot; na = la[ia].norm; nb = lb[ib].norm; ++ia; ++ib; }
        const auto* d = g.ch[(size_t) channelOf (slot)].desc[(size_t) localOf (slot)];
        const auto v = (d != nullptr && d->stepped()) ? (fader < 0.5f ? na : nb) : na + (nb - na) * fader;
        apply (slot, v);
    }
    }

    // 3. Break : sa scène s'applique d'un coup tant qu'il est enfoncé
    if (bi.breakActive && ! s.muted[breakScene])
        for (const auto& l : s.scenes[breakScene]) apply (l.slot, l.norm);

    // 4. Verrous du pas en cours
    const auto* pool = soundPool.get();
    for (int ch = 0; ch < numTracks; ++ch)
    {
        auto& c = st[(size_t) ch];
        // après la 1.1 : son du pas (numéro de préréglage verrouillé sur le pas ou tenu par une scène) :
        // tous les réglages du préréglage, sous les verrous du pas (posés ensuite, ils ont le dernier mot)
        if (pool != nullptr && ! pool->entries.empty() && g.ch[(size_t) ch].instrument != nullptr)
        {
            const auto sg = globalSlot (ch, trackSoundSlot);
            float sn = c.touched[(size_t) trackSoundSlot] ? c.overrideNorm[(size_t) trackSoundSlot] : -1.0f;
            for (int lane = 0; lane < 16; ++lane)
                for (int i = 0; i < c.numPlocks[(size_t) lane]; ++i)
                    if (c.plocks[(size_t) lane][(size_t) i].slot == sg) sn = c.plocks[(size_t) lane][(size_t) i].norm;
            const int num = sn > 0.0f ? (int) std::lround (sn * trackSoundMax) : 0;
            if (num > 0)
            {
                const auto* d = &g.ch[(size_t) ch].instrument->desc();
                for (const auto& e : pool->entries)
                    if (e.track == ch && e.number == num && e.desc == d)
                    {
                        for (const auto& [local, norm] : e.values) apply (globalSlot (ch, instrumentBase + local), norm);
                        break;
                    }
            }
        }
        if (c.anyPlocks == 0) continue;
        for (int lane = 0; lane < 16; ++lane)
        {
            // après la 1.1 : un réglage maintenu (pad) verrouillé n'est appuyé que le temps de la note du pas,
            // puis relâché : chaque passage du pas donne un appui et un relâchement
            const bool momLive = c.momentaryLeft[(size_t) lane] > 0.0;
            if (momLive) c.momentaryLeft[(size_t) lane] -= n;
            auto momentaryOff = [&] (int slot)
            {
                if (momLive) return false;
                const auto pc = channelOf (slot);
                if (pc < 0 || pc >= numChannels) return false;
                const auto* d = g.ch[(size_t) pc].desc[(size_t) localOf (slot)];
                return d != nullptr && d->momentary;
            };
            const auto sl = c.slideLen[(size_t) lane];
            if (sl > 0.0 && c.slidePos[(size_t) lane] < sl)
            {
                // glissé : de la valeur du moment à celle du verrou, sur la durée du pas
                const auto t = (float) std::min (1.0, (c.slidePos[(size_t) lane] + n) / sl);
                c.slidePos[(size_t) lane] += n;
                for (int i = 0; i < c.numPlocks[(size_t) lane]; ++i)
                {
                    const auto from = c.plockFrom[(size_t) lane][(size_t) i];
                    const auto& pl = c.plocks[(size_t) lane][(size_t) i];
                    const auto pc = channelOf (pl.slot);
                    if (pc < 0 || pc >= numChannels || momentaryOff (pl.slot)) continue;
                    const auto* d = g.ch[(size_t) pc].desc[(size_t) localOf (pl.slot)];
                    apply (pl.slot, (d != nullptr && d->stepped()) ? (t < 1.0f ? from : pl.norm) : from + (pl.norm - from) * t);
                }
                continue;
            }
            for (int i = 0; i < c.numPlocks[(size_t) lane]; ++i)
                if (! momentaryOff (c.plocks[(size_t) lane][(size_t) i].slot))
                    apply (c.plocks[(size_t) lane][(size_t) i].slot, c.plocks[(size_t) lane][(size_t) i].norm);
        }
    }

    // 5. Conversion des réglages modifiés, sources de modulation, puis modulations
    const bool playing = seq->isPlayingInBlock();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto& cg = g.ch[(size_t) ch];
        if (! cg.used) continue;
        auto& c = st[(size_t) ch];
        for (int i = 0; i < c.numTouched; ++i)
        {
            const auto local = c.touchedList[(size_t) i];
            c.eff[(size_t) local] = cg.desc[(size_t) local]->toPlain (c.overrideNorm[(size_t) local]);
        }

        // les sources tournent même sans destination : l'interface montre leur valeur et leur pas
        for (int m = 0; m < numModSources; ++m)
        {
            const auto* mp = &c.eff[(size_t) (modSourceBase + m * modSourceSlots)];
            auto level = c.level;
            // suiveur en side-chain (v0.23) : niveau d'une autre piste
            if ((int) mp[msType] == 2)
                if (const auto fs = (int) mp[msFollowSrc]; fs >= 1 && fs <= numTracks) level = st[(size_t) (fs - 1)].level;
            c.modValues[(size_t) m] = c.mods[(size_t) m].advance (mp, dt, bi.bpm, ppq, playing, level);
            modLive[(size_t) ch][(size_t) m].store (c.modValues[(size_t) m], std::memory_order_relaxed);
            modLiveStep[(size_t) ch][(size_t) m].store (c.mods[(size_t) m].step(), std::memory_order_relaxed);
        }
        c.modValues[numModSources] = c.lastVelocity;
        if (cg.mods.empty()) continue;

        for (const auto& ma : cg.mods)
        {
            if (ma.target < 0 || ma.target >= slotsPerChannel || cg.desc[(size_t) ma.target] == nullptr || cg.desc[(size_t) ma.target]->fixed || cg.desc[(size_t) ma.target]->noMod) continue;
            const auto amount = c.eff[(size_t) (modAmountBase + ma.amountIndex)] / 100.0f;
            if (amount == 0.0f) continue;
            const auto src = c.modValues[(size_t) std::clamp (ma.source, 0, numModSources)];
            const auto n0 = c.touched[(size_t) ma.target] ? c.overrideNorm[(size_t) ma.target] : baseNorm (globalSlot (ch, ma.target));
            touch (c, ma.target, n0 + amount * src);
            c.eff[(size_t) ma.target] = cg.desc[(size_t) ma.target]->toPlain (c.overrideNorm[(size_t) ma.target]);
        }
    }

    // 6. Valeurs jouées visibles par l'interface
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (! g.ch[(size_t) ch].used) continue;
        auto& c = st[(size_t) ch];
        for (int i = 0; i < c.numPrevTouched; ++i)
        {
            const auto local = c.prevTouched[(size_t) i];
            if (! c.touched[(size_t) local])
                effective[(size_t) globalSlot (ch, local)].store (baseNorm (globalSlot (ch, local)), std::memory_order_relaxed);
        }
        for (int i = 0; i < c.numTouched; ++i)
        {
            const auto local = c.touchedList[(size_t) i];
            effective[(size_t) globalSlot (ch, local)].store (c.overrideNorm[(size_t) local], std::memory_order_relaxed);
        }
    }
}

void Engine::setNoteDown (int track, int note, bool down) noexcept
{
    if (track < 0 || track >= numTracks || note < 0 || note > 127) return;
    auto& w = noteDown[(size_t) track][(size_t) (note >> 6)];
    const auto bit = std::uint64_t { 1 } << (note & 63);
    const auto v = w.load (std::memory_order_relaxed); // seul le fil audio écrit
    w.store (down ? (v | bit) : (v & ~bit), std::memory_order_relaxed);
}

void Engine::play (const Graph& g, int track, int note, float velocity, bool on) noexcept
{
    auto* inst = g.ch[(size_t) track].instrument;
    if (inst == nullptr) return;
    auto& c = (*state)[(size_t) track];
    const bool drum = inst->desc().drumKit;
    if (on)
    {
        inst->noteOn (note, velocity);
        c.lastVelocity = velocity;
        for (auto& m : c.mods) m.trigger();
        // suiveurs en side-chain sur les notes de cette piste (v0.23), sur tous les canaux
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (! g.ch[(size_t) ch].used) continue;
            auto& cc = (*state)[(size_t) ch];
            for (int m = 0; m < numModSources; ++m)
            {
                const auto* mp = &cc.eff[(size_t) (modSourceBase + m * modSourceSlots)];
                if ((int) mp[msType] == 2 && (int) mp[msFollowSrc] == numTracks + 1 + track) cc.mods[(size_t) m].sideNote (velocity);
            }
        }
        // GOLEM 1.0 : sur le FM Player, toutes les notes (même 0 à 15) sont des touches de la piste T jouée
        const int lowKey = inst->desc().laneLowKeys ? 0 : 16;
        int lane = drum ? (note < lowKey ? std::max (0, note) : note >= 128 ? std::clamp ((note - 128) / 128, 0, 15)
                                               : std::clamp (playLane[(size_t) track].load (std::memory_order_relaxed), 0, 15)) : ((note % 16) + 16) % 16;
        int stepNote = note;
        if (drum && note >= lowKey && note < 128 && inst->mapNote (note, lane, stepNote)) lane = std::clamp (lane, 0, 15); // v0.43 : zones du clavier
        hits[(size_t) track][(size_t) lane].fetch_add (1, std::memory_order_relaxed);
        const bool keys = ! drum || (note >= lowKey && note < 128 && inst->desc().laneNotes); // v0.43 : touches jouées sur une Pieuvre
        if (keys && note >= 0 && note < 128)
        {
            noteHits[(size_t) track][(size_t) note].fetch_add (1, std::memory_order_relaxed);
            setNoteDown (track, note, true);
        }
    }
    else
    {
        inst->noteOff (note);
        if (! drum || (note >= (inst->desc().laneLowKeys ? 0 : 16) && note < 128 && inst->desc().laneNotes)) setNoteDown (track, note, false);
    }
}

bool Engine::arpActive (const Graph& g, int track) const noexcept
{
    if (track < 0 || track >= numTracks) return false;
    const auto& cg = g.ch[(size_t) track];
    if (cg.instrument == nullptr || cg.arpParam < 0) return false;
    const auto* d = cg.desc[(size_t) (instrumentBase + cg.arpParam)];
    if (cg.arpOnParam >= 0)
        if (const auto* o = cg.desc[(size_t) (instrumentBase + cg.arpOnParam)];
            o != nullptr && o->toPlain (base[(size_t) globalSlot (track, instrumentBase + cg.arpOnParam)].load (std::memory_order_relaxed)) < 0.5f)
            return false;
    return d != nullptr && d->toPlain (base[(size_t) globalSlot (track, instrumentBase + cg.arpParam)].load (std::memory_order_relaxed)) >= 0.5f;
}

void Engine::dispatch (const Graph& g, int track, int note, float velocity, bool on, bool direct) noexcept
{
    if (track < 0 || track >= numTracks) return;
    // v0.46 fix8 : en mode montage, les clips jouent à la place des instruments : aucune note
    // nouvelle (les sons en cours finissent normalement, rien ne reste accroché)
    if (on)
        if (const auto* mf = montageFeed.load (std::memory_order_relaxed); mf != nullptr && mf->montageActive()) return;
    auto* inst = g.ch[(size_t) track].instrument;
    if (inst == nullptr) return;
    auto& c = (*state)[(size_t) track];
    const auto n = (size_t) std::clamp (note, 0, ChannelState::noteSlots - 1); // GOLEM 1.0 : notes d'une T comprises
    const auto& cg = g.ch[(size_t) track];
    if (c.arpOwner != inst)
    {
        // instrument changé : l'arpège repart de zéro (l'ancien instrument n'est plus joué)
        c.arp.clear();
        c.arpOwner = inst;
        c.arpOn = false;
        c.direct.fill (false); c.sustained.fill (false);
        noteDown[(size_t) track][0].store (0, std::memory_order_relaxed);
        noteDown[(size_t) track][1].store (0, std::memory_order_relaxed);
    }
    if (on)
    {
        c.sustained[n] = false;
        // arpège en marche : d'après la valeur de base (les valeurs jouées du bloc ne sont pas encore calculées).
        // Une note d'arpège enregistrée (direct) est rejouée telle quelle, sans repasser par lui.
        const bool arp = ! direct && arpActive (g, track);
        if (arp)
        {
            const bool hold = cg.desc[(size_t) (instrumentBase + cg.arpParam + 4)] != nullptr
                && cg.desc[(size_t) (instrumentBase + cg.arpParam + 4)]->toPlain (base[(size_t) globalSlot (track, instrumentBase + cg.arpParam + 4)].load (std::memory_order_relaxed)) > 0.5f;
            c.arp.noteOn (note, velocity, hold);
            c.lastVelocity = velocity;
            return;
        }
        c.direct[n] = true;
        play (g, track, note, velocity, true);
        return;
    }
    // fin de note : la pédale de maintien la garde ; sinon elle va là où la note est partie
    if (c.sustainDown) { c.sustained[n] = true; return; }
    if (c.direct[n]) { c.direct[n] = false; play (g, track, note, 0.0f, false); return; }
    c.arp.noteOff (note);
}

void Engine::expression (const Graph& g, int track, int kind, float value) noexcept
{
    if (track < 0 || track >= numTracks) return;
    auto& c = (*state)[(size_t) track];
    if (! std::isfinite (value)) return;
    switch (kind)
    {
        case InputEvent::pitchBend: c.bend = std::clamp (value, -1.0f, 1.0f); bendShown[(size_t) track].store (c.bend, std::memory_order_relaxed); break;
        case InputEvent::modWheel:  c.wheel = std::clamp (value, 0.0f, 1.0f); wheelShown[(size_t) track].store (c.wheel, std::memory_order_relaxed); break;
        case InputEvent::pressure:  c.pressure = std::clamp (value, 0.0f, 1.0f); break;
        case InputEvent::sustain:
        {
            const bool down = value >= 0.5f;
            if (c.sustainDown && ! down)
            {
                c.sustainDown = false;
                for (int k = 0; k < ChannelState::noteSlots; ++k)
                    if (c.sustained[(size_t) k]) { c.sustained[(size_t) k] = false; dispatch (g, track, k, 0.0f, false); }
            }
            c.sustainDown = down;
            break;
        }
        default: break;
    }
}

void Engine::runArpeggiators (const Graph& g, double ppq, bool playing, double bpm, int n, int pos) noexcept
{
    const auto pps = std::max (1.0, bpm) / 60.0 / sampleRate;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& cg = g.ch[(size_t) t];
        auto& c = (*state)[(size_t) t];
        auto* inst = cg.instrument;
        // ENR. avec arpège : ses notes s'écrivent dans le pattern telles qu'on les entend (durée comprise)
        const bool rec = arpRecording[(size_t) t] && seq->isRecording();
        auto off = [&] (int note) { play (g, t, note, 0.0f, false); if (rec) seq->recordRelease (t, note, pos); };
        auto onNote = [&] (int note, float vel)
        {
            play (g, t, note, vel, true);
            if (! rec) return;
            int pp = 0, ps = 0;
            const auto lane = seq->recordHit (t, 0, note, vel, pos, true, &pp, &ps);
            if (lane >= 0) arpMarks->push ({ t, pp, lane, ps });
        };
        if (c.arpOwner != inst)
        {
            // instrument changé : l'arpège repart de zéro (l'ancien instrument n'est plus joué)
            c.arp.clear();
            c.arpOwner = inst;
            c.direct.fill (false); c.sustained.fill (false);
            noteDown[(size_t) t][0].store (0, std::memory_order_relaxed);
            noteDown[(size_t) t][1].store (0, std::memory_order_relaxed);
        }
        const bool on = inst != nullptr && cg.arpParam >= 0 && c.eff[(size_t) (instrumentBase + cg.arpParam)] >= 0.5f
                        && (cg.arpOnParam < 0 || c.eff[(size_t) (instrumentBase + cg.arpOnParam)] >= 0.5f);
        if (! on)
        {
            if (c.arpOn) c.arp.flush (off);
            c.arpOn = false;
            continue;
        }
        c.arpOn = true;
        const auto* p = c.eff.data() + instrumentBase + cg.arpParam;
        Arpeggiator::Settings s;
        s.mode = (int) p[0];
        s.octaves = (int) std::lround (p[1]);
        s.stepQuarters = synth_arpRateQuarters ((int) p[2]);
        s.gate = p[3] / 100.0;
        s.hold = p[4] > 0.5f;
        s.swing = p[5] / 100.0;
        // arpège à pas (si l'instrument en a) : arp_steps, puis 16 transpositions, 16 vélocités
        if (cg.arpParam + 6 < instrumentSlots && cg.desc[(size_t) (instrumentBase + cg.arpParam + 6)] != nullptr
            && cg.desc[(size_t) (instrumentBase + cg.arpParam + 6)]->id == "arp_steps")
        {
            s.steps = (int) std::lround (p[6]);
            s.transpose = p + 7;
            s.velocity = p + 23;
        }
        c.arp.process (s, ppq, playing, pps, n, onNote, off);
    }
}

void Engine::renderChunk (const Graph& g, float* outL, float* outR, int n, double bpm, double ppq, bool playing) noexcept
{
    auto& st = *state;
    // v0.49 : plus grand retard d'instrument de ce morceau (FM Player)
    {
        // 1.1 (lot 6) : + latence des inserts actifs de chaque piste (outils spectraux en direct)
        auto insertLat = [&] (int ch)
        {
            int l = 0;
            const auto& cg = g.ch[(size_t) ch];
            const auto* p = st[(size_t) ch].eff.data();
            for (int i = 0; i < numInserts; ++i)
                if (auto* fx = cg.inserts[(size_t) i]; fx != nullptr && p[mixBypass1 + i] < 0.5f) l += std::max (0, fx->latency());
            return l;
        };
        int d = 0, ins = 0;
        for (int t = 0; t < numTracks; ++t)
        {
            trackLat[(size_t) t] = 0;
            if (const auto* ci = g.ch[(size_t) t].instrument; ci != nullptr && g.ch[(size_t) t].used)
            {
                const auto il = insertLat (t);
                ins = std::max (ins, il);
                trackLat[(size_t) t] = std::max (0, ci->latencySamples()) + il;
                d = std::max (d, trackLat[(size_t) t]);
            }
        }
        const auto cap = compL[0].empty() ? 0 : (int) compL[0].size() - controlBlock - 1;
        compDelay = std::clamp (d, 0, std::max (0, cap));
        const auto masterLat = insertLat (masterChannel);
        // groupes et retours (montage : les retours sont déjà enregistrés, sans effet)
        const auto* mfeed = montageFeed.load (std::memory_order_acquire);
        const bool montageNow = mfeed != nullptr && mfeed->montageActive();
        int bd = 0;
        for (int k = 0; k < numGroups; ++k) { groupLat[(size_t) k] = insertLat (groupBase + k); bd = std::max (bd, groupLat[(size_t) k]); }
        for (int r = 0; r < numReturns; ++r) { returnLat[(size_t) r] = montageNow ? 0 : insertLat (numTracks + r); bd = std::max (bd, returnLat[(size_t) r]); }
        const auto bcap = busDly[0].l.empty() ? 0 : (int) busDly[0].l.size() - controlBlock - 1;
        busComp = std::clamp (bd, 0, std::max (0, bcap));
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const int v = ch < numTracks ? (g.ch[(size_t) ch].instrument != nullptr && g.ch[(size_t) ch].used ? insertLat (ch) : 0)
                        : ch < masterChannel ? returnLat[(size_t) (ch - numTracks)]
                        : ch == masterChannel ? masterLat : groupLat[(size_t) (ch - groupBase)];
            chanLat[(size_t) ch].store (v, std::memory_order_relaxed);
        }
        compReported.store (compDelay + busComp + masterLat, std::memory_order_relaxed);
        insertLatReported.store (ins + busComp + masterLat, std::memory_order_relaxed);
    }
    float masterL[controlBlock] {}, masterR[controlBlock] {};
    float retL[numReturns][controlBlock] {}, retR[numReturns][controlBlock] {};
    float grpL[numGroups][controlBlock] {}, grpR[numGroups][controlBlock] {}; // bus de groupe (v0.24)
    const auto inv = 1.0f / (float) n;

    auto mixGains = [] (const float* p, float& gl, float& gr)
    {
        const auto vol = p[mixVolume] <= -59.9f ? 0.0f : std::pow (10.0f, p[mixVolume] / 20.0f);
        const auto pan = p[mixPan] / 100.0f;
        gl = vol * std::min (1.0f, 1.0f - pan);
        gr = vol * std::min (1.0f, 1.0f + pan);
    };

    // Coupure et solo des pistes : le séquenceur ne joue plus leurs notes, et leur son s'éteint
    // (sons tenus, boucles, traînes des inserts et envois compris) — comme la coupure d'une console.
    bool anySolo = false;
    for (int t = 0; t < numTracks; ++t) anySolo = anySolo || seq->getTrackSolo (t);
    // v0.31 : solo d'un groupe — ses pistes restent entendues ; un groupe reste ouvert si une de
    // ses pistes est en solo ; les retours ne sont jamais coupés par un solo
    bool groupSolo[numGroups] {}, groupHasSolo[numGroups] {};
    for (int k = 0; k < numGroups; ++k) { groupSolo[k] = getBusSolo (groupBase + k); anySolo = anySolo || groupSolo[k]; }
    for (int t = 0; t < numTracks; ++t)
        if (const auto o = trackOutput[(size_t) t].load (std::memory_order_relaxed); o >= 1 && o <= numGroups && seq->getTrackSolo (t)) groupHasSolo[o - 1] = true;
    auto groupOpen = [&] (int k) { return ! getBusMute (groupBase + k) && (! anySolo || groupSolo[k] || groupHasSolo[k]); };
    const bool muteTails = seq->getMuteTails(); // réglage : laisser finir les sons et les traînes
    const auto muteStep = (float) n / (float) std::max (1.0, 0.006 * sampleRate);

    // Side-chain (v0.23) : la sortie de chaque piste (après inserts, avant volume et coupure) est
    // gardée ; une piste déjà rendue dans ce bloc est lue telle quelle, une piste rendue plus loin
    // l'est avec un bloc de retard (32 échantillons, 0,7 ms), une piste sans son vaut le silence.
    scPrevValid = scCurValid;
    scCurValid.fill (false);
    for (int t = 0; t < numTracks; ++t)
    {
        std::copy (scCurL[(size_t) t].begin(), scCurL[(size_t) t].end(), scPrevL[(size_t) t].begin());
        std::copy (scCurR[(size_t) t].begin(), scCurR[(size_t) t].end(), scPrevR[(size_t) t].begin());
    }
    const float* scPtrL[numTracks];
    const float* scPtrR[numTracks];
    auto sidechainFor = [&] (int ch)
    {
        for (int s = 0; s < numTracks; ++s)
        {
            const bool cur = scCurValid[(size_t) s] && (s < ch || ch >= numTracks);
            scPtrL[s] = cur ? scCurL[(size_t) s].data() : scPrevValid[(size_t) s] ? scPrevL[(size_t) s].data() : scSilence.data();
            scPtrR[s] = cur ? scCurR[(size_t) s].data() : scPrevValid[(size_t) s] ? scPrevR[(size_t) s].data() : scSilence.data();
        }
    };

    // envois d'une piste ou d'un groupe vers les retours (gains en rampe sur le bloc)
    // (scale0 → scale1 : volume du groupe de la piste, en rampe ; 1 pour une piste sur le bus principal)
    auto sends = [&] (ChannelState& c, const float* srcL, const float* srcR, float scale0 = 1.0f, float scale1 = 1.0f)
    {
        const auto ds = (scale1 - scale0) * inv;
        for (int r = 0; r < numReturns; ++r)
        {
            const auto target = c.eff[(size_t) (mixSendA + r)] / 100.0f;
            const auto g2 = target * target;
            const auto from = c.sendGain[(size_t) r];
            c.sendGain[(size_t) r] = g2;
            if (from == 0.0f && g2 == 0.0f) continue;
            if (scale0 == 0.0f && scale1 == 0.0f) continue;
            const auto d = (g2 - from) * inv;
            for (int i = 0; i < n; ++i)
            {
                const auto k = (from + d * (float) i) * (scale0 + ds * (float) i);
                retL[r][i] += srcL[i] * k;
                retR[r][i] += srcR[i] * k;
            }
        }
    };

    // Bus de groupe (v0.24) : le volume d'un groupe s'applique aussi aux envois de ses pistes
    // (baisser le groupe baisse aussi leurs réverbérations et leurs échos, comme sur une console)
    float grpScale0[numGroups], grpScale1[numGroups];
    for (int k = 0; k < numGroups; ++k)
    {
        auto& gc = st[(size_t) (groupBase + k)];
        const auto v = gc.eff[(size_t) mixVolume];
        grpScale0[k] = gc.groupScale;
        grpScale1[k] = v <= -59.9f || ! groupOpen (k) ? 0.0f : std::pow (10.0f, v / 20.0f); // groupe coupé : ses envois aussi
        gc.groupScale = grpScale1[k];
    }

    auto channelPass = [&] (int ch, float* L, float* R, float* preL = nullptr, float* preR = nullptr)
    {
        const auto& cg = g.ch[(size_t) ch];
        auto& c = st[(size_t) ch];
        const auto mute0 = c.muteGain;
        if (ch < numTracks)
        {
            const auto og = trackOutput[(size_t) ch].load (std::memory_order_relaxed);
            const bool soloed = seq->getTrackSolo (ch) || (absorbedNow[(size_t) ch] && ch + 1 < numTracks && seq->getTrackSolo (ch + 1))
                             || (og >= 1 && og <= numGroups && groupSolo[og - 1]);
            const bool silent = ! muteTails && (seq->getTrackMute (ch) || (anySolo && ! soloed));
            c.muteGain = silent ? std::max (0.0f, mute0 - muteStep) : std::min (1.0f, mute0 + muteStep);
        }
        else if (isBus (ch))
        {
            // retours : coupure seule ; groupes : coupure et solo (v0.31)
            const bool open = ch >= groupBase ? groupOpen (ch - groupBase) : ! getBusMute (ch);
            c.muteGain = open ? std::min (1.0f, mute0 + muteStep) : std::max (0.0f, mute0 - muteStep);
        }
        const auto mute1 = c.muteGain;
        const auto* p = c.eff.data();
        bool scReady = false;
        // Égaliseur de la tranche (après les inserts, avant la largeur et le fader ; 1.1 : sur le
        // principal, AVANT les inserts, pour que le limiteur final reste le dernier étage et qu'aucune
        // correction de la tranche ne refasse passer de crêtes au-dessus de son plafond) ; à plat, rien
        // n'est calculé ; les coefficients ne changent que si les réglages bougent
        auto stripEq = [&]
        {
            const auto gLo = (double) p[mixEqLow], gMid = (double) p[mixEqMid], gHi = (double) p[mixEqHigh];
            const bool on = std::abs (gLo) > 0.05 || std::abs (gMid) > 0.05 || std::abs (gHi) > 0.05;
            if (on && ! c.eqOn) { c.eqLow.reset(); c.eqMid.reset(); c.eqHigh.reset(); }
            c.eqOn = on;
            if (on)
            {
                c.eqLow.set (grv::dsp::SvfEq::lowShelf, 100.0, 0.7, gLo, sampleRate);
                c.eqMid.set (grv::dsp::SvfEq::bell, std::clamp ((double) p[mixEqMidFreq], 200.0, 8000.0), 0.9, gMid, sampleRate);
                c.eqHigh.set (grv::dsp::SvfEq::highShelf, 10000.0, 0.7, gHi, sampleRate);
                for (int i = 0; i < n; ++i)
                {
                    L[i] = (float) c.eqHigh.l.process (c.eqMid.l.process (c.eqLow.l.process (L[i])));
                    R[i] = (float) c.eqHigh.r.process (c.eqMid.r.process (c.eqLow.r.process (R[i])));
                }
            }
        };
        if (ch == masterChannel) stripEq();
        for (int i = 0; i < numInserts; ++i)
            if (auto* fx = cg.inserts[(size_t) i]; fx != nullptr)
            {
                const auto* fp = p + insertBase + i * insertSlots;
                const bool active = p[mixBypass1 + i] < 0.5f;
                auto& g = c.fxOn[(size_t) i];
                // v0.46 fix22 : pavé vectoriel en mode effets — l'insert du principal est dosé (son
                // direct ↔ son traité) selon le poids de son coin
                if (ch == masterChannel && vecFxMapped[(size_t) i] && active)
                {
                    g = 1.0f;
                    auto& vg = vecFxGain[(size_t) i];
                    const auto v0 = vg, v1 = vecFxTarget[(size_t) i];
                    vg = v1;
                    float dryL[controlBlock], dryR[controlBlock];
                    std::copy (L, L + n, dryL); std::copy (R, R + n, dryR);
                    if (! scReady) { sidechainFor (ch); scReady = true; }
                    fx->setPosition (ppq, playing);
                    fx->setSidechainSources (scPtrL, scPtrR);
                    fx->process (L, R, n, fp, bpm);
                    const auto dv = (v1 - v0) * inv;
                    for (int k = 0; k < n; ++k)
                    {
                        const auto w = v0 + dv * (float) k;
                        L[k] = dryL[k] + (L[k] - dryL[k]) * w;
                        R[k] = dryR[k] + (R[k] - dryR[k]) * w;
                    }
                    continue;
                }
                // v0.46 : contourner / rallumer un effet sans traîne passe par un fondu de 6 ms
                // entre le son direct et le son traité (plus de saut au clic)
                const bool fading = ! fx->trails (fp) && (active ? g < 1.0f : g > 0.0f);
                if (fading)
                {
                    const auto g0 = g;
                    g = active ? std::min (1.0f, g + muteStep) : std::max (0.0f, g - muteStep);
                    float dryL[controlBlock], dryR[controlBlock];
                    std::copy (L, L + n, dryL); std::copy (R, R + n, dryR);
                    if (! scReady) { sidechainFor (ch); scReady = true; }
                    fx->setPosition (ppq, playing);
                    fx->setSidechainSources (scPtrL, scPtrR);
                    fx->process (L, R, n, fp, bpm);
                    const auto dg = (g - g0) * inv;
                    for (int k = 0; k < n; ++k)
                    {
                        const auto w = g0 + dg * (float) k;
                        L[k] = dryL[k] + (L[k] - dryL[k]) * w;
                        R[k] = dryR[k] + (R[k] - dryR[k]) * w;
                    }
                }
                else if (active)
                {
                    g = 1.0f;
                    if (! scReady) { sidechainFor (ch); scReady = true; }
                    fx->setPosition (ppq, playing);
                    fx->setSidechainSources (scPtrL, scPtrR);
                    fx->process (L, R, n, fp, bpm);
                }
                else if (g = 0.0f; fx->trails (fp))
                {
                    fx->setPosition (ppq, playing);
                    fx->processTrails (L, R, n, fp, bpm); // v0.35 : la queue continue, contourné
                }
            }

        if (ch != masterChannel) stripEq();

        // Largeur stéréo (mid/side)
        const auto width = p[mixWidth] / 100.0f;
        if (std::abs (width - 1.0f) > 1.0e-3f)
            for (int i = 0; i < n; ++i)
            {
                const auto m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * width;
                L[i] = m + s; R[i] = m - s;
            }

        float peak = 0.0f;
        for (int i = 0; i < n; ++i) peak = std::max (peak, std::max (std::abs (L[i]), std::abs (R[i])));
        c.level = peak;
        if (ch < numTracks)
        {
            std::copy (L, L + n, scCurL[(size_t) ch].begin());
            std::copy (R, R + n, scCurR[(size_t) ch].begin());
            std::fill (scCurL[(size_t) ch].begin() + n, scCurL[(size_t) ch].end(), 0.0f);
            std::fill (scCurR[(size_t) ch].begin() + n, scCurR[(size_t) ch].end(), 0.0f);
            scCurValid[(size_t) ch] = true;
        }

        if (preL != nullptr)
        {
            const auto dm = (mute1 - mute0) * inv;
            for (int i = 0; i < n; ++i) { const auto k = mute0 + dm * (float) i; preL[i] = L[i] * k; preR[i] = R[i] * k; }
        }

        float gl, gr;
        mixGains (p, gl, gr);
        gl *= mute1; gr *= mute1;
        const auto dl = (gl - c.gainL) * inv, dr = (gr - c.gainR) * inv;
        for (int i = 0; i < n; ++i)
        {
            L[i] *= c.gainL + dl * (float) i;
            R[i] *= c.gainR + dr * (float) i;
        }
        c.gainL = gl; c.gainR = gr;

        float post = 0.0f;
        for (int i = 0; i < n; ++i) post = std::max (post, std::max (std::abs (L[i]), std::abs (R[i])));
        const auto prev = peaks[(size_t) ch].load (std::memory_order_relaxed);
        peaks[(size_t) ch].store (std::max (post, prev * 0.97f), std::memory_order_relaxed);
    };

    // Pistes. La sortie de chaque piste (après effets et volume) est gardée pour la suivante :
    // une piste « Neighbor » la reçoit, et l'absorbe (elle ne va plus au mixage).
    float prevL[controlBlock] {}, prevR[controlBlock] {};
    bool prevValid = false;
    // sortie de chaque piste (après effets et volume) : un enregistreur peut en additionner certaines
    bool wantsTracks = false;
    for (int t = 0; t < numTracks; ++t)
        if (auto* inst = g.ch[(size_t) t].used ? g.ch[(size_t) t].instrument : nullptr; inst != nullptr && inst->wantsMainMix() && inst->capturedTracks() >= 0)
            wantsTracks = true;
    auto* tap = audioTap.load (std::memory_order_acquire);
    if (tap != nullptr && tap->wantsStems()) wantsTracks = true;
    const bool monSel = monOn.load (std::memory_order_acquire);
    if ((monSel || monMix > 0.0f) && monTracks.load (std::memory_order_relaxed) != 0) wantsTracks = true;
    float trackOutL[numTracks][controlBlock], trackOutR[numTracks][controlBlock];
    std::uint32_t trackOutMask = 0;
    // v0.46 fix8 : montage audio — les clips remplacent la sortie des pistes et des retours
    auto* montage = montageFeed.load (std::memory_order_acquire);
    const bool montageOn = montage != nullptr && montage->montageActive();
    float laneL[numMontageLanes][controlBlock], laneR[numMontageLanes][controlBlock];
    std::uint32_t laneMask = 0;
    if (montageOn) montage->pull (playing, n, laneL, laneR, laneMask);
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& cg = g.ch[(size_t) t];
        if (montageOn)
        {
            // l'instrument tourne encore (sans note nouvelle) pour finir ses sons, mais il n'est
            // pas entendu ; la voie du montage prend sa place, coupure et solo compris
            if (cg.used && cg.instrument != nullptr)
            {
                // tout ce que l'instrument écrit ou lit hors de lui part dans des tampons muets
                float dumpL[controlBlock] {}, dumpR[controlBlock] {}, sendDump[4][controlBlock] {};
                if (cg.instrument->hasPadSends()) cg.instrument->setPadSendBuses (sendDump[0], sendDump[1], sendDump[2], sendDump[3]);
                cg.instrument->setNeighbor (nullptr, nullptr);
                cg.instrument->setPadInserts (nullptr, nullptr, nullptr, bpm);
                for (int s2 = 0; s2 < numTracks; ++s2) { scPtrL[s2] = scSilence.data(); scPtrR[s2] = scSilence.data(); }
                cg.instrument->setSidechainSources (scPtrL, scPtrR);
                cg.instrument->render (dumpL, dumpR, n, st[(size_t) t].eff.data() + instrumentBase);
            }
            auto& c = st[(size_t) t];
            const auto og = trackOutput[(size_t) t].load (std::memory_order_relaxed);
            const bool soloed = seq->getTrackSolo (t) || (og >= 1 && og <= numGroups && groupSolo[og - 1]);
            const bool silent = seq->getTrackMute (t) || (anySolo && ! soloed);
            const auto m0 = c.muteGain;
            c.muteGain = silent ? std::max (0.0f, m0 - muteStep) : std::min (1.0f, m0 + muteStep);
            const auto dm = (c.muteGain - m0) * inv;
            float L[controlBlock], R[controlBlock];
            float peak = 0.0f;
            const bool has = (laneMask & (1u << t)) != 0;
            for (int i = 0; i < n; ++i)
            {
                const auto k = m0 + dm * (float) i;
                L[i] = has ? laneL[t][i] * k : 0.0f; R[i] = has ? laneR[t][i] * k : 0.0f;
                peak = std::max (peak, std::max (std::abs (L[i]), std::abs (R[i])));
            }
            c.level = peak;
            // v0.47 : vumètre de la piste pendant le montage (il restait figé)
            peaks[(size_t) t].store (std::max (peak, peaks[(size_t) t].load (std::memory_order_relaxed) * 0.97f), std::memory_order_relaxed);
            if (wantsTracks) { std::copy (L, L + n, trackOutL[t]); std::copy (R, R + n, trackOutR[t]); trackOutMask |= 1u << t; }
            if (! has && m0 == 0.0f) continue;
            float* dl = og >= 1 && og <= numGroups ? grpL[og - 1] : masterL;
            float* dr = og >= 1 && og <= numGroups ? grpR[og - 1] : masterR;
            for (int i = 0; i < n; ++i) { dl[i] += L[i]; dr[i] += R[i]; }
            continue;
        }
        if (! cg.used || cg.instrument == nullptr)
        {
            // piste vide : niveau nul (suiveurs en side-chain) ; v0.47 : son vumètre retombe aussi (il restait
            // figé quand on ouvrait un nouveau projet pendant la lecture)
            prevValid = false; st[(size_t) t].level = 0.0f;
            peaks[(size_t) t].store (peaks[(size_t) t].load (std::memory_order_relaxed) * 0.97f, std::memory_order_relaxed);
            continue;
        }
        auto& c = st[(size_t) t];
        float L[controlBlock] {}, R[controlBlock] {};
        // Envois par pad (batteries) : directement dans les retours A et B
        if (cg.instrument->hasPadSends()) cg.instrument->setPadSendBuses (retL[0], retR[0], retL[1], retR[1]);
        // piste précédente : après ses inserts, avant son volume et sa coupure (v0.25 : un vocodeur
        // en solo garde sa voix ; le volume de la piste source ne change pas le modulateur)
        const bool nb = prevValid && t > 0 && scCurValid[(size_t) (t - 1)];
        cg.instrument->setNeighbor (nb ? scCurL[(size_t) (t - 1)].data() : nullptr, nb ? scCurR[(size_t) (t - 1)].data() : nullptr);
        if (cg.anyPadFx)
        {
            for (const auto& row : cg.padFx) for (auto* e : row) if (e != nullptr) e->setPosition (ppq, playing);
            cg.instrument->setPadInserts (&cg.padFx, c.eff.data() + padInsertBase, c.eff.data() + padBypassBase, bpm);
        }
        else cg.instrument->setPadInserts (nullptr, nullptr, nullptr, bpm);
        // sorties des pistes offertes à l'instrument : compresseurs des pads (v0.24), modulateur
        // d'un vocodeur (v0.25)
        sidechainFor (t);
        scPtrL[t] = scPtrR[t] = scSilence.data(); // jamais sa propre sortie : pas de boucle de réinjection
        cg.instrument->setSidechainSources (scPtrL, scPtrR);
        {
            // après la 1.1 : une piste MIDI coupée (ou hors solo) ne joue plus l'appareil
            const auto og = trackOutput[(size_t) t].load (std::memory_order_relaxed);
            const bool soloed = seq->getTrackSolo (t) || (og >= 1 && og <= numGroups && groupSolo[og - 1]);
            cg.instrument->setTrackSilent (seq->getTrackMute (t) || (anySolo && ! soloed));
        }
        cg.instrument->render (L, R, n, c.eff.data() + instrumentBase);
        // v0.49 : retard ajouté pour tomber avec l'instrument le plus en retard (FM Player)
        // 1.1 (lot 6) : la ligne est toujours alimentée ; un changement de retard passe par un fondu
        if (! compL[(size_t) t].empty())
        {
            const auto dly = compDelay - std::min (compDelay, trackLat[(size_t) t]);
            auto& bl = compL[(size_t) t]; auto& br = compR[(size_t) t];
            const auto len = (int) bl.size();
            auto w = compWrite[(size_t) t];
            if (dly != compPrev[(size_t) t])
            {
                compOld[(size_t) t] = compPrev[(size_t) t];
                compPrev[(size_t) t] = dly;
                compFadeLeft[(size_t) t] = std::max (1, (int) (sampleRate * 0.01));
            }
            const auto fadeLen = std::max (1, (int) (sampleRate * 0.01));
            auto& fl = compFadeLeft[(size_t) t];
            const auto od = compOld[(size_t) t];
            for (int i = 0; i < n; ++i)
            {
                bl[(size_t) w] = L[i]; br[(size_t) w] = R[i];
                auto r = w - dly; if (r < 0) r += len;
                if (fl > 0)
                {
                    auto ro = w - od; if (ro < 0) ro += len;
                    const auto k = (float) fl / (float) fadeLen;   // 1 → 0 : de l'ancien retard au nouveau
                    L[i] = bl[(size_t) ro] * k + bl[(size_t) r] * (1.0f - k);
                    R[i] = br[(size_t) ro] * k + br[(size_t) r] * (1.0f - k);
                    --fl;
                }
                else { L[i] = bl[(size_t) r]; R[i] = br[(size_t) r]; }
                if (++w >= len) w = 0;
            }
            compWrite[(size_t) t] = w;
        }
        const auto* next = t + 1 < numTracks && g.ch[(size_t) (t + 1)].used ? g.ch[(size_t) (t + 1)].instrument : nullptr;
        const bool absorbed = next != nullptr && next->absorbsNeighbor (st[(size_t) (t + 1)].eff.data() + instrumentBase);
        // piste prise par la suivante (voix d'un vocodeur) : elle suit le solo de celle-ci
        absorbedNow[(size_t) t] = absorbed;
        seq->setSoloFollow (t, absorbed);
        const bool pre = c.eff[(size_t) mixSendPre] > 0.5f;
        float preL[controlBlock], preR[controlBlock];
        channelPass (t, L, R, pre ? preL : nullptr, pre ? preR : nullptr);
        const float* srcL = pre ? preL : L;
        const float* srcR = pre ? preR : R;
        std::copy (L, L + n, prevL); std::copy (R, R + n, prevR);
        prevValid = true;
        if (wantsTracks) { std::copy (L, L + n, trackOutL[t]); std::copy (R, R + n, trackOutR[t]); trackOutMask |= 1u << t; }
        if (absorbed) continue; // la piste suivante la prend à son compte

        // sortie : bus principal ou bus de groupe (v0.24)
        const auto out = trackOutput[(size_t) t].load (std::memory_order_relaxed);
        if (out >= 1 && out <= numGroups) sends (c, srcL, srcR, grpScale0[out - 1], grpScale1[out - 1]);
        else sends (c, srcL, srcR);
        float* dl = out >= 1 && out <= numGroups ? grpL[out - 1] : masterL;
        float* dr = out >= 1 && out <= numGroups ? grpR[out - 1] : masterR;
        for (int i = 0; i < n; ++i) { dl[i] += L[i]; dr[i] += R[i]; }
    }

    // 1.1 (lot 6) : le son direct des pistes attend les groupes et retours à latence
    const auto busFade = std::max (1, (int) (sampleRate * 0.01));
    busDly[0].process (masterL, masterR, n, busComp, busFade);

    // Bus de groupe (v0.24) : inserts, égaliseur, volume, envois vers les retours, puis le principal
    for (int k = 0; k < numGroups; ++k)
    {
        const auto ch = groupBase + k;
        auto& c = st[(size_t) ch];
        const bool pre = c.eff[(size_t) mixSendPre] > 0.5f;
        float preL[controlBlock], preR[controlBlock];
        channelPass (ch, grpL[k], grpR[k], pre ? preL : nullptr, pre ? preR : nullptr);
        if (! montageOn) sends (c, pre ? preL : grpL[k], pre ? preR : grpR[k]); // montage : les retours enregistrés ont déjà ces envois
        busDly[(size_t) (1 + k)].process (grpL[k], grpR[k], n, busComp - std::min (busComp, groupLat[(size_t) k]), busFade);
        for (int i = 0; i < n; ++i) { masterL[i] += grpL[k][i]; masterR[i] += grpR[k][i]; }
    }

    // Retours
    for (int r = 0; r < numReturns; ++r)
    {
        const auto ch = numTracks + r;
        if (montageOn)
        {
            // retour enregistré (après ses effets et son volume) : seule sa coupure s'applique
            auto& c = st[(size_t) ch];
            const auto m0 = c.muteGain;
            c.muteGain = getBusMute (ch) ? std::max (0.0f, m0 - muteStep) : std::min (1.0f, m0 + muteStep);
            const auto dm = (c.muteGain - m0) * inv;
            const bool has = (laneMask & (1u << (numTracks + r))) != 0;
            float peak = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const auto k = m0 + dm * (float) i;
                retL[r][i] = has ? laneL[numTracks + r][i] * k : 0.0f; retR[r][i] = has ? laneR[numTracks + r][i] * k : 0.0f;
                peak = std::max (peak, std::max (std::abs (retL[r][i]), std::abs (retR[r][i])));
            }
            c.level = peak;
        }
        else channelPass (ch, retL[r], retR[r]);
        busDly[(size_t) (1 + numGroups + r)].process (retL[r], retR[r], n, busComp - std::min (busComp, returnLat[(size_t) r]), busFade);
        for (int i = 0; i < n; ++i) { masterL[i] += retL[r][i]; masterR[i] += retR[r][i]; }
    }

    // Bus principal
    channelPass (masterChannel, masterL, masterR);
    for (int i = 0; i < n; ++i)
    {
        // garde-fou : jamais de valeur invalide ni d'excursion extrême vers la carte son
        const auto l = masterL[i], r = masterR[i];
        outL[i] = std::isfinite (l) ? std::clamp (l, -4.0f, 4.0f) : 0.0f;
        outR[i] = std::isfinite (r) ? std::clamp (r, -4.0f, 4.0f) : 0.0f;
    }
    {
        float pl = 0.0f, pr = 0.0f;
        for (int i = 0; i < n; ++i) { pl = std::max (pl, std::abs (outL[i])); pr = std::max (pr, std::abs (outR[i])); }
        outPeakL.store (std::max (pl, outPeakL.load (std::memory_order_relaxed) * 0.97f), std::memory_order_relaxed);
        outPeakR.store (std::max (pr, outPeakR.load (std::memory_order_relaxed) * 0.97f), std::memory_order_relaxed);
    }
    loudness.process (outL, outR, n);
    scope.write (outL, outR, n);

    // v0.32 : enregistrement audio (sortie principale, pistes séparées, retours)
    if (tap != nullptr)
    {
        const float* tl[numTracks]; const float* tr[numTracks];
        for (int t = 0; t < numTracks; ++t) { tl[t] = trackOutL[t]; tr[t] = trackOutR[t]; }
        const float* rl[numReturns]; const float* rr[numReturns];
        for (int r = 0; r < numReturns; ++r) { rl[r] = retL[r]; rr[r] = retR[r]; }
        tap->tapPosition (ppq, ppq + (double) n * bpm / 60.0 / sampleRate, playing);
        tap->tapInput (curInL, curInR);
        tap->tapBlock (outL, outR, tl, tr, wantsTracks ? trackOutMask : 0u, rl, rr, n);
    }

    // Rééchantillonnage : les enregistreurs qui écoutent le mixage principal reçoivent ce bloc
    for (int t = 0; t < numTracks; ++t)
        if (auto* inst = g.ch[(size_t) t].used ? g.ch[(size_t) t].instrument : nullptr; inst != nullptr && inst->wantsMainMix())
        {
            const auto mask = inst->capturedTracks();
            if (mask < 0) { inst->captureMain (outL, outR, n); continue; }
            // pistes choisies (jamais la piste elle-même : pas de larsen)
            float busL[controlBlock] {}, busR[controlBlock] {};
            const auto use = (std::uint32_t) mask & trackOutMask & ~(1u << t);
            for (int k = 0; k < numTracks; ++k)
                if (use & (1u << k))
                    for (int i = 0; i < n; ++i) { busL[i] += trackOutL[k][i]; busR[i] += trackOutR[k][i]; }
            inst->captureMain (busL, busR, n);
        }

    // 1.1 : écoute de l'écran AUDIO : on n'entend que les sources choisies pour la prise (après
    // l'enregistrement et le rééchantillonnage, qui gardent le vrai mixage), en fondu de 10 ms
    if (monSel || monMix > 0.0f)
    {
        const auto tm = monTracks.load (std::memory_order_relaxed) & trackOutMask;
        const auto rm = monReturns.load (std::memory_order_relaxed);
        float selL[controlBlock] {}, selR[controlBlock] {};
        for (int k = 0; k < numTracks; ++k)
            if (tm & (1u << k))
                for (int i = 0; i < n; ++i) { selL[i] += trackOutL[k][i]; selR[i] += trackOutR[k][i]; }
        for (int r = 0; r < numReturns; ++r)
            if (rm & (1u << r))
                for (int i = 0; i < n; ++i) { selL[i] += retL[r][i]; selR[i] += retR[r][i]; }
        if (monInput.load (std::memory_order_relaxed) && curInL != nullptr)
            for (int i = 0; i < n; ++i) { selL[i] += curInL[i]; selR[i] += curInR != nullptr ? curInR[i] : curInL[i]; }
        const float target = monSel ? 1.0f : 0.0f;
        const float step = (float) (1.0 / (0.01 * sampleRate));
        for (int i = 0; i < n; ++i)
        {
            monMix = target > monMix ? std::min (target, monMix + step) : std::max (target, monMix - step);
            outL[i] += (selL[i] - outL[i]) * monMix;
            outR[i] += (selR[i] - outR[i]) * monMix;
        }
    }

    // 1.1 : pré-écoute d'un son traité, après l'enregistrement et le rééchantillonnage (ni l'un ni l'autre ne la prend)
    if (const auto serial = auditionSerial.load (std::memory_order_acquire); serial != auditionSeen)
    {
        auditionSeen = serial;
        auditionCur = auditionNext.load (std::memory_order_acquire);
        auditionPos = 0.0;
    }
    if (auditionCur != nullptr)
    {
        const auto* b = auditionCur;
        const auto len = (int) b->data.size();
        const bool st = b->right.size() == b->data.size();
        const double step = b->sampleRate / sampleRate;
        for (int i = 0; i < n; ++i)
        {
            const auto k = (int) auditionPos;
            if (k + 1 >= len) { auditionCur = nullptr; auditionOn.store (false, std::memory_order_relaxed); break; }
            const auto f = (float) (auditionPos - k);
            const auto l = b->data[(size_t) k] + (b->data[(size_t) k + 1] - b->data[(size_t) k]) * f;
            const auto r = st ? b->right[(size_t) k] + (b->right[(size_t) k + 1] - b->right[(size_t) k]) * f : l;
            outL[i] = std::clamp (outL[i] + l, -4.0f, 4.0f);
            outR[i] = std::clamp (outR[i] + r, -4.0f, 4.0f);
            auditionPos += step;
        }
    }
}

void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples,
                      const BlockInfo& bi, const InputEvent* events, int numEvents) noexcept
{
    const auto* g = graph.get();
    const auto* sc = scenes.get();
    const auto* ex = extras.get();
    midiOut.count = 0; midiOut.at = 0; // après la 1.1 : messages MIDI de cet appel
    adoptTrackOrder (*g); // pistes déplacées : leur état les suit (v0.26)

    // Échantillons : l'instrument reçoit ceux du graphe en cours (jamais un pointeur périmé)
    for (int t = 0; t < numTracks; ++t)
    {
        auto* inst = g->ch[(size_t) t].instrument;
        if (inst == nullptr) continue;
        auto& c = (*state)[(size_t) t];
        if (c.sampleOwner != inst) { c.sampleOwner = inst; c.appliedSamples.fill (nullptr); c.appliedSpeech = nullptr; inst->setSpeech (nullptr); }
        for (int i = 0; i < maxSamples; ++i)
        {
            const auto* smp = g->ch[(size_t) t].samples[(size_t) i];
            if (smp != nullptr && smp->firmware != inst->acceptsFirmware()) smp = nullptr; // v0.49 : firmware → FM Player seulement
            if (smp != c.appliedSamples[(size_t) i]) { inst->setSample (i, smp); c.appliedSamples[(size_t) i] = smp; }
        }
        if (const auto* sp = g->ch[(size_t) t].speech; sp != c.appliedSpeech) { inst->setSpeech (sp); c.appliedSpeech = sp; }
        // v0.42 : pistes internes (Pieuvre 8) — ligne jouée au clavier, lignes coupées (coupure et solo des lignes)
        if (inst->desc().drumKit)
        {
            inst->setPlayLane (playLane[(size_t) t].load (std::memory_order_relaxed));
            std::uint32_t mute = 0;
            bool anySolo = false;
            for (int l = 0; l < Sequencer::numLanes; ++l) anySolo = anySolo || seq->getLaneSolo (t, l);
            for (int l = 0; l < Sequencer::numLanes; ++l)
                if (seq->getLaneMute (t, l) || (anySolo && ! seq->getLaneSolo (t, l))) mute |= 1u << l;
            inst->setLaneMutes (mute);
        }
        // v0.43 : Pieuvre 8 coupe le son de ses lignes elle-même → le séquenceur continue d'y jouer les notes
        seq->setLaneAudioMute (t, inst->desc().drumKit && inst->desc().laneNotes);
    }

    seq->beginBlock (numSamples, sampleRate, bi.bpm, bi.swing, bi.host);
    sampleClock.fetch_add (numSamples, std::memory_order_relaxed);
    if (const auto* sg = song.get()) songBlock (*sg); // mode morceau : bascules des rangées dans ce bloc

    // Arrêt du séquenceur : les notes qu'il avait lancées se relâchent tout de suite (avec leur
    // relâchement) au lieu de tenir jusqu'à leur durée prévue — une note de 128 pas tiendrait 30 s.
    const bool runningNow = seq->isPlayingInBlock();
    if (wasRunning && ! runningNow)
    {
        for (int i = 0; i < numPendingOffs; ++i) dispatch (*g, pendingOffs[(size_t) i].track, pendingOffs[(size_t) i].note, 0.0f, false);
        numPendingOffs = 0;
        // arrêt : les sons sans fin (boucle à tenue infinie, boucle Pickup) s'arrêtent aussi
        for (int t = 0; t < numTracks; ++t) if (auto* inst = g->ch[(size_t) t].instrument) inst->transportStopped();
    }
    if (! wasRunning && runningNow)
        for (int t = 0; t < numTracks; ++t) if (auto* inst = g->ch[(size_t) t].instrument) inst->transportStarted();
    const bool wasRunningBefore = wasRunning; // (arrêt / départ d'une piste seule : après la collecte)
    wasRunning = runningNow;
    if (! seq->isRecording()) arpRecording.fill (false);

    // Événements : interface, MIDI, puis séquenceur (l'enregistrement direct passe avant)
    using Ev = BlockEvent;
    auto& evs = *blockEvents;
    int numEvs = 0;
    auto add = [&] (const Ev& e) { if (numEvs < (int) evs.size()) evs[(size_t) numEvs++] = e; };

    auto handleInput = [&] (const InputEvent& e)
    {
        if (e.track < 0 || e.track >= numTracks) return;
        const auto* inst = g->ch[(size_t) e.track].instrument;
        if (inst == nullptr) return;
        const auto offset = std::clamp (e.offset, 0, std::max (0, numSamples - 1));
        if (e.type == InputEvent::recordAction)
        {
            add ({ offset, e.track, 0, 0, 0.0f, false, 0.0, nullptr, false, kindRecord + std::clamp (e.note, 0, 3) });
            return;
        }
        if (e.type != InputEvent::noteOn && e.type != InputEvent::noteOff)
        {
            add ({ offset, e.track, 0, e.note, e.velocity, false, 0.0, nullptr, false, (int) e.type });
            return;
        }
        if (e.type == InputEvent::noteOn && e.record)
        {
            const bool melodic = ! inst->desc().drumKit;
            // arpège en marche : ce sont ses notes (ce qu'on entend) qui s'écrivent, pas la touche tenue
            if (melodic && arpActive (*g, e.track)) arpRecording[(size_t) e.track] = true;
            else if (melodic) seq->recordHit (e.track, 0, e.note, e.velocity, offset, true);
            else
            {
                // v0.42 : clavier d'une Pieuvre (notes 16 et plus) → sa ligne jouée, avec la note jouée
                // v0.43 : zones du clavier — la note va dans la ligne de sa zone, transposée par rapport à la zone
                const int lowKey = inst->desc().laneLowKeys ? 0 : 16; // GOLEM 1.0 : FM Player, toutes les notes sont des touches
                // GOLEM 1.0 : note arrivée sur le canal MIDI propre d'une T (128 + T × 128 + note) : dans cette T
                const bool laneAddr = e.note >= 128 && inst->desc().laneNotes;
                int pp = -1, pos = -1, want = laneAddr ? std::clamp ((e.note - 128) / 128, 0, 15)
                                                       : e.note < lowKey ? e.note : playLane[(size_t) e.track].load (std::memory_order_relaxed),
                    stepNote = laneAddr ? (e.note - 128) % 128 : e.note;
                if (! laneAddr && e.note >= lowKey && e.note < 128) inst->mapNote (e.note, want, stepNote);
                const auto lane = seq->recordHit (e.track, want, e.note, e.velocity, offset, false, &pp, &pos);
                if (lane >= 0 && (laneAddr || e.note >= lowKey) && inst->desc().laneNotes) seq->setNoteP (e.track, pp, lane, pos, stepNote);
            }
        }
        if (e.type == InputEvent::noteOff) seq->recordRelease (e.track, e.note, offset); // durée de la note enregistrée
        // GOLEM 1.0 : note jouée sur le canal d'une T : sa touche s'allume aussi sur les claviers de l'écran
        if (e.note >= 128 && inst->desc().drumKit && inst->desc().laneNotes)
        {
            const auto key = (e.note - 128) % 128;
            if (e.type == InputEvent::noteOn) noteHits[(size_t) e.track][(size_t) key].fetch_add (1, std::memory_order_relaxed);
            setNoteDown (e.track, key, e.type == InputEvent::noteOn);
        }
        add ({ offset, e.track, 0, e.note, e.velocity, e.type == InputEvent::noteOn, 0.0, nullptr, false });
    };

    InputEvent ui;
    while (uiEvents.pop (ui)) { ui.offset = 0; handleInput (ui); }
    for (int i = 0; i < numEvents; ++i) handleInput (events[i]);

    seq->collect (ex, [&] (const SeqEvent& se)
    {
        const auto* inst = g->ch[(size_t) se.track].instrument;
        if (inst == nullptr) return;
        if (se.lane < 0) { add ({ se.offset, se.track, 0, 0, 0.0f, false, 0.0, nullptr, false, kindRecord + 3 }); return; } // pas d'enregistrement : (re)départ
        const bool drum = inst->desc().drumKit;
        const auto note = ! drum ? se.note : inst->desc().laneNotes ? 128 + se.lane * 128 + std::clamp (se.note, 0, 127) : se.lane; // v0.42 : ligne et note du pas
        add ({ se.offset, se.track, se.lane, note, se.velocity, true, se.lengthSamples, se.extra, true, 0, se.flags, se.stepSamples, se.repeat, se.accent,
               (double) se.glide * se.stepSamples });
    });

    captureBar(); // enregistrer le jeu : patterns joués au début de chaque mesure

    // v0.23 : une piste arrêtée seule (bouton d'arrêt, case vide d'une rangée, morceau) fait comme
    // l'arrêt général pour elle : ses notes se relâchent, ses boucles sans fin se taisent ; relancée,
    // elle repart du début (boucle Pickup, séquenceur de la percussion)
    // (vu juste après la collecte : la bascule du pattern a lieu pendant celle-ci, et l'instrument
    // est prévenu avant la première note de sa reprise)
    for (int t = 0; t < numTracks; ++t)
    {
        const auto pl = seq->getPlaying (t);
        if (runningNow && wasRunningBefore && trackWasPlaying[(size_t) t] >= 0 && pl < 0)
        {
            for (int i = 0; i < numPendingOffs;)
                if (pendingOffs[(size_t) i].track == t) { dispatch (*g, t, pendingOffs[(size_t) i].note, 0.0f, false); pendingOffs[(size_t) i] = pendingOffs[(size_t) --numPendingOffs]; }
                else ++i;
            if (auto* inst = g->ch[(size_t) t].instrument) inst->transportStopped();
        }
        else if (runningNow && wasRunningBefore && trackWasPlaying[(size_t) t] < 0 && pl >= 0)
            if (auto* inst = g->ch[(size_t) t].instrument) inst->transportStarted();
        trackWasPlaying[(size_t) t] = pl;
    }

    for (int i = 1; i < numEvs; ++i)
        for (int j = i; j > 0 && evs[(size_t) j].offset < evs[(size_t) (j - 1)].offset; --j)
            std::swap (evs[(size_t) j], evs[(size_t) (j - 1)]);

    // Rendu par blocs de contrôle, découpés aux événements
    const auto faderCoef = (float) std::exp (-(double) controlBlock / (0.015 * sampleRate));

    int pos = 0, ei = 0;
    while (pos < numSamples)
    {
        midiOut.at = pos; // après la 1.1 : position des messages MIDI de ce bloc
        // déclenche les événements arrivés
        while (ei < numEvs && evs[(size_t) ei].offset <= pos)
        {
            const auto& e = evs[(size_t) ei++];
            if (e.kind >= kindRecord)
            {
                if (auto* inst = g->ch[(size_t) e.track].instrument) inst->recordAction (std::clamp (e.kind - kindRecord, 0, 3));
                continue;
            }
            // v0.49 : CC non appris d'un contrôleur → l'instrument (FM Player : vers le firmware)
            if (e.kind == InputEvent::controller) { if (auto* ci = g->ch[(size_t) e.track].instrument) ci->controller (std::clamp (e.note, 0, 127), std::clamp (e.vel, 0.0f, 1.0f)); continue; }
            if (e.kind != 0) { expression (*g, e.track, e.kind, e.vel); continue; }
            // pas « sans déclenchement » : il pose ses verrous sans jouer de note
            const bool trigless = e.fromSeq && (e.flags & stepTrigless) != 0;
            if (! trigless)
            {
                // pas accentué : l'instrument reçoit la force de l'accent avec la note (batteries :
                // impulsion de déclenchement plus forte, comme le bus d'accent des boîtes analogiques)
                auto* accInst = e.accent > 0.0f && e.track >= 0 && e.track < numTracks ? g->ch[(size_t) e.track].instrument : nullptr;
                if (accInst != nullptr) accInst->setNextAccent (e.accent);
                dispatch (*g, e.track, e.note, e.vel, e.on, e.fromSeq && e.extra != nullptr && e.extra->direct);
                if (accInst != nullptr) accInst->setNextAccent (0.0f);
            }
            auto& c = (*state)[(size_t) e.track];
            if (e.fromSeq && ! e.repeat) // les répétitions d'un pas gardent ses verrous (et leur glissé)
            {
                // Un pas déclenché remplace les verrous de sa ligne (principe Elektron)
                const auto lane = (size_t) std::clamp (e.lane, 0, 15);
                auto& count = c.numPlocks[lane];
                c.anyPlocks -= count > 0 ? 1 : 0;
                count = 0;
                // 1.0 (6 nov.) : verrous de la ligne coupés : gardés dans le pas, mais pas joués
                const bool locksOff = seq != nullptr && seq->getLaneLockMute (e.track, (int) lane);
                if (e.extra != nullptr && ! locksOff)
                    for (const auto& l : e.extra->locks)
                        if (count < (int) c.plocks[lane].size())
                        {
                            // glissé : la valeur part de celle du moment (verrou, scène, modulation ou base)
                            const auto ch = channelOf (l.first), local = localOf (l.first);
                            float from = l.second;
                            if (ch >= 0 && ch < numChannels && local >= 0 && local < slotsPerChannel)
                            {
                                const auto& cs = (*state)[(size_t) ch];
                                from = cs.touched[(size_t) local] ? cs.overrideNorm[(size_t) local] : base[(size_t) l.first].load (std::memory_order_relaxed);
                            }
                            c.plockFrom[lane][(size_t) count] = from;
                            c.plocks[lane][(size_t) count++] = { l.first, l.second };
                        }
                // v0.46 fix27 : la note arrive par un glissé réglé : le portamento de l'instrument prend, pour ce
                // pas, la durée choisie (part du pas), comme un verrou (sans glisser lui-même)
                if (e.glideSamples > 0.0 && count < (int) c.plocks[lane].size() && e.track >= 0 && e.track < numTracks)
                    if (const auto gp = g->ch[(size_t) e.track].glideParam; gp >= 0)
                        if (const auto* d = g->ch[(size_t) e.track].desc[(size_t) (instrumentBase + gp)])
                        {
                            const auto ms = (float) (e.glideSamples / std::max (1.0, sampleRate) * 1000.0);
                            const auto norm = d->toNorm (std::clamp (ms, d->min, d->max));
                            const auto slot = e.track * slotsPerChannel + instrumentBase + gp;
                            c.plockFrom[lane][(size_t) count] = norm;
                            c.plocks[lane][(size_t) count++] = { slot, norm };
                        }
                c.anyPlocks += count > 0 ? 1 : 0;
                // glissé du pas (Gl.), ou verrous glissés pour toute la ligne (1.0) : d'une valeur à la suivante
                const bool slide = ((e.flags & stepSlide) != 0 || (seq != nullptr && seq->getLaneLockGlide (e.track, (int) lane))) && count > 0;
                c.slideLen[lane] = slide ? std::max (1.0, e.stepLen) : 0.0;
                c.slidePos[lane] = 0.0;
                // après la 1.1 : pads verrouillés appuyés le temps de la note (au plus 90 % du pas, au moins 1 ms)
                {
                    const double step = e.stepLen > 0.0 ? e.stepLen : sampleRate * 0.125;
                    c.momentaryLeft[lane] = std::clamp (e.len > 0.0 ? e.len : step * 0.5, sampleRate * 0.001, std::max (sampleRate * 0.001, step * 0.9));
                }
            }
            if (trigless) continue;
            if (e.on)
            {
                // Une note qui repart remplace la fin prévue de la même note (note liée par un glissé,
                // chevauchement de durées) : sinon cette fin couperait la nouvelle note.
                for (int i = 0; i < numPendingOffs;)
                    if (pendingOffs[(size_t) i].track == e.track && pendingOffs[(size_t) i].note == e.note)
                        pendingOffs[(size_t) i] = pendingOffs[(size_t) --numPendingOffs];
                    else ++i;
            }
            if (e.on && e.len > 0.0 && numPendingOffs < (int) pendingOffs.size())
                pendingOffs[(size_t) numPendingOffs++] = { e.track, e.note, e.len };
        }

        auto end = std::min (numSamples, pos + controlBlock);
        if (ei < numEvs) end = std::min (end, std::max (pos + 1, evs[(size_t) ei].offset));
        const auto n = end - pos;

        const auto ppq = seq->ppqAt (pos);
        // v0.46 : crossfader des scènes relu depuis sa courbe du morceau (sauf s'il est tenu à la main)
        auto faderTarget = bi.fader;
        bool faderFromLane = false;
        if (const auto* sg = song.get(); sg != nullptr && sg->autoRead && seq->isPlayingInBlock() && ! isHeld (xfadeLane))
            if (const auto* l = sg->laneFor (xfadeLane); l != nullptr && l->enabled && ! l->pts.empty())
                if (const auto v = l->valueAt (sg->wrap (ppq)); v >= 0.0f) { faderTarget = v; faderFromLane = true; }
        laneFader.store (faderFromLane ? faderTarget : -1.0f, std::memory_order_relaxed);
        smoothedFader = faderTarget + (smoothedFader - faderTarget) * faderCoef;
        if (seq->isPlayingInBlock() && lastPpq >= 0.0 && std::floor (ppq / 4.0) != std::floor (lastPpq / 4.0))
            for (auto& c : *state) for (auto& m : c.mods) m.barStart();
        lastPpq = seq->isPlayingInBlock() ? ppq : -1.0;

        // v0.46 fix22 : pavé vectoriel — poids des coins lissés comme le crossfader
        for (size_t k = 0; k < 4; ++k)
        {
            const auto t = bi.vecOn ? std::clamp (bi.vecW[k], 0.0f, 1.0f) : 0.0f;
            smoothedVec[k] = t + (smoothedVec[k] - t) * faderCoef;
        }
        vecScenesOn = bi.vecOn && bi.vecMode == 0;
        for (int i = 0; i < numInserts; ++i)
        {
            float w = 0.0f; bool mapped = false;
            if (bi.vecOn && bi.vecMode == 1)
                for (size_t k = 0; k < 4; ++k)
                    if (bi.vecFx[k] == i + 1) { mapped = true; w += smoothedVec[k]; }
            if (mapped && ! vecFxMapped[(size_t) i]) vecFxGain[(size_t) i] = 1.0f; // prise en main : part du son traité
            vecFxMapped[(size_t) i] = mapped;
            vecFxTarget[(size_t) i] = std::clamp (w, 0.0f, 1.0f);
        }
        updateParams (*g, *sc, bi, smoothedFader, n, ppq);
        runArpeggiators (*g, ppq, seq->isPlayingInBlock(), bi.bpm, n, pos);
        for (int t = 0; t < numTracks; ++t)
            if (auto* inst = g->ch[(size_t) t].instrument)
            {
                const auto& c = (*state)[(size_t) t];
                inst->setInput (inL != nullptr ? inL + pos : nullptr, inR != nullptr ? inR + pos : nullptr);
                inst->setTempo (bi.bpm);
                inst->setPosition (ppq, seq->isPlayingInBlock());
                inst->setExpression (c.bend, c.wheel, c.pressure);
            }
        curInL = inL != nullptr ? inL + pos : nullptr; curInR = inR != nullptr ? inR + pos : nullptr;
        if (curInL != nullptr)
        {
            float pk = 0.0f;
            for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (curInL[i]), curInR != nullptr ? std::abs (curInR[i]) : 0.0f));
            inPeak.store (std::max (pk, inPeak.load (std::memory_order_relaxed) * 0.97f), std::memory_order_relaxed);
        }
        renderChunk (*g, outL + pos, outR + pos, n, bi.bpm, ppq, seq->isPlayingInBlock());
        midiOut.at = std::min (pos + n, numSamples - 1); // fins de notes : à la fin du bloc

        // fins de notes
        for (int i = 0; i < numPendingOffs;)
        {
            auto& po = pendingOffs[(size_t) i];
            po.remaining -= (double) n;
            if (po.remaining <= 0.0)
            {
                dispatch (*g, po.track, po.note, 0.0f, false);
                po = pendingOffs[(size_t) --numPendingOffs];
            }
            else ++i;
        }
        pos = end;
    }

    // après la 1.1 : au même échantillon, la banque et le changement de programme partent avant les notes
    // (son verrouillé sur un pas : l'appareil change de son avant de jouer la note du pas). Tri stable.
    {
        auto first = [] (const MidiOutQueue::Ev& e) { const int st = e.b0 & 0xF0; return st == 0xC0 || (st == 0xB0 && (e.b1 == 0 || e.b1 == 32)); };
        auto& q = midiOut;
        for (int i = 1; i < q.count; ++i)
        {
            const auto x = q.ev[(size_t) i];
            if (! first (x)) continue;
            int j = i;
            while (j > 0 && q.ev[(size_t) (j - 1)].offset == x.offset && ! first (q.ev[(size_t) (j - 1)])) { q.ev[(size_t) j] = q.ev[(size_t) (j - 1)]; --j; }
            q.ev[(size_t) j] = x;
        }
    }

    seq->endBlock();
}

} // namespace grv::engine

//==============================================================================
// Morceau (arrangeur)

namespace grv::engine
{

void Engine::setSong (Song next, double nowMs)
{
    // bornes : rien d'invalide n'atteint le fil audio
    if (next.rows.size() > (size_t) maxSongRows) next.rows.resize ((size_t) maxSongRows);
    for (auto& row : next.rows)
    {
        row.bars = std::clamp (row.bars, 1, maxRowBars);
        row.scene = std::clamp (row.scene, -1, numScenes - 1);
        for (auto& p : row.pat) p = (std::int8_t) std::clamp ((int) p, songKeep, Sequencer::numPatterns - 1);
    }
    next.loopFrom = std::clamp (next.loopFrom, 0, maxSongRows - 1);
    next.loopTo = std::clamp (next.loopTo, -1, maxSongRows - 1);
    next.loopCount = std::clamp (next.loopCount, 0, 999);
    if (next.loopTo >= 0 && next.loopTo < next.loopFrom) std::swap (next.loopFrom, next.loopTo);
    std::vector<AutoLane> lanes;
    for (auto& l : next.lanes)
    {
        if ((int) lanes.size() >= maxAutoLanes) break;
        if (! isSpecialLane (l.global) && (l.global < 0 || l.global >= numSlots || describe (l.global) == nullptr)) continue;
        bool dup = false;
        for (const auto& k : lanes) dup = dup || k.global == l.global;
        if (dup) continue;
        std::vector<AutoPoint> pts;
        for (const auto& p : l.pts)
            if (std::isfinite (p.beat) && std::isfinite (p.norm) && p.beat >= 0.0f && p.beat < 1.0e6f)
                pts.push_back ({ p.beat, autoNorm (p.norm) });
        std::stable_sort (pts.begin(), pts.end(), [] (const AutoPoint& a, const AutoPoint& b) { return a.beat < b.beat; });
        if (pts.size() > (size_t) maxAutoPoints) pts.resize ((size_t) maxAutoPoints);
        l.pts = std::move (pts);
        lanes.push_back (std::move (l));
    }
    next.lanes = std::move (lanes);
    std::vector<PatternLane> pls;
    for (auto& pl : next.patLanes)
    {
        if ((int) pls.size() >= maxPatLanes) break;
        auto& l = pl.lane;
        if (pl.track < 0 || pl.track >= numTracks || pl.pattern < 0 || pl.pattern >= Sequencer::numPatterns) continue;
        if (l.global < 0 || l.global >= numSlots || describe (l.global) == nullptr) continue;
        bool dup = false;
        for (const auto& k : pls) dup = dup || (k.track == pl.track && k.pattern == pl.pattern && k.lane.global == l.global);
        if (dup) continue;
        std::vector<AutoPoint> pts;
        for (const auto& p : l.pts)
            if (std::isfinite (p.beat) && std::isfinite (p.norm) && p.beat >= 0.0f && p.beat <= (float) Sequencer::maxSteps)
                pts.push_back ({ p.beat, autoNorm (p.norm) });
        std::stable_sort (pts.begin(), pts.end(), [] (const AutoPoint& a, const AutoPoint& b) { return a.beat < b.beat; });
        if (pts.size() > (size_t) maxAutoPoints) pts.resize ((size_t) maxAutoPoints);
        if (pts.empty()) continue;
        l.pts = std::move (pts);
        pls.push_back (std::move (pl));
    }
    next.patLanes = std::move (pls);
    if (capRow >= (int) next.rows.size()) capRow = -1;
    song.publish (std::make_unique<Song> (std::move (next)), bin, nowMs);
    songVersion.fetch_add (1);
}

void Engine::remapSongLanes (const std::function<int (int)>& f, double nowMs)
{
    const auto& cur = *song.editable();
    bool changed = false;
    for (const auto& l : cur.lanes) changed = changed || f (l.global) != l.global;
    for (const auto& pl : cur.patLanes) changed = changed || f (pl.lane.global) != pl.lane.global;
    if (! changed) return;
    Song next = cur;
    for (auto& l : next.lanes) l.global = f (l.global);
    next.lanes.erase (std::remove_if (next.lanes.begin(), next.lanes.end(), [] (const AutoLane& l) { return l.global < 0; }), next.lanes.end());
    for (auto& pl : next.patLanes) pl.lane.global = f (pl.lane.global);
    next.patLanes.erase (std::remove_if (next.patLanes.begin(), next.patLanes.end(), [] (const PatternLane& pl) { return pl.lane.global < 0; }), next.patLanes.end());
    setSong (std::move (next), nowMs);
}

void Engine::clearPatternLanes (int track, int pattern, double nowMs)
{
    const auto& cur = *song.editable();
    bool any = false;
    for (const auto& pl : cur.patLanes) any = any || (pl.track == track && (pattern < 0 || pl.pattern == pattern));
    if (! any) return;
    Song next = cur;
    next.patLanes.erase (std::remove_if (next.patLanes.begin(), next.patLanes.end(),
                                         [&] (const PatternLane& pl) { return pl.track == track && (pattern < 0 || pl.pattern == pattern); }), next.patLanes.end());
    setSong (std::move (next), nowMs);
}

void Engine::copyPatternLanes (int srcTrack, int from, int track, int to, double nowMs)
{
    if (srcTrack == track && from == to) return;
    const auto& cur = *song.editable();
    Song next = cur;
    next.patLanes.erase (std::remove_if (next.patLanes.begin(), next.patLanes.end(),
                                         [&] (const PatternLane& pl) { return pl.track == track && pl.pattern == to; }), next.patLanes.end());
    for (const auto& pl : cur.patLanes)
    {
        if (pl.track != srcTrack || pl.pattern != from) continue;
        auto g = pl.lane.global;
        if (srcTrack != track && channelOf (g) == srcTrack)
        {
            // vers une autre piste : le même réglage, s'il existe là-bas
            const auto ng = globalSlot (track, g % slotsPerChannel);
            const auto* a = describe (g); const auto* b = describe (ng);
            if (a == nullptr || b == nullptr || a->id != b->id) continue;
            g = ng;
        }
        PatternLane copy = pl;
        copy.track = track; copy.pattern = to; copy.lane.global = g;
        next.patLanes.push_back (std::move (copy));
    }
    setSong (std::move (next), nowMs);
}

void Engine::holdGlobal (int global) noexcept
{
    const auto until = sampleClock.load (std::memory_order_relaxed) + (std::int64_t) (0.4 * sampleRate);
    for (int i = 0; i < numHeld; ++i)
        if (heldGlobal[(size_t) i].load (std::memory_order_relaxed) == global) { heldUntil[(size_t) i].store (until, std::memory_order_relaxed); return; }
    const auto k = (size_t) (heldNext.fetch_add (1, std::memory_order_relaxed) % numHeld);
    heldUntil[k].store (until, std::memory_order_relaxed);
    heldGlobal[k].store (global, std::memory_order_relaxed);
}

bool Engine::isHeld (int global) const noexcept
{
    const auto now = sampleClock.load (std::memory_order_relaxed);
    for (int i = 0; i < numHeld; ++i)
        if (heldGlobal[(size_t) i].load (std::memory_order_relaxed) == global && heldUntil[(size_t) i].load (std::memory_order_relaxed) > now) return true;
    return false;
}

bool Engine::isSongRecording() const noexcept
{
    // v0.46 fix7 : « Enr. les mouvements », ou ENR. en mode morceau : les gestes vont au morceau
    // (boutons, tirettes, égaliseurs, coupures, solos, groupes, crossfader). ENR. en boucle (mode
    // morceau éteint) : les réglages d'une piste vont dans la boucle du pattern qu'elle joue.
    if (recMoves.load (std::memory_order_relaxed)) return true;
    if (! seq->isRecording()) return false;
    const auto* sg = song.get();
    return sg != nullptr && sg->songMode && ! sg->rows.empty();
}

void Engine::recordMoveUi (int global, float norm)
{
    if (global < 0 || global >= numSlots || ! std::isfinite (norm)) return;
    holdGlobal (global); // la courbe se tait pendant le geste : on entend le bouton
    if (uiMoves.size() >= 20000) return;
    // ENR. : un réglage de piste devient une courbe lisse du pattern qu'elle joue — sauf si
    // « Enr. les mouvements » est allumé : alors tout va au morceau (v0.46, il l'emporte)
    const auto ch = channelOf (global);
    if (! isSongRecording() && seq->isRecording() && seq->isRunning() && ch >= 0 && ch < numTracks)
    {
        const auto phase = (double) trackPhase[(size_t) ch].load (std::memory_order_relaxed);
        const auto p = seq->getPlaying (ch);
        if (phase >= 0.0 && p >= 0 && p < Sequencer::numPatterns)
        {
            uiMoves.push_back ({ global, std::clamp (norm, 0.0f, 1.0f), phase, ch, p });
            return;
        }
    }
    if (! isSongRecording() || ! seq->isRunning()) return;
    const auto beat = seq->getPpq();
    if (beat < 0.0) return;
    uiMoves.push_back ({ global, std::clamp (norm, 0.0f, 1.0f), beat });
}

void Engine::recordSwitchUi (int track, bool solo, bool on)
{
    // v0.46 fix7 : « track » est un canal : piste (0 à 15) ou retour / principal / groupe
    if (track < 0 || track >= numChannels || (track >= numTracks && solo && ! isGroupChannel (track))) return;
    const auto g = track < numTracks ? (solo ? soloLane (track) : muteLane (track)) : (solo ? busSoloLane (track) : busMuteLane (track));
    holdGlobal (g); // la courbe ne reprend pas la main tout de suite
    if (! isSongRecording() || ! seq->isRunning() || uiMoves.size() >= 20000) return;
    if (const auto beat = seq->getPpq(); beat >= 0.0) uiMoves.push_back ({ g, on ? 1.0f : 0.0f, beat });
}

void Engine::recordFaderUi (float position)
{
    holdGlobal (xfadeLane);
    if (! isSongRecording() || ! seq->isRunning() || uiMoves.size() >= 20000 || ! std::isfinite (position)) return;
    if (const auto beat = seq->getPpq(); beat >= 0.0) uiMoves.push_back ({ xfadeLane, std::clamp (position, 0.0f, 1.0f), beat });
}

void Engine::recordFaderAudio (float position) noexcept
{
    holdGlobal (xfadeLane);
    if (! isSongRecording() || ! seq->isPlayingInBlock() || ! std::isfinite (position)) return;
    audioMoves->push ({ xfadeLane, std::clamp (position, 0.0f, 1.0f), seq->getBlockStartPpq() });
}

void Engine::recordSwitchAudio (int track, bool solo, bool on) noexcept
{
    if (track < 0 || track >= numChannels || (track >= numTracks && solo && ! isGroupChannel (track))) return;
    const auto g = track < numTracks ? (solo ? soloLane (track) : muteLane (track)) : (solo ? busSoloLane (track) : busMuteLane (track));
    holdGlobal (g);
    if (! isSongRecording() || ! seq->isPlayingInBlock()) return;
    audioMoves->push ({ g, on ? 1.0f : 0.0f, seq->getBlockStartPpq() });
}

void Engine::recordMoveAudio (int global, float norm) noexcept
{
    if (global < 0 || global >= numSlots || ! std::isfinite (norm)) return;
    holdGlobal (global);
    const auto ch = channelOf (global);
    if (! isSongRecording() && seq->isRecording() && seq->isPlayingInBlock() && ch >= 0 && ch < numTracks)
    {
        const auto phase = seq->stepPhase (ch, seq->getBlockStartPpq());
        const auto p = seq->getPlaying (ch);
        if (phase >= 0.0 && p >= 0 && p < Sequencer::numPatterns)
        {
            audioMoves->push ({ global, std::clamp (norm, 0.0f, 1.0f), phase, ch, p });
            return;
        }
    }
    if (! isSongRecording() || ! seq->isPlayingInBlock()) return;
    audioMoves->push ({ global, std::clamp (norm, 0.0f, 1.0f), seq->getBlockStartPpq() });
}

void Engine::storeArpMarks (const std::vector<ArpMark>& marks, double nowMs)
{
    if (marks.empty()) return;
    auto e = std::make_unique<PatternExtras> (*extras.editable());
    bool changed = false;
    for (const auto& m : marks)
    {
        if (! validStep (m.track, m.pattern, m.lane, m.pos)) continue;
        e->steps[PatternExtras::key (m.track, m.pattern, m.lane, m.pos)].direct = true;
        changed = true;
    }
    if (changed) publishExtras (std::move (e), nowMs);
}

void Engine::addMove (Song& sg, const MoveRecord& m, double nowMs)
{
    const bool inPattern = m.track >= 0;
    double songAt = m.beat;
    if (! inPattern)
    {
        // v0.46 fix7 : pendant « Enr. le jeu » (mode morceau éteint), la position du morceau est celle
        // des rangées qui s'ajoutent, pas celle de la lecture (qui a pu commencer avant) ; avant la
        // première mesure enregistrée, le mouvement n'a pas encore de place dans le morceau
        if (recPlay.load() && ! sg.songMode) { if (! capOffsetOk) return; songAt = m.beat + capOffset; }
        else songAt = sg.wrap (m.beat);
    }
    const auto beat = (float) songAt;
    if (! std::isfinite (beat) || beat < 0.0f) return;
    AutoLane* lane = nullptr;
    if (inPattern)
    {
        if (const auto i = sg.patLaneIndex (m.track, m.pattern, m.global); i >= 0) lane = &sg.patLanes[(size_t) i].lane;
        else
        {
            if ((int) sg.patLanes.size() >= maxPatLanes || describe (m.global) == nullptr) return;
            sg.patLanes.push_back ({ m.track, m.pattern, {} });
            lane = &sg.patLanes.back().lane;
            lane->global = m.global;
        }
    }
    else
    {
        for (auto& l : sg.lanes) if (l.global == m.global) lane = &l;
        if (lane == nullptr)
        {
            if ((int) sg.lanes.size() >= maxAutoLanes || (! isSpecialLane (m.global) && describe (m.global) == nullptr)) return;
            sg.lanes.push_back ({});
            lane = &sg.lanes.back();
            lane->global = m.global;
        }
        // v0.46 : interrupteur (coupure, solo) : une marche nette — l'ancien état juste avant, le nouveau ici
        if (isSwitchLane (m.global) || isBusSwitchLane (m.global))
        {
            auto& pts = lane->pts;
            const auto before = beat - 1.0e-3f;
            pts.erase (std::remove_if (pts.begin(), pts.end(), [&] (const AutoPoint& p) { return p.beat >= before - 1.0e-3f && p.beat <= beat + 1.0e-6f; }), pts.end());
            if ((int) pts.size() + 2 > maxAutoPoints) return;
            const auto at = [&] (float b) { return std::upper_bound (pts.begin(), pts.end(), b, [] (float x, const AutoPoint& p) { return x < p.beat; }); };
            if (before >= 0.0f) pts.insert (at (before), { before, m.norm >= 0.5f ? 0.0f : 1.0f });
            pts.insert (at (beat), { beat, m.norm >= 0.5f ? 1.0f : 0.0f });
            return;
        }
    }
    // prise en cours sur ce réglage : les anciens points qu'elle recouvre sont remplacés
    // (pattern : quand la boucle repart, la prise repart du début du pattern)
    struct Take { int global, track, pattern; double beat, ms; };
    static thread_local std::vector<Take> takes; // fil de l'interface seulement
    Take* take = nullptr;
    for (auto& t : takes) if (t.global == m.global && t.track == m.track && (! inPattern || t.pattern == m.pattern)) take = &t;
    const bool going = take != nullptr && nowMs - take->ms < 600.0 && beat >= take->beat - 1.0e-6;
    const float from = going ? (float) take->beat : beat - 1.0e-3f;
    auto& pts = lane->pts;
    pts.erase (std::remove_if (pts.begin(), pts.end(), [&] (const AutoPoint& p) { return p.beat > from && p.beat <= beat + 1.0e-6f; }), pts.end());
    if (going && ! pts.empty())
    {
        // un point au plus par 1/64 de noire dans une prise : le dernier prend la nouvelle valeur
        auto it = std::lower_bound (pts.begin(), pts.end(), (float) take->beat - 1.0e-6f, [] (const AutoPoint& p, float b) { return p.beat < b; });
        if (it != pts.end() && std::abs (it->beat - (float) take->beat) < 1.0e-6f && beat - it->beat < 1.0f / 64.0f) { it->norm = m.norm; it->beat = beat; }
        else if ((int) pts.size() < maxAutoPoints) pts.insert (std::upper_bound (pts.begin(), pts.end(), beat, [] (float b, const AutoPoint& p) { return b < p.beat; }), { beat, m.norm });
    }
    else if ((int) pts.size() < maxAutoPoints)
        pts.insert (std::upper_bound (pts.begin(), pts.end(), beat, [] (float b, const AutoPoint& p) { return b < p.beat; }), { beat, m.norm });
    if (take == nullptr) { takes.push_back ({ m.global, m.track, m.pattern, beat, nowMs }); if (takes.size() > 256) takes.erase (takes.begin()); }
    else { take->beat = beat; take->ms = nowMs; }
}

void Engine::songTick (double nowMs)
{
    std::unique_ptr<Song> work;
    auto edit = [&]() -> Song& { if (work == nullptr) work = std::make_unique<Song> (*song.editable()); return *work; };

    BarCapture bc;
    while (barCaptures.pop (bc))
    {
        if (! recPlay.load()) continue;
        auto& sg = edit();
        auto& rows = sg.rows;
        bool placed = false;
        if (capRow >= 0 && capRow < (int) rows.size() && capLastBar == bc.bar - 1
            && rows[(size_t) capRow].pat == bc.pat && rows[(size_t) capRow].bars < maxRowBars)
        {
            ++rows[(size_t) capRow].bars;
            placed = true;
        }
        else if ((int) rows.size() < maxSongRows)
        {
            SongRow row;
            row.bars = 1;
            row.pat = bc.pat;
            rows.push_back (row);
            capRow = (int) rows.size() - 1;
            placed = true;
        }
        capLastBar = bc.bar;
        // v0.46 fix7 : la mesure « bc.bar » de la lecture est la dernière mesure de la rangée capRow
        if (placed)
        {
            capOffset = sg.rowStart (capRow) + 4.0 * (rows[(size_t) capRow].bars - 1) - 4.0 * (double) bc.bar;
            capOffsetOk = true;
        }
        else capOffsetOk = false; // morceau plein : plus rien ne s'ajoute
    }
    // ENR. : notes d'arpège écrites dans les patterns
    {
        std::vector<ArpMark> marks;
        ArpMark am;
        while (arpMarks->pop (am)) marks.push_back (am);
        storeArpMarks (marks, nowMs);
    }
    MoveRecord m;
    while (audioMoves->pop (m)) addMove (edit(), m, nowMs);
    for (const auto& u : uiMoves) addMove (edit(), u, nowMs);
    uiMoves.clear();

    if (work != nullptr)
    {
        song.publish (std::move (work), bin, nowMs);
        songVersion.fetch_add (1);
    }
}

void Engine::songBlock (const Song& sg) noexcept
{
    const bool running = seq->isPlayingInBlock();
    const auto p0 = seq->getBlockStartPpq(), p1 = seq->getBlockEndPpq();
    songBeatShown.store (running ? sg.wrap (p0) : -1.0, std::memory_order_relaxed);
    if (! running || ! sg.songMode || sg.rows.empty())
    {
        songWasActive = false; songRowNow = -1; songStoppedAtEnd = false; songKey = -1;
        songRowShown.store (-1, std::memory_order_relaxed);
        return;
    }
    auto launch = [&] (int r, double at)
    {
        const auto& row = sg.rows[(size_t) r];
        for (int t = 0; t < numTracks; ++t)
        {
            const int v = row.pat[(size_t) t];
            if (v == songKeep) continue;
            seq->forceSwitch (t, v == songStop ? Sequencer::stopPattern : v, at);
        }
        if (row.scene >= 0) songSceneOut.store (row.scene, std::memory_order_relaxed);
        songRowNow = r;
    };
    auto stopAll = [&] (double at)
    {
        for (int t = 0; t < numTracks; ++t) seq->forceSwitch (t, Sequencer::stopPattern, at);
        seq->requestStop();
        if (auto* tp = audioTap.load (std::memory_order_acquire)) tp->tapStopAt (at); // v0.46.2 : prise coupée pile
        songStoppedAtEnd = true;
        songRowNow = -1;
    };
    const auto w0 = sg.wrap (p0);
    const auto r0 = sg.rowAt (w0);
    // passage dans le morceau (en boucle) : une même rangée rejouée au tour suivant repart aussi
    const long long pass = sg.loopPass (p0);
    const auto key = pass * maxSongRows + r0;
    if (! songWasActive)
    {
        songWasActive = true;
        songStoppedAtEnd = false;
        if (r0 >= 0) { launch (r0, p0); songKey = key; }
    }
    else if (r0 >= 0 && key != songKey && ! songStoppedAtEnd)
    {
        launch (r0, p0);                                   // la rangée a commencé pile au début du bloc (ou boucle)
        songKey = key;
    }
    else if (r0 < 0 && songRowNow >= 0 && ! songStoppedAtEnd)
        stopAll (p0);                                      // fin du morceau pile au début du bloc
    if (r0 >= 0 && songKey == key)
    {
        // fin de la rangée dans ce bloc : la suivante part exactement à sa mesure
        const auto boundary = p0 + (sg.rowStart (r0 + 1) - w0);
        if (boundary > p0 + 1.0e-9 && boundary < p1 - 1.0e-9)
        {
            // en boucle : la rangée qui suit la dernière bouclée est la première bouclée ; après le
            // dernier passage (nombre de passages fixé), la suite du morceau puis l'arrêt
            const bool lastPass = sg.loopCount > 0 && pass >= sg.loopCount - 1;
            const auto last = sg.loop && ! lastPass ? sg.loopLast() : (int) sg.rows.size() - 1;
            if (r0 + 1 <= last) { launch (r0 + 1, boundary); songKey = pass * maxSongRows + r0 + 1; }
            else if (sg.loop) { launch (sg.loopFirst(), boundary); songKey = (pass + 1) * maxSongRows + sg.loopFirst(); }
            else stopAll (boundary);
        }
    }
    songRowShown.store (songRowNow, std::memory_order_relaxed);
}

void Engine::captureBar() noexcept
{
    const bool running = seq->isPlayingInBlock();
    const auto* sg = song.get();
    if (! recPlay.load (std::memory_order_relaxed) || ! running || (sg != nullptr && sg->songMode)) { lastCapBar = -1; return; }
    const auto p0 = seq->getBlockStartPpq(), p1 = seq->getBlockEndPpq();
    const auto k = (long long) std::ceil (p0 / 4.0 - 1.0e-9);
    if (4.0 * (double) k >= p1 - 1.0e-9 || k == lastCapBar) return;
    lastCapBar = k;
    BarCapture bc;
    bc.bar = k;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto p = seq->getPlaying (t);
        bc.pat[(size_t) t] = (std::int8_t) (p >= 0 && p < Sequencer::numPatterns ? p : songStop);
    }
    barCaptures.push (bc);
}

} // namespace grv::engine
