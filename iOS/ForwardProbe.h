#pragma once

#include <eacp/GPU/Buffer/Buffer.h>
#include <Sampler/Sampler.h>

#include <mutex>
#include <vector>

namespace eacp::SA3App
{
// What the sampler is doing, from reductions taken inside its own passes.
struct ForwardState
{
    bool sampling = false;
    int step = 0;
    int steps = 0;
    float timestep = 1.f;

    // After the last finished step: the RMS of the latent the next step starts
    // from, and of the model's estimate of the clean latent.
    float latentRms = 1.f;
    float denoisedRms = 0.f;

    // Mean square of the clean-latent estimate per latent channel
    // (SA3DiT::ioChannels of them), from the last finished step.
    std::vector<float> channelEnergy;

    // Mean square of the hidden state after each transformer block of the step
    // running now, read while the GPU writes it: a block it has not reached is
    // negative. lastBlockEnergy is the whole of the step before.
    std::vector<float> blockEnergy;
    std::vector<float> lastBlockEnergy;
};

// Owns the GPU buffers the reductions land in. stepProbe() and stepFinished()
// are for the thread sampling; state() is for the main thread.
class ForwardProbe
{
public:
    ForwardProbe();

    static constexpr int maxBlocks = 64;

    void reset();
    SA3Sampler::StepProbe stepProbe();
    void stepFinished(int done, int steps);
    void samplingFinished();

    ForwardState state() const;

private:
    void beforeStep(int step, float timestep);
    void afterBlock(GPU::ComputePass& pass, int block, const ML::Tensor& hidden);
    void afterStep(GPU::ComputePass& pass, const SA3Sampler::StepTensors& tensors);
    std::vector<float> liveBlocks(int count) const;

    GPU::Buffer blocks;
    GPU::Buffer scratch;
    GPU::Buffer channels;

    mutable std::mutex mutex;
    ForwardState shared;
    int depth = 0;
};
} // namespace eacp::SA3App
