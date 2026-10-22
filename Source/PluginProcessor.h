#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TruncatorEngine.h"

class FourierTruncatorProcessor final : public juce::AudioProcessor
{
public:
    FourierTruncatorProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    ft::SpectrumSnapshot& spectrum() { return engine.spectrum(); }
    double getDisplaySampleRate() const { return displaySampleRate.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    ft::Engine engine;
    std::atomic<float>* nParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<double> displaySampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourierTruncatorProcessor)
};
