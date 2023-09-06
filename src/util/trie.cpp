/****************************************************************************
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
    if (itr->is_word) return false;

    itr->is_word = true;
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

    assert(itr->is_word);

    itr->is_word = false;

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

    return itr->is_word;
}

/**
 * @brief Find the shortest unique prefix of a word in the trie. If the word is not in the trie, return as if it were in the trie.
 *
 * @param word
 * @return string
 */
string Trie::shortest_unique_prefix(string const& word) const {
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

optional<string> Trie::find_with_prefix(string const& prefix) const {
    auto itr = _root.get();

    assert(itr != nullptr);
    string ret_str = "";

    for (auto& ch : prefix) {
        size_t idx = ch;
        if (!itr->children.contains(idx)) return nullopt;
        itr = itr->children.at(idx).get();
        assert(itr != nullptr);
        ret_str.push_back(ch);
    }

    if (itr->frequency > 1) {
        return (itr->is_word) ? std::make_optional<string>(ret_str) : nullopt;
    }

    while (!itr->is_word) {
        assert(!itr->children.empty());
        ret_str.push_back(itr->children.begin()->first);
        itr = itr->children.begin()->second.get();
    }

    return ret_str;
}

namespace detail {

void find_all_with_prefix_helper(TrieNode const* itr, vector<string>& ret, string& return_str) {
    if (itr->is_word) ret.push_back(return_str);

#ifndef NDEBUG
    std::string copy = return_str;
#endif
    for (auto& [ch, child] : itr->children) {
        return_str.push_back(ch);
        find_all_with_prefix_helper(child.get(), ret, return_str);
        return_str.pop_back();
    }
#ifndef NDEBUG
    assert(copy == return_str);
#endif
}

}  // namespace detail

vector<string> Trie::find_all_with_prefix(string const& prefix) const {
    auto itr = _root.get();

    assert(itr != nullptr);
    string ret_str = "";

    for (auto& ch : prefix) {
        size_t idx = ch;
        if (!itr->children.contains(idx)) return {};
        itr = itr->children.at(idx).get();
        assert(itr != nullptr);
        ret_str.push_back(ch);
    }

    vector<string> ret;

    detail::find_all_with_prefix_helper(itr, ret, ret_str);

    return ret;
}

}  // namespace utils

}  // namespace dvlab