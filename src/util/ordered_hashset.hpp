/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define ordered_hashset ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

/********************** Summary of this data structure **********************
 *
 *     ordered_hashset is designed to behave like a c++ unordered_map, except
 * that the elements are ordered.
 *
 * Important features:
 * - O(1) insertion (amortized)
 * - O(1) deletion  (amortized)
 * - O(1) lookup
 * - Elements are stored in the order of insertion.
 * - bidirectional iterator
 *
 * How does ordered_hashset work?
 *     ordered_hashset is composed of a linear storage of items anda vanilla
 * hash map that keeps tracks of the correspondence between item and storage
 * id. In our implementation, these two containers are implemented using
 * std::vector and std::unordered_map.
 *
 *       hash map
 *     +-------+----+
 *     | item  | id |
 *     +-------+----+               linear storage
 *     |       |    |             +-----+---------+
 *     +-------+----+             | id  | item    |
 *     | item2 | 2  | ------+     +-----+---------+
 *     +-------+----+    +--|---> | 0   | item0   |
 *     |       |    |    |  |     +-----+---------+
 *     +-------+----+    |  |     | 1   | deleted |
 *     |       |    |    |  |     +-----+---------+
 *     +-------+----+    |  +---> | 2   | item2   |
 *     | item0 | 0  | ---+        +-----+---------+
 *     +-------+----+       +---> | 3   | item3   |
 *     |       |    |       |     +-----+---------+
 *     +-------+----+       |
 *     | item3 | 3  | ------+
 *     +-------+----+
 *
 *     On traversal, we can just traverse through the linear storage, so that
 * we visit each element by the order of insertion. On the other hand, lookups
 * are just as simple: the hash map provides easy O(1) access.
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

#include "./ordered_hashtable.hpp"

namespace dvlab {

namespace utils {

template <typename Key, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class ordered_hashset final : public ordered_hashtable<Key, const Key, Key, Hash, KeyEqual> {  // NOLINT(readability-identifier-naming) : ordered_hashset intentionally mimics std::unordered_set
    using _Table_t = ordered_hashtable<Key, const Key, Key, Hash, KeyEqual>;

public:
    using key_type        = typename _Table_t::key_type;
    using value_type      = typename _Table_t::value_type;
    using stored_type     = typename _Table_t::stored_type;
    using size_type       = typename _Table_t::size_type;
    using difference_type = typename _Table_t::difference_type;
    using hasher          = typename _Table_t::hasher;
    using key_equal       = typename _Table_t::key_equal;
    using container       = typename _Table_t::container;
    using iterator        = typename _Table_t::iterator;
    using const_iterator  = typename _Table_t::const_iterator;

    ordered_hashset() : _Table_t() {}
    ordered_hashset(std::initializer_list<value_type> const& il) : _Table_t() {
        for (value_type const& item : il) {
            this->_key2id.emplace(key(item), this->_data.size());
            this->_data.emplace_back(item);
        }
        this->_size = il.size();
    }

    template <typename InputIt>
    ordered_hashset(InputIt first, InputIt last) {
        for (auto it = first; it != last; ++it) {
            this->_key2id.emplace(key(*(it)), this->_data.size());
            this->_data.emplace_back(*(it));
        }
        this->_size = this->_data.size();
    }

    // lookup
    virtual Key const& key(stored_type const& value) const override { return value; }
};

static_assert(std::ranges::bidirectional_range<ordered_hashset<int>>);

}  // namespace utils

}  // namespace dvlab