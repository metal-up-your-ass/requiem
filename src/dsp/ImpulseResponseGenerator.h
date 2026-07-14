#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Procedural impulse-response generation for Requiem's convolution reverb.
// Deliberately independent of juce::dsp::Convolution and of
// juce::AudioProcessor so it is directly unit-testable (see
// tests/ImpulseResponseGeneratorTests.cpp) and so it can run entirely off
// the audio thread: generation involves heap allocation (the returned
// AudioBuffer) and per-sample trig/exp calls, neither of which are
// real-time safe. Callers (ReverbEngine) only ever invoke this from
// prepare() or from a message-thread-only regeneration path - never from
// process().
//
// Approach: per channel, decorrelated white noise (a distinct juce::Random
// stream per channel, for stereo width in the tail) is run through a
// one-pole low-pass filter (the Damping control) and multiplied by an
// RT60-style exponential decay envelope sized so the envelope reaches
// -60 dBFS at t = decaySeconds. This is a simple but standard "filtered
// noise burst" algorithmic-reverb IR model - not a physically modelled
// room, but sufficient for a cinematic wash of cathedral/hall-style tail.
namespace ReverbIR
{
    // Decay is clamped to this range before sizing the generated buffer,
    // bounding both memory and the convolution engine's CPU cost regardless
    // of what a caller (or a corrupt saved state) requests.
    constexpr float minDecaySeconds = 0.1f;
    constexpr float maxDecaySeconds = 10.0f;

    // Damping is clamped to this range; the upper bound is also re-clamped
    // below Nyquist internally for whatever sample rate is passed in.
    constexpr float minDampingHz = 20.0f;
    constexpr float maxDampingHz = 20000.0f;

    // Generates a procedural impulse response. `numChannels` is clamped to
    // [1, 2] (mono or stereo); `seed` lets tests (and A/B channel
    // decorrelation) get reproducible, distinct noise sequences. Not
    // real-time safe - allocates the returned buffer and does per-sample
    // exp()/random calls. Call only from prepare() or a message-thread-only
    // regeneration path, never from process().
    juce::AudioBuffer<float> generateProceduralImpulseResponse (double sampleRate,
                                                                  float decaySeconds,
                                                                  float dampingHz,
                                                                  int numChannels,
                                                                  int seed = 1);
}
