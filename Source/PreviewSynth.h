#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

namespace PsyMelody {

class PreviewSynth {
public:
    PreviewSynth();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiMessages);
    void setEnabled(bool enabled) { isEnabled = enabled; if (!enabled) allNotesOff(); }
    void allNotesOff() { for (auto& v : voices) { v.active = false; v.envLevel = 0.0f; v.envTarget = 0.0f; v.filterState = 0.0f; } }
    bool getEnabled() const { return isEnabled; }
    void setVolume(float vol) { volume = std::clamp(vol, 0.0f, 1.0f); }

    // Waveform type
    enum class WaveType { Saw, Square, Sine, Triangle };
    void setWaveType(WaveType type) { waveType = type; }
    WaveType getWaveType() const { return waveType; }

private:
    bool isEnabled = false;
    float volume = 0.15f;
    double sampleRate = 44100.0;
    WaveType waveType = WaveType::Saw;

    // Simple polyphonic synth (8 voices)
    static constexpr int maxVoices = 8;
    struct Voice {
        bool active = false;
        int noteNumber = 0;
        float velocity = 0.0f;
        double phase = 0.0;
        double phaseIncrement = 0.0;
        float envLevel = 0.0f;      // simple AR envelope
        float envTarget = 0.0f;
        float filterState = 0.0f;   // simple low-pass filter
        uint64_t startOrder = 0;    // for voice stealing (oldest first)
    };
    Voice voices[maxVoices];
    uint64_t voiceCounter = 0;

    float filterCutoff = 0.3f;  // normalized 0-1
    float filterResonance = 0.2f;

    int findFreeVoice();
    int findVoiceForNote(int noteNumber);
    float generateSample(Voice& voice);
    float midiNoteToFreq(int noteNumber);
    float polyBlep(double t, double dt);
};

} // namespace PsyMelody
