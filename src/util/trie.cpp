/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Trie data structure for prefix-based lookups ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./trie.hpp"

#include <cassert>

#include "fmt/core.h"

namespace dvlab {

namespace utils {

bool Trie::insert(std::string_view word) {
    auto itr = _root.get();

    assert(itr != nullptr);

    for (auto& ch : word) {
        if (!itr->children.contains(ch)) {
            itr->children.emplace(ch, std::make_unique<TrieNode>());
        }
        itr = itr->children.at(ch).get();
        itr->frequency++;
    }
    if (itr->is_word) return false;

    itr->is_word = true;
    return true;
}

bool Trie::erase(std::string_view word) {
    if (!contains(word)) return false;
    auto itr = _root.get();

    assert(itr != nullptr);
    for (auto& ch : word) {
        itr = itr->children.at(ch).get();
        assert(itr != nullptr);
        itr->frequency--;
    }

    assert(itr->is_word);

    itr->is_word = false;

    itr = _root.get();

    for (auto& ch : word) {
        if (itr->children.at(ch)->frequency == 0) {
            itr->children.erase(ch);
            break;
        }
        itr = itr->children.at(ch).get();
        assert(itr != nullptr);
    }

    return true;
}

bool Trie::contains(std::string_view word) const {
    auto itr = _root.get();

    assert(itr != nullptr);

    for (auto& ch : word) {
        if (!itr->children.contains(ch)) return false;
        itr = itr->children.at(ch).get();
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
std::string Trie::shortest_unique_prefix(std::string_view word) const {
    auto itr = _root.get();

    assert(itr != nullptr);

    size_t pos = 0;
    for (auto& ch : word) {
        pos++;
        if (!itr->children.contains(ch)) break;
        itr = itr->children.at(ch).get();
        assert(itr != nullptr);
        if (itr->frequency == 1) break;
    }

    return std::string{word.begin(), word.begin() + pos};
}

size_t Trie::frequency(std::string_view prefix) const {
    auto itr = _root.get();

    assert(itr != nullptr);

    for (auto& ch : prefix) {
        if (!itr->children.contains(ch)) return 0;
        itr = itr->children.at(ch).get();
        assert(itr != nullptr);
    }

    return itr->frequency;
}

std::optional<std::string> Trie::find_with_prefix(std::string_view prefix) const {
    auto itr = _root.get();

    assert(itr != nullptr);
    std::string ret_str = "";

    for (auto& ch : prefix) {
        if (!itr->children.contains(ch)) return std::nullopt;
        itr = itr->children.at(ch).get();
        assert(itr != nullptr);
        ret_str.push_back(ch);
    }

    if (itr->frequency > 1) {
        return (itr->is_word) ? std::make_optional<std::string>(ret_str) : std::nullopt;
    }

    while (!itr->is_word) {
        assert(!itr->children.empty());
        ret_str.push_back(itr->children.begin()->first);
        itr = itr->children.begin()->second.get();
    }

    return ret_str;
}

namespace {

void find_all_with_prefix_helper(TrieNode const* itr, std::vector<std::string>& ret, std::string& return_str) {
    if (itr->is_word) ret.push_back(return_str);

    for (auto& [ch, child] : itr->children) {
        return_str.push_back(ch);
        find_all_with_prefix_helper(child.get(), ret, return_str);
        return_str.pop_back();
    }
}

}  // namespace

std::vector<std::string> Trie::find_all_with_prefix(std::string_view prefix) const {
    auto itr = _root.get();

    assert(itr != nullptr);
    std::string ret_str = "";

    for (auto& ch : prefix) {
        if (!itr->children.contains(ch)) return {};
        itr = itr->children.at(ch).get();
        assert(itr != nullptr);
        ret_str.push_back(ch);
    }

    std::vector<std::string> ret;

    find_all_with_prefix_helper(itr, ret, ret_str);

    return ret;
}

}  // namespace utils

}  // namespace dvlab
