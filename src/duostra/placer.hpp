/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Placer structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <memory>
#include <random>
#include <vector>

class Device;

class BasePlacer {
public:
    BasePlacer() {}
    virtual ~BasePlacer() = default;

    std::vector<size_t> place_and_assign(Device &);

protected:
    virtual std::vector<size_t> _place(Device &) const = 0;
};

class RandomPlacer : public BasePlacer {
public:
    ~RandomPlacer() override = default;

protected:
    std::vector<size_t> _place(Device & /*unused*/) const override;
};

class StaticPlacer : public BasePlacer {
public:
    ~StaticPlacer() override = default;

protected:
    std::vector<size_t> _place(Device & /*unused*/) const override;
};

class DFSPlacer : public BasePlacer {
public:
    ~DFSPlacer() override = default;

protected:
    std::vector<size_t> _place(Device & /*unused*/) const override;

private:
    void _dfs_device(size_t, Device &, std::vector<size_t> &, std::vector<bool> &) const;
};

std::unique_ptr<BasePlacer> get_placer();
