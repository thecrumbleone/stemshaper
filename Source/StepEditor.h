#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace ss
{
juce::Colour moduleColour (int module);

namespace Colours
{
    const juce::Colour bg       { 0xff15171c };
    const juce::Colour panel    { 0xff1e2128 };
    const juce::Colour panelHi  { 0xff272b34 };
    const juce::Colour line     { 0xff343945 };
    const juce::Colour text     { 0xffd8dce6 };
    const juce::Colour textDim  { 0xff8a91a0 };
}

//==============================================================================
class ShaperLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ShaperLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getLabelFont (juce::Label&) override;
};

//==============================================================================
// The 32-step sequencer grid. Each cell shows its envelope scaled by its level.
class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (StepShaperProcessor&);

    void setModule (int m);
    int getSelectedStep() const { return selected; }
    void setSelectedStep (int s);
    std::function<void()> onSelectionChanged;
    std::function<int()> getBrush;   // shape stamped when drawing with Cmd/Ctrl held (-1 = none)

    void refresh();   // called on a timer

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    StepShaperProcessor& proc;
    int module = Vol, selected = 0;
    int lastStep = -1; float lastValue = 0.0f;
    int lastEdit = -1; float lastPlay = -2.0f; int lastLen = -1;

    int length() const;
    juce::Rectangle<float> gridArea() const;
    int stepAt (float x) const;
    float valueAt (float y) const;
    void paintStep (int s, const juce::MouseEvent&);
    void showMenu (int step);
};

//==============================================================================
// Zoomed editor for the selected step's envelope: draw any curve.
class ShapeEditor : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ShapeEditor (StepShaperProcessor&);
    void setTarget (int module, int step);
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    StepShaperProcessor& proc;
    int module = Vol, step = 0, lastEdit = -1;
    int lastPoint = -1; float lastVal = 0.0f;
    juce::Rectangle<float> area() const;
    void drawAt (juce::Point<float>, bool interpolate);
};

//==============================================================================
// Clickable icons for the shape library.
class ShapeLibrary : public juce::Component, public juce::SettableTooltipClient
{
public:
    ShapeLibrary();
    std::function<void (int shape, bool applyToAll)> onShapeChosen;
    int getBrush() const { return brush; }
    void setAccent (juce::Colour c) { accent = c; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int brush = Flat, hover = -1;
    juce::Colour accent { 0xffffa040 };
    juce::Rectangle<float> cell (int i) const;
    int cellAt (juce::Point<float>) const;
};

//==============================================================================
// Left-hand module selector with power buttons and live activity meters.
class ModuleList : public juce::Component
{
public:
    explicit ModuleList (StepShaperProcessor&);
    std::function<void (int)> onSelect;
    void setSelected (int m) { selected = m; repaint(); }
    void refresh() { repaint(); }
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    StepShaperProcessor& proc;
    int selected = Vol;
    std::array<juce::ToggleButton, kNumModules> power;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, kNumModules> attach;
    juce::Rectangle<int> row (int m) const;
};

} // namespace ss
