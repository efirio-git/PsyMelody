#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "GoaMelodyGenerator.h"
#include <atomic>

namespace PsyMelody {

// Converts NoteEvents into MIDI messages synchronized with DAW transport
class MidiPatternEngine {
public:
    MidiPatternEngine();

    // Load a generated phrase for playback (message thread)
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
    double getPhraseLengthBeats() const { return phraseLengthBeats.load(); }

private:
    // phraseLock guards currentPhrase, activeNotes, flushActiveNotes and lastPanCC.
    // The message thread holds it only for a vector swap; the audio thread
    // uses a try-lock and skips the block on contention.
    juce::SpinLock phraseLock;
    std::vector<NoteEvent> currentPhrase;
    std::atomic<double> phraseLengthBeats { 16.0 }; // 4 bars default
    bool flushActiveNotes = false;

    // Track active notes for proper note-off
    struct ActiveNote {
        int noteNumber;
        int channel;
        double endBeat;
        bool hadPitchBend;
    };
    std::vector<ActiveNote> activeNotes;
    int lastPanCC = -1;

    void sendNoteOff(juce::MidiBuffer& buffer, const ActiveNote& note, int sampleOffset);
    void sendNoteOn(juce::MidiBuffer& buffer, int note, float velocity,
                    int channel, int sampleOffset);
    void sendPitchBend(juce::MidiBuffer& buffer, int bendValue,
                       int channel, int sampleOffset);
};

} // namespace PsyMelody
