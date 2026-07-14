#include "PluginProcessor.h"
#include "params/ParameterIds.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
    // Writes a short, valid stereo WAV file to a temp location and returns
    // it, for tests that need a real user-IR file to load. The caller is
    // responsible for deleting it afterwards.
    juce::File writeTestImpulseResponseFile()
    {
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("requiem_test_ir_" + juce::String (juce::Random::getSystemRandom().nextInt64()) + ".wav");

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        REQUIRE (stream != nullptr);

        auto writer = wavFormat.createWriterFor (stream,
            juce::AudioFormatWriterOptions()
                .withSampleRate (48000.0)
                .withNumChannels (2)
                .withBitsPerSample (16));
        REQUIRE (writer != nullptr);

        juce::AudioBuffer<float> irBuffer (2, 256);
        irBuffer.clear();
        irBuffer.setSample (0, 0, 1.0f);
        irBuffer.setSample (1, 0, 1.0f);

        writer->writeFromAudioSampleBuffer (irBuffer, 0, irBuffer.getNumSamples());
        writer.reset(); // flush/close before the caller reads the file back

        return file;
    }
}

TEST_CASE ("State round-trip preserves non-default values of every parameter", "[state]")
{
    RequiemAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto* decayParam = processor.apvts.getParameter (ParamIDs::decay);
    auto* preDelayParam = processor.apvts.getParameter (ParamIDs::preDelay);
    auto* dampingParam = processor.apvts.getParameter (ParamIDs::damping);
    auto* widthParam = processor.apvts.getParameter (ParamIDs::width);
    auto* mixParam = processor.apvts.getParameter (ParamIDs::mix);
    auto* outputParam = processor.apvts.getParameter (ParamIDs::output);

    REQUIRE (decayParam != nullptr);
    REQUIRE (preDelayParam != nullptr);
    REQUIRE (dampingParam != nullptr);
    REQUIRE (widthParam != nullptr);
    REQUIRE (mixParam != nullptr);
    REQUIRE (outputParam != nullptr);

    decayParam->setValueNotifyingHost (decayParam->convertTo0to1 (6.0f));
    preDelayParam->setValueNotifyingHost (preDelayParam->convertTo0to1 (120.0f));
    dampingParam->setValueNotifyingHost (dampingParam->convertTo0to1 (3500.0f));
    widthParam->setValueNotifyingHost (widthParam->convertTo0to1 (180.0f));
    mixParam->setValueNotifyingHost (mixParam->convertTo0to1 (70.0f));
    outputParam->setValueNotifyingHost (outputParam->convertTo0to1 (-4.5f));

    const auto savedDecay = decayParam->getValue();
    const auto savedPreDelay = preDelayParam->getValue();
    const auto savedDamping = dampingParam->getValue();
    const auto savedWidth = widthParam->getValue();
    const auto savedMix = mixParam->getValue();
    const auto savedOutput = outputParam->getValue();

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    REQUIRE (savedState.getSize() > 0);

    // Reset every parameter back to its default before restoring, so the
    // round-trip assertion below can't pass by accident.
    decayParam->setValueNotifyingHost (decayParam->getDefaultValue());
    preDelayParam->setValueNotifyingHost (preDelayParam->getDefaultValue());
    dampingParam->setValueNotifyingHost (dampingParam->getDefaultValue());
    widthParam->setValueNotifyingHost (widthParam->getDefaultValue());
    mixParam->setValueNotifyingHost (mixParam->getDefaultValue());
    outputParam->setValueNotifyingHost (outputParam->getDefaultValue());

    REQUIRE (decayParam->getValue() != Catch::Approx (savedDecay));
    REQUIRE (preDelayParam->getValue() != Catch::Approx (savedPreDelay));
    REQUIRE (dampingParam->getValue() != Catch::Approx (savedDamping));
    REQUIRE (widthParam->getValue() != Catch::Approx (savedWidth));
    REQUIRE (mixParam->getValue() != Catch::Approx (savedMix));
    REQUIRE (outputParam->getValue() != Catch::Approx (savedOutput));

    processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK (decayParam->getValue() == Catch::Approx (savedDecay).margin (1e-6));
    CHECK (preDelayParam->getValue() == Catch::Approx (savedPreDelay).margin (1e-6));
    CHECK (dampingParam->getValue() == Catch::Approx (savedDamping).margin (1e-6));
    CHECK (widthParam->getValue() == Catch::Approx (savedWidth).margin (1e-6));
    CHECK (mixParam->getValue() == Catch::Approx (savedMix).margin (1e-6));
    CHECK (outputParam->getValue() == Catch::Approx (savedOutput).margin (1e-6));
}

TEST_CASE ("State round-trip preserves an active user impulse-response file path", "[state]")
{
    const auto irFile = writeTestImpulseResponseFile();

    RequiemAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    REQUIRE (processor.loadUserImpulseResponseFile (irFile));
    REQUIRE (processor.isUsingUserImpulseResponse());

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    REQUIRE (savedState.getSize() > 0);

    // A fresh processor instance, as a host would create when reloading a
    // saved session.
    RequiemAudioProcessor restoredProcessor;
    restoredProcessor.prepareToPlay (48000.0, 512);
    restoredProcessor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK (restoredProcessor.isUsingUserImpulseResponse());
    CHECK (restoredProcessor.getUserImpulseResponseFile() == irFile);

    irFile.deleteFile();
}

TEST_CASE ("State round-trip falls back to procedural when no user IR is active", "[state]")
{
    RequiemAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    REQUIRE_FALSE (processor.isUsingUserImpulseResponse());

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);

    RequiemAudioProcessor restoredProcessor;
    restoredProcessor.prepareToPlay (48000.0, 512);
    restoredProcessor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK_FALSE (restoredProcessor.isUsingUserImpulseResponse());
}

TEST_CASE ("Restoring a state whose user IR file no longer exists falls back to procedural", "[state]")
{
    const auto irFile = writeTestImpulseResponseFile();

    RequiemAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    REQUIRE (processor.loadUserImpulseResponseFile (irFile));

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);

    irFile.deleteFile(); // simulate the IR file having moved/been deleted

    RequiemAudioProcessor restoredProcessor;
    restoredProcessor.prepareToPlay (48000.0, 512);
    CHECK_NOTHROW (restoredProcessor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize())));

    CHECK_FALSE (restoredProcessor.isUsingUserImpulseResponse());
}
