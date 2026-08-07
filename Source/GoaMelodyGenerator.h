#pragma once
#include <juce_core/juce_core.h>
#include "ScaleDefinitions.h"
#include <random>
#include <vector>

namespace PsyMelody {

// A single note event with timing and articulation
struct NoteEvent {
    int noteNumber;       // MIDI note 0-127
    float velocity;       // 0.0 - 1.0
    float pan;            // -1.0 (L) to 1.0 (R), 0.0 = center
    double startBeat;     // position in beats from phrase start
    double duration;       // length in beats
    int pitchBend;        // -8192 to 8191 (for slides/bends)
    bool accent;          // TB-303 style accent
    bool slide;           // TB-303 style slide to next note
    bool isGraceNote;     // ornamental grace note
};

// Pattern category for explicit selection
enum class PatternCategory {
    AcidArp = 0,     // Dense 303/arpeggio patterns
    LeadMelody,      // Mid-density melodic hooks
    SlowMelody,      // Sparse, deliberate melodies
    Ambient,         // Very sparse, atmospheric
    Euclidean,       // Mathematically even pulse (works with every subgenre)
    NumCategories
};

inline const std::vector<std::string> PATTERN_CATEGORY_NAMES = {
    "Acid Arp", "Lead Melody", "Slow Melody", "Ambient", "Euclidean"
};

// Parameters controlling the generation
struct GeneratorParams {
    int rootNote = 0;              // 0=C, 1=C#, ... 11=B
    int scaleIndex = 0;            // index into GOA_SCALES
    float bpm = 145.0f;
    int phraseLengthBars = 4;      // 1-16 bars
    int baseOctave = 4;            // center octave for melody
    int patternCategory = 1;       // PatternCategory index (default: LeadMelody)
    int progression = 0;           // GoaProgression index (default: Drone)
    int subgenre = 0;              // 0=Goa, 1=Full-On, 2=Dark Psy, 3=Progressive Psy
    float density = 0.6f;          // 0.0 sparse - 1.0 dense (within category)
    float acidAmount = 0.5f;       // 0.0 - 1.0 (303-style slides/accents)
    float ornamentAmount = 0.3f;   // 0.0 - 1.0 (trills, bends)
    float graceAmount = 0.0f;      // 0.0 - 1.0 (grace notes) default OFF
    float rhythmVariation = 0.4f;  // 0.0 straight - 1.0 syncopated
    float pitchRange = 0.5f;       // 0.0 narrow - 1.0 wide interval range
    // New fields must be appended here with defaults: Presets.h initializes
    // this struct positionally, so inserting in the middle silently shifts
    // every factory preset
    float humanize = 0.0f;         // 0.0 - 1.0 timing/velocity jitter
    float swing = 0.0f;            // 0.0 straight - 1.0 triplet shuffle
};

inline const std::vector<std::string> SUBGENRE_NAMES = {
    "Goa", "Full-On", "Dark Psy", "Progressive Psy"
};

// Goa trance chord progression types
enum class GoaProgression {
    Drone = 0,           // Single root drone (classic Goa)
    i_bII,               // i - bII (most iconic Goa progression)
    i_bVII,              // i - bVII
    i_bVI,               // i - bVI
    i_iv,                // i - iv
    i_bII_bVII_i,        // Extended 4-chord Goa progression
    i_iv_bVI_bVII,       // Full-On: i - iv - bVI - bVII
    ChromaticDrone,      // Dark Psy: root drone with chromatic bass
    i_bVII_bVI_v,        // Progressive Psy: i - bVII - bVI - v
    NumProgressions
};

inline const std::vector<std::string> GOA_PROGRESSION_NAMES = {
    "Drone", "i - bII", "i - bVII", "i - bVI", "i - iv", "i - bII - bVII - i",
    "i - iv - bVI - bVII", "Chromatic Drone", "i - bVII - bVI - v"
};

// Chord defined by root interval from key root
struct ChordInfo {
    int rootInterval;        // semitones from key root
    std::vector<int> tones;  // chord tones as intervals from chord root
};

class GoaMelodyGenerator {
public:
    GoaMelodyGenerator();

    // Generate a new phrase with current parameters
    std::vector<NoteEvent> generatePhrase(const GeneratorParams& params);

    // Generate a variation of an existing phrase
    std::vector<NoteEvent> generateVariation(const std::vector<NoteEvent>& original,
                                              const GeneratorParams& params,
                                              float variationAmount = 0.3f);

    // Partial regeneration: keep one dimension of the phrase, re-roll the other
    std::vector<NoteEvent> regeneratePitchesOnly(const std::vector<NoteEvent>& original,
                                                 const GeneratorParams& params);
    std::vector<NoteEvent> regenerateRhythmOnly(const std::vector<NoteEvent>& original,
                                                const GeneratorParams& params);

    // Humanize/swing post-processing; shared by all generation modes
    void applyGroove(std::vector<NoteEvent>& events, float humanize, float swing);

    void setSeed(unsigned int seed);

private:
    std::mt19937 rng;

    // Core generation methods
    std::vector<double> generateRhythm(const GeneratorParams& params);
    int chooseNextNote(int currentNote, const GeneratorParams& params,
                       const std::vector<int>& scaleNotes,
                       const ChordInfo& currentChord);
    void applyAcidArticulation(std::vector<NoteEvent>& events,
                                const GeneratorParams& params);
    void applyOrnaments(std::vector<NoteEvent>& events,
                        const GeneratorParams& params,
                        const std::vector<int>& scaleNotes);

    // Phrase structure methods
    std::vector<NoteEvent> generateMotif(const GeneratorParams& params,
                                          const std::vector<int>& scaleNotes,
                                          const ChordInfo& chord,
                                          int numBeats);
    std::vector<NoteEvent> developMotif(const std::vector<NoteEvent>& motif,
                                         const GeneratorParams& params,
                                         const ChordInfo& chord,
                                         double offsetBeats,
                                         float developAmount);
    void applyPhraseResolution(std::vector<NoteEvent>& events,
                                const GeneratorParams& params,
                                int phraseEndBar);
    void applyCallAndResponse(std::vector<NoteEvent>& events,
                               const GeneratorParams& params);
    void applyStrongBeatEmphasis(std::vector<NoteEvent>& events,
                                  const GeneratorParams& params);

    // Re-derive grace note pitches from their (possibly re-rolled) parent notes
    void rederiveGracePitches(std::vector<NoteEvent>& events,
                              const GeneratorParams& params);

    // Chord progression
    std::vector<ChordInfo> getProgression(int progressionIndex, int numBars) const;
    ChordInfo getChordAtBar(const std::vector<ChordInfo>& progression, int bar) const;

    // Weighted random helpers
    int weightedChoice(const std::vector<float>& weights);
    float randomFloat(float min = 0.0f, float max = 1.0f);
    int randomInt(int min, int max);

    // Goa-specific rhythm patterns (16th note grid, 1 = hit, 0 = rest)
    static const std::vector<std::vector<int>>& getGoaRhythmPatterns();

    // Bjorklund/Euclidean pattern: distributes `hits` evenly over `steps`
    static std::vector<int> euclideanPattern(int hits, int steps, int rotation);

    // Interval transition weights for Goa melodies
    static const std::vector<std::pair<int, float>>& getIntervalWeights();

    // Subgenre-specific interval weights
    static const std::vector<std::pair<int, float>>& getSubgenreIntervalWeights(int subgenre);
};

} // namespace PsyMelody
