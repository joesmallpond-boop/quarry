#pragma once
#include "Params.h"
#include "DSP.h"

namespace quarry
{
// One of the four parallel pipelines: Material -> Granular -> Filter -> Color -> Space
class Track
{
public:
    Track() = default;

    void bind (juce::AudioProcessorValueTreeState& state, int trackIndex)
    {
        index = trackIndex;
        for (int i = 0; i < TP_COUNT; ++i)
        {
            params[(size_t) i] = state.getParameter (trackParamID (trackIndex, i));
            jassert (params[(size_t) i] != nullptr);
        }
        for (int m = 0; m < kNumMods; ++m) mods[(size_t) m].seed (0x1234u + (uint32_t) (trackIndex * 31 + m * 7));
    }

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        material.prepare (sampleRate); granular.prepare (sampleRate);
        filter.prepare (sampleRate); color.prepare (sampleRate); space.prepare (sampleRate);
        work.setSize (2, maxBlock + 64);
        out.setSize (2, maxBlock + 64); out.clear();
        lastNote = 60; folLevel = 0;
        macroOffset.fill (0.0f);
    }

    // External per-block offsets (normalised) coming from macros
    std::array<float, TP_COUNT> macroOffset {};

    // Called by processor before process for each MIDI event that matches this track
    void handleMidi (const juce::MidiMessage& m)
    {
        const int ch = (int) params[MX_MIDICH]->convertFrom0to1 (params[MX_MIDICH]->getValue());
        if (ch != 0 && m.getChannel() != ch) return;
        if (m.isNoteOn())
        {
            refreshValues();
            lastNote = m.getNoteNumber();
            material.noteOn (lastNote, m.getFloatVelocity(), matParams());
            for (int k = 0; k < kNumMods; ++k)
                if ((int) val[(size_t) (M1_TYPE + k * 5)] == dsp::Modulator::Env) mods[(size_t) k].trigger();
        }
        else if (m.isNoteOff()) material.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) material.allNotesOff();
    }

    // Process a sub-block. in may be nullptr. Result written into out at offset.
    void process (const float* inL, const float* inR, int offset, int n, double bpm, bool playing, bool punchFreeze)
    {
        this->bpm = bpm; this->playing = playing;
        const float dt = (float) n / (float) sr;

        // ---- modulators ----
        refreshBase();
        std::array<float, TP_COUNT> modOff {};
        const auto& targets = modTargetIndices();
        for (int k = 0; k < kNumMods; ++k)
        {
            const int b = M1_TYPE + k * 5;
            const int type = (int) base[(size_t) b];
            const float v = mods[(size_t) k].tick (type, base[(size_t) b + 1], base[(size_t) b + 2], dt, folLevel);
            modValue[(size_t) k].store (v, std::memory_order_relaxed);
            const int tgt = (int) base[(size_t) b + 4];
            if (tgt > 0 && tgt <= (int) targets.size())
                modOff[(size_t) targets[(size_t) tgt - 1]] += v * base[(size_t) b + 3];
        }
        computeValues (modOff);

        float* wl = work.getWritePointer (0); float* wr = work.getWritePointer (1);

        // ---- Material ----
        auto mp = matParams();
        material.process (inL, inR, wl, wr, n, mp);

        // follower level from material output
        float pk = 0;
        for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (wl[i]), std::abs (wr[i]));
        folLevel = juce::jmax (pk, folLevel * std::exp (-dt * 8.0f));

        // ---- Granular ----
        dsp::GranularParams gp;
        gp.mix = val[GR_MIX]; gp.pos = val[GR_POS]; gp.size = val[GR_SIZE]; gp.density = val[GR_DENSITY];
        gp.spray = val[GR_SPRAY]; gp.scan = val[GR_SCAN]; gp.pitch = val[GR_PITCH]; gp.pitchRand = val[GR_PRAND];
        gp.quant = (int) val[GR_QUANT]; gp.shape = val[GR_SHAPE]; gp.spread = val[GR_SPREAD]; gp.reverse = val[GR_REVERSE];
        gp.freeze = val[GR_FREEZE] > 0.5f || punchFreeze;
        granular.process (wl, wr, n, gp);

        // ---- Filter ----
        dsp::FilterParams fp;
        fp.mix = val[FL_MIX]; fp.morph = val[FL_MORPH]; fp.cutoff = val[FL_CUTOFF]; fp.reso = val[FL_RESO];
        fp.type = val[FL_TYPE]; fp.drive = val[FL_DRIVE];
        float rootNote = val[FL_RPITCH];
        if (val[FL_KEY] > 0.5f) rootNote = (float) lastNote + (rootNote - 48.0f);
        fp.rootHz = 440.0f * std::exp2 ((rootNote - 69.0f) / 12.0f);
        fp.spread = val[FL_RSPREAD]; fp.decay = val[FL_RDECAY]; fp.bands = (int) val[FL_RBANDS]; fp.tilt = val[FL_RTILT];
        if (fp.mix > 0.0005f && (fp.morph > 0.001f || fp.cutoff < 19900.0f || fp.type > 0.001f || fp.drive > 0.0f))
            filter.process (wl, wr, n, fp);

        // ---- Color ----
        dsp::ColorParams cp;
        cp.mix = val[CO_MIX]; cp.drive = val[CO_DRIVE]; cp.type = (int) val[CO_TYPE]; cp.band = val[CO_BAND];
        cp.tone = val[CO_TONE]; cp.comp = val[CO_COMP]; cp.noise = val[CO_NOISE]; cp.out = dsp::dbToGain (val[CO_OUT]);
        cp.bits = val[CO_BITS]; cp.down = val[CO_DOWN];
        color.process (wl, wr, n, cp);

        // ---- Space ----
        dsp::SpaceParams sp;
        sp.mix = val[SP_MIX];
        static const float divs[] = { 0, 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f };
        const int sync = (int) val[SP_DSYNC];
        sp.time = sync == 0 ? val[SP_DTIME] : (float) (60.0 / juce::jmax (20.0, bpm)) * divs[juce::jlimit (0, 8, sync)];
        sp.fb = val[SP_DFB]; sp.shimmer = val[SP_SHIMMER];
        static const float shs[] = { 12, 7, 5, -12, 19, 24 };
        sp.shimSemis = shs[juce::jlimit (0, 5, (int) val[SP_SHPITCH])];
        sp.freeze = val[SP_FREEZE] > 0.5f || punchFreeze;
        sp.dLevel = val[SP_DLEVEL]; sp.rLevel = val[SP_RLEVEL]; sp.size = val[SP_RSIZE]; sp.decay = val[SP_RDECAY];
        sp.damp = val[SP_RDAMP]; sp.width = val[SP_WIDTH]; sp.dToR = val[SP_DTOR];
        if (sp.mix > 0.0005f || sp.freeze) space.process (wl, wr, n, sp);

        // ---- safety + write ----
        float* ol = out.getWritePointer (0, offset); float* orr = out.getWritePointer (1, offset);
        for (int i = 0; i < n; ++i)
        {
            float a = wl[i], b = wr[i];
            if (! std::isfinite (a)) a = 0; if (! std::isfinite (b)) b = 0;
            ol[i] = std::abs (a) > 1.0f ? std::tanh (a) : a;
            orr[i] = std::abs (b) > 1.0f ? std::tanh (b) : b;
        }
    }

    float v (int tp) const { return val[(size_t) tp]; }   // effective (modulated) value
    float baseValue (int tp) const { return base[(size_t) tp]; }
    float effectiveNorm (int tp) const { return effNorm[(size_t) tp].load (std::memory_order_relaxed); }

    juce::AudioBuffer<float> out;     // this block's output (pre-mixer)
    dsp::Material material;
    dsp::Granular granular;
    dsp::Filter filter;
    dsp::Color color;
    dsp::Space space;
    std::array<std::atomic<float>, kNumMods> modValue {};
    std::array<juce::RangedAudioParameter*, TP_COUNT> params {};
    int lastNote = 60;

private:
    int index = 0;
    double sr = 48000, bpm = 120; bool playing = true;
    juce::AudioBuffer<float> work;
    std::array<dsp::Modulator, kNumMods> mods;
    std::array<float, TP_COUNT> base {}, val {};
    std::array<std::atomic<float>, TP_COUNT> effNorm {};
    float folLevel = 0;

    void refreshBase()
    {
        for (int i = 0; i < TP_COUNT; ++i)
            base[(size_t) i] = params[(size_t) i]->convertFrom0to1 (params[(size_t) i]->getValue());
    }
    void refreshValues() { refreshBase(); val = base; }

    void computeValues (const std::array<float, TP_COUNT>& modOff)
    {
        for (int i = 0; i < TP_COUNT; ++i)
        {
            const float off = modOff[(size_t) i] + macroOffset[(size_t) i];
            auto* p = params[(size_t) i];
            const float n0 = p->getValue();
            if (off == 0.0f) { val[(size_t) i] = base[(size_t) i]; effNorm[(size_t) i].store (n0, std::memory_order_relaxed); continue; }
            const float nn = dsp::clamp01 (n0 + off);
            val[(size_t) i] = p->convertFrom0to1 (nn);
            effNorm[(size_t) i].store (nn, std::memory_order_relaxed);
        }
    }

    dsp::MaterialParams matParams() const
    {
        dsp::MaterialParams p;
        p.mode = (int) val[MAT_MODE]; p.rec = val[MAT_REC] > 0.5f; p.loop = val[MAT_LOOP] > 0.5f;
        p.midiTrig = (int) val[MAT_TRIG] == 1; p.playing = playing;
        p.dub = val[MAT_DUB]; p.speed = val[MAT_SPEED]; p.start = val[MAT_START];
        p.length = juce::jmin (val[MAT_LENGTH], 1.0f - p.start * 0.999f);
        p.gain = dsp::dbToGain (val[MAT_GAIN]); p.tune = val[MAT_TUNE]; p.wow = val[MAT_WOW]; p.monitor = val[MAT_MONITOR];
        p.attack = val[MAT_ATTACK]; p.release = val[MAT_RELEASE]; p.root = (int) val[MAT_ROOT];
        return p;
    }
};
} // namespace quarry
