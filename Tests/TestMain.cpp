// Headless test runner: exercises every factory preset through the DSP with a fake
// host transport, checks state save/restore, and renders editor screenshots.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 128.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        info.setIsPlaying (playing);
        return info;
    }
};

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    if (! ok) { ++failures; std::cout << "FAIL: " << what << "\n"; }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());

    StepShaperProcessor proc;
    FakePlayHead head;
    proc.setPlayHead (&head);
    const double sr = 48000.0;
    const int block = 512;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);

    const auto& presets = ss::factoryPresets();
    std::cout << "Factory presets: " << presets.size() << "\n";
    check (presets.size() >= 120, "at least 120 presets");

    juce::StringArray names;
    for (auto& p : presets)
    {
        check (! names.contains (p.name), "unique preset name " + p.name);
        names.add (p.name);
    }

    juce::Random rng (42);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;

    for (int i = 0; i < (int) presets.size(); ++i)
    {
        proc.setCurrentProgram (i);
        proc.prepareToPlay (sr, block);
        head.ppq = 0.0;
        double peak = 0.0, sumIn = 0.0, sumOut = 0.0;
        bool finite = true;
        const int blocks = (int) (sr * 4.0 / block);   // 4 seconds
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < block; ++s)
                {
                    const float v = 0.5f * std::sin ((float) (b * block + s) * 0.03f) + 0.1f * (rng.nextFloat() - 0.5f);
                    buf.setSample (ch, s, v);
                    sumIn += v * v;
                }
            proc.processBlock (buf, midi);
            head.ppq += block * head.bpm / (60.0 * sr);
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < block; ++s)
                {
                    const float v = buf.getSample (ch, s);
                    if (! std::isfinite (v)) finite = false;
                    peak = std::max (peak, (double) std::abs (v));
                    sumOut += v * v;
                }
        }
        const double gainDb = 10.0 * std::log10 ((sumOut + 1e-12) / (sumIn + 1e-12));
        check (finite, presets[(size_t) i].name + " produced NaN/inf");
        check (peak < 4.0, presets[(size_t) i].name + " peak too high: " + juce::String (peak));
        check (gainDb > -40.0, presets[(size_t) i].name + " near-silent: " + juce::String (gainDb, 1) + " dB");
        if (argc > 2)
            std::cout << juce::String (presets[(size_t) i].category).paddedRight (' ', 20) << presets[(size_t) i].name.paddedRight (' ', 26)
                      << " peak " << juce::String (peak, 2) << "  level " << juce::String (gainDb, 1) << " dB\n";
    }

    // Gate correctness: "Straight Eighths" (x.x. at 1/16) should silence every odd 16th.
    {
        int idx = names.indexOf ("Straight Eighths");
        proc.setCurrentProgram (idx);
        proc.prepareToPlay (sr, block);
        head.ppq = 0.0; head.bpm = 120.0;   // 1/16 = 6000 samples at 48k
        const int total = 48000;
        std::vector<float> out;
        juce::AudioBuffer<float> b1 (2, block);
        for (int done = 0; done < total; done += block)
        {
            for (int ch = 0; ch < 2; ++ch) for (int s = 0; s < block; ++s) b1.setSample (ch, s, 1.0f);
            proc.processBlock (b1, midi);
            head.ppq += block * head.bpm / (60.0 * sr);
            for (int s = 0; s < block; ++s) out.push_back (b1.getSample (0, s));
        }
        check (out[3000] > 0.95f, "gate open in step 1 (" + juce::String (out[3000]) + ")");
        check (out[9000] < 0.05f, "gate closed in step 2 (" + juce::String (out[9000]) + ")");
        check (out[15000] > 0.95f, "gate open in step 3");
    }

    // State round trip including a hand-drawn curve.
    {
        proc.setCurrentProgram (5);
        proc.lanes[ss::Flt].setCurvePoint (3, 10, 0.123f);
        proc.lanes[ss::Flt].values[3].store (0.42f);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        StepShaperProcessor other;
        other.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (other.lanes[ss::Flt].curves[3][10].load() - 0.123f) < 1e-3f, "custom curve restored");
        check (std::abs (other.lanes[ss::Flt].values[3].load() - 0.42f) < 1e-3f, "step value restored");
        check (other.lanes[ss::Flt].shapeIds[3].load() == -1, "custom flag restored");
        check (other.getPresetName() == proc.getPresetName(), "preset name restored");
        for (auto* p : proc.getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
            auto* op = other.apvts.getParameter (rp->getParameterID());
            check (std::abs (rp->getValue() - op->getValue()) < 1e-5f, "param restored " + rp->getParameterID());
        }
    }

    // Module mini-preset round trip, across modules.
    {
        auto f = juce::File::createTempFile ("ssmodule");
        proc.setCurrentProgram (names.indexOf ("Acid Steps"));
        check (proc.saveModulePreset (ss::Flt, f), "save module");
        check (proc.loadModulePreset (ss::Dst, f), "load module into another module");
        check (std::abs (proc.lanes[ss::Dst].values[2].load() - proc.lanes[ss::Flt].values[2].load()) < 1e-3f, "module steps copied");
        f.deleteFile();
    }

    // Editor screenshots.
    {
        proc.setCurrentProgram (names.indexOf ("Everything Moves"));
        head.ppq = 3.3;
        for (int b = 0; b < 8; ++b) proc.processBlock (buf, midi);
        std::unique_ptr<StepShaperEditor> ed (dynamic_cast<StepShaperEditor*> (proc.createEditor()));
        for (int m : { (int) ss::Vol, (int) ss::Flt, (int) ss::Dly })
        {
            ed->selectModule (m);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            auto file = outDir.getChildFile ("editor_" + juce::String (ss::moduleName (m)) + ".png");
            file.deleteFile();
            juce::FileOutputStream os (file);
            png.writeImageToStream (img, os);
            std::cout << "Wrote " << file.getFullPathName() << "\n";
        }
        proc.editorBeingDeleted (ed.get());
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " FAILURES") << "\n";
    return failures == 0 ? 0 : 1;
}
