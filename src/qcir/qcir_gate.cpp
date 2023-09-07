/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_gate.hpp"

#include <cassert>
#include <iomanip>
#include <string>

using namespace std;

size_t SINGLE_DELAY = 1;
size_t DOUBLE_DELAY = 2;
size_t SWAP_DELAY = 6;
size_t MULTIPLE_DELAY = 5;

std::ostream& operator<<(std::ostream& stream, GateType const& type) {
    return stream << static_cast<typename std::underlying_type<GateType>::type>(type);
}

std::unordered_map<std::string, GateType> STR_TO_GATE_TYPE = {
    {"x", GateType::x},
    {"rz", GateType::rz},
    {"h", GateType::h},
    {"sx", GateType::sx},
    {"cnot", GateType::cx},
    {"cx", GateType::cx},
    {"id", GateType::id},
};

std::unordered_map<GateType, std::string> GATE_TYPE_TO_STR = {
    {GateType::id, "ID"},
    // NOTE - Multi-control rotate
    {GateType::mcp, "MCP"},
    {GateType::mcrz, "MCRZ"},
    {GateType::mcpx, "MCPX"},
    {GateType::mcrx, "MCRX"},
    {GateType::mcpy, "MCPY"},
    {GateType::mcry, "MCRY"},
    {GateType::h, "H"},
    // NOTE - MCP(Z)
    {GateType::ccz, "CCZ"},
    {GateType::cz, "CZ"},
    {GateType::p, "P"},
    {GateType::z, "Z"},
    {GateType::s, "S"},
    {GateType::sdg, "SDG"},
    {GateType::t, "T"},
    {GateType::tdg, "TDG"},
    {GateType::rz, "RZ"},
    // NOTE - MCPX
    {GateType::ccx, "CCX"},
    {GateType::cx, "CX"},
    {GateType::swap, "SWAP"},
    {GateType::px, "PX"},
    {GateType::x, "X"},
    {GateType::sx, "SX"},
    {GateType::rx, "RX"},
    // NOTE - MCPY
    {GateType::y, "Y"},
    {GateType::py, "PY"},
    {GateType::sy, "SY"},
    {GateType::ry, "RY"},
};

/**
 * @brief Get delay of gate
 *
 * @return size_t
 */
size_t QCirGate::get_delay() const {
    if (get_type() == GateType::swap)
        return SWAP_DELAY;
    if (_qubits.size() == 1)
        return SINGLE_DELAY;
    else if (_qubits.size() == 2)
        return DOUBLE_DELAY;
    else
        return MULTIPLE_DELAY;
}

/**
 * @brief Get Qubit.
 *
 * @param qubit
 * @return BitInfo
 */
QubitInfo QCirGate::get_qubit(size_t qubit) const {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]._qubit == qubit)
            return _qubits[i];
    }
    cerr << "Not Found" << endl;
    return _qubits[0];
}

/**
 * @brief Add qubit to a gate
 *
 * @param qubit
 * @param isTarget
 */
void QCirGate::add_qubit(size_t qubit, bool is_target) {
    QubitInfo temp = {._qubit = qubit, ._parent = nullptr, ._child = nullptr, ._isTarget = is_target};
    // _qubits.emplace_back(temp);
    if (is_target)
        _qubits.emplace_back(temp);
    else
        _qubits.emplace(_qubits.begin(), temp);
}

/**
 * @brief Set the bit of target
 *
 * @param qubit
 */
void QCirGate::set_target_qubit(size_t qubit) {
    _qubits[_qubits.size() - 1]._qubit = qubit;
}

/**
 * @brief Set parent to the gate on qubit.
 *
 * @param qubit
 * @param p
 */
void QCirGate::set_parent(size_t qubit, QCirGate* p) {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]._qubit == qubit) {
            _qubits[i]._parent = p;
            break;
        }
    }
}

/**
 * @brief Add dummy child c to gate
 *
 * @param c
 */
void QCirGate::add_dummy_child(QCirGate* c) {
    QubitInfo temp = {._qubit = 0, ._parent = nullptr, ._child = c, ._isTarget = false};
    _qubits.emplace_back(temp);
}

/**
 * @brief Set child to gate on qubit.
 *
 * @param qubit
 * @param c
 */
void QCirGate::set_child(size_t qubit, QCirGate* c) {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]._qubit == qubit) {
            _qubits[i]._child = c;
            break;
        }
    }
}

/**
 * @brief Print Gate brief information
 */
void QCirGate::print_gate() const {
    cout << "ID:" << right << setw(4) << _id;
    cout << " (" << right << setw(3) << get_type_str() << ") ";
    cout << "     Time: " << right << setw(4) << _time << "     Qubit: ";
    for (size_t i = 0; i < _qubits.size(); i++) {
        cout << right << setw(3) << _qubits[i]._qubit << " ";
    }
    if (get_type() == GateType::p || get_type() == GateType::rx || get_type() == GateType::ry || get_type() == GateType::rz)
        cout << "      Phase: " << right << setw(4) << get_phase() << " ";
    cout << endl;
}

/**
 * @brief Print single-qubit gate.
 *
 * @param gtype
 * @param showTime
 */
void QCirGate::_print_single_qubit_gate(string gtype, bool show_time) const {
    QubitInfo info = get_qubits()[0];
    string qubit_info = "Q" + to_string(info._qubit);
    string parent_info = "";
    if (info._parent == nullptr)
        parent_info = "Start";
    else
        parent_info = ("G" + to_string(info._parent->get_id()));
    string child_info = "";
    if (info._child == nullptr)
        child_info = "End";
    else
        child_info = ("G" + to_string(info._child->get_id()));
    for (size_t i = 0; i < parent_info.size() + qubit_info.size() + 2; i++)
        cout << " ";
    cout << " ┌─";
    for (size_t i = 0; i < gtype.size(); i++) cout << "─";
    cout << "─┐ " << endl;
    cout << qubit_info << " " << parent_info << " ─┤ " << gtype << " ├─ " << child_info << endl;
    for (size_t i = 0; i < parent_info.size() + qubit_info.size() + 2; i++)
        cout << " ";
    cout << " └─";
    for (size_t i = 0; i < gtype.size(); i++) cout << "─";
    cout << "─┘ " << endl;
    if (gtype == "RX" || gtype == "RY" || gtype == "RZ" || gtype == "P")
        cout << "Rotate Phase: " << _phase << endl;
    if (show_time)
        cout << "Execute at t= " << get_time() << endl;
}

/**
 * @brief Print multiple-qubit gate.
 *
 * @param gtype
 * @param showRotate
 * @param showTime
 */
void QCirGate::_print_multiple_qubits_gate(string gtype, bool show_rotation, bool show_time) const {
    size_t padding_size = (gtype.size() - 1) / 2;
    string max_qubit = to_string(max_element(_qubits.begin(), _qubits.end(), [](QubitInfo const a, QubitInfo const b) {
                                     return a._qubit < b._qubit;
                                 })->_qubit);

    vector<string> parents;
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (get_qubits()[i]._parent == nullptr)
            parents.emplace_back("Start");
        else
            parents.emplace_back("G" + to_string(get_qubits()[i]._parent->get_id()));
    }
    string max_parent = *max_element(parents.begin(), parents.end(), [](string const a, string const b) {
        return a.size() < b.size();
    });

    for (size_t i = 0; i < _qubits.size(); i++) {
        QubitInfo info = get_qubits()[i];
        string qubit_info = "Q";
        for (size_t j = 0; j < max_qubit.size() - to_string(info._qubit).size(); j++)
            qubit_info += " ";
        qubit_info += to_string(info._qubit);
        string parent_info = "";
        if (info._parent == nullptr)
            parent_info = "Start";
        else
            parent_info = ("G" + to_string(info._parent->get_id()));
        for (size_t k = 0; k < max_parent.size() - (parents[i].size()); k++)
            parent_info += " ";
        string child_info = "";
        if (info._child == nullptr)
            child_info = "End";
        else
            child_info = ("G" + to_string(info._child->get_id()));
        if (info._isTarget) {
            for (size_t j = 0; j < max_qubit.size() + max_parent.size() + 3; j++)
                cout << " ";
            cout << " ┌─";
            for (size_t j = 0; j < padding_size; j++) cout << "─";
            if (_qubits.size() > 1)
                cout << "┴";
            else
                for (size_t j = 0; j < gtype.size(); j++) cout << "─";

            for (size_t j = 0; j < padding_size; j++) cout << "─";
            cout << "─┐ " << endl;
            cout << qubit_info << " " << parent_info << " ─┤ " << gtype << " ├─ " << child_info << endl;
            for (size_t j = 0; j < max_qubit.size() + max_parent.size() + 3; j++)
                cout << " ";
            cout << " └─";
            for (size_t j = 0; j < gtype.size(); j++) cout << "─";
            cout << "─┘ " << endl;
        } else {
            cout << qubit_info << " " << parent_info << " ──";
            for (size_t j = 0; j < padding_size; j++) cout << "─";
            cout << "─●─";
            for (size_t j = 0; j < padding_size; j++) cout << "─";
            cout << "── " << child_info << endl;
        }
    }
    if (show_rotation)
        cout << "Rotate Phase: " << _phase << endl;
    if (show_time)
        cout << "Execute at t= " << get_time() << endl;
}