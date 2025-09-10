/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
// Simple lock-free FIFO using AbstractFifo
template <typename T>
class AnalyserFifo
{
public:
    AnalyserFifo(int capacity) : fifo(capacity), buffer(capacity) {}

    bool push(T sample) {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(1, start1, size1, start2, size2);

        if (size1 > 0) {
            buffer[(size_t)start1] = sample;
        }
        if (size2 > 0) {
            buffer[(size_t)start2] = sample;
        }
        fifo.finishedWrite(size1 + size2);
        return (size1 + size2) > 0;
    }

    int pop(T* dest, int numToRead) {
        int start1, size1, start2, size2;
        fifo.prepareToRead(numToRead, start1, size1, start2, size2);
        int numRead = size1 + size2;

        if (size1 > 0)
            std::copy(buffer.begin() + start1, buffer.begin() + start1 + size1, dest);
        if (size2 > 0)
            std::copy(buffer.begin() + start2, buffer.begin() + start2 + size2, dest + size1);

        fifo.finishedRead(numRead);
        return numRead;
    }

    void clear() {
        fifo.reset();
    }

private:
    juce::AbstractFifo fifo;
    std::vector<T> buffer;
};

//==============================================================================
/**
*/
//template to have true logarithmic skew for frequency sliders, dont forget to cast to float :)
template <typename ValueT>
juce::NormalisableRange<ValueT> logRange(ValueT min, ValueT max)
{
    ValueT rng{ std::log(max / min) };
    return { min, max,
        [=](ValueT min, ValueT, ValueT v) { return std::exp(v * rng) * min; },
        [=](ValueT min, ValueT, ValueT v) { return std::log(v / min) / rng; }
    };
}

static juce::String formatFrequency(float value, int decimalsHz = 1, int decimalsKHz = 2)
{
    if (value < 1000.0f)
        return juce::String(value, decimalsHz) + " Hz";

    return juce::String(value / 1000.0f, decimalsKHz) + " kHz";
}

static juce::String formatGain(float value, int decimals = 2)
{
    return juce::String(value, decimals) + " dB";
}

static juce::String formatQuality(float value, int decimals = 2)
{
    return juce::String(value, decimals);
}

//==============================================================================
/**
*/
extern juce::StringArray params;
inline constexpr int MAX_EQS = 12;

struct FilterUpdateReq {
    std::atomic<bool> dirty{ false };
    std::atomic<float> freq{ 500.0f };
    std::atomic<float> gain{ 0.0f };
    std::atomic<float> quality{ 1.0f };
    std::atomic<int> type{ 0 };
    std::atomic<bool> bypass{ true };
    std::atomic<bool> isInit{ false };
};

//==============================================================================
/**
*/
class ProceduralEqAudioProcessor : public juce::AudioProcessor, juce::AudioProcessorValueTreeState::Listener {
public:
    //==============================================================================
    ProceduralEqAudioProcessor();
    ~ProceduralEqAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    using MultiFilter = juce::dsp::ProcessorDuplicator<Filter, Coeffs>;
    std::vector<MultiFilter> filters;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState tree{ *this, nullptr, "Parameters", createParameterLayout() };
    void updateAllFilters();
    void updateParameter(int id, int paramInd, float newValue);
    void updateFilter(int ind, const FilterUpdateReq& req);
    void resetEq(int ind);

    const std::array<FilterUpdateReq, MAX_EQS>& getPendingUpdates() const { return pendingUpdates; }
    const FilterUpdateReq& getUpdateForBand(int index) const { return pendingUpdates[index]; }

    AnalyserFifo<float>* getAnalyserFifo() { return analyserFifo.get(); }
    std::atomic<float>* analyserOnParam = nullptr;
    std::atomic<float>* analyserModeParam = nullptr;

    std::array<Coeffs, MAX_EQS> guiCoeffs;
    Coeffs makeCoefficients(const FilterUpdateReq& req) const;
    
private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::dsp::ProcessSpec spec;
    double lastSampleRate = 44100.0;
    std::unique_ptr<AnalyserFifo<float>> analyserFifo;
    std::array<FilterUpdateReq, MAX_EQS> pendingUpdates;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProceduralEqAudioProcessor)
};