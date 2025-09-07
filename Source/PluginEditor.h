/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class ProceduralEqAudioProcessorEditor;

enum {
    fftOrder = 11,
    fftSize = 1 << fftOrder,
    scopeSize = 512
};

const int TOOLTIP_DELAY = 200; //this is in milliseconds
const int TIMER_FPS = 30;

//==============================================================================
struct SpectrumAnalyser : juce::Component {
    SpectrumAnalyser(ProceduralEqAudioProcessor&, ProceduralEqAudioProcessorEditor&);
    ~SpectrumAnalyser();

    void onEditorTimer();
    void paint(juce::Graphics& g) override;
    void drawNextFrameOfSpectrum();

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
    ProceduralEqAudioProcessor& audioProcessor;
    int currEq;
    juce::Slider freqSlider, gainSlider, qualitySlider;
    juce::ComboBox typeComboBox;
    juce::ToggleButton bypassButton{ "BYPASS" };
    juce::ToggleButton deleteButton{ "DELETE" };  //change to different button style eventually
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqSliderAttachment, gainSliderAttachment, qualitySliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeBoxAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassButtonAttachment;
    juce::Rectangle<int> rect;
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
    float getCurrGain() {
        auto* val = audioProcessor.tree.getRawParameterValue(params[1 + associatedEq * 6]);
        if (val != nullptr) {
            return *val;
        }
        else {
            return 0;
        }
    }
    float getCurrFreq() {
        auto* val = audioProcessor.tree.getRawParameterValue(params[0 + associatedEq * 6]);
        if (val != nullptr) {
            return *val;
        }
        else {
            return 0;
        }
    }
    void resetEq();
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
class ProceduralEqAudioProcessorEditor : public juce::AudioProcessorEditor, juce::Timer {
public:
    ProceduralEqAudioProcessorEditor(ProceduralEqAudioProcessor&);
    ~ProceduralEqAudioProcessorEditor() override;

    //==============================================================================
    void timerCallback() override;
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
    juce::Image background;

    juce::TooltipWindow tooltipWindow{ this, TOOLTIP_DELAY };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProceduralEqAudioProcessorEditor)
};