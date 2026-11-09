#!/usr/bin/env python3
"""Apply Sculpt Drums MORPH Editor to the current JUCE mixer/presets sources.

This patch is checked against exact anchors before writing. It also installs the
prior internal-preset feature if necessary; no WAV data is changed.

Usage: python3 apply_morph_editor.py /path/to/SculptDrums
"""
from pathlib import Path
import subprocess
import sys
import shutil

root = Path(sys.argv[1] if len(sys.argv) > 1 else '.').expanduser().resolve()
bundle = Path(__file__).resolve().parent.parent
source = root/'Source'
if not (source/'PluginProcessor.cpp').is_file() or not (source/'PluginEditor.cpp').is_file():
    raise SystemExit('Manca un repository SculptDrums con Source/PluginProcessor.cpp e PluginEditor.cpp')
if 'SCULPT_DRUMS_MORPH_EDITOR_V1' in (source/'PluginProcessor.h').read_text():
    raise SystemExit('Il MORPH Editor è già presente: nessuna modifica applicata')

# Use the exact prior patch if this checkout predates the preset build.
if 'SCULPT_DRUMS_INTERNAL_PRESETS_V1' not in (source/'PluginEditor.h').read_text():
    previous = bundle/'tools'/'apply_presets.py'
    # The earlier patch requires the companion source assets at its own parent.
    if not (bundle/'tools'/'Source'/'PresetStore.h').exists():
        raise SystemExit('Mancano le risorse per aggiornare la versione precedente senza preset')
    subprocess.run([sys.executable, str(previous), str(root)], check=True)

files = ['MorphEngine.h', 'PluginProcessor.h', 'PluginProcessor.cpp',
         'PluginEditor.h', 'PluginEditor.cpp', 'PresetStore.h', 'PresetEditorMethods.inc']
text = {f:(source/f).read_text(encoding='utf-8') for f in files}

def one(f, old, new, label):
    count=text[f].count(old)
    if count!=1:
        raise RuntimeError(f'{f}, {label}: atteso 1 anchor, trovato {count}; \n'
                           'repository diverso dalla build prevista; sorgenti non modificati')
    text[f]=text[f].replace(old,new,1)

one('MorphEngine.h', '// Applica l\'offset round robin;',
    (bundle/'Source'/'MorphCurveMethods.inc').read_text(encoding='utf-8')
    + '\n// Applica l\'offset round robin;', 'new morph kernel')

one('PluginProcessor.h', '#include "MorphEngine.h"',
    '#include "MorphEngine.h"\n#include "MorphEditorState.h"', 'include morph state')
one('PluginProcessor.h',
    '    juce::String getKitMessage() const;',
    '''    juce::String getKitMessage() const;

    // SCULPT_DRUMS_MORPH_EDITOR_V1
    struct WavInfo { int index = -1; juce::String path, pieceId; int stage = 0; };
    sculpt::MorphCurve getMorphCurve() const;
    void setStagePosition (int stage, float position);
    void setStageOverlap (int stage, float overlap);
    std::vector<WavInfo> getWavInfos();
    float getWavGainDb (int index) const;
    void setWavGainDb (int index, float gainDb);
    void requestWavPreview (int index);
    void requestBlendPreview (int midiNote);
    sculpt::MorphEditorState captureMorphEditorState();
    void applyMorphEditorState (const sculpt::MorphEditorState&);''', 'processor API')
one('PluginProcessor.h',
    '''        juce::AudioBuffer<float> buf; // sempre stereo, alla frequenza dell'host
        int velLo = 0, velHi = 127;''',
    '''        juce::AudioBuffer<float> buf; // normalized original composite
        // Only multi-mic events retain the pre-mix WAVs; this permits individual
        // close/OH/room gain without resynthesizing or editing the source files.
        std::array<juce::AudioBuffer<float>, 3> micBuffers;
        std::array<int, 3> wavIds {{ -1, -1, -1 }};
        float normalizer = 1.0f;
        bool hasMicGroup = false;
        int velLo = 0, velHi = 127;''', 'sample structure')
one('PluginProcessor.h',
    '''        std::array<const juce::AudioBuffer<float>*, sculpt::stageCount> src {};''',
    '''        std::array<const juce::AudioBuffer<float>*, sculpt::stageCount> src {};
        std::array<const Sample*, sculpt::stageCount> sourceSamples {};''', 'voice metadata')
one('PluginProcessor.h',
    '''    static bool loadMicGroup (juce::AudioFormatManager&, const juce::File& base,
                              const juce::var& entry, double sr, juce::AudioBuffer<float>& out);''',
    '''    static bool loadMicGroup (juce::AudioFormatManager&, const juce::File& base,
                              const juce::var& entry, double sr, Sample& out);''', 'mic group signature')
one('PluginProcessor.h',
    '    juce::SmoothedValue<float> morphSm, volSm;',
    '''    juce::SmoothedValue<float> morphSm, volSm;
    std::array<std::atomic<float>, sculpt::stageCount> stagePosition, stageOverlap;
    std::array<std::atomic<float>, sculpt::maxMorphWavs> wavGainDb;
    std::array<juce::SmoothedValue<float>, sculpt::maxMorphWavs> wavGainSm;
    std::vector<WavInfo> wavInfos; // protected by kitLock
    std::array<const juce::AudioBuffer<float>*, sculpt::maxMorphWavs> wavPreviewBuffers {};
    std::array<float, sculpt::maxMorphWavs> wavPreviewNormalization {};
    std::atomic<int> wavPreviewRequest { -1 }, blendPreviewRequest { -1 };
    struct Audition { const juce::AudioBuffer<float>* buffer = nullptr; int index = -1;
                      int pos = 0; float baseGain = 1.0f; } audition;
    juce::CriticalSection savedGainLock;
    std::vector<std::pair<juce::String, float>> savedGainsDb;''', 'morph/wav state')

one('PluginProcessor.cpp',
    '    for (auto& w : layerWeight) w = 0.0f;',
    '''    for (auto& w : layerWeight) w = 0.0f;
    const sculpt::MorphCurve defaults;
    for (int i = 0; i < sculpt::stageCount; ++i)
    {
        stagePosition[(std::size_t) i].store (defaults.positions[(std::size_t) i]);
        stageOverlap[(std::size_t) i].store (defaults.overlaps[(std::size_t) i]);
    }
    for (auto& db : wavGainDb) db.store (0.0f);''', 'init new controls')

mic_start=text['PluginProcessor.cpp'].index('bool SculptDrumsProcessor::loadMicGroup (')
mic_end=text['PluginProcessor.cpp'].index('\nstatic float earlyRms',mic_start)
text['PluginProcessor.cpp']=text['PluginProcessor.cpp'][:mic_start]+'''bool SculptDrumsProcessor::loadMicGroup (juce::AudioFormatManager& fm, const juce::File& base,
                                         const juce::var& entry, double sr, Sample& out)
{
    if (! loadSample (fm, base.getChildFile (entry["file"].toString()), sr, out.buf)) return false;
    const auto ohPath = entry.getProperty ("oh", {}).toString();
    const auto roomPath = entry.getProperty ("room", {}).toString();
    if (ohPath.isEmpty() && roomPath.isEmpty()) return true;

    out.hasMicGroup = true;
    out.micBuffers[0].makeCopyOf (out.buf);
    const juce::String paths[3] = { {}, ohPath, roomPath };
    const float levels[3] = { 1.0f, 0.25f, 0.15f };
    for (int mic = 1; mic < 3; ++mic)
    {
        if (paths[mic].isEmpty()) continue;
        if (! loadSample (fm, base.getChildFile (paths[mic]), sr, out.micBuffers[(std::size_t) mic]))
            continue;
        const auto& additional = out.micBuffers[(std::size_t) mic];
        const int length = juce::jmax (out.buf.getNumSamples(), additional.getNumSamples());
        juce::AudioBuffer<float> mixed (2, length);
        mixed.clear();
        for (int ch = 0; ch < 2; ++ch)
        {
            mixed.copyFrom (ch, 0, out.buf, ch, 0, out.buf.getNumSamples());
            mixed.addFrom (ch, 0, additional, ch, 0, additional.getNumSamples(), levels[mic]);
        }
        out.buf = std::move (mixed);
    }
    return true;
}
''' + text['PluginProcessor.cpp'][mic_end:]

one('PluginProcessor.cpp', '        g[i].buf.applyGain (gn);',
    '        g[i].normalizer = gn;\n        g[i].buf.applyGain (gn);', 'record mic normalization')
one('PluginProcessor.cpp', '    std::vector<Piece> np;\n    int loaded = 0, missing = 0;',
    '''    std::vector<Piece> np;
    std::vector<WavInfo> newWavs;
    auto registerWav = [&newWavs] (const juce::String& relative,
                                   const juce::String& pieceId, int stage) -> int
    {
        if (relative.isEmpty()) return -1;
        for (const auto& wav : newWavs)
            if (wav.path == relative) return wav.index;
        if (newWavs.size() >= sculpt::maxMorphWavs) return -1;
        const int id = static_cast<int> (newWavs.size());
        newWavs.push_back (WavInfo { id, relative, pieceId, stage });
        return id;
    };
    int loaded = 0, missing = 0;''', 'register WAV names')
one('PluginProcessor.cpp',
    '                if (! loadMicGroup (fm, folder, e, sr, s.buf)) { ++missing; continue; }',
    '''                if (! loadMicGroup (fm, folder, e, sr, s)) { ++missing; continue; }
                s.wavIds[0] = registerWav (e["file"].toString(), p.id, layer);
                s.wavIds[1] = registerWav (e.getProperty ("oh", {}).toString(), p.id, layer);
                s.wavIds[2] = registerWav (e.getProperty ("room", {}).toString(), p.id, layer);''', 'attach WAV ids')
one('PluginProcessor.cpp',
    '''    {
        const juce::SpinLock::ScopedLockType sl (kitLock);
        for (auto& v : voices) v.active = false;
        pieces.swap (np);
    }
    loadedSr = sr;''',
    '''    {
        std::vector<std::pair<juce::String, float>> remembered;
        {
            const juce::ScopedLock saved (savedGainLock);
            remembered = savedGainsDb;
        }
        const juce::SpinLock::ScopedLockType sl (kitLock);
        // Preserve on-screen gains if the kit is reloaded at a new sample rate.
        for (const auto& wav : wavInfos)
        {
            const float db = wavGainDb[(std::size_t) wav.index].load();
            if (std::abs (db) > 0.04f)
            {
                bool replaced = false;
                for (auto& rec : remembered)
                    if (rec.first == wav.path) { rec.second = db; replaced = true; break; }
                if (! replaced) remembered.emplace_back (wav.path, db);
            }
        }
        for (auto& v : voices) v.active = false;
        audition = {};
        wavPreviewRequest.store (-1);
        pieces.swap (np);
        wavInfos.swap (newWavs);
        wavPreviewBuffers.fill (nullptr);
        wavPreviewNormalization.fill (1.0f);
        for (auto& db : wavGainDb) db.store (0.0f);
        for (const auto& wav : wavInfos)
            for (const auto& rec : remembered)
                if (wav.path == rec.first)
                { wavGainDb[(std::size_t) wav.index].store (rec.second); break; }
        constexpr float micLevels[3] = { 1.0f, 0.25f, 0.15f };
        for (const auto& piece : pieces)
            for (const auto& stage : piece.layers)
                for (const auto& s : stage)
                    for (int mic = 0; mic < 3; ++mic)
                    {
                        const int id = s.wavIds[(std::size_t) mic];
                        if (id < 0 || id >= sculpt::maxMorphWavs) continue;
                        wavPreviewBuffers[(std::size_t) id] =
                            s.hasMicGroup ? &s.micBuffers[(std::size_t) mic] : &s.buf;
                        wavPreviewNormalization[(std::size_t) id] =
                            s.hasMicGroup ? s.normalizer * micLevels[mic] : 1.0f;
                    }
    }
    loadedSr = sr;''', 'replace kit and rebuild preview')

one('PluginProcessor.cpp',
    '    for (std::size_t i = 0; i < sculpt::mixerChannelCount; ++i)\n    {\n        pieceGainSm[i].reset (sr, 0.025);',
    '''    for (auto& wav : wavGainSm)
    {
        wav.reset (sr, 0.04);
        wav.setCurrentAndTargetValue (1.0f);
    }
    for (std::size_t i = 0; i < sculpt::mixerChannelCount; ++i)
    {
        pieceGainSm[i].reset (sr, 0.025);''', 'prepare WAV gain smoothing')
one('PluginProcessor.cpp',
    '        slot->src.fill (nullptr);',
    '        slot->src.fill (nullptr);\n        slot->sourceSamples.fill (nullptr);', 'reset voice sample links')
one('PluginProcessor.cpp',
    '                    slot->src[(size_t) stage] = &p.layers[(size_t) nearest][(size_t) v].buf;',
    '''                {
                    const auto& sample = p.layers[(size_t) nearest][(size_t) v];
                    slot->src[(size_t) stage] = &sample.buf;
                    slot->sourceSamples[(size_t) stage] = &sample;
                }''', 'track original sample')
one('PluginProcessor.cpp',
    '        const auto w = sculpt::morphWeights (sculpt::applyOffset (morphSm.getCurrentValue(), slot->rrOffset), p.gamma);',
    '''        const auto w = sculpt::morphWeights (
            sculpt::applyOffset (morphSm.getCurrentValue(), slot->rrOffset),
            p.gamma, getMorphCurve());''', 'trigger editable curve')

one('PluginProcessor.cpp',
    '''    std::array<float, sculpt::mixerChannelCount> channelStart {}, channelEnd {};''',
    '''    const auto editableCurve = getMorphCurve();
    std::array<float, sculpt::maxMorphWavs> wavStart {}, wavEnd {};
    for (std::size_t i = 0; i < wavInfos.size(); ++i)
    {
        const float db = wavGainDb[i].load();
        wavGainSm[i].setTargetValue (juce::Decibels::decibelsToGain (db));
        wavStart[i] = wavGainSm[i].getCurrentValue();
        wavEnd[i] = wavGainSm[i].skip (n);
    }
    const auto sampleGainAt = [&wavStart, &wavEnd] (int index, float t)
    {
        return index < 0 || index >= sculpt::maxMorphWavs ? 1.0f
            : wavStart[(std::size_t) index] + (wavEnd[(std::size_t) index] - wavStart[(std::size_t) index]) * t;
    };
    std::array<float, sculpt::mixerChannelCount> channelStart {}, channelEnd {};''', 'segment gain smoothing')
one('PluginProcessor.cpp',
    'v.src, sculpt::morphWeights (sculpt::applyOffset (m1, v.rrOffset), p.gamma).stage);',
    'v.src, sculpt::morphWeights (sculpt::applyOffset (m1, v.rrOffset), p.gamma, editableCurve).stage);',
    'render editable morph')
one('PluginProcessor.cpp',
    '''                    sl += g * dl[(size_t) k][idx];
                    sr += g * dr[(size_t) k][idx];''',
    '''                    const auto* s = v.sourceSamples[(std::size_t) k];
                    if (s != nullptr && s->hasMicGroup)
                    {
                        constexpr float micLevels[3] = { 1.0f, 0.25f, 0.15f };
                        for (int mic = 0; mic < 3; ++mic)
                        {
                            const auto& wav = s->micBuffers[(std::size_t) mic];
                            if (idx >= wav.getNumSamples()) continue;
                            const float mg = g * s->normalizer * micLevels[mic]
                                             * sampleGainAt (s->wavIds[(std::size_t) mic], t);
                            sl += mg * wav.getReadPointer (0)[idx];
                            sr += mg * wav.getReadPointer (1)[idx];
                        }
                    }
                    else
                    {
                        const float mg = s != nullptr ? sampleGainAt (s->wavIds[0], t) : 1.0f;
                        sl += g * mg * dl[(size_t) k][idx];
                        sr += g * mg * dr[(size_t) k][idx];
                    }''', 'separate mic gains')
one('PluginProcessor.cpp',
    '    const auto cw = sculpt::morphWeights (m1, 1.0f);',
    '''    // Solo audition of a WAV (not a round-robin variant or a morph mixture).
    if (audition.buffer != nullptr)
    {
        const auto& wav = *audition.buffer;
        const int len = wav.getNumSamples();
        for (int i = 0; i < n && audition.pos < len; ++i, ++audition.pos)
        {
            const float tail = juce::jmin (1.0f, (float) (len - audition.pos) / 256.0f);
            const float g = audition.baseGain * tail
                            * sampleGainAt (audition.index, (float) i / (float) n)
                            * (v0 + (v1 - v0) * (float) i / (float) n);
            L[i] += wav.getReadPointer (0)[audition.pos] * g;
            R[i] += wav.getReadPointer (1)[audition.pos] * g;
        }
        if (audition.pos >= len) audition = {};
    }
    const auto cw = sculpt::morphWeights (m1, 1.0f, editableCurve);''', 'preview + layer meters')
one('PluginProcessor.cpp',
    '''    const int N = buffer.getNumSamples();
    morphSm.setTargetValue (pMorph->load());''',
    '''    const int N = buffer.getNumSamples();
    const int soloWav = wavPreviewRequest.exchange (-1);
    if (soloWav >= 0 && soloWav < sculpt::maxMorphWavs)
    {
        audition.buffer = wavPreviewBuffers[(std::size_t) soloWav];
        audition.index = soloWav;
        audition.pos = 0;
        audition.baseGain = wavPreviewNormalization[(std::size_t) soloWav];
    }
    const int blendNote = blendPreviewRequest.exchange (-1);
    if (blendNote >= 0 && blendNote < 128) trigger (blendNote, 0.90f);
    morphSm.setTargetValue (pMorph->load());''', 'UI preview requests')

one('PluginProcessor.cpp',
    '''void SculptDrumsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SculptDrumsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}''',
    '''void SculptDrumsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = apvts.copyState();
    auto morphData = juce::ValueTree ("MORPH_EDITOR");
    const auto state = captureMorphEditorState();
    for (int i = 0; i < sculpt::stageCount; ++i)
    {
        morphData.setProperty (juce::Identifier ("pos" + juce::String (i)), state.curve.positions[(std::size_t) i], nullptr);
        morphData.setProperty (juce::Identifier ("width" + juce::String (i)), state.curve.overlaps[(std::size_t) i], nullptr);
    }
    for (const auto& wav : state.gainsDb)
    {
        auto entry = juce::ValueTree ("GAIN");
        entry.setProperty ("path", wav.first, nullptr);
        entry.setProperty ("db", wav.second, nullptr);
        morphData.addChild (entry, -1, nullptr);
    }
    tree.addChild (morphData, -1, nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary (*xml, dest);
}

void SculptDrumsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (! xml->hasTagName (apvts.state.getType())) return;
        auto tree = juce::ValueTree::fromXml (*xml);
        auto extra = tree.getChildWithName ("MORPH_EDITOR");
        if (extra.isValid())
        {
            sculpt::MorphEditorState state;
            for (int i = 0; i < sculpt::stageCount; ++i)
            {
                state.curve.positions[(std::size_t) i] =
                    (float) extra.getProperty (juce::Identifier ("pos" + juce::String (i)), i * 20.0f);
                state.curve.overlaps[(std::size_t) i] =
                    (float) extra.getProperty (juce::Identifier ("width" + juce::String (i)), 40.0f);
            }
            for (int i = 0; i < extra.getNumChildren(); ++i)
            {
                const auto item = extra.getChild (i);
                if (! item.hasType (juce::Identifier ("GAIN"))) continue;
                state.gainsDb.emplace_back (item.getProperty ("path").toString(),
                                            (float) item.getProperty ("db"));
            }
            applyMorphEditorState (state);
            tree.removeChild (extra, nullptr);
        }
        else applyMorphEditorState (sculpt::MorphEditorState{}); // old DAW session
        apvts.replaceState (tree);
    }
}''', 'DAW state persistence')
text['PluginProcessor.cpp'] += '\n// SCULPT_DRUMS_MORPH_EDITOR_V1\n#include "MorphProcessorMethods.inc"\n'

one('PluginEditor.h', '#include "PresetStore.h"',
    '#include "PresetStore.h"\n#include "MorphEditorComponents.h"', 'editor includes')
one('PluginEditor.h', '    void updateMixerVisibility();',
    '''    void updateMixerVisibility();
    void rebuildMorphWavRows();
    void refreshMorphEditorControls();
    void drawMorphEditor (juce::Graphics&);''', 'editor API')
one('PluginEditor.h',
    '    juce::TextButton mixerButton;',
    '''    juce::TextButton mixerButton;
    juce::TextButton morphEditButton, blendPreviewButton;
    juce::ComboBox pieceSelector;
    std::array<juce::Slider, sculpt::stageCount> stagePositions, stageOverlaps;
    juce::Component wavRowContent;
    juce::Viewport wavViewport;
    std::vector<std::unique_ptr<MorphWavRow>> wavRows;
    std::vector<SculptDrumsProcessor::WavInfo> currentWavInfos;
    bool morphEditOpen = false;''', 'editor member fields')

one('PluginEditor.cpp',
    '''        mixerOpen = mixerButton.getToggleState();
        updateMixerVisibility();''',
    '''        mixerOpen = mixerButton.getToggleState();
        if (mixerOpen)
        {
            morphEditOpen = false;
            morphEditButton.setToggleState (false, juce::dontSendNotification);
        }
        updateMixerVisibility();''', 'mixer/editor mutual exclusion')
one('PluginEditor.cpp', '    addAndMakeVisible (mixerButton);',
    '''    addAndMakeVisible (mixerButton);

    morphEditButton.setButtonText ("EDIT MORPH");
    morphEditButton.setTooltip ("Curve MORPH e gain per singolo WAV");
    morphEditButton.setClickingTogglesState (true);
    morphEditButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242424));
    morphEditButton.setColour (juce::TextButton::textColourOffId, cream);
    morphEditButton.onClick = [this]
    {
        morphEditOpen = morphEditButton.getToggleState();
        if (morphEditOpen)
        {
            mixerOpen = false;
            mixerButton.setToggleState (false, juce::dontSendNotification);
            rebuildMorphWavRows();
            refreshMorphEditorControls();
        }
        updateMixerVisibility();
        repaint();
    };
    addAndMakeVisible (morphEditButton);

    for (const auto& channel : sculpt::mixerChannels)
        pieceSelector.addItem (channel.shortName, pieceSelector.getNumItems() + 1);
    pieceSelector.setSelectedId (1, juce::dontSendNotification);
    pieceSelector.onChange = [this] { rebuildMorphWavRows(); };
    addAndMakeVisible (pieceSelector);

    blendPreviewButton.setButtonText ("PLAY MIX");
    blendPreviewButton.setTooltip ("Ascolta questo elemento con il MORPH corrente");
    blendPreviewButton.onClick = [this]
    {
        const int selection = juce::jlimit (0, (int) sculpt::mixerChannelCount - 1,
                                           pieceSelector.getSelectedId() - 1);
        proc.requestBlendPreview (sculpt::mixerChannels[(std::size_t) selection].midiNote);
    };
    addAndMakeVisible (blendPreviewButton);

    wavViewport.setViewedComponent (&wavRowContent, false);
    wavViewport.setScrollBarsShown (true, false);
    wavViewport.setScrollBarThickness (9);
    addAndMakeVisible (wavViewport);

    for (int i = 0; i < sculpt::stageCount; ++i)
    {
        auto& pslider = stagePositions[(std::size_t) i];
        pslider.setRange (0.0, 100.0, 0.1);
        pslider.setSliderStyle (juce::Slider::LinearHorizontal);
        pslider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 18);
        pslider.setValue (i * 20.0f, juce::dontSendNotification);
        pslider.setEnabled (i > 0 && i < sculpt::stageCount - 1);
        pslider.onValueChange = [this, i]
        {
            proc.setStagePosition (i, (float) stagePositions[(std::size_t) i].getValue());
            repaint();
        };
        addAndMakeVisible (pslider);

        auto& overlapSlider = stageOverlaps[(std::size_t) i];
        overlapSlider.setRange (5.0, 70.0, 0.1);
        overlapSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        overlapSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 18);
        overlapSlider.setValue (40.0, juce::dontSendNotification);
        overlapSlider.onValueChange = [this, i]
        {
            proc.setStageOverlap (i, (float) stageOverlaps[(std::size_t) i].getValue());
            repaint();
        };
        addAndMakeVisible (overlapSlider);
    }''', 'morph editor controls')
one('PluginEditor.cpp',
    '''    mixerButton.setBounds (714, 12, 36, 28);''',
    '''    mixerButton.setBounds (714, 12, 36, 28);
    morphEditButton.setBounds (616, 12, 90, 28);
    pieceSelector.setBounds (22, 76, 178, 27);
    blendPreviewButton.setBounds (207, 76, 100, 27);
    for (int i = 0; i < sculpt::stageCount; ++i)
    {
        stagePositions[(std::size_t) i].setBounds (90 + i * 108, 244, 106, 43);
        stageOverlaps[(std::size_t) i].setBounds (90 + i * 108, 301, 106, 43);
    }
    wavViewport.setBounds (22, 365, 718, 122);''', 'morph layout')
one('PluginEditor.cpp',
    '''    morph.setVisible (! mixerOpen);
    volume.setVisible (! mixerOpen);
    for (auto& fader : mixerFaders) fader.setVisible (mixerOpen);''',
    '''    morph.setVisible (! mixerOpen && ! morphEditOpen);
    volume.setVisible (! mixerOpen && ! morphEditOpen);
    for (auto& fader : mixerFaders) fader.setVisible (mixerOpen);
    pieceSelector.setVisible (morphEditOpen);
    blendPreviewButton.setVisible (morphEditOpen);
    wavViewport.setVisible (morphEditOpen);
    for (int i = 0; i < sculpt::stageCount; ++i)
    {
        stagePositions[(std::size_t) i].setVisible (morphEditOpen);
        stageOverlaps[(std::size_t) i].setVisible (morphEditOpen);
    }''', 'visibility')
one('PluginEditor.cpp',
    '''    needle += (target - needle) * 0.35f;
    repaint();''',
    '''    needle += (target - needle) * 0.35f;
    if (morphEditOpen)
    {
        const auto newInfo = proc.getWavInfos();
        if (newInfo.size() != currentWavInfos.size()) rebuildMorphWavRows();
        refreshMorphEditorControls();
    }
    repaint();''', 'live editor updates')
one('PluginEditor.cpp',
    '''    if (mixerOpen)
    {
        const auto panel''',
    '''    if (morphEditOpen)
    {
        drawMorphEditor (g);
        return;
    }
    if (mixerOpen)
    {
        const auto panel''', 'draw morph editor')
text['PluginEditor.cpp'] += '\n// SCULPT_DRUMS_MORPH_EDITOR_V1\n#include "MorphEditorMethods.inc"\n'

# Add a pure C++ regression test to the existing macOS GitHub Actions workflow.
workflow = root/'.github'/'workflows'/'build-macos-vst3.yml'
if not workflow.is_file():
    raise RuntimeError('Manca .github/workflows/build-macos-vst3.yml: nessun file modificato')
yml = workflow.read_text(encoding='utf-8')
if 'Verify morph curves and sample manifest' not in yml:
    expected = '      - name: Configure\n'
    if yml.count(expected) != 1:
        raise RuntimeError('Workflow di compilazione differente: stop senza modifiche')
    yml = yml.replace(expected,
        "      - name: Verify morph curves and sample manifest\n"
        "        run: |\n"
        "          python3 tools/audit_install.py .\n"
        "          clang++ -std=c++17 -Wall -Wextra -pedantic tools/test_morph_curves.cpp -o /tmp/sculpt-morph-test\n"
        "          /tmp/sculpt-morph-test\n\n" + expected, 1)
if (root/'tools'/'test_morph_curves.cpp').exists():
    raise RuntimeError('Esiste già tools/test_morph_curves.cpp: stop senza modifiche')

# Use newly revised preset files with backward-compatible v1 JSON import.
text['PresetStore.h']=(bundle/'Source'/'PresetStore.h').read_text(encoding='utf-8')
text['PresetEditorMethods.inc']=(bundle/'Source'/'PresetEditorMethods.inc').read_text(encoding='utf-8')

# Validation phase completed. Leave marker backups for recovery.
for name in files:
    original=source/name
    shutil.copy2(original, original.with_suffix(original.suffix+'.pre-morph.bak'))
for name,value in text.items():
    (source/name).write_text(value, encoding='utf-8')
shutil.copy2(workflow, workflow.with_suffix(workflow.suffix+'.pre-morph.bak'))
workflow.write_text(yml, encoding='utf-8')
shutil.copy2(bundle/'tools'/'test_morph_curves.cpp', root/'tools'/'test_morph_curves.cpp')
for name in ['MorphEditorState.h','MorphPresetJson.h','MorphEditorComponents.h',
             'MorphProcessorMethods.inc','MorphEditorMethods.inc']:
    shutil.copy2(bundle/'Source'/name, source/name)
print('MORPH Editor patch applicata. Backup .pre-morph.bak disponibili, non committarli.')
print('Controlla: git diff --stat; quindi esegui la GitHub Action macOS.')
