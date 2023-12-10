/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define data structure manager template ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>

#include "./ordered_hashmap.hpp"

namespace dvlab {

namespace utils {

template <typename T>
std::string data_structure_info_string(T const& t);

template <typename T>
std::string data_structure_name(T const& t);

template <typename T>
concept manager_manageable = requires {
    { data_structure_info_string(std::declval<T>()) } -> std::convertible_to<std::string>;
    { data_structure_name(std::declval<T>()) } -> std::convertible_to<std::string>;
};

template <typename T>
requires manager_manageable<T>
class DataStructureManager {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    DataStructureManager(std::string_view name) : _type_name{name} {}
    virtual ~DataStructureManager() = default;

    DataStructureManager(DataStructureManager const& other) : _next_id{other._next_id}, _focused_id{other._focused_id} {
        for (auto& [id, data] : other._list) {
            _list.emplace(id, std::make_unique<T>(*data));
        }
    }
    DataStructureManager(DataStructureManager&& other) noexcept = default;

    DataStructureManager& operator=(DataStructureManager copy) {
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

    void clear() {
        _next_id    = 0;
        _focused_id = 0;
        _list.clear();
    }

    bool is_id(size_t id) const { return _list.contains(id); }

    size_t get_next_id() const { return _next_id; }

    T* get() const { return _list.at(_focused_id).get(); }

    void set_by_id(size_t id, std::unique_ptr<T> t) {
        if (_list.contains(id)) {
            spdlog::info("Note: Replacing {} {}...", _type_name, id);
        }
        _list.insert_or_assign(id, std::move(t));
    }

    void set(std::unique_ptr<T> t) {
        set_by_id(_focused_id, std::move(t));
    }

    bool empty() const { return _list.empty(); }
    size_t size() const { return _list.size(); }
    size_t focused_id() const { return _focused_id; }

    T* add(size_t id) {
        _list.emplace(id, std::make_unique<T>());
        _focused_id = id;
        if (id == _next_id || _next_id < id) _next_id = id + 1;

        spdlog::info("Successfully created and checked out to {0} {1}", _type_name, id);

        return this->get();
    }

    T* add(size_t id, std::unique_ptr<T> t) {
        _list.emplace(id, std::move(t));
        _focused_id = id;
        if (id == _next_id || _next_id < id) _next_id = id + 1;

        spdlog::info("Successfully created and checked out to {0} {1}", _type_name, id);

        return this->get();
    }

    void remove(size_t id) {
        if (!_list.contains(id)) {
            _print_id_does_not_exist_error_msg();
            return;
        }

        _list.erase(id);
        spdlog::info("Successfully removed {0} {1}", _type_name, id);

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
        spdlog::info("Checked out to {} {}", _type_name, _focused_id);
    }

    void copy(size_t new_id) {
        if (this->empty()) {
            spdlog::error("Cannot copy {0}: The {0} list is empty!!", _type_name);
            return;
        }
        auto copy = std::make_unique<T>(*get());

        if (_next_id <= new_id) _next_id = new_id + 1;
        _list.insert_or_assign(new_id, std::move(copy));

        spdlog::info("Successfully copied {0} {1} to {0} {2}", _type_name, _focused_id, new_id);
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
            auto name = data_structure_name(*get());
            fmt::println("-> Now focused on: {} {}{}", _type_name, _focused_id, name.empty() ? "" : fmt::format(" ({})", name));
        }
    }

    void print_list() const {
        if (this->size()) {
            for (auto& [id, data] : _list) {
                fmt::println("{} {}    {}", (id == _focused_id ? "★" : " "), id, data_structure_info_string(*data.get()));
            }
        } else {
            fmt::println("The {} list is empty", _type_name);
        }
    }

    void print_focus() const {
        if (this->size()) {
            auto name = data_structure_name(*get());
            fmt::println("-> Now focused on: {} {}{}", _type_name, _focused_id, name.empty() ? "" : fmt::format(" ({})", name));
        } else {
            fmt::println("The {} list is empty", _type_name);
        }
    }

    std::string get_type_name() const { return _type_name; }

private:
    size_t _next_id    = 0;
    size_t _focused_id = 0;
    ordered_hashmap<size_t, std::unique_ptr<T>> _list;
    std::string _type_name;

    void _print_id_does_not_exist_error_msg() const {
        fmt::println(stderr, "Error: The ID provided does not exist!!");
    }
};

}  // namespace utils

}  // namespace dvlab
