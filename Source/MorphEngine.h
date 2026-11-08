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
// Le dissolvenze a potenza costante coinvolgono esclusivamente due stadi adiacenti.
inline Weights morphWeights (float morph, float gamma)
{
    const float x = std::clamp (morph * (float) (stageCount - 1) * 0.01f,
                                0.0f, (float) (stageCount - 1));
    const int first = std::min ((int) x, stageCount - 1);
    const int second = std::min (first + 1, stageCount - 1);
    // La curva propria del pezzo agisce solo ALL'INTERNO dello stadio:
    // i riferimenti 0/20/40/60/80/100 restano identici per ogni nota MIDI.
    const float u = std::pow (x - (float) first, std::max (0.01f, gamma));
    constexpr float halfPi = 1.57079632679489661923f;
    Weights w;
    w.stage[(std::size_t) first] = std::cos (u * halfPi);
    if (second != first)
        w.stage[(std::size_t) second] = std::sin (u * halfPi);
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
