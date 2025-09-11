/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::StringArray params{ "1Freq", "1Gain", "1Quality", "1Type", "1Bypass", "1Init", 
                          "2Freq", "2Gain", "2Quality", "2Type", "2Bypass", "2Init",
                          "3Freq", "3Gain", "3Quality", "3Type", "3Bypass", "3Init", 
                          "4Freq", "4Gain", "4Quality", "4Type", "4Bypass", "4Init",
                          "5Freq", "5Gain", "5Quality", "5Type", "5Bypass", "5Init",                                                                                                                                                
                          "6Freq", "6Gain", "6Quality", "6Type", "6Bypass", "6Init",
                          "7Freq", "7Gain", "7Quality", "7Type", "7Bypass", "7Init", 
                          "8Freq", "8Gain", "8Quality", "8Type", "8Bypass", "8Init",
                          "9Freq", "9Gain", "9Quality", "9Type", "9Bypass", "9Init", 
                          "10Freq", "10Gain", "10Quality", "10Type", "10Bypass", "10Init",
                          "11Freq", "11Gain", "11Quality", "11Type", "11Bypass", "11Init", 
                          "12Freq", "12Gain", "12Quality", "12Type", "12Bypass", "12Init",
                          "PreGain", "PostGain"};

juce::StringArray bands{ "BANDPASS", "HIGHPASS", "LOWPASS", "HIGHSHELF", "LOWSHELF" };

//==============================================================================
ProceduralEqAudioProcessor::ProceduralEqAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif 
{
    lastSampleRate = getSampleRate();

    for (auto& id : params)
        tree.addParameterListener(id, this);

    for (int i = 0; i < MAX_EQS; ++i) {
        filters.emplace_back(MultiFilter(Coeffs::makePeakFilter(lastSampleRate, 250.0f + 500.0f * i, 0.1f, 0)));
        auto& req = pendingUpdates[i];
        req.freq.store(*tree.getRawParameterValue(params[0 + i * 6]));
        req.gain.store(*tree.getRawParameterValue(params[1 + i * 6]));
        req.quality.store(*tree.getRawParameterValue(params[2 + i * 6]));
        req.type.store(static_cast<int>(*tree.getRawParameterValue(params[3 + i * 6])));
        req.bypass.store(*tree.getRawParameterValue(params[4 + i * 6]) >= 0.5f);
        req.isInit.store(*tree.getRawParameterValue(params[5 + i * 6]) >= 0.5f);
        req.dirty.store(true);
    }
    analyserOnParam = tree.getRawParameterValue("analyserOn");
    analyserModeParam = tree.getRawParameterValue("analyserMode");

    for (int i = 0; i < MAX_EQS; ++i)
        guiCoeffs[i] = *filters[i].state;
    updateAllFilters();
}

ProceduralEqAudioProcessor::~ProceduralEqAudioProcessor() {
    for (auto& id : params)
        tree.removeParameterListener(id, this);
}

//==============================================================================
const juce::String ProceduralEqAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool ProceduralEqAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool ProceduralEqAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool ProceduralEqAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double ProceduralEqAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int ProceduralEqAudioProcessor::getNumPrograms() {
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int ProceduralEqAudioProcessor::getCurrentProgram() {
    return 0;
}

void ProceduralEqAudioProcessor::setCurrentProgram(int index) {}

const juce::String ProceduralEqAudioProcessor::getProgramName(int index) {
    return {};
}

void ProceduralEqAudioProcessor::changeProgramName(int index, const juce::String& newName) {}

//==============================================================================
void ProceduralEqAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    lastSampleRate = sampleRate;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    for (auto& filter : filters) {
        filter.prepare(spec);
        filter.reset();
    }
    updateAllFilters();
    preGain.prepare(spec);
    postGain.prepare(spec);
    updateGain(0);
    updateGain(1);

    analyserFifo = std::make_unique<AnalyserFifo<float>>(fftSize * 2);
    if (analyserFifo) {
        analyserFifo->clear();
    }
}

void ProceduralEqAudioProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    analyserFifo.reset();
}

bool ProceduralEqAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;

}

void ProceduralEqAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    bool analyserBool = analyserFifo && analyserOnParam && *analyserOnParam > 0.5f;
    if (analyserBool) {
        if (analyserModeParam && *analyserModeParam < 0.5f) {
            auto* left = buffer.getReadPointer(0);
            auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr;
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                float sample = right ? 0.5f * (left[i] + right[i]) : left[i];
                analyserFifo->push(sample);
            }
        }
    }

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    preGain.process(context);

    for (int i = 0; i < MAX_EQS; ++i) {
        auto& req = pendingUpdates[i];
        if (req.dirty.exchange(false))
            updateFilter(i, req);
        if (!req.bypass && req.isInit)
            filters[i].process(context);
    }

    postGain.process(context);

    if (analyserBool) {
        if (analyserModeParam && *analyserModeParam >= 0.5f) {
            auto* left = buffer.getReadPointer(0);
            auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr;
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                float sample = right ? 0.5f * (left[i] + right[i]) : left[i];
                analyserFifo->push(sample);
            }
        }
    }
}

//==============================================================================
bool ProceduralEqAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* ProceduralEqAudioProcessor::createEditor() {
    return new ProceduralEqAudioProcessorEditor(*this);
    //return new juce::GenericAudioProcessorEditor(*this); //use to test without editor
}

//==============================================================================
void ProceduralEqAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::MemoryOutputStream mos(destData, true);
    tree.state.writeToStream(mos);
}

void ProceduralEqAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto readData = juce::ValueTree::readFromData(data, sizeInBytes);
    if (readData.isValid()) {
        tree.replaceState(readData);
        updateAllFilters();
    }
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ProceduralEqAudioProcessor();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ProceduralEqAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < MAX_EQS; ++i) {
        //init freq
        layout.add(std::make_unique<juce::AudioParameterFloat>(params[0 + i * 6], params[0 + i * 6], logRange<float>(20.0f, 20000.0f), 500.0f + 500.0f * i, juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int) {
                return formatFrequency(value);
                })
            .withValueFromStringFunction([](const juce::String& text) {
                return text.getFloatValue();
                })
        ));
        //init gain
        layout.add(std::make_unique<juce::AudioParameterFloat>(params[1 + i * 6], params[1 + i * 6], juce::NormalisableRange<float>(-72.0f, 12.0f), 0.0f, juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int) {
                return formatGain(value);
                })
            .withValueFromStringFunction([](const juce::String& text) {
                return text.getFloatValue();
                })
        ));
        //init quality
        layout.add(std::make_unique<juce::AudioParameterFloat>(params[2 + i * 6], params[2 + i * 6], juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f), 1.0f, juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int) {
                return formatQuality(value);
                })
            .withValueFromStringFunction([](const juce::String& text) {
                return text.getFloatValue();
                })
        ));
        //init type(0 is peak, 1 is low cut, 2 is high cut, 3 is high shelf, 4 is low shelf)
        layout.add(std::make_unique<juce::AudioParameterChoice>(params[3 + i * 6], params[3 + i * 6], bands, 0));
        //init bypass
        layout.add(std::make_unique<juce::AudioParameterBool>(params[4 + i * 6], params[4 + i * 6], true));
        //init init :)
        layout.add(std::make_unique<juce::AudioParameterBool>(params[5 + i * 6], params[5 + i * 6], false));
    }
    layout.add(std::make_unique<juce::AudioParameterFloat>(params[72], params[72], -72.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(params[73], params[73], -72.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>("analyserOn", "Analyser On", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>("analyserMode", "Analyser Mode", juce::StringArray{ "Pre-EQ", "Post-EQ" }, 1));
    return layout;
}

void ProceduralEqAudioProcessor::updateFilter(int ind, const FilterUpdateReq& req) {
    auto c = makeCoefficients(req);
    *filters[ind].state = c;
    guiCoeffs[ind] = c;
}

void ProceduralEqAudioProcessor::parameterChanged(const juce::String& paramID, float newValue) {
    for (int i = 0; i < MAX_EQS; ++i) {
        for (int p = 0; p < 6; ++p) {
            if (params[p + i * 6] == paramID) {
                auto& req = pendingUpdates[i];
                switch (p) {
                case 0: req.freq = newValue; break;
                case 1: req.gain = newValue; break;
                case 2: req.quality = newValue; break;
                case 3: req.type = static_cast<int>(newValue); break;
                case 4: req.bypass = (newValue >= 0.5f); break;
                case 5: req.isInit = (newValue >= 0.5f); break;
                }
                req.dirty = true;
                guiCoeffs[i] = makeCoefficients(req);
                return;
            }
        }
    }
    if (params[72] == paramID)
        updateGain(0);
        
    else if (params[73] == paramID)
        updateGain(1);

}

void ProceduralEqAudioProcessor::updateAllFilters() {
    for (int i = 0; i < MAX_EQS; ++i)
        updateFilter(i, pendingUpdates[i]);
}

//give eq ind, param ind, and 0 to 1 value to change
void ProceduralEqAudioProcessor::updateParameter(int id, int paramInd, float newValue) {
    if (auto* pParam = tree.getParameter(params[paramInd + id * 6])) {
        pParam->beginChangeGesture();
        pParam->setValueNotifyingHost(newValue);
        pParam->endChangeGesture();
    }
}

ProceduralEqAudioProcessor::Coeffs ProceduralEqAudioProcessor::makeCoefficients(const FilterUpdateReq& req) const
{
    if (req.bypass || !req.isInit)
        return *Coeffs::makeAllPass(lastSampleRate, 1000.0f);

    switch (req.type) {
    case 0: return *Coeffs::makePeakFilter(lastSampleRate, req.freq, req.quality, juce::Decibels::decibelsToGain(float(req.gain), -80.0f));
    case 1: return *Coeffs::makeHighPass(lastSampleRate, req.freq, req.quality);
    case 2: return *Coeffs::makeLowPass(lastSampleRate, req.freq, req.quality);
    case 3: return *Coeffs::makeHighShelf(lastSampleRate, req.freq, req.quality, juce::Decibels::decibelsToGain(float(req.gain), -80.0f));
    case 4: return *Coeffs::makeLowShelf(lastSampleRate, req.freq, req.quality, juce::Decibels::decibelsToGain(float(req.gain), -80.0f));
    default: return *Coeffs::makeAllPass(lastSampleRate, 1000.0f);
    }
}

void ProceduralEqAudioProcessor::resetEq(int ind) {
    if (ind < 0 || ind >= MAX_EQS) return;
    updateParameter(ind, 2, 0.1f);
    updateParameter(ind, 3, 0);
    updateParameter(ind, 4, 1);
    updateParameter(ind, 5, 0);
    auto allpass = *Coeffs::makeAllPass(lastSampleRate, 1000.0f);
    guiCoeffs[ind] = allpass;
    *filters[ind].state = allpass;
}

void ProceduralEqAudioProcessor::updateGain(int id) {
    if (id == 0)
        preGain.setGainDecibels(tree.getRawParameterValue(params[72])->load());
    else if (id == 1)
        postGain.setGainDecibels(tree.getRawParameterValue(params[73])->load());
}
