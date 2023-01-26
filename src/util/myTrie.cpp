/****************************************************************************
  FileName     [ myTrie.cpp ]
  PackageName  [ util ]
  Synopsis     [ User-defined trie data structure for parsing ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "myTrie.h"

#include <cassert>

using namespace std;

bool MyTrie::insert(string const& word) {
    auto itr = _root.get();

    assert(itr != nullptr);

    for (auto& ch : word) {
        size_t idx = ch;
        if (itr->children[idx].get() == nullptr) {
            itr->children[idx] = make_unique<MyTrieNode>();
        } 
        itr = itr->children[idx].get();
        itr->frequency++;
    }
    if (itr->isWord) return false;

    itr->isWord = true;
    return true;
}

std::optional<std::string> MyTrie::shortestUniquePrefix(std::string const& word) const {
    auto itr = _root.get();

    assert(itr != nullptr);

    size_t pos = 0;
    for (auto& ch : word) {
        pos++;
        itr = itr->children[(size_t) ch].get();
        if (itr == nullptr) return std::nullopt;
        if (itr->frequency == 1) break;
    }


    return word.substr(0, pos);
}