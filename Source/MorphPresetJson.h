#pragma once
#include "MorphEditorState.h"
#include <cmath>

namespace sculpt
{
inline juce::var morphSettingsToVar (const MorphEditorState& state)
{
    auto* obj = new juce::DynamicObject();
    juce::Array<juce::var> positions, overlaps, wav;
    for (const auto x : state.curve.positions) positions.add ((double) x);
    for (const auto x : state.curve.overlaps)  overlaps.add ((double) x);
    for (const auto& record : state.gainsDb)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("path", record.first);
        entry->setProperty ("db", (double) record.second);
        wav.add (juce::var (entry));
    }
    obj->setProperty ("positions", juce::var (positions));
    obj->setProperty ("overlaps", juce::var (overlaps));
    obj->setProperty ("wavGainDb", juce::var (wav));
    return juce::var (obj);
}

inline bool morphSettingsFromVar (const juce::var& serialized,
                                  MorphEditorState& result, juce::String& error)
{
    result = {};
    if (serialized.isVoid() || serialized.isUndefined()) return true; // legacy preset
    if (! serialized.isObject()) { error = "Dati MORPH Editor non validi"; return false; }
    const auto pos = serialized.getProperty ("positions", {});
    const auto widths = serialized.getProperty ("overlaps", {});
    const auto wav = serialized.getProperty ("wavGainDb", {});
    const auto* pa = pos.getArray();
    const auto* wa = widths.getArray();
    const auto* ga = wav.getArray();
    if (pa == nullptr || wa == nullptr || ga == nullptr
        || pa->size() != stageCount || wa->size() != stageCount
        || ga->size() > maxMorphWavs)
    {
        error = "Curve o elenco dei gain MORPH incompleti";
        return false;
    }
    for (int i = 0; i < stageCount; ++i)
    {
        const float p = (float) (double) (*pa)[i];
        const float w = (float) (double) (*wa)[i];
        if (! std::isfinite (p) || ! std::isfinite (w) || p < 0.0f || p > 100.0f
            || w < 5.0f || w > 70.0f
            || (i > 0 && p < result.curve.positions[(std::size_t) i - 1] + 2.0f))
        {
            error = "Posizioni/overlap MORPH fuori intervallo";
            return false;
        }
        result.curve.positions[(std::size_t) i] = p;
        result.curve.overlaps[(std::size_t) i] = w;
    }
    if (std::abs (result.curve.positions[0]) > 0.01f
        || std::abs (result.curve.positions[stageCount - 1] - 100.0f) > 0.01f)
    {
        error = "Gli estremi MORPH devono essere 0 e 100";
        return false;
    }
    for (const auto& entry : *ga)
    {
        if (! entry.isObject()) { error = "WAV gain non valido"; return false; }
        const auto path = entry.getProperty ("path", {}).toString();
        const auto gainVar = entry.getProperty ("db", {});
        if (! (gainVar.isInt() || gainVar.isInt64() || gainVar.isDouble()))
        { error = "Gain WAV non numerico"; return false; }
        const float db = (float) (double) gainVar;
        if (path.isEmpty() || ! std::isfinite (db) || db < -24.0f || db > 12.0f
            || path.startsWithChar ('/') || path.contains (".."))
        {
            error = "Percorso/gain WAV non valido";
            return false;
        }
        for (const auto& existing : result.gainsDb)
            if (existing.first == path) { error = "WAV gain duplicato"; return false; }
        result.gainsDb.emplace_back (path, db);
    }
    return true;
}
}
