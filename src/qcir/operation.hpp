/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir gate types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <gsl/narrow>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "qcir/qcir.hpp"
#include "qsyn/qsyn_type.hpp"
#include "tableau/tableau.hpp"
#include "tensor/qtensor.hpp"
#include "util/phase.hpp"
#include "zx/zxgraph.hpp"

namespace qsyn {
template <typename T>
std::optional<zx::ZXGraph> to_zxgraph(T const& /* op */);
template <typename T>
std::optional<tensor::QTensor<double>> to_tensor(T const& /* op */);
template <typename T>
bool append_to_tableau(T const& /* op */,
                       tableau::Tableau& /* tableau */,
                       QubitIdList const& /* qubits */);
}  // namespace qsyn

namespace qsyn::qcir {

namespace detail {

class DummyOperationType {
public:
    std::string get_type_str() const { return "DummyOperation"; }
    std::string get_repr() const { return "DummyOperation"; }
    size_t get_num_qubits() const { return 0; }
};

}  // namespace detail

class Operation;

Operation adjoint(Operation const& op);
bool is_clifford(Operation const& op);

std::optional<Operation>
str_to_operation(std::string str, std::vector<dvlab::Phase> const& params = {});

/**
 * @brief A type-erased interface for a quantum gate.
 *
 */
class Operation {  // NOLINT(hicpp-special-member-functions,
                   // cppcoreguidelines-special-member-functions) : copy-swap
                   // idiom
public:
    Operation() = default;
    template <typename T>
    Operation(T op) : _pimpl(std::make_unique<Model<T>>(std::move(op))) {}
    ~Operation() = default;

    Operation(Operation const& other)
        : _pimpl(other._pimpl ? other._pimpl->clone() : nullptr) {}
    Operation(Operation&&) noexcept = default;

    void swap(Operation& rhs) noexcept { std::swap(_pimpl, rhs._pimpl); }
    friend void swap(Operation& lhs, Operation& rhs) noexcept { lhs.swap(rhs); }

    Operation& operator=(Operation copy) noexcept {
        copy.swap(*this);
        return *this;
    }

    std::string get_type() const { return _pimpl->do_get_type(); }
    std::string get_repr() const { return _pimpl->do_get_repr(); }
    size_t get_num_qubits() const { return _pimpl->do_get_num_qubits(); }

    friend Operation adjoint(Operation const& op) {
        return op._pimpl->do_adjoint();
    }
    friend bool is_clifford(Operation const& op) {
        return op._pimpl->do_is_clifford();
    }

    friend std::optional<zx::ZXGraph> to_zxgraph(Operation const& op) {
        return op._pimpl->do_to_zxgraph();
    }
    friend std::optional<tensor::QTensor<double>> to_tensor(Operation const& op) {
        return op._pimpl->do_to_tensor();
    }
    friend bool append_to_tableau(Operation const& op,
                                  tableau::Tableau& tableau,
                                  QubitIdList const& qubits) {
        return op._pimpl->do_append_to_tableau(tableau, qubits);
    }
    friend std::optional<QCir> to_basic_gates(Operation const& op) {
        return op._pimpl->do_to_basic_gates();
    }

    bool operator==(Operation const& rhs) const {
        return get_repr() == rhs.get_repr() &&
               get_num_qubits() == rhs.get_num_qubits();
    }
    bool operator!=(Operation const& rhs) const { return !(*this == rhs); }

    template <typename T>
    T get_underlying() const {
        if (auto* model = dynamic_cast<Model<T>*>(_pimpl.get())) {
            return model->value;
        }
        spdlog::error("Operation type is {}, but expected {}", this->get_type(),
                      typeid(T).name());
        throw std::bad_cast();
    }

    template <typename T>
    bool is() const {
        return dynamic_cast<Model<T>*>(_pimpl.get()) != nullptr;
    }

    template <typename T>
    std::optional<T> get_underlying_if() const {
        if (auto* model = dynamic_cast<Model<T>*>(_pimpl.get())) {
            return model->value;
        }
        return std::nullopt;
    }

private:
    struct Concept {
        virtual ~Concept()                       = default;
        virtual std::unique_ptr<Concept> clone() = 0;

        virtual std::string do_get_type() const  = 0;
        virtual std::string do_get_repr() const  = 0;
        virtual size_t do_get_num_qubits() const = 0;

        virtual Operation do_adjoint() const = 0;
        virtual bool do_is_clifford() const  = 0;

        virtual std::optional<zx::ZXGraph> do_to_zxgraph() const            = 0;
        virtual std::optional<tensor::QTensor<double>> do_to_tensor() const = 0;
        virtual bool do_append_to_tableau(tableau::Tableau& tableau,
                                          QubitIdList const& qubits) const  = 0;
        virtual std::optional<QCir> do_to_basic_gates() const               = 0;
    };

    template <typename T>
    struct Model : Concept {
        Model(T value) : value(std::forward<T>(value)) {}
        std::unique_ptr<Concept> clone() override {
            return std::make_unique<Model>(*this);
        }

        std::string do_get_type() const override { return value.get_type(); }
        std::string do_get_repr() const override { return value.get_repr(); }
        size_t do_get_num_qubits() const override { return value.get_num_qubits(); }

        Operation do_adjoint() const override { return adjoint(value); }
        bool do_is_clifford() const override { return is_clifford(value); }

        std::optional<zx::ZXGraph> do_to_zxgraph() const override {
            return qsyn::to_zxgraph(value);
        }
        std::optional<tensor::QTensor<double>> do_to_tensor() const override {
            return qsyn::to_tensor(value);
        }
        bool do_append_to_tableau(tableau::Tableau& tableau,
                                  QubitIdList const& qubits) const override {
            return qsyn::append_to_tableau(value, tableau, qubits);
        }
        std::optional<QCir> do_to_basic_gates() const override {
            return to_basic_gates(value);
        }

        T value;
    };

    std::unique_ptr<Concept> _pimpl;
};

struct OperationHash {
    size_t operator()(Operation const& op) const {
        return std::hash<std::string>{}(op.get_repr());
    }
};

}  // namespace qsyn::qcir
