#include "ExpertPanel.h"

#include <eacp/Core/ObjC/ObjC.h>
#include <eacp/Core/ObjC/Strings.h>

#import <UIKit/UIKit.h>

#include <cmath>
#include <string>

namespace eacp::SA3App
{
namespace
{
constexpr auto disclosureHeight = 34.f;
constexpr auto rowHeight = 42.f;
constexpr auto rowCount = 5;
constexpr auto titleWidth = 118.f;
constexpr auto valueWidth = 64.f;
constexpr auto maxSteps = 50;
constexpr auto minSeconds = 1.f;
constexpr auto maxSeconds = 30.f;
constexpr auto maxConditioningSeconds = 60.f;

UIColor* accent()
{
    return [UIColor colorWithRed:1.0 green:0.62 blue:0.3 alpha:1.0];
}

UILabel* makeLabel(const char* text, float alpha)
{
    auto* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.text = @(text);
    label.textColor = [UIColor colorWithWhite:1.0 alpha:alpha];
    label.font = [UIFont monospacedSystemFontOfSize:14 weight:UIFontWeightRegular];
    return label;
}

UISlider* makeSlider(float minimum, float maximum, float value)
{
    auto* slider = [[UISlider alloc] initWithFrame:CGRectZero];
    slider.minimumValue = minimum;
    slider.maximumValue = maximum;
    slider.value = value;
    slider.minimumTrackTintColor = accent();
    return slider;
}

float snappedToHalf(float value)
{
    return std::round(value * 2.f) * 0.5f;
}

NSString* secondsText(float seconds)
{
    return [NSString stringWithFormat:@"%.1f s", seconds];
}

NSString* seedText(std::uint64_t seed)
{
    return Strings::toNSString(std::to_string(seed));
}

void addHandler(UIControl* control,
                UIControlEvents events,
                std::function<void()> handler)
{
    auto* action = [UIAction actionWithHandler:^(UIAction*) {
      handler();
    }];
    [control addAction:action forControlEvents:events];
}
} // namespace

struct ExpertPanel::Native
{
    Native(ExpertPanel& ownerToUse,
           Graphics::View& parent,
           const ExpertSettings& initial)
        : owner(ownerToUse)
        , disclosure([[UIButton buttonWithType:UIButtonTypeSystem] retain])
        , rows([[UIView alloc] initWithFrame:CGRectZero])
        , stepsValue(makeLabel("", 0.9f))
        , stepper([[UIStepper alloc] initWithFrame:CGRectZero])
        , lengthValue(makeLabel("", 0.9f))
        , length(makeSlider(minSeconds, maxSeconds, initial.seconds))
        , conditioningValue(makeLabel("", 0.9f))
        , matchLength([[UISwitch alloc] initWithFrame:CGRectZero])
        , conditioning(makeSlider(
              minSeconds, maxConditioningSeconds, initial.conditioningSeconds))
        , seedField([[UITextField alloc] initWithFrame:CGRectZero])
        , seedMode([[UISegmentedControl alloc]
              initWithItems:@[ @"Fixed", @"Random" ]])
    {
        auto* view = (UIView*) parent.getHandle();

        disclosure.get().tintColor = accent();
        disclosure.get().contentHorizontalAlignment =
            UIControlContentHorizontalAlignmentLeft;
        disclosure.get().titleLabel.font = [UIFont systemFontOfSize:15
                                                             weight:UIFontWeightSemibold];
        addHandler(disclosure.get(), UIControlEventTouchUpInside, [this] { toggle(); });

        stepper.get().minimumValue = 1;
        stepper.get().maximumValue = maxSteps;
        stepper.get().value = initial.samplerSteps;
        stepper.get().tintColor = accent();

        matchLength.get().on = initial.conditioningMatchesLength;
        matchLength.get().onTintColor = accent();

        seedField.get().text = seedText(initial.seed);
        seedField.get().keyboardType = UIKeyboardTypeNumberPad;
        seedField.get().keyboardAppearance = UIKeyboardAppearanceDark;
        seedField.get().textColor = UIColor.whiteColor;
        seedField.get().font = [UIFont monospacedSystemFontOfSize:15
                                                           weight:UIFontWeightRegular];
        seedField.get().backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.12];
        seedField.get().layer.cornerRadius = 8;
        seedField.get().textAlignment = NSTextAlignmentCenter;

        seedMode.get().selectedSegmentIndex = initial.randomSeed ? 1 : 0;
        seedMode.get().selectedSegmentTintColor = accent();

        for (auto* control: {(UIControl*) stepper.get(),
                             (UIControl*) length.get(),
                             (UIControl*) conditioning.get(),
                             (UIControl*) matchLength.get(),
                             (UIControl*) seedMode.get()})
            addHandler(control, UIControlEventValueChanged, [this] { refresh(); });

        addRow(0, "Steps", stepsValue.get(), stepper.get());
        addRow(1, "Length", lengthValue.get(), length.get());
        addRow(2, "Told length", conditioningValue.get(), matchLength.get());
        addRow(3, "", nullptr, conditioning.get());
        addRow(4, "Seed", seedField.get(), seedMode.get());

        rows.get().hidden = YES;
        [view addSubview:disclosure.get()];
        [view addSubview:rows.get()];
        refresh();
    }

    ~Native()
    {
        [disclosure.get() removeFromSuperview];
        [rows.get() removeFromSuperview];
    }

    struct Row
    {
        UILabel* title;
        UIView* value;
        UIView* control;
    };

    void addRow(int index, const char* title, UIView* value, UIView* control)
    {
        auto row = Row {makeLabel(title, 0.6f), value, control};
        [rows.get() addSubview:[row.title autorelease]];

        if (value != nullptr)
            [rows.get() addSubview:value];

        [rows.get() addSubview:control];
        rowViews[index] = row;
    }

    void toggle()
    {
        isOpen = !isOpen;
        rows.get().hidden = !isOpen;

        if (!isOpen)
            dismissKeyboard();

        refresh();
        owner.onToggled();
    }

    void refresh()
    {
        auto* title = isOpen ? @"Expert  ▾" : @"Expert  ▸";
        [disclosure.get() setTitle:title forState:UIControlStateNormal];

        length.get().value = snappedToHalf(length.get().value);
        conditioning.get().value = snappedToHalf(conditioning.get().value);

        auto matching = (bool) matchLength.get().on;
        auto told = matching ? length.get().value : conditioning.get().value;

        stepsValue.get().text =
            [NSString stringWithFormat:@"%d", (int) stepper.get().value];
        lengthValue.get().text = secondsText(length.get().value);
        conditioningValue.get().text = secondsText(told);
        conditioning.get().enabled = !matching;
        conditioning.get().alpha = matching ? 0.3 : 1.0;

        auto random = seedMode.get().selectedSegmentIndex == 1;
        seedField.get().enabled = !random;
        seedField.get().alpha = random ? 0.5 : 1.0;
    }

    void layOut(float x, float y, float width)
    {
        disclosure.get().frame = CGRectMake(x, y, width, disclosureHeight);
        rows.get().frame =
            CGRectMake(x, y + disclosureHeight, width, rowHeight * rowCount);

        for (auto index = 0; index < rowCount; ++index)
            layOutRow(rowViews[index], index * rowHeight, width);

        auto* superview = rows.get().superview;
        [superview bringSubviewToFront:disclosure.get()];
        [superview bringSubviewToFront:rows.get()];
    }

    void layOutRow(const Row& row, float top, float width)
    {
        auto centre = top + rowHeight * 0.5f;
        row.title.frame = CGRectMake(0, top, titleWidth, rowHeight);

        auto controlX = titleWidth;

        if (row.value != nullptr)
        {
            auto isField = row.value == seedField.get();
            auto fieldWidth = isField ? 110.f : valueWidth;
            row.value.frame =
                CGRectMake(controlX, centre - 15.f, fieldWidth, 30.f);
            controlX += fieldWidth + 10.f;
        }

        auto size = [row.control sizeThatFits:CGSizeMake(width, rowHeight)];
        auto stretches = [row.control isKindOfClass:[UISlider class]];
        auto controlWidth = stretches ? width - controlX : size.width;
        auto left = stretches ? controlX : width - controlWidth;

        row.control.frame = CGRectMake(
            left, centre - size.height * 0.5f, controlWidth, size.height);
    }

    float height() const
    {
        return disclosureHeight + (isOpen ? rowHeight * rowCount : 0.f);
    }

    ExpertSettings settings() const
    {
        auto settings = ExpertSettings {};
        settings.samplerSteps = (int) stepper.get().value;
        settings.seconds = length.get().value;
        settings.conditioningMatchesLength = matchLength.get().on;
        settings.conditioningSeconds = conditioning.get().value;
        settings.randomSeed = seedMode.get().selectedSegmentIndex == 1;

        auto text = Strings::toStdString(seedField.get().text);
        settings.seed = text.empty() ? 0 : std::strtoull(text.c_str(), nullptr, 10);
        return settings;
    }

    void apply(const ExpertSettings& settings)
    {
        stepper.get().value = settings.samplerSteps;
        length.get().value = settings.seconds;
        matchLength.get().on = settings.conditioningMatchesLength;
        conditioning.get().value = settings.conditioningSeconds;
        seedMode.get().selectedSegmentIndex = settings.randomSeed ? 1 : 0;
        seedField.get().text = seedText(settings.seed);
        refresh();
    }

    void dismissKeyboard() { [seedField.get() resignFirstResponder]; }

    ExpertPanel& owner;
    bool isOpen = false;
    Row rowViews[rowCount] {};

    mutable ObjC::Ptr<UIButton> disclosure;
    mutable ObjC::Ptr<UIView> rows;
    mutable ObjC::Ptr<UILabel> stepsValue;
    mutable ObjC::Ptr<UIStepper> stepper;
    mutable ObjC::Ptr<UILabel> lengthValue;
    mutable ObjC::Ptr<UISlider> length;
    mutable ObjC::Ptr<UILabel> conditioningValue;
    mutable ObjC::Ptr<UISwitch> matchLength;
    mutable ObjC::Ptr<UISlider> conditioning;
    mutable ObjC::Ptr<UITextField> seedField;
    mutable ObjC::Ptr<UISegmentedControl> seedMode;
};

ExpertPanel::ExpertPanel(Graphics::View& parent, const ExpertSettings& initial)
    : impl(*this, parent, initial)
{
}

ExpertPanel::~ExpertPanel() = default;

void ExpertPanel::setFrame(float x, float y, float width)
{
    impl->layOut(x, y, width);
}

float ExpertPanel::height() const
{
    return impl->height();
}

ExpertSettings ExpertPanel::getSettings() const
{
    return impl->settings();
}

void ExpertPanel::setSettings(const ExpertSettings& settings)
{
    impl->apply(settings);
}

void ExpertPanel::setOpen(bool shouldBeOpen)
{
    if (impl->isOpen != shouldBeOpen)
        impl->toggle();
}

void ExpertPanel::showSeed(std::uint64_t seed)
{
    impl->seedField.get().text = seedText(seed);
}

void ExpertPanel::dismissKeyboard()
{
    impl->dismissKeyboard();
}
} // namespace eacp::SA3App
