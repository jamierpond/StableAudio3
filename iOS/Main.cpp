#include "AudioPlayer.h"
#include "Backdrop.h"
#include "ExpertPanel.h"
#include "Generator.h"
#include "PromptField.h"
#include "Redraw.h"

#include <eacp/Core/App/App.h>
#include <eacp/Graphics/Graphics.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <random>
#include <sstream>

using namespace eacp;
using namespace eacp::SA3App;

namespace
{
constexpr auto margin = 20.f;

using Clock = std::chrono::steady_clock;

struct Layout
{
    Graphics::Rect title;
    Graphics::Rect prompt;
    Graphics::Rect expert;
    Graphics::Rect generate;
    Graphics::Rect play;
    Graphics::Rect status;
    Graphics::Rect settings;
    Graphics::Rect footprint;
};

Layout layoutFor(const Graphics::Rect& bounds,
                 const Graphics::Insets& safeArea,
                 float expertHeight)
{
    auto x = margin + safeArea.left;
    auto width = bounds.w - x - margin - safeArea.right;
    auto y = std::max(safeArea.top, 20.f) + 16.f;
    auto generateWidth = width * 0.62f - 6.f;

    auto layout = Layout {};
    layout.title = {x, y, width, 34.f};
    layout.prompt = {x, y + 48.f, width, 46.f};
    layout.expert = {x, y + 100.f, width, expertHeight};

    y += expertHeight + 10.f;
    layout.generate = {x, y + 100.f, generateWidth, 52.f};
    layout.play = {
        x + generateWidth + 12.f, y + 100.f, width - generateWidth - 12.f, 52.f};
    layout.status = {x, y + 168.f, width, 22.f};
    layout.settings = {x, y + 192.f, width, 22.f};
    layout.footprint = {x, y + 216.f, width, 22.f};
    return layout;
}

std::string describe(const SA3Pipeline::Request& request)
{
    char line[160];
    std::snprintf(line,
                  sizeof(line),
                  "%.1fs  %d steps  seed %llu  told %.1fs",
                  request.seconds,
                  request.samplerSteps,
                  (unsigned long long) request.seed,
                  request.secondsTotal());
    return line;
}

// SA3_AUTORUN="steps=4 seed=7 seconds=8 told=4 random=1" opens the Expert
// panel with those values at launch and generates once the model is ready:
// how the simulator run is driven, which has no way to tap.
std::optional<ExpertSettings> autorunSettings()
{
    auto* text = std::getenv("SA3_AUTORUN");

    if (text == nullptr || *text == 0)
        return {};

    auto settings = ExpertSettings {};
    auto words = std::istringstream {text};
    auto word = std::string {};

    while (words >> word)
    {
        auto equals = word.find('=');

        if (equals == std::string::npos)
            continue;

        auto key = word.substr(0, equals);
        auto value = word.substr(equals + 1);

        if (key == "steps")
            settings.samplerSteps = std::stoi(value);
        else if (key == "seed")
            settings.seed = std::stoull(value);
        else if (key == "seconds")
            settings.seconds = std::stof(value);
        else if (key == "random")
            settings.randomSeed = value == "1";
        else if (key == "told")
        {
            settings.conditioningMatchesLength = false;
            settings.conditioningSeconds = std::stof(value);
        }
    }

    return settings;
}

std::uint64_t randomSeed()
{
    return std::random_device {}() % 1000000000u;
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

    void setSettings(const std::string& line)
    {
        settings = line;
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
        g.setColor({1.f, 0.82f, 0.6f, 0.75f});
        g.drawText(settings, baseline(layout.settings, smallFont), smallFont);
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
    std::string settings;
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
        controls.onBackgroundTap = [this]
        {
            prompt.dismissKeyboard();
            expert.dismissKeyboard();
        };
        expert.onToggled = [this] { layOut(); };
        prompt.onSubmit = [this] { generate(); };

        generator.onStatus = [this](const Status& status) { show(status); };
        generator.onResult = [this](const Result& result) { finished(result); };

        if (autorun.has_value())
        {
            expert.setSettings(*autorun);
            expert.setOpen(true);
        }

        layOut();
    }

    void layOut()
    {
        auto layout = layoutFor(
            root.getLocalBounds(), root.getSafeAreaInsets(), expert.height());
        controls.setLayout(layout);
        prompt.setBounds(layout.prompt);
        expert.setFrame(layout.expert.x, layout.expert.y, layout.expert.w);
    }

    void generate()
    {
        prompt.dismissKeyboard();
        expert.dismissKeyboard();

        auto settings = expert.getSettings();
        auto request = SA3Pipeline::Request {};
        request.prompt = prompt.getText();
        request.seconds = settings.seconds;
        request.samplerSteps = settings.samplerSteps;
        request.seed = settings.randomSeed ? randomSeed() : settings.seed;

        if (!settings.conditioningMatchesLength)
            request.conditioningSeconds = settings.conditioningSeconds;

        expert.showSeed(request.seed);
        controls.setSettings(describe(request));

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

        if (status.stage == Stage::Ready && std::exchange(autorun, std::nullopt))
            generate();
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
    ExpertPanel expert {root, ExpertSettings {}};

    AudioPlayer player;
    FilePath wav;
    Status latest;
    bool wasEverReady = false;
    std::optional<ExpertSettings> autorun = autorunSettings();
    Clock::time_point started = Clock::now();

    Generator generator;
    Threads::Timer timer {[this] { tick(); }, 20};
};

int main()
{
    return Apps::run<StableAudioApp>();
}
