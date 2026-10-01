#include "Backdrop.h"

#include "Generator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

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

// Pixels of the Octgrams texture per point of the screen.
constexpr auto scenePixelsPerPoint = 0.5f;
constexpr auto sceneFormat = TextureFormat::RGBA8Unorm;
constexpr auto timedFramesPerReport = 120;

TextureDescriptor describeWaveform()
{
    auto descriptor = TextureDescriptor {};
    descriptor.width = Generator::peakCount;
    descriptor.height = 1;
    descriptor.format = TextureFormat::R8Unorm;
    return descriptor;
}

TextureDescriptor describeScene(int width, int height)
{
    auto descriptor = TextureDescriptor {};
    descriptor.width = width;
    descriptor.height = height;
    descriptor.format = sceneFormat;
    descriptor.renderTarget = true;
    return descriptor;
}

float approach(float value, float target, float rate, double seconds)
{
    auto amount = std::min(1.f, rate * (float) seconds);
    return value + (target - value) * amount;
}

// GLSL's in-place swizzle writes, which an EDSL value has no place for.
Float3 withX(const Float3& p, const Float& x)
{
    return float3(x, p.y(), p.z());
}

Float3 withY(const Float3& p, const Float& y)
{
    return float3(p.x(), y, p.z());
}

Float3 withXY(const Float3& p, const Float2& xy)
{
    return float3(xy, p.z());
}

Float3 withYZ(const Float3& p, const Float2& yz)
{
    return float3(p.x(), yz);
}

Float2x2 rot(const Float& a)
{
    auto c = cos(a);
    auto s = sin(a);
    return float2x2(float2(c, s), float2(-s, c));
}

Float sdBox(const Float3& p, const Float3& b)
{
    auto q = abs(p) - b;
    return length(max(q, 0.f)) + min(max(q.x(), max(q.y(), q.z())), 0.f);
}
} // namespace

OctgramsShader::OctgramsShader()
{
    compile();
}

void OctgramsShader::define()
{
    auto position = vertexInput(&QuadVertex::position);
    auto uv = varying(position * 0.5f + 0.5f);

    setPosition(float4(position, 0.f, 1.f));

    auto gTime = var(0.f);

    auto box = [&](Float3 pos, const Float& scale)
    {
        pos = pos * scale;
        auto base = sdBox(pos, float3(constant(.4f), .4f, .1f)) / 1.5f;
        pos = withXY(pos, pos.xy() * 5.f);
        pos = withY(pos, pos.y() - 3.5f);
        pos = withXY(pos, pos.xy() * rot(constant(.75f)));
        auto result = -base;
        return result;
    };

    auto box_set = [&](const Float3& pos_origin, const Float&)
    {
        auto pos = pos_origin;
        pos = withY(pos, pos.y() + sin(gTime.get() * 0.4f) * 2.5f);
        pos = withXY(pos, pos.xy() * rot(constant(.8f)));
        auto box1 = box(pos, 2.f - abs(sin(gTime.get() * 0.4f)) * 1.5f);
        pos = pos_origin;
        pos = withY(pos, pos.y() - sin(gTime.get() * 0.4f) * 2.5f);
        pos = withXY(pos, pos.xy() * rot(constant(.8f)));
        auto box2 = box(pos, 2.f - abs(sin(gTime.get() * 0.4f)) * 1.5f);
        pos = pos_origin;
        pos = withX(pos, pos.x() + sin(gTime.get() * 0.4f) * 2.5f);
        pos = withXY(pos, pos.xy() * rot(constant(.8f)));
        auto box3 = box(pos, 2.f - abs(sin(gTime.get() * 0.4f)) * 1.5f);
        pos = pos_origin;
        pos = withX(pos, pos.x() - sin(gTime.get() * 0.4f) * 2.5f);
        pos = withXY(pos, pos.xy() * rot(constant(.8f)));
        auto box4 = box(pos, 2.f - abs(sin(gTime.get() * 0.4f)) * 1.5f);
        pos = pos_origin;
        pos = withXY(pos, pos.xy() * rot(constant(.8f)));
        auto box5 = box(pos, constant(.5f)) * 6.f;
        pos = pos_origin;
        auto box6 = box(pos, constant(.5f)) * 6.f;
        auto result = max(max(max(max(max(box1, box2), box3), box4), box5), box6);
        return result;
    };

    auto map = [&](const Float3& pos, const Float& time)
    {
        auto box_set1 = box_set(pos, time);

        return box_set1;
    };

    auto fragCoord = uv * iResolution;
    auto p = (fragCoord * 2.f - iResolution) / min(iResolution.x(), iResolution.y());
    auto ro = float3(constant(0.f), -0.2f, iTime * 4.f);
    auto ray = normalize(float3(p, 1.5f));
    ray = withXY(ray, ray.xy() * rot(sin(iTime * .03f) * 5.f));
    ray = withYZ(ray, ray.yz() * rot(sin(iTime * .05f) * .2f));
    auto t = var(0.1f);
    auto ac = var(0.f);

    auto i = var(0);

    loop(i < 99,
         [&]
         {
             auto pos = ro + ray * t.get();
             pos = mod(pos - 2.f, 4.f) - 2.f;
             gTime = iTime - toFloat(i.get()) * 0.01f;

             auto d = map(pos, iTime);

             d = max(abs(d), 0.01f);
             ac += exp(-d * 23.f);

             t += d * 0.55f;
             i += 1;
         });

    auto col = float3(ac.get() * 0.02f, ac.get() * 0.02f, ac.get() * 0.02f);

    col = col
          + float3(constant(0.f), 0.2f * abs(sin(iTime)), 0.5f + sin(iTime) * 0.2f);

    setFragment(float4(col, 1.f - t.get() * (0.02f + 0.02f * sin(iTime))));
}

BackdropShader::BackdropShader()
{
    scene.sampling = {TextureFilter::Linear, TextureAddressMode::Clamp};
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

    auto color = sample(scene, float2(x, 1.f - y)).xyz();

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

    setSampleCount(1);

    octgrams.setVertices(fullQuad);
    octgrams.prepare(1,
                     false,
                     PrimitiveTopology::Triangles,
                     BlendMode::None,
                     pixelFormatFor(sceneFormat));

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

void Backdrop::update(Threads::FrameTime frameTime)
{
    busyLevel = approach(busyLevel, busyTarget, 3.f, frameTime.delta);
    audioLevel = approach(audioLevel, hasAudio ? 1.f : 0.f, 4.f, frameTime.delta);
    time += (float) frameTime.delta;
    reportFrameTime(frameTime.delta);
}

void Backdrop::resizeScene(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);

    if (width == sceneWidth && height == sceneHeight)
        return;

    sceneWidth = width;
    sceneHeight = height;
    scene.emplace(Device::shared().makeTexture(describeScene(width, height)));
    shader.scene = *scene;
}

void Backdrop::reportFrameTime(double delta)
{
    for (const auto& pass: Device::shared().lastFrameTimings().passes)
        if (pass.label == "octgrams")
        {
            sceneMilliseconds += pass.milliseconds;
            ++gpuTimedFrames;
        }

    frameSeconds += delta;

    if (++timedFrames < timedFramesPerReport)
        return;

    std::printf("octgrams %dx%d: %.1f ms between frames, %.2f ms on the GPU "
                "(%d of %d frames timed)\n",
                sceneWidth,
                sceneHeight,
                1000.0 * frameSeconds / timedFrames,
                gpuTimedFrames > 0 ? sceneMilliseconds / gpuTimedFrames : 0.0,
                gpuTimedFrames,
                timedFrames);
    sceneMilliseconds = 0.0;
    frameSeconds = 0.0;
    gpuTimedFrames = 0;
    timedFrames = 0;
}

void Backdrop::render(Frame& frame)
{
    auto size = frame.logicalSize();
    resizeScene((int) std::lround(size.x * scenePixelsPerPoint),
                (int) std::lround(size.y * scenePixelsPerPoint));

    octgrams.iResolution = {(float) sceneWidth, (float) sceneHeight};
    octgrams.iTime = time;

    {
        auto descriptor = RenderPassDescriptor {};
        descriptor.clearColor = Graphics::Color {0.f, 0.f, 0.f};
        descriptor.label = "octgrams";

        auto pass = frame.beginPass(*scene, descriptor);
        pass.draw(octgrams);
    }

    shader.busy = busyLevel;
    shader.progress = progress;
    shader.playhead = playhead;
    shader.hasAudio = audioLevel;

    {
        auto pass = frame.beginPass({Graphics::Color {0.f, 0.f, 0.f}});
        pass.draw(shader);
    }
}
} // namespace eacp::SA3App
