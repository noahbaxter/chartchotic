/*
    ==============================================================================

        FrameProfileLogger.h
        Author: Noah Baxter

        Debug-only frame profiler that logs per-frame timing data to a TSV file.
        Pre-allocated ring buffer, batch-writes every 60 frames (~1 sec).
        Flags spike frames (>12ms absolute or >2x rolling average).

    ==============================================================================
*/

#pragma once

#ifdef DEBUG

#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include "../Visual/Utils/RenderTiming.h"
#include "../Visual/Utils/DrawingConstants.h"
#include "../Utils/Utils.h"

static constexpr int MAX_PROFILED_HIGHWAYS = 4;

struct HighwayProfileRecord
{
    int paint_us = 0;
    int scene_us = 0;
    int notes_us = 0;
    int sustains_us = 0;
    int execute_us = 0;
    int track_us = 0;
};

struct FrameProfileRecord
{
    int frameNumber = 0;
    int frameDelta_us = 0;
    int onFrame_us = 0;
    int dataBuild_us = 0;
    int lockWait_us = 0;
    int trackRender_us = 0;
    int textureRender_us = 0;
    int highwayPaint_us = 0;
    int sceneTotal_us = 0;
    int notes_us = 0;
    int sustains_us = 0;
    int gridlines_us = 0;
    int animation_us = 0;
    int execute_us = 0;
    int layer_us[DRAW_ORDER_COUNT] = {};
    int trackWindowSize = 0;
    int sustainWindowSize = 0;
    int gridlineCount = 0;
    int slotCount = 0;
    int activePart = 0;
    int skillLevel = 0;
    int viewportW = 0;
    int viewportH = 0;
    int isPlaying = 0;
    int isSpike = 0;

    // Multi-highway aggregate + per-highway breakdown
    int aggPaint_us = 0;
    int aggScene_us = 0;
    int aggNotes_us = 0;
    int aggSustains_us = 0;
    int aggExecute_us = 0;
    int aggTrack_us = 0;
    int hwCount = 0;
    HighwayProfileRecord hw[MAX_PROFILED_HIGHWAYS] = {};
};

class FrameProfileLogger
{
public:
    static constexpr int SPIKE_ABSOLUTE_US = 12000;
    static constexpr double SPIKE_RELATIVE_FACTOR = 2.0;
    static constexpr int ROLLING_WINDOW = 60;

    void start()
    {
        // Resolve repo root from __FILE__ (this file is in Chartchotic/Source/DebugTools/)
        const char* thisFile = __FILE__;
        std::string path(thisFile);
        // Walk up 4 levels: DebugTools/ -> Source/ -> Chartchotic/ -> repo root
        for (int i = 0; i < 4; ++i)
        {
            auto pos = path.find_last_of('/');
            if (pos == std::string::npos) break;
            path = path.substr(0, pos);
        }
        path += "/logs";

        // Ensure logs/ directory exists
        std::string mkdirCmd = "mkdir -p " + path;
        std::system(mkdirCmd.c_str());

        // Datetime-stamped filename so each run gets its own log
        std::time_t now = std::time(nullptr);
        std::tm* lt = std::localtime(&now);
        char timeBuf[32];
        std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", lt);
        path += "/";
        path += timeBuf;
        path += "_profiler_log.tsv";

        file = std::fopen(path.c_str(), "w");
        if (!file) return;

        // Write header
        std::fprintf(file,
            "frame\tdelta_us\tonframe_us\tdata_build_us\tlock_us\t"
            "track_render_us\ttexture_us\thighway_paint_us\t"
            "scene_total_us\tnotes_us\tsustains_us\tgrid_us\tanim_us\texecute_us\t"
            "L0\tL1\tL2\tL3\tL4\tL5\tL6\tL7\tL8\tL9\tL10\tL11\tL12\tL13\tL14\t"
            "track_win\tsustain_win\tgrid_count\t"
            "slots\tpart\tskill\tvp_w\tvp_h\tplaying\tspike\t"
            "agg_paint\tagg_scene\tagg_notes\tagg_sustains\tagg_execute\tagg_track\t"
            "hw0_paint\thw0_scene\thw0_notes\thw0_sustains\thw0_execute\thw0_track\t"
            "hw1_paint\thw1_scene\thw1_notes\thw1_sustains\thw1_execute\thw1_track\t"
            "hw2_paint\thw2_scene\thw2_notes\thw2_sustains\thw2_execute\thw2_track\t"
            "hw3_paint\thw3_scene\thw3_notes\thw3_sustains\thw3_execute\thw3_track\n");

        frameCounter = 0;
        rollingSum = 0.0;
        rollingCount = 0;
        rollingIndex = 0;
        std::memset(rollingRing, 0, sizeof(rollingRing));
        hasPending = false;
    }

    void stop()
    {
        if (!file) return;
        std::fclose(file);
        file = nullptr;
    }

    void beginFrame()
    {
        onFrameStart = std::chrono::high_resolution_clock::now();
    }

    void recordFrameData(double frameDelta_us, double dataBuild_us, double lockWait_us,
                         int trackWindowSize, int sustainWindowSize, int gridlineCount,
                         int slotCount, Part activePart, SkillLevel skillLevel,
                         int viewportW, int viewportH, bool isPlaying)
    {
        pendingRecord = {};
        pendingRecord.frameNumber = frameCounter++;
        pendingRecord.frameDelta_us = (int)frameDelta_us;
        pendingRecord.dataBuild_us = (int)dataBuild_us;
        pendingRecord.lockWait_us = (int)lockWait_us;
        pendingRecord.trackWindowSize = trackWindowSize;
        pendingRecord.sustainWindowSize = sustainWindowSize;
        pendingRecord.gridlineCount = gridlineCount;
        pendingRecord.slotCount = slotCount;
        pendingRecord.activePart = (int)activePart;
        pendingRecord.skillLevel = (int)skillLevel;
        pendingRecord.viewportW = viewportW;
        pendingRecord.viewportH = viewportH;
        pendingRecord.isPlaying = isPlaying ? 1 : 0;

        // Measure onFrame duration
        auto now = std::chrono::high_resolution_clock::now();
        pendingRecord.onFrame_us = (int)std::chrono::duration<double, std::micro>(now - onFrameStart).count();

        hasPending = true;
    }

    void recordPaintData(const PhaseTiming& phaseTiming,
                         double trackRender_us, double textureRender_us,
                         double highwayPaint_us,
                         const HighwayProfileRecord* hwRecords = nullptr, int hwCount = 0)
    {
        if (!file || !hasPending) return;

        pendingRecord.trackRender_us = (int)trackRender_us;
        pendingRecord.textureRender_us = (int)textureRender_us;
        pendingRecord.highwayPaint_us = (int)highwayPaint_us;
        pendingRecord.sceneTotal_us = (int)phaseTiming.total_us;
        pendingRecord.notes_us = (int)phaseTiming.notes_us;
        pendingRecord.sustains_us = (int)phaseTiming.sustains_us;
        pendingRecord.gridlines_us = (int)phaseTiming.gridlines_us;
        pendingRecord.animation_us = (int)phaseTiming.animation_us;
        pendingRecord.execute_us = (int)phaseTiming.execute_us;

        // Multi-highway aggregate + per-highway breakdown
        pendingRecord.hwCount = std::min(hwCount, (int)MAX_PROFILED_HIGHWAYS);
        pendingRecord.aggPaint_us = 0;
        pendingRecord.aggScene_us = 0;
        pendingRecord.aggNotes_us = 0;
        pendingRecord.aggSustains_us = 0;
        pendingRecord.aggExecute_us = 0;
        pendingRecord.aggTrack_us = 0;
        for (int i = 0; i < MAX_PROFILED_HIGHWAYS; ++i)
            pendingRecord.hw[i] = {};
        if (hwRecords)
        {
            for (int i = 0; i < pendingRecord.hwCount; ++i)
            {
                pendingRecord.hw[i] = hwRecords[i];
                pendingRecord.aggPaint_us += hwRecords[i].paint_us;
                pendingRecord.aggScene_us += hwRecords[i].scene_us;
                pendingRecord.aggNotes_us += hwRecords[i].notes_us;
                pendingRecord.aggSustains_us += hwRecords[i].sustains_us;
                pendingRecord.aggExecute_us += hwRecords[i].execute_us;
                pendingRecord.aggTrack_us += hwRecords[i].track_us;
            }
        }
        for (int i = 0; i < DRAW_ORDER_COUNT; ++i)
            pendingRecord.layer_us[i] = (int)phaseTiming.layer_us[i];

        // Spike detection via rolling mean (O(1))
        double frameTotal = (double)pendingRecord.frameDelta_us;
        double oldVal = rollingRing[rollingIndex];
        rollingRing[rollingIndex] = frameTotal;
        rollingSum += frameTotal - oldVal;
        rollingIndex = (rollingIndex + 1) % ROLLING_WINDOW;
        if (rollingCount < ROLLING_WINDOW) ++rollingCount;

        double rollingMean = (rollingCount > 0) ? rollingSum / rollingCount : 0.0;
        bool isSpike = (frameTotal > SPIKE_ABSOLUTE_US)
                    || (rollingCount >= 10 && frameTotal > rollingMean * SPIKE_RELATIVE_FACTOR);
        pendingRecord.isSpike = isSpike ? 1 : 0;

        // Write immediately — no batching.
        // Ensures data survives unclean shutdown (e.g. killall REAPER).
        writeRecord(pendingRecord);
        hasPending = false;
    }

private:
    FILE* file = nullptr;
    int frameCounter = 0;

    FrameProfileRecord pendingRecord;
    bool hasPending = false;

    std::chrono::high_resolution_clock::time_point onFrameStart;

    // Rolling mean for spike detection
    double rollingRing[ROLLING_WINDOW] = {};
    double rollingSum = 0.0;
    int rollingCount = 0;
    int rollingIndex = 0;

    void writeRecord(const FrameProfileRecord& r)
    {
        if (!file) return;
        std::fprintf(file,
            "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t"
            "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t"
            "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t"
            "%d\t%d\t%d\t%d\t%d\t%d",
            r.frameNumber, r.frameDelta_us, r.onFrame_us, r.dataBuild_us, r.lockWait_us,
            r.trackRender_us, r.textureRender_us, r.highwayPaint_us,
            r.sceneTotal_us, r.notes_us, r.sustains_us, r.gridlines_us,
            r.animation_us, r.execute_us,
            r.layer_us[0], r.layer_us[1], r.layer_us[2], r.layer_us[3],
            r.layer_us[4], r.layer_us[5], r.layer_us[6], r.layer_us[7],
            r.layer_us[8], r.layer_us[9], r.layer_us[10], r.layer_us[11],
            r.layer_us[12], r.layer_us[13], r.layer_us[14],
            r.trackWindowSize, r.sustainWindowSize, r.gridlineCount,
            r.slotCount, r.activePart, r.skillLevel,
            r.viewportW, r.viewportH, r.isPlaying, r.isSpike,
            r.aggPaint_us, r.aggScene_us, r.aggNotes_us, r.aggSustains_us,
            r.aggExecute_us, r.aggTrack_us);

        // Per-highway columns (always write 4, zero-filled for inactive)
        for (int i = 0; i < MAX_PROFILED_HIGHWAYS; ++i)
            std::fprintf(file, "\t%d\t%d\t%d\t%d\t%d\t%d",
                r.hw[i].paint_us, r.hw[i].scene_us, r.hw[i].notes_us,
                r.hw[i].sustains_us, r.hw[i].execute_us, r.hw[i].track_us);

        std::fprintf(file, "\n");
        std::fflush(file);
    }
};

#endif
