/*
  ==============================================================================

    CustomLookAndFeel.cpp
    Created: 9 Sep 2025 10:27:01am
    Author:  Cody

  ==============================================================================
*/

#include "CustomLookAndFeel.h"
using namespace juce;

CustomLookAndFeelA::CustomLookAndFeelA() {
    setColour(Slider::rotarySliderFillColourId, Colours::limegreen);
    setColour(Slider::thumbColourId, Colours::black);

    setColour(Label::backgroundColourId, Colours::black);
    setColour(Label::textColourId, Colours::white);
    setColour(Label::outlineColourId, Colours::white);

    setColour(TextEditor::backgroundColourId, Colours::black);
    setColour(TextEditor::textColourId, Colours::white);

    setColour(ComboBox::backgroundColourId, Colours::black);
}

void CustomLookAndFeelA::drawComboBox(Graphics& g, int width, int height, bool, int, int, int, int, ComboBox& box) {
    auto area = Rectangle<float>(0, 0, (float)width, (float)height);

    g.setColour(findColour(ComboBox::backgroundColourId));
    g.fillRoundedRectangle(area, 4.0f);

    g.setColour(Colours::limegreen);
    g.drawRoundedRectangle(area, 4.0f, 1.5f);
}

void CustomLookAndFeelA::positionComboBoxText(ComboBox& box, Label& label) {
    label.setBounds(box.getLocalBounds());
    label.setJustificationType(Justification::centred);   
    label.setFont(16.0f);
    label.setColour(Label::backgroundColourId, Colours::transparentBlack);
    label.setColour(Label::outlineColourId, Colours::transparentBlack);
    label.setColour(Label::textColourId, Colours::limegreen);
}

PopupMenu::Options CustomLookAndFeelA::getOptionsForComboBoxPopupMenu(ComboBox& box, Label& label) {
    auto opts = LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label);
    return opts.withMinimumWidth(int(box.getWidth() * 0.7f))
        .withMaximumNumColumns(1)
        .withStandardItemHeight(18);
}

void CustomLookAndFeelA::drawLabel(Graphics& g, Label& label) {
    g.setColour(label.findColour(Label::backgroundColourId));
    g.fillRect(label.getLocalBounds());

    g.setColour(label.findColour(Label::outlineColourId));
    g.drawRect(label.getLocalBounds());

    g.setColour(label.findColour(Label::textColourId));
    g.setFont(Font(14.0f, Font::plain));
    g.drawFittedText(label.getText(), label.getLocalBounds(), Justification::centred, 1);
}

void CustomLookAndFeelA::drawToggleButton(Graphics& g, ToggleButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    const auto bounds = button.getLocalBounds().toFloat();

    bool isBypassed = button.getToggleState();
    if (button.getComponentID() == "analyserOn") {
        isBypassed = !isBypassed;
    }
    Colour bg = isBypassed ? Colours::grey : Colours::green;
    if (shouldDrawButtonAsHighlighted) bg = bg.brighter(0.2f);
    if (shouldDrawButtonAsDown)        bg = bg.darker(0.2f);

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(Colours::black);
    g.drawRoundedRectangle(bounds, 4.0f, 1.5f);

    auto c = bounds.getCentre();
    float r = bounds.getHeight() * 0.3f;

    Path power;
    power.addCentredArc(c.x, c.y, r, r, 0.0f, MathConstants<float>::pi * 0.25f, MathConstants<float>::pi * 1.75f, true);
    power.startNewSubPath(c.x, c.y - r);
    power.lineTo(c.x, c.y);

    g.setColour(Colours::black);
    g.strokePath(power, PathStrokeType(5.0f));
}

void CustomLookAndFeelB::drawButtonBackground(Graphics& g, Button& b, const Colour& backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto bounds = b.getLocalBounds().toFloat();

    Colour base = Colours::red;
    if (shouldDrawButtonAsHighlighted) base = base.brighter(0.2f);
    if (shouldDrawButtonAsDown)        base = base.darker(0.2f);

    g.setColour(base);
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(Colours::black);
    g.drawRoundedRectangle(bounds, 4.0f, 1.5f);

    Path cross;
    float pad = bounds.getWidth() * 0.25f;
    Point<float> p1(bounds.getX() + pad, bounds.getY() + pad);
    Point<float> p2(bounds.getRight() - pad, bounds.getBottom() - pad);
    Point<float> p3(bounds.getRight() - pad, bounds.getY() + pad);
    Point<float> p4(bounds.getX() + pad, bounds.getBottom() - pad);

    cross.startNewSubPath(p1);
    cross.lineTo(p2);
    cross.startNewSubPath(p3);
    cross.lineTo(p4);

    g.strokePath(cross, PathStrokeType(5.0f));
}

void CustomLookAndFeelC::drawButtonBackground(Graphics& g, Button& b, const Colour&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto bounds = b.getLocalBounds().toFloat();

    bool isPost = b.getToggleState();

    Colour base = isPost ? Colours::limegreen : Colours::yellow;

    if (shouldDrawButtonAsHighlighted) base = base.brighter(0.2f);
    if (shouldDrawButtonAsDown)        base = base.darker(0.2f);

    g.setColour(base);
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(Colours::black);
    g.drawRoundedRectangle(bounds, 4.0f, 1.5f);
}

void CustomLookAndFeelC::drawButtonText(Graphics& g, TextButton& button, bool, bool) {
    auto bounds = button.getLocalBounds();
    bool isPost = button.getToggleState();

    String text = isPost ? "POST" : "PRE";

    g.setColour(Colours::black);
    g.setFont(Font(16.0f, Font::bold));
    g.drawFittedText(text, bounds, Justification::centred, 1);
}
