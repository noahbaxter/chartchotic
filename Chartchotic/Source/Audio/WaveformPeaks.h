/*
    ==============================================================================

        WaveformPeaks.h
        Author: Noah Baxter

        A track's audio reduced to min/max buckets on a fixed time grid, and the
        span query the renderer reads it through.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

namespace Waveform
{
    // How many min/max buckets one second of audio reduces to. THIS is the ceiling on detail,
    // not the row count: drawing more rows than there are buckets in view just draws the same
    // bucket twice. At 1000 a bucket is 1ms, so a two-second window holds 2000 of them and the
    // renderer runs out of rows before the cache runs out of answers, which is the right way
    // round. 400 was tried first and the attacks stayed blunt no matter how high Detail went.
    //
    // The cost is about 16KB per second of audio, so ten megabytes for a long song, paid once
    // on a background of frames that never touch it again.
    inline constexpr int PEAKS_PER_SECOND = 1000;

    inline constexpr double SECONDS_PER_PEAK = 1.0 / (double)PEAKS_PER_SECOND;

    // Stereo splits left and right; anything wider folds down to two. Mono draws its one
    // channel mirrored, so both sides still read as a waveform.
    inline constexpr int MAX_CHANNELS = 2;

    // What the accessor is asked for. REAPER resamples to whatever rate it is handed, so a
    // lower one would cost less and lose exactly the transient peaks this view exists to show.
    // 48k over 44.1k for one reason: it divides into whole samples per bucket, and a fractional
    // bucket boundary is a drifting one.
    inline constexpr int READ_SAMPLE_RATE = 48000;
    inline constexpr int SAMPLES_PER_PEAK = READ_SAMPLE_RATE / PEAKS_PER_SECOND;
    static_assert(READ_SAMPLE_RATE % PEAKS_PER_SECOND == 0,
                  "a bucket has to be a whole number of samples or its edges drift");

    // One bucket's extremes. Both sides are kept rather than a single magnitude because an
    // asymmetric waveform is a real thing to see, and a kick's shape is half the point.
    /*
        Amplitude to drawn width. LINEAR, the way every audio editor draws a waveform.

        A dB scale was tried here and it was the wrong instinct. Rescaling dB across the floor
        inflates everything in the middle: against a -48dB floor an amplitude of 0.05 draws at
        46% width and 0.5 draws at 87%, so the whole mix crowds the edges and a transient has
        nowhere left to stand out. The shape stops being a waveform and becomes a bar.

        So the floor is a GATE, not a rescale. Below it draws nothing, which clears the noise
        the quiet passages are full of; above it the amplitude is linear, and the rescale by
        the remaining range keeps the two sides continuous at the boundary.

        curve then bends what survives: above 1 the quiet end shrinks faster than the loud end,
        steepening a hit against the mix underneath it without touching the linear feel.
    */
    inline float shapeMagnitude(float magnitude, float floorDb, float curve)
    {
        if (magnitude <= 0.0f || floorDb >= 0.0f) return 0.0f;

        const float gate = juce::Decibels::decibelsToGain(floorDb);
        if (magnitude <= gate) return 0.0f;

        const float linear = juce::jlimit(0.0f, 1.0f, (magnitude - gate) / (1.0f - gate));

        return curve == 1.0f ? linear : std::pow(linear, curve);
    }

    /*
        A bucket's extremes AND its energy, because the two answer different questions and a
        waveform needs both.

        Peak alone is what makes a fine-resolution waveform look stringy: the loudest sample in
        one millisecond has almost nothing to do with the loudest sample in the next, so the
        outline whips in and out and the shape reads as hair rather than as audio. Smoothing
        hides that by averaging the peaks, which also rounds off the transients you wanted.

        RMS is the honest fix. It says how much energy the bucket holds rather than how far its
        single worst sample reached, so it moves like the music does: steady through a held
        note, jumping at an attack. Drawn as the body with the peak as the outline, the shape
        has both a readable mass and its true extremes, which is how every audio editor draws
        one and why none of them need a smoothing control.
    */
    struct Peak
    {
        float min = 0.0f;
        float max = 0.0f;
        float rms = 0.0f;

        void merge(const Peak& other)
        {
            min = juce::jmin(min, other.min);
            max = juce::jmax(max, other.max);
            // Not correct across differing bucket counts, which is why a span's RMS is summed
            // properly in over() instead of folded pairwise through here.
            rms = juce::jmax(rms, other.rms);
        }

        float magnitude() const { return juce::jmax(std::abs(min), std::abs(max)); }
    };

    /*
        The cache the renderer draws from. Built once per source and read every frame, so the
        read path is an index calculation and a bounded loop, never a fetch.

        Time is the project timeline, starting at the accessor's own start time: buckets are a
        fixed grid from there, so a bucket's index is arithmetic rather than a search.

        A windowed RING over just the visible span was tried and reverted on 2026-09-22. The
        arithmetic for it was right (a five minute song reads 300 seconds to draw two) and the
        result was still much worse: moving backwards always lands before the held run, which
        threw the ring away and re-read it, every frame, and a seek allowance that let those
        refills overrun the frame budget turned that into lag you could not chart through.

        Holding the whole source costs a background fill nobody watches and then never costs
        anything again, which is the behaviour that actually works. Do not re-litigate this
        without a profile showing the fill is a problem in practice rather than on paper.
    */
    class Peaks
    {
    public:
        // Start over for a new source. Channels above MAX_CHANNELS fold down to it; the
        // reader only ever draws two sides.
        void reset(int sourceChannels, double sourceStartTime)
        {
            channels = juce::jlimit(1, MAX_CHANNELS, sourceChannels);
            startTime = sourceStartTime;
            stereo = false;
            buckets.clear();
        }

        // One time slice, one Peak per channel, appended in channel order. Called by the
        // builder as it walks the source; nothing else writes here.
        void appendFrame(const Peak* perChannel)
        {
            // A stereo file whose two channels are identical is mono content in a stereo
            // wrapper, and drawing it as a split waveform claims a width that is not there.
            // Asked of the samples rather than of the track's channel count, which only says
            // how the audio is stored.
            if (channels > 1 && !stereo
                && (perChannel[0].min != perChannel[1].min || perChannel[0].max != perChannel[1].max))
                stereo = true;

            for (int c = 0; c < channels; ++c)
                buckets.push_back(perChannel[c]);
        }

        void reserveSeconds(double seconds)
        {
            buckets.reserve((size_t)std::ceil(seconds * PEAKS_PER_SECOND) * (size_t)channels);
        }

        int  channelCount() const { return channels; }

        // Whether the two channels ever differ. False for a mono source and for stereo that
        // happens to be identical on both. The drawing merges the channels either way, so this
        // is reported rather than acted on: it says what was read, which is worth knowing when
        // a source looks wrong.
        bool isStereo() const { return stereo; }

        bool isEmpty()    const { return buckets.empty(); }
        int  frameCount() const { return channels > 0 ? (int)(buckets.size() / (size_t)channels) : 0; }

        double firstTime() const { return startTime; }

        // Where the build has got to. The renderer draws up to here and leaves the rest blank
        // rather than drawing a silent tail that only means "not read yet".
        double builtUntil() const { return startTime + frameCount() * SECONDS_PER_PEAK; }

        // What this cache costs, for a pool that has to bound itself.
        size_t approxBytes() const { return buckets.capacity() * sizeof(Peak); }

        // The extreme across a span of time, which is what a scanline covering many buckets
        // needs. A span shorter than one bucket still answers with the bucket it lands in, so
        // the near end of the highway never reads as gaps.
        Peak over(int channel, double spanStart, double spanEnd) const
        {
            Peak result;
            if (channels <= 0 || buckets.empty()) return result;
            if (channel < 0 || channel >= channels) return result;

            if (spanEnd < spanStart) std::swap(spanStart, spanEnd);

            const int last  = frameCount() - 1;
            int firstBucket = bucketFor(spanStart);
            int lastBucket  = bucketFor(spanEnd);

            // Wholly outside what has been built: nothing to say, and a flat Peak says it.
            if (lastBucket < 0 || firstBucket > last) return result;

            firstBucket = juce::jmax(0, firstBucket);
            lastBucket  = juce::jmin(last, lastBucket);

            // Extremes take the widest of the span; energy is averaged over it. Averaging the
            // squares rather than the RMS values is the difference between an honest answer
            // and one that drifts with how the span happened to be divided up.
            double sumSquares = 0.0;

            for (int i = firstBucket; i <= lastBucket; ++i)
            {
                const auto& bucket = buckets[(size_t)i * (size_t)channels + (size_t)channel];
                result.min = juce::jmin(result.min, bucket.min);
                result.max = juce::jmax(result.max, bucket.max);
                sumSquares += (double)bucket.rms * (double)bucket.rms;
            }

            const int count = lastBucket - firstBucket + 1;
            result.rms = count > 0 ? (float)std::sqrt(sumSquares / (double)count) : 0.0f;

            return result;
        }

    private:
        // Floor rather than round: a bucket covers [t, t + SECONDS_PER_PEAK), so the time at
        // its start belongs to it. Can return out of range; over() clamps.
        int bucketFor(double time) const
        {
            return (int)std::floor((time - startTime) * PEAKS_PER_SECOND);
        }

        std::vector<Peak> buckets;   // frame-major, channels consecutive within a frame
        int    channels    = 0;
        double startTime   = 0.0;
        bool   stereo      = false;
    };
}
