/****************************************************************************
  FileName     [ apArgParser.tpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::ArgType template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef ARGPARSE_ARGPARSER_TPP
#define ARGPARSE_ARGPARSER_TPP

#include "argparse.h"

namespace ArgParse {

/**
 * @brief add an argument with the name to the ArgumentGroup
 *
 * @tparam T the type of argument
 * @param name the name of the argument
 * @return ArgType<T>& a reference to the added argument
 */
template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgumentGroup::addArgument(std::string const& name) {
    ArgType<T>& returnRef = _pimpl->_parser.addArgument<T>(name);
    _pimpl->_arguments.insert(returnRef._name);
    return returnRef;
}

/**
 * @brief add an argument with the name.
 *
 * @tparam T the type of argument
 * @param name the name of the argument
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

    if (!hasOptionPrefix(realname)) {
        returnRef.required(true).metavar(realname);
    } else {
        returnRef.metavar(toUpperString(realname.substr(realname.find_first_not_of(_pimpl->optionPrefix))));
    }

    _pimpl->optionsAnalyzed = false;

    return returnRef.name(realname);
}

/**
 * @brief implements the details of ArgumentParser::operator[]
 *
 * @tparam ArgT ArgType<T>
 * @param arg the ArgType<T> object
 * @param name name of the argument to look up
 * @return decltype(auto)
 */
template <typename ArgT>
decltype(auto)
ArgumentParser::operator_bracket_impl(ArgT&& arg, std::string const& name) {
    if (std::forward<ArgT>(arg)._pimpl->subparsers.has_value() && std::forward<ArgT>(arg)._pimpl->subparsers->isParsed()) {
        if (std::forward<ArgT>(arg).getActivatedSubParser()._pimpl->arguments.contains(toLowerString(name))) {
            return std::forward<ArgT>(arg).getActivatedSubParser()._pimpl->arguments.at(toLowerString(name));
        }
    }
    try {
        return std::forward<ArgT>(arg)._pimpl->arguments.at(toLowerString(name));
    } catch (std::out_of_range& e) {
        std::cerr << "Argument name \"" << name
                  << "\" does not exist for command \""
                  << formatter.styledCmdName(std::forward<ArgT>(arg).getName(), std::forward<ArgT>(arg).getNumRequiredChars()) << "\"\n";
        throw e;
    }
}

}  // namespace ArgParse

#endif