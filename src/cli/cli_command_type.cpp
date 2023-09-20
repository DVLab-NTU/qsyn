/****************************************************************************
  PackageName  [ cmd ]
  Synopsis     [ Define CLI command ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "cli/cli.hpp"

namespace dvlab {

void dvlab::Command::add_subcommand(dvlab::Command const& cmd) {
    auto old_definition       = this->_parser_definition;
    auto old_on_parse_success = this->_on_parse_success;
    this->_parser_definition  = [cmd, old_definition](dvlab::argparse::ArgumentParser& parser) {
        old_definition(parser);
        auto subparsers = std::invoke(
            [&parser]() {
                if (!parser.has_subparsers()) {
                    return parser.add_subparsers();
                }
                return parser.get_subparsers().value();
            });
        auto subparser = subparsers.add_parser(cmd._parser.get_name());

        cmd._parser_definition(subparser);
    };

    this->_on_parse_success = [cmd, old_on_parse_success](dvlab::argparse::ArgumentParser const& parser) {
        if (parser.used_subparser(cmd._parser.get_name())) {
            return cmd._on_parse_success(parser);
        }

        return old_on_parse_success(parser);
    };
}

/**
 * @brief Check the soundness of the parser before initializing the command with parserDefinition;
 *
 * @return true if succeeded
 * @return false if failed
 */
bool dvlab::Command::initialize(size_t n_req_chars) {
    if (!_parser_definition) {
        _print_missing_parser_definition_error_msg();
        return false;
    }
    if (!_on_parse_success) {
        _print_missing_on_parse_success_error_msg();
        return false;
    }
    _parser_definition(_parser);
    _parser.num_required_chars(n_req_chars);
    return _parser.analyze_options();
}

/**
 * @brief parse the argument. On parse success, execute the onParseSuccess callback.
 *
 * @return true if succeeded
 * @return false if failed
 */
CmdExecResult dvlab::Command::execute(std::string const& option) {
    if (!_parser.parse_args(option)) {
        return CmdExecResult::error;
    }

    return _on_parse_success(_parser);
}

void dvlab::Command::_print_missing_parser_definition_error_msg() const {
    fmt::println(stderr, "[ArgParse] Error:     please define parser definition for command \"{}\"!!", _parser.get_name());
    fmt::println(stderr, "           Signature: [](ArgumentParser& parser) {{ ... }};");
}

void dvlab::Command::_print_missing_on_parse_success_error_msg() const {
    fmt::println(stderr, "[ArgParse] Error:     please define on-parse-success action for command \"{}\"!!", _parser.get_name());
    fmt::println(stderr, "           Signature: [](ArgumentParser const& parser) {{ ... }};");
}

}  // namespace dvlab
