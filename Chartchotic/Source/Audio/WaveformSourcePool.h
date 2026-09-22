/*
    ==============================================================================

        WaveformSourcePool.h
        Author: Noah Baxter

        One peak cache per distinct set of audio tracks, so four highways
        reading four stems build four caches and four highways reading the same
        stem build one.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include "../DebugTools/ClipReadLog.h"
#include "ReaperWaveformProvider.h"

/*
    Keyed by the SET, not by the part and not by the track.

    By the part would build the same audio once per part that asked for it, which is the common
    case: everything reads the song track. By the track would mean the summing had to happen
    somewhere downstream of the cache, and summing reduced caches is wrong (max(a) + max(b) is
    not max(a + b)). The set is what a part actually asks for, and two parts asking for the same
    thing get the same answer for free.

    The key is the sorted track list, which PartMapping guarantees by keeping every audio set
    sorted as it is built: two parts that ticked the same tracks in a different order have to
    land on one entry or the work is done twice.

    Message thread only, like the providers it holds. REAPER's audio accessors cannot be created,
    read or destroyed anywhere else.
*/
class WaveformSourcePool
{
public:
    // Spelled out because JUCE_DECLARE_NON_COPYABLE below declares a copy constructor, and any
    // user-declared constructor suppresses the implicit default one.
    WaveformSourcePool() = default;

    using Key = std::vector<int>;

    void initialize(const ReaperAPIs* reaperApis) { apis = reaperApis; }

    /*
        The sets wanted this frame, in the order they should be built.

        Order is priority: tick() spends its budget down the list, so the caller puts the
        highways that are actually on screen first. Without that, four stems share one budget
        round-robin and the one being looked at finishes last.

        A set that stops being requested is KEPT, until the cache is over its size limit and it
        is the least recently wanted thing in it. Dropping on the frame it went unrequested
        meant switching the instrument threw away the audio for the part switched away from,
        and switching back re-read the whole thing: a second of blank board for audio that had
        not changed and was sitting in memory a moment earlier.
    */
    void setRequested(std::vector<Key> sets)
    {
        requested = std::move(sets);
        ++tick_;

        for (const auto& key : requested)
        {
            if (key.empty()) continue;

            auto it = entries.find(key);
            if (it != entries.end()) { it->second.lastWanted = tick_; continue; }

            // Room is made BEFORE the new set is created, not after, so the pool never holds
            // everything at once and then trims back down. Peak use is what runs a machine out
            // of memory, not the steady state.
            evictOverCap();

            // Still no room once everything droppable is gone: the sets in use already fill the
            // budget. Refusing is the only safe answer left, and it degrades to a highway with
            // no waveform rather than to an allocation nobody can service. Said once per set.
            if (cachedBytes() >= MAX_CACHED_BYTES)
            {
                if (refused.insert(key).second)
                    ClipReadLog::audio("waveform cache is full at "
                                       + juce::String(cachedBytes() / (1024 * 1024)) + "MB, "
                                       + ReaperWaveformProvider::describe(key)
                                       + " will not be built");
                continue;
            }

            Entry entry;
            entry.provider = std::make_unique<ReaperWaveformProvider>();
            entry.provider->initialize(apis);
            entry.provider->setSources(key);
            entry.lastWanted = tick_;
            entries.emplace(key, std::move(entry));
            refused.erase(key);
        }

        evictOverCap();
    }

    // The cache for one set, or nullptr when nothing has been built for it. The renderer draws
    // whatever exists, so a half-built cache is a partial waveform rather than no waveform.
    const Waveform::Peaks* peaksFor(const Key& key) const
    {
        auto it = entries.find(key);
        return it == entries.end() ? nullptr : &it->second.provider->getPeaks();
    }

    /*
        One frame's building, shared across every set, in requested order.

        The budget is spent rather than divided: a set that is already complete returns
        immediately and hands what is left to the next one, so the common case of one shared
        song track runs at full speed and only a genuinely multi-stem project pays to split it.
    */
    void tick(double budgetMs)
    {
        if (apis == nullptr) return;

        const double deadline = juce::Time::getMillisecondCounterHiRes() + budgetMs;

        for (const auto& key : requested)
        {
            auto it = entries.find(key);
            if (it == entries.end()) continue;

            const double left = deadline - juce::Time::getMillisecondCounterHiRes();
            if (left <= 0.0) break;

            it->second.provider->tick(left);
        }
    }

    // Whether every requested set has finished building, for the debug readout. Asked of the
    // requested sets only: a set being kept around for a switch back is not something anyone is
    // waiting on.
    bool isComplete() const
    {
        for (const auto& key : requested)
        {
            auto it = entries.find(key);
            if (it != entries.end() && !it->second.provider->isComplete()) return false;
        }

        return true;
    }

    int sourceCount() const { return (int)entries.size(); }

private:
    struct Entry
    {
        std::unique_ptr<ReaperWaveformProvider> provider;
        // When this set was last asked for, so the cap drops the one nobody has wanted longest
        // rather than whichever the map happens to order first.
        unsigned long long lastWanted = 0;
    };

    /*
        How much cache to keep, in BYTES rather than in sets.

        A cache is about 23KB per second of stereo audio, so what a set costs depends entirely
        on how long the audio is: a five minute song is 6.9MB and an hour-long file is 82MB. A
        cap counted in sets is therefore not a cap at all. Six sets is 41MB of four minute songs
        and 494MB of hour-long ones, which is the same number meaning two completely different
        things.

        192MB holds every set a normal session touches (four highways plus a couple of stems
        switched away from, at song length) and still bounds a project full of long files. Past
        the cap the point is memory, not correctness: an evicted set rebuilds when next wanted.
    */
    static constexpr size_t MAX_CACHED_BYTES = 192u * 1024u * 1024u;

    size_t cachedBytes() const
    {
        size_t total = 0;
        for (const auto& [key, entry] : entries)
            total += entry.provider->approxBytes();   // published AND any refresh in flight

        return total;
    }

    void evictOverCap()
    {
        while (cachedBytes() > MAX_CACHED_BYTES)
        {
            auto oldest = entries.end();

            for (auto it = entries.begin(); it != entries.end(); ++it)
            {
                // Never the sets in use this frame, however old: evicting one of those is the
                // blank board this cap exists to prevent.
                if (std::find(requested.begin(), requested.end(), it->first) != requested.end())
                    continue;

                if (oldest == entries.end() || it->second.lastWanted < oldest->second.lastWanted)
                    oldest = it;
            }

            if (oldest == entries.end()) break;   // everything held is in use
            entries.erase(oldest);
        }
    }

    const ReaperAPIs* apis = nullptr;

    // Ordered by key, which is only so lookups are a tree walk rather than a hash of a vector.
    // Iteration order here means nothing; priority lives in requested.
    std::map<Key, Entry> entries;
    std::vector<Key> requested;
    unsigned long long tick_ = 0;

    // Sets that did not fit, so the refusal is logged once rather than every frame for as long
    // as the highway asking for it is on screen.
    std::set<Key> refused;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformSourcePool)
};
