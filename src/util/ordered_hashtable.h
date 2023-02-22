/****************************************************************************
  FileName     [ ordered_hashtable.h ]
  PackageName  [ util ]
  Synopsis     [ Define ordered_hashtable interface ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

/********************** Summary of this data structure **********************
 *
 *     ordered_hashtable is the common interface of ordered_hashmap and
 * ordered_hashset. The data structure works like std::unordered_[map, set],
 * except that the elements are ordered.
 *
 * For more details, please see the descriptions in
 *     - src/util/ordered_hashmap.h
 *     - src/util/ordered_hashset.h
 *
 ****************************************************************************/

#ifndef ORDERED_HASHTABLE_H
#define ORDERED_HASHTABLE_H

#include <iterator>
#include <optional>
#include <ranges>
#include <tuple>
#include <unordered_map>
#include <vector>

template <typename Key, typename Value, typename StoredType, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class ordered_hashtable {
public:
    // class OTableIterator;
    using key_type = Key;
    using value_type = Value;
    using stored_type = StoredType;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using container = std::vector<std::optional<stored_type>>;

    template <typename VecIterType>
    class OTableIterator;

    using iterator = OTableIterator<typename container::iterator>;
    using const_iterator = OTableIterator<typename container::const_iterator>;

    template <typename VecIterType>
    class OTableIterator {
    public:
        using value_type = Value;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;
        OTableIterator() {}
        OTableIterator(const VecIterType& itr, const VecIterType& begin, const VecIterType& end) : _itr(itr), _begin(begin), _end(end) {}
        OTableIterator(const OTableIterator<VecIterType>& o_itr) = default;

        OTableIterator& operator++() noexcept {
            do {
                ++_itr;
            } while (_itr != _end && !_itr->has_value());
            return *this;
        }

        OTableIterator operator++(int) noexcept {
            OTableIterator tmp = *this;
            ++*this;
            return tmp;
        }

        OTableIterator& operator--() noexcept {
            do {
                --_itr;
            } while (_itr != _begin && !_itr->has_value());
            return *this;
        }

        OTableIterator operator--(int) noexcept {
            OTableIterator tmp = *this;
            --*this;
            return tmp;
        }

        bool operator==(const OTableIterator& rhs) const noexcept { return this->_itr == rhs._itr; }
        bool operator!=(const OTableIterator& rhs) const noexcept { return !(*this == rhs); }

        bool isValid() const noexcept { return *(this->_itr) != std::nullopt; }

        value_type& operator*() noexcept { return (value_type&)this->_itr->value(); }
        const value_type& operator*() const noexcept { return (value_type&)this->_itr->value(); }

        value_type* operator->() noexcept { return (value_type*)&(this->_itr->value()); }
        value_type const* operator->() const noexcept { return (value_type*)&(this->_itr->value()); }

    private:
        VecIterType _itr;
        VecIterType _begin;
        VecIterType _end;
    };

    // static_assert(std::bidirectional_iterator<iterator>);
    // static_assert(std::bidirectional_iterator<const_iterator>);

    ordered_hashtable() : _size(0) {}

    // iterators
    iterator begin() noexcept {
        auto itr = _data.begin();
        while (itr != _data.end() && !itr->has_value()) ++itr;
        return iterator(itr, this->_data.begin(), this->_data.end());
    }
    iterator end() noexcept { return iterator(this->_data.end(), this->_data.begin(), this->_data.end()); }
    const_iterator begin() const noexcept {
        auto itr = _data.begin();
        while (itr != _data.end() && !itr->has_value()) ++itr;
        return const_iterator(itr, this->_data.begin(), this->_data.end());
    }
    const_iterator end() const noexcept { return const_iterator(this->_data.end(), this->_data.begin(), this->_data.end()); }
    const_iterator cbegin() const noexcept { return const_iterator(this->_data.cbegin(), this->_data.begin(), this->_data.cend()); }
    const_iterator cend() const noexcept { return const_iterator(this->_data.cend(), this->_data.begin(), this->_data.cend()); }

    // lookup
    iterator find(const Key& key) {
        if (this->contains(key))
            return iterator(this->_data.begin() + this->id(key), this->_data.begin(), this->_data.end());
        else
            return this->end();
    }
    const_iterator find(const Key& key) const {
        if (this->contains(key))
            return const_iterator(this->_data.begin() + this->id(key), this->_data.begin(), this->_data.end());
        else
            return this->end();
    }
    size_type id(const Key& key) const;
    bool contains(const Key& key) const;
    virtual const Key& key(const stored_type& value) const = 0;

    // properties
    size_t size() const { return _size; }
    bool empty() const { return (this->size() == 0); }
    bool operator==(const ordered_hashtable& rhs) const { return _data == rhs._data; }
    bool operator!=(const ordered_hashtable& rhs) const { return !(*this == rhs); }

    // container manipulation
    void clear();
    std::pair<iterator, bool> insert(value_type&& value);
    std::pair<iterator, bool> insert(const value_type& value) { return this->insert(std::move(value)); }

    template <typename InputIt>
    void insert(const InputIt& first, const InputIt& last);

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args);

    void sweep();

    size_t erase(const Key& key);
    size_t erase(const iterator& itr);

    template <typename F>
    void sort(F lambda);

protected:
    std::unordered_map<Key, size_t, Hash, KeyEqual> _key2id;
    container _data;
    size_t _size;
};

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
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
typename ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::size_type
ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::id(const Key& key) const {
    return this->_key2id.at(key);
}

/**
 * @brief Check if the ordered hashmap contains an item with the given key.
 *
 * @param key
 * @return bool
 */
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
bool ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::contains(const Key& key) const {
    return (this->_key2id.contains(key) && this->_data[id(key)].has_value());
}

//------------------------------------------------------
//  container manipulation
//------------------------------------------------------

/**
 * @brief Clear the ordered hashmap
 *
 */
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
void ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::clear() {
    this->_key2id.clear();
    this->_data.clear();
    this->_size = 0;
}

/**
 * @brief Insert a value to the ordered hashmap
 *
 *
 * @param value
 */
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
std::pair<typename ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::iterator, bool>
ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::insert(value_type&& value) {
    return emplace(std::move(value));
}

template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
template <typename InputIt>
void ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::insert(const InputIt& first, const InputIt& last) {
    for (auto itr = first; itr != last; ++itr) {
        emplace(std::move(*itr));
    }
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
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
template <typename... Args>
std::pair<typename ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::iterator, bool>
ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::emplace(Args&&... args) {
    this->_data.emplace_back(value_type(std::forward<Args>(args)...));
    const key_type key = this->key(this->_data.back().value());
    bool hasItem = this->_key2id.contains(key);
    if (hasItem) {
        this->_data.pop_back();
    } else {
        this->_key2id.emplace(key, this->_data.size() - 1);
        this->_size++;
    }
    return std::make_pair(this->find(key), !hasItem);
}

/**
 * @brief Delete the placeholders for deleted data in the ordered hashmap
 *
 */
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
void ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::sweep() {
    std::erase_if(_data, [](const std::optional<stored_type>& v) { return !v.has_value(); });

    for (size_t i = 0; i < _data.size(); ++i) {
        _key2id[this->key(_data[i].value())] = i;
    }
}

/**
 * @brief Erase the key-value pair with the given key. To achieve amortized
 *        O(1) deletion, this function only mark the key-value pair as
 *        deleted. Actual deletion is performed when half of the stored data
 *        are placeholders.
 * @param key
 * @return size_t : the number of element deleted
 */
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
size_t ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::erase(const Key& key) {
    if (!this->contains(key)) return 0;

    this->_data[this->id(key)] = std::nullopt;
    this->_key2id.erase(key);
    this->_size--;

    if (this->_data.size() >= (this->_size << 1)) {
        this->sweep();
    }
    return 1;
}

/**
 * @brief Erase the key-value pair with the given iterator. To achieve a-
 *        mortized O(1) deletion, this function only mark the key-value pair
 *        as deleted. Actual deletion is performed when half of the stored
 *        data are placeholders.
 *
 * @param itr
 * @return size_t : the number of element deleted
 */
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
size_t ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::erase(
    const typename ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::iterator& itr) {
    return erase(key(*itr));
}

template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
template <typename F>
void ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::sort(F lambda) {
    std::sort(this->_data.begin(), this->_data.end(), [&lambda](const std::optional<stored_type>& a, const std::optional<stored_type>& b) {
        if (!a.has_value()) return false;
        if (!b.has_value()) return true;
        return lambda(a.value(), b.value());
    });

    // update key2id
    for (size_t i = 0; i < _data.size(); ++i) {
        if (_data[i].has_value()) {
            this->_key2id[this->key(this->_data[i].value())] = i;
        }
    }
}

#endif  // ORDERED_HASHTABLE_H