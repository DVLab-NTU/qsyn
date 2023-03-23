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
    Duostra(QCir* cir, DeviceTopo* dev) : _logicalCircuit(cir), _device(dev) {
    }
    ~Duostra() {}
    size_t flow();

private:
    QCir* _logicalCircuit;
    QCir* _physicalCircuit;
    DeviceTopo* _device;
};