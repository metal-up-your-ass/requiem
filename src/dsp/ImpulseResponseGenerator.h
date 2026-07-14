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
// -60 dBFS at t = decaySeconds - this is the diffuse "late tail" layer. A
// second, independent layer of sparse discrete "early reflection" taps
// (see SpaceType below) is generated ahead of/within the same buffer and
// blended against the diffuse layer via an equal-power Early/Late Balance
// crossfade. This is a simple but standard "filtered noise burst" (plus a
// discrete early-reflection train) algorithmic-reverb IR model - not a
// physically modelled room, but sufficient for a cinematic wash of
// cathedral/hall/chamber-style tails.
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

    // Shapes the discrete early-reflection layer generated ahead of the
    // diffuse late tail (see generateProceduralImpulseResponse() below).
    // Space does *not* change the late diffuse tail itself - that stays
    // governed purely by Decay/Damping, as in the v0.1 model - only the
    // density/spread/character of the early reflections layered on top of
    // it. Cathedral = long, dense, widely spread reflections; Hall = the
    // balanced default character; Chamber = short, sparse, tight
    // reflections.
    enum class SpaceType
    {
        cathedral,
        hall,
        chamber
    };

    // Generates a procedural impulse response.
    //
    // `numChannels` is clamped to [1, 2] (mono or stereo); `seed` lets tests
    // (and A/B channel decorrelation) get reproducible, distinct noise
    // sequences.
    //
    // `earlyLateBalance01` (clamped to [0, 1]) is an equal-power crossfade
    // between the discrete early-reflection layer (1.0 gain at 0.0) and the
    // diffuse late tail (1.0 gain at 1.0); the default (0.8) keeps the
    // diffuse tail dominant, close to the v0.1-era character, while still
    // giving the early layer some presence.
    //
    // `freeze`, when true, replaces the usual RT60-style exponential decay
    // envelope on the diffuse layer with a flat (non-decaying) one at full
    // gain, so the tail's spectral content sustains for the full generated
    // buffer length instead of dying away - and suppresses the early-
    // reflection layer entirely, for a clean sustained pad/wash. Both of
    // these override `earlyLateBalance01` unconditionally while frozen (a
    // frozen pad is always the full sustained diffuse wash, never a
    // sustained early-reflection pattern). The buffer is still bounded to
    // `decaySeconds` worth of samples (a convolution engine processes a
    // finite kernel, so this is a bounded sustain, not a literal infinite
    // loop) - see docs/architecture.md.
    //
    // Not real-time safe - allocates the returned buffer and does
    // per-sample exp()/random calls. Call only from prepare() or a
    // message-thread-only regeneration path, never from process().
    juce::AudioBuffer<float> generateProceduralImpulseResponse (double sampleRate,
                                                                  float decaySeconds,
                                                                  float dampingHz,
                                                                  int numChannels,
                                                                  SpaceType space = SpaceType::hall,
                                                                  float earlyLateBalance01 = 0.8f,
                                                                  bool freeze = false,
                                                                  int seed = 1);
}
