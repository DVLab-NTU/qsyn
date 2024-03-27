/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ read input in truth table format and convert to XAG via abc ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./input.hpp"

#include <base/abc/abc.h>
#include <base/io/ioAbc.h>
#include <base/main/mainInt.h>
#include <fmt/format.h>

#include <string>

// TODO: move abc related global variables and functions to a separate file
Abc_Frame_t* s_GlobalFrame = nullptr;  // NOLINT(readability-identifier-naming)  // ABC API naming

extern "C" {
// NOLINTBEGIN(readability-identifier-naming)  // ABC API naming
int Abc_NtkResubstitute(Abc_Ntk_t* pNtk,
                        int nCutsMax,
                        int nNodesMax,
                        int nMinSaved,
                        int nLevelsOdc,
                        int fUpdateLevel,
                        int fVerbose,
                        int fVeryVerbose);
// NOLINTEND(readability-identifier-naming)
}

namespace qsyn::qcir {

Abc_Ntk_t* truth_table_to_ntk(std::istream& input, bool hex) {
    std::string input_string(std::istreambuf_iterator<char>(input), {});
    char* input_cstring = input_string.data();

    Vec_Ptr_t* v_sops{};
    if (hex) {
        v_sops = Abc_SopFromTruthsHex(input_cstring);
    } else {
        v_sops = Abc_SopFromTruthsBin(input_cstring);
    }

    // Abc_NtkCreateWithNodes will fail if s_GlobalFrame is not set
    s_GlobalFrame  = Abc_FrameGetGlobalFrame();
    Abc_Ntk_t* ntk = Abc_NtkCreateWithNodes(v_sops);
    Vec_PtrFreeFree(v_sops);

    int const f_all_nodes   = 0;
    int const f_cleanup     = 0;
    int const f_record      = 0;
    Abc_Ntk_t* strashed_ntk = Abc_NtkStrash(ntk, f_all_nodes, f_cleanup, f_record);

    Abc_NtkDelete(ntk);
    return strashed_ntk;
}

Abc_Ntk_t* read_to_ntk(std::string file_name) {
    // bool f_check   = true;
    // bool f_bar_buf = false;
    auto file_type = Io_ReadFileType(file_name.data());
    // Io_Read will fail if s_GlobalFrame is not set
    s_GlobalFrame  = Abc_FrameGetGlobalFrame();
    Abc_Ntk_t* ntk = Io_Read(file_name.data(), file_type, 1, 0);

    int const f_all_nodes   = 0;
    int const f_cleanup     = 0;
    int const f_record      = 0;
    Abc_Ntk_t* strashed_ntk = Abc_NtkStrash(ntk, f_all_nodes, f_cleanup, f_record);

    Abc_NtkDelete(ntk);
    return strashed_ntk;
}

Abc_Ntk_t* abc_resyn(Abc_Ntk_t* p_ntk, bool consider_xor) {
    if (p_ntk == nullptr || !Abc_NtkIsStrash(p_ntk)) {
        return nullptr;
    }
    // alias resyn       "b; rw; rwz; b; rwz; b"
    Abc_Ntk_t* ntk = p_ntk;
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_rewrite(ntk);
    ntk            = abc_rewrite(ntk, true);
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_rewrite(ntk, true);
    ntk            = abc_balance(ntk, consider_xor);
    return ntk;
}

Abc_Ntk_t* abc_resyn2(Abc_Ntk_t* p_ntk, bool consider_xor) {
    if (p_ntk == nullptr || !Abc_NtkIsStrash(p_ntk)) {
        return nullptr;
    }
    // alias resyn2      "b; rw; rf; b; rw; rwz; b; rfz; rwz; b"
    Abc_Ntk_t* ntk = p_ntk;
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_rewrite(ntk);
    ntk            = abc_refactor(ntk);
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_rewrite(ntk);
    ntk            = abc_rewrite(ntk, true);
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_refactor(ntk, true);
    ntk            = abc_rewrite(ntk, true);
    ntk            = abc_balance(ntk, consider_xor);
    return ntk;
}

Abc_Ntk_t* abc_resyn2a(Abc_Ntk_t* p_ntk, bool consider_xor) {
    if (p_ntk == nullptr || !Abc_NtkIsStrash(p_ntk)) {
        return nullptr;
    }
    // alias resyn2a     "b; rw; b; rw; rwz; b; rwz; b"
    Abc_Ntk_t* ntk = p_ntk;
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_rewrite(ntk);
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_rewrite(ntk);
    ntk            = abc_rewrite(ntk, true);
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_rewrite(ntk, true);
    ntk            = abc_balance(ntk, consider_xor);
    return ntk;
}

Abc_Ntk_t* abc_resyn3(Abc_Ntk_t* p_ntk, bool consider_xor) {
    if (p_ntk == nullptr || !Abc_NtkIsStrash(p_ntk)) {
        return nullptr;
    }
    // alias resyn3      "b; rs; rs -K 6; b; rsz; rsz -K 6; b; rsz -K 5; b"
    Abc_Ntk_t* ntk = p_ntk;
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_resub(ntk);
    ntk            = abc_resub(ntk, false, 6);
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_resub(ntk, true);
    ntk            = abc_resub(ntk, true, 6);
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_resub(ntk, true, 5);
    ntk            = abc_balance(ntk, consider_xor);
    return ntk;
}

Abc_Ntk_t* abc_rewrite(Abc_Ntk_t* p_ntk, bool use_zeros) {
    if (p_ntk == nullptr || !Abc_NtkIsStrash(p_ntk) || Abc_NtkGetChoiceNum(p_ntk)) {
        return nullptr;
    }
    Abc_Ntk_t* p_dup         = Abc_NtkDup(p_ntk);
    int const f_update_level = 1;
    int const f_use_zeros    = use_zeros ? 1 : 0;
    int const f_verbose      = 0;
    int const f_very_verbose = 0;
    int const f_place_enable = 0;
    if (Abc_NtkRewrite(
            p_ntk,
            f_update_level,
            f_use_zeros,
            f_verbose,
            f_very_verbose,
            f_place_enable) == -1) {
        Abc_NtkDelete(p_ntk);
        return p_dup;
    } else {
        Abc_NtkDelete(p_dup);
        return p_ntk;
    }
}

Abc_Ntk_t* abc_refactor(Abc_Ntk_t* p_ntk, bool use_zeros) {
    if (p_ntk == nullptr || !Abc_NtkIsStrash(p_ntk) || Abc_NtkGetChoiceNum(p_ntk)) {
        return nullptr;
    }
    Abc_Ntk_t* p_dup          = Abc_NtkDup(p_ntk);
    int const n_node_size_max = 10;
    int const n_min_saved     = 1;
    int const n_cone_size_max = 16;
    int const f_update_level  = 1;
    int const f_use_zeros     = use_zeros ? 1 : 0;
    int const f_use_dcs       = 0;
    int const f_verbose       = 0;
    if (Abc_NtkRefactor(
            p_ntk,
            n_node_size_max,
            n_min_saved,
            n_cone_size_max,
            f_update_level,
            f_use_zeros,
            f_use_dcs,
            f_verbose) == -1) {
        Abc_NtkDelete(p_ntk);
        return p_dup;
    } else {
        Abc_NtkDelete(p_dup);
        return p_ntk;
    }
}

Abc_Ntk_t* abc_balance(Abc_Ntk_t* p_ntk, bool consider_xor) {
    if (p_ntk == nullptr || !Abc_NtkIsStrash(p_ntk)) {
        return nullptr;
    }
    int const f_duplicate    = 0;
    int const f_selective    = 0;
    int const f_update_level = 1;
    int const f_verbose      = 0;
    Abc_Ntk_t* p_ntk_res     = nullptr;
    if (consider_xor) {
        p_ntk_res = Abc_NtkBalanceExor(p_ntk, f_update_level, f_verbose);
    } else {
        p_ntk_res = Abc_NtkBalance(p_ntk, f_duplicate, f_selective, f_update_level);
    }
    Abc_NtkDelete(p_ntk);
    return p_ntk_res;
}

Abc_Ntk_t* abc_resub(Abc_Ntk_t* p_ntk, bool use_zeros, int n_cuts_max) {
    if (p_ntk == nullptr || !Abc_NtkIsStrash(p_ntk) || Abc_NtkGetChoiceNum(p_ntk)) {
        return nullptr;
    }
    int const n_nodes_max    = 1;
    int const n_levels_odc   = 0;
    int const n_min_saved    = use_zeros ? 0 : 1;
    int const f_update_level = 1;
    int const f_verbose      = 0;
    int const f_very_verbose = 0;
    if (!Abc_NtkResubstitute(p_ntk, n_cuts_max, n_nodes_max, n_min_saved, n_levels_odc, f_update_level, f_verbose, f_very_verbose)) {
        Abc_NtkDelete(p_ntk);
        return nullptr;
    }
    return p_ntk;
}

}  // namespace qsyn::qcir
