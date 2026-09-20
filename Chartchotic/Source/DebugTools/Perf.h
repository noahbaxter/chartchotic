/*
    ==============================================================================

        Perf.h
        Author:  Noah Baxter

        Frame profiler. Nested named scopes, self time, and an explicit bucket
        for everything not inside a scope, so a frame always adds up.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>

/*
    Why this exists rather than a few timers: a frame was chased four times by adding one
    more probe, and each round ended with "two thirds is unaccounted, add another". A profiler
    that cannot say where the rest went is a profiler that needs another round.

    Two properties make it terminal:

      - Scopes nest, and every scope reports SELF time (its own minus its children's). Time
        inside a scope that no child covers shows up as that scope's self time, named, instead
        of vanishing into a total.
      - The frame's own wall clock is the denominator, so UNPROFILED = interval - root total
        is computed, not inferred. It can only be the host's message loop, JUCE's compositing,
        or something nobody has wrapped yet, and it is always visible.

    Adding coverage is one line: PERF_SCOPE("name").

    Frames span onFrame to onFrame, NOT the body of onFrame, because JUCE paints components
    after the tick returns. A frame therefore collects its tick and the paints that follow it,
    which is what "a frame" means to anyone looking at frame rate.

    Message thread only. Paints and the editor tick both run there; a scope opened on a worker
    would corrupt the stack, so don't.
*/
namespace Perf
{
    inline constexpr int MAX_NODES = 96;
    inline constexpr int MAX_DEPTH = 16;

    struct Node
    {
        const char* name = nullptr;   // string literal, compared by pointer then by text
        int parent = -1;
        double inclusiveUs = 0.0;     // this scope including everything nested in it
        double childUs = 0.0;         // sum of direct children, so self = inclusive - child
        int calls = 0;
        int depth = 0;
    };

    struct State
    {
        std::array<Node, MAX_NODES> nodes {};
        int nodeCount = 0;

        std::array<int, MAX_DEPTH> stack {};
        std::array<double, MAX_DEPTH> stackStart {};
        int stackDepth = 0;

        double frameStartMs = 0.0;
        double lastIntervalUs = 0.0;
        int framesThisReport = 0;
        double reportLastMs = 0.0;

        bool on = false;
        bool armed = false;   // resolved once from the marker file
    };

    inline State& state()
    {
        static State s;
        return s;
    }

    /** Enabled by a marker file, so a normal build carries no cost and there is no flag to
        forget to switch off. Checked once; an fs stat per frame would be its own problem.

            enable:  touch ~/Library/Application Support/Chartchotic/perf
            read:    ~/Library/Logs/Chartchotic/perf.log */
    inline bool enabled()
    {
        auto& s = state();
        if (!s.armed)
        {
            s.armed = true;
            s.on = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Application Support/Chartchotic/perf")
                       .existsAsFile();
        }
        return s.on;
    }

    inline void setEnabled(bool on)   { enabled(); state().on = on; }

    inline int findNode(const char* name, int parent)
    {
        auto& s = state();
        for (int i = 0; i < s.nodeCount; i++)
            if (s.nodes[i].parent == parent
                && (s.nodes[i].name == name || std::strcmp(s.nodes[i].name, name) == 0))
                return i;

        if (s.nodeCount >= MAX_NODES) return -1;
        auto& n = s.nodes[s.nodeCount];
        n = {};
        n.name = name;
        n.parent = parent;
        n.depth = parent < 0 ? 0 : s.nodes[parent].depth + 1;
        return s.nodeCount++;
    }

    /** RAII timer. Use the PERF_SCOPE macro rather than constructing one by hand. */
    struct Scope
    {
        int node = -1;

        explicit Scope(const char* name)
        {
            if (!enabled()) return;
            auto& s = state();
            if (s.stackDepth >= MAX_DEPTH) return;

            const int parent = s.stackDepth > 0 ? s.stack[s.stackDepth - 1] : -1;
            node = findNode(name, parent);
            if (node < 0) return;

            s.stack[s.stackDepth] = node;
            s.stackStart[s.stackDepth] = juce::Time::getMillisecondCounterHiRes();
            s.stackDepth++;
        }

        /** Ends the scope before it goes out of scope, for a phase that ends partway through a
            function rather than at a closing brace. Idempotent, so the destructor is a no-op
            afterwards. */
        void close()
        {
            if (node < 0) return;
            auto& s = state();
            if (s.stackDepth <= 0) { node = -1; return; }

            s.stackDepth--;
            const double us = (juce::Time::getMillisecondCounterHiRes()
                               - s.stackStart[s.stackDepth]) * 1000.0;
            auto& n = s.nodes[node];
            n.inclusiveUs += us;
            n.calls++;
            if (n.parent >= 0)
                s.nodes[n.parent].childUs += us;
            node = -1;
        }

        ~Scope() { close(); }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };

    inline void writeReport();

    /** Call once at the top of the editor's frame tick. Closes the previous frame, which by
        then holds its tick AND the paints that followed it, then starts a new one. */
    inline void beginFrame()
    {
        if (!enabled()) return;
        auto& s = state();

        const double now = juce::Time::getMillisecondCounterHiRes();
        if (s.frameStartMs > 0.0)
        {
            s.lastIntervalUs += (now - s.frameStartMs) * 1000.0;
            s.framesThisReport++;
        }
        s.frameStartMs = now;

        // A scope left open across a frame boundary means an unbalanced macro somewhere. Reset
        // rather than accumulate garbage into whatever happens to be on top.
        s.stackDepth = 0;

        if (now - s.reportLastMs >= 1000.0 && s.framesThisReport > 0)
        {
            writeReport();
            s.reportLastMs = now;
            for (int i = 0; i < s.nodeCount; i++)
            {
                s.nodes[i].inclusiveUs = 0.0;
                s.nodes[i].childUs = 0.0;
                s.nodes[i].calls = 0;
            }
            s.lastIntervalUs = 0.0;
            s.framesThisReport = 0;
        }
    }

    inline juce::File logFile()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Logs/Chartchotic/perf.log");
    }

    /** One block per second: the tree, averaged per frame, with self time and the share of the
        frame interval each scope takes. UNPROFILED closes the books. */
    inline void writeReport()
    {
        auto& s = state();
        const double frames = (double)s.framesThisReport;
        if (frames <= 0.0) return;

        const double intervalUs = s.lastIntervalUs / frames;
        const double fps = intervalUs > 0.0 ? 1e6 / intervalUs : 0.0;

        double rootTotal = 0.0;
        for (int i = 0; i < s.nodeCount; i++)
            if (s.nodes[i].parent < 0)
                rootTotal += s.nodes[i].inclusiveUs;
        rootTotal /= frames;

        juce::String out;
        out << "\n=== frame " << juce::String(intervalUs / 1000.0, 2) << " ms  ("
            << juce::String(fps, 1) << " fps, avg of " << (int)frames << " frames) ===\n"
            << juce::String("scope").paddedRight(' ', 30) << "   total     self   calls    %\n";

        // Depth-first so a child prints under its parent. Node order is creation order, which
        // is not tree order, so walk it properly rather than assuming.
        std::function<void(int)> emit = [&](int parent)
        {
            for (int i = 0; i < s.nodeCount; i++)
            {
                const auto& n = s.nodes[i];
                if (n.parent != parent) continue;

                const double inc = n.inclusiveUs / frames;
                const double self = (n.inclusiveUs - n.childUs) / frames;
                out << juce::String(juce::String::repeatedString("  ", n.depth) + n.name)
                           .paddedRight(' ', 30)
                    << juce::String((int)inc).paddedLeft(' ', 8)
                    << juce::String((int)self).paddedLeft(' ', 9)
                    << juce::String(n.calls / (int)juce::jmax(1.0, frames)).paddedLeft(' ', 8)
                    << juce::String(intervalUs > 0.0 ? inc / intervalUs * 100.0 : 0.0, 1)
                           .paddedLeft(' ', 6)
                    << "\n";
                emit(i);
            }
        };
        emit(-1);

        const double unprofiled = intervalUs - rootTotal;
        out << juce::String("UNPROFILED").paddedRight(' ', 30)
            << juce::String((int)unprofiled).paddedLeft(' ', 8)
            << juce::String((int)unprofiled).paddedLeft(' ', 9)
            << juce::String("-").paddedLeft(' ', 8)
            << juce::String(intervalUs > 0.0 ? unprofiled / intervalUs * 100.0 : 0.0, 1)
                   .paddedLeft(' ', 6)
            << "   (host message loop, JUCE compositing, or not yet wrapped)\n";

        static std::unique_ptr<juce::FileLogger> logger = []
        {
            auto f = logFile();
            f.getParentDirectory().createDirectory();
            return std::make_unique<juce::FileLogger>(f, "Chartchotic perf", 0);
        }();
        if (logger != nullptr)
            logger->logMessage(out);
    }
}

#define PERF_CONCAT_(a, b) a##b
#define PERF_CONCAT(a, b) PERF_CONCAT_(a, b)
#define PERF_SCOPE(name) Perf::Scope PERF_CONCAT(perfScope_, __LINE__) { name }
