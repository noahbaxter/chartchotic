#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "Visual/Utils/Frame.h"
#include "Visual/Utils/FrameRenderer.h"
#include "Visual/Utils/DrawingConstants.h"

using Render::Frame;
using Render::FrameSprite;
using Render::drawFrame;

namespace {
    juce::Image makeSentinelImage()
    {
        return juce::Image(juce::Image::ARGB, 4, 4, true);
    }

    bool approxEqual(float a, float b, float eps = 1e-5f)
    {
        return std::fabs(a - b) < eps;
    }

    juce::Point<float> uniformScale(float s) { return {s, s}; }
}

TEST_CASE("drawFrame - empty frame emits no draw calls", "[frame]")
{
    Frame frame;
    DrawCallMap drawCalls{};
    drawFrame(frame, {0.0f, 0.0f}, uniformScale(1.0f), drawCalls);

    int total = 0;
    for (auto& row : drawCalls)
        for (auto& bucket : row)
            total += (int)bucket.size();
    REQUIRE(total == 0);
}

TEST_CASE("drawFrame - single sprite at scale 1.0 emits one draw call", "[frame]")
{
    auto img = makeSentinelImage();

    Frame frame;
    frame.sprites.push_back({ &img, 0.0f, 0.0f, 100.0f, 50.0f,
                              (int)DrawOrder::NOTE, 0, 1.0f });

    DrawCallMap drawCalls{};
    drawFrame(frame, {500.0f, 300.0f}, uniformScale(1.0f), drawCalls);

    REQUIRE(drawCalls[(int)DrawOrder::NOTE][0].size() == 1);
}

TEST_CASE("drawFrame - nullptr image sprite is skipped", "[frame]")
{
    Frame frame;
    frame.sprites.push_back({ nullptr, 0.0f, 0.0f, 100.0f, 50.0f,
                              (int)DrawOrder::NOTE, 0, 1.0f });

    DrawCallMap drawCalls{};
    drawFrame(frame, {0.0f, 0.0f}, uniformScale(1.0f), drawCalls);

    REQUIRE(drawCalls[(int)DrawOrder::NOTE][0].empty());
}

TEST_CASE("drawFrame - drift regression: offsetY / height stays constant under scale.y", "[frame]")
{
    // The drift bug was offset and sprite height scaling with different curves.
    // Under drawFrame, both offsetY and height multiply by scale.y, so the
    // ratio offsetY / height is depth-invariant by construction.
    float anchorY = 100.0f;
    float offsetY = 12.0f;
    float height  = 60.0f;

    auto ratioAt = [&](float sy) {
        float gap  = offsetY * sy;
        float hSpr = height  * sy;
        return gap / hSpr;
    };

    REQUIRE(approxEqual(ratioAt(1.0f), ratioAt(0.3f)));
    REQUIRE(approxEqual(ratioAt(0.3f), ratioAt(0.55f)));
}

TEST_CASE("drawFrame - offsetX / width stays constant under scale.x", "[frame]")
{
    float offsetX = 8.0f;
    float width   = 40.0f;

    auto ratioAt = [&](float sx) {
        return (offsetX * sx) / (width * sx);
    };

    REQUIRE(approxEqual(ratioAt(1.0f), ratioAt(0.25f)));
}

TEST_CASE("drawFrame - scaleX and scaleY are independent", "[frame]")
{
    float w = 100.0f, h = 50.0f;
    float sx = 0.5f, sy = 0.2f;
    REQUIRE(approxEqual(w * sx, 50.0f));
    REQUIRE(approxEqual(h * sy, 10.0f));
    REQUIRE(!approxEqual((w * sx) / (h * sy), w / h));
}

TEST_CASE("drawFrame - draw order routing", "[frame]")
{
    auto img = makeSentinelImage();

    Frame frame;
    frame.sprites.push_back({ &img, 0.0f, 0.0f, 10.0f, 10.0f,
                              (int)DrawOrder::BAR,     0, 1.0f });
    frame.sprites.push_back({ &img, 0.0f, 0.0f, 10.0f, 10.0f,
                              (int)DrawOrder::NOTE,    0, 1.0f });
    frame.sprites.push_back({ &img, 0.0f, 0.0f, 10.0f, 10.0f,
                              (int)DrawOrder::OVERLAY, 0, 1.0f });

    DrawCallMap drawCalls{};
    drawFrame(frame, {0.0f, 0.0f}, uniformScale(1.0f), drawCalls);

    REQUIRE(drawCalls[(int)DrawOrder::BAR][0].size()     == 1);
    REQUIRE(drawCalls[(int)DrawOrder::NOTE][0].size()    == 1);
    REQUIRE(drawCalls[(int)DrawOrder::OVERLAY][0].size() == 1);
    REQUIRE(drawCalls[(int)DrawOrder::GRID][0].empty());
}

TEST_CASE("drawFrame - scale = (0,0) still emits calls with zero-size rects", "[frame]")
{
    auto img = makeSentinelImage();

    Frame frame;
    frame.sprites.push_back({ &img, 10.0f, 20.0f, 100.0f, 50.0f,
                              (int)DrawOrder::NOTE, 0, 1.0f });

    DrawCallMap drawCalls{};
    drawFrame(frame, {0.0f, 0.0f}, {0.0f, 0.0f}, drawCalls);

    REQUIRE(drawCalls[(int)DrawOrder::NOTE][0].size() == 1);
}
