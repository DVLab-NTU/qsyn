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
#include "qcirMgr.h"
#include "qcirGate.h"

using namespace std;

extern QCirMgr *qCirMgr;

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

void QCirGate::addDummyChild(QCirGate *c)
{
  BitInfo temp = {._qubit = 0, ._parent = NULL, ._child = c, ._isTarget = false};
  _qubits.push_back(temp);
}

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

void HGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌───┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ H ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └───┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

void CnRZGate::printGateInfo(bool showTime) const
{
  cout << "Not Implement Yet!!" << endl;
}

void CnRYGate::printGateInfo(bool showTime) const
{
  cout << "Not Implement Yet!!" << endl;
}

void CnRXGate::printGateInfo(bool showTime) const
{
  cout << "Not Implement Yet!!" << endl;
}

void ZGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌───┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ Z ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └───┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

void SGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌───┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ S ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └───┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

void SDGGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌───┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ Sdg ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └───┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

void TGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌───┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ T ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └───┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

void TDGGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌─────┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ Tdg ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └─────┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

void RZGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌────┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ RZ ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └────┘ " << endl;
  cout << "Rotate Phase: " << _rotatePhase << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

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

void XGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌───┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ X ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └───┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

void SXGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌────┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ SX ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └────┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

void RXGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌────┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ RX ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └────┘ " << endl;
  cout << "Rotate Phase: " << _rotatePhase << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

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

void CCXGate::printGateInfo(bool showTime) const
{
  cout << "Not Implement Yet!!" << endl;
}

void YGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌───┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ Y ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └───┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}

void SYGate::printGateInfo(bool showTime) const
{
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
  cout << " ┌────┐ " << endl;
  cout << qubitInfo << " " << parentInfo << " ─┤ SY ├─ " << childInfo << endl;
  for (size_t i = 0; i < parentInfo.size() + qubitInfo.size() + 2; i++)
    cout << " ";
  cout << " └────┘ " << endl;
  if (showTime)
    cout << "Execute at t= " << getTime() << endl;
}