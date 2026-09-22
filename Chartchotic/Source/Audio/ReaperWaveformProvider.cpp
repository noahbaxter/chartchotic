/*
  ==============================================================================

    ReaperWaveformProvider.cpp

  ==============================================================================
*/

#include "ReaperWaveformProvider.h"
#include "../DebugTools/Perf.h"
#include "../DebugTools/ClipReadLog.h"
#include <algorithm>
#include <cmath>

void* ReaperWaveformProvider::resolveTrack(int sourceTrackIndex) const
{
    if (apis == nullptr) return nullptr;

    // Negative is "no track to read", not an error: a project with no audio in it yet resolves
    // to nothing, and there is nothing to open until one arrives.
    if (sourceTrackIndex < 0) return nullptr;

    // Null project: REAPER reads that as the current one, which is how every other call in
    // this plugin asks for it.
    return apis->GetTrack != nullptr ? apis->GetTrack(nullptr, sourceTrackIndex) : nullptr;
}

bool ReaperWaveformProvider::ensureAccessor()
{
    if (!accessors.empty()) return true;
    if (sources.empty()) return false;

    // Each failure gets its own line. "No waveform" has four causes that look identical on
    // screen, and naming which one happened is the whole point of logging here.
    if (apis == nullptr || apis->CreateTrackAudioAccessor == nullptr)
    {
        if (ClipReadLog::changed(loggedFailure, Failure::NoApi))
            ClipReadLog::audio("CreateTrackAudioAccessor not resolved: no accessor API to call");
        return false;
    }

    // All or nothing. A set that opened half its sources would draw a sum that is quietly
    // missing a stem, which looks like a mix rather than like a failure, so a set that cannot
    // open completely opens not at all and says which track refused.
    std::vector<void*> opened;

    for (int sourceTrackIndex : sources)
    {
        void* track = resolveTrack(sourceTrackIndex);
        if (track == nullptr)
        {
            if (ClipReadLog::changed(loggedFailure, Failure::NoTrack))
                ClipReadLog::audio(ClipReadLog::track(sourceTrackIndex)
                                   + " did not resolve to a REAPER track");
            break;
        }

        void* opening = apis->CreateTrackAudioAccessor(track);
        if (opening == nullptr)
        {
            if (ClipReadLog::changed(loggedFailure, Failure::NoAccessor))
                ClipReadLog::audio("REAPER refused an accessor for "
                                   + ClipReadLog::track(sourceTrackIndex));
            break;
        }

        opened.push_back(opening);
    }

    if (opened.size() != sources.size())
    {
        if (apis->DestroyAudioAccessor != nullptr)
            for (void* a : opened)
                apis->DestroyAudioAccessor(a);
        return false;
    }

    accessors = std::move(opened);
    loggedFailure = Failure::None;
    ClipReadLog::audio("accessors opened on " + describe(sources));

    restartBuild();
    return true;
}

void ReaperWaveformProvider::releaseAccessor()
{
    if (apis != nullptr && apis->DestroyAudioAccessor != nullptr)
        for (void* a : accessors)
            apis->DestroyAudioAccessor(a);

    accessors.clear();
    complete = false;
    startTime  = 0.0;
    cursor     = 0.0;
    endTime    = 0.0;
    refreshing = false;
    staging.reset(0, 0.0);
    peaks.reset(0, 0.0);
}

void ReaperWaveformProvider::restartBuild()
{
    // The widest extent any source covers. A sum is only whole where every stem has been read,
    // so the span has to start at the earliest and run to the latest; taking one accessor's
    // extent would cut the sum off wherever that particular stem happened to stop.
    startTime = 0.0;
    double accessorEnd = 0.0;

    for (size_t i = 0; i < accessors.size(); ++i)
    {
        const double thisStart = apis->GetAudioAccessorStartTime != nullptr
                               ? apis->GetAudioAccessorStartTime(accessors[i]) : 0.0;
        const double thisEnd   = apis->GetAudioAccessorEndTime != nullptr
                               ? apis->GetAudioAccessorEndTime(accessors[i]) : 0.0;

        startTime   = i == 0 ? thisStart : juce::jmin(startTime, thisStart);
        accessorEnd = juce::jmax(accessorEnd, thisEnd);
    }

    // The master carries the whole mix but owns no media items, so its accessor reports an
    // empty extent and the build would finish before reading a sample. The project's own
    // length is what there is to read; the accessor's extent still wins where it is longer,
    // since a stem can run past the last marker.
    const double projectEnd =
        apis->GetProjectLength != nullptr ? apis->GetProjectLength(nullptr) : 0.0;

    endTime  = juce::jmax(accessorEnd, projectEnd);

    // Clamped before it reaches reserveSeconds, which allocates the whole span up front.
    const double fullSpan = endTime - startTime;
    const bool truncated = fullSpan > MAX_SOURCE_SECONDS;
    if (truncated) endTime = startTime + MAX_SOURCE_SECONDS;

    cursor        = startTime;
    complete      = endTime <= startTime;
    buildStartMs    = juce::Time::getMillisecondCounterHiRes();
    buildAccessorMs = 0.0;
    loudest       = 0.0f;
    silentChunks  = 0;
    audibleChunks = 0;

    // The accessor's extent and the project's length are logged separately because they
    // disagree in the case that matters: the master reports nothing of its own.
    ClipReadLog::audio("build start on " + describe(sources)
                       + " | accessor " + ClipReadLog::range(startTime, accessorEnd)
                       + " | project length " + ClipReadLog::secs(projectEnd)
                       + " | reading " + ClipReadLog::range(startTime, endTime)
                       + (truncated ? "  TRUNCATED, source runs to "
                                      + ClipReadLog::secs(startTime + fullSpan) : "")
                       + (complete ? "  NOTHING TO READ" : ""));

    // A cache that already holds something is refreshed into staging and swapped when whole, so
    // the waveform on screen does not blank for the second it takes to re-read. With nothing to
    // show yet, the published cache is filled directly and the board fills in as it goes.
    refreshing = !peaks.isEmpty();

    // Always two channels: what the accessor is asked to deliver, not what the track holds.
    // Whether the source is really mono is answered by the samples, in Peaks::isStereo.
    target().reset(Waveform::MAX_CHANNELS, startTime);
    // Guarded: a negative span casts to a vast size_t and reserve would take the process with it.
    if (!complete)
        target().reserveSeconds(endTime - startTime);

    // The poll is a "has it changed since we read it" question, so it starts counting from the
    // read. Left at zero it fires on the first tick and restarts the build it just began.
    lastPollMs = juce::Time::getMillisecondCounterHiRes();

    const size_t chunkSamples = (size_t)(CHUNK_SECONDS * Waveform::READ_SAMPLE_RATE)
                              * (size_t)Waveform::MAX_CHANNELS;
    sampleBuffer.assign(chunkSamples, 0.0);
    mixBuffer.assign(chunkSamples, 0.0);
}

void ReaperWaveformProvider::pollForChanges()
{
    if (apis->AudioAccessorStateChanged == nullptr) return;

    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastPollMs < CHANGE_POLL_MS) return;
    lastPollMs = now;

    // StateChanged only reports; ValidateState is what reloads the accessor. Split that way by
    // REAPER so a caller can decide whether the reload is worth it, and here it always is: the
    // cache describes audio that has moved.
    // Any one source moving invalidates the sum, so the question is asked of all of them and
    // the first yes is enough. Every accessor is still revalidated, because the rebuild reads
    // all of them and a stale one would be summed in as it was.
    const bool moved = std::any_of(accessors.begin(), accessors.end(),
                                   [this](void* a) { return apis->AudioAccessorStateChanged(a); });
    if (!moved) return;

    ClipReadLog::audio("audio under " + describe(sources) + " changed, rebuilding");

    if (apis->AudioAccessorValidateState != nullptr)
        for (void* a : accessors)
            apis->AudioAccessorValidateState(a);

    restartBuild();
}

bool ReaperWaveformProvider::buildChunk()
{
    if (complete || cursor >= endTime) { complete = true; return false; }
    if (apis->GetAudioAccessorSamples == nullptr) { complete = true; return false; }

    constexpr int framesPerChunk = (int)(CHUNK_SECONDS * Waveform::READ_SAMPLE_RATE);
    constexpr int bucketsPerChunk = framesPerChunk / Waveform::SAMPLES_PER_PEAK;

    // Every source read into the same span, added together. Summing SAMPLES is the only correct
    // place to do it: max(a) + max(b) is not max(a + b), so adding reduced caches would
    // overstate every transient, and a transient is the whole content of this view.
    std::fill(mixBuffer.begin(), mixBuffer.end(), 0.0);
    bool silent = true;

    for (void* a : accessors)
    {
        // Timed separately from the reduction below: this is the part a peak-file read would
        // replace, so knowing its share says what switching API would actually buy.
        const double readStart = juce::Time::getMillisecondCounterHiRes();
        const int result = apis->GetAudioAccessorSamples(a, Waveform::READ_SAMPLE_RATE,
                                                         Waveform::MAX_CHANNELS, cursor,
                                                         framesPerChunk, sampleBuffer.data());
        buildAccessorMs += juce::Time::getMillisecondCounterHiRes() - readStart;

        // -1 is the accessor refusing. Stopping is the honest answer: retrying every frame
        // would spend the budget forever on a source that is not going to read.
        if (result < 0)
        {
            ClipReadLog::audio("accessor returned an error at " + ClipReadLog::secs(cursor)
                               + ", stopping the build");
            complete = true;
            return false;
        }

        // 0 means no audio over this span, and the buffer is left UNTOUCHED rather than
        // cleared, so it still holds the previous source's samples. Adding it would double
        // that source into the mix, so a silent read contributes nothing and is skipped.
        if (result == 0) continue;

        silent = false;
        for (size_t s = 0; s < mixBuffer.size(); ++s)
            mixBuffer[s] += sampleBuffer[s];
    }
    if (silent) ++silentChunks;
    else        ++audibleChunks;

    // The last chunk runs past the end of the audio. Trimming it there keeps builtUntil honest:
    // buckets beyond the source would read as silence the renderer draws rather than as nothing.
    const int bucketsLeft = (int)std::ceil((endTime - cursor) * Waveform::PEAKS_PER_SECOND);
    const int bucketsThisChunk = juce::jmin(bucketsPerChunk, juce::jmax(0, bucketsLeft));

    for (int b = 0; b < bucketsThisChunk; ++b)
    {
        Waveform::Peak frame[Waveform::MAX_CHANNELS];

        if (!silent)
        {
            for (int c = 0; c < Waveform::MAX_CHANNELS; ++c)
            {
                float lo = 0.0f, hi = 0.0f;
                double sumSquares = 0.0;
                const size_t base = (size_t)b * Waveform::SAMPLES_PER_PEAK;

                for (int s = 0; s < Waveform::SAMPLES_PER_PEAK; ++s)
                {
                    const float v = (float)mixBuffer[(base + (size_t)s)
                                                         * (size_t)Waveform::MAX_CHANNELS
                                                     + (size_t)c];
                    lo = juce::jmin(lo, v);
                    hi = juce::jmax(hi, v);
                    sumSquares += (double)v * (double)v;
                }

                frame[c] = { lo, hi,
                             (float)std::sqrt(sumSquares / (double)Waveform::SAMPLES_PER_PEAK) };
                loudest = juce::jmax(loudest, frame[c].magnitude());
            }
        }

        target().appendFrame(frame);
    }

    cursor += CHUNK_SECONDS;
    if (cursor >= endTime) complete = true;

    // Whole at last, so the refreshed copy becomes the published one in a single step. Nothing
    // ever sees a partially re-read cache.
    if (complete && refreshing)
    {
        peaks = std::move(staging);
        staging.reset(0, 0.0);
        refreshing = false;
    }

    // Said once each time the window goes from incomplete to covered, rather than once per
    // source: with a ring there is no single moment a source is "done", only a window that has
    // caught up with where the board is looking.
    // "Read nothing" and "read silence" are different faults with the same picture, so the
    // loudest sample found is reported either way. The timings say what a whole-source fill
    // costs and how much of it is REAPER reading rather than our own reduction to buckets.
    if (complete)
        ClipReadLog::audio("build done on " + describe(sources)
                           + " | " + juce::String(peaks.frameCount()) + " buckets covering "
                           + ClipReadLog::range(peaks.firstTime(), peaks.builtUntil())
                           + " | " + juce::String(audibleChunks) + " chunks with audio, "
                           + juce::String(silentChunks) + " silent"
                           + " | wall "
                           + juce::String((juce::Time::getMillisecondCounterHiRes()
                                           - buildStartMs) / 1000.0, 1) + "s"
                           + ", accessor " + juce::String(buildAccessorMs, 0) + "ms"
                           + " | loudest " + juce::String(loudest, 4)
                           + " | " + (peaks.isStereo() ? "stereo" : "mono")
                           + (loudest <= 0.0f ? "   READ ONLY SILENCE" : ""));

    return !complete;
}

void ReaperWaveformProvider::tick(double budgetMs)
{
    if (apis == nullptr) return;

    PERF_SCOPE("tick/waveform");

    if (!ensureAccessor()) return;

    pollForChanges();
    if (complete) return;

    const double deadline = juce::Time::getMillisecondCounterHiRes() + budgetMs;
    while (buildChunk() && juce::Time::getMillisecondCounterHiRes() < deadline)
        ;
}
