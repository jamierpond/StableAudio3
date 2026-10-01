#include "Backdrop.h"

#include "Generator.h"

#include <algorithm>

namespace eacp::SA3App
{
using namespace GPU;

namespace
{
constexpr QuadVertex fullQuad[] = {
    {{-1.f, -1.f}},
    {{+1.f, -1.f}},
    {{-1.f, +1.f}},
    {{+1.f, -1.f}},
    {{+1.f, +1.f}},
    {{-1.f, +1.f}},
};

constexpr auto waveformCentre = 0.32f;
constexpr auto waveformHalfHeight = 0.14f;

TextureDescriptor describeWaveform()
{
    auto descriptor = TextureDescriptor {};
    descriptor.width = Generator::peakCount;
    descriptor.height = 1;
    descriptor.format = TextureFormat::R8Unorm;
    return descriptor;
}

float approach(float value, float target, float rate, double seconds)
{
    auto amount = std::min(1.f, rate * (float) seconds);
    return value + (target - value) * amount;
}
} // namespace

BackdropShader::BackdropShader()
{
    waveform.sampling = {TextureFilter::Linear, TextureAddressMode::Clamp};
    compile();
}

void BackdropShader::define()
{
    auto position = vertexInput(&QuadVertex::position);
    auto uv = varying(position * 0.5f + 0.5f);

    setPosition(float4(position, 0.f, 1.f));

    auto rgb = [this](float r, float g, float b)
    { return float3(constant(r), constant(g), constant(b)); };

    auto x = uv.x();
    auto y = uv.y();

    auto wave = sin(x * 5.f + phase) + sin(y * 4.f - phase * 0.8f)
                + sin((x + y) * 3.f + phase * 1.3f);
    auto shade = wave * (1.f / 6.f) + 0.5f;

    auto low = mix(rgb(0.02f, 0.03f, 0.08f), rgb(0.10f, 0.03f, 0.06f), busy);
    auto high = mix(rgb(0.08f, 0.16f, 0.36f), rgb(0.55f, 0.18f, 0.22f), busy);
    auto color = mix(low, high, shade * 0.7f);

    auto bar = (1.f - step(progress, x)) * (1.f - step(0.012f, y));
    color = mix(color, rgb(1.f, 0.62f, 0.3f), bar * busy);

    auto amplitude = sample(waveform, float2(x, constant(0.5f))).x();
    auto distance = abs(y - waveformCentre) * (1.f / waveformHalfHeight);
    auto inside = 1.f - smoothstep(amplitude, amplitude + 0.04f, distance);
    auto played = 1.f - step(playhead, x);
    auto waveColor = mix(rgb(0.45f, 0.65f, 1.f), rgb(1.f, 0.82f, 0.45f), played);
    color = mix(color, waveColor, inside * hasAudio);

    setFragment(float4(color, 1.f));
}

Backdrop::Backdrop()
    : waveform(Device::shared().makeTexture(describeWaveform()))
{
    auto silence = std::vector<std::uint8_t>(Generator::peakCount, 0);
    waveform.update(silence.data());

    shader.setVertices(fullQuad);
    shader.waveform = waveform;
    shader.prepare(sampleCount());

    setContinuous(true);
}

void Backdrop::setBusy(bool isBusy)
{
    busyTarget = isBusy ? 1.f : 0.f;
}

void Backdrop::setProgress(float fraction)
{
    progress = fraction;
}

void Backdrop::setPlayhead(float fraction)
{
    playhead = fraction;
}

void Backdrop::setPeaks(const std::vector<std::uint8_t>& peaks)
{
    waveform.update(peaks.data());
    hasAudio = true;
}

void Backdrop::update(Threads::FrameTime time)
{
    busyLevel = approach(busyLevel, busyTarget, 3.f, time.delta);
    audioLevel = approach(audioLevel, hasAudio ? 1.f : 0.f, 4.f, time.delta);
    phase += (0.25f + busyLevel * 2.f) * (float) time.delta;
}

void Backdrop::render(Frame& frame)
{
    shader.phase = phase;
    shader.busy = busyLevel;
    shader.progress = progress;
    shader.playhead = playhead;
    shader.hasAudio = audioLevel;

    auto pass = frame.beginPass({Graphics::Color {0.f, 0.f, 0.f}});
    pass.draw(shader);
}
} // namespace eacp::SA3App
