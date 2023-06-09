/****************************************************************************
  FileName     [ extract.cpp ]
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "extract.h"

#include <assert.h>  // for assert

#include <memory>

#include "mappingEQChecker.h"
#include "simplify.h"  // for Simplifier
#include "zxGraph.h"   // for ZXGraph
#include "zxRules.h"   // for PivotBoundary

using namespace std;

bool SORT_FRONTIER = 0;
bool SORT_NEIGHBORS = 1;
bool PERMUTE_QUBITS = 1;
bool FILTER_DUPLICATED_CXS = 1;
size_t BLOCK_SIZE = 5;
size_t OPTIMIZE_LEVEL = 2;
extern size_t verbose;

/**
 * @brief Construct a new Extractor:: Extractor object
 *
 * @param g
 * @param c
 * @param d
 */
Extractor::Extractor(ZXGraph* g, QCir* c, std::optional<Device> d) : _graph(g), _device(d), _deviceBackup(d) {
    _logicalCircuit = (c == nullptr) ? new QCir(-1) : c;
    _physicalCircuit = toPhysical() ? new QCir(-1) : nullptr;
    initialize(c == nullptr);
    _cntCXFiltered = 0;
    _cntCXIter = 0;
    _cntSwap = 0;
}

/**
 * @brief Initialize the extractor. Set ZXGraph to QCir qubit map.
 *
 */
void Extractor::initialize(bool fromEmpty) {
    if (verbose >= 4) cout << "Initialize" << endl;
    size_t cnt = 0;
    for (auto& o : _graph->getOutputs()) {
        if (!o->getFirstNeighbor().first->isBoundary()) {
            o->getFirstNeighbor().first->setQubit(o->getQubit());
            _frontier.emplace(o->getFirstNeighbor().first);
        }
        _qubitMap[o->getQubit()] = cnt;
        if (fromEmpty)
            _logicalCircuit->addQubit(1);
        cnt += 1;
    }

    // NOTE - get zx to qc qubit mapping
    _frontier.sort([](const ZXVertex* a, const ZXVertex* b) {
        return a->getQubit() < b->getQubit();
    });

    updateNeighbors();
    for (auto& v : _graph->getVertices()) {
        if (_graph->isGadgetLeaf(v)) {
            _axels.emplace(v->getFirstNeighbor().first);
        }
    }
    if (verbose >= 8) {
        printFrontier();
        printNeighbors();
        _graph->printQubits();
        _logicalCircuit->printQubits();
    }
}

/**
 * @brief Extract the graph into circuit
 *
 * @return QCir*
 */
QCir* Extractor::extract() {
    if (!extractionLoop(-1))
        return nullptr;
    else
        cout << "Finish extracting!" << endl;
    if (verbose >= 8) {
        _logicalCircuit->printQubits();
        _graph->printQubits();
    }

    if (PERMUTE_QUBITS) {
        permuteQubit();
        if (verbose >= 8) {
            _logicalCircuit->printQubits();
            _graph->printQubits();
        }
    }
    // cout << "Iteration of extract CX: " << _cntCXIter << endl;
    // cout << "Total Filtered CX count: " << _cntCXFiltered << endl;
    _logicalCircuit->addProcedure("ZX2QC", _graph->getProcedures());
    _graph->addProcedure("ZX2QC");
    _logicalCircuit->setFileName(_graph->getFileName());

    return _logicalCircuit;
}

/**
 * @brief Extraction Loop
 *
 * @param max_iter perform max_iter iterations
 * @return true if successfully extracted
 * @return false if not
 */
bool Extractor::extractionLoop(size_t max_iter) {
    while (max_iter > 0) {
        cleanFrontier();
        updateNeighbors();

        if (_frontier.empty()) break;

        if (removeGadget()) {
            if (verbose >= 4) cout << "Gadget(s) are removed." << endl;
            if (verbose >= 8) {
                printFrontier();
                _graph->printQubits();
                _logicalCircuit->printQubits();
            }
            continue;
        }

        if (containSingleNeighbor()) {
            if (verbose >= 4) cout << "Construct an easy matrix." << endl;
            _biAdjacency.fromZXVertices(_frontier, _neighbors);
        } else {
            if (verbose >= 4) cout << "Perform Gaussian Elimination." << endl;
            extractCXs();
        }
        if (extractHsFromM2() == 0) {
            cerr << "Error: No Candidate Found!! in extractHsFromM2" << endl;
            _biAdjacency.printMatrix();
            return false;
        }
        _biAdjacency.reset();
        _cnots.clear();

        if (verbose >= 8) {
            printFrontier();
            printNeighbors();
            _graph->printQubits();
            _logicalCircuit->printQubits();
        }

        if (max_iter != size_t(-1)) max_iter--;
    }
    return true;
}

/**
 * @brief Clean frontier. Contain extract singles and CZs. Used in extract.
 *
 */
void Extractor::cleanFrontier() {
    if (verbose >= 4) cout << "Clean Frontier" << endl;
    // NOTE - Edge and Phase
    extractSingles();
    // NOTE - CZs
    extractCZs();
}

/**
 * @brief Extract single qubit gates, i.e. z-rotate family and H. Used in clean frontier.
 *
 */
void Extractor::extractSingles() {
    if (verbose >= 4) cout << "Extract Singles" << endl;
    vector<pair<ZXVertex*, ZXVertex*>> toggleList;
    for (ZXVertex* o : _graph->getOutputs()) {
        if (o->getFirstNeighbor().second == EdgeType::HADAMARD) {
            prependSingleQubitGate("h", _qubitMap[o->getQubit()], Phase(0));
            toggleList.emplace_back(o, o->getFirstNeighbor().first);
        }
        Phase ph = o->getFirstNeighbor().first->getPhase();
        if (ph != Phase(0)) {
            prependSingleQubitGate("rotate", _qubitMap[o->getQubit()], ph);
            o->getFirstNeighbor().first->setPhase(Phase(0));
        }
    }
    for (auto& [s, t] : toggleList) {
        _graph->addEdge(s, t, EdgeType::SIMPLE);
        _graph->removeEdge(s, t, EdgeType::HADAMARD);
    }
    if (verbose >= 8) {
        _logicalCircuit->printQubits();
        _graph->printQubits();
    }
}

/**
 * @brief Extract CZs from frontier. Used in clean frontier.
 *
 * @param check check no phase in the frontier if true
 * @return true
 * @return false
 */
bool Extractor::extractCZs(bool check) {
    if (verbose >= 4) cout << "Extract CZs" << endl;

    if (check) {
        for (auto& f : _frontier) {
            if (f->getPhase() != Phase(0)) {
                cout << "Note: frontier contains phases, extract them first." << endl;
                return false;
            }
            for (auto& [n, e] : f->getNeighbors()) {
                if (_graph->getOutputs().contains(n) && e == EdgeType::HADAMARD) {
                    cout << "Note: frontier contains hadamard edge to boundary, extract them first." << endl;
                    return false;
                }
            }
        }
    }

    vector<pair<ZXVertex*, ZXVertex*>> removeList;

    for (auto itr = _frontier.begin(); itr != _frontier.end(); itr++) {
        for (auto jtr = next(itr); jtr != _frontier.end(); jtr++) {
            if ((*itr)->isNeighbor((*jtr))) {
                removeList.emplace_back((*itr), (*jtr));
            }
        }
    }
    vector<Operation> ops;
    for (const auto& [s, t] : removeList) {
        _graph->removeEdge(s, t, EdgeType::HADAMARD);
        Operation op(GateType::CZ, Phase(0), {_qubitMap[s->getQubit()], _qubitMap[t->getQubit()]}, {});
        ops.emplace_back(op);
    }
    if (ops.size() > 0) {
        prependSeriesGates(ops);
    }
    if (verbose >= 8) {
        _logicalCircuit->printQubits();
        _graph->printQubits();
    }

    return true;
}

/**
 * @brief Extract CXs
 *
 * @param strategy (0: directly by result of Gaussian Elimination, 1-default: Filter Duplicated CNOT)
 */
void Extractor::extractCXs(size_t strategy) {
    _cntCXIter++;
    biadjacencyElimination();
    updateGraphByMatrix();

    if (verbose >= 4) cout << "Extract CXs" << endl;
    unordered_map<size_t, ZXVertex*> frontId2Vertex;
    size_t cnt = 0;
    for (auto& f : _frontier) {
        frontId2Vertex[cnt] = f;
        cnt++;
    }

    for (auto& [t, c] : _cnots) {
        // NOTE - targ and ctrl are opposite here
        size_t ctrl = _qubitMap[frontId2Vertex[c]->getQubit()];
        size_t targ = _qubitMap[frontId2Vertex[t]->getQubit()];
        if (verbose >= 4) cout << "Add CX: " << ctrl << " " << targ << endl;
        prependDoubleQubitGate("cx", {ctrl, targ}, Phase(0));
    }
}

/**
 * @brief Extract Hadamard if singly connected vertex in frontier is found
 *
 * @param check if true, check frontier is cleaned and axels not connected to frontiers
 * @return size_t
 */
size_t Extractor::extractHsFromM2(bool check) {
    if (verbose >= 4) cout << "Extract Hs" << endl;

    if (check) {
        if (!frontierIsCleaned()) {
            cout << "Note: frontier is dirty, please clean it first." << endl;
            return 0;
        }
        if (axelInNeighbors()) {
            cout << "Note: axel(s) are in the neighbors, please remove gadget(s) first." << endl;
            return 0;
        }
        _biAdjacency.fromZXVertices(_frontier, _neighbors);
    }

    unordered_map<size_t, ZXVertex*> frontId2Vertex;
    unordered_map<size_t, ZXVertex*> neighId2Vertex;
    size_t cnt = 0;
    for (auto& f : _frontier) {
        frontId2Vertex[cnt] = f;
        cnt++;
    }
    cnt = 0;
    for (auto& n : _neighbors) {
        neighId2Vertex[cnt] = n;
        cnt++;
    }

    // NOTE - Store pairs to be modified
    vector<pair<ZXVertex*, ZXVertex*>> frontNeighPairs;

    for (size_t row = 0; row < _biAdjacency.numRows(); ++row) {
        if (!_biAdjacency[row].isOneHot()) continue;

        for (size_t col = 0; col < _biAdjacency.numCols(); col++) {
            if (_biAdjacency[row][col] == 1) {
                frontNeighPairs.emplace_back(frontId2Vertex[row], neighId2Vertex[col]);
                break;
            }
        }
    }

    for (auto& [f, n] : frontNeighPairs) {
        // NOTE - Add Hadamard according to the v of frontier (row)
        prependSingleQubitGate("h", _qubitMap[f->getQubit()], Phase(0));
        // NOTE - Set #qubit and #col according to the old frontier
        n->setQubit(f->getQubit());
        n->setCol(f->getCol());

        // NOTE - Connect edge between boundary and neighbor
        for (auto& [bound, ep] : f->getNeighbors()) {
            if (bound->isBoundary()) {
                _graph->addEdge(bound, n, ep);
                break;
            }
        }
        // NOTE - Replace frontier by neighbor
        _frontier.erase(f);
        _frontier.emplace(n);
        _graph->removeVertex(f);
    }

    if (check && frontNeighPairs.size() == 0) {
        cerr << "Error: no candidate found!!" << endl;
        printMatrix();
    }
    return frontNeighPairs.size();
}

/**
 * @brief Remove gadget according to Pivot Boundary Rule
 *
 * @param check if true, check the frontier is clean
 * @return true if gadget(s) are removed
 * @return false if not
 */
bool Extractor::removeGadget(bool check) {
    if (verbose >= 4) cout << "Remove Gadget" << endl;

    if (check) {
        if (_frontier.empty()) {
            cout << "Note: no vertex left." << endl;
            return false;
        }
        if (!frontierIsCleaned()) {
            cout << "Note: frontier is dirty, please clean it first." << endl;
            return false;
        }
    }

    Simplifier simp(make_unique<PivotBoundary>(), _graph);
    auto pivotBoundaryRule = static_cast<PivotBoundary*>(simp.getRule());

    if (verbose >= 8) _graph->printVertices();
    if (verbose >= 5) {
        printFrontier();
        printAxels();
    }
    bool gadgetRemoved = false;
    for (auto& n : _neighbors) {
        if (!_axels.contains(n)) {
            continue;
        }
        for (auto& [candidate, _] : n->getNeighbors()) {
            if (_frontier.contains(candidate)) {
                PivotBoundary::MatchTypeVec matches;
                matches.push_back({n, candidate});

                pivotBoundaryRule->setMatchTypeVec(matches);

                ZXVertex* targetBoundary = nullptr;
                for (auto& [boundary, _] : candidate->getNeighbors()) {
                    if (boundary->isBoundary()) {
                        pivotBoundaryRule->addBoundary(boundary);
                        targetBoundary = boundary;
                        break;
                    }
                }
                _axels.erase(n);
                _frontier.erase(candidate);

                simp.rewrite();
                simp.amend();

                pivotBoundaryRule->clearBoundary();

                if (targetBoundary != nullptr)
                    _frontier.emplace(targetBoundary->getFirstNeighbor().first);
                // REVIEW - qubit_map
                gadgetRemoved = true;
                break;
            }
        }
    }
    if (verbose >= 8) _graph->printVertices();
    if (verbose >= 5) {
        printFrontier();
        printAxels();
    }
    return gadgetRemoved;
}

/**
 * @brief Swap columns (order of neighbors) to put the most of them on the diagonal of the bi-adjacency matrix
 *
 */
void Extractor::columnOptimalSwap() {
    // NOTE - Swap columns of matrix and order of neighbors

    _rowInfo.clear();
    _colInfo.clear();

    size_t rowCnt = _biAdjacency.numRows();

    size_t colCnt = _biAdjacency.numCols();
    _rowInfo.resize(rowCnt);
    _colInfo.resize(colCnt);

    for (size_t i = 0; i < rowCnt; i++) {
        for (size_t j = 0; j < colCnt; j++) {
            if (_biAdjacency[i][j] == 1) {
                _rowInfo[i].emplace(j);
                _colInfo[j].emplace(i);
            }
        }
    }

    Target target;
    target = findColumnSwap(target);

    set<size_t> colSet, left, right, targKey, targVal;
    for (size_t i = 0; i < colCnt; i++) colSet.emplace(i);
    for (auto& [k, v] : target) {
        targKey.emplace(k);
        targVal.emplace(v);
    }

    set_difference(colSet.begin(), colSet.end(), targVal.begin(), targVal.end(), inserter(left, left.end()));
    set_difference(colSet.begin(), colSet.end(), targKey.begin(), targKey.end(), inserter(right, right.end()));
    vector<size_t> lvec(left.begin(), left.end());
    vector<size_t> rvec(right.begin(), right.end());
    for (size_t i = 0; i < lvec.size(); i++) {
        target[rvec[i]] = lvec[i];
    }
    Target perm;
    for (auto& [k, v] : target) {
        perm[v] = k;
    }
    vector<ZXVertex*> nebVec(_neighbors.begin(), _neighbors.end());
    vector<ZXVertex*> newNebVec = nebVec;
    for (size_t i = 0; i < nebVec.size(); i++) {
        newNebVec[i] = nebVec[perm[i]];
    }
    _neighbors.clear();
    for (auto& v : newNebVec) _neighbors.emplace(v);
}

/**
 * @brief Find the swap target. Used in function columnOptimalSwap
 *
 * @param target
 * @return Target (unordered_map of swaps)
 */
Extractor::Target Extractor::findColumnSwap(Target target) {
    size_t rowCnt = _rowInfo.size();
    // size_t colCnt = _colInfo.size();

    set<size_t> claimedCols;
    set<size_t> claimedRows;
    for (auto& [key, value] : target) {
        claimedCols.emplace(key);
        claimedRows.emplace(value);
    }

    while (true) {
        int min_index = -1;
        set<size_t> min_options;
        for (size_t i = 0; i < 1000; i++) min_options.emplace(i);
        bool foundCol = false;
        for (size_t i = 0; i < rowCnt; i++) {
            if (claimedRows.contains(i)) continue;
            set<size_t> freeCols;
            // NOTE - find the free columns
            set_difference(_rowInfo[i].begin(), _rowInfo[i].end(), claimedCols.begin(), claimedCols.end(), inserter(freeCols, freeCols.end()));
            if (freeCols.size() == 1) {
                // NOTE - pop the only element
                size_t j = *(freeCols.begin());
                freeCols.erase(j);

                target[j] = i;
                claimedCols.emplace(j);
                claimedRows.emplace(i);
                foundCol = true;
                break;
            }

            if (freeCols.size() == 0) {
                if (verbose >= 5) cout << "Note: no free column for column optimal swap!!" << endl;
                Target t;
                return t;  // NOTE - Contradiction
            }

            for (auto& j : freeCols) {
                set<size_t> freeRows;
                set_difference(_colInfo[j].begin(), _colInfo[j].end(), claimedRows.begin(), claimedRows.end(), inserter(freeRows, freeRows.end()));
                if (freeRows.size() == 1) {
                    target[j] = i;  // NOTE - j can only be connected to i
                    claimedCols.emplace(j);
                    claimedRows.emplace(i);
                    foundCol = true;
                    break;
                }
            }
            if (foundCol) break;
            if (freeCols.size() < min_options.size()) {
                min_index = i;
                min_options = freeCols;
            }
        }

        if (!foundCol) {
            bool done = true;
            for (size_t r = 0; r < rowCnt; ++r) {
                if (!claimedRows.contains(r)) {
                    done = false;
                    break;
                }
            }
            if (done) {
                return target;
            }
            if (min_index == -1) {
                cerr << "Error: this shouldn't happen ever" << endl;
                assert(false);
            }
            // NOTE -  depth-first search
            Target copiedTarget = target;
            if (verbose >= 8) cout << "Backtracking on " << min_index << endl;

            for (auto& idx : min_options) {
                if (verbose >= 8) cout << "> trying option" << idx << endl;
                copiedTarget[idx] = min_index;
                Target newTarget = findColumnSwap(copiedTarget);
                if (!newTarget.empty())
                    return newTarget;
            }
            if (verbose >= 8) cout << "Unsuccessful" << endl;
            return target;
        }
    }
}

/**
 * @brief Perform elimination on biadjacency matrix
 *
 * @param check if true, check the frontier is clean and no axels connecting to frontier
 * @return true if check pass
 * @return false if not
 */
bool Extractor::biadjacencyElimination(bool check) {
    if (check) {
        if (!frontierIsCleaned()) {
            cout << "Note: frontier is dirty, please clean it first." << endl;
            return false;
        }
        if (axelInNeighbors()) {
            cout << "Note: axel(s) are in the neighbors, please remove gadget(s) first." << endl;
            return false;
        }
    }

    if (SORT_FRONTIER == true) {
        _frontier.sort([](const ZXVertex* a, const ZXVertex* b) {
            return a->getQubit() < b->getQubit();
        });
    }
    if (SORT_NEIGHBORS == true) {
        // REVIEW - Do not know why sort in here would be better
        _neighbors.sort([](const ZXVertex* a, const ZXVertex* b) {
            return a->getId() < b->getId();
        });
    }

    vector<M2::Oper> greedyOpers;

    _biAdjacency.fromZXVertices(_frontier, _neighbors);
    M2 greedyMat = _biAdjacency;
    ZXVertexList backupNeighbors = _neighbors;
    if (OPTIMIZE_LEVEL > 1) {
        // NOTE - opt = 2 or 3
        greedyOpers = greedyReduction(greedyMat);
        for (auto oper : greedyOpers) {
            greedyMat.xorOper(oper.first, oper.second, true);
        }
    }

    if (OPTIMIZE_LEVEL != 2) {
        // NOTE - opt = 0, 1 or 3
        columnOptimalSwap();
        _biAdjacency.fromZXVertices(_frontier, _neighbors);

        if (OPTIMIZE_LEVEL == 0) {
            _biAdjacency.gaussianElimSkip(BLOCK_SIZE, true, true);
            if (FILTER_DUPLICATED_CXS) {
                size_t old = _cntCXFiltered;
                while (true) {
                    size_t reduce = _biAdjacency.filterDuplicatedOps();
                    _cntCXFiltered += reduce;
                    if (reduce == 0) break;
                }
                if (verbose >= 4) cout << "Filter " << _cntCXFiltered - old << " CXs. Total: " << _cntCXFiltered << endl;
            }
            _cnots = _biAdjacency.getOpers();
        } else if (OPTIMIZE_LEVEL == 1 || OPTIMIZE_LEVEL == 3) {
            size_t minCnots = size_t(-1);
            M2 bestMatrix;
            for (size_t blk = 1; blk < _biAdjacency.numCols(); blk++) {
                blockElimination(bestMatrix, minCnots, blk);
            }
            if (OPTIMIZE_LEVEL == 1) {
                _biAdjacency = bestMatrix;
                _cnots = _biAdjacency.getOpers();
            } else {
                size_t nGaussOpers = bestMatrix.getOpers().size();
                size_t nSingleOneRows = accumulate(bestMatrix.getMatrix().begin(), bestMatrix.getMatrix().end(), 0,
                                                   [](size_t acc, const Row& r) { return acc + size_t(r.isOneHot()); });
                // NOTE - opers per extractable rows for Gaussian is bigger than greedy
                bool selectGreedy = float(nGaussOpers) / float(nSingleOneRows) > float(greedyOpers.size()) - 0.1;
                if (!greedyOpers.empty() && selectGreedy) {
                    _biAdjacency = greedyMat;
                    _cnots = greedyMat.getOpers();
                    _neighbors = backupNeighbors;
                    if (verbose > 3) cout << "Found greedy reduction with " << _cnots.size() << " CX(s)" << endl;
                } else {
                    _biAdjacency = bestMatrix;
                    _cnots = _biAdjacency.getOpers();
                    if (verbose > 3) cout << "Gaussian elimination with " << _cnots.size() << " CX(s)" << endl;
                }
            }
        } else {
            cerr << "Error: Wrong Optimize Level" << endl;
            abort();
        }

    } else {
        // NOTE - OPT level 2
        _biAdjacency = greedyMat;
        _cnots = greedyMat.getOpers();
    }

    // TODO - update matrix to correct one
    return true;
}

/**
 * @brief Perform Gaussian Elimination with block size `blockSize`
 *
 * @param bestMatrix Currently best matrix
 * @param minCnots Minimum value
 * @param blockSize
 */
void Extractor::blockElimination(M2& bestMatrix, size_t& minCnots, size_t blockSize) {
    M2 copiedMatrix = _biAdjacency;
    copiedMatrix.gaussianElimSkip(blockSize, true, true);
    if (FILTER_DUPLICATED_CXS) {
        while (true) {
            size_t reduce = copiedMatrix.filterDuplicatedOps();
            _cntCXFiltered += reduce;
            if (reduce == 0) break;
        }
    }
    if (copiedMatrix.getOpers().size() < minCnots) {
        minCnots = copiedMatrix.getOpers().size();
        bestMatrix = copiedMatrix;
    }
}

void Extractor::blockElimination(size_t& bestBlock, M2& bestMatrix, size_t& minCost, size_t blockSize) {
    M2 copiedMatrix = _biAdjacency;
    copiedMatrix.gaussianElimSkip(blockSize, true, true);
    if (FILTER_DUPLICATED_CXS) {
        while (true) {
            size_t reduce = copiedMatrix.filterDuplicatedOps();
            _cntCXFiltered += reduce;
            if (reduce == 0) break;
        }
    }

    // NOTE - Construct Duostra Input
    unordered_map<size_t, ZXVertex*> frontId2Vertex;
    size_t cnt = 0;
    for (auto& f : _frontier) {
        frontId2Vertex[cnt] = f;
        cnt++;
    }
    vector<Operation> ops;
    for (auto& [t, c] : copiedMatrix.getOpers()) {
        // NOTE - targ and ctrl are opposite here
        size_t ctrl = _qubitMap[frontId2Vertex[c]->getQubit()];
        size_t targ = _qubitMap[frontId2Vertex[t]->getQubit()];
        if (verbose > 4) cout << "Add CX: " << ctrl << " " << targ << endl;
        Operation op(GateType::CX, Phase(0), {ctrl, targ}, {});
        ops.emplace_back(op);
    }

    // NOTE - Get Mapping result, Device is passed by copy
    Duostra duo(ops, _graph->getNumOutputs(), _device.value(), false, false, true);
    size_t depth = duo.flow(true);
    if (verbose > 4) cout << blockSize << ", depth:" << depth << ", #cx: " << ops.size() << endl;
    if (depth < minCost) {
        minCost = depth;
        bestMatrix = copiedMatrix;
        bestBlock = blockSize;
    }
}

/**
 * @brief Permute qubit if input and output are not match
 *
 */
void Extractor::permuteQubit() {
    if (verbose >= 4) cout << "Permute Qubit" << endl;
    // REVIEW - ordered_hashmap?
    unordered_map<size_t, size_t> swapMap;     // o to i
    unordered_map<size_t, size_t> swapInvMap;  // i to o
    bool matched = true;
    for (auto& o : _graph->getOutputs()) {
        if (o->getNumNeighbors() != 1) {
            cout << "Note: output is not connected to only one vertex" << endl;
            return;
        }
        ZXVertex* i = o->getFirstNeighbor().first;
        if (!_graph->getInputs().contains(i)) {
            cout << "Note: output is not connected to input" << endl;
            return;
        }
        if (i->getQubit() != o->getQubit()) {
            matched = false;
        }
        swapMap[o->getQubit()] = i->getQubit();
    }

    if (matched) return;

    for (auto& [o, i] : swapMap) {
        swapInvMap[i] = o;
    }
    for (auto& [o, i] : swapMap) {
        if (o == i) continue;
        size_t t2 = swapInvMap.at(o);
        prependSwapGate(_qubitMap[o], _qubitMap[t2], _logicalCircuit);
        swapMap[t2] = i;
        swapInvMap[i] = t2;
    }
    for (auto& o : _graph->getOutputs()) {
        _graph->removeAllEdgesBetween(o->getFirstNeighbor().first, o);
    }
    for (auto& o : _graph->getOutputs()) {
        for (auto& i : _graph->getInputs()) {
            if (o->getQubit() == i->getQubit()) {
                _graph->addEdge(o, i, EdgeType::SIMPLE);
                break;
            }
        }
    }
}

/**
 * @brief Update neighbors according to frontier
 *
 */
void Extractor::updateNeighbors() {
    _neighbors.clear();
    vector<ZXVertex*> rmVs;

    for (auto& f : _frontier) {
        size_t numBoundaries = count_if(
            f->getNeighbors().begin(),
            f->getNeighbors().end(),
            [](const NeighborPair& nbp) { return nbp.first->isBoundary(); });

        if (numBoundaries != 2) continue;

        if (f->getNumNeighbors() == 2) {
            // NOTE - Remove
            for (auto& [b, ep] : f->getNeighbors()) {
                if (_graph->getInputs().contains(b)) {
                    if (ep == EdgeType::HADAMARD) {
                        prependSingleQubitGate("h", _qubitMap[f->getQubit()], Phase(0));
                    }
                    break;
                }
            }
            rmVs.push_back(f);
        } else {
            for (auto [b, ep] : f->getNeighbors()) {  // The pass-by-copy is deliberate. Pass by ref will cause segfault
                if (_graph->getInputs().contains(b)) {
                    _graph->addBuffer(b, f, ep);
                    break;
                }
            }
        }
    }

    for (auto& v : rmVs) {
        if (verbose >= 8) cout << "Remove " << v->getId() << " (q" << v->getQubit() << ") from frontiers." << endl;
        _frontier.erase(v);
        _graph->addEdge(v->getFirstNeighbor().first, v->getSecondNeighbor().first, EdgeType::SIMPLE);
        _graph->removeVertex(v);
    }

    for (auto& f : _frontier) {
        for (auto& [n, _] : f->getNeighbors()) {
            if (!n->isBoundary() && !_frontier.contains(n))
                _neighbors.emplace(n);
        }
    }
}

/**
 * @brief Update graph according to bi-adjacency matrix
 *
 * @param et EdgeType, default: EdgeType::HADAMARD
 */
void Extractor::updateGraphByMatrix(EdgeType et) {
    size_t r = 0;
    if (verbose >= 4) cout << "Update Graph by Matrix" << endl;
    for (auto& f : _frontier) {
        size_t c = 0;
        for (auto& nb : _neighbors) {
            if (_biAdjacency[r][c] == 1 && !f->isNeighbor(nb)) {  // NOTE - Should connect but not connected
                _graph->addEdge(f, nb, et);
            } else if (_biAdjacency[r][c] == 0 && f->isNeighbor(nb)) {  // NOTE - Should not connect but connected
                _graph->removeEdge(f, nb, et);
            }
            c++;
        }
        r++;
    }
}

/**
 * @brief Create bi-adjacency matrix fron frontier and neighbors
 *
 */
void Extractor::createMatrix() {
    _biAdjacency.fromZXVertices(_frontier, _neighbors);
}

/**
 * @brief Prepend single-qubit gate to circuit. If _device is given, directly map to physical device.
 *
 * @param type
 * @param qubit logical
 * @param phase
 */
void Extractor::prependSingleQubitGate(string type, size_t qubit, Phase phase) {
    if (type == "rotate") {
        _logicalCircuit->addSingleRZ(qubit, phase, false);
        // if (toPhysical()) {
        //     size_t physicalQId = _device.value().getPhysicalbyLogical(qubit);
        //     assert(physicalQId != ERROR_CODE);
        //     _device.value().applySingleQubitGate(physicalQId);
        //     _physicalCircuit->addSingleRZ(physicalQId, phase, false);
        // }
    } else {
        _logicalCircuit->addGate(type, {qubit}, phase, false);
        // if (toPhysical()) {
        //     size_t physicalQId = _device.value().getPhysicalbyLogical(qubit);
        //     assert(physicalQId != ERROR_CODE);
        //     _device.value().applySingleQubitGate(physicalQId);
        //     _physicalCircuit->addGate(type, {physicalQId}, phase, false);
        // }
    }
    // if (toPhysical()) _device.value().printStatus();
}

/**
 * @brief Prepend double-qubit gate to circuit. If _device is given, directly map to physical device.
 *
 * @param type
 * @param qubits
 * @param phase
 */
void Extractor::prependDoubleQubitGate(string type, const vector<size_t>& qubits, Phase phase) {
    assert(qubits.size() == 2);
    _logicalCircuit->addGate(type, qubits, phase, false);
}

/**
 * @brief Prepend series of gates.
 *
 * @param logical
 * @param physical
 */
void Extractor::prependSeriesGates(const std::vector<Operation>& logical, const std::vector<Operation>& physical) {
    for (const auto& gates : logical) {
        tuple<size_t, size_t> qubits = gates.getQubits();
        _logicalCircuit->addGate(gateType2Str[gates.getType()], {get<0>(qubits), get<1>(qubits)}, gates.getPhase(), false);
    }

    for (const auto& gates : physical) {
        tuple<size_t, size_t> qubits = gates.getQubits();
        if (gates.getType() == GateType::SWAP) {
            prependSwapGate(get<0>(qubits), get<1>(qubits), _physicalCircuit);
            _cntSwap++;
        } else
            _physicalCircuit->addGate(gateType2Str[gates.getType()], {get<0>(qubits), get<1>(qubits)}, gates.getPhase(), false);
    }
}

/**
 * @brief Prepend swap gate. Decompose into three CXs
 *
 * @param q0 logical
 * @param q1 logical
 * @param circuit
 */
void Extractor::prependSwapGate(size_t q0, size_t q1, QCir* circuit) {
    // NOTE - No qubit permutation in Physical Circuit
    circuit->addGate("cx", {q0, q1}, Phase(0), false);
    circuit->addGate("cx", {q1, q0}, Phase(0), false);
    circuit->addGate("cx", {q0, q1}, Phase(0), false);
}

/**
 * @brief Check whether the frontier is clean
 *
 * @return true if clean
 * @return false if dirty
 */
bool Extractor::frontierIsCleaned() {
    for (auto& f : _frontier) {
        if (f->getPhase() != Phase(0)) return false;
        for (auto& [n, e] : f->getNeighbors()) {
            if (_frontier.contains(n)) return false;
            if (_graph->getOutputs().contains(n) && e == EdgeType::HADAMARD) return false;
        }
    }
    return true;
}

/**
 * @brief Check any axel in neighbors
 *
 * @return true
 * @return false
 */
bool Extractor::axelInNeighbors() {
    for (auto& n : _neighbors) {
        if (_axels.contains(n)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check whether the frontier contains a vertex has only a single neighbor (boundary excluded).
 *
 * @return true
 * @return false
 */
bool Extractor::containSingleNeighbor() {
    for (auto& f : _frontier) {
        if (f->getNumNeighbors() == 2)
            return true;
    }
    return false;
}

/**
 * @brief Print frontier
 *
 */
void Extractor::printFrontier() {
    cout << "Frontier:" << endl;
    for (auto& f : _frontier)
        cout << "Qubit:" << f->getQubit() << ": " << f->getId() << endl;
    cout << endl;
}

/**
 * @brief Print neighbors
 *
 */
void Extractor::printNeighbors() {
    cout << "Neighbors:" << endl;
    for (auto& n : _neighbors)
        cout << n->getId() << endl;
    cout << endl;
}

/**
 * @brief Print axels
 *
 */
void Extractor::printAxels() {
    cout << "Axels:" << endl;
    for (auto& n : _axels) {
        cout << n->getId() << " (phase gadget: ";
        for (auto& [pg, _] : n->getNeighbors()) {
            if (_graph->isGadgetLeaf(pg))
                cout << pg->getId() << ")" << endl;
        }
    }
    cout << endl;
}

void Extractor::printCXs() {
    cout << "CXs: " << endl;
    for (auto& [c, t] : _cnots) {
        cout << "(" << c << ", " << t << ")  ";
    }
    cout << endl;
}