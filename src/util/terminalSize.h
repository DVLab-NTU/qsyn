/****************************************************************************
  FileName     [ terminalSize.h ]
  PackageName  [ util ]
  Synopsis     [ get the size of terminal ]
  Author       [ Adapted from https://stackoverflow.com/a/62485211 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif  // Windows/Linux
namespace dvlab_utils {

struct TerminalSize {
    unsigned width;
    unsigned height;
};

inline TerminalSize get_terminal_size() {
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return {
        csbi.srWindow.Right - csbi.srWindow.Left + 1,
        csbi.srWindow.Bottom - csbi.srWindow.Top + 1};
#else
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return {w.ws_col, w.ws_row};
#endif  // Windows/Linux
}

}  // namespace dvlab_utils