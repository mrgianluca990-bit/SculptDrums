// Configurable, energy-normalized wide morph. Default 20-point stage spacing
// and 40-point radius retain the sound of the previous fixed morph engine.
struct MorphCurve
{
    std::array<float, stageCount> positions {{ 0.0f, 20.0f, 40.0f, 60.0f, 80.0f, 100.0f }};
    std::array<float, stageCount> overlaps  {{ 40.0f, 40.0f, 40.0f, 40.0f, 40.0f, 40.0f }};
};

inline Weights morphWeights (float morph, float gamma, const MorphCurve& curve)
{
    Weights result;
    const float input = std::clamp (morph, 0.0f, 100.0f);
    if (input <= 0.0f) { result.stage[0] = 1.0f; return result; }
    if (input >= 100.0f) { result.stage[stageCount - 1] = 1.0f; return result; }

    // Piece-dependent gamma acts locally within the adjustable stage positions.
    float x = input;
    for (int i = 0; i < stageCount - 1; ++i)
    {
        const float a = curve.positions[(std::size_t) i];
        const float b = curve.positions[(std::size_t) i + 1];
        if (input >= a && input <= b && b > a + 0.01f)
        {
            const float t = (input - a) / (b - a);
            x = a + (b - a) * std::pow (t, std::max (0.01f, gamma));
            break;
        }
    }

    constexpr float pi = 3.14159265358979323846f;
    float energy = 0.0f;
    for (int i = 0; i < stageCount; ++i)
    {
        const float radius = std::clamp (curve.overlaps[(std::size_t) i], 5.0f, 70.0f);
        const float distance = std::abs (x - curve.positions[(std::size_t) i]);
        if (distance >= radius) continue;
        float gain = 0.5f + 0.5f * std::cos (pi * distance / radius);
        // Preserve fully natural/electronic endpoints while smoothly introducing neighbors.
        if (i != 0 && input < 3.0f)
        {
            const float edge = std::clamp (input / 3.0f, 0.0f, 1.0f);
            gain *= edge * edge * (3.0f - 2.0f * edge);
        }
        if (i != stageCount - 1 && input > 97.0f)
        {
            const float edge = std::clamp ((100.0f - input) / 3.0f, 0.0f, 1.0f);
            gain *= edge * edge * (3.0f - 2.0f * edge);
        }
        result.stage[(std::size_t) i] = gain;
        energy += gain * gain;
    }
    if (energy < 1.0e-10f)
    {
        int nearest = 0;
        for (int i = 1; i < stageCount; ++i)
            if (std::abs (x - curve.positions[(std::size_t) i])
                < std::abs (x - curve.positions[(std::size_t) nearest])) nearest = i;
        result.stage[(std::size_t) nearest] = 1.0f;
    }
    else
    {
        const float scale = 1.0f / std::sqrt (energy);
        for (auto& gain : result.stage) gain *= scale;
    }
    return result;
}
