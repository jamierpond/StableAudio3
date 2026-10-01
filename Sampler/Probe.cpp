#include "Probe.h"

#include <eacp/GPU/Codegen/KernelCache.h>
#include <eacp/GPU/Frame/ComputePass.h>

namespace eacp::SA3Sampler
{
using namespace eacp::GPU;
using namespace eacp::ML;

MeanSquareColumnsKernel::MeanSquareColumnsKernel()
{
    compile();
}

void MeanSquareColumnsKernel::dispatch(ComputePass& pass,
                                       int rows,
                                       int columns,
                                       int offset)
{
    rowCount = (std::uint32_t) rows;
    columnCount = (std::uint32_t) columns;
    outputOffset = (std::uint32_t) offset;
    pass.dispatch(*this, columns);
}

void MeanSquareColumnsKernel::define()
{
    auto column = threadId();
    auto sum = var(0.f);
    auto row = var(0u);

    loop(row.get() < rowCount,
         [&]
         {
             auto value = input[row.get() * columnCount + column];
             sum = sum.get() + value * value;
             row = row.get() + 1u;
         });

    write(output, outputOffset + column, sum.get() / toFloat(rowCount));
}

MeanOfRunsKernel::MeanOfRunsKernel()
{
    compile();
}

void MeanOfRunsKernel::dispatch(ComputePass& pass, int runs, int length, int offset)
{
    runLength = (std::uint32_t) length;
    outputOffset = (std::uint32_t) offset;
    pass.dispatch(*this, runs);
}

void MeanOfRunsKernel::define()
{
    auto run = threadId();
    auto start = run * runLength;
    auto sum = var(0.f);
    auto index = var(0u);

    loop(index.get() < runLength,
         [&]
         {
             sum = sum.get() + input[start + index.get()];
             index = index.get() + 1u;
         });

    write(output, outputOffset + run, sum.get() / toFloat(runLength));
}

void meanSquareColumns(ComputePass& pass,
                       const Tensor& input,
                       const BufferRange& output,
                       int offset,
                       Device& device)
{
    auto& kernel = sharedKernel<MeanSquareColumnsKernel>(device);
    kernel.input = input;
    kernel.output = output;
    kernel.dispatch(pass, input.rows(), input.cols(), offset);
}

void meanOfRuns(ComputePass& pass,
                const BufferRange& input,
                int runs,
                int length,
                const BufferRange& output,
                int offset,
                Device& device)
{
    auto& kernel = sharedKernel<MeanOfRunsKernel>(device);
    kernel.input = input;
    kernel.output = output;
    kernel.dispatch(pass, runs, length, offset);
}
} // namespace eacp::SA3Sampler
