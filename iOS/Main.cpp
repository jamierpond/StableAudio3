#include "AudioPlayer.h"
#include "Backdrop.h"
#include "Generator.h"
#include "PromptField.h"
#include "Redraw.h"

#include <eacp/Core/App/App.h>
#include <eacp/Graphics/Graphics.h>

#include <chrono>
#include <cstdio>

using namespace eacp;
using namespace eacp::SA3App;

namespace
{
constexpr auto phoneSeconds = 8.f;
constexpr auto phoneSamplerSteps = 8;
constexpr auto margin = 20.f;

using Clock = std::chrono::steady_clock;

struct Layout
{
    Graphics::Rect title;
    Graphics::Rect prompt;
    Graphics::Rect generate;
    Graphics::Rect play;
    Graphics::Rect status;
    Graphics::Rect footprint;
};

Layout layoutFor(const Graphics::Rect& bounds, const Graphics::Insets& safeArea)
{
    auto x = margin + safeArea.left;
    auto width = bounds.w - x - margin - safeArea.right;
    auto y = std::max(safeArea.top, 20.f) + 16.f;
    auto generateWidth = width * 0.62f - 6.f;

    auto layout = Layout {};
    layout.title = {x, y, width, 34.f};
    layout.prompt = {x, y + 48.f, width, 46.f};
    layout.generate = {x, y + 108.f, generateWidth, 52.f};
    layout.play = {
        x + generateWidth + 12.f, y + 108.f, width - generateWidth - 12.f, 52.f};
    layout.status = {x, y + 176.f, width, 22.f};
    layout.footprint = {x, y + 200.f, width, 22.f};
    return layout;
}

std::string describe(const Status& status, double seconds)
{
    char line[160];

    switch (status.stage)
    {
        case Stage::Loading:
            return "Loading the small model...";
        case Stage::Ready:
            return "Ready";
        case Stage::Sampling:
            std::snprintf(line,
                          sizeof(line),
                          "Sampling step %d / %d  -  %.1fs",
                          status.step,
                          status.steps,
                          seconds);
            return line;
        case Stage::Decoding:
            std::snprintf(line, sizeof(line), "Decoding  -  %.1fs", seconds);
            return line;
        case Stage::Done:
            std::snprintf(line, sizeof(line), "Done in %.1fs", seconds);
            return line;
        case Stage::Failed:
            return "Failed: " + status.error;
    }

    return {};
}

bool isWorking(Stage stage)
{
    return stage == Stage::Loading || stage == Stage::Sampling
           || stage == Stage::Decoding;
}

// Title, the two buttons and the status lines, painted over the backdrop.
class Controls final : public Graphics::View
{
public:
    Controls() { setHandlesMouseEvents(); }

    void setLayout(const Layout& newLayout)
    {
        layout = newLayout;
        redraw(*this);
    }

    void setStatus(const std::string& line, const std::string& footprintLine)
    {
        status = line;

        if (!footprintLine.empty())
            footprint = footprintLine;

        redraw(*this);
    }

    void setGenerateEnabled(bool enabled)
    {
        generateEnabled = enabled;
        redraw(*this);
    }

    void setPlayLabel(const std::string& label, bool enabled)
    {
        playLabel = label;
        playEnabled = enabled;
        redraw(*this);
    }

    void paint(Graphics::Context& g) override
    {
        g.setColor(Graphics::Color::white());
        g.drawText("Stable Audio 3", baseline(layout.title, titleFont), titleFont);

        paintButton(g, layout.generate, "Generate", generateEnabled);
        paintButton(g, layout.play, playLabel, playEnabled);

        g.setColor({1.f, 1.f, 1.f, 0.85f});
        g.drawText(status, baseline(layout.status, smallFont), smallFont);
        g.setColor({1.f, 1.f, 1.f, 0.55f});
        g.drawText(footprint, baseline(layout.footprint, smallFont), smallFont);
    }

    void mouseDown(const Graphics::MouseEvent& event) override
    {
        if (layout.generate.contains(event.pos) && generateEnabled)
            onGenerate();
        else if (layout.play.contains(event.pos) && playEnabled)
            onPlay();
        else
            onBackgroundTap();
    }

    std::function<void()> onGenerate = [] {};
    std::function<void()> onPlay = [] {};
    std::function<void()> onBackgroundTap = [] {};

private:
    static Graphics::Point baseline(const Graphics::Rect& area,
                                    const Graphics::Font& font)
    {
        auto ascent = Graphics::TextMetrics::getAscent(font);
        auto descent = Graphics::TextMetrics::getDescent(font);
        return {area.x, area.y + (area.h + ascent - descent) * 0.5f};
    }

    void paintButton(Graphics::Context& g,
                     const Graphics::Rect& area,
                     const std::string& label,
                     bool enabled)
    {
        auto alpha = enabled ? 1.f : 0.35f;

        g.setColor({1.f, 0.62f, 0.3f, 0.9f * alpha});
        g.fillRoundedRect(area, 12.f);

        auto width = Graphics::TextMetrics::measureWidth(label, buttonFont);
        auto position = baseline(area, buttonFont);
        position.x = area.x + (area.w - width) * 0.5f;

        g.setColor({0.08f, 0.04f, 0.04f, alpha});
        g.drawText(label, position, buttonFont);
    }

    Layout layout;
    std::string status;
    std::string footprint;
    std::string playLabel = "Play";
    bool generateEnabled = false;
    bool playEnabled = false;

    Graphics::Font titleFont {
        Graphics::FontOptions().withName("Helvetica-Bold").withSize(28.f)};
    Graphics::Font buttonFont {
        Graphics::FontOptions().withName("Helvetica-Bold").withSize(18.f)};
    Graphics::Font smallFont {
        Graphics::FontOptions().withName("Menlo").withSize(13.f)};
};

class Root final : public Graphics::View
{
public:
    std::function<void()> onLayout = [] {};

    void resized() override
    {
        for (auto* child: getSubviews())
            child->setBounds(getLocalBounds());

        onLayout();
    }

    void safeAreaInsetsChanged() override { resized(); }
};
} // namespace

struct StableAudioApp
{
    StableAudioApp()
    {
        root.addChildren({backdrop, controls});

        root.onLayout = [this] { layOut(); };
        controls.onGenerate = [this] { generate(); };
        controls.onPlay = [this] { togglePlayback(); };
        controls.onBackgroundTap = [this] { prompt.dismissKeyboard(); };
        prompt.onSubmit = [this] { generate(); };

        generator.onStatus = [this](const Status& status) { show(status); };
        generator.onResult = [this](const Result& result) { finished(result); };

        layOut();
    }

    void layOut()
    {
        auto layout = layoutFor(root.getLocalBounds(), root.getSafeAreaInsets());
        controls.setLayout(layout);
        prompt.setBounds(layout.prompt);
    }

    void generate()
    {
        prompt.dismissKeyboard();

        auto request = SA3Pipeline::Request {};
        request.prompt = prompt.getText();
        request.seconds = phoneSeconds;
        request.samplerSteps = phoneSamplerSteps;

        player.stop();
        started = Clock::now();
        controls.setGenerateEnabled(false);
        generator.generate(request);
    }

    void togglePlayback()
    {
        if (player.isPlaying())
            player.stop();
        else
            player.play(wav);

        refreshPlayback();
    }

    void show(const Status& status)
    {
        latest = status;

        auto progress =
            status.steps > 0 ? (float) status.step / (float) status.steps : 0.f;

        backdrop.setBusy(isWorking(status.stage));
        backdrop.setProgress(progress);
        controls.setGenerateEnabled(
            status.stage == Stage::Ready || status.stage == Stage::Done
            || (status.stage == Stage::Failed && wasEverReady));
        wasEverReady = wasEverReady || status.stage == Stage::Ready;
        refreshStatus();
    }

    void finished(const Result& result)
    {
        wav = result.wav;
        backdrop.setPeaks(result.peaks);
        player.play(wav);
        refreshPlayback();
    }

    void refreshStatus()
    {
        auto seconds =
            std::chrono::duration<double> {Clock::now() - started}.count();
        controls.setStatus(describe(latest, seconds), latest.footprint);
        latest.footprint.clear();
    }

    void refreshPlayback()
    {
        backdrop.setPlayhead(player.position());
        controls.setPlayLabel(player.isPlaying() ? "Stop" : "Play", !wav.empty());
    }

    void tick()
    {
        if (latest.stage == Stage::Sampling || latest.stage == Stage::Decoding)
            refreshStatus();

        refreshPlayback();
    }

    Root root;
    Backdrop backdrop;
    Controls controls;
    Graphics::Window window {root};
    PromptField prompt {root, "lofi house loop"};

    AudioPlayer player;
    FilePath wav;
    Status latest;
    bool wasEverReady = false;
    Clock::time_point started = Clock::now();

    Generator generator;
    Threads::Timer timer {[this] { tick(); }, 20};
};

int main()
{
    return Apps::run<StableAudioApp>();
}
