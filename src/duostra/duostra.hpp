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
#include "device/device.hpp"
#include "qcir/qcir.hpp"

class QCir;

class Duostra {
public:
    struct DuostraConfig {
        bool verifyResult = false;
        bool silent = false;
        bool useTqdm = true;
    };
    Duostra(QCir* qcir, Device dev, DuostraConfig const& config = {.verifyResult = false, .silent = false, .useTqdm = true});
    Duostra(std::vector<Operation> const& cir, size_t n_qubit, Device dev, DuostraConfig const& config = {.verifyResult = false, .silent = false, .useTqdm = true});
    ~Duostra() {}

    std::unique_ptr<QCir> const& get_physical_circuit() const { return _physical_circuit; }
    std::unique_ptr<QCir>&& get_physical_circuit() { return std::move(_physical_circuit); }
    std::vector<Operation> const& get_result() const { return _result; }
    std::vector<Operation> const& get_order() const { return _order; }
    Device get_device() const { return _device; }

    void make_dependency();
    void make_dependency(std::vector<Operation> const&, size_t);
    size_t flow(bool = false);
    void store_order_info(std::vector<size_t> const&);
    void print_assembly() const;
    void build_circuit_by_result();

private:
    QCir* _logical_circuit;
    std::unique_ptr<QCir> _physical_circuit = std::make_unique<QCir>();
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

size_t get_scheduler_type(std::string);
size_t get_router_type(std::string);
size_t get_placer_type(std::string);
