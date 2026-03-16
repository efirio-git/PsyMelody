#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "GoaMelodyGenerator.h"

namespace PsyMelody {

// Converts NoteEvents into MIDI messages synchronized with DAW transport
class MidiPatternEngine {
public:
    MidiPatternEngine();

    // Load a generated phrase for playback
    void loadPhrase(const std::vector<NoteEvent>& phrase);

    // Process a block - adds MIDI events to the buffer based on playhead position
    void processBlock(juce::MidiBuffer& midiBuffer,
                      double bpm,
                      double ppqPosition,
                      int numSamples,
                      double sampleRate,
                      bool isPlaying);

    // Reset playback state
    void reset();

    bool hasPhrase() const { return !currentPhrase.empty(); }
    double getPhraseLengthBeats() const { return phraseLengthBeats; }

private:
    std::vector<NoteEvent> currentPhrase;
    double phraseLengthBeats = 16.0; // 4 bars default

    // Track active notes for proper note-off
    struct ActiveNote {
        int noteNumber;
        int channel;
        double endBeat;
    };
    std::vector<ActiveNote> activeNotes;

    double lastPpqPosition = -1.0;

    void sendNoteOff(juce::MidiBuffer& buffer, int note, int channel, int sampleOffset);
    void sendNoteOn(juce::MidiBuffer& buffer, int note, float velocity,
                    int channel, int sampleOffset);
    void sendPitchBend(juce::MidiBuffer& buffer, int bendValue,
                       int channel, int sampleOffset);
};

} // namespace PsyMelody
