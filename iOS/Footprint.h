#pragma once

#include <string>

namespace eacp::SA3App
{
// The process's phys_footprint in MB, which is what jetsam judges an iOS app
// by. Negative when the kernel would not say.
double footprintMegabytes();

// Prints "footprint <label>: N MB" and returns it, for the status line.
std::string reportFootprint(const std::string& label);
} // namespace eacp::SA3App
