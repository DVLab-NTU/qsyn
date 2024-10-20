/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Search Scheduler member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <omp.h>

#include <algorithm>
#include <cstddef>
#include <queue>
#include <tl/enumerate.hpp>
#include <vector>

#include "./scheduler.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::duostra {

// SECTION - Class StarNode Member Functions

/**
 * @brief Construct a new Tree Node:: Tree Node object
 *
 * @param gateId
 * @param router
 * @param scheduler
 */
StarNode::StarNode( size_t type,
                    size_t gate_id,
                    std::unique_ptr<Router> router,
                    std::unique_ptr<BaseScheduler> scheduler,
                    StarNode* parent)
    : _type(type),
      _gate_id(gate_id),
      _parent(parent),
      _router(std::move(router)),
      _scheduler(std::move(scheduler)){
        if(_type == 1) _route_and_estimate(); // do the exact routing for the given gate_id and estimate the rest
      }

/**
 * @brief Grow by adding available gates to children.
 *
 */
void StarNode::grow(StarNode* self_pointer) {
    auto const& avail_gates = scheduler().get_available_gates();
    assert(children.empty());
    for (auto gate_id : avail_gates)
        children.emplace_back(1, gate_id, router().clone(), scheduler().clone(), self_pointer);
}


/**
 * @brief Route the exact gate given gate_id and estimate the cost
 *
 */

void StarNode::_route_and_estimate(){

    // route the exact gate given gate_id
    cost = _scheduler->route_one_gate(*_router, gate_id, false);

    // estimate the cost
}


// SECTION - Class Search Scheduler Member Functions

/**
 * @brief Construct a new Search Scheduler:: Search Scheduler object
 *
 * @param topo
 * @param tqdm
 */
AStarScheduler::AStarScheduler(CircuitTopology const& topo, bool tqdm)
    : GreedyScheduler(topo, tqdm),
      _never_cache(DuostraConfig::NEVER_CACHE),
      _execute_single(DuostraConfig::EXECUTE_SINGLE_QUBIT_GATES_ASAP),
      _lookahead(DuostraConfig::SEARCH_DEPTH) {
    _cache_when_necessary();
}

/**
 * @brief Clone scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
std::unique_ptr<BaseScheduler> AStarScheduler::clone() const {
    return std::make_unique<AStarScheduler>(*this);
}

/**
 * @brief Cache only when necessary
 *
 */
void AStarScheduler::_cache_when_necessary() {
    if (!_never_cache && _lookahead == 1) {
        spdlog::error("When _lookAhead = 1, '_neverCache' is used by default.");
        _never_cache = true;
    }
}
/**
 * @brief Priority Queue Cmp for AStar Candidate
 *
 * @param router
 * @return Device
 */

// Proirity Queue Cmp
struct cmp {
    bool operator()(StarNode* a, StarNode* b) {
        return a->get_estimated_cost() > b->get_estimated_cost();
    }
};
 
/**
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
AStarScheduler::Device AStarScheduler::_assign_gates(std::unique_ptr<Router> router) {
    // get the total gates count in the circuit
    auto total_gates = _circuit_topology.get_num_gates();
    
    // construct the root node
    auto root = std::make_unique<StarNode>(0, 0, router->clone(), clone(), NULL);
    
    // construct a list to store the best cost of each node
    // -1 means the node is not visited yet
    std::vector<size_t> best_cost_list(total_gates + 1, 0);

    // get the available gates
    auto available_gates = root->scheduler().get_available_gates();

    // construct a candidate list to store the current unvisited nodes sorted by their best cost
    std::priority_queue<StarNode, std::vector<StarNode>, cmp> candidate_list;

    // flag to indicate whether there is a feasible solution
    bool done = false;
    size_t final_cost = 0;
    StarNode* finish_node = nullptr;
    // build the first layer of the tree
    root->grow(&root);
    for(auto child : root.child) {
        auto cost = child.get_estimated_cost();
        auto id = child.get_gate_id();

        if(best_cost_list[id] == -1 || cost < best_cost_list[id]){
            if(child.is_leaf()) {
                done = true;
                if(final_cost == 0 || cost < final_cost) {
                    final_cost = cost;
                    finish_node = &child;
                }
            }
            best_cost_list[id] = cost;
            candidate_list.push(&child);
        }
    }
    while(!done){
        // if the candidate list is empty, there is no feasible solution
        if(candidate_list.empty()) {
            spdlog::error("No feasible solution found");
            return router->get_device();
        }
        // get the node with the smallest cost
        auto node = candidate_list.top();
        candidate_list.pop();
        // grow the node
        node->grow();
        for(auto child : node->children) {
            auto cost = child.get_estimated_cost();
            auto id = child.get_gate_id();
            if(best_cost_list[id] == 0 || cost < best_cost_list[id]){
                if(child.is_leaf()) {
                    done = true;
                    if(final_cost == 0 || cost < final_cost) {
                        final_cost = cost;
                        finish_node = &child;
                    }
                }
                best_cost_list[id] = cost;
                candidate_list.push(&child);
            }
            else{
                child->delete_self();
            }
        }

        return router->get_device();
    }




    // auto root = make_unique<StarNode>(
    //     StarNodeConf{_never_cache, _execute_single, _conf.num_candidates},
    //     std::vector<size_t>{}, router->clone(), clone(), 0);

    // For each step. (all nodes + 1 dummy)
    // dvlab::TqdmWrapper bar{total_gates + 1, _tqdm};
    // assert(!root->done());
    // while (!root->done()) {
    //     // Update the _candidates.
    //     if (stop_requested()) {
    //         return router->get_device();
    //     }
    //     auto selected_node = std::make_unique<StarNode>(root->best_child(static_cast<int>(_lookahead)));
    //     root               = std::move(selected_node);

    //     for (auto const& gate_id : root->get_executed_gates()) {
    //         route_one_gate(*router, gate_id);
    //         ++bar;
    //     }
    // }
    return router->get_device();
}

}  // namespace qsyn::duostra
