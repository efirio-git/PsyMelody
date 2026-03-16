#pragma once
#include <vector>
#include <string>
#include <map>

namespace PsyMelody {

// Intervals defined as semitones from root
struct ScaleType {
    std::string name;
    std::vector<int> intervals;  // semitones from root
};

// Goa Trance core scales
inline const std::vector<ScaleType> GOA_SCALES = {
    // The quintessential Goa scale - Middle Eastern / Indian flavor
    {"Phrygian Dominant",    {0, 1, 4, 5, 7, 8, 10}},
    // Dark, exotic feel used extensively in Goa
    {"Double Harmonic",      {0, 1, 4, 5, 7, 8, 11}},
    // Classic minor with raised 7th - tension and resolution
    {"Harmonic Minor",       {0, 2, 3, 5, 7, 8, 11}},
    // Natural minor - more straightforward passages
    {"Natural Minor",        {0, 2, 3, 5, 7, 8, 10}},
    // Japanese scale - occasionally used for variation
    {"Hirajoshi",            {0, 2, 3, 7, 8}},
    // Hungarian Minor - dramatic, cinematic Goa leads
    {"Hungarian Minor",      {0, 2, 3, 6, 7, 8, 11}},
    // Phrygian - darker passages
    {"Phrygian",             {0, 1, 3, 5, 7, 8, 10}},
};

// Root note names for display
inline const std::vector<std::string> NOTE_NAMES = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Get MIDI notes for a scale within an octave range
inline std::vector<int> getScaleNotes(int rootNote, const ScaleType& scale,
                                       int lowestOctave = 3, int highestOctave = 6)
{
    std::vector<int> notes;
    for (int octave = lowestOctave; octave <= highestOctave; ++octave) {
        for (int interval : scale.intervals) {
            int midiNote = rootNote + (octave * 12) + interval;
            if (midiNote >= 0 && midiNote <= 127)
                notes.push_back(midiNote);
        }
    }
    return notes;
}

// Quantize a MIDI note to the nearest scale degree
inline int quantizeToScale(int midiNote, int rootNote, const ScaleType& scale)
{
    int noteInOctave = ((midiNote - rootNote) % 12 + 12) % 12;
    int octave = (midiNote - rootNote) / 12;
    if (midiNote < rootNote) octave--;

    int bestInterval = 0;
    int bestDist = 12;
    for (int interval : scale.intervals) {
        int dist = std::abs(noteInOctave - interval);
        if (dist < bestDist) {
            bestDist = dist;
            bestInterval = interval;
        }
    }
    return rootNote + (octave * 12) + bestInterval;
}

} // namespace PsyMelody
