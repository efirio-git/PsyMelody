#include "MidiPatternEngine.h"

namespace PsyMelody {

MidiPatternEngine::MidiPatternEngine() {}

void MidiPatternEngine::loadPhrase(const std::vector<NoteEvent>& phrase)
{
    currentPhrase = phrase;
    if (!phrase.empty()) {
        // Calculate phrase length from the last event
        double maxEnd = 0.0;
        for (const auto& e : phrase)
            maxEnd = std::max(maxEnd, e.startBeat + e.duration);
        // Round up to nearest bar (4 beats)
        phraseLengthBeats = std::ceil(maxEnd / 4.0) * 4.0;
    }
    reset();
}

void MidiPatternEngine::reset()
{
    activeNotes.clear();
    lastPpqPosition = -1.0;
}

void MidiPatternEngine::processBlock(juce::MidiBuffer& midiBuffer,
                                       double bpm,
                                       double ppqPosition,
                                       int numSamples,
                                       double sampleRate,
                                       bool isPlaying)
{
    if (!isPlaying || currentPhrase.empty()) {
        // Send note-offs for any active notes
        for (const auto& active : activeNotes)
            sendNoteOff(midiBuffer, active.noteNumber, active.channel, 0);
        activeNotes.clear();
        lastPpqPosition = ppqPosition;
        return;
    }

    double beatsPerSample = bpm / (60.0 * sampleRate);
    double blockStartBeat = ppqPosition;
    double blockEndBeat = ppqPosition + numSamples * beatsPerSample;

    // Loop position within phrase
    auto loopPos = [this](double beat) -> double {
        double pos = std::fmod(beat, phraseLengthBeats);
        if (pos < 0.0) pos += phraseLengthBeats;
        return pos;
    };

    // Check for note-offs first
    for (auto it = activeNotes.begin(); it != activeNotes.end();) {
        double noteEndInPhrase = it->endBeat;
        double currentPos = loopPos(blockStartBeat);

        // Check if note should end during this block
        bool shouldEnd = false;
        double endPos = loopPos(blockEndBeat);

        if (currentPos <= endPos) {
            shouldEnd = (noteEndInPhrase >= currentPos && noteEndInPhrase < endPos);
        } else {
            // Wrapped around
            shouldEnd = (noteEndInPhrase >= currentPos || noteEndInPhrase < endPos);
        }

        if (shouldEnd) {
            double beatDelta = noteEndInPhrase - currentPos;
            if (beatDelta < 0) beatDelta += phraseLengthBeats;
            int sampleOffset = std::max(0, std::min(numSamples - 1,
                (int)(beatDelta / beatsPerSample)));
            sendNoteOff(midiBuffer, it->noteNumber, it->channel, sampleOffset);
            it = activeNotes.erase(it);
        } else {
            ++it;
        }
    }

    // Check for note-ons
    double currentPos = loopPos(blockStartBeat);
    double endPos = loopPos(blockEndBeat);

    for (const auto& event : currentPhrase) {
        bool shouldTrigger = false;

        if (currentPos <= endPos) {
            shouldTrigger = (event.startBeat >= currentPos && event.startBeat < endPos);
        } else {
            // Wrapped around phrase boundary
            shouldTrigger = (event.startBeat >= currentPos || event.startBeat < endPos);
        }

        if (shouldTrigger) {
            double beatDelta = event.startBeat - currentPos;
            if (beatDelta < 0) beatDelta += phraseLengthBeats;
            int sampleOffset = std::max(0, std::min(numSamples - 1,
                (int)(beatDelta / beatsPerSample)));

            // Send pitch bend if needed
            if (event.pitchBend != 0)
                sendPitchBend(midiBuffer, event.pitchBend + 8192, 1, sampleOffset);

            sendNoteOn(midiBuffer, event.noteNumber, event.velocity, 1, sampleOffset);

            ActiveNote active;
            active.noteNumber = event.noteNumber;
            active.channel = 1;
            active.endBeat = std::fmod(event.startBeat + event.duration, phraseLengthBeats);
            activeNotes.push_back(active);
        }
    }

    lastPpqPosition = ppqPosition;
}

void MidiPatternEngine::sendNoteOn(juce::MidiBuffer& buffer, int note,
                                     float velocity, int channel, int sampleOffset)
{
    auto msg = juce::MidiMessage::noteOn(channel, note,
                                          (juce::uint8)(velocity * 127));
    buffer.addEvent(msg, sampleOffset);
}

void MidiPatternEngine::sendNoteOff(juce::MidiBuffer& buffer, int note,
                                      int channel, int sampleOffset)
{
    auto msg = juce::MidiMessage::noteOff(channel, note);
    buffer.addEvent(msg, sampleOffset);
}

void MidiPatternEngine::sendPitchBend(juce::MidiBuffer& buffer, int bendValue,
                                        int channel, int sampleOffset)
{
    auto msg = juce::MidiMessage::pitchWheel(channel, bendValue);
    buffer.addEvent(msg, sampleOffset);
}

} // namespace PsyMelody
