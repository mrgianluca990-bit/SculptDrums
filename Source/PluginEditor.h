#pragma once
#include "PluginProcessor.h"

struct SculptLookAndFeel : public juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

class SculptDrumsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SculptDrumsEditor (SculptDrumsProcessor&);
    ~SculptDrumsEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawVU (juce::Graphics&, juce::Rectangle<float>);
    void updateMixerVisibility();

    SculptDrumsProcessor& proc;
    SculptLookAndFeel laf;
    juce::Slider morph, volume;
    juce::AudioProcessorValueTreeState::SliderAttachment morphAtt, volumeAtt;
    juce::TextButton mixerButton;
    std::array<juce::Slider, sculpt::mixerChannelCount> mixerFaders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, sculpt::mixerChannelCount> mixerAttachments;
    bool mixerOpen = false;
    float needle = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptDrumsEditor)
};
