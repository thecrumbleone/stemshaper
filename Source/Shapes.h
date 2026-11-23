#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include <cmath>

namespace ss
{
constexpr int kMaxSteps = 32;
constexpr int kCurvePoints = 32;   // resolution of each step's drawable envelope

//==============================================================================
// FX modules. Each has its own step sequencer lane.
enum ModuleId { Vol = 0, Flt, Dst, Phs, Flg, Pit, Dly, kNumModules };

inline const char* modulePrefix (int m)
{
    static const char* p[] { "vol_", "flt_", "dst_", "phs_", "flg_", "pit_", "dly_" };
    return p[m];
}

inline const char* moduleName (int m)
{
    static const char* n[] { "Volume", "Filter", "Distortion", "Phaser", "Flanger", "Pitch", "Delay" };
    return n[m];
}

//==============================================================================
// Shape library: normalised curves (0..1) over a step, p = 0..1.
enum Shape
{
    Flat = 0, RampUp, RampDown, Pump, Duck, Triangle, HalfGate, Stutter,
    Sine, SCurve, ExpDown, Stairs, Swell, Bounce, NumShapes
};

inline const juce::StringArray& shapeNames()
{
    static const juce::StringArray names { "Flat", "Ramp Up", "Ramp Down", "Pump", "Duck", "Triangle",
                                           "Half Gate", "Stutter", "Sine", "S-Curve", "Exp Down",
                                           "Stairs", "Swell", "Bounce" };
    return names;
}

inline float libraryShape (int shape, float p) noexcept
{
    constexpr float pi = 3.14159265358979f;
    switch (shape)
    {
        case Flat:     return 1.0f;
        case RampUp:   return p;
        case RampDown: return 1.0f - p;
        case Pump:     { const float q = 1.0f - p; return 1.0f - q * q * q; }
        case Duck:     { const float x = juce::jmin (1.0f, p * 4.0f); return x * x * (3.0f - 2.0f * x); }
        case Triangle: return 1.0f - std::abs (2.0f * p - 1.0f);
        case HalfGate: return p < 0.5f ? 1.0f : 0.0f;
        case Stutter:  return std::fmod (p * 4.0f, 1.0f) < 0.5f ? 1.0f : 0.0f;
        case Sine:     return 0.5f - 0.5f * std::cos (2.0f * pi * p);
        case SCurve:   return p * p * (3.0f - 2.0f * p);
        case ExpDown:  { const float q = 1.0f - p; return q * q * q; }
        case Stairs:   return juce::jmin (1.0f, (std::floor (p * 4.0f) + 1.0f) / 4.0f);
        case Swell:    return p * p * p;
        case Bounce:   return std::abs (std::sin (pi * p * 3.0f)) * (1.0f - p * 0.7f);
        default:       return 1.0f;
    }
}

//==============================================================================
// Step rates, in quarter-note beats.
inline const juce::StringArray& rateNames()
{
    static const juce::StringArray names { "1/1", "1/2", "1/2T", "1/4", "1/4D", "1/4T", "1/8", "1/8D",
                                           "1/8T", "1/16", "1/16D", "1/16T", "1/32", "1/32T" };
    return names;
}

inline double rateBeats (int index) noexcept
{
    static constexpr double beats[] { 4.0, 2.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75,
                                      1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125, 1.0 / 12.0 };
    return beats[juce::jlimit (0, 13, index)];
}

namespace Rate
{
    enum { Whole = 0, Half, HalfT, Quarter, QuarterD, QuarterT, Eighth, EighthD,
           EighthT, Sixteenth, SixteenthD, SixteenthT, ThirtySecond, ThirtySecondT };
}

//==============================================================================
// One sequencer lane: 32 steps, each with a level and its own drawable envelope.
// Lock-free so the UI can edit while audio is running.
struct LaneSteps
{
    std::array<std::atomic<float>, kMaxSteps> values;
    std::array<std::atomic<int>, kMaxSteps> shapeIds;   // library shape, or -1 if hand-drawn
    std::array<std::array<std::atomic<float>, kCurvePoints>, kMaxSteps> curves;
    std::atomic<float> playPos { -1.0f };               // step index + phase, for the UI
    std::atomic<float> envOut { 0.0f };                 // current modulation value, for the UI
    std::atomic<int> editCounter { 0 };                 // bumped on every edit

    LaneSteps()
    {
        for (int s = 0; s < kMaxSteps; ++s)
        {
            values[(size_t) s].store (1.0f);
            setShape (s, Flat);
        }
    }

    void setShape (int step, int shape)
    {
        auto& c = curves[(size_t) step];
        for (int i = 0; i < kCurvePoints; ++i)
        {
            // Sample at point centres so hard-edged shapes keep their edges.
            const float p = (float) i / (float) (kCurvePoints - 1);
            c[(size_t) i].store (libraryShape (shape, juce::jmin (p, 0.9999f)));
        }
        shapeIds[(size_t) step].store (shape);
        ++editCounter;
    }

    void setCurvePoint (int step, int point, float v)
    {
        curves[(size_t) step][(size_t) point].store (juce::jlimit (0.0f, 1.0f, v));
        shapeIds[(size_t) step].store (-1);
        ++editCounter;
    }

    float curveAt (int step, float p) const noexcept
    {
        const auto& c = curves[(size_t) step];
        const float pos = juce::jlimit (0.0f, 1.0f, p) * (float) (kCurvePoints - 1);
        const int i = juce::jmin ((int) pos, kCurvePoints - 2);
        const float f = pos - (float) i;
        const float a = c[(size_t) i].load (std::memory_order_relaxed);
        const float b = c[(size_t) i + 1].load (std::memory_order_relaxed);
        return a + (b - a) * f;
    }

    float evaluate (int step, float p) const noexcept
    {
        return values[(size_t) step].load (std::memory_order_relaxed) * curveAt (step, p);
    }

    void copyStepFrom (int dst, const LaneSteps& src, int srcStep)
    {
        values[(size_t) dst].store (src.values[(size_t) srcStep].load());
        shapeIds[(size_t) dst].store (src.shapeIds[(size_t) srcStep].load());
        for (int i = 0; i < kCurvePoints; ++i)
            curves[(size_t) dst][(size_t) i].store (src.curves[(size_t) srcStep][(size_t) i].load());
    }

    //==============================================================================
    void writeTo (juce::ValueTree& t) const
    {
        juce::StringArray vals, ids;
        for (int s = 0; s < kMaxSteps; ++s)
        {
            vals.add (juce::String (values[(size_t) s].load(), 4));
            const int id = shapeIds[(size_t) s].load();
            ids.add (juce::String (id));
            if (id < 0)
                t.setProperty ("curve" + juce::String (s), curveToString (s), nullptr);
            else
                t.removeProperty ("curve" + juce::String (s), nullptr);
        }
        t.setProperty ("values", vals.joinIntoString (","), nullptr);
        t.setProperty ("shapes", ids.joinIntoString (","), nullptr);
    }

    void readFrom (const juce::ValueTree& t)
    {
        auto vals = juce::StringArray::fromTokens (t.getProperty ("values").toString(), ",", "");
        auto ids  = juce::StringArray::fromTokens (t.getProperty ("shapes").toString(), ",", "");
        for (int s = 0; s < kMaxSteps; ++s)
        {
            values[(size_t) s].store (s < vals.size() ? juce::jlimit (0.0f, 1.0f, vals[s].getFloatValue()) : 1.0f);
            const int id = s < ids.size() ? ids[s].getIntValue() : Flat;
            if (id >= 0)
                setShape (s, juce::jmin (id, NumShapes - 1));
            else
                curveFromString (s, t.getProperty ("curve" + juce::String (s)).toString());
        }
        ++editCounter;
    }

    juce::String curveToString (int s) const
    {
        juce::StringArray a;
        for (auto& v : curves[(size_t) s]) a.add (juce::String (v.load(), 4));
        return a.joinIntoString (",");
    }

    void curveFromString (int s, const juce::String& str)
    {
        auto a = juce::StringArray::fromTokens (str, ",", "");
        for (int i = 0; i < kCurvePoints; ++i)
            curves[(size_t) s][(size_t) i].store (i < a.size() ? juce::jlimit (0.0f, 1.0f, a[i].getFloatValue()) : 1.0f);
        shapeIds[(size_t) s].store (-1);
    }
};

} // namespace ss
