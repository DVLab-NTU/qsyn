/****************************************************************************
  FileName     [ ordered_hashmap.h ]
  PackageName  [ util ]
  Synopsis     [ Define ordered_hashmap ]
  Author       [ Mu-Te Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
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
 *     As ordered_hashmap automatically manages the size of its internal stor-
 * age, iterators may be invalidated upon insertion/deletion. Consequently, one
 * should not perform insertion/deletion during traversal. If this must be done,
 * it is suggested to collect the keys to another containers, and then perform
 * the insertion/deletion during traversal of another container.
 *
 ****************************************************************************/

#ifndef ORDERED_HASHMAP_H
#define ORDERED_HASHMAP_H

#include <exception>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <vector>

template <typename Key, typename T, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class ordered_hashmap {
public:
    class iterator;
    using key_type = Key;
    using mapped_type = T;                                 // REVIEW - change for OrderedHashSet
    using value_type = std::pair<const Key, T>;            // REVIEW - change for OrderedHashSet
    using stored_type = std::optional<std::pair<Key, T>>;  // REVIEW - change for OrderedHashSet
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using iterator = ordered_hashmap::iterator;
    using const_iterator = const iterator;

    class iterator {
    public:
        iterator() {}
        iterator(const std::vector<stored_type>::iterator& itr, const std::vector<stored_type>::iterator& end) : _itr(itr), _end(end) {}

        iterator& operator++() noexcept {
            do {
                ++_itr;
            } while (_itr != _end && *_itr == std::nullopt);
            return *this;
        }

        iterator& operator++(int) noexcept {
            iterator tmp = *this;
            ++*this;
            return tmp;
        }

        bool operator==(const iterator& rhs) const noexcept { return this->_itr == rhs._itr; }
        bool operator!=(const iterator& rhs) const noexcept { return !(*this == rhs); }

        value_type& operator*() noexcept { return (value_type&)this->_itr->value(); }
        const value_type& operator*() const noexcept { return (value_type&)this->_itr->value(); }

        value_type* operator->() noexcept { return (value_type*)&(this->_itr->value()); }
        value_type const* operator->() const noexcept { return (value_type*)&(this->_itr->value()); }

    private:
        std::vector<stored_type>::iterator _itr;
        std::vector<stored_type>::iterator _end;
    };

    friend class iterator;

    ordered_hashmap() noexcept : _size(0) {}
    ordered_hashmap(const std::initializer_list<value_type>& il) noexcept : _size(il.size()) {
        for (const value_type& item : il) {
            _key2id.emplace(item.first, _data.size());
            _data.emplace_back(item);
        }
    }

    // properties

    size_t size() { return _size; }
    bool empty() { return (this->size() == 0); }
    bool operator==(const ordered_hashmap& rhs) { return _data = rhs->_data; }
    bool operator!=(const ordered_hashmap& rhs) { return !(*this == rhs); }

    // container manipulation

    std::pair<iterator, bool> insert(value_type&& value);

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args);

    void sweep();

    size_t erase(const Key& key);
    size_t erase(const iterator& itr);

    void clear();

    // look-up

    iterator begin() noexcept { return iterator(_data.begin(), _data.end()); }
    iterator end() noexcept { return iterator(_data.end(), _data.end()); }
    const_iterator begin() const noexcept { return iterator(_data.begin(), _data.end()); }
    const_iterator end() const noexcept { return iterator(_data.end(), _data.end()); }
    const_iterator cbegin() const noexcept { return iterator(_data.begin(), _data.end()); }
    const_iterator cend() const noexcept { return iterator(_data.end(), _data.end()); }
    iterator find(const Key& key) { return iterator(_data.begin() + this->id(key), _data.end()); }
    const_iterator find(const Key& key) const { return iterator(_data.begin() + id(key), _data.end()); }

    size_t id(const Key& key);
    bool contains(const Key& key) const;
    T& at(const Key& key);
    const T& at(const Key& key) const;

    T& operator[](const Key& key);
    T& operator[](Key&& key);

    // test

    void printMap();

private:
    std::unordered_map<Key, size_t, Hash, KeyEqual> _key2id;
    std::vector<stored_type> _data;
    size_t _size;
};

//------------------------------------------------------
//  container manipulation
//------------------------------------------------------

/**
 * @brief Insert a key-value pair to the ordered hashmap
 *
 *
 * @param value
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
std::pair<typename ordered_hashmap<Key, T, Hash, KeyEqual>::iterator, bool>
ordered_hashmap<Key, T, Hash, KeyEqual>::insert(value_type&& value) {
    return emplace(std::move(value));
}

/**
 * @brief Emplace a key-value pair to the ordered hashmap in place.
 *
 *
 * @param value
 * @return std::pair<OrderedHashmap::iterator, bool>. If emplacement succeeds,
 *         the pair consists of an iterator to the emplaced pair and `true`;
 *         otherwise, the pair consists of `this->end()` and `false`.
 *
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
template <typename... Args>
std::pair<typename ordered_hashmap<Key, T, Hash, KeyEqual>::iterator, bool>
ordered_hashmap<Key, T, Hash, KeyEqual>::emplace(Args&&... args) { // REVIEW - change for OrderedHashset
    _data.emplace_back(value_type(std::forward<Args>(args)...));
    if (_key2id.contains(_data.back().value().first)) {
        // std::cout << "key collision" << std::endl;
        _data.pop_back();
        return std::make_pair(this->end(), false);
    }
    _key2id.emplace(_data.back().value().first, _data.size() - 1);
    // std::cout << "inserted " << _data.back().value().first << " : " << _data.back().value().second << std::endl;
    _size++;
    return std::make_pair(this->find(_data.back().value().first), true);
}

/**
 * @brief Delete the placeholders for deleted data in the ordered hashmap
 *
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
void ordered_hashmap<Key, T, Hash, KeyEqual>::sweep() { 
    auto hasValue = [](const stored_type& value) -> bool {
        return value != std::nullopt;
    };
    std::vector<stored_type> newData;
    size_t count = 0;
    for (size_t i = 0; i < _data.size(); ++i) {
        if (hasValue(_data[i])) {
            newData.emplace_back(_data[i]);
            _key2id.at(_data[i].value().first) = count;
            count++;
        }
    }
    _data = newData;
}

/**
 * @brief Erase the key-value pair with the given key. To achieve amortized
 *        O(1) deletion, this function only mark the key-value pair as
 *        deleted. Actual deletion is performed when half of the stored data
 *        are placeholders.
 * @param key
 * @return size_t : the number of element deleted
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
size_t ordered_hashmap<Key, T, Hash, KeyEqual>::erase(const Key& key) { // REVIEW - change for OrderedHashset
    if (!this->contains(key)) return 0;

    _data[this->id(key)] = std::nullopt;
    _key2id.erase(key);
    _size--;

    if (_data.size() >= (_size << 1)) {
        this->sweep();
    }
    return 1;
}

/**
 * @brief Erase the key-value pair with the given iterator. To achieve a-
 *        mortized O(1) deletion, this function only mark the key-value pair
 *        as deleted.Actual deletion is performed when half of the stored
 *        data are placeholders.
 *
 * @param itr
 * @return size_t : the number of element deleted
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
size_t ordered_hashmap<Key, T, Hash, KeyEqual>::erase(const typename ordered_hashmap<Key, T, Hash, KeyEqual>::iterator& itr) { // REVIEW - change for OrderedHashset
    return erase(itr->first);
}
/**
 * @brief Clear the ordered hashmap
 *
 *
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
void ordered_hashmap<Key, T, Hash, KeyEqual>::clear() {
    _key2id.clear();
    _data.clear();
    _size = 0;
}

//------------------------------------------------------
//  lookup
//------------------------------------------------------

/**
 * @brief Return the internal index where the element with the key is stored.
 *        Note that as Ordered hashmap dynamically resizes its internal stor-
 *        age, this information is not reliable once insertion and erasure
 *        happens.
 *
 * @param key
 * @return size_type
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
size_t ordered_hashmap<Key, T, Hash, KeyEqual>::id(const Key& key) {
    return _key2id[key];
}

/**
 * @brief Check if the ordered hashmap contains an item with the given key.
 *
 * @param key
 * @return bool
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
bool ordered_hashmap<Key, T, Hash, KeyEqual>::contains(const Key& key) const {
    return (_key2id.contains(key) && _data[_key2id.at(key)] != std::nullopt);
}

/**
 * @brief Return a const reference to the value associated with the given key.
 *        If no such element exists, throw an std::out_of_range exception.
 *
 * @param key
 * @return T&
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
T& ordered_hashmap<Key, T, Hash, KeyEqual>::at(const Key& key) {
    if (!this->contains(key)) {
        throw std::out_of_range("no value corresponding to the key");
    }
    return _data[this->id(key)].value().second;
}

/**
 * @brief Return a const reference to the value associated with the given key.
 *        If no such element exists, throw an std::out_of_range exception.
 *
 * @param key
 * @return cosnt T&
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
const T& ordered_hashmap<Key, T, Hash, KeyEqual>::at(const Key& key) const {
    if (!this->contains(key)) {
        throw std::out_of_range("no value corresponding to the key");
    }
    return _data[this->id(key)].value().second;
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
    if (!this->contains(key)) emplace(key, T());
    return at(key);
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
    if (!this->contains(key)) emplace(key, T());
    return at(key);
}

//------------------------------------------------------
//  test functions
//------------------------------------------------------

/**
 * @brief Test function that prints the content of the ordered hashmap.
 *
 */
template <typename Key, typename T, typename Hash, typename KeyEqual>
void ordered_hashmap<Key, T, Hash, KeyEqual>::printMap() {
    std::cout << "----  umap  ----" << std::endl;
    for (const auto& [k, v] : _key2id) {
        std::cout << k << " : " << v << std::endl;
    }
    std::cout << "---- vector ----" << std::endl;
    for (const auto& item : _data) {
        if (item == std::nullopt) {
            std::cout << "None" << std::endl;
            continue;
        }
        std::cout << item.value().first << " : " << item.value().second << std::endl;
    }
    std::cout << std::endl;
    std::cout << "size  : " << size() << std::endl
              << "none  : " << _data.size() - size() << std::endl
              << "total : " << _data.size() << std::endl;
}

#endif  // ORDERED_HASHMAP_H