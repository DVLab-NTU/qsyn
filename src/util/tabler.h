/****************************************************************************
  FileName     [ tabler.h ]
  PackageName  [ util ]
  Synopsis     [ A simple tabler for output formatting ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_TABLER_H
#define QSYN_TABLER_H

#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <ranges>
#include <vector>

#include "myConcepts.h"

namespace qsutil {

class Tabler {
public:
    class Skip {};  // dummy for skipping table
    class HSep {};
    class DoubleHSep {};

    Tabler(std::ostream& os = std::cout, size_t nCols = 0) : _os{os}, _numCols{nCols}, _counter{0} {}

    Tabler& widths(std::vector<size_t> const& w);
    Tabler& stream(std::ostream& os);
    Tabler& numCols(size_t n);

    Tabler& indent(unsigned n);
    Tabler& leftMargin(unsigned n);
    Tabler& rightMargin(unsigned n);
    Tabler& vSep(std::string_view sep);
    Tabler& hSep(char sep);
    Tabler& doubleHSep(char sep);
    Tabler& doVSep(bool doV);

    enum class PresetStyle {
        CSV,
        ASCII_MINIMAL,
        ASCII_FULL
    };

    Tabler& presetStyle(PresetStyle style);

    /**
     * @brief Tabularizing non-string printables
     *
     * @param val
     * @return Tabler&
     */
    Tabler& operator<<(PrintableButNotStringLike auto const& val) {
        printBeforeText();
        _os << std::setw(_widths[_counter++]) << std::right << val;
        _os << std::string(_attrs.rightMargin, ' ');
        printAfterText();

        return *this;
    }

    /**
     * @brief Tabularizing string printables
     *
     * @param val
     * @return Tabler&
     */
    Tabler& operator<<(StringLike auto const& val) {
        printBeforeText();
        _os << std::setw(_widths[_counter++] + countNumStyleChars(val))
            << std::left << val;
        printAfterText();

        return *this;
    }
    Tabler& operator<<(Skip const&);
    Tabler& operator<<(HSep const&);
    Tabler& operator<<(DoubleHSep const&);

private:
    std::ostream& _os;
    std::vector<size_t> _widths;
    size_t _numCols;
    size_t _counter;

    struct TablerAttrs {
        TablerAttrs() : indent{0}, leftMargin{1}, rightMargin{1}, vsep{"|"}, hsep{'-'}, dhsep{'='}, doVSep{false} {}
        unsigned indent;
        unsigned leftMargin;
        unsigned rightMargin;
        std::string vsep;
        char hsep;
        char dhsep;
        bool doVSep;
    } _attrs;

    void printBeforeText();
    void printAfterText();
    size_t countNumStyleChars(std::string_view str) const;
};

}  // namespace qsutil

#endif  // QSYN_TABLER_H