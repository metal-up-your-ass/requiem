#include "dsp/ImpulseResponseGenerator.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
    constexpr double testSampleRate = 48000.0;

    double sumOfSquares (const juce::AudioBuffer<float>& buffer)
    {
        double total = 0.0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                total += static_cast<double> (data[sample]) * static_cast<double> (data[sample]);
        }

        return total;
    }
}

TEST_CASE ("Generated impulse response is finite for the full parameter range", "[dsp][ir][robustness]")
{
    const float decayValues[] = { ReverbIR::minDecaySeconds, 0.5f, 1.0f, 2.5f, 5.0f, ReverbIR::maxDecaySeconds };
    const float dampingValues[] = { ReverbIR::minDampingHz, 500.0f, 2000.0f, 8000.0f, ReverbIR::maxDampingHz };

    for (const auto decay : decayValues)
    {
        for (const auto damping : dampingValues)
        {
            const auto ir = ReverbIR::generateProceduralImpulseResponse (testSampleRate, decay, damping, 2);

            CHECK (ir.getNumChannels() == 2);
            CHECK (ir.getNumSamples() > 0);
            CHECK (TestHelpers::allSamplesFinite (ir));
        }
    }
}

TEST_CASE ("Generated impulse response is finite at extreme/unusual sample rates", "[dsp][ir][robustness]")
{
    for (const double sampleRate : { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto ir = ReverbIR::generateProceduralImpulseResponse (sampleRate, 2.0f, 8000.0f, 2);

        CHECK (TestHelpers::allSamplesFinite (ir));
    }
}

TEST_CASE ("Mono and stereo channel counts are honoured and clamped", "[dsp][ir]")
{
    CHECK (ReverbIR::generateProceduralImpulseResponse (testSampleRate, 1.0f, 8000.0f, 1).getNumChannels() == 1);
    CHECK (ReverbIR::generateProceduralImpulseResponse (testSampleRate, 1.0f, 8000.0f, 2).getNumChannels() == 2);

    // Out-of-range channel counts are clamped to [1, 2] rather than
    // crashing or producing a degenerate (0-channel) buffer.
    CHECK (ReverbIR::generateProceduralImpulseResponse (testSampleRate, 1.0f, 8000.0f, 0).getNumChannels() == 1);
    CHECK (ReverbIR::generateProceduralImpulseResponse (testSampleRate, 1.0f, 8000.0f, 8).getNumChannels() == 2);
}

TEST_CASE ("Larger Decay produces a longer impulse response with more total tail energy", "[dsp][ir]")
{
    // Same seed (default) and Damping for both calls: the only difference
    // is Decay, so any difference in outcome is attributable to it alone.
    const auto shortIr = ReverbIR::generateProceduralImpulseResponse (testSampleRate, 1.0f, 8000.0f, 2);
    const auto longIr = ReverbIR::generateProceduralImpulseResponse (testSampleRate, 5.0f, 8000.0f, 2);

    CHECK (longIr.getNumSamples() > shortIr.getNumSamples());
    CHECK (sumOfSquares (longIr) > sumOfSquares (shortIr));
}

TEST_CASE ("Decay is clamped to the documented [min, max] range", "[dsp][ir]")
{
    const auto belowMin = ReverbIR::generateProceduralImpulseResponse (testSampleRate, 0.0f, 8000.0f, 2);
    const auto atMin = ReverbIR::generateProceduralImpulseResponse (testSampleRate, ReverbIR::minDecaySeconds, 8000.0f, 2);
    CHECK (belowMin.getNumSamples() == atMin.getNumSamples());

    const auto aboveMax = ReverbIR::generateProceduralImpulseResponse (testSampleRate, 1000.0f, 8000.0f, 2);
    const auto atMax = ReverbIR::generateProceduralImpulseResponse (testSampleRate, ReverbIR::maxDecaySeconds, 8000.0f, 2);
    CHECK (aboveMax.getNumSamples() == atMax.getNumSamples());
}

TEST_CASE ("Left and right channels are decorrelated (stereo width in the tail)", "[dsp][ir]")
{
    const auto ir = ReverbIR::generateProceduralImpulseResponse (testSampleRate, 2.0f, 8000.0f, 2);

    REQUIRE (ir.getNumChannels() == 2);

    bool anyDifferent = false;

    for (int i = 0; i < ir.getNumSamples(); ++i)
    {
        if (std::abs (ir.getSample (0, i) - ir.getSample (1, i)) > 1.0e-12f)
        {
            anyDifferent = true;
            break;
        }
    }

    CHECK (anyDifferent);
}
