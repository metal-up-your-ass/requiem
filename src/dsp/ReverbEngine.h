#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "ImpulseResponseGenerator.h"

// The complete Requiem signal path, independent of juce::AudioProcessor so
// it can be exercised directly by unit tests without instantiating a full
// plugin (see tests/EngineTests.cpp). Owns all DSP state; every buffer/
// filter/delay-line/convolution engine is allocated in prepare() and never
// reallocated on the audio thread.
//
// Signal flow (see docs/architecture.md for the full diagram and the
// latency-compensation rationale):
//
//   input -> Pre-Delay -> Convolution (wet) -> Width (M/S, wet only)
//         -> Dry/Wet Mix (latency-compensated) -> Output trim
//
// The impulse response driving the Convolution stage is generated
// procedurally off the audio thread (see ImpulseResponseGenerator.h) from
// the Decay/Damping parameters, or - if a user IR has been loaded - read
// from an audio file. Decay/Damping changes only ever update an atomic
// "requested value"; the actual (re)generation and
// juce::dsp::Convolution::loadImpulseResponse call happens only inside
// regenerateImpulseResponseIfNeeded()/loadUserImpulseResponse(), which the
// owning PluginProcessor calls from a message-thread juce::Timer - never
// from process(). This is the "ROBUSTNESS first" v0.1 approach called out
// in the DSP spec: no attempt is made to interpolate/crossfade between old
// and new IRs beyond whatever juce::dsp::Convolution itself does internally
// when a new IR is loaded while processing continues.
class ReverbEngine
{
public:
    ReverbEngine();

    // Allocates all DSP state and (re)generates the initial impulse
    // response (procedural, from the last commanded Decay/Damping, or the
    // active user IR file if one was loaded before this call). Must be
    // called (and completed) before the first process() call, and again
    // whenever sample rate/block size/channel count change. Not real-time
    // safe - allocates and does file/generation work; call only from the
    // message thread (e.g. AudioProcessor::prepareToPlay).
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Clears all filter/delay-line/convolution state without deallocating.
    // Safe to call from the audio thread (e.g. on playback stop/loop).
    void reset();

    // Processes `block` in place. `block` must have at most the maximum
    // sample/channel counts declared to prepare(); a zero-sample block is a
    // safe no-op. No allocation occurs here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

    //==============================================================================
    // Real-time-safe parameter setters: smoothed, no allocation/locks. Safe
    // to call every block from the audio thread.
    void setPreDelayMs (float newPreDelayMs);
    void setWidthPercent (float newWidthPercent);
    void setMixProportion (float newProportion01);
    void setOutputDb (float newOutputDb);

    // Real-time-safe to *call* - this only stores the requested value in an
    // atomic. No allocation, no impulse-response regeneration happens here;
    // that only occurs inside regenerateImpulseResponseIfNeeded() (message
    // thread only, see below). Safe to call every block from the audio
    // thread (e.g. so the processor can always forward the current APVTS
    // value without needing to know which thread it's on).
    void setDecaySeconds (float newDecaySeconds);
    void setDampingHz (float newDampingHz);

    // Message-thread only. Regenerates and loads a new procedural impulse
    // response if Decay and/or Damping have changed since the last
    // generated IR, and no user IR override is currently active. A cheap
    // no-op otherwise, so it is safe to call frequently (e.g. from a
    // ~20 Hz juce::Timer in PluginProcessor).
    void regenerateImpulseResponseIfNeeded();

    // Message-thread only. Loads a user-supplied impulse-response audio
    // file (WAV/AIFF/etc, anything juce::AudioFormatManager's basic formats
    // support), overriding the procedural generator until
    // clearUserImpulseResponse() is called. Returns false, without changing
    // any state, if the file doesn't exist or isn't readable as audio.
    bool loadUserImpulseResponse (const juce::File& file);

    // Message-thread only. Reverts to the procedural generator, discarding
    // any active user IR override, and forces the next
    // regenerateImpulseResponseIfNeeded() call to reload.
    void clearUserImpulseResponse();

    bool isUsingUserImpulseResponse() const noexcept { return usingUserImpulseResponse; }
    juce::File getUserImpulseResponseFile() const { return userImpulseResponseFile; }

    // Latency reported by the convolution engine (samples), valid after
    // prepare() has run. juce::dsp::Convolution's default configuration
    // (used here) is zero-latency/uniformly partitioned, so this is
    // normally 0, but it is queried rather than assumed so the plugin stays
    // correct if that ever changes.
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    void loadProceduralImpulseResponse (float decaySeconds, float dampingHz);
    void applyWidth (juce::dsp::AudioBlock<float>& block) noexcept;

    static constexpr double smoothingTimeSeconds = 0.05;
    // 250 ms at up to 192 kHz, plus a small margin - sized once so
    // setMaximumDelayInSamples() never needs to be called again (that call
    // is not real-time safe) regardless of host sample rate.
    static constexpr int maxPreDelaySamples = static_cast<int> (0.25 * 192000.0) + 4;

    double sampleRate = 44100.0;
    int numChannels = 2;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelayLine { maxPreDelaySamples };
    juce::dsp::Convolution convolution;
    juce::dsp::Gain<float> outputGain;

    // Sized generously above any realistic convolution latency (the default
    // zero-latency configuration reports 0) so setWetLatency() never
    // exceeds the mixer's internal delay-line capacity.
    juce::dsp::DryWetMixer<float> dryWetMixer { 4096 };

    // Pre-Delay is a filter-coefficient-style parameter (recomputing the
    // delay-line's sample count is cheap, but modulating a fractional delay
    // continuously per-sample is not needed here), so - like Overture's
    // Tight/Tone frequencies - it is smoothed and re-applied once per block.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> preDelayMsSmoothed;
    // Width is a plain per-sample multiply once resolved to a scalar per
    // block, smoothed the same way to avoid zipper noise on automation.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthAmountSmoothed;

    // Last commanded values (ParameterLayout defaults until a setter is
    // called), re-applied to the smoothers on every prepare() so re-prepare
    // (sample-rate change, etc.) never resets a live parameter back to a
    // default.
    float lastPreDelayMs = 20.0f;
    float lastWidthPercent = 100.0f;
    float lastMixProportion = 0.35f;

    // Decay/Damping: the "requested" value is just an atomic, safe to write
    // from any thread; the "last generated" value is message-thread-only
    // state used to detect changes in regenerateImpulseResponseIfNeeded().
    std::atomic<float> requestedDecaySeconds { 2.5f };
    std::atomic<float> requestedDampingHz { 8000.0f };
    float lastGeneratedDecaySeconds = -1.0f; // forces generation on first prepare()
    float lastGeneratedDampingHz = -1.0f;

    bool usingUserImpulseResponse = false;
    juce::File userImpulseResponseFile;

    int latencySamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbEngine)
};
