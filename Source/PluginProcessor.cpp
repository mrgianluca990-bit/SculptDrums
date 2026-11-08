#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout SculptDrumsProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "morph", 1 }, "Morph",
           NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume",
           NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "rrRange", 1 }, "RR Range",
           NormalisableRange<float> (0.0f, 10.0f, 0.1f), 3.0f));
    return l;
}

SculptDrumsProcessor::SculptDrumsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pMorph  = apvts.getRawParameterValue ("morph");
    pVolume = apvts.getRawParameterValue ("volume");
    pRange  = apvts.getRawParameterValue ("rrRange");
    for (auto& w : layerWeight) w = 0.0f;

    pieces[0].note = 36; pieces[0].gamma = 1.00f; // cassa
    pieces[1].note = 38; pieces[1].gamma = 0.85f; // rullante
    pieces[2].note = 42; pieces[2].gamma = 1.20f; // charleston
}

bool SculptDrumsProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// Segnaposto sintetici: da sostituire con i campioni reali.
void SculptDrumsProcessor::buildPlaceholderKit (double sr)
{
    const float lens[3][3] = { { 0.5f, 0.5f, 0.7f }, { 0.35f, 0.35f, 0.4f }, { 0.12f, 0.12f, 0.2f } };
    const float twoPi = juce::MathConstants<float>::twoPi;

    for (int p = 0; p < 3; ++p)
        for (int k = 0; k < 3; ++k)
        {
            auto& b = pieces[(size_t) p].layers[(size_t) k];
            const int n = (int) (lens[p][k] * (float) sr);
            b.setSize (1, n);
            auto* d = b.getWritePointer (0);
            juce::Random rnd (1234 + p * 10 + k);
            float ph = 0.0f, prev = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) sr;
                const float noise = rnd.nextFloat() * 2.0f - 1.0f;
                float s = 0.0f;

                if (p == 0) // cassa
                {
                    ph += twoPi * (45.0f + 90.0f * std::exp (-t * 25.0f)) / (float) sr;
                    s = std::sin (ph) * std::exp (-t * 8.0f);
                    if (k == 1) s = std::tanh (2.5f * s);
                    if (k == 2) s = std::sin (twoPi * 52.0f * t) * std::exp (-t * 5.0f) + (i < 40 ? 0.5f : 0.0f);
                }
                else if (p == 1) // rullante
                {
                    s = noise * std::exp (-t * (k == 2 ? 12.0f : 18.0f)) * 0.6f
                      + std::sin (twoPi * (k == 2 ? 220.0f : 190.0f) * t) * std::exp (-t * 30.0f) * 0.5f;
                    if (k == 1) s = std::tanh (2.0f * s);
                }
                else // charleston
                {
                    const float hp = noise - prev; prev = noise;
                    s = hp * std::exp (-t * (k == 0 ? 60.0f : k == 1 ? 45.0f : 35.0f)) * 0.5f;
                }
                d[i] = s;
            }
            const float mag = b.getMagnitude (0, n);
            if (mag > 0.0f) b.applyGain (0.8f / mag);
        }
}

void SculptDrumsProcessor::prepareToPlay (double sr, int)
{
    buildPlaceholderKit (sr);
    morphSm.reset (sr, 0.05);
    morphSm.setCurrentAndTargetValue (pMorph->load());
    volSm.reset (sr, 0.02);
    volSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pVolume->load(), -60.0f));
    for (auto& v : voices) v.active = false;
    for (size_t i = 0; i < pieces.size(); ++i) pieces[i].rr.setStart ((int) i * 3); // pezzi indipendenti
}

void SculptDrumsProcessor::trigger (int note, float velocity)
{
    for (size_t pi = 0; pi < pieces.size(); ++pi)
    {
        if (pieces[pi].note != note) continue;

        Voice* slot = &voices[0];
        for (auto& v : voices)
        {
            if (! v.active) { slot = &v; break; }
            if (v.pos > slot->pos) slot = &v; // altrimenti ruba la più vecchia
        }

        slot->active = true;
        slot->piece = (int) pi;
        slot->pos = 0;
        slot->vel = std::pow (velocity, 1.4f);
        slot->rrOffset = pieces[pi].rr.next (pRange->load());

        const auto w = sculpt::morphWeights (sculpt::applyOffset (morphSm.getCurrentValue(), slot->rrOffset),
                                             pieces[pi].gamma);
        slot->w[0] = w.natural; slot->w[1] = w.treated; slot->w[2] = w.electronic;
        rrStep = pieces[pi].rr.step();
        return;
    }
}

void SculptDrumsProcessor::renderSegment (juce::AudioBuffer<float>& buf, int start, int n)
{
    if (n <= 0) return;
    const float m1 = morphSm.skip (n);
    const float v0 = volSm.getCurrentValue();
    const float v1 = volSm.skip (n);
    float* L = buf.getWritePointer (0) + start;
    float* R = buf.getWritePointer (1) + start;

    for (auto& v : voices)
    {
        if (! v.active) continue;
        auto& p = pieces[(size_t) v.piece];
        const auto w = sculpt::morphWeights (sculpt::applyOffset (m1, v.rrOffset), p.gamma);
        const float w1[3] = { w.natural, w.treated, w.electronic };
        const float* d[3] = { p.layers[0].getReadPointer (0), p.layers[1].getReadPointer (0), p.layers[2].getReadPointer (0) };
        const int len[3] = { p.layers[0].getNumSamples(), p.layers[1].getNumSamples(), p.layers[2].getNumSamples() };

        // pesi interpolati nel blocco: MORPH segue anche i colpi già partiti, senza salti
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) n;
            const int idx = v.pos + i;
            float s = 0.0f;
            for (int k = 0; k < 3; ++k)
                if (idx < len[k]) s += (v.w[k] + (w1[k] - v.w[k]) * t) * d[k][idx];
            const float g = v.vel * (v0 + (v1 - v0) * t);
            L[i] += s * g;
            R[i] += s * g;
        }

        v.pos += n;
        for (int k = 0; k < 3; ++k) v.w[k] = w1[k];
        if (v.pos >= juce::jmax (len[0], len[1], len[2])) v.active = false;
    }

    const auto cw = sculpt::morphWeights (m1, 1.0f);
    layerWeight[0] = cw.natural; layerWeight[1] = cw.treated; layerWeight[2] = cw.electronic;
}

void SculptDrumsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int N = buffer.getNumSamples();

    morphSm.setTargetValue (pMorph->load());
    volSm.setTargetValue (juce::Decibels::decibelsToGain (pVolume->load(), -60.0f));

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int t = juce::jlimit (pos, N, meta.samplePosition);
        renderSegment (buffer, pos, t - pos);
        pos = t;

        if (msg.isNoteOn()) trigger (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) for (auto& v : voices) v.active = false;
    }
    renderSegment (buffer, pos, N - pos);

    const float peak = buffer.getMagnitude (0, N);
    meterLevel = juce::jmax (peak, meterLevel.load() * 0.92f);
}

juce::AudioProcessorEditor* SculptDrumsProcessor::createEditor() { return new SculptDrumsEditor (*this); }

void SculptDrumsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SculptDrumsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SculptDrumsProcessor(); }
