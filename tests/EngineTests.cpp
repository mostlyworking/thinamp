// Offline checks for the behaviour listed in README.md. Exit code is the number of failures.

#include "../Source/TruncatorEngine.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <random>
#include <vector>

namespace
{

constexpr double fs = 48000.0;
int failures = 0;

void check (bool ok, const char* name, double value)
{
    std::printf ("%s  %-52s %g\n", ok ? "PASS" : "FAIL", name, value);
    failures += ok ? 0 : 1;
}

double binFreq (int bin) { return bin * fs / ft::fftSize; }

std::vector<float> sines (std::initializer_list<std::pair<double, double>> freqAmp, int length)
{
    std::vector<float> x ((size_t) length, 0.0f);
    for (auto [f, a] : freqAmp)
        for (int t = 0; t < length; ++t)
            x[(size_t) t] += (float) (a * std::sin (juce::MathConstants<double>::twoPi * f * t / fs));
    return x;
}

std::vector<float> run (std::vector<float> x, ft::Mode mode, float n, float mix, int blockSize = 512)
{
    ft::Engine engine;
    engine.prepare (1);
    engine.setParameters (n, mode, mix);

    for (size_t start = 0; start < x.size(); start += (size_t) blockSize)
    {
        float* ch[] = { x.data() + start };
        engine.process (ch, 1, (int) std::min ((size_t) blockSize, x.size() - start));
    }
    return x;
}

// Amplitude of a bin-centred sine over a whole number of frames, skipping the warm-up.
double amplitudeAt (const std::vector<float>& y, double freq)
{
    const int start = 4 * ft::fftSize, length = 8 * ft::fftSize;
    std::complex<double> acc;
    for (int t = start; t < start + length; ++t)
        acc += (double) y[(size_t) t] * std::polar (1.0, -juce::MathConstants<double>::twoPi * freq * t / fs);
    return 2.0 * std::abs (acc) / length;
}

double maxDelayedError (const std::vector<float>& in, const std::vector<float>& out)
{
    double err = 0.0;
    for (size_t t = 2 * ft::fftSize; t < out.size(); ++t)
        err = std::max (err, (double) std::abs (out[t] - in[t - ft::latencySamples]));
    return err;
}

void testWeights()
{
    std::vector<float> mags (ft::numBins, 0.0f), w (ft::numBins);
    std::vector<int> scratch (ft::numBins);
    auto sum = [&] { double s = 0; for (auto v : w) s += v; return s; };

    ft::computeWeights (ft::Mode::Low, 2.5f, mags.data(), w.data(), scratch.data());
    check (w[0] == 1 && w[1] == 1 && w[2] == 0.5f && sum() == 2.5, "weights: Low n=2.5 keeps 0,1 and half of 2", sum());

    ft::computeWeights (ft::Mode::High, 1.0f, mags.data(), w.data(), scratch.data());
    check (w[2048] == 1 && sum() == 1, "weights: High n=1 keeps only Nyquist", sum());

    ft::computeWeights (ft::Mode::Odd, 2.5f, mags.data(), w.data(), scratch.data());
    check (w[1] == 1 && w[3] == 1 && w[5] == 0.5f && w[0] == 0 && w[2] == 0 && sum() == 2.5,
           "weights: Odd n=2.5 keeps 1,3 and half of 5", sum());

    ft::computeWeights (ft::Mode::Odd, 2048.0f, mags.data(), w.data(), scratch.data());
    check (sum() == 1024, "weights: Odd caps at the 1024 odd bins", sum());

    mags[700] = 3.0f; mags[10] = 2.0f; mags[1500] = 1.0f;
    ft::computeWeights (ft::Mode::Peak, 2.5f, mags.data(), w.data(), scratch.data());
    check (w[700] == 1 && w[10] == 1 && w[1500] == 0.5f && sum() == 2.5, "weights: Peak n=2.5 keeps two loudest + half", sum());
}

void testIdentityAndLatency()
{
    const int length = 16 * ft::fftSize;
    auto x = sines ({ { 440.0, 0.3 }, { 1234.5, 0.3 }, { 5000.0, 0.2 } }, length);

    check (maxDelayedError (x, run (x, ft::Mode::Low, 2048, 1.0f)) < 1e-3, "identity: Low n=2048 == delayed input",
           maxDelayedError (x, run (x, ft::Mode::Low, 2048, 1.0f)));

    const double dryErr = maxDelayedError (x, run (x, ft::Mode::Peak, 3, 0.0f));
    check (dryErr < 1e-7, "mix 0 == delayed input exactly", dryErr);

    std::vector<float> impulse ((size_t) length, 0.0f);
    impulse[5000] = 1.0f;
    auto y = run (impulse, ft::Mode::Low, 2048, 1.0f);
    const auto peak = std::max_element (y.begin(), y.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });
    check (peak - y.begin() == 5000 + ft::latencySamples, "latency: impulse arrives after 4096 samples",
           (double) (peak - y.begin() - 5000));
}

void testModes()
{
    const int length = 16 * ft::fftSize;
    const double fa = binFreq (30), fb = binFreq (600);

    auto x = sines ({ { fa, 0.5 }, { fb, 0.5 } }, length);
    auto low = run (x, ft::Mode::Low, 100, 1.0f);
    check (std::abs (amplitudeAt (low, fa) - 0.5) < 0.005, "Low n=100 keeps bin 30", amplitudeAt (low, fa));
    check (amplitudeAt (low, fb) < 0.5e-3, "Low n=100 removes bin 600 (< -60 dB)", amplitudeAt (low, fb));

    auto high = run (x, ft::Mode::High, 1500, 1.0f);
    check (amplitudeAt (high, fa) < 0.5e-3, "High n=1500 removes bin 30 (< -60 dB)", amplitudeAt (high, fa));
    check (std::abs (amplitudeAt (high, fb) - 0.5) < 0.005, "High n=1500 keeps bin 600", amplitudeAt (high, fb));

    // A bin-centred Hann-windowed sine occupies exactly 3 bins, so N=3 keeps the loud one whole.
    auto y = sines ({ { fa, 0.5 }, { fb, 0.05 } }, length);
    auto peak = run (y, ft::Mode::Peak, 3, 1.0f);
    check (std::abs (amplitudeAt (peak, fa) - 0.5) < 0.005, "Peak n=3 keeps the loud sine", amplitudeAt (peak, fa));
    check (amplitudeAt (peak, fb) < 0.05e-3, "Peak n=3 removes the quiet sine (< -60 dB)", amplitudeAt (peak, fb));
}

void testBlockSizeInvariance()
{
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
    std::vector<float> left (10 * ft::fftSize), right (left.size());
    for (size_t t = 0; t < left.size(); ++t) { left[t] = noise (rng); right[t] = noise (rng); }

    auto render = [&] (bool randomBlocks)
    {
        ft::Engine engine;
        engine.prepare (2);
        auto l = left, r = right;
        std::uniform_int_distribution<int> sizes (1, 3000);
        std::mt19937 blockRng (7);
        size_t start = 0;
        while (start < l.size())
        {
            // Automate N and mode on a fixed sample grid so both renders see identical parameters.
            const auto blockLen = (size_t) (randomBlocks ? sizes (blockRng) : 256);
            const auto len = std::min ({ blockLen, l.size() - start, 256 - start % 256 });
            const auto grid = (int) (start / 256);
            engine.setParameters ((float) (1 + (grid * 37) % 2048), (ft::Mode) ((grid / 40) % 4), 0.7f);
            float* ch[] = { l.data() + start, r.data() + start };
            engine.process (ch, 2, (int) len);
            start += len;
        }
        return std::make_pair (l, r);
    };

    auto a = render (false), b = render (true);
    double diff = 0.0;
    bool finite = true;
    for (size_t t = 0; t < left.size(); ++t)
    {
        diff = std::max ({ diff, (double) std::abs (a.first[t] - b.first[t]), (double) std::abs (a.second[t] - b.second[t]) });
        finite = finite && std::isfinite (b.first[t]) && std::isfinite (b.second[t]);
    }
    check (diff == 0.0 && finite, "random block sizes == fixed blocks (stereo, automated)", diff);
}

} // namespace

int main()
{
    testWeights();
    testIdentityAndLatency();
    testModes();
    testBlockSizeInvariance();
    std::printf ("%d failure(s)\n", failures);
    return failures;
}
