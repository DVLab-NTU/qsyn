/****************************************************************************
  FileName     [ rnGen.h ]
  PackageName  [ util ]
  Synopsis     [ Random number generator ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef RN_GEN_H
#define RN_GEN_H

#include <limits.h>
#include <stdlib.h>

#define my_srandom srandom
#define my_random random

class RandomNumGen {
public:
    RandomNumGen() { my_srandom(getpid()); }
    RandomNumGen(unsigned seed) { my_srandom(seed); }
    const int operator()(const int range) const {
        return int(range * (double(my_random()) / INT_MAX));
    }
};

#endif  // RN_GEN_H
