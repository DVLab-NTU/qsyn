/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ read input in truth table format and convert to XAG via abc ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./input.hpp"

#include <base/abc/abc.h>
#include <base/main/mainInt.h>
#include <fmt/format.h>

#include <string>

// TODO: move abc related global variables and functions to a separate file
Abc_Frame_t* s_GlobalFrame = nullptr;

extern "C" {

int Abc_NtkResubstitute(Abc_Ntk_t* pNtk,
                        int nCutsMax,
                        int nNodesMax,
                        int nMinSaved,
                        int nLevelsOdc,
                        int fUpdateLevel,
                        int fVerbose,
                        int fVeryVerbose);
}

namespace qsyn::qcir {

Abc_Ntk_t* truth_table_to_ntk(std::istream& input, bool hex) {
    std::string input_string(std::istreambuf_iterator<char>(input), {});
    char* input_cstring = input_string.data();

    Vec_Ptr_t* vSops{};
    if (hex) {
        vSops = Abc_SopFromTruthsHex(input_cstring);
    } else {
        vSops = Abc_SopFromTruthsBin(input_cstring);
    }

    // Abc_NtkCreateWithNodes will fail if s_GlobalFrame is not set
    s_GlobalFrame   = Abc_FrameGetGlobalFrame();
    Abc_Ntk_t* pNtk = Abc_NtkCreateWithNodes(vSops);
    Vec_PtrFreeFree(vSops);

    Abc_Ntk_t* ntk = Abc_NtkStrash(pNtk, 0, 0, 0);
    Abc_NtkDelete(pNtk);
    ntk = abc_resyn(ntk, true);
    return ntk;
}

Abc_Ntk_t* abc_resyn(Abc_Ntk_t* pNtk, bool consider_xor) {
    if (pNtk == nullptr || !Abc_NtkIsStrash(pNtk)) {
        return nullptr;
    }
    // alias resyn       "b; rw; rwz; b; rwz; b"
    Abc_Ntk_t* ntk = pNtk;
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_rewrite(ntk);
    ntk            = abc_rewrite(ntk, true);
    ntk            = abc_balance(ntk, consider_xor);
    ntk            = abc_rewrite(ntk, true);
    ntk            = abc_balance(ntk, consider_xor);
    return ntk;
}

Abc_Ntk_t* abc_resyn2(Abc_Ntk_t* pNtk, bool consider_xor) {
    if (pNtk == nullptr || !Abc_NtkIsStrash(pNtk)) {
        return nullptr;
    }
    // alias resyn2      "b; rw; rf; b; rw; rwz; b; rfz; rwz; b"
    Abc_Ntk_t* ntk = pNtk;
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

Abc_Ntk_t* abc_resyn2a(Abc_Ntk_t* pNtk, bool consider_xor) {
    if (pNtk == nullptr || !Abc_NtkIsStrash(pNtk)) {
        return nullptr;
    }
    // alias resyn2a     "b; rw; b; rw; rwz; b; rwz; b"
    Abc_Ntk_t* ntk = pNtk;
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

Abc_Ntk_t* abc_resyn3(Abc_Ntk_t* pNtk, bool consider_xor) {
    if (pNtk == nullptr || !Abc_NtkIsStrash(pNtk)) {
        return nullptr;
    }
    // alias resyn3      "b; rs; rs -K 6; b; rsz; rsz -K 6; b; rsz -K 5; b"
    Abc_Ntk_t* ntk = pNtk;
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

Abc_Ntk_t* abc_rewrite(Abc_Ntk_t* pNtk, bool use_zeros) {
    if (pNtk == nullptr || !Abc_NtkIsStrash(pNtk) || Abc_NtkGetChoiceNum(pNtk)) {
        return nullptr;
    }
    Abc_Ntk_t* pDup  = Abc_NtkDup(pNtk);
    int fUpdateLevel = 1;
    int fUseZeros    = use_zeros ? 1 : 0;
    int fVerbose     = 0;
    int fVeryVerbose = 0;
    int fPlaceEnable = 0;
    if (Abc_NtkRewrite(
            pNtk,
            fUpdateLevel,
            fUseZeros,
            fVerbose,
            fVeryVerbose,
            fPlaceEnable) == -1) {
        Abc_NtkDelete(pNtk);
        return pDup;
    } else {
        Abc_NtkDelete(pDup);
        return pNtk;
    }
}

Abc_Ntk_t* abc_refactor(Abc_Ntk_t* pNtk, bool use_zeros) {
    if (pNtk == nullptr || !Abc_NtkIsStrash(pNtk) || Abc_NtkGetChoiceNum(pNtk)) {
        return nullptr;
    }
    Abc_Ntk_t* pDup  = Abc_NtkDup(pNtk);
    int nNodeSizeMax = 10;
    int nMinSaved    = 1;
    int nConeSizeMax = 16;
    int fUpdateLevel = 1;
    int fUseZeros    = use_zeros ? 1 : 0;
    int fUseDcs      = 0;
    int fVerbose     = 0;
    if (Abc_NtkRefactor(
            pNtk,
            nNodeSizeMax,
            nMinSaved,
            nConeSizeMax,
            fUpdateLevel,
            fUseZeros,
            fUseDcs,
            fVerbose) == -1) {
        Abc_NtkDelete(pNtk);
        return pDup;
    } else {
        Abc_NtkDelete(pDup);
        return pNtk;
    }
}

Abc_Ntk_t* abc_balance(Abc_Ntk_t* pNtk, bool consider_xor) {
    if (pNtk == nullptr || !Abc_NtkIsStrash(pNtk)) {
        return nullptr;
    }
    int fDuplicate     = 0;
    int fSelective     = 0;
    int fUpdateLevel   = 1;
    int fVerbose       = 0;
    Abc_Ntk_t* pNtkRes = nullptr;
    if (consider_xor) {
        pNtkRes = Abc_NtkBalanceExor(pNtk, fUpdateLevel, fVerbose);
    } else {
        pNtkRes = Abc_NtkBalance(pNtk, fDuplicate, fSelective, fUpdateLevel);
    }
    Abc_NtkDelete(pNtk);
    return pNtkRes;
}

Abc_Ntk_t* abc_resub(Abc_Ntk_t* pNtk, bool use_zeros, int n_cuts_max) {
    if (pNtk == nullptr || !Abc_NtkIsStrash(pNtk) || Abc_NtkGetChoiceNum(pNtk)) {
        return nullptr;
    }
    int nCutsMax     = n_cuts_max;
    int nNodesMax    = 1;
    int nLevelsOdc   = 0;
    int nMinSaved    = use_zeros ? 0 : 1;
    int fUpdateLevel = 1;
    int fVerbose     = 0;
    int fVeryVerbose = 0;
    if (!Abc_NtkResubstitute(pNtk, nCutsMax, nNodesMax, nMinSaved, nLevelsOdc, fUpdateLevel, fVerbose, fVeryVerbose)) {
        Abc_NtkDelete(pNtk);
        return nullptr;
    }
    return pNtk;
}

}  // namespace qsyn::qcir
