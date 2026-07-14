#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class RequiemAudioProcessor;

// A simple, functional v0.1 editor: one rotary slider per parameter, bound
// to the APVTS via SliderAttachment, plus a "Load IR..."/"Clear IR" pair of
// buttons for the optional user impulse-response override. A custom
// vector-drawn GUI is a later milestone; this is deliberately plain but
// fully wired and usable.
class RequiemAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit RequiemAudioProcessorEditor (RequiemAudioProcessor& processorToEdit);
    ~RequiemAudioProcessorEditor() override;

    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // One knob + label per float/choice parameter, in signal-flow order.
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText);
    void updateIrStatusLabel();

    RequiemAudioProcessor& audioProcessor;

    Knob decayKnob;
    Knob preDelayKnob;
    Knob dampingKnob;
    Knob widthKnob;
    Knob mixKnob;
    Knob outputKnob;

    // Space/Early-Late-Balance/Modulation/Freeze: a second row, added for
    // the M1 DSP-completion parameters. Space is a choice parameter (a
    // ComboBox suits it far better than a rotary slider); Freeze is a
    // boolean toggle.
    juce::Label spaceLabel;
    juce::ComboBox spaceCombo;
    std::unique_ptr<ComboBoxAttachment> spaceAttachment;

    Knob earlyLateBalanceKnob;
    Knob modulationKnob;

    juce::ToggleButton freezeButton { "Freeze" };
    std::unique_ptr<ButtonAttachment> freezeAttachment;

    juce::TextButton loadIrButton { "Load IR..." };
    juce::TextButton clearIrButton { "Clear IR" };
    juce::Label irStatusLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RequiemAudioProcessorEditor)
};
