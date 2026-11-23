#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "StepEditor.h"

//==============================================================================
struct Knob : public juce::Component
{
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& name, juce::Colour);
    void resized() override;

    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

struct Choice : public juce::Component
{
    Choice (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& name);
    void resized() override;

    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

//==============================================================================
class StepShaperEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StepShaperEditor (StepShaperProcessor&);
    ~StepShaperEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void selectModule (int m);   // public for tests

private:
    void timerCallback() override;
    void rebuildPresetList();
    void stepPreset (int delta);
    void savePresetDialog();
    void saveShape();
    void loadShape();
    void saveModule();
    void loadModule();

    StepShaperProcessor& proc;
    ss::ShaperLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    int module = ss::Vol;
    int lastPresetCounter = -1;

    // Top bar
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" }, randomAllButton { "Randomise" };
    std::vector<juce::File> userPresetFiles;

    // Main area
    ss::ModuleList moduleList;
    ss::StepGrid grid;
    ss::ShapeEditor shapeEditor;
    ss::ShapeLibrary shapeLibrary;
    juce::TextButton copyStep { "Copy Step" }, pasteStep { "Paste" }, pasteAll { "Paste All" },
                     saveShapeButton { "Save Shape" }, loadShapeButton { "Load Shape" };

    // Lane controls (one set per module, only the selected one is visible)
    struct LaneUI
    {
        std::unique_ptr<Choice> rate;
        std::unique_ptr<Knob> length, depth, smooth;
        std::vector<std::unique_ptr<juce::Component>> settings;   // module-specific controls
    };
    std::array<LaneUI, ss::kNumModules> laneUI;

    juce::TextButton randButton { "Random" }, clearButton { "Clear" }, invertButton { "Invert" },
                     shiftLeft { "<<" }, shiftRight { ">>" }, saveModuleButton { "Save Module" },
                     loadModuleButton { "Load Module" };

    // Global
    std::unique_ptr<Knob> swingKnob, mixKnob, outKnob;

    // Clipboard for one step
    float clipLevel = 1.0f;
    std::array<float, ss::kCurvePoints> clipCurve {};
    int clipShape = ss::Flat;
    bool hasClip = false;

    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> lanePanel, settingsPanel, globalPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepShaperEditor)
};
