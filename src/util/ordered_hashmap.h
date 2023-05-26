/****************************************************************************
  FileName     [ ordered_hashmap.h ]
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

#ifndef ORDERED_HASHMAP_H
#define ORDERED_HASHMAP_H

#include "ordered_hashtable.h"

template <typename Key, typename T, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class ordered_hashmap final : public ordered_hashtable<Key, std::pair<const Key, T>, std::pair<Key, T>, Hash, KeyEqual> {
    using __OrderedHashTable = ordered_hashtable<Key, std::pair<const Key, T>, std::pair<Key, T>, Hash, KeyEqual>;

public:
    using key_type = typename __OrderedHashTable::key_type;
    using mapped_type = T;
    using value_type = typename __OrderedHashTable::value_type;
    using stored_type = typename __OrderedHashTable::stored_type;
    using size_type = typename __OrderedHashTable::size_type;
    using difference_type = typename __OrderedHashTable::difference_type;
    using hasher = typename __OrderedHashTable::hasher;
    using key_equal = typename __OrderedHashTable::key_equal;
    using iterator = typename __OrderedHashTable::iterator;
    using const_iterator = typename __OrderedHashTable::const_iterator;

    ordered_hashmap() noexcept : __OrderedHashTable() {}
    ordered_hashmap(const std::initializer_list<value_type>& il) noexcept : __OrderedHashTable() {
        for (const value_type& item : il) {
            this->_key2id.emplace(key(item), this->_data.size());
            this->_data.emplace_back(item);
        }
        this->_size = il.size();
    }

    // lookup
    virtual const Key& key(const stored_type& value) const override { return value.first; }

    T& at(const Key& key);
    const T& at(const Key& key) const;

    T& operator[](const Key& key);
    T& operator[](Key&& key);
};

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
T& ordered_hashmap<Key, T, Hash, KeyEqual>::at(const Key& key) {
    return const_cast<T&>(std::as_const(*this).at(key));
}

/**
 * @brief Return a const reference to the value associated with the given key.
 *        tf no such element exists, throw an std::out_of_range exception.
 *
 * @param key
 * @return cosnt T&
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
const T& ordered_hashmap<Key, T, Hash, KeyEqual>::at(const Key& key) const {
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
T& ordered_hashmap<Key, T, Hash, KeyEqual>::operator[](const Key& key) {
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
    if (!this->contains(key)) this->emplace(key, T());
    return at(key);
}

#endif  // ORDERED_HASHMAP_H