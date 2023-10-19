/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class MappingEquivalenceChecker structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <optional>
#include <unordered_set>

#include "device/device.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn {

namespace qcir {

class QCir;
class QCirGate;
struct QubitInfo;

}  // namespace qcir

namespace duostra {

class MappingEquivalenceChecker {
public:
    using Device = qsyn::device::Device;
    MappingEquivalenceChecker(qcir::QCir*, qcir::QCir*, Device, std::vector<QubitIdType> = {}, bool = false);

    bool check();
    bool is_swap(qcir::QCirGate*);
    bool execute_swap(qcir::QCirGate*, std::unordered_set<qcir::QCirGate*>&);
    bool execute_single(qcir::QCirGate*);
    bool execute_double(qcir::QCirGate*);

    void check_remaining();
    qcir::QCirGate* get_next(qcir::QubitInfo const&);

private:
    qcir::QCir* _physical;
    qcir::QCir* _logical;
    Device _device;
    bool _reverse;
    // <qubit, gate to execute (from back)> for logical circuit
    std::unordered_map<QubitIdType, qcir::QCirGate*> _dependency;
};

}  // namespace duostra

}  // namespace qsyn
