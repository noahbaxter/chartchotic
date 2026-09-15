/*
    ==============================================================================

        StripPhraseMath.h
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
#include <optional>
#include <vector>
#include "../Midi/Utils/InstrumentMapper.h"
#include "../Midi/Utils/GridMath.h"

namespace StripPhraseMath
{
    enum class Kind { StarPower, Solo, FillBre };

    struct PhraseSpan { double startQN, endQN; };

    inline std::vector<int> pitches(Kind kind, Part part)
    {
        using Guitar = MidiPitchDefinitions::Guitar;

        switch (kind)
        {
            case Kind::StarPower: return { InstrumentMapper::starPowerPitch(part) };
            case Kind::Solo:      return { (int)Guitar::SOLO };
            case Kind::FillBre:
                // Elite reads a fill off 120 alone. Everywhere else the format wants all five
                // markers on identical spans, and a phrase missing one is not a phrase.
                if (part == Part::ELITE_DRUMS) return { (int)Guitar::BRE_5 };
                return { (int)Guitar::BRE_5, (int)Guitar::BRE_4, (int)Guitar::BRE_3,
                         (int)Guitar::BRE_2, (int)Guitar::BRE_1 };
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

    // Phrases in one column may not overlap, so an end dragged past the next phrase stops at
    // its start rather than running into it.
    inline double clampEnd(double startQN, double wantEndQN, const std::vector<PhraseSpan>& others)
    {
        double end = wantEndQN;
        for (const auto& o : others)
            if (GridMath::after(o.startQN, startQN))
                end = std::min(end, o.startQN);
        return end;
    }

    // Fill or BRE is decided entirely by where the coda is.
    inline bool isBre(double startQN, std::optional<double> codaQN)
    {
        return codaQN.has_value() && !GridMath::before(startQN, *codaQN);
    }

    enum class CodaChange { Keep, Place, Refuse };

    struct FillBrePlan
    {
        bool bre = false;
        CodaChange coda = CodaChange::Keep;
        double codaQN = 0.0;
        // Drum fills the new coda would turn into BREs. Any at all refuses the write.
        int reinterpreted = 0;
    };

    // What writing a fill/BRE phrase at `startQN` means for the coda. On drums the position
    // decides, unless a BRE was asked for outright. Everywhere else 120-124 can only be a BRE,
    // so the coda has to sit at or before it, and moving the coda there must not silently
    // reinterpret a drum fill that is already charted.
    inline FillBrePlan planFillBre(double startQN, Part part, bool wantBre,
                                   std::optional<double> codaQN,
                                   const std::vector<PhraseSpan>& drumFills)
    {
        FillBrePlan plan;

        if (isDrumLike(part) && !wantBre)
        {
            plan.bre = isBre(startQN, codaQN);
            return plan;
        }

        plan.bre = true;
        if (isBre(startQN, codaQN)) return plan;

        // Only fills that are not already BREs change meaning: the ones from here up to the
        // old coda, or to the end of the song when there was none.
        for (const auto& f : drumFills)
            if (!GridMath::before(f.startQN, startQN) && !isBre(f.startQN, codaQN))
                ++plan.reinterpreted;

        plan.coda = plan.reinterpreted > 0 ? CodaChange::Refuse : CodaChange::Place;
        plan.codaQN = startQN;
        return plan;
    }

    // A coda with no BRE after it is a trap: the next drum fill charted past it silently
    // becomes a BRE. `fillBre` is every 120 range left on every part.
    inline bool codaOrphaned(std::optional<double> codaQN, const std::vector<PhraseSpan>& fillBre)
    {
        if (!codaQN.has_value()) return false;
        for (const auto& r : fillBre)
            if (isBre(r.startQN, codaQN)) return false;
        return true;
    }
}
