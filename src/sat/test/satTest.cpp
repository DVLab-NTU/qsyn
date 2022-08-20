#include <iostream>
#include <vector>
#include "sat.h"

using namespace std;

class Gate
{
public:
   Gate(unsigned i = 0): _gid(i) {}
   ~Gate() {}

   Var getVar() const { return _var; }
   void setVar(const Var& v) { _var = v; }

private:
   unsigned   _gid;  // for debugging purpose...
   Var        _var;
};

// 
//[0] PI  1 (a)
//[1] PI  2 (b)
//[2] AIG 4 1 2
//[3] PI  3 (c)
//[4] AIG 5 1 3
//[5] AIG 6 !4 !5
//[6] PO  9 !6
//[7] AIG 7 !2 !3
//[8] AIG 8 !7 1
//[9] PO  10 8
//
vector<Gate *> gates;

void
initCircuit()
{
   // Init gates
   gates.push_back(new Gate(1));  // gates[0]
   gates.push_back(new Gate(2));  // gates[1]
   gates.push_back(new Gate(4));  // gates[2]
   gates.push_back(new Gate(3));  // gates[3]
   gates.push_back(new Gate(5));  // gates[4]
   gates.push_back(new Gate(6));  // gates[5]
   gates.push_back(new Gate(9));  // gates[6]
   gates.push_back(new Gate(7));  // gates[7]
   gates.push_back(new Gate(8));  // gates[8]
   gates.push_back(new Gate(10)); // gates[9]

   // POs are not needed in this demo example
}

void
genProofModel(SatSolver& s)
{
   // Allocate and record variables; No Var ID for POs
   for (size_t i = 0, n = gates.size(); i < n; ++i) {
      Var v = s.newVar();
      gates[i]->setVar(v);
   }

   // Hard code the model construction here...
   // [2] AIG 4 1 2 ==> [2] = [0] & [1]
   s.addAigCNF(gates[2]->getVar(), gates[0]->getVar(), false,
               gates[1]->getVar(), false);
   // [4] AIG 5 1 3 ==> [4] = [0] & [3]
   s.addAigCNF(gates[4]->getVar(), gates[0]->getVar(), false,
               gates[3]->getVar(), false);
   // [5] AIG 6 !4 !5 ==> [5] = ![2] & ![4]
   s.addAigCNF(gates[5]->getVar(), gates[2]->getVar(), true,
               gates[4]->getVar(), true);
   // [7] AIG 7 !2 !3 ==> [7] = ![1] & ![3]
   s.addAigCNF(gates[7]->getVar(), gates[1]->getVar(), true,
               gates[3]->getVar(), true);
   // [8] AIG 8 !7 1 ==> [8] = ![7] & [0]
   s.addAigCNF(gates[8]->getVar(), gates[7]->getVar(), true,
               gates[0]->getVar(), false);
}

void reportResult(const SatSolver& solver, bool result)
{
   solver.printStats();
   cout << (result? "SAT" : "UNSAT") << endl;
   if (result) {
      for (size_t i = 0, n = gates.size(); i < n; ++i)
         cout << solver.getValue(gates[i]->getVar()) << endl;
   }
}

int main()
{
   initCircuit();

   SatSolver solver;
   solver.initialize();

   //
   genProofModel(solver);

   bool result;
   // k = Solve(Gate(5) ^ !Gate(8))
   Var newV = solver.newVar();
   solver.addXorCNF(newV, gates[5]->getVar(), false, gates[8]->getVar(), true);
   solver.assumeRelease();  // Clear assumptions
   solver.assumeProperty(newV, true);  // k = 1
   result = solver.assumpSolve();
   reportResult(solver, result);

   cout << endl << endl << "======================" << endl;

   // k = Solve(Gate(3) & !Gate(7))
   newV = solver.newVar();
   solver.addAigCNF(newV, gates[3]->getVar(), false, gates[7]->getVar(), true);
   solver.assumeRelease();  // Clear assumptions
   solver.assumeProperty(newV, true);  // k = 1
   result = solver.assumpSolve();
   reportResult(solver, result);
}
