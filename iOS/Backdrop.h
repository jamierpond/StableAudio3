#pragma once

#include "ForwardProbe.h"

#include <eacp/GPU/GPU.h>

#include <cstdint>
#include <optional>
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
// "Octgrams" (iOS/Reference/octgrams.glsl), ported to the EDSL: a raymarch
// through a repeating field of boxes. drive brings in the model: at 0 the
// shader is the original exactly, at 1 the terms README.md lists take over.
struct OctgramsShader final : GPU::ShaderProgram
{
    OctgramsShader();

    void define() override;

    GPU::Uniform<GPU::Float2> iResolution;
    GPU::Uniform<GPU::Float> iTime;

    GPU::Uniform<GPU::Float> drive;
    GPU::Uniform<GPU::Float> breath;
    GPU::Uniform<GPU::Float> timestep;
    GPU::Uniform<GPU::Float> layerCount;
    GPU::Uniform<GPU::Texture2D> layers;
    GPU::Uniform<GPU::Texture2D> channels;

    EACP_SHADER(
        iResolution, iTime, drive, breath, timestep, layerCount, layers, channels)
};

// A plain gradient, the Octgrams texture in a rounded rectangle while the model
// works, a progress bar along the bottom, and the last result's waveform with
// the played part lit.
struct BackdropShader final : GPU::ShaderProgram
{
    BackdropShader();

    void define() override;

    GPU::Uniform<GPU::Texture2D> scene;
    GPU::Uniform<GPU::Texture2D> waveform;
    GPU::Uniform<GPU::Float4> area;
    GPU::Uniform<GPU::Float2> viewSize;
    GPU::Uniform<GPU::Float> loader;
    GPU::Uniform<GPU::Float> busy;
    GPU::Uniform<GPU::Float> progress;
    GPU::Uniform<GPU::Float> playhead;
    GPU::Uniform<GPU::Float> hasAudio;

    EACP_SHADER(
        scene, waveform, area, viewSize, loader, busy, progress, playhead, hasAudio)
};

class Backdrop final : public GPU::GPUView
{
public:
    explicit Backdrop(const ForwardProbe& probe);

    // Where the loader shows while the model works, in points.
    void setLoaderArea(const Graphics::Rect& area);
    void setBusy(bool isBusy);
    void setProgress(float fraction);
    void setPlayhead(float fraction);
    void setPeaks(const std::vector<std::uint8_t>& peaks);

    void update(Threads::FrameTime time) override;
    void render(GPU::Frame& frame) override;

private:
    void follow(const ForwardState& state, double seconds);
    void resizeScene(int width, int height);
    void renderScene(GPU::Frame& frame);
    void writeCheckImage(GPU::Frame& frame);
    void reportFrameTime(double delta);

    const ForwardProbe& probe;
    GPU::Texture waveform;
    GPU::Texture layers;
    GPU::Texture channels;
    std::optional<GPU::Texture> scene;
    OctgramsShader octgrams;
    BackdropShader shader;

    Graphics::Rect loaderArea;
    int sceneWidth = 0;
    int sceneHeight = 0;
    float time = 0.f;
    float drive = 0.f;
    float breath = 1.f;
    float timestep = 1.f;
    int layerCount = 1;
    float checkTime = -1.f;
    float busyLevel = 0.f;
    float busyTarget = 0.f;
    float progress = 0.f;
    float playhead = 0.f;
    float audioLevel = 0.f;
    bool hasAudio = false;

    int partialStepFrames = 0;
    double sceneMilliseconds = 0.0;
    double frameSeconds = 0.0;
    int gpuTimedFrames = 0;
    int timedFrames = 0;
};
} // namespace eacp::SA3App
