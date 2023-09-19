/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <filesystem>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "qcir/qcir_gate.hpp"
#include "qcir/qcir_qubit.hpp"
#include "qsyn/qsyn_type.hpp"

namespace dvlab {

class Phase;

}

namespace qsyn::qcir {

class QCir;

struct QubitInfo;

enum class QCirDrawerType {
    text,
    mpl,
    latex,
    latex_source,
};

inline std::optional<QCirDrawerType> str_to_qcir_drawer_type(std::string const& str) {
    if (str == "text") {
        return QCirDrawerType::text;
    }
    if (str == "mpl") {
        return QCirDrawerType::mpl;
    }
    if (str == "latex") {
        return QCirDrawerType::latex;
    }
    if (str == "latex_source") {
        return QCirDrawerType::latex_source;
    }
    return std::nullopt;
}

class QCir {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    using QubitIdType = qsyn::QubitIdType;
    QCir() {}
    ~QCir() = default;
    QCir(QCir const& other);
    QCir(QCir&& other) noexcept = default;

    QCir& operator=(QCir copy) {
        copy.swap(*this);
        return *this;
    }

    void swap(QCir& other) noexcept {
        std::swap(_gate_id, other._gate_id);
        std::swap(_qubit_id, other._qubit_id);
        std::swap(_dirty, other._dirty);
        std::swap(_global_dfs_counter, other._global_dfs_counter);
        std::swap(_filename, other._filename);
        std::swap(_procedures, other._procedures);
        std::swap(_qgates, other._qgates);
        std::swap(_qubits, other._qubits);
        std::swap(_topological_order, other._topological_order);
    }

    friend void swap(QCir& a, QCir& b) noexcept {
        a.swap(b);
    }

    // Access functions
    size_t get_num_qubits() const { return _qubits.size(); }
    size_t get_depth();
    std::vector<QCirQubit*> const& get_qubits() const { return _qubits; }
    std::vector<QCirGate*> const& get_topologically_ordered_gates() const { return _topological_order; }
    std::vector<QCirGate*> const& get_gates() const { return _qgates; }
    QCirGate* get_gate(size_t gid) const;
    QCirQubit* get_qubit(size_t qid) const;
    std::string get_filename() const { return _filename; }
    std::vector<std::string> const& get_procedures() const { return _procedures; }

    void set_filename(std::string f) { _filename = std::move(f); }
    void add_procedures(std::vector<std::string> const& ps) { _procedures.insert(_procedures.end(), ps.begin(), ps.end()); }
    void add_procedure(std::string const& p) { _procedures.emplace_back(p); }

    //
    void reset();
    QCir* compose(QCir const& other);
    QCir* tensor_product(QCir const& other);
    // Member functions about circuit construction
    QCirQubit* push_qubit();
    QCirQubit* insert_qubit(QubitIdType id);
    void add_qubits(size_t num);
    bool remove_qubit(QubitIdType qid);
    QCirGate* add_gate(std::string type, QubitIdList bits, dvlab::Phase phase, bool append);
    QCirGate* add_single_rz(QubitIdType bit, dvlab::Phase phase, bool append);
    bool remove_gate(size_t id);

    bool read_qcir_file(std::string const& filename);
    bool read_qc(std::string const& filename);
    bool read_qasm(std::string const& filename);
    bool read_qsim(std::string const& filename);
    bool read_quipper(std::string const& filename);

    bool write_qasm(std::string const& filename);

    bool draw(QCirDrawerType drawer, std::filesystem::path const& output_path = "", float scale = 1.0f);

    std::vector<int> count_gates(bool detail = false, bool print = true);

    void update_gate_time() const;
    void print_zx_form_topological_order();

    // DFS functions
    template <typename F>
    void topological_traverse(F lambda) const {
        if (_dirty) {
            update_topological_order();
            _dirty = false;
        }
        for_each(_topological_order.begin(), _topological_order.end(), lambda);
    }

    bool print_topological_order();

    // pass a function F (public functions) into for_each
    // lambdaFn such as mappingToZX / updateGateTime
    std::vector<QCirGate*> const& update_topological_order() const;

    // Member functions about circuit reporting
    void print_depth();
    void print_gates();
    void print_qcir();
    bool print_gate_info(size_t, bool);
    void print_summary();
    void print_qubits();
    void print_qcir_info();

private:
    void _dfs(QCirGate* curr_gate) const;

    // For Copy
    void _set_next_gate_id(size_t id) { _gate_id = id; }
    void _set_next_qubit_id(QubitIdType qid) { _qubit_id = qid; }

    size_t _gate_id                      = 0;
    QubitIdType _qubit_id                = 0;
    bool mutable _dirty                  = true;
    unsigned mutable _global_dfs_counter = 0;
    std::string _filename;
    std::vector<std::string> _procedures;

    std::vector<QCirGate*> _qgates;
    std::vector<QCirQubit*> _qubits;
    std::vector<QCirGate*> mutable _topological_order;
};

}  // namespace qsyn::qcir

template <>
struct fmt::formatter<qsyn::qcir::QCirDrawerType> {
    auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    auto format(qsyn::qcir::QCirDrawerType const& type, format_context& ctx) {
        using qsyn::qcir::QCirDrawerType;
        switch (type) {
            case QCirDrawerType::text:
                return fmt::format_to(ctx.out(), "text");
            case QCirDrawerType::mpl:
                return fmt::format_to(ctx.out(), "mpl");
            case QCirDrawerType::latex:
                return fmt::format_to(ctx.out(), "latex");
            case QCirDrawerType::latex_source:
            default:
                return fmt::format_to(ctx.out(), "latex_source");
        }
    }
};
