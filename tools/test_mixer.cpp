#include "../Source/DrumChannels.h"
#include <cassert>
#include <iostream>
#include <set>

int main()
{
    assert (sculpt::mixerChannelCount == 15);
    std::set<int> notes;
    for (std::size_t i = 0; i < sculpt::mixerChannelCount; ++i)
    {
        const auto note = sculpt::mixerChannels[i].midiNote;
        assert (notes.insert (note).second);
        assert (sculpt::mixerIndexForMidiNote (note) == static_cast<int> (i));
    }
    assert (sculpt::mixerIndexForMidiNote (39) == -1); // clap non disponibile
    assert (sculpt::mixerIndexForMidiNote (70) == -1); // shaker non disponibile
    assert (sculpt::mixerIndexForMidiNote (76) == -1); // combo non disponibile
    std::cout << "Mixer: 15 canali MIDI univoci OK\n";
}
