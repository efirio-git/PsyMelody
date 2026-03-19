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
    PsyMelody::GeneratorParams& getGeneratorParams() { return genParams; }
    const PsyMelody::GeneratorParams& getGeneratorParams() const { return genParams; }
    const std::vector<PsyMelody::NoteEvent>& getCurrentPhrase() const { return currentPhrase; }
    double getPlaybackPositionBeats() const { return playbackPosition.load(); }
    double getPhraseLengthBeats() const { return patternEngine.getPhraseLengthBeats(); }
    bool isCurrentlyPlaying() const { return playing.load(); }
    double getDawBpm() const { return dawBpm.load(); }

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

    double currentSampleRate = 44100.0;
    std::atomic<double> playbackPosition{0.0};
    std::atomic<bool> playing{false};
    std::atomic<double> dawBpm{0.0};  // 0 = no DAW BPM available

    // Undo/Redo history
    std::vector<std::vector<PsyMelody::NoteEvent>> undoHistory;
    std::vector<std::vector<PsyMelody::NoteEvent>> redoHistory;
    static constexpr int maxUndoHistory = 50;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsyMelodyProcessor)
};
