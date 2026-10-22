#include "TruncatorEngine.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ft
{

namespace
{
    // Periodic Hann squared sums to 1.5 at 75% overlap; this undoes it.
    constexpr float olaGain = 2.0f / 3.0f;
    // A full-scale bin-centred sine peaks at fftSize / 4 after the Hann window.
    constexpr float displayNorm = 4.0f / (float) fftSize;
    // Per-sample one-pole for the mix (~10 ms) and per-frame log-domain glide for N (~0.2 s).
    constexpr float mixSmoothing = 0.002f;
    constexpr float nGlide = 0.4f;
}

void computeWeights (Mode mode, float n, const float* magnitudes, float* weights, int* scratch)
{
    n = std::clamp (n, 1.0f, (float) maxN);
    std::fill (weights, weights + numBins, 0.0f);

    auto keep = [n] (int rank) { return std::clamp (n - (float) rank, 0.0f, 1.0f); };

    switch (mode)
    {
        case Mode::Low:
            for (int k = 0; k < numBins; ++k)
                weights[k] = keep (k);
            break;

        case Mode::High:
            for (int k = 0; k < numBins; ++k)
                weights[k] = keep (numBins - 1 - k);
            break;

        case Mode::Odd:
            for (int k = 1; k < numBins; k += 2)
                weights[k] = keep ((k - 1) / 2);
            break;

        case Mode::Peak:
        {
            std::iota (scratch, scratch + numBins, 0);
            const int whole = (int) n; // n <= maxN < numBins, so scratch[whole] exists
            std::nth_element (scratch, scratch + whole, scratch + numBins,
                              [magnitudes] (int a, int b) { return magnitudes[a] > magnitudes[b]; });

            for (int i = 0; i < whole; ++i)
                weights[scratch[i]] = 1.0f;

            weights[scratch[whole]] = n - (float) whole;
            break;
        }
    }
}

void Engine::prepare (int numChannels)
{
    window.resize (fftSize);
    for (int j = 0; j < fftSize; ++j)
        window[(size_t) j] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) j / (float) fftSize);

    fftBuffer.assign (2 * fftSize, 0.0f);
    magnitudes.assign (numBins, 0.0f);
    weights.assign (numBins, 0.0f);
    scratch.assign (numBins, 0);

    // Log-spaced bands from bin 4 (~45 Hz) to Nyquist.
    for (int b = 0; b <= numBands; ++b)
    {
        const auto edge = (int) std::lround (4.0 * std::pow (512.0, (double) b / numBands));
        bandEdges[(size_t) b] = b == 0 ? edge : std::max (edge, bandEdges[(size_t) b - 1] + 1);
    }

    channels.assign ((size_t) numChannels, {});
    for (auto& c : channels)
    {
        c.in.assign (fftSize, 0.0f);
        c.out.assign (fftSize, 0.0f);
        c.dry.assign (latencySamples, 0.0f);
    }

    reset();
}

void Engine::reset()
{
    for (auto& c : channels)
    {
        std::fill (c.in.begin(), c.in.end(), 0.0f);
        std::fill (c.out.begin(), c.out.end(), 0.0f);
        std::fill (c.dry.begin(), c.dry.end(), 0.0f);
    }

    for (int b = 0; b < numBands; ++b)
    {
        snapshot.total[(size_t) b].store (0.0f);
        snapshot.kept[(size_t) b].store (0.0f);
    }

    hopPos = 0;
    dryPos = 0;
    snapParameters = true;
}

void Engine::setParameters (float n, Mode newMode, float mix)
{
    targetN = std::clamp (n, 1.0f, (float) maxN);
    targetMix = std::clamp (mix, 0.0f, 1.0f);
    mode = newMode;

    if (snapParameters)
    {
        currentN = targetN;
        currentMix = targetMix;
        snapParameters = false;
    }
}

void Engine::process (float* const* data, int numChannels, int numSamples)
{
    numChannels = std::min (numChannels, (int) channels.size());

    for (int i = 0; i < numSamples; ++i)
    {
        currentMix += (targetMix - currentMix) * mixSmoothing;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            const float x = data[ch][i];

            c.in[(size_t) (fftSize - hopSize + hopPos)] = x;
            const float wet = c.out[(size_t) hopPos];
            const float dry = c.dry[(size_t) dryPos];
            c.dry[(size_t) dryPos] = x;

            data[ch][i] = dry + currentMix * (wet - dry);
        }

        if (++dryPos == latencySamples)
            dryPos = 0;

        if (++hopPos == hopSize)
        {
            hopPos = 0;
            advanceN();

            for (int ch = 0; ch < numChannels; ++ch)
                processFrame (channels[(size_t) ch], ch == 0);
        }
    }
}

void Engine::advanceN()
{
    if (std::abs (targetN - currentN) < 0.01f)
        currentN = targetN;
    else
        currentN = std::exp (std::log (currentN) + (std::log (targetN) - std::log (currentN)) * nGlide);
}

void Engine::processFrame (Channel& c, bool publish)
{
    auto* buf = fftBuffer.data();

    for (int j = 0; j < fftSize; ++j)
        buf[j] = c.in[(size_t) j] * window[(size_t) j];

    std::fill (buf + fftSize, buf + 2 * fftSize, 0.0f);
    fft.performRealOnlyForwardTransform (buf, true);

    for (int k = 0; k < numBins; ++k)
        magnitudes[(size_t) k] = std::hypot (buf[2 * k], buf[2 * k + 1]);

    computeWeights (mode, currentN, magnitudes.data(), weights.data(), scratch.data());

    for (int k = 0; k < numBins; ++k)
    {
        buf[2 * k] *= weights[(size_t) k];
        buf[2 * k + 1] *= weights[(size_t) k];
    }

    if (publish)
        publishSpectrum();

    fft.performRealOnlyInverseTransform (buf);

    std::move (c.out.begin() + hopSize, c.out.end(), c.out.begin());
    std::fill (c.out.end() - hopSize, c.out.end(), 0.0f);

    for (int j = 0; j < fftSize; ++j)
        c.out[(size_t) j] += buf[j] * window[(size_t) j] * olaGain;

    std::move (c.in.begin() + hopSize, c.in.end(), c.in.begin());
}

void Engine::publishSpectrum()
{
    for (int b = 0; b < numBands; ++b)
    {
        float total = 0.0f, kept = 0.0f;

        for (int k = bandEdges[(size_t) b]; k < bandEdges[(size_t) b + 1]; ++k)
        {
            total = std::max (total, magnitudes[(size_t) k]);
            kept = std::max (kept, magnitudes[(size_t) k] * weights[(size_t) k]);
        }

        snapshot.total[(size_t) b].store (total * displayNorm, std::memory_order_relaxed);
        snapshot.kept[(size_t) b].store (kept * displayNorm, std::memory_order_relaxed);
    }
}

} // namespace ft
