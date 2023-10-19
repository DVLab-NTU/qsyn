/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define type-erased argument class to hold ArgType<T> ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./argparse.hpp"

namespace dvlab::argparse {

/**
 * @brief If the argument has a default value, reset to it.
 *
 */
void Argument::reset() {
    _pimpl->do_reset();
}

/**
 * @brief get tokens from `tokens` and takes actions accordingly.
 *        if tokens are not empty, mark the
 *
 * @param tokens
 * @return true if action success, or
 * @return false if action failed or < l argument are available
 */
bool Argument::take_action(TokensView tokens) {
    if (!_pimpl->do_take_action(tokens) || !is_constraints_satisfied()) return false;

    return true;
}

/**
 * @brief Get a range of at most nargs.upper consecutive unparsed tokens.
 *
 * @param tokens
 * @return TokensView
 */
TokensView Argument::get_parse_range(TokensView tokens) const {
    auto parse_start = std::ranges::find_if(
        tokens, [](Token& token) { return token.parsed == false; });

    auto parse_end = std::ranges::find_if(
        parse_start, tokens.end(),
        [](Token& token) { return token.parsed == true; });
    return tokens.subspan(parse_start - std::begin(tokens), std::min(get_nargs().upper, static_cast<size_t>(parse_end - parse_start)));
}

bool Argument::tokens_enough_to_parse(TokensView tokens) const {
    return (tokens.size() >= get_nargs().lower);
}
/**
 * @brief If the argument is parsed, print out the parsed value. If not,
 *        print the default value if it has one, or "(unparsed)" if not.
 *
 */
void Argument::print_status() const {
    using namespace std::string_literals;
    fmt::println("  {:<8}   = {}", get_name(), std::invoke([this]() {
                     if (is_parsed()) {
                         return fmt::format("{}", *this);
                     } else if (has_default_value()) {
                         return fmt::format("{} (default)", *this);
                     } else {
                         return "(unparsed)"s;
                     }
                 }));
}

}  // namespace dvlab::argparse
