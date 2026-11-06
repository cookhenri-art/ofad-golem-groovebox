/*
    Groovebox — OFAD — AGPL-3.0-or-later
*/

#include "Bridge.h"
#include "../synth/Wavetable.h"

#include <complex>
#include "Catalog.h"

#include <cmath>
#include <cstdio>

namespace grv::engine::bridge
{

std::string quote (const std::string& s)
{
    std::string out;
    out.reserve (s.size() + 8);
    out += '"';
    for (size_t i = 0; i < s.size();)
    {
        const auto c = (unsigned char) s[i];
        std::uint32_t cp = c;
        int len = 1;
        if (c >= 0xF0 && i + 3 < s.size()) { cp = ((c & 0x07u) << 18) | ((s[i + 1] & 0x3Fu) << 12) | ((s[i + 2] & 0x3Fu) << 6) | (s[i + 3] & 0x3Fu); len = 4; }
        else if (c >= 0xE0 && i + 2 < s.size()) { cp = ((c & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu); len = 3; }
        else if (c >= 0xC0 && i + 1 < s.size()) { cp = ((c & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu); len = 2; }
        else if (c >= 0x80) { cp = 0xFFFD; len = 1; }
        i += (size_t) len;

        char buf[16];
        if (cp == '"') out += "\\\"";
        else if (cp == '\\') out += "\\\\";
        else if (cp < 0x20) { std::snprintf (buf, sizeof buf, "\\u%04x", cp); out += buf; }
        else if (cp < 0x80) out += (char) cp;
        else if (cp < 0x10000) { std::snprintf (buf, sizeof buf, "\\u%04x", cp); out += buf; }
        else
        {
            cp -= 0x10000;
            std::snprintf (buf, sizeof buf, "\\u%04x\\u%04x", 0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
            out += buf;
        }
    }
    out += '"';
    return out;
}

namespace
{
std::string num (double v)
{
    if (! std::isfinite (v)) return "0";
    char buf[32];
    std::snprintf (buf, sizeof buf, "%.6g", v);
    return buf;
}

std::string paramJson (const ParamDesc& p)
{
    std::string o = "{\"id\":" + quote (p.id) + ",\"name\":" + quote (p.name) + ",\"unit\":" + quote (p.unit)
                  + ",\"min\":" + num (p.min) + ",\"max\":" + num (p.max) + ",\"def\":" + num (p.def) + ",\"skew\":" + num (p.skew);
    if (p.integer) o += ",\"integer\":true";
    if (! p.choices.empty())
    {
        o += ",\"choices\":[";
        for (size_t i = 0; i < p.choices.size(); ++i) o += (i ? "," : "") + quote (p.choices[i]);
        o += "]";
    }
    return o + "}";
}

std::string moduleJson (const ModuleDesc& d)
{
    std::string o = "{\"type\":" + quote (d.type) + ",\"name\":" + quote (d.name) + ",\"family\":" + quote (d.family)
                  + ",\"principle\":" + quote (d.principle) + (d.form.empty() ? std::string() : ",\"form\":" + quote (d.form)) + (d.role.empty() ? std::string() : ",\"role\":" + quote (d.role)) + ",\"drumKit\":" + (d.drumKit ? "true" : "false") + (d.laneNotes ? ",\"laneNotes\":true" : "") + ",\"params\":[";
    for (size_t i = 0; i < d.params.size(); ++i) o += (i ? "," : "") + paramJson (d.params[i]);
    o += "],\"sections\":[";
    for (size_t i = 0; i < d.sections.size(); ++i)
    {
        const auto& s = d.sections[i];
        o += (i ? "," : "") + std::string ("{\"id\":") + quote (s.id) + ",\"name\":" + quote (s.name) + ",\"visual\":" + quote (s.visual)
           + ",\"tab\":" + quote (s.tab) + ",\"params\":[";
        bool first = true;
        for (const auto& id : s.params)
        {
            const auto k = d.indexOf (id);
            if (k < 0) continue;
            o += (first ? "" : ",") + std::to_string (k);
            first = false;
        }
        o += "]}";
    }
    o += "],\"presets\":[";
    for (size_t i = 0; i < d.presets.size(); ++i) o += (i ? "," : "") + quote (d.presets[i].name);
    // banque de chaque préréglage d'usine (même ordre) ; vide = « Classiques »
    o += "],\"presetCats\":[";
    for (size_t i = 0; i < d.presets.size(); ++i) o += (i ? "," : "") + quote (d.presets[i].category);
    o += "],\"padNames\":[";
    for (size_t i = 0; i < d.padNames.size(); ++i) o += (i ? "," : "") + quote (d.padNames[i]);
    o += "],\"padKinds\":[";
    for (size_t i = 0; i < d.padKinds.size(); ++i) o += (i ? "," : "") + quote (d.padKinds[i]);
    o += "]";
    if (! d.waveShapes.empty())
    {
        o += ",\"waveShapes\":[";
        for (size_t i = 0; i < d.waveShapes.size(); ++i)
        {
            o += i ? ",[" : "[";
            for (size_t k = 0; k < d.waveShapes[i].size(); ++k)
            {
                char b[16];
                std::snprintf (b, sizeof b, "%.3g", (double) d.waveShapes[i][k]);
                o += (k ? "," : "") + std::string (b);
            }
            o += "]";
        }
        o += "]";
    }
    if (! d.machineNames.empty())
    {
        o += ",\"machines\":[";
        for (size_t i = 0; i < d.machineNames.size(); ++i)
        {
            o += (i ? "," : "") + std::string ("{\"name\":") + quote (d.machineNames[i]) + ",\"labels\":[";
            const auto& lab = i < d.machineLabels.size() ? d.machineLabels[i] : std::vector<std::string> {};
            for (size_t k = 0; k < lab.size(); ++k) o += (k ? "," : "") + quote (lab[k]);
            o += "]}";
        }
        o += "]";
    }
    return o + "}";
}

std::string valuesJson (const Engine& e, int channel, int base, size_t count)
{
    std::string o = "[";
    for (size_t i = 0; i < count; ++i)
        o += (i ? "," : "") + num (e.getParam (globalSlot (channel, base + (int) i)));
    return o + "]";
}

std::string hexRow (const Sequencer& s, int t, int l, int what)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string o;
    o.reserve (Sequencer::maxSteps * 3);
    for (int st = 0; st < Sequencer::maxSteps; ++st)
    {
        int v = 0;
        switch (what)
        {
            case 0: v = s.getStep (t, l, st); break;
            case 1: v = s.getNote (t, l, st); break;
            case 2: v = s.getLength8 (t, l, st); break;
            default: v = s.getMicro (t, l, st) & 0xFF; break;
        }
        if (what == 2) o += digits[(v >> 8) & 15]; // durées : trois chiffres (jusqu'à 1024 huitièmes)
        o += digits[(v >> 4) & 15];
        o += digits[v & 15];
    }
    return o;
}

std::string locksJson (const std::vector<std::pair<int, float>>& locks)
{
    std::string o = "[";
    for (size_t i = 0; i < locks.size(); ++i) o += (i ? "," : "") + std::string ("[") + std::to_string (locks[i].first) + "," + num (locks[i].second) + "]";
    return o + "]";
}
} // namespace

std::string catalogJson()
{
    std::string o = "{\"instruments\":[";
    const auto& inst = instrumentCatalog();
    for (size_t i = 0; i < inst.size(); ++i) o += (i ? "," : "") + moduleJson (*inst[i]);
    o += "],\"effects\":[";
    const auto& fx = effectCatalog();
    for (size_t i = 0; i < fx.size(); ++i) o += (i ? "," : "") + moduleJson (*fx[i]);
    o += "],\"mixerTrack\":" + moduleJson (mixerDesc (0)) + ",\"mixerReturn\":" + moduleJson (mixerDesc (numTracks))
       + ",\"mixerMaster\":" + moduleJson (mixerDesc (masterChannel)) + ",\"modSource\":" + moduleJson (modSourceDesc());
    o += ",\"layout\":{\"numTracks\":" + std::to_string (numTracks) + ",\"numReturns\":" + std::to_string (numReturns)
       + ",\"masterChannel\":" + std::to_string (masterChannel) + ",\"numGroups\":" + std::to_string (numGroups)
       + ",\"groupBase\":" + std::to_string (groupBase) + ",\"slotsPerChannel\":" + std::to_string (slotsPerChannel)
       + ",\"mixerBase\":" + std::to_string (mixerBase) + ",\"instrumentBase\":" + std::to_string (instrumentBase)
       + ",\"instrumentSlots\":" + std::to_string (instrumentSlots) + ",\"insertBase\":" + std::to_string (insertBase) + ",\"insertSlots\":" + std::to_string (insertSlots)
       + ",\"numInserts\":" + std::to_string (numInserts) + ",\"modSourceBase\":" + std::to_string (modSourceBase)
       + ",\"modSourceSlots\":" + std::to_string (modSourceSlots) + ",\"numModSources\":" + std::to_string (numModSources)
       + ",\"modAmountBase\":" + std::to_string (modAmountBase) + ",\"padBypassBase\":" + std::to_string (padBypassBase)
       + ",\"padInsertBase\":" + std::to_string (padInsertBase) + ",\"padInserts\":" + std::to_string (padInserts) + ",\"numScenes\":" + std::to_string (numScenes)
       + ",\"maxSteps\":" + std::to_string (Sequencer::maxSteps) + ",\"numLanes\":" + std::to_string (Sequencer::numLanes)
       + ",\"numPatterns\":" + std::to_string (Sequencer::numPatterns) + "}";
    o += ",\"chains\":[";
    const auto& chains = chainCatalog();
    for (size_t i = 0; i < chains.size(); ++i)
    {
        o += (i ? "," : "") + std::string ("{\"name\":") + quote (chains[i].name) + ",\"use\":" + quote (chains[i].use) + ",\"inserts\":[";
        for (size_t k = 0; k < chains[i].inserts.size(); ++k)
            o += (k ? "," : "") + std::string ("[") + quote (chains[i].inserts[k].first) + "," + quote (chains[i].inserts[k].second) + "]";
        o += "]}";
    }
    o += "]}";
    return o;
}

std::string tracksJson (const Engine& e)
{
    std::string o = "{\"tracks\":[";
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& i = e.getTrackInfo (t);
        o += (t ? "," : "") + std::string ("{\"name\":") + quote (i.name) + ",\"color\":" + std::to_string (i.color) + ",\"out\":" + std::to_string (i.output)
           + ",\"instrument\":" + quote (i.instrumentType) + ",\"preset\":" + quote (i.preset)
           + (i.speech.empty() ? std::string() : ",\"speech\":" + quote (i.speech))
           + ",\"mute\":" + (e.sequencer().getTrackMute (t) ? "true" : "false") + ",\"solo\":" + (e.sequencer().getTrackSolo (t) ? "true" : "false") + "}";
    }
    o += "],\"inserts\":[";
    for (int ch = 0; ch < numChannels; ++ch)
    {
        o += (ch ? "," : "") + std::string ("[");
        for (int s = 0; s < numInserts; ++s) o += (s ? "," : "") + quote (e.getInsertTypes (ch)[(size_t) s]);
        o += "]";
    }
    // v0.31 : retours, principal et groupes (canaux 16 à 24) : coupure, solo, nom
    o += "],\"buses\":[";
    for (int ch = numTracks; ch < numChannels; ++ch)
        o += (ch > numTracks ? "," : "") + std::string ("{\"mute\":") + (e.getBusMute (ch) ? "true" : "false") + ",\"solo\":" + (e.getBusSolo (ch) ? "true" : "false")
           + ",\"name\":" + quote (ch >= groupBase ? e.getGroupName (ch - groupBase) : std::string()) + "}";
    o += "],\"insertPresets\":[";
    for (int ch = 0; ch < numChannels; ++ch)
    {
        o += (ch ? "," : "") + std::string ("[");
        for (int s = 0; s < numInserts; ++s) o += (s ? "," : "") + quote (e.getInsertPresets (ch)[(size_t) s]);
        o += "]";
    }
    return o + "]}";
}

std::string channelJson (const Engine& e, int ch)
{
    if (ch < 0 || ch >= numChannels) return "null";
    std::string o = "{\"channel\":" + std::to_string (ch) + ",\"mix\":" + valuesJson (e, ch, mixerBase, mixerDesc (ch).params.size());
    if (ch < numTracks)
    {
        const auto* d = e.instrumentDesc (ch);
        o += ",\"instrument\":" + quote (d ? d->type : std::string()) + ",\"inst\":" + valuesJson (e, ch, instrumentBase, d ? d->params.size() : 0);
        o += ",\"preset\":" + quote (e.getTrackInfo (ch).preset);
        std::string samples = "[";
        const auto nS = d != nullptr && d->type == "octa" ? maxSamples : 16; // v0.42 : réserve de 128 de Pieuvre 8
        for (int i = 0; i < nS; ++i)
        {
            const auto* s = e.getSample (ch, i);
            samples += (i ? "," : "") + (s ? quote (s->name) : std::string ("null"));
        }
        o += ",\"samples\":" + samples + "]";
        // inserts des pads (batteries) : seulement ceux qui ont un effet
        std::string pfx = "[";
        bool first = true;
        for (int pad = 0; pad < numPads; ++pad)
            for (int k = 0; k < padInserts; ++k)
                if (const auto* pd = e.padEffectDesc (ch, pad, k))
                {
                    pfx += std::string (first ? "" : ",") + "{\"pad\":" + std::to_string (pad) + ",\"slot\":" + std::to_string (k) + ",\"type\":" + quote (pd->type)
                         + ",\"bypass\":" + num (e.getParam (globalSlot (ch, padBypassBase + pad * padInserts + k)))
                         + ",\"values\":" + valuesJson (e, ch, padInsertSlot (pad, k), pd->params.size()) + "}";
                    first = false;
                }
        o += ",\"padfx\":" + pfx + "]";
    }
    o += ",\"fx\":[";
    for (int s = 0; s < numInserts; ++s)
    {
        const auto* d = e.effectDesc (ch, s);
        o += (s ? "," : "") + std::string ("{\"type\":") + quote (d ? d->type : std::string())
           + ",\"values\":" + valuesJson (e, ch, insertBase + s * insertSlots, d ? d->params.size() : 0) + "}";
    }
    o += "],\"mod\":[";
    for (int s = 0; s < numModSources; ++s)
        o += (s ? "," : "") + valuesJson (e, ch, modSourceBase + s * modSourceSlots, (size_t) modSourceSlots);
    o += "],\"mods\":[";
    const auto mods = e.getModulations (ch);
    for (size_t i = 0; i < mods.size(); ++i)
    {
        const auto& m = mods[i];
        o += (i ? "," : "") + std::string ("{\"source\":") + std::to_string (m.source) + ",\"target\":" + std::to_string (m.target)
           + ",\"amount\":" + num (e.getParam (globalSlot (ch, modAmountBase + m.amountIndex)) * 2.0 - 1.0)
           + ",\"amountSlot\":" + std::to_string (modAmountBase + m.amountIndex) + "}";
    }
    return o + "]}";
}

std::string patternJson (const Engine& e, int t)
{
    if (t < 0 || t >= numTracks) return "null";
    const auto& s = e.sequencer();
    std::string o = "{\"track\":" + std::to_string (t) + ",\"length\":" + std::to_string (s.getTrackLength (t))
                  + ",\"speed\":" + std::to_string (s.getTrackSpeed (t)) + ",\"mute\":" + (s.getTrackMute (t) ? "true" : "false")
                  + ",\"solo\":" + (s.getTrackSolo (t) ? "true" : "false") + ",\"division\":" + std::to_string ((int) s.getTrackDivision (t))
                  + ",\"pattern\":" + std::to_string (s.getEditing (t)) + ",\"playing\":" + std::to_string (s.getPlaying (t))
                  + ",\"recQuant\":" + std::to_string (s.getRecordQuantize()) + ",\"accent\":" + std::to_string (s.getAccentAmount (t));
    {
        static constexpr char digits[] = "0123456789abcdef";
        std::string f;
        for (int st = 0; st < Sequencer::maxSteps; ++st) f += digits[s.getStepFlags (t, st) & 15];
        o += ",\"flags\":\"" + f + "\"";
        // v0.46 fix27 : valeurs des pas (force de l'accent, durée du glissé), 2 × 2 chiffres par pas, s'il y en a
        std::string am;
        bool anyAmt = false;
        for (int st = 0; st < Sequencer::maxSteps; ++st)
        {
            const auto v = s.getStepAmounts (t, st);
            anyAmt = anyAmt || v != 0;
            am += digits[(v >> 4) & 15]; am += digits[v & 15]; am += digits[(v >> 12) & 15]; am += digits[(v >> 8) & 15];
        }
        if (anyAmt) o += ",\"amt\":\"" + am + "\"";
    }
    o += ",\"lanes\":[";
    for (int l = 0; l < Sequencer::numLanes; ++l)
    {
        o += (l ? "," : "") + std::string ("{\"dir\":") + std::to_string ((int) s.getDirection (t, l))
           + ",\"length\":" + std::to_string (s.getLaneLength (t, l))
           + ",\"mute\":" + (s.getLaneMute (t, l) ? "true" : "false") + ",\"solo\":" + (s.getLaneSolo (t, l) ? "true" : "false")
           + ",\"lglide\":" + (s.getLaneLockGlide (t, l) ? "true" : "false") + ",\"lmute\":" + (s.getLaneLockMute (t, l) ? "true" : "false")
           + ",\"vel\":\"" + hexRow (s, t, l, 0) + "\",\"note\":\"" + hexRow (s, t, l, 1) + "\",\"len3\":\"" + hexRow (s, t, l, 2)
           + "\",\"micro\":\"" + hexRow (s, t, l, 3) + "\"}";
    }
    return o + "]}";
}

std::string extrasJson (const Engine& e, int t)
{
    std::string o = "[";
    bool first = true;
    for (const auto& [key, x] : e.getExtras().steps)
    {
        if (PatternExtras::keyTrack (key) != t || PatternExtras::keyPattern (key) != e.sequencer().getEditing (t)) continue;
        o += (first ? "" : ",") + std::string ("{\"lane\":") + std::to_string (PatternExtras::keyLane (key)) + ",\"step\":" + std::to_string (PatternExtras::keyStep (key))
           + ",\"cond\":" + std::to_string ((int) x.cond) + ",\"a\":" + std::to_string (x.a) + ",\"b\":" + std::to_string (x.b)
           + ",\"prob\":" + std::to_string (x.prob) + ",\"rat\":" + std::to_string (x.ratchet) + ",\"rs\":" + std::to_string (x.ratchetSlope)
           + ",\"flam\":" + std::to_string (x.flam) + (x.direct ? ",\"arp\":1" : "") + ",\"locks\":" + locksJson (x.locks) + "}";
        first = false;
    }
    return o + "]";
}

std::string spectrumJson (const Engine& e, int channel, int slot, int bands)
{
    constexpr int n = 4096;
    std::vector<float> x ((size_t) n, 0.0f);
    if (e.effectAnalysis (channel, slot, x.data(), n) < n) return "[]";
    std::vector<std::complex<double>> a ((size_t) n);
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < n; ++i) a[(size_t) i] = x[(size_t) i] * (0.5 - 0.5 * std::cos (2.0 * pi * i / (n - 1)));
    synth::fft (a, false);
    const auto sr = e.getSampleRate();
    bands = std::clamp (bands, 8, 200);
    std::string o = "[";
    for (int b = 0; b < bands; ++b)
    {
        // bandes logarithmiques de 20 Hz à 20 kHz ; niveau en dB (sinus pleine échelle ≈ 0 dB)
        const auto f0 = 20.0 * std::pow (1000.0, (double) b / bands), f1 = 20.0 * std::pow (1000.0, (double) (b + 1) / bands);
        auto k0 = (int) std::floor (f0 / sr * n), k1 = std::max (k0 + 1, (int) std::ceil (f1 / sr * n));
        k0 = std::clamp (k0, 1, n / 2 - 1); k1 = std::clamp (k1, k0 + 1, n / 2);
        double m = 0.0;
        for (int k = k0; k < k1; ++k) m = std::max (m, std::abs (a[(size_t) k]));
        const auto db = 20.0 * std::log10 (std::max (1.0e-9, m * 4.0 / n));
        o += (b ? "," : "") + num (std::max (-120.0, db));
    }
    return o + "]";
}

std::string launchJson (const Engine& e)
{
    const auto& s = e.sequencer();
    std::string o = "{\"quant\":" + std::to_string (s.getLaunchQuant()) + ",\"tracks\":[";
    for (int t = 0; t < numTracks; ++t)
    {
        o += (t ? "," : "") + std::string ("{\"playing\":") + std::to_string (s.getPlaying (t)) + ",\"queued\":" + std::to_string (s.getQueued (t))
           + ",\"editing\":" + std::to_string (s.getEditing (t)) + ",\"filled\":[";
        for (int p = 0; p < Sequencer::numPatterns; ++p) o += (p ? "," : "") + std::string (s.hasNotes (t, p) ? "1" : "0");
        o += "],\"length\":[";
        for (int p = 0; p < Sequencer::numPatterns; ++p) o += (p ? "," : "") + std::to_string (s.getPatternLength (t, p));
        o += "],\"division\":[";
        for (int p = 0; p < Sequencer::numPatterns; ++p) o += (p ? "," : "") + std::to_string ((int) s.getPatternDivision (t, p));
        o += "]}";
    }
    return o + "]}";
}

std::string scenesJson (const Engine& e)
{
    const auto& sc = e.getScenes();
    std::string o = "[";
    for (int s = 0; s <= numScenes; ++s)
    {
        std::vector<std::pair<int, float>> locks;
        for (const auto& l : sc.scenes[(size_t) s]) locks.push_back ({ l.slot, l.norm });
        o += (s ? "," : "") + std::string ("{\"muted\":") + (sc.muted[(size_t) s] ? "true" : "false") + ",\"locks\":" + locksJson (locks) + "}";
    }
    return o + "]";
}

std::string waveJson (const Engine& e, int track, int index, int points)
{
    const auto* s = e.getSample (track, index);
    if (s == nullptr || s->data.empty()) return "null";
    points = std::clamp (points, 16, 2048);
    const auto n = s->data.size();
    std::string o = "{\"name\":" + quote (s->name) + ",\"seconds\":" + num ((double) n / s->sampleRate)
                  + ",\"stereo\":" + (s->right.size() == n ? "true" : "false") + ",\"peaks\":[";
    for (int p = 0; p < points; ++p)
    {
        const auto a = (size_t) ((double) p / points * (double) n), b = std::max (a + 1, (size_t) ((double) (p + 1) / points * (double) n));
        float mn = 0.0f, mx = 0.0f;
        for (size_t i = a; i < std::min (b, n); ++i) { mn = std::min (mn, s->data[i]); mx = std::max (mx, s->data[i]); }
        o += (p ? "," : "") + num (mn) + "," + num (mx);
    }
    o += "]";
    // v0.43.3 : attaques détectées (fractions) : découpes « Attaques » visibles dans l'éditeur
    if (! s->onsets.empty())
    {
        o += ",\"onsets\":[";
        for (size_t k = 0; k < s->onsets.size(); ++k) o += (k ? "," : "") + num ((double) s->onsets[k]);
        o += "]";
    }
    return o + "}";
}

} // namespace grv::engine::bridge
