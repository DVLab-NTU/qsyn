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

#include "util/dataStructureManager.hpp"
#include "util/phase.hpp"

template <typename T>
class QTensor;

using TensorMgr = dvlab_utils::DataStructureManager<QTensor<double>>;

#endif  // TENSOR_MGR_H