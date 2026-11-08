#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace sculpt
{
struct Weights { float natural, treated, electronic; };

// MORPH 0..100 -> pesi a potenza costante su tre regioni:
// 0 naturale | 50 trattato | 100 elettronico. 'gamma' = curva propria del pezzo.
inline Weights morphWeights (float morph, float gamma)
{
    float t = juce::jlimit (0.0f, 1.0f, morph * 0.01f);
    t = std::pow (t, gamma);
    const float u = t * 2.0f;
    const float hp = juce::MathConstants<float>::halfPi;

    if (u < 1.0f) return { std::cos (u * hp), std::sin (u * hp), 0.0f };
    return { 0.0f, std::cos ((u - 1.0f) * hp), std::sin ((u - 1.0f) * hp) };
}

// Applica l'offset round robin; vicino agli estremi la variazione si attenua
// per rispettare il carattere acustico/elettronico scelto.
inline float applyOffset (float morph, float offset)
{
    const float edge = juce::jlimit (0.0f, 1.0f, juce::jmin (morph, 100.0f - morph) / 10.0f);
    return juce::jlimit (0.0f, 100.0f, morph + offset * edge);
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
