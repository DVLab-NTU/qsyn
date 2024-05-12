/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define QCir translation functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_translate.hpp"

#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir_gate.hpp"

namespace qsyn::qcir {

dvlab::utils::ordered_hashmap<std::string, Equivalence> EQUIVALENCE_LIBRARY = {
    {"sherbrooke", {{HGate(), {
                                  {SGate(), {0}},
                                  {SXGate(), {0}},
                                  {SGate(), {0}},
                              }},
                    {CXGate(), {
                                   {SdgGate(), {0}},
                                   {ZGate(), {1}},
                                   {SXGate(), {1}},
                                   {ZGate(), {1}},
                                   {ECRGate(), {0, 1}},
                                   {XGate(), {0}},
                               }},
                    {CZGate(), {
                                   {SdgGate(), {0}},
                                   {SXGate(), {1}},
                                   {SGate(), {1}},
                                   {ECRGate(), {0, 1}},
                                   {XGate(), {0}},
                                   {SGate(), {1}},
                                   {SXGate(), {1}},
                                   {SGate(), {1}},
                               }}}},
    {"kyiv", {{HGate(), {
                            {SGate(), {0}},
                            {SXGate(), {0}},
                            {SGate(), {0}},
                        }},
              {CZGate(), {
                             {SGate(), {1}},
                             {SXGate(), {1}},
                             {SGate(), {1}},
                             {CXGate(), {0, 1}},
                             {SGate(), {1}},
                             {SXGate(), {1}},
                             {SGate(), {1}},
                         }},
              {ECRGate(), {
                              {SGate(), {0}},
                              {SXGate(), {1}},
                              {CXGate(), {0, 1}},
                              {XGate(), {0}},
                          }}}},
    {"prague", {{HGate(), {
                              {SGate(), {0}},
                              {SXGate(), {0}},
                              {SGate(), {0}},
                          }},
                {CXGate(), {
                               {SGate(), {1}},
                               {SXGate(), {1}},
                               {ZGate(), {1}},
                               {CZGate(), {0, 1}},
                               {SXGate(), {1}},
                               {SGate(), {1}},
                           }},
                {ECRGate(), {
                                {SGate(), {0}},
                                {SdgGate(), {1}},
                                {SXdgGate(), {1}},
                                {CZGate(), {0, 1}},
                                {XGate(), {0}},
                                {SGate(), {1}},
                                {SXGate(), {1}},
                                {SGate(), {1}},
                            }}}},
};

std::optional<QCir>
translate(QCir const& qcir, std::string const& gate_set) {
    QCir result{qcir.get_num_qubits()};
    Equivalence const& equivalence = EQUIVALENCE_LIBRARY[gate_set];
    for (auto const* cur_gate : qcir.get_gates()) {
        auto const op = cur_gate->get_operation();

        if (!equivalence.contains(op)) {
            result.append(*cur_gate);
            continue;
        }

        for (auto const& gate : equivalence.at(op)) {
            QubitIdList gate_qubit_id_list;
            for (auto qubit_num : gate.get_qubits()) {
                gate_qubit_id_list.emplace_back(cur_gate->get_qubit(qubit_num));
            }
            result.append(gate.get_operation(), gate_qubit_id_list);
        }
    }
    result.set_gate_set(gate_set);

    return result;
}

}  // namespace qsyn::qcir
