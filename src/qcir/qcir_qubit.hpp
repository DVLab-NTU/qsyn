/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirQubit structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <spdlog/spdlog.h>

namespace qsyn::qcir {

class QCirGate;

//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------

class QCirQubit {
public:
    // Basic access method
    void set_last(QCirGate* l) { _bit_last = l; }
    void set_first(QCirGate* f) { _bit_first = f; }
    QCirGate* get_last() const { return _bit_last; }
    QCirGate* get_first() const { return _bit_first; }

private:
    QCirGate* _bit_last  = nullptr;
    QCirGate* _bit_first = nullptr;
};

}  // namespace qsyn::qcir
