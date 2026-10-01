#include "Backdrop.h"

#include "Generator.h"

#include <eacp/Core/Utils/Files.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

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

// The Octgrams texture is this fraction of the screen's resolution.
constexpr auto sceneResolution = 0.5f;
constexpr auto sceneFormat = TextureFormat::RGBA8Unorm;
constexpr auto loaderCornerRadius = 18.f;
constexpr auto marchSteps = 99.f;

// A block the step running now has not reached shows the step before's
// activation at this fraction, so the blocks this step has done stand out.
constexpr auto pendingBlockLevel = 0.45f;
constexpr auto twoPi = 6.283185307179586f;
constexpr auto timedFramesPerReport = 120;

TextureDescriptor describeWaveform()
{
    auto descriptor = TextureDescriptor {};
    descriptor.width = Generator::peakCount;
    descriptor.height = 1;
    descriptor.format = TextureFormat::R8Unorm;
    return descriptor;
}

TextureDescriptor describeStrip(int width)
{
    auto descriptor = TextureDescriptor {};
    descriptor.width = width;
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

void OctgramsShader::define()
{
    auto position = vertexInput(&QuadVertex::position);
    auto uv = varying(position * 0.5f + 0.5f);

    setPosition(float4(position, 0.f, 1.f));

    auto gTime = var(0.f);
    auto breathing = mix(constant(1.f), breath, drive);

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
        auto box1 = box(pos, 2.f - abs(sin(gTime.get() * 0.4f)) * 1.5f * breathing);
        pos = pos_origin;
        pos = withY(pos, pos.y() - sin(gTime.get() * 0.4f) * 2.5f);
        pos = withXY(pos, pos.xy() * rot(constant(.8f)));
        auto box2 = box(pos, 2.f - abs(sin(gTime.get() * 0.4f)) * 1.5f * breathing);
        pos = pos_origin;
        pos = withX(pos, pos.x() + sin(gTime.get() * 0.4f) * 2.5f);
        pos = withXY(pos, pos.xy() * rot(constant(.8f)));
        auto box3 = box(pos, 2.f - abs(sin(gTime.get() * 0.4f)) * 1.5f * breathing);
        pos = pos_origin;
        pos = withX(pos, pos.x() - sin(gTime.get() * 0.4f) * 2.5f);
        pos = withXY(pos, pos.xy() * rot(constant(.8f)));
        auto box4 = box(pos, 2.f - abs(sin(gTime.get() * 0.4f)) * 1.5f * breathing);
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
    auto angle = atan2(p.y(), p.x()) * (1.f / twoPi) + 0.5f;
    auto energy = sample(channels, float2(angle, constant(0.5f))).x();
    p = p * (1.f + drive * (energy - 0.5f) * 0.35f);
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

             auto layer = floor(toFloat(i.get()) * layerCount / marchSteps);
             auto layerU = (layer + 0.5f) / (float) ForwardProbe::maxBlocks;
             auto activation = sample(layers, float2(layerU, constant(0.5f))).x();
             auto layerGain = mix(constant(1.f), activation * 1.6f, drive);

             d = max(abs(d), 0.01f);
             ac += exp(-d * 23.f) * layerGain;

             t += d * 0.55f;
             i += 1;
         });

    auto col = float3(ac.get() * 0.02f, ac.get() * 0.02f, ac.get() * 0.02f);
    auto tint = mix(float3(constant(0.45f), 0.65f, 1.15f),
                    float3(constant(1.15f), 0.7f, 0.35f),
                    energy);
    col = col * mix(float3(constant(1.f), 1.f, 1.f), tint, drive);

    auto base =
        float3(constant(0.f), 0.2f * abs(sin(iTime)), 0.5f + sin(iTime) * 0.2f);
    auto denoisedBase = float3(
        0.3f + sin(iTime) * 0.1f, 0.08f + 0.06f * abs(sin(iTime)), constant(0.04f));
    col = col + mix(base, denoisedBase, drive * (1.f - timestep));

    setFragment(float4(col, 1.f - t.get() * (0.02f + 0.02f * sin(iTime))));
}

OctgramsShader::OctgramsShader()
{
    layers.sampling = {TextureFilter::Nearest, TextureAddressMode::Clamp};
    channels.sampling = {TextureFilter::Linear, TextureAddressMode::Repeat};
    compile();
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

    auto point = float2(x * viewSize.x(), (1.f - y) * viewSize.y());
    auto origin = float2(area.x(), area.y());
    auto size = float2(area.z(), area.w());
    auto local = (point - origin) / size;

    auto q =
        abs(point - (origin + size * 0.5f)) - (size * 0.5f - loaderCornerRadius);
    auto edge =
        length(max(q, 0.f)) + min(max(q.x(), q.y()), 0.f) - loaderCornerRadius;
    auto inLoader = (1.f - smoothstep(-1.f, 1.f, edge)) * loader;

    auto background = mix(rgb(0.02f, 0.025f, 0.06f), rgb(0.05f, 0.07f, 0.15f), y);
    auto sceneColor = sample(scene, local).xyz();
    auto color = mix(background, sceneColor, inLoader);

    auto bar = (1.f - step(progress, x)) * (1.f - step(0.012f, y));
    color = mix(color, rgb(1.f, 0.62f, 0.3f), bar * busy);

    auto inArea = 1.f - smoothstep(-1.f, 1.f, edge);
    auto amplitude = sample(waveform, float2(local.x(), constant(0.5f))).x();
    auto distance = abs(local.y() - waveformCentre) * (1.f / waveformHalfHeight);
    auto inside =
        (1.f - smoothstep(amplitude, amplitude + 0.04f, distance)) * inArea;
    auto played = 1.f - step(playhead, local.x());
    auto waveColor = mix(rgb(0.45f, 0.65f, 1.f), rgb(1.f, 0.82f, 0.45f), played);
    color = mix(color, waveColor, inside * hasAudio * (1.f - inLoader));

    setFragment(float4(color, 1.f));
}

Backdrop::Backdrop(const ForwardProbe& probeToUse)
    : probe(probeToUse)
    , waveform(Device::shared().makeTexture(describeWaveform()))
    , layers(Device::shared().makeTexture(describeStrip(ForwardProbe::maxBlocks)))
    , channels(Device::shared().makeTexture(describeStrip(SA3DiT::ioChannels)))
{
    auto silence = std::vector<std::uint8_t>(Generator::peakCount, 0);
    waveform.update(silence.data());

    if (auto* check = std::getenv("SA3_OCTGRAMS_CHECK"))
        checkTime = std::strtof(check, nullptr);

    setSampleCount(1);

    octgrams.layers = layers;
    octgrams.channels = channels;
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

void Backdrop::setLoaderArea(const Graphics::Rect& area)
{
    loaderArea = area;
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
    follow(probe.state(), frameTime.delta);
    reportFrameTime(frameTime.delta);
}

// The model's numbers into the shader's: README.md's iOS section has the table.
void Backdrop::follow(const ForwardState& state, double seconds)
{
    auto hasModel = busyTarget > 0.f && (state.sampling || state.steps > 0);
    drive = approach(drive, hasModel ? 1.f : 0.f, 4.f, seconds);

    auto& reference =
        state.lastBlockEnergy.empty() ? state.blockEnergy : state.lastBlockEnergy;
    auto low = std::numeric_limits<float>::max();
    auto high = 0.f;

    for (auto energy: reference)
        if (energy > 0.f)
        {
            low = std::min(low, std::log(energy));
            high = std::max(high, std::log(energy));
        }

    auto strip = std::vector<std::uint8_t>(ForwardProbe::maxBlocks, 0);
    auto activity = 0.f;
    auto reached = 0;

    auto levelOf = [&](float energy)
    {
        auto spread = std::max(high - low, 1e-3f);
        return 0.15f
               + 0.85f * std::clamp((std::log(energy) - low) / spread, 0.f, 1.f);
    };

    for (auto block = 0; block < (int) state.blockEnergy.size(); ++block)
    {
        auto index = (std::size_t) block;
        auto energy = state.blockEnergy[index];
        auto level = 0.f;

        if (energy > 0.f)
        {
            level = levelOf(energy);
            activity = level;
            ++reached;
        }
        else if (index < state.lastBlockEnergy.size())
            level = levelOf(state.lastBlockEnergy[index]) * pendingBlockLevel;

        strip[index] = (std::uint8_t) std::lround(level * 255.f);
    }

    if (state.sampling && reached > 0 && reached < (int) state.blockEnergy.size())
        ++partialStepFrames;

    layers.update(strip.data());
    layerCount = std::max(1, (int) state.blockEnergy.size());

    auto energies = std::vector<std::uint8_t>(SA3DiT::ioChannels, 128);

    if (!state.channelEnergy.empty())
    {
        auto loudest = *std::max_element(state.channelEnergy.begin(),
                                         state.channelEnergy.end());

        for (auto c = 0; c < SA3DiT::ioChannels; ++c)
            energies[(std::size_t) c] = (std::uint8_t) std::lround(
                255.f * std::sqrt(state.channelEnergy[(std::size_t) c] / loudest));
    }

    channels.update(energies.data());

    breath = std::clamp(1.f + 1.5f * (state.latentRms - 1.f), 0.5f, 1.1f);
    timestep = state.timestep;

    auto rate = 1.f + drive * (0.35f + 3.f * activity - 1.f);
    time += rate * (float) seconds;
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
                "(%d of %d frames timed), %d frames saw a step part-done\n",
                sceneWidth,
                sceneHeight,
                1000.0 * frameSeconds / timedFrames,
                gpuTimedFrames > 0 ? sceneMilliseconds / gpuTimedFrames : 0.0,
                gpuTimedFrames,
                timedFrames,
                partialStepFrames);
    sceneMilliseconds = 0.0;
    frameSeconds = 0.0;
    gpuTimedFrames = 0;
    timedFrames = 0;
    partialStepFrames = 0;
}

void Backdrop::renderScene(Frame& frame)
{
    auto scale = frame.backingScale() * sceneResolution;
    resizeScene((int) std::lround(loaderArea.w * scale),
                (int) std::lround(loaderArea.h * scale));

    octgrams.iResolution = {(float) sceneWidth, (float) sceneHeight};
    octgrams.iTime = time;
    octgrams.drive = drive;
    octgrams.breath = breath;
    octgrams.timestep = timestep;
    octgrams.layerCount = (float) layerCount;

    if (checkTime >= 0.f)
    {
        octgrams.iTime = checkTime;
        octgrams.drive = 0.f;
    }

    auto descriptor = RenderPassDescriptor {};
    descriptor.clearColor = Graphics::Color {0.f, 0.f, 0.f};
    descriptor.label = "octgrams";

    auto pass = frame.beginPass(*scene, descriptor);
    pass.draw(octgrams);
}

// SA3_OCTGRAMS_CHECK=<iTime> writes one frame of the bare port, RGBA, to the
// app's cache directory, for comparing against the GLSL.
void Backdrop::writeCheckImage(Frame& frame)
{
    frame.flush();

    auto pixels =
        std::vector<std::uint8_t>((std::size_t) sceneWidth * sceneHeight * 4);
    scene->read(pixels.data());

    auto directory = FilePath::appCacheDirectory();
    Files::createDirectories(directory);
    auto path = directory
                / ("octgrams-" + std::to_string(sceneWidth) + "x"
                   + std::to_string(sceneHeight) + ".rgba");
    Files::writeFile(path, {pixels.data(), pixels.size()});
    std::printf("octgrams check written to %s\n", path.str().c_str());
    checkTime = -1.f;
}

void Backdrop::render(Frame& frame)
{
    auto size = frame.logicalSize();
    auto showsLoader = busyLevel > 0.001f || checkTime >= 0.f;

    if (showsLoader && loaderArea.w > 0.f && loaderArea.h > 0.f)
    {
        renderScene(frame);

        if (checkTime >= 0.f)
            writeCheckImage(frame);
    }

    shader.area = {loaderArea.x, loaderArea.y, loaderArea.w, loaderArea.h};
    shader.viewSize = {size.x, size.y};
    shader.loader = scene.has_value() ? busyLevel : 0.f;
    shader.busy = busyLevel;
    shader.progress = progress;
    shader.playhead = playhead;
    shader.hasAudio = audioLevel;

    auto pass = frame.beginPass({Graphics::Color {0.f, 0.f, 0.f}});
    pass.draw(shader);
}
} // namespace eacp::SA3App
