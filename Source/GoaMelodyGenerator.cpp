#include "GoaMelodyGenerator.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace PsyMelody {

GoaMelodyGenerator::GoaMelodyGenerator()
    : rng(std::random_device{}())
{
}

void GoaMelodyGenerator::setSeed(unsigned int seed)
{
    rng.seed(seed);
}

// ============================================================
// Goa rhythm patterns on 16th note grid (per bar)
// ============================================================
const std::vector<std::vector<int>>& GoaMelodyGenerator::getGoaRhythmPatterns()
{
    static const std::vector<std::vector<int>> patterns = {
        // === DENSE (indices 0-7): acid lines, arpeggios ===
        {1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1},  // 0: full 16ths
        {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0},  // 1: straight 8ths
        {1,0,1,1, 1,0,1,1, 1,0,1,1, 1,0,1,1},  // 2: dotted 8th feel
        {1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0},  // 3: 3+rest (classic Goa arp)
        {1,1,0,1, 0,1,1,0, 1,1,0,1, 0,1,1,0},  // 4: shuffle-arp feel
        {1,0,1,1, 0,1,1,0, 1,0,1,1, 0,1,1,0},  // 5: shifting accent arp
        {1,1,1,1, 0,1,1,1, 1,1,1,1, 0,1,1,1},  // 6: 303 with rest on beat 2
        {1,0,1,0, 1,1,0,1, 1,0,1,0, 1,1,0,1},  // 7: Hallucinogen-style stutter

        // === MID-DENSITY (indices 8-19): lead melodies, hooks ===
        {1,0,0,1, 0,0,1,0, 1,0,0,1, 0,0,1,0},  // 8:  off-beat emphasis
        {1,0,1,0, 0,1,0,1, 1,0,1,0, 0,1,0,1},  // 9:  alternating feel
        {1,1,0,1, 1,0,1,0, 1,1,0,1, 1,0,1,0},  // 10: galloping
        {1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1},  // 11: dotted 8th delay feel
        {1,0,1,0, 0,0,1,0, 1,0,1,0, 0,0,1,0},  // 12: Astral Projection melodic
        {1,0,0,0, 1,0,1,0, 1,0,0,0, 1,0,1,0},  // 13: call-response 8th
        {1,0,1,0, 1,0,0,1, 0,0,1,0, 1,0,0,0},  // 14: Infected Mushroom phrase
        {1,0,0,1, 0,1,0,0, 1,0,1,0, 0,1,0,0},  // 15: Eastern triplet approx
        {0,0,1,0, 1,0,0,1, 0,0,1,0, 1,0,0,1},  // 16: upbeat-driven
        {1,0,1,0, 0,1,0,0, 1,0,1,0, 0,1,0,0},  // 17: Man With No Name style
        {1,1,0,0, 1,0,0,1, 1,1,0,0, 1,0,0,1},  // 18: punchy lead
        {1,0,0,1, 0,1,1,0, 0,1,0,0, 1,0,1,0},  // 19: asymmetric Goa phrase

        // === SPARSE (indices 20-27): pads, slow melodies ===
        {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0},  // 20: quarter notes
        {1,0,0,0, 0,0,1,0, 0,0,1,0, 0,0,0,0},  // 21: sparse melodic
        {1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0},  // 22: half notes
        {1,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0},  // 23: wide space melody
        {1,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,0},  // 24: minimal 2-note
        {1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,1,0},  // 25: building phrase
        {1,0,0,1, 0,0,0,0, 1,0,0,1, 0,0,0,0},  // 26: call and response
        {1,0,0,0, 1,0,0,0, 0,0,1,0, 0,0,0,0},  // 27: descending space

        // === VERY SPARSE (indices 28-31): ambient ===
        {1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},  // 28: whole note
        {1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0},  // 29: whole + pickup
        {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0},  // 30: off-beat whole
        {1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0},  // 31: slow pulse

        // === FULL-ON (indices 32-39): offbeat emphasis, supersaw riffs, build-ups ===
        {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0},  // 32: pure offbeat 16ths
        {1,0,1,0, 1,0,1,0, 1,0,1,1, 1,0,1,1},  // 33: 8ths building to 16ths
        {1,1,0,1, 1,1,0,1, 1,1,0,1, 1,1,0,1},  // 34: supersaw riff (skip beat 3)
        {0,0,1,0, 1,0,1,0, 0,0,1,0, 1,0,1,0},  // 35: offbeat hook
        {1,0,0,1, 0,1,0,1, 1,0,0,1, 0,1,0,1},  // 36: syncopated anthem
        {1,0,1,0, 0,0,1,1, 1,0,1,0, 0,0,1,1},  // 37: catchy build pattern
        {1,1,1,1, 1,1,1,1, 0,0,0,0, 1,1,1,1},  // 38: build-up with gap
        {0,1,0,1, 0,1,0,1, 0,1,0,1, 1,1,1,1},  // 39: upbeat build to fill

        // === DARK PSY (indices 40-47): glitchy, irregular, dense with random gaps ===
        {1,1,0,1, 1,0,1,1, 0,1,1,0, 1,0,1,1},  // 40: glitchy dense
        {1,0,1,1, 0,1,0,0, 1,1,0,1, 0,0,1,0},  // 41: irregular stutter
        {1,1,1,0, 0,1,1,1, 1,0,0,1, 1,1,0,1},  // 42: chaotic density
        {0,1,0,1, 1,0,1,0, 1,1,0,0, 0,1,1,1},  // 43: random cluster
        {1,0,0,0, 1,1,1,0, 0,0,1,1, 1,0,0,1},  // 44: sparse then dense
        {1,1,1,1, 0,0,0,0, 1,1,1,1, 0,0,0,0},  // 45: block stutter
        {0,1,1,0, 1,0,0,1, 0,1,0,1, 1,0,1,0},  // 46: asymmetric glitch
        {1,0,1,0, 0,1,1,1, 0,0,0,1, 1,1,0,0},  // 47: broken machine

        // === PROGRESSIVE PSY (indices 48-55): minimal, repetitive, slowly evolving ===
        {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0},  // 48: quarter note pulse
        {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0},  // 49: steady 8ths
        {1,0,0,0, 1,0,0,0, 1,0,1,0, 1,0,0,0},  // 50: minimal variation
        {1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1},  // 51: dotted repetition
        {1,0,0,0, 0,0,1,0, 1,0,0,0, 0,0,1,0},  // 52: wide space minimal
        {1,0,1,0, 0,0,0,0, 1,0,1,0, 0,0,0,0},  // 53: two-note motif
        {1,0,0,0, 1,0,0,0, 0,0,1,0, 0,0,0,0},  // 54: evolving sparse
        {1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,1,0},  // 55: slow evolution
    };
    return patterns;
}

// ============================================================
// Interval weights
// ============================================================
const std::vector<std::pair<int, float>>& GoaMelodyGenerator::getIntervalWeights()
{
    static const std::vector<std::pair<int, float>> weights = {
        {0,  0.15f},   // unison
        {1,  0.20f},   // minor 2nd (Phrygian tension)
        {2,  0.12f},   // major 2nd
        {3,  0.15f},   // minor 3rd
        {4,  0.10f},   // major 3rd
        {5,  0.10f},   // perfect 4th
        {7,  0.08f},   // perfect 5th
        {-1, 0.18f},   // descending minor 2nd
        {-2, 0.10f},   // descending major 2nd
        {-3, 0.12f},   // descending minor 3rd
        {-5, 0.06f},   // descending 4th
        {-7, 0.05f},   // descending 5th
        {12, 0.02f},   // octave up
        {-12, 0.02f},  // octave down
    };
    return weights;
}

// ============================================================
// Subgenre-specific interval weights
// ============================================================
const std::vector<std::pair<int, float>>& GoaMelodyGenerator::getSubgenreIntervalWeights(int subgenre)
{
    // 0 = Goa (use default)
    if (subgenre == 0)
        return getIntervalWeights();

    // 1 = Full-On: major intervals, catchy hooks, wider jumps, high unison for driving repetition
    static const std::vector<std::pair<int, float>> fullOnWeights = {
        {0,  0.25f},   // unison (driving repetition)
        {1,  0.05f},   // minor 2nd (less Phrygian tension)
        {2,  0.10f},   // major 2nd
        {3,  0.12f},   // minor 3rd
        {4,  0.18f},   // major 3rd (catchy)
        {5,  0.10f},   // perfect 4th
        {7,  0.15f},   // perfect 5th (dramatic)
        {-1, 0.04f},   // descending minor 2nd
        {-2, 0.08f},   // descending major 2nd
        {-3, 0.10f},   // descending minor 3rd
        {-4, 0.12f},   // descending major 3rd
        {-5, 0.08f},   // descending 4th
        {-7, 0.10f},   // descending 5th (dramatic drop)
        {12, 0.05f},   // octave up
        {-12, 0.05f},  // octave down
    };

    // 2 = Dark Psy: chromatic, tritone, whole-tone, dissonant, more random
    static const std::vector<std::pair<int, float>> darkPsyWeights = {
        {0,  0.08f},   // unison
        {1,  0.22f},   // minor 2nd (chromatic)
        {2,  0.15f},   // whole-tone step
        {3,  0.08f},   // minor 3rd
        {4,  0.05f},   // major 3rd
        {5,  0.06f},   // perfect 4th
        {6,  0.18f},   // tritone (dissonant)
        {7,  0.04f},   // perfect 5th
        {-1, 0.22f},   // descending chromatic
        {-2, 0.14f},   // descending whole-tone
        {-3, 0.06f},   // descending minor 3rd
        {-6, 0.15f},   // descending tritone
        {-5, 0.05f},   // descending 4th
        {-7, 0.03f},   // descending 5th
        {12, 0.03f},   // octave up
        {-12, 0.03f},  // octave down
    };

    // 3 = Progressive Psy: stepwise motion, minimal intervals, very high repetition
    static const std::vector<std::pair<int, float>> progressiveWeights = {
        {0,  0.35f},   // unison (very high repetition)
        {1,  0.12f},   // minor 2nd
        {2,  0.18f},   // major 2nd (stepwise)
        {3,  0.08f},   // minor 3rd
        {4,  0.03f},   // major 3rd
        {5,  0.04f},   // perfect 4th
        {7,  0.02f},   // perfect 5th
        {-1, 0.10f},   // descending minor 2nd
        {-2, 0.16f},   // descending major 2nd (stepwise)
        {-3, 0.06f},   // descending minor 3rd
        {-5, 0.02f},   // descending 4th
        {-7, 0.01f},   // descending 5th
        {12, 0.01f},   // octave up
        {-12, 0.01f},  // octave down
    };

    switch (subgenre) {
        case 1: return fullOnWeights;
        case 2: return darkPsyWeights;
        case 3: return progressiveWeights;
        default: return getIntervalWeights();
    }
}

// ============================================================
// Chord progressions
// ============================================================
std::vector<ChordInfo> GoaMelodyGenerator::getProgression(int progressionIndex, int numBars) const
{
    // Define chord progressions as chord-per-bar sequences
    // Each ChordInfo has rootInterval (semitones from key) and chord tones
    std::vector<std::vector<ChordInfo>> progressions = {
        // Drone: just root throughout
        {{0, {0, 7}}},
        // i - bII
        {{0, {0, 3, 7}}, {1, {0, 4, 7}}},
        // i - bVII
        {{0, {0, 3, 7}}, {10, {0, 4, 7}}},
        // i - bVI
        {{0, {0, 3, 7}}, {8, {0, 4, 7}}},
        // i - iv
        {{0, {0, 3, 7}}, {5, {0, 3, 7}}},
        // i - bII - bVII - i
        {{0, {0, 3, 7}}, {1, {0, 4, 7}}, {10, {0, 4, 7}}, {0, {0, 3, 7}}},
        // Full-On: i - iv - bVI - bVII
        {{0, {0, 3, 7}}, {5, {0, 3, 7}}, {8, {0, 4, 7}}, {10, {0, 4, 7}}},
        // Dark Psy: Chromatic drone (root with chromatic bass movement)
        {{0, {0, 7}}, {1, {0, 7}}, {0, {0, 7}}, {11, {0, 7}}},
        // Progressive Psy: i - bVII - bVI - v
        {{0, {0, 3, 7}}, {10, {0, 4, 7}}, {8, {0, 4, 7}}, {7, {0, 3, 7}}},
    };

    int idx = std::clamp(progressionIndex, 0, (int)progressions.size() - 1);
    auto& prog = progressions[static_cast<size_t>(idx)];

    // Extend progression to fill all bars by cycling
    std::vector<ChordInfo> result;
    for (int bar = 0; bar < numBars; ++bar) {
        result.push_back(prog[static_cast<size_t>(bar % prog.size())]);
    }
    return result;
}

ChordInfo GoaMelodyGenerator::getChordAtBar(const std::vector<ChordInfo>& progression, int bar) const
{
    if (progression.empty()) return {0, {0, 7}};
    return progression[static_cast<size_t>(bar % progression.size())];
}

// ============================================================
// Generate rhythm grid
// ============================================================
std::vector<double> GoaMelodyGenerator::generateRhythm(const GeneratorParams& params)
{
    std::vector<double> onsets;
    const auto& patterns = getGoaRhythmPatterns();

    struct CategoryRange { int start; int end; };

    // Euclidean mode: works with every subgenre, so it is checked before the
    // subgenre pattern ranges. One pattern per call (per motif) - bar-to-bar
    // variation still comes from the dropout/ghost logic below.
    const bool useEuclid =
        (params.patternCategory == (int)PatternCategory::Euclidean);
    std::vector<int> euclid;
    if (useEuclid) {
        int hits = std::clamp((int)std::lround(params.density * 15.0f) + 1, 1, 16);
        euclid = euclideanPattern(hits, 16, randomInt(0, 15));
    }

    int rangeStart = 0, rangeEnd = 0;

    if (useEuclid) {
        // pattern ranges unused
    } else if (params.subgenre == 1) {
        // Full-On: use patterns 32-39
        rangeStart = 32;
        rangeEnd = 39;
    } else if (params.subgenre == 2) {
        // Dark Psy: use patterns 40-47
        rangeStart = 40;
        rangeEnd = 47;
    } else if (params.subgenre == 3) {
        // Progressive Psy: use patterns 48-55
        rangeStart = 48;
        rangeEnd = 55;
    } else {
        // Goa (0): use existing category-based behavior
        const CategoryRange ranges[] = {
            {0, 7}, {8, 19}, {20, 27}, {28, 31},
        };
        int cat = std::clamp(params.patternCategory, 0, 3);
        rangeStart = ranges[cat].start;
        rangeEnd = ranges[cat].end;
    }
    int rangeSize = rangeEnd - rangeStart + 1;

    for (int bar = 0; bar < params.phraseLengthBars; ++bar) {
        const std::vector<int>* pattern = &euclid;
        if (!useEuclid) {
            int offset = (int)(params.density * (float)(rangeSize - 1));
            int jitter = randomInt(-2, 2);
            int patternIdx = std::clamp(rangeStart + offset + jitter, rangeStart, rangeEnd);
            pattern = &patterns[static_cast<size_t>(patternIdx)];
        }

        for (int step = 0; step < 16; ++step) {
            if ((*pattern)[static_cast<size_t>(step)] == 1) {
                if (randomFloat() > params.rhythmVariation * 0.3f) {
                    double beatPos = bar * 4.0 + step * 0.25;
                    onsets.push_back(beatPos);
                }
            } else if (params.rhythmVariation > 0.5f && randomFloat() < 0.05f) {
                double beatPos = bar * 4.0 + step * 0.25;
                onsets.push_back(beatPos);
            }
        }
    }

    return onsets;
}

// ============================================================
// Choose next note - now chord-aware
// ============================================================
int GoaMelodyGenerator::chooseNextNote(int currentNote, const GeneratorParams& params,
                                        const std::vector<int>& scaleNotes,
                                        const ChordInfo& currentChord)
{
    const auto& intervals = getSubgenreIntervalWeights(params.subgenre);

    std::vector<float> weights;
    std::vector<int> candidates;

    // Subgenre-specific chord tone multiplier
    float chordToneMultiplier = 1.8f; // default Goa
    if (params.subgenre == 2)       // Dark Psy: reduced chord emphasis
        chordToneMultiplier = 0.8f;
    else if (params.subgenre == 3)  // Progressive Psy: moderate chord emphasis
        chordToneMultiplier = 1.4f;

    for (const auto& [interval, weight] : intervals) {
        int candidate = currentNote + interval;
        if (candidate < 0 || candidate > 127) continue;

        float adjustedWeight = weight;
        int absInterval = std::abs(interval);

        if (params.pitchRange < 0.3f && absInterval > 4)
            adjustedWeight *= 0.2f;
        else if (params.pitchRange > 0.7f && absInterval <= 2)
            adjustedWeight *= 0.6f;

        int centerNote = params.rootNote + params.baseOctave * 12;
        int distFromCenter = std::abs(candidate - centerNote);
        if (distFromCenter > 12) adjustedWeight *= 0.5f;
        if (distFromCenter > 18) adjustedWeight *= 0.3f;

        // Progressive Psy: extra unison/repeat emphasis
        if (params.subgenre == 3 && interval == 0)
            adjustedWeight *= 1.5f;

        // Chord tone emphasis: boost weight if candidate is a chord tone
        int noteRelChord = ((candidate - params.rootNote - currentChord.rootInterval) % 12 + 12) % 12;
        for (int ct : currentChord.tones) {
            if (noteRelChord == ct) {
                adjustedWeight *= chordToneMultiplier;
                break;
            }
        }

        candidates.push_back(candidate);
        weights.push_back(adjustedWeight);
    }

    if (candidates.empty())
        return currentNote;

    int chosen = candidates[static_cast<size_t>(weightedChoice(weights))];

    const auto& scale = GOA_SCALES[static_cast<size_t>(params.scaleIndex)];
    return quantizeToScale(chosen, params.rootNote, scale);
}

// ============================================================
// Apply 303-style articulation
// ============================================================
void GoaMelodyGenerator::applyAcidArticulation(std::vector<NoteEvent>& events,
                                                 const GeneratorParams& params)
{
    // Subgenre-specific slide amount multiplier
    float slideMultiplier = 0.35f;  // default Goa
    if (params.subgenre == 2)       // Dark Psy: more slides
        slideMultiplier = 0.5f;
    else if (params.subgenre == 3)  // Progressive Psy: fewer slides
        slideMultiplier = 0.2f;
    // Full-On (1): standard acid behavior (0.35f)

    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].isGraceNote) continue;

        if (randomFloat() < params.acidAmount * 0.4f) {
            events[i].accent = true;
            // Accent pushes velocity to 0.9-1.0 range
            events[i].velocity = 0.9f + randomFloat(0.0f, 0.1f);
        }

        // Non-accented notes with high acid get slightly reduced for contrast
        if (!events[i].accent && params.acidAmount > 0.5f) {
            events[i].velocity *= (1.0f - params.acidAmount * 0.15f);
        }

        if (i + 1 < events.size() && randomFloat() < params.acidAmount * slideMultiplier) {
            events[i].slide = true;
            events[i].duration = events[i + 1].startBeat - events[i].startBeat + 0.05;
            // Slides are slightly softer
            if (!events[i].accent)
                events[i].velocity *= 0.85f;
        }

        // Dark Psy: random pitch bend jitter on slides
        if (params.subgenre == 2 && events[i].slide) {
            events[i].pitchBend += randomInt(-1500, 1500);
            events[i].pitchBend = std::clamp(events[i].pitchBend, -8192, 8191);
        }

        events[i].velocity = std::clamp(events[i].velocity, 0.15f, 1.0f);
    }
}

// ============================================================
// Apply Goa-style ornaments
// ============================================================
void GoaMelodyGenerator::applyOrnaments(std::vector<NoteEvent>& events,
                                          const GeneratorParams& params,
                                          const std::vector<int>& /*scaleNotes*/)
{
    // Subgenre-specific ornament amount
    float effectiveOrnamentAmount = params.ornamentAmount;
    if (params.subgenre == 3)       // Progressive Psy: minimal ornaments
        effectiveOrnamentAmount *= 0.3f;

    std::vector<NoteEvent> ornaments;

    // Grace notes (separate from ornaments)
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].isGraceNote) continue;
        if (randomFloat() > params.graceAmount) continue;

        NoteEvent grace;
        int direction = randomFloat() < 0.5f ? 1 : -1;
        grace.noteNumber = quantizeToScale(
            events[i].noteNumber + direction * randomInt(1, 3),
            params.rootNote, GOA_SCALES[static_cast<size_t>(params.scaleIndex)]);
        grace.velocity = events[i].velocity * 0.35f;
        grace.pan = 0.0f;
        grace.startBeat = events[i].startBeat - 0.0625;
        grace.duration = 0.04;
        grace.pitchBend = 0;
        grace.accent = false;
        grace.slide = true;
        grace.isGraceNote = true;
        if (grace.startBeat >= 0.0)
            ornaments.push_back(grace);
    }

    // Ornaments (trills, pitch bends)
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].isGraceNote) continue;
        if (randomFloat() > effectiveOrnamentAmount) continue;

        float ornamentType = randomFloat();

        if (ornamentType < 0.5f) {
            events[i].pitchBend = -4096;
        }
        else {
            if (events[i].duration >= 0.5) {
                double trillNoteLen = 0.0625;
                int trillNote = quantizeToScale(
                    events[i].noteNumber + randomInt(1, 2),
                    params.rootNote, GOA_SCALES[static_cast<size_t>(params.scaleIndex)]);

                int numTrills = std::min(4, (int)(events[i].duration / (trillNoteLen * 2)));
                double origDuration = events[i].duration;
                events[i].duration = origDuration - numTrills * trillNoteLen * 2;

                for (int t = 0; t < numTrills; ++t) {
                    NoteEvent trill;
                    trill.noteNumber = trillNote;
                    trill.velocity = events[i].velocity * 0.8f;
                    trill.pan = 0.0f;
                    trill.startBeat = events[i].startBeat + events[i].duration
                                      + t * trillNoteLen * 2 + trillNoteLen;
                    trill.duration = trillNoteLen;
                    trill.pitchBend = 0;
                    trill.accent = false;
                    trill.slide = false;
                    trill.isGraceNote = false;
                    ornaments.push_back(trill);
                }
            }
        }

        // Dark Psy: add random pitch bend jitter to ornamented notes
        if (params.subgenre == 2) {
            events[i].pitchBend += randomInt(-2000, 2000);
            events[i].pitchBend = std::clamp(events[i].pitchBend, -8192, 8191);
        }
    }

    events.insert(events.end(), ornaments.begin(), ornaments.end());
    std::sort(events.begin(), events.end(),
              [](const NoteEvent& a, const NoteEvent& b) {
                  return a.startBeat < b.startBeat;
              });
}

// ============================================================
// Generate a short motif (building block)
// ============================================================
std::vector<NoteEvent> GoaMelodyGenerator::generateMotif(
    const GeneratorParams& params,
    const std::vector<int>& scaleNotes,
    const ChordInfo& chord,
    int numBeats)
{
    // Generate rhythm for motif length
    GeneratorParams motifParams = params;
    motifParams.phraseLengthBars = std::max(1, numBeats / 4);

    auto onsets = generateRhythm(motifParams);
    // Filter to only the requested beats
    double maxBeat = (double)numBeats;
    onsets.erase(std::remove_if(onsets.begin(), onsets.end(),
                 [maxBeat](double b) { return b >= maxBeat; }), onsets.end());

    if (onsets.empty()) return {};

    std::vector<NoteEvent> events;

    // Start on a chord tone
    int startNote = params.rootNote + params.baseOctave * 12 + chord.rootInterval;
    if (!chord.tones.empty() && randomFloat() < 0.5f) {
        int toneIdx = randomInt(0, (int)chord.tones.size() - 1);
        startNote = params.rootNote + params.baseOctave * 12 + chord.rootInterval
                    + chord.tones[static_cast<size_t>(toneIdx)];
    }
    startNote = quantizeToScale(startNote, params.rootNote,
                                 GOA_SCALES[static_cast<size_t>(params.scaleIndex)]);
    int currentNote = startNote;

    for (size_t i = 0; i < onsets.size(); ++i) {
        NoteEvent event;
        event.noteNumber = currentNote;
        event.pan = 0.0f;
        event.startBeat = onsets[i];
        event.pitchBend = 0;
        event.accent = false;
        event.slide = false;
        event.isGraceNote = false;

        // Musical velocity based on beat position
        double beatInBar = std::fmod(onsets[i], 4.0);
        double step16th = std::fmod(onsets[i], 0.25);
        bool isDownbeat = std::abs(beatInBar) < 0.01;
        bool isBeat3 = std::abs(beatInBar - 2.0) < 0.01;
        bool isOnBeat = std::abs(std::fmod(beatInBar, 1.0)) < 0.01;
        bool isUpbeat8th = std::abs(std::fmod(beatInBar, 0.5) - 0.25) < 0.01;

        if (isDownbeat) {
            // Beat 1: strongest
            event.velocity = 0.85f + randomFloat(0.0f, 0.15f);
        } else if (isBeat3) {
            // Beat 3: second strongest
            event.velocity = 0.75f + randomFloat(0.0f, 0.15f);
        } else if (isOnBeat) {
            // Beat 2, 4: moderate
            event.velocity = 0.65f + randomFloat(0.0f, 0.15f);
        } else if (isUpbeat8th) {
            // Upbeat 8ths: lighter
            event.velocity = 0.55f + randomFloat(0.0f, 0.15f);
        } else {
            // 16th note subdivisions: lightest
            event.velocity = 0.4f + randomFloat(0.0f, 0.2f);
        }

        // Humanize: slight random variation
        event.velocity = std::clamp(event.velocity + randomFloat(-0.05f, 0.05f), 0.2f, 1.0f);

        if (i + 1 < onsets.size()) {
            double gap = onsets[i + 1] - onsets[i];
            event.duration = gap * (0.7f + randomFloat(0.0f, 0.25f));
        } else {
            event.duration = 0.25;
        }

        events.push_back(event);
        currentNote = chooseNextNote(currentNote, params, scaleNotes, chord);
    }

    return events;
}

// ============================================================
// Develop (vary) a motif - creates variations while preserving shape
// ============================================================
std::vector<NoteEvent> GoaMelodyGenerator::developMotif(
    const std::vector<NoteEvent>& motif,
    const GeneratorParams& params,
    const ChordInfo& chord,
    double offsetBeats,
    float developAmount)
{
    auto events = motif;
    const auto& scale = GOA_SCALES[static_cast<size_t>(params.scaleIndex)];

    for (auto& event : events) {
        // Shift to new position
        event.startBeat += offsetBeats;

        if (randomFloat() < developAmount) {
            // Transpose to fit new chord - find nearest chord tone
            int noteRelRoot = ((event.noteNumber - params.rootNote) % 12 + 12) % 12;
            int noteRelChord = ((noteRelRoot - chord.rootInterval) % 12 + 12) % 12;

            // If not a chord tone, nudge toward one
            bool isChordTone = false;
            for (int ct : chord.tones) {
                if (noteRelChord == ct) { isChordTone = true; break; }
            }
            if (!isChordTone && !chord.tones.empty()) {
                // Move to nearest chord tone
                int bestTone = chord.tones[0];
                int bestDist = 12;
                for (int ct : chord.tones) {
                    int dist = std::min(std::abs(noteRelChord - ct),
                                        12 - std::abs(noteRelChord - ct));
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestTone = ct;
                    }
                }
                int newNote = event.noteNumber + (bestTone - noteRelChord);
                event.noteNumber = quantizeToScale(newNote, params.rootNote, scale);
            }
        }

        // Small random variation
        if (randomFloat() < developAmount * 0.5f) {
            int direction = randomFloat() < 0.5f ? 1 : -1;
            event.noteNumber = quantizeToScale(
                event.noteNumber + direction * randomInt(1, 2),
                params.rootNote, scale);
        }

        // Velocity variation
        if (randomFloat() < developAmount * 0.3f) {
            event.velocity = std::clamp(
                event.velocity + randomFloat(-0.1f, 0.1f), 0.3f, 1.0f);
        }
    }

    return events;
}

// ============================================================
// Phrase resolution - end phrases on strong notes
// ============================================================
void GoaMelodyGenerator::applyPhraseResolution(std::vector<NoteEvent>& events,
                                                 const GeneratorParams& params,
                                                 int phraseEndBar)
{
    if (events.empty()) return;

    double phraseEnd = phraseEndBar * 4.0;
    int rootNote = params.rootNote + params.baseOctave * 12;
    int fifthNote = quantizeToScale(rootNote + 7, params.rootNote,
                                     GOA_SCALES[static_cast<size_t>(params.scaleIndex)]);

    // Find the last few notes in the phrase and guide them toward resolution
    for (int i = (int)events.size() - 1; i >= 0; --i) {
        auto& event = events[static_cast<size_t>(i)];
        if (event.isGraceNote) continue;

        double beatsFromEnd = phraseEnd - event.startBeat;
        if (beatsFromEnd > 4.0) break; // only affect last bar

        if (beatsFromEnd <= 0.5) {
            // Very last note: land on root
            event.noteNumber = rootNote;
            event.velocity = std::min(1.0f, event.velocity + 0.1f);
        } else if (beatsFromEnd <= 1.0) {
            // Second to last: approach from 5th or 2nd
            if (randomFloat() < 0.6f)
                event.noteNumber = fifthNote;
            else
                event.noteNumber = quantizeToScale(rootNote + 2, params.rootNote,
                                                    GOA_SCALES[static_cast<size_t>(params.scaleIndex)]);
        } else if (beatsFromEnd <= 2.0) {
            // Build tension - approach tone
            int approachNote = quantizeToScale(rootNote + 1, params.rootNote,
                                               GOA_SCALES[static_cast<size_t>(params.scaleIndex)]);
            if (randomFloat() < 0.4f)
                event.noteNumber = approachNote;
        }
    }
}

// ============================================================
// Strong beat emphasis - place important notes on beats 1 and 3
// ============================================================
void GoaMelodyGenerator::applyStrongBeatEmphasis(std::vector<NoteEvent>& events,
                                                   const GeneratorParams& params)
{
    int rootNote = params.rootNote + params.baseOctave * 12;
    const auto& scale = GOA_SCALES[static_cast<size_t>(params.scaleIndex)];

    // Apply crescendo/decrescendo across phrases for dynamics
    if (events.empty()) return;
    double totalBeats = params.phraseLengthBars * 4.0;

    for (auto& event : events) {
        if (event.isGraceNote) continue;

        double beatInBar = std::fmod(event.startBeat, 4.0);

        // Phrase-level dynamics: build toward 75% then resolve
        double phrasePos = event.startBeat / totalBeats;
        float phraseDynamic = 0.0f;
        if (phrasePos < 0.75)
            phraseDynamic = (float)(phrasePos / 0.75) * 0.1f;  // build up
        else
            phraseDynamic = 0.1f - (float)((phrasePos - 0.75) / 0.25) * 0.05f;  // settle

        event.velocity = std::clamp(event.velocity + phraseDynamic, 0.2f, 1.0f);

        // Beat 1: sometimes force root/5th on downbeat
        if (std::abs(beatInBar) < 0.01) {
            if (randomFloat() < 0.3f) {
                if (randomFloat() < 0.6f)
                    event.noteNumber = quantizeToScale(rootNote, params.rootNote, scale);
                else
                    event.noteNumber = quantizeToScale(rootNote + 7, params.rootNote, scale);
            }
        }
    }
}

// ============================================================
// Call and Response structure
// ============================================================
void GoaMelodyGenerator::applyCallAndResponse(std::vector<NoteEvent>& events,
                                                const GeneratorParams& params)
{
    if (events.empty()) return;

    double totalBeats = params.phraseLengthBars * 4.0;
    double halfPhrase = totalBeats / 2.0;

    // Collect "call" notes (first half)
    std::vector<NoteEvent> callNotes;
    for (const auto& e : events) {
        if (e.startBeat < halfPhrase && !e.isGraceNote)
            callNotes.push_back(e);
    }

    if (callNotes.empty()) return;

    // Modify "response" (second half) to mirror the call with variation
    const auto& scale = GOA_SCALES[static_cast<size_t>(params.scaleIndex)];
    int rootNote = params.rootNote + params.baseOctave * 12;

    for (auto& event : events) {
        if (event.startBeat < halfPhrase || event.isGraceNote) continue;

        // Find corresponding call note
        double responsePos = event.startBeat - halfPhrase;
        const NoteEvent* matchedCall = nullptr;
        double bestDist = 999.0;

        for (const auto& call : callNotes) {
            double dist = std::abs(call.startBeat - responsePos);
            if (dist < bestDist) {
                bestDist = dist;
                matchedCall = &call;
            }
        }

        if (matchedCall != nullptr && bestDist < 1.0) {
            if (randomFloat() < 0.5f) {
                // Mirror: invert the interval from root
                int callInterval = matchedCall->noteNumber - rootNote;
                int responseNote = rootNote - callInterval;
                event.noteNumber = quantizeToScale(responseNote, params.rootNote, scale);
            } else {
                // Echo: same rhythm, transposed
                int transposition = randomFloat() < 0.5f ? 3 : -3;
                event.noteNumber = quantizeToScale(
                    matchedCall->noteNumber + transposition, params.rootNote, scale);
            }
        }
    }
}

// ============================================================
// Main phrase generation - now with motif development + structure
// ============================================================
std::vector<NoteEvent> GoaMelodyGenerator::generatePhrase(const GeneratorParams& params)
{
    const auto& scale = GOA_SCALES[static_cast<size_t>(params.scaleIndex)];
    auto scaleNotes = getScaleNotes(params.rootNote, scale,
                                     params.baseOctave - 1, params.baseOctave + 1);

    // Get chord progression
    auto progression = getProgression(params.progression, params.phraseLengthBars);

    std::vector<NoteEvent> events;

    // Determine motif length based on subgenre
    int motifBars;
    if (params.subgenre == 3) {
        // Progressive Psy: longer motifs (8 bars or phrase length)
        motifBars = std::min(8, params.phraseLengthBars);
    } else if (params.subgenre == 2) {
        // Dark Psy: shorter motifs (1-2 bars)
        motifBars = std::min(2, params.phraseLengthBars);
        if (params.phraseLengthBars >= 4) motifBars = 1 + randomInt(0, 1);
    } else {
        // Goa / Full-On: standard (2 bars for short phrases, 2-4 for longer)
        motifBars = params.phraseLengthBars <= 4 ? 2 : std::min(4, params.phraseLengthBars / 2);
    }
    int motifBeats = motifBars * 4;

    // Subgenre-specific development multiplier
    float developMultiplier = 1.0f;
    if (params.subgenre == 3)       // Progressive Psy: less development
        developMultiplier = 0.5f;
    else if (params.subgenre == 2)  // Dark Psy: more development
        developMultiplier = 1.5f;

    // Generate the core motif using the first chord
    auto motif = generateMotif(params, scaleNotes,
                                getChordAtBar(progression, 0), motifBeats);

    // Build the phrase by developing the motif across bars
    int numSections = (params.phraseLengthBars + motifBars - 1) / motifBars;

    for (int section = 0; section < numSections; ++section) {
        int startBar = section * motifBars;
        if (startBar >= params.phraseLengthBars) break;

        double offsetBeats = startBar * 4.0;
        auto chord = getChordAtBar(progression, startBar);

        if (section == 0) {
            // First section: use original motif
            for (auto note : motif) {
                note.startBeat += offsetBeats;
                events.push_back(note);
            }
        } else {
            // Subsequent sections: develop the motif
            float developAmount = 0.2f + (float)section * 0.1f;
            developAmount = std::min(developAmount, 0.6f);
            developAmount *= developMultiplier;
            developAmount = std::min(developAmount, 0.9f);

            auto developed = developMotif(motif, params, chord, offsetBeats, developAmount);

            // Trim to remaining bars
            double maxBeat = (double)params.phraseLengthBars * 4.0;
            for (const auto& note : developed) {
                if (note.startBeat < maxBeat)
                    events.push_back(note);
            }
        }
    }

    // Full-On: apply velocity crescendo in last 2 bars for build-up effect
    if (params.subgenre == 1 && params.phraseLengthBars >= 4) {
        double buildStart = (params.phraseLengthBars - 2) * 4.0;
        double phraseEnd = params.phraseLengthBars * 4.0;
        for (auto& event : events) {
            if (event.isGraceNote) continue;
            if (event.startBeat >= buildStart) {
                float buildProgress = (float)((event.startBeat - buildStart) / (phraseEnd - buildStart));
                event.velocity = std::clamp(event.velocity + buildProgress * 0.25f, 0.2f, 1.0f);
            }
        }
    }

    // Apply structural elements
    applyStrongBeatEmphasis(events, params);
    applyCallAndResponse(events, params);
    applyPhraseResolution(events, params, params.phraseLengthBars);

    // Apply articulation and ornaments
    applyAcidArticulation(events, params);
    applyOrnaments(events, params, scaleNotes);

    // Final sort
    std::sort(events.begin(), events.end(),
              [](const NoteEvent& a, const NoteEvent& b) {
                  return a.startBeat < b.startBeat;
              });

    return events;
}

// ============================================================
// Generate variation of existing phrase
// ============================================================
std::vector<NoteEvent> GoaMelodyGenerator::generateVariation(
    const std::vector<NoteEvent>& original,
    const GeneratorParams& params,
    float variationAmount)
{
    auto events = original;
    const auto& scale = GOA_SCALES[static_cast<size_t>(params.scaleIndex)];
    auto progression = getProgression(params.progression, params.phraseLengthBars);

    for (auto& event : events) {
        if (event.isGraceNote) continue;

        int bar = (int)(event.startBeat / 4.0);
        auto chord = getChordAtBar(progression, bar);

        if (randomFloat() < variationAmount) {
            int direction = randomFloat() < 0.5f ? 1 : -1;
            int step = randomInt(1, 3);
            int candidate = event.noteNumber + direction * step;

            // Prefer chord tones when varying
            int noteRelChord = ((candidate - params.rootNote - chord.rootInterval) % 12 + 12) % 12;
            bool isChordTone = false;
            for (int ct : chord.tones) {
                if (noteRelChord == ct) { isChordTone = true; break; }
            }
            if (!isChordTone && randomFloat() < 0.4f) {
                // Try to land on a chord tone instead
                if (!chord.tones.empty()) {
                    int toneIdx = randomInt(0, (int)chord.tones.size() - 1);
                    int octave = (event.noteNumber / 12) * 12;
                    candidate = octave + params.rootNote % 12 + chord.rootInterval
                                + chord.tones[static_cast<size_t>(toneIdx)];
                }
            }

            event.noteNumber = quantizeToScale(candidate, params.rootNote, scale);
        }

        if (randomFloat() < variationAmount * 0.5f) {
            event.startBeat += randomFloat(-0.0625f, 0.0625f);
            if (event.startBeat < 0) event.startBeat = 0;
        }

        if (randomFloat() < variationAmount * 0.3f) {
            event.velocity = std::clamp(
                event.velocity + randomFloat(-0.15f, 0.15f), 0.1f, 1.0f);
        }
    }

    // Re-apply resolution
    applyPhraseResolution(events, params, params.phraseLengthBars);

    return events;
}

// ============================================================
// Utility methods
// ============================================================
int GoaMelodyGenerator::weightedChoice(const std::vector<float>& weights)
{
    float sum = std::accumulate(weights.begin(), weights.end(), 0.0f);
    float r = randomFloat(0.0f, sum);
    float cumulative = 0.0f;
    for (size_t i = 0; i < weights.size(); ++i) {
        cumulative += weights[i];
        if (r <= cumulative) return static_cast<int>(i);
    }
    return static_cast<int>(weights.size() - 1);
}

float GoaMelodyGenerator::randomFloat(float min, float max)
{
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

int GoaMelodyGenerator::randomInt(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

// ============================================================
// Euclidean (Bjorklund) pattern: k hits spread evenly over n steps
// ============================================================
std::vector<int> GoaMelodyGenerator::euclideanPattern(int hits, int steps, int rotation)
{
    std::vector<int> pattern(static_cast<size_t>(steps), 0);
    int bucket = 0;
    for (int i = 0; i < steps; ++i) {
        bucket += hits;
        if (bucket >= steps) {
            bucket -= steps;
            pattern[static_cast<size_t>((i + rotation) % steps)] = 1;
        }
    }
    return pattern;
}

// ============================================================
// Humanize / swing groove, shared by all generation modes
// ============================================================
void GoaMelodyGenerator::applyGroove(std::vector<NoteEvent>& events,
                                      float humanize, float swing)
{
    if (events.empty() || (humanize <= 0.0f && swing <= 0.0f))
        return;

    humanize = std::clamp(humanize, 0.0f, 1.0f);
    swing = std::clamp(swing, 0.0f, 1.0f);

    // One offset per 16th slot: keeps chords together, grace notes attached
    // to their parents, and the odd/even swing decision on the clean grid
    std::map<long long, double> slotOffsets;
    auto offsetForSlot = [&](long long slot) {
        auto it = slotOffsets.find(slot);
        if (it != slotOffsets.end())
            return it->second;
        double off = 0.0;
        if ((slot % 2) != 0)
            off += swing * (1.0 / 12.0); // full swing = triplet position
        if (humanize > 0.0f)
            off += (double)randomFloat(-1.0f, 1.0f) * humanize * 0.03;
        slotOffsets[slot] = off;
        return off;
    };

    for (auto& e : events) {
        long long slot = (long long)std::llround(e.startBeat / 0.25);
        e.startBeat = std::max(0.0, e.startBeat + offsetForSlot(slot));
        if (humanize > 0.0f && !e.isGraceNote)
            e.velocity = std::clamp(
                e.velocity * (1.0f + randomFloat(-1.0f, 1.0f) * humanize * 0.15f),
                0.05f, 1.0f);
    }

    // Max relative displacement between adjacent slots is < 0.25 beats, so no
    // reordering is actually possible - sorted as a guarantee for consumers
    std::sort(events.begin(), events.end(),
              [](const NoteEvent& a, const NoteEvent& b) {
                  return a.startBeat < b.startBeat;
              });

    // Slide durations were computed against pre-groove neighbour positions
    // (applyAcidArticulation / applyBassAcid); recompute so legato survives.
    // Grace notes carry slide=true but must keep their short fixed length.
    for (size_t i = 0; i < events.size(); ++i) {
        if (!events[i].slide || events[i].isGraceNote)
            continue;
        for (size_t j = i + 1; j < events.size(); ++j) {
            if (events[j].isGraceNote)
                continue;
            if (events[j].startBeat > events[i].startBeat + 1.0e-6) {
                events[i].duration = events[j].startBeat - events[i].startBeat + 0.05;
                break;
            }
        }
    }
}

// ============================================================
// Partial regeneration
// ============================================================
void GoaMelodyGenerator::rederiveGracePitches(std::vector<NoteEvent>& events,
                                               const GeneratorParams& params)
{
    const auto& scale = GOA_SCALES[static_cast<size_t>(params.scaleIndex)];
    for (size_t i = 0; i < events.size(); ++i) {
        if (!events[i].isGraceNote)
            continue;
        // Parent = first main note at or after the grace; fall back to the
        // last main note before it
        const NoteEvent* parent = nullptr;
        for (size_t j = 0; j < events.size(); ++j) {
            if (events[j].isGraceNote)
                continue;
            if (events[j].startBeat >= events[i].startBeat - 1.0e-6) {
                parent = &events[j];
                break;
            }
            parent = &events[j];
        }
        if (parent == nullptr)
            continue;
        int direction = randomFloat() < 0.5f ? 1 : -1;
        events[i].noteNumber = quantizeToScale(
            parent->noteNumber + direction * randomInt(1, 3),
            params.rootNote, scale);
    }
}

std::vector<NoteEvent> GoaMelodyGenerator::regeneratePitchesOnly(
    const std::vector<NoteEvent>& original, const GeneratorParams& params)
{
    auto events = original;
    std::sort(events.begin(), events.end(),
              [](const NoteEvent& a, const NoteEvent& b) {
                  return a.startBeat < b.startBeat;
              });

    const auto& scale = GOA_SCALES[static_cast<size_t>(params.scaleIndex)];
    auto scaleNotes = getScaleNotes(params.rootNote, scale,
                                     params.baseOctave - 1, params.baseOctave + 1);
    auto progression = getProgression(params.progression, params.phraseLengthBars);

    // Same start-note policy as generateMotif
    auto chord0 = getChordAtBar(progression, 0);
    int currentNote = params.rootNote + params.baseOctave * 12 + chord0.rootInterval;
    if (!chord0.tones.empty() && randomFloat() < 0.5f)
        currentNote += chord0.tones[static_cast<size_t>(
            randomInt(0, (int)chord0.tones.size() - 1))];
    currentNote = quantizeToScale(currentNote, params.rootNote, scale);

    // Walk the existing onsets, re-rolling only the pitch. The chord context
    // follows the actual bar of each event.
    for (auto& e : events) {
        if (e.isGraceNote)
            continue;
        auto chord = getChordAtBar(progression, std::max(0, (int)(e.startBeat / 4.0)));
        e.noteNumber = currentNote;
        currentNote = chooseNextNote(currentNote, params, scaleNotes, chord);
    }

    rederiveGracePitches(events, params);
    // Pitch-only mutations: restore call/response structure and the ending
    // (applyStrongBeatEmphasis is skipped - it would alter velocities)
    applyCallAndResponse(events, params);
    applyPhraseResolution(events, params, params.phraseLengthBars);
    return events;
}

std::vector<NoteEvent> GoaMelodyGenerator::regenerateRhythmOnly(
    const std::vector<NoteEvent>& original, const GeneratorParams& params)
{
    // Old melody as a (time, pitch) step function
    std::vector<std::pair<double, int>> oldPitches;
    for (const auto& e : original)
        if (!e.isGraceNote)
            oldPitches.push_back({ e.startBeat, e.noteNumber });
    std::sort(oldPitches.begin(), oldPitches.end());

    auto fresh = generatePhrase(params);
    if (oldPitches.empty())
        return fresh;

    // Time-based lookup keeps "which pitch sounds when"; denser new rhythms
    // re-strike the held pitch. Trill-length notes keep their own pitches so
    // ornaments stay ornamental.
    for (auto& e : fresh) {
        if (e.isGraceNote || e.duration < 0.1)
            continue;
        int pitch = oldPitches.front().second;
        for (const auto& [start, p] : oldPitches) {
            if (start <= e.startBeat + 1.0e-6)
                pitch = p;
            else
                break;
        }
        e.noteNumber = pitch;
    }

    rederiveGracePitches(fresh, params);
    // No applyPhraseResolution: the mapped ending comes from the old phrase
    return fresh;
}

} // namespace PsyMelody
