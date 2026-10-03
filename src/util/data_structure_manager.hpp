/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define data structure manager template ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dvlab {

namespace utils {

// Forward declaration of DataStructureManager (without requires clause for now)
template <typename T>
class DataStructureManager;

// Forward declarations for function templates
template <typename T>
std::string data_structure_info_string(DataStructureManager<T> const& mgr, size_t id);

template <typename T>
std::string data_structure_name(DataStructureManager<T> const& mgr, size_t id);

template <typename T>
struct ManagerItemAndAttrs {  // NOLINT(cppcoreguidelines-special-member-functions)
                              // : copy-swap idiom
    std::unique_ptr<T> data;
    std::string filename;
    std::vector<std::string> procedures;

    ManagerItemAndAttrs() : data{nullptr} {}

    ManagerItemAndAttrs(std::unique_ptr<T> d)
        : data{std::move(d)} {}

    ManagerItemAndAttrs(std::unique_ptr<T> d, std::string f, std::vector<std::string> p)
        : data{std::move(d)}, filename{std::move(f)}, procedures{std::move(p)} {}

    ManagerItemAndAttrs(ManagerItemAndAttrs const& other)
        : data{other.data ? std::make_unique<T>(*other.data) : nullptr},
          filename{other.filename},
          procedures{other.procedures} {}

    ManagerItemAndAttrs(ManagerItemAndAttrs&& other) noexcept = default;

    ~ManagerItemAndAttrs() = default;

    ManagerItemAndAttrs& operator=(ManagerItemAndAttrs copy) {
        swap(copy);
        return *this;
    }

    void swap(ManagerItemAndAttrs& other) noexcept {
        std::swap(data, other.data);
        std::swap(filename, other.filename);
        std::swap(procedures, other.procedures);
    }

    friend void swap(ManagerItemAndAttrs& a, ManagerItemAndAttrs& b) noexcept {
        a.swap(b);
    }
};

template <typename T>
class DataStructureManager {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    DataStructureManager(std::string_view name) : _type_name{name} {}
    virtual ~DataStructureManager() = default;

    DataStructureManager(DataStructureManager const& other) : _next_id{other._next_id}, _focused_id{other._focused_id} {
        for (auto& [id, item] : other._list) {
            _list.emplace(id, item);
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

    T* get() const { return size() ? _list.at(_focused_id).data.get() : nullptr; }

    void set_by_id(size_t id, std::unique_ptr<T> t) {
        if (_list.contains(id)) {
            spdlog::info("Note: Replacing {} {}...", _type_name, id);
            // Preserve filename and procedures when replacing
            auto filename   = _list.at(id).filename;
            auto procedures = _list.at(id).procedures;
            _list.insert_or_assign(id, ManagerItemAndAttrs<T>{std::move(t), std::move(filename), std::move(procedures)});
        } else {
            _list.insert_or_assign(id, ManagerItemAndAttrs<T>{std::move(t)});
        }
    }

    void set(std::unique_ptr<T> t) {
        set_by_id(_focused_id, std::move(t));
    }

    bool empty() const { return _list.empty(); }
    size_t size() const { return _list.size(); }
    size_t focused_id() const { return _focused_id; }

    T* add(size_t id) {
        _list.emplace(id, ManagerItemAndAttrs<T>{std::make_unique<T>()});
        _focused_id = id;
        if (id == _next_id || _next_id < id) _next_id = id + 1;

        spdlog::info("Successfully created and checked out to {0} {1}", _type_name, id);

        return this->get();
    }

    T* add(size_t id, std::unique_ptr<T> t) {
        _list.emplace(id, ManagerItemAndAttrs<T>{std::move(t)});
        _focused_id = id;
        if (id == _next_id || _next_id < id) _next_id = id + 1;

        spdlog::info("Successfully created and checked out to {0} {1}", _type_name, id);

        return this->get();
    }

    void remove(size_t id) {
        // Signal focused id
        id = id == SIZE_MAX ? _focused_id : id;
        if (!_list.contains(id)) {
            _print_id_does_not_exist_error_msg();
            return;
        }
        _list.erase(id);
        spdlog::info("Successfully removed {0} {1}", _type_name, id);

        if (this->size() && _focused_id == id) {
            fmt::println("Note: Focused graph is deleted. Checked out to {} 0", _type_name);
            checkout(0);
        }
        if (this->empty()) {
            fmt::println("Note: The {} list is empty now", _type_name);
        }
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
        auto const& source_item = _list.at(_focused_id);
        auto copy_data          = std::make_unique<T>(*source_item.data);
        ManagerItemAndAttrs<T> copy_item{std::move(copy_data), source_item.filename, source_item.procedures};

        if (_next_id <= new_id) _next_id = new_id + 1;
        _list.insert_or_assign(new_id, std::move(copy_item));

        spdlog::info("Successfully copied {0} {1} to {0} {2}", _type_name, _focused_id, new_id);
        checkout(new_id);
    }

    T* find_by_id(size_t id) const {
        if (!is_id(id)) {
            _print_id_does_not_exist_error_msg();
            return nullptr;
        }
        return _list.at(id).data.get();
    }

    void print_manager() const {
        fmt::println("-> #{}: {}", _type_name, this->size());
        if (this->size()) {
            auto name = data_structure_name(*this, _focused_id);
            fmt::println("-> Now focused on: {} {}{}", _type_name, _focused_id, name.empty() ? "" : fmt::format(" ({})", name));
        }
    }

    void print_list() const {
        if (this->size()) {
            for (auto& [id, item] : _list) {
                fmt::println("{} {}    {}", (id == _focused_id ? "★" : " "), id, data_structure_info_string(*this, id));
            }
        } else {
            fmt::println("The {} list is empty", _type_name);
        }
    }

    void print_focus() const {
        if (this->size()) {
            auto name = data_structure_name(*this, _focused_id);
            fmt::println("-> Now focused on: {} {}{}", _type_name, _focused_id, name.empty() ? "" : fmt::format(" ({})", name));
        } else {
            fmt::println("The {} list is empty", _type_name);
        }
    }

    std::string get_type_name() const { return _type_name; }

    // Filename access methods
    std::string get_filename(size_t id) const {
        if (!is_id(id)) {
            _print_id_does_not_exist_error_msg();
            return {};
        }
        return _list.at(id).filename;
    }

    std::string get_filename() const {
        return size() ? get_filename(_focused_id) : std::string{};
    }

    void set_filename(size_t id, std::string const& filename) {
        if (!is_id(id)) {
            _print_id_does_not_exist_error_msg();
            return;
        }
        _list.at(id).filename = filename;
    }

    void set_filename(std::string const& filename) {
        if (size()) {
            set_filename(_focused_id, filename);
        }
    }

    // Procedures access methods
    std::vector<std::string> const& get_procedures(size_t id) const {
        if (!is_id(id)) {
            _print_id_does_not_exist_error_msg();
            static std::vector<std::string> const empty;
            return empty;
        }
        return _list.at(id).procedures;
    }

    std::vector<std::string> const& get_procedures() const {
        if (size()) {
            return get_procedures(_focused_id);
        }
        static std::vector<std::string> const empty;
        return empty;
    }

    void add_procedure(size_t id, std::string procedure) {
        if (!is_id(id)) {
            _print_id_does_not_exist_error_msg();
            return;
        }
        _list.at(id).procedures.push_back(std::move(procedure));
    }

    void add_procedure(std::string procedure) {
        if (size()) {
            add_procedure(_focused_id, std::move(procedure));
        }
    }

    void add_procedures(size_t id, std::vector<std::string> const& procedures) {
        if (!is_id(id)) {
            _print_id_does_not_exist_error_msg();
            return;
        }
        _list.at(id).procedures.insert(_list.at(id).procedures.end(), procedures.begin(), procedures.end());
    }

    void add_procedures(std::vector<std::string> const& procedures) {
        if (size()) {
            add_procedures(_focused_id, procedures);
        }
    }

    void set_procedures(size_t id, std::vector<std::string> const& procedures) {
        if (!is_id(id)) {
            _print_id_does_not_exist_error_msg();
            return;
        }
        _list.at(id).procedures = procedures;
    }

    void set_procedures(std::vector<std::string> const& procedures) {
        if (size()) {
            set_procedures(_focused_id, procedures);
        }
    }

private:
    size_t _next_id    = 0;
    size_t _focused_id = 0;
    std::map<size_t, ManagerItemAndAttrs<T>> _list;
    std::string _type_name;

    void _print_id_does_not_exist_error_msg() const {
        fmt::println(stderr, "Error: The ID provided does not exist!!");
    }
};

// Concept definition - must come after DataStructureManager is defined
template <typename T>
concept manager_manageable = requires {
    { data_structure_info_string(std::declval<DataStructureManager<T> const&>(), std::declval<size_t>()) } -> std::convertible_to<std::string>;
    { data_structure_name(std::declval<DataStructureManager<T> const&>(), std::declval<size_t>()) } -> std::convertible_to<std::string>;
};

}  // namespace utils

}  // namespace dvlab
