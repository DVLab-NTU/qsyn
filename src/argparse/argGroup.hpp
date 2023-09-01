/****************************************************************************
  FileName     [ argGroup.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Definitions for subparsers of ArgumentParser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <concepts>
#include <memory>

#include "./argType.hpp"
#include "util/ordered_hashset.hpp"

namespace ArgParse {

class ArgumentParser;

/**
 * @brief A view for adding argument groups.
 *        All copies of this class represents the same underlying group.
 *
 */
class MutuallyExclusiveGroup {
    struct MutExGroupImpl {
        MutExGroupImpl(ArgumentParser& parser)
            : _parser{parser}, _required{false}, _parsed{false} {}
        ArgumentParser& _parser;
        ordered_hashset<std::string> _arguments;
        bool _required;
        bool _parsed;
    };

public:
    MutuallyExclusiveGroup(ArgumentParser& parser)
        : _pimpl{std::make_shared<MutExGroupImpl>(parser)} {}

    template <typename T>
    requires ValidArgumentType<T>
    ArgType<T>& addArgument(std::string const& name, std::convertible_to<std::string> auto... alias);  // defined in argParser.tpp

    bool contains(std::string const& name) const { return _pimpl->_arguments.contains(name); }
    MutuallyExclusiveGroup required(bool isReq) {
        _pimpl->_required = isReq;
        return *this;
    }
    void setParsed(bool isParsed) { _pimpl->_parsed = isParsed; }

    bool isRequired() const { return _pimpl->_required; }
    bool isParsed() const { return _pimpl->_parsed; }

    size_t size() const noexcept { return _pimpl->_arguments.size(); }

    ordered_hashset<std::string> const& getArguments() const { return _pimpl->_arguments; }

private:
    std::shared_ptr<MutExGroupImpl> _pimpl;
};

}  // namespace ArgParse