#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ss;

namespace
{
juce::NormalisableRange<float> freqRange (float lo, float hi)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return r;
}

float smoothCoef (float smoothParam, double sr)
{
    const double ms = 0.3 * std::pow (200.0, (double) smoothParam);   // 0.3 ms .. 60 ms
    return (float) std::exp (-1.0 / (ms * 0.001 * sr));
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout StepShaperProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    int version = 1;

    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, const String& unit = "%")
    {
        auto fmt = [unit] (float v, int) -> String
        {
            if (unit == "%")     return String (roundToInt (v * 100.0f)) + "%";
            if (unit == "swing") return String (roundToInt (50.0f + v * 25.0f)) + "%";
            if (unit == "Hz")    return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
            if (unit == "dB")    return String (v, 1) + " dB";
            if (unit == "ms")    return String (v, 1) + " ms";
            return String (v, 2);
        };
        auto parse = [unit] (const String& t) -> float
        {
            const float v = t.retainCharacters ("-0123456789.").getFloatValue();
            if (unit == "%")     return v / 100.0f;
            if (unit == "swing") return (v - 50.0f) / 25.0f;
            if (unit == "Hz" && t.containsIgnoreCase ("k")) return v * 1000.0f;
            return v;
        };
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, version }, name, r, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (fmt)
                                                                                          .withValueFromStringFunction (parse)));
    };
    auto addChoice = [&] (const String& id, const String& name, const StringArray& choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, version }, name, choices, def));
    };

    for (int m = 0; m < kNumModules; ++m)
    {
        const String n (moduleName (m));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid (m, "on"), version }, n + " On", m == Vol));
        addChoice (pid (m, "rate"), n + " Rate", rateNames(), Rate::Sixteenth);
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { pid (m, "len"), version }, n + " Length", 1, kMaxSteps, 16));
        addFloat (pid (m, "depth"), n + " Depth", { 0.0f, 1.0f }, 1.0f);
        addFloat (pid (m, "smooth"), n + " Smooth", { 0.0f, 1.0f }, (m == Pit || m == Flg) ? 0.4f : 0.2f);
    }

    addChoice ("flt_type", "Filter Type", { "Low Pass", "High Pass", "Band Pass", "Notch" }, 0);
    addFloat ("flt_min", "Filter Min", freqRange (20.0f, 20000.0f), 150.0f, "Hz");
    addFloat ("flt_max", "Filter Max", freqRange (20.0f, 20000.0f), 12000.0f, "Hz");
    addFloat ("flt_res", "Filter Resonance", { 0.0f, 1.0f }, 0.25f);

    addChoice ("dst_type", "Distortion Type", { "Soft", "Hard", "Fold", "Crush" }, 0);
    addFloat ("dst_drive", "Distortion Drive", { 0.0f, 40.0f }, 18.0f, "dB");
    addFloat ("dst_mix", "Distortion Mix", { 0.0f, 1.0f }, 1.0f);

    addFloat ("phs_min", "Phaser Min", freqRange (50.0f, 5000.0f), 200.0f, "Hz");
    addFloat ("phs_max", "Phaser Max", freqRange (200.0f, 15000.0f), 4000.0f, "Hz");
    addFloat ("phs_fb", "Phaser Feedback", { 0.0f, 0.9f }, 0.5f);
    addFloat ("phs_mix", "Phaser Mix", { 0.0f, 1.0f }, 1.0f);

    addFloat ("flg_range", "Flanger Range", { 0.2f, 10.0f }, 4.0f, "ms");
    addFloat ("flg_fb", "Flanger Feedback", { -0.9f, 0.9f }, 0.5f);
    addFloat ("flg_mix", "Flanger Mix", { 0.0f, 1.0f }, 0.7f);

    addFloat ("pit_range", "Pitch Range", { 0.5f, 25.0f }, 6.0f, "ms");
    addFloat ("pit_mix", "Pitch Mix", { 0.0f, 1.0f }, 1.0f);

    addChoice ("dly_time", "Delay Time", rateNames(), Rate::EighthD);
    addFloat ("dly_fb", "Delay Feedback", { 0.0f, 0.9f }, 0.45f);
    addFloat ("dly_mix", "Delay Mix", { 0.0f, 1.0f }, 0.6f);

    addFloat ("swing", "Swing", { 0.0f, 1.0f }, 0.0f, "swing");
    addFloat ("mix", "Mix", { 0.0f, 1.0f }, 1.0f);
    addFloat ("out", "Output", { -24.0f, 12.0f }, 0.0f, "dB");

    return layout;
}

//==============================================================================
StepShaperProcessor::StepShaperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "StepShaper", createLayout())
{
    for (int m = 0; m < kNumModules; ++m)
    {
        laneParams[(size_t) m] = { apvts.getRawParameterValue (pid (m, "on")),
                                   apvts.getRawParameterValue (pid (m, "rate")),
                                   apvts.getRawParameterValue (pid (m, "len")),
                                   apvts.getRawParameterValue (pid (m, "depth")),
                                   apvts.getRawParameterValue (pid (m, "smooth")) };
    }

    swing = p ("swing"); mix = p ("mix"); outGain = p ("out");
    fltType = p ("flt_type"); fltMin = p ("flt_min"); fltMax = p ("flt_max"); fltRes = p ("flt_res");
    dstType = p ("dst_type"); dstDrive = p ("dst_drive"); dstMix = p ("dst_mix");
    phsMin = p ("phs_min"); phsMax = p ("phs_max"); phsFb = p ("phs_fb"); phsMix = p ("phs_mix");
    flgRange = p ("flg_range"); flgFb = p ("flg_fb"); flgMix = p ("flg_mix");
    pitRange = p ("pit_range"); pitMix = p ("pit_mix");
    dlyTime = p ("dly_time"); dlyFb = p ("dly_fb"); dlyMix = p ("dly_mix");

    applyPreset (factoryPresets().front());
}

bool StepShaperProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void StepShaperProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) kMaxCh };
    filter.prepare (spec);
    filter.reset();
    lastFilterType = -1;

    for (int ch = 0; ch < kMaxCh; ++ch)
    {
        flgLine[(size_t) ch].prepare ((int) (sampleRate * 0.03));
        pitLine[(size_t) ch].prepare ((int) (sampleRate * 0.05));
        dlyLine[(size_t) ch].prepare ((int) (sampleRate * 4.5));
    }
    phsState = {}; phsLast = {}; flgLast = {}; dlyLast = {}; crushHold = {};
    laneEnv.fill (0.0f);
    pitDelaySm = flgDelaySm = dlyTimeSm = 0.0f;

    mixSm.reset (sampleRate, 0.02);
    outSm.reset (sampleRate, 0.02);
    mixSm.setCurrentAndTargetValue (mix->load());
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outGain->load()));
}

//==============================================================================
static inline float evalLane (LaneSteps& lane, double beat, double stepBeats, int len, double swingAmt, float& playPos)
{
    const double pos = beat / stepBeats;
    const double pairs = std::floor (pos * 0.5);
    const double t = pos - pairs * 2.0;          // 0..2 within a pair of steps
    int sub; double ph;
    if (t < 1.0 + swingAmt) { sub = 0; ph = t / (1.0 + swingAmt); }
    else                    { sub = 1; ph = (t - 1.0 - swingAmt) / (1.0 - swingAmt); }

    const long long absStep = (long long) pairs * 2 + sub;
    const int idx = (int) (((absStep % len) + len) % len);
    playPos = (float) idx + (float) ph;
    return lane.evaluate (idx, (float) ph);
}

void StepShaperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), kMaxCh, getTotalNumInputChannels());
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
    if (numCh == 0 || numSamples == 0)
        return;

    // --- Transport ---------------------------------------------------------
    double bpm = 120.0, ppq = freeRunPpq;
    bool playing = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            playing = pos->getIsPlaying();
            if (playing)
                if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    }
    if (playing && ! wasPlaying)
        laneEnv.fill (0.0f);
    wasPlaying = playing;

    const double beatsPerSample = bpm / (60.0 * sr);
    freeRunPpq = ppq + beatsPerSample * numSamples;   // keeps running when stopped so you can audition live input
    const double samplesPerBeat = 1.0 / beatsPerSample;
    const double swingAmt = (double) swing->load() * 0.5;

    // --- Per-lane settings ---------------------------------------------------
    struct LaneRun { bool on; double stepBeats; int len; float depth; float coef; float playPos; };
    std::array<LaneRun, kNumModules> run {};
    for (int m = 0; m < kNumModules; ++m)
    {
        auto& lp = laneParams[(size_t) m];
        auto& r = run[(size_t) m];
        r.on = lp.on->load() > 0.5f;
        r.stepBeats = rateBeats ((int) lp.rate->load());
        r.len = juce::jlimit (1, kMaxSteps, (int) lp.length->load());
        r.depth = lp.depth->load();
        float sm = lp.smooth->load();
        if (m == Pit || m == Flg) sm = juce::jmax (sm, 0.3f);   // delay-based modules need some glide
        r.coef = smoothCoef (sm, sr);
        r.playPos = -1.0f;
    }

    // --- Module settings ---------------------------------------------------------
    const int fType = (int) fltType->load();
    if (fType != lastFilterType)
    {
        using T = juce::dsp::StateVariableTPTFilterType;
        filter.setType (fType == 1 ? T::highpass : (fType == 0 ? T::lowpass : T::bandpass));
        lastFilterType = fType;
    }
    const float fLo = fltMin->load(), fHi = juce::jmax (fltMin->load(), fltMax->load());
    const float fQ = 0.5f + fltRes->load() * fltRes->load() * 9.5f;
    filter.setResonance (fType == 3 ? juce::jmax (0.7f, fQ) : fQ);
    const float resComp = 1.0f / std::sqrt (juce::jmax (1.0f, fQ * 0.7f));   // tame resonant peaks
    const float notchScale = 1.0f / (fType == 3 ? juce::jmax (0.7f, fQ) : fQ);
    const float nyq = (float) sr * 0.45f;

    const int dType = (int) dstType->load();
    const float dDrive = dstDrive->load(), dMix = dstMix->load();
    const float pLo = phsMin->load(), pHi = juce::jmax (phsMin->load(), phsMax->load());
    const float pFb = phsFb->load(), pMix = phsMix->load();
    const float gRange = flgRange->load() * 0.001f * (float) sr, gFb = flgFb->load(), gMix = flgMix->load();
    const float tRange = pitRange->load() * 0.001f * (float) sr, tMix = pitMix->load();
    const float yTime = juce::jmin ((float) (rateBeats ((int) dlyTime->load()) * samplesPerBeat), (float) sr * 4.0f);
    const float yFb = dlyFb->load(), yMix = dlyMix->load();
    if (dlyTimeSm <= 0.0f) dlyTimeSm = yTime;

    mixSm.setTargetValue (mix->load());
    outSm.setTargetValue (juce::Decibels::decibelsToGain (outGain->load()));

    const float timeCoef = smoothCoef (0.6f, sr);   // ~7 ms glide on delay times
    float phaserA = 0.0f;
    float m[kNumModules] {};

    for (int i = 0; i < numSamples; ++i)
    {
        const double beat = ppq + (double) i * beatsPerSample;

        // Envelopes -> modulation amount per module (0..1).
        for (int mod = 0; mod < kNumModules; ++mod)
        {
            auto& r = run[(size_t) mod];
            if (! r.on) { m[mod] = 1.0f; continue; }
            const float target = evalLane (lanes[(size_t) mod], beat, r.stepBeats, r.len, swingAmt, r.playPos);
            auto& e = laneEnv[(size_t) mod];
            e = target + (e - target) * r.coef;
            m[mod] = 1.0f - r.depth * (1.0f - e);
        }

        // Control-rate updates every 16 samples.
        if ((i & 15) == 0)
        {
            if (run[Flt].on)
                filter.setCutoffFrequency (juce::jmin (nyq, fLo * std::pow (fHi / fLo, m[Flt])));
            if (run[Phs].on)
            {
                const float f = juce::jmin (nyq, pLo * std::pow (pHi / pLo, m[Phs]));
                const float tn = std::tan (juce::MathConstants<float>::pi * f / (float) sr);
                phaserA = (tn - 1.0f) / (tn + 1.0f);
            }
        }

        const float flgTarget = 0.1f * 0.001f * (float) sr + gRange * m[Flg];
        const float pitTarget = 1.0f + tRange * m[Pit];
        flgDelaySm = flgTarget + (flgDelaySm - flgTarget) * timeCoef;
        pitDelaySm = pitTarget + (pitDelaySm - pitTarget) * timeCoef;
        dlyTimeSm = yTime + (dlyTimeSm - yTime) * 0.9995f;

        // Crush sample-and-hold rate
        const float crushAmt = (dDrive / 40.0f) * m[Dst];
        const int holdN = 1 + (int) (crushAmt * crushAmt * 24.0f);
        const bool crushTake = (crushCounter++ % holdN) == 0;

        const float wetMix = mixSm.getNextValue();
        const float out = outSm.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            const float dry = d[i];
            float x = dry;

            if (run[Dst].on)
            {
                const float g = juce::Decibels::decibelsToGain (dDrive * m[Dst]);
                float y;
                switch (dType)
                {
                    case 0:  y = std::tanh (x * g) / std::sqrt (g); break;
                    case 1:  y = juce::jlimit (-1.0f, 1.0f, x * g) / std::sqrt (g); break;
                    case 2:  y = std::sin (x * g * 1.5707963f) * 0.8f; break;
                    default:
                    {
                        if (crushTake) crushHold[(size_t) ch] = x;
                        const float levels = std::pow (2.0f, 16.0f - 13.0f * crushAmt);
                        y = std::round (crushHold[(size_t) ch] * levels) / levels;
                        break;
                    }
                }
                x += dMix * (y - x);
            }

            if (run[Flt].on)
            {
                const float y = filter.processSample (ch, x);
                x = (fType == 3) ? x - y * notchScale : y * resComp;   // notch = input minus normalised band pass
            }

            if (run[Phs].on)
            {
                auto& st = phsState[(size_t) ch];
                float a = x + pFb * phsLast[(size_t) ch];
                for (auto& s : st)
                {
                    const float y = phaserA * a + s;
                    s = a - phaserA * y;
                    a = y;
                }
                phsLast[(size_t) ch] = a;
                x += pMix * (0.5f * (x + a) - x);
            }

            if (run[Flg].on)
            {
                auto& dl = flgLine[(size_t) ch];
                const float y = dl.read (flgDelaySm);
                dl.push (x + gFb * y);
                x = x * (1.0f - 0.5f * gMix) + y * 0.5f * gMix;
            }

            if (run[Pit].on)
            {
                auto& dl = pitLine[(size_t) ch];
                dl.push (x);
                const float y = dl.read (pitDelaySm);
                x += tMix * (y - x);
            }

            if (run[Dly].on)
            {
                auto& dl = dlyLine[(size_t) ch];
                const float y = dl.read (dlyTimeSm);
                dl.push (x * m[Dly] + yFb * y);
                x += yMix * y;
            }

            if (run[Vol].on)
                x *= m[Vol];

            d[i] = (dry + wetMix * (x - dry)) * out;
        }
    }

    // Publish playheads for the UI.
    for (int mod = 0; mod < kNumModules; ++mod)
    {
        lanes[(size_t) mod].playPos.store (run[(size_t) mod].on ? run[(size_t) mod].playPos : -1.0f);
        lanes[(size_t) mod].envOut.store (run[(size_t) mod].on ? laneEnv[(size_t) mod] : 0.0f);
    }
}

//==============================================================================
int StepShaperProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

void StepShaperProcessor::setCurrentProgram (int index)
{
    const auto& list = factoryPresets();
    if (juce::isPositiveAndBelow (index, (int) list.size()))
    {
        currentProgram = index;
        applyPreset (list[(size_t) index]);
    }
}

const juce::String StepShaperProcessor::getProgramName (int index)
{
    const auto& list = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return {};
    return list[(size_t) index].category + ": " + list[(size_t) index].name;
}

void StepShaperProcessor::applyPreset (const Preset& preset)
{
    // Reset everything to defaults, then apply the preset's values.
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    };

    for (int m = 0; m < kNumModules; ++m)
    {
        const auto& l = preset.lanes[(size_t) m];
        set (pid (m, "on"), l.on ? 1.0f : 0.0f);
        set (pid (m, "rate"), (float) l.rate);
        set (pid (m, "len"), (float) l.length);
        set (pid (m, "depth"), l.depth);
        set (pid (m, "smooth"), l.smooth);
        for (int s = 0; s < kMaxSteps; ++s)
        {
            lanes[(size_t) m].values[(size_t) s].store (l.values[(size_t) s]);
            lanes[(size_t) m].setShape (s, l.shapes[(size_t) s]);
        }
    }
    for (auto& [id, v] : preset.params)
        set (id, v);

    presetName = preset.name;
    ++presetChangeCounter;
}

//==============================================================================
juce::ValueTree StepShaperProcessor::createStateTree()
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("Lanes"), nullptr);
    juce::ValueTree lanesTree ("Lanes");
    for (int m = 0; m < kNumModules; ++m)
    {
        juce::ValueTree t ("Lane");
        t.setProperty ("module", m, nullptr);
        lanes[(size_t) m].writeTo (t);
        lanesTree.appendChild (t, nullptr);
    }
    state.appendChild (lanesTree, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    return state;
}

void StepShaperProcessor::restoreStateTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (apvts.state.getType()))
        return;
    auto lanesTree = tree.getChildWithName ("Lanes");
    for (auto t : lanesTree)
    {
        const int m = t.getProperty ("module", -1);
        if (juce::isPositiveAndBelow (m, (int) kNumModules))
            lanes[(size_t) m].readFrom (t);
    }
    auto copy = tree.createCopy();
    copy.removeChild (copy.getChildWithName ("Lanes"), nullptr);
    apvts.replaceState (copy);
    presetName = tree.getProperty ("presetName", "Init").toString();
    ++presetChangeCounter;
}

void StepShaperProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void StepShaperProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreStateTree (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
static juce::File baseFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("StepShaper");
}
juce::File StepShaperProcessor::userPresetFolder() { return baseFolder().getChildFile ("Presets"); }
juce::File StepShaperProcessor::userShapeFolder()  { return baseFolder().getChildFile ("Shapes"); }
juce::File StepShaperProcessor::userModuleFolder() { return baseFolder().getChildFile ("Modules"); }

juce::Array<juce::File> StepShaperProcessor::getUserPresets() const
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.stepshaper");
    files.sort();
    return files;
}

bool StepShaperProcessor::saveUserPreset (const juce::String& name)
{
    const auto safe = juce::File::createLegalFileName (name.trim());
    if (safe.isEmpty()) return false;
    userPresetFolder().createDirectory();
    presetName = name.trim();
    auto xml = createStateTree().createXml();
    const bool ok = xml != nullptr && xml->writeTo (userPresetFolder().getChildFile (safe + ".stepshaper"));
    ++presetChangeCounter;
    return ok;
}

bool StepShaperProcessor::loadUserPreset (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    restoreStateTree (juce::ValueTree::fromXml (*xml));
    presetName = f.getFileNameWithoutExtension();
    ++presetChangeCounter;
    return true;
}

bool StepShaperProcessor::saveModulePreset (int module, const juce::File& file)
{
    juce::ValueTree t ("StepShaperModule");
    t.setProperty ("module", module, nullptr);
    lanes[(size_t) module].writeTo (t);
    const juce::String prefix (modulePrefix (module));
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (rp->getParameterID().startsWith (prefix))
                t.setProperty (rp->getParameterID(), rp->convertFrom0to1 (rp->getValue()), nullptr);
    file.getParentDirectory().createDirectory();
    auto xml = t.createXml();
    return xml != nullptr && xml->writeTo (file);
}

bool StepShaperProcessor::loadModulePreset (int module, const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    auto t = juce::ValueTree::fromXml (*xml);
    if (! t.hasType ("StepShaperModule")) return false;

    // Settings from another module type are mapped by suffix (e.g. vol_rate -> flt_rate).
    const int srcModule = t.getProperty ("module", module);
    const juce::String srcPrefix (modulePrefix (srcModule)), dstPrefix (modulePrefix (module));
    for (int i = 0; i < t.getNumProperties(); ++i)
    {
        const auto name = t.getPropertyName (i).toString();
        if (! name.startsWith (srcPrefix)) continue;
        if (auto* rp = apvts.getParameter (dstPrefix + name.substring (srcPrefix.length())))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) t.getProperty (name)));
    }
    lanes[(size_t) module].readFrom (t);
    return true;
}

//==============================================================================
void StepShaperProcessor::randomiseLane (int module)
{
    juce::Random rng;
    auto& l = lanes[(size_t) module];
    for (int s = 0; s < kMaxSteps; ++s)
    {
        if (module == Vol)
            l.values[(size_t) s].store (rng.nextFloat() < 0.65f ? (rng.nextFloat() < 0.75f ? 1.0f : 0.5f) : 0.0f);
        else
            l.values[(size_t) s].store (rng.nextFloat());
        if (rng.nextFloat() < 0.3f)
            l.setShape (s, rng.nextInt (NumShapes));
    }
    ++l.editCounter;
}

void StepShaperProcessor::clearLane (int module)
{
    auto& l = lanes[(size_t) module];
    for (int s = 0; s < kMaxSteps; ++s)
    {
        l.values[(size_t) s].store (1.0f);
        l.setShape (s, Flat);
    }
}

void StepShaperProcessor::invertLane (int module)
{
    auto& l = lanes[(size_t) module];
    for (auto& v : l.values) v.store (1.0f - v.load());
    ++l.editCounter;
}

void StepShaperProcessor::shiftLane (int module, int direction)
{
    auto& l = lanes[(size_t) module];
    const int len = juce::jlimit (1, kMaxSteps, (int) laneParams[(size_t) module].length->load());
    ss::LaneSteps tmp;
    for (int s = 0; s < len; ++s) tmp.copyStepFrom (s, l, s);
    for (int s = 0; s < len; ++s)
        l.copyStepFrom ((s + direction + len) % len, tmp, s);
    ++l.editCounter;
}

//==============================================================================
juce::AudioProcessorEditor* StepShaperProcessor::createEditor() { return new StepShaperEditor (*this); }

#if ! STEPSHAPER_TESTS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StepShaperProcessor(); }
#endif
