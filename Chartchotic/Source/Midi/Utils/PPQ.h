#pragma once

#include <cstdint>
#include <cmath>

class PPQ
{
public:
    static constexpr double PPQ_RESOLUTION = 960.0;

    // Constructors
    //
    // Rounded, not truncated. A position that has been through seconds and back comes in a
    // hair either side of the tick it means, and truncating sent every one of those a whole
    // tick backwards: a seek would land below the marker it asked for, and positions built
    // by adding a triplet at a time drifted a tick at a time.
    PPQ() : scaledValue(0) {}
    PPQ(float ppq) : scaledValue(std::llround(ppq * PPQ_RESOLUTION)) {}
    PPQ(double ppq) : scaledValue(std::llround(ppq * PPQ_RESOLUTION)) {}
    PPQ(int scaledPPQ) : scaledValue(static_cast<int64_t>(scaledPPQ)) {}
    PPQ(int64_t scaledPPQ) : scaledValue(scaledPPQ) {}

    // Conversion operators
    operator double() const { return static_cast<double>(scaledValue) / PPQ_RESOLUTION; }
    operator int64_t() const { return scaledValue; }

    // Arithmetic operations
    PPQ operator+(const PPQ &other) const { return PPQ(scaledValue + other.scaledValue); }
    PPQ operator-(const PPQ &other) const { return PPQ(scaledValue - other.scaledValue); }
    PPQ operator*(const PPQ &other) const { return PPQ((scaledValue * other.scaledValue) / PPQ_RESOLUTION); }
    PPQ operator/(const PPQ &other) const { return PPQ((scaledValue * PPQ_RESOLUTION) / other.scaledValue); }
    PPQ operator+(double ppq) const { return PPQ(scaledValue + std::llround(ppq * PPQ_RESOLUTION)); }
    PPQ operator-(double ppq) const { return PPQ(scaledValue - std::llround(ppq * PPQ_RESOLUTION)); }
    PPQ operator*(double ppq) const { return PPQ(std::llround(scaledValue * ppq)); }
    PPQ operator/(double ppq) const { return PPQ(std::llround(scaledValue / ppq)); }

    // Comparison operators
    bool operator<(const PPQ &other) const { return scaledValue < other.scaledValue; }
    bool operator<=(const PPQ &other) const { return scaledValue <= other.scaledValue; }
    bool operator>(const PPQ &other) const { return scaledValue > other.scaledValue; }
    bool operator>=(const PPQ &other) const { return scaledValue >= other.scaledValue; }
    bool operator==(const PPQ &other) const { return scaledValue == other.scaledValue; }
    bool operator!=(const PPQ &other) const { return scaledValue != other.scaledValue; }

    // Comparison operators (PPQ vs double)
    //
    // The operand keeps its full precision: quantising it first turned any tolerance finer
    // than a tick into zero, so `pos > now + epsilon` silently asked `pos > now` and every
    // guard written that way did nothing. Compare where we actually are against the number
    // as given, and a caller's epsilon means what it says.
    bool operator>(double other) const { return toDouble() > other; }
    bool operator<(double other) const { return toDouble() < other; }
    bool operator>=(double other) const { return toDouble() >= other; }
    bool operator<=(double other) const { return toDouble() <= other; }
    bool operator==(double other) const { return toDouble() == other; }
    bool operator!=(double other) const { return toDouble() != other; }

    // Comparison operators (PPQ vs int64_t)
    bool operator>(int64_t other) const { return scaledValue > other; }
    bool operator<(int64_t other) const { return scaledValue < other; }
    bool operator>=(int64_t other) const { return scaledValue >= other; }
    bool operator<=(int64_t other) const { return scaledValue <= other; }
    bool operator==(int64_t other) const { return scaledValue == other; }
    bool operator!=(int64_t other) const { return scaledValue != other; }

    // Assignment operators
    PPQ &operator+=(const PPQ &other)
    {
        scaledValue += other.scaledValue;
        return *this;
    }
    PPQ &operator+=(double ppq)
    {
        scaledValue += std::llround(ppq * PPQ_RESOLUTION);
        return *this;
    }
    PPQ &operator+=(int scaledPPQ)
    {
        scaledValue += scaledPPQ;
        return *this;
    }
    PPQ &operator-=(const PPQ &other)
    {
        scaledValue -= other.scaledValue;
        return *this;
    }
    PPQ &operator-=(double ppq)
    {
        scaledValue -= std::llround(ppq * PPQ_RESOLUTION);
        return *this;
    }
    PPQ &operator-=(int scaledPPQ)
    {
        scaledValue -= scaledPPQ;
        return *this;
    }

    // Utility methods
    double toDouble() const { return static_cast<double>(scaledValue) / PPQ_RESOLUTION; }
    int64_t toScaled() const { return scaledValue; }

private:
    int64_t scaledValue;
};

// Global comparison operators for double vs PPQ
inline bool operator<(double lhs, const PPQ &rhs) { return rhs > lhs; }
inline bool operator<=(double lhs, const PPQ &rhs) { return rhs >= lhs; }
inline bool operator>(double lhs, const PPQ &rhs) { return rhs < lhs; }
inline bool operator>=(double lhs, const PPQ &rhs) { return rhs <= lhs; }
inline bool operator==(double lhs, const PPQ &rhs) { return rhs == lhs; }
inline bool operator!=(double lhs, const PPQ &rhs) { return rhs != lhs; }

// Global comparison operators for int64_t vs PPQ
inline bool operator<(int64_t lhs, const PPQ &rhs) { return rhs > lhs; }
inline bool operator<=(int64_t lhs, const PPQ &rhs) { return rhs >= lhs; }
inline bool operator>(int64_t lhs, const PPQ &rhs) { return rhs < lhs; }
inline bool operator>=(int64_t lhs, const PPQ &rhs) { return rhs <= lhs; }
inline bool operator==(int64_t lhs, const PPQ &rhs) { return rhs == lhs; }
inline bool operator!=(int64_t lhs, const PPQ &rhs) { return rhs != lhs; }

// Helper functions
static PPQ fromScaled(int64_t scaledValue)
{
    return PPQ(scaledValue);
}

static PPQ fromDouble(double value)
{
    return PPQ(static_cast<int64_t>(std::llround(value * PPQ::PPQ_RESOLUTION)));
}