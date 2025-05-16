#include "./zx_arrange.hpp"
#include <fmt/core.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>
#include "qsyn/qsyn_type.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"
#include <fstream>
#include <sstream>
#include <queue>




using namespace qsyn::zx;

struct Task{
    int priority;
    ZXVertex* vertex;

    bool operator<(const Task& other) const{
        return priority < other.priority;
    }
};

void Arranger::arrange(){
    fmt::println("Start Arrange");

    // arrange input/output position
    io_vertex_arrange();

    // do graph coloring so that there is no edge that have both nodes at the same col
    internal_vertex_arrange();

    // split the vertex if the neighbor vertice is not at the neighbor columns
    internal_vertex_splitting();

    // absorb hadamard edge
    hadamard_edge_absorb();

    
}

void Arranger::hadamard_edge_absorb(){
    fmt::println("In Hadamard Edge Absortion");

    int cost_z_start = 0;
    int cost_x_start = 0;
    
    // estimate the hadamard cost if start with Z spiders
    for(size_t i=2; i<_vertex_map.size(); i+=2){
        for(size_t j=0; j<_vertex_map[0].size(); j++){
            if(_vertex_map[i][j] == NULL) continue;
            for(const auto& [neighbor, edge]: _graph->get_neighbors(_vertex_map[i][j])){
                if(edge == EdgeType::simple) cost_z_start ++;
                else cost_z_start --;
            }
        }
    }


    // estimate the hadamard cost if start with X spiders
    for(size_t i=1; i<_vertex_map.size(); i+=2){
        for(size_t j=0; j<_vertex_map[0].size(); j++){
            if(_vertex_map[i][j] == NULL) continue;
            for(const auto& [neighbor, edge]: _graph->get_neighbors(_vertex_map[i][j])){
                if(edge == EdgeType::simple) cost_x_start ++;
                else cost_x_start --;
            }
        }
    }

    fmt::println("z start cost: {} x start cost: {}", cost_z_start, cost_x_start);
    // absorb with smaller cost
    size_t start_index = 2;
    fmt::println("vertex map size: {}", _vertex_map.size());
    if(cost_z_start > cost_x_start) start_index=1;
    for(size_t i=start_index; i<_vertex_map.size()-1; i+=2){
        fmt::println("i={}: ", i);
        for(size_t j=0; j<_vertex_map[0].size(); j++){
            fmt::println("j={}, ptr={}", j, static_cast<void*>(_vertex_map[i][j]));
            if(_vertex_map[i][j] == NULL || _vertex_map[i][j]->is_boundary()) continue;
            toggle_vertex(*_graph, _vertex_map[i][j]->get_id());
        }
    }

};

void Arranger::internal_vertex_splitting(){
    fmt::println("In Internal Vertex Splitting");

    // create vertex map
    for(size_t i=0; i<=_max_col; i++){
        _vertex_map.emplace_back(std::vector<ZXVertex*>(_graph->num_inputs(), nullptr));
    }
    for(const auto& vertex: _graph->get_vertices()){
        if(_io_marks[vertex->get_id()] != 1 && _io_marks[vertex->get_id()] != 2){
            _vertex_map[vertex->get_col()][vertex->get_row()] = vertex;
        }
    }

    // find the split order, using priority queue
    std::priority_queue<Task> priority_queue; 


    for(const auto& vertex: _graph->get_vertices()){
        if(_io_marks[vertex->get_id()] != 1 && _io_marks[vertex->get_id()] != 2){
            int count_far_neighbor = 0;
            for(const auto& neighbor: _graph->get_neighbors(vertex)){
                
                if(_io_marks[neighbor.first->get_id()] != 1 && _io_marks[neighbor.first->get_id()] != 2){
                    // find the edge that cross > 1 column
                    if(abs(vertex->get_col()-neighbor.first->get_col()) > 1 && vertex->get_row() != neighbor.first->get_row()){
                        count_far_neighbor ++;
                    }
                }
            }
            if(count_far_neighbor > 0) priority_queue.push({count_far_neighbor, vertex});
            
        }
    }

    // fmt::println("need splitting vertex: {}", priority_queue.size());

    while(!priority_queue.empty()){
        Task cur_task = priority_queue.top();
        priority_queue.pop();
        // fmt::println("Processing id: {}", cur_task.vertex->get_id());
        auto cur_vertex = cur_task.vertex;
        std::vector<ZXVertex*> smaller_vertices;
        std::vector<ZXVertex*> bigger_vertices;
        for(const auto& neighbor: _graph->get_neighbors(cur_vertex)){
            if(neighbor.first->get_col() - cur_vertex->get_col() < -1 && neighbor.first->get_row() != cur_vertex->get_row()){
                smaller_vertices.emplace_back(neighbor.first);
            }
            else if (neighbor.first->get_col() - cur_vertex->get_col() > 1 && neighbor.first->get_row() != cur_vertex->get_row()){
                bigger_vertices.emplace_back(neighbor.first);
            }
        }
        std::sort(smaller_vertices.begin(), smaller_vertices.end(), [](ZXVertex* a, ZXVertex* b){
            return a->get_col() <= b->get_col();
        });
        std::sort(bigger_vertices.begin(), bigger_vertices.end(), [](ZXVertex* a, ZXVertex* b){
            return a->get_col() >= b->get_col();
        });
        // fmt::println("smaller vertices: {}", smaller_vertices.size());
        // fmt::println("bigger vertices: {}", bigger_vertices.size());
        if(!smaller_vertices.empty()){
            if(_io_marks[cur_vertex->get_id()] == 5 || _io_marks[cur_vertex->get_id()] == 3){
                for(const auto& vertex: smaller_vertices){
                    if(abs((vertex->get_col())-cur_vertex->get_col()) <= 1) continue;
                    size_t new_col = vertex->get_col()+1;
                    if(_vertex_map[new_col][cur_vertex->get_row()] == NULL){
                        auto new_vertex = _graph->add_vertex(cur_vertex->type(), Phase{0}, cur_vertex->get_row(), new_col);
                        _vertex_map[new_col][cur_vertex->get_row()] = new_vertex;
                        // fmt::println("add vertex (smaller input): {} ({},{})", new_vertex->get_id(), new_vertex->get_col(), new_vertex->get_row());
                        _graph->add_edge(new_vertex, cur_vertex, EdgeType::simple);
                        for(auto [neighbor, edge]: _graph->get_neighbors(cur_vertex)){
                            if(neighbor->get_col() - cur_vertex->get_col() < -1 && (neighbor->get_col() < new_vertex->get_col())){
                                _graph->add_edge(neighbor, new_vertex, edge);
                                _graph->remove_edge(std::make_pair(std::make_pair(neighbor, cur_vertex), edge));
                            }
                        }
                    }
                }
            }
            else if((_io_marks[cur_vertex->get_id()] == 4 || _io_marks[cur_vertex->get_id()] == 5) && cur_vertex->get_col() > _output_boundary){
                for(const auto& vertex: smaller_vertices){
                    if(abs(vertex->get_col()-cur_vertex->get_col()) <= 1) continue;
                    size_t new_col = vertex->get_col()+1;
                    if(new_col < _output_boundary) new_col = _output_boundary;
                    if(_vertex_map[new_col][cur_vertex->get_row()] == NULL){
                        auto new_vertex = _graph->add_vertex(cur_vertex->type(), Phase{0}, cur_vertex->get_row(), new_col);
                        _vertex_map[new_col][cur_vertex->get_row()] = new_vertex;
                        // fmt::println("add vertex (smaller output): {} ({},{})", new_vertex->get_id(), new_vertex->get_col(), new_vertex->get_row());
                        _graph->add_edge(new_vertex, cur_vertex, EdgeType::simple);
                        for(auto [neighbor, edge]: _graph->get_neighbors(cur_vertex)){
                            // fmt::println("neighbor col: {} cur col: {}", neighbor->get_col(), cur_vertex->get_col());
                            if(neighbor->get_col() - cur_vertex->get_col() < -1 && (neighbor->get_col() < new_vertex->get_col())){
                                _graph->add_edge(neighbor, new_vertex, edge);
                                _graph->remove_edge(std::make_pair(std::make_pair(neighbor, cur_vertex), edge));
                            }
                        }
                    }
                }
            }
        }
        if(!bigger_vertices.empty()){
            if(_io_marks[cur_vertex->get_id()] == 5 || _io_marks[cur_vertex->get_id()] == 3){
                for(const auto& vertex: bigger_vertices){
                    if(abs(vertex->get_col()-cur_vertex->get_col()) <= 1) continue;
                    size_t new_col = vertex->get_col()-1;
                    if(new_col > _input_boundary) new_col = _input_boundary;
                    if(_vertex_map[new_col][cur_vertex->get_row()] == NULL){
                        auto new_vertex = _graph->add_vertex(cur_vertex->type(), Phase{0}, cur_vertex->get_row(), new_col);
                        _vertex_map[new_col][cur_vertex->get_row()] = new_vertex;
                        // fmt::println("add vertex (bigger input): {} ({},{})", new_vertex->get_id(), new_vertex->get_col(), new_vertex->get_row());
                        _graph->add_edge(new_vertex, cur_vertex, EdgeType::simple);
                        for(auto [neighbor, edge]: _graph->get_neighbors(cur_vertex)){
                            if(neighbor->get_col() - cur_vertex->get_col() > 1 && (neighbor->get_col() > new_vertex->get_col())){
                                _graph->add_edge(neighbor, new_vertex, edge);
                                _graph->remove_edge(std::make_pair(std::make_pair(neighbor, cur_vertex), edge));
                            }
                        }
                    }
                }
            }
            else if((_io_marks[cur_vertex->get_id()] == 4 || _io_marks[cur_vertex->get_id()] == 5) && cur_vertex->get_col() > _output_boundary){
                for(const auto& vertex: bigger_vertices){
                    if(abs(vertex->get_col() - cur_vertex->get_col()) <= 1) continue;
                    size_t new_col = vertex->get_col()-1;
                    if(_vertex_map[new_col][cur_vertex->get_row()] == NULL){
                        auto new_vertex = _graph->add_vertex(cur_vertex->type(), Phase{0}, cur_vertex->get_row(), new_col);
                        _vertex_map[new_col][cur_vertex->get_row()] = new_vertex;
                        // fmt::println("add vertex: {} ({},{})", new_vertex->get_id(), new_vertex->get_col(), new_vertex->get_row());
                        _graph->add_edge(new_vertex, cur_vertex, EdgeType::simple);
                        for(auto [neighbor, edge]: _graph->get_neighbors(cur_vertex)){
                            if(neighbor->get_col() - cur_vertex->get_col() > 1 && (neighbor->get_col() > new_vertex->get_col())){
                                _graph->add_edge(neighbor, new_vertex, edge);
                                _graph->remove_edge(std::make_pair(std::make_pair(neighbor, cur_vertex), edge));
                            }
                        }
                    }
                }
            }
        }
    }

}

void Arranger::internal_vertex_arrange(){
    fmt::println("In Internal Vertex Arrange");

    std::vector<size_t> index_map(_io_marks.size(), 0);
    std::vector<ZXVertex*> internal_vertex;
    size_t number_vertex = 0;
    size_t number_edge = 0;
    // find a map for vertex index from zx graph to graph coloring
    for(const auto& vertex: _graph->get_vertices()){
        if(_io_marks[vertex->get_id()] != 1 && _io_marks[vertex->get_id()] != 2){
            // fmt::println("{}: ({},{})", vertex->get_id(), vertex->get_col(), vertex->get_row());
            number_vertex ++;
            index_map[vertex->get_id()] = number_vertex;
            // fmt::println("map {}->{}", vertex->get_id(), number_vertex);
            internal_vertex.emplace_back(vertex);
            for(const auto& neighbor: _graph->get_neighbors(vertex)){
                if(_io_marks[neighbor.first->get_id()] != 1 && _io_marks[neighbor.first->get_id()] != 2) {
                    number_edge ++;
                    // fmt::println("edge {} {}", vertex->get_id(), neighbor.first->get_id());
                }
            }
        }
    }
    number_edge = number_edge/2;
    // fmt::println("number of edges (before): {}",number_edge);

    // input neighbor
    std::vector<ZXVertex*> input_neighbor;

    // output neighbor
    std::vector<ZXVertex*> output_neighbor;

    // edges for input and output block
    for(const auto& vertex: _graph->get_vertices()){
        if(_io_marks[vertex->get_id()] == 3){
            input_neighbor.emplace_back(vertex);
        }
        else if (_io_marks[vertex->get_id()] == 4){
            output_neighbor.emplace_back(vertex);
        }
    }

    number_edge += input_neighbor.size()*output_neighbor.size();

    // fmt::println("number of internal vertices: {}",number_vertex);
    // fmt::println("number of edges: {}",number_edge);
    // fmt::println("input neighbor: {}", input_neighbor.size());
    // fmt::println("output neighbor: {}", output_neighbor.size());

    // Step 1: Write graph data to input file
    
    

    // fmt::println("start create graph file");

    
    std::ofstream graph_file("/home/enfest/popsatgcpbcp/input.col");
    if (!graph_file.is_open()) throw std::runtime_error("Failed to open input file for writing.");

    // heading of the file
    graph_file << fmt::format("p edge {} {}\n", number_vertex, number_edge);
    // fmt::println("p edge {} {}\n", number_vertex, number_edge);

    

    // edges of the graph
    for(const auto& vertex: _graph->get_vertices()){
        if(_io_marks[vertex->get_id()] != 1 && _io_marks[vertex->get_id()] != 2){
            for(const auto& neighbor: _graph->get_neighbors(vertex)){
                if(_io_marks[neighbor.first->get_id()] != 1 && _io_marks[neighbor.first->get_id()] != 2 && index_map[vertex->get_id()] < index_map[neighbor.first->get_id()]){ 
                    graph_file << fmt::format("e {} {}\n", index_map[vertex->get_id()], index_map[neighbor.first->get_id()]);
                    // fmt::println("edge {} {}", vertex->get_id(), neighbor.first->get_id());
                }
            }
        }
    }
    for(const auto& input_n: input_neighbor){
        for(const auto& output_n: output_neighbor){
            graph_file << fmt::format("e {} {}\n", index_map[input_n->get_id()], index_map[output_n->get_id()]);
            // fmt::println("edge {} {}", input_n->get_id(), output_n->get_id());
        }
    }
    graph_file.close();

    // fmt::println("finish create graph file");

    // Step 2: Run the Python solver
    int ret_code = std::system("python3 /home/enfest/popsatgcpbcp/source/main.py --instance=/home/enfest/popsatgcpbcp/input.col --model=POP-S > /home/enfest/popsatgcpbcp/output.col");
    if (ret_code != 0) {
        throw std::runtime_error("Solver execution failed.");
    }

    // Step 3: Read the output file
    std::ifstream output_file("/home/enfest/popsatgcpbcp/output.col");
    if (!output_file.is_open()) {
        throw std::runtime_error("Failed to open output file for reading.");
    }

    std::string line;
    while (std::getline(output_file, line)) {
        // fmt::println("{}\n", line);
        if (line.rfind("coloring: ", 0) == 0) { // Check if the line starts with "coloring: "
            std::unordered_map<size_t, std::vector<size_t>> color_map;
            std::string coloring_data = line.substr(10); // Extract the part after "coloring: "
            
            // Remove '{' and '}' from the string
            coloring_data.erase(std::remove(coloring_data.begin(), coloring_data.end(), '{'), coloring_data.end());
            coloring_data.erase(std::remove(coloring_data.begin(), coloring_data.end(), '}'), coloring_data.end());

            // fmt::println("coloring data: {}", coloring_data);
            std::istringstream iss(coloring_data);
            std::string token;

            while (std::getline(iss, token, ']')) {
                if (token.find(',') != std::string::npos) {
                    token.erase(std::remove(token.begin(), token.end(), ','), token.end());
                }
                size_t color_index = 0;
                std::vector<size_t> vertex_ids;
                // fmt::println("token: {}", token);
                // Parse the color index and vertex IDs
                size_t colon_pos = token.find(':');
                // fmt::println("token substream: {}", token.substr(0, colon_pos));
                if (colon_pos != std::string::npos) {
                    color_index = std::stoul(token.substr(0, colon_pos));
                    std::string vertices = token.substr(colon_pos + 1);
                    vertices.erase(std::remove(vertices.begin(), vertices.end(), '['), vertices.end());
                    vertices.erase(std::remove(vertices.begin(), vertices.end(), ']'), vertices.end());
                    std::istringstream vertex_stream(vertices);
                    std::string vertex_id;
                    // fmt::println("vertex stream: {}", vertices);
                    while (std::getline(vertex_stream, vertex_id, ' ')) {
                        if (!vertex_id.empty()) {
                            vertex_ids.push_back(std::stoul(vertex_id));
                        }
                    }
                }

                color_map[color_index] = vertex_ids;
            }

            _input_boundary = 0;
            _output_boundary = color_map.size()+1;
            _max_col = color_map.size()+1;
            // Debug print the parsed color map
            for (const auto& [color, vertices] : color_map) {
                // fmt::print("Color {}: ", color);
                size_t col = 0;
                for(const auto& vertex: vertices){
                    if(_io_marks[internal_vertex[vertex]->get_id()] == 3){
                        _input_boundary ++;
                        col = _input_boundary;
                        break;
                    }
                    else if(_io_marks[internal_vertex[vertex]->get_id()] == 4){
                        _output_boundary --;
                        col = _output_boundary;
                        break;
                    }
                }
                if(col == 0){
                    _input_boundary ++;
                    col = _input_boundary;
                }
                assert(col > 0);
                for (const auto& vertex : vertices) {
                    // fmt::print("{}, ", vertex);
                    internal_vertex[vertex]->set_col(col);
                }
                // fmt::print("\n");
            }

        }
    }
    output_file.close();

    assert(_max_col > 0 && _input_boundary < _output_boundary);

    for(const auto& output: _graph->get_outputs()){
        output->set_col(_max_col);
    }
    fmt::println("input boundary: {}", _input_boundary);
    fmt::println("output boundary: {}", _output_boundary);



    // construct the graph formula for graph coloring
}

void Arranger::io_vertex_arrange(){
    fmt::println("In IO Vertex Arrange");
    // fmt::println( "Input List: ");
    
    // arrange the sequence of the input vertex, so every vertex will be at the right column and no input edge cross one another.
    // meaning for input index: >0 the id of the inputs, -1 there are collision, -2 unassign
    std::vector<int> input_index(_graph->num_inputs(), -2);
    std::vector<ZXVertex*> collision_inputs;
    for(const auto& input: _graph->get_inputs()){
        auto neighbor = _graph->get_first_neighbor(input).first;
        auto neighbor_row = neighbor->get_row();

        // if the number of row > number of inputs
        if (neighbor_row > input_index.size()) input_index.resize(neighbor_row+1, -2);


        if(_graph->get_neighbors(neighbor).size() > 2 && input_index[neighbor_row] == -2){
            input->set_row(neighbor_row);
            input_index[neighbor_row] = input->get_id();
        }
        else if (_graph->get_neighbors(neighbor).size() == 2){
            auto descendent_row = _graph->get_first_neighbor(neighbor).first->get_row();
            if (_graph->get_first_neighbor(neighbor).first->get_id() == input->get_id()){
                descendent_row = _graph->get_second_neighbor(neighbor).first->get_row();
            }

            if(input_index[descendent_row] == -2){
                input->set_row(descendent_row);
                neighbor->set_row(descendent_row);
                input_index[descendent_row] = input->get_id();
            }
            else if(input_index[neighbor_row] == -2){
                input->set_row(neighbor_row);
            }
            else{
                collision_inputs.emplace_back(input);
            }
        }
        else{
            if(input_index[neighbor_row] == -2){
                input->set_row(neighbor_row);
                input_index[neighbor_row] = input->get_id();
            }
            else{
                collision_inputs.emplace_back(input);
            }
        }
        neighbor->set_col(input->get_col()+1);
        if(_io_marks[neighbor->get_id()] == 0){
            _io_marks[neighbor->get_id()] = 3;
        }
    }
    int count_index = 0;
    for(const auto& input: collision_inputs){
        while(input_index[count_index] != -2) count_index++;
        input->set_row(count_index);
        input_index[count_index] = input->get_id();
    }
    // for(const auto& input: _graph->get_inputs()){
    //     fmt::println("{}: ({},{})", input->get_id(), input->get_col(), input->get_row());
    //     for(const auto& neighbor: _graph->get_neighbors(input)){
    //         fmt::println("\t {}: ({}, {}) {}", neighbor.first->get_id(), neighbor.first->get_col(), neighbor.first->get_row(), _graph->get_neighbors(neighbor.first).size());
    //     }

    // }

    // fmt::println("Output List: ");
    
    std::vector<int> output_index(_graph->num_outputs(), -2);
    std::vector<ZXVertex*> collision_outputs;
    for(auto output: _graph->get_outputs()){
        auto neighbor = _graph->get_first_neighbor(output).first;
        auto neighbor_row = neighbor->get_row();

        // if the number of row > number of outputs
        if (neighbor_row > output_index.size()) output_index.resize(neighbor_row+1, -2);


        if(_graph->get_neighbors(neighbor).size() > 2 && output_index[neighbor_row] == -2){
            output->set_row(neighbor_row);
            output_index[neighbor_row] = output->get_id();
        }
        else if (_graph->get_neighbors(neighbor).size() == 2){
            auto descendent_row = _graph->get_first_neighbor(neighbor).first->get_row();
            if (_graph->get_first_neighbor(neighbor).first->get_id() == output->get_id()){
                descendent_row = _graph->get_second_neighbor(neighbor).first->get_row();
            }

            if(output_index[descendent_row] == -2){
                output->set_row(descendent_row);
                neighbor->set_row(descendent_row);
                output_index[descendent_row] = output->get_id();
            }
            else if(output_index[neighbor_row] == -2){
                output->set_row(neighbor_row);
            }
            else{
                collision_outputs.emplace_back(output);
            }
        }
        else{
            if(output_index[neighbor_row] == -2){
                output->set_row(neighbor_row);
                output_index[neighbor_row] = output->get_id();
            }
            else{
                collision_outputs.emplace_back(output);
            }
        }
        neighbor->set_col(output->get_col()-1);
        if (_io_marks[neighbor->get_id()] == 3){
            _io_marks[neighbor->get_id()] = 5;
        }
        else if (_io_marks[neighbor->get_id()] == 0){
            _io_marks[neighbor->get_id()] = 4;
        }
    }
    count_index = 0;
    for(const auto& output: collision_outputs){
        while(output_index[count_index] != -2) count_index++;
        output->set_row(count_index);
        output_index[count_index] = output->get_id();
    }
    // for(const auto& output: _graph->get_outputs()){
    //     fmt::println("{}: ({},{})", output->get_id(), output->get_col(), output->get_row());
    //     for(const auto& neighbor: _graph->get_neighbors(output)){
    //         fmt::println("\t {}: ({}, {}) {}", neighbor.first->get_id(), neighbor.first->get_col(), neighbor.first->get_row(), _graph->get_neighbors(neighbor.first).size());
    //     }

    // }


}