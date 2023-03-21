/****************************************************************************
  FileName     [ router.h ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Router structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ROUTER_H
#define ROUTER_H

#include <string>
// #include <tuple>
#include "topology.h"
#include "qcir.h"
// #include "topo.hpp"

class QFTRouter {
   public:
    QFTRouter(DeviceTopo* device,
              const std::string& typ,
              const std::string& cost,
              bool orient) noexcept;
    QFTRouter(const QFTRouter& other) noexcept;
    QFTRouter(QFTRouter&& other) noexcept;

    
    DeviceTopo* get_device() { return device_; }
    const DeviceTopo* get_device() const { return device_; }

    size_t get_gate_cost(QCirGate* gate, bool min_max, size_t apsp_coef) const;
    bool is_executable(QCirGate* gate) const;
    std::unique_ptr<QFTRouter> clone() const;

    // Main Router function
    // FIXME - Merge from device.h 
    const std::vector<QCirGate*>& duostra_routing(QCirGate* gate, std::tuple<size_t, size_t> qs, bool orient);
    const std::vector<QCirGate*>& apsp_routing(QCirGate* gate, std::tuple<size_t, size_t> qs, bool orient);
    // FIXME - After Duostra Routing
    std::vector<QCirGate*> assign_gate(QCirGate* gate);

   private:
    bool greedy_type_;
    bool duostra_;
    bool orient_;
    bool apsp_;
    DeviceTopo* device_;
    std::vector<size_t> topo_to_dev_;

    void init(const std::string& typ, const std::string& cost);
    std::tuple<size_t, size_t> get_device_qubits_idx(QCirGate* gate) const;
};

#endif