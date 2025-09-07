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

extern juce::StringArray params;
extern const int MAX_EQS;

//==============================================================================
/**
*/
class ProceduralEqAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================

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
    using MonoFilter = juce::dsp::ProcessorDuplicator<Filter, Coeffs>;
    std::vector<MonoFilter> filters;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState tree{ *this, nullptr, "Parameters", createParameterLayout() };
    void updateFilter(int ind);
    void updateAllFilters();
    void updateParameter(int id, int paramInd, float newValue);
    AnalyserFifo<float>* getAnalyserFifo() { return analyserFifo.get(); }
    
    std::unique_ptr<AnalyserFifo<float>> analyserFifo;

private:
    juce::dsp::ProcessSpec spec;
    double lastSampleRate = 44100.0;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProceduralEqAudioProcessor)
};