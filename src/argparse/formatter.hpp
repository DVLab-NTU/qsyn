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
/**
 * @brief Pretty printer for the command usages and helps.
 *
 */
struct Formatter {
public:
    static void printUsage(ArgumentParser const& parser);
    static void printSummary(ArgumentParser const& parser);
    static void printHelp(ArgumentParser const& parser);

    static std::string styledArgName(ArgumentParser const& parser, Argument const& arg);
    static std::string styledCmdName(std::string const& name, size_t numRequired);

    static std::string getSyntaxString(ArgumentParser const& parser, Argument const& arg);
    static std::string getSyntaxString(SubParsers const& parsers);

    static std::string requiredArgBracket(std::string const& str);
    static std::string optionalArgBracket(std::string const& str);

private:
    static void printHelpString(ArgumentParser const& parser, fort::utf8_table& table, size_t max_help_string_width, Argument const& arg);
    static void printHelpString(ArgumentParser const& parser, fort::utf8_table& table, size_t max_help_string_width, SubParsers const& parsers);
};

}  // namespace ArgParse