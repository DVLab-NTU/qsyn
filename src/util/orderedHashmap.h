/****************************************************************************
  FileName     [ ordered_hashmap.h ]
  PackageName  [ util ]
  Synopsis     [ Define ordered_hashmap ]
  Author       [ Mu-Te Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
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

template <
    typename Key,
    typename T,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>,
    typename Allocator = std::allocator<std::pair<const Key, T>>>
class OrderedHashmap {
public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using stored_type = std::optional<std::pair<Key, T>>;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using pointer = std::allocator_traits<Allocator>::pointer;
    using const_pointer = std::allocator_traits<Allocator>::const_pointer;

    OrderedHashmap() : _size(0) {}
    OrderedHashmap(const std::initializer_list<value_type>& il) : _size(il.size()) {
        for (const value_type& item : il) {
            _key2id.emplace(item.first, _data.size());
            _data.emplace_back(item);
        }
    }

    // OrderedHashmap(const OrderedHashmap& omap) = default;
    // OrderedHashmap(OrderedHashmap&& omap) = default;

    // properties
    size_t size() { return _size; }
    bool empty() { return (size() == 0); }
    bool operator==(const OrderedHashmap& rhs) {
        return _data = rhs->_data;
    }
    bool operator!=(const OrderedHashmap& rhs) {
        return !(*this == rhs);
    }

    // REVIEW - return iterator and bool
    void insert(value_type&& value) {
        emplace(std::move(value));
    }
    template <typename... Args>
    void emplace(Args&&... args) {
        _data.emplace_back(value_type(std::forward<Args>(args)...));
        if (_key2id.contains(_data.back().value().first)) {
            std::cout << "key collision" << std::endl;
            _data.pop_back();
            return;
        }
        _key2id.emplace(_data.back().value().first, _data.size() - 1);
        std::cout << "inserted " << _data.back().value().first << " : " << _data.back().value().second << std::endl;
        _size++;
    }

    // REVIEW - add iterator version
    /**
     * @brief Erase the value with the key. This function perform lazy deletion.
     *
     * @param key
     * @return size_type
     */
    size_type erase(const Key& key) {
        if (!_key2id.contains(key)) {
            std::cout << "not in map" << std::endl;
            return 0;
        }
        if (_data[_key2id.at(key)] == std::nullopt) {
            std::cout << "nullopt" << std::endl;
            return 0;
        }

        _data[_key2id[key]] = std::nullopt;
        _key2id.erase(key);
        std::cout << "erased " << key << std::endl;
        _size -= 1;
        // REVIEW - batch deletion
        if (_data.size() >= (_size << 1)) {
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
            _data.clear();
            for (const auto& value : newData) {
                _data.push_back(value);
            }
            std::cout << "Triggered batch deletion" << std::endl;
        }
        return 1;
    }

    // look-up

    bool contains(const Key& key) const {
        return (_key2id.contains(key) && _data[_key2id.at(key)] != std::nullopt);
    }
    /**
     * @brief return the value corresponding to the key.
     *        Throws `std::out_of_range error` if no values are found.
     *
     * @param key
     * @return T&
     */
    T& at(const Key& key) {
        if (!contains(key)) {
            throw std::out_of_range("no value corresponding to the key");
        }
        return _data[_key2id[key]].value().second;
        // try {
        // } catch (std::out_of_range &e) {
        //     throw std::out_of_range("no value corresponding to the key");
        // }
    }
    /**
     * @brief return the value corresponding to the key.
     * Throws `std::out_of_range error` if no values are found.
     *
     * @param key
     * @return const T&
     */
    const T& at(const Key& key) const {
        if (!contains(key)) {
            throw std::out_of_range("no value corresponding to the key");
        }
        return _data[_key2id[key]];
        // try {
        // } catch (std::out_of_range &e) {
        //     throw std::out_of_range("no value corresponding to the key");
        // }
    }

    T& operator[](const Key& key) {
        try {
            return at(key);
        } catch (std::out_of_range& e) {
            emplace(key, T());
            return at(key);
        }
    }

    T& operator[](Key&& key) {
        try {
            return at(key);
        } catch (std::out_of_range& e) {
            emplace(key, T());
            return at(key);
        }
    }

    // test
    void printMap() {
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

private:
    std::unordered_map<Key, size_t> _key2id;
    std::vector<stored_type> _data;
    size_t _size;
};

#endif  // ORDERED_HASHMAP_H