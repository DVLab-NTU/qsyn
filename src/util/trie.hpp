/****************************************************************************
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

namespace dvlab {

namespace utils {

struct TrieNode {
public:
    TrieNode() : is_word(false), frequency(0) {}
    ~TrieNode() = default;

    TrieNode(TrieNode const& other) : is_word{other.is_word}, frequency{other.frequency} {
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
        swap(is_word, other.is_word);
        swap(frequency, other.frequency);
    }

    friend void swap(TrieNode& a, TrieNode& b) {
        a.swap(b);
    }

    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    bool is_word;
    size_t frequency;
};

template <typename It>
concept string_retrivable = requires {
    std::input_iterator<It>;
    std::convertible_to<typename std::iterator_traits<It>::value_type, std::string>;
};

static_assert(string_retrivable<std::vector<std::string>::iterator>);
static_assert(string_retrivable<std::vector<std::string>::const_iterator>);

class Trie {
public:
    Trie() : _root(std::make_unique<TrieNode>()) {}

    template <typename InputIt>
    requires string_retrivable<InputIt>
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
    bool erase(std::string const& word);
    bool contains(std::string const& word) const;
    bool empty() const { return _root->children.empty(); }
    size_t frequency(std::string const& prefix) const;

    std::string shortest_unique_prefix(std::string const& word) const;
    std::optional<std::string> find_with_prefix(std::string const& prefix) const;
    std::vector<std::string> find_all_with_prefix(std::string const& prefix) const;

private:
    std::unique_ptr<TrieNode> _root;
};

}  // namespace utils

}  // namespace dvlab