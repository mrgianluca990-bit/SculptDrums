#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <memory>
#include <vector>
#include "MorphEngine.h"
#include "DrumChannels.h"

class SculptDrumsProcessor : public juce::AudioProcessor
{
public:
    SculptDrumsProcessor();
    ~SculptDrumsProcessor() override;

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
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::String getKitMessage() const;

    juce::AudioProcessorValueTreeState apvts;

    // letti dalla GUI
    std::atomic<float> meterLevel { 0.0f };
    std::atomic<int>   rrStep { 0 };
    std::atomic<float> layerWeight[3];

private:
    enum KitStatus { Placeholder = 0, Loading, Loaded, Error };

    struct Sample
    {
        juce::AudioBuffer<float> buf; // sempre stereo, alla frequenza dell'host
        int velLo = 0, velHi = 127;
    };

    struct Piece
    {
        juce::String id;
        int note = -1;
        int mixerIndex = -1;
        float gamma = 1.0f;
        int choke = 0;                                  // 0 = nessun gruppo
        std::array<std::vector<Sample>, sculpt::stageCount> layers;
        std::array<int, sculpt::stageCount> rrCounter {}; // varianti indipendenti per stadio
        sculpt::RoundRobin rr;                          // round robin delle miscele
    };

    struct Voice
    {
        bool active = false;
        int piece = 0, pos = 0;
        float vel = 1.0f;
        float rrOffset = 0.0f; // fissato al colpo, resta durante il decadimento
        std::array<float, sculpt::stageCount> w {};
        std::array<const juce::AudioBuffer<float>*, sculpt::stageCount> src {};
        bool choking = false;
        float chokeGain = 1.0f;
    };

    class KitLoader;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static std::vector<Piece> makePlaceholderKit (double sr);
    static bool loadSample (juce::AudioFormatManager&, const juce::File&, double sr, juce::AudioBuffer<float>& out);
    static bool loadMicGroup (juce::AudioFormatManager&, const juce::File& base,
                              const juce::var& entry, double sr, juce::AudioBuffer<float>& out);
    static void levelMatchGroup (std::vector<Sample>&, double sr);
    static int pickVariant (Piece&, int layer, int vel127);

    void loadKit (double sr, juce::Thread& thread);
    void setKitMessage (const juce::String&);
    void trigger (int note, float velocity);
    void renderSegment (juce::AudioBuffer<float>&, int start, int n);

    std::vector<Piece> pieces;
    juce::SpinLock kitLock;
    std::array<Voice, 32> voices;
    juce::SmoothedValue<float> morphSm, volSm;
    std::array<juce::SmoothedValue<float>, sculpt::mixerChannelCount> pieceGainSm;
    std::array<std::atomic<float>*, sculpt::mixerChannelCount> pPieceGains {};
    std::atomic<float>* pMorph = nullptr;
    std::atomic<float>* pVolume = nullptr;
    std::atomic<float>* pRange = nullptr;

    std::unique_ptr<KitLoader> loader;
    std::atomic<int> kitStatus { Placeholder };
    std::atomic<double> loadedSr { 0.0 };
    juce::CriticalSection msgLock;
    juce::String kitMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptDrumsProcessor)
};
