#pragma once

// Central definition of all AudioProcessorValueTreeState parameter IDs for
// Requiem. See docs/architecture.md for the corresponding signal-flow
// diagram.
//
// FROZEN AS OF THE v0.1 PARAMETER LAYOUT:
// Parameter IDs below must NEVER change once shipped - saved sessions and
// presets persist the APVTS state keyed by these string IDs, and renaming or
// removing one would silently break every user's saved state. Ranges,
// defaults, and skew MAY still be refined during voicing/tuning milestones;
// only the IDs themselves are frozen.
namespace ParamIDs
{
    // Reverb time: controls both the length of the procedurally generated
    // impulse response and its RT60-style exponential decay envelope.
    inline constexpr auto decay = "decay";

    // Delay, in milliseconds, before the wet (convolved) signal begins -
    // separates the direct sound from the onset of the reverb tail.
    inline constexpr auto preDelay = "preDelay";

    // HF damping cutoff: the one-pole low-pass frequency applied to the
    // procedural impulse response's filtered-noise tail. Higher = brighter/
    // less damped tail, lower = darker/more damped.
    inline constexpr auto damping = "damping";

    // Stereo width applied to the wet signal only, via mid/side scaling.
    // 0 = mono, 100 = the convolution engine's natural stereo image,
    // 200 = exaggerated/extra-wide.
    inline constexpr auto width = "width";

    // Dry/wet mix. At 0% the plugin is a delay-compensated passthrough of
    // the input (see ReverbEngine's DryWetMixer usage).
    inline constexpr auto mix = "mix";

    // Output trim, applied after the dry/wet mix.
    inline constexpr auto output = "output";
}

// Not an APVTS parameter (it's a string, not automatable) - the path of an
// optional user-loaded impulse response file, persisted alongside the APVTS
// state as a plain XML attribute. See PluginProcessor::getStateInformation/
// setStateInformation and ReverbEngine::loadUserImpulseResponse.
namespace StateKeys
{
    inline constexpr auto userIrPath = "userIrPath";
}
