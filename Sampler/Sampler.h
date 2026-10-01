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
    const StepCallback& onStep = [](int, int) {});

ML::Tensor pingpongSample(
    const SA3DiT::Weights& weights,
    const ML::Tensor& crossAttnContext,
    int latentLength,
    float secondsTotal,
    int steps,
    const NoiseSource& noiseSource,
    GPU::Device& device = GPU::Device::shared(),
    const StepCallback& onStep = [](int, int) {});

} // namespace eacp::SA3Sampler
