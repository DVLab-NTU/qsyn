/****************************************************************************
  FileName     [ trie.cpp ]
  PackageName  [ util ]
  Synopsis     [ User-defined trie data structure for parsing ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./trie.hpp"

#include <cassert>

using namespace std;

namespace dvlab {

namespace utils {

bool Trie::insert(string const& word) {
    auto itr = _root.get();

    assert(itr != nullptr);

    for (auto& ch : word) {
        size_t idx = ch;
        if (!itr->children.contains(idx)) {
            itr->children.emplace(idx, make_unique<TrieNode>());
        }
        itr = itr->children.at(idx).get();
        itr->frequency++;
    }
    if (itr->isWord) return false;

    itr->isWord = true;
    return true;
}

bool Trie::erase(std::string const& word) {
    if (!contains(word)) return false;
    auto itr = _root.get();

    assert(itr != nullptr);
    for (auto& ch : word) {
        size_t idx = ch;
        itr = itr->children.at(idx).get();
        assert(itr != nullptr);
        itr->frequency--;
    }

    assert(itr->isWord);

    itr->isWord = false;

    itr = _root.get();

    for (auto& ch : word) {
        size_t idx = ch;
        if (itr->children.at(idx)->frequency == 0) {
            itr->children.erase(idx);
            break;
        }
        itr = itr->children.at(idx).get();
        assert(itr != nullptr);
    }

    return true;
}

bool Trie::contains(std::string const& word) const {
    auto itr = _root.get();

    assert(itr != nullptr);

    for (auto& ch : word) {
        size_t idx = ch;
        if (!itr->children.contains(idx)) return false;
        itr = itr->children.at(idx).get();
        assert(itr != nullptr);
    }

    return itr->isWord;
}

/**
 * @brief Find the shortest unique prefix of a word in the trie. If the word is not in the trie, return as if it were in the trie.
 *
 * @param word
 * @return string
 */
string Trie::shortestUniquePrefix(string const& word) const {
    auto itr = _root.get();

    assert(itr != nullptr);

    size_t pos = 0;
    for (auto& ch : word) {
        pos++;
        size_t idx = ch;
        if (!itr->children.contains(idx)) break;
        itr = itr->children.at(idx).get();
        assert(itr != nullptr);
        if (itr->frequency == 1) break;
    }

    return word.substr(0, pos);
}

size_t Trie::frequency(string const& prefix) const {
    auto itr = _root.get();

    assert(itr != nullptr);

    for (auto& ch : prefix) {
        size_t idx = ch;
        if (!itr->children.contains(idx)) return 0;
        itr = itr->children.at(idx).get();
        assert(itr != nullptr);
    }

    return itr->frequency;
}

optional<string> Trie::findWithPrefix(string const& prefix) const {
    auto itr = _root.get();

    assert(itr != nullptr);
    string retStr = "";

    for (auto& ch : prefix) {
        size_t idx = ch;
        if (!itr->children.contains(idx)) return nullopt;
        itr = itr->children.at(idx).get();
        assert(itr != nullptr);
        retStr.push_back(ch);
    }

    if (itr->frequency > 1) {
        return (itr->isWord) ? std::make_optional<string>(retStr) : nullopt;
    }

    while (!itr->isWord) {
        assert(!itr->children.empty());
        retStr.push_back(itr->children.begin()->first);
        itr = itr->children.begin()->second.get();
    }

    return retStr;
}

namespace detail {

void findAllStringsWithPrefixHelper(TrieNode const* itr, vector<string>& ret, string& retStr) {
    if (itr->isWord) ret.push_back(retStr);

#ifndef NDEBUG
    std::string copy = retStr;
#endif
    for (auto& [ch, child] : itr->children) {
        retStr.push_back(ch);
        findAllStringsWithPrefixHelper(child.get(), ret, retStr);
        retStr.pop_back();
    }
#ifndef NDEBUG
    assert(copy == retStr);
#endif
}

}  // namespace detail

vector<string> Trie::findAllStringsWithPrefix(string const& prefix) const {
    auto itr = _root.get();

    assert(itr != nullptr);
    string retStr = "";

    for (auto& ch : prefix) {
        size_t idx = ch;
        if (!itr->children.contains(idx)) return {};
        itr = itr->children.at(idx).get();
        assert(itr != nullptr);
        retStr.push_back(ch);
    }

    vector<string> ret;

    detail::findAllStringsWithPrefixHelper(itr, ret, retStr);

    return ret;
}

}  // namespace utils

}  // namespace dvlab