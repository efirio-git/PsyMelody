#pragma once
#include "GoaMelodyGenerator.h"
#include "ScaleDefinitions.h"
#include <random>
#include <vector>

namespace PsyMelody {

enum class VoicingStyle {
    Pad = 0,        // Sustained chord pads
    Stab,           // Short rhythmic stabs
    Arp,            // Arpeggiated chords
    Pluck,          // Short plucked voicings
    NumStyles
};

inline const std::vector<std::string> VOICING_STYLE_NAMES = {
    "Pad", "Stab", "Arp", "Pluck"
};

struct ChordVoicingParams {
    int rootNote = 9;       // A
    int scaleIndex = 0;
    float bpm = 145.0f;
    int phraseLengthBars = 4;
    int progression = 0;    // GoaProgression index
    int voicingStyle = 0;   // VoicingStyle index
    int baseOctave = 4;
    float density = 0.5f;
    int numVoices = 3;      // 2-5 voices
};

class ChordVoicingGenerator {
public:
    ChordVoicingGenerator() : rng(std::random_device{}()) {}

    void setSeed(unsigned int seed) { rng.seed(seed); }

    std::vector<NoteEvent> generateVoicing(const ChordVoicingParams& params)
    {
        std::vector<NoteEvent> events;
        auto progression = getChordProgression(params);
        int style = std::clamp(params.voicingStyle, 0, 3);

        for (int bar = 0; bar < params.phraseLengthBars; ++bar) {
            auto& chord = progression[static_cast<size_t>(bar % progression.size())];
            auto voicing = buildVoicing(chord, params);

            switch (style) {
            case 0: // Pad - sustained whole/half notes
                addPadVoicing(events, voicing, bar, params);
                break;
            case 1: // Stab - short rhythmic hits
                addStabVoicing(events, voicing, bar, params);
                break;
            case 2: // Arp - arpeggiated
                addArpVoicing(events, voicing, bar, params);
                break;
            case 3: // Pluck - short plucked notes
                addPluckVoicing(events, voicing, bar, params);
                break;
            }
        }

        return events;
    }

private:
    std::mt19937 rng;

    struct ChordDef {
        int rootInterval;
        std::vector<int> tones;
    };

    std::vector<ChordDef> getChordProgression(const ChordVoicingParams& params)
    {
        // Reuse the same progressions as GoaMelodyGenerator
        std::vector<std::vector<ChordDef>> progressions = {
            {{0, {0, 3, 7}}},                                          // Drone (minor)
            {{0, {0, 3, 7}}, {1, {0, 4, 7}}},                         // i - bII
            {{0, {0, 3, 7}}, {10, {0, 4, 7}}},                        // i - bVII
            {{0, {0, 3, 7}}, {8, {0, 4, 7}}},                         // i - bVI
            {{0, {0, 3, 7}}, {5, {0, 3, 7}}},                         // i - iv
            {{0, {0, 3, 7}}, {1, {0, 4, 7}}, {10, {0, 4, 7}}, {0, {0, 3, 7}}}, // i-bII-bVII-i
            {{0, {0, 3, 7}}, {5, {0, 3, 7}}, {8, {0, 4, 7}}, {10, {0, 4, 7}}}, // i-iv-bVI-bVII (Full-On)
            {{0, {0, 7}}, {1, {0, 7}}, {0, {0, 7}}, {11, {0, 7}}},             // Chromatic Drone (Dark Psy)
            {{0, {0, 3, 7}}, {10, {0, 4, 7}}, {8, {0, 4, 7}}, {7, {0, 3, 7}}}, // i-bVII-bVI-v (Progressive)
        };

        int idx = std::clamp(params.progression, 0, (int)progressions.size() - 1);
        auto& prog = progressions[static_cast<size_t>(idx)];

        std::vector<ChordDef> result;
        for (int bar = 0; bar < params.phraseLengthBars; ++bar)
            result.push_back(prog[static_cast<size_t>(bar % prog.size())]);

        return result;
    }

    std::vector<int> buildVoicing(const ChordDef& chord, const ChordVoicingParams& params)
    {
        std::vector<int> notes;
        int baseNote = params.rootNote + params.baseOctave * 12 + chord.rootInterval;
        int numV = std::clamp(params.numVoices, 2, 5);

        const auto& scale = GOA_SCALES[static_cast<size_t>(
            std::clamp(params.scaleIndex, 0, (int)GOA_SCALES.size() - 1))];

        for (int v = 0; v < numV && v < (int)chord.tones.size(); ++v) {
            int note = baseNote + chord.tones[static_cast<size_t>(v)];
            note = quantizeToScale(note, params.rootNote, scale);
            notes.push_back(note);
        }

        // Add extra voices by doubling with octave spread
        while ((int)notes.size() < numV) {
            int idx = randomInt(0, (int)notes.size() - 1);
            int note = notes[static_cast<size_t>(idx)] + 12;
            if (note <= 127)
                notes.push_back(note);
            else
                break;
        }

        return notes;
    }

    void addPadVoicing(std::vector<NoteEvent>& events, const std::vector<int>& voicing,
                        int bar, const ChordVoicingParams& params)
    {
        double startBeat = bar * 4.0;
        double duration = 4.0; // whole bar
        if (params.density < 0.5f) duration = 8.0; // two bars for sparse

        for (int note : voicing) {
            NoteEvent e;
            e.noteNumber = note;
            e.velocity = 0.5f + randomFloat(0.0f, 0.1f);
            e.pan = randomFloat(-0.3f, 0.3f); // slight stereo spread
            e.startBeat = startBeat;
            e.duration = duration;
            e.pitchBend = 0;
            e.accent = false;
            e.slide = false;
            e.isGraceNote = false;
            events.push_back(e);
        }
    }

    void addStabVoicing(std::vector<NoteEvent>& events, const std::vector<int>& voicing,
                         int bar, const ChordVoicingParams& params)
    {
        // Stabs on beat 1 and optionally 3
        static const std::vector<std::vector<double>> stabPatterns = {
            {0.0},
            {0.0, 2.0},
            {0.0, 1.5, 3.0},
            {0.0, 0.75, 2.0, 2.75},
        };
        int patIdx = std::clamp((int)(params.density * 3.0f), 0, 3);
        const auto& pattern = stabPatterns[static_cast<size_t>(patIdx)];

        for (double beat : pattern) {
            for (int note : voicing) {
                NoteEvent e;
                e.noteNumber = note;
                e.velocity = 0.7f + randomFloat(0.0f, 0.15f);
                e.pan = randomFloat(-0.2f, 0.2f);
                e.startBeat = bar * 4.0 + beat;
                e.duration = 0.15;
                e.pitchBend = 0;
                e.accent = beat < 0.01;
                e.slide = false;
                e.isGraceNote = false;
                events.push_back(e);
            }
        }
    }

    void addArpVoicing(std::vector<NoteEvent>& events, const std::vector<int>& voicing,
                        int bar, const ChordVoicingParams& params)
    {
        if (voicing.empty()) return;
        int stepsPerBar = 8 + (int)(params.density * 8.0f); // 8-16 steps
        double stepLen = 4.0 / stepsPerBar;

        auto sorted = voicing;
        std::sort(sorted.begin(), sorted.end());

        for (int step = 0; step < stepsPerBar; ++step) {
            // Cycle through notes up then down
            int totalNotes = (int)sorted.size();
            int cycleLen = totalNotes * 2 - 2;
            if (cycleLen <= 0) cycleLen = 1;
            int cyclePos = step % cycleLen;
            int noteIdx;
            if (cyclePos < totalNotes)
                noteIdx = cyclePos;
            else
                noteIdx = cycleLen - cyclePos;
            noteIdx = std::clamp(noteIdx, 0, totalNotes - 1);

            NoteEvent e;
            e.noteNumber = sorted[static_cast<size_t>(noteIdx)];
            e.velocity = 0.6f + randomFloat(0.0f, 0.15f);
            e.pan = 0.0f;
            e.startBeat = bar * 4.0 + step * stepLen;
            e.duration = stepLen * 0.8;
            e.pitchBend = 0;
            e.accent = (step % 4 == 0);
            e.slide = false;
            e.isGraceNote = false;
            events.push_back(e);
        }
    }

    void addPluckVoicing(std::vector<NoteEvent>& events, const std::vector<int>& voicing,
                          int bar, const ChordVoicingParams& params)
    {
        // Similar to stab but with slightly staggered timing for strumming effect
        double baseTime = bar * 4.0;
        double strumDelay = 0.02;

        // Pluck on beats determined by density
        std::vector<double> beats = {0.0};
        if (params.density > 0.3f) beats.push_back(2.0);
        if (params.density > 0.6f) { beats.push_back(1.0); beats.push_back(3.0); }

        for (double beat : beats) {
            for (int v = 0; v < (int)voicing.size(); ++v) {
                NoteEvent e;
                e.noteNumber = voicing[static_cast<size_t>(v)];
                e.velocity = 0.65f + randomFloat(0.0f, 0.15f);
                e.pan = randomFloat(-0.4f, 0.4f);
                e.startBeat = baseTime + beat + v * strumDelay;
                e.duration = 0.3;
                e.pitchBend = 0;
                e.accent = false;
                e.slide = false;
                e.isGraceNote = false;
                events.push_back(e);
            }
        }
    }

    float randomFloat(float min = 0.0f, float max = 1.0f)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng);
    }

    int randomInt(int min, int max)
    {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }
};

} // namespace PsyMelody
