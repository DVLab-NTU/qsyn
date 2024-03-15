/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir gate types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cstdint>
#include <functional>
#include <gsl/narrow>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "qsyn/qsyn_type.hpp"
#include "tableau/tableau.hpp"
#include "tensor/qtensor.hpp"
#include "util/phase.hpp"
#include "zx/zxgraph.hpp"

namespace qsyn {
template <typename T>
std::optional<zx::ZXGraph> to_zxgraph(T const& /* op */) { return std::nullopt; }
template <typename T>
std::optional<tensor::QTensor<double>> to_tensor(T const& /* op */) { return std::nullopt; }
template <typename T>
bool append_to_tableau(T const& /* op */, experimental::Tableau& /* tableau */, QubitIdList const& /* qubits */) { return false; }

}  // namespace qsyn

namespace qsyn::qcir {

enum class GateRotationCategory {
    id,
    h,
    swap,
    pz,
    rz,
    px,
    rx,
    py,
    ry,
    ecr
};

using GateType = std::tuple<GateRotationCategory, std::optional<size_t>, std::optional<dvlab::Phase>>;

std::optional<GateType> str_to_gate_type(std::string_view str);
std::string gate_type_to_str(GateRotationCategory category, std::optional<size_t> num_qubits = std::nullopt, std::optional<dvlab::Phase> phase = std::nullopt);
std::string gate_type_to_str(GateType const& type);

bool is_fixed_phase_gate(GateRotationCategory category);

dvlab::Phase get_fixed_phase(GateRotationCategory category);

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

/**
 * @brief A type-erased interface for a quantum gate.
 *
 */
class Operation {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    Operation() = default;
    template <typename T>
    Operation(T op) : _pimpl(std::make_unique<Model<T>>(std::move(op))) {}
    ~Operation() = default;

    Operation(Operation const& other) : _pimpl(other._pimpl ? other._pimpl->clone() : nullptr) {}
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

    friend Operation adjoint(Operation const& op) { return op._pimpl->do_adjoint(); }
    friend bool is_clifford(Operation const& op) { return op._pimpl->do_is_clifford(); }

    friend std::optional<zx::ZXGraph> to_zxgraph(Operation const& op) {
        return op._pimpl->do_to_zxgraph();
    }
    friend std::optional<tensor::QTensor<double>> to_tensor(Operation const& op) {
        return op._pimpl->do_to_tensor();
    }
    friend bool append_to_tableau(Operation const& op, experimental::Tableau& tableau, QubitIdList const& qubits) {
        return op._pimpl->do_append_to_tableau(tableau, qubits);
    }

    bool operator==(Operation const& rhs) const { return get_repr() == rhs.get_repr() && get_num_qubits() == rhs.get_num_qubits(); }
    bool operator!=(Operation const& rhs) const { return !(*this == rhs); }

    template <typename T>
    T get_underlying() const {
        if (auto* model = dynamic_cast<Model<T>*>(_pimpl.get())) {
            return model->value;
        }
        throw std::bad_cast();
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

        virtual std::optional<zx::ZXGraph> do_to_zxgraph() const                                           = 0;
        virtual std::optional<tensor::QTensor<double>> do_to_tensor() const                                = 0;
        virtual bool do_append_to_tableau(experimental::Tableau& tableau, QubitIdList const& qubits) const = 0;
    };

    template <typename T>
    struct Model : Concept {
        Model(T&& value) : value(std::forward<T>(value)) {}
        std::unique_ptr<Concept> clone() override { return std::make_unique<Model>(*this); }

        std::string do_get_type() const override { return value.get_type(); }
        std::string do_get_repr() const override { return value.get_repr(); }
        size_t do_get_num_qubits() const override { return value.get_num_qubits(); }

        Operation do_adjoint() const override { return adjoint(value); }
        bool do_is_clifford() const override { return is_clifford(value); }

        std::optional<zx::ZXGraph> do_to_zxgraph() const override { return qsyn::to_zxgraph(value); }
        std::optional<tensor::QTensor<double>> do_to_tensor() const override { return qsyn::to_tensor(value); }
        bool do_append_to_tableau(experimental::Tableau& tableau, QubitIdList const& qubits) const override { return qsyn::append_to_tableau(value, tableau, qubits); }

        T value;
    };

    std::unique_ptr<Concept> _pimpl;
};

class LegacyGateType {
public:
    LegacyGateType(GateType type) : _type(type) {}
    std::string get_type() const { return gate_type_to_str(_type); }
    std::string get_repr() const {
        if (is_fixed_phase_gate(std::get<0>(_type)))
            return get_type();
        return get_type() + ((get_type().find_first_of("pr") != std::string::npos) ? fmt::format("({})", std::get<2>(_type)->get_print_string()) : "");
    }
    size_t get_num_qubits() const { return std::get<1>(_type).value_or(0); }

    GateRotationCategory get_rotation_category() const { return std::get<0>(_type); }
    dvlab::Phase get_phase() const { return std::get<2>(_type).value_or(dvlab::Phase(0, 1)); }
    void set_phase(dvlab::Phase phase) { std::get<2>(_type) = phase; }

private:
    GateType _type;
};

Operation adjoint(LegacyGateType const& op);
bool is_clifford(LegacyGateType const& op);

class HGate {
public:
    HGate() = default;
    std::string get_type() const { return "h"; }
    std::string get_repr() const { return "h"; }
    size_t get_num_qubits() const { return 1; }
};

inline Operation adjoint(HGate const& op) { return op; }
inline bool is_clifford(HGate const& /* op */) { return true; }

class XGate {
public:
    XGate() = default;
    std::string get_type() const { return "x"; }
    std::string get_repr() const { return "x"; }
    size_t get_num_qubits() const { return 1; }
};

inline Operation adjoint(XGate const& op) { return op; }
inline bool is_clifford(XGate const& /* op */) { return true; }

class YGate {
public:
    YGate() = default;
    std::string get_type() const { return "y"; }
    std::string get_repr() const { return "y"; }
    size_t get_num_qubits() const { return 1; }
};

inline Operation adjoint(YGate const& op) { return op; }
inline bool is_clifford(YGate const& /* op */) { return true; }

class ZGate {
public:
    ZGate() = default;
    std::string get_type() const { return "z"; }
    std::string get_repr() const { return "z"; }
    size_t get_num_qubits() const { return 1; }
};

inline Operation adjoint(ZGate const& op) { return op; }
inline bool is_clifford(ZGate const& /* op */) { return true; }

class CXGate {
public:
    CXGate() = default;
    std::string get_type() const { return "cx"; }
    std::string get_repr() const { return "cx"; }
    size_t get_num_qubits() const { return 2; }
};

inline Operation adjoint(CXGate const& op) { return op; }
inline bool is_clifford(CXGate const& /* op */) { return true; }

class CYGate {
public:
    CYGate() = default;
    std::string get_type() const { return "cy"; }
    std::string get_repr() const { return "cy"; }
    size_t get_num_qubits() const { return 2; }
};

inline Operation adjoint(CYGate const& op) { return op; }
inline bool is_clifford(CYGate const& /* op */) { return true; }

class CZGate {
public:
    CZGate() = default;
    std::string get_type() const { return "cz"; }
    std::string get_repr() const { return "cz"; }
    size_t get_num_qubits() const { return 2; }
};

inline Operation adjoint(CZGate const& op) { return op; }
inline bool is_clifford(CZGate const& /* op */) { return true; }

class SwapGate {
public:
    SwapGate() = default;
    std::string get_type() const { return "swap"; }
    std::string get_repr() const { return "swap"; }
    size_t get_num_qubits() const { return 2; }
};

inline Operation adjoint(SwapGate const& op) { return op; }
inline bool is_clifford(SwapGate const& /* op */) { return true; }

class SGate {
public:
    SGate() = default;
    std::string get_type() const { return "s"; }
    std::string get_repr() const { return "s"; }
    size_t get_num_qubits() const { return 1; }
};

class SdgGate {
public:
    SdgGate() = default;
    std::string get_type() const { return "sdg"; }
    std::string get_repr() const { return "sdg"; }
    size_t get_num_qubits() const { return 1; }
};

inline Operation adjoint(SGate const& /* op */) { return SdgGate{}; }
inline Operation adjoint(SdgGate const& /* op */) { return SGate{}; }
inline bool is_clifford(SGate const& /* op */) { return true; }
inline bool is_clifford(SdgGate const& /* op */) { return true; }

}  // namespace qsyn::qcir

namespace qsyn {
template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::LegacyGateType const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::LegacyGateType const& op);
template <>
bool append_to_tableau(qcir::LegacyGateType const& op, experimental::Tableau& tableau, QubitIdList const& qubits);
}  // namespace qsyn

template <>
struct fmt::formatter<qsyn::qcir::GateRotationCategory> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(qsyn::qcir::GateRotationCategory const& type, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{}", qsyn::qcir::gate_type_to_str(type));
    }
};
