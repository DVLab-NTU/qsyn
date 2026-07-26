/****************************************************************************
  PackageName  [ ncf ]
  Synopsis     [ NcfProgram manager ]
****************************************************************************/

#pragma once

#include "cmd/qcir_mgr.hpp"
#include "cmd/tableau_mgr.hpp"
#include "ncf/ncf_program.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::experimental {

using NcfMgr = dvlab::utils::DataStructureManager<NcfProgram>;

bool add_ncf_command(dvlab::CommandLineInterface& cli, NcfMgr& ncf_mgr, TableauMgr& tableau_mgr, qcir::QCirMgr& qcir_mgr);

void register_ncf_from_tableau(NcfMgr& ncf_mgr, TableauMgr const& tableau_mgr, Tableau const* pre_ncf);

}  // namespace qsyn::experimental

template <>
inline std::string dvlab::utils::data_structure_info_string(qsyn::experimental::NcfProgram const& p) {
    return fmt::format("{:<19} {} blocks / {} terms",
                       p.filename().substr(0, 19),
                       p.n_blocks(),
                       p.fusion_report().n_pauli_terms);
}

template <>
inline std::string dvlab::utils::data_structure_name(qsyn::experimental::NcfProgram const& p) {
    return p.filename();
}
