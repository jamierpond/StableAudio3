#pragma once

#include <eacp/Graphics/View/View.h>

namespace eacp::SA3App
{
// View::repaint on iOS calls -[UIView setNeedsDisplay], which UIKit ignores for
// a view with no drawRect:, so a view painted through its layer delegate
// never repaints. This marks the layer itself.
void redraw(Graphics::View& view);
} // namespace eacp::SA3App
