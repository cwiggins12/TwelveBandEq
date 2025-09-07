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

juce::StringArray bands{ "BandPass", "HighPass", "LowPass", "HighShelf", "LowShelf" };

const int MAX_EQS = 12;

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
    for (int i = 0; i < MAX_EQS; ++i) {
        filters.emplace_back(MonoFilter(Coeffs::makePeakFilter(lastSampleRate, 250.0f + 500.0f * i, 0.1f, 0)));
    }
}

ProceduralEqAudioProcessor::~ProceduralEqAudioProcessor() {}

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
    updateAllFilters(); //if all goes well this can be removed soon, but may just leave it for a fallback in case params are not updated properly

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

    juce::dsp::AudioBlock<float> block(buffer);

    for (int i = 0; i < MAX_EQS; ++i) {
        int stringIndex = 4 + (i * 6);
        auto* isBypassed = tree.getRawParameterValue(params[stringIndex]);
        if (isBypassed && *isBypassed < 0.5f) {
            juce::dsp::ProcessContextReplacing<float> context(block);
            filters[i].process(context);
        }
    }

    if (analyserFifo) {
        auto* left = buffer.getReadPointer(0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr;

        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            float sample = right ? 0.5f * (left[i] + right[i]) : left[i];
            analyserFifo->push(sample);
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
    //juce::MemoryOutputStream mos(destData, true);
    //apvts.state.writeToStream(mos);
}

void ProceduralEqAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    //auto readData = juce::ValueTree::readFromData(data, sizeInBytes);
    //if (readData.isValid()) {
    //    tree.replaceState(readData);
    //    updateFilters();
    //}
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
            .withStringFromValueFunction([](float value, int /*maxLen*/) {
                return (value < 999.9f) ? (juce::String((int)std::round(value)) + " Hz") : (juce::String((int)std::round(value / 1000)) + " kHz");  // show as integer
                })
            .withValueFromStringFunction([](const juce::String& text) {
                return text.getFloatValue(); // parse float input if user types in
                })
        ));
        //init gain
        layout.add(std::make_unique<juce::AudioParameterFloat>(params[1 + i * 6],
            params[1 + i * 6],
            juce::NormalisableRange<float>(-72.0f, 12.0f),
            0.0f,
            juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int) {
                return juce::String(value, 1) + " dB";
                })
            .withValueFromStringFunction([](const juce::String& text) {
                return text.getFloatValue();
                })
        ));
        //init quality
        layout.add(std::make_unique<juce::AudioParameterFloat>(params[2 + i * 6], params[2 + i * 6], juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f), 1.0f));
        //init type(0 is peak, 1 is low cut, 2 is high cut)
        layout.add(std::make_unique<juce::AudioParameterChoice>(params[3 + i * 6], params[3 + i * 6], bands, 0));
        //init bypass
        layout.add(std::make_unique<juce::AudioParameterBool>(params[4 + i * 6], params[4 + i * 6], true));
        //init init :)
        layout.add(std::make_unique<juce::AudioParameterBool>(params[5 + i * 6], params[5 + i * 6], false));
    }
    return layout;
}

//could do an immediate check of is init and bypass to optimize this. Will do later
void ProceduralEqAudioProcessor::updateFilter(int ind) {
    int stringArrStartInd = ind * 6;
    float freq = *tree.getRawParameterValue(params[stringArrStartInd++]);
    float gain = *tree.getRawParameterValue(params[stringArrStartInd++]);
    float quality = *tree.getRawParameterValue(params[stringArrStartInd++]);
    int type = *tree.getRawParameterValue(params[stringArrStartInd++]);
    bool bypass = *tree.getRawParameterValue(params[stringArrStartInd++]);
    bool isInit = *tree.getRawParameterValue(params[stringArrStartInd++]);
    if (!bypass && isInit) {
        switch (type) {
        case 0: {
            *filters[ind].state = *Coeffs::makePeakFilter(lastSampleRate, freq, quality, juce::Decibels::decibelsToGain(gain, -72.0f));
            break;
        }
        case 1: {
            *filters[ind].state = *Coeffs::makeHighPass(lastSampleRate, freq, quality);
            break;
        }
        case 2: {
            *filters[ind].state = *Coeffs::makeLowPass(lastSampleRate, freq, quality);
            break;
        }
        case 3: {
            *filters[ind].state = *Coeffs::makeHighShelf(lastSampleRate, freq, quality, gain);
            break;
        }
        case 4: {
            *filters[ind].state = *Coeffs::makeLowShelf(lastSampleRate, freq, quality, gain);
            break;
        }
        }
    }
}

void ProceduralEqAudioProcessor::updateAllFilters() {
    for (int i = 0; i < MAX_EQS; ++i) {
        updateFilter(i);
    }
}

//give eq ind, param ind, and 0 to 1 value to change
void ProceduralEqAudioProcessor::updateParameter(int id, int paramInd, float newValue) {
    juce::AudioProcessorParameterWithID* pParam = tree.getParameter(params[paramInd + id * 6]);
    pParam->beginChangeGesture();
    pParam->setValueNotifyingHost(newValue);
    pParam->endChangeGesture();
    updateFilter(id);
}