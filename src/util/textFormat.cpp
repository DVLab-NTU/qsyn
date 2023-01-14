/****************************************************************************
  FileName     [ textFormat.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define text formatting functions such as colors, bold, etc. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "textFormat.h"

extern size_t colorLevel;

namespace TextFormat {

std::string SET_FORMAT(const std::string& str, const size_t& code) {
    if (colorLevel >= 1)
        return "\033[" + std::to_string(code) + "m" + str + "\033[0m";
    else
        return str;
}

std::string BOLD(const std::string& str) { return SET_FORMAT(str, 1); }
std::string DIM(const std::string& str) { return SET_FORMAT(str, 2); }
std::string ITALIC(const std::string& str) { return SET_FORMAT(str, 3); }
std::string ULINE(const std::string& str) { return SET_FORMAT(str, 4); }
std::string SWAP(const std::string& str) { return SET_FORMAT(str, 7); }
std::string STRIKE(const std::string& str) { return SET_FORMAT(str, 9); }

std::string BLACK(const std::string& str) { return SET_FORMAT(str, 30); }
std::string RED(const std::string& str) { return SET_FORMAT(str, 31); }
std::string GREEN(const std::string& str) { return SET_FORMAT(str, 32); }
std::string YELLOW(const std::string& str) { return SET_FORMAT(str, 33); }
std::string BLUE(const std::string& str) { return SET_FORMAT(str, 34); }
std::string MAGENTA(const std::string& str) { return SET_FORMAT(str, 35); }
std::string CYAN(const std::string& str) { return SET_FORMAT(str, 36); }
std::string WHITE(const std::string& str) { return SET_FORMAT(str, 37); }

std::string BG_BLACK(const std::string& str) { return SET_FORMAT(str, 40); }
std::string BG_RED(const std::string& str) { return SET_FORMAT(str, 41); }
std::string BG_GREEN(const std::string& str) { return SET_FORMAT(str, 42); }
std::string BG_YELLOW(const std::string& str) { return SET_FORMAT(str, 43); }
std::string BG_BLUE(const std::string& str) { return SET_FORMAT(str, 44); }
std::string BG_MAGENTA(const std::string& str) { return SET_FORMAT(str, 45); }
std::string BG_CYAN(const std::string& str) { return SET_FORMAT(str, 46); }
std::string BG_WHITE(const std::string& str) { return SET_FORMAT(str, 47); }

std::string GRAY(const std::string& str) { return SET_FORMAT(str, 90); }
std::string LIGHT_RED(const std::string& str) { return SET_FORMAT(str, 91); }
std::string LIGHT_GREEN(const std::string& str) { return SET_FORMAT(str, 92); }
std::string LIGHT_YELLOW(const std::string& str) { return SET_FORMAT(str, 93); }
std::string LIGHT_BLUE(const std::string& str) { return SET_FORMAT(str, 94); }
std::string LIGHT_MAGENTA(const std::string& str) { return SET_FORMAT(str, 95); }
std::string LIGHT_CYAN(const std::string& str) { return SET_FORMAT(str, 96); }
std::string LIGHT_WHITE(const std::string& str) { return SET_FORMAT(str, 97); }

std::string BG_GRAY(const std::string& str) { return SET_FORMAT(str, 100); }
std::string LIGHT_BG_RED(const std::string& str) { return SET_FORMAT(str, 101); }
std::string LIGHT_BG_GREEN(const std::string& str) { return SET_FORMAT(str, 102); }
std::string LIGHT_BG_YELLOW(const std::string& str) { return SET_FORMAT(str, 103); }
std::string LIGHT_BG_BLUE(const std::string& str) { return SET_FORMAT(str, 104); }
std::string LIGHT_BG_MAGENTA(const std::string& str) { return SET_FORMAT(str, 105); }
std::string LIGHT_BG_CYAN(const std::string& str) { return SET_FORMAT(str, 106); }
std::string LIGHT_BG_WHITE(const std::string& str) { return SET_FORMAT(str, 107); }

};  // namespace TextFormat