#include "PluginProcessor.h"
#include "PluginEditor.h"

FourierTruncatorProcessor::FourierTruncatorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    nParam = apvts.getRawParameterValue ("n");
    mixParam = apvts.getRawParameterValue ("mix");
    modeParam = apvts.getRawParameterValue ("mode");
}

juce::AudioProcessorValueTreeState::ParameterLayout FourierTruncatorProcessor::createLayout()
{
    // Log-scaled so the knob spends as much travel on 1..45 as on 45..2048.
    juce::NormalisableRange<float> nRange {
        1.0f, (float) ft::maxN,
        [] (float start, float end, float t) { return start * std::pow (end / start, t); },
        [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
        [] (float, float, float v) { return std::round (v); }
    };

    return {
        std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "n", 1 }, "N", nRange, (float) ft::maxN,
            juce::AudioParameterFloatAttributes()
                .withLabel ("bins")
                .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v); })),
        std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "mix", 1 }, "Dry/Wet", juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel ("%")
                .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)); })
                .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; })),
        std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "mode", 1 }, "Mode",
            juce::StringArray { "Low", "High", "Sparse Odd", "Strongest" }, 0)
    };
}

void FourierTruncatorProcessor::prepareToPlay (double sampleRate, int)
{
    displaySampleRate.store (sampleRate);
    engine.prepare (getTotalNumOutputChannels());
    setLatencySamples (ft::latencySamples);
}

bool FourierTruncatorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && out == layouts.getMainInputChannelSet();
}

void FourierTruncatorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setParameters (nParam->load(), (ft::Mode) juce::roundToInt (modeParam->load()), mixParam->load());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

double FourierTruncatorProcessor::getTailLengthSeconds() const
{
    return ft::fftSize / juce::jmax (1.0, getSampleRate());
}

juce::AudioProcessorEditor* FourierTruncatorProcessor::createEditor()
{
    return new FourierTruncatorEditor (*this);
}

void FourierTruncatorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FourierTruncatorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FourierTruncatorProcessor();
}
