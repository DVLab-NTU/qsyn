/****************************************************************************
  FileName     [ dataStructureManager.hpp ]
  PackageName  [ util ]
  Synopsis     [ Define data structure manager template ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/core.h>
#include <fmt/format.h>

#include <memory>
#include <string>

#include "./logger.hpp"
#include "./ordered_hashmap.hpp"

extern dvlab::utils::Logger logger;

namespace dvlab {

namespace utils {

template <typename T>
std::string dataInfoString(T* t);

template <typename T>
std::string dataName(T* t);

template <typename T>
requires requires(T t) {
    { dataInfoString(&t) } -> std::convertible_to<std::string>;
    { dataName(&t) } -> std::convertible_to<std::string>;
    { t.setId(size_t{}) } -> std::same_as<void>;
    { t.getId() } -> std::convertible_to<size_t>;
}
class DataStructureManager {
public:
    DataStructureManager(std::string_view name) : _nextID{0}, _currID{0}, _typeName{name} {}
    virtual ~DataStructureManager() = default;

    DataStructureManager(DataStructureManager const& other) : _nextID{other._nextID}, _currID{other._currID} {
        for (auto& [id, data] : other._list) {
            _list.emplace(id, std::make_unique<T>(*data));
        }
    }
    DataStructureManager(DataStructureManager&& other) noexcept = default;

    virtual DataStructureManager& operator=(DataStructureManager copy) {
        copy.swap(*this);
        return *this;
    }

    void swap(DataStructureManager& other) noexcept {
        std::swap(_nextID, other._nextID);
        std::swap(_currID, other._currID);
        std::swap(_list, other._list);
    }

    friend void swap(DataStructureManager& a, DataStructureManager& b) noexcept {
        a.swap(b);
    }

    void reset() {
        _nextID = 0;
        _currID = 0;
        _list.clear();
    }

    bool isID(size_t id) const { return _list.contains(id); }

    size_t getNextID() const { return _nextID; }

    T* get() const { return _list.at(_currID).get(); }

    void set(std::unique_ptr<T> t) {
        t->setId(_currID);
        _list.at(_currID).swap(t);
    }

    bool empty() const { return _list.empty(); }
    size_t size() const { return _list.size(); }

    T* add(size_t id) {
        _list.emplace(id, std::make_unique<T>(id));
        _currID = id;
        if (id == _nextID || _nextID < id) _nextID = id + 1;

        logger.info("Successfully created and checked out to {0} {1}", _typeName, id);

        return this->get();
    }

    void remove(size_t id) {
        if (!_list.contains(id)) {
            printIdDoesNotExistErrorMsg();
            return;
        }

        _list.erase(id);
        logger.info("Successfully removed {0} {1}", _typeName, id);

        if (this->size() && _currID == id) {
            checkout(0);
        }
        if (this->empty()) {
            fmt::println("Note: The {} list is empty now", _typeName);
        }
        return;
    }

    void checkout(size_t id) {
        if (!_list.contains(id)) {
            printIdDoesNotExistErrorMsg();
            return;
        }

        _currID = id;
        logger.info("Checked out to {} {}", _typeName, _currID);
    }

    void copy(size_t newID) {
        if (this->empty()) {
            logger.error("Cannot copy {0}: The {0} list is empty!!", _typeName);
            return;
        }
        auto copy = std::make_unique<T>(*get());
        copy->setId(newID);

        if (_nextID <= newID) _nextID = newID + 1;
        _list.insert_or_assign(newID, std::move(copy));

        logger.info("Successfully copied {0} {1} to {0} {2}", _typeName, _currID, newID);
        checkout(newID);
    }

    T* findByID(size_t id) const {
        if (!isID(id)) {
            printIdDoesNotExistErrorMsg();
            return nullptr;
        }
        return _list.at(id).get();
    }

    void printManager() const {
        fmt::println("-> #{}: {}", _typeName, this->size());
        if (this->size()) {
            auto name = dataName(get());
            fmt::println("-> Now focused on: {} {}{}", _typeName, _currID, name.empty() ? "" : fmt::format(" ({})", name));
        }
    }

    void printList() const {
        if (this->size()) {
            for (auto& [id, data] : _list) {
                fmt::println("{} {}    {}", (id == _currID ? "★" : " "), id, dataInfoString(data.get()));
            }
        } else {
            fmt::println("The {} list is empty", _typeName);
        }
    }

    void printFocus() const {
        if (this->size()) {
            auto name = dataName(get());
            fmt::println("-> Now focused on: {} {}{}", _typeName, _currID, name.empty() ? "" : fmt::format(" ({})", name));
        } else {
            fmt::println("The {} list is empty", _typeName);
        }
    }

private:
    size_t _nextID;
    size_t _currID;
    ordered_hashmap<size_t, std::unique_ptr<T>> _list;
    std::string _typeName;

    void printIdDoesNotExistErrorMsg() const {
        fmt::println(stderr, "Error: The ID provided does not exist!!");
    }
};

}  // namespace utils

}  // namespace dvlab
