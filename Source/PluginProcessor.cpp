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
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midiMessages.clear();

    auto playHead = getPlayHead();
    if (playHead == nullptr) return;

    auto posInfo = playHead->getPosition();
    if (!posInfo.hasValue()) return;

    double bpm = fallbackBpm.load();
    if (auto bpmOpt = posInfo->getBpm()) {
        bpm = *bpmOpt;
        dawBpm.store(bpm);
    }

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

    // Persist the edited sequence (same attribute names as user preset XML)
    juce::ValueTree seq("Sequence");
    for (const auto& n : currentPhrase) {
        juce::ValueTree note("Note");
        note.setProperty("nn", n.noteNumber, nullptr);
        note.setProperty("vel", n.velocity, nullptr);
        note.setProperty("pan", n.pan, nullptr);
        note.setProperty("start", n.startBeat, nullptr);
        note.setProperty("dur", n.duration, nullptr);
        note.setProperty("pb", n.pitchBend, nullptr);
        note.setProperty("acc", n.accent, nullptr);
        note.setProperty("sld", n.slide, nullptr);
        note.setProperty("grace", n.isGraceNote, nullptr);
        seq.appendChild(note, nullptr);
    }
    state.appendChild(seq, nullptr);

    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void PsyMelodyProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (state.isValid()) {
        // Hosts can feed us arbitrary state data - clamp everything
        genParams.rootNote = std::clamp((int)state.getProperty("rootNote", 0), 0, 11);
        genParams.scaleIndex = std::clamp((int)state.getProperty("scaleIndex", 0),
                                          0, (int)PsyMelody::GOA_SCALES.size() - 1);
        setBpm(std::clamp((float)state.getProperty("bpm", 145.0f), 60.0f, 200.0f));
        genParams.phraseLengthBars = std::clamp((int)state.getProperty("phraseLengthBars", 4), 1, 16);
        genParams.baseOctave = std::clamp((int)state.getProperty("baseOctave", 4), 2, 6);
        genParams.patternCategory = std::clamp((int)state.getProperty("patternCategory", 1), 0, 3);
        genParams.progression = std::clamp((int)state.getProperty("progression", 0), 0, 8);
        genParams.subgenre = std::clamp((int)state.getProperty("subgenre", 0), 0, 3);
        genParams.density = std::clamp((float)state.getProperty("density", 0.6f), 0.0f, 1.0f);
        genParams.acidAmount = std::clamp((float)state.getProperty("acidAmount", 0.5f), 0.0f, 1.0f);
        genParams.ornamentAmount = std::clamp((float)state.getProperty("ornamentAmount", 0.3f), 0.0f, 1.0f);
        genParams.graceAmount = std::clamp((float)state.getProperty("graceAmount", 0.0f), 0.0f, 1.0f);
        genParams.rhythmVariation = std::clamp((float)state.getProperty("rhythmVariation", 0.4f), 0.0f, 1.0f);
        genParams.pitchRange = std::clamp((float)state.getProperty("pitchRange", 0.5f), 0.0f, 1.0f);

        auto seq = state.getChildWithName("Sequence");
        if (seq.isValid()) {
            std::vector<PsyMelody::NoteEvent> phrase;
            phrase.reserve(static_cast<size_t>(seq.getNumChildren()));
            for (const auto& note : seq) {
                if (!note.hasType("Note")) continue;
                PsyMelody::NoteEvent n;
                n.noteNumber = std::clamp((int)note.getProperty("nn", 60), 0, 127);
                n.velocity = std::clamp((float)note.getProperty("vel", 0.8f), 0.0f, 1.0f);
                n.pan = std::clamp((float)note.getProperty("pan", 0.0f), -1.0f, 1.0f);
                n.startBeat = std::max(0.0, (double)note.getProperty("start", 0.0));
                n.duration = std::max(0.0625, (double)note.getProperty("dur", 0.25));
                n.pitchBend = std::clamp((int)note.getProperty("pb", 0), -8192, 8191);
                n.accent = note.getProperty("acc", false);
                n.slide = note.getProperty("sld", false);
                n.isGraceNote = note.getProperty("grace", false);
                phrase.push_back(n);
            }
            currentPhrase = std::move(phrase);
            patternEngine.loadPhrase(currentPhrase);
        }

        stateVersion.fetch_add(1);
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
