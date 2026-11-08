#pragma once
#include "PluginProcessor.h"

struct SculptLookAndFeel : public juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override;
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

    SculptDrumsProcessor& proc;
    SculptLookAndFeel laf;
    juce::Slider morph, volume;
    juce::AudioProcessorValueTreeState::SliderAttachment morphAtt, volumeAtt;
    float needle = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptDrumsEditor)
};
