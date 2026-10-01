#include <eacp/GPU/Device/Device.h>
#include <eacp/GPU/Frame/ComputePass.h>
#include <eacp/GPU/Timing/CallCost.h>
#include <Codec/WavFile.h>
#include <Pipeline/Pipeline.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

using namespace eacp;
using namespace eacp::GPU;
using namespace eacp::ML;

namespace
{
struct Options
{
    std::string prompt = "lofi house loop";
    float seconds = 8.f;
    std::string output = "stable-audio-3-output.wav";
    std::uint64_t seed = 42;
    int samplerSteps = 8;
    std::string model = "small";
    bool profile = false;
    int repeat = 0;
    bool fetchOnly = false;
};

using Clock = std::chrono::steady_clock;

double secondsSince(Clock::time_point start)
{
    return std::chrono::duration<double> {Clock::now() - start}.count();
}

Options parseOptions(int argc, char** argv)
{
    auto options = Options {};

    for (auto i = 1; i < argc; ++i)
    {
        auto flag = std::string {argv[i]};
        auto hasValue = i + 1 < argc;

        if (flag == "--prompt" && hasValue)
            options.prompt = argv[++i];
        else if (flag == "--seconds" && hasValue)
            options.seconds = std::stof(argv[++i]);
        else if (flag == "--output" && hasValue)
            options.output = argv[++i];
        else if (flag == "--seed" && hasValue)
            options.seed = (std::uint64_t) std::stoull(argv[++i]);
        else if (flag == "--samplerSteps" && hasValue)
            options.samplerSteps = std::atoi(argv[++i]);
        else if (flag == "--model" && hasValue)
            options.model = argv[++i];
        else if (flag == "--profile")
            options.profile = true;
        else if (flag == "--repeat" && hasValue)
            options.repeat = std::atoi(argv[++i]);
        else if (flag == "--fetch-only")
            options.fetchOnly = true;
    }

    return options;
}

// One more DiT step, on the noise the sampler would start from, with every
// kernel it dispatches timed on its own - after sampling, so the audio is what
// it would have been without the flag.
void printStepProfile(const SA3DiT::Weights& weights,
                      const Tensor& crossAttnContext,
                      int latentLength,
                      float secondsTotal,
                      std::uint64_t seed,
                      Device& device)
{
    auto noise =
        SA3Sampler::randomNoiseSource(seed)(latentLength * SA3DiT::ioChannels);
    auto latent = Tensor::fromHostF32(
        noise.data(), {latentLength, SA3DiT::ioChannels}, device);

    auto commands = device.makeCommandBuffer();

    {
        auto pass = commands.beginCompute(
            {}, DispatchOrder::Serial, TimingScope::EachDispatch);
        auto velocity = SA3DiT::forward(
            pass, weights, latent, 1.f, secondsTotal, crossAttnContext, device);
    }

    commands.commit();

    const auto& timings = commands.timings();

    if (timings.passes.empty())
    {
        std::printf("This device cannot time dispatches.\n");
        return;
    }

    auto kernelTotal = 0.0;

    for (const auto& pass: timings.passes)
        kernelTotal += pass.milliseconds;

    std::printf("\nOne DiT step, %d dispatches, %.1f ms in kernels (%.1f ms end to "
                "end, timed one encoder per dispatch):\n",
                (int) timings.passes.size(),
                kernelTotal,
                timings.milliseconds);

    for (const auto& total: timings.totalsByLabel())
        std::printf("  %-32s %8.2f ms  %5.1f%%  %5d dispatches\n",
                    total.label.c_str(),
                    total.milliseconds,
                    100.0 * total.milliseconds / kernelTotal,
                    total.count);

    std::printf("\n");
}
} // namespace

int main(int argc, char** argv)
{
    auto options = parseOptions(argc, argv);
    auto totalStart = Clock::now();

    if (options.model != "small" && options.model != "medium")
    {
        std::fprintf(stderr, "--model must be small or medium.\n");
        return 1;
    }

    auto isMedium = options.model == "medium";
    auto& repo = isMedium ? SA3Checkpoints::medium : SA3Checkpoints::smallMusic;
    auto files = SA3Pipeline::CheckpointFiles {};

    try
    {
        files = SA3Pipeline::fetchCheckpoints(repo);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }

    if (options.fetchOnly)
        return 0;

    auto& device = Device::shared();

    if (!device.isValid())
    {
        std::fprintf(stderr, "No GPU device available.\n");
        return 1;
    }

    auto model = SA3Pipeline::load(files, options.model, device);

    if (model == nullptr)
        return 1;

    auto request = SA3Pipeline::Request {};
    request.prompt = options.prompt;
    request.seconds = options.seconds;
    request.seed = options.seed;
    request.samplerSteps = options.samplerSteps;
    request.releaseAsItGoes = options.repeat == 0;

    if (options.profile)
        request.afterSampling = [&](const SA3DiT::Weights& weights,
                                    const Tensor& crossAttnContext,
                                    int latentLength)
        {
            printStepProfile(weights,
                             crossAttnContext,
                             latentLength,
                             options.seconds,
                             options.seed,
                             device);
        };

    auto waveform = SA3Pipeline::generate(*model, request, device);

    auto start = Clock::now();

    if (!SA3Codec::writeWavFile(options.output, waveform, SA3Pipeline::sampleRate))
    {
        std::fprintf(stderr, "Could not write %s\n", options.output.c_str());
        return 1;
    }

    std::printf("WAV write took %.2fs\n", secondsSince(start));

    auto sampleCount =
        (int) std::lround((double) options.seconds * SA3Pipeline::sampleRate);
    auto latentLength = SA3Pipeline::latentLengthFor(sampleCount);

    // Generating again in the same process, which is what a server, a
    // plugin or a UI does and what a one-shot run cannot show: the first
    // time through pays for every pipeline the driver compiles and every
    // buffer the pool has not got yet, and none of that is what the work
    // costs once it is running. It is also the like-for-like against a
    // PyTorch number taken from a second generate() in a loaded process.
    for (auto again = 0; again < options.repeat; ++again)
    {
        auto repeatStart = Clock::now();
        auto repeatCommands = device.makeCommandBuffer();
        auto repeatEncoding = std::optional<SA3TextEncoder::PromptEncoding> {};

        {
            auto pass = repeatCommands.beginCompute();
            repeatEncoding =
                model->textEncoder->encodePrompt(pass, options.prompt, device);
        }

        repeatCommands.commit();

        auto repeatLatent =
            SA3Sampler::pingpongSample(*model->weights,
                                       repeatEncoding->embeddings,
                                       latentLength,
                                       options.seconds,
                                       options.samplerSteps,
                                       SA3Sampler::randomNoiseSource(options.seed),
                                       device);

        auto repeatWaveform =
            model->decoder->decode(repeatLatent, sampleCount, device);
        std::printf("Generating again took %.3fs\n", secondsSince(repeatStart));
    }
    std::printf("Wrote %s\n", options.output.c_str());
    std::printf("Total took %.2fs\n", secondsSince(totalStart));

    if (options.profile)
    {
        auto counts = model->file->loadCounts();
        std::printf("Checkpoint tensors: %d in place, %d copied, %d converted\n",
                    counts.inPlace,
                    counts.copied,
                    counts.converted);

        for (const auto& cost: GPU::callCosts())
            std::printf("%s: %d calls, %.2fs\n",
                        cost.label.c_str(),
                        cost.calls,
                        cost.seconds);
    }

    return 0;
}
