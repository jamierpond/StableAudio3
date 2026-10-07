#pragma once

#include <eacp/GPU/GPU.h>

#include <cstdint>
#include <vector>

namespace eacp::SA3App
{
struct QuadVertex
{
    float position[2];
};
} // namespace eacp::SA3App

EACP_SHADER_VALUE(eacp::SA3App::QuadVertex, Float2)

namespace eacp::SA3App
{
// A plain gradient, a flat panel where the waveform goes while the model works,
// a progress bar along the bottom, and the last result's waveform with the
// played part lit.
struct BackdropShader final : GPU::ShaderProgram
{
    BackdropShader();

    void define() override;

    GPU::Uniform<GPU::Texture2D> waveform;
    GPU::Uniform<GPU::Float4> area;
    GPU::Uniform<GPU::Float2> viewSize;
    GPU::Uniform<GPU::Float> busy;
    GPU::Uniform<GPU::Float> progress;
    GPU::Uniform<GPU::Float> playhead;
    GPU::Uniform<GPU::Float> hasAudio;

    EACP_SHADER(waveform, area, viewSize, busy, progress, playhead, hasAudio)
};

// Renders only when something it shows changes, never on a display link, so
// it leaves the GPU to the model.
class Backdrop final : public GPU::GPUView
{
public:
    Backdrop();

    // Where the panel and the waveform go, in points.
    void setLoaderArea(const Graphics::Rect& area);
    void setBusy(bool isBusy);
    void setProgress(float fraction);
    void setPlayhead(float fraction);
    void setPeaks(const std::vector<std::uint8_t>& peaks);

    void render(GPU::Frame& frame) override;

private:
    void changed();

    GPU::Texture waveform;
    BackdropShader shader;

    Graphics::Rect loaderArea;
    bool busy = false;
    float progress = 0.f;
    float playhead = 0.f;
    bool hasAudio = false;
};
} // namespace eacp::SA3App
