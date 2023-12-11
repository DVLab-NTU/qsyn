/****************************************************************************
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
 *     - src/util/ordered_hashmap.hpp
 *     - src/util/ordered_hashset.hpp
 *
 ****************************************************************************/

#pragma once

#include <algorithm>
#include <iterator>
#include <optional>
#include <ranges>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace dvlab {

namespace utils {

template <typename Key, typename Value, typename StoredType, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class ordered_hashtable {  // NOLINT(readability-identifier-naming) : ordered_hashtable intentionally mimics std containers
public:
    // class OTableIterator;
    using key_type        = Key;
    using value_type      = Value;
    using stored_type     = StoredType;
    using size_type       = size_t;
    using difference_type = std::ptrdiff_t;
    using hasher          = Hash;
    using key_equal       = KeyEqual;
    using container       = std::vector<std::optional<stored_type>>;

    template <typename VecIterType>
    class OTableIterator;

    using iterator       = OTableIterator<typename container::iterator>;
    using const_iterator = OTableIterator<typename container::const_iterator>;

    template <typename VecIterType>
    class OTableIterator {
    public:
        using value_type        = Value;
        using difference_type   = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;
        OTableIterator() {}
        OTableIterator(VecIterType const& itr, VecIterType const& begin, VecIterType const& end) : _itr(itr), _begin(begin), _end(end) {}

        OTableIterator& operator++() noexcept {
            ++_itr;
            while (_itr != _end && !_itr->has_value()) {
                ++_itr;
            }
            return *this;
        }

        OTableIterator operator++(int) noexcept {
            OTableIterator tmp = *this;
            ++*this;
            return tmp;
        }

        OTableIterator& operator--() noexcept {
            --_itr;
            while (_itr != _begin && !_itr->has_value()) {
                --_itr;
            }
            return *this;
        }

        OTableIterator operator--(int) noexcept {
            OTableIterator tmp = *this;
            --*this;
            return tmp;
        }

        bool operator==(OTableIterator const& rhs) const noexcept { return this->_itr == rhs._itr; }
        bool operator!=(OTableIterator const& rhs) const noexcept { return !(*this == rhs); }

        bool is_valid() const noexcept { return *(this->_itr) != std::nullopt; }

        value_type& operator*() noexcept { return (value_type&)this->_itr->value(); }
        value_type& operator*() const noexcept { return (value_type&)this->_itr->value(); }

        value_type* operator->() noexcept { return (value_type*)&(this->_itr->value()); }
        value_type* operator->() const noexcept { return (value_type*)&(this->_itr->value()); }

    private:
        VecIterType _itr;
        VecIterType _begin;
        VecIterType _end;
    };

    ordered_hashtable() {}
    virtual ~ordered_hashtable() = default;

    ordered_hashtable(ordered_hashtable const& other)                = default;
    ordered_hashtable(ordered_hashtable&& other) noexcept            = default;
    ordered_hashtable& operator=(ordered_hashtable const& other)     = default;
    ordered_hashtable& operator=(ordered_hashtable&& other) noexcept = default;

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
    /**
     * @brief Return the internal index where the element with the key is stored.
     *        Note that as Ordered hashmap dynamically resizes its internal stor-
     *        age, this information is not reliable once insertion and erasure
     *        happens.
     *
     * @param key
     * @return size_type
     */
    inline iterator find(Key const& key) {
        return this->contains(key) ? iterator(this->_data.begin() + this->id(key), this->_data.begin(), this->_data.end()) : this->end();
    }

    /**
     * @brief Return the internal index where the element with the key is stored.
     *        Note that as Ordered hashmap dynamically resizes its internal stor-
     *        age, this information is not reliable once insertion and erasure
     *        happens.
     *
     * @param key
     * @return size_type
     */
    inline const_iterator find(Key const& key) const {
        return this->contains(key) ? iterator(this->_data.begin() + this->id(key), this->_data.begin(), this->_data.end()) : this->end();
    }
    /**
     * @brief Return the internal index where the element with the key is stored.
     *        Note that as Ordered hashmap dynamically resizes its internal stor-
     *        age, this information is not reliable once insertion and erasure
     *        happens.
     *
     * @param key
     * @return size_type
     */
    inline size_type id(Key const& key) const { return this->_key2id.at(key); }
    bool contains(Key const& key) const;
    template <typename KT>
    bool contains(KT const& key) const;
    virtual Key const& key(stored_type const& value) const = 0;

    // properties
    size_t size() const { return _size; }
    bool empty() const { return (this->size() == 0); }
    bool operator==(ordered_hashtable const& rhs) const {
        if (_size != rhs._size) return false;

        return any_of(this->begin(), this->end(), [&rhs](auto const& item) {
            return rhs.contains(item);
        });
    }
    bool operator!=(ordered_hashtable const& rhs) const { return !(*this == rhs); }

    // container manipulation
    void clear();
    std::pair<iterator, bool> insert(value_type&& value);
    std::pair<iterator, bool> insert(value_type const& value) { return this->insert(std::move(value)); }

    template <typename InputIt>
    void insert(InputIt const& first, InputIt const& last);

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args);

    void sweep();

    size_t erase(Key const& key);
    size_t erase(iterator const& itr);

    template <typename F>
    void sort(F lambda);

protected:
    std::unordered_map<Key, size_t, Hash, KeyEqual> _key2id = {};
    container _data                                         = {};
    size_t _size                                            = 0;
};

//------------------------------------------------------
//  lookup
//------------------------------------------------------

/**
 * @brief Check if the ordered hashmap contains an item with the given key.
 *
 * @param key
 * @return bool
 */
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
bool ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::contains(Key const& key) const {
    return (this->_key2id.contains(key) && this->_data[id(key)].has_value());
}

template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
template <typename KT>
bool ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::contains(KT const& key) const {
    // REVIEW - Can we avoid casting the key to Key?
    return (this->_key2id.contains(key) && this->_data[id(Key{key})].has_value());
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
void ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::insert(InputIt const& first, InputIt const& last) {
    for (auto itr = first; itr != last; ++itr) {
        emplace(std::move(*itr));
    }
}

/**
 * @brief Emplace a key-value pair to the ordered hashmap in place.
 *        Note that if the emplacement fails, the values are still moved-from.
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
    key_type const key  = this->key(this->_data.back().value());
    bool const has_item = this->_key2id.contains(key);
    if (has_item) {
        this->_data.pop_back();
    } else {
        this->_key2id.emplace(key, this->_data.size() - 1);
        this->_size++;
    }
    return std::make_pair(this->find(key), !has_item);
}

/**
 * @brief Delete the placeholders for deleted data in the ordered hashmap
 *
 */
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
void ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::sweep() {
    container new_data;
    new_data.reserve(_size * 2);
    for (auto&& v : _data) {
        if (v.has_value()) new_data.emplace_back(std::move(v));
    }
    // std::erase_if(_data, [](const std::optional<stored_type>& v) { return !v.has_value(); });
    _data.swap(new_data);
    // _data.resize();
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
size_t ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::erase(Key const& key) {
    if (!this->contains(key)) return 0;

    this->_data[this->id(key)] = std::nullopt;
    this->_key2id.erase(key);
    this->_size--;

    if (this->_data.size() >= (this->_size * 4)) {
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
    typename ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::iterator const& itr) {
    return erase(key(*itr));
}

template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
template <typename F>
void ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::sort(F lambda) {
    std::sort(this->_data.begin(), this->_data.end(), [&lambda](std::optional<stored_type> const& a, std::optional<stored_type> const& b) {
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

}  // namespace utils
}  // namespace dvlab
