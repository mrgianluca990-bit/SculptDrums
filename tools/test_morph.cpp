#include "../Source/MorphEngine.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    for (float gamma : { 0.85f, 1.0f, 1.2f })
    {
        for (int stage = 0; stage < sculpt::stageCount; ++stage)
        {
            auto w = sculpt::morphWeights ((float) stage * 20.0f, gamma);
            for (int i = 0; i < sculpt::stageCount; ++i)
                if (i != stage) assert (w.stage[(std::size_t) stage] > w.stage[(std::size_t) i]);
        }
        for (int morph = 0; morph <= 100; ++morph)
        {
            const auto w = sculpt::morphWeights ((float) morph, gamma);
            float power = 0.0f;
            for (auto gain : w.stage) power += gain * gain;
            assert (std::abs (power - 1.0f) < 0.0001f);

            if (morph == 50)
            {
                int overlapping = 0;
                for (auto gain : w.stage) overlapping += gain > 0.001f ? 1 : 0;
                assert (overlapping == 4); // due stadi centrali e due ai lati
            }
        }
    }

    // Estremi puri: anche con il kernel largo MORPH 0 e 100 non sono contaminati.
    assert (sculpt::morphWeights (0.0f, 1.0f).stage[0] == 1.0f);
    assert (sculpt::morphWeights (100.0f, 1.0f).stage[5] == 1.0f);
    assert (sculpt::morphWeights (20.0f, 1.0f).stage[0] > 0.3f);
    assert (sculpt::morphWeights (20.0f, 1.0f).stage[2] > 0.3f);

    assert (sculpt::applyOffset (0.0f, 3.0f) == 0.0f);
    assert (sculpt::applyOffset (100.0f, -3.0f) == 100.0f);
    assert (std::abs (sculpt::applyOffset (60.0f, 3.0f) - 63.0f) < 0.0001f);
    sculpt::RoundRobin rr;
    const float a = rr.next (3.0f), b = rr.next (3.0f);
    assert (a != b);
    std::cout << "Morph: 6 anchors + 4 overlapping stages + constant power + RR OK\n";
}
