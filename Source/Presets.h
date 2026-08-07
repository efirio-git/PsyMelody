#pragma once
#include "GoaMelodyGenerator.h"
#include <vector>
#include <string>

namespace PsyMelody {

struct Preset {
    std::string name;
    std::string category;
    GeneratorParams params;
    std::vector<NoteEvent> sequence;
    bool hasSequence() const { return !sequence.empty(); }
};

// Field order in GeneratorParams (positional aggregate init - trailing fields
// omitted here, e.g. humanize/swing, take their in-struct defaults):
// rootNote, scaleIndex, bpm, phraseLengthBars, baseOctave,
// patternCategory, progression, subgenre,
// density, acidAmount, ornamentAmount, graceAmount, rhythmVariation, pitchRange

inline std::vector<Preset> getFactoryPresets()
{
    std::vector<Preset> presets;

    // ============================================================
    // Acid - 303 driven, high density, slide/accent focused
    // ============================================================
    presets.push_back({"Classic 303",                     "Acid", {
        9, 0, 145, 4, 3,
        0, 1,
        0,
        0.8f, 0.85f, 0.15f, 0.0f, 0.2f, 0.3f
    }});

    presets.push_back({"Deep Acid",                       "Acid", {
        9, 0, 145, 8, 4,
        0, 1,
        0,
        0.85f, 0.9f, 0.2f, 0.0f, 0.2f, 0.35f
    }});

    presets.push_back({"Full Acid Drive",                 "Acid", {
        9, 0, 145, 4, 4,
        0, 1,
        0,
        0.95f, 1.0f, 0.1f, 0.0f, 0.15f, 0.25f
    }});

    presets.push_back({"Acid Squelch",                    "Acid", {
        4, 0, 145, 4, 3,
        0, 2,
        0,
        0.9f, 0.95f, 0.1f, 0.0f, 0.25f, 0.2f
    }});

    presets.push_back({"Phrygian Acid",                   "Acid", {
        0, 6, 145, 4, 4,
        0, 1,
        0,
        0.75f, 0.8f, 0.2f, 0.0f, 0.15f, 0.35f
    }});

    // ============================================================
    // Melodic - Hook-driven leads, memorable phrases
    // ============================================================
    presets.push_back({"Cosmic Melody",                   "Melodic", {
        4, 0, 143, 8, 4,
        1, 5,
        0,
        0.55f, 0.4f, 0.5f, 0.0f, 0.4f, 0.5f
    }});

    presets.push_back({"Twisted Lead",                    "Melodic", {
        9, 0, 145, 8, 4,
        1, 1,
        0,
        0.65f, 0.7f, 0.4f, 0.0f, 0.3f, 0.45f
    }});

    presets.push_back({"Phrygian Arp",                    "Melodic", {
        0, 6, 145, 4, 4,
        0, 1,
        0,
        0.75f, 0.5f, 0.2f, 0.0f, 0.15f, 0.4f
    }});

    presets.push_back({"Hypnotic Sequence",               "Melodic", {
        4, 1, 143, 16, 4,
        0, 0,
        0,
        0.7f, 0.6f, 0.3f, 0.0f, 0.2f, 0.3f
    }});

    presets.push_back({"Mysterious Lead",                 "Melodic", {
        7, 0, 142, 8, 4,
        1, 3,
        0,
        0.5f, 0.3f, 0.4f, 0.0f, 0.3f, 0.5f
    }});

    presets.push_back({"Call & Response",                  "Melodic", {
        9, 2, 145, 8, 4,
        1, 1,
        0,
        0.6f, 0.4f, 0.35f, 0.0f, 0.35f, 0.5f
    }});

    // ============================================================
    // Eastern - Oriental/Indian flavored, ornamental
    // ============================================================
    presets.push_back({"Eastern Meditation",              "Eastern", {
        4, 1, 140, 8, 4,
        2, 0,
        0,
        0.3f, 0.1f, 0.8f, 0.4f, 0.3f, 0.4f
    }});

    presets.push_back({"Organic Drift",                   "Eastern", {
        2, 4, 138, 8, 4,
        2, 0,
        0,
        0.35f, 0.2f, 0.7f, 0.3f, 0.5f, 0.6f
    }});

    presets.push_back({"Double Harmonic",                 "Eastern", {
        9, 1, 142, 8, 4,
        1, 0,
        0,
        0.5f, 0.3f, 0.6f, 0.25f, 0.3f, 0.45f
    }});

    presets.push_back({"Hirajoshi Drift",                 "Eastern", {
        4, 4, 140, 8, 4,
        2, 0,
        0,
        0.4f, 0.15f, 0.5f, 0.2f, 0.35f, 0.5f
    }});

    // ============================================================
    // Epic - Dramatic, wide range, cinematic Goa
    // ============================================================
    presets.push_back({"Cinematic Epic",                  "Epic", {
        2, 5, 140, 16, 4,
        1, 5,
        0,
        0.5f, 0.3f, 0.5f, 0.0f, 0.4f, 0.7f
    }});

    presets.push_back({"Dark Lead",                       "Epic", {
        0, 2, 145, 8, 4,
        1, 1,
        0,
        0.6f, 0.5f, 0.6f, 0.0f, 0.5f, 0.6f
    }});

    presets.push_back({"Hungarian Drama",                 "Epic", {
        9, 5, 143, 8, 4,
        1, 5,
        0,
        0.55f, 0.4f, 0.5f, 0.0f, 0.45f, 0.7f
    }});

    presets.push_back({"Chaotic Glitch",                  "Epic", {
        0, 2, 148, 4, 4,
        0, 2,
        0,
        0.9f, 0.8f, 0.3f, 0.0f, 0.7f, 0.4f
    }});

    // ============================================================
    // Ambient - Sparse, atmospheric, intro/outro/breakdown
    // ============================================================
    presets.push_back({"Ambient Intro",                   "Ambient", {
        0, 4, 140, 16, 4,
        3, 0,
        0,
        0.1f, 0.0f, 0.5f, 0.0f, 0.1f, 0.3f
    }});

    presets.push_back({"Breakdown Melody",                "Ambient", {
        9, 2, 145, 8, 5,
        2, 1,
        0,
        0.3f, 0.1f, 0.6f, 0.0f, 0.2f, 0.6f
    }});

    presets.push_back({"Slow Drift",                      "Ambient", {
        4, 0, 140, 16, 4,
        3, 0,
        0,
        0.15f, 0.05f, 0.4f, 0.15f, 0.15f, 0.4f
    }});

    presets.push_back({"Sparse Pulse",                    "Ambient", {
        7, 2, 142, 8, 4,
        3, 0,
        0,
        0.2f, 0.0f, 0.3f, 0.0f, 0.1f, 0.35f
    }});

    // ============================================================
    // Full-On subgenre
    // ============================================================
    presets.push_back({"Festival Anthem",                 "Full-On", {
        9, 3, 145, 8, 4,
        1, 6,  // pattern: LeadMelody, prog: i-iv-bVI-bVII
        1,     // subgenre: Full-On
        0.6f, 0.4f, 0.3f, 0.0f, 0.3f, 0.6f
    }});

    presets.push_back({"Driving Supersaw",               "Full-On", {
        4, 2, 146, 8, 4,
        0, 1,
        1,
        0.8f, 0.5f, 0.2f, 0.0f, 0.25f, 0.5f
    }});

    presets.push_back({"Offbeat Hook",                   "Full-On", {
        7, 3, 145, 4, 4,
        1, 6,
        1,
        0.55f, 0.3f, 0.25f, 0.0f, 0.4f, 0.55f
    }});

    presets.push_back({"Build-Up Riff",                  "Full-On", {
        9, 2, 146, 16, 4,
        0, 1,
        1,
        0.7f, 0.6f, 0.15f, 0.0f, 0.2f, 0.4f
    }});

    // ============================================================
    // Dark Psy subgenre
    // ============================================================
    presets.push_back({"Creeping Chromatic",              "Dark Psy", {
        0, 6, 150, 4, 3,
        0, 7,  // prog: Chromatic Drone
        2,     // subgenre: Dark Psy
        0.8f, 0.7f, 0.4f, 0.0f, 0.6f, 0.3f
    }});

    presets.push_back({"Hi-Tech Chaos",                  "Dark Psy", {
        0, 6, 155, 4, 4,
        0, 7,
        2,
        0.95f, 0.9f, 0.2f, 0.0f, 0.8f, 0.5f
    }});

    presets.push_back({"Glitch Machine",                 "Dark Psy", {
        3, 6, 152, 4, 4,
        0, 7,
        2,
        0.9f, 0.85f, 0.3f, 0.0f, 0.7f, 0.4f
    }});

    presets.push_back({"Forest Crawl",                   "Dark Psy", {
        7, 6, 148, 8, 3,
        1, 0,
        2,
        0.5f, 0.6f, 0.5f, 0.0f, 0.5f, 0.35f
    }});

    // ============================================================
    // Progressive Psy subgenre
    // ============================================================
    presets.push_back({"Minimal Groove",                  "Progressive", {
        9, 3, 138, 8, 4,
        2, 8,  // prog: i-bVII-bVI-v
        3,     // subgenre: Progressive Psy
        0.35f, 0.15f, 0.15f, 0.0f, 0.2f, 0.3f
    }});

    presets.push_back({"Hypnotic Pulse",                 "Progressive", {
        4, 3, 140, 16, 4,
        2, 8,
        3,
        0.45f, 0.2f, 0.1f, 0.0f, 0.15f, 0.25f
    }});

    presets.push_back({"Slow Build",                     "Progressive", {
        7, 3, 138, 16, 4,
        3, 0,
        3,
        0.2f, 0.1f, 0.2f, 0.0f, 0.1f, 0.4f
    }});

    presets.push_back({"Subtle Hook",                    "Progressive", {
        2, 3, 140, 8, 4,
        1, 8,
        3,
        0.5f, 0.2f, 0.2f, 0.0f, 0.25f, 0.35f
    }});

    return presets;
}

} // namespace PsyMelody
