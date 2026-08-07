#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "GoaMelodyGenerator.h"
#include <vector>

namespace PsyMelody {

class MidiExport {
public:
    // Export note events to a MIDI file
    static bool exportToFile(const std::vector<NoteEvent>& events,
                              const juce::File& file,
                              double bpm,
                              int timeSigNum = 4,
                              int timeSigDen = 4)
    {
        juce::MidiMessageSequence sequence;
        int ticksPerBeat = 480;

        // Tempo event
        auto tempoEvent = juce::MidiMessage::tempoMetaEvent(
            (int)(60000000.0 / bpm));  // microseconds per beat
        tempoEvent.setTimeStamp(0);
        sequence.addEvent(tempoEvent);

        // Time signature
        auto timeSig = juce::MidiMessage::timeSignatureMetaEvent(timeSigNum, timeSigDen);
        timeSig.setTimeStamp(0);
        sequence.addEvent(timeSig);

        // Add note events (assumed sorted by startBeat)
        int lastPanCC = -1;
        for (const auto& event : events) {
            double startTick = event.startBeat * ticksPerBeat;
            double endTick = (event.startBeat + event.duration) * ticksPerBeat;
            int vel = std::clamp((int)(event.velocity * 127.0f), 1, 127);
            int channel = 1;

            // Pan as CC10, written only on change so a panned note is
            // followed by a re-center for later centered notes
            int panCC = std::clamp((int)std::lround((event.pan + 1.0f) * 0.5f * 127.0f), 0, 127);
            if (panCC != lastPanCC) {
                auto panMsg = juce::MidiMessage::controllerEvent(channel, 10, panCC);
                panMsg.setTimeStamp(startTick);
                sequence.addEvent(panMsg);
                lastPanCC = panCC;
            }

            // Pitch bend before note if needed
            if (event.pitchBend != 0) {
                auto pb = juce::MidiMessage::pitchWheel(channel, event.pitchBend + 8192);
                pb.setTimeStamp(startTick);
                sequence.addEvent(pb);
            }

            // Note on
            auto noteOn = juce::MidiMessage::noteOn(channel, event.noteNumber, (juce::uint8)vel);
            noteOn.setTimeStamp(startTick);
            sequence.addEvent(noteOn);

            // Note off
            auto noteOff = juce::MidiMessage::noteOff(channel, event.noteNumber);
            noteOff.setTimeStamp(endTick);
            sequence.addEvent(noteOff);

            // Reset pitch bend after note
            if (event.pitchBend != 0) {
                auto pbReset = juce::MidiMessage::pitchWheel(channel, 8192);
                pbReset.setTimeStamp(endTick);
                sequence.addEvent(pbReset);
            }
        }

        sequence.sort();
        sequence.updateMatchedPairs();

        // Create MIDI file
        juce::MidiFile midiFile;
        midiFile.setTicksPerQuarterNote(ticksPerBeat);
        midiFile.addTrack(sequence);

        // Write to file
        juce::FileOutputStream stream(file);
        if (!stream.openedOk()) return false;
        return midiFile.writeTo(stream);
    }

    // Export to a MidiMessageSequence (for clipboard)
    static juce::MidiMessageSequence toMidiSequence(const std::vector<NoteEvent>& events,
                                                      int ticksPerBeat = 480)
    {
        juce::MidiMessageSequence sequence;

        for (const auto& event : events) {
            double startTick = event.startBeat * ticksPerBeat;
            double endTick = (event.startBeat + event.duration) * ticksPerBeat;
            int vel = std::clamp((int)(event.velocity * 127.0f), 1, 127);
            int channel = 1;

            auto noteOn = juce::MidiMessage::noteOn(channel, event.noteNumber, (juce::uint8)vel);
            noteOn.setTimeStamp(startTick);
            sequence.addEvent(noteOn);

            auto noteOff = juce::MidiMessage::noteOff(channel, event.noteNumber);
            noteOff.setTimeStamp(endTick);
            sequence.addEvent(noteOff);
        }

        sequence.sort();
        sequence.updateMatchedPairs();
        return sequence;
    }
};

} // namespace PsyMelody
