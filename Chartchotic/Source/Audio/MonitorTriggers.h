#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../Midi/Utils/InstrumentMapper.h"
#include "../Midi/Utils/GridMath.h"
#include "../Visual/Managers/GridlineGenerator.h"

// What the monitor plays and when, worked out off the audio thread from the chart itself. The
// audio thread only ever walks these lists, so everything that needs the parts, the skill or the
// tempo map happens here.
enum class MonitorSample : uint8_t
{
    Bar, Beat, GuitarClick,
    Kick, Snare, HatClosed, HatOpen, Tom1, Tom2, Tom3, Crash, Ride, RideBell,
    Count
};

struct MonitorTrigger
{
    double qn;
    MonitorSample sample;
    float gain;
};

// Kept apart so each toggle silences its own list without a rebuild.
struct MonitorTriggers
{
    std::vector<MonitorTrigger> click;
    std::vector<MonitorTrigger> notes;
};

namespace MonitorTriggerBuilder
{
    // What the InstrumentMapper column lookups return for a pitch outside the lane set; its own
    // constant is private.
    constexpr uint kNoColumn = uint(-1);

    // What a velocity means, under the same rule the highway draws it by: ghost and accent only
    // exist with chart dynamics on, and a 4-lane kick never has them.
    inline Dynamic dynamicOf(unsigned velocity, bool dynamics)
    {
        if (!dynamics) return Dynamic::NONE;
        if (velocity == (unsigned)Dynamic::GHOST)  return Dynamic::GHOST;
        if (velocity == (unsigned)Dynamic::ACCENT) return Dynamic::ACCENT;
        return Dynamic::NONE;
    }

    // About 7 dB either side of a normal hit, wide enough to hear on a single sample.
    inline float drumGain(Dynamic dynamic)
    {
        if (dynamic == Dynamic::GHOST)  return 0.27f;
        if (dynamic == Dynamic::ACCENT) return 1.0f;
        return 0.6f;
    }

    // An accent on the hat or the ride is played as the articulation a drummer would reach for:
    // the hat opens and the ride goes to the bell.
    inline MonitorSample accented(MonitorSample sample, Dynamic dynamic)
    {
        if (dynamic != Dynamic::ACCENT) return sample;
        if (sample == MonitorSample::HatClosed) return MonitorSample::HatOpen;
        if (sample == MonitorSample::Ride)      return MonitorSample::RideBell;
        return sample;
    }

    // Half-open spans, the way the resolver tests its modifier ranges.
    struct Span { double start, end; };
    inline bool covers(const std::vector<Span>& spans, double qn)
    {
        for (const auto& s : spans)
            if (qn >= s.start && qn < s.end) return true;
        return false;
    }

    // One hit per sample per moment: a flam's two kicks or a chord's frets sound once.
    inline void sortAndDedupe(std::vector<MonitorTrigger>& out)
    {
        std::stable_sort(out.begin(), out.end(), [](const MonitorTrigger& a, const MonitorTrigger& b)
        {
            if (!GridMath::same(a.qn, b.qn)) return a.qn < b.qn;
            return (int)a.sample < (int)b.sample;
        });
        out.erase(std::unique(out.begin(), out.end(), [](const MonitorTrigger& a, const MonitorTrigger& b)
        {
            return a.sample == b.sample && GridMath::same(a.qn, b.qn);
        }), out.end());
    }

    // `Notes` is any list of notes with startPPQ, endPPQ, pitch, velocity and muted, which is
    // the session's cache in the plugin and a plain struct in the tests.
    template <typename Notes>
    std::vector<MonitorTrigger> guitar(const Notes& notes, SkillLevel skill)
    {
        std::vector<MonitorTrigger> out;
        for (const auto& n : notes)
            if (!n.muted && InstrumentMapper::getGuitarColumn(n.pitch, skill) != kNoColumn)
                out.push_back({ n.startPPQ.toDouble(), MonitorSample::GuitarClick, 0.8f });
        sortAndDedupe(out);
        return out;
    }

    // 4-lane: kick, snare, then yellow/blue/green as hat/ride/crash, or as toms 1-3 under a tom
    // marker. Without pro drums there are no cymbals, so every pad is a tom.
    template <typename Notes>
    std::vector<MonitorTrigger> drums(const Notes& notes, SkillLevel skill, bool kick2x, bool proDrums,
                                      bool dynamics)
    {
        // Tom markers are spans, so a gem is a tom when its color's marker covers it.
        std::vector<Span> toms[3];
        for (const auto& n : notes)
        {
            if (n.muted) continue;
            int color = (int)n.pitch - (int)MidiPitchDefinitions::Drums::TOM_YELLOW;
            if (color >= 0 && color < 3)
                toms[color].push_back({ n.startPPQ.toDouble(), n.endPPQ.toDouble() });
        }

        std::vector<MonitorTrigger> out;
        for (const auto& n : notes)
        {
            if (n.muted) continue;
            const uint col = InstrumentMapper::getDrumColumn(n.pitch, skill, kick2x);
            if (col == kNoColumn) continue;

            const double qn = n.startPPQ.toDouble();
            if (isDrumKick(col, Part::DRUMS))
            {
                out.push_back({ qn, MonitorSample::Kick, drumGain(Dynamic::NONE) });
                continue;
            }

            const Dynamic dynamic = dynamicOf(n.velocity, dynamics);
            MonitorSample sample;
            if (col == 1)
                sample = MonitorSample::Snare;
            else
            {
                const int color = (int)col - 2;
                static constexpr MonitorSample tom[] = { MonitorSample::Tom1, MonitorSample::Tom2, MonitorSample::Tom3 };
                static constexpr MonitorSample cym[] = { MonitorSample::HatClosed, MonitorSample::Ride, MonitorSample::Crash };
                if (color < 0 || color > 2) continue;
                sample = (!proDrums || covers(toms[color], qn)) ? tom[color] : accented(cym[color], dynamic);
            }
            out.push_back({ qn, sample, drumGain(dynamic) });
        }
        sortAndDedupe(out);
        return out;
    }

    // Elite lanes are fixed instruments. The hi-hat is open unless a pedal-down or indifferent
    // span covers it, which is how the format defines it: a held pedal closes every hat under it.
    // The hat's state comes from those spans, so an accent only opens the ride to its bell here.
    template <typename Notes>
    std::vector<MonitorTrigger> eliteDrums(const Notes& notes, SkillLevel skill, bool kick2x, bool dynamics)
    {
        const int skillIndex = (int)skill - 1;
        std::vector<Span> closedSpans;
        for (const auto& n : notes)
            if (!n.muted && (InstrumentMapper::eliteHiHatPedalSkillIndex(n.pitch) == skillIndex
                          || InstrumentMapper::eliteHiHatIndifferentSkillIndex(n.pitch) == skillIndex))
                closedSpans.push_back({ n.startPPQ.toDouble(), n.endPPQ.toDouble() });

        const auto pitches = InstrumentMapper::getEliteDrumPitchesForSkill(skill);
        std::vector<MonitorTrigger> out;
        for (const auto& n : notes)
        {
            if (n.muted) continue;
            if (std::find(pitches.begin(), pitches.end(), n.pitch) == pitches.end()) continue;
            const uint col = InstrumentMapper::getEliteDrumColumn(n.pitch, skill, kick2x);
            if (col == kNoColumn) continue;

            const double qn = n.startPPQ.toDouble();
            const Dynamic dynamic = dynamicOf(n.velocity, dynamics);
            MonitorSample sample;
            switch (col)
            {
                case 1:  sample = MonitorSample::Snare; break;
                case 2:  sample = covers(closedSpans, qn) ? MonitorSample::HatClosed : MonitorSample::HatOpen; break;
                case 3:  sample = MonitorSample::Crash; break;
                case 4:  sample = MonitorSample::Tom1;  break;
                case 5:  sample = MonitorSample::Tom2;  break;
                case 6:  sample = MonitorSample::Tom3;  break;
                case 7:  sample = accented(MonitorSample::Ride, dynamic); break;
                case 8:  sample = MonitorSample::Crash; break;
                default: sample = MonitorSample::Kick;  break;   // 0 and the virtual 2x column
            }
            out.push_back({ qn, sample, drumGain(dynamic) });
        }
        sortAndDedupe(out);
        return out;
    }

    // A bar click on every measure line and a beat click on every beat line, off the same
    // generator the highway draws them with, so the click never disagrees with the grid.
    inline std::vector<MonitorTrigger> metronome(const TempoTimeSignatureMap& map, double startQN, double endQN)
    {
        std::vector<MonitorTrigger> out;
        if (endQN <= startQN) return out;

        auto identity = [](double ppq) { return ppq; };
        for (const auto& line : GridlineGenerator::generateGridlines(map, PPQ(startQN), PPQ(endQN), PPQ(0.0), identity))
        {
            if (line.type == Gridline::MEASURE)   out.push_back({ line.time, MonitorSample::Bar,  0.9f });
            else if (line.type == Gridline::BEAT) out.push_back({ line.time, MonitorSample::Beat, 0.6f });
        }
        sortAndDedupe(out);
        return out;
    }
}

// Which stretch of the timeline a block should fire. A block's end is projected from the tempo
// at its start, so across a tempo change it lands past or short of where the host starts the
// next block. Firing [start, projected end) per block then plays a hit near the boundary in both
// blocks, or in neither. Carrying on from where the last block stopped makes playback
// contiguous, and anything that is not a small step from there is a seek or a loop.
struct MonitorFireWindow
{
    double lastEnd = 0.0;
    bool   running = false;

    // Returns where to start firing; `end` is the block's projected end.
    double begin(double start, double end)
    {
        const double length = end - start;
        const bool contiguous = running && std::abs(start - lastEnd) <= 4.0 * length;
        const double from = contiguous ? lastEnd : start;
        lastEnd = std::max(from, end);
        running = true;
        return from;
    }

    void stop() { running = false; }
};
