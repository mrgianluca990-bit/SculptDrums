#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace sculpt
{
constexpr int stageCount = 6;
struct Weights { std::array<float, stageCount> stage {}; };

// Sei posizioni ordinate come le cartelle dell'archivio dell'utente:
// 01/02 Natural, 03/04/05 processed, 06 Electro.
// Kernel coseno esteso: gli stadi si sovrappongono per +/- 40 punti MORPH.
// Alle posizioni intermedie possono suonare quattro stadi insieme.
// La normalizzazione energetica evita aumenti di livello al variare del mix.
inline Weights morphWeights (float morph, float gamma)
{
    float x = std::clamp (morph * (float) (stageCount - 1) * 0.01f,
                           0.0f, (float) (stageCount - 1));
    const int anchor = std::min (static_cast<int> (x), stageCount - 1);
    // Gamma conserva tutte le sei posizioni: agisce solo nel segmento corrente.
    if (anchor < stageCount - 1)
        x = (float) anchor + std::pow (x - (float) anchor, std::max (0.01f, gamma));

    constexpr float pi = 3.14159265358979323846f;
    constexpr float radius = 2.0f; // due intervalli da 20 punti per lato
    auto smoothstep = [] (float v)
    {
        v = std::clamp (v, 0.0f, 1.0f);
        return v * v * (3.0f - 2.0f * v);
    };

    Weights w;
    float power = 0.0f;
    for (int i = 0; i < stageCount; ++i)
    {
        const float dist = std::abs (x - (float) i);
        if (dist >= radius) continue;

        float gain = 0.5f + 0.5f * std::cos (pi * dist / radius);
        // Gli estremi restano completamente naturali/elettronici, senza salti.
        if (i != 0 && x < 0.15f) gain *= smoothstep (x / 0.15f);
        if (i != stageCount - 1 && x > (float) (stageCount - 1) - 0.15f)
            gain *= smoothstep (((float) (stageCount - 1) - x) / 0.15f);

        w.stage[(std::size_t) i] = gain;
        power += gain * gain;
    }
    if (power > 0.0f)
    {
        const float normalization = 1.0f / std::sqrt (power);
        for (auto& gain : w.stage) gain *= normalization;
    }
    return w;
}

// Applica l'offset round robin; vicino agli estremi la variazione si attenua
// per rispettare il carattere acustico/elettronico scelto.
inline float applyOffset (float morph, float offset)
{
    const float edge = std::clamp (std::min (morph, 100.0f - morph) / 10.0f, 0.0f, 1.0f);
    return std::clamp (morph + offset * edge, 0.0f, 100.0f);
}

// Sequenza di variazioni +/- senza ripetizioni immediate (valori da calibrare per pezzo).
// Normalizzata: +-1 = intero "RR Range". Esempio con range 3: -1, +2, -2, +1 ...
class RoundRobin
{
public:
    void setStart (int s) { idx = ((s % N) + N) % N; }
    int step() const { return idx; }

    float next (float range)
    {
        const float v = seq[idx] * range;
        idx = (idx + 1) % N;
        return v;
    }

private:
    static constexpr int N = 8;
    static constexpr float seq[N] = { -0.33f, 0.67f, -0.67f, 0.33f, 1.0f, -1.0f, -0.17f, 0.5f };
    int idx = 0;
};
} // namespace sculpt
