#pragma once

#include "../spdlog.h"

namespace spdlog {
namespace sinks {

class basic_file_sink_mt : public sink {
public:
    basic_file_sink_mt(const std::string&, bool = true) {}
};

} // namespace sinks
} // namespace spdlog
