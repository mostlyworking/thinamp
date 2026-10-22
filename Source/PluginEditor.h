#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace ft::ui
{
    // Everything is drawn in Winamp's 275x116 main-window units, doubled.
    constexpr float scale = 2.0f;
    constexpr int baseWidth = 275, baseHeight = 116;
}

class WinampLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override;
};

// The LOW / HIGH / ODD / PEAK row, drawn like Winamp's transport buttons.
class ModeButtons final : public juce::Component
{
public:
    explicit ModeButtons (juce::RangedAudioParameter& modeParameter);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int buttonWidth = 27, gap = 2, numButtons = 4;

private:
    juce::ParameterAttachment attachment;
    int selected = 0;
};

class ThinampEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ThinampEditor (ThinampProcessor&);
    ~ThinampEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawTitleBar (juce::Graphics&);
    void drawLcd (juce::Graphics&);
    void drawMarquee (juce::Graphics&);
    void drawReadouts (juce::Graphics&);

    ThinampProcessor& truncator;
    WinampLookAndFeel lookAndFeel;

    juce::Slider nSlider, mixSlider;
    juce::AudioProcessorValueTreeState::SliderAttachment nAttachment, mixAttachment;
    ModeButtons modeButtons;

    // Analyzer bar heights in base units, with Winamp-style falling peak caps.
    std::array<float, ft::numBands> barTotal {}, barKept {}, barPeak {};
    std::array<int, ft::numBands> peakHold {};

    juce::String marqueeText;
    float marqueeWidth = 0.0f, marqueeOffset = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThinampEditor)
};
