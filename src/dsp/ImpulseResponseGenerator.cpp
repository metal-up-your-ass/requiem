#include "ImpulseResponseGenerator.h"

#include <cmath>

namespace ReverbIR
{
    juce::AudioBuffer<float> generateProceduralImpulseResponse (double sampleRate,
                                                                  float decaySeconds,
                                                                  float dampingHz,
                                                                  int numChannels,
                                                                  int seed)
    {
        const auto clampedDecay = juce::jlimit (minDecaySeconds, maxDecaySeconds, decaySeconds);

        // Keep the damping cutoff comfortably below Nyquist regardless of
        // sample rate, mirroring the defensive clamping pattern used for
        // filter cutoffs elsewhere in the suite (see e.g. Overture's
        // clampBelowNyquist) - a cutoff at or above Nyquist would produce a
        // degenerate/unstable one-pole coefficient below.
        const auto nyquist = static_cast<float> (sampleRate * 0.5);
        const auto clampedDamping = juce::jlimit (minDampingHz, juce::jmin (maxDampingHz, nyquist * 0.9f), dampingHz);

        const auto lengthSamples = juce::jmax (1, static_cast<int> (std::round (clampedDecay * sampleRate)));
        const auto channels = juce::jlimit (1, 2, numChannels);

        juce::AudioBuffer<float> impulseResponse (channels, lengthSamples);

        // One-pole low-pass coefficient (RC/exponential-smoothing form):
        // higher dampingHz -> smaller `a` -> less smoothing -> brighter tail.
        const auto a = std::exp (-juce::MathConstants<float>::twoPi * clampedDamping / static_cast<float> (sampleRate));

        // -60 dB (RT60) point at t == clampedDecay: ln(1000) = 6.907755...
        constexpr float negSixtyDbTimeConstant = 6.90775528f;
        const auto decayRate = negSixtyDbTimeConstant / clampedDecay;

        for (int channel = 0; channel < channels; ++channel)
        {
            // Distinct, deterministic noise stream per channel and per
            // caller-supplied seed: decorrelated L/R tails are what gives a
            // procedurally generated IR any sense of stereo width, and
            // determinism (rather than juce::Random::getSystemRandom())
            // keeps this function's output reproducible for unit tests.
            juce::Random random (static_cast<juce::int64> (seed) * 104729 + static_cast<juce::int64> (channel) * 7919 + 17);

            auto* data = impulseResponse.getWritePointer (channel);
            float filterState = 0.0f;

            for (int i = 0; i < lengthSamples; ++i)
            {
                const auto whiteNoise = random.nextFloat() * 2.0f - 1.0f;
                filterState = (1.0f - a) * whiteNoise + a * filterState;

                const auto t = static_cast<float> (static_cast<double> (i) / sampleRate);
                const auto envelope = std::exp (-t * decayRate);

                data[i] = filterState * envelope;
            }
        }

        return impulseResponse;
    }
}
