#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

PsyMelodyProcessor::PsyMelodyProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

PsyMelodyProcessor::~PsyMelodyProcessor() {}

void PsyMelodyProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    patternEngine.reset();
    previewSynth.prepareToPlay(sampleRate, samplesPerBlock);
}

void PsyMelodyProcessor::releaseResources()
{
    patternEngine.reset();
}

void PsyMelodyProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages)
{
    buffer.clear();
    midiMessages.clear();

    auto playHead = getPlayHead();
    if (playHead == nullptr) return;

    auto posInfo = playHead->getPosition();
    if (!posInfo.hasValue()) return;

    double bpm = genParams.bpm;
    if (auto bpmOpt = posInfo->getBpm())
        bpm = *bpmOpt;

    double ppq = 0.0;
    if (auto ppqOpt = posInfo->getPpqPosition())
        ppq = *ppqOpt;

    bool isPlaying = posInfo->getIsPlaying();
    bool wasPlaying = playing.load();
    playing.store(isPlaying);

    // Reset preview synth when playback stops
    if (wasPlaying && !isPlaying)
        previewSynth.allNotesOff();

    patternEngine.processBlock(midiMessages, bpm, ppq,
                                buffer.getNumSamples(),
                                currentSampleRate, isPlaying);

    // Store looped position for UI
    double phraseLen = patternEngine.getPhraseLengthBeats();
    if (phraseLen > 0.0 && isPlaying) {
        double looped = std::fmod(ppq, phraseLen);
        if (looped < 0.0) looped += phraseLen;
        playbackPosition.store(looped);
    }

    // Preview synth: render audio from the MIDI that was just generated
    if (previewSynth.getEnabled()) {
        previewSynth.processBlock(buffer, midiMessages);
    }
}

void PsyMelodyProcessor::generateNewPhrase()
{
    pushUndoState();
    previewSynth.allNotesOff();
    currentPhrase = generator.generatePhrase(genParams);
    patternEngine.loadPhrase(currentPhrase);
}

void PsyMelodyProcessor::generateBassline(const PsyMelody::BassParams& params)
{
    pushUndoState();
    currentPhrase = bassGenerator.generateBassline(params);
    patternEngine.loadPhrase(currentPhrase);
}

void PsyMelodyProcessor::generateChordVoicing(const PsyMelody::ChordVoicingParams& params)
{
    pushUndoState();
    currentPhrase = chordGenerator.generateVoicing(params);
    patternEngine.loadPhrase(currentPhrase);
}

void PsyMelodyProcessor::generateVariation()
{
    if (!patternEngine.hasPhrase()) {
        generateNewPhrase();
        return;
    }
    pushUndoState();
    currentPhrase = generator.generateVariation(currentPhrase, genParams);
    patternEngine.loadPhrase(currentPhrase);
}

void PsyMelodyProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state("PsyMelodyState");
    state.setProperty("rootNote", genParams.rootNote, nullptr);
    state.setProperty("scaleIndex", genParams.scaleIndex, nullptr);
    state.setProperty("bpm", genParams.bpm, nullptr);
    state.setProperty("phraseLengthBars", genParams.phraseLengthBars, nullptr);
    state.setProperty("baseOctave", genParams.baseOctave, nullptr);
    state.setProperty("patternCategory", genParams.patternCategory, nullptr);
    state.setProperty("progression", genParams.progression, nullptr);
    state.setProperty("subgenre", genParams.subgenre, nullptr);
    state.setProperty("density", genParams.density, nullptr);
    state.setProperty("acidAmount", genParams.acidAmount, nullptr);
    state.setProperty("ornamentAmount", genParams.ornamentAmount, nullptr);
    state.setProperty("graceAmount", genParams.graceAmount, nullptr);
    state.setProperty("rhythmVariation", genParams.rhythmVariation, nullptr);
    state.setProperty("pitchRange", genParams.pitchRange, nullptr);

    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void PsyMelodyProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (state.isValid()) {
        genParams.rootNote = state.getProperty("rootNote", 0);
        genParams.scaleIndex = state.getProperty("scaleIndex", 0);
        genParams.bpm = state.getProperty("bpm", 145.0f);
        genParams.phraseLengthBars = state.getProperty("phraseLengthBars", 4);
        genParams.baseOctave = state.getProperty("baseOctave", 4);
        genParams.patternCategory = state.getProperty("patternCategory", 1);
        genParams.progression = state.getProperty("progression", 0);
        genParams.subgenre = state.getProperty("subgenre", 0);
        genParams.density = state.getProperty("density", 0.6f);
        genParams.acidAmount = state.getProperty("acidAmount", 0.5f);
        genParams.ornamentAmount = state.getProperty("ornamentAmount", 0.3f);
        genParams.graceAmount = state.getProperty("graceAmount", 0.0f);
        genParams.rhythmVariation = state.getProperty("rhythmVariation", 0.4f);
        genParams.pitchRange = state.getProperty("pitchRange", 0.5f);
    }
}

void PsyMelodyProcessor::updatePhrase(const std::vector<PsyMelody::NoteEvent>& phrase)
{
    // Note: pushUndoState is NOT called here - caller is responsible
    previewSynth.allNotesOff();
    currentPhrase = phrase;
    patternEngine.loadPhrase(currentPhrase);
}

void PsyMelodyProcessor::addNote(const PsyMelody::NoteEvent& note)
{
    // Note: pushUndoState is NOT called here - caller is responsible
    currentPhrase.push_back(note);
    std::sort(currentPhrase.begin(), currentPhrase.end(),
              [](const PsyMelody::NoteEvent& a, const PsyMelody::NoteEvent& b) {
                  return a.startBeat < b.startBeat;
              });
    patternEngine.loadPhrase(currentPhrase);
}

void PsyMelodyProcessor::removeNoteAt(int index)
{
    if (index >= 0 && index < (int)currentPhrase.size()) {
        // Note: pushUndoState is NOT called here - caller is responsible
        currentPhrase.erase(currentPhrase.begin() + index);
        patternEngine.loadPhrase(currentPhrase);
    }
}

void PsyMelodyProcessor::moveNote(int index, int newNoteNumber, double newStartBeat)
{
    if (index >= 0 && index < (int)currentPhrase.size()) {
        // Note: pushUndoState is NOT called here - called once at drag start
        currentPhrase[static_cast<size_t>(index)].noteNumber = std::clamp(newNoteNumber, 0, 127);
        currentPhrase[static_cast<size_t>(index)].startBeat = std::max(0.0, newStartBeat);
        std::sort(currentPhrase.begin(), currentPhrase.end(),
                  [](const PsyMelody::NoteEvent& a, const PsyMelody::NoteEvent& b) {
                      return a.startBeat < b.startBeat;
                  });
        patternEngine.loadPhrase(currentPhrase);
    }
}

void PsyMelodyProcessor::pushUndoState()
{
    undoHistory.push_back(currentPhrase);
    if ((int)undoHistory.size() > maxUndoHistory)
        undoHistory.erase(undoHistory.begin());
    redoHistory.clear();
}

void PsyMelodyProcessor::undo()
{
    if (!canUndo())
        return;
    redoHistory.push_back(currentPhrase);
    currentPhrase = undoHistory.back();
    undoHistory.pop_back();
    patternEngine.loadPhrase(currentPhrase);
}

void PsyMelodyProcessor::redo()
{
    if (!canRedo())
        return;
    undoHistory.push_back(currentPhrase);
    currentPhrase = redoHistory.back();
    redoHistory.pop_back();
    patternEngine.loadPhrase(currentPhrase);
}

bool PsyMelodyProcessor::canUndo() const
{
    return !undoHistory.empty();
}

bool PsyMelodyProcessor::canRedo() const
{
    return !redoHistory.empty();
}

// Preview synth wrapper methods
void PsyMelodyProcessor::setPreviewEnabled(bool enabled)
{
    previewSynth.setEnabled(enabled);
}

bool PsyMelodyProcessor::isPreviewEnabled() const
{
    return previewSynth.getEnabled();
}

void PsyMelodyProcessor::setPreviewWaveType(PsyMelody::PreviewSynth::WaveType type)
{
    previewSynth.setWaveType(type);
}

PsyMelody::PreviewSynth::WaveType PsyMelodyProcessor::getPreviewWaveType() const
{
    return previewSynth.getWaveType();
}

void PsyMelodyProcessor::setPreviewVolume(float vol)
{
    previewSynth.setVolume(vol);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PsyMelodyProcessor();
}
