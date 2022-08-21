/****************************************************************************
  FileName     [ cirCmd.cpp ]
  PackageName  [ cir ]
  Synopsis     [ Define basic cir package commands ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2008-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <iostream>
#include <iomanip>
#include "cirMgr.h"
#include "cirGate.h"
#include "cirCmd.h"
#include "util.h"

using namespace std;

extern CirMgr* cirMgr;
extern int effLimit;

bool
initCirCmd()
{
   if (!(cmdMgr->regCmd("CIRRead", 4, new CirReadCmd) &&
         cmdMgr->regCmd("CIRPrint", 4, new CirPrintCmd) &&
         cmdMgr->regCmd("CIRGate", 4, new CirGateCmd) &&
         cmdMgr->regCmd("CIRSWeep", 5, new CirSweepCmd) &&
         cmdMgr->regCmd("CIROPTimize", 6, new CirOptCmd) &&
         cmdMgr->regCmd("CIRSTRash", 6, new CirStrashCmd) &&
         cmdMgr->regCmd("CIRSIMulate", 6, new CirSimCmd) &&
         cmdMgr->regCmd("CIRFraig", 4, new CirFraigCmd) &&
         cmdMgr->regCmd("CIRWrite", 4, new CirWriteCmd)
      )) {
      cerr << "Registering \"cir\" commands fails... exiting" << endl;
      return false;
   }
   return true;
}

enum CirCmdState
{
   // Order matters! Do not change the order!!
   CIRINIT,
   CIRREAD,
   CIROPT,
   CIRSTRASH,
   CIRSIMULATE,
   CIRFRAIG,
   // dummy end
   CIRCMDTOT
};

static CirCmdState curCmd = CIRINIT;

//----------------------------------------------------------------------
//    CIRRead <(string fileName)> [-Replace]
//----------------------------------------------------------------------
CmdExecStatus
CirReadCmd::exec(const string& option)
{
   // check option
   vector<string> options;
   if (!CmdExec::lexOptions(option, options))
      return CMD_EXEC_ERROR;
   if (options.empty())
      return CmdExec::errorOption(CMD_OPT_MISSING, "");

   bool doReplace = false;
   string fileName;
   for (size_t i = 0, n = options.size(); i < n; ++i) {
      if (myStrNCmp("-Replace", options[i], 2) == 0) {
         if (doReplace) return CmdExec::errorOption(CMD_OPT_EXTRA,options[i]);
         doReplace = true;
      }
      else {
         if (fileName.size())
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         fileName = options[i];
      }
   }

   if (cirMgr != 0) {
      if (doReplace) {
         cerr << "Note: original circuit is replaced..." << endl;
         curCmd = CIRINIT;
         delete cirMgr; cirMgr = 0;
      }
      else {
         cerr << "Error: circuit already exists!!" << endl;
         return CMD_EXEC_ERROR;
      }
   }
   cirMgr = new CirMgr;

   if (!cirMgr->readCircuit(fileName)) {
      curCmd = CIRINIT;
      delete cirMgr; cirMgr = 0;
      return CMD_EXEC_ERROR;
   }

   curCmd = CIRREAD;

   return CMD_EXEC_DONE;
}

void
CirReadCmd::usage(ostream& os) const
{
   os << "Usage: CIRRead <(string fileName)> [-Replace]" << endl;
}

void
CirReadCmd::help() const
{
   cout << setw(15) << left << "CIRRead: "
        << "read in a circuit and construct the netlist" << endl;
}

//----------------------------------------------------------------------
//    CIRPrint [-Summary | -Netlist | -PI | -PO | -FLoating | -FECpairs]
//----------------------------------------------------------------------
CmdExecStatus
CirPrintCmd::exec(const string& option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;

   if (!cirMgr) {
      cerr << "Error: circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   if (token.empty() || myStrNCmp("-Summary", token, 2) == 0)
      cirMgr->printSummary();
   else if (myStrNCmp("-Netlist", token, 2) == 0)
      cirMgr->printNetlist();
   else if (myStrNCmp("-PI", token, 3) == 0)
      cirMgr->printPIs();
   else if (myStrNCmp("-PO", token, 3) == 0)
      cirMgr->printPOs();
   else if (myStrNCmp("-FLoating", token, 3) == 0)
      cirMgr->printFloatGates();
   else if (myStrNCmp("-FECpairs", token, 4) == 0)
      cirMgr->printFECPairs();
   else
      return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);

   return CMD_EXEC_DONE;
}

void
CirPrintCmd::usage(ostream& os) const
{  
   os << "Usage: CIRPrint [-Summary | -Netlist | -PI | -PO | -FLoating "
      << "| -FECpairs]" << endl;
}

void
CirPrintCmd::help() const
{  
   cout << setw(15) << left << "CIRPrint: " << "print circuit\n";
}

//----------------------------------------------------------------------
//    CIRGate <<(int gateId)> [<-FANIn | -FANOut><(int level)>]>
//----------------------------------------------------------------------
CmdExecStatus
CirGateCmd::exec(const string& option)
{
   if (!cirMgr) {
      cerr << "Error: circuit has not been read!!" << endl;
      return CMD_EXEC_ERROR;
   }

   // check option
   vector<string> options;
   if (!CmdExec::lexOptions(option, options))
      return CMD_EXEC_ERROR;

   if (options.empty())
      return CmdExec::errorOption(CMD_OPT_MISSING, "");

   int gateId = -1, level = 0;
   bool doFanin = false, doFanout = false;
   CirGate* thisGate = 0;
   for (size_t i = 0, n = options.size(); i < n; ++i) {
      bool checkLevel = false;
      if (myStrNCmp("-FANIn", options[i], 5) == 0) {
         if (doFanin || doFanout)
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         doFanin = true;
         checkLevel = true;
      }
      else if (myStrNCmp("-FANOut", options[i], 5) == 0) {
         if (doFanin || doFanout)
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         doFanout = true;
         checkLevel = true;
      }
      else if (!thisGate) {
         if (!myStr2Int(options[i], gateId) || gateId < 0)
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         thisGate = cirMgr->getGate(gateId);
         if (!thisGate) {
            cerr << "Error: Gate(" << gateId << ") not found!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
         }
      }
      else if (thisGate)
         return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
      else
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
      if (checkLevel) {
         if (++i == n)
            return CmdExec::errorOption(CMD_OPT_MISSING, options[i-1]);
         if (!myStr2Int(options[i], level) || level < 0)
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         checkLevel = true;
      }
   }

   if (!thisGate) {
      cerr << "Error: Gate id is not specified!!" << endl;
      return CmdExec::errorOption(CMD_OPT_MISSING, options.back());
   }

   if (doFanin)
      thisGate->reportFanin(level);
   else if (doFanout)
      thisGate->reportFanout(level);
   else
      thisGate->reportGate();

   return CMD_EXEC_DONE;
}

void
CirGateCmd::usage(ostream& os) const
{
   os << "Usage: CIRGate <<(int gateId)> [<-FANIn | -FANOut><(int level)>]>"
      << endl;
}

void
CirGateCmd::help() const
{
   cout << setw(15) << left << "CIRGate: " << "report a gate\n";
}

//----------------------------------------------------------------------
//    CIRSWeep
//----------------------------------------------------------------------
CmdExecStatus
CirSweepCmd::exec(const string& option)
{
   if (!cirMgr) {
      cerr << "Error: circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   // check option
   vector<string> options;
   CmdExec::lexOptions(option, options);

   if (!options.empty())
      return CmdExec::errorOption(CMD_OPT_EXTRA, options[0]);

   assert(curCmd != CIRINIT);
   cirMgr->sweep();

   return CMD_EXEC_DONE;
}

void
CirSweepCmd::usage(ostream& os) const
{
   os << "Usage: CIRSWeep" << endl;
}

void
CirSweepCmd::help() const
{
   cout << setw(15) << left << "CIRSWeep: "
        << "remove unused gates\n";
}

//----------------------------------------------------------------------
//    CIROPTimize
//----------------------------------------------------------------------
CmdExecStatus
CirOptCmd::exec(const string& option)
{
   if (!cirMgr) {
      cerr << "Error: circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   // check option
   vector<string> options;
   CmdExec::lexOptions(option, options);

   if (!options.empty())
      return CmdExec::errorOption(CMD_OPT_EXTRA, options[0]);

   assert(curCmd != CIRINIT);
   if (curCmd == CIRSIMULATE) {
      cerr << "Error: circuit has been simulated!! Do \"CIRFraig\" first!!"
           << endl;
      return CMD_EXEC_ERROR;
   }
   cirMgr->optimize();
   curCmd = CIROPT;

   return CMD_EXEC_DONE;
}

void
CirOptCmd::usage(ostream& os) const
{
   os << "Usage: CIROPTimize" << endl;
}

void
CirOptCmd::help() const
{
   cout << setw(15) << left << "CIROPTimize: "
        << "perform trivial optimizations\n";
}

//----------------------------------------------------------------------
//    CIRSTRash
//----------------------------------------------------------------------
CmdExecStatus
CirStrashCmd::exec(const string& option)
{
   if (!cirMgr) {
      cerr << "Error: circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   // check option
   vector<string> options;
   CmdExec::lexOptions(option, options);

   if (!options.empty())
      return CmdExec::errorOption(CMD_OPT_EXTRA, options[0]);

   assert(curCmd != CIRINIT);
   if (curCmd == CIRSTRASH) {
      cerr << "Error: circuit has been strashed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   else if (curCmd == CIRSIMULATE) {
      cerr << "Error: circuit has been simulated!! Do \"CIRFraig\" first!!"
           << endl;
      return CMD_EXEC_ERROR;
   }
   cirMgr->strash();
   curCmd = CIRSTRASH;

   return CMD_EXEC_DONE;
}

void
CirStrashCmd::usage(ostream& os) const
{
   os << "Usage: CIRSTRash" << endl;
}

void
CirStrashCmd::help() const
{
   cout << setw(15) << left << "CIRSTRash: "
        << "perform structural hash on the circuit netlist\n";
}

//----------------------------------------------------------------------
//    CIRSIMulate <-Random | -File <string patternFile>>
//                [-Output (string logFile)]
//----------------------------------------------------------------------
CmdExecStatus
CirSimCmd::exec(const string& option)
{
   if (!cirMgr) {
      cerr << "Error: circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   // check option
   vector<string> options;
   CmdExec::lexOptions(option, options);

   ifstream patternFile;
   ofstream logFile;
   bool doRandom = false, doFile = false, doLog = false;
   for (size_t i = 0, n = options.size(); i < n; ++i) {
      if (myStrNCmp("-Random", options[i], 2) == 0) {
         if (doRandom || doFile)
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         doRandom = true;
      }
      else if (myStrNCmp("-File", options[i], 2) == 0) {
         if (doRandom || doFile)
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         if (++i == n)
            return CmdExec::errorOption(CMD_OPT_MISSING, options[i-1]);
         patternFile.open(options[i].c_str(), ios::in);
         if (!patternFile)
            return CmdExec::errorOption(CMD_OPT_FOPEN_FAIL, options[i]);
         doFile = true;
      }
      else if (myStrNCmp("-Output", options[i], 2) == 0) {
         if (doLog)
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         if (++i == n)
            return CmdExec::errorOption(CMD_OPT_MISSING, options[i-1]);
         logFile.open(options[i].c_str(), ios::out);
         if (!logFile)
            return CmdExec::errorOption(CMD_OPT_FOPEN_FAIL, options[i]);
         doLog = true;
      }
      else
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
   }

   if (!doRandom && !doFile)
      return CmdExec::errorOption(CMD_OPT_MISSING, "");

   assert (curCmd != CIRINIT);
   if (doLog)
      cirMgr->setSimLog(&logFile);
   else cirMgr->setSimLog(0);

   if (doRandom)
      cirMgr->randomSim();
   else
      cirMgr->fileSim(patternFile);
   cirMgr->setSimLog(0);
   curCmd = CIRSIMULATE;
   
   return CMD_EXEC_DONE;
}

void
CirSimCmd::usage(ostream& os) const
{
   os << "Usage: CIRSIMulate <-Random | -File <string patternFile>>\n"
      << "                   [-Output (string logFile)]" << endl;
}

void
CirSimCmd::help() const
{
   cout << setw(15) << left << "CIRSIMulate: "
        << "perform Boolean logic simulation on the circuit\n";
}

//----------------------------------------------------------------------
//    CIRFraig
//----------------------------------------------------------------------
CmdExecStatus
CirFraigCmd::exec(const string& option)
{
   if (!cirMgr) {
      cerr << "Error: circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   // check option
   vector<string> options;
   CmdExec::lexOptions(option, options);

   if (!options.empty())
      return CmdExec::errorOption(CMD_OPT_EXTRA, options[0]);

   if (curCmd != CIRSIMULATE) {
      cerr << "Error: circuit is not yet simulated!!" << endl;
      return CMD_EXEC_ERROR;
   }
   cirMgr->fraig();
   curCmd = CIRFRAIG;

   return CMD_EXEC_DONE;
}

void
CirFraigCmd::usage(ostream& os) const
{
   os << "Usage: CIRFraig" << endl;
}

void
CirFraigCmd::help() const
{
   cout << setw(15) << left << "CIRFraig: "
        << "perform Boolean logic simulation on the circuit\n";
}

//----------------------------------------------------------------------
//    CIRWrite [(int gateId)][-Output (string aagFile)]
//----------------------------------------------------------------------
CmdExecStatus
CirWriteCmd::exec(const string& option)
{
   if (!cirMgr) {
      cerr << "Error: circuit is not yet constructed!!" << endl;
      return CMD_EXEC_ERROR;
   }
   // check option
   vector<string> options;
   CmdExec::lexOptions(option, options);

   if (options.empty()) {
      cirMgr->writeAag(cout);
      return CMD_EXEC_DONE;
   }
   bool hasFile = false;
   int gateId;
   CirGate *thisGate = NULL;
   ofstream outfile;
   for (size_t i = 0, n = options.size(); i < n; ++i) {
      if (myStrNCmp("-Output", options[i], 2) == 0) {
         if (hasFile) 
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
         if (++i == n)
            return CmdExec::errorOption(CMD_OPT_MISSING, options[i-1]);
         outfile.open(options[i].c_str(), ios::out);
         if (!outfile)
            return CmdExec::errorOption(CMD_OPT_FOPEN_FAIL, options[1]);
         hasFile = true;
      }
      else if (myStr2Int(options[i], gateId) && gateId >= 0) {
         if (thisGate != NULL)
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
         thisGate = cirMgr->getGate(gateId);
         if (!thisGate) {
            cerr << "Error: Gate(" << gateId << ") not found!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         }
         if (!thisGate->isAig()) {
             cerr << "Error: Gate(" << gateId << ") is NOT an AIG!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
         }
      }
      else return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
   }

   if (!thisGate) {
      assert (hasFile);
      cirMgr->writeAag(outfile);
   }
   else if (hasFile) cirMgr->writeGate(outfile, thisGate);
   else cirMgr->writeGate(cout, thisGate);

   return CMD_EXEC_DONE;
}

void
CirWriteCmd::usage(ostream& os) const
{
   os << "Usage: CIRWrite [(int gateId)][-Output (string aagFile)]" << endl;
}

void
CirWriteCmd::help() const
{
   cout << setw(15) << left << "CIRWrite: "
        << "write the netlist to an ASCII AIG file (.aag)\n";
}

