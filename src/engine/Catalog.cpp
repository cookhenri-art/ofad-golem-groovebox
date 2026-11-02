/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Catalogue : chaque type de module a une description (statique) et une fabrique.
*/

#include "Catalog.h"

#include "../instruments/DrumKit.h"
#include "../instruments/AcidBass.h"
#include "../instruments/MonoBass.h"
#include "../instruments/Poly5.h"
#include "../instruments/PolyChorus.h"
#include "../instruments/HybridPoly.h"
#include "../instruments/WaveMorph.h"
#include "../instruments/Pads.h"
#include "../instruments/Sampler.h"
#include "../instruments/SpSampler.h"
#include "../instruments/Octa.h"
#include "../instruments/MS20.h"
#include "../instruments/SH101.h"
#include "../instruments/Blofeld.h"
#include "../instruments/Ob6.h"
#include "../instruments/Fm6.h"
#include "../instruments/Fm4.h"
#include "../instruments/Muse.h"
#include "../instruments/FmX.h"
#include "../instruments/HotSauce.h"
#include "../instruments/Lyra.h"
#include "../instruments/Bitmask.h"
#include "../instruments/Orbit.h"
#include "../instruments/Dfam.h"
#include "../instruments/Vocoders.h"
#include "../effects/Distortion.h"
#include "../effects/Delay.h"
#include "../effects/Reverb.h"
#include "../effects/ModFx.h"
#include "../effects/ModFx36.h"
#include "../effects/Grinder.h"
#include "../effects/Sculptor.h"
#include "../effects/Fx38.h"
#include "../effects/Eq.h"
#include "../effects/Dynamics.h"
#include "../effects/Mastering.h"
#include "../effects/Gate.h"
#include "../effects/Atmos.h"
#include "../effects/MultiDelay.h"
#include "../effects/StepFx.h"
#include "../effects/Looper.h"
#include "../effects/StudioFx.h"
#include "../effects/SpaceFx.h"

#include <functional>

namespace grv::engine
{

namespace
{
    struct InstrumentEntry { const ModuleDesc* (*desc)(); std::function<std::unique_ptr<Instrument>()> make; };
    struct EffectEntry { const ModuleDesc* (*desc)(); std::function<std::unique_ptr<Effect>()> make; };

    const std::vector<InstrumentEntry>& instrumentEntries()
    {
        using namespace grv::instruments;
        static const std::vector<InstrumentEntry> e {
            { [] { return &kitAnalogDesc(); }, [] { return std::make_unique<DrumKit> (kitAnalogDesc(), kitAnalogPads()); } },
            { [] { return &kit80Desc(); },     [] { return std::make_unique<DrumKit> (kit80Desc(), kit80Pads()); } },
            { [] { return &kit90Desc(); },     [] { return std::make_unique<DrumKit> (kit90Desc(), kit90Pads()); } },
            { [] { return &kitImpactDesc(); }, [] { return std::make_unique<DrumKit> (kitImpactDesc(), kitImpactPads()); } },
            { [] { return &kitNerveDesc(); },  [] { return std::make_unique<DrumKit> (kitNerveDesc(), kitNervePads()); } },
            { [] { return &acid::desc(); },    [] { return std::make_unique<AcidBass>(); } },
            { [] { return &mono::desc(); },    [] { return std::make_unique<MonoBass>(); } },
            { [] { return &poly5::desc(); },   [] { return std::make_unique<Poly5>(); } },
            { [] { return &polyj::desc(); },   [] { return std::make_unique<PolyChorus>(); } },
            { [] { return &hybrid::desc(); },  [] { return std::make_unique<HybridPoly>(); } },
            { [] { return &morph::desc(); },   [] { return std::make_unique<WaveMorph>(); } },
            { [] { return &pads::desc(); },    [] { return std::make_unique<Pads>(); } },
            { [] { return &slicer::desc(); },  [] { return std::make_unique<SlicerSampler>(); } },
            { [] { return &keys::desc(); },    [] { return std::make_unique<KeysSampler>(); } },
            { [] { return &sp::desc(); },      [] { return std::make_unique<SpSampler>(); } },
            { [] { return &ms20::desc(); },    [] { return std::make_unique<Ms20Synth>(); } },
            { [] { return &sh101::desc(); },   [] { return std::make_unique<Sh101Synth>(); } },
            { [] { return &blo::desc(); },     [] { return std::make_unique<BloSynth>(); } },
            { [] { return &ob6::desc(); },     [] { return std::make_unique<Ob6Synth>(); } },
            { [] { return &orbit::desc(); },   [] { return std::make_unique<OrbitSynth>(); } },
            { [] { return &fm6::desc(); },     [] { return std::make_unique<Fm6Synth>(); } },
            { [] { return &fm4::desc(); },     [] { return std::make_unique<Fm4Synth>(); } },
            { [] { return &fmx::desc(); },     [] { return std::make_unique<FmxSynth>(); } },
            { [] { return &hots::desc(); },    [] { return std::make_unique<HotsSynth>(); } },
            { [] { return &lyra::desc(); },    [] { return std::make_unique<LyraSynth>(); } },
            { [] { return &bitm::desc(); },    [] { return std::make_unique<BitmSynth>(); } },
            { [] { return &muse::desc(); },    [] { return std::make_unique<MuseSynth>(); } },
            { [] { return &dfam::desc(); },    [] { return std::make_unique<DfamSynth>(); } },
            { [] { return &voc::desc (voc::m16); },      [] { return std::make_unique<VocoderSynth> (voc::m16); } },
            { [] { return &voc::desc (voc::strings); },  [] { return std::make_unique<VocoderSynth> (voc::strings); } },
            { [] { return &voc::desc (voc::spectral); }, [] { return std::make_unique<VocoderSynth> (voc::spectral); } },
            { [] { return &voc::desc (voc::vsynth); },   [] { return std::make_unique<VocoderSynth> (voc::vsynth); } },
            { [] { return &octa::desc(); },    [] { return std::make_unique<OctaSampler>(); } },
            { [] { return &input::desc(); },   [] { return std::make_unique<AudioInput>(); } },
        };
        return e;
    }

    const std::vector<EffectEntry>& effectEntries()
    {
        using namespace grv::effects;
        static const std::vector<EffectEntry> e {
            { [] { return &eq::desc(); },       [] { return std::make_unique<Equalizer>(); } },
            { [] { return &opto::desc(); },     [] { return std::make_unique<OptoCompressor>(); } },
            { [] { return &fet::desc(); },      [] { return std::make_unique<FetCompressor>(); } },
            { [] { return &buscomp::desc(); },  [] { return std::make_unique<BusCompressor>(); } },
            { [] { return &modern::desc(); },   [] { return std::make_unique<ModernCompressor>(); } },
            { [] { return &limit::desc(); },    [] { return std::make_unique<Limiter>(); } },
            { [] { return &master::desc(); },   [] { return std::make_unique<Mastering>(); } },
            { [] { return &gate::desc(); },     [] { return std::make_unique<GateDeesser>(); } },
            { [] { return &mband::desc(); },    [] { return std::make_unique<Multiband>(); } },        // v0.34
            { [] { return &varimu::desc(); },   [] { return std::make_unique<VariMuCompressor>(); } },
            { [] { return &tubeeq::desc(); },   [] { return std::make_unique<TubeEq>(); } },
            { [] { return &consoleeq::desc(); }, [] { return std::make_unique<ConsoleEq>(); } },
            { [] { return &dist::desc(); },     [] { return std::make_unique<Distortion>(); } },
            { [] { return &grinder::desc(); },  [] { return std::make_unique<Grinder>(); } },      // v0.36 : principe Ohmicide
            { [] { return &tapesat::desc(); },  [] { return std::make_unique<TapeSaturator>(); } }, // v0.38
            { [] { return &tubesat::desc(); },  [] { return std::make_unique<TubeSaturator>(); } },
            { [] { return &distcomp::desc(); }, [] { return std::make_unique<DistortionCompressor>(); } },
            { [] { return &transient::desc(); }, [] { return std::make_unique<TransientShaper>(); } },
            { [] { return &echo::desc(); },     [] { return std::make_unique<Delay>(); } },
            { [] { return &tapeecho::desc(); }, [] { return std::make_unique<TapeEcho>(); } },     // v0.35
            { [] { return &verb::desc(); },     [] { return std::make_unique<Reverb>(); } },
            { [] { return &vintverb::desc(); }, [] { return std::make_unique<VintageReverb>(); } }, // v0.35
            { [] { return &plate::desc(); },    [] { return std::make_unique<PlateReverb>(); } },
            { [] { return &spring::desc(); },   [] { return std::make_unique<SpringReverb>(); } },
            { [] { return &prestige::desc(); }, [] { return std::make_unique<PrestigeReverb>(); } }, // v0.38
            { [] { return &chorus::desc(); },   [] { return std::make_unique<Chorus>(); } },
            { [] { return &phaser::desc(); },   [] { return std::make_unique<Phaser>(); } },
            { [] { return &flanger::desc(); },  [] { return std::make_unique<Flanger>(); } },      // v0.36
            { [] { return &phaser2::desc(); },  [] { return std::make_unique<DualPhaser>(); } },
            { [] { return &dimension::desc(); }, [] { return std::make_unique<Dimension>(); } },
            { [] { return &chorus3::desc(); },  [] { return std::make_unique<TriChorus>(); } },      // v0.38
            { [] { return &stutter::desc(); },  [] { return std::make_unique<Stutter>(); } },
            { [] { return &filterfx::desc(); }, [] { return std::make_unique<FilterFx>(); } },
            { [] { return &atmos::desc(); },    [] { return std::make_unique<Atmos>(); } },
            { [] { return &mdelay::desc(); },   [] { return std::make_unique<MultiDelay>(); } },
            { [] { return &stepfx::desc(); },   [] { return std::make_unique<StepFx>(); } },
            { [] { return &sculptor::desc(); }, [] { return std::make_unique<Sculptor>(); } },     // v0.37 : principe ShaperBox
            { [] { return &looper::desc(); },   [] { return std::make_unique<Looper>(); } },
        };
        return e;
    }
}

const std::vector<const ModuleDesc*>& instrumentCatalog()
{
    static const std::vector<const ModuleDesc*> list = []
    {
        std::vector<const ModuleDesc*> v;
        for (const auto& e : instrumentEntries()) v.push_back (e.desc());
        return v;
    }();
    return list;
}

const std::vector<const ModuleDesc*>& effectCatalog()
{
    static const std::vector<const ModuleDesc*> list = []
    {
        std::vector<const ModuleDesc*> v;
        for (const auto& e : effectEntries()) v.push_back (e.desc());
        return v;
    }();
    return list;
}

const ModuleDesc* findInstrument (const std::string& type)
{
    for (const auto* d : instrumentCatalog()) if (d->type == type) return d;
    return nullptr;
}

const ModuleDesc* findEffect (const std::string& type)
{
    for (const auto* d : effectCatalog()) if (d->type == type) return d;
    return nullptr;
}

std::unique_ptr<Instrument> createInstrument (const std::string& type)
{
    for (const auto& e : instrumentEntries()) if (e.desc()->type == type) return e.make();
    return nullptr;
}

std::unique_ptr<Effect> createEffect (const std::string& type)
{
    for (const auto& e : effectEntries()) if (e.desc()->type == type) return e.make();
    return nullptr;
}

const std::vector<ChainDesc>& chainCatalog()
{
    // Chaînes créées pour le groovebox (aucun préréglage d'usine copié), dans l'ordre de traitement.
    static const std::vector<ChainDesc> chains {
        { "Voix", "Coupe-bas, compresseur optique doux, pr\xC3\xA9sence", { { "eq", "Voix" }, { "opto", "Voix" } } },
        { "Voix propre", "Porte et d\xC3\xA9-esseur, coupe-bas, compresseur optique doux", { { "gate", "Voix : porte et d\xC3\xA9-esseur" }, { "eq", "Voix" }, { "opto", "Voix" } } },
        { "Voix rap en avant", "FET rapide, puis optique, puis \xC3\xA9galiseur", { { "fet", "Voix rock" }, { "opto", "Voix" }, { "eq", "Voix" } } },
        { "Basse", "Coupe-bas 30 Hz, compresseur optique, saturation l\xC3\xA9g\xC3\xA8re", { { "eq", "Basse" }, { "opto", "Basse" }, { "dist", "Chaleur" } } },
        { "Grosse caisse", "\xC3\x89galiseur (attaque et bas), puis FET", { { "eq", "Grosse caisse" }, { "fet", "Batterie" } } },
        { "Bus batterie", "\xC3\x89galiseur, compresseur de bus, compression parall\xC3\xA8le", { { "eq", "Bus batterie" }, { "bus", "Bus batterie" }, { "fet", "\xC3\x89" "crasement parall\xC3\xA8le" } } },
        { "Instruments, nappes", "Creux dans les m\xC3\xA9" "diums, compresseur moderne doux", { { "eq", "Nappe (place pour la voix)" }, { "comp", "Piano" } } },
        { "Guitare", "Coupe-bas, \xC3\xA9galiseur, compresseur optique", { { "eq", "Guitare" }, { "opto", "Guitare" } } },
        { "Bus principal", "\xC3\x89galiseur large, compresseur de bus 2 dB, limiteur -1 dB", { { "eq", "Master" }, { "bus", "Master doux" }, { "limiter", "Master transparent" } } },
        { "Mastering streaming", "Cha\xC3\xAEne de mastering compl\xC3\xA8te, -14 LUFS, -1 dBTP", { { "master", "Streaming (-14 LUFS, -1 dBTP)" } } },
        { "Mastering CD", "Cha\xC3\xAEne de mastering compl\xC3\xA8te, fort (-9 LUFS)", { { "master", "Master CD (-9 LUFS)" } } },
    };
    return chains;
}

} // namespace grv::engine
