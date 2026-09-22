#include <catch2/catch_test_macros.hpp>

#include "Audio/WaveformPeaks.h"

namespace
{
    // One time slice, written to both channels unless told otherwise.
    void append(Waveform::Peaks& peaks, float min, float max)
    {
        Waveform::Peak frame[Waveform::MAX_CHANNELS] = { { min, max }, { min, max } };
        peaks.appendFrame(frame);
    }

    void appendSplit(Waveform::Peaks& peaks, Waveform::Peak left, Waveform::Peak right)
    {
        Waveform::Peak frame[Waveform::MAX_CHANNELS] = { left, right };
        peaks.appendFrame(frame);
    }
}

TEST_CASE("WaveformPeaks - a span takes the extreme of every bucket it covers", "[waveform]")
{
    Waveform::Peaks peaks;
    peaks.reset(2, 0.0);

    // Three buckets: quiet, loud, quiet. A span over all three has to find the loud one, which
    // is the whole reason spans take extremes instead of averaging or sampling the middle.
    append(peaks, -0.1f, 0.1f);
    append(peaks, -0.9f, 0.8f);
    append(peaks, -0.1f, 0.1f);

    const double threeBuckets = 3 * Waveform::SECONDS_PER_PEAK;
    const auto over = peaks.over(0, 0.0, threeBuckets);

    REQUIRE(over.min == -0.9f);
    REQUIRE(over.max == 0.8f);
    REQUIRE(over.magnitude() == 0.9f);
}

TEST_CASE("WaveformPeaks - a span shorter than a bucket still answers", "[waveform]")
{
    Waveform::Peaks peaks;
    peaks.reset(1, 0.0);
    append(peaks, -0.5f, 0.5f);

    // The near end of the highway asks for slivers of time. Answering nothing there would read
    // as gaps in the waveform where it is largest on screen.
    const auto sliver = peaks.over(0, Waveform::SECONDS_PER_PEAK * 0.25,
                                      Waveform::SECONDS_PER_PEAK * 0.3);
    REQUIRE(sliver.magnitude() == 0.5f);
}

TEST_CASE("WaveformPeaks - past what has been built reads as flat, not as silence", "[waveform]")
{
    Waveform::Peaks peaks;
    peaks.reset(1, 0.0);
    append(peaks, -1.0f, 1.0f);

    REQUIRE(peaks.builtUntil() == Waveform::SECONDS_PER_PEAK);

    const auto beyond = peaks.over(0, 10.0, 11.0);
    REQUIRE(beyond.min == 0.0f);
    REQUIRE(beyond.max == 0.0f);
}

TEST_CASE("WaveformPeaks - buckets are placed against the source's own start time", "[waveform]")
{
    Waveform::Peaks peaks;
    peaks.reset(1, 5.0);   // audio that starts five seconds into the project
    append(peaks, -0.7f, 0.7f);

    REQUIRE(peaks.over(0, 5.0, 5.0 + Waveform::SECONDS_PER_PEAK).magnitude() == 0.7f);
    // Before the source starts there is nothing, and that is not the first bucket.
    REQUIRE(peaks.over(0, 0.0, 1.0).magnitude() == 0.0f);
}

TEST_CASE("WaveformPeaks - identical channels are mono however they are stored", "[waveform]")
{
    Waveform::Peaks peaks;
    peaks.reset(2, 0.0);
    append(peaks, -0.4f, 0.4f);
    append(peaks, -0.6f, 0.6f);

    // Two channels that never differ is mono content in a stereo wrapper, and drawing it split
    // would claim a width that is not there.
    REQUIRE_FALSE(peaks.isStereo());

    appendSplit(peaks, { -0.2f, 0.2f }, { -0.3f, 0.3f });
    REQUIRE(peaks.isStereo());
}

TEST_CASE("WaveformPeaks - a reset drops the old source entirely", "[waveform]")
{
    Waveform::Peaks peaks;
    peaks.reset(2, 0.0);
    appendSplit(peaks, { -0.2f, 0.2f }, { -0.9f, 0.9f });
    REQUIRE(peaks.isStereo());

    peaks.reset(2, 0.0);
    REQUIRE(peaks.isEmpty());
    REQUIRE(peaks.frameCount() == 0);
    // Stereo is a property of the samples, so a new source has to work it out again rather
    // than inherit the last one's answer.
    REQUIRE_FALSE(peaks.isStereo());
}
