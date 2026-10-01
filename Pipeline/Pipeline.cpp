#include "Pipeline.h"

#include <eacp/GPU/Device/Device.h>
#include <eacp/GPU/Frame/ComputePass.h>
#include <TextEncoder/Tokenizer/BpeTokenizer.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <future>

namespace eacp::SA3Pipeline
{
using namespace eacp::GPU;
using namespace eacp::ML;

namespace
{
using Clock = std::chrono::steady_clock;

double secondsSince(Clock::time_point start)
{
    return std::chrono::duration<double> {Clock::now() - start}.count();
}
} // namespace

CheckpointFiles fetchCheckpoints(const SA3Checkpoints::Repo& repo)
{
    auto files = CheckpointFiles {};

    files.model = SA3Checkpoints::fetch(repo, "model.safetensors");
    files.tokenizer = SA3Checkpoints::fetch(repo, "t5gemma-b-b-ul2/tokenizer.json");
    files.textEncoder =
        SA3Checkpoints::fetch(repo, "t5gemma-b-b-ul2/model.safetensors");

    return files;
}

std::unique_ptr<Model> load(const CheckpointFiles& files,
                            const std::string& modelName,
                            Device& device,
                            const LoadedPart& onLoaded)
{
    auto isMedium = modelName == "medium";
    auto model = std::make_unique<Model>();

    auto ditConfig =
        isMedium ? SA3DiT::DiTConfig::medium() : SA3DiT::DiTConfig::smallMusic();
    auto codecConfig =
        isMedium ? SA3Codec::CodecConfig::sameL() : SA3Codec::CodecConfig::sameS();

    auto start = Clock::now();

    // Started before the weights rather than after them. Parsing the 33 MB
    // vocabulary is the largest single piece of the text encoder's load and
    // touches no device, so it runs on a thread of its own while the DiT
    // weights come off the disk and onto the GPU, and is waited for below.
    auto tokenizer =
        std::async(std::launch::async,
                   [path = files.tokenizer.str()]
                   { return SA3TextEncoder::BpeTokenizer::load(path); });

    model->file = SafetensorsFile::open(files.model);

    if (!model->file.has_value())
    {
        std::fprintf(
            stderr, "Could not open checkpoint at %s\n", files.model.str().c_str());
        return nullptr;
    }

    auto& file = model->file;

    std::printf("Loading DiT weights (%s)...\n", modelName.c_str());
    start = Clock::now();
    model->weights = SA3DiT::loadWeights(*file, ditConfig, device);
    std::printf("DiT weights took %.2fs\n", secondsSince(start));
    onLoaded("DiT");

    std::printf("Loading SAME decoder...\n");
    start = Clock::now();
    model->decoder = SA3Codec::SameDecoder::loadFromSafetensors(
        *file, codecConfig, "pretransform.model", device);
    std::printf("Decoder took %.2fs\n", secondsSince(start));
    onLoaded("decoder");

    std::printf("Loading T5Gemma text encoder...\n");
    start = Clock::now();
    model->textEncoder = SA3TextEncoder::SA3TextEncoderModel::load(
        tokenizer.get(), files.textEncoder.str(), *file, device);

    if (!model->textEncoder.has_value())
    {
        std::fprintf(stderr, "Could not load the text encoder.\n");
        return nullptr;
    }

    std::printf("Text encoder took %.2fs\n", secondsSince(start));
    onLoaded("text encoder");

    return model;
}

SA3Codec::StereoWaveform
    generate(Model& model, const Request& request, Device& device)
{
    auto& textEncoder = model.textEncoder;
    auto& weights = model.weights;

    std::printf("Encoding prompt: \"%s\"\n", request.prompt.c_str());
    auto start = Clock::now();

    auto promptCommands = device.makeCommandBuffer();
    auto promptEncoding = std::optional<SA3TextEncoder::PromptEncoding> {};

    {
        auto pass = promptCommands.beginCompute();
        promptEncoding = textEncoder->encodePrompt(pass, request.prompt, device);
    }

    promptCommands.commit();
    if (request.releaseAsItGoes)
        textEncoder.reset();
    std::printf("Prompt encoding took %.2fs\n", secondsSince(start));
    request.afterEncoding();

    auto sampleCount = (int) std::lround((double) request.seconds * sampleRate);
    auto latentLength = latentLengthFor(sampleCount);

    std::printf("Sampling %d steps over %d latent frames (%.2fs)...\n",
                request.samplerSteps,
                latentLength,
                request.seconds);

    if (request.conditioningSeconds.has_value())
        std::printf("Conditioned on %.2fs\n", request.secondsTotal());

    start = Clock::now();

    auto latent =
        SA3Sampler::pingpongSample(*weights,
                                   promptEncoding->embeddings,
                                   latentLength,
                                   request.secondsTotal(),
                                   request.samplerSteps,
                                   SA3Sampler::randomNoiseSource(request.seed),
                                   device,
                                   request.onStep,
                                   request.probe);

    std::printf("Sampling took %.2fs\n", secondsSince(start));

    request.afterSampling(*weights, promptEncoding->embeddings, latentLength);

    if (request.releaseAsItGoes)
    {
        weights.reset();
        promptEncoding.reset();
    }

    std::printf("Decoding audio...\n");
    start = Clock::now();
    auto waveform = model.decoder->decode(latent, sampleCount, device);
    std::printf("Decoding took %.2fs\n", secondsSince(start));

    return waveform;
}

int latentLengthFor(int sampleCount)
{
    return (sampleCount + downsamplingRatio - 1) / downsamplingRatio;
}
} // namespace eacp::SA3Pipeline
