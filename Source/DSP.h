#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <array>
#include <atomic>
#include <random>

// =====================================================================================
//  QUARRY DSP building blocks. Header-only, allocation-free in process calls.
// =====================================================================================
namespace quarry::dsp
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float dbToGain (float db)     { return db <= -59.5f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float semisToRatio (float st) { return std::exp2 (st / 12.0f); }
inline float clamp01 (float x)       { return x < 0 ? 0 : (x > 1 ? 1 : x); }

inline float hermite (const float* d, int len, double pos)
{
    // 4-point cubic Hermite on a circular buffer of length len
    int i1 = (int) std::floor (pos);
    const float f = (float) (pos - i1);
    i1 %= len; if (i1 < 0) i1 += len;
    const int i0 = (i1 - 1 + len) % len, i2 = (i1 + 1) % len, i3 = (i1 + 2) % len;
    const float y0 = d[i0], y1 = d[i1], y2 = d[i2], y3 = d[i3];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

inline float linearRead (const float* d, int len, double pos)
{
    int i = (int) std::floor (pos);
    const float f = (float) (pos - i);
    i %= len; if (i < 0) i += len;
    const int j = (i + 1) % len;
    return d[i] + f * (d[j] - d[i]);
}

struct Random
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) { s = v ? v : 1u; }
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni()     { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
    inline float bi()      { return uni() * 2.0f - 1.0f; }                             // -1..1
};

// Half-cosine table for grain windows
struct CosTable
{
    std::array<float, 2049> t {};
    CosTable() { for (int i = 0; i <= 2048; ++i) t[(size_t) i] = 0.5f - 0.5f * std::cos (kPi * (float) i / 2048.0f); }
    inline float rise (float x) const   // 0..1 -> 0..1 raised cosine
    {
        const float p = clamp01 (x) * 2048.0f;
        const int i = (int) p; const float f = p - (float) i;
        return i >= 2048 ? 1.0f : t[(size_t) i] + f * (t[(size_t) i + 1] - t[(size_t) i]);
    }
    static const CosTable& get() { static CosTable c; return c; }
};

// -------------------------------------------------------------------------------------
//  TPT state-variable filter (Zavalishin / Cytomic)
// -------------------------------------------------------------------------------------
struct SVF
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    float ic1 = 0, ic2 = 0;
    void set (float fc, float q, float sr)
    {
        fc = juce::jlimit (5.0f, sr * 0.49f, fc);
        g = std::tan (kPi * fc / sr);
        k = 1.0f / juce::jmax (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void setK (float fc, float kk, float sr)
    {
        fc = juce::jlimit (5.0f, sr * 0.49f, fc);
        g = std::tan (kPi * fc / sr); k = kk;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    inline float bandpass (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return v1;
    }
    void reset() { ic1 = ic2 = 0; }
    void sanitize() { if (! std::isfinite (ic1) || ! std::isfinite (ic2)) reset(); }
};

struct OnePole
{
    float a = 0, z = 0;
    void set (float fc, float sr) { a = 1.0f - std::exp (-kTwoPi * fc / sr); }
    inline float lp (float x) { z += a * (x - z); return z; }
    void reset() { z = 0; }
};

struct DCBlock
{
    float x1 = 0, y1 = 0, r = 0.995f;
    inline float tick (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0; }
};

// =====================================================================================
//  Sample data shared between message thread (loading/drawing) and audio thread
// =====================================================================================
struct SampleData : juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SampleData>;
    juce::AudioBuffer<float> buffer;   // always 2 channels
    double sourceRate = 48000.0;
    juce::String name, path;
    std::vector<float> peaks;          // 512 overview peaks for display
    void buildPeaks()
    {
        const int n = buffer.getNumSamples(), bins = 512;
        peaks.assign ((size_t) bins, 0.0f);
        if (n <= 0) return;
        for (int b = 0; b < bins; ++b)
        {
            const int s0 = (int) ((int64_t) b * n / bins), s1 = juce::jmax (s0 + 1, (int) ((int64_t) (b + 1) * n / bins));
            float m = 0;
            for (int c = 0; c < 2; ++c)
                for (int i = s0; i < s1 && i < n; ++i) m = juce::jmax (m, std::abs (buffer.getSample (c, i)));
            peaks[(size_t) b] = m;
        }
    }
};

// Lock-free peak overview written by the audio thread, read by the UI
template <int N>
struct AtomicPeaks
{
    std::array<std::atomic<float>, N> v;
    AtomicPeaks() { clear(); }
    void clear() { for (auto& a : v) a.store (0.0f, std::memory_order_relaxed); }
};

// =====================================================================================
//  ADSR-ish envelope (attack / full sustain / release) for sampler voices
// =====================================================================================
struct AREnv
{
    enum Stage { Idle, Attack, Sustain, Release } stage = Idle;
    float value = 0, attackInc = 1, releaseMul = 0.99f;
    void setTimes (float atk, float rel, float sr)
    {
        attackInc = 1.0f / juce::jmax (1.0f, atk * sr);
        releaseMul = std::exp (-6.9f / juce::jmax (1.0f, rel * sr));   // -60 dB over release time
    }
    void noteOn()  { stage = Attack; }
    void noteOff() { if (stage != Idle) stage = Release; }
    void kill()    { stage = Idle; value = 0; }
    inline float tick()
    {
        switch (stage)
        {
            case Attack:  value += attackInc; if (value >= 1.0f) { value = 1.0f; stage = Sustain; } break;
            case Release: value *= releaseMul; if (value < 1.0e-4f) { value = 0; stage = Idle; } break;
            default: break;
        }
        return value;
    }
    bool active() const { return stage != Idle; }
};

// =====================================================================================
//  1. MATERIAL
// =====================================================================================
struct MaterialParams
{
    int mode = 1;          // 0 tape, 1 disc, 2 poly
    bool rec = false, loop = true, midiTrig = false, playing = true;
    float dub = 0.7f, speed = 1, start = 0, length = 1, gain = 1, tune = 0, wow = 0, monitor = 0;
    float attack = 0.005f, release = 0.3f;
    int root = 60;
};

class Material
{
public:
    static constexpr int kVoices = 8;
    static constexpr int kPeakBins = 512;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        tapeMax = (int) (sampleRate * 32.0);
        tape.setSize (2, tapeMax + 4); tape.clear();
        tapeLen = 0; tapeWrite = 0; tapeRecording = false; tapePos = 0;
        tapePeaks.clear();
        for (auto& v : voices) v = {};
        discPos = 0; discEnv.kill();
    }

    SampleData::Ptr sample;    // only touched on the audio thread
    SampleData::Ptr pending;   // handed over by the processor on the audio thread

    void clearTape() { tapeLen = 0; tapeWrite = 0; tapeRecording = false; tapePos = 0; tape.clear(); tapePeaks.clear(); }

    void noteOn (int note, float vel, const MaterialParams& p)
    {
        if (p.mode == 2)
        {
            int idx = -1; uint32_t oldest = UINT32_MAX;
            for (int i = 0; i < kVoices; ++i) if (! voices[(size_t) i].env.active()) { idx = i; break; }
            if (idx < 0) for (int i = 0; i < kVoices; ++i) if (voices[(size_t) i].age < oldest) { oldest = voices[(size_t) i].age; idx = i; }
            auto& v = voices[(size_t) idx];
            v.note = note; v.vel = vel; v.age = ++ageCounter; v.started = false;
            v.env.noteOn();
        }
        else if (p.mode == 1 && p.midiTrig)
        {
            discNote = note; discVel = vel; discRestart = true;
            discEnv.noteOn();
        }
        else if (p.mode == 0 && p.midiTrig)
        {
            tapeNote = note; tapeRestart = true; tapeEnv.noteOn();
        }
    }
    void noteOff (int note)
    {
        for (auto& v : voices) if (v.note == note) v.env.noteOff();
        if (note == discNote) discEnv.noteOff();
        if (note == tapeNote) tapeEnv.noteOff();
    }
    void allNotesOff() { for (auto& v : voices) v.env.noteOff(); discEnv.noteOff(); tapeEnv.noteOff(); }

    // in: input (may be null). out: written (not added)
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const MaterialParams& p)
    {
        if (pending != nullptr) { sample = pending; pending = nullptr; discPos = 0; for (auto& v : voices) v.env.kill(); }

        for (int i = 0; i < n; ++i) { outL[i] = 0; outR[i] = 0; }
        lastPlayNorm = -1.0f;

        if (p.mode == 0) processTape (inL, inR, outL, outR, n, p);
        else
        {
            if (tapeRecording) finishRecording();
            if (p.mode == 1) processDisc (outL, outR, n, p);
            else             processPoly (outL, outR, n, p);
        }

        for (int i = 0; i < n; ++i)
        {
            float l = outL[i] * p.gain, r = outR[i] * p.gain;
            if (inL != nullptr && p.monitor > 0.0f) { l += inL[i] * p.monitor; r += inR[i] * p.monitor; }
            outL[i] = l; outR[i] = r;
        }
        playNorm.store (lastPlayNorm, std::memory_order_relaxed);
    }

    // ---- UI readable state ----
    std::atomic<float> playNorm { -1.0f };
    std::array<std::atomic<float>, kVoices> voiceNorm {};
    AtomicPeaks<kPeakBins> tapePeaks;
    std::atomic<int> tapeLenAtomic { 0 };
    std::atomic<bool> tapeRecAtomic { false };
    int getTapeMax() const { return tapeMax; }

private:
    struct Voice { AREnv env; double pos = 0; int note = 60; float vel = 1; uint32_t age = 0; bool started = false; };

    float sr = 48000;
    juce::AudioBuffer<float> tape;
    int tapeMax = 0, tapeLen = 0, tapeWrite = 0;
    bool tapeRecording = false;
    double tapePos = 0;
    float wowPhase = 0, flutPhase = 0;
    int tapeNote = -1; bool tapeRestart = false; AREnv tapeEnv;

    std::array<Voice, kVoices> voices;
    uint32_t ageCounter = 0;
    double discPos = 0;
    int discNote = -1; float discVel = 1; bool discRestart = false;
    AREnv discEnv;
    float lastPlayNorm = -1;

    void finishRecording()
    {
        tapeRecording = false;
        if (tapeLen == 0) tapeLen = juce::jmax (1, tapeWrite);
        tapeLenAtomic.store (tapeLen); tapeRecAtomic.store (false);
    }

    void markPeak (int idx, float a)
    {
        const int bin = (int) ((int64_t) idx * kPeakBins / juce::jmax (1, tapeMax));
        auto& slot = tapePeaks.v[(size_t) juce::jlimit (0, kPeakBins - 1, bin)];
        if (a > slot.load (std::memory_order_relaxed)) slot.store (a, std::memory_order_relaxed);
    }

    void processTape (const float* inL, const float* inR, float* outL, float* outR, int n, const MaterialParams& p)
    {
        float* tl = tape.getWritePointer (0); float* tr = tape.getWritePointer (1);
        const bool wantRec = p.rec && inL != nullptr;

        // Start of a fresh recording: tape is empty -> record linearly and define the loop length
        if (wantRec && ! tapeRecording && tapeLen == 0) { tapeRecording = true; tapeWrite = 0; tapePeaks.clear(); tapeRecAtomic.store (true); }
        if (! p.rec && tapeRecording) finishRecording();

        if (tapeRecording && tapeLen == 0)
        {
            for (int i = 0; i < n; ++i)
            {
                if (tapeWrite >= tapeMax) { finishRecording(); break; }
                tl[tapeWrite] = inL[i]; tr[tapeWrite] = inR[i];
                markPeak (tapeWrite, juce::jmax (std::abs (inL[i]), std::abs (inR[i])));
                ++tapeWrite;
            }
            lastPlayNorm = (float) tapeWrite / (float) tapeMax;
            tapeLenAtomic.store (tapeWrite);
            return;
        }
        if (tapeLen <= 1) return;

        tapeEnv.setTimes (p.attack, p.release, sr);
        if (tapeRestart) { tapeRestart = false; tapePos = p.start * tapeLen; }

        const double regionStart = (double) p.start * tapeLen;
        const double regionLen   = juce::jmax (64.0, (double) p.length * tapeLen);
        const float pitch = semisToRatio (p.tune + (p.midiTrig && tapeNote >= 0 ? (float) (tapeNote - p.root) : 0.0f));
        const float wowRate = 0.6f / sr, flutRate = 7.3f / sr;
        const bool overdub = wantRec;

        for (int i = 0; i < n; ++i)
        {
            wowPhase += wowRate; if (wowPhase > 1) wowPhase -= 1;
            flutPhase += flutRate; if (flutPhase > 1) flutPhase -= 1;
            const float wobble = 1.0f + p.wow * (0.012f * std::sin (kTwoPi * wowPhase) + 0.003f * std::sin (kTwoPi * flutPhase));
            const float amp = p.midiTrig ? tapeEnv.tick() : 1.0f;

            // Wrap inside loop region (supports negative varispeed)
            double rel = tapePos - regionStart;
            if (rel >= regionLen || rel < 0) { rel = std::fmod (rel, regionLen); if (rel < 0) rel += regionLen; tapePos = regionStart + rel; }
            double readPos = std::fmod (tapePos, (double) tapeLen); if (readPos < 0) readPos += tapeLen;

            const float l = hermite (tl, tapeLen, readPos), r = hermite (tr, tapeLen, readPos);
            outL[i] = l * amp; outR[i] = r * amp;

            if (overdub)
            {
                const int w = (int) readPos;
                tl[w] = tl[w] * p.dub + inL[i]; tr[w] = tr[w] * p.dub + inR[i];
                markPeak (w, juce::jmax (std::abs (tl[w]), std::abs (tr[w])));
            }
            if (p.playing) tapePos += (double) (p.speed * wobble * pitch);
        }
        lastPlayNorm = (float) (std::fmod (tapePos, (double) tapeLen) / tapeMax);
    }

    void processDisc (float* outL, float* outR, int n, const MaterialParams& p)
    {
        if (sample == nullptr || sample->buffer.getNumSamples() < 4) return;
        const int len = sample->buffer.getNumSamples();
        const float* sl = sample->buffer.getReadPointer (0); const float* sr2 = sample->buffer.getReadPointer (1);
        discEnv.setTimes (p.attack, p.release, sr);

        const double regionStart = (double) p.start * len;
        const double regionLen = juce::jmax (64.0, (double) p.length * len);
        const double rateFix = sample->sourceRate / sr;
        const float pitch = semisToRatio (p.tune + (p.midiTrig && discNote >= 0 ? (float) (discNote - p.root) : 0.0f));
        const double inc = rateFix * p.speed * pitch;

        if (discRestart) { discRestart = false; discPos = inc >= 0 ? regionStart : regionStart + regionLen - 1; }
        if (! p.midiTrig && ! p.playing) return;

        for (int i = 0; i < n; ++i)
        {
            double rel = discPos - regionStart;
            if (rel >= regionLen || rel < 0)
            {
                if (! p.loop && p.midiTrig) { discEnv.kill(); break; }
                rel = std::fmod (rel, regionLen); if (rel < 0) rel += regionLen;
                discPos = regionStart + rel;
            }
            const float amp = p.midiTrig ? discEnv.tick() * discVel : 1.0f;
            if (p.midiTrig && ! discEnv.active()) break;
            const double pos = juce::jlimit (0.0, (double) len - 1.001, discPos);
            outL[i] = hermite (sl, len, pos) * amp;
            outR[i] = hermite (sr2, len, pos) * amp;
            discPos += inc;
        }
        lastPlayNorm = (float) (discPos / len);
    }

    void processPoly (float* outL, float* outR, int n, const MaterialParams& p)
    {
        for (int vi = 0; vi < kVoices; ++vi) voiceNorm[(size_t) vi].store (-1.0f, std::memory_order_relaxed);
        if (sample == nullptr || sample->buffer.getNumSamples() < 4) return;
        const int len = sample->buffer.getNumSamples();
        const float* sl = sample->buffer.getReadPointer (0); const float* sr2 = sample->buffer.getReadPointer (1);
        const double regionStart = (double) p.start * len;
        const double regionLen = juce::jmax (64.0, (double) p.length * len);
        const double rateFix = sample->sourceRate / sr;

        for (int vi = 0; vi < kVoices; ++vi)
        {
            auto& v = voices[(size_t) vi];
            if (! v.env.active()) continue;
            v.env.setTimes (p.attack, p.release, sr);
            const double inc = rateFix * p.speed * semisToRatio (p.tune + (float) (v.note - p.root));
            if (! v.started) { v.started = true; v.pos = inc >= 0 ? regionStart : regionStart + regionLen - 1; }
            for (int i = 0; i < n; ++i)
            {
                double rel = v.pos - regionStart;
                if (rel >= regionLen || rel < 0)
                {
                    if (! p.loop) { v.env.kill(); break; }
                    rel = std::fmod (rel, regionLen); if (rel < 0) rel += regionLen;
                    v.pos = regionStart + rel;
                }
                const float amp = v.env.tick() * v.vel * 0.6f;
                const double pos = juce::jlimit (0.0, (double) len - 1.001, v.pos);
                outL[i] += hermite (sl, len, pos) * amp;
                outR[i] += hermite (sr2, len, pos) * amp;
                v.pos += inc;
                if (! v.env.active()) break;
            }
            voiceNorm[(size_t) vi].store ((float) (v.pos / len), std::memory_order_relaxed);
            lastPlayNorm = (float) (v.pos / len);
        }
    }
};

// =====================================================================================
//  2. GRANULAR  (4-second buffer, up to 128 grains)
// =====================================================================================
struct GranularParams
{
    float mix = 0, pos = 0.2f, size = 0.12f, density = 20, spray = 0.1f, scan = 0, pitch = 0, pitchRand = 0;
    int quant = 0; float shape = 0.5f, spread = 0.5f, reverse = 0; bool freeze = false;
};

class Granular
{
public:
    static constexpr int kMaxGrains = 128;
    static constexpr int kPeakBins = 256;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        len = (int) (sampleRate * 4.0);
        buf.setSize (2, len); buf.clear();
        write = 0; scanOffset = 0; spawnAcc = 0;
        for (auto& g : grains) g.active = false;
        rng.seed ((uint32_t) (size_t) this);
        peaks.clear();
    }

    void process (float* l, float* r, int n, const GranularParams& p)
    {
        float* bl = buf.getWritePointer (0); float* br = buf.getWritePointer (1);
        const auto& win = CosTable::get();
        const float sizeSamps = juce::jlimit (32.0f, sr * 1.0f, p.size * sr);
        const float est = juce::jmax (1.0f, p.density * p.size);
        const float norm = 1.0f / std::sqrt (est);
        const float shapePk = juce::jlimit (0.02f, 0.98f, p.shape);

        if (p.freeze && ! frozen) { frozen = true; freezeAnchor = write; scanOffset = 0; }
        if (! p.freeze) frozen = false;

        for (int i = 0; i < n; ++i)
        {
            const float inL = l[i], inR = r[i];
            if (! frozen)
            {
                bl[write] = inL; br[write] = inR;
                updatePeak (write, juce::jmax (std::abs (inL), std::abs (inR)));
                if (++write >= len) write = 0;
            }

            // scan: moves the read centre at 'scan' x realtime
            scanOffset += p.scan;
            const float scanRange = frozen ? (float) len : (float) len * 0.85f;
            if (scanOffset > scanRange) scanOffset -= scanRange;
            if (scanOffset < -scanRange) scanOffset += scanRange;

            spawnAcc += p.density / sr;
            while (spawnAcc >= 1.0f) { spawnAcc -= 1.0f; spawn (p, sizeSamps); }

            float wl = 0, wr = 0;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const float t = (float) g.age / g.length;
                const float w = t < g.peak ? win.rise (t / g.peak) : 1.0f - win.rise ((t - g.peak) / (1.0f - g.peak));
                const float sl = linearRead (bl, len, g.pos), sr2 = linearRead (br, len, g.pos);
                const float mono = 0.5f * (sl + sr2);
                // spread grains across the stereo field, keeping some of the source image
                wl += (sl * (1 - g.spreadAmt) + mono * g.spreadAmt * g.gl) * w;
                wr += (sr2 * (1 - g.spreadAmt) + mono * g.spreadAmt * g.gr) * w;
                g.pos += g.inc;
                if (++g.age >= (int) g.length) g.active = false;
            }
            (void) shapePk;
            l[i] = inL * (1 - p.mix) + wl * norm * p.mix;
            r[i] = inR * (1 - p.mix) + wr * norm * p.mix;
        }

        // report for UI
        for (int gi = 0; gi < kMaxGrains; ++gi)
        {
            const auto& g = grains[(size_t) gi];
            float v = -1.0f;
            if (g.active) { double pp = std::fmod (g.pos, (double) len); if (pp < 0) pp += len; v = (float) (pp / len); }
            grainPos[(size_t) gi].store (v, std::memory_order_relaxed);
        }
        writeNorm.store ((float) write / (float) len, std::memory_order_relaxed);
        frozenAtomic.store (frozen, std::memory_order_relaxed);
    }

    std::array<std::atomic<float>, kMaxGrains> grainPos {};
    std::atomic<float> writeNorm { 0 };
    std::atomic<bool> frozenAtomic { false };
    AtomicPeaks<kPeakBins> peaks;

private:
    struct Grain { bool active = false; double pos = 0, inc = 1; float length = 1; int age = 0; float gl = 1, gr = 1, spreadAmt = 0, peak = 0.5f; };
    std::array<Grain, kMaxGrains> grains;
    juce::AudioBuffer<float> buf;
    int len = 1, write = 0, freezeAnchor = 0, peakBin = -1;
    bool frozen = false;
    float sr = 48000, spawnAcc = 0, scanOffset = 0;
    Random rng;

    void updatePeak (int idx, float a)
    {
        const int bin = idx * kPeakBins / len;
        auto& s = peaks.v[(size_t) bin];
        if (bin != peakBin) { peakBin = bin; s.store (a, std::memory_order_relaxed); }
        else if (a > s.load (std::memory_order_relaxed)) s.store (a, std::memory_order_relaxed);
    }

    void spawn (const GranularParams& p, float sizeSamps)
    {
        Grain* g = nullptr;
        for (auto& gg : grains) if (! gg.active) { g = &gg; break; }
        if (g == nullptr) return;

        float semis = p.pitch + rng.bi() * p.pitchRand;
        if (p.quant == 1) semis = std::round (semis);
        else if (p.quant == 2) { const float q[] = { 0, 7, 12 }; float o = std::floor (semis / 12.0f) * 12.0f, rem = semis - o, best = 0, bd = 99; for (float c : q) if (std::abs (rem - c) < bd) { bd = std::abs (rem - c); best = c; } semis = o + best; }
        else if (p.quant == 3) semis = std::round (semis / 12.0f) * 12.0f;
        double inc = semisToRatio (semis);
        const bool rev = rng.uni() < p.reverse;
        if (rev) inc = -inc;

        // How far behind the write head the grain starts
        const float minDelay = 64.0f + (inc > 1.0 ? (float) ((inc - 1.0) * sizeSamps) : 0.0f) + (rev ? 0.0f : 0.0f);
        const float usable = (float) len * 0.95f - minDelay - sizeSamps * 2.0f;
        float delay = minDelay + p.pos * juce::jmax (0.0f, usable) + rng.bi() * p.spray * sr * 0.5f;
        if (rev) delay += (float) (std::abs (inc) * sizeSamps);   // reversed grains read backwards from here
        double centre;
        if (frozen) centre = (double) freezeAnchor - delay + scanOffset;
        else
        {
            delay -= scanOffset;  // scanOffset is wrapped to the usable range
            delay = juce::jlimit (minDelay, (float) len * 0.97f - sizeSamps, delay);
            centre = (double) write - delay;
        }
        centre = std::fmod (centre, (double) len); if (centre < 0) centre += len;

        g->active = true; g->pos = centre; g->inc = inc; g->length = sizeSamps; g->age = 0;
        g->peak = juce::jlimit (0.02f, 0.98f, p.shape);
        const float pan = rng.bi() * p.spread;      // -1..1
        g->spreadAmt = p.spread;
        g->gl = std::sqrt (0.5f * (1.0f - pan)) * 1.414f;
        g->gr = std::sqrt (0.5f * (1.0f + pan)) * 1.414f;
    }
};

// =====================================================================================
//  3. FILTER  (SVF morphing LP-BP-HP  <->  48-band spectral resonator)
// =====================================================================================
struct FilterParams
{
    float mix = 1, morph = 0, cutoff = 20000, reso = 0.1f, type = 0, drive = 0;
    float rootHz = 130.8f, spread = 0.5f, decay = 0.5f, tilt = -0.3f; int bands = 24;
};

class Filter
{
public:
    static constexpr int kBands = 48;

    void prepare (double sampleRate) { sr = (float) sampleRate; for (auto& s : svf) s.reset(); for (auto& b : bank) b.reset(); lastRoot = -1; }

    void process (float* l, float* r, int n, const FilterParams& p)
    {
        const float q = 0.5f + p.reso * p.reso * 24.0f;
        svf[0].set (p.cutoff, q, sr); svf[1].set (p.cutoff, q, sr);
        const bool useRes = p.morph > 0.001f;
        if (useRes) updateBank (p);

        const float lpA = juce::jlimit (0.0f, 1.0f, 1.0f - p.type);
        const float bpA = 1.0f - std::abs (p.type - 1.0f);
        const float hpA = juce::jlimit (0.0f, 1.0f, p.type - 1.0f);
        const float drv = 1.0f + p.drive * 8.0f, drvNorm = 1.0f / std::tanh (drv) * (p.drive > 0 ? 1.0f : 0.0f);

        float* ch[2] = { l, r };
        for (int c = 0; c < 2; ++c)
        {
            float* x = ch[c];
            for (int i = 0; i < n; ++i)
            {
                const float dry = x[i];
                float in = p.drive > 0.0f ? std::tanh (dry * drv) * drvNorm : dry;
                float lp, bp, hp;
                svf[c].tick (in, lp, bp, hp);
                const float sv = lp * lpA + bp * bpA + hp * hpA;
                float res = 0;
                if (useRes)
                {
                    for (int b = 0; b < activeBands; ++b)
                        res += bank[(size_t) (b * 2 + c)].bandpass (in) * bankGain[(size_t) b];
                    res = std::tanh (res * resMakeup * 0.5f) * 2.0f;
                }
                const float wet = sv * (1 - p.morph) + res * p.morph;
                x[i] = dry * (1 - p.mix) + wet * p.mix;
            }
            svf[c].sanitize();
        }
        if (useRes) for (auto& b : bank) b.sanitize();
    }

    // Curve for UI (magnitude approx at freq)
    std::atomic<float> uiRoot { 130.8f };

private:
    float sr = 48000;
    SVF svf[2];
    std::array<SVF, kBands * 2> bank;
    std::array<float, kBands> bankGain {};
    int activeBands = 0;
    float resMakeup = 1;
    float lastRoot = -1, lastSpread = -1, lastDecay = -1, lastTilt = -9; int lastBands = -1;

    void updateBank (const FilterParams& p)
    {
        if (std::abs (p.rootHz - lastRoot) < 0.01f && std::abs (p.spread - lastSpread) < 1e-4f && std::abs (p.decay - lastDecay) < 1e-4f
            && std::abs (p.tilt - lastTilt) < 1e-4f && p.bands == lastBands) return;
        lastRoot = p.rootHz; lastSpread = p.spread; lastDecay = p.decay; lastTilt = p.tilt; lastBands = p.bands;
        const float expo = std::exp2 ((p.spread - 0.5f) * 2.0f);     // 0.5 .. 2 (1 = harmonic)
        const float Q = 4.0f * std::pow (100.0f, p.decay);           // 4 .. 400
        activeBands = 0;
        float sumG = 0;
        for (int b = 0; b < juce::jlimit (1, kBands, p.bands); ++b)
        {
            const float f = p.rootHz * std::pow ((float) (b + 1), expo);
            if (f > sr * 0.45f) break;
            bank[(size_t) (b * 2)].set (f, Q, sr); bank[(size_t) (b * 2 + 1)].set (f, Q, sr);
            const float g = std::pow ((float) (b + 1), p.tilt);
            bankGain[(size_t) b] = g / Q * 2.0f;   // bp peak gain is Q; normalise and keep some ring
            sumG += g;
            ++activeBands;
        }
        resMakeup = 4.0f / std::sqrt (juce::jmax (1.0f, sumG)) * std::sqrt (Q) * 0.5f;
        uiRoot.store (p.rootHz);
    }
};

// =====================================================================================
//  4. COLOR  (dual-band shaper, tone, bitcrush, downsample, noise, glue compressor)
// =====================================================================================
struct ColorParams
{
    float mix = 1, drive = 0, band = 0, tone = 0, comp = 0, noise = 0, out = 1, bits = 16, down = 1; int type = 0;
};

inline float shapeSample (float x, int type)
{
    switch (type)
    {
        case 0: return std::tanh (x);
        case 1: return juce::jlimit (-1.0f, 1.0f, x);
        case 2: { // wavefolder
            float y = x * 0.25f + 0.25f; y = y - std::floor (y);    // 0..1 triangle phase
            return (std::abs (y * 4.0f - 2.0f) - 1.0f);
        }
        default: { // tube-ish asymmetric
            return x >= 0 ? std::tanh (x) : std::tanh (x * 0.6f) / 0.6f * 0.8f;
        }
    }
}

class Color
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (int c = 0; c < 2; ++c) { split[c].set (300.0f, sr); tilt[c].set (900.0f, sr); dc[c].reset(); held[c] = 0; }
        env = 0; holdPhase = 0;
        rng.seed (0x51ED270Bu);
    }

    void process (float* l, float* r, int n, const ColorParams& p)
    {
        const float lowAmt = p.drive * (1.0f - juce::jmax (0.0f, p.band));
        const float highAmt = p.drive * (1.0f - juce::jmax (0.0f, -p.band));
        const float gL = 1.0f + lowAmt * lowAmt * 40.0f, gH = 1.0f + highAmt * highAmt * 40.0f;
        const float mL = 1.0f / std::pow (gL, 0.7f), mH = 1.0f / std::pow (gH, 0.7f);
        const bool crush = p.bits < 15.95f;
        const float q = std::exp2 (p.bits - 1.0f);
        const float holdInc = 1.0f / juce::jmax (1.0f, p.down);
        const float thr = dbToGain (-6.0f - p.comp * 30.0f), ratio = 4.0f;
        const float atk = std::exp (-1.0f / (0.008f * sr)), rel = std::exp (-1.0f / (0.15f * sr));
        const float makeup = dbToGain (p.comp * 14.0f);
        const float noiseAmt = p.noise * p.noise * 0.12f;

        for (int i = 0; i < n; ++i)
        {
            float x[2] = { l[i], r[i] };
            float y[2];
            for (int c = 0; c < 2; ++c)
            {
                float s = x[c];
                if (p.drive > 0.001f)
                {
                    const float lo = split[c].lp (s), hi = s - lo;
                    s = shapeSample (lo * gL, p.type) * mL + shapeSample (hi * gH, p.type) * mH;
                }
                // tilt EQ
                const float tl = tilt[c].lp (s), th = s - tl;
                s = tl * (1.0f - p.tone * 0.8f) + th * (1.0f + p.tone * 0.8f);
                s += rng.bi() * noiseAmt;
                y[c] = s;
            }
            // downsample (sample & hold) + bit reduction
            holdPhase += holdInc;
            if (holdPhase >= 1.0f || p.down <= 1.001f)
            {
                holdPhase -= std::floor (holdPhase);
                for (int c = 0; c < 2; ++c) held[c] = crush ? std::round (y[c] * q) / q : y[c];
            }
            y[0] = held[0]; y[1] = held[1];

            if (p.comp > 0.001f)
            {
                const float lvl = juce::jmax (std::abs (y[0]), std::abs (y[1]));
                env = lvl > env ? atk * env + (1 - atk) * lvl : rel * env + (1 - rel) * lvl;
                float gr = 1.0f;
                if (env > thr) gr = std::pow (env / thr, 1.0f / ratio - 1.0f);
                y[0] *= gr * makeup; y[1] *= gr * makeup;
            }
            for (int c = 0; c < 2; ++c) y[c] = dc[c].tick (y[c]) * p.out;
            l[i] = x[0] * (1 - p.mix) + y[0] * p.mix;
            r[i] = x[1] * (1 - p.mix) + y[1] * p.mix;
        }
    }

private:
    float sr = 48000, env = 0, holdPhase = 0, held[2] {};
    OnePole split[2], tilt[2];
    DCBlock dc[2];
    Random rng;
};

// =====================================================================================
//  5. SPACE  (stereo delay with shimmer + freeze, 8-line FDN reverb)
// =====================================================================================
struct SpaceParams
{
    float mix = 0, time = 0.375f, fb = 0.4f, shimmer = 0, shimSemis = 12, dLevel = 0.6f, rLevel = 0.6f;
    float size = 0.6f, decay = 0.6f, damp = 0.4f, width = 1, dToR = 0.3f; bool freeze = false;
};

class Space
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        dLen = (int) (sampleRate * 2.6);
        for (auto& d : dly) { d.assign ((size_t) dLen, 0.0f); }
        dW = 0; curTime = 0.375f * sr;
        for (int c = 0; c < 2; ++c) { fbLp[c].set (7000.0f, sr); fbHp[c].set (90.0f, sr); fbLp[c].reset(); fbHp[c].reset(); }
        shLen = (int) (sampleRate * 0.25);
        for (auto& s : shBuf) s.assign ((size_t) shLen, 0.0f);
        shW = 0; shPhase = 0;

        static const int base[8] = { 1117, 1301, 1493, 1709, 1931, 2129, 2357, 2579 };
        for (int k = 0; k < 8; ++k)
        {
            fdnMax[k] = (int) (base[k] * 2.0f * sampleRate / 48000.0) + 8;
            fdn[k].assign ((size_t) fdnMax[k], 0.0f);
            fdnW[k] = 0; fdnLp[k] = 0;
        }
        static const int ap[4] = { 142, 107, 379, 277 };
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k)
            {
                apLen[c][k] = (int) (ap[k] * (c == 0 ? 1.0f : 1.07f) * sampleRate / 48000.0) + 1;
                apBuf[c][k].assign ((size_t) apLen[c][k], 0.0f); apW[c][k] = 0;
            }
    }

    void process (float* l, float* r, int n, const SpaceParams& p)
    {
        const float target = juce::jlimit (16.0f, (float) dLen - 8.0f, p.time * sr);
        const float fb = p.freeze ? 1.0f : juce::jmin (0.97f, p.fb * 1.05f);
        const float inGain = p.freeze ? 0.0f : 1.0f;
        const float shRatio = semisToRatio (p.shimSemis);
        const float sizeScale = 0.35f + p.size * 1.6f;
        const float rt60 = p.freeze ? 1.0e9f : 0.3f + p.decay * p.decay * 18.0f;
        const float dampA = 1.0f - std::exp (-kTwoPi * (18000.0f - p.damp * 16500.0f) / sr);
        int lenK[8]; float gK[8];
        for (int k = 0; k < 8; ++k)
        {
            lenK[k] = juce::jlimit (8, fdnMax[k] - 2, (int) ((fdnMax[k] - 8) * 0.5f * sizeScale));
            gK[k] = p.freeze ? 1.0f : std::pow (10.0f, -3.0f * (float) lenK[k] / (rt60 * sr));
        }
        const float ws = p.width;

        for (int i = 0; i < n; ++i)
        {
            const float inL = l[i], inR = r[i];
            curTime += (target - curTime) * 0.0004f;              // tape-style time glide

            // ---- delay ----
            float dOut[2];
            for (int c = 0; c < 2; ++c)
            {
                const double rp = (double) dW - curTime * (c == 0 ? 1.0f : 1.013f);
                dOut[c] = hermite (dly[c].data(), dLen, rp);
            }
            // shimmer pitch shifter on the feedback signal
            float shOut[2] = { 0, 0 };
            if (p.shimmer > 0.001f)
            {
                for (int c = 0; c < 2; ++c) shBuf[c][(size_t) shW] = dOut[c];
                shPhase += (1.0f - shRatio) / (float) shLen * 0.5f;     // window sweeps
                shPhase -= std::floor (shPhase);
                for (int t = 0; t < 2; ++t)
                {
                    float ph = shPhase + t * 0.5f; ph -= std::floor (ph);
                    const float w = std::sin (kPi * ph);
                    const double rp = (double) shW - ph * shLen * 0.5f - 4;
                    for (int c = 0; c < 2; ++c) shOut[c] += linearRead (shBuf[c].data(), shLen, rp) * w;
                }
                if (++shW >= shLen) shW = 0;
            }
            for (int c = 0; c < 2; ++c)
            {
                float f = dOut[c] * (1.0f - p.shimmer) + shOut[c] * p.shimmer;
                if (! p.freeze) { f = fbLp[c].lp (f); f = f - fbHp[c].lp (f); }
                const float in = (c == 0 ? inL : inR) * inGain;
                float w = in + f * fb;
                w = p.freeze ? w : std::tanh (w * 0.7f) / 0.7f;
                dly[c][(size_t) dW] = w;
            }
            if (++dW >= dLen) dW = 0;

            // ---- reverb ----
            float rin[2] = { (inL + dOut[0] * p.dToR) * inGain, (inR + dOut[1] * p.dToR) * inGain };
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < 4; ++k)
                {
                    auto& b = apBuf[c][k]; int& w = apW[c][k];
                    const float d = b[(size_t) w];
                    const float v = rin[c] + 0.6f * d;
                    b[(size_t) w] = v; rin[c] = d - 0.6f * v;
                    if (++w >= apLen[c][k]) w = 0;
                }
            float o[8];
            for (int k = 0; k < 8; ++k)
            {
                int rp = fdnW[k] - lenK[k]; if (rp < 0) rp += fdnMax[k];
                float v = fdn[k][(size_t) rp];
                if (! p.freeze) { fdnLp[k] += dampA * (v - fdnLp[k]); v = fdnLp[k]; }
                o[k] = v * gK[k];
            }
            // Hadamard 8x8 mix
            float h[8];
            hadamard8 (o, h);
            for (int k = 0; k < 8; ++k)
            {
                const float inj = (k & 1) ? rin[1] : rin[0];
                fdn[k][(size_t) fdnW[k]] = h[k] + inj * 0.5f;
                if (++fdnW[k] >= fdnMax[k]) fdnW[k] = 0;
            }
            float rvL = (o[0] + o[2] + o[4] + o[6]) * 0.35f;
            float rvR = (o[1] + o[3] + o[5] + o[7]) * 0.35f;

            float wetL = dOut[0] * p.dLevel + rvL * p.rLevel;
            float wetR = dOut[1] * p.dLevel + rvR * p.rLevel;
            const float m = 0.5f * (wetL + wetR), s = 0.5f * (wetL - wetR) * ws;
            wetL = m + s; wetR = m - s;
            l[i] = inL * (1.0f - p.mix * 0.5f) + wetL * p.mix;
            r[i] = inR * (1.0f - p.mix * 0.5f) + wetR * p.mix;
        }
        for (auto& v : fdnLp) if (! std::isfinite (v)) v = 0;
    }

private:
    float sr = 48000, curTime = 1000;
    std::vector<float> dly[2]; int dLen = 1, dW = 0;
    OnePole fbLp[2], fbHp[2];
    std::vector<float> shBuf[2]; int shLen = 1, shW = 0; float shPhase = 0;
    std::vector<float> fdn[8]; int fdnMax[8] {}, fdnW[8] {}; float fdnLp[8] {};
    std::vector<float> apBuf[2][4]; int apLen[2][4] {}, apW[2][4] {};

    static void hadamard8 (const float* a, float* o)
    {
        float t[8];
        for (int i = 0; i < 8; ++i) t[i] = a[i];
        for (int h = 1; h < 8; h <<= 1)
            for (int i = 0; i < 8; i += h * 2)
                for (int j = i; j < i + h; ++j) { const float x = t[j], y = t[j + h]; t[j] = x + y; t[j + h] = x - y; }
        const float s = 0.35355339f;
        for (int i = 0; i < 8; ++i) o[i] = t[i] * s;
    }
};

// =====================================================================================
//  Modulator: LFO shapes, S&H, random walk, AD envelope, envelope follower
// =====================================================================================
class Modulator
{
public:
    enum Type { Sine, Tri, Saw, Square, SH, Walk, Env, Follow };
    // advance by dt seconds, returns value (bipolar for LFOs, unipolar for env/follower)
    float tick (int type, float rate, float shape, float dt, float followerLevel)
    {
        switch (type)
        {
            case Env:
            {
                const float T = 1.0f / juce::jmax (0.01f, rate);
                const float atk = juce::jmax (0.001f, T * shape), dec = juce::jmax (0.005f, T * (1.0f - shape));
                if (envStage == 1) { envVal += dt / atk; if (envVal >= 1) { envVal = 1; envStage = 2; } }
                else if (envStage == 2) { envVal -= dt / dec; if (envVal <= 0) { envVal = 0; envStage = 0; } }
                value = envVal; break;
            }
            case Follow:
            {
                const float coef = 1.0f - std::exp (-dt * juce::jmax (0.05f, rate) * 6.0f);
                const float target = clamp01 (followerLevel * (1.0f + shape * 8.0f));
                folVal += ((target > folVal ? 1.0f : coef) * (target - folVal)) * (target > folVal ? 0.5f : 1.0f);
                value = folVal; break;
            }
            default:
            {
                phase += rate * dt;
                bool wrapped = false;
                if (phase >= 1.0f) { phase -= std::floor (phase); wrapped = true; }
                const float sk = juce::jlimit (0.01f, 0.99f, shape);
                const float p = phase < sk ? phase / sk * 0.5f : 0.5f + (phase - sk) / (1 - sk) * 0.5f;
                switch (type)
                {
                    case Sine:   value = std::sin (kTwoPi * p); break;
                    case Tri:    value = 1.0f - 4.0f * std::abs (p - 0.5f); break;
                    case Saw:    value = 2.0f * p - 1.0f; break;
                    case Square: value = phase < sk ? 1.0f : -1.0f; break;
                    case SH:
                        if (wrapped) shTarget = rng.bi();
                        value += (shTarget - value) * (1.0f - std::exp (-dt / (0.0005f + shape * shape * 0.3f)));
                        break;
                    case Walk:
                        value += rng.bi() * shape * std::sqrt (dt * rate) * 0.8f;
                        if (value > 1) value = 2 - value; if (value < -1) value = -2 - value;
                        break;
                }
            }
        }
        return value;
    }
    void trigger() { envStage = 1; }
    float value = 0;
    void seed (uint32_t s) { rng.seed (s); }
private:
    float phase = 0, shTarget = 0, envVal = 0, folVal = 0; int envStage = 0;
    Random rng;
};
} // namespace quarry::dsp
