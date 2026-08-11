#include "pressurehex/PressureHexEngine.h"

#include <algorithm>
#include <cmath>

namespace pressurehex
{
namespace
{
constexpr float outputCeiling = 0.980f;
constexpr float minDb = -120.0f;
}

PressureHexEngine::PressureHexEngine() { prepare (44100.0); reset(); }

void PressureHexEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    setParameters (params);
    reset();
}

void PressureHexEngine::reset() noexcept
{
    delay.fill ({});
    writeIndex = 0;
    rmsLeft.reset();
    rmsRight.reset();
    detectorDb = minDb;
    gainDb = 0.0f;
}

void PressureHexEngine::setParameters (const PressureHexParameters& p) noexcept
{
    params.threshold = clampFinite (p.threshold, -60.0f, 0.0f, PressureHexParameters {}.threshold);
    params.ratio = clampFinite (p.ratio, 1.0f, 20.0f, PressureHexParameters {}.ratio);
    params.attack = clampFinite (p.attack, 0.1f, 100.0f, PressureHexParameters {}.attack);
    params.release = clampFinite (p.release, 5.0f, 800.0f, PressureHexParameters {}.release);
    params.knee = clampFinite (p.knee, 0.0f, 30.0f, PressureHexParameters {}.knee);
    params.focus = clampFinite (p.focus, 0.0f, 1.0f, PressureHexParameters {}.focus);
    params.lookahead = clampFinite (p.lookahead, 0.0f, 20.0f, PressureHexParameters {}.lookahead);
    lookaheadSamples = std::clamp (static_cast<int> (std::round (params.lookahead * sampleRate * 0.001)), 0, maxLookaheadSamples - 1);
}

float PressureHexEngine::coefficientForMs (float milliseconds) const noexcept
{
    return std::exp (-1.0f / (std::max (0.001f, milliseconds) * 0.001f * static_cast<float> (sampleRate)));
}

float PressureHexEngine::gainForEnvelope (float envelope) noexcept
{
    const auto inputDb = 20.0f * std::log10 (std::max (envelope, 1.0e-6f));
    const auto over = inputDb - params.threshold;
    float compressedOver = over;
    if (params.knee > 0.001f)
    {
        const auto halfKnee = params.knee * 0.5f;
        if (over <= -halfKnee)
            compressedOver = over;
        else if (over >= halfKnee)
            compressedOver = over / params.ratio;
        else
        {
            const auto x = (over + halfKnee) / params.knee;
            const auto target = over / params.ratio;
            compressedOver = over + (target - over) * x * x * (3.0f - 2.0f * x);
        }
    }
    else if (over > 0.0f)
        compressedOver = over / params.ratio;

    const auto wantedGainDb = std::min (0.0f, compressedOver - over);
    const auto coefficient = wantedGainDb < gainDb ? coefficientForMs (params.attack) : coefficientForMs (params.release);
    gainDb = coefficient * gainDb + (1.0f - coefficient) * wantedGainDb;
    return std::pow (10.0f, gainDb / 20.0f);
}

StereoFrame PressureHexEngine::delayedFrame (float left, float right) noexcept
{
    delay[static_cast<std::size_t> (writeIndex)] = { left, right };
    auto readIndex = writeIndex - lookaheadSamples;
    if (readIndex < 0)
        readIndex += maxLookaheadSamples;
    ++writeIndex;
    if (writeIndex >= maxLookaheadSamples)
        writeIndex = 0;
    return delay[static_cast<std::size_t> (readIndex)];
}

StereoFrame PressureHexEngine::processSample (float inputLeft, float inputRight) noexcept
{
    const auto left = sanitizeAudio (inputLeft);
    const auto right = sanitizeAudio (inputRight);
    const auto peak = std::max (std::fabs (left), std::fabs (right));
    const auto rms = std::sqrt (0.5f * (rmsLeft.process (left * left, 0.997f) + rmsRight.process (right * right, 0.997f)));
    const auto detector = params.focus * peak + (1.0f - params.focus) * rms;
    detectorDb = std::max (minDb, 20.0f * std::log10 (std::max (detector, 1.0e-6f)));
    const auto gain = gainForEnvelope (std::pow (10.0f, detectorDb / 20.0f));
    const auto delayed = delayedFrame (left, right);
    return sanitizeFrame (delayed.left * gain, delayed.right * gain);
}

void PressureHexEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0) return;
    for (int i = 0; i < numSamples; ++i) { const auto f = processSample (left[i], right[i]); left[i] = f.left; right[i] = f.right; }
}

StereoFrame PressureHexEngine::sanitizeFrame (float left, float right) const noexcept
{
    auto l = std::clamp (softClip (left, 1.03f), -outputCeiling, outputCeiling);
    auto r = std::clamp (softClip (right, 1.03f), -outputCeiling, outputCeiling);
    if (std::fabs (l) < 1.0e-20f) l = 0.0f;
    if (std::fabs (r) < 1.0e-20f) r = 0.0f;
    return { l, r };
}

} // namespace pressurehex
