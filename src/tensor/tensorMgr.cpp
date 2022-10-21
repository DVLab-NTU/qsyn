/****************************************************************************
  FileName     [ tensorMgr.cpp ]
  PackageName  [ tensor ]
  Synopsis     [ Define tensor manager ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "tensorMgr.h"
#include <iostream>
#include <algorithm>

using namespace std;

extern size_t verbose;

TensorMgr* tensorMgr = 0;

void TensorMgr::reset() {
    _tensorList.clear();
}

QTensor<double>* TensorMgr::addTensor(const size_t& id, const string& str = "") {
    if (_tensorList.find(id) != _tensorList.end()) {
        return nullptr;
    }
    _tensorList[id] = {new QTensor<double>(), str};
    if (verbose >= 5) {
        cout << "Successfully added Tensor " << id << endl;
    }
    return _tensorList[id].tensor;
}

void TensorMgr::removeTensor(const size_t& id) {
    if (_tensorList.find(id) == _tensorList.end()) {
        cerr << "[Error] The id provided does not exist!!" << endl;
    }
    if (_tensorList[id].tensor) {
        delete _tensorList[id].tensor;
    }
    _tensorList.erase(id);

    if (verbose >= 5) cout << "Successfully removed Tensor " << id << endl;

    return;
}

void TensorMgr::printTensorMgr() const {
    for (const auto& [id, tsInfo] : _tensorList) {
        cout << "Tensor " << setw(4) << right << id << ": " << tsInfo << endl;
    }
    cout << "Total #Tensor: " << _tensorList.size() << endl;
}

void TensorMgr::printTensor(const size_t& id) const {
    cout << *getTensor(id) << "\n"
         << _tensorList.at(id) << endl;
}

size_t TensorMgr::nextID() {
    size_t id = 0;
    while (_tensorList.find(id) != _tensorList.end()) ++id;
    return id;
}