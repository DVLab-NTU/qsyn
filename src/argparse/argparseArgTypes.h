/****************************************************************************
  FileName     [ argparseArgument.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define per-type details for types that represents an ArgParse::Argument ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef QSYN_ARG_PARSE_ARG_TYPES_H
#define QSYN_ARG_PARSE_ARG_TYPES_H

#include "argparseDef.h"

#include <string>
#include <span>

namespace ArgParse {

class SubParsers;

/**
 * @brief per-type implementation to enable type-erasure
 *
 */
namespace detail {

// argument type: int

std::string getTypeString(int const& arg);
ParseResult parse(int& arg, std::span<TokenPair> tokens);

// argument type: std::string

std::string getTypeString(std::string const& arg);
ParseResult parse(std::string& arg, std::span<TokenPair> tokens);

// argument type: bool

std::string getTypeString(bool const& arg);
ParseResult parse(bool& arg, std::span<TokenPair> tokens);

// argument type: subparser (aka std::unique_ptr<ArgParse::ArgumentParser>)

std::string getTypeString(SubParsers const& arg);
ParseResult parse(SubParsers& arg, std::span<TokenPair> tokens);

// argument type: unsigned

std::string getTypeString(unsigned const& arg);
ParseResult parse(unsigned& arg, std::span<TokenPair> tokens);

// argument type: size_t

std::string getTypeString(size_t const& arg);
ParseResult parse(size_t& arg, std::span<TokenPair> tokens);

}  // namespace detail

};      // namespace ArgParse

#endif  // QSYN_ARG_PARSE_ARG_TYPES_H