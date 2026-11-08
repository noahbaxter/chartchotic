#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include "MonitorTriggers.h"

// Plays the chart back as sound: a metronome, and a sample per note. The trigger lists are
// built on the message thread and handed over whole; the audio thread only reads them.
class MonitorPlayer
{
public:
    MonitorPlayer();

    void prepare(double sampleRate);

    // Message thread. The old list is released here, never on the audio thread.
    void publish(std::shared_ptr<const MonitorTriggers> triggers);

    void setClickEnabled(bool on) { clickEnabled.store(on); }
    void setNotesEnabled(bool on) { notesEnabled.store(on); }

    // Audio thread. `ppqStart` is the block's first sample and `ppqPerSample` how far each
    // sample moves; while stopped nothing new fires but ringing hits finish.
    void render(juce::AudioBuffer<float>& buffer, double ppqStart, double ppqPerSample, bool playing);

private:
    struct Sample
    {
        juce::AudioBuffer<float> data;
        double sourceRate = 44100.0;
    };

    struct Voice
    {
        const Sample* sample = nullptr;
        double position = 0.0;   // in source samples
        double step = 1.0;       // source samples per output sample
        float gain = 0.0f;
        int startOffset = 0;     // output samples to wait before sounding, this block only
    };

    void trigger(const MonitorTrigger& t, int offset);
    // Fires what sits in [from, ppqEnd), placed against the block's own start.
    void fire(const std::vector<MonitorTrigger>& list, double from, double ppqEnd,
              double ppqStart, double ppqPerSample, int numSamples);

    MonitorFireWindow window;

    std::array<Sample, (size_t)MonitorSample::Count> samples;
    std::array<Voice, 32> voices;
    size_t nextVoice = 0;
    double outputRate = 44100.0;

    juce::SpinLock triggersLock;
    std::shared_ptr<const MonitorTriggers> triggers;

    std::atomic<bool> clickEnabled { false };
    std::atomic<bool> notesEnabled { false };
};
