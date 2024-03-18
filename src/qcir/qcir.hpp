/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "qcir/qcir_gate.hpp"
#include "qcir/qcir_qubit.hpp"
#include "qsyn/qsyn_type.hpp"
#include "spdlog/common.h"
#include "util/ordered_hashmap.hpp"

namespace dvlab {

class Phase;

}

namespace qsyn::qcir {

class QCir;
class Operation;

struct QubitInfo;

enum class QCirDrawerType {
    text,
    mpl,
    latex,
    latex_source,
};

inline std::optional<QCirDrawerType>
str_to_qcir_drawer_type(std::string const& str) {
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

class QCir {  // NOLINT(hicpp-special-member-functions,
              // cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    using QubitIdType = qsyn::QubitIdType;
    QCir() {}
    QCir(size_t n_qubits) { add_qubits(n_qubits); }
    ~QCir() = default;
    QCir(QCir const& other);
    QCir(QCir&& other) noexcept = default;

    QCir& operator=(QCir copy) {
        copy.swap(*this);
        return *this;
    }

    void swap(QCir& other) noexcept {
        std::swap(_gate_id, other._gate_id);
        std::swap(_dirty, other._dirty);
        std::swap(_filename, other._filename);
        std::swap(_gate_set, other._gate_set);
        std::swap(_procedures, other._procedures);
        std::swap(_qubits, other._qubits);
        std::swap(_gate_list, other._gate_list);
        std::swap(_id_to_gates, other._id_to_gates);
        std::swap(_predecessors, other._predecessors);
        std::swap(_successors, other._successors);
    }

    friend void swap(QCir& a, QCir& b) noexcept { a.swap(b); }

    // Access functions
    size_t get_num_qubits() const { return _qubits.size(); }
    size_t get_num_gates() const { return _id_to_gates.size(); }
    size_t calculate_depth() const;
    std::unordered_map<size_t, size_t> calculate_gate_times() const;
    std::vector<QCirQubit*> const& get_qubits() const { return _qubits; }

    /**
     * @brief Get the gates as a topologically ordered list
     *
     * @return std::vector<QCirGate*> const&
     */
    std::vector<QCirGate*> const& get_gates() const {
        _update_topological_order();
        return _gate_list;
    }

    QCirGate* get_gate(std::optional<size_t> gid) const;
    QCirQubit* get_qubit(QubitIdType qid) const;
    std::string get_filename() const { return _filename; }
    std::vector<std::string> const& get_procedures() const { return _procedures; }
    std::string get_gate_set() const { return _gate_set; }

    bool is_empty() const { return _qubits.empty() || _id_to_gates.empty(); }

    void set_filename(std::string f) { _filename = std::move(f); }
    void add_procedures(std::vector<std::string> const& ps) {
        _procedures.insert(_procedures.end(), ps.begin(), ps.end());
    }
    void add_procedure(std::string const& p) { _procedures.emplace_back(p); }
    void set_gate_set(std::string g) { _gate_set = std::move(g); }

    void reset();
    QCir& compose(QCir const& other);
    QCir& tensor_product(QCir const& other);
    // Member functions about circuit construction
    QCirQubit* push_qubit();
    QCirQubit* insert_qubit(QubitIdType id);
    void add_qubits(size_t num);
    bool remove_qubit(QubitIdType qid);
    size_t append(Operation const& op, QubitIdList const& bits);
    size_t prepend(Operation const& op, QubitIdList const& bits);
    bool remove_gate(size_t id);

    bool read_qcir_file(std::filesystem::path const& filepath);
    bool read_qc(std::filesystem::path const& filepath);
    bool read_qasm(std::filesystem::path const& filepath);

    bool write_qasm(std::filesystem::path const& filepath) const;

    bool draw(QCirDrawerType drawer,
              std::filesystem::path const& output_path = "",
              float scale                              = 1.0f) const;

    void print_gate_statistics(bool detail = false) const;

    void translate(QCir const& qcir, std::string const& gate_set);

    void append(QCir const& other,
                std::map<QubitIdType, QubitIdType> const& qubit_map = {});

    void update_gate_time() const;
    void print_zx_form_topological_order();

    void adjoint_inplace();

    bool print_topological_order();

    void concat(
        QCir const& other,
        std::map<QubitIdType /* new */, QubitIdType /* orig */> const& qubit_map);

    // Member functions about circuit reporting
    void print_gates(bool print_neighbors       = false,
                     std::span<size_t> gate_ids = {}) const;
    void print_qcir() const;
    // bool print_gate_as_diagram(size_t id, bool show_time) const;
    void print_circuit_diagram(
        spdlog::level::level_enum lvl = spdlog::level::off) const;
    void print_qcir_info() const;

    std::optional<size_t> get_predecessor(std::optional<size_t> gate_id,
                                          size_t pin) const;
    std::optional<size_t> get_successor(std::optional<size_t> gate_id,
                                        size_t pin) const;
    std::vector<std::optional<size_t>>
    get_predecessors(std::optional<size_t> gate_id) const;
    std::vector<std::optional<size_t>>
    get_successors(std::optional<size_t> gate_id) const;

    // additional APIs to make qcir::QCir an qcir::Operation
    std::string get_type() const { return "qcir"; }
    std::string get_repr() const { return _filename; }

private:
    size_t _gate_id = 0;
    std::string _filename;
    std::string _gate_set;
    std::vector<std::string> _procedures;
    std::vector<QCirQubit*> _qubits;
    dvlab::utils::ordered_hashmap<size_t, std::unique_ptr<QCirGate>> _id_to_gates;
    std::unordered_map<size_t, std::vector<std::optional<size_t>>> _predecessors;
    std::unordered_map<size_t, std::vector<std::optional<size_t>>> _successors;

    std::vector<QCirGate*> mutable _gate_list;  // a cache for topologically
                                                // ordered gates. This member
                                                // should not be accessed
                                                // directly. Instead, use
                                                // get_gates() to ensure the cache
                                                // is up-to-date.
    bool mutable _dirty = true;                 // mark if the topological order is dirty

    void _update_topological_order() const;

    void _set_predecessor(size_t gate_id, size_t pin,
                          std::optional<size_t> pred = std::nullopt);
    void _set_successor(size_t gate_id, size_t pin,
                        std::optional<size_t> succ = std::nullopt);
    void _set_predecessors(size_t gate_id,
                           std::vector<std::optional<size_t>> const& preds);
    void _set_successors(size_t gate_id,
                         std::vector<std::optional<size_t>> const& succs);
    void _connect(size_t gid1, size_t gid2, QubitIdType qubit);
};

std::string to_qasm(QCir const& qcir);
std::unordered_map<std::string, size_t>
get_gate_statistics(qcir::QCir const& qcir);

bool is_clifford(qcir::QCir const& qcir);
Operation adjoint(qcir::QCir const& qcir);

}  // namespace qsyn::qcir

template <>
struct fmt::formatter<qsyn::qcir::QCirDrawerType> {
    auto parse(format_parse_context& ctx) { return ctx.begin(); }
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
