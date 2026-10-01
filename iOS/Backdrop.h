#pragma once

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
// through a repeating field of boxes, rendered into a texture smaller than the
// screen.
struct OctgramsShader final : GPU::ShaderProgram
{
    OctgramsShader();

    void define() override;

    GPU::Uniform<GPU::Float2> iResolution;
    GPU::Uniform<GPU::Float> iTime;

    EACP_SHADER(iResolution, iTime)
};

// The Octgrams texture stretched over the screen, a progress bar along the
// bottom, and the last result's waveform with the played part lit.
struct BackdropShader final : GPU::ShaderProgram
{
    BackdropShader();

    void define() override;

    GPU::Uniform<GPU::Texture2D> scene;
    GPU::Uniform<GPU::Texture2D> waveform;
    GPU::Uniform<GPU::Float> busy;
    GPU::Uniform<GPU::Float> progress;
    GPU::Uniform<GPU::Float> playhead;
    GPU::Uniform<GPU::Float> hasAudio;

    EACP_SHADER(scene, waveform, busy, progress, playhead, hasAudio)
};

class Backdrop final : public GPU::GPUView
{
public:
    Backdrop();

    void setBusy(bool isBusy);
    void setProgress(float fraction);
    void setPlayhead(float fraction);
    void setPeaks(const std::vector<std::uint8_t>& peaks);

    void update(Threads::FrameTime time) override;
    void render(GPU::Frame& frame) override;

private:
    void resizeScene(int width, int height);
    void reportFrameTime(double delta);

    GPU::Texture waveform;
    std::optional<GPU::Texture> scene;
    OctgramsShader octgrams;
    BackdropShader shader;

    int sceneWidth = 0;
    int sceneHeight = 0;
    float time = 0.f;
    float busyLevel = 0.f;
    float busyTarget = 0.f;
    float progress = 0.f;
    float playhead = 0.f;
    float audioLevel = 0.f;
    bool hasAudio = false;

    double sceneMilliseconds = 0.0;
    double frameSeconds = 0.0;
    int gpuTimedFrames = 0;
    int timedFrames = 0;
};
} // namespace eacp::SA3App
