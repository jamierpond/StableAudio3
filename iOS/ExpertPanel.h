#pragma once

#include <eacp/Core/Utils/Pimpl.h>
#include <eacp/Graphics/View/View.h>

#include <cstdint>
#include <functional>

namespace eacp::SA3App
{
// Every knob the small model has, as the request takes them.
struct ExpertSettings
{
    int samplerSteps = 8;
    float seconds = 8.f;
    bool conditioningMatchesLength = true;
    float conditioningSeconds = 8.f;
    bool randomSeed = false;
    std::uint64_t seed = 42;
};

// A disclosure button that opens a column of UIKit controls, one row per knob.
// UIKit for the same reason as PromptField: the seed wants a keyboard.
class ExpertPanel
{
public:
    ExpertPanel(Graphics::View& parent, const ExpertSettings& initial);
    ~ExpertPanel();

    // Lays the panel out from its top-left corner; height() is what it takes,
    // closed or open.
    void setFrame(float x, float y, float width);
    float height() const;

    ExpertSettings getSettings() const;
    void setSettings(const ExpertSettings& settings);
    void setOpen(bool shouldBeOpen);
    void showSeed(std::uint64_t seed);
    void dismissKeyboard();

    std::function<void()> onToggled = [] {};

private:
    struct Native;
    Pimpl<Native> impl;
};
} // namespace eacp::SA3App
