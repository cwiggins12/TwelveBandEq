/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::StringArray params{ "1 Freq", "1 Gain", "1 Quality", "1 Type", "1 Bypass", "1 Init", "2 Freq", "2 Gain", "2 Quality", "2 Type", "2 Bypass", "2 Init",
                  "3 Freq", "3 Gain", "3 Quality", "3 Type", "3 Bypass", "3 Init", "4 Freq", "4 Gain", "4 Quality", "4 Type", "4 Bypass", "4 Init",
                  "5 Freq", "5 Gain", "5 Quality", "5 Type", "5 Bypass", "5 Init", "6 Freq", "6 Gain", "6 Quality", "6 Type", "6 Bypass", "6 Init",
                  "7 Freq", "7 Gain", "7 Quality", "7 Type", "7 Bypass", "7 Init", "8 Freq", "8 Gain", "8 Quality", "8 Type", "8 Bypass", "8 Init",
                  "9 Freq", "9 Gain", "9 Quality", "9 Type", "9 Bypass", "9 Init", "10 Freq", "10 Gain", "10 Quality", "10 Type", "10 Bypass", "10 Init",
                  "11 Freq", "11 Gain", "11 Quality", "11 Type", "11 Bypass", "11 Init", "12 Freq", "12 Gain", "12 Quality", "12 Type", "12 Bypass", "12 Init" };

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

    for (int i = 0; i < MAX_EQS; ++i) {
        auto& req = pendingUpdates[i];
        if (req.dirty.exchange(false))
            updateFilter(i, req);
        if (!req.bypass && req.isInit)
            filters[i].process(context);
    }

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