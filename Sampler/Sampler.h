#pragma once

#include "../DiT/SA3DiT.h"
#include "../DiT/Weights.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace eacp::SA3Sampler
{
using NoiseSource = std::function<std::vector<float>(int count)>;

// Called after each step's commands have finished, with the steps done so far.
using StepCallback = std::function<void(int done, int steps)>;

// What one step made, while its pass is still recording: the model's estimate
// of the clean latent and the latent the next step starts from.
struct StepTensors
{
    const ML::Tensor& denoised;
    const ML::Tensor& latent;
    int step;
    float timestep;
    float nextTimestep;
};

// Ways to watch the sampler work without changing what it computes. beforeStep
// runs on the host before a step records; afterBlock and afterStep record into
// the step's own pass, so what they dispatch is done when onStep is called.
struct StepProbe
{
    std::function<void(int step, float timestep)> beforeStep = [](int, float) {};
    SA3DiT::BlockObserver afterBlock =
        [](GPU::ComputePass&, int, const ML::Tensor&) {};
    std::function<void(GPU::ComputePass&, const StepTensors&)> afterStep =
        [](GPU::ComputePass&, const StepTensors&) {};
};

using ModelForward =
    std::function<ML::Tensor(GPU::ComputePass&, const ML::Tensor&, float timestep)>;

NoiseSource randomNoiseSource(std::uint64_t seed);

ML::Tensor pingpongSampleWithModel(
    const ModelForward& model,
    int latentRows,
    int latentColumns,
    int steps,
    const NoiseSource& noiseSource,
    GPU::Device& device = GPU::Device::shared(),
    const StepCallback& onStep = [](int, int) {},
    const StepProbe& probe = {});

ML::Tensor pingpongSample(
    const SA3DiT::Weights& weights,
    const ML::Tensor& crossAttnContext,
    int latentLength,
    float secondsTotal,
    int steps,
    const NoiseSource& noiseSource,
    GPU::Device& device = GPU::Device::shared(),
    const StepCallback& onStep = [](int, int) {},
    const StepProbe& probe = {});

} // namespace eacp::SA3Sampler
