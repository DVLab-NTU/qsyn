// SPDX-License-Identifier: MIT
#include "cppgridsynth/normal_form.hpp"

#include <sstream>
#include <stdexcept>

#include "cppgridsynth/mymath.hpp"

namespace cppgridsynth {

namespace {

// Tables transcribed from pygridsynth/normal_form.py.
constexpr std::array<std::pair<int, int>, 8> CONJ2_TABLE = {{
    {0, 0}, {0, 0}, {1, 0}, {3, 2}, {2, 0}, {2, 4}, {3, 0}, {1, 6},
}};

constexpr std::array<std::array<int, 4>, 24> CONJ3_TABLE = {{
    {0, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 2, 0}, {0, 0, 3, 0},
    {0, 1, 0, 0}, {0, 1, 1, 0}, {0, 1, 2, 0}, {0, 1, 3, 0},
    {1, 0, 0, 0}, {2, 0, 3, 6}, {1, 1, 2, 2}, {2, 1, 3, 6},
    {1, 0, 2, 0}, {2, 1, 1, 0}, {1, 1, 0, 6}, {2, 0, 1, 4},
    {2, 0, 0, 0}, {1, 1, 3, 4}, {2, 1, 0, 0}, {1, 0, 1, 2},
    {2, 1, 2, 2}, {1, 1, 1, 0}, {2, 0, 2, 6}, {1, 0, 3, 2},
}};

constexpr std::array<std::array<int, 4>, 24> CINV_TABLE = {{
    {0, 0, 0, 0}, {0, 0, 3, 0}, {0, 0, 2, 0}, {0, 0, 1, 0},
    {0, 1, 0, 0}, {0, 1, 1, 6}, {0, 1, 2, 4}, {0, 1, 3, 2},
    {2, 0, 0, 0}, {1, 0, 1, 2}, {2, 1, 0, 0}, {1, 1, 3, 4},
    {2, 1, 1, 2}, {1, 1, 1, 6}, {2, 0, 2, 2}, {1, 0, 3, 4},
    {1, 0, 0, 0}, {2, 1, 3, 6}, {1, 1, 2, 2}, {2, 0, 3, 6},
    {1, 0, 2, 0}, {2, 1, 1, 6}, {1, 1, 0, 2}, {2, 0, 1, 6},
}};

struct TConj { Axis axis; int c; int d; };
constexpr std::array<TConj, 6> TCONJ_TABLE = {{
    {Axis::I, 0, 0}, {Axis::I, 1, 7}, {Axis::H, 3, 3},
    {Axis::H, 2, 0}, {Axis::SH, 0, 5}, {Axis::SH, 1, 4},
}};

inline std::pair<int, int> conj2(int c, int b) {
    return CONJ2_TABLE[(c << 1) | b];
}
inline std::array<int, 4> conj3(int b, int c, int a) {
    return CONJ3_TABLE[(a << 3) | (b << 2) | c];
}
inline std::array<int, 4> cinv(int a, int b, int c) {
    return CINV_TABLE[(a << 3) | (b << 2) | c];
}
inline TConj tconj(int a, int b) { return TCONJ_TABLE[(a << 1) | b]; }

}  // namespace

Clifford::Clifford(int a, int b, int c, int d) {
    a_ = ((a % 3) + 3) % 3;
    b_ = b & 1;
    c_ = c & 0b11;
    d_ = d & 0b111;
}

Clifford Clifford::from_str(const std::string& g) {
    if (g == "H") return CLIFFORD_H;
    if (g == "S") return CLIFFORD_S;
    if (g == "X") return CLIFFORD_X;
    if (g == "W") return CLIFFORD_W;
    throw std::invalid_argument("Clifford::from_str: " + g);
}

Clifford Clifford::from_gate(const Operation& g) {
    switch (g.kind()) {
        case GateKind::H: return CLIFFORD_H;
        case GateKind::S: return CLIFFORD_S;
        case GateKind::X: return CLIFFORD_X;
        case GateKind::W: return CLIFFORD_W;
        default:
            throw std::invalid_argument("Clifford::from_gate: unsupported gate");
    }
}

Clifford Clifford::operator*(const Clifford& o) const {
    auto cf = conj3(b_, c_, o.a_);
    int a1 = cf[0], b1 = cf[1], c1 = cf[2], d1 = cf[3];
    auto cf2 = conj2(c1, o.b_);
    int c2 = cf2.first, d2 = cf2.second;
    return Clifford(a_ + a1, b1 + o.b_, c2 + o.c_, d2 + d1 + d_ + o.d_);
}

Clifford Clifford::inv() const {
    auto cf = cinv(a_, b_, c_);
    return Clifford(cf[0], cf[1], cf[2], cf[3] - d_);
}

std::pair<Axis, Clifford> Clifford::decompose_coset() const {
    if (a_ == 0) return {Axis::I, *this};
    if (a_ == 1) return {Axis::H, CLIFFORD_H.inv() * (*this)};
    if (a_ == 2) return {Axis::SH, CLIFFORD_SH.inv() * (*this)};
    throw std::logic_error("Clifford::decompose_coset");
}

std::pair<Axis, Clifford> Clifford::decompose_tconj() const {
    auto t = tconj(a_, b_);
    return {t.axis, Clifford(0, b_, t.c + c_, t.d + d_)};
}

QuantumCircuit Clifford::to_circuit(const std::vector<int>& wires) const {
    auto [axis, c] = decompose_coset();
    QuantumCircuit circuit;
    if (axis == Axis::H) {
        circuit.append(std::make_shared<HGate>(wires[0]));
    } else if (axis == Axis::SH) {
        circuit.append(std::make_shared<SGate>(wires[0]));
        circuit.append(std::make_shared<HGate>(wires[0]));
    }
    for (int i = 0; i < c.b(); ++i) circuit.append(std::make_shared<SXGate>(wires[0]));
    for (int i = 0; i < c.c(); ++i) circuit.append(std::make_shared<SGate>(wires[0]));
    for (int i = 0; i < c.d(); ++i) circuit.append(std::make_shared<WGate>());
    return circuit;
}

std::string Clifford::to_string() const {
    std::ostringstream os;
    os << "Clifford(" << a_ << ", " << b_ << ", " << c_ << ", " << d_ << ")";
    return os.str();
}

// ---- NormalForm ---------------------------------------------------------
NormalForm::NormalForm(std::vector<Syllable> syl, Clifford c, MPFloat phase)
    : syllables_(std::move(syl)), c_(c), phase_(std::move(phase)) {}

void NormalForm::append_gate(const Operation& g) {
    if (g.kind() == GateKind::T) {
        auto [axis, new_c] = c_.decompose_tconj();
        if (axis == Axis::I) {
            if (syllables_.empty()) {
                syllables_.push_back(Syllable::T);
            } else if (syllables_.back() == Syllable::T) {
                syllables_.pop_back();
                c_ = CLIFFORD_S * new_c;
            } else if (syllables_.back() == Syllable::HT) {
                syllables_.pop_back();
                c_ = CLIFFORD_HS * new_c;
            } else if (syllables_.back() == Syllable::SHT) {
                syllables_.pop_back();
                c_ = CLIFFORD_SHS * new_c;
            }
        } else if (axis == Axis::H) {
            syllables_.push_back(Syllable::HT);
            c_ = new_c;
        } else if (axis == Axis::SH) {
            syllables_.push_back(Syllable::SHT);
            c_ = new_c;
        }
    } else {
        c_ = c_ * Clifford::from_gate(g);
    }
}

NormalForm NormalForm::from_circuit(const QuantumCircuit& circuit) {
    NormalForm nf({}, CLIFFORD_I, circuit.phase());
    for (const auto& g : circuit) {
        nf.append_gate(*g);
    }
    return nf;
}

QuantumCircuit NormalForm::to_circuit(const std::vector<int>& wires) const {
    QuantumCircuit circuit(phase_);
    for (auto syl : syllables_) {
        switch (syl) {
            case Syllable::T:
                circuit.append(std::make_shared<TGate>(wires[0]));
                break;
            case Syllable::HT:
                circuit.append(std::make_shared<HGate>(wires[0]));
                circuit.append(std::make_shared<TGate>(wires[0]));
                break;
            case Syllable::SHT:
                circuit.append(std::make_shared<SGate>(wires[0]));
                circuit.append(std::make_shared<HGate>(wires[0]));
                circuit.append(std::make_shared<TGate>(wires[0]));
                break;
            default: break;
        }
    }
    QuantumCircuit tail = c_.to_circuit(wires);
    circuit += tail;
    return circuit;
}

const Clifford CLIFFORD_I (0, 0, 0, 0);
const Clifford CLIFFORD_X (0, 1, 0, 0);
const Clifford CLIFFORD_H (1, 0, 1, 5);
const Clifford CLIFFORD_S (0, 0, 1, 0);
const Clifford CLIFFORD_W (0, 0, 0, 1);
const Clifford CLIFFORD_SH = CLIFFORD_S * CLIFFORD_H;
const Clifford CLIFFORD_HS = CLIFFORD_H * CLIFFORD_S;
const Clifford CLIFFORD_SHS = CLIFFORD_S * CLIFFORD_H * CLIFFORD_S;

}  // namespace cppgridsynth
