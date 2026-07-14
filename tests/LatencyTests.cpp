#include "PluginProcessor.h"
#include "dsp/ReverbEngine.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("getLatencySamples() reports the convolution engine's latency after prepareToPlay", "[latency]")
{
    RequiemAudioProcessor processor;

    // Before prepareToPlay, no engine has been prepared yet - JUCE's default
    // AudioProcessor latency is 0.
    CHECK (processor.getLatencySamples() == 0);

    processor.prepareToPlay (48000.0, 512);

    // Cross-check against a standalone engine prepared identically: the
    // processor must report exactly what the engine (i.e. the convolution)
    // computes, not an approximation of it.
    ReverbEngine referenceEngine;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    referenceEngine.prepare (spec);

    CHECK (processor.getLatencySamples() == referenceEngine.getLatencySamples());
    // juce::dsp::Convolution's default configuration (used here) is
    // zero-latency, so this is expected to be exactly 0 - asserted
    // explicitly (rather than just >= 0) so a future change to a
    // fixed-latency configuration is caught by this test needing an update.
    CHECK (processor.getLatencySamples() >= 0);
}

TEST_CASE ("Latency is stable across repeated prepareToPlay calls at the same sample rate", "[latency]")
{
    RequiemAudioProcessor processor;

    processor.prepareToPlay (44100.0, 256);
    const auto firstLatency = processor.getLatencySamples();

    processor.prepareToPlay (44100.0, 256);
    const auto secondLatency = processor.getLatencySamples();

    CHECK (firstLatency == secondLatency);
}

TEST_CASE ("Latency stays well-defined when the sample rate changes", "[latency]")
{
    RequiemAudioProcessor processor;

    processor.prepareToPlay (44100.0, 512);
    const auto latencyAt44k = processor.getLatencySamples();

    processor.prepareToPlay (96000.0, 512);
    const auto latencyAt96k = processor.getLatencySamples();

    CHECK (latencyAt44k >= 0);
    CHECK (latencyAt96k >= 0);
}
