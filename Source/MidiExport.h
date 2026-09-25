#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "GoaMelodyGenerator.h"
#include <map>
#include <optional>
#include <utility>
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

        // Accent / slide / grace flags, which plain MIDI cannot represent
        if (auto articulation = createArticulationMetaEvent(events, ticksPerBeat)) {
            articulation->setTimeStamp(0);
            sequence.addEvent(*articulation);
        }

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

        // Serialise in memory first so a failure never touches the target
        juce::MemoryOutputStream data;
        if (!midiFile.writeTo(data)) return false;

        // Overwrite in place. FileOutputStream appends to an existing file, so
        // it must be truncated explicitly or an overwritten .mid gets corrupted.
        // Rewriting the same file (rather than renaming a temp file over it)
        // keeps its identity: a replaced file was shown greyed out in the next
        // macOS open panel until the panel was reopened. It also keeps the
        // file's permissions and other attributes.
        juce::FileOutputStream stream(file);
        if (!stream.openedOk()) return false;
        if (!stream.setPosition(0) || stream.truncate().failed()) return false;
        if (!stream.write(data.getData(), data.getDataSize())) return false;
        stream.flush();
        return !stream.getStatus().failed();
    }

    // ------------------------------------------------------------------
    // Articulation round-trip
    //
    // Accent, slide and grace-note flags have no MIDI equivalent; on export
    // they only survive as velocity / length. To restore them when PsyMelody
    // re-imports its own file, the flags are stored in one sequencer-specific
    // meta event (FF 7F), which the MIDI spec reserves for this purpose and
    // other software ignores. Payload (ASCII):
    //     "}PSYM1;" { "<tick>,<note>,<flags>;" }
    // where 0x7D ('}') is the non-commercial manufacturer ID, <tick> is the
    // note-on tick exactly as written to the file, and <flags> is a bitmask
    // (1 = accent, 2 = slide, 4 = grace). Only flagged notes are listed.
    // ------------------------------------------------------------------
    enum ArticulationFlag { accentFlag = 1, slideFlag = 2, graceFlag = 4 };
    using ArticulationMap = std::map<std::pair<int, int>, std::vector<int>>;  // (tick, note) -> flags

    static juce::String articulationPrefix() { return "}PSYM1;"; }

    // The tick MidiFile writes for a note-on (it rounds timestamps to the nearest tick)
    static int noteOnTick(double startBeat, int ticksPerBeat)
    {
        return juce::roundToInt(startBeat * ticksPerBeat);
    }

    static std::optional<juce::MidiMessage> createArticulationMetaEvent(
        const std::vector<NoteEvent>& events, int ticksPerBeat)
    {
        auto flagsOf = [](const NoteEvent& e) {
            return (e.accent ? accentFlag : 0) | (e.slide ? slideFlag : 0)
                 | (e.isGraceNote ? graceFlag : 0);
        };

        // Notes sharing a (tick, note) key are matched in order on import, so a
        // shared key lists every note on it (including unflagged ones) to keep
        // the flags aligned; unique keys list only flagged notes
        std::map<std::pair<int, int>, int> keyCount;
        for (const auto& e : events)
            ++keyCount[{ noteOnTick(e.startBeat, ticksPerBeat), e.noteNumber }];

        juce::String payload;
        bool anyFlag = false;
        for (const auto& e : events) {
            const int tick = noteOnTick(e.startBeat, ticksPerBeat);
            const int flags = flagsOf(e);
            anyFlag = anyFlag || flags != 0;
            if (flags != 0 || keyCount[{ tick, e.noteNumber }] > 1)
                payload << tick << ',' << e.noteNumber << ',' << flags << ';';
        }
        if (!anyFlag) return std::nullopt;

        // Build FF 7F <variable-length size> <payload> by hand: JUCE's
        // textMetaEvent only accepts the text types 1-15
        const auto body = articulationPrefix() + payload;
        const auto* bytes = body.toRawUTF8();
        const auto size = (juce::uint32) body.getNumBytesAsUTF8();

        juce::MemoryBlock data;
        const juce::uint8 header[] = { 0xff, 0x7f };
        data.append(header, sizeof(header));
        juce::uint8 lengthBytes[5];
        int numLengthBytes = 0;
        for (auto v = size; ; v >>= 7) {
            lengthBytes[numLengthBytes++] = (juce::uint8) (v & 0x7f);
            if (v < 0x80) break;
        }
        for (int i = numLengthBytes - 1; i >= 0; --i) {
            const juce::uint8 b = (juce::uint8) (lengthBytes[i] | (i > 0 ? 0x80 : 0));
            data.append(&b, 1);
        }
        data.append(bytes, size);
        return juce::MidiMessage(data.getData(), (int) data.getSize());
    }

    // Returns the flags stored by createArticulationMetaEvent, or an empty map
    // if the message is not a PsyMelody articulation event
    static ArticulationMap parseArticulationMetaEvent(const juce::MidiMessage& msg)
    {
        ArticulationMap result;
        if (!msg.isMetaEvent() || msg.getMetaEventType() != 0x7F) return result;

        auto text = juce::String::fromUTF8(reinterpret_cast<const char*>(msg.getMetaEventData()),
                                           msg.getMetaEventLength());
        if (!text.startsWith(articulationPrefix())) return result;

        auto entries = juce::StringArray::fromTokens(text.substring(articulationPrefix().length()),
                                                     ";", {});
        for (const auto& entry : entries) {
            auto fields = juce::StringArray::fromTokens(entry, ",", {});
            if (fields.size() != 3) continue;
            result[{ fields[0].getIntValue(), fields[1].getIntValue() }]
                .push_back(fields[2].getIntValue());
        }
        return result;
    }

    // Converts a MIDI file to note events: note-on/off pairs from every track,
    // CC10 as pan, pitch wheel as bend, and PsyMelody's articulation flags when
    // the file carries them. Durations under 0.01 beat become 0.25.
    static std::vector<NoteEvent> importFromMidiFile(const juce::MidiFile& midiFile)
    {
        std::vector<NoteEvent> imported;
        int ticksPerBeat = midiFile.getTimeFormat();
        if (ticksPerBeat <= 0) ticksPerBeat = 480;

        ArticulationMap articulation;
        for (int track = 0; track < midiFile.getNumTracks(); ++track)
            if (const auto* seq = midiFile.getTrack(track))
                for (int i = 0; i < seq->getNumEvents(); ++i)
                    for (auto& [key, flags] : parseArticulationMetaEvent(seq->getEventPointer(i)->message))
                        articulation[key].insert(articulation[key].end(), flags.begin(), flags.end());

        for (int track = 0; track < midiFile.getNumTracks(); ++track) {
            const auto* seq = midiFile.getTrack(track);
            if (!seq) continue;

            struct NoteStart { double tick; float velocity; };
            std::map<int, NoteStart> activeNotes;
            float currentPan = 0.0f;
            int currentPitchBend = 0;

            for (int i = 0; i < seq->getNumEvents(); ++i) {
                const auto& evt = seq->getEventPointer(i)->message;

                if (evt.isController() && evt.getControllerNumber() == 10)
                    currentPan = (evt.getControllerValue() - 64) / 64.0f;
                else if (evt.isPitchWheel())
                    currentPitchBend = evt.getPitchWheelValue() - 8192;
                else if (evt.isNoteOn() && evt.getVelocity() > 0)
                    activeNotes[evt.getNoteNumber()] = {evt.getTimeStamp(), evt.getFloatVelocity()};
                else if (evt.isNoteOff() || (evt.isNoteOn() && evt.getVelocity() == 0)) {
                    auto it = activeNotes.find(evt.getNoteNumber());
                    if (it != activeNotes.end()) {
                        NoteEvent note;
                        note.noteNumber = evt.getNoteNumber();
                        note.velocity = it->second.velocity;
                        note.pan = currentPan;
                        note.startBeat = it->second.tick / ticksPerBeat;
                        note.duration = (evt.getTimeStamp() - it->second.tick) / ticksPerBeat;
                        if (note.duration < 0.01) note.duration = 0.25;
                        note.pitchBend = currentPitchBend;

                        int flags = 0;
                        auto found = articulation.find({ juce::roundToInt(it->second.tick), note.noteNumber });
                        if (found != articulation.end() && !found->second.empty()) {
                            flags = found->second.front();
                            found->second.erase(found->second.begin());
                        }
                        note.accent = (flags & accentFlag) != 0;
                        note.slide = (flags & slideFlag) != 0;
                        note.isGraceNote = (flags & graceFlag) != 0;

                        imported.push_back(note);
                        activeNotes.erase(it);
                    }
                }
            }
        }
        return imported;
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
