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
#include "qcir.h"
#include "qcirGate.h"
#include "qcirMgr.h"
#include "qcirCmd.h"
#include "util.h"
#include "phase.h"

using namespace std;

extern QCirMgr *qcirMgr;
extern size_t verbose;
extern int effLimit;

bool initQCirCmd()
{
   qcirMgr = new QCirMgr;
   if (!(cmdMgr->regCmd("QCCHeckout", 4, new QCirCheckoutCmd) &&
         cmdMgr->regCmd("QCReset", 3, new QCirResetCmd) &&
         cmdMgr->regCmd("QCDelete", 3, new QCirDeleteCmd) &&
         cmdMgr->regCmd("QCNew", 3, new QCirNewCmd) &&
         cmdMgr->regCmd("QCCOPy", 5, new QCirCopyCmd) &&
         cmdMgr->regCmd("QCCOMpose", 5, new QCirComposeCmd) &&
         cmdMgr->regCmd("QCTensor", 3, new QCirTensorCmd) &&
         cmdMgr->regCmd("QCPrint", 3, new QCPrintCmd) &&
         cmdMgr->regCmd("QCCRead", 4, new QCirReadCmd) &&
         cmdMgr->regCmd("QCCPrint", 4, new QCirPrintCmd) &&
         cmdMgr->regCmd("QCGAdd", 4, new QCirAddGateCmd) &&
         cmdMgr->regCmd("QCBAdd", 4, new QCirAddQubitCmd) &&
         cmdMgr->regCmd("QCGDelete", 4, new QCirDeleteGateCmd) &&
         cmdMgr->regCmd("QCBDelete", 4, new QCirDeleteQubitCmd) &&
         cmdMgr->regCmd("QCGPrint", 4, new QCirGatePrintCmd) &&
         cmdMgr->regCmd("QC2ZX", 5, new QCir2ZXCmd) &&
         cmdMgr->regCmd("QC2TS", 5, new QCir2TSCmd) &&
         cmdMgr->regCmd("QCCWrite", 4, new QCirWriteCmd) &&
         cmdMgr->regCmd("QCGMAdd", 5, new QCirAddMultipleCmd)
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

//----------------------------------------------------------------------
//    QCCHeckout <(size_t id)>
//----------------------------------------------------------------------

CmdExecStatus
QCirCheckoutCmd::exec(const string &option) {    
   string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    
    if (token.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
    else {
        unsigned id;
        QC_CMD_ID_VALID_OR_RETURN(token, id, "QCir");
        QC_CMD_QCIR_ID_EXISTED_OR_RETURN(id);
        qcirMgr->checkout2QCir(id);
    }
    return CMD_EXEC_DONE;
}

void QCirCheckoutCmd::usage(ostream &os) const {
    os << "Usage: QCCHeckout <(size_t id)>" << endl;
}

void QCirCheckoutCmd::help() const {
    cout << setw(15) << left << "QCCHeckout: "
         << "checkout to QCir <id> in QCirMgr" << endl;
}

//----------------------------------------------------------------------
//    QCReset
//----------------------------------------------------------------------
CmdExecStatus
QCirResetCmd::exec(const string &option) {
    if(!lexNoOption(option)) return CMD_EXEC_ERROR;
    if(!qcirMgr) qcirMgr = new QCirMgr;
    else qcirMgr->reset();
    return CMD_EXEC_DONE;
}

void QCirResetCmd::usage(ostream &os) const {
    os << "Usage: QCReset" << endl;
}

void QCirResetCmd::help() const {
    cout << setw(15) << left << "QCReset: "
         << "reset QCirMgr" << endl;
}

//----------------------------------------------------------------------
//    QCDelete <(size_t id)>
//----------------------------------------------------------------------
CmdExecStatus
QCirDeleteCmd::exec(const string &option) {    
   string token;
   if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    
   if (token.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
   else {
      unsigned id;
      QC_CMD_ID_VALID_OR_RETURN(token, id, "QCir");
      QC_CMD_QCIR_ID_EXISTED_OR_RETURN(id);
      qcirMgr->removeQCir(id);
   }
   return CMD_EXEC_DONE;
}

void QCirDeleteCmd::usage(ostream &os) const {
    os << "Usage: QCDelete <size_t id>" << endl;
}

void QCirDeleteCmd::help() const {
    cout << setw(15) << left << "QCDelete: "
         << "remove a QCir from QCirMgr" << endl;
}

//----------------------------------------------------------------------
//    QCNew [(size_t id)]
//----------------------------------------------------------------------
CmdExecStatus
QCirNewCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if (token.empty())
        qcirMgr->addQCir(qcirMgr->getNextID());
    else {
        unsigned id;
        QC_CMD_ID_VALID_OR_RETURN(token, id, "QCir");
        qcirMgr->addQCir(id);
    }
    return CMD_EXEC_DONE;
}

void QCirNewCmd::usage(ostream &os) const {
    os << "Usage: QCNew [size_t id]" << endl;
}

void QCirNewCmd::help() const {
    cout << setw(15) << left << "QCNew: "
         << "new QCir to QCirMgr" << endl;
}

//----------------------------------------------------------------------
//    QCCOPy [(size_t id)]
//----------------------------------------------------------------------
CmdExecStatus
QCirCopyCmd::exec(const string &option) {    // check option
    vector<string> options;
    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    
    CMD_N_OPTS_AT_MOST_OR_RETURN(options, 2);
    QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCCOPy");

    if(options.size() == 2){
        bool doReplace = false;
        size_t id_idx = 0;
        for(size_t i = 0; i < options.size(); i++){
            if(myStrNCmp("-Replace", options[i], 2) == 0){
                doReplace = true;
                id_idx = 1 - i;
                break;
            } 
        }
        if(!doReplace) return CmdExec::errorOption(CMD_OPT_MISSING, "-Replace");
        unsigned id_g;
        QC_CMD_ID_VALID_OR_RETURN(options[id_idx], id_g, "QCir");
        QC_CMD_QCIR_ID_EXISTED_OR_RETURN(id_g);
        qcirMgr->copy(id_g, false);
    }
    else if(options.size() == 1){
        unsigned id_g;
        QC_CMD_ID_VALID_OR_RETURN(options[0], id_g, "QCir");
        QC_CMD_QCIR_ID_EXISTED_OR_RETURN(id_g);
        qcirMgr->copy(id_g);
    }
    else{
        qcirMgr->copy(qcirMgr->getNextID());
    }
    return CMD_EXEC_DONE;
}

void QCirCopyCmd::usage(ostream &os) const {
   os << "Usage: ZXCOPy <size_t id> [-Replace]" << endl;
}

void QCirCopyCmd::help() const {
   cout << setw(15) << left << "ZXCOPy: "
         << "copy a ZX-graph" << endl;
}

//----------------------------------------------------------------------
//    QCCOMpose <size_t id>
//----------------------------------------------------------------------
CmdExecStatus
QCirComposeCmd::exec(const string &option) {    
   string token;
   if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
   if (token.empty()) {
      cerr << "Error: the QCir id you want to compose must be provided!" << endl;
      return CmdExec::errorOption(CMD_OPT_MISSING, token);
   } 
   else {
      unsigned id;
      QC_CMD_ID_VALID_OR_RETURN(token, id, "QCir");
      QC_CMD_QCIR_ID_EXISTED_OR_RETURN(id);
      qcirMgr->getQCircuit()->compose(qcirMgr->findQCirByID(id));
   }
   return CMD_EXEC_DONE;
}

void QCirComposeCmd::usage(ostream &os) const {
   os << "Usage: QCCOMpose <size_t id>" << endl;
}

void QCirComposeCmd::help() const {
   cout << setw(15) << left << "QCCOMpose: "
         << "compose a QCir" << endl;
}

//----------------------------------------------------------------------
//    QCTensor <size_t id>
//----------------------------------------------------------------------
CmdExecStatus
QCirTensorCmd::exec(const string &option) {    
   string token;
   if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
   if (token.empty()) {
      cerr << "Error: the QCir id you want to tensor must be provided!" << endl;
      return CmdExec::errorOption(CMD_OPT_MISSING, token);
   } 
   else {
      unsigned id;
      QC_CMD_ID_VALID_OR_RETURN(token, id, "QCir");
      QC_CMD_QCIR_ID_EXISTED_OR_RETURN(id);
      qcirMgr->getQCircuit()->tensorProduct(qcirMgr->findQCirByID(id));
   }

   return CMD_EXEC_DONE;
}

void QCirTensorCmd::usage(ostream &os) const {
    os << "Usage: QCTensor <size_t id>" << endl;
}

void QCirTensorCmd::help() const {
    cout << setw(15) << left << "QCTensor: "
         << "tensor a QCir" << endl;
}

//----------------------------------------------------------------------
//    QCPrint [-Summary | -Focus | -Num]
//----------------------------------------------------------------------
CmdExecStatus
QCPrintCmd::exec(const string &option) {    
   string token;
   if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
   if (token.empty() || myStrNCmp("-Summary", token, 2) == 0) {
      qcirMgr->printQCirMgr();
   } 
   else if (myStrNCmp("-Focus", token, 2) == 0) qcirMgr->printCListItr();
   else if (myStrNCmp("-Num", token, 2) == 0) qcirMgr->printQCircuitListSize();
   else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   return CMD_EXEC_DONE;
}

void QCPrintCmd::usage(ostream &os) const {
    os << "Usage: QCPrint [-Summary | -Focus | -Num]" << endl;
}

void QCPrintCmd::help() const {
    cout << setw(15) << left << "QCPrint: "
         << "print info in QCirMgr" << endl;
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
   if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end()) {
      cout << "Note: QCir list is empty now. Create a new one." << endl;
      qcirMgr->addQCir(qcirMgr->getNextID());
      if (!qcirMgr->getQCircuit()->readQCirFile(fileName)){
         return CMD_EXEC_ERROR;
      }
   }
   else{
      if (doReplace)
      {
         if(verbose >=1) cout << "Note: original QCir is replaced..." << endl;
         qcirMgr->getQCircuit()->reset();
         if (!qcirMgr->getQCircuit()->readQCirFile(fileName)){
            return CMD_EXEC_ERROR;
         }
      }
      else
      {
         qcirMgr->addQCir(qcirMgr->getNextID());
         if (!qcirMgr->getQCircuit()->readQCirFile(fileName)){
            return CMD_EXEC_ERROR;
         }
      }
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
        << "read a circuit and construct corresponding netlist" << endl;
}

//----------------------------------------------------------------------
//    QCGPrint <(size_t gateID)> [-Time | -ZXform]
//----------------------------------------------------------------------
CmdExecStatus
QCirGatePrintCmd::exec(const string &option)
{
   // check option
   QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCGPrint");
   
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
      if(qcirMgr->getQCircuit()->getGate(id)==NULL){
         cerr << "Error: id " << id << " not found!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, strID);
      }
      qcirMgr->getQCircuit()->getGate(id)->getZXform()->printVertices();
   }
   else{
      if (!qcirMgr->getQCircuit()->printGateInfo(id, showTime))
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
//    QCCPrint [-Summary | -List | -Qubit | -ZXform]
//----------------------------------------------------------------------
CmdExecStatus
QCirPrintCmd::exec(const string &option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;

   QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCCPrint");

   if (token.empty() || myStrNCmp("-Summary", token, 2) == 0)
      qcirMgr->getQCircuit()->printSummary();
   else if (myStrNCmp("-List", token, 2) == 0)
      qcirMgr->getQCircuit()->printGates();
   else if (myStrNCmp("-Qubit", token, 2) == 0)
      qcirMgr->getQCircuit()->printQubits();
   else if (myStrNCmp("-ZXform", token, 3) == 0)
      qcirMgr->getQCircuit()->printZXTopoOrder();
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

//-------------------------------------------------------------------------------------------------------------------------------
//     QCGAdd <-H | -X | -Z | -T | -TDG | -S | -V> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-CX> <(size_t ctrl)> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-RZ> <-PHase (Phase phase_inp)> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-CRZ> <-PHase (Phase phase_inp)> <(size_t ctrl)> <(size_t targ)> [-APpend|-PRepend]
// .   QCGAdd <-CNRX> <-PHase (Phase phase_inp)> <(size_t ctrl1)> ... <(size_t ctrln)> <(size_t targ)> [-APpend|-PRepend]
// .   QCGAdd <-CNRZ> <-PHase (Phase phase_inp)> <(size_t ctrl1)> ... <(size_t ctrln)> <(size_t targ)> [-APpend|-PRepend]
//-------------------------------------------------------------------------------------------------------------------------------
CmdExecStatus
QCirAddGateCmd::exec(const string &option)
{
   QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCGAdd");
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
   // <-H | -X | -Z | -TG | -TDg | -S | -V | -Y | -SY | -SDG>
   if (myStrNCmp("-H", type, 2) == 0|| myStrNCmp("-X", type, 2) == 0 || myStrNCmp("-Z", type, 2) == 0 || myStrNCmp("-T", type, 2) == 0 ||
   myStrNCmp("-TDG", type, 4) == 0 || myStrNCmp("-S", type, 2) == 0 || myStrNCmp("-V", type, 2) == 0 || myStrNCmp("-Y", type, 2) == 0 || 
   myStrNCmp("-SY", type, 3) == 0 || myStrNCmp("-SDG", type, 4) == 0)
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
      if (qcirMgr->getQCircuit()->getQubit(id) == NULL)
      {
         cerr << "Error: qubit ID is not in current circuit!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[1]);
      }
      qubits.push_back(id);
      type = type.erase(0,1);
      qcirMgr->getQCircuit()->addGate(type, qubits, Phase(0), appendGate);
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
         if (qcirMgr->getQCircuit()->getQubit(id) == NULL)
         {
            cerr << "Error: qubit ID is not in current circuit!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         }
         qubits.push_back(id);
      }
      type = type.erase(0,1);
      qcirMgr->getQCircuit()->addGate(type, qubits, Phase(0),appendGate);
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
      if (qcirMgr->getQCircuit()->getQubit(id) == NULL)
      {
         cerr << "Error: qubit ID is not in current circuit!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[3]);
      }
      qubits.push_back(id);
      type = type.erase(0,1);
      qcirMgr->getQCircuit()->addGate(type, qubits, phase, appendGate);
   }
   else if(myStrNCmp("-CNRX", type, 5) == 0 || myStrNCmp("-CNRZ", type, 5) == 0){
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
      for(size_t i=3; i<options.size(); i++){
         unsigned id;
         if(!myStr2Uns(options[i],id)){
            cerr << "Error: target ID should be a positive integer!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         }
         if (qcirMgr->getQCircuit()->getQubit(id) == NULL)
         {
            cerr << "Error: qubit ID is not in current circuit!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         }
         qubits.push_back(id);
      }
      if(qubits.size() == 1)  {
         if( myStrNCmp("-CNRX", type, 5) == 0)
            qcirMgr->getQCircuit()->addGate("rx", qubits, phase, appendGate);
         else
            qcirMgr->getQCircuit()->addGate("rz", qubits, phase, appendGate);
      }
      else {
         type = type.erase(0,1);
         qcirMgr->getQCircuit()->addGate(type, qubits, phase, appendGate);
      } 
   }
   else if(myStrNCmp("-CCX", type, 4) == 0){
      if (options.size() < 4)
         return CmdExec::errorOption(CMD_OPT_MISSING, options[options.size()-1]);
      if (options.size() > 4)
         return CmdExec::errorOption(CMD_OPT_EXTRA, options[3]);
      for(size_t i=1; i<options.size(); i++){
         unsigned id;
         if(!myStr2Uns(options[i],id)){
            cerr << "Error: target ID should be a positive integer!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         }
         if (qcirMgr->getQCircuit()->getQubit(id) == NULL)
         {
            cerr << "Error: qubit ID is not in current circuit!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         }
         qubits.push_back(id);
      }
      type = type.erase(0,1);
      qcirMgr->getQCircuit()->addGate(type, qubits, Phase(0), appendGate);
   }
   else{
      cerr << "Error: type is not implemented!!" << endl;
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, type);
   }
     
   return CMD_EXEC_DONE;
}

void QCirAddGateCmd::usage(ostream &os) const
{
   os << "QCGAdd <-H | -X | -Z | -T | -TDG | -S | -SDG | -V | -Y | -SY> <(size_t targ)> [-APpend|-PRepend]" << endl;
   os << "QCGAdd <-CX> <(size_t ctrl)> <(size_t targ)> [-APpend|-PRepend]" << endl;
   os << "QCGAdd <-CCX> <(size_t ctrl)> <(size_t targ)> [-APpend|-PRepend]" << endl;
   os << "QCGAdd <-RZ> <-PHase (Phase phase_inp)> <(size_t targ)> [-APpend|-PRepend]" << endl;
   os << "QCGAdd <-CNRX> <-PHase (Phase phase_inp)> <(size_t ctrl1)> ... <(size_t ctrln)> <(size_t targ)> [-APpend|-PRepend]" << endl;
   os << "QCGAdd <-CNRZ> <-PHase (Phase phase_inp)> <(size_t ctrl1)> ... <(size_t ctrln)> <(size_t targ)> [-APpend|-PRepend]" << endl;
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
      if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end()) {
         cout << "Note: QCir list is empty now. Create a new one." << endl;
         qcirMgr->addQCir(qcirMgr->getNextID());
      }
      qcirMgr->getQCircuit()->addQubit(1);
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
         if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end()) {
            cout << "Note: QCir list is empty now. Create a new one." << endl;
            qcirMgr->addQCir(qcirMgr->getNextID());
         }
         qcirMgr->getQCircuit()->addQubit(id);
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
        << "add qubit(s)\n";
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
   QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCGDelete");
   if (token.empty())
      return CmdExec::errorOption(CMD_OPT_MISSING, "");
   unsigned id;
   if(!myStr2Uns(token,id)){
      cerr << "Error: target ID should be a positive integer!!" << endl;
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   }   
   if (!qcirMgr->getQCircuit()->removeGate(id))
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
        << "delete quantum gate\n";
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

   QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCBDelete");

   if (token.empty())
      return CmdExec::errorOption(CMD_OPT_MISSING, "");
   unsigned id;
   if(!myStr2Uns(token,id)){
      cerr << "Error: target ID should be a positive integer!!" << endl;
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   }   
   if (!qcirMgr->getQCircuit()->removeQubit(id))
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
//    QC2ZX
//----------------------------------------------------------------------
CmdExecStatus
QCir2ZXCmd::exec(const string &option)
{
   // check option
   if (!CmdExec::lexNoOption(option))
      return CMD_EXEC_ERROR;

   QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QC2ZX");
   qcirMgr->getQCircuit() -> ZXMapping();
   return CMD_EXEC_DONE;
}

void QCir2ZXCmd::usage(ostream &os) const
{
   os << "Usage: QC2ZX" << endl;
}

void QCir2ZXCmd::help() const
{
   cout << setw(15) << left << "QC2ZX: "
        << "convert the QCir to ZX-graph\n";
}

//----------------------------------------------------------------------
//    QC2TS
//----------------------------------------------------------------------
CmdExecStatus
QCir2TSCmd::exec(const string &option)
{
   // check option
   if (!CmdExec::lexNoOption(option))
      return CMD_EXEC_ERROR;

   QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QC2TS");
   qcirMgr->getQCircuit() -> tensorMapping();
   return CMD_EXEC_DONE;
}

void QCir2TSCmd::usage(ostream &os) const
{
   os << "Usage: QC2TS" << endl;
}

void QCir2TSCmd::help() const
{
   cout << setw(15) << left << "QC2TS: "
        << "convert the QCir to tensor\n";
}

//----------------------------------------------------------------------
//    QCCWrite
//----------------------------------------------------------------------
CmdExecStatus
QCirWriteCmd::exec(const string &option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;

   QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCCWrite");
   if(! qcirMgr->getQCircuit() -> writeQASM(token)){
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

CmdExecStatus
QCirAddMultipleCmd::exec(const string &option){
   QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCGMAdd");
   // check option
   vector<string> options;
   if (!CmdExec::lexOptions(option, options))
      return CMD_EXEC_ERROR;
   vector<size_t> qids;
   for(size_t i=0; i< options.size(); i++){
      unsigned id;
      if(!myStr2Uns(options[i], id)){
         cerr << "Error: target ID should be a positive integer!!" << endl;
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
      }
      qids.push_back(id);
   }

     
   qcirMgr->getQCircuit()->addGate("cnrz", qids, Phase(1), true);
   // qcirMgr->getQCircuit()->addGate("mcx", qids, Phase(0), true);
   return CMD_EXEC_DONE;
}

void QCirAddMultipleCmd::usage(ostream &os) const
{
   os << "Usage: QCGMAdd 0 1 2 3 4" << endl;
}

void QCirAddMultipleCmd::help() const
{
   cout << setw(15) << left << "QCGMAdd: "
      << "add multiple control\n";
}