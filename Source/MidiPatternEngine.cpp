#include "MidiPatternEngine.h"

namespace PsyMelody {

MidiPatternEngine::MidiPatternEngine()
{
    activeNotes.reserve(128); // avoid heap allocation on the audio thread
}

void MidiPatternEngine::loadPhrase(const std::vector<NoteEvent>& phrase)
{
    // Build the copy and compute the length outside the lock so the
    // critical section is only a pointer swap.
    std::vector<NoteEvent> local(phrase);
    double len = phraseLengthBeats.load();
    if (!phrase.empty()) {
        // Calculate phrase length from the last event
        double maxEnd = 0.0;
        for (const auto& e : phrase)
            maxEnd = std::max(maxEnd, e.startBeat + e.duration);
        // Round up to nearest bar (4 beats)
        len = std::ceil(maxEnd / 4.0) * 4.0;
    }

    juce::SpinLock::ScopedLockType sl(phraseLock);
    currentPhrase.swap(local);
    phraseLengthBeats.store(len);
    // Sounding notes get their note-offs on the next audio block instead of
    // being dropped here, so the DAW never receives an orphaned note-on.
    flushActiveNotes = true;
}

void MidiPatternEngine::reset()
{
    juce::SpinLock::ScopedLockType sl(phraseLock);
    activeNotes.clear();
    flushActiveNotes = false;
    lastPanCC = -1;
    hasExpectedNextBeat = false;
}

void MidiPatternEngine::processBlock(juce::MidiBuffer& midiBuffer,
                                       double bpm,
                                       double ppqPosition,
                                       int numSamples,
                                       double sampleRate,
                                       bool isPlaying)
{
    juce::SpinLock::ScopedTryLockType tl(phraseLock);
    if (!tl.isLocked())
        return; // phrase is being swapped; skip this block

    // Must run before this block's note-on scan so a reloaded phrase can
    // retrigger the same pitches without the flush killing them.
    if (flushActiveNotes) {
        for (const auto& active : activeNotes)
            sendNoteOff(midiBuffer, active, 0);
        activeNotes.clear();
        flushActiveNotes = false;
        lastPanCC = -1;
    }

    if (!isPlaying || currentPhrase.empty()) {
        // Send note-offs for any active notes
        for (const auto& active : activeNotes)
            sendNoteOff(midiBuffer, active, 0);
        activeNotes.clear();
        hasExpectedNextBeat = false;
        return;
    }

    const double phraseLen = phraseLengthBeats.load();
    double beatsPerSample = bpm / (60.0 * sampleRate);
    double blockStartBeat = ppqPosition;
    double blockEndBeat = ppqPosition + numSamples * beatsPerSample;

    // Transport jump (DAW loop wrap, locate, scrub): sounding notes may never
    // reach their end position again - e.g. a 2-bar DAW loop over a 4-bar
    // phrase never plays past bar 2 - so they would hang. Stop them first.
    if (hasExpectedNextBeat && std::abs(blockStartBeat - expectedNextBeat) > jumpToleranceBeats) {
        for (const auto& active : activeNotes)
            sendNoteOff(midiBuffer, active, 0);
        activeNotes.clear();
    }
    expectedNextBeat = blockEndBeat;
    hasExpectedNextBeat = true;

    // Loop position within phrase
    auto loopPos = [phraseLen](double beat) -> double {
        double pos = std::fmod(beat, phraseLen);
        if (pos < 0.0) pos += phraseLen;
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
            if (beatDelta < 0) beatDelta += phraseLen;
            int sampleOffset = std::max(0, std::min(numSamples - 1,
                (int)(beatDelta / beatsPerSample)));
            sendNoteOff(midiBuffer, *it, sampleOffset);
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
            if (beatDelta < 0) beatDelta += phraseLen;
            int sampleOffset = std::max(0, std::min(numSamples - 1,
                (int)(beatDelta / beatsPerSample)));

            // Pan as CC10, emitted only on change so a panned note is
            // followed by a re-center for later centered notes
            int panCC = juce::jlimit(0, 127,
                (int)std::lround((event.pan + 1.0f) * 0.5f * 127.0f));
            if (panCC != lastPanCC) {
                midiBuffer.addEvent(juce::MidiMessage::controllerEvent(1, 10, panCC),
                                    sampleOffset);
                lastPanCC = panCC;
            }

            // Send pitch bend if needed
            if (event.pitchBend != 0)
                sendPitchBend(midiBuffer, event.pitchBend + 8192, 1, sampleOffset);

            sendNoteOn(midiBuffer, event.noteNumber, event.velocity, 1, sampleOffset);

            ActiveNote active;
            active.noteNumber = event.noteNumber;
            active.channel = 1;
            active.hadPitchBend = (event.pitchBend != 0);
            active.endBeat = std::fmod(event.startBeat + event.duration, phraseLen);
            // A note ending exactly at the phrase end wraps to 0 and would
            // collide with the next loop's beat-0 note-ons; end it just before
            if (active.endBeat <= 0.0 || active.endBeat >= phraseLen)
                active.endBeat = phraseLen - 1.0e-4;
            activeNotes.push_back(active);
        }
    }
}

void MidiPatternEngine::sendNoteOn(juce::MidiBuffer& buffer, int note,
                                     float velocity, int channel, int sampleOffset)
{
    auto msg = juce::MidiMessage::noteOn(channel, note,
                                          (juce::uint8)(velocity * 127));
    buffer.addEvent(msg, sampleOffset);
}

void MidiPatternEngine::sendNoteOff(juce::MidiBuffer& buffer,
                                      const ActiveNote& note, int sampleOffset)
{
    buffer.addEvent(juce::MidiMessage::noteOff(note.channel, note.noteNumber),
                    sampleOffset);
    // Re-center the pitch wheel so the bend doesn't detune later notes
    if (note.hadPitchBend)
        sendPitchBend(buffer, 8192, note.channel, sampleOffset);
}

void MidiPatternEngine::sendPitchBend(juce::MidiBuffer& buffer, int bendValue,
                                        int channel, int sampleOffset)
{
    auto msg = juce::MidiMessage::pitchWheel(channel, bendValue);
    buffer.addEvent(msg, sampleOffset);
}

} // namespace PsyMelody
