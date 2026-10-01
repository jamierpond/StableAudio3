#include "Generator.h"

#include "Footprint.h"

#include <eacp/Core/Threads/EventLoop.h>
#include <eacp/Core/Utils/Files.h>
#include <eacp/GPU/Device/Device.h>
#include <Codec/WavFile.h>

#include <algorithm>
#include <cmath>

namespace eacp::SA3App
{
namespace
{
std::vector<std::uint8_t> peaksOf(const SA3Codec::StereoWaveform& waveform,
                                  int count)
{
    auto peaks = std::vector<std::uint8_t>((std::size_t) count);
    auto length = waveform.left.size();

    for (auto i = 0; i < count; ++i)
    {
        auto begin = length * (std::size_t) i / (std::size_t) count;
        auto end = length * (std::size_t) (i + 1) / (std::size_t) count;
        auto peak = 0.f;

        for (auto frame = begin; frame < end; ++frame)
            peak = std::max({peak,
                             std::abs(waveform.left[frame]),
                             std::abs(waveform.right[frame])});

        peaks[(std::size_t) i] =
            (std::uint8_t) std::lround(std::clamp(peak, 0.f, 1.f) * 255.f);
    }

    return peaks;
}

FilePath outputFile()
{
    auto directory = FilePath::appCacheDirectory();
    Files::createDirectories(directory);
    return directory / "generated.wav";
}

Status failure(const std::string& error)
{
    return {.stage = Stage::Failed, .error = error};
}
} // namespace

Generator::Generator(ForwardProbe& probeToUse)
    : probe(probeToUse)
{
    worker = std::jthread {[this](std::stop_token stopToken) { run(stopToken); }};
}

Generator::~Generator()
{
    worker.request_stop();
    wake.notify_all();
}

void Generator::generate(const SA3Pipeline::Request& request)
{
    {
        auto lock = std::scoped_lock {mutex};

        if (busy)
            return;

        busy = true;
        pending = request;
    }

    wake.notify_all();
}

void Generator::run(std::stop_token stopToken)
{
    reportFootprint({.stage = Stage::Loading}, "at launch");

    auto& device = GPU::Device::shared();

    if (!device.isValid())
    {
        report(failure("No GPU device available."));
        return;
    }

    auto model = std::unique_ptr<SA3Pipeline::Model> {};

    try
    {
        auto files = SA3Pipeline::fetchCheckpoints(SA3Checkpoints::smallMusic);
        model = SA3Pipeline::load(
            files,
            "small",
            device,
            [this](const std::string& part)
            { reportFootprint({.stage = Stage::Loading}, "after " + part); });
    }
    catch (const std::exception& error)
    {
        report(failure(error.what()));
        return;
    }

    if (model == nullptr)
    {
        report(failure("Could not load the model."));
        return;
    }

    reportFootprint({.stage = Stage::Ready}, "loaded");

    while (auto request = waitForRequest(stopToken))
    {
        runRequest(*model, *request);

        auto lock = std::scoped_lock {mutex};
        busy = false;
    }
}

std::optional<SA3Pipeline::Request>
    Generator::waitForRequest(std::stop_token stopToken)
{
    auto lock = std::unique_lock {mutex};
    wake.wait(lock, stopToken, [this] { return pending.has_value(); });

    if (stopToken.stop_requested())
        return {};

    return std::exchange(pending, std::nullopt);
}

void Generator::runRequest(SA3Pipeline::Model& model, SA3Pipeline::Request request)
{
    auto steps = request.samplerSteps;

    probe.reset();
    report({.stage = Stage::Sampling, .step = 0, .steps = steps});

    request.afterEncoding = [this, steps]
    {
        reportFootprint({.stage = Stage::Sampling, .step = 0, .steps = steps},
                        "after prompt encode");
    };

    request.probe = probe.stepProbe();

    request.onStep = [this](int done, int total)
    {
        probe.stepFinished(done, total);
        report({.stage = Stage::Sampling, .step = done, .steps = total});
    };

    request.afterSampling =
        [this, steps](const SA3DiT::Weights&, const ML::Tensor&, int)
    {
        probe.samplingFinished();
        reportFootprint({.stage = Stage::Decoding, .step = steps, .steps = steps},
                        "after sampling");
    };

    try
    {
        auto& device = GPU::Device::shared();
        auto waveform = SA3Pipeline::generate(model, request, device);
        auto afterDecode = SA3App::reportFootprint("after decode");
        auto wav = outputFile();

        if (!SA3Codec::writeWavFile(wav.str(), waveform, SA3Pipeline::sampleRate))
        {
            report(failure("Could not write " + wav.str()));
            return;
        }

        deliver({.wav = wav, .peaks = peaksOf(waveform, peakCount)});
        report({.stage = Stage::Done,
                .step = steps,
                .steps = steps,
                .footprint = afterDecode});
    }
    catch (const std::exception& error)
    {
        report(failure(error.what()));
    }
}

void Generator::report(const Status& status)
{
    Threads::callAsync(
        [alive = std::weak_ptr {self}, status]
        {
            if (auto generator = alive.lock())
                (*generator)->onStatus(status);
        });
}

void Generator::reportFootprint(Status status, const std::string& label)
{
    status.footprint = SA3App::reportFootprint(label);
    report(status);
}

void Generator::deliver(Result result)
{
    Threads::callAsync(
        [alive = std::weak_ptr {self}, result = std::move(result)]
        {
            if (auto generator = alive.lock())
                (*generator)->onResult(result);
        });
}
} // namespace eacp::SA3App
