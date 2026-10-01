#pragma once

#include <eacp/GPU/Codegen/ComputeProgram.h>
#include <eacp/ML/Tensor/Tensor.h>

// Small reductions for watching a forward pass from a StepProbe: a few floats
// out of a tensor, written where the caller says, so the host reads those
// rather than the tensor.
namespace eacp::SA3Sampler
{
class MeanSquareColumnsKernel final : public GPU::ComputeProgram
{
public:
    MeanSquareColumnsKernel();

    void dispatch(GPU::ComputePass& pass, int rows, int columns, int offset);

    GPU::Uniform<GPU::InputBuffer> input;
    GPU::Uniform<GPU::OutputBuffer> output;
    GPU::Uniform<GPU::UInt> rowCount;
    GPU::Uniform<GPU::UInt> columnCount;
    GPU::Uniform<GPU::UInt> outputOffset;

    EACP_SHADER(input, output, rowCount, columnCount, outputOffset)

private:
    void define() override;
};

class MeanOfRunsKernel final : public GPU::ComputeProgram
{
public:
    MeanOfRunsKernel();

    void dispatch(GPU::ComputePass& pass, int runs, int length, int offset);

    GPU::Uniform<GPU::InputBuffer> input;
    GPU::Uniform<GPU::OutputBuffer> output;
    GPU::Uniform<GPU::UInt> runLength;
    GPU::Uniform<GPU::UInt> outputOffset;

    EACP_SHADER(input, output, runLength, outputOffset)

private:
    void define() override;
};

// output[offset + c] = mean over rows of input[r][c]^2.
void meanSquareColumns(GPU::ComputePass& pass,
                       const ML::Tensor& input,
                       const GPU::BufferRange& output,
                       int offset,
                       GPU::Device& device = GPU::Device::shared());

// output[offset + r] = mean of input[r * length .. (r + 1) * length).
void meanOfRuns(GPU::ComputePass& pass,
                const GPU::BufferRange& input,
                int runs,
                int length,
                const GPU::BufferRange& output,
                int offset,
                GPU::Device& device = GPU::Device::shared());
} // namespace eacp::SA3Sampler
