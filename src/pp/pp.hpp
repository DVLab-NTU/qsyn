/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Define class Phase_Polynomial structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "util/boolean_matrix.hpp"
#include "util/phase.hpp"

namespace dvlab {

class Phase;

}

namespace qsyn::qcir {

class QCir;

}

namespace qsyn::pp {

class Phase_Polynomial {
    // using Monomial   = std::pair<dvlab::BooleanMatrix, dvlab::Phase>;
    // using Polynomial_terms = std::unordered_map<dvlab::BooleanMatrix::Row, dvlab::Phase>;

public:
    Phase_Polynomial(){};

    bool calculate_pp(qcir::QCir const& qcir);
    bool insert_phase(size_t, dvlab::Phase);
    void reset();
    void intial_wire(size_t);
    void remove_coeff_0_monomial();
    void extend_h_map();


    // Resynthesis
    void gaussian_resynthesis(std::vector<dvlab::BooleanMatrix> p, dvlab::BooleanMatrix initial_wires, dvlab::BooleanMatrix terminal_wires); // todo: replace data type to Partitions
    void add_H_gate(size_t);
    std::vector<Phase> get_phase_of_terms(dvlab::BooleanMatrix p);

    

    // get function
    dvlab::BooleanMatrix get_pp_terms() const { return _pp_terms; };
    dvlab::BooleanMatrix get_wires() const { return _wires; };
    std::vector<dvlab::Phase> get_pp_coeff() const { return _pp_coeff; };
    std::vector<std::pair<dvlab::BooleanMatrix, dvlab::BooleanMatrix>> get_h_map() const { return _h_map; }
    size_t get_data_qubit_num() const { return _qubit_number; };
    qcir::QCir get_result() const {return _result;}

    // print function
    void print_polynomial(spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;
    void print_wires(spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;

private:
    size_t _qubit_number;
    // size_t _ancillae;
    dvlab::BooleanMatrix _pp_terms;
    std::vector<dvlab::Phase> _pp_coeff;
    dvlab::BooleanMatrix _wires;
    std::vector<qcir::QCirGate*> _hadamard;
    // Wires before H and qubit of H
    std::vector<std::pair<dvlab::BooleanMatrix, dvlab::BooleanMatrix>> _h_map;
    std::vector<size_t> _h;
    qcir::QCir _result;
};

}  // namespace qsyn::pp
