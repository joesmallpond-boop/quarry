// Offline test harness: drives the processor without a host, checks the audio is sane,
// measures CPU, and renders editor screenshots of every view.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <chrono>

using namespace juce;
using namespace quarry;

static void setP (QuarryProcessor& p, int t, int tp, float value)
{
    auto* prm = p.trackParam (t, tp);
    prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}
static void setG (QuarryProcessor& p, const String& id, float value)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

struct Stats { float peak = 0; double rms = 0; bool finite = true; };

static Stats render (QuarryProcessor& p, int blocks, int bs, std::function<void (AudioBuffer<float>&, MidiBuffer&, int)> feed, AudioBuffer<float>* capture = nullptr)
{
    Stats s; double sum = 0; int64 n = 0;
    AudioBuffer<float> buf (2, bs); MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear(); midi.clear();
        if (feed) feed (buf, midi, b);
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < bs; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) s.finite = false;
                s.peak = jmax (s.peak, std::abs (v)); sum += v * v; ++n;
            }
        if (capture) for (int c = 0; c < 2; ++c) capture->copyFrom (c, b * bs, buf, c, 0, bs);
    }
    s.rms = std::sqrt (sum / jmax ((int64) 1, n));
    return s;
}

static void report (const String& name, const Stats& s)
{
    std::cout << name.paddedRight (' ', 34) << " peak " << String (s.peak, 3).paddedLeft (' ', 6)
              << "  rms " << String (s.rms, 4).paddedLeft (' ', 7) << (s.finite ? "" : "  NON-FINITE!") << std::endl;
}

static File makeTestSample (const File& dir)
{
    // 2 s of drum-ish hits and a pitched tone, 44.1 kHz (exercises resampling)
    const double sr = 44100; const int len = (int) (sr * 2);
    AudioBuffer<float> b (2, len); b.clear();
    Random rng (1);
    for (int hit = 0; hit < 8; ++hit)
    {
        const int s0 = hit * len / 8;
        for (int i = 0; i < 6000 && s0 + i < len; ++i)
        {
            const float env = std::exp (-(float) i / 900.0f);
            const float v = (hit % 2 == 0 ? std::sin (2 * 3.14159f * 60.0f * i / (float) sr) * 0.9f : (rng.nextFloat() * 2 - 1) * 0.6f) * env;
            b.addSample (0, s0 + i, v); b.addSample (1, s0 + i, v);
        }
    }
    for (int i = 0; i < len; ++i) { const float t = std::sin (2 * 3.14159f * 220.0f * i / (float) sr) * 0.15f; b.addSample (0, i, t); b.addSample (1, i, t * 0.8f); }
    auto f = dir.getChildFile ("test_sample.wav");
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, len);
    return f;
}

static void writeWav (const File& f, const AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir (argc > 1 ? argv[1] : File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    const double sr = 48000; const int bs = 512;
    const int sec = (int) (sr / bs);
    int failures = 0;
    auto check = [&] (bool ok, const String& what) { if (! ok) { ++failures; std::cout << "  FAIL: " << what << std::endl; } };

    auto proc = std::make_unique<QuarryProcessor>();
    proc->setPlayConfigDetails (2, 2, sr, bs);
    proc->prepareToPlay (sr, bs);

    // 1. silence by default
    auto s = render (*proc, sec, bs, nullptr); report ("default (no material)", s);
    check (s.finite && s.peak < 1e-4f, "default should be silent");

    // 2. Disc mode, free-running sample through all five stages
    const auto sample = makeTestSample (outDir);
    check (proc->loadSample (0, sample), "sample load");
    s = render (*proc, sec, bs, nullptr); report ("T1 disc dry", s);
    check (s.finite && s.rms > 0.01, "disc playback should be audible");

    setP (*proc, 0, GR_MIX, 0.8f); setP (*proc, 0, GR_DENSITY, 60); setP (*proc, 0, GR_PITCH, 7); setP (*proc, 0, GR_PRAND, 5);
    setP (*proc, 0, GR_REVERSE, 0.4f); setP (*proc, 0, GR_SPRAY, 0.4f);
    s = render (*proc, sec * 2, bs, nullptr); report ("T1 + granular", s);
    check (s.finite && s.rms > 0.005, "granular output");

    setP (*proc, 0, FL_MORPH, 1.0f); setP (*proc, 0, FL_RDECAY, 0.8f);
    s = render (*proc, sec * 2, bs, nullptr); report ("T1 + resonator (morph 100%)", s);
    check (s.finite && s.rms > 0.003, "resonator output");
    setP (*proc, 0, FL_MORPH, 0.4f); setP (*proc, 0, FL_CUTOFF, 1500); setP (*proc, 0, FL_RESO, 0.7f); setP (*proc, 0, FL_TYPE, 1.0f);

    setP (*proc, 0, CO_DRIVE, 0.7f); setP (*proc, 0, CO_TYPE, 2); setP (*proc, 0, CO_BITS, 6); setP (*proc, 0, CO_DOWN, 4); setP (*proc, 0, CO_COMP, 0.6f); setP (*proc, 0, CO_NOISE, 0.2f);
    s = render (*proc, sec * 2, bs, nullptr); report ("T1 + color (fold, crush, glue)", s);
    check (s.finite && s.rms > 0.003, "color output");

    setP (*proc, 0, SP_MIX, 0.7f); setP (*proc, 0, SP_SHIMMER, 0.5f); setP (*proc, 0, SP_DFB, 0.8f); setP (*proc, 0, SP_RDECAY, 0.9f);
    s = render (*proc, sec * 3, bs, nullptr); report ("T1 + space (shimmer, long verb)", s);
    check (s.finite && s.rms > 0.003, "space output");

    // modulators on everything
    setP (*proc, 0, M1_TGT, 2.0f + (float) std::distance (modTargetIndices().begin(), std::find (modTargetIndices().begin(), modTargetIndices().end(), (int) FL_CUTOFF)) - 1.0f);
    setP (*proc, 0, M1_DEPTH, 0.6f); setP (*proc, 0, M1_RATE, 3.0f);
    setP (*proc, 0, M2_TGT, 3); setP (*proc, 0, M2_DEPTH, -0.4f);
    setP (*proc, 0, M3_TGT, 10); setP (*proc, 0, M3_DEPTH, 0.5f);
    setP (*proc, 0, M4_TYPE, 7); setP (*proc, 0, M4_TGT, 20); setP (*proc, 0, M4_DEPTH, 0.5f);
    s = render (*proc, sec * 2, bs, nullptr); report ("T1 + 4 modulators", s);
    check (s.finite, "mods finite");

    // render a full-chain demo to disk
    {
        AudioBuffer<float> cap (2, sec * 6 * bs);
        s = render (*proc, sec * 6, bs, nullptr, &cap);
        writeWav (outDir.getChildFile ("demo_track1_full_chain.wav"), cap, sr);
        report ("demo render (6 s)", s);
    }

    // 3. Tape mode: record external input then loop it
    setP (*proc, 1, MAT_MODE, 0); setP (*proc, 1, MAT_INPUT, 1); setP (*proc, 0, MX_MUTE, 1);
    setP (*proc, 1, MAT_REC, 1);
    double ph = 0;
    auto sine = [&] (AudioBuffer<float>& b, MidiBuffer&, int) { for (int i = 0; i < b.getNumSamples(); ++i) { const float v = (float) std::sin (ph) * 0.5f; ph += 2 * 3.14159 * 330 / sr; b.setSample (0, i, v); b.setSample (1, i, v); } };
    render (*proc, sec * 2, bs, sine);
    setP (*proc, 1, MAT_REC, 0);
    s = render (*proc, sec * 2, bs, nullptr); report ("T2 tape loop playback", s);
    check (s.finite && s.rms > 0.05, "tape loop should replay recorded input");
    check (proc->tracks[1].material.tapeLenAtomic.load() > (int) sr, "tape length recorded");
    // overdub
    setP (*proc, 1, MAT_REC, 1); render (*proc, sec, bs, sine); setP (*proc, 1, MAT_REC, 0);
    setP (*proc, 1, MAT_SPEED, -0.5f); setP (*proc, 1, MAT_WOW, 1.0f);
    s = render (*proc, sec * 2, bs, nullptr); report ("T2 tape reverse half-speed + wow", s);
    check (s.finite && s.rms > 0.02, "varispeed tape");

    // live granular freeze on tape input
    setP (*proc, 1, GR_MIX, 1); setP (*proc, 1, GR_FREEZE, 1); setP (*proc, 1, GR_SCAN, 0.5f);
    s = render (*proc, sec * 2, bs, nullptr); report ("T2 frozen grain cloud", s);
    check (s.finite && s.rms > 0.01, "frozen granular");
    setP (*proc, 1, MX_MUTE, 1);

    // 4. Poly mode with MIDI
    setP (*proc, 2, MAT_MODE, 2);
    proc->pasteTrack (2); // (clipboard empty - should be a no-op)
    proc->copyTrack (0); proc->pasteTrack (2);
    setP (*proc, 2, MAT_MODE, 2); setP (*proc, 2, MX_MUTE, 0); setP (*proc, 2, MAT_LENGTH, 0.3f);
    setP (*proc, 2, MAT_RELEASE, 0.2f);
    auto chords = [&] (AudioBuffer<float>&, MidiBuffer& m, int b)
    {
        if (b % (sec / 2) == 0) for (int n : { 60, 64, 67, 71 }) m.addEvent (MidiMessage::noteOn (1, n + (b / (sec / 2)) % 5, 0.8f), 10);
        if (b % (sec / 2) == sec / 4) for (int nn = 55; nn < 90; ++nn) m.addEvent (MidiMessage::noteOff (1, nn), 5);
    };
    s = render (*proc, sec * 3, bs, chords); report ("T3 poly chords (pasted from T1)", s);
    check (s.finite && s.rms > 0.003, "poly playback");
    // 9-note stress (voice stealing)
    auto cluster = [&] (AudioBuffer<float>&, MidiBuffer& m, int b) { if (b == 0) for (int n = 48; n < 60; ++n) m.addEvent (MidiMessage::noteOn (1, n, 1.0f), n); };
    s = render (*proc, sec, bs, cluster); report ("T3 12-note cluster (8 voices)", s);
    check (s.finite, "voice stealing");

    // 5. inter-track feedback: T4 listens to T3 and to itself
    setP (*proc, 3, MAT_MODE, 0); setP (*proc, 3, MAT_INPUT, 4); setP (*proc, 3, MAT_MONITOR, 1); setP (*proc, 3, SP_MIX, 0.6f); setP (*proc, 3, SP_DFB, 0.9f);
    setP (*proc, 3, GR_MIX, 0.5f);
    s = render (*proc, sec * 2, bs, chords); report ("T4 <- T3 serial routing", s);
    check (s.finite && s.rms > 0.002, "serial routing");
    setP (*proc, 3, MAT_INPUT, 5); // self-feedback
    s = render (*proc, sec * 4, bs, chords); report ("T4 self-feedback loop", s);
    check (s.finite && s.peak <= 1.01f, "self-feedback must stay bounded");

    // 6. macros, punch FX, scenes, DJ filter
    setG (*proc, GID::macroTgt (0, 0), 3); setG (*proc, GID::macroAmt (0, 0), 1.0f); setG (*proc, GID::macro (0), 0.8f);
    setG (*proc, GID::dj, -0.6f);
    s = render (*proc, sec, bs, chords); report ("macro + DJ low-pass", s); check (s.finite, "macro");
    for (int i = 0; i < 4; ++i)
    {
        setG (*proc, GID::punch (i), 1);
        s = render (*proc, sec, bs, chords); report ("punch " + punchNames()[i], s); check (s.finite, "punch fx");
        setG (*proc, GID::punch (i), 0);
    }
    setG (*proc, GID::dj, 0);
    proc->storeScene (5);
    setP (*proc, 0, GR_SIZE, 0.9f);
    proc->recallScene (5);
    check (std::abs (proc->trackParam (0, GR_SIZE)->convertFrom0to1 (proc->trackParam (0, GR_SIZE)->getValue()) - 0.12f) < 0.01f, "scene recall restores value");

    // 7. state round trip
    MemoryBlock state; proc->getStateInformation (state);
    auto proc2 = std::make_unique<QuarryProcessor>();
    proc2->setPlayConfigDetails (2, 2, sr, bs); proc2->prepareToPlay (sr, bs);
    proc2->setStateInformation (state.getData(), (int) state.getSize());
    check (proc2->sceneExists (5), "scenes saved in state");
    check (proc2->getSample (0) != nullptr, "sample path restored");
    check (std::abs (proc2->trackParam (0, CO_BITS)->getValue() - proc->trackParam (0, CO_BITS)->getValue()) < 1e-5f, "params restored");
    std::cout << "state size: " << state.getSize() << " bytes" << std::endl;

    // 8. CPU: everything on, all four tracks busy
    for (int t = 0; t < 4; ++t)
    {
        setP (*proc2, t, MX_MUTE, 0); setP (*proc2, t, GR_MIX, 0.7f); setP (*proc2, t, GR_DENSITY, 120); setP (*proc2, t, GR_SIZE, 0.5f);
        setP (*proc2, t, FL_MORPH, 0.6f); setP (*proc2, t, FL_RBANDS, 48); setP (*proc2, t, FL_RPITCH, 30);
        setP (*proc2, t, CO_DRIVE, 0.5f); setP (*proc2, t, CO_COMP, 0.5f); setP (*proc2, t, SP_MIX, 0.5f); setP (*proc2, t, SP_SHIMMER, 0.4f);
        setP (*proc2, t, MAT_MODE, 1);
        if (t > 0) { proc2->copyTrack (0); proc2->pasteTrack (t); }
    }
    for (int t = 0; t < 4; ++t) { setP (*proc2, t, GR_MIX, 0.7f); setP (*proc2, t, GR_DENSITY, 120); setP (*proc2, t, GR_SIZE, 0.5f); }
    render (*proc2, 20, bs, nullptr);
    const int cpuBlocks = sec * 10;
    const auto t0 = std::chrono::high_resolution_clock::now();
    s = render (*proc2, cpuBlocks, bs, nullptr);
    const double el = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
    report ("worst-case 4 tracks (10 s)", s);
    std::cout << "CPU: " << String (el / 10.0 * 100.0, 1) << "% of one core (realtime x" << String (10.0 / el, 1) << ")" << std::endl;
    check (s.finite, "worst case finite");

    // 9. Editor screenshots of every view
    {
        proc->prepareToPlay (sr, bs);
        proc->loadSample (0, sample);
        setP (*proc, 0, MX_MUTE, 0); setP (*proc, 0, GR_MIX, 0.6f); setP (*proc, 0, MAT_START, 0.1f); setP (*proc, 0, MAT_LENGTH, 0.7f);
        MessageManager::getInstance()->runDispatchLoopUntil (50);
        render (*proc, sec, bs, chords);
        std::unique_ptr<AudioProcessorEditor> ed (proc->createEditor());
        auto* qe = dynamic_cast<QuarryEditor*> (ed.get());
        ed->setSize (QuarryEditor::kW, QuarryEditor::kH);
        const std::pair<const char*, int> shots[] = { { "material", 0 }, { "granular", 1 }, { "filter", 2 }, { "color", 3 }, { "space", 4 },
                                                      { "mod", 5 }, { "mix", 6 }, { "perform", 7 }, { "scene", 8 }, { "config", 9 }, { "project", 10 }, { "track", 11 } };
        proc->storeScene (0); proc->storeScene (3);
        for (auto& [nm, v] : shots)
        {
            render (*proc, 8, bs, chords);
            if (v == 2) { setP (*proc, 0, FL_MORPH, 0.5f); }
            if (v == 5) { setP (*proc, 0, M1_DEPTH, 0.5f); setP (*proc, 0, M1_TGT, 5); }
            qe->setView ((View) v);
            MessageManager::getInstance()->runDispatchLoopUntil (100);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = outDir.getChildFile (String ("ui_") + nm + ".png");
            f.deleteFile();
            FileOutputStream os (f); PNGImageFormat png; png.writeImageToStream (img, os);
        }
    }

    std::cout << (failures == 0 ? "ALL CHECKS PASSED" : String (failures) + " CHECK(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
