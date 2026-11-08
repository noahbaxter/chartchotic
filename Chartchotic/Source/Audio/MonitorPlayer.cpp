#include "MonitorPlayer.h"
#include "BinaryData.h"

namespace
{
    juce::AudioBuffer<float> decode(const void* data, int size, double& rate)
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader(
            wav.createReaderFor(new juce::MemoryInputStream(data, (size_t)size, false), true));
        if (reader == nullptr) return {};

        juce::AudioBuffer<float> out((int)reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&out, 0, (int)reader->lengthInSamples, 0, true, true);
        rate = reader->sampleRate;
        return out;
    }
}

MonitorPlayer::MonitorPlayer()
{
    auto load = [this](MonitorSample s, const char* data, int size)
    {
        auto& slot = samples[(size_t)s];
        slot.data = decode(data, size, slot.sourceRate);
    };

    load(MonitorSample::Bar,         BinaryData::bar_wav,       BinaryData::bar_wavSize);
    load(MonitorSample::Beat,        BinaryData::quarter_wav,   BinaryData::quarter_wavSize);
    load(MonitorSample::GuitarClick, BinaryData::sixteenth_wav, BinaryData::sixteenth_wavSize);
    load(MonitorSample::Kick,        BinaryData::KICK_wav,      BinaryData::KICK_wavSize);
    load(MonitorSample::Snare,       BinaryData::SNARE_wav,     BinaryData::SNARE_wavSize);
    load(MonitorSample::HatClosed,   BinaryData::HAT_wav,       BinaryData::HAT_wavSize);
    load(MonitorSample::HatOpen,     BinaryData::HH_o_wav,      BinaryData::HH_o_wavSize);
    load(MonitorSample::Tom1,        BinaryData::TOM1_wav,      BinaryData::TOM1_wavSize);
    load(MonitorSample::Tom2,        BinaryData::TOM2_wav,      BinaryData::TOM2_wavSize);
    load(MonitorSample::Tom3,        BinaryData::TOM3_wav,      BinaryData::TOM3_wavSize);
    load(MonitorSample::Crash,       BinaryData::CRASH2_wav,    BinaryData::CRASH2_wavSize);
    load(MonitorSample::Ride,        BinaryData::RIDE_wav,      BinaryData::RIDE_wavSize);
    load(MonitorSample::RideBell,    BinaryData::RIDE_BELL_wav, BinaryData::RIDE_BELL_wavSize);
}

void MonitorPlayer::prepare(double sampleRate)
{
    outputRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (auto& v : voices) v.sample = nullptr;
}

void MonitorPlayer::publish(std::shared_ptr<const MonitorTriggers> next)
{
    {
        const juce::SpinLock::ScopedLockType lock(triggersLock);
        std::swap(triggers, next);
    }
    // `next` now holds the old list and is released here, outside the lock.
}

void MonitorPlayer::trigger(const MonitorTrigger& t, int offset)
{
    const auto& s = samples[(size_t)t.sample];
    if (s.data.getNumSamples() == 0) return;

    // Round robin: with 32 voices the one stolen is long finished or nearly so.
    auto& v = voices[nextVoice];
    nextVoice = (nextVoice + 1) % voices.size();
    v.sample = &s;
    v.position = 0.0;
    v.step = s.sourceRate / outputRate;
    v.gain = t.gain;
    v.startOffset = offset;
}

void MonitorPlayer::fire(const std::vector<MonitorTrigger>& list, double from, double ppqEnd,
                         double ppqStart, double ppqPerSample, int numSamples)
{
    auto it = std::lower_bound(list.begin(), list.end(), from,
                               [](const MonitorTrigger& t, double qn) { return t.qn < qn; });
    for (; it != list.end() && it->qn < ppqEnd; ++it)
    {
        // A hit the last block fell short of lands at this block's first sample.
        const int offset = juce::jlimit(0, numSamples - 1, (int)((it->qn - ppqStart) / ppqPerSample));
        trigger(*it, offset);
    }
}

void MonitorPlayer::render(juce::AudioBuffer<float>& buffer, double ppqStart, double ppqPerSample,
                           bool playing)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    if (playing && ppqPerSample > 0.0)
    {
        const double ppqEnd = ppqStart + ppqPerSample * numSamples;
        const double from = window.begin(ppqStart, ppqEnd);

        // Skipped rather than waited on: a block that loses the race to a publish only misses
        // the hits inside it, it never stalls the audio thread.
        const juce::SpinLock::ScopedTryLockType lock(triggersLock);
        if (lock.isLocked() && triggers != nullptr)
        {
            if (clickEnabled.load()) fire(triggers->click, from, ppqEnd, ppqStart, ppqPerSample, numSamples);
            if (notesEnabled.load()) fire(triggers->notes, from, ppqEnd, ppqStart, ppqPerSample, numSamples);
        }
    }
    else
    {
        window.stop();
    }

    const int outChannels = buffer.getNumChannels();
    if (outChannels == 0) return;

    for (auto& v : voices)
    {
        if (v.sample == nullptr) continue;

        const auto& data = v.sample->data;
        const int length = data.getNumSamples();
        const int srcChannels = data.getNumChannels();

        for (int i = v.startOffset; i < numSamples; ++i)
        {
            const int idx = (int)v.position;
            if (idx + 1 >= length) { v.sample = nullptr; break; }
            const float frac = (float)(v.position - idx);

            for (int ch = 0; ch < outChannels; ++ch)
            {
                // Mono samples feed every output; stereo ones map left to left, right to right.
                const float* src = data.getReadPointer(juce::jmin(ch, srcChannels - 1));
                const float value = src[idx] + (src[idx + 1] - src[idx]) * frac;
                buffer.addSample(ch, i, value * v.gain);
            }
            v.position += v.step;
        }
        v.startOffset = 0;
    }
}
