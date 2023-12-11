/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define ordered_hashmap ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

/********************** Summary of this data structure **********************
 *
 *     ordered_hashmap is designed to behave like a c++ unordered_map, except
 * that the elements are ordered. In other words, ordered_hashmap behave like
 * Python 3.7+ `dict`.
 *
 * Important features:
 * - O(1) insertion (amortized)
 * - O(1) deletion  (amortized)
 * - O(1) lookup/update
 * - Elements are stored in the order of insertion.
 * - bidirectional iterator
 *
 * How does ordered_hashmap work?
 *     ordered_hashmap is composed of a linear storage of key-value pairs and
 * a vanilla hash map that keeps tracks of the correspondence between key and
 * storage id. In our implementation, these two containers are implemented us-
 * ing std::vector and std::unordered_map.
 *
 *       hash map
 *     +------+----+
 *     | key  | id |
 *     +------+----+                   linear storage
 *     |      |    |             +-----+---------+--------+
 *     +------+----+             | id  | key     | value  |
 *     | key2 | 2  | ------+     +-----+---------+--------+
 *     +------+----+    +--|---> | 0   | key0    | value0 |
 *     |      |    |    |  |     +-----+---------+--------+
 *     +------+----+    |  |     | 1   | NONE --  Deleted |
 *     |      |    |    |  |     +-----+---------+--------+
 *     +------+----+    |  +---> | 2   | key2    | value2 |
 *     | key0 | 0  | ---+        +-----+---------+--------+
 *     +------+----+       +---> | 3   | key3    | value3 |
 *     |      |    |       |     +-----+---------+--------+
 *     +------+----+       |
 *     | key3 | 3  | ------+
 *     +------+----+
 *
 *     On traversal, we can just traverse through the linear storage, so that
 * we visit each element by the order of insertion. On the other hand, lookups
 * and updatings are just as simple: the hash map provides easy O(1) access.
 *
 *     Insertion is performed by appending to the linear storage and creating
 * a key-to-id map in the hash map. Deletion can be implemented similarly, ex-
 * cept that, to avoid O(n) deletion of the linear storage, we only mark the
 * item as deleted without actually changing the size of the linear storage.
 * If half of the data are marked deleted, a batch deletion is performed to
 * sweep the linear storage, so that the traversal stays efficient.
 *
 * Caveats:
 * 1.  As ordered_hashset automatically manages the size of its internal stor-
 *     age, iterators may be invalidated upon insertion/deletion. Consequently,
 *     one should not perform insertion/deletion during traversal. If this must
 *     be done, it is suggested to collect the keys to another containers, and
 *     then perform the insertion/deletion during traversal of another container.
 *
 * 2.  As ordered_hashset::iterator is not random access iterator, it cannot be
 *     sorted using std::sort; please use ordered_hashset::sort.
 *
 ****************************************************************************/

#pragma once

#include <utility>

#include "util/ordered_hashtable.hpp"

namespace dvlab {

namespace utils {

template <typename Key, typename T, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class ordered_hashmap final : public ordered_hashtable<Key, std::pair<Key const, T>, std::pair<Key, T>, Hash, KeyEqual> {  // NOLINT(readability-identifier-naming) : ordered_hashmap intentionally mimics std::unordered_map
    using _Table_t = ordered_hashtable<Key, std::pair<Key const, T>, std::pair<Key, T>, Hash, KeyEqual>;

public:
    using key_type        = typename _Table_t::key_type;
    using mapped_type     = T;
    using value_type      = typename _Table_t::value_type;
    using stored_type     = typename _Table_t::stored_type;
    using size_type       = typename _Table_t::size_type;
    using difference_type = typename _Table_t::difference_type;
    using hasher          = typename _Table_t::hasher;
    using key_equal       = typename _Table_t::key_equal;
    using iterator        = typename _Table_t::iterator;
    using const_iterator  = typename _Table_t::const_iterator;

    ordered_hashmap() : _Table_t() {}
    ordered_hashmap(std::initializer_list<value_type> const& il) : _Table_t() {
        for (value_type const& item : il) {
            this->_key2id.emplace(key(item), this->_data.size());
            this->_data.emplace_back(item);
        }
        this->_size = il.size();
    }

    template <typename InputIt>
    ordered_hashmap(InputIt first, InputIt last) {
        for (auto it = first; it != last; ++it) {
            this->_key2id.emplace(key(*(it)), this->_data.size());
            this->_data.emplace_back(*(it));
        }
        this->_size = this->_data.size();
    }

    // lookup
    Key const& key(stored_type const& value) const override { return value.first; }

    T& at(Key const& key);
    T const& at(Key const& key) const;

    T& operator[](Key const& key);
    T& operator[](Key&& key);

    /**
     * @brief If the key does not exist, emplace a key-value pair to the ordered hashmap in place.
     *        Otherwise, do nothing.
     *
     *
     * @param value
     * @return std::pair<OrderedHashmap::iterator, bool>. If emplacement succeeds,
     *         the pair consists of an iterator to the emplaced pair and `true`;
     *         otherwise, the pair consists of `this->end()` and `false`.
     *
     */
    template <typename... Args>
    std::pair<iterator, bool> try_emplace(Key&& key, Args&&... args) {
        auto itr = this->find(key);
        if (itr != this->end()) {
            return std::make_pair(itr, false);
        }
        this->_data.emplace_back(value_type(key, std::forward<Args>(args)...));
        this->_key2id.emplace(std::move(key), this->_data.size() - 1);
        this->_size++;

        return std::make_pair(this->find(std::move(key)), true);
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(Key const& key, Args&&... args) {
        auto itr = this->find(key);
        if (itr != this->end()) {
            return std::make_pair(itr, false);
        }
        this->_data.emplace_back(value_type(key, std::forward<Args>(args)...));
        this->_key2id.emplace(key, this->_data.size() - 1);
        this->_size++;

        return std::make_pair(this->find(key), true);
    }

    std::pair<iterator, bool> insert_or_assign(Key&& key, T&& obj) {
        auto ret = try_emplace(std::move(key), std::move(obj));
        if (ret.second == false) {
            ret.first->second = std::move(obj);
        }
        return ret;
    }

    std::pair<iterator, bool> insert_or_assign(Key const& key, T&& obj) {
        auto ret = try_emplace(key, std::move(obj));
        if (ret.second == false) {
            ret.first->second = std::move(obj);
        }
        return ret;
    }
};

static_assert(std::ranges::bidirectional_range<ordered_hashmap<int, int>>);

//------------------------------------------------------
//  lookup
//------------------------------------------------------

/**
 * @brief Return a const reference to the value associated with the given key.
 *        If no such element exists, throw an std::out_of_range exception.
 *
 * @param key
 * @return T&
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
T& ordered_hashmap<Key, T, Hash, KeyEqual>::at(Key const& key) {
    if (!this->contains(key)) {
        throw std::out_of_range("no value corresponding to the key");
    }
    return this->_data[this->id(key)].value().second;
}

/**
 * @brief Return a const reference to the value associated with the given key.
 *        tf no such element exists, throw an std::out_of_range exception.
 *
 * @param key
 * @return cosnt T&
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
T const& ordered_hashmap<Key, T, Hash, KeyEqual>::at(Key const& key) const {
    if (!this->contains(key)) {
        throw std::out_of_range("no value corresponding to the key");
    }
    return this->_data[this->id(key)].value().second;
}

/**
 * @brief Return a reference to the value associated with the given key.
 *        If no such element exists, create one with the default constructor of T.
 *
 * @param key
 * @return T&
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
T& ordered_hashmap<Key, T, Hash, KeyEqual>::operator[](Key const& key) {
    return at(std::move(key));
}

/**
 * @brief Return a reference to the value associated with the given key.
 *        If no such element exists, create one with the default constructor of T.
 *
 * @param key
 * @return T&
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
T& ordered_hashmap<Key, T, Hash, KeyEqual>::operator[](Key&& key) {
    if (!this->contains(std::move(key))) this->emplace(std::move(key), T());
    return at(std::move(key));
}

}  // namespace utils

}  // namespace dvlab
