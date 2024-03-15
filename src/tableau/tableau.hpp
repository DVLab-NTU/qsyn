/**
 * @file tableau.cpp
 * @brief define tableau class for quantum circuit representation
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "./stabilizer_tableau.hpp"

namespace qsyn {

namespace experimental {

using SubTableau = std::variant<StabilizerTableau, std::vector<PauliRotation>>;

class Tableau : public PauliProductTrait<Tableau> {
public:
    Tableau(size_t n_qubits) : _subtableaux{StabilizerTableau{n_qubits}} {}
    Tableau(std::initializer_list<SubTableau> subtableaux) : _subtableaux{subtableaux} {}

    auto begin() const { return _subtableaux.begin(); }
    auto end() const { return _subtableaux.end(); }
    auto begin() { return _subtableaux.begin(); }
    auto end() { return _subtableaux.end(); }

    auto size() const { return _subtableaux.size(); }

    auto const& front() const { return _subtableaux.front(); }
    auto const& back() const { return _subtableaux.back(); }
    auto& front() { return _subtableaux.front(); }
    auto& back() { return _subtableaux.back(); }

    auto n_qubits() const -> size_t;

    auto insert(std::vector<SubTableau>::iterator pos, std::vector<SubTableau>::iterator first, std::vector<SubTableau>::iterator last) {
        return _subtableaux.insert(pos, first, last);
    }

    auto insert(std::vector<SubTableau>::iterator pos, SubTableau const& subtableau) { return _subtableaux.insert(pos, subtableau); }

    auto erase(std::vector<SubTableau>::iterator first, std::vector<SubTableau>::iterator last) {
        return _subtableaux.erase(first, last);
    }

    auto push_back(SubTableau const& subtableau) { return _subtableaux.push_back(subtableau); }

    template <typename... Args>
    auto emplace_back(Args&&... args) {
        return _subtableaux.emplace_back(std::forward<Args>(args)...);
    }

    auto& operator[](size_t idx) { return _subtableaux[idx]; }
    auto const& operator[](size_t idx) const { return _subtableaux[idx]; }

    auto erase(std::ranges::range auto const& range) { return _subtableaux.erase(range); }

    auto get_filename() const { return _filename; }
    auto set_filename(std::string const& filename) { _filename = filename; }

    auto get_procedures() const { return _procedures; }
    auto add_procedure(std::string const& procedure) { _procedures.push_back(procedure); }
    auto add_procedures(std::vector<std::string> const& procedures) { _procedures.insert(_procedures.end(), procedures.begin(), procedures.end()); }

    Tableau& h(size_t qubit) noexcept override;
    Tableau& s(size_t qubit) noexcept override;
    Tableau& cx(size_t control, size_t target) noexcept override;

private:
    std::vector<SubTableau> _subtableaux;
    std::string _filename;
    std::vector<std::string> _procedures;
};

void adjoint_inplace(SubTableau& subtableau);
[[nodiscard]] SubTableau adjoint(SubTableau const& subtableau);

void adjoint_inplace(Tableau& tableau);
[[nodiscard]] Tableau adjoint(Tableau const& tableau);

}  // namespace experimental

}  // namespace qsyn
template <>
struct fmt::formatter<qsyn::experimental::SubTableau> {
    char presentation = 'c';
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 'b')) presentation = *it++;
        if (it != end && *it != '}') detail::throw_format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::SubTableau const& subtableau, FormatContext& ctx) const -> format_context::iterator {
        // NOTE - cannot use run-time formatting to choose between 'c' and 'b'
        //        because the format function may be called in compile-time
        return std::visit(
            dvlab::overloaded(
                [&](qsyn::experimental::StabilizerTableau const& st) -> format_context::iterator {
                    return fmt::format_to(ctx.out(), "Clifford:\n{}\n", presentation == 'c' ? st.to_string() : st.to_bit_string());
                },
                [&](std::vector<qsyn::experimental::PauliRotation> const& pr) -> format_context::iterator {
                    if (presentation == 'c')
                        return fmt::format_to(ctx.out(), "Pauli Rotations:\n{:c}\n", fmt::join(pr, "\n"));
                    else
                        return fmt::format_to(ctx.out(), "Pauli Rotations:\n{:b}\n", fmt::join(pr, "\n"));
                }),
            subtableau);
    }
};

template <>
struct fmt::formatter<qsyn::experimental::Tableau> {
    char presentation = 'c';
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 'b')) presentation = *it++;
        if (it != end && *it != '}') detail::throw_format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::Tableau const& tableau, FormatContext& ctx) const {
        return presentation == 'c'
                   ? fmt::format_to(ctx.out(), "{:c}", fmt::join(tableau, "\n"))
                   : fmt::format_to(ctx.out(), "{:b}", fmt::join(tableau, "\n"));
    }
};
