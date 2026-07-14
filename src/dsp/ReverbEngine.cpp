#include "ReverbEngine.h"

ReverbEngine::ReverbEngine() = default;

void ReverbEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = static_cast<int> (spec.numChannels);

    const auto stereo = numChannels >= 2 ? juce::dsp::Convolution::Stereo::yes
                                          : juce::dsp::Convolution::Stereo::no;

    // Load the active impulse response (procedural or user override) before
    // convolution.prepare(), per juce::dsp::Convolution's own recommended
    // ordering ("it is recommended to call loadImpulseResponse() *before*
    // prepare() if a specific IR must be active during the first process()
    // call") - otherwise the very first block after prepare() could run
    // against whatever IR (if any) happened to be active previously.
    if (usingUserImpulseResponse && userImpulseResponseFile.existsAsFile())
    {
        convolution.loadImpulseResponse (userImpulseResponseFile,
                                          stereo,
                                          juce::dsp::Convolution::Trim::yes,
                                          0,
                                          juce::dsp::Convolution::Normalise::yes);
    }
    else
    {
        // Force (re)generation even if Decay/Damping happen to match the
        // last-generated values: prepare() can run after a sample-rate
        // change, which invalidates any previously generated buffer (it was
        // sized and sampled for the old rate).
        lastGeneratedDecaySeconds = -1.0f;
        lastGeneratedDampingHz = -1.0f;

        loadProceduralImpulseResponse (requestedDecaySeconds.load (std::memory_order_relaxed),
                                        requestedDampingHz.load (std::memory_order_relaxed));
    }

    convolution.prepare (spec);

    preDelayLine.prepare (spec);

    outputGain.setRampDurationSeconds (smoothingTimeSeconds);
    outputGain.prepare (spec);

    dryWetMixer.prepare (spec);

    // juce::dsp::Convolution's default configuration (used here) is
    // zero-latency and uniformly partitioned, so this is normally 0 -
    // queried rather than assumed so the plugin stays correct if a fixed-
    // latency configuration is ever adopted instead.
    latencySamples = convolution.getLatency();
    dryWetMixer.setWetLatency (static_cast<float> (latencySamples));

    // See docs/architecture.md ("DryWetMixer gotcha") for the full
    // explanation: priming the real target here, before reset() runs, means
    // the mixer is already sitting at the correct dry/wet balance for the
    // very first process() call instead of ramping up from "fully wet" over
    // its internal ~50 ms default ramp.
    dryWetMixer.setWetMixProportion (lastMixProportion);

    preDelayMsSmoothed.reset (sampleRate, smoothingTimeSeconds);
    preDelayMsSmoothed.setCurrentAndTargetValue (lastPreDelayMs);
    widthAmountSmoothed.reset (sampleRate, smoothingTimeSeconds);
    widthAmountSmoothed.setCurrentAndTargetValue (lastWidthPercent * 0.01f);

    reset();

    // Prime the delay line's sample count immediately - reset() clears
    // buffer/position state but knows nothing about lastPreDelayMs - so the
    // very first process() call already uses the correct Pre-Delay instead
    // of ramping up from 0.
    const auto preDelaySamples = juce::jlimit (0.0f,
                                                static_cast<float> (preDelayLine.getMaximumDelayInSamples()),
                                                lastPreDelayMs * 0.001f * static_cast<float> (sampleRate));
    preDelayLine.setDelay (preDelaySamples);
}

void ReverbEngine::reset()
{
    preDelayLine.reset();
    convolution.reset();
    outputGain.reset();
    dryWetMixer.reset();
}

void ReverbEngine::setPreDelayMs (float newPreDelayMs)
{
    lastPreDelayMs = newPreDelayMs;
    preDelayMsSmoothed.setTargetValue (newPreDelayMs);
}

void ReverbEngine::setWidthPercent (float newWidthPercent)
{
    lastWidthPercent = newWidthPercent;
    widthAmountSmoothed.setTargetValue (newWidthPercent * 0.01f);
}

void ReverbEngine::setMixProportion (float newProportion01)
{
    lastMixProportion = newProportion01;
    // juce::dsp::DryWetMixer smooths this internally (~50 ms ramp), so no
    // extra SmoothedValue is needed on this side - re-applying the target
    // every block is cheap and keeps automation zipper-free.
    dryWetMixer.setWetMixProportion (newProportion01);
}

void ReverbEngine::setOutputDb (float newOutputDb)
{
    outputGain.setGainDecibels (newOutputDb);
}

void ReverbEngine::setDecaySeconds (float newDecaySeconds)
{
    requestedDecaySeconds.store (newDecaySeconds, std::memory_order_relaxed);
}

void ReverbEngine::setDampingHz (float newDampingHz)
{
    requestedDampingHz.store (newDampingHz, std::memory_order_relaxed);
}

void ReverbEngine::regenerateImpulseResponseIfNeeded()
{
    if (usingUserImpulseResponse)
        return; // A user IR override is active; Decay/Damping don't drive the procedural generator while it is.

    const auto decay = requestedDecaySeconds.load (std::memory_order_relaxed);
    const auto damping = requestedDampingHz.load (std::memory_order_relaxed);

    // Small epsilon avoids reloading on floating-point noise from repeated
    // identical parameter pushes (e.g. a host re-sending the same
    // automation value on every block).
    constexpr float epsilon = 1.0e-3f;

    if (std::abs (decay - lastGeneratedDecaySeconds) < epsilon
        && std::abs (damping - lastGeneratedDampingHz) < epsilon)
        return;

    loadProceduralImpulseResponse (decay, damping);
}

bool ReverbEngine::loadUserImpulseResponse (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const auto stereo = numChannels >= 2 ? juce::dsp::Convolution::Stereo::yes
                                          : juce::dsp::Convolution::Stereo::no;

    convolution.loadImpulseResponse (file, stereo, juce::dsp::Convolution::Trim::yes, 0,
                                      juce::dsp::Convolution::Normalise::yes);

    usingUserImpulseResponse = true;
    userImpulseResponseFile = file;
    return true;
}

void ReverbEngine::clearUserImpulseResponse()
{
    if (! usingUserImpulseResponse)
        return;

    usingUserImpulseResponse = false;
    userImpulseResponseFile = juce::File();

    // Reload the procedural IR immediately (rather than waiting for the
    // next regenerateImpulseResponseIfNeeded() tick) so the revert is
    // effective as soon as this call returns.
    loadProceduralImpulseResponse (requestedDecaySeconds.load (std::memory_order_relaxed),
                                    requestedDampingHz.load (std::memory_order_relaxed));
}

void ReverbEngine::loadProceduralImpulseResponse (float decaySeconds, float dampingHz)
{
    auto impulseResponse = ReverbIR::generateProceduralImpulseResponse (sampleRate, decaySeconds, dampingHz, numChannels);

    const auto stereo = numChannels >= 2 ? juce::dsp::Convolution::Stereo::yes
                                          : juce::dsp::Convolution::Stereo::no;

    convolution.loadImpulseResponse (std::move (impulseResponse), sampleRate, stereo,
                                      juce::dsp::Convolution::Trim::no,
                                      juce::dsp::Convolution::Normalise::yes);

    lastGeneratedDecaySeconds = decaySeconds;
    lastGeneratedDampingHz = dampingHz;
}

void ReverbEngine::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numSamples = block.getNumSamples();

    if (numSamples == 0)
        return;

    // Capture the pre-processing signal as "dry" before Pre-Delay/
    // convolution/Width touch `block`. DryWetMixer internally delays this
    // by getLatencySamples() (set via setWetLatency in prepare()) so it
    // stays time-aligned with the (normally zero-latency) wet path below.
    // Pre-Delay is deliberately *not* part of this compensation - it is an
    // audible effect parameter (the gap before the tail), not something to
    // hide from the dry signal.
    dryWetMixer.pushDrySamples (block);

    const auto preDelaySamples = juce::jlimit (0.0f,
                                                static_cast<float> (preDelayLine.getMaximumDelayInSamples()),
                                                preDelayMsSmoothed.skip (static_cast<int> (numSamples)) * 0.001f * static_cast<float> (sampleRate));
    preDelayLine.setDelay (preDelaySamples);

    juce::dsp::ProcessContextReplacing<float> context (block);

    preDelayLine.process (context);
    convolution.process (context);

    applyWidth (block);

    dryWetMixer.mixWetSamples (block);

    outputGain.process (context);
}

void ReverbEngine::applyWidth (juce::dsp::AudioBlock<float>& block) noexcept
{
    if (block.getNumChannels() != 2)
        return; // Width is only meaningful for a stereo wet signal.

    const auto numSamples = block.getNumSamples();
    const auto width = widthAmountSmoothed.skip (static_cast<int> (numSamples));

    auto* left = block.getChannelPointer (0);
    auto* right = block.getChannelPointer (1);

    for (size_t i = 0; i < numSamples; ++i)
    {
        const auto mid = 0.5f * (left[i] + right[i]);
        const auto side = 0.5f * (left[i] - right[i]) * width;

        left[i] = mid + side;
        right[i] = mid - side;
    }
}
