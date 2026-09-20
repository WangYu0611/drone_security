#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace spdlog {

namespace level {
enum level_enum {
    trace,
    debug,
    info,
    warn,
    err,
    critical,
    off
};
} // namespace level

namespace sinks {
class sink {
public:
    virtual ~sink() = default;
};
} // namespace sinks

class logger {
public:
    using sink_ptr = std::shared_ptr<sinks::sink>;
    using iterator = std::vector<sink_ptr>::const_iterator;

    template <typename It>
    logger(const std::string&, It, It) {}

    template <typename It>
    logger(const std::string&, It begin_it, It end_it, bool)
    {
        (void)begin_it;
        (void)end_it;
    }

    template <typename... Args>
    void log(level::level_enum level, const char* fmt, Args&&...) const
    {
        (void)level;
        output(fmt);
    }

    template <typename... Args> void trace(const char* fmt, Args&&...) const { log(level::trace, fmt); }
    template <typename... Args> void debug(const char* fmt, Args&&...) const { log(level::debug, fmt); }
    template <typename... Args> void info(const char* fmt, Args&&...) const { log(level::info, fmt); }
    template <typename... Args> void warn(const char* fmt, Args&&...) const { log(level::warn, fmt); }
    template <typename... Args> void error(const char* fmt, Args&&...) const { log(level::err, fmt); }
    template <typename... Args> void critical(const char* fmt, Args&&...) const { log(level::critical, fmt); }
    template <typename... Args> void set_level(level::level_enum) const {}
    template <typename... Args> void flush_on(level::level_enum) const {}
    template <typename... Args> void set_pattern(const std::string&) const {}

private:
    template <typename... Args>
    static void output(const std::string& text, Args&&...)
    {
        std::lock_guard<std::mutex> lock(output_mutex());
        std::cout << text << std::endl;
    }

    static std::mutex& output_mutex()
    {
        static std::mutex m;
        return m;
    }
};

inline std::shared_ptr<logger> g_default_logger;

inline void set_default_logger(std::shared_ptr<logger> log) { g_default_logger = std::move(log); }
inline void set_pattern(const std::string&) {}
inline void set_level(level::level_enum) {}
inline void flush_on(level::level_enum) {}

template <typename... Args>
void info(const char* fmt, Args&&...) {
    if (g_default_logger) g_default_logger->info(fmt);
}
template <typename... Args>
void debug(const char* fmt, Args&&...) {
    if (g_default_logger) g_default_logger->debug(fmt);
}
template <typename... Args>
void warn(const char* fmt, Args&&...) {
    if (g_default_logger) g_default_logger->warn(fmt);
}
template <typename... Args>
void error(const char* fmt, Args&&...) {
    if (g_default_logger) g_default_logger->error(fmt);
}

} // namespace spdlog
