#include "zxgraph_to_latticesurgery.hpp"
#include <fmt/core.h>
#include "latticesurgery/latticesurgery.hpp"
#include "util/dvlab_string.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include <cstddef>
#include <limits>
#include <queue>
#include <ranges>
#include <utility>
#include <vector>


namespace qsyn {

using zx::ZXVertex;
using latticesurgery::LatticeSurgery;
using zx::VertexType;
using zx::EdgeType;

std::optional<LatticeSurgery> to_latticesurgery(const zx::ZXGraph* zxgraph){
    fmt::println("In zx to lattice surgery");

    // create result lattice surgery
    LatticeSurgery result{std::max(zxgraph->num_inputs(), zxgraph->num_outputs()), std::max(zxgraph->num_inputs(), zxgraph->num_outputs())};

    // initialize logical patch in lattice surgery
    for(size_t i=0; i< zxgraph->num_inputs(); i++){
        result.add_logical_patch(i, i);
    }
    result.init_logical_tracking(std::max(zxgraph->num_inputs(), zxgraph->num_outputs()) * std::max(zxgraph->num_inputs(), zxgraph->num_outputs()));



    // get the frontier zx vertices for each row
    std::vector<ZXVertex*> frontiers(zxgraph->num_inputs(), nullptr);
    std::vector<ZXVertex*> next_frontiers(zxgraph->num_inputs(), nullptr);
    size_t frontier_col = 0;
    size_t max_col = (*zxgraph->get_outputs().begin())->get_col();
    for(auto input: zxgraph->get_inputs()){
        frontiers[input->get_row()] = input;
    }

    // set the orientation of lattice surgery <-> z | x

    // default logical qubit patch will be at the diagonal

    while(frontier_col < max_col){
        fmt::println("frontier col = {}", frontier_col);
        // set next_frontier to frontier, so we will get frontier if there is no merge and split needed for this col
        for(size_t index=0; index < frontiers.size(); index++){
            next_frontiers[index] = frontiers[index];
        }

        // orientation (for next frontier), default orientation z <-> x |
        VertexType orientation = VertexType::boundary;

        // if orientation = z, split in |, merge in <->
        // if orientation = x, split in <->, merge in |

        std::vector<std::vector<std::pair<size_t,size_t>>> merge_patches(zxgraph->num_inputs(), std::vector<std::pair<size_t,size_t>>{});

        fmt::println("before parsing frontiers");
        // for each frontier find how it should split
        for(size_t index = 0; index < frontiers.size(); ++index){
            fmt::println("parsing frontier {}", index);
            auto* frontier = frontiers[index];
            std::vector<unsigned> split_index(zxgraph->num_inputs(), 0); // no: 0, simple edge: 1, hadamard edge: 2
            std::vector<bool> hadamard_edges(zxgraph->num_inputs(), false);
            size_t count_hadamard = 0;
            std::vector<std::pair<size_t, size_t>> split_patches;
            bool exist_neighbor = false;
            size_t count_split = 0;

            // find the index for split patches
            for(auto [neighbor, edge]: zxgraph->get_neighbors(frontier)){
                if(neighbor->get_col() == frontier_col+1){
                    exist_neighbor = true;
                    next_frontiers[neighbor->get_row()] = neighbor;
                    split_index[neighbor->get_row()] = (edge == EdgeType::simple)? 1 : 2; // simple edge: 1, hadamard edge: 2
                    if(orientation == VertexType::boundary){
                        orientation = neighbor->type();
                    }
                    if(edge == EdgeType::hadamard) count_hadamard++;
                    count_split ++;
                }
            }

            fmt::println("after finding split patches");
            // if the current patch don't need to split and merge with others, assign it to a position that won't block others
            if(!exist_neighbor) continue;

            if(count_hadamard == 1 and count_split == 1){
                fmt::println("In only 1 hadamard");
                size_t final_position = 0;
                for(size_t i=0; i<split_index.size(); i++){ 
                    if(split_index[i] > 0){
                        final_position = i;
                        break;
                    }
                }
                fmt::println("after finding the final position");
                if(orientation == VertexType::z){ // if next frontier orientation = z, split | => merge <->
                    if(final_position == index-1 || index == split_index.size()-1){
                        result.hadamard(std::make_pair(index, index), std::make_pair(index, index-1));
                        if(final_position != index-1){
                            std::vector<std::pair<size_t, size_t>> final_pos{std::make_pair(index, final_position)};
                            result.one_to_n(std::make_pair(index, index-1), final_pos);
                        }
                    }
                    else{
                        result.hadamard(std::make_pair(index, index), std::make_pair(index, index+1));
                        if(final_position != index+1){
                            std::vector<std::pair<size_t, size_t>> final_pos{std::make_pair(index, final_position)};
                            result.one_to_n(std::make_pair(index, index+1), final_pos);
                        }
                    }
                }
                else{
                    if(final_position == index-1 || index == split_index.size()-1){
                        result.hadamard(std::make_pair(index, index), std::make_pair(index-1, index));
                        if(final_position != index-1){
                            std::vector<std::pair<size_t, size_t>> final_pos{std::make_pair(final_position,index)};
                            result.one_to_n(std::make_pair(index-1, index), final_pos);
                        }
                    }
                    else{
                        result.hadamard(std::make_pair(index, index), std::make_pair(index+1, index));
                        if(final_position != index+1){
                            std::vector<std::pair<size_t, size_t>> final_pos{std::make_pair(final_position,index)};
                            result.one_to_n(std::make_pair(index+1, index), final_pos);
                        }
                    }
                }
                fmt::println("finish in only 1 hadamard");
                continue;
            }

            // No hadamard: direct split and continue
            if(count_hadamard == 0){
                fmt::println("In no hadamard ls operation appending");
                std::vector<std::pair<size_t,size_t>> split_id_list;
                if(orientation == VertexType::z){ // if next frontier orientation = z, split | => merge <->
                    for(size_t i=0; i<split_index.size(); i++){
                        if(split_index[i] == 0) continue;
                        else{
                            split_id_list.emplace_back(std::make_pair(index , i));
                            merge_patches[i].emplace_back(std::make_pair(index , i));
                        }
                    }
                }
                else{  // if next frontier orientation = x, split <-> => merge |
                    for(size_t i=0; i<split_index.size(); i++){
                        if(split_index[i] == 0) continue;
                        else{
                            split_id_list.emplace_back(std::make_pair(i,index));
                            merge_patches[i].emplace_back(std::make_pair(i, index));
                        }
                    }
                }
                result.one_to_n(std::make_pair(index, index), split_id_list);
                fmt::println("finish in on hadamard");

                continue;
            }


            fmt::println("In hadamard edge ls operation appending");
            
            // partition
            std::vector<unsigned> partitions(split_index.size(), 0);
            unsigned pre_type = 0; // 0: not yet assign, 1: simple, 2: hadamard

            fmt::println("split index size: {}", split_index.size());

            // partition into a sequence of 1(simple) and 2(hadamard)
            for(size_t p_index=0; p_index < split_index.size(); p_index++){ 
                fmt::println("p_index: {} type({})", p_index, pre_type);
                switch(split_index[p_index]){
                    case 1: // simple
                        fmt::println("in case 1");
                        partitions[p_index] = 1;
                        if(pre_type == 0){
                            for(int i=p_index-1; i>=0; i--){ 
                                fmt::println("i={}", i);
                                partitions[i] = 1;
                            }
                        }
                        pre_type = 1;
                        break;
                    case 2: // hadamard
                        fmt::println("in case 2");
                        partitions[p_index] = 2;
                        if(pre_type == 0){
                            for(int i=p_index-1; i>=0; i--) partitions[i] = 2;
                        }
                        pre_type = 2;
                        break;
                    default: 
                        fmt::println("in default");
                        partitions[p_index] = pre_type;
                        break;
                }
            }
            fmt::println("");

            fmt::println("After patition");
            
            auto cur_type = partitions[0]; // 1: simple, 2: hadamard

            size_t pre_count = 0;
            size_t cur_count = 0;
            for(size_t i=0; i<partitions.size(); i++){
                if(partitions[i] == cur_type){
                    size_t start = i;
                    cur_count++;
                    while(i+1 < partitions.size() && partitions[i+1] == cur_type){
                        cur_count++;
                        i++;
                    }
                    if(cur_count == 1 && cur_type == 2){
                        if(pre_count > 1 || i == partitions.size()-1) partitions[start-1] = cur_type;
                        else{
                            partitions[i+1] = cur_type;
                            i++;
                        } 
                    }
                    cur_type = partitions[i+1];
                    pre_count = cur_count;
                    cur_count = 0;
                }
                else{
                    fmt::println("should not be here");
                }
            }

            fmt::println("After rearrange partition");
            

            // first split of logical qubits

            std::vector<std::pair<size_t,size_t>> first_split;
            std::vector<std::pair<std::pair<size_t, size_t>,std::pair<size_t, size_t>>> hadamards_b_splits;
            std::vector<std::pair<std::pair<size_t,size_t>, std::vector<std::pair<size_t,size_t>>>> second_split;

            cur_type = partitions[0];
            for(size_t i=0; i<split_index.size(); i++){
                fmt::println("i={}", i);
                if(cur_type == 1){ // simple
                    fmt::println("in simple");
                    size_t ancilla_index = i;
                    size_t end_index = i;
                    std::vector<size_t> simple_list;
                    std::vector<size_t> second_list;
                    fmt::println("before simple while");
                    while(i < split_index.size() && split_index[i] != 2){
                        end_index = i;
                        if(split_index[i] == cur_type && partitions[i] == 1) {
                            first_split.emplace_back(std::make_pair(index, i));
                            simple_list.emplace_back(i);
                        }
                        else if (split_index[i] == cur_type && partitions[i] != 1) {
                            second_list.emplace_back(i);
                            end_index = i-1;
                        }
                        else if(partitions[i] == 1){
                            ancilla_index = i;
                        }
                        i++;
                    }
                    fmt::println("after simple while");
                    i = end_index;
                    if(orientation == VertexType::z){ // if next frontier orientation = z, split | => merge <->
                        for(auto i: simple_list) first_split.emplace_back(std::make_pair(index, i));
                        if (!second_list.empty()){
                            std::vector<std::pair<size_t, size_t>> second_sp;
                            
                            if(simple_list.empty()) {
                                for(auto i: second_list) second_sp.emplace_back(std::make_pair(index, i));
                                second_split.emplace_back(std::make_pair(std::make_pair(index, ancilla_index), second_sp));
                            }
                            else{
                                for(auto i: second_list){
                                    if(i < ancilla_index) second_split.emplace_back(std::make_pair(std::make_pair(index, simple_list.front()), std::vector<std::pair<size_t, size_t>>{std::make_pair(index, simple_list.front()), std::make_pair(index, i)}));
                                    else second_split.emplace_back(std::make_pair(std::make_pair(index, simple_list.back()), std::vector<std::pair<size_t, size_t>>{std::make_pair(index, simple_list.back()), std::make_pair(index, i)}));
                                }
                            }
                        }
                        
                    }
                    else{
                        for(auto i: simple_list) first_split.emplace_back(std::make_pair(i,index));
                        if (!second_list.empty()){
                            std::vector<std::pair<size_t, size_t>> second_sp;
                            
                            if(simple_list.empty()) {
                                for(auto i: second_list) second_sp.emplace_back(std::make_pair(i,index));
                                second_split.emplace_back(std::make_pair(std::make_pair(ancilla_index,index), second_sp));
                            }
                            else{
                                for(auto i: second_list){
                                    if(i < ancilla_index) second_split.emplace_back(std::make_pair(std::make_pair(simple_list.front(), index), std::vector<std::pair<size_t, size_t>>{std::make_pair(simple_list.front(), index), std::make_pair(i, index)}));
                                    else second_split.emplace_back(std::make_pair(std::make_pair(simple_list.back(), index), std::vector<std::pair<size_t, size_t>>{std::make_pair(simple_list.back(), index), std::make_pair( i, index)}));
                                }
                            }
                        }
                    }
                    cur_type=2;
                }
                else if(cur_type == 2){ // hadamard
                    size_t ancilla_index = i;
                    size_t first_hadamard = i+1;
                    size_t start_index = i;
                    size_t end_index = i;
                    size_t hadamard_count = (split_index[i]==2)? 1:0;
                    std::vector<size_t> hadamards_second_split;
                    std::vector<std::pair<size_t, size_t>> hadamards_list; // (ancilla, destination)
                    i++;
                    while(i < split_index.size() && partitions[i] == cur_type){
                        end_index = i;
                        if(split_index[i] == 0 || split_index[i] == 1){
                            if(split_index[i-1] == cur_type && first_hadamard != i-1){
                                ancilla_index = i;
                                first_hadamard = i-1;
                                hadamards_list.emplace_back(std::make_pair(i, i-1));
                            } 
                            else if(i+1 < split_index.size() && split_index[i+1] == cur_type){
                                ancilla_index = i;
                                first_hadamard = i+1;
                                hadamards_list.emplace_back(std::make_pair(i, i+1));
                            }
                        }
                        else hadamard_count++;
                        i++;
                    }

                    if(split_index[end_index] == 1) i=end_index-1;
                    else i=end_index;

                    if(orientation == VertexType::z){ // if next frontier orientation = z, split | => merge <->
                        if(hadamards_list.empty() || hadamards_list.size() != hadamard_count){
                            first_split.emplace_back(std::make_pair(index, ancilla_index));
                            hadamards_b_splits.emplace_back(std::make_pair(std::make_pair(index, ancilla_index), std::make_pair(index, first_hadamard)));
                            if(hadamard_count > 1){
                                std::vector<std::pair<size_t, size_t>> hadamard_index;
                                for(size_t check_h=start_index; check_h<=i; check_h++){
                                    if(split_index[check_h] == 2){
                                        hadamard_index.emplace_back(std::make_pair(index, check_h));
                                    }
                                }
                                second_split.emplace_back(std::make_pair(std::make_pair(index, first_hadamard), hadamard_index));
                            }
                        }
                        else{
                            for(auto[ancilla, h]: hadamards_list){
                                first_split.emplace_back(std::make_pair(index, ancilla));
                                hadamards_b_splits.emplace_back(std::make_pair(std::make_pair(index, ancilla), std::make_pair(index, h)));
                                split_index[h] = 0;
                            }
                        }
                    }
                    else{ // if next frontier orientation = x, split <-> => merge |
                        if(hadamards_list.empty() || hadamards_list.size() != hadamard_count){
                            first_split.emplace_back(std::make_pair(index, ancilla_index));
                            hadamards_b_splits.emplace_back(std::make_pair(std::make_pair(ancilla_index, index), std::make_pair(first_hadamard, index)));
                            if(hadamard_count > 1){
                                std::vector<std::pair<size_t, size_t>> hadamard_index;
                                for(size_t check_h=start_index; check_h<=i; check_h++){
                                    if(split_index[check_h] == 2){
                                        hadamard_index.emplace_back(std::make_pair(check_h, index));
                                    }
                                }
                                second_split.emplace_back(std::make_pair(std::make_pair(first_hadamard,index), hadamard_index));
                            }
                        }
                        else{
                            for(auto[ancilla, h]: hadamards_list){
                                first_split.emplace_back(std::make_pair(ancilla, index));
                                hadamards_b_splits.emplace_back(std::make_pair(std::make_pair(ancilla,index), std::make_pair(h,index)));
                                split_index[h] = 0;
                            }
                        }
                    }
                    cur_type = 1;
                }
            }

            // first split

            result.one_to_n(std::make_pair(index, index), first_split);
            
            // hadamard between split

            for(auto [start, dest]: hadamards_b_splits){
                result.hadamard(start, dest); // to be fixed
            }

            // second split

            for(auto [start, dest_list]: second_split){
                result.one_to_n(start, dest_list);
            }

            // final merge
            for(size_t i=0; i<split_index.size(); i++){
                if(split_index[i] > 0){
                    if(orientation == VertexType::z){ // if next frontier orientation = z, split | => merge <->
                        merge_patches[i].emplace_back(std::make_pair(index, i));
                    }
                    else{ // if next frontier orientation = x, split <-> => merge |
                        merge_patches[i].emplace_back(std::make_pair(i, index));
                    }
                }
            }
        }

        for(size_t i=0; i<merge_patches.size(); i++){
            result.n_to_one(merge_patches[i], std::make_pair(i, i));
        }
        std::swap(frontiers, next_frontiers);
        frontier_col++;
    }







    

    return result;
};

}