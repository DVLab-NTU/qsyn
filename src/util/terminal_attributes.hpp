/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ get the attributes of terminal ]
  Author       [ Adapted from https://stackoverflow.com/a/62485211 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstdio>

namespace dvlab {

namespace utils {

struct TerminalSize {
    unsigned width;
    unsigned height;
};

bool is_terminal(FILE* f = stdout);
bool ansi_supported(FILE* f = stdout);
TerminalSize get_terminal_size();

}  // namespace utils

}  // namespace dvlab
