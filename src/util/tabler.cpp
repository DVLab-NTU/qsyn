/****************************************************************************
  FileName     [ tabler.cpp ]
  PackageName  [ util ]
  Synopsis     [ A simple tabler for output formatting ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "tabler.h"

namespace dvlab_utils {

/**
 * @brief Set the widths of each column. If numCols does not match the size of the input
 *        vector, resize the table to fit.
 *
 * @param w
 * @return Tabler&
 */
Tabler& Tabler::widths(std::vector<size_t> const& w) {
    _widths = w;
    if (w.size() != _numCols) {
        _numCols = w.size();
    }
    return *this;
}

/**
 * @brief Set the output stream of the tabler.
 *
 * @param os
 * @return Tabler&
 */
Tabler& Tabler::stream(std::ostream& os) {
    _os.rdbuf(os.rdbuf());
    return *this;
}

/**
 * @brief Set the number of columns. If the number does not match the number of column widths, resize the column widths
 *
 * @param n
 * @return Tabler&
 */
Tabler& Tabler::numCols(size_t n) {
    _numCols = n;
    if (_widths.size() != _numCols) {
        _widths.resize(n);
    }
    return *this;
}

/**
 * @brief set the indent of the table
 *
 * @param n
 * @return Tabler&
 */
Tabler& Tabler::indent(unsigned n) {
    _attrs.indent = n;
    return *this;
}

/**
 * @brief set the left margin of each cell
 *
 * @param n
 * @return Tabler&
 */
Tabler& Tabler::leftMargin(unsigned n) {
    _attrs.leftMargin = n;
    return *this;
}

/**
 * @brief set the right margin of each cell
 *
 * @param n
 * @return Tabler&
 */
Tabler& Tabler::rightMargin(unsigned n) {
    _attrs.rightMargin = n;
    return *this;
}

/**
 * @brief set the vertical separator
 *
 * @param n
 * @return Tabler&
 */
Tabler& Tabler::vSep(std::string_view sep) {
    _attrs.vsep = sep;
    return *this;
}

/**
 * @brief set the horizontal separator
 *
 * @param n
 * @return Tabler&
 */
Tabler& Tabler::hSep(char sep) {
    _attrs.vsep = sep;
    return *this;
}

/**
 * @brief set the double horizontal separator
 *
 * @param n
 * @return Tabler&
 */
Tabler& Tabler::doubleHSep(char sep) {
    _attrs.vsep = sep;
    return *this;
}

/**
 * @brief toggle whether to use vertical separators
 *
 * @param doV
 * @return Tabler&
 */
Tabler& Tabler::doVSep(bool doV) {
    _attrs.doVSep = doV;
    return *this;
}

/**
 * @brief quick options for common styles
 *
 * @param style
 * @return Tabler&
 */
Tabler& Tabler::presetStyle(PresetStyle style) {
    switch (style) {
        case PresetStyle::CSV:
            return this->vSep(", ").leftMargin(0).rightMargin(0).doVSep(true);
        case PresetStyle::ASCII_MINIMAL:
            return this->vSep("|").leftMargin(1).rightMargin(1).doVSep(false);
        case PresetStyle::ASCII_FULL:
            return this->vSep("|").leftMargin(1).rightMargin(1).doVSep(true);
        default:
            return *this;
    }
    return *this;
}

/**
 * @brief specialization for skipping a cell
 *
 * @return Tabler&
 */
Tabler& Tabler::operator<<(Skip const&) {
    printBeforeText();
    _os << std::string(_widths[_counter++], ' ');
    printAfterText();

    return *this;
}

/**
 * @brief specialization for horizontal separation line
 *
 * @return Tabler&
 */
Tabler& Tabler::operator<<(HSep const&) {
    if (_counter > 0) _os << std::endl;
    _os << std::string(_attrs.indent, ' ')
        << std::string(std::accumulate(_widths.begin(), _widths.end(), 0) + _widths.size() * (_attrs.leftMargin + _attrs.rightMargin), _attrs.hsep)
        << std::endl;
    return *this;
}

/**
 * @brief specialization for horizontal double separation line
 *
 * @return Tabler&
 */
Tabler& Tabler::operator<<(DoubleHSep const&) {
    if (_counter > 0) _os << std::endl;
    _os << std::string(_attrs.indent, ' ')
        << std::string(std::accumulate(_widths.begin(), _widths.end(), 0) + _widths.size() * (_attrs.leftMargin + _attrs.rightMargin), _attrs.dhsep)
        << std::endl;
    return *this;
}

// -------------------------
// pretty printing helpers
// -------------------------

/**
 * @brief printing indents and left margins
 *
 */
void Tabler::printBeforeText() {
    if (_counter == 0) _os << std::string(_attrs.indent, ' ');
    _os << std::string(_attrs.leftMargin, ' ');
}

/**
 * @brief printing right margins and endl
 *
 */
void Tabler::printAfterText() {
    _os << std::string(_attrs.rightMargin, ' ');
    if (_counter >= _widths.size()) {
        _os << std::endl;
        _counter = 0;
    } else if (_attrs.doVSep)
        _os << _attrs.vsep;
}

/**
 * @brief calculate the number of style characters, i.e., \033....m in a string
 *
 * @param str
 * @return size_t
 */
size_t Tabler::countNumStyleChars(std::string_view str) const {
    bool isSpecial = false;
    size_t count = 0;
    for (auto ch : str) {
        if (isSpecial) {
            count++;
            if (ch == 'm') isSpecial = false;
        } else if (ch == '\033') {
            isSpecial = true;
            count++;
        }
    }
    return count;
}

}  // namespace dvlab_utils