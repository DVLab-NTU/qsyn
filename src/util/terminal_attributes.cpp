/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ get the attributes of terminal ]
  Author       [ Adapted from https://stackoverflow.com/a/62485211 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "util/terminal_attributes.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif  // Windows/Linux

#include <cstdlib>
#include <cstring>

namespace dvlab {

namespace utils {

/**
 * @brief check if the file is terminal
 *
 * @param f
 * @return true
 * @return false
 */
bool is_terminal(FILE* f) {
#if defined(_WIN32)
    return _isatty(_fileno(f)) != 0;
#else  // UNIX-like
    return isatty(fileno(f)) != 0;
#endif
}

bool ansi_supported(FILE* f) {
#if defined(_WIN32)
    return true;
#else  // UNIX-like
    char const* term = std::getenv("TERM");
    if (term == nullptr) return false;
    if (strcasecmp(term, "dumb") == 0) return false;
    return is_terminal(f);
#endif
}

/**
 * @brief Get the terminal sizes. Adapted from https://stackoverflow.com/a/62485211
 *
 * @return TerminalSize { unsigned width, unsigned height }
 */
TerminalSize get_terminal_size() {
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return {
        csbi.srWindow.Right - csbi.srWindow.Left + 1,
        csbi.srWindow.Bottom - csbi.srWindow.Top + 1};
#else
    struct winsize w {};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);  // NOLINT(cppcoreguidelines-pro-type-vararg) : conform to POSIX
    return {w.ws_col, w.ws_row};
#endif  // Windows/Linux
}

}  // namespace utils

}  // namespace dvlab
