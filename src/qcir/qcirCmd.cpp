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
#include "qcir.h"
#include "qcirGate.h"
#include "qcirCmd.h"
#include "util.h"
#include "phase.h"

using namespace std;

extern QCir *qCir;
extern size_t verbose;
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
         cmdMgr->regCmd("QCZXMap", 5, new QCirZXMappingCmd) &&
         cmdMgr->regCmd("QCTSMap", 5, new QCirTSMappingCmd) &&
         cmdMgr->regCmd("QCCWrite", 4, new QCirWriteCmd)
         // && cmdMgr->regCmd("QCT", 3, new QCirTestCmd)
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

// CmdExecStatus
// QCirTestCmd::exec(const string &option)
// {
//    qCir->writeQASM("test.qasm");
//    return CMD_EXEC_DONE;
// }
// void QCirTestCmd::usage(ostream &os) const
// {
//    os << "Usage: QCT" << endl;
// }

// void QCirTestCmd::help() const
// {
//    cout << setw(15) << left << "QCT: "
//         << "Test what function you want (for developement)" << endl;
// }

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

   if (qCir != 0)
   {
      if (doReplace)
      {
         cerr << "Note: original quantum circuit is replaced..." << endl;
         curCmd = QCIRINIT;
         delete qCir;
         qCir = 0;
      }
      else
      {
         cerr << "Error: circuit already exists!!" << endl;
         return CMD_EXEC_ERROR;
      }
   }
   qCir = new QCir;

   if (!qCir->parse(fileName))
   {
      curCmd = QCIRINIT;
      delete qCir;
      qCir = 0;
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
   if (!qCir)
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
      if(qCir->getGate(id)==NULL){
         cerr << "Error: id " << id << " not found!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, strID);
      }
      size_t tmp = 4;
      qCir->getGate(id)->getZXform(tmp)->printVertices();
   }
   else{
      if (!qCir->printGateInfo(id, showTime))
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

   if (!qCir)
   {
      cerr << "Error: quantum circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   if (token.empty() || myStrNCmp("-List", token, 2) == 0)
      qCir->printSummary();
   else if (myStrNCmp("-Qubit", token, 2) == 0)
      qCir->printQubits();
   else if (myStrNCmp("-ZXform", token, 2) == 0)
      qCir->printZXTopoOrder();
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
   if (!qCir)
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
      if (qCir->getQubit(id) == NULL)
      {
         cerr << "Error: qubit ID is not in current circuit!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[1]);
      }
      qubits.push_back(id);
      type = type.erase(0,1);
      qCir->addGate(type, qubits, Phase(0),appendGate);
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
         if (qCir->getQubit(id) == NULL)
         {
            cerr << "Error: qubit ID is not in current circuit!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         }
         qubits.push_back(id);
      }
      type = type.erase(0,1);
      qCir->addGate(type, qubits, Phase(0),appendGate);
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
      if (qCir->getQubit(id) == NULL)
      {
         cerr << "Error: qubit ID is not in current circuit!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[3]);
      }
      qubits.push_back(id);
      type = type.erase(0,1);
      qCir->addGate(type, qubits, phase, appendGate);
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
      if (qCir == 0)
         qCir = new QCir;
      qCir->addQubit(1);
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
         if (qCir == 0)
            qCir = new QCir;
         qCir->addQubit(id);
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
   if (!qCir)
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
   if (!qCir->removeGate(id))
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
   if (!qCir)
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
   if (!qCir->removeQubit(id))
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
   if (!CmdExec::lexNoOption(option))
      return CMD_EXEC_ERROR;

   if (!qCir)
   {
      cerr << "Error: quantum circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   qCir -> ZXMapping();
   return CMD_EXEC_DONE;
}

void QCirZXMappingCmd::usage(ostream &os) const
{
   os << "Usage: QCZXMapping" << endl;
}

void QCirZXMappingCmd::help() const
{
   cout << setw(15) << left << "QCZXMapping: "
        << "mapping to ZX diagram\n";
}

//----------------------------------------------------------------------
//    QCTSMapping
//----------------------------------------------------------------------
CmdExecStatus
QCirTSMappingCmd::exec(const string &option)
{
   // check option
   if (!CmdExec::lexNoOption(option))
      return CMD_EXEC_ERROR;

   if (!qCir)
   {
      cerr << "Error: quantum circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   qCir -> tensorMapping();
   return CMD_EXEC_DONE;
}

void QCirTSMappingCmd::usage(ostream &os) const
{
   os << "Usage: QCTSMapping" << endl;
}

void QCirTSMappingCmd::help() const
{
   cout << setw(15) << left << "QCTSMapping: "
        << "mapping to tensor\n";
}

//----------------------------------------------------------------------
//    QCCWriter
//----------------------------------------------------------------------
CmdExecStatus
QCirWriteCmd::exec(const string &option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;

   if (!qCir)
   {
      cerr << "Error: quantum circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   if(! qCir -> writeQASM(token)){
      cerr << "Error: file " << token << " path not found!!" << endl;
      return CMD_EXEC_ERROR;
   }
   return CMD_EXEC_DONE;
}

void QCirWriteCmd::usage(ostream &os) const
{
   os << "Usage: QCCWrite <string Output.qasm>" << endl;
}

void QCirWriteCmd::help() const
{
   cout << setw(15) << left << "QCCWrite: "
        << "write QASM file\n";
}
