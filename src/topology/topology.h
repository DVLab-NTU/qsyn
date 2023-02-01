/****************************************************************************
  FileName     [ topology.h ]
  PackageName  [ topology ]
  Synopsis     [ Define class DeviceTopo structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include <cstddef>  // for size_t
#include <string>   // for string
#include <unordered_map>

#include "util.h"

class DeviceTopo;

class DeviceTopo {
public:
    DeviceTopo(size_t id) : _id(id) {
    }
    ~DeviceTopo() {}

    void setId(size_t id) { _id = id; }

private:
    size_t _id;
};

#endif  // TOPOLOGY_H