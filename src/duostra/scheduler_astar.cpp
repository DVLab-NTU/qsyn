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
#include <memory>
#include <queue>
#include <tl/enumerate.hpp>
#include <vector>

#include "./scheduler.hpp"
#include "duostra/router.hpp"

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
                    std::unique_ptr<Router> est_router,
                    std::unique_ptr<Router> router,
                    std::unique_ptr<BaseScheduler> scheduler,
                    StarNode* parent)
    : _type(type),
      _gate_id(gate_id),
      _parent(parent),
      _est_router(std::move(est_router)),
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
    children.reserve(avail_gates.size());
    for (auto gate_id : avail_gates)
        children.emplace_back(1, gate_id, est_router().clone(), router().clone(), scheduler().clone(), self_pointer);
}


/**
 * @brief Route the exact gate given gate_id and estimate the cost
 *
 */

size_t StarNode::_route_and_estimate() {
        // Clone the actual scheduler and router to avoid modifying the original state
        auto cloned_router = _router->clone();
        auto cloned_scheduler = _scheduler->clone();

        // Route the current gate on the cloned router and scheduler
        size_t cost_so_far = cloned_scheduler->route_one_gate(*cloned_router, _gate_id, false);
        cloned_scheduler->circuit_topology().update_available_gates(_gate_id);

        // Estimate the remaining cost using a GreedyScheduler and APSP router (est_router)
        auto temp_router = _est_router->clone();
        auto temp_topo = cloned_scheduler->circuit_topology().clone();
        GreedyScheduler temp_scheduler(*temp_topo, false);
        temp_scheduler.set_conf(GreedyConf()); // Use default Greedy configuration

        // Copy the current state to temp_scheduler
        temp_scheduler._operations = cloned_scheduler->get_operations();
        temp_scheduler._sorted = cloned_scheduler->_sorted;

        // Now simulate routing the remaining gates
        while (!temp_scheduler.get_available_gates().empty()) {
            if (stop_requested()) {
                break; // Continue to sum what is processed so far
            }
            auto waitlist = temp_scheduler.get_available_gates();
            assert(!waitlist.empty());

            auto gate_idx_opt = temp_scheduler.get_executable_gate(*temp_router);
            size_t gate_idx;
            if (gate_idx_opt.has_value()) {
                gate_idx = gate_idx_opt.value();
            } else {
                gate_idx = temp_scheduler.greedy_fallback(*temp_router, waitlist);
            }
            assert(gate_idx < temp_scheduler.circuit_topology().get_num_gates());

            // Route gate with forget=false to track operations
            temp_scheduler.route_one_gate(*temp_router, gate_idx, false);  // forget=false
            temp_scheduler.circuit_topology().update_available_gates(gate_idx);
        }

        // Sort the operations to satisfy the assertion in get_total_time()
        temp_scheduler._sort();

        // Calculate total_time as the sum of all operation durations
        size_t estimated_total_time = temp_scheduler.get_total_time();

        _cost = estimated_total_time;
        // The total estimated cost is estimated_total_time
        return estimated_total_time;
    }

    // Granting access to the comparison struct
    



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
BaseScheduler::Device AStarScheduler::assign_gates_and_sort(std::unique_ptr<Router> router, std::unique_ptr<Router> est_router) {
    Device d = assign_gates(std::move(router), std::move(est_router));
    _sort();
    return d;
}

// BaseScheduler::Device AStarScheduler::_assign_gates(std::unique_ptr<Router> router, std::unique_ptr<Router> est_router, std::unique_ptr<BaseScheduler> est_scheduler) {
//     for (dvlab::TqdmWrapper bar{_circuit_topology.get_num_gates()}; !bar.done(); ++bar) {
//         if (stop_requested()) {
//             return router->get_device();
//         }
//         route_one_gate(*router, bar.idx());
//     }
//     return router->get_device();
// }

AStarScheduler::Device AStarScheduler::assign_gates(std::unique_ptr<Router> router, std::unique_ptr<Router> est_router) {
    // get the total gates count in the circuit
    auto total_gates = _circuit_topology.get_num_gates();
    
    // construct the root node
    auto root = std::make_unique<StarNode>(0, 0, est_router->clone(), router->clone(), clone(), nullptr);
    
    // construct a list to store the best cost of each node
    // -1 means the node is not visited yet
    std::vector<size_t> best_cost_list(total_gates + 1, 0);

    // get the available gates
    auto available_gates = root->scheduler().get_available_gates();

    // construct a candidate list to store the current unvisited nodes sorted by their best cost
    std::priority_queue<StarNode*, std::vector<StarNode*>, cmp> candidate_list;

    // flag to indicate whether there is a feasible solution
    bool done = false;
    size_t final_cost = 0;
    StarNode* finish_node = nullptr;
    // build the first layer of the tree
    root->grow(root.get());
    for(auto& child : root->children) {
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
        node->grow(root.get());
        for(auto& child : node->children) {
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
                child.delete_self();
            }
        }



        return router->get_device();
    }

    std::vector<size_t> order;
    while(finish_node->get_parent() != nullptr){
        order.push_back(finish_node->get_gate_id());
        finish_node = finish_node->get_parent();
    }
    std::reverse(order.begin(), order.end());
    for(auto gate_id : order){
        route_one_gate(*router, gate_id);
    }
    return router->get_device();
}
}  // namespace qsyn::duostra

