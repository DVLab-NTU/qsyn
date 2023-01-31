/****************************************************************************
  FileName     [ textFormat.cpp ]
  PackageName  [ util ]
  Synopsis     [ Define text formatting functions such as colors, bold, etc. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "textFormat.h"

extern size_t colorLevel;

namespace TextFormat {

std::string decorate(std::string const& str, const size_t& code) {
    return "\033[" + std::to_string(code) + "m" + str + "\033[0m";
}

std::string setFormat(const std::string& str, const size_t& code) {
    return (colorLevel >= 1) ? decorate(str, code) : str;
}

size_t tokenSize(std::function<std::string(std::string const&)> F) {
    return F("").size();
}

std::string BOLD(const std::string& str) { return setFormat(str, 1); }
std::string DIM(const std::string& str) { return setFormat(str, 2); }
std::string ITALIC(const std::string& str) { return setFormat(str, 3); }
std::string ULINE(const std::string& str) { return setFormat(str, 4); }
std::string SWAP(const std::string& str) { return setFormat(str, 7); }
std::string STRIKE(const std::string& str) { return setFormat(str, 9); }

std::string BLACK(const std::string& str) { return setFormat(str, 30); }
std::string RED(const std::string& str) { return setFormat(str, 31); }
std::string GREEN(const std::string& str) { return setFormat(str, 32); }
std::string YELLOW(const std::string& str) { return setFormat(str, 33); }
std::string BLUE(const std::string& str) { return setFormat(str, 34); }
std::string MAGENTA(const std::string& str) { return setFormat(str, 35); }
std::string CYAN(const std::string& str) { return setFormat(str, 36); }
std::string WHITE(const std::string& str) { return setFormat(str, 37); }

std::string BG_BLACK(const std::string& str) { return setFormat(str, 40); }
std::string BG_RED(const std::string& str) { return setFormat(str, 41); }
std::string BG_GREEN(const std::string& str) { return setFormat(str, 42); }
std::string BG_YELLOW(const std::string& str) { return setFormat(str, 43); }
std::string BG_BLUE(const std::string& str) { return setFormat(str, 44); }
std::string BG_MAGENTA(const std::string& str) { return setFormat(str, 45); }
std::string BG_CYAN(const std::string& str) { return setFormat(str, 46); }
std::string BG_WHITE(const std::string& str) { return setFormat(str, 47); }

std::string GRAY(const std::string& str) { return setFormat(str, 90); }
std::string LIGHT_RED(const std::string& str) { return setFormat(str, 91); }
std::string LIGHT_GREEN(const std::string& str) { return setFormat(str, 92); }
std::string LIGHT_YELLOW(const std::string& str) { return setFormat(str, 93); }
std::string LIGHT_BLUE(const std::string& str) { return setFormat(str, 94); }
std::string LIGHT_MAGENTA(const std::string& str) { return setFormat(str, 95); }
std::string LIGHT_CYAN(const std::string& str) { return setFormat(str, 96); }
std::string LIGHT_WHITE(const std::string& str) { return setFormat(str, 97); }

std::string BG_GRAY(const std::string& str) { return setFormat(str, 100); }
std::string LIGHT_BG_RED(const std::string& str) { return setFormat(str, 101); }
std::string LIGHT_BG_GREEN(const std::string& str) { return setFormat(str, 102); }
std::string LIGHT_BG_YELLOW(const std::string& str) { return setFormat(str, 103); }
std::string LIGHT_BG_BLUE(const std::string& str) { return setFormat(str, 104); }
std::string LIGHT_BG_MAGENTA(const std::string& str) { return setFormat(str, 105); }
std::string LIGHT_BG_CYAN(const std::string& str) { return setFormat(str, 106); }
std::string LIGHT_BG_WHITE(const std::string& str) { return setFormat(str, 107); }

};  // namespace TextFormat