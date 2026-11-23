#include "StepEditor.h"

namespace ss
{
juce::Colour moduleColour (int module)
{
    static const juce::Colour c[] {
        juce::Colour (0xffffa53d),   // Volume
        juce::Colour (0xff3ec9ff),   // Filter
        juce::Colour (0xffff5a5a),   // Distortion
        juce::Colour (0xffb67dff),   // Phaser
        juce::Colour (0xff5be3a6),   // Flanger
        juce::Colour (0xffff7ad9),   // Pitch
        juce::Colour (0xfff2e15c),   // Delay
    };
    return c[juce::jlimit (0, kNumModules - 1, module)];
}

static juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
ShaperLookAndFeel::ShaperLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::bg);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, Colours::panelHi);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::outlineColourId, Colours::line);
    setColour (juce::ComboBox::arrowColourId, Colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::headerTextColourId, Colours::textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::panelHi.brighter (0.2f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, Colours::panelHi);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::AlertWindow::backgroundColourId, Colours::panel);
    setColour (juce::AlertWindow::textColourId, Colours::text);
    setColour (juce::TextEditor::backgroundColourId, Colours::bg);
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::outlineColourId, Colours::line);
}

void ShaperLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float start, float end, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float thick = juce::jmax (2.5f, r * 0.16f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0.0f, start, end, true);
    g.setColour (Colours::line);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0.0f, start, angle, true);
    g.setColour (accent);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = r - thick * 1.6f;
    g.setColour (Colours::panelHi);
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2.0f, inner * 2.0f);
    juce::Point<float> tip (c.x + std::sin (angle) * inner * 0.8f, c.y - std::cos (angle) * inner * 0.8f);
    g.setColour (Colours::text);
    g.drawLine ({ c, tip }, 2.0f);
}

void ShaperLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto col = b.getToggleState() ? base.brighter (0.25f) : base;
    if (down) col = col.brighter (0.2f); else if (over) col = col.brighter (0.08f);
    g.setColour (col);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void ShaperLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    // Round power-style toggle.
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    auto circle = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (b.getToggleState() ? accent : Colours::line.brighter (over ? 0.3f : 0.0f));
    g.fillEllipse (circle);
    g.setColour (Colours::bg);
    g.fillEllipse (circle.reduced (d * 0.3f));
    if (b.getToggleState())
    {
        g.setColour (accent);
        g.fillEllipse (circle.reduced (d * 0.4f));
    }
}

void ShaperLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font ShaperLookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (13.0f); }
juce::Font ShaperLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return uiFont (12.5f, true); }
juce::Font ShaperLookAndFeel::getLabelFont (juce::Label& l) { return uiFont (juce::jmin (13.0f, (float) l.getHeight() * 0.8f)); }

//==============================================================================
StepGrid::StepGrid (StepShaperProcessor& p) : proc (p) {}

void StepGrid::setModule (int m) { module = m; lastEdit = -1; repaint(); }

void StepGrid::setSelectedStep (int s)
{
    selected = juce::jlimit (0, kMaxSteps - 1, s);
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

int StepGrid::length() const
{
    return juce::jlimit (1, kMaxSteps, (int) proc.apvts.getRawParameterValue (StepShaperProcessor::pid (module, "len"))->load());
}

void StepGrid::refresh()
{
    auto& lane = proc.lanes[(size_t) module];
    const int edit = lane.editCounter.load();
    const float play = lane.playPos.load();
    const int len = length();
    if (edit != lastEdit || std::abs (play - lastPlay) > 0.001f || len != lastLen)
    {
        lastEdit = edit; lastPlay = play; lastLen = len;
        repaint();
    }
}

juce::Rectangle<float> StepGrid::gridArea() const { return getLocalBounds().toFloat().reduced (8.0f, 8.0f).withTrimmedBottom (14.0f); }

int StepGrid::stepAt (float x) const
{
    auto a = gridArea();
    return juce::jlimit (0, kMaxSteps - 1, (int) ((x - a.getX()) / a.getWidth() * kMaxSteps));
}

float StepGrid::valueAt (float y) const
{
    auto a = gridArea();
    return juce::jlimit (0.0f, 1.0f, 1.0f - (y - a.getY()) / a.getHeight());
}

void StepGrid::paint (juce::Graphics& g)
{
    const auto accent = moduleColour (module);
    auto& lane = proc.lanes[(size_t) module];
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    auto a = gridArea();
    const float cw = a.getWidth() / (float) kMaxSteps;
    const int len = length();
    const float play = lane.playPos.load();
    const int playStep = play >= 0.0f ? (int) play : -1;

    for (int s = 0; s < kMaxSteps; ++s)
    {
        auto cell = juce::Rectangle<float> (a.getX() + cw * (float) s, a.getY(), cw, a.getHeight()).reduced (1.0f, 0.0f);
        const bool active = s < len;
        g.setColour (((s / 4) % 2 == 0) ? Colours::panelHi : Colours::panelHi.darker (0.15f));
        g.fillRect (cell);

        if (s == playStep)
        {
            g.setColour (accent.withAlpha (0.14f));
            g.fillRect (cell);
        }

        // Envelope curve
        juce::Path fill, stroke;
        const int pts = 24;
        for (int i = 0; i <= pts; ++i)
        {
            const float p = (float) i / (float) pts;
            const float v = lane.evaluate (s, p);
            const float px = cell.getX() + p * cell.getWidth();
            const float py = cell.getBottom() - v * cell.getHeight();
            if (i == 0) { fill.startNewSubPath (px, cell.getBottom()); stroke.startNewSubPath (px, py); }
            fill.lineTo (px, py);
            if (i > 0) stroke.lineTo (px, py);
        }
        fill.lineTo (cell.getRight(), cell.getBottom());
        fill.closeSubPath();

        const float alpha = active ? 1.0f : 0.25f;
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.55f * alpha), cell.getX(), cell.getY(),
                                                 accent.withAlpha (0.12f * alpha), cell.getX(), cell.getBottom(), false));
        g.fillPath (fill);
        g.setColour (accent.withAlpha (alpha));
        g.strokePath (stroke, juce::PathStrokeType (1.5f));

        if (s == selected)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawRect (cell, 1.5f);
        }

        // Step numbers
        g.setColour (active ? Colours::textDim : Colours::textDim.withAlpha (0.35f));
        g.setFont (uiFont (10.0f));
        g.drawText (juce::String (s + 1), juce::Rectangle<float> (cell.getX(), a.getBottom() + 1.0f, cell.getWidth(), 12.0f),
                    juce::Justification::centred);
    }

    // Loop end marker
    {
        const float x = a.getX() + cw * (float) len;
        g.setColour (accent);
        g.fillRect (x - 1.0f, a.getY(), 2.0f, a.getHeight());
    }

    // Playhead
    if (play >= 0.0f)
    {
        const float x = a.getX() + cw * play;
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillRect (x - 0.75f, a.getY(), 1.5f, a.getHeight());
    }
}

void StepGrid::paintStep (int s, const juce::MouseEvent& e)
{
    auto& lane = proc.lanes[(size_t) module];
    const float v = valueAt ((float) e.position.y);
    lane.values[(size_t) s].store (v);
    if (e.mods.isCommandDown() && getBrush != nullptr && getBrush() >= 0)
        lane.setShape (s, getBrush());
    ++lane.editCounter;
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int s = stepAt ((float) e.position.x);
    if (e.mods.isPopupMenu())
    {
        setSelectedStep (s);
        showMenu (s);
        return;
    }
    auto& lane = proc.lanes[(size_t) module];
    if (e.mods.isAltDown())
    {
        // Alt-click toggles a step fully on/off.
        lane.values[(size_t) s].store (lane.values[(size_t) s].load() > 0.5f ? 0.0f : 1.0f);
        ++lane.editCounter;
        lastStep = -1;
    }
    else
    {
        paintStep (s, e);
        lastStep = s;
        lastValue = valueAt ((float) e.position.y);
    }
    setSelectedStep (s);
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || lastStep < 0) return;
    const int s = stepAt ((float) e.position.x);
    const float v = valueAt ((float) e.position.y);
    auto& lane = proc.lanes[(size_t) module];

    // Fill any steps skipped over by a fast drag.
    const int lo = juce::jmin (s, lastStep), hi = juce::jmax (s, lastStep);
    for (int i = lo; i <= hi; ++i)
    {
        const float t = (hi == lo) ? 1.0f : (float) (i - lastStep) / (float) (s - lastStep);
        lane.values[(size_t) i].store (juce::jlimit (0.0f, 1.0f, lastValue + (v - lastValue) * t));
        if (e.mods.isCommandDown() && getBrush != nullptr && getBrush() >= 0)
            lane.setShape (i, getBrush());
    }
    ++lane.editCounter;
    lastStep = s; lastValue = v;
    if (s != selected) setSelectedStep (s);
}

void StepGrid::mouseUp (const juce::MouseEvent&) { lastStep = -1; }

void StepGrid::showMenu (int step)
{
    juce::PopupMenu menu, all;
    menu.addSectionHeader ("Step " + juce::String (step + 1));
    for (int i = 0; i < NumShapes; ++i)
    {
        menu.addItem (100 + i, shapeNames()[i], true, proc.lanes[(size_t) module].shapeIds[(size_t) step].load() == i);
        all.addItem (200 + i, shapeNames()[i]);
    }
    menu.addSeparator();
    menu.addSubMenu ("Apply shape to all steps", all);
    menu.addItem (1, "Level 100%");
    menu.addItem (2, "Level 50%");
    menu.addItem (3, "Level 0% (mute step)");
    menu.addItem (4, "Set loop length to here");

    juce::Component::SafePointer<StepGrid> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
        [safe, step] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& self = *safe;
            auto& lane = self.proc.lanes[(size_t) self.module];
            if (r >= 200) for (int s = 0; s < kMaxSteps; ++s) lane.setShape (s, r - 200);
            else if (r >= 100) lane.setShape (step, r - 100);
            else if (r <= 3) lane.values[(size_t) step].store (r == 1 ? 1.0f : (r == 2 ? 0.5f : 0.0f));
            else if (r == 4)
                if (auto* lp = self.proc.apvts.getParameter (StepShaperProcessor::pid (self.module, "len")))
                    lp->setValueNotifyingHost (lp->convertTo0to1 ((float) (step + 1)));
            ++lane.editCounter;
            if (self.onSelectionChanged) self.onSelectionChanged();
        });
}

//==============================================================================
ShapeEditor::ShapeEditor (StepShaperProcessor& p) : proc (p) {}

void ShapeEditor::setTarget (int m, int s) { module = m; step = s; repaint(); }

void ShapeEditor::refresh()
{
    const int edit = proc.lanes[(size_t) module].editCounter.load();
    if (edit != lastEdit) { lastEdit = edit; repaint(); }
}

juce::Rectangle<float> ShapeEditor::area() const { return getLocalBounds().toFloat().reduced (10.0f).withTrimmedTop (18.0f); }

void ShapeEditor::paint (juce::Graphics& g)
{
    const auto accent = moduleColour (module);
    auto& lane = proc.lanes[(size_t) module];
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    const int id = lane.shapeIds[(size_t) step].load();
    g.setColour (Colours::textDim);
    g.setFont (uiFont (12.0f, true));
    g.drawText ("STEP " + juce::String (step + 1) + "  -  " + (id >= 0 ? shapeNames()[id].toUpperCase() : juce::String ("CUSTOM")),
                getLocalBounds().reduced (10, 4).removeFromTop (16), juce::Justification::centredLeft);

    auto a = area();
    g.setColour (Colours::panelHi);
    g.fillRect (a);
    g.setColour (Colours::line);
    for (int i = 1; i < 4; ++i)
    {
        g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * (float) i / 4.0f), a.getX(), a.getRight());
        g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) i / 4.0f), a.getY(), a.getBottom());
    }

    // Curve (unscaled by level), then the effective curve (scaled by level).
    juce::Path curve, fill;
    for (int i = 0; i < kCurvePoints; ++i)
    {
        const float x = a.getX() + a.getWidth() * (float) i / (float) (kCurvePoints - 1);
        const float y = a.getBottom() - lane.curves[(size_t) step][(size_t) i].load() * a.getHeight();
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, a.getBottom()); }
        else curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (a.getRight(), a.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.45f), a.getX(), a.getY(),
                                             accent.withAlpha (0.05f), a.getX(), a.getBottom(), false));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    for (int i = 0; i < kCurvePoints; ++i)
    {
        const float x = a.getX() + a.getWidth() * (float) i / (float) (kCurvePoints - 1);
        const float y = a.getBottom() - lane.curves[(size_t) step][(size_t) i].load() * a.getHeight();
        g.fillEllipse (x - 2.0f, y - 2.0f, 4.0f, 4.0f);
    }

    const float level = lane.values[(size_t) step].load();
    g.setColour (Colours::textDim);
    g.setFont (uiFont (11.0f));
    g.drawText ("level " + juce::String (juce::roundToInt (level * 100.0f)) + "%", a.reduced (4.0f), juce::Justification::topRight);

    // Playhead inside this step
    const float play = lane.playPos.load();
    if (play >= 0.0f && (int) play == step)
    {
        const float x = a.getX() + a.getWidth() * (play - (float) step);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillRect (x - 0.75f, a.getY(), 1.5f, a.getHeight());
    }
}

void ShapeEditor::drawAt (juce::Point<float> pos, bool interpolate)
{
    auto a = area();
    const int pt = juce::jlimit (0, kCurvePoints - 1, juce::roundToInt ((pos.x - a.getX()) / a.getWidth() * (float) (kCurvePoints - 1)));
    const float v = juce::jlimit (0.0f, 1.0f, (a.getBottom() - pos.y) / a.getHeight());
    auto& lane = proc.lanes[(size_t) module];
    if (interpolate && lastPoint >= 0 && lastPoint != pt)
    {
        const int dir = pt > lastPoint ? 1 : -1;
        for (int i = lastPoint; i != pt; i += dir)
        {
            const float t = (float) (i - lastPoint) / (float) (pt - lastPoint);
            lane.setCurvePoint (step, i, lastVal + (v - lastVal) * t);
        }
    }
    lane.setCurvePoint (step, pt, v);
    lastPoint = pt; lastVal = v;
}

void ShapeEditor::mouseDown (const juce::MouseEvent& e) { lastPoint = -1; drawAt (e.position, false); }
void ShapeEditor::mouseDrag (const juce::MouseEvent& e) { drawAt (e.position, true); }

//==============================================================================
ShapeLibrary::ShapeLibrary() { setRepaintsOnMouseActivity (false); }

juce::Rectangle<float> ShapeLibrary::cell (int i) const
{
    const int cols = 7;
    const float w = (float) getWidth() / (float) cols;
    const float h = (float) getHeight() / 2.0f;
    return { (float) (i % cols) * w, (float) (i / cols) * h, w, h };
}

int ShapeLibrary::cellAt (juce::Point<float> p) const
{
    for (int i = 0; i < NumShapes; ++i)
        if (cell (i).contains (p)) return i;
    return -1;
}

void ShapeLibrary::paint (juce::Graphics& g)
{
    for (int i = 0; i < NumShapes; ++i)
    {
        auto c = cell (i).reduced (2.0f);
        g.setColour (i == brush ? Colours::panelHi.brighter (0.15f) : (i == hover ? Colours::panelHi.brighter (0.05f) : Colours::panelHi));
        g.fillRoundedRectangle (c, 4.0f);
        if (i == brush)
        {
            g.setColour (accent);
            g.drawRoundedRectangle (c, 4.0f, 1.5f);
        }
        auto icon = c.reduced (6.0f, 6.0f);
        juce::Path p;
        for (int k = 0; k <= 30; ++k)
        {
            const float t = (float) k / 30.0f;
            const float x = icon.getX() + t * icon.getWidth();
            const float y = icon.getBottom() - libraryShape (i, juce::jmin (t, 0.999f)) * icon.getHeight();
            if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (i == brush ? accent : Colours::text.withAlpha (0.8f));
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }
}

void ShapeLibrary::mouseDown (const juce::MouseEvent& e)
{
    const int i = cellAt (e.position);
    if (i < 0) return;
    brush = i;
    repaint();
    if (onShapeChosen) onShapeChosen (i, e.mods.isShiftDown());
}

void ShapeLibrary::mouseMove (const juce::MouseEvent& e)
{
    const int i = cellAt (e.position);
    if (i != hover)
    {
        hover = i;
        setTooltip (i >= 0 ? shapeNames()[i] + "  (click: selected step, shift-click: all steps)" : juce::String());
        repaint();
    }
}

void ShapeLibrary::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

//==============================================================================
ModuleList::ModuleList (StepShaperProcessor& p) : proc (p)
{
    for (int m = 0; m < kNumModules; ++m)
    {
        auto& b = power[(size_t) m];
        b.setColour (juce::ToggleButton::tickColourId, moduleColour (m));
        b.setTooltip ("Turn " + juce::String (moduleName (m)) + " on/off");
        addAndMakeVisible (b);
        attach[(size_t) m] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, StepShaperProcessor::pid (m, "on"), b);
    }
}

juce::Rectangle<int> ModuleList::row (int m) const
{
    const int h = getHeight() / kNumModules;
    return { 0, m * h, getWidth(), h };
}

void ModuleList::resized()
{
    for (int m = 0; m < kNumModules; ++m)
        power[(size_t) m].setBounds (row (m).reduced (6, 0).removeFromRight (22).withSizeKeepingCentre (20, 20));
}

void ModuleList::paint (juce::Graphics& g)
{
    for (int m = 0; m < kNumModules; ++m)
    {
        auto r = row (m).reduced (0, 2).toFloat();
        const auto accent = moduleColour (m);
        const bool on = power[(size_t) m].getToggleState();
        g.setColour (m == selected ? Colours::panelHi.brighter (0.1f) : Colours::panel);
        g.fillRoundedRectangle (r, 6.0f);
        if (m == selected)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (r.withWidth (4.0f), 2.0f);
        }

        g.setColour (on ? Colours::text : Colours::textDim);
        g.setFont (uiFont (14.0f, m == selected));
        g.drawText (moduleName (m), r.withTrimmedLeft (14.0f).withTrimmedRight (30.0f).withTrimmedBottom (10.0f),
                    juce::Justification::centredLeft);

        // Activity meter
        auto meter = r.withTrimmedLeft (14.0f).withTrimmedRight (34.0f).removeFromBottom (12.0f).withHeight (4.0f);
        g.setColour (Colours::line);
        g.fillRoundedRectangle (meter, 2.0f);
        const float env = juce::jlimit (0.0f, 1.0f, proc.lanes[(size_t) m].envOut.load());
        if (on)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * env), 2.0f);
        }
    }
}

void ModuleList::mouseDown (const juce::MouseEvent& e)
{
    for (int m = 0; m < kNumModules; ++m)
        if (row (m).contains (e.getPosition()))
        {
            selected = m;
            repaint();
            if (onSelect) onSelect (m);
        }
}

} // namespace ss
