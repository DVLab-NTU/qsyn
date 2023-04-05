/****************************************************************************
  FileName     [ util.cpp ]
  PackageName  [ util ]
  Synopsis     [ Define global utility functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "util.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "myUsage.h"  // for MyUsage
#include "rnGen.h"    // for RandomNumGen

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

size_t intPow(size_t base, size_t n) {
    if (n == 0) return 1;
    if (n == 1) return base;
    size_t tmp = intPow(base, n / 2);
    if (n % 2 == 0)
        return tmp * tmp;
    else
        return base * tmp * tmp;
}

TqdmWrapper::TqdmWrapper(size_t total, bool show)
    : _counter(0), _total(total), _tqdm(make_unique<tqdm>(show)) {}

TqdmWrapper::TqdmWrapper(int total, bool show) : TqdmWrapper(static_cast<size_t>(total), show) {}

TqdmWrapper::~TqdmWrapper() {
    _tqdm->finish();
}

void TqdmWrapper::add() {
    _tqdm->progress(_counter++, _total);
}