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

    // arrange the io vertex
    io_vertex_arrange();

    // calculate DAG from the undirected ZXGraph
    auto dag = calculate_smallest_dag();
    fmt::println("DAG size: {}", dag.size());

    // rearrange the graph using the DAG
    layer_scheduling(dag);

    // create vertex map
    create_vertex_map();

    // split the vertex if the neighbor vertice is not at the neighbor columns
    stitching_vertex();

    // absorb hadamard edge
    hadamard_edge_absorb();

    // for(auto* v: _graph->get_vertices()){
    //     if(v->is_boundary()) continue;
    //     fmt::println("v: {}, phase: {}", v->get_id(), Phase(0));
    //     v->set_phase(Phase(0));
    // }
    
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

    for (auto* v : vertices) {
        // fmt::println("Spider {}: ({}, {})", v->get_id(), Final_x[v], y_initial[v]);
        v->set_col(Final_x[v]);
    }

    std::vector<size_t> visit_num(std::max(_graph->num_inputs(), _graph->num_outputs()), 0);
    std::vector<ZXVertex*> exist_internal_input(std::max(_graph->num_inputs(), _graph->num_outputs()), nullptr);
    std::vector<ZXVertex*> reorder_internal_output;
    for(auto* v: vertices){
        if(v->is_boundary()) continue;
        visit_num[v->get_row()] ++;
        if(exist_internal_input[v->get_row()] == nullptr) exist_internal_input[v->get_row()] = v;
        else{
            if(v->get_col() < exist_internal_input[v->get_row()]->get_col()){
                reorder_internal_output.push_back(exist_internal_input[v->get_row()]);
                exist_internal_input[v->get_row()] = v;
            }
            else{
                reorder_internal_output.push_back(v);
            }
        }
    }
    size_t internal_boundary = 0;
    for(auto v: exist_internal_input){
        if(v == nullptr) continue;
        if(visit_num[v->get_row()] >1) internal_boundary = std::max(internal_boundary, (size_t)v->get_col());
    }

    for(size_t i=0; i<Max_x_in_row.size(); i++){
        Max_x_in_row[i] = std::max(Max_x_in_row[i], internal_boundary);
    }
    
    for(auto u: reorder_internal_output){
        if(u->get_col() > internal_boundary) continue;
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
        u->set_col(target_x);

    }


    size_t max_x = 0;
    for(auto v: vertices){
        if(v->is_boundary()) continue;
        max_x = std::max(max_x, (size_t)v->get_col());
    }
    fmt::println("max_x: {}", max_x);
    // Output the final coordinates
    fmt::println("Final scheduled coordinates (x, y) for each spider:");
    
    for(auto* v: _graph->get_outputs()){
        v->set_col(max_x+1);
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
            fmt::println("toggle vertex: {}, phase: {}", _vertex_map[i][j]->get_id(), _vertex_map[i][j]->phase());
            toggle_vertex(*_graph, _vertex_map[i][j]->get_id());
        }
    }
    // for(auto v: _graph->get_vertices()){
    //     if(v->is_boundary()) continue;
    //     if((v->phase()*2) == Phase(0) && _graph->get_neighbors(v).size() == 2){
    //         bool is_hadamard = true;
    //         for(const auto& [neighbor, edge]: _graph->get_neighbors(v)){
    //             if(edge == EdgeType::simple) is_hadamard = false;
    //             break;
    //         }
    //         if(is_hadamard) toggle_vertex(*_graph, v->get_id());
    //     }
    // }

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

void Arranger::io_vertex_arrange(){
    fmt::println("In IO Vertex Arrange");
    // fmt::println( "Input List: ");
    for(auto input: _graph->get_inputs()){
        input->set_col(0);
        for(const auto& [neighbor, edge]: _graph->get_neighbors(input)){
            if(neighbor->is_boundary()) continue;
            if(input->get_row() != neighbor->get_row()){
                neighbor->set_row(input->get_row());
            }
            neighbor->set_col(input->get_col()+1);
        }
    }

    for(auto output: _graph->get_outputs()){
        for(const auto& [neighbor, edge]: _graph->get_neighbors(output)){
            if(neighbor->is_boundary()) continue;
            if(output->get_row() != neighbor->get_row()){
                neighbor->set_row(output->get_row());
            }
            neighbor->set_col(output->get_col()-1);
        }
    }

    std::vector<std::tuple<ZXVertex*, ZXVertex*, EdgeType>> nodes_to_split;

    for(const auto& input: _graph->get_inputs()){
        for(const auto& [neighbor, edge]: _graph->get_neighbors(input)){
            if(neighbor->is_boundary()) continue;
            if(input->get_row() != neighbor->get_row()){
                nodes_to_split.emplace_back(input, neighbor, edge);
            }
        }
    }

    for(const auto& [input, neighbor, edge]: nodes_to_split){
        // fmt::println("input: {} neighbor: {}", input->get_id(), neighbor->get_id());
        ZXVertex* new_vertex = _graph->add_vertex(neighbor->type(), Phase{0}, input->get_row(), input->get_col()+1);
        _graph->add_edge(input, new_vertex, EdgeType::simple);
        _graph->add_edge(new_vertex, neighbor, edge);
        _graph->remove_edge(input, neighbor);
    }
}

