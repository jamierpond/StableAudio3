#include "ForwardProbe.h"

#include <DiT/Weights.h>
#include <Sampler/Probe.h>

#import <Metal/Metal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace eacp::SA3App
{
using namespace GPU;

namespace
{
constexpr auto scratchFloats = 4096;
constexpr auto channelCount = SA3DiT::ioChannels;
constexpr auto pendingBlock = -1.f;

std::int64_t floatBytes(int count)
{
    return (std::int64_t) count * (std::int64_t) sizeof(float);
}

BufferRange whole(const Buffer& buffer)
{
    return {&buffer, 0, buffer.size()};
}

float rmsOf(const float* meanSquares, int count)
{
    auto sum = 0.0;

    for (auto i = 0; i < count; ++i)
        sum += meanSquares[i];

    return (float) std::sqrt(sum / count);
}
} // namespace

ForwardProbe::ForwardProbe()
    : blocks(Device::shared().makeBuffer(floatBytes(maxBlocks)))
    , scratch(Device::shared().makeBuffer(floatBytes(scratchFloats)))
    , channels(Device::shared().makeBuffer(floatBytes(2 * channelCount)))
{
}

void ForwardProbe::reset()
{
    auto lock = std::scoped_lock {mutex};
    shared = {};
}

SA3Sampler::StepProbe ForwardProbe::stepProbe()
{
    auto probe = SA3Sampler::StepProbe {};

    probe.beforeStep = [this](int step, float timestep)
    { beforeStep(step, timestep); };

    probe.afterBlock = [this](ComputePass& pass, int block, const ML::Tensor& hidden)
    { afterBlock(pass, block, hidden); };

    probe.afterStep = [this](ComputePass& pass, const SA3Sampler::StepTensors& tensors)
    { afterStep(pass, tensors); };

    return probe;
}

void ForwardProbe::beforeStep(int step, float timestep)
{
    auto pending = std::vector<float>(maxBlocks, pendingBlock);
    blocks.update(pending.data(), floatBytes(maxBlocks));

    auto lock = std::scoped_lock {mutex};
    shared.sampling = true;
    shared.step = step;
    shared.timestep = timestep;
}

void ForwardProbe::afterBlock(ComputePass& pass, int block, const ML::Tensor& hidden)
{
    if (block >= maxBlocks || hidden.cols() > scratchFloats)
        return;

    SA3Sampler::meanSquareColumns(pass, hidden, whole(scratch), 0);
    SA3Sampler::meanOfRuns(pass, whole(scratch), 1, hidden.cols(), whole(blocks), block);

    auto lock = std::scoped_lock {mutex};
    depth = std::max(depth, block + 1);
}

void ForwardProbe::afterStep(ComputePass& pass,
                             const SA3Sampler::StepTensors& tensors)
{
    SA3Sampler::meanSquareColumns(pass, tensors.denoised, whole(channels), 0);
    SA3Sampler::meanSquareColumns(pass, tensors.latent, whole(channels), channelCount);
}

void ForwardProbe::stepFinished(int done, int steps)
{
    auto energies = std::vector<float>(2 * channelCount);
    channels.read(energies.data(), floatBytes(2 * channelCount));

    auto count = 0;

    {
        auto lock = std::scoped_lock {mutex};
        count = depth;
    }

    auto blockEnergies = std::vector<float>((std::size_t) count);
    blocks.read(blockEnergies.data(), floatBytes(count));

    auto [low, high] = std::minmax_element(blockEnergies.begin(), blockEnergies.end());
    std::printf("probe step %d/%d: latent rms %.3f, denoised rms %.3f, "
                "block mean square %.3g..%.3g\n",
                done,
                steps,
                rmsOf(energies.data() + channelCount, channelCount),
                rmsOf(energies.data(), channelCount),
                count > 0 ? *low : 0.f,
                count > 0 ? *high : 0.f);

    auto lock = std::scoped_lock {mutex};
    shared.step = done;
    shared.steps = steps;
    shared.denoisedRms = rmsOf(energies.data(), channelCount);
    shared.latentRms = rmsOf(energies.data() + channelCount, channelCount);
    shared.channelEnergy.assign(energies.begin(), energies.begin() + channelCount);
    shared.lastBlockEnergy = blockEnergies;
}

void ForwardProbe::samplingFinished()
{
    auto lock = std::scoped_lock {mutex};
    shared.sampling = false;
}

std::vector<float> ForwardProbe::liveBlocks(int count) const
{
    auto* contents = (const float*) [(id<MTLBuffer>) blocks.nativeBuffer() contents];
    return {contents, contents + count};
}

ForwardState ForwardProbe::state() const
{
    auto lock = std::scoped_lock {mutex};
    auto state = shared;

    if (state.sampling)
        state.blockEnergy = liveBlocks(depth);
    else
        state.blockEnergy = state.lastBlockEnergy;

    return state;
}
} // namespace eacp::SA3App
