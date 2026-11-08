#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "MorphEngine.h"

class SculptDrumsProcessor : public juce::AudioProcessor
{
public:
    SculptDrumsProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sculpt Drums"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // letti dalla GUI
    std::atomic<float> meterLevel { 0.0f };
    std::atomic<int>   rrStep { 0 };
    std::atomic<float> layerWeight[3];

private:
    struct Piece
    {
        int note = 36;
        float gamma = 1.0f;
        std::array<juce::AudioBuffer<float>, 3> layers; // naturale, trattato, elettronico
        sculpt::RoundRobin rr;
    };

    struct Voice
    {
        bool active = false;
        int piece = 0;
        int pos = 0;
        float vel = 1.0f;
        float rrOffset = 0.0f; // fissato al colpo, resta durante il decadimento
        float w[3] { 1, 0, 0 };
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void buildPlaceholderKit (double sr);
    void trigger (int note, float velocity);
    void renderSegment (juce::AudioBuffer<float>&, int start, int n);

    std::array<Piece, 3> pieces;
    std::array<Voice, 32> voices;
    juce::SmoothedValue<float> morphSm, volSm;
    std::atomic<float>* pMorph = nullptr;
    std::atomic<float>* pVolume = nullptr;
    std::atomic<float>* pRange = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptDrumsProcessor)
};
