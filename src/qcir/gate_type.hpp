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
    pz,
    rz,
    px,
    rx,
    py,
    ry,
};

using GateType = std::tuple<GateRotationCategory, std::optional<size_t>, std::optional<dvlab::Phase>>;

std::optional<GateType> str_to_gate_type(std::string_view str);
std::string gate_type_to_str(GateRotationCategory category, std::optional<size_t> num_qubits = std::nullopt, std::optional<dvlab::Phase> phase = std::nullopt);
std::string gate_type_to_str(GateType const& type);

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

std::optional<Operation> str_to_operation(std::string const& str, std::vector<dvlab::Phase> const& params = {});

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
        spdlog::error("Operation type is {}, but expected {}", this->get_type(), typeid(T).name());
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

        virtual std::optional<zx::ZXGraph> do_to_zxgraph() const                                           = 0;
        virtual std::optional<tensor::QTensor<double>> do_to_tensor() const                                = 0;
        virtual bool do_append_to_tableau(experimental::Tableau& tableau, QubitIdList const& qubits) const = 0;
    };

    template <typename T>
    struct Model : Concept {
        Model(T value) : value(std::forward<T>(value)) {}
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

class IdGate {
public:
    IdGate() {}
    std::string get_type() const { return "id"; }
    std::string get_repr() const { return "id"; }
    size_t get_num_qubits() const { return 1; }
};

inline Operation adjoint(IdGate const& op) { return op; }
inline bool is_clifford(IdGate const& /* op */) { return true; }

class HGate {
public:
    HGate() = default;
    std::string get_type() const { return "h"; }
    std::string get_repr() const { return "h"; }
    size_t get_num_qubits() const { return 1; }
};

inline Operation adjoint(HGate const& op) { return op; }
inline bool is_clifford(HGate const& /* op */) { return true; }

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

class ECRGate {
public:
    ECRGate() = default;
    std::string get_type() const { return "ecr"; }
    std::string get_repr() const { return "ecr"; }
    size_t get_num_qubits() const { return 2; }
};

inline Operation adjoint(ECRGate const& op) { return op; }
inline bool is_clifford(ECRGate const& /* op */) { return true; }

class PZGate {
public:
    PZGate(dvlab::Phase phase) : _phase(phase) {}
    std::string get_type() const { return "p"; }
    inline std::string get_repr() const {
        if (_phase == dvlab::Phase(1)) {
            return "z";
        }
        if (_phase == dvlab::Phase(1, 2)) {
            return "s";
        }
        if (_phase == dvlab::Phase(-1, 2)) {
            return "sdg";
        }
        if (_phase == dvlab::Phase(1, 4)) {
            return "t";
        }
        if (_phase == dvlab::Phase(-1, 4)) {
            return "tdg";
        }
        return fmt::format("p({})", _phase.get_print_string());
    }
    size_t get_num_qubits() const { return 1; }

    auto get_phase() const { return _phase; }
    void set_phase(dvlab::Phase phase) { _phase = phase; }

private:
    dvlab::Phase _phase;
};

class PXGate {
public:
    PXGate(dvlab::Phase phase) : _phase(phase) {}
    std::string get_type() const { return "px"; }
    std::string get_repr() const {
        if (_phase == dvlab::Phase(1)) {
            return "x";
        }
        if (_phase == dvlab::Phase(1, 2)) {
            return "sx";
        }
        if (_phase == dvlab::Phase(-1, 2)) {
            return "sxdg";
        }
        if (_phase == dvlab::Phase(1, 4)) {
            return "tx";
        }
        if (_phase == dvlab::Phase(-1, 4)) {
            return "txdg";
        }
        return fmt::format("px({})", _phase.get_print_string());
    }
    size_t get_num_qubits() const { return 1; }

    auto get_phase() const { return _phase; }
    void set_phase(dvlab::Phase phase) { _phase = phase; }

private:
    dvlab::Phase _phase;
};

class PYGate {
public:
    PYGate(dvlab::Phase phase) : _phase(phase) {}
    std::string get_type() const { return "py"; }
    std::string get_repr() const {
        if (_phase == dvlab::Phase(1)) {
            return "y";
        }
        if (_phase == dvlab::Phase(1, 2)) {
            return "sy";
        }
        if (_phase == dvlab::Phase(-1, 2)) {
            return "sydg";
        }
        if (_phase == dvlab::Phase(1, 4)) {
            return "ty";
        }
        if (_phase == dvlab::Phase(-1, 4)) {
            return "tydg";
        }
        return fmt::format("py({})", _phase.get_print_string());
    }
    size_t get_num_qubits() const { return 1; }

    auto get_phase() const { return _phase; }
    void set_phase(dvlab::Phase phase) { _phase = phase; }

private:
    dvlab::Phase _phase;
};

// NOLINTBEGIN(readability-identifier-naming)  // pseudo classes
inline PZGate ZGate() { return PZGate(dvlab::Phase(1)); }
inline PZGate SGate() { return PZGate(dvlab::Phase(1, 2)); }
inline PZGate SdgGate() { return PZGate(dvlab::Phase(-1, 2)); }
inline PZGate TGate() { return PZGate(dvlab::Phase(1, 4)); }
inline PZGate TdgGate() { return PZGate(dvlab::Phase(-1, 4)); }
inline PXGate XGate() { return PXGate(dvlab::Phase(1)); }
inline PXGate SXGate() { return PXGate(dvlab::Phase(1, 2)); }
inline PXGate SXdgGate() { return PXGate(dvlab::Phase(-1, 2)); }
inline PXGate TXGate() { return PXGate(dvlab::Phase(1, 4)); }
inline PXGate TXdgGate() { return PXGate(dvlab::Phase(-1, 4)); }
inline PYGate YGate() { return PYGate(dvlab::Phase(1)); }
inline PYGate SYGate() { return PYGate(dvlab::Phase(1, 2)); }
inline PYGate SYdgGate() { return PYGate(dvlab::Phase(-1, 2)); }
inline PYGate TYGate() { return PYGate(dvlab::Phase(1, 4)); }
inline PYGate TYdgGate() { return PYGate(dvlab::Phase(-1, 4)); }
// NOLINTEND(readability-identifier-naming)

inline Operation adjoint(PZGate const& op) { return PZGate(-op.get_phase()); }
inline bool is_clifford(PZGate const& op) { return op.get_phase().denominator() <= 2; }
inline Operation adjoint(PXGate const& op) { return PXGate(-op.get_phase()); }
inline bool is_clifford(PXGate const& op) { return op.get_phase().denominator() <= 2; }
inline Operation adjoint(PYGate const& op) { return PYGate(-op.get_phase()); }
inline bool is_clifford(PYGate const& op) { return op.get_phase().denominator() <= 2; }

class RZGate {
public:
    RZGate(dvlab::Phase phase) : _phase(phase) {}
    std::string get_type() const { return "rz"; }
    std::string get_repr() const {
        return fmt::format("rz({})", _phase.get_print_string());
    }
    size_t get_num_qubits() const { return 1; }

    auto get_phase() const { return _phase; }
    void set_phase(dvlab::Phase phase) { _phase = phase; }

private:
    dvlab::Phase _phase;
};

class RXGate {
public:
    RXGate(dvlab::Phase phase) : _phase(phase) {}
    std::string get_type() const { return "rx"; }
    std::string get_repr() const {
        return fmt::format("rx({})", _phase.get_print_string());
    }
    size_t get_num_qubits() const { return 1; }

    auto get_phase() const { return _phase; }
    void set_phase(dvlab::Phase phase) { _phase = phase; }

private:
    dvlab::Phase _phase;
};

class RYGate {
public:
    RYGate(dvlab::Phase phase) : _phase(phase) {}
    std::string get_type() const { return "ry"; }
    std::string get_repr() const {
        return fmt::format("ry({})", _phase.get_print_string());
    }
    size_t get_num_qubits() const { return 1; }

    auto get_phase() const { return _phase; }
    void set_phase(dvlab::Phase phase) { _phase = phase; }

private:
    dvlab::Phase _phase;
};

inline Operation adjoint(RZGate const& op) { return RZGate(-op.get_phase()); }
inline bool is_clifford(RZGate const& op) { return op.get_phase().denominator() <= 2; }
inline Operation adjoint(RXGate const& op) { return RXGate(-op.get_phase()); }
inline bool is_clifford(RXGate const& op) { return op.get_phase().denominator() <= 2; }
inline Operation adjoint(RYGate const& op) { return RYGate(-op.get_phase()); }
inline bool is_clifford(RYGate const& op) { return op.get_phase().denominator() <= 2; }

}  // namespace qsyn::qcir

namespace qsyn {
template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::LegacyGateType const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::LegacyGateType const& op);
template <>
bool append_to_tableau(qcir::LegacyGateType const& op, experimental::Tableau& tableau, QubitIdList const& qubits);

template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::IdGate const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::IdGate const& op);
template <>
bool append_to_tableau(qcir::IdGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits);

template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::HGate const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::HGate const& op);
template <>
bool append_to_tableau(qcir::HGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits);

template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::SwapGate const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::SwapGate const& op);
template <>
bool append_to_tableau(qcir::SwapGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits);

template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::ECRGate const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::ECRGate const& op);
template <>
bool append_to_tableau(qcir::ECRGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits);

template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::PZGate const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::PZGate const& op);
template <>
bool append_to_tableau(qcir::PZGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits);

template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::PXGate const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::PXGate const& op);
template <>
bool append_to_tableau(qcir::PXGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits);

template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::PYGate const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::PYGate const& op);
template <>
bool append_to_tableau(qcir::PYGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits);

template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::RZGate const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::RZGate const& op);
template <>
bool append_to_tableau(qcir::RZGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits);

template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::RXGate const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::RXGate const& op);
template <>
bool append_to_tableau(qcir::RXGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits);

template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::RYGate const& op);
template <>
std::optional<tensor::QTensor<double>> to_tensor(qcir::RYGate const& op);
template <>
bool append_to_tableau(qcir::RYGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits);
}  // namespace qsyn

template <>
struct fmt::formatter<qsyn::qcir::GateRotationCategory> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(qsyn::qcir::GateRotationCategory const& type, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{}", qsyn::qcir::gate_type_to_str(type));
    }
};
