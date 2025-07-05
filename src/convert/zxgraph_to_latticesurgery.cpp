#include "zxgraph_to_latticesurgery.hpp"
#include <fmt/core.h>
#include "latticesurgery/latticesurgery.hpp"
#include "util/dvlab_string.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include <cstddef>
#include <iterator>
#include <limits>
#include <queue>
#include <ranges>
#include <utility>
#include <vector>
#include <functional>

namespace qsyn {

using zx::ZXVertex;
using latticesurgery::LatticeSurgery;
using zx::VertexType;
using zx::EdgeType;

std::optional<LatticeSurgery> to_latticesurgery(zx::ZXGraph* zxgraph){
    fmt::println("In zx to lattice surgery");

    LatticeSurgerySynthesisStrategy strategy(zxgraph);
    
    return strategy.synthesize();

};

std::optional<LatticeSurgery> LatticeSurgerySynthesisStrategy::synthesize(){

    fmt::println("start to synthesize the lattice surgery");
    // <-> z | x

   // identify the color of each layer 
   fmt::println("identify the color of each layer");
   std::vector<bool> color_map(_vertex_map.size(), false); // false: z, true: x
   for(size_t i=0; i<_vertex_map.size(); i++){
        for(size_t j=0; j<_vertex_map[i].size(); j++){
            if(_vertex_map[i][j] == nullptr) continue;
            if(_vertex_map[i][j]->type() == VertexType::x){
                color_map[i] = true;
                break;
            } 
        }
   }
   if(!color_map[1]) color_map.front() = true;
   if(!color_map[_vertex_map.size()-2]) color_map.back() = true;

   // initialize lattice surgery layout for each previous layer
   fmt::println("initialize lattice surgery layout for each previous layer");
   std::vector<std::vector<PatchType>> pre_layer(_num_qubits, std::vector<PatchType>(_num_qubits, PatchType::empty));

    for(size_t i=0; i<_num_qubits; i++){
        pre_layer[i][i] = PatchType::simple;
    }

    // start to create the lattice surgery
   fmt::println("start to create the lattice surgery");
   for(size_t i=0; i<_vertex_map.size(); i++){

        fmt::println("mapping layer {}", i);

        if(i == _vertex_map.size()-1) {
            for(size_t j=0; j<_num_qubits; j++){
                std::vector<size_t> start_patches;  
                std::vector<size_t> dest_patches{j};
                for(size_t k=0; k<_num_qubits; k++){
                    if(pre_layer[j][k] == PatchType::simple || pre_layer[j][k] == PatchType::hadamard) start_patches.emplace_back(k);
                }
                n_to_n_merge(j, start_patches, dest_patches, color_map[i]);
            }
            break;
        }

        fmt::println("pre_layer:");
        for(size_t j=0; j<_num_qubits; j++){
            for(size_t k=0; k<_num_qubits; k++){
                if(pre_layer[j][k] == PatchType::simple) fmt::print("1 ");
                else if(pre_layer[j][k] == PatchType::hadamard) fmt::print("2 ");
                else fmt::print("0 ");
            }
            fmt::println("");
        }

        // create layout for current layer
        std::vector<std::vector<PatchType>> cur_layer(_num_qubits, std::vector<PatchType>(_num_qubits, PatchType::empty));
        fmt::println("cur_layer:");
        for(size_t j=0; j<_num_qubits; j++){
            if(_vertex_map[i][j] == nullptr) cur_layer[j][j] = PatchType::simple;
            else{
                for(auto& [neighbor, edge]: _zxgraph->get_neighbors(_vertex_map[i][j])){
                    if(neighbor->get_col() > i){
                        cur_layer[j][neighbor->get_row()] = (edge == EdgeType::hadamard) ? PatchType::hadamard : PatchType::simple;
                    }
                }
            }
            for(size_t k=0; k<_num_qubits; k++){
                if(cur_layer[j][k] == PatchType::simple) fmt::print("1 ");
                else if(cur_layer[j][k] == PatchType::hadamard) fmt::print("2 ");
                else fmt::print("0 ");
            }
            fmt::println("");
        }

        // initialize the directed graph for row/column scheduling
        std::vector<std::vector<size_t>> rc_dependency(_num_qubits, std::vector<size_t>());

        for(size_t j=0; j<_num_qubits; j++){
            std::vector<size_t> hadamard_patches;
            for(size_t k=0; k<_num_qubits; k++){
                if(cur_layer[j][k] == PatchType::hadamard){
                    hadamard_patches.push_back(k);
                }
            }
            if(hadamard_patches.size() > 0){
                // check right row/column can be used for hadamard ancilla
                if(j+1 < _num_qubits){
                    bool can_use = true;
                    for(auto h: hadamard_patches){
                        if(cur_layer[j+1][h] != PatchType::empty){
                            can_use = false;
                            break;
                        }
                    }
                    if(can_use){
                        rc_dependency[j].push_back(j+1);
                    }
                }

                // check left row/column can be used for hadamard ancilla
                if(j > 0){
                    bool can_use = true;
                    for(auto h: hadamard_patches){
                        if(cur_layer[j-1][h] != PatchType::empty){
                            can_use = false;
                            break;
                        }
                    }
                    if(can_use){
                        rc_dependency[j].push_back(j-1);
                    }
                }
                if(rc_dependency[j].size() == 0){
                    if(j+1 < _num_qubits) rc_dependency[j].push_back(j+1);
                    if(j > 0) rc_dependency[j].push_back(j-1);
                }
            }
        }

        // initialize qubit scheduling list
        auto qubit_schedule = qubit_schedule_min_depth(rc_dependency, _num_qubits);


        // check if there is any row/column that is used more than once
        // std::vector<size_t> reused_col(_num_qubits, 0);
        // for(auto [_, ancilla]: qubit_schedule) reused_col[ancilla]++;

        std::vector<std::tuple<char, size_t, std::pair<std::vector<size_t>, std::vector<size_t>>>> ls_operations; // <operation type, qubit id, <start_indices, dest_indices>>

        // (mapped position, original position)
        std::map<std::pair<size_t,size_t>, std::pair<size_t, size_t>> hadamard_patches; // (cur_qubit, j) -> (ancilla, dest_qubit)

        std::vector<std::vector<PatchType>> cur_layer_occupied(_num_qubits, std::vector<PatchType>(_num_qubits, PatchType::empty)); // check if any position is occupied by hadamard
        std::vector<std::vector<PatchType>> next_layer(_num_qubits, std::vector<PatchType>(_num_qubits, PatchType::empty));

        fmt::println("qubit_schedule size: {}", qubit_schedule.size());

        for(auto [cur_qubit, ancilla]: qubit_schedule){
            fmt::println("cur_qubit: {}, ancilla: {}", cur_qubit, ancilla);
            // find the split patches from the previous layer merge
            std::vector<PatchType> first_split_patches(_num_qubits, PatchType::empty);
            std::vector<PatchType> second_split_patches(_num_qubits, PatchType::empty);
            std::vector<size_t> unmapped_simple_patches;
            std::vector<std::vector<size_t>> unmapped_hadamard;

            // bool pre_row_hadamard = false;

            for(size_t j=0; j<_num_qubits; j++){
                if(cur_layer[cur_qubit][j] == PatchType::simple && cur_layer_occupied[cur_qubit][j] == PatchType::empty){
                    first_split_patches[j] = PatchType::split;
                    second_split_patches[j] = PatchType::split;
                }
                else if(cur_layer[cur_qubit][j] == PatchType::simple) unmapped_simple_patches.emplace_back(j);
                else if (cur_layer[cur_qubit][j] == PatchType::hadamard){
                    unmapped_hadamard.emplace_back(std::vector<size_t>{j});
                }
            }

            for(auto unmapped_simple: unmapped_simple_patches){
                // find the nearest patch
                auto [best_idx, best_ops] = find_nearest_patch_both_sides(
                    cur_qubit,
                    unmapped_simple,
                    cur_layer[cur_qubit],
                    cur_layer_occupied[cur_qubit],
                    first_split_patches,
                    hadamard_patches,
                    color_map[i]
                );

                if(first_split_patches[best_idx] == PatchType::empty) first_split_patches[best_idx] = PatchType::split;

                if(best_ops.size() > 0) ls_operations.insert(ls_operations.end(), best_ops.begin(), best_ops.end());
                
                if(best_idx != -1 && best_idx < unmapped_simple){
                    for(size_t j=best_idx; j<unmapped_simple; j++){
                        if(second_split_patches[j] == PatchType::empty) second_split_patches[j] = PatchType::path;
                    }
                    second_split_patches[unmapped_simple] = PatchType::split;
                }
                else if(best_idx != -1 && best_idx >= unmapped_simple){
                    for(size_t j=unmapped_simple+1; j<=best_idx; j++){
                        if(second_split_patches[j] == PatchType::empty) second_split_patches[j] = PatchType::path;
                    }
                    second_split_patches[unmapped_simple] = PatchType::split;
                }

            }

            std::vector<std::tuple<char, size_t, std::pair<std::vector<size_t>, std::vector<size_t>>>> hadamard_ls_operations;
            std::vector<size_t> count_hadamard_start(_num_qubits, 0);
            for(size_t j=0; j<_num_qubits; j++){
                if(cur_layer[cur_qubit][j] == PatchType::simple) count_hadamard_start[j] = 1;
            }
            
            for(auto hs: unmapped_hadamard){
                if(hs.size() == 1){
                    // fmt::println("first_split_patches: {}", fmt::join(first_split_patches, ", "));
                    auto [best_idx, best_ops] = find_nearest_patch_both_sides_hadamard(
                        cur_qubit,
                        hs[0],
                        cur_layer[cur_qubit],
                        cur_layer_occupied[cur_qubit],
                        second_split_patches,
                        hadamard_patches,
                        color_map[i]
                    );

                    hadamard_patches[std::make_pair(ancilla, hs[0])] = std::make_pair(cur_qubit, hs[0]);
                    // fmt::println("first_split_patches: {}", fmt::join(first_split_patches, ", "));
                    // fmt::println("best_idx: {}", best_idx);
                    if (first_split_patches[best_idx] == PatchType::empty){
                        first_split_patches[best_idx] = PatchType::split;
                        second_split_patches[best_idx] = PatchType::split;
                    }
                    else{
                        second_split_patches[best_idx] = PatchType::split;
                    }

                    if(best_ops.size() > 0) ls_operations.insert(hadamard_ls_operations.end(), best_ops.begin(), best_ops.end());

                    count_hadamard_start[best_idx]++;

                    if(!color_map[i]){ // <-> z
                        hadamard_ls_operations.emplace_back(
                            std::make_tuple(
                                'h',
                                0,
                                std::make_pair(
                                    std::vector<size_t>{(size_t)best_idx, cur_qubit},
                                    std::vector<size_t>{hs[0], ancilla}
                                )
                            )
                        );
                        cur_layer_occupied[ancilla][hs[0]] = PatchType::hadamard;
                        // fmt::println("HADAMARD:");
                        // fmt::println("start_indices: ({}, {})", best_idx, cur_qubit);
                        // fmt::println("dest_patches: ({}, {})", hs[0], ancilla);
                    }
                    else{ // x
                        hadamard_ls_operations.emplace_back(
                            std::make_tuple(
                                'h',
                                0,
                                std::make_pair(
                                    std::vector<size_t>{cur_qubit, (size_t)best_idx},
                                    std::vector<size_t>{ancilla, hs[0]}
                                )
                            )
                        );
                        cur_layer_occupied[ancilla][hs[0]] = PatchType::hadamard;
                        // fmt::println("HADAMARD:");
                        // fmt::println("start_indices: ({}, {})", cur_qubit, best_idx);
                        // fmt::println("dest_patches: ({}, {})", ancilla, hs[0]);
                    }

                }
                else{
                    auto [best_idx, best_ops] = find_nearest_patch_both_sides_hadamard(
                        cur_qubit,
                        hs[0],
                        cur_layer[cur_qubit],
                        cur_layer_occupied[cur_qubit],
                        second_split_patches,
                        hadamard_patches,
                        color_map[i]
                    );

                    hadamard_patches[std::make_pair(ancilla, hs[0])] = std::make_pair(cur_qubit, hs[0]);

                    if (first_split_patches[best_idx] == PatchType::empty){
                        first_split_patches[best_idx] = PatchType::split;
                        second_split_patches[best_idx] = PatchType::split;
                    }
                    else{
                        second_split_patches[best_idx] = PatchType::split;
                    }

                    count_hadamard_start[best_idx]++;

                    if(best_ops.size() > 0) ls_operations.insert(hadamard_ls_operations.end(), best_ops.begin(), best_ops.end());
                    if(!color_map[i]){ // <-> z
                        hadamard_ls_operations.emplace_back(
                            std::make_tuple(
                                'h',
                                0,
                                std::make_pair(
                                    std::vector<size_t>{(size_t)best_idx, cur_qubit},
                                    std::vector<size_t>{hs[0], ancilla}
                                )
                            )
                        );
                        cur_layer_occupied[ancilla][hs[0]] = PatchType::hadamard;
                        // fmt::println("HADAMARD:");
                        // fmt::println("start_indices: ({}, {})", best_idx, cur_qubit);
                        // fmt::println("dest_patches: ({}, {})", hs[0], ancilla);
                    }
                    else{ // |x
                        hadamard_ls_operations.emplace_back(
                            std::make_tuple(
                                'h',
                                0,
                                std::make_pair(
                                    std::vector<size_t>{cur_qubit, (size_t)best_idx},
                                    std::vector<size_t>{ancilla, hs[0]}
                                )
                            )
                        );
                        cur_layer_occupied[ancilla][hs[0]] = PatchType::hadamard;
                        // fmt::println("HADAMARD:");
                        // fmt::println("start_indices: ({}, {})", cur_qubit, best_idx);
                        // fmt::println("dest_patches: ({}, {})", ancilla, hs[0]);
                    }
                }
            }
            std::vector<size_t> start_patches;
            std::vector<size_t> dest_patches;
            for(auto j=0; j<_num_qubits; j++){
                if(pre_layer[cur_qubit][j] == PatchType::simple || pre_layer[cur_qubit][j] == PatchType::hadamard) start_patches.emplace_back(j);
                if(first_split_patches[j] == PatchType::split) dest_patches.emplace_back(j);
            }
            
            n_to_n_merge(cur_qubit, start_patches, dest_patches, color_map[i]);
            int cur_first_split_index = -1;
            // fmt::print("first_split_patches: ");
            // for(auto fs: first_split_patches){
            //     if(fs == PatchType::split) fmt::print("1 ");
            //     else fmt::print("0 ");
            // }
            // fmt::println("");
            // fmt::print("second_split_patches: ");
            // for(auto fs: second_split_patches){
            //     if(fs == PatchType::split) fmt::print("1 ");
            //     else fmt::print("0 ");
            // }
            // fmt::println("");
            std::vector<size_t> first_split_indices;
            std::vector<size_t> second_split_indices;
            bool is_first_split = false;
            for(size_t j=0; j<_num_qubits; j++){
                if(second_split_patches[j] == PatchType::split) second_split_indices.emplace_back(j);
                if(first_split_patches[j] == PatchType::split) {
                    first_split_indices.emplace_back(j);
                }
                if(second_split_patches[j] == PatchType::empty){
                    if(second_split_indices.size() > 0 && first_split_indices.size() > 0 && (second_split_indices.size() == 1 && second_split_indices[0] != first_split_indices[0]) ) ls_operations.emplace_back(color_map[i] ? 'x' : 'z', cur_qubit, std::make_pair(first_split_indices, second_split_indices));
                    second_split_indices.clear();
                    first_split_indices.clear();
                } 
                if(j == _num_qubits-1){
                    if(second_split_indices.size() > 0 && first_split_indices.size() > 0 && (second_split_indices.size() == 1 && second_split_indices[0] != first_split_indices[0])) ls_operations.emplace_back(color_map[i] ? 'x' : 'z', cur_qubit, std::make_pair(first_split_indices, second_split_indices));
                }
            }
            for(size_t j=0; j<_num_qubits; j++){
                if(first_split_patches[j] == PatchType::split) cur_first_split_index = j;
            }

            for(auto op: hadamard_ls_operations){
                auto& [op_type, qubit_id, indices_pair] = op;
                auto& [start_indices, dest_indices] = indices_pair;
                if(!color_map[i]){ // <-> z
                    count_hadamard_start[start_indices[0]]--;
                    if(count_hadamard_start[start_indices[0]] == 0){
                        ls_operations.emplace_back(op_type, 0, indices_pair);
                    }
                    else{
                       ls_operations.emplace_back(op_type, 1, indices_pair);
                    }
                }
                else{ // | x
                    count_hadamard_start[start_indices[1]]--;
                    if(count_hadamard_start[start_indices[1]] == 0){
                        ls_operations.emplace_back(op_type, 0, indices_pair);
                    }
                    else{
                       ls_operations.emplace_back(op_type, 1, indices_pair);
                    }
                }
            }
                

            for(auto start_idx: start_patches) next_layer[cur_qubit][start_idx] = PatchType::empty;
            for(auto dest_idx: dest_patches) next_layer[cur_qubit][dest_idx] = PatchType::simple;

        }
        for (auto& op : ls_operations) {
            auto& [op_type, qubit_id, indices_pair] = op;
            auto& [start_indices, dest_indices] = indices_pair;
            if(op_type == 'h'){
                std::vector<std::pair<size_t, size_t>> dest_patches;
                for(auto j=0; j<dest_indices.size(); j+=2){
                    dest_patches.emplace_back(dest_indices[j], dest_indices[j+1]);
                    if(!color_map[i]){
                        next_layer[dest_indices[j+1]][dest_indices[j]] = PatchType::hadamard;
                    }
                    else{
                        next_layer[dest_indices[j]][dest_indices[j+1]] = PatchType::hadamard;
                    }
                }
                fmt::println("HADAMARD:");
                fmt::println("start_indices: ({}, {})", start_indices[0], start_indices[1]);
                fmt::println("dest_patches: ({}, {})", dest_patches[0].first, dest_patches[0].second);
                if(!color_map[i]){ // <-> z
                    _result.hadamard(std::make_pair(start_indices[0], start_indices[1]), dest_patches, (qubit_id > 0) ? true : false);
                    next_layer[dest_indices[1]][dest_indices[0]] = PatchType::simple;
                    if(qubit_id > 0){
                        next_layer[start_indices[1]][start_indices[0]] = PatchType::simple;
                    }
                    else{
                        next_layer[start_indices[1]][start_indices[0]] = PatchType::empty;
                    }
                    
                    fmt::println("Z Preserve {}: ({}, {})", qubit_id > 0 ? "true" : "false", start_indices[1], start_indices[0]);
                }
                else{ // | x
                    _result.hadamard(std::make_pair(start_indices[0], start_indices[1]), dest_patches, (qubit_id > 0) ? true : false);
                    next_layer[dest_indices[0]][dest_indices[1]] = PatchType::simple;
                    if(qubit_id > 0){
                        next_layer[start_indices[0]][start_indices[1]] = PatchType::simple;
                    }
                    else{
                        next_layer[start_indices[0]][start_indices[1]] = PatchType::empty;
                    }
                    fmt::println("X Preserve {}: ({}, {})", qubit_id > 0 ? "true" : "false", start_indices[0], start_indices[1]);
                }
            }
            else if(op_type == 'z'){ // <-> z
                for(auto start_idx: start_indices) next_layer[start_idx][qubit_id] = PatchType::empty;
                for(auto dest_idx: dest_indices) next_layer[dest_idx][qubit_id] = PatchType::simple;
                
                n_to_n_merge(qubit_id, start_indices, dest_indices, true);
            }
            else if(op_type == 'x'){ // | x
                for(auto start_idx: start_indices) next_layer[qubit_id][start_idx] = PatchType::empty;
                for(auto dest_idx: dest_indices) next_layer[qubit_id][dest_idx] = PatchType::simple;
                
                n_to_n_merge(qubit_id, start_indices, dest_indices, false);
            }
        }
        for(size_t j=0; j<_num_qubits; j++){
            for(size_t k=0; k<_num_qubits; k++){
                pre_layer[j][k] = next_layer[k][j];
            }
        }
        
    }


    return _result;
}

std::pair<int, std::vector<std::tuple<char, size_t, std::pair<std::vector<size_t>, std::vector<size_t>>>>> LatticeSurgerySynthesisStrategy::find_nearest_patch_both_sides(
    size_t cur_qubit,
    size_t j,
    const std::vector<PatchType>& cur_layer_row,
    std::vector<PatchType>& cur_layer_occupied_row,
    std::vector<PatchType>& first_split_patches,
    std::map<std::pair<size_t, size_t>, std::pair<size_t, size_t>>& hadamard_patches,
    bool color_map) const {

    size_t n = cur_layer_row.size();
    struct Result {
        int idx;
        int dist;
        bool blocked;
        std::vector<std::tuple<char, size_t, std::pair<std::vector<size_t>, std::vector<size_t>>>> ops;
    };
    std::vector<Result> candidates;

    for (int dir : {-1, 1}) {
        
        std::vector<std::tuple<char, size_t, std::pair<std::vector<size_t>, std::vector<size_t>>>> ops;
        // int cur_j = j;
        if(cur_layer_occupied_row[j] == PatchType::hadamard) {
            auto it = hadamard_patches.find(std::make_pair(cur_qubit, j));
                if (it != hadamard_patches.end()) {
                    char op_type = color_map ? 'z' : 'x';
                    std::vector<size_t> start_indices{ j };
                    std::vector<size_t> dest_indices{ it->second.second };
                    ops.emplace_back(op_type, cur_qubit, std::make_pair(start_indices, dest_indices));
                    // cur_layer_occupied_row[cur_qubit] = PatchType::borrowed;
                    // Optionally, mark as resolved (update cur_layer_occupied_row[k] if needed)
                }
        }

        bool blocked = false;
        for(int i = j+dir; i>=0 && i<n; i+=dir){
            if(cur_layer_occupied_row[i] == PatchType::empty){
                candidates.push_back({i, std::abs(i - (int)j), blocked, ops});
                break;
            }
            else if(cur_layer_occupied_row[i] == PatchType::borrowed){
                continue;
            }
            else if(cur_layer_occupied_row[i] == PatchType::hadamard){
                blocked = true;
                auto it = hadamard_patches.find(std::make_pair(cur_qubit, (size_t)i));
                if (it != hadamard_patches.end()) {
                    char op_type = color_map ? 'z' : 'x';
                    std::vector<size_t> start_indices = { (size_t)i };
                    std::vector<size_t> dest_indices = { it->second.second };
                    ops.emplace_back(op_type, cur_qubit, std::make_pair(start_indices, dest_indices));
                    // cur_layer_occupied_row[cur_qubit] = PatchType::borrowed;
                    // Optionally, mark as resolved (update cur_layer_occupied_row[k] if needed)
                }
            }

        }
    }

    // Choose the closest candidate
    if (!candidates.empty()) {
        Result best = candidates[0];
        // auto best = std::min_element(candidates.begin(), candidates.end(),
        //                              [](const Result& a, const Result& b) { return (a.dist < b.dist); });
        if(candidates.size() > 1){
            if(candidates[0].blocked && !candidates[1].blocked){
                best = candidates[1];
            }
            else if(!candidates[0].blocked && candidates[1].blocked){
                best = candidates[0];
            }
            else if(candidates[0].dist > candidates[1].dist){
                best = candidates[1];
            }
           
        }

        for(auto op: best.ops){
            auto& [op_type, qubit_id, indices_pair] = op;
            auto& [start_indices, dest_indices] = indices_pair;
            if(op_type == 'h'){
                for(auto start_idx: start_indices) cur_layer_occupied_row[start_idx] = PatchType::borrowed;
            }
        }
        return {best.idx, best.ops};
    }
    return {-1, {}};
}

std::pair<int, std::vector<std::tuple<char, size_t, std::pair<std::vector<size_t>, std::vector<size_t>>>>> LatticeSurgerySynthesisStrategy::find_nearest_patch_both_sides_hadamard(
    size_t cur_qubit,
    size_t j,
    const std::vector<PatchType>& cur_layer_row,    
    std::vector<PatchType>& cur_layer_occupied_row,
    std::vector<PatchType>& second_split_patches,
    std::map<std::pair<size_t, size_t>, std::pair<size_t, size_t>>& hadamard_patches,
    bool color_map ) const {

    size_t n = cur_layer_row.size();
    struct Result {
        int idx;
        int dist;
        bool blocked;
        std::vector<std::tuple<char, size_t, std::pair<std::vector<size_t>, std::vector<size_t>>>> ops;
    };
    std::vector<Result> candidates;

    for (int dir : {-1, 1}) {
        
        std::vector<std::tuple<char, size_t, std::pair<std::vector<size_t>, std::vector<size_t>>>> ops;
        // int cur_j = j;
        if(cur_layer_occupied_row[j] == PatchType::hadamard) {
            auto it = hadamard_patches.find(std::make_pair(cur_qubit, j));
                if (it != hadamard_patches.end()) {
                    char op_type = color_map ? 'z' : 'x';
                    std::vector<size_t> start_indices = { j };
                    std::vector<size_t> dest_indices = { it->second.second };
                    ops.emplace_back(op_type, cur_qubit, std::make_pair(start_indices, dest_indices));
                    // cur_layer_occupied_row[cur_qubit] = PatchType::borrowed;
                    // Optionally, mark as resolved (update cur_layer_occupied_row[k] if needed)
                }
        }

        bool blocked = false;
        for(int i = j+dir; i>=0 && i<n; i+=dir){
            if (second_split_patches[i] == PatchType::split || second_split_patches[i] == PatchType::path){
                candidates.push_back({i, std::abs(i - (int)j), blocked, ops});
                break;
            }
            else if(cur_layer_occupied_row[i] == PatchType::empty){
                candidates.push_back({i, std::abs(i - (int)j), blocked, ops});
                break;
            }
            else if(cur_layer_occupied_row[i] == PatchType::borrowed){
                continue;
            }
            else if(cur_layer_occupied_row[i] == PatchType::hadamard){
                blocked = true;
                auto it = hadamard_patches.find(std::make_pair(cur_qubit, (size_t)i));
                if (it != hadamard_patches.end()) {
                    char op_type = color_map ? 'z' : 'x';
                    std::vector<size_t> start_indices = { (size_t)i };
                    std::vector<size_t> dest_indices = { it->second.second };
                    ops.emplace_back(op_type, cur_qubit, std::make_pair(start_indices, dest_indices));
                    // cur_layer_occupied_row[cur_qubit] = PatchType::borrowed;
                    // Optionally, mark as resolved (update cur_layer_occupied_row[k] if needed)
                }
            }

        }
    }

    // Choose the closest candidate
    if (!candidates.empty()) {
        Result best = candidates[0];
        // auto best = std::min_element(candidates.begin(), candidates.end(),
        //                              [](const Result& a, const Result& b) { return (a.dist < b.dist); });
        if(candidates.size() > 1){
            if(candidates[0].blocked && !candidates[1].blocked){
                best = candidates[1];
            }
            else if(!candidates[0].blocked && candidates[1].blocked){
                best = candidates[0];
            }
            else if(candidates[0].dist > candidates[1].dist){
                best = candidates[1];
            }
           
        }

        for(auto op: best.ops){
            auto& [op_type, qubit_id, indices_pair] = op;
            auto& [start_indices, dest_indices] = indices_pair;
            if(op_type == 'h'){
                for(auto start_idx: start_indices) cur_layer_occupied_row[start_idx] = PatchType::borrowed;
            }
        }
        return {best.idx, best.ops};
    }
    return {-1, {}};
}

std::vector<std::pair<size_t, size_t>> LatticeSurgerySynthesisStrategy::qubit_schedule_min_depth(
    std::vector<std::vector<size_t>>& rc_dependency, size_t num_qubits) const {

    // print the rc_dependency
    // fmt::println("rc_dependency: ");
    // for(int i=0; i<rc_dependency.size(); i++){
    //     fmt::println("{}: {}", i, fmt::join(rc_dependency[i], "|"));
    // }
    
    size_t n = rc_dependency.size();
    // Helper to find a cycle and return the edge to remove
    std::function<bool(const std::vector<std::vector<size_t>>&, std::vector<bool>&, std::vector<bool>&, size_t, std::vector<size_t>&)> find_cycle_path;
    find_cycle_path = [&](const std::vector<std::vector<size_t>>& graph, std::vector<bool>& visited, std::vector<bool>& rec_stack, size_t u, std::vector<size_t>& path) -> bool {
        visited[u] = true;
        rec_stack[u] = true;
        path.push_back(u);
        for (size_t v : graph[u]) {
            if (!visited[v]) {
                if (find_cycle_path(graph, visited, rec_stack, v, path))
                    return true;
            } else if (rec_stack[v]) {
                // Found a cycle, record the cycle path
                path.push_back(v);
                return true;
            }
        }
        rec_stack[u] = false;
        path.pop_back();
        return false;
    };
    // 1. Break cycles by removing only one edge per cycle, prefer parent with out-degree >= 2
    while (true) {
        // Compute in-degree
        std::vector<int> in_degree(n, 0);
        for (size_t u = 0; u < n; ++u)
            for (size_t v : rc_dependency[u])
                in_degree[v]++;
        // Kahn's algorithm for topological sort
        std::queue<size_t> q;
        for (size_t i = 0; i < n; ++i)
            if (in_degree[i] == 0)
                q.push(i);
        std::vector<size_t> topo_order;
        std::vector<bool> visited(n, false);
        while (!q.empty()) {
            size_t u = q.front(); q.pop();
            topo_order.push_back(u);
            visited[u] = true;
            for (size_t v : rc_dependency[u]) {
                in_degree[v]--;
                if (in_degree[v] == 0)
                    q.push(v);
            }
        }
        if (topo_order.size() == n) {
            break; // No cycles
        } else {
            // Cycle detected, find and remove only one edge in the cycle
            std::vector<bool> dfs_visited(n, false), rec_stack(n, false);
            std::vector<size_t> cycle_path;
            bool found = false;
            for (size_t i = 0; i < n; ++i) {
                if (!dfs_visited[i]) {
                    if (find_cycle_path(rc_dependency, dfs_visited, rec_stack, i, cycle_path)) {
                        found = true;
                        break;
                    }
                }
            }
            if (found && cycle_path.size() > 1) {
                // Special case: 2-node cycle
                // if (cycle_path.size() == 3) {
                //     size_t u = cycle_path[1];
                //     size_t v = cycle_path[0];
                //     auto& edges = rc_dependency[u];
                //     edges.erase(std::remove(edges.begin(), edges.end(), v), edges.end());
                //     fmt::println("[WARNING] 2-node cycle detected and broken by removing edge {} -> {}", u, v);
                //     continue;
                // }
                // The cycle is from cycle_path.back() to cycle_path[cycle_path.size()-2], and so on
                // Find an edge (u, v) in the cycle where u has out-degree >= 2
                size_t cycle_start = cycle_path.back();
                size_t cycle_end = cycle_path.size() - 1;
                //print the cycle path
                // fmt::println("cycle path: {}", fmt::join(cycle_path, "|"));
                // for(auto c: cycle_path){
                //     fmt::println("{}: {}", c, fmt::join(rc_dependency[c], "|"));
                // }
                std::pair<size_t, size_t> edge_to_remove = {n, n};
                bool found_outdeg2 = false;
                for (size_t i = cycle_end; i > 0; --i) {
                    size_t u = cycle_path[i];
                    size_t v = cycle_path[i-1];
                    // print the cycle path
                    // fmt::println("u: {}, v: {}", u, v);
                    if (std::find(rc_dependency[u].begin(), rc_dependency[u].end(), v) != rc_dependency[u].end()) {
                        if (rc_dependency[u].size() >= 2) {
                            edge_to_remove = {u, v};
                            found_outdeg2 = true;
                            break;
                        }
                        // fallback: remember any edge in the cycle
                        if (edge_to_remove.first == n) {
                            edge_to_remove = {u, v};
                        }
                    }
                    if (v == cycle_start) break;
                }
                if (edge_to_remove.first != n && edge_to_remove.second != n) {
                    auto& edges = rc_dependency[edge_to_remove.first];
                    edges.erase(std::remove(edges.begin(), edges.end(), edge_to_remove.second), edges.end());
                    if (found_outdeg2) {
                        fmt::println("[WARNING] Cycle detected and broken by removing edge {} -> {} (parent out-degree >= 2)", edge_to_remove.first, edge_to_remove.second);
                    } else {
                        fmt::println("[WARNING] Cycle detected and broken by removing edge {} -> {} (fallback)", edge_to_remove.first, edge_to_remove.second);
                    }
                } else {
                    fmt::println("[ERROR] Could not find cycle edge to break");
                    break;
                }
            } else {
                fmt::println("[ERROR] Could not find cycle to break");
                break;
            }
        }
    }
    // 2. BFS from all roots
    std::vector<int> in_degree(n, 0);
    for (size_t u = 0; u < n; ++u)
        for (size_t v : rc_dependency[u])
            in_degree[v]++;
    std::queue<size_t> q;
    for (size_t i = 0; i < n; ++i)
        if (in_degree[i] == 0)
            q.push(i);
    std::vector<bool> scheduled(n, false);
    std::vector<std::pair<size_t, size_t>> qubit_schedule;
    while (!q.empty()) {
        size_t u = q.front(); q.pop();
        if (scheduled[u]) continue;
        scheduled[u] = true;
        if (!rc_dependency[u].empty()) {
            qubit_schedule.emplace_back(u, rc_dependency[u][0]); // first neighbor
        } else {
            qubit_schedule.emplace_back(u, num_qubits);
        }
        for (size_t v : rc_dependency[u]) {
            in_degree[v]--;
            if (in_degree[v] == 0)
                q.push(v);
        }
    }
    // 4. Ensure all nodes appear once
    // (already handled by scheduled[] check)
    return qubit_schedule;
}


void LatticeSurgerySynthesisStrategy::n_to_n_merge(size_t qubit_id, std::vector<size_t> start_indices, std::vector<size_t> dest_indices, bool is_x){
    // fmt::println("start to merge the qubits");

    // print the start and dest indices
    // fmt::println("start indices: {}", fmt::join(start_indices, ", "));
    // fmt::println("dest indices: {}", fmt::join(dest_indices, ", "));

    std::vector<std::pair<size_t, size_t>> start_patches;
    std::vector<std::pair<size_t, size_t>> dest_patches;

    // <-> z | x
    fmt::println("MERGE:");
    if(!is_x){
        for(auto start_idx: start_indices) start_patches.emplace_back(start_idx, qubit_id);
        for(auto dest_idx: dest_indices) dest_patches.emplace_back(dest_idx, qubit_id);
        fmt::println("start indices: ({}, {})", fmt::join(start_indices, "|"), qubit_id);
        fmt::println("dest indices: ({}, {})", fmt::join(dest_indices, "|"), qubit_id);
    } else{
        for(auto start_idx: start_indices) start_patches.emplace_back(qubit_id, start_idx);
        for(auto dest_idx: dest_indices) dest_patches.emplace_back(qubit_id, dest_idx);
        fmt::println("start indices: ({}, {})", qubit_id, fmt::join(start_indices, "|"));
        fmt::println("dest indices: ({}, {})", qubit_id, fmt::join(dest_indices, "|"));
    }
    
    // fmt::println("start indices: {}", fmt::join(start_patches, ", "));
    // fmt::println("dest indices: {}", fmt::join(dest_patches, ", "));
    _result.n_to_n(start_patches, dest_patches);
}

std::vector<std::vector<ZXVertex*>> LatticeSurgerySynthesisStrategy::create_vertex_map(const zx::ZXGraph* zxgraph){
    std::vector<std::vector<ZXVertex*>> vertex_map(5, std::vector<ZXVertex*>(std::max(zxgraph->num_inputs(), zxgraph->num_outputs()), nullptr));
    for(auto& vertex: zxgraph->get_vertices()){
        if(vertex->get_col() >= vertex_map.size()) vertex_map.resize(vertex->get_col()+1, std::vector<ZXVertex*>(std::max(zxgraph->num_inputs(), zxgraph->num_outputs()), nullptr));
        vertex_map[vertex->get_col()][vertex->get_row()] = vertex;
    }
    fmt::println("vertex_map size: {} X {}", vertex_map.size(), vertex_map[0].size());
    return vertex_map;
};

LatticeSurgerySynthesisStrategy::LatticeSurgerySynthesisStrategy(zx::ZXGraph* zxgraph):_zxgraph(zxgraph){

    fmt::println("start to initialize the lattice surgery synthesis strategy");
    _num_qubits = std::max(zxgraph->num_inputs(), zxgraph->num_outputs());
    fmt::println("number of qubits: {}", _num_qubits);

    // create result lattice surgery
    fmt::println("create result lattice surgery");
    _result = LatticeSurgery{_num_qubits, _num_qubits};

    // Initialize logical tracking for the grid
    fmt::println("initialize logical tracking");
    _result.init_logical_tracking(_num_qubits * _num_qubits);

    // create vertex map
    fmt::println("create vertex map");
    _vertex_map = create_vertex_map(zxgraph);

   // initialize the logical patch
   fmt::println("initialize the logical path");
   for(size_t i=0; i<_num_qubits; i++){
        _result.add_logical_patch(i, i);
   }

}

}