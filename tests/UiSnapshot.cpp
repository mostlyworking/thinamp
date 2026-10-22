// Dev tool: renders the editor with live analyzer data to a PNG. Usage: UiSnapshot out.png [mode 0-3] [n]

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    FourierTruncatorProcessor processor;
    processor.setRateAndBufferSizeDetails (48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    auto set = [&processor] (const char* id, float value)
    {
        auto* p = processor.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    set ("mode", argc > 2 ? (float) std::atoi (argv[2]) : 3.0f);
    set ("n", argc > 3 ? (float) std::atoi (argv[3]) : 64.0f);
    set ("mix", 0.85f);

    // A sawtooth chord: rich harmonics so every band has something to keep or drop.
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    double phase[3] {};
    const double freqs[3] { 110.0, 164.8, 220.0 };
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    for (int block = 0; block < 200; ++block)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float s = 0.0f;
            for (int v = 0; v < 3; ++v)
            {
                phase[v] = std::fmod (phase[v] + freqs[v] / 48000.0, 1.0);
                s += (float) (0.25 * (2.0 * phase[v] - 1.0));
            }
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s);
        }
        processor.processBlock (buffer, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
    }

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "ui.png"));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    editor.reset();
    return 0;
}
