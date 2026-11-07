#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================================
//  QUARRY parameter tables.
//  Every track has an identical set of parameters, declared once here as an X-macro so
//  the enum, the parameter layout and the UI pages can never drift out of sync.
//
//  X(ENUM, id, display name, kind, min, max, default, skewCentre(<0 = linear), unit, modulatable, choices)
// =====================================================================================

namespace quarry
{
constexpr int kNumTracks = 4;
constexpr int kNumMods   = 4;
constexpr int kNumMacros = 8;
constexpr int kNumScenes = 128;

enum class Kind { Float, Int, Bool, Choice };

#define QUARRY_TRACK_PARAMS(X) \
    /* ---------------- MATERIAL ---------------- */ \
    X(MAT_MODE,    "mat_mode",    "Mode",       Choice, 0, 2, 1, -1, "", 0, "Tape|Disc|Poly") \
    X(MAT_INPUT,   "mat_input",   "Input",      Choice, 0, 5, 1, -1, "", 0, "Off|Ext In|Track 1|Track 2|Track 3|Track 4") \
    X(MAT_REC,     "mat_rec",     "Record",     Bool,   0, 1, 0, -1, "", 0, "") \
    X(MAT_DUB,     "mat_dub",     "Overdub",    Float,  0, 1, 0.7f, -1, "%", 1, "") \
    X(MAT_SPEED,   "mat_speed",   "Speed",      Float, -2, 2, 1, -1, "x", 1, "") \
    X(MAT_START,   "mat_start",   "Start",      Float,  0, 1, 0, -1, "%", 1, "") \
    X(MAT_LENGTH,  "mat_length",  "Length",     Float,  0.005f, 1, 1, -1, "%", 1, "") \
    X(MAT_GAIN,    "mat_gain",    "Gain",       Float, -36, 12, 0, -1, "dB", 1, "") \
    X(MAT_TUNE,    "mat_tune",    "Tune",       Float, -24, 24, 0, -1, "st", 1, "") \
    X(MAT_ROOT,    "mat_root",    "Root",       Int,    0, 127, 60, -1, "note", 0, "") \
    X(MAT_ATTACK,  "mat_attack",  "Attack",     Float,  0.001f, 4, 0.005f, 0.2f, "s", 1, "") \
    X(MAT_RELEASE, "mat_release", "Release",    Float,  0.005f, 8, 0.3f, 0.5f, "s", 1, "") \
    X(MAT_WOW,     "mat_wow",     "Wow",        Float,  0, 1, 0, -1, "%", 1, "") \
    X(MAT_MONITOR, "mat_monitor", "Monitor",    Float,  0, 1, 0, -1, "%", 1, "") \
    X(MAT_TRIG,    "mat_trig",    "Trigger",    Choice, 0, 1, 0, -1, "", 0, "Free|MIDI") \
    X(MAT_LOOP,    "mat_loop",    "Loop",       Bool,   0, 1, 1, -1, "", 0, "") \
    /* ---------------- GRANULAR ---------------- */ \
    X(GR_MIX,      "gr_mix",      "Grain Mix",  Float,  0, 1, 0, -1, "%", 1, "") \
    X(GR_POS,      "gr_pos",      "Position",   Float,  0, 1, 0.2f, -1, "%", 1, "") \
    X(GR_SIZE,     "gr_size",     "Size",       Float,  0.005f, 1, 0.12f, 0.1f, "s", 1, "") \
    X(GR_DENSITY,  "gr_density",  "Density",    Float,  0.5f, 200, 20, 15, "Hz", 1, "") \
    X(GR_SPRAY,    "gr_spray",    "Spray",      Float,  0, 1, 0.1f, -1, "%", 1, "") \
    X(GR_SCAN,     "gr_scan",     "Scan",       Float, -2, 2, 0, -1, "x", 1, "") \
    X(GR_PITCH,    "gr_pitch",    "Pitch",      Float, -24, 24, 0, -1, "st", 1, "") \
    X(GR_PRAND,    "gr_prand",    "Pitch Rnd",  Float,  0, 12, 0, -1, "st", 1, "") \
    X(GR_QUANT,    "gr_quant",    "Quantize",   Choice, 0, 3, 0, -1, "", 0, "Off|Semis|Fifths|Octaves") \
    X(GR_SHAPE,    "gr_shape",    "Shape",      Float,  0, 1, 0.5f, -1, "%", 1, "") \
    X(GR_SPREAD,   "gr_spread",   "Spread",     Float,  0, 1, 0.5f, -1, "%", 1, "") \
    X(GR_REVERSE,  "gr_reverse",  "Reverse",    Float,  0, 1, 0, -1, "%", 1, "") \
    X(GR_FREEZE,   "gr_freeze",   "Freeze",     Bool,   0, 1, 0, -1, "", 0, "") \
    /* ---------------- FILTER ---------------- */ \
    X(FL_MIX,      "fl_mix",      "Filter Mix", Float,  0, 1, 1, -1, "%", 1, "") \
    X(FL_MORPH,    "fl_morph",    "Morph",      Float,  0, 1, 0, -1, "%", 1, "") \
    X(FL_CUTOFF,   "fl_cutoff",   "Cutoff",     Float,  20, 20000, 20000, 1000, "Hz", 1, "") \
    X(FL_RESO,     "fl_reso",     "Resonance",  Float,  0, 1, 0.1f, -1, "%", 1, "") \
    X(FL_TYPE,     "fl_type",     "LP-BP-HP",   Float,  0, 2, 0, -1, "type", 1, "") \
    X(FL_DRIVE,    "fl_drive",    "Drive",      Float,  0, 1, 0, -1, "%", 1, "") \
    X(FL_RPITCH,   "fl_rpitch",   "Res Pitch",  Float,  24, 96, 48, -1, "note", 1, "") \
    X(FL_RSPREAD,  "fl_rspread",  "Res Spread", Float,  0, 1, 0.5f, -1, "spread", 1, "") \
    X(FL_RDECAY,   "fl_rdecay",   "Res Decay",  Float,  0, 1, 0.5f, -1, "%", 1, "") \
    X(FL_RBANDS,   "fl_rbands",   "Bands",      Int,    1, 48, 24, -1, "", 0, "") \
    X(FL_RTILT,    "fl_rtilt",    "Res Tilt",   Float, -1, 1, -0.3f, -1, "", 1, "") \
    X(FL_KEY,      "fl_key",      "Key Track",  Bool,   0, 1, 0, -1, "", 0, "") \
    /* ---------------- COLOR ---------------- */ \
    X(CO_MIX,      "co_mix",      "Color Mix",  Float,  0, 1, 1, -1, "%", 1, "") \
    X(CO_DRIVE,    "co_drive",    "Drive",      Float,  0, 1, 0, -1, "%", 1, "") \
    X(CO_TYPE,     "co_type",     "Shaper",     Choice, 0, 3, 0, -1, "", 0, "Soft|Hard|Fold|Tube") \
    X(CO_BAND,     "co_band",     "Low/High",   Float, -1, 1, 0, -1, "bal", 1, "") \
    X(CO_TONE,     "co_tone",     "Tone",       Float, -1, 1, 0, -1, "", 1, "") \
    X(CO_COMP,     "co_comp",     "Glue",       Float,  0, 1, 0, -1, "%", 1, "") \
    X(CO_NOISE,    "co_noise",    "Noise",      Float,  0, 1, 0, -1, "%", 1, "") \
    X(CO_OUT,      "co_out",      "Output",     Float, -24, 12, 0, -1, "dB", 1, "") \
    X(CO_BITS,     "co_bits",     "Bits",       Float,  1, 16, 16, -1, "bit", 1, "") \
    X(CO_DOWN,     "co_down",     "Downsample", Float,  1, 64, 1, 6, "x", 1, "") \
    /* ---------------- SPACE ---------------- */ \
    X(SP_MIX,      "sp_mix",      "Space Mix",  Float,  0, 1, 0, -1, "%", 1, "") \
    X(SP_DTIME,    "sp_dtime",    "Time",       Float,  0.01f, 2, 0.375f, 0.3f, "s", 1, "") \
    X(SP_DSYNC,    "sp_dsync",    "Sync",       Choice, 0, 8, 0, -1, "", 0, "Free|1/16|1/8T|1/8|1/8D|1/4|1/4D|1/2|1 Bar") \
    X(SP_DFB,      "sp_dfb",      "Feedback",   Float,  0, 1, 0.4f, -1, "%", 1, "") \
    X(SP_SHIMMER,  "sp_shimmer",  "Shimmer",    Float,  0, 1, 0, -1, "%", 1, "") \
    X(SP_SHPITCH,  "sp_shpitch",  "Shim Pitch", Choice, 0, 5, 0, -1, "", 0, "+12|+7|+5|-12|+19|+24") \
    X(SP_FREEZE,   "sp_freeze",   "Freeze",     Bool,   0, 1, 0, -1, "", 0, "") \
    X(SP_DLEVEL,   "sp_dlevel",   "Delay Lvl",  Float,  0, 1, 0.6f, -1, "%", 1, "") \
    X(SP_RLEVEL,   "sp_rlevel",   "Reverb Lvl", Float,  0, 1, 0.6f, -1, "%", 1, "") \
    X(SP_RSIZE,    "sp_rsize",    "Size",       Float,  0, 1, 0.6f, -1, "%", 1, "") \
    X(SP_RDECAY,   "sp_rdecay",   "Decay",      Float,  0, 1, 0.6f, -1, "%", 1, "") \
    X(SP_RDAMP,    "sp_rdamp",    "Damping",    Float,  0, 1, 0.4f, -1, "%", 1, "") \
    X(SP_WIDTH,    "sp_width",    "Width",      Float,  0, 1, 1, -1, "%", 1, "") \
    X(SP_DTOR,     "sp_dtor",     "Dly>Verb",   Float,  0, 1, 0.3f, -1, "%", 1, "") \
    /* ---------------- MODULATORS ---------------- */ \
    X(M1_TYPE,  "m1_type",  "Mod1 Type",  Choice, 0, 7, 0, -1, "", 0, "Sine|Tri|Saw|Square|S&H|Walk|Envelope|Follower") \
    X(M1_RATE,  "m1_rate",  "Mod1 Rate",  Float, 0.01f, 30, 0.5f, 1.5f, "Hz", 0, "") \
    X(M1_SHAPE, "m1_shape", "Mod1 Shape", Float, 0, 1, 0.5f, -1, "%", 0, "") \
    X(M1_DEPTH, "m1_depth", "Mod1 Depth", Float, -1, 1, 0, -1, "%", 0, "") \
    X(M1_TGT,   "m1_tgt",   "Mod1 Target",Choice, 0, 0, 0, -1, "", 0, "@targets") \
    X(M2_TYPE,  "m2_type",  "Mod2 Type",  Choice, 0, 7, 1, -1, "", 0, "Sine|Tri|Saw|Square|S&H|Walk|Envelope|Follower") \
    X(M2_RATE,  "m2_rate",  "Mod2 Rate",  Float, 0.01f, 30, 0.2f, 1.5f, "Hz", 0, "") \
    X(M2_SHAPE, "m2_shape", "Mod2 Shape", Float, 0, 1, 0.5f, -1, "%", 0, "") \
    X(M2_DEPTH, "m2_depth", "Mod2 Depth", Float, -1, 1, 0, -1, "%", 0, "") \
    X(M2_TGT,   "m2_tgt",   "Mod2 Target",Choice, 0, 0, 0, -1, "", 0, "@targets") \
    X(M3_TYPE,  "m3_type",  "Mod3 Type",  Choice, 0, 7, 4, -1, "", 0, "Sine|Tri|Saw|Square|S&H|Walk|Envelope|Follower") \
    X(M3_RATE,  "m3_rate",  "Mod3 Rate",  Float, 0.01f, 30, 2, 1.5f, "Hz", 0, "") \
    X(M3_SHAPE, "m3_shape", "Mod3 Shape", Float, 0, 1, 0.2f, -1, "%", 0, "") \
    X(M3_DEPTH, "m3_depth", "Mod3 Depth", Float, -1, 1, 0, -1, "%", 0, "") \
    X(M3_TGT,   "m3_tgt",   "Mod3 Target",Choice, 0, 0, 0, -1, "", 0, "@targets") \
    X(M4_TYPE,  "m4_type",  "Mod4 Type",  Choice, 0, 7, 6, -1, "", 0, "Sine|Tri|Saw|Square|S&H|Walk|Envelope|Follower") \
    X(M4_RATE,  "m4_rate",  "Mod4 Rate",  Float, 0.01f, 30, 1, 1.5f, "Hz", 0, "") \
    X(M4_SHAPE, "m4_shape", "Mod4 Shape", Float, 0, 1, 0.1f, -1, "%", 0, "") \
    X(M4_DEPTH, "m4_depth", "Mod4 Depth", Float, -1, 1, 0, -1, "%", 0, "") \
    X(M4_TGT,   "m4_tgt",   "Mod4 Target",Choice, 0, 0, 0, -1, "", 0, "@targets") \
    /* ---------------- MIXER / CONFIG ---------------- */ \
    X(MX_LEVEL,  "mx_level",  "Level",      Float, -60, 6, 0, -12, "dB", 1, "") \
    X(MX_PAN,    "mx_pan",    "Pan",        Float, -1, 1, 0, -1, "pan", 1, "") \
    X(MX_MUTE,   "mx_mute",   "Mute",       Bool,  0, 1, 0, -1, "", 0, "") \
    X(MX_SOLO,   "mx_solo",   "Solo",       Bool,  0, 1, 0, -1, "", 0, "") \
    X(MX_MASTER, "mx_master", "To Master",  Bool,  0, 1, 1, -1, "", 0, "") \
    X(MX_MIDICH, "mx_midich", "MIDI Ch",    Choice, 0, 16, 0, -1, "", 0, "Omni|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16")

enum TP
{
#define QUARRY_ENUM(e, ...) e,
    QUARRY_TRACK_PARAMS(QUARRY_ENUM)
#undef QUARRY_ENUM
    TP_COUNT
};

struct Spec
{
    juce::String id, name;
    Kind kind;
    float min, max, def, centre;
    juce::String unit;
    bool modulatable;
    juce::StringArray choices;
};

// ------------------------------------------------------------------------------------
inline const std::vector<int>& modTargetIndices();   // TP indices that can be modulated

inline const std::vector<Spec>& trackSpecs()
{
    static const std::vector<Spec> specs = []
    {
        std::vector<Spec> v;
#define QUARRY_SPEC(e, id, nm, kind, mn, mx, df, ce, un, md, ch) \
        v.push_back (Spec { id, nm, Kind::kind, (float) (mn), (float) (mx), (float) (df), (float) (ce), un, (md) != 0, \
                            juce::StringArray::fromTokens (ch, "|", "") });
        QUARRY_TRACK_PARAMS(QUARRY_SPEC)
#undef QUARRY_SPEC
        // Fill the "@targets" choice lists for modulator targets
        juce::StringArray targets { "None" };
        for (int i = 0; i < (int) v.size(); ++i)
            if (v[(size_t) i].modulatable) targets.add (v[(size_t) i].name);
        for (auto& s : v)
            if (s.choices.size() == 1 && s.choices[0] == "@targets")
            {
                s.choices = targets;
                s.max = (float) (targets.size() - 1);
            }
        return v;
    }();
    return specs;
}

inline const std::vector<int>& modTargetIndices()
{
    static const std::vector<int> idx = []
    {
        std::vector<int> r;
        const auto& s = trackSpecs();
        for (int i = 0; i < (int) s.size(); ++i)
            if (s[(size_t) i].modulatable) r.push_back (i);
        return r;
    }();
    return idx;
}

inline juce::String trackParamID (int track, int tp)
{
    return "t" + juce::String (track + 1) + "_" + trackSpecs()[(size_t) tp].id;
}

// Global macro targets: "None", then "T1 Speed", "T1 Start", ... for every track
inline const juce::StringArray& macroTargetNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray n { "None" };
        for (int t = 0; t < kNumTracks; ++t)
            for (int i : modTargetIndices())
                n.add ("T" + juce::String (t + 1) + " " + trackSpecs()[(size_t) i].name);
        return n;
    }();
    return names;
}

inline juce::String noteName (int n)
{
    return juce::MidiMessage::getMidiNoteName (n, true, true, 3);
}

inline juce::String formatValue (const Spec& s, float v)
{
    if (s.kind == Kind::Choice)  return s.choices[juce::jlimit (0, s.choices.size() - 1, (int) std::round (v))];
    if (s.kind == Kind::Bool)    return v > 0.5f ? "ON" : "OFF";
    const auto& u = s.unit;
    if (u == "%")    return juce::String (juce::roundToInt (v * 100.0f)) + "%";
    if (u == "dB")   return (v <= -59.5f ? juce::String ("-inf") : juce::String (v, 1)) + " dB";
    if (u == "s")    return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
    if (u == "Hz")   return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, v < 10 ? 2 : 1) + " Hz";
    if (u == "st")   return (v > 0 ? "+" : "") + juce::String (v, 1) + " st";
    if (u == "x")    return juce::String (v, 2) + "x";
    if (u == "note") return noteName (juce::roundToInt (v));
    if (u == "pan")  return std::abs (v) < 0.01f ? juce::String ("C") : (v < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (v) * 100));
    if (u == "type") return v < 0.5f ? "LP" : v < 1.5f ? (v < 1.0f ? "LP>BP" : "BP>HP") : (v < 1.95f ? "BP>HP" : "HP");
    if (u == "bal")  return std::abs (v) < 0.02f ? juce::String ("Both") : (v < 0 ? "Low " : "High ") + juce::String (juce::roundToInt (std::abs (v) * 100)) + "%";
    if (u == "spread") return std::abs (v - 0.5f) < 0.02f ? juce::String ("Harmonic") : (v < 0.5f ? "Squeeze " : "Stretch ") + juce::String (juce::roundToInt (std::abs (v - 0.5f) * 200)) + "%";
    if (u == "bit")  return juce::String (v, 1) + " bit";
    if (s.kind == Kind::Int) return juce::String (juce::roundToInt (v));
    return juce::String (v, 2);
}

inline std::unique_ptr<juce::RangedAudioParameter> makeParam (const juce::String& id, const juce::String& name, const Spec& s)
{
    using namespace juce;
    const ParameterID pid { id, 1 };
    if (s.kind == Kind::Bool)
        return std::make_unique<AudioParameterBool> (pid, name, s.def > 0.5f);
    if (s.kind == Kind::Choice)
        return std::make_unique<AudioParameterChoice> (pid, name, s.choices, (int) s.def);

    NormalisableRange<float> r (s.min, s.max, s.kind == Kind::Int ? 1.0f : 0.0f);
    if (s.centre > 0.0f) r.setSkewForCentre (s.centre);
    auto spec = s;
    return std::make_unique<AudioParameterFloat> (pid, name, r, s.def,
        AudioParameterFloatAttributes().withLabel (s.unit)
            .withStringFromValueFunction ([spec] (float v, int) { return formatValue (spec, v); }));
}
} // namespace quarry
