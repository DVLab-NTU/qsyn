/****************************************************************************
  FileName     [ ordered_hashtable.h ]
  PackageName  [ util ]
  Synopsis     [ Define ordered_hashtable interface ]
  Author       [ Mu-Te Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef ORDERED_HASHTABLE_H
#define ORDERED_HASHTABLE_H

#include <exception>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <vector>

template <typename Key, typename Value, typename StoredType, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class ordered_hashtable {
public:
    // class OTableIterator;
    using key_type = Key;
    using value_type = Value;
    using stored_type = std::optional<StoredType>;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    
    template <typename VecIterType>
    class OTableIterator {
    public:
        OTableIterator() {}
        OTableIterator(const VecIterType& itr, const VecIterType& end) : _itr(itr), _end(end) {}

        OTableIterator& operator++() noexcept {
            do {
                ++_itr;
            } while (_itr != _end && *_itr == std::nullopt);
            return *this;
        }

        OTableIterator& operator++(int) noexcept {
            OTableIterator tmp = *this;
            ++*this;
            return tmp;
        }

        bool operator==(const OTableIterator& rhs) const noexcept { return this->_itr == rhs._itr; }
        bool operator!=(const OTableIterator& rhs) const noexcept { return !(*this == rhs); }

        value_type& operator*() noexcept { return (value_type&)this->_itr->value(); }
        const value_type& operator*() const noexcept { return (value_type&)this->_itr->value(); }

        value_type* operator->() noexcept { return (value_type*)&(this->_itr->value()); }
        value_type const* operator->() const noexcept { return (value_type*)&(this->_itr->value()); }

    private:
        VecIterType _itr;
        VecIterType _end;
    };

    using iterator = OTableIterator<typename std::vector<stored_type>::iterator>;
    using const_iterator = OTableIterator<typename std::vector<stored_type>::const_iterator>;

    ordered_hashtable(): _size(0) {}

    // iterators
    iterator begin() noexcept { return iterator(this->_data.begin(), this->_data.end()); }
    iterator end() noexcept { return iterator(this->_data.end(), this->_data.end()); }
    const_iterator begin() const noexcept { return iterator(this->_data.begin(), this->_data.end()); }
    const_iterator end() const noexcept { return iterator(this->_data.end(), this->_data.end()); }
    const_iterator cbegin() const noexcept { return iterator(this->_data.begin(), this->_data.end()); }
    const_iterator cend() const noexcept { return iterator(this->_data.end(), this->_data.end()); }
    
    
    // lookup
    iterator find(const Key& key) { return iterator(this->_data.begin() + this->id(key), this->_data.end()); }
    const_iterator find(const Key& key) const { return iterator(this->_data.begin() + this->id(key), this->_data.end()); }
    size_type id (const Key& key) const;
    bool contains(const Key& key) const;
    // virtual Key& key(const value_type& value) = 0;
    virtual const Key& key(const value_type& value) const = 0;
    

    // properties

    size_t size() { return _size; }
    bool empty() { return (this->size() == 0); }
    bool operator==(const ordered_hashtable& rhs) { return _data = rhs->_data; }
    bool operator!=(const ordered_hashtable& rhs) { return !(*this == rhs); }

    // container manipulation
    void clear();
    std::pair<iterator, bool> insert(value_type&& value);

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args);

    void sweep();

    size_t erase(const Key& key);
    size_t erase(const iterator& itr);


protected:
    std::unordered_map<Key, size_t, Hash, KeyEqual> _key2id;
    std::vector<stored_type> _data;
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
    return (this->_key2id.contains(key) && this->_data[id(key)] != std::nullopt);
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
 * @brief Insert a key-value pair to the ordered hashmap
 *
 *
 * @param value
 */
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
std::pair<typename ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::iterator, bool>
ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::insert(value_type&& value) {
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
template <typename Key, typename Value, typename StoredType, typename Hash, typename KeyEqual>
template <typename... Args>
std::pair<typename ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::iterator, bool>
ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::emplace(Args&&... args) { // REVIEW - change for OrderedHashset
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
    auto hasValue = [](const stored_type& value) -> bool {
        return value != std::nullopt;
    };
    
    std::vector<stored_type> newData;
    size_t count = 0;
    for (size_t i = 0; i < this->_data.size(); ++i) {
        if (hasValue(this->_data[i])) {
            newData.emplace_back(this->_data[i]);
            this->_key2id.at(this->key(this->_data[i].value())) = count;
            count++;
        }
    }
    this->_data.swap(newData);
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
size_t ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::erase(const Key& key) { // REVIEW - change for OrderedHashset
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
    const typename ordered_hashtable<Key, Value, StoredType, Hash, KeyEqual>::iterator& itr
) { 
    return erase(key(*itr));
}



#endif // ORDERED_HASHTABLE_H