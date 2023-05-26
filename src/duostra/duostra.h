/****************************************************************************
  FileName     [ duostra.h ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef DUOSTRA_H
#define DUOSTRA_H

#include "device.h"
#include "placer.h"
#include "qcir.h"
#include "router.h"
#include "scheduler.h"

class Duostra {
public:
    Duostra(QCir*, Device, bool = false, bool = true, bool = false);
    Duostra(const std::vector<Operation>&, size_t, Device, bool = false, bool = true, bool = false);
    ~Duostra() {}

    QCir* getPhysicalCircuit() { return _physicalCircuit; }
    const std::vector<Operation>& getResult() const { return _result; }
    const std::vector<Operation>& getOrder() const { return _order; }
    Device getDevice() const { return _device; }

    void makeDependency();
    void makeDependency(const std::vector<Operation>&, size_t);
    size_t flow(bool = false);
    void storeOrderInfo(const std::vector<size_t>&);
    void printAssembly() const;
    void buildCircuitByResult();

private:
    QCir* _logicalCircuit;
    QCir* _physicalCircuit;
    Device _device;
    bool _check;
    bool _tqdm;
    bool _silent;
    std::unique_ptr<BaseScheduler> _scheduler;
    std::shared_ptr<DependencyGraph> _dependency;
    std::vector<Operation> _result;
    std::vector<Operation> _order;
};

std::string getSchedulerTypeStr();
std::string getRouterTypeStr();
std::string getPlacerTypeStr();

size_t getSchedulerType(std::string);
size_t getRouterType(std::string);
size_t getPlacerType(std::string);

#endif
