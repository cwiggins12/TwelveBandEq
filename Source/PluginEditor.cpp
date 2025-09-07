/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::Array<juce::Colour> colours = { juce::Colours::red, juce::Colours::darkorange, juce::Colours::yellow, juce::Colours::green, juce::Colours::blue, juce::Colours::indigo, 
                         juce::Colours::violet, juce::Colours::darkgoldenrod, juce::Colours::pink, juce::Colours::olive, juce::Colours::beige, juce::Colours::crimson};

//==============================================================================
/**
*/
SpectrumAnalyser::SpectrumAnalyser(ProceduralEqAudioProcessor& p, ProceduralEqAudioProcessorEditor& e) : audioProcessor(p), editor(e), 
                                   forwardFFT(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hann) {
    setInterceptsMouseClicks(false, false);
    setOpaque(false);
}

SpectrumAnalyser::~SpectrumAnalyser() {}

void SpectrumAnalyser::onEditorTimer() {
    auto* fifo = audioProcessor.getAnalyserFifo();
    if (!fifo)
        return;

    float temp[fftSize];
    const int numRead = fifo->pop(temp, fftSize);

    if (numRead > 0) {
        juce::zeromem(fftData, sizeof(fftData));

        // Only copy the actual data we received
        int samplesToCopy = juce::jmin(numRead, static_cast<int>(fftSize));
        std::copy(temp, temp + samplesToCopy, fftData);

        // Zero-pad if we didn't get enough samples
        if (samplesToCopy < fftSize) {
            std::fill(fftData + samplesToCopy, fftData + fftSize, 0.0f);
        }

        window.multiplyWithWindowingTable(fftData, fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform(fftData);

        drawNextFrameOfSpectrum();
        repaint();
    }
}

void SpectrumAnalyser::drawNextFrameOfSpectrum() {
    auto mindB = -100.0f;
    auto maxdB = 24.0f;

    for (int i = 0; i < scopeSize; ++i) {
        auto skewedProportionX = 1.0f - std::exp(std::log(1.0 - (double)i / (double)scopeSize) * 0.2);
        auto fftIndex = juce::jlimit(0, fftSize / 2, (int)(skewedProportionX * (double)(fftSize / 2)));

        auto level = juce::jmap(juce::Decibels::gainToDecibels(fftData[fftIndex])
            - maxdB,
            mindB, maxdB, 0.0f, 1.0f);

        scopeData[i] = juce::jlimit(0.0f, 1.0f, level);
    }
}

void SpectrumAnalyser::drawFrame(juce::Graphics& g) {
    g.setColour(juce::Colours::lime);
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    juce::Path p;
    auto mapY = [h](float v) { return juce::jmap(v, 0.0f, 1.0f, h, 0.0f); };

    p.startNewSubPath(0.0f, mapY(scopeData[0]));
    for (int i = 1; i < scopeSize; ++i)
        p.lineTo(juce::jmap((float)i, 0.0f, (float)scopeSize - 1.0f, 0.0f, w),
            mapY(scopeData[i]));

    g.strokePath(p, juce::PathStrokeType(2.0f));
}

void SpectrumAnalyser::paint(juce::Graphics& g) {
    drawFrame(g);
}

//==============================================================================
/**
*/
SelectedEqComponent::SelectedEqComponent(ProceduralEqAudioProcessor& p, int id) : audioProcessor(p), currEq(id) {
    setTopLeftPosition(450, 500);
    setSize(300, 150);

    addAndMakeVisible(freqSlider);
    freqSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    freqSliderAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(audioProcessor.tree, params[0 + id * 6], freqSlider));

    addAndMakeVisible(gainSlider);
    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSliderAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(audioProcessor.tree, params[1 + id * 6], gainSlider));

    addAndMakeVisible(qualitySlider);
    qualitySlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    qualitySliderAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(audioProcessor.tree, params[2 + id * 6], qualitySlider));

    addAndMakeVisible(typeComboBox);
    typeComboBox.addItem("BandPass", 1);
    typeComboBox.addItem("HighPass", 2);
    typeComboBox.addItem("LowPass", 3);
    typeComboBox.addItem("HighShelf", 4);
    typeComboBox.addItem("LowShelf", 5);
    typeBoxAttachment.reset(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(audioProcessor.tree, params[3 + id * 6], typeComboBox));

    addAndMakeVisible(bypassButton);
    bypassButtonAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(audioProcessor.tree, params[4 + id * 6], bypassButton));

    addAndMakeVisible(deleteButton);
    deleteButton.onClick = [this] {
        if (currEq >= 0) {
            if (auto* editor = dynamic_cast<ProceduralEqAudioProcessorEditor*>(getParentComponent())) {
                editor->buttonReset(currEq);
            }
        }
    };
}

SelectedEqComponent::~SelectedEqComponent() {}

void SelectedEqComponent::updateEqAndSliders(int id) {
    currEq = id;

    freqSliderAttachment.reset();
    gainSliderAttachment.reset();
    qualitySliderAttachment.reset();
    typeBoxAttachment.reset();
    bypassButtonAttachment.reset();

    freqSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.tree, params[0 + currEq * 6], freqSlider);
    gainSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.tree, params[1 + currEq * 6], gainSlider);
    qualitySliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.tree, params[2 + currEq * 6], qualitySlider);
    typeBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.tree, params[3 + currEq * 6], typeComboBox);
    bypassButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.tree, params[4 + currEq * 6], bypassButton);
    deleteButton.setToggleState(false, juce::NotificationType::dontSendNotification);
}

void SelectedEqComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::grey);
}

void SelectedEqComponent::resized() {
    auto bounds = getLocalBounds();
    auto w = bounds.getWidth() / 3;
    auto top = bounds.removeFromTop(75);
    freqSlider.setBounds(top.removeFromLeft(w));
    qualitySlider.setBounds(top.removeFromRight(w));
    gainSlider.setBounds(top);
    typeComboBox.setBounds(bounds.removeFromLeft(w));
    deleteButton.setBounds(bounds.removeFromRight(w));
    bypassButton.setBounds(bounds);
}

//==============================================================================
/**
*/
ResponseCurveComponent::ResponseCurveComponent(ProceduralEqAudioProcessor& p, ProceduralEqAudioProcessorEditor& e) : audioProcessor(p), editor(e) {
    setInterceptsMouseClicks(false, false);
    //startTimerHz(30);
}

ResponseCurveComponent::~ResponseCurveComponent() {}

////gonna need to put the timer on the editor to account for the rta 
//void ResponseCurveComponent::timerCallback() {
//    repaint();
//}

//this draws the response curve. I hate this. eventually this needs to be changed to check the bandwidth of each eq and only have to check the pixels within that bandwidth. oughta be way more efficient
//need to add a function to paint all init'd buttons, set their centre at the mapped x and y according to gain and freqency
void ResponseCurveComponent::paint(juce::Graphics& g) {
    using namespace juce;

    auto responseArea = getLocalBounds();
    const int w = responseArea.getWidth();
    if (w <= 0)
        return;
    auto sampleRate = audioProcessor.getSampleRate();
    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    std::vector<double> mags;
    mags.resize(w);

    for (int i = 0; i < w; ++i) {
        double mag = 1.0f;
        double frac = double(i) / double(w - 1);
        auto freq = mapToLog10(frac, 20.0, 20000.0);
        for (int j = 0; j < MAX_EQS; ++j) {
            auto* isInit = audioProcessor.tree.getRawParameterValue(params[5 + j * 6]);
            if (isInit && *isInit >= 0.5f) {
                auto* isBypassed = audioProcessor.tree.getRawParameterValue(params[4 + j * 6]);
                if (isBypassed && *isBypassed < 0.5f) {
                    mag *= audioProcessor.filters[j].state->getMagnitudeForFrequency(freq, sampleRate);
                }
            }
        }
        mags[i] = Decibels::gainToDecibels(mag);
    }

    Path responseCurve;

    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    auto map = [outputMin, outputMax](double input) {
        return jmap(input, -72.0, 24.0, outputMin, outputMax);
    };

    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));

    for (size_t i = 1; i < mags.size(); ++i)
        responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));

    g.setColour(Colours::orange);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.0f, 1.0f);

    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.0f));
}

//==============================================================================
/**
*/
DraggableButton::DraggableButton(ProceduralEqAudioProcessor& p, ProceduralEqAudioProcessorEditor& e, int eqId) :
                                 Button(juce::String()), audioProcessor(p), editor(e), associatedEq(eqId), circleColour(colours[eqId]) {
    setSize(20, 20);

    audioProcessor.tree.addParameterListener(params[0 + eqId * 6], this); //freq
    audioProcessor.tree.addParameterListener(params[1 + eqId * 6], this); //gain
    audioProcessor.tree.addParameterListener(params[4 + eqId * 6], this); //bypass
}

DraggableButton::~DraggableButton() {
    audioProcessor.tree.removeParameterListener(params[0 + associatedEq * 6], this);
    audioProcessor.tree.removeParameterListener(params[1 + associatedEq * 6], this);
    audioProcessor.tree.removeParameterListener(params[4 + associatedEq * 6], this);
}

void DraggableButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    if (isBypassed)
        g.setColour(juce::Colours::grey);
    else
        g.setColour(circleColour);
    auto bounds = getLocalBounds().toFloat();
    g.fillEllipse(bounds);

    if (editor.selectedEq == associatedEq) {
        g.setColour(juce::Colours::white);
        g.drawEllipse(bounds.reduced(2.0f), 2.0f);
    }
}

void DraggableButton::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isLeftButtonDown()) {
        editor.setSelectedEq(associatedEq);
    }
    if (event.mods.isRightButtonDown()) {
        resetEq();
    }
    dragger.startDraggingComponent(this, event);
}

void DraggableButton::mouseDrag(const juce::MouseEvent& event) {
    dragger.dragComponent(this, event, nullptr);

    auto x = juce::jlimit(editor.buttonBounds.getX(), editor.buttonBounds.getRight(), getX());
    auto y = juce::jlimit(editor.buttonBounds.getY(), editor.buttonBounds.getBottom(), getY());

    setCentrePosition(x, y);
    updateParamsFromPosition();
}

void DraggableButton::updateParamsFromPosition() {
    auto centre = getBounds().getCentre();

    float xNorm = (centre.getX() - editor.buttonBounds.getX()) / float(editor.buttonBounds.getWidth());
    float yNorm = (centre.getY() - editor.buttonBounds.getY()) / float(editor.buttonBounds.getHeight());

    auto freqRange = logRange<float>(20.0f, 20000.0f);
    float freq = freqRange.convertFrom0to1(xNorm);
    float freqNorm = freqRange.convertTo0to1(freq);

    juce::NormalisableRange<float> gainRange(-72.0f, 12.0f);
    float gain = juce::jmap(1 - yNorm, 0.0f, 1.0f, -72.0f, 12.0f);
    float gainNorm = gainRange.convertTo0to1(gain);

    audioProcessor.updateParameter(associatedEq, 0, freqNorm);
    audioProcessor.updateParameter(associatedEq, 1, gainNorm);
    updateTooltip();
}

void DraggableButton::updatePositionFromParams() {
    setCentreFromFreq(getCurrFreq());
    setCentreFromGain(getCurrGain());
}

//may wanna extend radius a bit so user doesn't have to click exactly in it
bool DraggableButton::hitTest(int x, int y) {
    auto centre = getLocalBounds().getCentre();
    auto radius = getLocalBounds().getWidth() / 2.0f;
    return centre.getDistanceFrom({ x, y }) <= radius;
}

void DraggableButton::parameterChanged(const juce::String& paramID, float newValue) {
    if (editor.selectedEq != associatedEq)
        return; // ignore param changes for unselected buttons

    if (paramID == params[0 + associatedEq * 6])
        setCentreFromFreq(newValue);
    else if (paramID == params[1 + associatedEq * 6])
        setCentreFromGain(newValue);
    else if (paramID == params[4 + associatedEq * 6]) {
        isBypassed = newValue >= 0.5f;
        repaint();
    }
    updateTooltip();
}

void DraggableButton::setCentreFromFreq(float freq) {
    auto range = logRange<float>(20.0f, 20000.0f);
    float norm = range.convertTo0to1(freq);
    int x = editor.buttonBounds.getX() + norm * editor.buttonBounds.getWidth();

    setCentrePosition(x, getBounds().getCentreY());
}

void DraggableButton::setCentreFromGain(float gain) {
    int y = juce::jmap(gain, -72.0f, 12.0f, float(editor.buttonBounds.getBottom()), float(editor.buttonBounds.getY()));

    setCentrePosition(getBounds().getCentreX(), y);
}

void DraggableButton::resetEq() {
    audioProcessor.updateParameter(associatedEq, 2, 0.1f);  //set quality to 1
    audioProcessor.updateParameter(associatedEq, 3, 0);     //set type to peak
    audioProcessor.updateParameter(associatedEq, 4, 1);     //set bypass to true
    audioProcessor.updateParameter(associatedEq, 5, 0);     //set init to false
    setVisible(false);
    editor.secVisiblityCheck();
}

void DraggableButton::updateTooltip() { //needs work I think
    auto* freq = audioProcessor.tree.getRawParameterValue(params[0 + associatedEq * 6]);
    auto* gain = audioProcessor.tree.getRawParameterValue(params[1 + associatedEq * 6]);

    if (freq && gain) {

        juce::String tip;
        tip << juce::String(*freq, 1) << " Hz, "
            << juce::String(*gain, 1) << " dB";
        setTooltip(tip);
    }
    else {
        setTooltip({});
    }
}


//==============================================================================
/**
*/
ProceduralEqAudioProcessorEditor::ProceduralEqAudioProcessorEditor(ProceduralEqAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), selectedEqComponent(audioProcessor, 0), rcc(audioProcessor, *this), analyser(audioProcessor, *this) {
    setSize(1200, 675);
    buttonBounds = backgroundImage();
    auto area = getRenderArea();
    analyser.setBounds(area);
    rcc.setBounds(area);
    addAndMakeVisible(analyser);
    addAndMakeVisible(rcc);
    int selectedEq = -1;

    for (int i = 0; i < MAX_EQS; ++i) {
        buttonArr.add(new DraggableButton(audioProcessor, *this, i));
        addChildComponent(buttonArr[i]);
    }
    addChildComponent(selectedEqComponent);
    startTimerHz(30); // may want to allow user to define this at some point
}

ProceduralEqAudioProcessorEditor::~ProceduralEqAudioProcessorEditor() {}

//==============================================================================
void ProceduralEqAudioProcessorEditor::timerCallback() {
    analyser.onEditorTimer();
    rcc.repaint();
}

void ProceduralEqAudioProcessorEditor::paint(juce::Graphics& g) {
    g.drawImage(background, getLocalBounds().toFloat());
}

void ProceduralEqAudioProcessorEditor::resized() {
    buttonBounds = backgroundImage();
    auto area = getRenderArea();
    analyser.setBounds(area);
    rcc.setBounds(area);
}

void ProceduralEqAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& event) {
    auto mousePos = event.getPosition();
    if (!buttonBounds.contains(mousePos))
        return;

    for (int i = 0; i < MAX_EQS; ++i) {
        bool isInit = *audioProcessor.tree.getRawParameterValue(params[5 + i * 6]);
        if (!isInit) {
            buttonArr[i]->setCentrePosition(mousePos);
            buttonArr[i]->updateParamsFromPosition();
            audioProcessor.updateParameter(i, 5, 1);
            audioProcessor.updateParameter(i, 4, 0);
            setSelectedEq(i);
            buttonArr[i]->setVisible(true);
            return;
        }
    }
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Maximum EQ Limit Reached", "The current limit of equalizers is 12.");
}

void ProceduralEqAudioProcessorEditor::setSelectedEq(int id) {
    if (selectedEq > -1 && selectedEq < buttonArr.size())
        buttonArr[selectedEq]->repaint();

    selectedEq = id;
    if (selectedEq > -1 && selectedEq < buttonArr.size()) {
        selectedEqComponent.updateEqAndSliders(id);
        selectedEqComponent.setVisible(true);
        buttonArr[selectedEq]->repaint();
    }
    else {
        selectedEqComponent.setVisible(false);
    }
}

void ProceduralEqAudioProcessorEditor::secVisiblityCheck() {
    for (int i = MAX_EQS - 1; i >= 0; --i) {
        if (buttonArr[i]->isVisible()) {
            setSelectedEq(i);
            return;
        }
    }
    selectedEqComponent.setVisible(false);
}

void ProceduralEqAudioProcessorEditor::buttonReset(int id) {
    buttonArr[id]->resetEq();
}

juce::Rectangle<int> ProceduralEqAudioProcessorEditor::getRenderArea() {
    auto bounds = getLocalBounds();
    bounds.removeFromTop(15);
    bounds.removeFromBottom(15);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);
    return bounds;
}

//fix up the labels being printed on the side they look like shit
juce::Rectangle<int> ProceduralEqAudioProcessorEditor::backgroundImage() {
    using namespace juce;
    background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g(background);
    StringArray freqs{ "20Hz", "30Hz", "40Hz", "50Hz", "100Hz", "200Hz", "300Hz", "400Hz",
                       "500Hz", "1KHz", "2KHz", "3KHz", "4KHz", "5KHz", "10KHz", "20KHz" };
    Array<float> normX{ 0, 0.0586971, 0.100343, 0.132647, 0.23299, 0.333333, 0.39203, 0.433677,
                        0.46598, 0.566323, 0.666667, 0.725364, 0.76701, 0.799313, 0.899657, 1 };
    Array<float> xs;
    Array<float> ys;
    StringArray gain{ "-72", "-60", "-48", "-36","-24", "-12", "0", "+12", "+24" };
    auto renderArea = getRenderArea();
    auto left = renderArea.getX();
    auto right = renderArea.getRight();
    auto top = renderArea.getY();
    auto bottom = renderArea.getBottom();
    auto width = renderArea.getWidth();

    for (auto norm : normX) {
        xs.add(left + width * norm);
    }

    g.setColour(Colours::dimgrey);
    for (auto x : xs) {
        g.drawVerticalLine(x, top, bottom);
    }

    for (float i = -72; i < 36; i += 12) {
        auto y = jmap(i, -72.0f, 24.0f, float(bottom), float(top));
        ys.add(y);
        g.setColour(i == 0.0f ? Colour(0u, 172u, 1u) : Colours::darkgrey);
        g.drawHorizontalLine(y, left, right);
    }

    g.setColour(Colours::lightgrey);
    const int fontHeight = 12; //freq and dB font for image!!
    g.setFont(fontHeight);
    for (int i = 0; i < freqs.size(); ++i) {
        auto f = freqs[i];
        auto x = xs[i];

        auto textWidth = g.getCurrentFont().getStringWidth(f);
        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setCentre(x - 5, 0);
        r.setY(1);
        g.drawFittedText(f, r, juce::Justification::centred, 1);
    }

    for (int i = 0; i < ys.size(); ++i) {
        auto textWidth = g.getCurrentFont().getStringWidth(gain[i]);

        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setX(getWidth() - textWidth);
        r.setCentre(r.getCentreX(), ys[i] + 5);

        g.setColour(i == 6 ? Colour(0u, 172u, 1u) : Colours::lightgrey);
        g.drawFittedText(gain[i], r, juce::Justification::centred, 1);
    }
    return Rectangle<int>(xs[0], ys[7], xs[15] - xs[0], ys[0] - ys[7]);
}
