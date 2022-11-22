/****************************************************************************
  FileName     [ qcirGate.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir gate functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "qcir.h"
#include "qcirGate.h"

using namespace std;

extern QCir *qCir;
extern size_t verbose;

/**
 * @brief Get Qubit.
 *
 * @param qubit
 * @return BitInfo
 */
const BitInfo QCirGate::getQubit(size_t qubit) const {
  for (size_t i = 0; i < _qubits.size(); i++){
    if(_qubits[i]._qubit==qubit)
      return _qubits[i];
  }
  cerr << "Not Found" << endl;
  return _qubits[0];
}

/**
 * @brief Adc qubit to a gate
 *
 * @param qubit
 * @param isTarget
 */
void QCirGate::addQubit(size_t qubit, bool isTarget){
  BitInfo temp = {._qubit = qubit, ._parent = NULL, ._child = NULL, ._isTarget = isTarget};
  _qubits.push_back(temp);
}

/**
 * @brief Set parent to the gate on qubit.
 *
 * @param qubit
 * @param p
 */
void QCirGate::setParent(size_t qubit, QCirGate *p)
{
  for (size_t i = 0; i < _qubits.size(); i++)
  {
    if (_qubits[i]._qubit == qubit)
    {
      _qubits[i]._parent = p;
      break;
    }
  }
}

/**
 * @brief Add dummy child c to gate
 *
 * @param c
 */
void QCirGate::addDummyChild(QCirGate *c)
{
  BitInfo temp = {._qubit = 0, ._parent = NULL, ._child = c, ._isTarget = false};
  _qubits.push_back(temp);
}

/**
 * @brief Set child to gate on qubit.
 *
 * @param qubit
 * @param c
 */
void QCirGate::setChild(size_t qubit, QCirGate *c)
{
  for (size_t i = 0; i < _qubits.size(); i++)
  {
    if (_qubits[i]._qubit == qubit)
    {
      _qubits[i]._child = c;
      break;
    }
  }
}

/**
 * @brief Print Gate brief information
 */
void QCirGate::printGate() const
{
  cout << "Gate " << _id << ": " << getTypeStr() << "   \t"
       << " Exec Time: " << _time << " \t"
       << " Qubit: ";
  for (size_t i = 0; i < _qubits.size(); i++)
  {
    cout << _qubits[i]._qubit << " ";
  }
  cout << endl;
}

void QCirGate::printSingleQubitGate(string gtype, bool showTime) const {
  BitInfo Info = getQubits()[0];
  string qubitInfo = "Q" + to_string(Info._qubit);
  string parentInfo = "";
  if (Info._parent == NULL)
    parentInfo = "Start";
  else
    parentInfo = ("G" + to_string(Info._parent->getId()));
  string childInfo = "";
  if (Info._child == NULL)
    childInfo = "End";
  else
    childInfo = ("G" + to_string(Info._child->getId()));
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " ┌─";
  for (size_t i = 0; i < gtype.size(); i++) cout << "─";
  cout << "─┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ "<< gtype <<" ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └─";
  for (size_t i = 0; i < gtype.size(); i++) cout << "─";
  cout << "─┘ " << endl;
  if(gtype == "RX" || gtype == "RY" || gtype == "RZ")
    cout << "Rotate Phase: " << _rotatePhase << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

/**
 * @brief Print Gate brief information
 */
void QCirGate::printGateInfo(bool showTime) const
{
  string max_qubit = to_string(max_element(_qubits.begin(), _qubits.end(), [] (BitInfo const a, BitInfo const b) {
    return a._qubit < b._qubit;
  })->_qubit);
  
  vector<string> parents;
  for (size_t i = 0; i < _qubits.size(); i++)
  {
    if (getQubits()[i]._parent == NULL)
      parents.push_back("Start");
    else
      parents.push_back("G" + to_string(getQubits()[i]._parent->getId()));
  }
  string max_parent = *max_element(parents.begin(), parents.end(), [] (string const a, string const b) {
    return a.size() < b.size();
  });

  for (size_t i = 0; i < _qubits.size(); i++)
  {
    BitInfo Info = getQubits()[i];
    string qubitInfo = "Q ";
    for (size_t j = 0; j < max_qubit.size() - to_string(Info._qubit).size(); j++)
      qubitInfo += " ";
    qubitInfo += to_string(Info._qubit);
    string parentInfo = "";
    if (Info._parent == NULL)
      parentInfo = "Start";
    else
      parentInfo = ("G" + to_string(Info._parent->getId()));
    for (size_t k = 0; k < max_parent.size() - (parents[i].size()); k++)
      parentInfo += " ";
    string childInfo = "";
    if (Info._child == NULL)
      childInfo = "End";
    else
      childInfo = ("G" + to_string(Info._child->getId()));
    if (Info._isTarget)
    {
      for (size_t j = 0; j < max_qubit.size() + max_parent.size() + 4; j++)
        cout << " ";
      cout << " ┌──┴──┐ " << endl;
      cout << qubitInfo << " " << parentInfo << " ─┤  R"<< ((_type=="cnrx") ? "X" : "Z") <<" ├─ " << childInfo << endl;
      for (size_t j = 0; j < max_qubit.size() + max_parent.size() + 4; j++)
        cout << " ";
      cout << " └─────┘ " << endl;
    }
    else{
      cout << qubitInfo << " " << parentInfo << " ────●──── " << childInfo << endl;
    }
  }
  cout << "Rotate Phase: " << _rotatePhase << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

/**
 * @brief Print gate detailed information
 * 
 * @param showTime
 */
void CnRYGate::printGateInfo(bool showTime) const
{
  cout << "Not Implement Yet!!" << endl;
}

/**
 * @brief Print gate detailed information
 * 
 * @param showTime
 */
void CZGate::printGateInfo(bool showTime) const
{
  BitInfo Info = getQubits()[0];
  string qubitInfo = "Q" + Info._qubit;
  string parentInfo = "";
  if (Info._parent == NULL)
  {
    parentInfo = "Start";
  }
  else
    parentInfo = "G" + Info._parent->getId();
  string childInfo = "";
  if (Info._child == NULL)
  {
    childInfo = "End";
  }
  else
    childInfo = "G" + Info._child->getId();
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 1; i++)
    cout << " ";
  cout << " ┌───┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ H ├─ "
       << "G" << endl;
  cout << " └───┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

/**
 * @brief Print gate detailed information
 * 
 * @param showTime
 */
void CCZGate::printGateInfo(bool showTime) const
{
  cout << "Not Implement Yet!!" << endl;
}

/**
 * @brief Print gate detailed information
 * 
 * @param showTime
 */
void CXGate::printGateInfo(bool showTime) const
{
  string qubit;
  qubit = to_string(max(getQubits()[0]._qubit, getQubits()[1]._qubit));
  string parent[2];
  for (size_t i = 0; i < 2; i++)
  {
    if (getQubits()[i]._parent == NULL)
      parent[i] = "Start";
    else
      parent[i] = ("G" + to_string(getQubits()[i]._parent->getId()));
  }
  size_t max_parent = max(parent[0].size(), parent[1].size());
  for (size_t i = 0; i < 2; i++)
  {
    BitInfo Info = getQubits()[i];
    string qubitInfo = "Q";
    for (size_t j = 0; j < qubit.size() - to_string(Info._qubit).size(); j++)
      qubitInfo += " ";
    qubitInfo += to_string(Info._qubit);
    string parentInfo = "";
    if (Info._parent == NULL)
      parentInfo = "Start";
    else
      parentInfo = ("G" + to_string(Info._parent->getId()));
    for (size_t k = 0; k < max_parent - (parent[i].size()); k++)
      parentInfo += " ";
    string childInfo = "";
    if (Info._child == NULL)
      childInfo = "End";
    else
      childInfo = ("G" + to_string(Info._child->getId()));
    if (Info._isTarget)
    {
      for (size_t i = 0; i < qubit.size() + max_parent + 3; i++)
        cout << " ";
      cout << " ┌─┴─┐ " << endl;
      cout << qubitInfo << " " << parentInfo << " ─┤ X ├─ " << childInfo << endl;
      for (size_t i = 0; i < qubit.size() + max_parent + 3; i++)
        cout << " ";
      cout << " └───┘ " << endl;
    }
    else
    {
      cout << qubitInfo << " " << parentInfo << " ───■─── " << childInfo << endl;
    }
  }
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

/**
 * @brief Print gate detailed information
 * 
 * @param showTime
 */
void CCXGate::printGateInfo(bool showTime) const
{
  cout << "Not Implement Yet!!" << endl;
}