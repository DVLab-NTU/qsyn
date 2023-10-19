/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define argument groups ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <concepts>
#include <memory>

#include "./arg_type.hpp"
#include "util/ordered_hashset.hpp"

namespace dvlab::argparse {

class ArgumentParser;

/**
 * @brief A view for adding argument groups.
 *        All copies of this class represents the same underlying group.
 *
 */
class MutuallyExclusiveGroup {
    struct MutExGroupImpl {
        MutExGroupImpl(ArgumentParser& parser)
            : _parser{&parser}, _required{false}, _parsed{false} {}
        ArgumentParser* _parser;
        dvlab::utils::ordered_hashset<std::string> _arguments;
        bool _required;
        bool _parsed;
    };

public:
    MutuallyExclusiveGroup(ArgumentParser& parser)
        : _pimpl{std::make_shared<MutExGroupImpl>(parser)} {}

    template <typename T>
    requires valid_argument_type<T>
    ArgType<T>& add_argument(std::string const& name, std::convertible_to<std::string> auto... alias);

    bool contains(std::string const& name) const { return _pimpl->_arguments.contains(name); }
    MutuallyExclusiveGroup required(bool is_req) {
        _pimpl->_required = is_req;
        return *this;
    }
    void set_parsed(bool is_parsed) { _pimpl->_parsed = is_parsed; }

    bool is_required() const { return _pimpl->_required; }
    bool is_parsed() const { return _pimpl->_parsed; }

    size_t size() const noexcept { return _pimpl->_arguments.size(); }

    dvlab::utils::ordered_hashset<std::string> const& get_arg_names() const { return _pimpl->_arguments; }

private:
    std::shared_ptr<MutExGroupImpl> _pimpl;
};

}  // namespace dvlab::argparse
