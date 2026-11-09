#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// A row controls one actual WAV (including optional close/OH/room stems).
// WAV assignments and round robin are deliberately not editable.
class MorphWavRow final : public juce::Component
{
public:
    MorphWavRow (SculptDrumsProcessor& p, const SculptDrumsProcessor::WavInfo& wav)
        : processor (p), wavId (wav.index)
    {
        const auto name = wav.path.fromLastOccurrenceOf ("/", false, false)
                                   .upToLastOccurrenceOf (".wav", false, true);
        title.setText ("S" + juce::String (wav.stage + 1) + "  " + name, juce::dontSendNotification);
        title.setColour (juce::Label::textColourId, juce::Colour (0xffe6dfce));
        title.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (title);

        gain.setRange (-24.0, 12.0, 0.1);
        gain.setSliderStyle (juce::Slider::LinearHorizontal);
        gain.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
        gain.setTextValueSuffix (" dB");
        gain.setValue (processor.getWavGainDb (wavId), juce::dontSendNotification);
        gain.setDoubleClickReturnValue (true, 0.0);
        gain.onValueChange = [this] { processor.setWavGainDb (wavId, (float) gain.getValue()); };
        addAndMakeVisible (gain);

        preview.setButtonText (juce::String::fromUTF8 ("▶"));
        preview.setTooltip ("Ascolta solo questo WAV, con il suo gain impostato");
        preview.onClick = [this] { processor.requestWavPreview (wavId); };
        addAndMakeVisible (preview);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4, 1);
        title.setBounds (r.removeFromLeft (310));
        preview.setBounds (r.removeFromRight (35).reduced (2, 3));
        gain.setBounds (r.reduced (5, 0));
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xffd7c6a3).withAlpha (0.11f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void syncFromProcessor()
    {
        const auto db = processor.getWavGainDb (wavId);
        if (std::abs ((float) gain.getValue() - db) > 0.08f)
            gain.setValue (db, juce::dontSendNotification);
    }

private:
    SculptDrumsProcessor& processor;
    int wavId = -1;
    juce::Label title;
    juce::Slider gain;
    juce::TextButton preview;
};
