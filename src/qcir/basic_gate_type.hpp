/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir basic gate types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./operation.hpp"
#include "./qcir.hpp"

namespace qsyn::qcir {

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
    std::string get_repr() const {
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
inline bool is_clifford(PZGate const& op) {
    return op.get_phase().denominator() <= 2;
}
inline Operation adjoint(PXGate const& op) { return PXGate(-op.get_phase()); }
inline bool is_clifford(PXGate const& op) {
    return op.get_phase().denominator() <= 2;
}
inline Operation adjoint(PYGate const& op) { return PYGate(-op.get_phase()); }
inline bool is_clifford(PYGate const& op) {
    return op.get_phase().denominator() <= 2;
}

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
inline bool is_clifford(RZGate const& op) {
    return op.get_phase().denominator() <= 2;
}
inline Operation adjoint(RXGate const& op) { return RXGate(-op.get_phase()); }
inline bool is_clifford(RXGate const& op) {
    return op.get_phase().denominator() <= 2;
}
inline Operation adjoint(RYGate const& op) { return RYGate(-op.get_phase()); }
inline bool is_clifford(RYGate const& op) {
    return op.get_phase().denominator() <= 2;
}

class ControlGate {
public:
    ControlGate(Operation op, size_t n_ctrls = 1)
        : _op(std::move(op)), _n_ctrls(n_ctrls) {
        DVLAB_ASSERT(n_ctrls > 0,
                     "Cannot instantiate a control gate with zero controls");
    }
    std::string get_type() const {
        return std::string(_n_ctrls, 'c') + _op.get_type();
    }
    std::string get_repr() const {
        return std::string(_n_ctrls, 'c') + _op.get_repr();
    }
    size_t get_num_qubits() const { return _op.get_num_qubits() + _n_ctrls; }

    Operation get_target_operation() const { return _op; }
    void set_target_operation(Operation op) { _op = std::move(op); }

    size_t get_num_ctrls() const { return _n_ctrls; }

private:
    Operation _op;
    size_t _n_ctrls;
};

inline Operation adjoint(ControlGate const& op) {
    return ControlGate(adjoint(op.get_target_operation()), op.get_num_ctrls());
}
inline bool is_clifford(ControlGate const& op) {
    return op.get_num_ctrls() == 1 &&
           (op.get_target_operation() == XGate() ||
            op.get_target_operation() == YGate() ||
            op.get_target_operation() == ZGate());
}

// NOLINTBEGIN(readability-identifier-naming)  // pseudo classes
inline ControlGate CXGate() { return ControlGate(XGate()); }
inline ControlGate CYGate() { return ControlGate(YGate()); }
inline ControlGate CZGate() { return ControlGate(ZGate()); }
inline ControlGate CCXGate() { return ControlGate(XGate(), 2); }
inline ControlGate CCYGate() { return ControlGate(YGate(), 2); }
inline ControlGate CCZGate() { return ControlGate(ZGate(), 2); }
// NOLINTEND(readability-identifier-naming)

}  // namespace qsyn::qcir
