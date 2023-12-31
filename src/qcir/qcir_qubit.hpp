/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirQubit structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>

#include "qsyn/qsyn_type.hpp"

namespace qsyn::qcir {

class QCirGate;

//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------

class QCirQubit {
public:
    using QubitIdType = qsyn::QubitIdType;
    QCirQubit(QubitIdType id) : _id(id) {}

    // Basic access method
    void set_id(QubitIdType id) { _id = id; }
    void set_last(QCirGate* l) { _bit_last = l; }
    void set_first(QCirGate* f) { _bit_first = f; }
    QubitIdType get_id() const { return _id; }
    QCirGate* get_last() const { return _bit_last; }
    QCirGate* get_first() const { return _bit_first; }
    // Printing functions
    void print_qubit_line(spdlog::level::level_enum lvl = spdlog::level::off) const;

private:
    QubitIdType _id;
    QCirGate* _bit_last  = nullptr;
    QCirGate* _bit_first = nullptr;
};

}  // namespace qsyn::qcir
