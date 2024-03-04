/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define wrapper to system-dependent functions ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cstdio>
#include <optional>
#ifdef _WIN32
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <string>

namespace dvlab {

namespace utils {

inline std::optional<std::string> get_home_directory() {
#ifdef _WIN32
    if (const char* home = std::getenv("USERPROFILE")) {
        return home;
    }
    if (const char* home = std::getenv("HOMEDRIVE")) {
        if (const char* path = std::getenv("HOMEPATH")) {
            return std::string{home} + path;
        }
    }
#else  // UNIX-like
    if (const char* home = std::getenv("HOME")) {
        return home;
    }

    auto passwd = getpwuid(getuid());
    if (passwd != nullptr) {
        return passwd->pw_dir;
    }
#endif
    return std::nullopt;
}

inline void clear_terminal() {
#ifdef _WIN32
    int const result = system("cls");
#else
    int const result = system("clear");
#endif
    if (result != 0) {
        std::perror("Error clearing terminal");
    }
}

inline auto python_package_exists(std::string_view package_name) -> bool {
    return system(fmt::format("python3 -c 'import importlib.util; exit(0 if importlib.util.find_spec(\"{}\") is not None else 1)'", package_name).c_str()) == 0;
};

inline auto pdflatex_exists() -> bool {
    return system("pdflatex --version > /dev/null 2>&1") == 0;
};

// if system(...) returns 0, then qiskit is installed

}  // namespace utils

}  // namespace dvlab
