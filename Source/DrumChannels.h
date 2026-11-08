#pragma once

#include <array>

namespace sculpt
{
// Un solo kit: il mixer regola le articolazioni/pezzi, non i sei stadi MORPH.
// Gli ID sono stabili per automazioni e preset della DAW.
struct DrumChannel
{
    const char* id;
    const char* shortName;
    int midiNote;
};

inline constexpr std::array<DrumChannel, 15> mixerChannels {{
    { "kick",           "KICK", 36 },
    { "snare",          "SNARE", 38 },
    { "snare_rim",      "RIM", 37 },
    { "snare_rimshot",  "SHOT", 40 },
    { "hihat_closed",   "HH C", 42 },
    { "hihat_foot",     "HH F", 44 },
    { "hihat_open",     "HH O", 46 },
    { "tom",            "TOM", 45 },
    { "floor_tom",      "FLOOR", 41 },
    { "crash",          "CRASH", 49 },
    { "reverse_cymbal", "REV", 52 },
    { "ride",           "RIDE", 51 },
    { "ride_bell",      "BELL", 53 },
    { "splash",         "SPLASH", 55 },
    { "percussion",     "PERC", 75 }
}};
inline constexpr std::size_t mixerChannelCount = mixerChannels.size();

inline int mixerIndexForMidiNote (int note)
{
    for (int i = 0; i < static_cast<int> (mixerChannels.size()); ++i)
        if (mixerChannels[static_cast<std::size_t> (i)].midiNote == note)
            return i;
    return -1;
}
}
