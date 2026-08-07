#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "GoaMelodyGenerator.h"
#include "BasslineGenerator.h"
#include "ChordVoicingGenerator.h"
#include "MidiPatternEngine.h"
#include "PreviewSynth.h"

class PsyMelodyProcessor : public juce::AudioProcessor {
public:
    PsyMelodyProcessor();
    ~PsyMelodyProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Generation modes
    enum class GenMode { Melody = 0, Bassline, ChordVoicing };
    void setGenMode(GenMode m) { genMode = m; }
    GenMode getGenMode() const { return genMode; }

    void generateNewPhrase();
    void generateVariation();
    void generateBassline(const PsyMelody::BassParams& params);
    void generateChordVoicing(const PsyMelody::ChordVoicingParams& params);

    // Partial regeneration (melody mode); always uses fresh entropy so
    // repeated presses differ even while the seed is locked
    void regeneratePitches();
    void regenerateRhythm();

    // Seed lock: locked seeds make GENERATE reproduce the exact same phrase
    unsigned int getCurrentSeed() const { return currentSeed; }
    void setCurrentSeed(unsigned int s) { currentSeed = s; }
    bool isSeedLocked() const { return seedLocked; }
    void setSeedLocked(bool locked) { seedLocked = locked; }
    PsyMelody::GeneratorParams& getGeneratorParams() { return genParams; }
    const PsyMelody::GeneratorParams& getGeneratorParams() const { return genParams; }
    const std::vector<PsyMelody::NoteEvent>& getCurrentPhrase() const { return currentPhrase; }
    double getPlaybackPositionBeats() const { return playbackPosition.load(); }
    double getPhraseLengthBeats() const { return patternEngine.getPhraseLengthBeats(); }
    bool isCurrentlyPlaying() const { return playing.load(); }
    double getDawBpm() const { return dawBpm.load(); }

    // All BPM writes must go through here so the audio thread's fallback
    // (used when the host reports no tempo) stays in sync lock-free
    void setBpm(float b) { genParams.bpm = b; fallbackBpm.store(b); }

    // Bumped by setStateInformation so an already-open editor can resync
    juce::uint32 getStateVersion() const { return stateVersion.load(); }

    // Editing interface
    void updatePhrase(const std::vector<PsyMelody::NoteEvent>& phrase);
    void addNote(const PsyMelody::NoteEvent& note);
    void removeNoteAt(int index);
    void moveNote(int index, int newNoteNumber, double newStartBeat);

    // Undo/Redo
    void pushUndoState();
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // Preview synth controls
    void setPreviewEnabled(bool enabled);
    bool isPreviewEnabled() const;
    void setPreviewWaveType(PsyMelody::PreviewSynth::WaveType type);
    PsyMelody::PreviewSynth::WaveType getPreviewWaveType() const;
    void setPreviewVolume(float vol);

private:
    PsyMelody::GoaMelodyGenerator generator;
    PsyMelody::BasslineGenerator bassGenerator;
    PsyMelody::ChordVoicingGenerator chordGenerator;
    PsyMelody::MidiPatternEngine patternEngine;
    PsyMelody::PreviewSynth previewSynth;
    GenMode genMode = GenMode::Melody;
    PsyMelody::GeneratorParams genParams;
    std::vector<PsyMelody::NoteEvent> currentPhrase;

    // Seed management: picks a fresh seed unless locked, then seeds all
    // three generators so a locked GENERATE is fully deterministic
    void prepareSeedForGenerate();
    unsigned int currentSeed = 0;
    bool seedLocked = false;

    double currentSampleRate = 44100.0;
    std::atomic<double> playbackPosition{0.0};
    std::atomic<bool> playing{false};
    std::atomic<double> dawBpm{0.0};  // 0 = no DAW BPM available
    std::atomic<float> fallbackBpm{145.0f};
    std::atomic<juce::uint32> stateVersion{0};

    // Undo/Redo history
    std::vector<std::vector<PsyMelody::NoteEvent>> undoHistory;
    std::vector<std::vector<PsyMelody::NoteEvent>> redoHistory;
    static constexpr int maxUndoHistory = 50;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsyMelodyProcessor)
};
