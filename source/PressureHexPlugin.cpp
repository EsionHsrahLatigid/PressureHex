#include "PressureHexPlugin.h"

#include "ProductState.h"

#if ! PRESSUREHEX_HEADLESS_TEST
#include "ParameterGridEditor.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>

namespace pressurehex::plugin
{
namespace
{
constexpr std::array<char, 4> stateMagic {{ 'P', 'H', 'X', '1' }};
constexpr int stateVersion = 1;
constexpr std::size_t presetParameterCount = 7;
constexpr std::array<std::array<float, presetParameterCount>, 4> presetValues {{
    {{ -18.0f, 4.0f, 8.0f, 120.0f, 8.0f, 0.45f, 3.0f }},
    {{ -30.0f, 8.0f, 2.0f, 180.0f, 14.0f, 0.85f, 5.0f }},
    {{ -12.0f, 2.5f, 30.0f, 320.0f, 4.0f, 0.15f, 1.0f }},
    {{ -24.0f, 12.0f, 1.0f, 70.0f, 18.0f, 0.65f, 8.0f }}
}};

yup::AudioParameter::Ptr makeParameter (const char* id, const char* name, int hostID, float minValue, float maxValue, float defaultValue, yup::AudioParameter::ParameterUnit unit, float smoothingMs)
{
    return yup::AudioParameterBuilder().withID (id).withName (name).withHostID (static_cast<yup::uint32> (hostID)).withRange (minValue, maxValue).withDefault (defaultValue).withSmoothing (smoothingMs).withModulatable (true).withUnit (unit).build();
}
}

PressureHexPlugin::PressureHexPlugin()
    : yup::AudioProcessor ("PressureHex", yup::AudioBusLayout ({ yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Input, 2) }, { yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2) }))
{
    parameters[threshold] = makeParameter ("threshold", "Threshold", threshold, -60.0f, 0.0f, presetValues[0][threshold], yup::AudioParameter::ParameterUnit::Decibels, 8.0f);
    parameters[ratio] = makeParameter ("ratio", "Ratio", ratio, 1.0f, 20.0f, presetValues[0][ratio], yup::AudioParameter::ParameterUnit::Generic, 20.0f);
    parameters[attack] = makeParameter ("attack", "Attack", attack, 0.1f, 100.0f, presetValues[0][attack], yup::AudioParameter::ParameterUnit::Milliseconds, 18.0f);
    parameters[release] = makeParameter ("release", "Release", release, 5.0f, 800.0f, presetValues[0][release], yup::AudioParameter::ParameterUnit::Milliseconds, 28.0f);
    parameters[knee] = makeParameter ("knee", "Knee", knee, 0.0f, 30.0f, presetValues[0][knee], yup::AudioParameter::ParameterUnit::Decibels, 18.0f);
    parameters[focus] = makeParameter ("focus", "Focus", focus, 0.0f, 1.0f, presetValues[0][focus], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[lookahead] = makeParameter ("lookahead", "Lookahead", lookahead, 0.0f, 20.0f, presetValues[0][lookahead], yup::AudioParameter::ParameterUnit::Milliseconds, 18.0f);
    for (const auto& parameter : parameters)
        addParameter (parameter);
    syncParameterValuesFromParameters();
    updateEngineParameters();
}

void PressureHexPlugin::prepareToPlay (const yup::AudioSpec& spec)
{
    engine.prepare (spec.sampleRate);
    engine.reset();
    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i] = yup::AudioParameterHandle (*parameters[i], spec.sampleRate);
    syncParameterValuesFromParameters();
    updateEngineParameters();
    controlUpdateCountdown = 0;
    inputPeakMilli.store (0, std::memory_order_relaxed);
    outputPeakMilli.store (0, std::memory_order_relaxed);
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionSampleRate = std::isfinite (spec.sampleRate) && spec.sampleRate > 1.0 ? spec.sampleRate : 44100.0;
    auditionPhase = 0.0f;
    auditionNoise = 0x6d2b79f5u;
#endif
}

void PressureHexPlugin::releaseResources() {}

void PressureHexPlugin::processBlock (yup::AudioProcessContext<float>& context)
{
    auto& audio = context.audio;
    const auto numSamples = audio.getNumSamples();
    const auto numChannels = audio.getNumChannels();
    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i].prepareBlock (context.params, parameters[i]->getIndexInContainer());
    auto* left = numChannels > 0 ? audio.getWritePointer (0) : nullptr;
    auto* right = numChannels > 1 ? audio.getWritePointer (1) : nullptr;
    float blockInputPeak = 0.0f;
    float blockOutputPeak = 0.0f;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        advanceParameterHandles (sample);
        if (controlUpdateCountdown <= 0) { updateEngineParameters(); controlUpdateCountdown = parameterUpdateCadenceSamples; }
        --controlUpdateCountdown;
        auto inputLeft = left != nullptr ? left[sample] : 0.0f;
        auto inputRight = right != nullptr ? right[sample] : inputLeft;
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
        const auto audition = renderAuditionFrame();
        inputLeft += audition.left;
        inputRight += audition.right;
#endif
        blockInputPeak = std::max (blockInputPeak, std::max (std::fabs (inputLeft), std::fabs (inputRight)));
        const auto frame = engine.processSample (inputLeft, inputRight);
        if (left != nullptr) left[sample] = frame.left;
        if (right != nullptr) right[sample] = frame.right;
        blockOutputPeak = std::max (blockOutputPeak, std::max (std::fabs (frame.left), std::fabs (frame.right)));
        for (int channel = 2; channel < numChannels; ++channel) audio.getWritePointer (channel)[sample] = 0.0f;
    }
    inputPeakMilli.store (static_cast<int> (std::clamp (blockInputPeak, 0.0f, 1.0f) * 1000.0f + 0.5f), std::memory_order_relaxed);
    outputPeakMilli.store (static_cast<int> (std::clamp (blockOutputPeak, 0.0f, 1.0f) * 1000.0f + 0.5f), std::memory_order_relaxed);
    context.midi.clear();
}

void PressureHexPlugin::flush()
{
    engine.reset(); controlUpdateCountdown = 0; inputPeakMilli.store (0, std::memory_order_relaxed); outputPeakMilli.store (0, std::memory_order_relaxed);
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionPhase = 0.0f; auditionNoise = 0x6d2b79f5u;
#endif
}

bool PressureHexPlugin::acceptsMidi() const noexcept { return false; }
bool PressureHexPlugin::producesMidi() const noexcept { return false; }
int PressureHexPlugin::getCurrentPreset() const noexcept { return currentPreset.load (std::memory_order_relaxed); }
void PressureHexPlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size()))) return;
    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i) parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);
}
int PressureHexPlugin::getNumPresets() const { return static_cast<int> (presetNames.size()); }
yup::String PressureHexPlugin::getPresetName (int index) const { return yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())) ? presetNames[static_cast<std::size_t> (index)] : "Invalid Preset"; }
void PressureHexPlugin::setPresetName (int index, yup::StringRef newName) { if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size()))) presetNames[static_cast<std::size_t> (index)] = newName; }
yup::Result PressureHexPlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    int loadedPreset = 0; const auto result = loadProductState (*this, data, stateMagic, stateVersion, getNumPresets(), loadedPreset);
    if (result.failed()) return result; currentPreset.store (loadedPreset, std::memory_order_relaxed); return yup::Result::ok();
}
yup::Result PressureHexPlugin::saveStateIntoMemory (yup::MemoryBlock& data) { return saveProductState (*this, data, stateMagic, stateVersion, currentPreset.load (std::memory_order_relaxed)); }
bool PressureHexPlugin::hasEditor() const
{
#if PRESSUREHEX_HEADLESS_TEST
    return false;
#else
    return true;
#endif
}
yup::AudioProcessorEditor* PressureHexPlugin::createEditor()
{
#if PRESSUREHEX_HEADLESS_TEST
    return nullptr;
#else
    return new ParameterGridEditor (*this, "PressureHex", "Feed-forward log-domain compressor effect with standalone-only audition.", 0xffd8d8d8u);
#endif
}
float PressureHexPlugin::getInputPeakLevel() const noexcept { return static_cast<float> (inputPeakMilli.load (std::memory_order_relaxed)) * 0.001f; }
float PressureHexPlugin::getOutputPeakLevel() const noexcept { return static_cast<float> (outputPeakMilli.load (std::memory_order_relaxed)) * 0.001f; }
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
void PressureHexPlugin::setAuditionEnabled (bool shouldBeEnabled) noexcept { auditionEnabled.store (shouldBeEnabled ? 1 : 0, std::memory_order_relaxed); }
bool PressureHexPlugin::isAuditionEnabled() const noexcept { return auditionEnabled.load (std::memory_order_relaxed) != 0; }
void PressureHexPlugin::setAuditionType (int type) noexcept { auditionType.store (std::clamp (type, 0, 1), std::memory_order_relaxed); }
int PressureHexPlugin::getAuditionType() const noexcept { return auditionType.load (std::memory_order_relaxed); }
#endif
void PressureHexPlugin::advanceParameterHandles (int samplePosition) noexcept
{
    for (std::size_t i = 0; i < parameterHandles.size(); ++i) { parameterHandles[i].advanceToSample (samplePosition); currentParameterValues[i] = parameterHandles[i].getNextValue(); }
}
void PressureHexPlugin::syncParameterValuesFromParameters() noexcept { for (std::size_t i = 0; i < parameters.size(); ++i) currentParameterValues[i] = parameters[i]->getValue(); }
void PressureHexPlugin::updateEngineParameters() noexcept
{
    pressurehex::PressureHexParameters engineParameters;
    engineParameters.threshold = currentParameterValues[threshold];
    engineParameters.ratio = currentParameterValues[ratio];
    engineParameters.attack = currentParameterValues[attack];
    engineParameters.release = currentParameterValues[release];
    engineParameters.knee = currentParameterValues[knee];
    engineParameters.focus = currentParameterValues[focus];
    engineParameters.lookahead = currentParameterValues[lookahead];
    engine.setParameters (engineParameters);
}
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
StereoFrame PressureHexPlugin::renderAuditionFrame() noexcept
{
    if (auditionEnabled.load (std::memory_order_relaxed) == 0) return {};
    auditionPhase += 96.0f / static_cast<float> (auditionSampleRate);
    if (auditionPhase >= 1.0f) auditionPhase -= 1.0f;
    auditionNoise ^= auditionNoise << 13u; auditionNoise ^= auditionNoise >> 17u; auditionNoise ^= auditionNoise << 5u;
    if (auditionNoise == 0u) auditionNoise = 0x6d2b79f5u;
    const auto type = auditionType.load (std::memory_order_relaxed);
    const auto noise = static_cast<float> (static_cast<double> (auditionNoise) / 2147483648.0 - 1.0);
    const auto pulse = auditionPhase < 0.18f ? 1.0f : -0.55f;
    const auto saw = auditionPhase * 2.0f - 1.0f;
    const auto source = type == 0 ? saw * 0.22f + noise * 0.035f : pulse * 0.18f + noise * 0.055f;
    return { source, source * 0.93f };
}
#endif

} // namespace pressurehex::plugin

extern "C" yup::AudioProcessor* createPluginProcessor() { return new pressurehex::plugin::PressureHexPlugin(); }
