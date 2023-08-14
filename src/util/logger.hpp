/****************************************************************************
  FileName     [ logger.hpp ]
  PackageName  [ util ]
  Synopsis     [ Define data structure manager template ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <climits>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "textFormat.hpp"
#include "util/util.hpp"

namespace dvlab_utils {

class Logger {
public:
    enum class LogLevel : unsigned short {
        NONE = 0,
        FATAL = 1,
        ERROR = 2,
        WARNING = 4,
        INFO = 8,
        DEBUG = 16,
        TRACE = 32,
    };

    using LogFilter = std::underlying_type<LogLevel>::type;

    inline Logger() noexcept(false) : _logLevel{LogLevel::WARNING}, _logFilter{0} {}

    inline LogLevel getLogLevel() const noexcept { return _logLevel; }

    inline void setLogLevel(LogLevel level) noexcept { _logLevel = level; }

    inline void unmask(LogLevel level) noexcept { _logFilter = _logFilter & ~static_cast<LogFilter>(level); }
    inline void mask(LogLevel level) noexcept { _logFilter = _logFilter | static_cast<LogFilter>(level); }
    inline bool isMasked(LogLevel level) const noexcept { return (_logFilter & static_cast<LogFilter>(level)) != 0; }
    inline bool isPrinting(LogLevel level) const noexcept {
        return !isMasked(level) && _logLevel >= level;
    }

    inline void printLogs(std::optional<size_t> nLogs = std::nullopt) {
        if (nLogs == std::nullopt) {
            nLogs = _log.size();
        }

        for (size_t i = _log.size() - *nLogs; i < _log.size(); ++i) {
            fmt::println("{}", _log[i]);
        }
    }

    inline static std::string logLevel2Str(LogLevel level) {
        using enum LogLevel;

        switch (level) {
            case NONE:
                return "none";
            case FATAL:
                return "fatal";
            case ERROR:
                return "error";
            case WARNING:
                return "warning";
            case INFO:
                return "info";
            case DEBUG:
                return "debug";
            case TRACE:
            default:
                return "trace";
        }
    }

    inline static std::optional<LogLevel> str2LogLevel(std::string const& str) {
        using enum LogLevel;
        using namespace std::string_literals;
        if ("none"s.starts_with(toLowerString(str))) return NONE;
        if ("fatal"s.starts_with(toLowerString(str))) return FATAL;
        if ("error"s.starts_with(toLowerString(str))) return ERROR;
        if ("warning"s.starts_with(toLowerString(str))) return WARNING;
        if ("info"s.starts_with(toLowerString(str))) return INFO;
        if ("debug"s.starts_with(toLowerString(str))) return DEBUG;
        if ("trace"s.starts_with(toLowerString(str))) return TRACE;

        return std::nullopt;
    }

    /**
     * @brief print log about fatal (irrecoverable) errors. Fatal logs (and only Fatal logs) are logged through the stderr.
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    void fatal(fmt::format_string<Args...> fmt, Args&&... args) {
        namespace TF = TextFormat;

        _log.emplace_back(fmt::format("[{}] {}",
                                      TF::BLACK(TF::BG_RED("Fatal")),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (isPrinting(LogLevel::FATAL)) {
            fmt::println(stderr, "{}", _log.back());
        }
    }

    /**
     * @brief print log about errors. Note that this level is reserved for non-fatal errors, and is therefore logged through stdout.
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    void error(fmt::format_string<Args...> fmt, Args&&... args) {
        namespace TF = TextFormat;

        _log.emplace_back(fmt::format("[{}] {}",
                                      TF::RED("Error"),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (isPrinting(LogLevel::ERROR)) {
            fmt::println(stderr, "{}", _log.back());
        }
    }

    /**
     * @brief print log about warnings.
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    void warning(fmt::format_string<Args...> fmt, Args&&... args) {
        namespace TF = TextFormat;

        _log.emplace_back(fmt::format("[{}] {}",
                                      TF::YELLOW("Warning"),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (isPrinting(LogLevel::WARNING)) {
            fmt::println(stderr, "{}", _log.back());
        }
    }

    /**
     * @brief print log about notes (info)
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    void info(fmt::format_string<Args...> fmt, Args&&... args) {
        _log.emplace_back(fmt::format("[{}] {}",
                                      "Info",
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (isPrinting(LogLevel::INFO)) {
            fmt::println(stderr, "{}", _log.back());
        }
    }

    /**
     * @brief print log about debug messages
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    void debug(fmt::format_string<Args...> fmt, Args&&... args) {
        namespace TF = TextFormat;

        _log.emplace_back(fmt::format("[{}] {}",
                                      TF::GREEN("Debug"),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (isPrinting(LogLevel::DEBUG)) {
            fmt::println(stderr, "{}", _log.back());
        }
    }

    /**
     * @brief print log about trace messages. These are usually very verbose debug messages
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    void trace(fmt::format_string<Args...> fmt, Args&&... args) {
        namespace TF = TextFormat;

        _log.emplace_back(fmt::format("[{}] {}",
                                      TF::CYAN("Trace"),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (isPrinting(LogLevel::TRACE)) {
            fmt::println(stderr, "{}", _log.back());
        }
    }

private:
    std::vector<std::string> mutable _log;
    LogLevel _logLevel;
    LogFilter _logFilter;
};

}  // namespace dvlab_utils