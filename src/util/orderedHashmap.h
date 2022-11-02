/****************************************************************************
  FileName     [ ordered_hashmap.h ]
  PackageName  [ util ]
  Synopsis     [ Define ordered_hashmap ]
  Author       [ Mu-Te Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ORDERED_HASHMAP_H
#define ORDERED_HASHMAP_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include <tuple>
#include <optional>
#include <stdexcept>
#include <exception>

template <
    typename Key, 
    typename T, 
    typename Hash      = std::hash<Key>,
    typename KeyEqual  = std::equal_to<Key>,
    typename Allocator = std::allocator<std::pair<const Key, T>>
> 
class OrderedHashmap {

public:
    using key_type        = Key;
    using mapped_type     = T;
    using value_type      = std::pair<const Key, T>;
    using size_type       = size_t;
    using difference_type = std::ptrdiff_t;
    using hasher          = Hash;
    using key_equal       = KeyEqual;
    using allocator_type  = Allocator;
    using pointer         = std::allocator_traits<Allocator>::pointer;
    using const_pointer   = std::allocator_traits<Allocator>::const_pointer;

    OrderedHashmap() {}
    OrderedHashmap(const std::initializer_list<value_type>& il): _size(il.size()) {
        for (const value_type& item : il) {
            _key2id.emplace(item.first, _data.size());
            _data.emplace_back(item);
        }
    }

    OrderedHashmap(const OrderedHashmap& omap) = default;
    OrderedHashmap(OrderedHashmap&& omap) = default;

    size_t size() { return _size; }

    /**
     * @brief return the value corresponding to the key. 
     *        Throws `std::out_of_range error` if no values are found.
     * 
     * @param key 
     * @return T&
     */
    T& at(const Key& key) {
        if (!_key2id.contains(key) || _data[_key2id[key]] == std::nullopt) {
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
        if (!_key2id.contains(key) || _data[_key2id[key]] == std::nullopt) {
            throw std::out_of_range("no value corresponding to the key");
        }
        return _data[_key2id[key]];
        // try { 
        // } catch (std::out_of_range &e) {
        //     throw std::out_of_range("no value corresponding to the key");
        // }
    }

    //REVIEW - return iterator and bool
    void insert(value_type&& value) {
        if (_key2id.contains(value.first)) {
            return;
        }
        _key2id.emplace(value.first, _data.size());
        _data.emplace_back(value);
        _size++;
        std::cout << "inserted " << value.first << " : " << value.second << std::endl;
    }

    //REVIEW - add iterator version
    /**
     * @brief Erase the value with the key. This function perform lazy deletion.
     * 
     * @param key 
     * @return size_type 
     */
    size_type erase(const Key& key) {
        if (!_key2id.contains(key) || _data[_key2id.at(key)] == std::nullopt) {
            return 0;
        }
        
        _data[_key2id[key]] = std::nullopt;
        _key2id.erase(key);
        std::cout << "erased " << key << std::endl;
        _size -= 1;
        //REVIEW - batch deletion
        return 1;
    }

    // test
    void printMap() {
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
    std::vector<std::optional<value_type>> _data;
    size_t _size;
};

#endif //ORDERED_HASHMAP_H