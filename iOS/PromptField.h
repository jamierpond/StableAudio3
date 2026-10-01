#pragma once

#include <eacp/Core/Utils/Pimpl.h>
#include <eacp/Graphics/View/View.h>

#include <functional>
#include <string>

namespace eacp::SA3App
{
// A UITextField inside a view: eacp's TextInput takes no keyboard on iOS,
// since its native view is never a first responder.
class PromptField
{
public:
    PromptField(Graphics::View& parent, const std::string& initialText);
    ~PromptField();

    std::string getText() const;
    void setText(const std::string& text);
    void setBounds(const Graphics::Rect& bounds);
    void dismissKeyboard();

    std::function<void()> onSubmit = [] {};

private:
    struct Native;
    Pimpl<Native> impl;
};
} // namespace eacp::SA3App
