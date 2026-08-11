#pragma once

#include "pressurehex/PressureHexDspPrimitives.h"

#include <array>

namespace pressurehex
{

struct PressureHexParameters
{
    float threshold = -18.0f;
    float ratio = 4.0f;
    float attack = 8.0f;
    float release = 120.0f;
    float knee = 8.0f;
    float focus = 0.45f;
    float lookahead = 3.0f;
};

class PressureHexEngine
{
public:
    PressureHexEngine();
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setParameters (const PressureHexParameters& parameters) noexcept;
    [[nodiscard]] StereoFrame processSample (float inputLeft, float inputRight) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

private:
    static constexpr int maxLookaheadSamples = 2048;
    [[nodiscard]] float coefficientForMs (float milliseconds) const noexcept;
    [[nodiscard]] float gainForEnvelope (float envelope) noexcept;
    [[nodiscard]] StereoFrame delayedFrame (float left, float right) noexcept;
    [[nodiscard]] StereoFrame sanitizeFrame (float left, float right) const noexcept;

    PressureHexParameters params;
    double sampleRate = 44100.0;
    int lookaheadSamples = 0;
    int writeIndex = 0;
    std::array<StereoFrame, maxLookaheadSamples> delay {};
    OnePole rmsLeft;
    OnePole rmsRight;
    float detectorDb = -120.0f;
    float gainDb = 0.0f;
};

} // namespace pressurehex
