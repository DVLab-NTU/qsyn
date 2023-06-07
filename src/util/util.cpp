/****************************************************************************
  FileName     [ util.cpp ]
  PackageName  [ util ]
  Synopsis     [ Define global utility functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "util.h"

#include <cerrno>
#include <cstring>
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

/**
 * @brief
 *
 * @param prefix the filename prefix
 * @param dir the directory to search
 * @return vector<string>
 */
vector<string> listDir(string const& prefix, string const& dir) {
    vector<string> files;

    namespace fs = std::filesystem;

    if (!fs::exists(dir)) {
        cerr << "Error: failed to open " << dir << "!!\n";
        return files;
    }

    for (auto& entry : fs::directory_iterator(dir)) {
        if (prefix.empty() || string(entry.path().filename()).compare(0, prefix.size(), prefix) == 0) {
            files.push_back(entry.path().filename());
        }
    }

    sort(files.begin(), files.end());

    return files;
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

/**
 * @brief Create a temp directory with a name starting with `prefix`.
 *
 * @param prefix
 * @return std::string the temp directory name starting with `prefix` followed by six random characters.
 */
std::string createTempDir(std::string const& prefix) {
    // NOTE - All-cap to avoid clashing with keyword
    char* TEMPLATE = new char[prefix.size() + 7];  // 6 for the Xs, 1 for null terminator
    prefix.copy(TEMPLATE, prefix.size());
    for (int i = 0; i < 6; i++) TEMPLATE[prefix.size() + i] = 'X';

    IGNORE_UNUSED_RETURN_WARNING mkdtemp(TEMPLATE);

    if (errno) {
        cerr << "Error: " << std::strerror(errno) << endl;
        return "";
    }

    string ret{TEMPLATE};

    delete[] TEMPLATE;

    return ret;
}

/**
 * @brief Create a temp file with a name starting with `prefix`.
 *
 * @param prefix
 * @return std::string the temp file name starting with `prefix` followed by six random characters.
 */
std::string createTempFile(std::string const& prefix) {
    char* TEMPLATE = new char[prefix.size() + 7];  // 6 for the Xs, 1 for null terminator
    prefix.copy(TEMPLATE, prefix.size());
    for (int i = 0; i < 6; i++) TEMPLATE[prefix.size() + i] = 'X';

    IGNORE_UNUSED_RETURN_WARNING mkstemp(TEMPLATE);

    if (errno) {
        cerr << "Error: " << std::strerror(errno) << endl;
        return "";
    }

    string ret{TEMPLATE};

    delete[] TEMPLATE;

    return ret;
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