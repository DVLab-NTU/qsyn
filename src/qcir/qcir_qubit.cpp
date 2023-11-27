/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirQubit member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_qubit.hpp"

#include <iomanip>

#include "./qcir_gate.hpp"
#include "fmt/core.h"
#include "util/util.hpp"

namespace qsyn::qcir {

/**
 * @brief Print qubit info
 *
 */
void QCirQubit::print_qubit_line(spdlog::level::level_enum lvl) const {
    QCirGate* current = _bit_first;
    size_t last_time  = 1;
    std::string line  = fmt::format("Q{:>2}  ", _id);
    while (current != nullptr) {
        DVLAB_ASSERT(last_time <= current->get_time(), "Gate time should not be smaller than last time!!");
        line += fmt::format(
            "{}-{:>2}({:>2})-",
            std::string(8 * (current->get_time() - last_time), '-'),
            current->get_type_str().substr(0, 2),
            current->get_id());

        last_time = current->get_time() + 1;
        current   = current->get_qubit(_id)._next;
    }

    spdlog::log(lvl, "{}", line);
}

}  // namespace qsyn::qcir
