#include "Redraw.h"

#import <QuartzCore/QuartzCore.h>

namespace eacp::SA3App
{
void redraw(Graphics::View& view)
{
    [(CALayer*) view.getNativeLayer() setNeedsDisplay];
}
} // namespace eacp::SA3App
