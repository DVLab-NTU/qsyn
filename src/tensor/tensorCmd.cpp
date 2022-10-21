/****************************************************************************
  FileName     [ tensorCmd.h ]
  PackageName  [ tensor ]
  Synopsis     [ Define tensor commands ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <vector>
#include "tensorCmd.h"
#include "qtensor.h"

using namespace std;

vector<QTensor<double>> tensors;

bool initTensorCmd(){
    if (!(
        cmdMgr->regCmd("TSMPrint". 4, new TSMPrintCmd) &&
        cmdMgr->regCmd("TSPrint". 4, new TSPrintCmd) &&
        cmdMgr->regCmd("TSMEquiv". 4, new TSMEquivalenceCmd)
    )) {
        cerr << "Registering \"tensor\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    TSMPrintCmd
//----------------------------------------------------------------------
CmdExecStatus
TSMPrintCmd::exec(const string &option)
{
   return CMD_EXEC_DONE;
}

void TSMPrintCmd::usage(ostream &os) const
{
   os << "Usage: QCCRead <(string fileName)> [-Replace]" << endl;
}

void TSMPrintCmd::help() const
{
   cout << setw(15) << left << "TSMPrint: "
        << "Print the summary for " << endl;
}

