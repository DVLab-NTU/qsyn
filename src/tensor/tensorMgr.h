/****************************************************************************
  FileName     [ tensorMgr.h ]
  PackageName  [ tensor ]
  Synopsis     [ Define class TensorMgr structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef TENSOR_MGR_H
#define TENSOR_MGR_H

#include <iosfwd>
#include <map>
#include <string>

#include "phase.h"

class TensorMgr;  // lines 17-17
template <typename T>

class QTensor;  // lines 19-20

extern TensorMgr* tensorMgr;

struct TensorInfo {
    QTensor<double>* tensor;
    std::string info;

    friend std::ostream& operator<<(std::ostream& os, const TensorInfo& tsInfo);
};

using TensorList = std::map<size_t, TensorInfo>;

class TensorMgr {
public:
    TensorMgr() {}

    void reset();

    const TensorList& getTensorList() const { return _tensorList; }
    QTensor<double>* getTensor(const size_t& id) const { return _tensorList.at(id).tensor; }
    const std::string& getInfo(const size_t& id) const { return _tensorList.at(id).info; }

    void setTensor(const size_t& id, QTensor<double>* tensor) { _tensorList[id].tensor = tensor; }
    void setInfo(const size_t& id, const std::string& str = "") { _tensorList[id].info = str; }

    QTensor<double>* addTensor(const size_t& id, const std::string& str);
    void removeTensor(const size_t& id);

    void printTensorMgr() const;
    void printTensor(const size_t& id, bool brief) const;

    size_t nextID();
    bool hasId(const size_t& id) const { return _tensorList.contains(id); }

    bool isEquivalent(const size_t& id1, const size_t& id2, const double& eps = 1e-6) const;

    double getGlobalNorm(const size_t& id1, const size_t& id2) const;
    Phase getGlobalPhase(const size_t& id1, const size_t& id2) const;

    void adjoint(const size_t& id);

private:
    TensorList _tensorList;
};

#endif  // TENSOR_MGR_H