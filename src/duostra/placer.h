/****************************************************************************
  FileName     [ placer.h ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Placer structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef PLACER_H
#define PLACER_H

#include <memory>
#include <random>
#include <vector>

#include "topology.h"

class BasePlacer {
public:
    BasePlacer() {}
    BasePlacer(const BasePlacer& other) = delete;
    BasePlacer(BasePlacer&& other) = delete;
    virtual ~BasePlacer() {}

    void place_and_assign(DeviceTopo* device) {
        auto assign = place(device);
        device->place(assign);
    }

protected:
    virtual std::vector<size_t> place(DeviceTopo* device) const = 0;
};

class RandomPlacer : public BasePlacer {
public:
    ~RandomPlacer() override {}

protected:
    std::vector<size_t> place(DeviceTopo* device) const override;
};

class StaticPlacer : public BasePlacer {
public:
    ~StaticPlacer() override {}

protected:
    std::vector<size_t> place(DeviceTopo* device) const override;
};

class DFSPlacer : public BasePlacer {
public:
    ~DFSPlacer() override {}

protected:
    std::vector<size_t> place(DeviceTopo* device) const override;

private:
    void dfs_device(size_t current,
                    DeviceTopo* device,
                    std::vector<size_t>& assign,
                    std::vector<bool>& qubit_mark) const;
};

std::unique_ptr<BasePlacer> getPlacer(const std::string& typ);

#endif