#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

namespace ft
{

enum class Mode { Low = 0, High, Odd, Peak };

constexpr int fftOrder = 12;
constexpr int fftSize = 1 << fftOrder;        // 4096
constexpr int hopSize = fftSize / 4;          // 75% overlap
constexpr int numBins = fftSize / 2 + 1;      // 2049 basis functions, DC..Nyquist
constexpr int maxN = 2048;
constexpr int latencySamples = fftSize;
constexpr int numBands = 19;                  // Winamp's analyzer bar count

// Fills per-bin keep weights in [0, 1]. A fractional n keeps one boundary bin at partial weight,
// so a gliding N never jumps by a whole bin between frames. scratch must hold numBins ints.
void computeWeights (Mode mode, float n, const float* magnitudes, float* weights, int* scratch);

// Latest frame's band levels for the UI (linear, 1.0 = full-scale sine). Written by the audio thread.
struct SpectrumSnapshot
{
    std::array<std::atomic<float>, numBands> total {};
    std::array<std::atomic<float>, numBands> kept {};
};

class Engine
{
public:
    void prepare (int numChannels);
    void reset();
    void setParameters (float n, Mode mode, float mix);
    void process (float* const* channels, int numChannels, int numSamples);

    SpectrumSnapshot& spectrum() { return snapshot; }

private:
    struct Channel
    {
        std::vector<float> in, out, dry;
    };

    void advanceN();
    void processFrame (Channel& channel, bool publish);
    void publishSpectrum();

    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window, fftBuffer, magnitudes, weights;
    std::vector<int> scratch;
    std::array<int, numBands + 1> bandEdges {};
    std::vector<Channel> channels;

    int hopPos = 0, dryPos = 0;
    float targetN = maxN, currentN = maxN;
    float targetMix = 1.0f, currentMix = 1.0f;
    Mode mode = Mode::Low;
    bool snapParameters = true;

    SpectrumSnapshot snapshot;
};

} // namespace ft
