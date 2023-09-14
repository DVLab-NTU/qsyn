/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <memory>

#include "./scheduler.hpp"
#include "qcir/qcir.hpp"

namespace qsyn {

namespace qcir {

class QCir;

}

namespace duostra {

class Duostra {
public:
    using Device = qsyn::device::Device;
    using Operation = qsyn::device::Operation;
    struct DuostraConfig {
        bool verifyResult = false;
        bool silent = false;
        bool useTqdm = true;
    };
    Duostra(qcir::QCir* qcir, Device dev, DuostraConfig const& config = {.verifyResult = false, .silent = false, .useTqdm = true});
    Duostra(std::vector<Operation> const& cir, size_t n_qubit, Device dev, DuostraConfig const& config = {.verifyResult = false, .silent = false, .useTqdm = true});

    std::unique_ptr<qcir::QCir> const& get_physical_circuit() const { return _physical_circuit; }
    std::unique_ptr<qcir::QCir>&& get_physical_circuit() { return std::move(_physical_circuit); }
    std::vector<Operation> const& get_result() const { return _result; }
    std::vector<Operation> const& get_order() const { return _order; }
    Device get_device() const { return _device; }

    void make_dependency();
    void make_dependency(std::vector<Operation> const& ops, size_t n_qubits);
    size_t flow(bool = false);
    void store_order_info(std::vector<size_t> const&);
    void print_assembly() const;
    void build_circuit_by_result();

private:
    qcir::QCir* _logical_circuit;
    std::unique_ptr<qcir::QCir> _physical_circuit = std::make_unique<qcir::QCir>();
    Device _device;
    bool _check;
    bool _tqdm;
    bool _silent;
    std::unique_ptr<BaseScheduler> _scheduler;
    std::shared_ptr<DependencyGraph> _dependency;
    std::vector<Operation> _result;
    std::vector<Operation> _order;
};

std::string get_scheduler_type_str();
std::string get_router_type_str();
std::string get_placer_type_str();

size_t get_scheduler_type(std::string const&);
size_t get_router_type(std::string const&);
size_t get_placer_type(std::string const&);

}  // namespace duostra

}  // namespace qsyn