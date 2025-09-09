/*
  ==============================================================================

    CustomLookAndFeel.h
    Created: 9 Sep 2025 10:27:01am
    Author:  Cody

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

using namespace juce;

//Draws all sliders, combo boxes, labels, and power buttons (bypasses)
struct CustomLookAndFeelA : public juce::LookAndFeel_V4 {
    CustomLookAndFeelA();

	void drawComboBox(Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW, int buttonH, ComboBox&) override;
    void positionComboBoxText(ComboBox& b, Label& l) override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(ComboBox&, Label&);
    //void CustomLookAndFeelA::drawPopupMenuItem(Graphics&, const Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
        //bool isTicked, bool hasSubMenu, const String&, const String& shortcutKeyText, const Drawable*, const Colour*) override;
    void drawToggleButton(Graphics&, ToggleButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawLabel(Graphics&, Label&) override;
};

//Draws delete button
struct CustomLookAndFeelB : public juce::LookAndFeel_V4 {
    CustomLookAndFeelB() = default;
    ~CustomLookAndFeelB() override = default;

    void drawButtonBackground(Graphics&, Button&, const Colour& backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

//Draws Pre/Post eq RTA button
struct CustomLookAndFeelC : public juce::LookAndFeel_V4 {
    CustomLookAndFeelC();

    void drawButtonBackground(Graphics&, Button&, const Colour& backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};