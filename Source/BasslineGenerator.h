#pragma once
#include "GoaMelodyGenerator.h"
#include "ScaleDefinitions.h"
#include <random>
#include <vector>

namespace PsyMelody {

// Bassline generation mode
enum class BassStyle {
    Rolling = 0,    // Classic rolling 16th bass (Full-On, Goa)
    Offbeat,        // Offbeat pumping bass
    Acid303,        // TB-303 style acid bass with slides
    Minimal,        // Progressive Psy minimal bass
    DarkGroove,     // Dark Psy twisted bass
    NumStyles
};

inline const std::vector<std::string> BASS_STYLE_NAMES = {
    "Rolling", "Offbeat", "Acid 303", "Minimal", "Dark Groove"
};

struct BassParams {
    int rootNote = 9;       // A
    int scaleIndex = 0;
    float bpm = 145.0f;
    int phraseLengthBars = 4;
    int bassStyle = 0;      // BassStyle index
    float density = 0.7f;
    float acidAmount = 0.5f;
    float variation = 0.3f;
};

class BasslineGenerator {
public:
    BasslineGenerator() : rng(std::random_device{}()) {}

    void setSeed(unsigned int seed) { rng.seed(seed); }

    std::vector<NoteEvent> generateBassline(const BassParams& params)
    {
        std::vector<NoteEvent> events;
        const auto& scale = GOA_SCALES[static_cast<size_t>(
            std::clamp(params.scaleIndex, 0, (int)GOA_SCALES.size() - 1))];

        int baseNote = params.rootNote + 2 * 12; // Octave 2 for bass

        for (int bar = 0; bar < params.phraseLengthBars; ++bar) {
            auto barEvents = generateBar(params, baseNote, bar, scale);
            for (auto& e : barEvents) {
                e.startBeat += bar * 4.0;
                events.push_back(e);
            }
        }

        // Apply acid articulation
        if (params.acidAmount > 0.0f)
            applyBassAcid(events, params);

        return events;
    }

private:
    std::mt19937 rng;

    std::vector<NoteEvent> generateBar(const BassParams& params, int baseNote,
                                        int /*barIndex*/, const ScaleType& scale)
    {
        std::vector<NoteEvent> events;
        int style = std::clamp(params.bassStyle, 0, 4);

        // Different rhythm patterns per style
        std::vector<std::pair<double, double>> hits; // beat position, duration

        switch (style) {
        case 0: // Rolling 16ths
            for (int i = 0; i < 16; ++i) {
                if (randomFloat() < params.density)
                    hits.push_back({i * 0.25, 0.2});
            }
            break;

        case 1: // Offbeat
            // Classic psytrance offbeat: notes on upbeats
            hits.push_back({0.5, 0.4});
            hits.push_back({1.5, 0.4});
            hits.push_back({2.5, 0.4});
            hits.push_back({3.5, 0.4});
            if (params.density > 0.5f) {
                hits.push_back({0.0, 0.3});
                hits.push_back({2.0, 0.3});
            }
            break;

        case 2: // Acid 303
            {
                // Classic 303 patterns
                static const std::vector<std::vector<int>> acidPatterns = {
                    {1,0,1,1, 0,1,0,1, 1,0,1,1, 0,1,0,1},
                    {1,1,0,1, 1,0,1,0, 1,1,0,1, 1,0,1,0},
                    {1,0,0,1, 0,0,1,0, 1,0,0,1, 0,1,0,0},
                };
                int patIdx = randomInt(0, (int)acidPatterns.size() - 1);
                const auto& pat = acidPatterns[static_cast<size_t>(patIdx)];
                for (int i = 0; i < 16; ++i)
                    if (pat[static_cast<size_t>(i)])
                        hits.push_back({i * 0.25, 0.22});
            }
            break;

        case 3: // Minimal
            // Very sparse
            hits.push_back({0.0, 0.8});
            if (params.density > 0.3f) hits.push_back({2.0, 0.8});
            if (params.density > 0.6f) hits.push_back({3.0, 0.4});
            break;

        case 4: // Dark Groove
            {
                // Irregular, syncopated
                static const std::vector<std::vector<int>> darkPatterns = {
                    {1,0,1,0, 0,1,1,0, 1,0,0,1, 0,1,0,0},
                    {1,1,0,0, 1,0,0,1, 0,1,1,0, 0,0,1,0},
                };
                int patIdx = randomInt(0, (int)darkPatterns.size() - 1);
                const auto& pat = darkPatterns[static_cast<size_t>(patIdx)];
                for (int i = 0; i < 16; ++i)
                    if (pat[static_cast<size_t>(i)])
                        hits.push_back({i * 0.25, 0.2});
            }
            break;
        }

        // Create note events from hits
        int currentNote = baseNote;
        for (auto& [beatPos, dur] : hits) {
            NoteEvent e;
            e.noteNumber = currentNote;
            e.velocity = 0.7f + randomFloat(0.0f, 0.2f);
            e.pan = 0.0f;
            e.startBeat = beatPos;
            e.duration = dur;
            e.pitchBend = 0;
            e.accent = false;
            e.slide = false;
            e.isGraceNote = false;

            // Emphasize downbeat
            if (std::abs(beatPos) < 0.01)
                e.velocity = std::min(1.0f, e.velocity + 0.15f);

            events.push_back(e);

            // Choose next note (bass stays close to root)
            if (randomFloat() < params.variation) {
                int interval = randomInt(0, 3);
                static const int bassIntervals[] = {0, 0, -5, 7}; // root, root, 4th below, 5th
                currentNote = quantizeToScale(baseNote + bassIntervals[interval],
                                               params.rootNote, scale);
                // Keep in bass range
                while (currentNote > baseNote + 7) currentNote -= 12;
                while (currentNote < baseNote - 7) currentNote += 12;
            } else {
                currentNote = baseNote;
            }
        }

        return events;
    }

    void applyBassAcid(std::vector<NoteEvent>& events, const BassParams& params)
    {
        for (size_t i = 0; i < events.size(); ++i) {
            if (randomFloat() < params.acidAmount * 0.3f) {
                events[i].accent = true;
                events[i].velocity = std::min(1.0f, events[i].velocity + 0.2f);
            }
            if (i + 1 < events.size() && randomFloat() < params.acidAmount * 0.25f) {
                events[i].slide = true;
                events[i].duration = events[i + 1].startBeat - events[i].startBeat + 0.04;
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
