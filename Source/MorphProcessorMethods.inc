// === Sculpt MORPH Editor: control surface and state (no disk I/O on audio thread) ===
sculpt::MorphCurve SculptDrumsProcessor::getMorphCurve() const
{
    sculpt::MorphCurve curve;
    for (int i = 0; i < sculpt::stageCount; ++i)
    {
        curve.positions[(std::size_t) i] = stagePosition[(std::size_t) i].load();
        curve.overlaps[(std::size_t) i] = stageOverlap[(std::size_t) i].load();
    }
    return curve;
}

void SculptDrumsProcessor::setStagePosition (int stage, float value)
{
    if (stage <= 0 || stage >= sculpt::stageCount - 1) return;
    const auto low  = stagePosition[(std::size_t) stage - 1].load() + 2.0f;
    const auto high = stagePosition[(std::size_t) stage + 1].load() - 2.0f;
    if (low <= high)
        stagePosition[(std::size_t) stage].store (juce::jlimit (low, high, value));
}

void SculptDrumsProcessor::setStageOverlap (int stage, float value)
{
    if (stage < 0 || stage >= sculpt::stageCount) return;
    stageOverlap[(std::size_t) stage].store (juce::jlimit (5.0f, 70.0f, value));
}

std::vector<SculptDrumsProcessor::WavInfo> SculptDrumsProcessor::getWavInfos()
{
    const juce::SpinLock::ScopedLockType guard (kitLock);
    return wavInfos;
}

float SculptDrumsProcessor::getWavGainDb (int index) const
{
    if (index < 0 || index >= sculpt::maxMorphWavs) return 0.0f;
    return wavGainDb[(std::size_t) index].load();
}

void SculptDrumsProcessor::setWavGainDb (int index, float value)
{
    if (index < 0 || index >= sculpt::maxMorphWavs) return;
    wavGainDb[(std::size_t) index].store (juce::jlimit (-24.0f, 12.0f, value));
}

void SculptDrumsProcessor::requestWavPreview (int index)
{
    if (index >= 0 && index < sculpt::maxMorphWavs)
        wavPreviewRequest.store (index);
}

void SculptDrumsProcessor::requestBlendPreview (int midiNote)
{
    if (midiNote >= 0 && midiNote <= 127)
        blendPreviewRequest.store (midiNote);
}

sculpt::MorphEditorState SculptDrumsProcessor::captureMorphEditorState()
{
    sculpt::MorphEditorState state;
    state.curve = getMorphCurve();
    bool noLoadedWavs = false;
    {
        const juce::SpinLock::ScopedLockType guard (kitLock);
        noLoadedWavs = wavInfos.empty();
        for (const auto& wav : wavInfos)
        {
            const float db = wavGainDb[(std::size_t) wav.index].load();
            if (std::abs (db) > 0.04f)
                state.gainsDb.emplace_back (wav.path, db);
        }
    }
    if (state.gainsDb.empty())
    {
        const juce::ScopedLock guard (savedGainLock);
        if (noLoadedWavs) state.gainsDb = savedGainsDb;
    }
    return state;
}

void SculptDrumsProcessor::applyMorphEditorState (const sculpt::MorphEditorState& state)
{
    // First sanitize all centers as a set to preserve the monotonic ordering.
    sculpt::MorphCurve curve = state.curve;
    curve.positions[0] = 0.0f;
    curve.positions[sculpt::stageCount - 1] = 100.0f;
    for (int i = 1; i < sculpt::stageCount - 1; ++i)
        curve.positions[(std::size_t) i] = juce::jlimit (
            curve.positions[(std::size_t) i - 1] + 2.0f,
            100.0f - (sculpt::stageCount - 1 - i) * 2.0f,
            std::isfinite (curve.positions[(std::size_t) i]) ? curve.positions[(std::size_t) i] : i * 20.0f);
    for (int i = 0; i < sculpt::stageCount; ++i)
    {
        stagePosition[(std::size_t) i].store (curve.positions[(std::size_t) i]);
        const float width = curve.overlaps[(std::size_t) i];
        stageOverlap[(std::size_t) i].store (
            std::isfinite (width) ? juce::jlimit (5.0f, 70.0f, width) : 40.0f);
    }

    {
        const juce::ScopedLock savedGuard (savedGainLock);
        savedGainsDb = state.gainsDb;
    }
    const juce::SpinLock::ScopedLockType guard (kitLock);
    for (int i = 0; i < sculpt::maxMorphWavs; ++i)
        wavGainDb[(std::size_t) i].store (0.0f);
    for (const auto& wav : wavInfos)
        for (const auto& record : state.gainsDb)
            if (wav.path == record.first && std::isfinite (record.second))
            {
                wavGainDb[(std::size_t) wav.index].store (
                    juce::jlimit (-24.0f, 12.0f, record.second));
                break;
            }
}
