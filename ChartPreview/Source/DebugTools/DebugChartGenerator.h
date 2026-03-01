#pragma once

#ifdef DEBUG

#include "../Utils/Utils.h"

namespace DebugChartGenerator
{

inline SustainEvent makeSustain(PPQ start, PPQ end, uint col, SustainType type, Gem gem = Gem::NOTE, bool sp = false)
{
    SustainEvent s;
    s.startPPQ = start;
    s.endPPQ = end;
    s.gemColumn = col;
    s.sustainType = type;
    s.gemType = GemWrapper(gem, sp);
    return s;
}

inline TrackWindow generateDebugChart(PPQ startPPQ, bool isDrums)
{
    TrackWindow tw;
    auto b = [&](double beat) { return startPPQ + PPQ(beat); };
    auto N  = [](Gem g = Gem::NOTE, bool sp = false) { return GemWrapper(g, sp); };
    auto _  = []() { return GemWrapper(); };

    auto frame = [&](std::initializer_list<std::pair<uint, GemWrapper>> gems) {
        TrackFrame f = {_(), _(), _(), _(), _(), _(), _()};
        for (auto& [col, g] : gems) f[col] = g;
        return f;
    };

    if (isDrums)
    {
        // Drum lanes: 0=kick, 1=red/snare, 2=yellow, 3=blue, 4=green, 6=2xkick
        // Pads use NOTE/HOPO_GHOST/TAP_ACCENT; cymbals use CYM/CYM_GHOST/CYM_ACCENT

        // Individual pads (0-5)
        tw[b(0)] = frame({{0, N()}});
        tw[b(1)] = frame({{1, N()}});
        tw[b(2)] = frame({{2, N()}});
        tw[b(3)] = frame({{3, N()}});
        tw[b(4)] = frame({{4, N()}});
        tw[b(5)] = frame({{6, N()}});

        // Individual cymbals (6-8)
        tw[b(6)] = frame({{2, N(Gem::CYM)}});
        tw[b(7)] = frame({{3, N(Gem::CYM)}});
        tw[b(8)] = frame({{4, N(Gem::CYM)}});

        // All pads — ghost / normal / accent (9-20)
        for (int i = 0; i < 4; ++i)
            tw[b(9 + i)]  = frame({{1, N(Gem::HOPO_GHOST)}, {2, N(Gem::HOPO_GHOST)}, {3, N(Gem::HOPO_GHOST)}, {4, N(Gem::HOPO_GHOST)}});
        for (int i = 0; i < 4; ++i)
            tw[b(13 + i)] = frame({{1, N()}, {2, N()}, {3, N()}, {4, N()}});
        for (int i = 0; i < 4; ++i)
            tw[b(17 + i)] = frame({{1, N(Gem::TAP_ACCENT)}, {2, N(Gem::TAP_ACCENT)}, {3, N(Gem::TAP_ACCENT)}, {4, N(Gem::TAP_ACCENT)}});

        // All cymbals — ghost / normal / accent (21-32)
        for (int i = 0; i < 4; ++i)
            tw[b(21 + i)] = frame({{2, N(Gem::CYM_GHOST)}, {3, N(Gem::CYM_GHOST)}, {4, N(Gem::CYM_GHOST)}});
        for (int i = 0; i < 4; ++i)
            tw[b(25 + i)] = frame({{2, N(Gem::CYM)}, {3, N(Gem::CYM)}, {4, N(Gem::CYM)}});
        for (int i = 0; i < 4; ++i)
            tw[b(29 + i)] = frame({{2, N(Gem::CYM_ACCENT)}, {3, N(Gem::CYM_ACCENT)}, {4, N(Gem::CYM_ACCENT)}});

        // Double bass 16ths (33-36)
        for (int i = 0; i < 16; ++i)
            tw[b(33.0 + i * 0.25)] = frame({{(uint)(i % 2 == 0 ? 0 : 6), N()}});

        // Snare lane with snare notes (37-42)
        for (int i = 0; i < 6; ++i)
            tw[b(37 + i)] = frame({{1, N()}});

        // Yellow cymbal swell with lane (43-48)
        tw[b(43)] = frame({{2, N(Gem::CYM)}});
        tw[b(44)] = frame({{2, N(Gem::CYM)}});
        tw[b(45)]   = frame({{2, N(Gem::CYM)}});
        tw[b(45.5)] = frame({{2, N(Gem::CYM)}});
        for (int i = 0; i < 6; ++i)
            tw[b(46.0 + i * (1.0 / 3.0))] = frame({{2, N(Gem::CYM)}});
        for (int i = 0; i < 4; ++i)
            tw[b(48.0 + i * 0.25)] = frame({{2, N(Gem::CYM)}});

        // Dual cymbal lane alternating blue+green (49-54)
        for (int i = 0; i < 12; ++i)
            tw[b(49.0 + i * 0.5)] = frame({{(uint)(i % 2 == 0 ? 3 : 4), N(Gem::CYM)}});
    }
    else
    {
        // Guitar lanes: 0=open, 1=green, 2=red, 3=yellow, 4=blue, 5=orange

        // Single notes (0-5)
        tw[b(0)] = frame({{0, N()}});
        tw[b(1)] = frame({{1, N()}});
        tw[b(2)] = frame({{2, N()}});
        tw[b(3)] = frame({{3, N()}});
        tw[b(4)] = frame({{4, N()}});
        tw[b(5)] = frame({{5, N()}});

        // All 2-note chords (6-15)
        tw[b(6)]  = frame({{1, N()}, {2, N()}});
        tw[b(7)]  = frame({{1, N()}, {3, N()}});
        tw[b(8)]  = frame({{1, N()}, {4, N()}});
        tw[b(9)]  = frame({{1, N()}, {5, N()}});
        tw[b(10)] = frame({{2, N()}, {3, N()}});
        tw[b(11)] = frame({{2, N()}, {4, N()}});
        tw[b(12)] = frame({{2, N()}, {5, N()}});
        tw[b(13)] = frame({{3, N()}, {4, N()}});
        tw[b(14)] = frame({{3, N()}, {5, N()}});
        tw[b(15)] = frame({{4, N()}, {5, N()}});

        // All 3-note chords (16-25)
        tw[b(16)] = frame({{1, N()}, {2, N()}, {3, N()}});
        tw[b(17)] = frame({{1, N()}, {2, N()}, {4, N()}});
        tw[b(18)] = frame({{1, N()}, {2, N()}, {5, N()}});
        tw[b(19)] = frame({{1, N()}, {3, N()}, {4, N()}});
        tw[b(20)] = frame({{1, N()}, {3, N()}, {5, N()}});
        tw[b(21)] = frame({{1, N()}, {4, N()}, {5, N()}});
        tw[b(22)] = frame({{2, N()}, {3, N()}, {4, N()}});
        tw[b(23)] = frame({{2, N()}, {3, N()}, {5, N()}});
        tw[b(24)] = frame({{2, N()}, {4, N()}, {5, N()}});
        tw[b(25)] = frame({{3, N()}, {4, N()}, {5, N()}});

        // All 4-note chords + 5-note chord (26-31)
        tw[b(26)] = frame({{1, N()}, {2, N()}, {3, N()}, {4, N()}});
        tw[b(27)] = frame({{1, N()}, {2, N()}, {3, N()}, {5, N()}});
        tw[b(28)] = frame({{1, N()}, {2, N()}, {4, N()}, {5, N()}});
        tw[b(29)] = frame({{1, N()}, {3, N()}, {4, N()}, {5, N()}});
        tw[b(30)] = frame({{2, N()}, {3, N()}, {4, N()}, {5, N()}});
        tw[b(31)] = frame({{1, N()}, {2, N()}, {3, N()}, {4, N()}, {5, N()}});

        // Open note chords (32-36)
        tw[b(32)] = frame({{0, N()}, {1, N()}});
        tw[b(33)] = frame({{0, N()}, {2, N()}});
        tw[b(34)] = frame({{0, N()}, {3, N()}});
        tw[b(35)] = frame({{0, N()}, {4, N()}});
        tw[b(36)] = frame({{0, N()}, {5, N()}});

        // Single sustains (37-48, 2 beats each)
        tw[b(37)] = frame({{0, N()}});
        tw[b(39)] = frame({{1, N()}});
        tw[b(41)] = frame({{2, N()}});
        tw[b(43)] = frame({{3, N()}});
        tw[b(45)] = frame({{4, N()}});
        tw[b(47)] = frame({{5, N()}});

        // HOPOs (49-53)
        tw[b(49)] = frame({{1, N(Gem::HOPO_GHOST)}});
        tw[b(50)] = frame({{2, N(Gem::HOPO_GHOST)}});
        tw[b(51)] = frame({{3, N(Gem::HOPO_GHOST)}});
        tw[b(52)] = frame({{4, N(Gem::HOPO_GHOST)}});
        tw[b(53)] = frame({{5, N(Gem::HOPO_GHOST)}});

        // Tap notes (54-58)
        tw[b(54)] = frame({{1, N(Gem::TAP_ACCENT)}});
        tw[b(55)] = frame({{2, N(Gem::TAP_ACCENT)}});
        tw[b(56)] = frame({{3, N(Gem::TAP_ACCENT)}});
        tw[b(57)] = frame({{4, N(Gem::TAP_ACCENT)}});
        tw[b(58)] = frame({{5, N(Gem::TAP_ACCENT)}});

        // Star Power (59-63)
        tw[b(59)] = frame({{1, N(Gem::NOTE, true)}});
        tw[b(60)] = frame({{2, N(Gem::NOTE, true)}});
        tw[b(61)] = frame({{3, N(Gem::NOTE, true)}});
        tw[b(62)] = frame({{4, N(Gem::NOTE, true)}});
        tw[b(63)] = frame({{5, N(Gem::NOTE, true)}});

        // Lanes with notes (64-71)
        tw[b(64)] = frame({{1, N()}});
        tw[b(65)] = frame({{2, N()}});
        tw[b(66)] = frame({{3, N()}});
        tw[b(67)] = frame({{4, N()}});
        tw[b(68)] = frame({{5, N()}});
        tw[b(69)] = frame({{3, N()}});
        tw[b(70)] = frame({{2, N()}});
        tw[b(71)] = frame({{1, N()}});
    }

    return tw;
}

inline SustainWindow generateDebugSustains(PPQ startPPQ, bool isDrums)
{
    SustainWindow sw;
    auto b = [&](double beat) { return startPPQ + PPQ(beat); };

    if (isDrums)
    {
        // Snare lane (37-42)
        sw.push_back(makeSustain(b(37), b(42), 1, SustainType::LANE));
        // Yellow cymbal swell lane (43-48)
        sw.push_back(makeSustain(b(43), b(49), 2, SustainType::LANE));
        // Dual cymbal lane (49-54)
        sw.push_back(makeSustain(b(49), b(55), 3, SustainType::LANE));
        sw.push_back(makeSustain(b(49), b(55), 4, SustainType::LANE));
    }
    else
    {
        // Single sustains (37-48, 2 beats each)
        sw.push_back(makeSustain(b(37), b(39), 0, SustainType::SUSTAIN));
        sw.push_back(makeSustain(b(39), b(41), 1, SustainType::SUSTAIN));
        sw.push_back(makeSustain(b(41), b(43), 2, SustainType::SUSTAIN));
        sw.push_back(makeSustain(b(43), b(45), 3, SustainType::SUSTAIN));
        sw.push_back(makeSustain(b(45), b(47), 4, SustainType::SUSTAIN));
        sw.push_back(makeSustain(b(47), b(49), 5, SustainType::SUSTAIN));

        // Lane markers (64-71)
        sw.push_back(makeSustain(b(64), b(68), 1, SustainType::LANE));
        sw.push_back(makeSustain(b(64), b(68), 2, SustainType::LANE));
        sw.push_back(makeSustain(b(64), b(68), 3, SustainType::LANE));
        sw.push_back(makeSustain(b(68), b(71), 4, SustainType::LANE));
        sw.push_back(makeSustain(b(68), b(71), 5, SustainType::LANE));
    }

    return sw;
}

} // namespace DebugChartGenerator

#endif
