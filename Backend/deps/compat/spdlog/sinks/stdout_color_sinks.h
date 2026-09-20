#pragma once

#include "../spdlog.h"

namespace spdlog {
namespace sinks {

class stdout_color_sink_mt : public sink {
public:
    stdout_color_sink_mt() = default;
};

} // namespace sinks
} // namespace spdlog
