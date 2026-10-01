#include "PromptField.h"

#include <eacp/Core/ObjC/ObjC.h>
#include <eacp/Core/ObjC/Strings.h>

#import <UIKit/UIKit.h>

namespace eacp::SA3App
{
namespace
{
UITextField* makeTextField(const std::string& initialText)
{
    auto* field = [[UITextField alloc] initWithFrame:CGRectZero];

    field.text = Strings::toNSString(initialText);
    field.placeholder = @"Describe the music";
    field.textColor = UIColor.whiteColor;
    field.tintColor = [UIColor colorWithRed:1.0 green:0.7 blue:0.4 alpha:1.0];
    field.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.12];
    field.font = [UIFont systemFontOfSize:17];
    field.borderStyle = UITextBorderStyleNone;
    field.layer.cornerRadius = 10;
    field.returnKeyType = UIReturnKeyGo;
    field.autocorrectionType = UITextAutocorrectionTypeNo;
    field.clearButtonMode = UITextFieldViewModeWhileEditing;
    field.leftView = [[[UIView alloc] initWithFrame:CGRectMake(0, 0, 12, 1)]
        autorelease];
    field.leftViewMode = UITextFieldViewModeAlways;
    field.keyboardAppearance = UIKeyboardAppearanceDark;

    return field;
}
} // namespace

struct PromptField::Native
{
    Native(PromptField& owner, Graphics::View& parent, const std::string& initialText)
        : field(makeTextField(initialText))
    {
        auto* ownerPointer = &owner;
        auto* submit = [UIAction actionWithHandler:^(UIAction*) {
          ownerPointer->onSubmit();
        }];

        [field.get() addAction:submit
              forControlEvents:UIControlEventEditingDidEndOnExit];
        [(UIView*) parent.getHandle() addSubview:field.get()];
    }

    ~Native() { [field.get() removeFromSuperview]; }

    mutable ObjC::Ptr<UITextField> field;
};

PromptField::PromptField(Graphics::View& parent, const std::string& initialText)
    : impl(*this, parent, initialText)
{
}

PromptField::~PromptField() = default;

std::string PromptField::getText() const
{
    return Strings::toStdString(impl->field.get().text);
}

void PromptField::setText(const std::string& text)
{
    impl->field.get().text = Strings::toNSString(text);
}

void PromptField::setBounds(const Graphics::Rect& bounds)
{
    impl->field.get().frame = CGRectMake(bounds.x, bounds.y, bounds.w, bounds.h);
    [impl->field.get().superview bringSubviewToFront:impl->field.get()];
}

void PromptField::dismissKeyboard()
{
    [impl->field.get() resignFirstResponder];
}
} // namespace eacp::SA3App
