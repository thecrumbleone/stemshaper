#include "PluginEditor.h"

using namespace ss;

static constexpr int kWidth = 1120, kHeight = 720;

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name, juce::Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Colours::text);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

Choice::Choice (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
        box.addItemList (p->choices, 1);
    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (box);
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    box.setBounds (r.removeFromTop (26).reduced (2, 0));
}

//==============================================================================
StepShaperEditor::StepShaperEditor (StepShaperProcessor& p)
    : AudioProcessorEditor (&p), proc (p), moduleList (p), grid (p), shapeEditor (p)
{
    setLookAndFeel (&lnf);

    // --- Presets -------------------------------------------------------------
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id >= 10000)
        {
            const int idx = id - 10000;
            if (juce::isPositiveAndBelow (idx, (int) userPresetFiles.size()))
                proc.loadUserPreset (userPresetFiles[(size_t) idx]);
        }
        else
        {
            proc.setCurrentProgram (id - 1);
        }
        lastPresetCounter = proc.presetChangeCounter.load();
        grid.setSelectedStep (grid.getSelectedStep());
    };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &randomAllButton })
        addAndMakeVisible (b);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { savePresetDialog(); };
    randomAllButton.setTooltip ("Randomise the steps of every active module");
    randomAllButton.onClick = [this]
    {
        for (int m = 0; m < kNumModules; ++m)
            if (proc.apvts.getRawParameterValue (StepShaperProcessor::pid (m, "on"))->load() > 0.5f)
                proc.randomiseLane (m);
    };
    rebuildPresetList();

    // --- Main area -----------------------------------------------------------------
    addAndMakeVisible (moduleList);
    addAndMakeVisible (grid);
    addAndMakeVisible (shapeEditor);
    addAndMakeVisible (shapeLibrary);
    moduleList.onSelect = [this] (int m) { selectModule (m); };
    grid.getBrush = [this] { return shapeLibrary.getBrush(); };
    grid.onSelectionChanged = [this] { shapeEditor.setTarget (module, grid.getSelectedStep()); };
    grid.setTooltip ("Drag: draw step levels   Alt-click: toggle step   Cmd/Ctrl-drag: stamp selected shape   Right-click: step menu");
    shapeEditor.setTooltip ("Draw the envelope for the selected step");
    shapeLibrary.onShapeChosen = [this] (int shape, bool all)
    {
        auto& lane = proc.lanes[(size_t) module];
        if (all) for (int s = 0; s < kMaxSteps; ++s) lane.setShape (s, shape);
        else lane.setShape (grid.getSelectedStep(), shape);
    };

    for (auto* b : { &copyStep, &pasteStep, &pasteAll, &saveShapeButton, &loadShapeButton })
        addAndMakeVisible (b);
    copyStep.onClick = [this]
    {
        auto& lane = proc.lanes[(size_t) module];
        const int s = grid.getSelectedStep();
        clipLevel = lane.values[(size_t) s].load();
        clipShape = lane.shapeIds[(size_t) s].load();
        for (int i = 0; i < kCurvePoints; ++i) clipCurve[(size_t) i] = lane.curves[(size_t) s][(size_t) i].load();
        hasClip = true;
    };
    auto paste = [this] (int s)
    {
        auto& lane = proc.lanes[(size_t) module];
        lane.values[(size_t) s].store (clipLevel);
        for (int i = 0; i < kCurvePoints; ++i) lane.curves[(size_t) s][(size_t) i].store (clipCurve[(size_t) i]);
        lane.shapeIds[(size_t) s].store (clipShape);
        ++lane.editCounter;
    };
    pasteStep.onClick = [this, paste] { if (hasClip) paste (grid.getSelectedStep()); };
    pasteAll.onClick = [this, paste] { if (hasClip) for (int s = 0; s < kMaxSteps; ++s) paste (s); };
    saveShapeButton.onClick = [this] { saveShape(); };
    loadShapeButton.onClick = [this] { loadShape(); };

    // --- Per-module controls -----------------------------------------------------------
    auto& st = proc.apvts;
    for (int m = 0; m < kNumModules; ++m)
    {
        auto& ui = laneUI[(size_t) m];
        const auto accent = moduleColour (m);
        ui.rate   = std::make_unique<Choice> (st, StepShaperProcessor::pid (m, "rate"), "Rate");
        ui.length = std::make_unique<Knob> (st, StepShaperProcessor::pid (m, "len"), "Steps", accent);
        ui.depth  = std::make_unique<Knob> (st, StepShaperProcessor::pid (m, "depth"), "Depth", accent);
        ui.smooth = std::make_unique<Knob> (st, StepShaperProcessor::pid (m, "smooth"), "Smooth", accent);
        ui.depth->slider.setTooltip ("How far the sequence moves this module (0 = static at maximum)");
        ui.smooth->slider.setTooltip ("Glide between values: low = snappy gates, high = smooth curves");

        auto knob = [&] (const char* id, const char* name) { ui.settings.push_back (std::make_unique<Knob> (st, id, name, accent)); };
        auto choice = [&] (const char* id, const char* name) { ui.settings.push_back (std::make_unique<Choice> (st, id, name)); };
        switch (m)
        {
            case Flt: choice ("flt_type", "Type"); knob ("flt_min", "Min Freq"); knob ("flt_max", "Max Freq"); knob ("flt_res", "Resonance"); break;
            case Dst: choice ("dst_type", "Type"); knob ("dst_drive", "Drive"); knob ("dst_mix", "Mix"); break;
            case Phs: knob ("phs_min", "Min Freq"); knob ("phs_max", "Max Freq"); knob ("phs_fb", "Feedback"); knob ("phs_mix", "Mix"); break;
            case Flg: knob ("flg_range", "Range"); knob ("flg_fb", "Feedback"); knob ("flg_mix", "Mix"); break;
            case Pit: knob ("pit_range", "Range"); knob ("pit_mix", "Mix"); break;
            case Dly: choice ("dly_time", "Time"); knob ("dly_fb", "Feedback"); knob ("dly_mix", "Mix"); break;
            default: break;
        }

        for (auto* c : std::initializer_list<juce::Component*> { ui.rate.get(), ui.length.get(), ui.depth.get(), ui.smooth.get() })
            addChildComponent (c);
        for (auto& c : ui.settings)
            addChildComponent (c.get());
    }

    for (auto* b : { &randButton, &clearButton, &invertButton, &shiftLeft, &shiftRight, &saveModuleButton, &loadModuleButton })
        addAndMakeVisible (b);
    randButton.onClick   = [this] { proc.randomiseLane (module); };
    clearButton.onClick  = [this] { proc.clearLane (module); };
    invertButton.onClick = [this] { proc.invertLane (module); };
    shiftLeft.onClick    = [this] { proc.shiftLane (module, -1); };
    shiftRight.onClick   = [this] { proc.shiftLane (module, 1); };
    shiftLeft.setTooltip ("Rotate steps left");
    shiftRight.setTooltip ("Rotate steps right");
    saveModuleButton.setTooltip ("Save this module's sequence and settings as a mini-preset");
    loadModuleButton.setTooltip ("Load a module mini-preset into this module");
    saveModuleButton.onClick = [this] { saveModule(); };
    loadModuleButton.onClick = [this] { loadModule(); };

    const auto globalAccent = juce::Colour (0xffe8ecf4);
    swingKnob = std::make_unique<Knob> (st, "swing", "Swing", globalAccent);
    mixKnob   = std::make_unique<Knob> (st, "mix", "Mix", globalAccent);
    outKnob   = std::make_unique<Knob> (st, "out", "Output", globalAccent);
    addAndMakeVisible (*swingKnob);
    addAndMakeVisible (*mixKnob);
    addAndMakeVisible (*outKnob);

    setSize (kWidth, kHeight);
    selectModule (Vol);
    startTimerHz (30);
}

StepShaperEditor::~StepShaperEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void StepShaperEditor::selectModule (int m)
{
    module = juce::jlimit (0, kNumModules - 1, m);
    moduleList.setSelected (module);
    grid.setModule (module);
    shapeEditor.setTarget (module, grid.getSelectedStep());
    shapeLibrary.setAccent (moduleColour (module));

    for (int i = 0; i < kNumModules; ++i)
    {
        auto& ui = laneUI[(size_t) i];
        const bool vis = i == module;
        ui.rate->setVisible (vis);
        ui.length->setVisible (vis);
        ui.depth->setVisible (vis);
        ui.smooth->setVisible (vis);
        for (auto& c : ui.settings) c->setVisible (vis);
    }
    resized();
    repaint();
}

void StepShaperEditor::timerCallback()
{
    grid.refresh();
    shapeEditor.refresh();
    moduleList.refresh();

    const int pc = proc.presetChangeCounter.load();
    if (pc != lastPresetCounter)
    {
        lastPresetCounter = pc;
        rebuildPresetList();
        shapeEditor.setTarget (module, grid.getSelectedStep());
    }
}

void StepShaperEditor::rebuildPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& list = factoryPresets();
    juce::String lastCat;
    int selectedId = 0;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& pr = list[(size_t) i];
        if (pr.category != lastCat)
        {
            presetBox.addSectionHeading (pr.category);
            lastCat = pr.category;
        }
        presetBox.addItem (pr.name, i + 1);
        if (pr.name == proc.getPresetName() && i == proc.getCurrentProgram()) selectedId = i + 1;
    }

    userPresetFiles.clear();
    for (auto& f : proc.getUserPresets()) userPresetFiles.push_back (f);
    if (! userPresetFiles.empty())
    {
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < (int) userPresetFiles.size(); ++i)
        {
            const auto name = userPresetFiles[(size_t) i].getFileNameWithoutExtension();
            presetBox.addItem (name, 10000 + i);
            if (name == proc.getPresetName()) selectedId = 10000 + i;
        }
    }

    if (selectedId > 0) presetBox.setSelectedId (selectedId, juce::dontSendNotification);
    else presetBox.setText (proc.getPresetName(), juce::dontSendNotification);
}

void StepShaperEditor::stepPreset (int delta)
{
    const int n = presetBox.getNumItems();
    if (n == 0) return;
    int idx = presetBox.getSelectedItemIndex();
    idx = idx < 0 ? 0 : (idx + delta + n) % n;
    presetBox.setSelectedItemIndex (idx, juce::sendNotificationSync);
}

void StepShaperEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save Preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.getPresetName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<StepShaperEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe != nullptr && result == 1)
        {
            safe->proc.saveUserPreset (w->getTextEditorContents ("name"));
            safe->rebuildPresetList();
        }
    }), true);
}

//==============================================================================
void StepShaperEditor::saveShape()
{
    StepShaperProcessor::userShapeFolder().createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save step shape", StepShaperProcessor::userShapeFolder(), "*.ssshape");
    juce::Component::SafePointer<StepShaperEditor> safe (this);
    const int s = grid.getSelectedStep(), m = module;
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe, s, m] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File()) return;
            juce::XmlElement xml ("StepShaperShape");
            xml.setAttribute ("curve", safe->proc.lanes[(size_t) m].curveToString (s));
            xml.writeTo (fc.getResult().withFileExtension ("ssshape"));
        });
}

void StepShaperEditor::loadShape()
{
    chooser = std::make_unique<juce::FileChooser> ("Load step shape", StepShaperProcessor::userShapeFolder(), "*.ssshape");
    juce::Component::SafePointer<StepShaperEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || ! fc.getResult().existsAsFile()) return;
            if (auto xml = juce::XmlDocument::parse (fc.getResult()))
            {
                auto& lane = safe->proc.lanes[(size_t) safe->module];
                lane.curveFromString (safe->grid.getSelectedStep(), xml->getStringAttribute ("curve"));
                ++lane.editCounter;
            }
        });
}

void StepShaperEditor::saveModule()
{
    StepShaperProcessor::userModuleFolder().createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save " + juce::String (moduleName (module)) + " module",
                                                   StepShaperProcessor::userModuleFolder(), "*.ssmodule");
    juce::Component::SafePointer<StepShaperEditor> safe (this);
    const int m = module;
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe, m] (const juce::FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult() != juce::File())
                safe->proc.saveModulePreset (m, fc.getResult().withFileExtension ("ssmodule"));
        });
}

void StepShaperEditor::loadModule()
{
    chooser = std::make_unique<juce::FileChooser> ("Load module", StepShaperProcessor::userModuleFolder(), "*.ssmodule");
    juce::Component::SafePointer<StepShaperEditor> safe (this);
    const int m = module;
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, m] (const juce::FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult().existsAsFile())
                safe->proc.loadModulePreset (m, fc.getResult());
        });
}

//==============================================================================
static void panelBox (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent)
{
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText (title, r.reduced (12, 8).removeFromTop (16), juce::Justification::centredLeft);
}

void StepShaperEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);

    // Title
    g.setColour (Colours::text);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.drawText ("STEPSHAPER", 18, 12, 200, 32, juce::Justification::centredLeft);
    g.setColour (Colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("MOTION SEQUENCER", 172, 20, 140, 20, juce::Justification::centredLeft);

    const auto accent = moduleColour (module);
    const juce::String name = juce::String (moduleName (module)).toUpperCase();
    panelBox (g, lanePanel, name + " SEQUENCE", accent);
    if (module == Vol)
    {
        panelBox (g, settingsPanel, "VOLUME", accent);
        g.setColour (Colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawFittedText ("Volume follows the step levels and shapes directly.\n"
                          "Use Pump / Duck shapes for sidechain-style movement,\n"
                          "Flat steps with gaps for trance gates.",
                          settingsPanel.reduced (16, 36), juce::Justification::topLeft, 4);
    }
    else
    {
        panelBox (g, settingsPanel, name + " SETTINGS", accent);
    }
    panelBox (g, globalPanel, "GLOBAL", Colours::text);

    // Right-hand panel title
    g.setColour (Colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("SHAPE LIBRARY", shapeLibrary.getBounds().translated (0, -16).withHeight (14), juce::Justification::centredLeft);
}

void StepShaperEditor::resized()
{
    // Top bar
    auto top = juce::Rectangle<int> (0, 10, kWidth, 36);
    auto presetArea = top.withSizeKeepingCentre (560, 30).translated (60, 0);
    prevButton.setBounds (presetArea.removeFromLeft (30));
    presetArea.removeFromLeft (4);
    nextButton.setBounds (presetArea.removeFromRight (30));
    presetArea.removeFromRight (4);
    presetBox.setBounds (presetArea);
    randomAllButton.setBounds (kWidth - 16 - 96, 13, 96, 30);
    saveButton.setBounds (kWidth - 16 - 96 - 8 - 70, 13, 70, 30);

    // Main row
    const int mainY = 60, mainH = 340;
    moduleList.setBounds (16, mainY, 160, mainH);
    const int rightW = 340;
    const int gridX = 184, gridW = kWidth - gridX - 16 - rightW - 8;
    grid.setBounds (gridX, mainY, gridW, mainH);

    const int rx = gridX + gridW + 8;
    shapeEditor.setBounds (rx, mainY, rightW, 190);
    shapeLibrary.setBounds (rx, mainY + 190 + 20, rightW, 80);
    auto btnRow = juce::Rectangle<int> (rx, mainY + mainH - 26, rightW, 26);
    const int bw = (rightW - 4 * 4) / 5;
    for (auto* b : { &copyStep, &pasteStep, &pasteAll, &saveShapeButton, &loadShapeButton })
    {
        b->setBounds (btnRow.removeFromLeft (bw));
        btnRow.removeFromLeft (4);
    }

    // Lower row
    const int lowY = mainY + mainH + 12, lowH = kHeight - lowY - 16;
    lanePanel     = { 16, lowY, 430, lowH };
    settingsPanel = { 454, lowY, 410, lowH };
    globalPanel   = { 872, lowY, kWidth - 872 - 16, lowH };

    {
        auto& ui = laneUI[(size_t) module];
        auto r = lanePanel.reduced (12).withTrimmedTop (22);
        auto controls = r.removeFromTop (110);
        ui.rate->setBounds (controls.removeFromLeft (100).withHeight (46).translated (0, 24));
        controls.removeFromLeft (10);
        const int kw = (controls.getWidth()) / 3;
        ui.length->setBounds (controls.removeFromLeft (kw));
        ui.depth->setBounds (controls.removeFromLeft (kw));
        ui.smooth->setBounds (controls);

        r.removeFromTop (14);
        auto tools = r.removeFromTop (28);
        const int tw = (tools.getWidth() - 4 * 4) / 5;
        for (auto* b : { &randButton, &clearButton, &invertButton, &shiftLeft, &shiftRight })
        {
            b->setBounds (tools.removeFromLeft (tw));
            tools.removeFromLeft (4);
        }
        r.removeFromTop (8);
        auto mod = r.removeFromTop (28);
        saveModuleButton.setBounds (mod.removeFromLeft (mod.getWidth() / 2 - 2));
        mod.removeFromLeft (4);
        loadModuleButton.setBounds (mod);

        auto s = settingsPanel.reduced (12).withTrimmedTop (30);
        const int n = juce::jmax (1, (int) ui.settings.size());
        const int sw = s.getWidth() / juce::jmax (n, 3);
        for (auto& c : ui.settings)
        {
            auto cell = s.removeFromLeft (sw);
            if (dynamic_cast<Choice*> (c.get()) != nullptr)
                c->setBounds (cell.reduced (4, 0).withHeight (46).translated (0, 30));
            else
                c->setBounds (cell.withHeight (110));
        }
    }

    auto gr = globalPanel.reduced (12).withTrimmedTop (30);
    const int gw = gr.getWidth() / 3;
    swingKnob->setBounds (gr.removeFromLeft (gw).withHeight (110));
    mixKnob->setBounds (gr.removeFromLeft (gw).withHeight (110));
    outKnob->setBounds (gr.withHeight (110));
}
