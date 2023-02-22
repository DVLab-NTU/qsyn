/****************************************************************************
  FileName     [ myTrie.h ]
  PackageName  [ util ]
  Synopsis     [ User-defined trie data structure for parsing ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_MY_TRIE_H
#define QSYN_MY_TRIE_H

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct MyTrieNode {
#define NUM_ASCII_CHARS 128
public:
    MyTrieNode() : isWord(false), frequency(0) {}
    std::unordered_map<char, std::unique_ptr<MyTrieNode>> children;
    bool isWord;
    size_t frequency;
};

class MyTrie {
public:
    MyTrie() : _root(std::make_unique<MyTrieNode>()) {}

    void clear() { _root = std::make_unique<MyTrieNode>(); }
    bool insert(std::string const& word);
    std::optional<std::string> shortestUniquePrefix(std::string const& word) const;
    size_t frequency(std::string const& word) const;

    std::optional<std::string> findWithPrefix(std::string const& word) const;

private:
    std::unique_ptr<MyTrieNode> _root;
};

#endif  // QSYN_MY_TRIE_H