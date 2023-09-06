/****************************************************************************
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

extern dvlab::Logger LOGGER;

namespace dvlab {

namespace utils {

template <typename T>
std::string data_structure_info_string(T* t);

template <typename T>
std::string data_structure_name(T* t);

template <typename T>
requires requires(T t) {
    { data_structure_info_string(&t) } -> std::convertible_to<std::string>;
    { data_structure_name(&t) } -> std::convertible_to<std::string>;
}
class DataStructureManager {
public:
    DataStructureManager(std::string_view name) : _next_id{0}, _focused_id{0}, _type_name{name} {}
    virtual ~DataStructureManager() = default;

    DataStructureManager(DataStructureManager const& other) : _next_id{other._next_id}, _focused_id{other._focused_id} {
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
        std::swap(_next_id, other._next_id);
        std::swap(_focused_id, other._focused_id);
        std::swap(_list, other._list);
    }

    friend void swap(DataStructureManager& a, DataStructureManager& b) noexcept {
        a.swap(b);
    }

    void reset() {
        _next_id = 0;
        _focused_id = 0;
        _list.clear();
    }

    bool is_id(size_t id) const { return _list.contains(id); }

    size_t get_next_id() const { return _next_id; }

    T* get() const { return _list.at(_focused_id).get(); }

    void set(std::unique_ptr<T> t) {
        if (_list.contains(_focused_id)) {
            LOGGER.info("Note: Replacing {} {}...", _type_name, _focused_id);
        }
        _list.at(_focused_id).swap(t);
    }

    bool empty() const { return _list.empty(); }
    size_t size() const { return _list.size(); }
    size_t focused_id() const { return _focused_id; }

    T* add(size_t id) {
        _list.emplace(id, std::make_unique<T>());
        _focused_id = id;
        if (id == _next_id || _next_id < id) _next_id = id + 1;

        LOGGER.info("Successfully created and checked out to {0} {1}", _type_name, id);

        return this->get();
    }

    T* add(size_t id, std::unique_ptr<T> t) {
        _list.emplace(id, std::move(t));
        _focused_id = id;
        if (id == _next_id || _next_id < id) _next_id = id + 1;

        LOGGER.info("Successfully created and checked out to {0} {1}", _type_name, id);

        return this->get();
    }

    void remove(size_t id) {
        if (!_list.contains(id)) {
            _print_id_does_not_exist_error_msg();
            return;
        }

        _list.erase(id);
        LOGGER.info("Successfully removed {0} {1}", _type_name, id);

        if (this->size() && _focused_id == id) {
            checkout(0);
        }
        if (this->empty()) {
            fmt::println("Note: The {} list is empty now", _type_name);
        }
        return;
    }

    void checkout(size_t id) {
        if (!_list.contains(id)) {
            _print_id_does_not_exist_error_msg();
            return;
        }

        _focused_id = id;
        LOGGER.info("Checked out to {} {}", _type_name, _focused_id);
    }

    void copy(size_t new_id) {
        if (this->empty()) {
            LOGGER.error("Cannot copy {0}: The {0} list is empty!!", _type_name);
            return;
        }
        auto copy = std::make_unique<T>(*get());

        if (_next_id <= new_id) _next_id = new_id + 1;
        _list.insert_or_assign(new_id, std::move(copy));

        LOGGER.info("Successfully copied {0} {1} to {0} {2}", _type_name, _focused_id, new_id);
        checkout(new_id);
    }

    T* find_by_id(size_t id) const {
        if (!is_id(id)) {
            _print_id_does_not_exist_error_msg();
            return nullptr;
        }
        return _list.at(id).get();
    }

    void print_manager() const {
        fmt::println("-> #{}: {}", _type_name, this->size());
        if (this->size()) {
            auto name = data_structure_name(get());
            fmt::println("-> Now focused on: {} {}{}", _type_name, _focused_id, name.empty() ? "" : fmt::format(" ({})", name));
        }
    }

    void print_list() const {
        if (this->size()) {
            for (auto& [id, data] : _list) {
                fmt::println("{} {}    {}", (id == _focused_id ? "★" : " "), id, data_structure_info_string(data.get()));
            }
        } else {
            fmt::println("The {} list is empty", _type_name);
        }
    }

    void print_focus() const {
        if (this->size()) {
            auto name = data_structure_name(get());
            fmt::println("-> Now focused on: {} {}{}", _type_name, _focused_id, name.empty() ? "" : fmt::format(" ({})", name));
        } else {
            fmt::println("The {} list is empty", _type_name);
        }
    }

private:
    size_t _next_id;
    size_t _focused_id;
    ordered_hashmap<size_t, std::unique_ptr<T>> _list;
    std::string _type_name;

    void _print_id_does_not_exist_error_msg() const {
        fmt::println(stderr, "Error: The ID provided does not exist!!");
    }
};

}  // namespace utils

}  // namespace dvlab