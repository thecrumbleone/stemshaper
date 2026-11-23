#pragma once

#include "Shapes.h"
#include <map>
#include <vector>

namespace ss
{
struct LaneSpec
{
    bool on = false;
    int rate = Rate::Sixteenth;
    int length = 16;
    float depth = 1.0f;
    float smooth = 0.2f;
    std::array<float, kMaxSteps> values {};
    std::array<int, kMaxSteps> shapes {};

    LaneSpec() { values.fill (1.0f); shapes.fill (Flat); }
};

struct Preset
{
    juce::String name, category;
    std::array<LaneSpec, kNumModules> lanes;
    std::map<juce::String, float> params;   // parameter id -> real (denormalised) value
};

const std::vector<Preset>& factoryPresets();

} // namespace ss
