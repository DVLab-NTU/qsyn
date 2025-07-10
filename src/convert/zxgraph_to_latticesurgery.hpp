/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion from QCir to ZXGraph ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <optional>
#include "latticesurgery/latticesurgery.hpp"
#include "qcir/operation.hpp"  // clangd might gives unused include warning,
                               // but this header is actually necessary for
                               // the to_zxgraph function

#include "zx/zxgraph.hpp"

namespace qsyn {

using zx::ZXVertex;
using latticesurgery::LatticeSurgery;

enum class PatchType{
    empty,
    simple,
    hadamard,
    borrowed,
    path,
    split
};

std::optional<latticesurgery::LatticeSurgery> to_latticesurgery(zx::ZXGraph* zxgraph);

class LatticeSurgerySynthesisStrategy{
public:
    LatticeSurgerySynthesisStrategy(zx::ZXGraph* zxgraph);
    std::optional<latticesurgery::LatticeSurgery> synthesize();
    std::vector<std::vector<zx::ZXVertex*>> create_vertex_map(const zx::ZXGraph* zxgraph);
    void n_to_n_merge(size_t qubit_id, std::vector<size_t> start_indices, std::vector<size_t> dest_indices, bool is_z);
    std::vector<std::pair<size_t, size_t>> qubit_schedule_min_depth(std::vector<std::vector<std::pair<size_t, double>>>& rc_dependency, size_t num_qubits) const;
    std::pair<int, std::vector<std::tuple<char, size_t, std::pair<std::vector<size_t>, std::vector<size_t>>>>> find_nearest_patch_both_sides(
        size_t cur_qubit,
        size_t j,
        const std::vector<PatchType>& cur_layer_row,
        std::vector<PatchType>& cur_layer_occupied_row,
        std::vector<PatchType>& first_split_patches,
        std::map<std::pair<size_t, size_t>, std::pair<size_t, size_t>>& hadamard_patches,
        bool color_map
    ) const;
    std::pair<int, std::vector<std::tuple<char, size_t, std::pair<std::vector<size_t>, std::vector<size_t>>>>> find_nearest_patch_both_sides_hadamard(
        size_t cur_qubit,
        size_t j,
        const std::vector<PatchType>& cur_layer_row,    
        std::vector<PatchType>& cur_layer_occupied_row,
        std::vector<PatchType>& second_split_patches,
        std::map<std::pair<size_t, size_t>, std::pair<size_t, size_t>>& hadamard_patches,
        bool color_map 
    ) const ;

private:
    zx::ZXGraph* _zxgraph;
    std::vector<std::vector<zx::ZXVertex*>> _vertex_map;
    LatticeSurgery _result;
    size_t _num_qubits;
};

// std::optional<zx::ZXGraph> to_zxgraph(qcir::QCir const& qcir);

}  // namespace qsyn
