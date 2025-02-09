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

#include "duostra/duostra_def.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::device {
class Device;
}

namespace qsyn::duostra {

class BasePlacer {
public:
    using Device = qsyn::device::Device;
    BasePlacer() {}
    virtual ~BasePlacer() = default;

    std::vector<QubitIdType> place_and_assign(
        Device& device,
        const std::vector<QubitIdType>& qubit_priority = std::vector<QubitIdType>(),
        const std::vector<std::vector<size_t>>& qpi = std::vector<std::vector<size_t>>());

protected:
    virtual std::vector<QubitIdType> _place(
        Device& device,
        const std::vector<QubitIdType>& qubit_priority,
        const std::vector<std::vector<size_t>>& qpi) const = 0;
};

class RandomPlacer : public BasePlacer {
public:
    using Device = BasePlacer::Device;

protected:
    std::vector<QubitIdType> _place(
        Device& device,
        const std::vector<QubitIdType>& /*unused*/,
        const std::vector<std::vector<size_t>>& /*unused*/) const override;
};

class StaticPlacer : public BasePlacer {
public:
    using Device = BasePlacer::Device;

protected:
    std::vector<QubitIdType> _place(
        Device& device,
        const std::vector<QubitIdType>& /*unused*/,
        const std::vector<std::vector<size_t>>& /*unused*/) const override;
};

class DFSPlacer : public BasePlacer {
public:
    using Device = BasePlacer::Device;

protected:
    std::vector<QubitIdType> _place(
        Device& device,
        const std::vector<QubitIdType>& /*unused*/,
        const std::vector<std::vector<size_t>>& /*unused*/) const override;

private:
    void _dfs_device(QubitIdType current, Device& device, std::vector<QubitIdType>& assign, std::vector<bool>& qubit_marks) const;
};

std::unique_ptr<BasePlacer> get_placer(PlacerType type);

class QMDLAPlacer : public BasePlacer {
public:
    using Device = BasePlacer::Device;

protected:
    std::vector<QubitIdType> _place(
        Device& device,
        const std::vector<QubitIdType>& qubit_priority,
        const std::vector<std::vector<size_t>>& qpi) const override;
};


}  // namespace qsyn::duostra
