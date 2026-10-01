#include "Footprint.h"

#include <mach/mach.h>

#include <cstdio>

namespace eacp::SA3App
{
double footprintMegabytes()
{
    auto info = task_vm_info_data_t {};
    auto count = mach_msg_type_number_t {TASK_VM_INFO_COUNT};

    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t) &info, &count)
        != KERN_SUCCESS)
        return -1.0;

    return (double) info.phys_footprint / (1024.0 * 1024.0);
}

std::string reportFootprint(const std::string& label)
{
    char line[128];
    std::snprintf(line,
                  sizeof(line),
                  "footprint %s: %.0f MB",
                  label.c_str(),
                  footprintMegabytes());
    std::printf("%s\n", line);
    std::fflush(stdout);
    return line;
}
} // namespace eacp::SA3App
