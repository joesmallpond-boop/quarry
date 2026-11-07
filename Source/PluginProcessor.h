#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Params.h"
#include "Track.h"

namespace quarry
{
namespace GID
{
    inline const juce::String comp  = "ms_comp";
    inline const juce::String dj    = "ms_dj";
    inline const juce::String out   = "ms_out";
    inline const juce::String glide = "ms_glide";
    inline const juce::String play  = "ms_play";
    inline juce::String punch (int i)     { return "pf_" + juce::String (i + 1); }
    inline juce::String macro (int i)     { return "mc_" + juce::String (i + 1); }
    inline juce::String macroTgt (int i, int s) { return "mc_" + juce::String (i + 1) + (s == 0 ? "_ta" : "_tb"); }
    inline juce::String macroAmt (int i, int s) { return "mc_" + juce::String (i + 1) + (s == 0 ? "_aa" : "_ab"); }
}

inline const juce::StringArray& punchNames() { static juce::StringArray n { "STUTTER", "TAPE STOP", "SWEEP", "FREEZE" }; return n; }

class QuarryProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    QuarryProcessor();
    ~QuarryProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- message-thread API used by the editor ----
    bool loadSample (int track, const juce::File& file);
    juce::String sampleName (int track) const { return loaded[(size_t) track] ? loaded[(size_t) track]->name : juce::String(); }
    dsp::SampleData::Ptr getSample (int track) const { return loaded[(size_t) track]; }
    void clearTrack (int track);               // clears tape + unloads sample
    void resetTrack (int track);               // all params to default
    void resetDevice (int track, const juce::String& prefix);
    void copyTrack (int from);
    void pasteTrack (int to);
    bool hasClipboard() const { return clipboardValid; }
    void storeScene (int slot);
    void recallScene (int slot);
    void clearScene (int slot);
    bool sceneExists (int slot) const;
    int  currentScene() const { return lastScene.load(); }
    juce::RangedAudioParameter* trackParam (int t, int tp) { return tracks[(size_t) t].params[(size_t) tp]; }

    juce::AudioProcessorValueTreeState apvts;
    std::array<Track, kNumTracks> tracks;
    std::array<std::atomic<float>, kNumTracks> trackPeak {};
    std::atomic<float> masterPeak { 0 }, bpmAtomic { 120 };
    std::atomic<float> sweepAmount { 0 };
    juce::AudioFormatManager formats;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void processMaster (juce::AudioBuffer<float>& buf, int n);
    void updateMacros();

    double sr = 48000; int maxBlock = 512;
    std::array<juce::AudioBuffer<float>, kNumTracks> prevOut;
    juce::AudioBuffer<float> extIn;

    // sample handoff
    juce::SpinLock sampleLock;
    std::array<dsp::SampleData::Ptr, kNumTracks> loaded, pendingSample;
    std::array<bool, kNumTracks> pendingFlag {};
    juce::ReferenceCountedArray<dsp::SampleData> graveyard;
    std::array<std::atomic<bool>, kNumTracks> clearTapeFlag {};

    // master section
    dsp::SVF djSvf[2], sweepSvf[2];
    float compEnv = 0;
    juce::AudioBuffer<float> history; int histW = 0, histLen = 1;
    bool stutterOn = false, tapeStopOn = false; int stutterStart = 0, stutterLen = 1, stutterPos = 0;
    double tsRead = 0; float tsRate = 1; float sweepPos = 0;
    float duckIn = 1;

    // macros
    std::array<juce::RangedAudioParameter*, kNumMacros> macroP {}, tgtA {}, tgtB {}, amtA {}, amtB {};
    std::array<juce::RangedAudioParameter*, 4> punchP {};
    juce::RangedAudioParameter *compP {}, *djP {}, *outP {}, *glideP {}, *playP {};

    // scenes
    juce::ValueTree scenes { "SCENES" };
    struct GlideItem { juce::RangedAudioParameter* p; float from, to; };
    std::vector<GlideItem> glideItems; double glideStart = 0, glideDur = 0;
    std::atomic<int> pendingProgram { -1 }, lastScene { -1 };
    std::vector<juce::RangedAudioParameter*> sceneParams;

    std::array<float, TP_COUNT> clipboard {}; dsp::SampleData::Ptr clipSample; bool clipboardValid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuarryProcessor)
};
} // namespace quarry
