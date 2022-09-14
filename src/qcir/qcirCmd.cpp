/****************************************************************************
  FileName     [ qcirCmd.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define basic qcir package commands ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <iostream>
#include <iomanip>
#include "qcirMgr.h"
#include "qcirGate.h"
#include "qcirCmd.h"
#include "util.h"
#include "phase.h"

using namespace std;

extern QCirMgr *qCirMgr;
extern int effLimit;

bool initQCirCmd()
{
   if (!(cmdMgr->regCmd("QCCRead", 4, new QCirReadCmd) &&
         cmdMgr->regCmd("QCCPrint", 4, new QCirPrintCmd) &&
         cmdMgr->regCmd("QCGAdd", 4, new QCirAddGateCmd) &&
         cmdMgr->regCmd("QCBAdd", 4, new QCirAddQubitCmd) &&
         cmdMgr->regCmd("QCGDelete", 4, new QCirDeleteGateCmd) &&
         cmdMgr->regCmd("QCBDelete", 4, new QCirDeleteQubitCmd) &&
         cmdMgr->regCmd("QCGPrint", 4, new QCirGatePrintCmd) &&
         cmdMgr->regCmd("QCZXMapping", 5, new QCirZXMappingCmd)
         && cmdMgr->regCmd("QCT", 3, new QCirTestCmd)
         ))
   {
      cerr << "Registering \"qcir\" commands fails... exiting" << endl;
      return false;
   }
   return true;
}

enum QCirCmdState
{
   // Order matters! Do not change the order!!
   QCIRINIT,
   QCIRREAD,
   // dummy end
   QCIRCMDTOT
};

static QCirCmdState curCmd = QCIRINIT;

CmdExecStatus
QCirTestCmd::exec(const string &option)
{
   qCirMgr->mapping();
   return CMD_EXEC_DONE;
}
void QCirTestCmd::usage(ostream &os) const
{
   os << "Usage: QCT" << endl;
}

void QCirTestCmd::help() const
{
   cout << setw(15) << left << "QCT: "
        << "Test what function you want (for developement)" << endl;
}

//----------------------------------------------------------------------
//    QCCRead <(string fileName)> [-Replace]
//----------------------------------------------------------------------
CmdExecStatus
QCirReadCmd::exec(const string &option)
{
   // check option
   vector<string> options;
   if (!CmdExec::lexOptions(option, options))
      return CMD_EXEC_ERROR;
   if (options.empty())
      return CmdExec::errorOption(CMD_OPT_MISSING, "");

   bool doReplace = false;
   string fileName;
   for (size_t i = 0, n = options.size(); i < n; ++i)
   {
      if (myStrNCmp("-Replace", options[i], 2) == 0)
      {
         if (doReplace)
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
         doReplace = true;
      }
      else
      {
         if (fileName.size())
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         fileName = options[i];
      }
   }

   if (qCirMgr != 0)
   {
      if (doReplace)
      {
         cerr << "Note: original quantum circuit is replaced..." << endl;
         curCmd = QCIRINIT;
         delete qCirMgr;
         qCirMgr = 0;
      }
      else
      {
         cerr << "Error: circuit already exists!!" << endl;
         return CMD_EXEC_ERROR;
      }
   }
   qCirMgr = new QCirMgr;

   if (!qCirMgr->parse(fileName))
   {
      curCmd = QCIRINIT;
      delete qCirMgr;
      qCirMgr = 0;
      return CMD_EXEC_ERROR;
   }

   curCmd = QCIRREAD;

   return CMD_EXEC_DONE;
}

void QCirReadCmd::usage(ostream &os) const
{
   os << "Usage: QCCRead <(string fileName)> [-Replace]" << endl;
}

void QCirReadCmd::help() const
{
   cout << setw(15) << left << "QCCRead: "
        << "read in a circuit and construct the netlist" << endl;
}

//----------------------------------------------------------------------
//    QCGPrint <(size_t gateID)> [-Time | -ZXform]
//----------------------------------------------------------------------
CmdExecStatus
QCirGatePrintCmd::exec(const string &option)
{
   // check option
   if (!qCirMgr)
   {
      cerr << "Error: quantum circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   vector<string> options;
   if (!CmdExec::lexOptions(option, options))
      return CMD_EXEC_ERROR;
   if (options.empty())
      return CmdExec::errorOption(CMD_OPT_MISSING, "");

   bool hasOption = false;
   bool showTime = false;
   bool ZXtrans = false;
   string strID = "";
   for (size_t i = 0, n = options.size(); i < n; ++i)
   {
      if (myStrNCmp("-Time", options[i], 2) == 0)
      {
         if (hasOption)
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
         showTime = true;
         hasOption = true;
      }
      else if(myStrNCmp("-ZXform", options[i], 3) == 0)
      {
         if (hasOption)
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
         ZXtrans = true;
         hasOption = true;
      }
      else
      {
         if (strID.size())
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         strID = options[i];
      }
   }
   if (strID.size() == 0)
      return CmdExec::errorOption(CMD_OPT_MISSING, "");
   unsigned id;
   if(!myStr2Uns(strID,id)){
      cerr << "Error: target ID should be a positive integer!!" << endl;
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, strID);
   }
   if (ZXtrans){
      if(qCirMgr->getGate(id)==NULL){
         cerr << "Error: id " << id << " not found!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, strID);
      }
      size_t tmp = 4;
      qCirMgr->getGate(id)->getZXform(tmp)->printVertices();
   }
   else{
      if (!qCirMgr->printGateInfo(id, showTime))
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, strID);
   }
   
   return CMD_EXEC_DONE;
}

void QCirGatePrintCmd::usage(ostream &os) const
{
   os << "Usage: QCGPrint <(size_t gateID)> [-Time | -ZXform]" << endl;
}

void QCirGatePrintCmd::help() const
{
   cout << setw(15) << left << "QCGPrint: "
        << "print quantum gate information\n";
}

//----------------------------------------------------------------------
//    QCCPrint [-List | -Qubit | -ZXform]
//----------------------------------------------------------------------
CmdExecStatus
QCirPrintCmd::exec(const string &option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;

   if (!qCirMgr)
   {
      cerr << "Error: quantum circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   if (token.empty() || myStrNCmp("-List", token, 2) == 0)
      qCirMgr->printSummary();
   else if (myStrNCmp("-Qubit", token, 2) == 0)
      qCirMgr->printQubits();
   else if (myStrNCmp("-ZXform", token, 2) == 0)
      qCirMgr->printZXTopoOrder();
   else
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);

   return CMD_EXEC_DONE;
}

void QCirPrintCmd::usage(ostream &os) const
{
   os << "Usage: QCCPrint [-List | -Qubit | -ZXform]" << endl;
}

void QCirPrintCmd::help() const
{
   cout << setw(15) << left << "QCCPrint: "
        << "print quanutm circuit\n";
}

//---------------------------------------------------------------------------------------------------
//     QCGAdd <-H | -X | -Z | -T | -TDG | -S | -V> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-CX> <(size_t ctrl)> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-RZ> <-PHase (Phase phase_inp)> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-CRZ> <-PHase (Phase phase_inp)> <(size_t ctrl)> <(size_t targ)> [-APpend|-PRepend]
//---------------------------------------------------------------------------------------------------
CmdExecStatus
QCirAddGateCmd::exec(const string &option)
{
   if (!qCirMgr)
   {
      cerr << "Error: no available qubits. Please read a quantum circuit or add qubit(s)!!" << endl;
      return CMD_EXEC_ERROR;
   }
   // check option
   vector<string> options;
   if (!CmdExec::lexOptions(option, options))
      return CMD_EXEC_ERROR;
   if (options.empty())
      return CmdExec::errorOption(CMD_OPT_MISSING, "");

   bool flag = false;
   bool appendGate = true;
   size_t eraseIndex = 0;
   for (size_t i = 0, n = options.size(); i < n; ++i)
   {
      if (myStrNCmp("-APpend", options[i], 3) == 0)
      {
         if (flag)
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
         flag = true;
         eraseIndex = i;
      }
      else if (myStrNCmp("-PRepend", options[i], 3) == 0)
      {
         if (flag)
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
         appendGate = false;
         flag = true;
         eraseIndex = i;
      }
   }
   string flagStr = options[eraseIndex];
   if (flag)
      options.erase(options.begin() + eraseIndex);
   if (options.empty())
      return CmdExec::errorOption(CMD_OPT_MISSING, flagStr);
   string type = options[0];
   vector<size_t> qubits;
   // <-H | -X | -Z | -TG | -TDg | -S | -V>
   if (myStrNCmp("-H", type, 2) == 0|| myStrNCmp("-X", type, 2) == 0 || myStrNCmp("-Z", type, 2) == 0 || myStrNCmp("-T", type, 2) == 0 ||
   myStrNCmp("-TDG", type, 4) == 0 || myStrNCmp("-S", type, 2) == 0 || myStrNCmp("-V", type, 2) == 0)
   {
      if (options.size() == 1)
         return CmdExec::errorOption(CMD_OPT_MISSING, type);
      if (options.size() > 2)
         return CmdExec::errorOption(CMD_OPT_EXTRA, options[2]);
      unsigned id;
      if(!myStr2Uns(options[1],id)){
         cerr << "Error: target ID should be a positive integer!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[1]);
      }
      if (qCirMgr->getQubit(id) == NULL)
      {
         cerr << "Error: qubit ID is not in current circuit!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[1]);
      }
      qubits.push_back(id);
      type = type.erase(0,1);
      qCirMgr->addGate(type, qubits, Phase(0),appendGate);
   }
   else if (myStrNCmp("-CX", type, 3) == 0){
      if (options.size() < 3)
         return CmdExec::errorOption(CMD_OPT_MISSING, options[options.size()-1]);
      if (options.size() > 3)
         return CmdExec::errorOption(CMD_OPT_EXTRA, options[3]);
      for(size_t i=1; i<options.size(); i++){
         unsigned id;
         if(!myStr2Uns(options[i],id)){
            cerr << "Error: target ID should be a positive integer!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         }
         if (qCirMgr->getQubit(id) == NULL)
         {
            cerr << "Error: qubit ID is not in current circuit!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         }
         qubits.push_back(id);
      }
      type = type.erase(0,1);
      qCirMgr->addGate(type, qubits, Phase(0),appendGate);
   }
   else if (myStrNCmp("-RZ", type, 3) == 0){
      Phase phase;
      if(options.size()==1){
         cerr << "Error: missing -PHase flag!!" << endl;
         return CmdExec::errorOption(CMD_OPT_MISSING, options[0]);
      }
      else{
         if(myStrNCmp("-PHase", options[1], 3) != 0){
            cerr << "Error: missing -PHase flag before (" << options[1] <<")!!" << endl;
            return CmdExec::errorOption(CMD_OPT_MISSING, options[0]);
         }
         else{
            if(options.size()==2){
               cerr << "Error: missing phase after -PHase flag!!" << endl;
               return CmdExec::errorOption(CMD_OPT_MISSING, options[1]);
            }
            else{
               // Check Phase Legal
               if(!phase.fromString(options[2])){
                  cerr << "Error: not a legal phase!!" << endl;
                  return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[2]);
               }
            }
         }
      }
      if (options.size() < 4)
         return CmdExec::errorOption(CMD_OPT_MISSING, options[2]);
      if (options.size() > 4)
         return CmdExec::errorOption(CMD_OPT_EXTRA, options[4]);
      unsigned id;
      if(!myStr2Uns(options[3],id)){
         cerr << "Error: target ID should be a positive integer!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[3]);
      }
      if (qCirMgr->getQubit(id) == NULL)
      {
         cerr << "Error: qubit ID is not in current circuit!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[3]);
      }
      qubits.push_back(id);
      type = type.erase(0,1);
      qCirMgr->addGate(type, qubits, phase, appendGate);
   }
   else{
      cerr << "Error: type is not implemented!!" << endl;
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, type);
   }
     
   return CMD_EXEC_DONE;
}

void QCirAddGateCmd::usage(ostream &os) const
{
   os << "QCGAdd <-H | -X | -Z | -T | -TDG | -S | -V> <(size_t targ)> [-APpend|-PRepend]" << endl;
   os << "QCGAdd <-CX> <(size_t ctrl)> <(size_t targ)> [-APpend|-PRepend]" << endl;
   os << "QCGAdd <-RZ> <-PHase (Phase phase_inp)> <(size_t targ)> [-APpend|-PRepend]" << endl;
}

void QCirAddGateCmd::help() const
{
   cout << setw(15) << left << "QCGAdd: "
        << "add quantum gate\n";
}

//----------------------------------------------------------------------
//    QCBAdd [size_t addNum]
//----------------------------------------------------------------------
CmdExecStatus
QCirAddQubitCmd::exec(const string &option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;
   if (token.empty())
   {
      if (qCirMgr == 0)
         qCirMgr = new QCirMgr;
      qCirMgr->addQubit(1);
   }
   else
   {
      unsigned id;
      if(!myStr2Uns(token,id)){
         cerr << "Error: target ID should be a positive integer!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
      }   
      else
      {
         if (qCirMgr == 0)
            qCirMgr = new QCirMgr;
         qCirMgr->addQubit(id);
      }
   }
   return CMD_EXEC_DONE;
}

void QCirAddQubitCmd::usage(ostream &os) const
{
   os << "Usage: QCBAdd [size_t addNum] " << endl;
}

void QCirAddQubitCmd::help() const
{
   cout << setw(15) << left << "QCBAdd: "
        << "add qubit bit(s)\n";
}

//----------------------------------------------------------------------
//    QCGDelete <(size_t gateID)>
//----------------------------------------------------------------------
CmdExecStatus
QCirDeleteGateCmd::exec(const string &option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;
   if (!qCirMgr)
   {
      cerr << "Error: quantum circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   if (token.empty())
      return CmdExec::errorOption(CMD_OPT_MISSING, "");
   unsigned id;
   if(!myStr2Uns(token,id)){
      cerr << "Error: target ID should be a positive integer!!" << endl;
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   }   
   if (!qCirMgr->removeGate(id))
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   else
      return CMD_EXEC_DONE;
}

void QCirDeleteGateCmd::usage(ostream &os) const
{
   os << "Usage: QCGDelete <(size_t gateID)> " << endl;
}

void QCirDeleteGateCmd::help() const
{
   cout << setw(15) << left << "QCGDelete: "
        << "delete a gate\n";
}

//----------------------------------------------------------------------
//    QCBDelete <(size_t qubitID)>
//----------------------------------------------------------------------
CmdExecStatus
QCirDeleteQubitCmd::exec(const string &option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;
   if (!qCirMgr)
   {
      cerr << "Error: quantum circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   if (token.empty())
      return CmdExec::errorOption(CMD_OPT_MISSING, "");
   unsigned id;
   if(!myStr2Uns(token,id)){
      cerr << "Error: target ID should be a positive integer!!" << endl;
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   }   
   if (!qCirMgr->removeQubit(id))
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   else
      return CMD_EXEC_DONE;
}

void QCirDeleteQubitCmd::usage(ostream &os) const
{
   os << "Usage: QCBDelete <(size_t qubitID)> " << endl;
}

void QCirDeleteQubitCmd::help() const
{
   cout << setw(15) << left << "QCBDelete: "
        << "delete an empty qubit\n";
}

//----------------------------------------------------------------------
//    QCZXMapping
//----------------------------------------------------------------------
CmdExecStatus
QCirZXMappingCmd::exec(const string &option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;

   if (!qCirMgr)
   {
      cerr << "Error: quantum circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   if (token.empty())
      qCirMgr -> mapping();
   else if (myStrNCmp("-Log", token, 2) == 0)
      qCirMgr -> mapping(false);
   else
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);

   return CMD_EXEC_DONE;
}

void QCirZXMappingCmd::usage(ostream &os) const
{
   os << "Usage: QCZXMapping [-Log]" << endl;
}

void QCirZXMappingCmd::help() const
{
   cout << setw(15) << left << "QCZXMapping: "
        << "mapping to ZX diagram\n";
}

// //----------------------------------------------------------------------
// //    CIRGate <<(int gateId)> [<-FANIn | -FANOut><(int level)>]>
// //----------------------------------------------------------------------
// CmdExecStatus
// CirGateCmd::exec(const string& option)
// {
//    if (!cirMgr) {
//       cerr << "Error: circuit has not been read!!" << endl;
//       return CMD_EXEC_ERROR;
//    }

//    // check option
//    vector<string> options;
//    if (!CmdExec::lexOptions(option, options))
//       return CMD_EXEC_ERROR;

//    if (options.empty())
//       return CmdExec::errorOption(CMD_OPT_MISSING, "");

//    int gateId = -1, level = 0;
//    bool doFanin = false, doFanout = false;
//    QCirGate* thisGate = 0;
//    for (size_t i = 0, n = options.size(); i < n; ++i) {
//       bool checkLevel = false;
//       if (myStrNCmp("-FANIn", options[i], 5) == 0) {
//          if (doFanin || doFanout)
//             return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//          doFanin = true;
//          checkLevel = true;
//       }
//       else if (myStrNCmp("-FANOut", options[i], 5) == 0) {
//          if (doFanin || doFanout)
//             return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//          doFanout = true;
//          checkLevel = true;
//       }
//       else if (!thisGate) {
//          if (!myStr2Int(options[i], gateId) || gateId < 0)
//             return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//          thisGate = cirMgr->getGate(gateId);
//          if (!thisGate) {
//             cerr << "Error: Gate(" << gateId << ") not found!!" << endl;
//             return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
//          }
//       }
//       else if (thisGate)
//          return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
//       else
//          return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//       if (checkLevel) {
//          if (++i == n)
//             return CmdExec::errorOption(CMD_OPT_MISSING, options[i-1]);
//          if (!myStr2Int(options[i], level) || level < 0)
//             return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//          checkLevel = true;
//       }
//    }

//    if (!thisGate) {
//       cerr << "Error: Gate id is not specified!!" << endl;
//       return CmdExec::errorOption(CMD_OPT_MISSING, options.back());
//    }

//    if (doFanin)
//       thisGate->reportFanin(level);
//    else if (doFanout)
//       thisGate->reportFanout(level);
//    else
//       thisGate->reportGate();

//    return CMD_EXEC_DONE;
// }

// void
// CirGateCmd::usage(ostream& os) const
// {
//    os << "Usage: CIRGate <<(int gateId)> [<-FANIn | -FANOut><(int level)>]>"
//       << endl;
// }

// void
// CirGateCmd::help() const
// {
//    cout << setw(15) << left << "CIRGate: " << "report a gate\n";
// }

// //----------------------------------------------------------------------
// //    CIRSWeep
// //----------------------------------------------------------------------
// CmdExecStatus
// CirSweepCmd::exec(const string& option)
// {
//    if (!cirMgr) {
//       cerr << "Error: circuit is not yet constructed!!" << endl;
//       return CMD_EXEC_ERROR;
//    }
//    // check option
//    vector<string> options;
//    CmdExec::lexOptions(option, options);

//    if (!options.empty())
//       return CmdExec::errorOption(CMD_OPT_EXTRA, options[0]);

//    assert(curCmd != CIRINIT);
//    cirMgr->sweep();

//    return CMD_EXEC_DONE;
// }

// void
// CirSweepCmd::usage(ostream& os) const
// {
//    os << "Usage: CIRSWeep" << endl;
// }

// void
// CirSweepCmd::help() const
// {
//    cout << setw(15) << left << "CIRSWeep: "
//         << "remove unused gates\n";
// }

// //----------------------------------------------------------------------
// //    CIROPTimize
// //----------------------------------------------------------------------
// CmdExecStatus
// CirOptCmd::exec(const string& option)
// {
//    if (!cirMgr) {
//       cerr << "Error: circuit is not yet constructed!!" << endl;
//       return CMD_EXEC_ERROR;
//    }
//    // check option
//    vector<string> options;
//    CmdExec::lexOptions(option, options);

//    if (!options.empty())
//       return CmdExec::errorOption(CMD_OPT_EXTRA, options[0]);

//    assert(curCmd != CIRINIT);
//    if (curCmd == CIRSIMULATE) {
//       cerr << "Error: circuit has been simulated!! Do \"CIRFraig\" first!!"
//            << endl;
//       return CMD_EXEC_ERROR;
//    }
//    cirMgr->optimize();
//    curCmd = CIROPT;

//    return CMD_EXEC_DONE;
// }

// void
// CirOptCmd::usage(ostream& os) const
// {
//    os << "Usage: CIROPTimize" << endl;
// }

// void
// CirOptCmd::help() const
// {
//    cout << setw(15) << left << "CIROPTimize: "
//         << "perform trivial optimizations\n";
// }

// //----------------------------------------------------------------------
// //    CIRSTRash
// //----------------------------------------------------------------------
// CmdExecStatus
// CirStrashCmd::exec(const string& option)
// {
//    if (!cirMgr) {
//       cerr << "Error: circuit is not yet constructed!!" << endl;
//       return CMD_EXEC_ERROR;
//    }
//    // check option
//    vector<string> options;
//    CmdExec::lexOptions(option, options);

//    if (!options.empty())
//       return CmdExec::errorOption(CMD_OPT_EXTRA, options[0]);

//    assert(curCmd != CIRINIT);
//    if (curCmd == CIRSTRASH) {
//       cerr << "Error: circuit has been strashed!!" << endl;
//       return CMD_EXEC_ERROR;
//    }
//    else if (curCmd == CIRSIMULATE) {
//       cerr << "Error: circuit has been simulated!! Do \"CIRFraig\" first!!"
//            << endl;
//       return CMD_EXEC_ERROR;
//    }
//    cirMgr->strash();
//    curCmd = CIRSTRASH;

//    return CMD_EXEC_DONE;
// }

// void
// CirStrashCmd::usage(ostream& os) const
// {
//    os << "Usage: CIRSTRash" << endl;
// }

// void
// CirStrashCmd::help() const
// {
//    cout << setw(15) << left << "CIRSTRash: "
//         << "perform structural hash on the circuit netlist\n";
// }

// //----------------------------------------------------------------------
// //    CIRSIMulate <-Random | -File <string patternFile>>
// //                [-Output (string logFile)]
// //----------------------------------------------------------------------
// CmdExecStatus
// CirSimCmd::exec(const string& option)
// {
//    if (!cirMgr) {
//       cerr << "Error: circuit is not yet constructed!!" << endl;
//       return CMD_EXEC_ERROR;
//    }
//    // check option
//    vector<string> options;
//    CmdExec::lexOptions(option, options);

//    ifstream patternFile;
//    ofstream logFile;
//    bool doRandom = false, doFile = false, doLog = false;
//    for (size_t i = 0, n = options.size(); i < n; ++i) {
//       if (myStrNCmp("-Random", options[i], 2) == 0) {
//          if (doRandom || doFile)
//             return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//          doRandom = true;
//       }
//       else if (myStrNCmp("-File", options[i], 2) == 0) {
//          if (doRandom || doFile)
//             return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//          if (++i == n)
//             return CmdExec::errorOption(CMD_OPT_MISSING, options[i-1]);
//          patternFile.open(options[i].c_str(), ios::in);
//          if (!patternFile)
//             return CmdExec::errorOption(CMD_OPT_FOPEN_FAIL, options[i]);
//          doFile = true;
//       }
//       else if (myStrNCmp("-Output", options[i], 2) == 0) {
//          if (doLog)
//             return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//          if (++i == n)
//             return CmdExec::errorOption(CMD_OPT_MISSING, options[i-1]);
//          logFile.open(options[i].c_str(), ios::out);
//          if (!logFile)
//             return CmdExec::errorOption(CMD_OPT_FOPEN_FAIL, options[i]);
//          doLog = true;
//       }
//       else
//          return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//    }

//    if (!doRandom && !doFile)
//       return CmdExec::errorOption(CMD_OPT_MISSING, "");

//    assert (curCmd != CIRINIT);
//    if (doLog)
//       cirMgr->setSimLog(&logFile);
//    else cirMgr->setSimLog(0);

//    if (doRandom)
//       cirMgr->randomSim();
//    else
//       cirMgr->fileSim(patternFile);
//    cirMgr->setSimLog(0);
//    curCmd = CIRSIMULATE;

//    return CMD_EXEC_DONE;
// }

// void
// CirSimCmd::usage(ostream& os) const
// {
//    os << "Usage: CIRSIMulate <-Random | -File <string patternFile>>\n"
//       << "                   [-Output (string logFile)]" << endl;
// }

// void
// CirSimCmd::help() const
// {
//    cout << setw(15) << left << "CIRSIMulate: "
//         << "perform Boolean logic simulation on the circuit\n";
// }

// //----------------------------------------------------------------------
// //    CIRFraig
// //----------------------------------------------------------------------
// CmdExecStatus
// CirFraigCmd::exec(const string& option)
// {
//    if (!cirMgr) {
//       cerr << "Error: circuit is not yet constructed!!" << endl;
//       return CMD_EXEC_ERROR;
//    }
//    // check option
//    vector<string> options;
//    CmdExec::lexOptions(option, options);

//    if (!options.empty())
//       return CmdExec::errorOption(CMD_OPT_EXTRA, options[0]);

//    if (curCmd != CIRSIMULATE) {
//       cerr << "Error: circuit is not yet simulated!!" << endl;
//       return CMD_EXEC_ERROR;
//    }
//    cirMgr->fraig();
//    curCmd = CIRFRAIG;

//    return CMD_EXEC_DONE;
// }

// void
// CirFraigCmd::usage(ostream& os) const
// {
//    os << "Usage: CIRFraig" << endl;
// }

// void
// CirFraigCmd::help() const
// {
//    cout << setw(15) << left << "CIRFraig: "
//         << "perform Boolean logic simulation on the circuit\n";
// }

// //----------------------------------------------------------------------
// //    CIRWrite [(int gateId)][-Output (string aagFile)]
// //----------------------------------------------------------------------
// CmdExecStatus
// CirWriteCmd::exec(const string& option)
// {
//    if (!cirMgr) {
//       cerr << "Error: circuit is not yet constructed!!" << endl;
//       return CMD_EXEC_ERROR;
//    }
//    // check option
//    vector<string> options;
//    CmdExec::lexOptions(option, options);

//    if (options.empty()) {
//       cirMgr->writeAag(cout);
//       return CMD_EXEC_DONE;
//    }
//    bool hasFile = false;
//    int gateId;
//    CirGate *thisGate = NULL;
//    ofstream outfile;
//    for (size_t i = 0, n = options.size(); i < n; ++i) {
//       if (myStrNCmp("-Output", options[i], 2) == 0) {
//          if (hasFile)
//             return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
//          if (++i == n)
//             return CmdExec::errorOption(CMD_OPT_MISSING, options[i-1]);
//          outfile.open(options[i].c_str(), ios::out);
//          if (!outfile)
//             return CmdExec::errorOption(CMD_OPT_FOPEN_FAIL, options[1]);
//          hasFile = true;
//       }
//       else if (myStr2Int(options[i], gateId) && gateId >= 0) {
//          if (thisGate != NULL)
//             return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
//          thisGate = cirMgr->getGate(gateId);
//          if (!thisGate) {
//             cerr << "Error: Gate(" << gateId << ") not found!!" << endl;
//             return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//          }
//          if (!thisGate->isAig()) {
//              cerr << "Error: Gate(" << gateId << ") is NOT an AIG!!" << endl;
//             return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//          }
//       }
//       else return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
//    }

//    if (!thisGate) {
//       assert (hasFile);
//       cirMgr->writeAag(outfile);
//    }
//    else if (hasFile) cirMgr->writeGate(outfile, thisGate);
//    else cirMgr->writeGate(cout, thisGate);

//    return CMD_EXEC_DONE;
// }

// void
// CirWriteCmd::usage(ostream& os) const
// {
//    os << "Usage: CIRWrite [(int gateId)][-Output (string aagFile)]" << endl;
// }

// void
// CirWriteCmd::help() const
// {
//    cout << setw(15) << left << "CIRWrite: "
//         << "write the netlist to an ASCII AIG file (.aag)\n";
// }
