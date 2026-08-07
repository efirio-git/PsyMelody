#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace PsyMelody {

class PreviewSynth {
public:
    PreviewSynth();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiMessages);
    void setEnabled(bool enabled) { isEnabled.store(enabled); if (!enabled) allNotesOff(); }
    // Safe from any thread: voices are zeroed by the audio thread at the
    // start of the next processed block
    void allNotesOff() { killRequested.store(true); }
    bool getEnabled() const { return isEnabled.load(); }
    void setVolume(float vol) { volume.store(std::clamp(vol, 0.0f, 1.0f)); }

    // Waveform type
    enum class WaveType { Saw, Square, Sine, Triangle };
    void setWaveType(WaveType type) { waveType.store(type); }
    WaveType getWaveType() const { return waveType.load(); }

private:
    std::atomic<bool> isEnabled { false };
    std::atomic<float> volume { 0.15f };
    std::atomic<WaveType> waveType { WaveType::Saw };
    std::atomic<bool> killRequested { false };
    double sampleRate = 44100.0;

    static_assert(std::atomic<WaveType>::is_always_lock_free,
                  "WaveType atomic must be lock-free for the audio thread");
    static_assert(std::atomic<float>::is_always_lock_free,
                  "float atomic must be lock-free for the audio thread");

    // Simple polyphonic synth (8 voices)
    static constexpr int maxVoices = 8;
    struct Voice {
        bool active = false;
        int noteNumber = 0;
        float velocity = 0.0f;
        float pan = 0.0f;           // from CC10; 0 = use auto spread
        double phase = 0.0;
        double phaseIncrement = 0.0;
        float envLevel = 0.0f;      // simple AR envelope
        float envTarget = 0.0f;
        float filterState = 0.0f;   // simple low-pass filter
        uint64_t startOrder = 0;    // for voice stealing (oldest first)
    };
    Voice voices[maxVoices];
    uint64_t voiceCounter = 0;
    float channelPan = 0.0f;        // audio thread only, last received CC10

    float filterCutoff = 0.3f;  // normalized 0-1
    float filterResonance = 0.2f;

    int findFreeVoice();
    int findVoiceForNote(int noteNumber);
    float generateSample(Voice& voice, WaveType wave);
    float midiNoteToFreq(int noteNumber);
    float polyBlep(double t, double dt);
};

} // namespace PsyMelody
