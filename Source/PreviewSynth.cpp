#include "PreviewSynth.h"

namespace PsyMelody {

PreviewSynth::PreviewSynth()
{
    for (auto& v : voices)
        v = Voice{};
}

void PreviewSynth::prepareToPlay(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;
    // Reset all voices on prepare
    for (auto& v : voices) {
        v = Voice{};
    }
    voiceCounter = 0;
    channelPan = 0.0f;
    killRequested.store(false);
}

void PreviewSynth::processBlock(juce::AudioBuffer<float>& buffer,
                                 const juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Deferred allNotesOff so message-thread callers never touch the voices
    if (killRequested.exchange(false)) {
        for (auto& v : voices) {
            v.active = false;
            v.envLevel = 0.0f;
            v.envTarget = 0.0f;
            v.filterState = 0.0f;
        }
    }

    // Snapshot atomics once per block; the per-sample path stays load-free
    const float vol = volume.load();
    const WaveType wave = waveType.load();

    // Envelope time constants
    const float attackCoeff  = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.003));  // ~3ms attack
    const float releaseCoeff = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.015));  // ~15ms release

    // Filter coefficient from cutoff (one-pole low-pass with resonance feedback)
    // Map cutoff 0-1 to a frequency range suitable for synth preview
    const float cutoffFreq = 200.0f + filterCutoff * 8000.0f; // 200 Hz to 8200 Hz
    float fc = 2.0f * std::sin(juce::MathConstants<float>::pi * cutoffFreq / (float)sampleRate);
    fc = std::clamp(fc, 0.0f, 1.0f);

    // Process MIDI events into a sample-indexed structure
    // We iterate MIDI and audio samples together
    auto midiIt = midiMessages.cbegin();

    for (int sample = 0; sample < numSamples; ++sample) {
        // Handle MIDI messages at this sample position
        while (midiIt != midiMessages.cend()) {
            const auto metadata = *midiIt;
            if (metadata.samplePosition > sample)
                break;

            const auto msg = metadata.getMessage();
            if (msg.isNoteOn()) {
                int vi = findVoiceForNote(msg.getNoteNumber());
                if (vi < 0)
                    vi = findFreeVoice();

                Voice& v = voices[vi];
                v.active = true;
                v.noteNumber = msg.getNoteNumber();
                v.velocity = msg.getFloatVelocity();
                v.pan = channelPan;
                v.phase = 0.0;
                v.phaseIncrement = (double)midiNoteToFreq(msg.getNoteNumber()) / sampleRate;
                v.envTarget = 1.0f;
                v.startOrder = voiceCounter++;
                // Don't reset filterState for smoother transitions
            }
            else if (msg.isNoteOff()) {
                int vi = findVoiceForNote(msg.getNoteNumber());
                if (vi >= 0) {
                    voices[vi].envTarget = 0.0f;
                    voices[vi].active = false;  // mark inactive immediately
                }
            }
            else if (msg.isController() && msg.getControllerNumber() == 10) {
                channelPan = std::clamp(((float)msg.getControllerValue() - 64.0f) / 63.5f,
                                        -1.0f, 1.0f);
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
                for (auto& v : voices) {
                    v.envTarget = 0.0f;
                }
            }
            ++midiIt;
        }

        // Generate audio from all active voices
        float mixL = 0.0f;
        float mixR = 0.0f;

        for (auto& v : voices) {
            if (!v.active && v.envLevel < 0.005f)
                continue;

            // Envelope
            if (v.envTarget > 0.5f) {
                v.envLevel += attackCoeff * (v.envTarget - v.envLevel);
            } else {
                v.envLevel += releaseCoeff * (v.envTarget - v.envLevel);
                // Deactivate voice when envelope is essentially zero
                if (v.envLevel < 0.005f) {
                    v.active = false;
                    v.envLevel = 0.0f;
                    continue;
                }
            }

            // Generate raw oscillator sample
            float raw = generateSample(v, wave);

            // Advance phase
            v.phase += v.phaseIncrement;
            if (v.phase >= 1.0)
                v.phase -= 1.0;

            // Apply one-pole resonant low-pass filter
            // Simple state-variable style for a bit of resonance
            float feedback = filterResonance * (1.0f - 0.15f * fc);
            float input = raw - feedback * v.filterState;
            v.filterState += fc * input;

            float filtered = v.filterState;

            // Apply envelope and velocity
            float out = filtered * v.envLevel * v.velocity;

            // CC10 pan wins when present, otherwise auto spread by note number
            float autoSpread = std::clamp((float)(v.noteNumber - 60) / 24.0f, -1.0f, 1.0f) * 0.3f;
            float pan = (v.pan != 0.0f) ? v.pan * 0.7f : autoSpread;
            float gainL = std::cos((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            float gainR = std::sin((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);

            mixL += out * gainL;
            mixR += out * gainR;
        }

        // Apply master volume
        mixL *= vol;
        mixR *= vol;

        // Write to buffer
        if (numChannels >= 1) buffer.addSample(0, sample, mixL);
        if (numChannels >= 2) buffer.addSample(1, sample, mixR);
    }
}

float PreviewSynth::generateSample(Voice& voice, WaveType wave)
{
    double p = voice.phase;
    double dt = voice.phaseIncrement;

    switch (wave) {
        case WaveType::Saw: {
            // Naive saw: 2*p - 1, then apply PolyBLEP at discontinuity
            float saw = (float)(2.0 * p - 1.0);
            saw -= polyBlep(p, dt);
            return saw;
        }
        case WaveType::Square: {
            // Naive square, with PolyBLEP at both transitions
            float sq = (p < 0.5) ? 1.0f : -1.0f;
            sq += polyBlep(p, dt);                              // discontinuity at 0/1
            sq -= polyBlep(std::fmod(p + 0.5, 1.0), dt);       // discontinuity at 0.5
            return sq;
        }
        case WaveType::Sine: {
            return (float)std::sin(juce::MathConstants<double>::twoPi * p);
        }
        case WaveType::Triangle: {
            return 2.0f * std::abs(2.0f * (float)p - 1.0f) - 1.0f;
        }
    }
    return 0.0f;
}

float PreviewSynth::polyBlep(double t, double dt)
{
    // PolyBLEP anti-aliasing for saw/square waveforms
    if (t < dt) {
        // Rising discontinuity
        double tn = t / dt;
        return (float)(tn + tn - tn * tn - 1.0);
    }
    else if (t > 1.0 - dt) {
        // Falling discontinuity
        double tn = (t - 1.0) / dt;
        return (float)(tn * tn + tn + tn + 1.0);
    }
    return 0.0f;
}

int PreviewSynth::findFreeVoice()
{
    // First: find an inactive voice
    for (int i = 0; i < maxVoices; ++i) {
        if (!voices[i].active && voices[i].envLevel < 0.005f)
            return i;
    }

    // Second: find a releasing voice (envTarget == 0) with lowest envelope
    int bestIdx = -1;
    float bestLevel = 999.0f;
    for (int i = 0; i < maxVoices; ++i) {
        if (voices[i].envTarget < 0.5f && voices[i].envLevel < bestLevel) {
            bestLevel = voices[i].envLevel;
            bestIdx = i;
        }
    }
    if (bestIdx >= 0)
        return bestIdx;

    // Third: steal the oldest active voice
    uint64_t oldest = UINT64_MAX;
    int oldestIdx = 0;
    for (int i = 0; i < maxVoices; ++i) {
        if (voices[i].startOrder < oldest) {
            oldest = voices[i].startOrder;
            oldestIdx = i;
        }
    }
    return oldestIdx;
}

int PreviewSynth::findVoiceForNote(int noteNumber)
{
    for (int i = 0; i < maxVoices; ++i) {
        if (voices[i].active && voices[i].noteNumber == noteNumber)
            return i;
    }
    return -1;
}

float PreviewSynth::midiNoteToFreq(int noteNumber)
{
    return 440.0f * std::pow(2.0f, (float)(noteNumber - 69) / 12.0f);
}

} // namespace PsyMelody
