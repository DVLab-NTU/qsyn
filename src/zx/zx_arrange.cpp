#include "./zx_arrange.hpp"
#include <fmt/core.h>

#include <cstddef>
#include <memory>
#include <stack>
#include <utility>
#include <vector>
#include "qsyn/qsyn_type.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"
#include <fstream>
#include <sstream>
#include <queue>
#include <set>
#include <map>




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
    // io_vertex_arrange();

    // calculate DAG from the undirected ZXGraph
    auto dag = calculate_smallest_dag();
    fmt::println("DAG size: {}", dag.size());

    // rearrange the graph using the DAG
    layer_scheduling(dag);

    // do graph coloring so that there is no edge that have both nodes at the same col
    // internal_vertex_arrange();

    // create vertex map
    create_vertex_map();

    // split the vertex if the neighbor vertice is not at the neighbor columns
    stitching_vertex();

    // absorb hadamard edge
    hadamard_edge_absorb();

    // split vertices
    // split_vertices();

    

    
}

void Arranger::create_vertex_map(){
    fmt::println("In Create Vertex Map");
    for(auto& vertex: _graph->get_vertices()){
        if(vertex->is_boundary()) continue;
        if(vertex->get_col() >= _vertex_map.size()) _vertex_map.resize(vertex->get_col() + 1, std::vector<ZXVertex*>(_graph->num_inputs(), nullptr));
        _vertex_map[vertex->get_col()][vertex->get_row()] = vertex;
    }
}

void Arranger::layer_scheduling(std::unordered_map<ZXVertex*, std::unordered_set<ZXVertex*>> dag){
    fmt::println("In Grid-Aware Layer Scheduling with Criticality");
    auto& vertices = _graph->get_vertices();

    // Phase 1: Global Criticality Analysis
    // Topological sort for ASAP
    std::unordered_map<ZXVertex*, int> indegree;
    for (auto* v : vertices) indegree[v] = 0;
    for (const auto& [u, succs] : dag) {
        for (auto* v : succs) indegree[v]++;
    }
    std::queue<ZXVertex*> q;
    for (auto* v : vertices) if (indegree[v] == 0) q.push(v);
    std::vector<ZXVertex*> topo_order;
    while (!q.empty()) {
        ZXVertex* u = q.front(); q.pop();
        topo_order.push_back(u);
        auto it = dag.find(u);
        if (it != dag.end()) {
            for (auto* v : it->second) {
                if (--indegree[v] == 0) q.push(v);
            }
        }
    }
    // ASAP
    std::unordered_map<ZXVertex*, int> t_ASAP;
    for (auto* v : vertices) t_ASAP[v] = 0;
    for (auto* u : topo_order) {
        auto it = dag.find(u);
        if (it != dag.end()) {
            for (auto* v : it->second) {
                t_ASAP[v] = std::max(t_ASAP[v], t_ASAP[u] + 1);
            }
        }
    }
    int D_crit = 0;
    for (auto* v : vertices) D_crit = std::max(D_crit, t_ASAP[v]);
    // ALAP
    std::unordered_map<ZXVertex*, int> t_ALAP;
    for (auto* v : vertices) t_ALAP[v] = D_crit;
    // Build reverse DAG (predecessors)
    std::unordered_map<ZXVertex*, std::unordered_set<ZXVertex*>> pred;
    for (const auto& [u, succs] : dag) {
        for (auto* v : succs) pred[v].insert(u);
    }
    for (auto it = topo_order.rbegin(); it != topo_order.rend(); ++it) {
        ZXVertex* u = *it;
        for (auto* p : pred[u]) {
            t_ALAP[p] = std::min(t_ALAP[p], t_ALAP[u] - 1);
        }
    }
    std::unordered_map<ZXVertex*, int> slack;
    for (auto* v : vertices) slack[v] = t_ALAP[v] - t_ASAP[v];

    // Initial coordinates
    std::unordered_map<ZXVertex*, int> x_initial, y_initial;
    for (auto* v : vertices) {
        x_initial[v] = static_cast<int>(v->get_col());
        y_initial[v] = static_cast<int>(v->get_row());
    }
    // Final x assignment
    std::unordered_map<ZXVertex*, int> Final_x;
    std::set<std::pair<int, int>> Scheduled_Coordinates;
    std::unordered_set<ZXVertex*> scheduled;
    std::vector<size_t> Max_x_in_row(_graph->num_inputs(), 0);
    // Schedule all in-degree 0 nodes
    for (auto* v : vertices) {
        if (pred[v].empty()) {
            Final_x[v] = x_initial[v];
            Scheduled_Coordinates.insert({x_initial[v], y_initial[v]});
            scheduled.insert(v);
        }
    }
    // Phase 2: Iterative Placement with Slack-based Prioritization
    while (scheduled.size() < vertices.size()) {
        std::vector<ZXVertex*> ReadyList;
        for (auto* v : vertices) {
            if (scheduled.count(v)) continue;
            bool ready = true;
            for (auto* p : pred[v]) {
                if (!scheduled.count(p)) {
                    ready = false;
                    break;
                }
            }
            if (ready) ReadyList.push_back(v);
        }
        // Prioritize by slack (criticality), then by initial x for tie-breaking
        std::sort(ReadyList.begin(), ReadyList.end(), [&](ZXVertex* a, ZXVertex* b) {
            if (slack[a] != slack[b]) return slack[a] < slack[b];
            return x_initial[a] < x_initial[b];
        });
        for (auto* u : ReadyList) {
            // a. Earliest causal timestep
            fmt::println("u: {}, min_x: {}", u->get_id(), Max_x_in_row[u->get_row()]);
            size_t min_x = Max_x_in_row[u->get_row()];
            std::vector<bool> empty_slot(5, true);
            for (auto& [p, _] : _graph->get_neighbors(u)) {
                if(scheduled.count(p)) {
                    if(Final_x[p] < min_x) continue;
                    else if(Final_x[p]-min_x < empty_slot.size()) empty_slot[Final_x[p]-min_x] = false;
                    else{
                        empty_slot.resize(Final_x[p]-min_x+1, true);
                        empty_slot[Final_x[p]-min_x] = false;
                    }
                }
            }
            // b. Find available spatial slot
            size_t target_x = min_x;
            for(size_t i=1; i<empty_slot.size(); i++){
                if(empty_slot[i]){
                    target_x = min_x + i;
                    if(u->get_id()==114) fmt::println("114 target_x: {}", target_x);
                    break;
                }
            }
            if(target_x == min_x) target_x = min_x + empty_slot.size();
            
            int y = y_initial[u];
            while (Scheduled_Coordinates.count({target_x, y})) {
                target_x++;
            }
            // c. Schedule
            fmt::println("Spider {}: ({}, {})", u->get_id(), target_x, y);
            Final_x[u] = target_x;
            Scheduled_Coordinates.insert({target_x, y});
            scheduled.insert(u);
            Max_x_in_row[u->get_row()] = std::max(Max_x_in_row[u->get_row()], target_x);
        }
    }
    size_t max_x = 0;
    for(size_t i=0; i<Max_x_in_row.size(); i++){
        max_x = std::max(max_x, Max_x_in_row[i]);
    }
    fmt::println("max_x: {}", max_x);
    // Output the final coordinates
    fmt::println("Final scheduled coordinates (x, y) for each spider:");
    for (auto* v : vertices) {
        // fmt::println("Spider {}: ({}, {})", v->get_id(), Final_x[v], y_initial[v]);
        v->set_col(Final_x[v]);
    }
    for(auto* v: _graph->get_outputs()){
        v->set_col(max_x);
    }
}

std::unordered_map<ZXVertex*, std::unordered_set<ZXVertex*>> Arranger::calculate_smallest_dag(){
    fmt::println("In Calculate Smallest DAG");

    // Phase 1: Initialization
    auto& vertices = _graph->get_vertices();
    auto& inputs = _graph->get_inputs();
    auto& outputs = _graph->get_outputs();
    fmt::println("inputs: {}", _graph->num_inputs());
    fmt::println("outputs: {}", _graph->num_outputs());
    fmt::println("vertices: {}", _graph->num_vertices());

    // d: causal depth, g: causal successors
    std::unordered_map<ZXVertex*, size_t> d;
    std::unordered_map<ZXVertex*, std::unordered_set<ZXVertex*>> g;

    // Min-priority queue: (depth, ZXVertex*)
    auto cmp = [](const std::pair<size_t, ZXVertex*>& a, const std::pair<size_t, ZXVertex*>& b) {
        return a.first > b.first;
    };
    std::priority_queue<std::pair<size_t, ZXVertex*>, std::vector<std::pair<size_t, ZXVertex*>>, decltype(cmp)> Q(cmp);

    // Initialize all vertices
    for (auto* u : vertices) {
        d[u] = std::numeric_limits<size_t>::max();
        g[u] = {};
    }
    // For outputs
    for (auto* o : outputs) {
        d[o] = 0;
        g[o] = {o};
        Q.push({0, o});
    }
    std::unordered_set<ZXVertex*> input_set(inputs.begin(), inputs.end());

    // Phase 2: Iterative Backward Propagation
    while (!Q.empty()) {
        auto [priority, u] = Q.top();
        Q.pop();
        for (const auto& [v, edge] : _graph->get_neighbors(u)) {
            // if (input_set.count(v)) continue; // skip inputs
            size_t potential_d = d[u] + 1;
            std::unordered_set<ZXVertex*> potential_g = g[u];
            potential_g.insert(u);
            if (potential_d < d[v]) {
                d[v] = potential_d;
                g[v] = potential_g;
                Q.push({d[v], v});
            } else if (potential_d == d[v]) {
                // merge successor sets
                g[v].insert(potential_g.begin(), potential_g.end());
            }
        }
    }

    // Phase 3: Final DAG Construction
    // We'll use a map: ZXVertex* -> set<ZXVertex*> for the DAG
    std::unordered_map<ZXVertex*, std::unordered_set<ZXVertex*>> dag;
    for (auto* u : vertices) {
        for (auto* v : g[u]) {
            if (u == v) continue;
            // Only add edge if u and v are direct neighbors in the original graph
            if (_graph->is_neighbor(u, v)) {
                dag[u].insert(v);
            }
        }
    }

    // print the DAG
    fmt::println("DAG size: {}", dag.size());
    for (const auto& [u, succs] : dag) {
        for (auto* v : succs) {
            fmt::println("{} -> {}", u->get_id(), v->get_id());
        }
    }
    
    return dag;
}

void Arranger::split_vertices(){
    for(size_t y=0; y<_vertex_map[0].size(); y++){
        std::stack<size_t> null_node;
        int full_node = -1;
        for(size_t x=1; x<_vertex_map.size()-1; x++){
            if(_vertex_map[x][y] == nullptr) null_node.push(x);
            else{
                full_node = x;
                if(null_node.empty()) continue;
                auto cur_vertex = _vertex_map[full_node][y];
                EdgeType front_edge = EdgeType::simple;
                ZXVertex* front_node = NULL;
                for(auto [neighbor, edge]: _graph->get_neighbors(cur_vertex)){
                    if(neighbor->get_row() == y && neighbor->get_col() < x) {
                        front_edge = edge;
                        front_node = neighbor;
                        break;
                    }
                }
                _graph->remove_edge(std::make_pair(std::make_pair(front_node, cur_vertex), front_edge));
                while(!null_node.empty()){
                    auto cur_x = null_node.top();
                    null_node.pop();
                    auto new_vertex = _graph->add_vertex(cur_vertex->type(), Phase{0}, y, cur_x);
                    _vertex_map[cur_x][y] = new_vertex;
                    // fmt::println("add vertex (smaller output): {} ({},{})", new_vertex->get_id(), new_vertex->get_col(), new_vertex->get_row());
                    _graph->add_edge(new_vertex, cur_vertex, EdgeType::simple);
                    if(null_node.empty()) _graph->add_edge(front_node, new_vertex, front_edge);
                    cur_vertex = new_vertex;
                }
            }

        }
        if(!null_node.empty() && full_node >= 0){
            auto cur_vertex = _vertex_map[full_node][y];
            EdgeType back_edge = EdgeType::simple;
            ZXVertex* back_node = NULL;
            for(auto [neighbor, edge]: _graph->get_neighbors(cur_vertex)){
                if(neighbor->get_row() == y && neighbor->get_col() > full_node) {
                    back_edge = edge;
                    back_node = neighbor;
                    break;
                }
            }
            _graph->remove_edge(std::make_pair(std::make_pair(back_node, cur_vertex), back_edge));
            while(!null_node.empty()){
                auto cur_x = null_node.top();
                null_node.pop();
                auto new_vertex = _graph->add_vertex(cur_vertex->type(), Phase{0}, y, cur_x);
                _vertex_map[cur_x][y] = new_vertex;
                // fmt::println("add vertex (smaller output): {} ({},{})", new_vertex->get_id(), new_vertex->get_col(), new_vertex->get_row());
                if(cur_vertex->get_col() == full_node)_graph->add_edge(new_vertex, cur_vertex, back_edge);
                else _graph->add_edge(new_vertex, cur_vertex, EdgeType::simple);
                if(null_node.empty()) _graph->add_edge(back_node, new_vertex, EdgeType::simple);
                cur_vertex = new_vertex;
            }
        }
    }
}

void Arranger::hadamard_edge_absorb(){
    fmt::println("In Hadamard Edge Absortion");
    // fmt::println("vertex map size: {}", _vertex_map.size());
    // fmt::println("vertex map[0] size: {}", _vertex_map[0].size());
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

    // fmt::println("z start cost: {} x start cost: {}", cost_z_start, cost_x_start);
    // absorb with smaller cost
    size_t start_index = 2;
    // fmt::println("vertex map size: {}", _vertex_map.size());
    if(cost_z_start > cost_x_start) start_index=1;
    for(size_t i=start_index; i<_vertex_map.size(); i+=2){
        // fmt::println("i={}: ", i);
        for(size_t j=0; j<_vertex_map[0].size(); j++){
            // fmt::println("j={}, ptr={}", j, static_cast<void*>(_vertex_map[i][j]));
            if(_vertex_map[i][j] == NULL || _vertex_map[i][j]->is_boundary()) continue;
            toggle_vertex(*_graph, _vertex_map[i][j]->get_id());
        }
    }

};

void Arranger::stitching_vertex() {
    fmt::println("In Stitching Vertex");
    auto& vertices = _graph->get_vertices();
    size_t stitch_boundary = 0;
    
    // Make a static list of the original spiders
    // fmt::println("vertex map size: {}", _vertex_map.size());
    std::vector<ZXVertex*> Spider_vector;
    for(size_t i=0; i<_graph->num_inputs(); i++){
        size_t vertex_count = 0;
        size_t stitch_col = 0;
        for(size_t j=0; j<_vertex_map.size(); j++){
            if(_vertex_map[j][i] == nullptr) continue;
            if(_vertex_map[j][i]->is_boundary()) continue;
            vertex_count ++;
            if(vertex_count == 1) stitch_col = _vertex_map[j][i]->get_col();
            Spider_vector.emplace_back(_vertex_map[j][i]);
        }
        if(vertex_count > 1) stitch_boundary = std::max(stitch_boundary, stitch_col);
    }
    for(size_t i=0; i<_graph->num_outputs(); i++){
        size_t vertex_count = 0;
        size_t stitch_col = 0;
    }

    // fmt::println("Spider_vector size 1: {}", Spider_vector.size());
    std::stack<ZXVertex*> Spider_stack;
    for(int i=Spider_vector.size()-1; i>=0; i--){
        Spider_stack.push(Spider_vector[i]);
    }
    // fmt::println("Spider_vector size 2: {}", Spider_vector.size());
    while(!Spider_stack.empty()){
        auto spider_A = Spider_stack.top();
        Spider_stack.pop();
        if (!spider_A) continue;
        if(spider_A->is_boundary()) continue;
        // fmt::println("stitching spider: {}", spider_A->get_id());
        int layer_A = static_cast<int>(spider_A->get_col());
        int row_A = static_cast<int>(spider_A->get_row());
        // Group far neighbors by direction
        std::map<int, std::vector<ZXVertex*>> FarNeighborsByDirection = { {-1, {}}, {+1, {}} };
        for (const auto& [neighbor_v, _] : _graph->get_neighbors(spider_A)) {
            // if(neighbor_v->is_boundary()) continue;
            int distance = static_cast<int>(neighbor_v->get_col()) - layer_A;
            if (distance > 1) {
                FarNeighborsByDirection[+1].push_back(neighbor_v);
            } else if (distance < -1) {
                FarNeighborsByDirection[-1].push_back(neighbor_v);
            }
        }
        // For each direction, try to unfuse
        for (int direction : {-1, +1}) {
            if (FarNeighborsByDirection[direction].empty()) continue;
            if(FarNeighborsByDirection[direction].size() == 1 && FarNeighborsByDirection[direction][0]->get_row() == row_A) continue;
            int target_layer = layer_A + direction;
            int target_row = row_A;
            // Check for empty spot
            if (target_layer < 1) continue;
            if ((layer_A <= stitch_boundary && target_layer > stitch_boundary) || (layer_A > stitch_boundary && target_layer <= stitch_boundary)) continue;
            if (target_layer >= static_cast<int>(_vertex_map.size())) {
                _vertex_map.resize(target_layer + 1, std::vector<ZXVertex*>(_graph->num_inputs(), nullptr));
            }
            if (target_row >= static_cast<int>(_vertex_map[target_layer].size())) {
                _vertex_map[target_layer].resize(target_row + 1, nullptr);
            }
            if (_vertex_map[target_layer][target_row]) continue; // Occupied
            // Decide which edges to move
            std::vector<ZXVertex*> Edges_To_Move;
            for (const auto& [neighbor_v, _] : _graph->get_neighbors(spider_A)) {
                int original_distance = std::abs(static_cast<int>(neighbor_v->get_col()) - layer_A);
                int new_distance = std::abs(static_cast<int>(neighbor_v->get_col()) - target_layer);
                if (new_distance < original_distance && new_distance > 0) {
                    Edges_To_Move.push_back(neighbor_v);
                }
            }
            if (Edges_To_Move.empty()) continue;
            // Perform the unfusion
            ZXVertex* spider_B = _graph->add_vertex(spider_A->type(), Phase{0}, target_row, target_layer);
            _vertex_map[target_layer][target_row] = spider_B;
            _graph->add_edge(spider_A, spider_B, EdgeType::simple);
            for (auto* neighbor_v : Edges_To_Move) {
                auto edge_type = _graph->get_edge_type(spider_A, neighbor_v).value_or(EdgeType::simple);
                _graph->remove_edge(spider_A, neighbor_v);
                _graph->add_edge(spider_B, neighbor_v, edge_type);
            }
            Spider_stack.push(spider_B);
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