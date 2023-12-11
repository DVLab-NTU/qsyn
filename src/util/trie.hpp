/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Trie data structure for prefix-based lookups ]
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

struct TrieNode {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    TrieNode() {}
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

    void swap(TrieNode& other) noexcept {
        std::swap(children, other.children);
        std::swap(is_word, other.is_word);
        std::swap(frequency, other.frequency);
    }

    friend void swap(TrieNode& a, TrieNode& b) noexcept {
        a.swap(b);
    }

    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    bool is_word     = false;
    size_t frequency = 0;
};

template <typename It>
concept string_retrivable = requires {
    requires std::input_iterator<It>;
    requires std::convertible_to<typename std::iterator_traits<It>::value_type, std::string>;
};

static_assert(string_retrivable<std::vector<std::string>::iterator>);
static_assert(string_retrivable<std::vector<std::string>::const_iterator>);

class Trie {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
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

    void swap(Trie& other) noexcept {
        std::swap(_root, other._root);
    }

    friend void swap(Trie& a, Trie& b) noexcept {
        a.swap(b);
    }

    void clear() { _root = std::make_unique<TrieNode>(); }
    bool insert(std::string_view word);
    bool erase(std::string_view word);
    bool contains(std::string_view word) const;
    bool empty() const { return _root->children.empty(); }
    size_t frequency(std::string_view prefix) const;

    std::string shortest_unique_prefix(std::string_view word) const;
    std::optional<std::string> find_with_prefix(std::string_view prefix) const;
    std::vector<std::string> find_all_with_prefix(std::string_view prefix) const;

private:
    std::unique_ptr<TrieNode> _root;
};

}  // namespace utils

}  // namespace dvlab
