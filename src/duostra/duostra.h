/****************************************************************************
  FileName     [ duostra.h ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "placer.h"
#include "qcir.h"
#include "router.h"
#include "scheduler.h"
#include "topology.h"

class Duostra {
public:
    Duostra(QCir*, Device);
    ~Duostra() {}

    QCir* getPhysicalCircuit() { return _physicalCircuit; }

    void makeDependency();
    size_t flow();
    void printAssembly() const;
    void buildCircuitByResult();

private:
    QCir* _logicalCircuit;
    QCir* _physicalCircuit;
    Device _device;
    std::unique_ptr<BaseScheduler> _scheduler;
    std::shared_ptr<DependencyGraph> _dependency;
    std::vector<Operation> _result;
};