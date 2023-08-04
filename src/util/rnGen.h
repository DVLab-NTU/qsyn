/****************************************************************************
  FileName     [ rnGen.h ]
  PackageName  [ util ]
  Synopsis     [ Random number generator ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef RN_GEN_H
#define RN_GEN_H

#include <climits>
#include <cstdlib>

class RandomNumGen {
public:
    RandomNumGen() { srandom(getpid()); }
    RandomNumGen(unsigned seed) { srandom(seed); }
    const int operator()(const int range) const {
        return int(range * (double(random()) / INT_MAX));
    }
};

#endif  // RN_GEN_H
