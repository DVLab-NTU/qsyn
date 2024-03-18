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
    void set_last_gate(QCirGate* l) { _last_gate = l; }
    void set_first_gate(QCirGate* f) { _first_gate = f; }
    QCirGate* get_last_gate() const { return _last_gate; }
    QCirGate* get_first_gate() const { return _first_gate; }

private:
    QCirGate* _last_gate  = nullptr;
    QCirGate* _first_gate = nullptr;
};

}  // namespace qsyn::qcir
