/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define basic QCir functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir.hpp"
#include "./qcir_gate.hpp"
#include "./qcir_qubit.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

namespace qsyn::qcir {

using GateInfo    = std::tuple<std::string, QubitIdList, dvlab::Phase>;
using Equivalence = dvlab::utils::ordered_hashmap<std::string, std::vector<GateInfo>>;

dvlab::utils::ordered_hashmap<std::string, Equivalence> equivalence_library = {
    {"sherbrooke", {{"h", {
                              {"s", {0}, dvlab::Phase(0)},
                              {"sx", {0}, dvlab::Phase(0)},
                              {"s", {0}, dvlab::Phase(0)},
                          }},
                    {"cx", {
                               {"sdg", {0}, dvlab::Phase(0)},
                               {"z", {1}, dvlab::Phase(0)},
                               {"sx", {1}, dvlab::Phase(0)},
                               {"z", {1}, dvlab::Phase(0)},
                               {"ecr", {0, 1}, dvlab::Phase(0)},
                               {"x", {0}, dvlab::Phase(0)},
                           }},
                    {"cz", {
                               {"sdg", {0}, dvlab::Phase(0)},
                               {"sx", {1}, dvlab::Phase(0)},
                               {"s", {1}, dvlab::Phase(0)},
                               {"ecr", {0, 1}, dvlab::Phase(0)},
                               {"x", {0}, dvlab::Phase(0)},
                               {"s", {1}, dvlab::Phase(0)},
                               {"sx", {1}, dvlab::Phase(0)},
                               {"s", {1}, dvlab::Phase(0)},
                           }}}},
    {"kyiv", {{"h", {
                        {"s", {0}, dvlab::Phase(0)},
                        {"sx", {0}, dvlab::Phase(0)},
                        {"s", {0}, dvlab::Phase(0)},
                    }},
              {"cz", {
                         {"s", {1}, dvlab::Phase(0)},
                         {"sx", {1}, dvlab::Phase(0)},
                         {"s", {1}, dvlab::Phase(0)},
                         {"cx", {0, 1}, dvlab::Phase(0)},
                         {"s", {1}, dvlab::Phase(0)},
                         {"sx", {1}, dvlab::Phase(0)},
                         {"s", {1}, dvlab::Phase(0)},
                     }}}},
    {"prague", {{"h", {
                          {"s", {0}, dvlab::Phase(0)},
                          {"sx", {0}, dvlab::Phase(0)},
                          {"s", {0}, dvlab::Phase(0)},
                      }},
                {"cx", {
                           {"s", {1}, dvlab::Phase(0)},
                           {"sx", {1}, dvlab::Phase(0)},
                           {"z", {1}, dvlab::Phase(0)},
                           {"cz", {0, 1}, dvlab::Phase(0)},
                           {"sx", {1}, dvlab::Phase(0)},
                           {"s", {1}, dvlab::Phase(0)},
                       }}}},
};

}  // namespace qsyn::qcir
