/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define data structure manager template ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/color.h>
#include <fmt/core.h>

#include <climits>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "text_format.hpp"
#include "util/util.hpp"

namespace dvlab {

class Logger {
public:
    enum class LogLevel : unsigned short {
        none = 0,
        fatal = 1,
        error = 2,
        warning = 4,
        info = 8,
        debug = 16,
        trace = 32,
    };

    using LogFilter = std::underlying_type<LogLevel>::type;

    inline LogLevel get_log_level() const noexcept { return _log_level; }

    inline void set_log_level(LogLevel level) noexcept { _log_level = level; }

    inline void unmask(LogLevel level) noexcept { _log_filter = _log_filter & ~static_cast<LogFilter>(level); }
    inline void mask(LogLevel level) noexcept { _log_filter = _log_filter | static_cast<LogFilter>(level); }
    inline bool is_masked(LogLevel level) const noexcept { return (_log_filter & static_cast<LogFilter>(level)) != 0; }
    inline bool is_printing(LogLevel level) const noexcept {
        return !is_masked(level) && _log_level >= level;
    }

    inline Logger& indent() noexcept {
        ++_indent_level;
        return *this;
    }
    inline Logger& unindent() noexcept {
        if (_indent_level) --_indent_level;
        return *this;
    }

    inline void print_logs(std::optional<size_t> n_logs = std::nullopt) {
        if (n_logs == std::nullopt) {
            n_logs = _log.size();
        }

        for (size_t i = _log.size() - *n_logs; i < _log.size(); ++i) {
            fmt::println("{}", _log[i]);
        }
    }

    inline static std::string log_level_to_str(LogLevel level) {
        using enum LogLevel;

        switch (level) {
            case none:
                return "none";
            case fatal:
                return "fatal";
            case error:
                return "error";
            case warning:
                return "warning";
            case info:
                return "info";
            case debug:
                return "debug";
            case trace:
            default:
                return "trace";
        }
    }

    inline static std::optional<LogLevel> str_to_log_level(std::string const& str) {
        using enum LogLevel;
        using namespace std::string_literals;
        using dvlab::str::to_lower_string;
        if ("none"s.starts_with(to_lower_string(str))) return none;
        if ("fatal"s.starts_with(to_lower_string(str))) return fatal;
        if ("error"s.starts_with(to_lower_string(str))) return error;
        if ("warning"s.starts_with(to_lower_string(str))) return warning;
        if ("info"s.starts_with(to_lower_string(str))) return info;
        if ("debug"s.starts_with(to_lower_string(str))) return debug;
        if ("trace"s.starts_with(to_lower_string(str))) return trace;

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
    Logger& fatal(fmt::format_string<Args...> fmt, Args&&... args) {
        _log.emplace_back(fmt::format("[{}]{} {}",
                                      fmt_ext::styled_if_ansi_supported("Fatal", fmt::fg(fmt::terminal_color::white) | fmt::bg(fmt::terminal_color::red)),
                                      std::string(_indent_level * _indent_width, ' '),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (is_printing(LogLevel::fatal)) {
            fmt::println(stderr, "{}", _log.back());
        }
        return *this;
    }

    /**
     * @brief print log about errors. Note that this level is reserved for non-fatal errors, and is therefore logged through stdout.
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    Logger& error(fmt::format_string<Args...> fmt, Args&&... args) {
        _log.emplace_back(fmt::format("[{}]{} {}",
                                      fmt_ext::styled_if_ansi_supported("Error", fmt::fg(fmt::terminal_color::red)),
                                      std::string(_indent_level * _indent_width, ' '),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (is_printing(LogLevel::error)) {
            fmt::println(stderr, "{}", _log.back());
        }
        return *this;
    }

    /**
     * @brief print log about warnings.
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    Logger& warning(fmt::format_string<Args...> fmt, Args&&... args) {
        _log.emplace_back(fmt::format("[{}]{} {}",
                                      fmt_ext::styled_if_ansi_supported("Warning", fmt::fg(fmt::terminal_color::yellow)),
                                      std::string(_indent_level * _indent_width, ' '),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (is_printing(LogLevel::warning)) {
            fmt::println(stderr, "{}", _log.back());
        }
        return *this;
    }

    /**
     * @brief print log about notes (info)
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    Logger& info(fmt::format_string<Args...> fmt, Args&&... args) {
        _log.emplace_back(fmt::format("[{}]{} {}",
                                      "Info",
                                      std::string(_indent_level * _indent_width, ' '),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (is_printing(LogLevel::info)) {
            fmt::println(stdout, "{}", _log.back());
        }
        return *this;
    }

    /**
     * @brief print log about debug messages
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    Logger& debug(fmt::format_string<Args...> fmt, Args&&... args) {
        _log.emplace_back(fmt::format("[{}]{} {}",
                                      fmt_ext::styled_if_ansi_supported("Debug", fmt::fg(fmt::terminal_color::green)),
                                      std::string(_indent_level * _indent_width, ' '),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (is_printing(LogLevel::debug)) {
            fmt::println(stdout, "{}", _log.back());
        }
        return *this;
    }

    /**
     * @brief print log about trace messages. These are usually very verbose debug messages
     *
     * @tparam Args
     * @param fmt
     * @param args
     */
    template <typename... Args>
    Logger& trace(fmt::format_string<Args...> fmt, Args&&... args) {
        _log.emplace_back(fmt::format("[{}]{} {}",
                                      fmt_ext::styled_if_ansi_supported("Trace", fmt::fg(fmt::terminal_color::cyan)),
                                      std::string(_indent_level * _indent_width, ' '),
                                      fmt::format(fmt, std::forward<Args>(args)...)));
        if (is_printing(LogLevel::trace)) {
            fmt::println(stdout, "{}", _log.back());
        }
        return *this;
    }

private:
    std::vector<std::string> mutable _log;
    LogLevel _log_level = LogLevel::warning;
    LogFilter _log_filter = 0;
    size_t _indent_level = 0;
    size_t _indent_width = 2;
};

}  // namespace dvlab