#include "Backdrop.h"

#include "Generator.h"

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

constexpr auto waveformCentre = 0.5f;
constexpr auto waveformHalfHeight = 0.42f;
constexpr auto loaderCornerRadius = 18.f;

TextureDescriptor describeWaveform()
{
    auto descriptor = TextureDescriptor {};
    descriptor.width = Generator::peakCount;
    descriptor.height = 1;
    descriptor.format = TextureFormat::R8Unorm;
    return descriptor;
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

    auto point = float2(x * viewSize.x(), (1.f - y) * viewSize.y());
    auto origin = float2(area.x(), area.y());
    auto size = float2(area.z(), area.w());
    auto local = (point - origin) / size;

    auto q =
        abs(point - (origin + size * 0.5f)) - (size * 0.5f - loaderCornerRadius);
    auto edge =
        length(max(q, 0.f)) + min(max(q.x(), q.y()), 0.f) - loaderCornerRadius;
    auto inArea = 1.f - smoothstep(-1.f, 1.f, edge);
    auto inLoader = inArea * busy;

    auto background = mix(rgb(0.02f, 0.025f, 0.06f), rgb(0.05f, 0.07f, 0.15f), y);
    auto color = mix(background, rgb(0.07f, 0.09f, 0.19f), inLoader);

    auto bar = (1.f - step(progress, x)) * (1.f - step(0.012f, y));
    color = mix(color, rgb(1.f, 0.62f, 0.3f), bar * busy);

    auto amplitude = sample(waveform, float2(local.x(), constant(0.5f))).x();
    auto distance = abs(local.y() - waveformCentre) * (1.f / waveformHalfHeight);
    auto inside =
        (1.f - smoothstep(amplitude, amplitude + 0.04f, distance)) * inArea;
    auto played = 1.f - step(playhead, local.x());
    auto waveColor = mix(rgb(0.45f, 0.65f, 1.f), rgb(1.f, 0.82f, 0.45f), played);
    color = mix(color, waveColor, inside * hasAudio * (1.f - inLoader));

    setFragment(float4(color, 1.f));
}

Backdrop::Backdrop()
    : waveform(Device::shared().makeTexture(describeWaveform()))
{
    auto silence = std::vector<std::uint8_t>(Generator::peakCount, 0);
    waveform.update(silence.data());

    setSampleCount(1);

    shader.setVertices(fullQuad);
    shader.waveform = waveform;
    shader.prepare(sampleCount());
}

void Backdrop::changed()
{
    renderNow();
}

void Backdrop::setLoaderArea(const Graphics::Rect& area)
{
    loaderArea = area;
    changed();
}

void Backdrop::setBusy(bool isBusy)
{
    if (busy == isBusy)
        return;

    busy = isBusy;
    changed();
}

void Backdrop::setProgress(float fraction)
{
    if (progress == fraction)
        return;

    progress = fraction;
    changed();
}

void Backdrop::setPlayhead(float fraction)
{
    if (playhead == fraction)
        return;

    playhead = fraction;
    changed();
}

void Backdrop::setPeaks(const std::vector<std::uint8_t>& peaks)
{
    waveform.update(peaks.data());
    hasAudio = true;
    changed();
}

void Backdrop::render(Frame& frame)
{
    auto size = frame.logicalSize();

    shader.area = {loaderArea.x, loaderArea.y, loaderArea.w, loaderArea.h};
    shader.viewSize = {size.x, size.y};
    shader.busy = busy ? 1.f : 0.f;
    shader.progress = progress;
    shader.playhead = playhead;
    shader.hasAudio = hasAudio ? 1.f : 0.f;

    auto pass = frame.beginPass({Graphics::Color {0.f, 0.f, 0.f}});
    pass.draw(shader);
}
} // namespace eacp::SA3App
