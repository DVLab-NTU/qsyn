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

namespace qsyn::device {
class Device;
}

namespace qsyn::duostra {

class BasePlacer {
public:
    using Device = qsyn::device::Device;
    BasePlacer() {}
    virtual ~BasePlacer() = default;

    std::vector<size_t> place_and_assign(Device &);

protected:
    virtual std::vector<size_t> _place(Device &) const = 0;
};

class RandomPlacer : public BasePlacer {
public:
    using Device = BasePlacer::Device;
    ~RandomPlacer() override = default;

protected:
    std::vector<size_t> _place(Device & /*unused*/) const override;
};

class StaticPlacer : public BasePlacer {
public:
    using Device = BasePlacer::Device;
    ~StaticPlacer() override = default;

protected:
    std::vector<size_t> _place(Device & /*unused*/) const override;
};

class DFSPlacer : public BasePlacer {
public:
    using Device = BasePlacer::Device;
    ~DFSPlacer() override = default;

protected:
    std::vector<size_t> _place(Device & /*unused*/) const override;

private:
    void _dfs_device(size_t, Device &, std::vector<size_t> &, std::vector<bool> &) const;
};

std::unique_ptr<BasePlacer> get_placer();

}  // namespace qsyn::duostra