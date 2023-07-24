/****************************************************************************
  FileName     [ dataStructureManager.h ]
  PackageName  [ util ]
  Synopsis     [ Define data structure manager template ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef DVLAB_DATA_STRUCTURE_MANAGER_H
#define DVLAB_DATA_STRUCTURE_MANAGER_H

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "ordered_hashmap.h"

extern size_t verbose;

namespace dvlab_utils {

template <typename T>
class DataStructureManager {
public:
    DataStructureManager(std::string_view name) : _nextID{0}, _currID{0}, _typeName{name} {}
    ~DataStructureManager() = default;

    DataStructureManager(DataStructureManager const& other) : _nextID{other._nextID}, _currID{other._currID} {
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

        if (verbose >= 3) {
            std::cout << "Created and checked out to " << _typeName << " " << id << std::endl;
        }

        return this->get();
    }

    void remove(size_t id) {
        if (!_list.contains(id)) {
            printIdDoesNotExistErrorMsg();
            return;
        }

        _list.erase(id);
        _currID = 0;
        if (verbose >= 3) {
            std::cout << "Successfully removed " << _typeName << " " << id << std::endl;
            if (!this->empty())
                printCheckOutMsg();
            else
                std::cout << "Note: The " << _typeName << " list is empty now" << std::endl;
        }
        return;
    }

    void checkout(size_t id) {
        if (!_list.contains(id)) {
            printIdDoesNotExistErrorMsg();
            return;
        }

        _currID = id;
        if (verbose >= 3) printCheckOutMsg();
    }
    void copy(size_t id, bool toNew = true) {
        if (this->empty()) {
            printMgrEmptyErrorMsg();
            return;
        }
        size_t origID = _currID;
        auto copy = std::make_unique<T>(*get());
        copy->setId(id);

        if (toNew || !_list.contains(id)) {
            _list.emplace(id, std::move(copy));
            _currID = id;
            if (_nextID <= id) _nextID = id + 1;
            if (verbose >= 3) {
                printCopySuccessMsg(origID, id);
                printCheckOutMsg();
            }
        } else {
            _list.at(id).swap(copy);
            if (verbose >= 3) printCopySuccessMsg(origID, id);
            checkout(id);
        }
    }

    T* findByID(size_t id) const {
        if (!isID(id)) {
            printIdDoesNotExistErrorMsg();
            return nullptr;
        }
        return _list.at(id).get();
    }

    void printMgr() const {
        std::cout << "-> #" << _typeName << ": " << this->size() << std::endl;
        if (this->size()) {
            std::cout << "-> ";
            printFocusMsg();
        }
    }

    void printList() const {
        if (this->size()) {
            for (auto& [id, data] : _list) {
                std::cout << (id == this->_currID ? "★ " : "  ")
                          << id << "    " << std::left << std::setw(20) << data->getFileName().substr(0, 20);

                size_t i = 0;
                for (auto& proc : data->getProcedures()) {
                    if (i != 0) std::cout << " ➔ ";
                    std::cout << proc;
                    ++i;
                }
                std::cout << std::endl;
            }
        } else {
            printMgrEmptyErrorMsg();
        }
    }

    void printFocus() const {
        if (this->size()) {
            printFocusMsg();
        } else {
            printMgrEmptyErrorMsg();
        }
    }

    void printListSize() const {
        std::cout << "#" << _typeName << size() << std::endl;
    }

private:
    size_t _nextID;
    size_t _currID;
    ordered_hashmap<size_t, std::unique_ptr<T>> _list;
    std::string _typeName;

    void printCheckOutMsg() const {
        std::cout << "Checked out to " << _typeName << " " << this->_currID << std::endl;
    }

    void printIdDoesNotExistErrorMsg() const {
        std::cerr << "Error: The ID provided does not exist!!" << std::endl;
    }

    void printFocusMsg() const {
        std::cout << "Now focused on: " << _currID << std::endl;
    }

    void printMgrEmptyErrorMsg() const {
        std::cerr << "Error: " << _typeName << "Mgr is empty now!!" << std::endl;
    }

    void printCopySuccessMsg(size_t oldID, size_t newID) const {
        std::cout << "Successfully copied "
                  << _typeName << " " << oldID << " to "
                  << _typeName << " " << newID << std::endl;
    }
};

}  // namespace dvlab_utils

#endif  // DVLAB_DATA_STRUCTURE_MANAGER_H