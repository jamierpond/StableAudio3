#pragma once

#include "../Checkpoints.h"

#include <eacp/ML/Loader/SafetensorsFile.h>
#include <Codec/SA3Codec.h>
#include <DiT/SA3DiT.h>
#include <DiT/Weights.h>
#include <Sampler/Sampler.h>
#include <TextEncoder/SA3TextEncoder.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>

// Text in, stereo audio out: what the command line app and the iOS app share.
// Each stage prints its progress and timing to stdout.
namespace eacp::SA3Pipeline
{
constexpr auto sampleRate = 44100;
constexpr auto downsamplingRatio = 4096;

struct CheckpointFiles
{
    FilePath model;
    FilePath tokenizer;
    FilePath textEncoder;
};

// Blocking; throws when a file could not be had.
CheckpointFiles fetchCheckpoints(const SA3Checkpoints::Repo& repo);

// The DiT and the text encoder hold views into file, which outlives them.
struct Model
{
    std::optional<ML::SafetensorsFile> file;
    std::optional<SA3DiT::Weights> weights;
    std::optional<SA3Codec::SameDecoder> decoder;
    std::optional<SA3TextEncoder::SA3TextEncoderModel> textEncoder;
};

using LoadedPart = std::function<void(const std::string& part)>;

// modelName is "small" or "medium". Null, with the reason on stderr, when a
// file could not be loaded. onLoaded names each part as it lands.
std::unique_ptr<Model> load(
    const CheckpointFiles& files,
    const std::string& modelName,
    GPU::Device& device,
    const LoadedPart& onLoaded = [](const std::string&) {});

using AfterSampling = std::function<void(const SA3DiT::Weights& weights,
                                         const ML::Tensor& crossAttnContext,
                                         int latentLength)>;

struct Request
{
    std::string prompt = "lofi house loop";
    float seconds = 8.f;
    std::uint64_t seed = 42;
    int samplerSteps = 8;

    // The length the DiT is told the audio is (its seconds_total global
    // conditioning, clamped to 0..384). Unset, it is seconds. Asking for less
    // than is generated makes the model end the music early and fill the
    // rest; more makes the window a slice of a longer piece.
    std::optional<float> conditioningSeconds;

    float secondsTotal() const { return conditioningSeconds.value_or(seconds); }

    // Drops the text encoder once the prompt is encoded and the DiT once
    // sampling is done, for a process that generates once.
    bool releaseAsItGoes = false;

    std::function<void()> afterEncoding = [] {};
    SA3Sampler::StepCallback onStep = [](int, int) {};
    SA3Sampler::StepProbe probe;
    AfterSampling afterSampling =
        [](const SA3DiT::Weights&, const ML::Tensor&, int) {};
};

SA3Codec::StereoWaveform
    generate(Model& model, const Request& request, GPU::Device& device);

int latentLengthFor(int sampleCount);
} // namespace eacp::SA3Pipeline
