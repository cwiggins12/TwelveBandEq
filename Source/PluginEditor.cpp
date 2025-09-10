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
    lineColor = juce::Colours::lime;
    startTimerHz(TIMER_FPS);
}

SpectrumAnalyser::~SpectrumAnalyser() {}

void SpectrumAnalyser::timerCallback() {
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

void SpectrumAnalyser::paint(juce::Graphics& g) {
    g.setColour(lineColor);
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    juce::Path p;
    auto mapY = [h](float v) { return juce::jmap(v, 0.0f, 1.0f, h, 0.0f); };

    p.startNewSubPath(0.0f, mapY(scopeData[0]));
    for (int i = 1; i < scopeSize; ++i)
        p.lineTo(juce::jmap((float)i, 0.0f, (float)scopeSize - 1.0f, 0.0f, w),
            mapY(scopeData[i]));
    juce::Path rounded = p.createPathWithRoundedCorners(16.0f);
    g.strokePath(rounded, juce::PathStrokeType(2.0f));
}

//==============================================================================
/**
*/
SelectedEqComponent::SelectedEqComponent(ProceduralEqAudioProcessor& p, int id) : audioProcessor(p), currEq(id) {
    setTopLeftPosition(400, 450);
    setSize(300, 200);
    auto w = getWidth() / 3;

    addAndMakeVisible(freqSlider);
    freqSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    freqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, w, textboxHeight);
    freqSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.tree, params[0 + currEq * 6], freqSlider);
    addAndMakeVisible(freqLabel);
    freqLabel.setText("FREQ", juce::NotificationType::dontSendNotification);
    freqLabel.setJustificationType(juce::Justification::centred);
    freqLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(gainSlider);
    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, w, textboxHeight);
    gainSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.tree, params[1 + currEq * 6], gainSlider);
    addAndMakeVisible(gainLabel);
    gainLabel.setText("GAIN", juce::NotificationType::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    gainLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(qualitySlider);
    qualitySlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    qualitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, w, textboxHeight);
    qualitySliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.tree, params[2 + currEq * 6], qualitySlider);
    addAndMakeVisible(qualityLabel);
    qualityLabel.setText("Q", juce::NotificationType::dontSendNotification);
    qualityLabel.setJustificationType(juce::Justification::centred);
    qualityLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(typeComboBox);
    typeComboBox.addItem("BANDPASS", 1);
    typeComboBox.addItem("HIGHPASS", 2);
    typeComboBox.addItem("LOWPASS", 3);
    typeComboBox.addItem("HIGH-SHELF", 4);
    typeComboBox.addItem("LOW-SHELF", 5);
    typeBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.tree, params[3 + currEq * 6], typeComboBox);
    addAndMakeVisible(typeLabel);
    typeLabel.setText("TYPE", juce::NotificationType::dontSendNotification);
    typeLabel.setJustificationType(juce::Justification::centred);
    typeLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(bypassButton);
    bypassButton.setClickingTogglesState(true);
    bypassButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.tree, params[4 + currEq * 6], bypassButton);
    addAndMakeVisible(bypassLabel);
    bypassLabel.setText("BYPASS", juce::NotificationType::dontSendNotification);
    bypassLabel.setJustificationType(juce::Justification::centred);
    bypassLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    deleteButton.setLookAndFeel(&lnfb);
    addAndMakeVisible(deleteButton);
    deleteButton.onClick = [this] {
        if (currEq >= 0) {
            if (auto* editor = dynamic_cast<ProceduralEqAudioProcessorEditor*>(getParentComponent())) {
                editor->buttonReset(currEq);
            }
        }
    };
    addAndMakeVisible(deleteLabel);
    deleteLabel.setText("DELETE", juce::NotificationType::dontSendNotification);
    deleteLabel.setJustificationType(juce::Justification::centred);
    deleteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
}

SelectedEqComponent::~SelectedEqComponent() {}

void SelectedEqComponent::updateEqAndSliders(int id) {
    //empty the sliders to not trigger a param change to prior eq
    freqSliderAttachment.reset();
    gainSliderAttachment.reset();
    qualitySliderAttachment.reset();
    typeBoxAttachment.reset();
    bypassButtonAttachment.reset();
    //set new eq and attach all params to sliders
    currEq = id;
    freqSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.tree, params[0 + currEq * 6], freqSlider);
    gainSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.tree, params[1 + currEq * 6], gainSlider);
    qualitySliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.tree, params[2 + currEq * 6], qualitySlider);
    typeBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.tree, params[3 + currEq * 6], typeComboBox);
    bypassButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.tree, params[4 + currEq * 6], bypassButton);
    deleteButton.setToggleState(false, juce::NotificationType::dontSendNotification);
}

void SelectedEqComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::grey);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colours::white);
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 2.0f);
}

void SelectedEqComponent::resized() {
    auto bounds = getLocalBounds();
    auto w = bounds.getWidth() / 3;
    auto top = bounds.removeFromTop(100);

    auto fArea = top.removeFromLeft(w);
    freqLabel.setBounds(fArea.removeFromTop(labelHeight));
    freqSlider.setBounds(makeSquareForSlider(fArea).withBottom(fArea.getBottom()));

    auto qArea = top.removeFromRight(w);
    qualityLabel.setBounds(qArea.removeFromTop(labelHeight));
    qualitySlider.setBounds(makeSquareForSlider(qArea).withBottom(qArea.getBottom()));

    gainLabel.setBounds(top.removeFromTop(labelHeight));
    gainSlider.setBounds(makeSquareForSlider(top).withBottom(top.getBottom()));

    auto tArea = bounds.removeFromLeft(w);
    typeLabel.setBounds(tArea.removeFromTop(labelHeight));
    typeComboBox.setBounds(tArea.reduced(10));

    auto dArea = bounds.removeFromRight(w);
    deleteLabel.setBounds(dArea.removeFromTop(labelHeight));
    deleteButton.setBounds(makeSquare(dArea.reduced(10)));

    bypassLabel.setBounds(bounds.removeFromTop(labelHeight));
    bypassButton.setBounds(makeSquare(bounds.reduced(10)));
}

//==============================================================================
/**
*/
ResponseCurveComponent::ResponseCurveComponent(ProceduralEqAudioProcessor& p, ProceduralEqAudioProcessorEditor& e) : audioProcessor(p), editor(e) {
    setInterceptsMouseClicks(false, false);
    for (int i = 0; i < params.size(); ++i)
        audioProcessor.tree.addParameterListener(params[i], this);
}

ResponseCurveComponent::~ResponseCurveComponent() {
    for (int i = 0; i < params.size(); ++i)
        audioProcessor.tree.removeParameterListener(params[i], this);
}

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
            const auto& req = audioProcessor.getPendingUpdates()[j];
            if (req.isInit && !req.bypass) {
                auto coeffs = audioProcessor.guiCoeffs[j];
                mag *= coeffs.getMagnitudeForFrequency(freq, sampleRate);
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

void ResponseCurveComponent::parameterChanged(const juce::String& paramID, float newValue) {
    repaint();
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
        editor.buttonReset(associatedEq);
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
    const auto& req = audioProcessor.getUpdateForBand(associatedEq);
    setCentreFromFreq(req.freq);
    setCentreFromGain(req.gain);
}

bool DraggableButton::hitTest(int x, int y) {
    auto centre = getLocalBounds().getCentre();
    auto radius = getLocalBounds().getWidth() / 2.0f;
    return centre.getDistanceFrom({ x, y }) <= radius;
}

void DraggableButton::parameterChanged(const juce::String& paramID, float newValue) {
    const auto& req = audioProcessor.getUpdateForBand(associatedEq);
    if (paramID == params[0 + associatedEq * 6])
        setCentreFromFreq(req.freq);
    else if (paramID == params[1 + associatedEq * 6])
        setCentreFromGain(req.gain);
    else if (paramID == params[4 + associatedEq * 6]) {
        isBypassed = req.bypass;
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

void DraggableButton::updateTooltip() {
    const auto& req = audioProcessor.getUpdateForBand(associatedEq);
    juce::String tip;
    tip << formatFrequency(req.freq) << ", " << formatGain(req.gain);
    setTooltip(tip);
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

    for (int i = 0; i < MAX_EQS; ++i) {
        bool isInit = *audioProcessor.tree.getRawParameterValue(params[5 + i * 6]);
        if (isInit) {
            buttonArr[i]->updatePositionFromParams();
            buttonArr[i]->setVisible(true);
        }
        else {
            buttonArr[i]->setVisible(false);
        }
    }
    secVisiblityCheck();

    selectedEqComponent.setLookAndFeel(&lnfa);
    addChildComponent(selectedEqComponent);

    addAndMakeVisible(analyserOnButton);
    analyserOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.tree, "analyserOn", analyserOnButton);
    analyserOnButton.setComponentID("analyserOn");
    analyserOnButton.setLookAndFeel(&lnfa);
    analyserOnButton.onClick = [this]() {
        analyser.setVisible(analyserOnButton.getToggleState());
    };

    addAndMakeVisible(analyserModeButton);
    analyserModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.tree, "analyserMode", analyserModeButton);
    analyserModeButton.setClickingTogglesState(true);
    analyserModeButton.setLookAndFeel(&lnfc);
    analyserModeButton.onClick = [this]() {
        analyser.lineColor = analyserModeButton.getToggleState() ? juce::Colours::lime : juce::Colours::yellow;
    };

    analyser.setVisible(analyserOnButton.getToggleState());
    analyser.lineColor = analyserModeButton.getToggleState() ? juce::Colours::lime : juce::Colours::yellow;
}

ProceduralEqAudioProcessorEditor::~ProceduralEqAudioProcessorEditor() {
    selectedEqComponent.setLookAndFeel(nullptr);
    analyserOnButton.setLookAndFeel(nullptr);
}

//==============================================================================
void ProceduralEqAudioProcessorEditor::paint(juce::Graphics& g) {
    g.drawImage(background, getLocalBounds().toFloat());
}

void ProceduralEqAudioProcessorEditor::resized() {
    background = background.rescaled(getLocalBounds().getWidth(), getLocalBounds().getHeight(), juce::Graphics::mediumResamplingQuality);
    auto area = getRenderArea();
    analyser.setBounds(area);
    rcc.setBounds(area);
    auto buttonHeight = getHeight() - 80;
    analyserOnButton.setBounds(40, buttonHeight, 60, 30);
    analyserModeButton.setBounds(100, buttonHeight, 60, 30);
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

            const auto& req = audioProcessor.getUpdateForBand(i);
            audioProcessor.guiCoeffs[i] = audioProcessor.makeCoefficients(req);

            setSelectedEq(i);
            buttonArr[i]->setVisible(true);
            return;
        }
    }
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
        "Maximum EQ Limit Reached",
        "The limit of equalizers is 12.");
}

void ProceduralEqAudioProcessorEditor::setSelectedEq(int id) {
    if (selectedEq == id && selectedEq > -1 && selectedEq < buttonArr.size()) {
        selectedEqComponent.setVisible(true);
        return;
    }
    if (selectedEq > -1 && selectedEq < buttonArr.size())
        buttonArr[selectedEq]->repaint();

    selectedEq = id;
    if (selectedEq > -1 && selectedEq < buttonArr.size() ) {
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
    audioProcessor.resetEq(id);
    buttonArr[id]->setVisible(false);
    secVisiblityCheck();
}

juce::Rectangle<int> ProceduralEqAudioProcessorEditor::getRenderArea() {
    auto bounds = getLocalBounds();
    bounds.removeFromTop(15);
    bounds.removeFromBottom(15);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);
    return bounds;
}

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
