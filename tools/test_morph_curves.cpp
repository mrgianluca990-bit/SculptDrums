#include "../Source/MorphEngine.h"
#include <cmath>
#include <iostream>

int main()
{
    sculpt::MorphCurve curve;
    for (int step = 0; step <= 1000; ++step)
    {
        const auto weights = sculpt::morphWeights (step / 10.0f, 1.0f, curve);
        float energy = 0.0f;
        for (float v : weights.stage) energy += v * v;
        if (std::abs (energy - 1.0f) > 1.e-4f) return 1;
    }
    curve.positions = {{ 0, 12, 33, 62, 85, 100 }};
    curve.overlaps = {{ 30, 35, 50, 55, 42, 32 }};
    for (int step = 0; step <= 1000; ++step)
    {
        const auto weights = sculpt::morphWeights (step / 10.0f, 0.85f, curve);
        float energy = 0.0f;
        for (float v : weights.stage) energy += v * v;
        if (std::abs (energy - 1.0f) > 1.e-4f) return 2;
    }
    if (sculpt::morphWeights (0, 1.0f, curve).stage[0] != 1.0f) return 3;
    if (sculpt::morphWeights (100, 1.0f, curve).stage[5] != 1.0f) return 4;
    std::cout << "Morph curves: 1001 default + 1001 edited positions OK\n";
}
