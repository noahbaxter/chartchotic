/*
    ==============================================================================

        PhraseMath.h
        Author: Noah Baxter

        The rules behind phrases, kept free of JUCE components so they can be tested
        on their own: which pitches a phrase is written as, where a phrase may end,
        and what [coda] has to do when a BRE is written.

        Design: docs/superpowers/specs/2026-09-12-strip-authoring-modes-design.md
                docs/superpowers/specs/2026-09-15-editor-views-design.md

    ==============================================================================
*/

#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <vector>
#include "../Midi/Utils/InstrumentMapper.h"
#include "../Midi/Utils/GridMath.h"

namespace PhraseMath
{
    // Fill and BRE are the same notes (120-124) told apart only by [coda], but they are not the
    // same thing to author: a fill is a per-part star power activation and charts are full of
    // them, while a BRE is one song-global moment written across every part at once, and is
    // vanishingly rare (one in 44 Rock Band charts). Keeping them separate kinds is what lets the
    // common one stay trivial.
    enum class Kind { StarPower, Solo, Fill, Bre };

    struct PhraseSpan { double startQN, endQN; };

    // The columns a fill lane covers: every hand lane, never the kick. Star power and solo
    // cover the whole board instead, so they have no columns.
    inline std::pair<uint, uint> handColumns(Part part)
    {
        switch (getRenderType(part))
        {
            case RenderType::FOUR_LANE_DRUMS: return { 1, 4 };
            case RenderType::FIVE_LANE_DRUMS: return { 1, 5 };
            case RenderType::ELITE_DRUMS:     return { 1, 8 };
            case RenderType::SIX_FRET:        return { 1, 6 };
            default:                          return { 1, 5 };
        }
    }

    // Whether this part can carry a phrase of a kind at all. Fills are the drum star power
    // activation, so nothing else has one; 6-fret has no BRE notes either.
    inline bool hasKind(Kind kind, Part part)
    {
        switch (kind)
        {
            case Kind::Fill: return isDrumLike(part);
            case Kind::Bre:  return part != Part::GHL_GUITAR && part != Part::GHL_BASS
                                 && part != Part::VOCALS && part != Part::HARMONIES
                                 && part != Part::EVENTS;
            default:         return true;
        }
    }

    // What the phrase view can write on this part, in picker order. Drums get all four;
    // everything else drops the kinds it has no notes for rather than offering them.
    inline std::vector<Kind> kindsFor(Part part)
    {
        std::vector<Kind> out;
        for (Kind k : { Kind::StarPower, Kind::Solo, Kind::Fill, Kind::Bre })
            if (hasKind(k, part)) out.push_back(k);
        return out;
    }

    // Short form for the picker; phraseLabel is the long form the hover says.
    inline const char* phraseShortLabel(Kind kind)
    {
        switch (kind)
        {
            case Kind::StarPower: return "SP";
            case Kind::Solo:      return "solo";
            case Kind::Fill:      return "fill";
            case Kind::Bre:       return "BRE";
        }
        return "";
    }

    inline const char* phraseLabel(Kind kind, Part)
    {
        switch (kind)
        {
            case Kind::StarPower: return "star power";
            case Kind::Solo:      return "solo";
            case Kind::Fill:      return "fill";
            case Kind::Bre:       return "BRE (all parts)";
        }
        return "";
    }

    inline std::vector<int> pitches(Kind kind, Part part)
    {
        using Guitar = MidiPitchDefinitions::Guitar;

        switch (kind)
        {
            case Kind::StarPower: return { InstrumentMapper::starPowerPitch(part) };
            case Kind::Solo:      return { (int)Guitar::SOLO };

            // A fill is a drum phrase, and every drum kind writes the same markers elite aside.
            case Kind::Fill:
                if (!hasKind(Kind::Fill, part)) return {};
                if (part == Part::ELITE_DRUMS)  return { (int)Guitar::BRE_5 };
                return { (int)Guitar::BRE_5, (int)Guitar::BRE_4, (int)Guitar::BRE_3,
                         (int)Guitar::BRE_2, (int)Guitar::BRE_1 };

            case Kind::Bre:
            {
                // How many markers a BRE takes is per part, and getting it wrong writes a phrase
                // no game reads. Elite drums and Pro Keys use 120 alone; Pro Guitar/Bass want six
                // (120-125); the 5-fret family and drums want all five (120-124) on identical
                // spans. 6-fret has none.
                const int first = (int)Guitar::BRE_5;   // 120
                if (!hasKind(Kind::Bre, part)) return {};

                switch (part)
                {
                    case Part::ELITE_DRUMS:
                    case Part::PRO_KEYS:
                        return { first };

                    case Part::PRO_GUITAR:
                    case Part::PRO_BASS:
                        return { first, first + 1, first + 2, first + 3, first + 4, first + 5 };

                    default:
                        return { first, first + 1, first + 2, first + 3, first + 4 };
                }
            }
        }
        return {};
    }

    // Every note on one pitch as a range, sorted by start. Templated over the note type so
    // the cache's notes and a test's plain structs both work.
    template <typename Note>
    std::vector<PhraseSpan> rangesOf(const std::vector<Note>& notes, int pitch)
    {
        std::vector<PhraseSpan> out;
        for (const auto& n : notes)
            if ((int)n.pitch == pitch)
                out.push_back({ n.startPPQ.toDouble(), n.endPPQ.toDouble() });

        std::sort(out.begin(), out.end(),
                  [](const PhraseSpan& a, const PhraseSpan& b) { return a.startQN < b.startQN; });
        return out;
    }

    // The phrase covering `qn`, or -1. Its end is not inside it, so two phrases that touch
    // never both claim the boundary.
    inline int phraseAt(const std::vector<PhraseSpan>& ranges, double qn)
    {
        for (int i = 0; i < (int)ranges.size(); ++i)
            if (!GridMath::before(qn, ranges[(size_t)i].startQN)
                && GridMath::before(qn, ranges[(size_t)i].endQN))
                return i;
        return -1;
    }

    // Whether two spans are the same phrase: overlapping, or touching end to start. Abutting
    // counts, so a drag begun exactly where the last phrase ended extends it.
    inline bool touches(const PhraseSpan& a, const PhraseSpan& b)
    {
        return !GridMath::before(a.endQN, b.startQN) && !GridMath::after(a.startQN, b.endQN);
    }

    // Whether this span covers the whole of [lo, hi], edges counting as covered.
    inline bool contains(const PhraseSpan& outer, double lo, double hi)
    {
        return !GridMath::after(outer.startQN, lo) && !GridMath::before(outer.endQN, hi);
    }

    // Phrases of one kind are a set of ranges, so one drawn over another is not a conflict: it
    // is the same phrase, extended. Returns the union of `range` with everything it touches,
    // which is what gets written in their place.
    inline PhraseSpan mergeSpan(PhraseSpan range, const std::vector<PhraseSpan>& existing)
    {
        // Repeated until stable, because swallowing one span can widen the range into the next.
        for (bool grew = true; grew; )
        {
            grew = false;
            for (const auto& o : existing)
            {
                if (!touches(range, o)) continue;
                if (GridMath::before(o.startQN, range.startQN)) { range.startQN = o.startQN; grew = true; }
                if (GridMath::after(o.endQN, range.endQN))      { range.endQN   = o.endQN;   grew = true; }
            }
        }
        return range;
    }

    // The spans a merged range absorbed, which have to be erased before it is written. Pass the
    // range mergeSpan returned: everything touching that is inside it.
    inline std::vector<PhraseSpan> spansWithin(const PhraseSpan& range,
                                               const std::vector<PhraseSpan>& existing)
    {
        std::vector<PhraseSpan> hit;
        for (const auto& o : existing)
            if (touches(range, o)) hit.push_back(o);
        return hit;
    }

    // A 120 phrase reads as a BRE when the coda covers it. Only reading needs this now: writing a
    // BRE sets the coda to its own start, and writing a fill never touches the coda at all.
    inline bool isBre(double startQN, std::optional<double> codaQN)
    {
        return codaQN.has_value() && !GridMath::before(startQN, *codaQN);
    }
}
