/****************************************************************************
  FileName     [ apArgParser.tpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::ArgType template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef AP_ARG_PARSER_H
#define AP_ARG_PARSER_H

#include "argparse.h"

namespace ArgParse {

// SECTION - ArgumentParser template functions

/**
 * @brief add an argument with the name.
 *
 * @tparam T
 * @param name
 * @return ArgType<T>&
 */
template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgumentParser::addArgument(std::string const& name) {
    auto realname = toLowerString(name);
    if (_pimpl->arguments.contains(realname)) {
        printDuplicateArgNameErrorMsg(name);
    } else {
        _pimpl->arguments.emplace(realname, Argument(T{}));
    }

    ArgType<T>& returnRef = dynamic_cast<Argument::Model<ArgType<T>>*>(_pimpl->arguments.at(realname)._pimpl.get())->inner;

    if (!isOption(realname)) {
        returnRef.required(true).metavar(realname);
    } else {
        returnRef.metavar(toUpperString(realname.substr(realname.find_first_not_of(_pimpl->optionPrefix))));
    }

    _pimpl->optionsAnalyzed = false;

    return returnRef.name(name);
}

template <typename T>
decltype(auto)
ArgumentParser::operatorBracketImpl(T&& t, std::string const& name) {
    if (std::forward<T>(t)._pimpl->subparsers.has_value() && std::forward<T>(t)._pimpl->subparsers->isParsed()) {
        if (std::forward<T>(t).getActivatedSubParser()._pimpl->arguments.contains(toLowerString(name))) {
            return std::forward<T>(t).getActivatedSubParser()._pimpl->arguments.at(toLowerString(name));
        }
    }
    try {
        return std::forward<T>(t)._pimpl->arguments.at(toLowerString(name));
    } catch (std::out_of_range& e) {
        std::cerr << "Argument name \"" << name
                  << "\" does not exist for command \""
                  << std::forward<T>(t)._pimpl->formatter.styledCmdName(std::forward<T>(t).getName(), std::forward<T>(t).getNumRequiredChars()) << "\"\n";
        throw e;
    }
}

}  // namespace ArgParse

#endif