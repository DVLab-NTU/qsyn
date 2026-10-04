// SPDX-License-Identifier: MIT
//
// Quantum gates -- modeled after qsyn's `Operation` / `QCirGate` design:
// every gate is an `Operation` carrying a name, a matrix payload, and an
// ordered qubit list.  Concrete gate classes (HGate, TGate, ...) refine
// the base type with extra metadata, but all of them participate in the
// same polymorphic hierarchy through `std::shared_ptr<Operation>`.
//
// This mirrors pygridsynth's `QuantumGate` family while exposing a
// qsyn-style abstract base that can later be extended with controlled
// gates, multi-qubit primitives, etc.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cppgridsynth/mpfloat.hpp"

namespace cppgridsynth {

// =====================================================================
//  Identifier of a gate type, used for fast dispatch.
// =====================================================================
enum class GateKind {
    H,
    T,
    S,
    X,
    W,
    Rz,
    Rx,
    Cx,
    Custom,
};

// =====================================================================
//  Complex number using MPFloat for arbitrary precision.
// =====================================================================
struct MPComplex {
    MPFloat re;
    MPFloat im;

    MPComplex() : re(), im() {}
    MPComplex(MPFloat r) : re(std::move(r)), im() {}
    MPComplex(MPFloat r, MPFloat i) : re(std::move(r)), im(std::move(i)) {}
    MPComplex(double r) : re(r), im() {}
    MPComplex(double r, double i) : re(r), im(i) {}

    MPComplex operator+(const MPComplex& o) const { return {re + o.re, im + o.im}; }
    MPComplex operator-(const MPComplex& o) const { return {re - o.re, im - o.im}; }
    MPComplex operator-() const { return {-re, -im}; }
    MPComplex operator*(const MPComplex& o) const {
        return {re * o.re - im * o.im, re * o.im + im * o.re};
    }
    MPComplex conj() const { return {re, -im}; }
    MPFloat abs() const { return sqrt(re * re + im * im); }
    static MPComplex from_phase(const MPFloat& theta) { return {cos(theta), sin(theta)}; }
};

inline MPComplex operator*(const MPFloat& s, const MPComplex& z) { return {s * z.re, s * z.im}; }
inline MPComplex operator*(double s, const MPComplex& z) { return MPFloat(s) * z; }

// =====================================================================
//  Matrix<MPComplex>: a tiny dense matrix used to materialise the gate
//  payload.  Stored row-major.
// =====================================================================
class CMatrix {
public:
    CMatrix() : rows_(0), cols_(0) {}
    CMatrix(size_t rows, size_t cols)
        : rows_(rows), cols_(cols), data_(rows * cols) {}

    static CMatrix identity(size_t n);
    static CMatrix zeros(size_t r, size_t c) { return CMatrix(r, c); }

    size_t rows() const noexcept { return rows_; }
    size_t cols() const noexcept { return cols_; }

    MPComplex&       operator()(size_t r, size_t c)       { return data_[r * cols_ + c]; }
    const MPComplex& operator()(size_t r, size_t c) const { return data_[r * cols_ + c]; }

    CMatrix operator*(const CMatrix& o) const;

private:
    size_t rows_;
    size_t cols_;
    std::vector<MPComplex> data_;
};

// =====================================================================
//  Operation: abstract base, qsyn-style.
// =====================================================================
class Operation {
public:
    Operation(GateKind kind, std::string name, std::vector<int> qubits)
        : kind_(kind), name_(std::move(name)), qubits_(std::move(qubits)) {}
    virtual ~Operation() = default;

    GateKind                  kind()       const noexcept { return kind_; }
    const std::string&        name()       const noexcept { return name_; }
    const std::vector<int>&   qubits()     const noexcept { return qubits_; }
    size_t                    num_qubits() const noexcept { return qubits_.size(); }

    // Materialise this gate as a 2^n x 2^n matrix at the current working
    // precision.  Concrete gates override this to provide their unitary.
    virtual CMatrix matrix() const = 0;

    // Short single-letter symbol for this gate ("H", "T", ...).
    virtual std::string to_simple_str() const = 0;

    virtual std::string to_string() const;

protected:
    GateKind kind_;
    std::string name_;
    std::vector<int> qubits_;
};

using OperationPtr = std::shared_ptr<Operation>;

// =====================================================================
//  Single-qubit base.
// =====================================================================
class SingleQubitGate : public Operation {
public:
    SingleQubitGate(GateKind kind, std::string name, int target)
        : Operation(kind, std::move(name), std::vector<int>{target}) {}
    int target_qubit() const noexcept { return qubits_.front(); }
};

// =====================================================================
//  Concrete Clifford+T set + extras.
// =====================================================================
class HGate final : public SingleQubitGate {
public:
    explicit HGate(int target) : SingleQubitGate(GateKind::H, "H", target) {}
    CMatrix matrix() const override;
    std::string to_simple_str() const override { return "H"; }
};

class TGate final : public SingleQubitGate {
public:
    explicit TGate(int target) : SingleQubitGate(GateKind::T, "T", target) {}
    CMatrix matrix() const override;
    std::string to_simple_str() const override { return "T"; }
};

class SGate final : public SingleQubitGate {
public:
    explicit SGate(int target) : SingleQubitGate(GateKind::S, "S", target) {}
    CMatrix matrix() const override;
    std::string to_simple_str() const override { return "S"; }
};

class SXGate final : public SingleQubitGate {
public:
    explicit SXGate(int target) : SingleQubitGate(GateKind::X, "X", target) {}
    CMatrix matrix() const override;
    std::string to_simple_str() const override { return "X"; }
};

// "W" gate: a global phase exp(i·π/4).  qsyn-style "no-qubit" operation.
class WGate final : public Operation {
public:
    WGate() : Operation(GateKind::W, "W", {}) {}
    CMatrix matrix() const override;
    std::string to_simple_str() const override { return "W"; }
};

class RzGate final : public SingleQubitGate {
public:
    RzGate(MPFloat theta, int target)
        : SingleQubitGate(GateKind::Rz, "Rz", target),
          theta_(std::move(theta)) {}

    const MPFloat& theta() const noexcept { return theta_; }
    CMatrix matrix() const override;
    std::string to_simple_str() const override { return "Rz"; }

private:
    MPFloat theta_;
};

class RxGate final : public SingleQubitGate {
public:
    RxGate(MPFloat theta, int target)
        : SingleQubitGate(GateKind::Rx, "Rx", target),
          theta_(std::move(theta)) {}

    const MPFloat& theta() const noexcept { return theta_; }
    CMatrix matrix() const override;
    std::string to_simple_str() const override { return "Rx"; }

private:
    MPFloat theta_;
};

class CxGate final : public Operation {
public:
    CxGate(int control, int target)
        : Operation(GateKind::Cx, "CX", {control, target}) {}
    int control_qubit() const noexcept { return qubits_[0]; }
    int target_qubit()  const noexcept { return qubits_[1]; }
    CMatrix matrix() const override;
    std::string to_simple_str() const override { return "CX"; }
};

// Phase used by the `W` gate: π/4.
inline MPFloat w_phase() { return MPFloat::pi() / MPFloat(4); }

// Constructors for primitive 2x2/4x4 unitaries (mirrors pygridsynth helpers).
CMatrix Rz_matrix(const MPFloat& theta);
CMatrix Rx_matrix(const MPFloat& theta);
CMatrix cnot01();
CMatrix cnot10();

}  // namespace cppgridsynth
