/****************************************************************************
  FileName     [ apArgGroup.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Definitions for argument groups of ArgumentParser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "argparse.h"

namespace ArgParse {

ArgumentGroup ArgumentParser::addMutuallyExclusiveGroup() {
    _pimpl->mutuallyExclusiveGroups.emplace_back(*this);
    return _pimpl->mutuallyExclusiveGroups.back();
}

}  // namespace ArgParse