/****************************************************************************
  FileName     [ trie.h ]
  PackageName  [ util ]
  Synopsis     [ User-defined trie data structure for parsing ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dvlab_utils {

struct TrieNode {
public:
    TrieNode() : isWord(false), frequency(0) {}
    ~TrieNode() = default;

    TrieNode(TrieNode const& other) : isWord{other.isWord}, frequency{other.frequency} {
        for (auto&& [ch, node] : other.children) {
            children.emplace(ch, std::make_unique<TrieNode>(*node));
        }
    }

    TrieNode(TrieNode&&) noexcept = default;

    TrieNode& operator=(TrieNode copy) {
        copy.swap(*this);
        return *this;
    }

    void swap(TrieNode& other) {
        using std::swap;
        swap(children, other.children);
        swap(isWord, other.isWord);
        swap(frequency, other.frequency);
    }

    friend void swap(TrieNode& a, TrieNode& b) {
        a.swap(b);
    }

    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    bool isWord;
    size_t frequency;
};

template <typename It>
concept StringRetrivable = requires {
    std::input_iterator<It>;
    std::convertible_to<typename std::iterator_traits<It>::value_type, std::string>;
};

static_assert(StringRetrivable<std::vector<std::string>::iterator>);
static_assert(StringRetrivable<std::vector<std::string>::const_iterator>);

class Trie {
public:
    Trie() : _root(std::make_unique<TrieNode>()) {}

    template <typename InputIt>
    requires StringRetrivable<InputIt>
    Trie(InputIt first, InputIt last) : _root(std::make_unique<TrieNode>()) {
        for (; first != last; ++first) {
            insert(*first);
        }
    }

    ~Trie() = default;

    Trie(Trie const& other) : _root{std::make_unique<TrieNode>(*other._root)} {}

    Trie(Trie&&) noexcept = default;
    Trie& operator=(Trie copy) {
        copy.swap(*this);
        return *this;
    }

    void swap(Trie& other) {
        using std::swap;
        swap(_root, other._root);
    }

    friend void swap(Trie& a, Trie& b) {
        a.swap(b);
    }

    void clear() { _root = std::make_unique<TrieNode>(); }
    bool insert(std::string const& word);
    std::optional<std::string> shortestUniquePrefix(std::string const& word) const;
    size_t frequency(std::string const& word) const;

    std::optional<std::string> findWithPrefix(std::string const& word) const;

private:
    std::unique_ptr<TrieNode> _root;
};

}  // namespace dvlab_utils
