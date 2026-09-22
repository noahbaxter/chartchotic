/*
    ==============================================================================

        ReaperWaveformProvider.h
        Author: Noah Baxter

        Builds a track's peak cache from a REAPER audio accessor, a slice at a
        time, and rebuilds it when the audio underneath moves.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include "WaveformPeaks.h"
#include "../Midi/Providers/REAPER/ReaperApiHelpers.h"
#include "../DebugTools/ClipReadLog.h"

/*
    Accessors are main thread only: create, sample and destroy all have to happen on the
    message thread. So the build runs there too, in slices with a time budget, rather than on
    a worker that would be illegal or a single pass that would stall a frame. What crosses to
    the renderer is the finished cache, never the accessor.

    The renderer never asks this class for samples. It reads Peaks, which is already built.
*/
class ReaperWaveformProvider
{
public:
    ~ReaperWaveformProvider() { releaseAccessor(); }

    void initialize(const ReaperAPIs* reaperApis) { apis = reaperApis; }

    /*
        Which tracks to read, summed into one waveform. Empty is "nothing to read", which is
        what a project with no audio in it looks like and is not an error.

        Several tracks give ONE shape, not several drawn over each other: overlapping waveforms
        about a shared centre line are mush, and stacking them is the only version of this with
        a per-frame cost. Summed, the render draws exactly what it drew for one track.

        A new set drops the accessors and the cache with them; there is nothing in either worth
        keeping when the audio they describe is different audio.
    */
    void setSources(std::vector<int> sourceTrackIndices)
    {
        if (sourceTrackIndices == sources) return;

        ClipReadLog::audio("sources changed from " + describe(sources)
                           + " to " + describe(sourceTrackIndices));

        sources = std::move(sourceTrackIndices);
        loggedFailure = Failure::None;
        releaseAccessor();
    }

    const std::vector<int>& getSources() const { return sources; }

    static juce::String describe(const std::vector<int>& set)
    {
        if (set.empty()) return "nothing";

        juce::String out;
        for (size_t i = 0; i < set.size(); ++i)
            out << (i > 0 ? " + " : "") << ClipReadLog::track(set[i]);
        return out;
    }

    // One frame's worth of building, bounded by budgetMs. Main thread only. Cheap and early-out
    // when the cache is already complete, so it is safe to call every frame.
    void tick(double budgetMs);

    const Waveform::Peaks& getPeaks() const { return peaks; }

    // Everything this provider is holding, which during a refresh is TWO caches: the one still
    // being drawn and the one being filled to replace it. A pool budgeting by the published
    // cache alone would miss the moment of peak use, which is exactly the moment that matters.
    // Everything this provider is holding, which during a refresh is TWO caches: the one still
    // being drawn and the one being filled to replace it. A pool budgeting by the published
    // cache alone would miss the moment of peak use, which is exactly the moment that matters.
    size_t approxBytes() const { return peaks.approxBytes() + staging.approxBytes(); }

    // Whether the cache covers the whole source yet. The renderer draws what exists either
    // way; this is for the debug readout.
    bool isComplete() const { return complete; }

private:
    // How much audio one read asks for. Small on purpose: the budget is only checked between
    // chunks, so a chunk is the longest the build can overrun by, and reading the master means
    // REAPER rendering that much of the mix on the message thread.
    static constexpr double CHUNK_SECONDS = 0.25;

    // How often to ask whether the audio moved. Every frame would be a needless API call for
    // an answer that changes when someone edits, not when someone looks.
    static constexpr double CHANGE_POLL_MS = 250.0;

    /*
        The longest span a single source will ever be read over, and the one hard stop between
        this class and someone else's memory.

        The span comes from GetProjectLength, so it is whatever the project happens to hold: one
        item dragged out to three hours makes the reserve a quarter of a gigabyte, in a single
        allocation, before any pool cap gets a say. That is not a caching decision, it is a
        stray item in someone's project turning into an out-of-memory.

        Thirty minutes is about 41MB per set. Past it the tail is not read and the waveform
        stops there, which is visible and logged rather than silent.
    */
    static constexpr double MAX_SOURCE_SECONDS = 30.0 * 60.0;

    void* resolveTrack(int sourceTrackIndex) const;
    bool  ensureAccessor();
    void  releaseAccessor();
    void  restartBuild();
    void  pollForChanges();
    // Reads one chunk and reduces it into the cache. False when there is nothing left to read.
    bool  buildChunk();

    const ReaperAPIs* apis = nullptr;

    // Empty until someone names a track, which is also what "this project has no audio" looks
    // like. Either way there is nothing to open. One accessor per source, in the same order.
    std::vector<int>   sources;
    std::vector<void*> accessors;

    /*
        The published cache, and the one being filled when a rebuild is refreshing a cache that
        already exists.

        A rebuild used to reset the published cache and refill it, so any change at all blanked
        the waveform and it crawled back up the board over the next second. Nothing about the
        audio was unknown during that second; the data was thrown away and re-read.

        So a refresh fills staging and swaps only when it is whole. The FIRST build for a source
        still fills the published cache directly, because filling in from the bottom is the
        right thing to show when there is genuinely nothing yet.

        getPeaks() hands out a reference to the member, which the swap does not invalidate: the
        object stays, its contents change. Both live on the message thread, so a frame cannot
        read a half-swapped cache.
    */
    Waveform::Peaks peaks;
    Waveform::Peaks staging;
    bool refreshing = false;

    Waveform::Peaks& target() { return refreshing ? staging : peaks; }

    double startTime = 0.0;
    double cursor    = 0.0;   // where the build has read up to, in project time
    double endTime   = 0.0;
    bool   complete  = false;
    double lastPollMs = 0.0;

    // Reused across chunks: interleaved doubles, sized once for a full chunk. sampleBuffer
    // takes one source's read; mixBuffer accumulates them.
    //
    // Summed as SAMPLES, before the reduction to buckets, which is the only correct place for
    // it: max(a) + max(b) is not max(a + b), so adding finished caches together would overstate
    // every transient, and transients are the whole content of this view.
    std::vector<double> sampleBuffer;
    std::vector<double> mixBuffer;

    // Diagnostics, read by ClipReadLog. The counters separate "read nothing" from "read
    // silence", which look the same on the highway and have different causes.
    float loudest       = 0.0f;
    int   silentChunks  = 0;
    int   audibleChunks = 0;

    // How long the whole build took and how much of that was spent inside REAPER actually
    // reading audio, so the cost of filling a source is a number rather than a guess.
    double buildStartMs    = 0.0;
    double buildAccessorMs = 0.0;

    // Which failure has already been reported, so a source that cannot open says so once
    // rather than every frame for as long as the layer is on.
    enum class Failure { None, NoApi, NoTrack, NoAccessor };
    Failure loggedFailure = Failure::None;
};
