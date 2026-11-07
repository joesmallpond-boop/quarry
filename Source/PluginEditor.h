#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace quarry
{
enum class View { Material, Granular, Filter, Color, Space, Mod, Mix, Perform, Scene, Config, Project, TrackSelect };

struct Page
{
    juce::String name;
    std::array<juce::RangedAudioParameter*, 8> p {};
    std::array<juce::String, 8> label {};
};

class QuarryEditor;

// ------------------------------------------------------------------ hardware-style key
class Key : public juce::Component
{
public:
    enum Icon { None, Wave, Dots, Curve, Rings, Cube, Play, Triangle, Square, Hexagon, Circle };
    Key (juce::String text, Icon icon = None) : label (std::move (text)), icon (icon) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); if (onPress) onPress(); }
    void mouseUp (const juce::MouseEvent&) override   { down = false; repaint(); if (onRelease) onRelease(); }
    void setLit (bool l, juce::Colour c = juce::Colours::white) { if (l != lit || c != litColour) { lit = l; litColour = c; repaint(); } }
    std::function<void()> onPress, onRelease;
    juce::String label; Icon icon;
private:
    bool down = false, lit = false; juce::Colour litColour = juce::Colours::white;
};

// ------------------------------------------------------------------ endless-pot style encoder
class Encoder : public juce::Slider
{
public:
    Encoder() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox)
    {
        setMouseDragSensitivity (220);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    }
    void paint (juce::Graphics&) override;
    juce::Colour accent = juce::Colours::white;
    bool bound = false;
};

// ------------------------------------------------------------------ the colour LCD
class Screen : public juce::Component
{
public:
    explicit Screen (QuarryEditor& e) : ed (e) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    juce::Rectangle<float> vizArea() const;
    juce::Rectangle<float> cellArea (int i) const;
    std::array<std::array<float, 120>, 4> modHist {};
    int modHistPos = 0;
private:
    QuarryEditor& ed;
    int dragCell = -1; float dragStartNorm = 0; int dragStartY = 0;
    void drawHeader (juce::Graphics&, juce::Rectangle<float>);
    void drawCells (juce::Graphics&);
    void drawViz (juce::Graphics&, juce::Rectangle<float>);
    void drawWave (juce::Graphics&, juce::Rectangle<float>, const float* peaks, int n, int validBins = -1);
    void drawMaterial (juce::Graphics&, juce::Rectangle<float>);
    void drawGranular (juce::Graphics&, juce::Rectangle<float>);
    void drawFilter (juce::Graphics&, juce::Rectangle<float>);
    void drawColor (juce::Graphics&, juce::Rectangle<float>);
    void drawSpace (juce::Graphics&, juce::Rectangle<float>);
    void drawMod (juce::Graphics&, juce::Rectangle<float>);
    void drawMix (juce::Graphics&, juce::Rectangle<float>);
    void drawPerform (juce::Graphics&, juce::Rectangle<float>);
    void drawScene (juce::Graphics&, juce::Rectangle<float>);
    void drawConfig (juce::Graphics&, juce::Rectangle<float>);
    void drawProject (juce::Graphics&, juce::Rectangle<float>);
    void drawTrackSelect (juce::Graphics&, juce::Rectangle<float>);
    int sceneSlotAt (juce::Point<float>) const;
};

// ------------------------------------------------------------------ the whole panel
class Panel : public juce::Component
{
public:
    explicit Panel (QuarryEditor& e) : ed (e) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    QuarryEditor& ed;
};

class QuarryEditor : public juce::AudioProcessorEditor, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    static constexpr int kW = 1210, kH = 780;
    explicit QuarryEditor (QuarryProcessor&);
    ~QuarryEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff111113)); }
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;
    bool keyPressed (const juce::KeyPress&) override;

    // state used by Screen
    QuarryProcessor& proc;
    View view = View::Material, prevView = View::Material;
    int track = 0, page = 0, modSlot = 0, sceneBank = 0;
    bool ctrl = false, temp = false, pasteMode = false, sceneStore = false, sceneDelete = false;
    std::vector<Page> pages;
    juce::String flash; double flashUntil = 0;
    int lastTouchedCell = -1; double lastTouchTime = 0;
    std::array<int, 4> punchHeld {};

    static juce::Colour trackColour (int t);
    juce::Colour accent() const { return trackColour (track); }
    juce::String viewName() const;
    juce::RangedAudioParameter* tp (int tpIndex) { return proc.trackParam (track, tpIndex); }
    juce::RangedAudioParameter* gp (const juce::String& id) { return proc.apvts.getParameter (id); }
    std::vector<juce::String> softKeyLabels() const;
    void showFlash (const juce::String& s) { flash = s; flashUntil = juce::Time::getMillisecondCounterHiRes() + 1400; }
    void touchCell (int i) { lastTouchedCell = i; lastTouchTime = juce::Time::getMillisecondCounterHiRes(); }
    void sceneClicked (int slot);
    void openLoadDialog();

private:
    Panel panel { *this };
    Screen screen { *this };
    Encoder master;
    std::unique_ptr<juce::SliderParameterAttachment> masterAttach;
    std::array<Encoder, 8> enc;
    std::array<std::unique_ptr<juce::SliderParameterAttachment>, 8> encAttach;
    Key project { "project" }, config { "config" };
    Key modKey { "mod" }, s1 { "", Key::Triangle }, s2 { "", Key::Square }, s3 { "", Key::Hexagon }, s4 { "", Key::Circle };
    Key dMat { "", Key::Wave }, dGr { "", Key::Dots }, dFl { "", Key::Curve }, dCo { "", Key::Rings }, dSp { "", Key::Cube };
    Key play { "", Key::Play }, copy { "copy" }, clear { "clear" }, ctrlKey { "ctrl" };
    Key trackKey { "track" }, scene { "scene" }, mix { "mix" }, perform { "perform" }, tempKey { "temp" };
    std::array<Key*, 4> soft { &s1, &s2, &s3, &s4 };
    std::unique_ptr<juce::FileChooser> chooser;
    std::array<bool, 4> encoderWasDragging {};

    void timerCallback() override;
    void rebuildPages();
    void bindEncoders();
    void softPressed (int i);
    void softReleased (int i);
    void deviceKey (View v, const juce::String& prefix);
    void updateLights();
    void consumeCtrl() { ctrl = false; }
public:
    void setView (View v);
    void selectTrack (int t);
    std::array<juce::Rectangle<int>, 4> trackLedArea;
};
} // namespace quarry
