/****************************************************************************
  FileName     [ util.cpp ]
  PackageName  [ util ]
  Synopsis     [ Define global utility functions ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2017-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "myUsage.h"
#include "rnGen.h"

using namespace std;

//----------------------------------------------------------------------
//    Global variables in util
//----------------------------------------------------------------------

RandomNumGen rnGen(0);  // use random seed = 0
MyUsage myUsage;

//----------------------------------------------------------------------
//    Global functions in util
//----------------------------------------------------------------------
//
// List all the file names under "dir" with prefix "prefix"
// Ignore "." and ".."
//
int listDir(vector<string>& files, const string& prefix, const string& dir = ".") {
    namespace fs = std::filesystem;
    if (!fs::exists(dir)) {
        cerr << "Error(" << errno << "): failed to open " << dir << "!!\n";
        return errno;
    }

    for (auto& entry : fs::directory_iterator(dir)) {
        if (prefix.empty() || string(entry.path().filename()).compare(0, prefix.size(), prefix) == 0) {
            files.push_back(entry.path().filename());
        }
    }

    sort(files.begin(), files.end());
    
    return 0;
}

size_t getHashSize(size_t s) {
    if (s < 8) return 7;
    if (s < 16) return 13;
    if (s < 32) return 31;
    if (s < 64) return 61;
    if (s < 128) return 127;
    if (s < 512) return 509;
    if (s < 2048) return 1499;
    if (s < 8192) return 4999;
    if (s < 32768) return 13999;
    if (s < 131072) return 59999;
    if (s < 524288) return 100019;
    if (s < 2097152) return 300007;
    if (s < 8388608) return 900001;
    if (s < 33554432) return 1000003;
    if (s < 134217728) return 3000017;
    if (s < 536870912) return 5000011;
    return 7000003;
}

size_t intPow(size_t base, size_t n) {
    if (n == 0) return 1;
    if (n == 1) return base;
    size_t tmp = intPow(base, n / 2);
    if (n % 2 == 0)
        return tmp * tmp;
    else
        return base * tmp * tmp;
}
