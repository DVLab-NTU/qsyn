/****************************************************************************
  FileName     [ zxRules.h ]
  PackageName  [ simplifier ]
  Synopsis     [ Define class ZXRule structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_RULES_H
#define ZX_RULES_H

#include <array>
#include <string>   // for string
#include <utility>  // for pair

#include "zxDef.h"  // for EdgePair

class ZXGraph;
class ZXVertex;

namespace std {
template <>
struct hash<vector<ZXVertex*>> {
    size_t operator()(const vector<ZXVertex*>& k) const {
        size_t ret = hash<ZXVertex*>()(k[0]);
        for (size_t i = 1; i < k.size(); i++) {
            ret ^= hash<ZXVertex*>()(k[i]);
        }

        return ret;
    }
};
}  // namespace std

/**
 * @brief ZXRule: Base class
 *
 */
class ZXRule {
public:
    using MatchType = int;
    using MatchTypeVec = std::vector<MatchType>;

    ZXRule() {}
    virtual ~ZXRule() { reset(); }

    virtual void reset();

    virtual void match(ZXGraph* g) = 0;
    virtual void rewrite(ZXGraph* g) = 0;

    // Getter and Setter
    virtual const int& getMatchTypeVecNum() const { return _matchTypeVecNum; }
    virtual const std::string& getName() const { return _name; }
    virtual const std::vector<ZXVertex*>& getRemoveVertices() const { return _removeVertices; }
    virtual const std::vector<EdgePair>& getRemoveEdges() const { return _removeEdges; }
    virtual const std::vector<std::pair<ZXVertex*, ZXVertex*>>& getEdgeTableKeys() const { return _edgeTableKeys; }
    virtual const std::vector<std::pair<int, int>>& getEdgeTableValues() const { return _edgeTableValues; }

    virtual void setMatchTypeVecNum(int n) { _matchTypeVecNum = n; }
    virtual void setRemoveVertices(std::vector<ZXVertex*> v) { _removeVertices = v; }
    virtual void setName(std::string name) { _name = name; }

    virtual void pushRemoveEdge(const EdgePair& ep) { _removeEdges.push_back(ep); }

protected:
    int _matchTypeVecNum;
    std::string _name;
    std::vector<ZXVertex*> _removeVertices;
    std::vector<EdgePair> _removeEdges;
    std::vector<std::pair<ZXVertex*, ZXVertex*>> _edgeTableKeys;
    std::vector<std::pair<int, int>> _edgeTableValues;
};

/**
 * @brief Bialgebra Rule(b): Find noninteracting matchings of the bialgebra rule. (in bialg.cpp)
 *
 */
class Bialgebra : public ZXRule {
public:
    using MatchType = EdgePair;
    using MatchTypeVec = std::vector<MatchType>;

    Bialgebra() {
        _name = "Bialgebra Rule";
    }
    virtual ~Bialgebra() {}

    void match(ZXGraph* g) override;
    void rewrite(ZXGraph* g) override;

    // checker
    bool check_duplicated_vertex(std::vector<ZXVertex*> vec);

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v) { _matchTypeVec = v; }

protected:
    MatchTypeVec _matchTypeVec;
};

/**
 * @brief State copy rule(pi): Find spiders with a 0 or pi phase that have a single neighbor. (in copy.cpp)
 *
 */
class StateCopy : public ZXRule {
public:
    using MatchType = std::tuple<ZXVertex*, ZXVertex*, std::vector<ZXVertex*>>;  // vertex with a pi,  vertex with a, neighbors
    using MatchTypeVec = std::vector<MatchType>;

    StateCopy() {
        _name = "State Copy Rule";
    }
    virtual ~StateCopy() {}

    void match(ZXGraph* g) override;
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v) { _matchTypeVec = v; }

protected:
    MatchTypeVec _matchTypeVec;
};

/**
 * @brief Hadamard Cancellation(i2): Fuse two neighboring H-boxes together. (in hfusion.cpp)
 *
 */
class HboxFusion : public ZXRule {
public:
    using MatchType = ZXVertex*;
    using MatchTypeVec = std::vector<MatchType>;

    HboxFusion() {
        _name = "Hadamard Cancellation Rule";
    }
    virtual ~HboxFusion() {}

    void match(ZXGraph* g) override;
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v) { _matchTypeVec = v; }

protected:
    MatchTypeVec _matchTypeVec;
};

/**
 * @brief Hadamard rule (h): H_BOX vertex -> HADAMARD edge (in hrule.cpp)
 * @ ZXRule's Derived class
 */
class HRule : public ZXRule {
public:
    using MatchType = ZXVertex*;
    using MatchTypeVec = std::vector<MatchType>;

    HRule() {
        _name = "Hadamard Rule";
    }
    virtual ~HRule() {}

    void match(ZXGraph* g) override;
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v) { _matchTypeVec = v; }

protected:
    MatchTypeVec _matchTypeVec;
};

/**
 * @brief Identity Removal Rule(i1): Find non-interacting identity vertices. (in id.cpp)
 *
 */
class IdRemoval : public ZXRule {
public:
    using MatchType = std::tuple<ZXVertex*, ZXVertex*, ZXVertex*, EdgeType>;  // vertex, neighbor0, neighbor1, new edge type
    using MatchTypeVec = std::vector<MatchType>;

    IdRemoval() {
        _name = "Identity Removal Rule";
    }
    virtual ~IdRemoval() {}

    void match(ZXGraph* g) override;
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v) { _matchTypeVec = v; }

protected:
    MatchTypeVec _matchTypeVec;
};

/**
 * @brief Find noninteracting matchings of the local complementation rule.
 *
 */
class LComp : public ZXRule {
public:
    using MatchType = std::pair<ZXVertex*, std::vector<ZXVertex*>>;  // Vertex to remove, its neighbors
    using MatchTypeVec = std::vector<MatchType>;

    LComp() {
        _name = "Local Complementation Rule";
    }
    virtual ~LComp() {}

    void match(ZXGraph* g) override;
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v) { _matchTypeVec = v; }

protected:
    MatchTypeVec _matchTypeVec;
};

/**
 * @brief Find non-interacting matchings of the phase gadget rule.
 *
 */
class PhaseGadget : public ZXRule {
public:
    using MatchType = std::tuple<Phase, std::vector<ZXVertex*>, std::vector<ZXVertex*>>;
    using MatchTypeVec = std::vector<MatchType>;

    PhaseGadget() {
        _name = "Phase Gadget Rule";
    }
    virtual ~PhaseGadget() {}

    void match(ZXGraph* g) override;
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v) { _matchTypeVec = v; }

protected:
    MatchTypeVec _matchTypeVec;
};

/**
 * @brief Common interface to all pivot-like rules
 *
 */
class PivotInterface : public ZXRule {
public:
    using MatchType = std::array<ZXVertex*, 2>;
    using MatchTypeVec = std::vector<MatchType>;

    PivotInterface() {}
    virtual ~PivotInterface() {}

    virtual void match(ZXGraph* g) override = 0;
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v) { _matchTypeVec = v; }

protected:
    virtual void preprocess(ZXGraph* g) = 0;
    MatchTypeVec _matchTypeVec;
};

/**
 * @brief Find non-interacting matchings of the pivot rule.
 *
 */
class Pivot : public PivotInterface {
public:
    using MatchType = PivotInterface::MatchType;
    using MatchTypeVec = PivotInterface::MatchTypeVec;

    Pivot() {
        _name = "Pivot Rule";
    }
    virtual ~Pivot() {}

    void match(ZXGraph* g) override;

protected:
    void preprocess(ZXGraph* g) override;
    std::vector<ZXVertex*> _boundaries;
};

/**
 * @brief Find non-interacting matchings of the pivot gadget rule.
 *
 */
class PivotGadget : public PivotInterface {
public:
    using MatchType = PivotInterface::MatchType;
    using MatchTypeVec = PivotInterface::MatchTypeVec;

    PivotGadget() {
        _name = "Pivot Gadget Rule";
    }
    virtual ~PivotGadget() {}

    void match(ZXGraph* g) override;

protected:
    void preprocess(ZXGraph* g) override;
};

/**
 * @brief Find non-interacting matchings of the pivot gadget rule.
 *
 */
class PivotBoundary : public PivotInterface {
public:
    using MatchType = PivotInterface::MatchType;
    using MatchTypeVec = PivotInterface::MatchTypeVec;

    PivotBoundary() {
        _name = "Pivot Boundary Rule";
    }
    virtual ~PivotBoundary() {}

    void match(ZXGraph* g) override;
    void addBoundary(ZXVertex* v) { _boundaries.push_back(v); }
    void clearBoundary() { _boundaries.clear(); }

protected:
    void preprocess(ZXGraph* g) override;
    std::vector<ZXVertex*> _boundaries;
};

/**
 * @brief Spider Fusion(f): Find non-interacting matchings of the spider fusion rule. (in sfusion.cpp)
 *
 */
class SpiderFusion : public ZXRule {
public:
    using MatchType = std::pair<ZXVertex*, ZXVertex*>;
    using MatchTypeVec = std::vector<MatchType>;

    SpiderFusion() {
        _name = "Spider Fusion Rule";
    }
    virtual ~SpiderFusion() { _matchTypeVec.clear(); }

    void match(ZXGraph* g) override;
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v) { _matchTypeVec = v; }

protected:
    MatchTypeVec _matchTypeVec;
};

#endif