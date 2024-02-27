/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./decomposer.hpp"

#include <cassert>
#include <memory>
#include <ranges>
#include <tuple>

#include "qcir/qcir.hpp"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include "tensor/qtensor.hpp"

using namespace qsyn::tensor;
using namespace qsyn::qcir;

namespace qsyn::decomposer {

/**
 * @brief Construct a new Decomposer :: Decomposer object
 *
 */
template <typename U>

Decomposer::Decomposer(int reg){
    qreg = reg;
    _quantum_circuit = new QCir(reg);
};

void Decomposer::decompose(){
  fmt::println("in decompose function");
}


}

