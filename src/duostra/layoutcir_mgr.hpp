#pragma once

#include <fmt/core.h>
#include <memory>

#include "./duostra_def.hpp"
#include "qcir/qcir.hpp"
#include "util/data_structure_manager.hpp"
#include "device/device.hpp"

namespace qsyn {

namespace qcir {

    class QCir;

}

class LayoutCir{
public:
    using Device = qsyn::device::Device;
    using PhysicalQubit = qsyn::device::PhysicalQubit;

private:
    std::vector<PhysicalQubit*> _physical_qubits;
    std::vector<qcir::QCirGate> _logical_gate;

};

namespace duostra {

class LayoutCirMgr: public dvlab::utils::DataStructureManager<LayoutCir>{

public:
    using Device = qsyn::device::Device;

    LayoutCirMgr(
        qcir::QCir* qcir,
        Device dev)
        // DuostraConfig const& config)
        : dvlab::utils::DataStructureManager<LayoutCir>("LayoutCirMgr"),
          _device(std::move(dev)),
        //   _config{config},
          _logical_circuit{std::make_shared<qcir::QCir>(*qcir)} { fmt::println("create layout circuit mgr"); };

private:

    std::unique_ptr<qcir::QCir> _physical_circuit =
        std::make_unique<qcir::QCir>();
    Device _device;
    // DuostraConfig _config;
    // bool _check;
    // bool _tqdm;
    // bool _silent;
    // std::unique_ptr<BaseScheduler> _scheduler;
    std::shared_ptr<qcir::QCir> _logical_circuit;
    std::vector<qcir::QCirGate> _result;


};

}; // namespace duostra


} // namespace qsyn
