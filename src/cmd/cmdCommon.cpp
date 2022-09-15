/****************************************************************************
  FileName     [ cmdCommon.cpp ]
  PackageName  [ cmd ]
  Synopsis     [ Define common commands ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2007-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/
#include <iomanip>
#include <string>
#include "util.h"
#include "cmdCommon.h"

using namespace std;

bool
initCommonCmd()
{
   if (!(cmdMgr->regCmd("Quit", 1, new QuitCmd) &&
         cmdMgr->regCmd("HIStory", 3, new HistoryCmd) &&
         cmdMgr->regCmd("HELp", 3, new HelpCmd) &&
         cmdMgr->regCmd("DOfile", 2, new DofileCmd) &&
         cmdMgr->regCmd("USAGE", 5, new UsageCmd)
      )) {
      cerr << "Registering \"init\" commands fails... exiting" << endl;
      return false;
   }
   return true;
}


//----------------------------------------------------------------------
//    HELp [(string cmd)]
//----------------------------------------------------------------------
CmdExecStatus
HelpCmd::exec(const string& option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;
   if (token.size()) {
      CmdExec* e = cmdMgr->getCmd(token);
      if (!e) return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
      e->usage(cout);
   }
   else
      cmdMgr->printHelps();
   return CMD_EXEC_DONE;
}

void
HelpCmd::usage(ostream& os) const
{
   os << "Usage: HELp [(string cmd)]" << endl;
}

void
HelpCmd::help() const
{
   cout << setw(15) << left << "HELp: "
        << "print this help message" << endl;
}

//----------------------------------------------------------------------
//    Quit [-Force]
//----------------------------------------------------------------------
CmdExecStatus
QuitCmd::exec(const string& option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;
   if (token.size()) {
      if (myStrNCmp("-Forced", token, 2) != 0)
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
      else
         return CMD_EXEC_QUIT;  // ready to quit
   }

   cout << "Are you sure to quit (Yes/No)? [No] ";
   char str[1024];
   cin.getline(str, 1024);
   string ss = string(str);
   size_t s = ss.find_first_not_of(' ', 0);
   if (s != string::npos) {
      ss = ss.substr(s);
      if (myStrNCmp("Yes", ss, 1) == 0)
         return CMD_EXEC_QUIT;  // ready to quit
   }
   return CMD_EXEC_DONE;     // not yet to quit
}

void
QuitCmd::usage(ostream& os) const
{
   os << "Usage: Quit [-Force]" << endl;
}

void
QuitCmd::help() const
{
   cout << setw(15) << left << "Quit: "
        << "quit the execution" << endl;
}

//----------------------------------------------------------------------
//    HIStory [(int nPrint)]
//----------------------------------------------------------------------
CmdExecStatus
HistoryCmd::exec(const string& option)
{
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token))
      return CMD_EXEC_ERROR;
   int nPrint = -1;
   if (token.size()) {
      if (!myStr2Int(token, nPrint))
         return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   }

   cmdMgr->printHistory(nPrint);

   return CMD_EXEC_DONE;
}

void
HistoryCmd::usage(ostream& os) const
{
   os << "Usage: HIStory [(int nPrint)]" << endl;
}

void
HistoryCmd::help() const
{
   cout << setw(15) << left << "HIStory: "
        << "print command history" << endl;
}


//----------------------------------------------------------------------
//    DOfile <(string file)>
//----------------------------------------------------------------------
// TODO: You DON'T need to modify this function!
//       But you should modify CmdParser::openDofile(), etc, in order
//       to support the following features.
//
// Supported features
// (1) mcalc> dofile do1
//     mcalc> ...        <== some other commands
//     mcalc> dofile do2 <== there is a "dofile do1" in do2
//     mcalc>          
// (2) mcalc> dofile t
//     Error: cannot open file "t"!!
//     mcalc> dofile do <== can open a dofile "do" after failing to open "t"
//     mcalc>          
// (3) If a dofile xx contains a line "dofile xx" calling itself,
//     where xx may or may not exist...  (recursive dofiles)
//     (Let the max recursion depth = 1024)
//
CmdExecStatus
DofileCmd::exec(const string& option)
{     
   // check option 
   string token;
   if (!CmdExec::lexSingleOption(option, token, false))
      return CMD_EXEC_ERROR;
   if (!cmdMgr->openDofile(token))
      return CmdExec::errorOption(CMD_OPT_FOPEN_FAIL, token);
   return CMD_EXEC_DONE;
}

void
DofileCmd::usage(ostream& os) const
{  
   os << "Usage: DOfile <(string file)>" << endl;
}  
      
void
DofileCmd::help() const
{
   cout << setw(15) << left << "DOfile: "
        << "execute the commands in the dofile" << endl;
}


//----------------------------------------------------------------------
//    USAGE [-All | -Time | -Memory]
//----------------------------------------------------------------------
CmdExecStatus
UsageCmd::exec(const string& option)
{
   // check option
   vector<string> options;
   CmdExec::lexOptions(option, options);

   bool repTime = false, repMem = false, repAll = false;
   size_t n = options.size();
   if (n == 0)
      repAll = true;
   else {
      for (size_t i = 0; i < n; ++i) {
         const string& token = options[i];
         if (myStrNCmp("-All", token, 2) == 0) {
            if (repTime | repMem | repAll)
               return CmdExec::errorOption(CMD_OPT_EXTRA, token);
            repAll = true;
         }
         else if (myStrNCmp("-Time", token, 2) == 0) {
            if (repTime | repMem | repAll)
               return CmdExec::errorOption(CMD_OPT_EXTRA, token);
            repTime = true;
         }
         else if (myStrNCmp("-Memory", token, 2) == 0) {
            if (repTime | repMem | repAll)
               return CmdExec::errorOption(CMD_OPT_EXTRA, token);
            repMem = true;
         }
         else
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
      }
   }

   if (repAll) repTime = repMem = true;

   myUsage.report(repTime, repMem);

   return CMD_EXEC_DONE;
}

void
UsageCmd::usage(ostream& os) const
{
   os << "Usage: USAGE [-All | -Time | -Memory]" << endl;
}

void
UsageCmd::help() const
{
   cout << setw(15) << left << "USAGE: "
        << "report the runtime and/or memory usage" << endl;
}
