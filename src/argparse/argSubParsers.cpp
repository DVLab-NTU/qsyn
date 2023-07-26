/****************************************************************************
  FileName     [ apSubParsers.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Definitions for subparsers of ArgumentParser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "argparse.h"

namespace ArgParse {

ArgumentParser SubParsers::addParser(std::string const& n) {
    _pimpl->subparsers.emplace(toLowerString(n), ArgumentParser{n});
    return _pimpl->subparsers.at(toLowerString(n));
}

SubParsers ArgumentParser::addSubParsers() {
    if (_pimpl->subparsers.has_value()) {
        std::cerr << "Error: An ArgumentParser can have only one set of subparsers!!" << std::endl;
        exit(-1);
    }
    _pimpl->subparsers = SubParsers{};
    return _pimpl->subparsers.value();
}

}  // namespace ArgParse