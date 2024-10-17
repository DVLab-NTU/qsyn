#include "zx/zxgraph_action.hpp"

namespace qsyn::zx::simplify {

/**
 * @brief Calculate the 2Q-cost decrease of a rule. This function assumes that
 *        the rule is applied to a graph-like ZXGraph admitting a causal flow
 *        and the causal flow is preserved after the rule is applied.
 *
 * @tparam Rule the type of the rule to apply. Each rule should implement its
 *         own `calculate_2q_decrease` specialization.
 * @param rule
 * @param g
 * @return the 2Q-cost decrease. The higher the better.
 */
template <typename Rule>
requires std::is_base_of_v<ZXRule, Rule>
long calculate_2q_decrease(Rule const& rule, ZXGraph const& g);

}  // namespace qsyn::zx::simplify
