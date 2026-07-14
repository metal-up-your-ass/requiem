#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "params/ParameterIds.h"

namespace
{
    constexpr int knobSize = 90;
    constexpr int textBoxHeight = 20;
    constexpr int labelHeight = 20;
    constexpr int margin = 20;
    constexpr int numKnobs = 6;
    constexpr int editorWidth = margin * 2 + numKnobs * knobSize + (numKnobs - 1) * margin;
    constexpr int irRowHeight = 30;
    constexpr int editorHeight = margin * 3 + labelHeight + knobSize + textBoxHeight + irRowHeight;
}

RequiemAudioProcessorEditor::RequiemAudioProcessorEditor (RequiemAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (&processorToEdit),
      audioProcessor (processorToEdit)
{
    configureKnob (decayKnob, ParamIDs::decay, "Decay");
    configureKnob (preDelayKnob, ParamIDs::preDelay, "Pre-Delay");
    configureKnob (dampingKnob, ParamIDs::damping, "Damping");
    configureKnob (widthKnob, ParamIDs::width, "Width");
    configureKnob (mixKnob, ParamIDs::mix, "Mix");
    configureKnob (outputKnob, ParamIDs::output, "Output");

    loadIrButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Load impulse response...",
                                                             juce::File(),
                                                             "*.wav;*.aif;*.aiff");

        constexpr auto chooserFlags = juce::FileBrowserComponent::openMode
                                       | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (file != juce::File())
                audioProcessor.loadUserImpulseResponseFile (file);

            updateIrStatusLabel();
        });
    };
    addAndMakeVisible (loadIrButton);

    clearIrButton.onClick = [this]
    {
        audioProcessor.clearUserImpulseResponseFile();
        updateIrStatusLabel();
    };
    addAndMakeVisible (clearIrButton);

    irStatusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (irStatusLabel);
    updateIrStatusLabel();

    setResizable (false, false);
    setSize (editorWidth, editorHeight);
}

RequiemAudioProcessorEditor::~RequiemAudioProcessorEditor() = default;

void RequiemAudioProcessorEditor::configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobSize, textBoxHeight);
    addAndMakeVisible (knob.slider);

    knob.label.setText (labelText, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    // false => label sits above the slider it tracks; JUCE repositions it
    // automatically whenever the slider's bounds change, so resized() only
    // needs to place the sliders themselves.
    knob.label.attachToComponent (&knob.slider, false);
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, parameterId, knob.slider);
}

void RequiemAudioProcessorEditor::updateIrStatusLabel()
{
    if (audioProcessor.isUsingUserImpulseResponse())
        irStatusLabel.setText ("IR: " + audioProcessor.getUserImpulseResponseFile().getFileName(), juce::dontSendNotification);
    else
        irStatusLabel.setText ("IR: procedural (Decay/Damping)", juce::dontSendNotification);
}

void RequiemAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (margin);

    auto irRow = bounds.removeFromBottom (irRowHeight);
    loadIrButton.setBounds (irRow.removeFromLeft (100));
    irRow.removeFromLeft (margin / 2);
    clearIrButton.setBounds (irRow.removeFromLeft (80));
    irRow.removeFromLeft (margin / 2);
    irStatusLabel.setBounds (irRow);

    bounds.removeFromBottom (margin);
    bounds.removeFromTop (labelHeight); // room for the attached labels above each knob

    const auto slotWidth = bounds.getWidth() / numKnobs;

    for (auto* knob : { &decayKnob, &preDelayKnob, &dampingKnob, &widthKnob, &mixKnob, &outputKnob })
        knob->slider.setBounds (bounds.removeFromLeft (slotWidth).reduced (margin / 2, 0));
}
