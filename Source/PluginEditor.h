/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"

//==============================================================================
/**
*/
class ProceduralEqAudioProcessorEditor;

enum {
    fftOrder = 11,
    fftSize = 1 << fftOrder,
    scopeSize = 512
};

constexpr int TOOLTIP_DELAY = 200;  //milliseconds
constexpr int TIMER_FPS = 30;       //hz

static auto makeSquareForSlider(juce::Rectangle<int> area) {
    auto knobArea = area.removeFromTop(area.getHeight() - 15);
    int side = std::min(knobArea.getWidth(), knobArea.getHeight());
    return knobArea.withSizeKeepingCentre(side, side);
}

//==============================================================================
/**
*/
struct GainComponent : juce::Component {
    GainComponent(ProceduralEqAudioProcessor&);
    ~GainComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ProceduralEqAudioProcessor& audioProcessor;

    juce::Rectangle<int> gainCanvas;
    juce::Slider preGainSlider, postGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preGainAttachment, postGainAttachment;
    juce::Label preGainLabel, postGainLabel;
};

//==============================================================================
/**
*/
struct SpectrumAnalyser : juce::Component, juce::Timer {
    SpectrumAnalyser(ProceduralEqAudioProcessor&, ProceduralEqAudioProcessorEditor&);
    ~SpectrumAnalyser();

    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void drawNextFrameOfSpectrum();

    juce::Colour lineColor;

private:
    ProceduralEqAudioProcessor& audioProcessor;
    ProceduralEqAudioProcessorEditor& editor;
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    float fftData[2 * fftSize]{};
    float scopeData[scopeSize]{};
};

//==============================================================================
/**
*/
struct SelectedEqComponent : juce::Component {
    SelectedEqComponent(ProceduralEqAudioProcessor&, int id);
    ~SelectedEqComponent();

    void updateEqAndSliders(int id);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    auto makeSquare(juce::Rectangle<int> area) {
        int side = std::min(area.getWidth(), area.getHeight());
        return area.withSizeKeepingCentre(side, side);
    }

    ProceduralEqAudioProcessor& audioProcessor;
    int labelHeight = 20;
    int textboxHeight = 15;
    int currEq;
    juce::Slider freqSlider, gainSlider, qualitySlider;
    juce::ComboBox typeComboBox;
    juce::ToggleButton bypassButton;
    juce::TextButton deleteButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqSliderAttachment, gainSliderAttachment, qualitySliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeBoxAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassButtonAttachment;
    juce::Label freqLabel, gainLabel, qualityLabel, typeLabel, bypassLabel, deleteLabel;
    CustomLookAndFeelB lnfb;
};

//==============================================================================
/**
*/
struct ResponseCurveComponent : juce::Component, private juce::AudioProcessorValueTreeState::Listener {
    ResponseCurveComponent(ProceduralEqAudioProcessor&, ProceduralEqAudioProcessorEditor&);
    ~ResponseCurveComponent();

    void paint(juce::Graphics& g) override;

private:
    void parameterChanged(const juce::String& paramID, float newValue) override;

    ProceduralEqAudioProcessor& audioProcessor;
    ProceduralEqAudioProcessorEditor& editor;
};

//==============================================================================
/**
*/
struct DraggableButton : juce::Button, private juce::AudioProcessorValueTreeState::Listener {
    DraggableButton(ProceduralEqAudioProcessor&, ProceduralEqAudioProcessorEditor&, int eqId);
    ~DraggableButton();

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    bool hitTest(int x, int y) override;
    void updateParamsFromPosition();
    void updatePositionFromParams();
    void updateTooltip();

private:
    void parameterChanged(const juce::String& paramID, float newValue) override;
    void setCentreFromFreq(float freq);
    void setCentreFromGain(float gain);

    ProceduralEqAudioProcessor& audioProcessor;
    ProceduralEqAudioProcessorEditor& editor;
    juce::Colour circleColour;
    int associatedEq;
    juce::ComponentDragger dragger;
    bool isBypassed = false;
};

//==============================================================================
/**
*/
class ProceduralEqAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    ProceduralEqAudioProcessorEditor(ProceduralEqAudioProcessor&);
    ~ProceduralEqAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    juce::Rectangle<int> getRenderArea();
    void setSelectedEq(int id);
    juce::Rectangle<int> backgroundImage();

    juce::Rectangle<int> responseCurveBounds;
    int selectedEq;
    juce::Rectangle<int> buttonBounds;
    void secVisiblityCheck();
    void buttonReset(int id);

private:
    ProceduralEqAudioProcessor& audioProcessor;
    SpectrumAnalyser analyser;
    ResponseCurveComponent rcc;
    juce::OwnedArray<DraggableButton> buttonArr;
    SelectedEqComponent selectedEqComponent;
    GainComponent gainComponent;

    juce::Image background;
    juce::TooltipWindow tooltipWindow{ this, TOOLTIP_DELAY };

    juce::ToggleButton analyserOnButton;
    juce::TextButton analyserModeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> analyserOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> analyserModeAttachment;

    CustomLookAndFeelA lnfa;
    CustomLookAndFeelC lnfc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProceduralEqAudioProcessorEditor)
};
