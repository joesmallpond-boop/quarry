#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace quarry
{
using namespace juce;

AudioProcessorValueTreeState::ParameterLayout QuarryProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto& specs = trackSpecs();
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto group = std::make_unique<AudioProcessorParameterGroup> ("track" + String (t + 1), "Track " + String (t + 1), " | ");
        for (int i = 0; i < TP_COUNT; ++i)
            group->addChild (makeParam (trackParamID (t, i), "T" + String (t + 1) + " " + specs[(size_t) i].name, specs[(size_t) i]));
        layout.add (std::move (group));
    }

    auto master = std::make_unique<AudioProcessorParameterGroup> ("master", "Master", " | ");
    auto pct = [] (float v, int) { return String (roundToInt (v * 100)) + "%"; };
    master->addChild (std::make_unique<AudioParameterFloat> (ParameterID { GID::comp, 1 }, "Master Glue", NormalisableRange<float> (0, 1), 0.2f,
                                                              AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    master->addChild (std::make_unique<AudioParameterFloat> (ParameterID { GID::dj, 1 }, "DJ Filter", NormalisableRange<float> (-1, 1), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
            return std::abs (v) < 0.02f ? String ("OFF") : (v < 0 ? "LP " : "HP ") + String (roundToInt (std::abs (v) * 100)) + "%"; })));
    master->addChild (std::make_unique<AudioParameterFloat> (ParameterID { GID::out, 1 }, "Master Out", NormalisableRange<float> (-60, 6, 0, 2.5f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= -59.5f ? String ("-inf dB") : String (v, 1) + " dB"; })));
    master->addChild (std::make_unique<AudioParameterFloat> (ParameterID { GID::glide, 1 }, "Scene Glide", NormalisableRange<float> (0, 8, 0, 0.5f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.01f ? String ("Instant") : String (v, 2) + " s"; })));
    master->addChild (std::make_unique<AudioParameterBool> (ParameterID { GID::play, 1 }, "Play", true));
    for (int i = 0; i < 4; ++i)
        master->addChild (std::make_unique<AudioParameterBool> (ParameterID { GID::punch (i), 1 }, "Punch " + punchNames()[i], false));
    layout.add (std::move (master));

    auto perf = std::make_unique<AudioProcessorParameterGroup> ("perform", "Perform", " | ");
    const auto& names = macroTargetNames();
    auto bip = [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v * 100)) + "%"; };
    for (int m = 0; m < kNumMacros; ++m)
    {
        perf->addChild (std::make_unique<AudioParameterFloat> (ParameterID { GID::macro (m), 1 }, "Macro " + String (m + 1), NormalisableRange<float> (0, 1), 0.0f,
                                                                AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        for (int s = 0; s < 2; ++s)
        {
            perf->addChild (std::make_unique<AudioParameterChoice> (ParameterID { GID::macroTgt (m, s), 1 },
                            "Macro " + String (m + 1) + (s == 0 ? " Target A" : " Target B"), names, 0));
            perf->addChild (std::make_unique<AudioParameterFloat> (ParameterID { GID::macroAmt (m, s), 1 },
                            "Macro " + String (m + 1) + (s == 0 ? " Amount A" : " Amount B"), NormalisableRange<float> (-1, 1), 0.5f,
                            AudioParameterFloatAttributes().withStringFromValueFunction (bip)));
        }
    }
    layout.add (std::move (perf));
    return layout;
}

QuarryProcessor::QuarryProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "QUARRY", createLayout())
{
    formats.registerBasicFormats();
    for (int t = 0; t < kNumTracks; ++t) tracks[(size_t) t].bind (apvts, t);

    for (int m = 0; m < kNumMacros; ++m)
    {
        macroP[(size_t) m] = apvts.getParameter (GID::macro (m));
        tgtA[(size_t) m] = apvts.getParameter (GID::macroTgt (m, 0)); tgtB[(size_t) m] = apvts.getParameter (GID::macroTgt (m, 1));
        amtA[(size_t) m] = apvts.getParameter (GID::macroAmt (m, 0)); amtB[(size_t) m] = apvts.getParameter (GID::macroAmt (m, 1));
    }
    for (int i = 0; i < 4; ++i) punchP[(size_t) i] = apvts.getParameter (GID::punch (i));
    compP = apvts.getParameter (GID::comp); djP = apvts.getParameter (GID::dj); outP = apvts.getParameter (GID::out);
    glideP = apvts.getParameter (GID::glide); playP = apvts.getParameter (GID::play);

    // Scenes capture every track parameter plus the macro values/assignments
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (id.startsWith ("pf_") || id == GID::glide || id == GID::play) continue;
            if (id.contains ("_mat_rec")) continue;
            sceneParams.push_back (rp);
        }

    startTimerHz (60);
}

QuarryProcessor::~QuarryProcessor() { stopTimer(); }

bool QuarryProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    return in.isDisabled() || in == AudioChannelSet::stereo() || in == AudioChannelSet::mono();
}

void QuarryProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; maxBlock = jmax (32, samplesPerBlock);
    for (auto& t : tracks) t.prepare (sampleRate, maxBlock);
    for (auto& b : prevOut) { b.setSize (2, maxBlock + 64); b.clear(); }
    extIn.setSize (2, maxBlock + 64);
    histLen = (int) (sampleRate * 4.0);
    history.setSize (2, histLen); history.clear(); histW = 0;
    for (auto& f : djSvf) f.reset();
    for (auto& f : sweepSvf) f.reset();
    compEnv = 0; stutterOn = tapeStopOn = false; tsRate = 1; sweepPos = 0;
}

void QuarryProcessor::updateMacros()
{
    const auto& tgts = modTargetIndices();
    const int perTrack = (int) tgts.size();
    for (auto& t : tracks) t.macroOffset.fill (0.0f);
    for (int m = 0; m < kNumMacros; ++m)
    {
        const float mv = macroP[(size_t) m]->getValue();
        if (mv <= 0.0f) continue;
        for (int s = 0; s < 2; ++s)
        {
            auto* tp = s == 0 ? tgtA[(size_t) m] : tgtB[(size_t) m];
            auto* ap = s == 0 ? amtA[(size_t) m] : amtB[(size_t) m];
            const int idx = roundToInt (tp->convertFrom0to1 (tp->getValue())) - 1;
            if (idx < 0) continue;
            const int track = idx / perTrack, which = idx % perTrack;
            if (track >= kNumTracks) continue;
            tracks[(size_t) track].macroOffset[(size_t) tgts[(size_t) which]] += mv * ap->convertFrom0to1 (ap->getValue());
        }
    }
}

void QuarryProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples > maxBlock) prepareToPlay (sr, numSamples);   // defensive: host exceeded block size

    // tempo / transport
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    bpmAtomic.store ((float) bpm);
    const bool playing = playP->getValue() > 0.5f;

    // sample handoff (message thread -> audio thread)
    {
        const SpinLock::ScopedTryLockType lock (sampleLock);
        if (lock.isLocked())
            for (int t = 0; t < kNumTracks; ++t)
                if (pendingFlag[(size_t) t])
                {
                    tracks[(size_t) t].material.sample = pendingSample[(size_t) t];
                    tracks[(size_t) t].material.pending = nullptr;
                    pendingFlag[(size_t) t] = false;
                }
    }
    for (int t = 0; t < kNumTracks; ++t)
        if (clearTapeFlag[(size_t) t].exchange (false)) tracks[(size_t) t].material.clearTape();

    // capture external input before we overwrite the buffer
    const int inCh = getTotalNumInputChannels();
    const bool haveIn = inCh > 0;
    if (haveIn)
    {
        extIn.copyFrom (0, 0, buffer, 0, 0, numSamples);
        extIn.copyFrom (1, 0, buffer, jmin (1, inCh - 1), 0, numSamples);
    }
    else extIn.clear();

    updateMacros();
    const bool punchFreeze = punchP[3]->getValue() > 0.5f;

    // ---- per-track processing in 32-sample sub-blocks ----
    auto midiIt = midi.cbegin();
    constexpr int kSub = 32;
    for (int off = 0; off < numSamples; off += kSub)
    {
        const int n = jmin (kSub, numSamples - off);
        for (; midiIt != midi.cend(); ++midiIt)
        {
            const auto meta = *midiIt;
            if (meta.samplePosition >= off + n) break;
            const auto msg = meta.getMessage();
            if (msg.isProgramChange()) { pendingProgram.store (msg.getProgramChangeNumber()); continue; }
            for (auto& t : tracks) t.handleMidi (msg);
        }
        for (int t = 0; t < kNumTracks; ++t)
        {
            auto& tr = tracks[(size_t) t];
            const int src = (int) tr.v (MAT_INPUT);   // 0 off, 1 ext, 2..5 track 1..4
            const float* il = nullptr; const float* ir = nullptr;
            if (src == 1 && haveIn) { il = extIn.getReadPointer (0, off); ir = extIn.getReadPointer (1, off); }
            else if (src >= 2)      { il = prevOut[(size_t) src - 2].getReadPointer (0, off); ir = prevOut[(size_t) src - 2].getReadPointer (1, off); }
            tr.process (il, ir, off, n, bpm, playing, punchFreeze);
        }
    }

    // ---- mixer ----
    buffer.clear();
    bool anySolo = false;
    for (auto& t : tracks) anySolo |= t.v (MX_SOLO) > 0.5f;
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& tr = tracks[(size_t) t];
        const float lvl = dsp::dbToGain (tr.v (MX_LEVEL));
        const float pan = tr.v (MX_PAN);
        const bool audible = tr.v (MX_MUTE) < 0.5f && (! anySolo || tr.v (MX_SOLO) > 0.5f) && tr.v (MX_MASTER) > 0.5f;
        const float gl = lvl * std::sqrt (0.5f * (1.0f - pan)) * 1.4142f, gr = lvl * std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;
        float pk = 0;
        for (int c = 0; c < 2; ++c)
        {
            const float* s = tr.out.getReadPointer (c);
            const float g = c == 0 ? gl : gr;
            for (int i = 0; i < numSamples; ++i) pk = jmax (pk, std::abs (s[i] * g));
            if (audible) buffer.addFrom (c, 0, s, numSamples, g);
        }
        trackPeak[(size_t) t].store (jmax (pk, trackPeak[(size_t) t].load() * 0.9f));
        // keep for inter-track routing next block (post-fader so levels shape feedback)
        for (int c = 0; c < 2; ++c) prevOut[(size_t) t].copyFrom (c, 0, tr.out, c, 0, numSamples);
        if (lvl != 1.0f) prevOut[(size_t) t].applyGain (0, numSamples, lvl);
    }

    processMaster (buffer, numSamples);
}

void QuarryProcessor::processMaster (AudioBuffer<float>& buf, int n)
{
    float* L = buf.getWritePointer (0); float* R = buf.getWritePointer (1);
    const float glue = compP->getValue();
    const float dj = djP->convertFrom0to1 (djP->getValue());
    const float outG = dsp::dbToGain (outP->convertFrom0to1 (outP->getValue()));
    const float fs = (float) sr;

    // glue compressor (stereo-linked, gentle)
    if (glue > 0.001f)
    {
        const float thr = dsp::dbToGain (-4.0f - glue * 22.0f), ratio = 2.0f + glue * 4.0f;
        const float atk = std::exp (-1.0f / (0.01f * fs)), rel = std::exp (-1.0f / (0.25f * fs));
        const float mk = dsp::dbToGain (glue * 8.0f);
        for (int i = 0; i < n; ++i)
        {
            const float lvl = jmax (std::abs (L[i]), std::abs (R[i]));
            compEnv = lvl > compEnv ? atk * compEnv + (1 - atk) * lvl : rel * compEnv + (1 - rel) * lvl;
            const float g = (compEnv > thr ? std::pow (compEnv / thr, 1.0f / ratio - 1.0f) : 1.0f) * mk;
            L[i] *= g; R[i] *= g;
        }
    }

    // DJ filter: one knob, left = low-pass, right = high-pass
    if (std::abs (dj) > 0.02f)
    {
        const float a = (std::abs (dj) - 0.02f) / 0.98f;
        const float fc = dj < 0 ? 20000.0f * std::pow (60.0f / 20000.0f, a) : 20.0f * std::pow (9000.0f / 20.0f, a);
        for (auto& f : djSvf) f.set (fc, 0.9f + a * 1.6f, fs);
        for (int c = 0; c < 2; ++c)
        {
            float* x = c == 0 ? L : R;
            for (int i = 0; i < n; ++i) { float lp, bp, hp; djSvf[c].tick (x[i], lp, bp, hp); x[i] = dj < 0 ? lp : hp; }
        }
    }

    // ---- punch-in FX (use a 4 s history ring) ----
    const bool stut = punchP[0]->getValue() > 0.5f, tstop = punchP[1]->getValue() > 0.5f, sweep = punchP[2]->getValue() > 0.5f;
    float* hl = history.getWritePointer (0); float* hr = history.getWritePointer (1);
    const double bpm = bpmAtomic.load();
    if (stut && ! stutterOn) { stutterOn = true; stutterLen = jlimit (64, histLen / 2, (int) (sr * 60.0 / bpm / 4.0)); stutterStart = (histW - stutterLen + histLen) % histLen; stutterPos = 0; }
    if (! stut) stutterOn = false;
    if (tstop && ! tapeStopOn) { tapeStopOn = true; tsRead = histW; tsRate = 1.0f; }
    if (! tstop) tapeStopOn = false;

    sweepPos = sweep ? jmin (1.0f, sweepPos + (float) n / (fs * 2.0f)) : jmax (0.0f, sweepPos - (float) n / (fs * 0.15f));
    sweepAmount.store (sweepPos);
    if (sweepPos > 0.001f) for (auto& f : sweepSvf) f.set (40.0f * std::pow (150.0f, sweepPos), 3.0f + sweepPos * 4.0f, fs);

    for (int i = 0; i < n; ++i)
    {
        hl[histW] = L[i]; hr[histW] = R[i];
        float l = L[i], r = R[i];
        if (stutterOn)
        {
            const int p = (stutterStart + stutterPos) % histLen;
            const int fade = jmin (128, stutterLen / 4);
            float g = 1.0f;
            if (stutterPos < fade) g = (float) stutterPos / fade; else if (stutterPos > stutterLen - fade) g = (float) (stutterLen - stutterPos) / fade;
            l = hl[p] * g; r = hr[p] * g;
            if (++stutterPos >= stutterLen) stutterPos = 0;
        }
        else if (tapeStopOn)
        {
            tsRate = jmax (0.0f, tsRate - 1.0f / (0.7f * fs));
            tsRead += tsRate;
            l = dsp::linearRead (hl, histLen, tsRead) * jmin (1.0f, tsRate * 4.0f);
            r = dsp::linearRead (hr, histLen, tsRead) * jmin (1.0f, tsRate * 4.0f);
        }
        if (++histW >= histLen) histW = 0;
        if (sweepPos > 0.001f)
        {
            float lp, bp, hp;
            sweepSvf[0].tick (l, lp, bp, hp); const float hl2 = hp;
            sweepSvf[1].tick (r, lp, bp, hp); const float hr2 = hp;
            l = l * (1 - sweepPos) + std::tanh (hl2 * 1.5f) * sweepPos;
            r = r * (1 - sweepPos) + std::tanh (hr2 * 1.5f) * sweepPos;
        }
        l *= outG; r *= outG;
        // soft safety clip
        L[i] = std::abs (l) > 0.95f ? std::tanh (l) : l;
        R[i] = std::abs (r) > 0.95f ? std::tanh (r) : r;
    }
    float pk = 0;
    for (int i = 0; i < n; ++i) pk = jmax (pk, std::abs (L[i]), std::abs (R[i]));
    masterPeak.store (jmax (pk, masterPeak.load() * 0.9f));
}

// =====================================================================================
//  Message-thread actions
// =====================================================================================
bool QuarryProcessor::loadSample (int track, const File& file)
{
    std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return false;
    const int64 maxLen = (int64) (reader->sampleRate * 600.0);          // 10 minutes max
    const int len = (int) jmin (reader->lengthInSamples, maxLen);
    if (len < 16) return false;

    dsp::SampleData::Ptr s = new dsp::SampleData();
    s->buffer.setSize (2, len);
    reader->read (&s->buffer, 0, len, 0, true, reader->numChannels > 1);
    if (reader->numChannels == 1) s->buffer.copyFrom (1, 0, s->buffer, 0, 0, len);
    s->sourceRate = reader->sampleRate; s->name = file.getFileNameWithoutExtension(); s->path = file.getFullPathName();
    s->buildPeaks();

    if (loaded[(size_t) track] != nullptr) graveyard.add (loaded[(size_t) track].get());
    loaded[(size_t) track] = s;
    {
        const SpinLock::ScopedLockType lock (sampleLock);
        pendingSample[(size_t) track] = s; pendingFlag[(size_t) track] = true;
    }
    // If the track was in Tape mode, switch it to Disc so the new sample is heard
    auto* mode = trackParam (track, MAT_MODE);
    if (roundToInt (mode->convertFrom0to1 (mode->getValue())) == 0) mode->setValueNotifyingHost (mode->convertTo0to1 (1.0f));
    return true;
}

void QuarryProcessor::clearTrack (int track)
{
    clearTapeFlag[(size_t) track].store (true);
    if (loaded[(size_t) track] != nullptr) graveyard.add (loaded[(size_t) track].get());
    loaded[(size_t) track] = nullptr;
    const SpinLock::ScopedLockType lock (sampleLock);
    pendingSample[(size_t) track] = nullptr; pendingFlag[(size_t) track] = true;
}

void QuarryProcessor::resetDevice (int track, const String& prefix)
{
    for (int i = 0; i < TP_COUNT; ++i)
        if (trackSpecs()[(size_t) i].id.startsWith (prefix))
        {
            auto* p = trackParam (track, i);
            p->setValueNotifyingHost (p->getDefaultValue());
        }
}

void QuarryProcessor::resetTrack (int track) { resetDevice (track, ""); }

void QuarryProcessor::copyTrack (int from)
{
    for (int i = 0; i < TP_COUNT; ++i) clipboard[(size_t) i] = trackParam (from, i)->getValue();
    clipSample = loaded[(size_t) from];
    clipboardValid = true;
}

void QuarryProcessor::pasteTrack (int to)
{
    if (! clipboardValid) return;
    for (int i = 0; i < TP_COUNT; ++i)
        if (i != MAT_REC) trackParam (to, i)->setValueNotifyingHost (clipboard[(size_t) i]);
    if (clipSample != nullptr && clipSample != loaded[(size_t) to])
    {
        if (loaded[(size_t) to] != nullptr) graveyard.add (loaded[(size_t) to].get());
        loaded[(size_t) to] = clipSample;
        const SpinLock::ScopedLockType lock (sampleLock);
        pendingSample[(size_t) to] = clipSample; pendingFlag[(size_t) to] = true;
    }
}

void QuarryProcessor::storeScene (int slot)
{
    ValueTree s ("S");
    s.setProperty ("slot", slot, nullptr);
    for (auto* p : sceneParams) s.setProperty (Identifier (p->getParameterID()), p->getValue(), nullptr);
    if (auto old = scenes.getChildWithProperty ("slot", slot); old.isValid()) scenes.removeChild (old, nullptr);
    scenes.appendChild (s, nullptr);
    lastScene.store (slot);
}

bool QuarryProcessor::sceneExists (int slot) const { return scenes.getChildWithProperty ("slot", slot).isValid(); }

void QuarryProcessor::clearScene (int slot)
{
    if (auto old = scenes.getChildWithProperty ("slot", slot); old.isValid()) scenes.removeChild (old, nullptr);
}

void QuarryProcessor::recallScene (int slot)
{
    auto s = scenes.getChildWithProperty ("slot", slot);
    if (! s.isValid()) return;
    glideItems.clear();
    const float glide = glideP->convertFrom0to1 (glideP->getValue());
    for (auto* p : sceneParams)
    {
        const Identifier id (p->getParameterID());
        if (! s.hasProperty (id)) continue;
        const float to = (float) s.getProperty (id);
        const bool stepped = dynamic_cast<AudioParameterFloat*> (p) == nullptr || p->getNumSteps() < 200;
        if (glide < 0.01f || stepped) { if (p->getValue() != to) p->setValueNotifyingHost (to); }
        else glideItems.push_back ({ p, p->getValue(), to });
    }
    glideStart = Time::getMillisecondCounterHiRes(); glideDur = glide * 1000.0;
    lastScene.store (slot);
}

void QuarryProcessor::timerCallback()
{
    const int prog = pendingProgram.exchange (-1);
    if (prog >= 0) recallScene (prog);

    if (! glideItems.empty())
    {
        const double t = jlimit (0.0, 1.0, (Time::getMillisecondCounterHiRes() - glideStart) / jmax (1.0, glideDur));
        const float e = (float) (t * t * (3 - 2 * t));
        for (auto& g : glideItems) g.p->setValueNotifyingHost (g.from + (g.to - g.from) * e);
        if (t >= 1.0) glideItems.clear();
    }

    // free samples that the audio thread no longer references
    for (int i = graveyard.size(); --i >= 0;)
        if (graveyard.getObjectPointerUnchecked (i)->getReferenceCount() == 1) graveyard.remove (i);
}

// =====================================================================================
//  State
// =====================================================================================
void QuarryProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    ValueTree extra ("EXTRA");
    for (int t = 0; t < kNumTracks; ++t)
        extra.setProperty ("sample" + String (t), loaded[(size_t) t] ? loaded[(size_t) t]->path : String(), nullptr);
    extra.appendChild (scenes.createCopy(), nullptr);
    state.removeChild (state.getChildWithName ("EXTRA"), nullptr);
    state.appendChild (extra, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void QuarryProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    auto extra = state.getChildWithName ("EXTRA");
    state.removeChild (extra, nullptr);
    apvts.replaceState (state);
    if (extra.isValid())
    {
        scenes = extra.getChildWithName ("SCENES").createCopy();
        if (! scenes.isValid()) scenes = ValueTree ("SCENES");
        for (int t = 0; t < kNumTracks; ++t)
        {
            const String path = extra.getProperty ("sample" + String (t)).toString();
            if (path.isNotEmpty() && File (path).existsAsFile())
            {
                auto* mode = trackParam (t, MAT_MODE);
                const float keep = mode->getValue();
                loadSample (t, File (path));
                mode->setValueNotifyingHost (keep);   // restore the saved mode
            }
        }
    }
    for (int t = 0; t < kNumTracks; ++t) trackParam (t, MAT_REC)->setValueNotifyingHost (0.0f);
}
} // namespace quarry

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new quarry::QuarryProcessor(); }

juce::AudioProcessorEditor* quarry::QuarryProcessor::createEditor() { return new QuarryEditor (*this); }
