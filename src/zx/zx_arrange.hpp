#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include "zx/zx_def.hpp"
#include <cstddef>
#include <vector>
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

namespace qsyn::zx {

class ZXGraph;
class ZXVertex;

class Arranger{

public:
    Arranger(ZXGraph* g): _graph(g){
        size_t max_id = 0;
        for(auto vertex: _graph->get_vertices()){
            if(max_id < vertex->get_id()) max_id = vertex->get_id();
        }
        _io_marks.resize(max_id+1, false);
        for(auto input: _graph->get_inputs()) _io_marks[input->get_id()] = 1;
        for(auto output: _graph->get_outputs()){
            _io_marks[output->get_id()] = 2;
            if(_max_col < output->get_col()){
                _max_col = output->get_col();
            }
        }
    };
    void arrange();
    void internal_vertex_splitting();
    // void separate_internal_input_output();
    void create_vertex_map();
    void stitching_vertex();
    void hadamard_edge_absorb();
    std::unordered_map<ZXVertex*, std::unordered_set<ZXVertex*>> calculate_smallest_dag();
    void layer_scheduling(std::unordered_map<ZXVertex*, std::unordered_set<ZXVertex*>> dag);
    // std::vector<std::vector<ZXVertex*>>& get_vertex_map(){ return _vertex_map; };

private:
    ZXGraph* _graph;
    std::vector<unsigned short> _io_marks; // 1: input, 2: output, 3: input neighbor, 4: output neighbor, 5: input and output neighbor
    size_t _max_col = 0;
    size_t _input_boundary = 0;
    size_t _output_boundary = 0;
    std::vector<std::vector<ZXVertex*>> _vertex_map; // (x,y) = (col,row)

};

}


