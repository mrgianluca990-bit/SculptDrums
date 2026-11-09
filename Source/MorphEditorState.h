#pragma once
#include "MorphEngine.h"
#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

namespace sculpt
{
inline constexpr int maxMorphWavs = 512;

struct MorphEditorState
{
    MorphCurve curve;
    // Stable, relative WAV paths rather than indices: preset remains meaningful
    // if the manifest order changes. Only non-default gains need to be stored.
    std::vector<std::pair<juce::String, float>> gainsDb;
};
}
