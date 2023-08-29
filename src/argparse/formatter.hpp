/****************************************************************************
  FileName     [ formatter.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser formatter functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fort.hpp>
#include <string>

#include "argument.hpp"

namespace ArgParse {

class Argument;
class ArgumentParser;
class SubParsers;
class MutuallyExclusiveGroup;
/**
 * @brief Pretty printer for the command usages and helps.
 *
 */
struct Formatter {
public:
    static void printUsage(ArgumentParser const& parser);
    static void printSummary(ArgumentParser const& parser);
    static void printHelp(ArgumentParser const& parser);

    static std::string styledArgName(Argument const& arg);
    static std::string styledParserName(ArgumentParser const& parser);

    static std::string getSyntax(SubParsers const& parsers);
    static std::string getSyntax(ArgumentParser parser, MutuallyExclusiveGroup const& group);

    static std::string getUsageString(Argument const& arg);

private:
    static void tabulateHelpString(fort::utf8_table& table, size_t max_help_string_width, Argument const& arg);
    static std::string getSyntax(Argument const& arg);
};

}  // namespace ArgParse