#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Shapes.h"
#include "Presets.h"

//==============================================================================
// Simple fractional delay line (linear interpolation).
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, write = 0;

    void prepare (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        write = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) noexcept { buf[(size_t) write] = x; write = (write + 1) & mask; }
    float read (float delaySamples) const noexcept
    {
        const float rp = (float) write - 1.0f - delaySamples;
        const int i0 = (int) std::floor (rp);
        const float f = rp - (float) i0;
        const float a = buf[(size_t) (i0 & mask)];
        const float b = buf[(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * f;
    }
};

//==============================================================================
class StepShaperProcessor : public juce::AudioProcessor
{
public:
    StepShaperProcessor();
    ~StepShaperProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "StepShaper"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Presets
    void applyPreset (const ss::Preset&);
    juce::ValueTree createStateTree();
    void restoreStateTree (const juce::ValueTree&);

    static juce::File userPresetFolder();
    static juce::File userShapeFolder();
    static juce::File userModuleFolder();
    juce::Array<juce::File> getUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File&);

    // Module mini-presets (one module's lane + settings)
    bool saveModulePreset (int module, const juce::File&);
    bool loadModulePreset (int module, const juce::File&);

    juce::String getPresetName() const { return presetName; }
    std::atomic<int> presetChangeCounter { 0 };

    //==============================================================================
    // Lane edit helpers (message thread)
    void randomiseLane (int module);
    void clearLane (int module);
    void invertLane (int module);
    void shiftLane (int module, int direction);

    juce::AudioProcessorValueTreeState apvts;
    std::array<ss::LaneSteps, ss::kNumModules> lanes;

    static juce::String pid (int module, const char* name) { return juce::String (ss::modulePrefix (module)) + name; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct LaneParams
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* length = nullptr;
        std::atomic<float>* depth = nullptr;
        std::atomic<float>* smooth = nullptr;
    };
    std::array<LaneParams, ss::kNumModules> laneParams;
    std::array<float, ss::kNumModules> laneEnv {};   // smoothed envelope per lane

    std::atomic<float>* p (const char* id) { return apvts.getRawParameterValue (id); }

    std::atomic<float> *swing, *mix, *outGain;
    std::atomic<float> *fltType, *fltMin, *fltMax, *fltRes;
    std::atomic<float> *dstType, *dstDrive, *dstMix;
    std::atomic<float> *phsMin, *phsMax, *phsFb, *phsMix;
    std::atomic<float> *flgRange, *flgFb, *flgMix;
    std::atomic<float> *pitRange, *pitMix;
    std::atomic<float> *dlyTime, *dlyFb, *dlyMix;

    double sr = 44100.0;
    double freeRunPpq = 0.0;
    bool wasPlaying = false;

    // DSP state
    static constexpr int kMaxCh = 2;
    juce::dsp::StateVariableTPTFilter<float> filter;
    int lastFilterType = -1;
    std::array<std::array<float, 6>, kMaxCh> phsState {};
    std::array<float, kMaxCh> phsLast {};
    std::array<DelayLine, kMaxCh> flgLine, pitLine, dlyLine;
    std::array<float, kMaxCh> flgLast {}, dlyLast {}, crushHold {};
    int crushCounter = 0;
    float pitDelaySm = 0.0f, flgDelaySm = 0.0f, dlyTimeSm = 0.0f;
    juce::SmoothedValue<float> mixSm, outSm;

    int currentProgram = 0;
    juce::String presetName { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepShaperProcessor)
};
