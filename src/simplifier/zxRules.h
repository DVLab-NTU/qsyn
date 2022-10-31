/****************************************************************************
  FileName     [ ZXRule.h ]
  PackageName  [ simplifier ]
  Synopsis     [ ZX Basic Rules ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#ifndef ZX_RULES_H
#define ZX_RULES_H

#include <tuple>
#include <vector>
#include <iostream>
#include <unordered_map>

#include "zxDef.h"
#include "zxGraph.h"


namespace std{
template <>
struct hash<vector<ZXVertex*>>
  {
    size_t operator()(const vector<ZXVertex*>& k) const
    {
      size_t ret = hash<ZXVertex*>()(k[0]);
      for(size_t i=1; i<k.size(); i++){
        ret ^= hash<ZXVertex*>()(k[i]);
      }
      // size_t ret = hash<ZXVertex*>()(k[0]);
      // for(size_t i=1; i<k.size(); i++){
      //   ret ^= hash<ZXVertex*>()(k[i]) + 0x9e3779b9 + (ret << 6) + (ret >> 2);
      // }
      return ret;
    }
  };
}

/**
 * @brief ZXRule: Base class
 * 
 */
class ZXRule{
    public:
      typedef int MatchType;
      typedef vector<MatchType> MatchTypeVec;
      
      ZXRule(){};
      ~ZXRule(){};

      virtual void reset();

      virtual void match(ZXGraph* g){}
      virtual void rewrite(ZXGraph* g){}

      // Getter and Setter
      virtual const int& getMatchTypeVecNum() const                                  { return _matchTypeVecNum; }
      virtual const string& getName() const                                          { return _name; }       
      virtual const vector<ZXVertex* >& getRemoveVertices() const                    { return _removeVertices; }
      virtual const vector<EdgePair>& getRemoveEdges() const                         { return _removeEdges; }
      virtual const vector<pair<ZXVertex*, ZXVertex*> >& getEdgeTableKeys() const    { return _edgeTableKeys; }
      virtual const vector<pair<int, int> >& getEdgeTableValues() const              { return _edgeTableValues; }
      
      virtual void setMatchTypeVecNum(int n)                                  { _matchTypeVecNum = n; }
      virtual void setRemoveVertices(vector<ZXVertex* > v)                    { _removeVertices = v; }
      virtual void setName(string name)                                       { _name = name; }

      virtual void pushRemoveEdge(const EdgePair& ep)                         { _removeEdges.push_back(ep); }
      
      
    protected:
      int                                                                      _matchTypeVecNum;
      string                                                                   _name;
      vector<ZXVertex* >                                                       _removeVertices;
      vector<EdgePair>                                                         _removeEdges;
      vector<pair<ZXVertex*, ZXVertex*> >                                      _edgeTableKeys;
      vector<pair<int, int> >                                                  _edgeTableValues;

};



/**
 * @brief Bialgebra Rule(b): Finds noninteracting matchings of the bialgebra rule. (in bialg.cpp)
 * 
 */
class Bialgebra : public ZXRule{
  public:
    typedef EdgePair MatchType;
    typedef vector<MatchType> MatchTypeVec;

    Bialgebra(){
      _matchTypeVec.clear();
      _name = "Bialgebra Rule";
    }
    ~Bialgebra(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // checker
    bool check_duplicated_vertex(vector<ZXVertex*> vec);

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};



/**
 * @brief Pi copy rule(pi): Finds spiders with a 0 or pi phase that have a single neighbor. (in copy.cpp)
 * 
 */
class StateCopy : public ZXRule{
  public:
    typedef tuple<ZXVertex*, ZXVertex*, vector<ZXVertex*>> MatchType; // vertex with a pi,  vertex with a, neighbors
    typedef vector<MatchType> MatchTypeVec;

    StateCopy(){
      _matchTypeVec.clear();
      _name = "State Copy Rule";
    }
    ~StateCopy(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};



/**
 * @brief Hadamard Cancellation(i2): Fuses two neighboring H-boxes together. (in hfusion.cpp)
 * 
 */
class HboxFusion : public ZXRule{
  public:
    typedef ZXVertex* MatchType;
    typedef vector<MatchType> MatchTypeVec;

    HboxFusion(){
      _matchTypeVec.clear();
      _name = "Hadamard Cancellation Rule";
    }
    ~HboxFusion(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};



/**
 * @brief 
 * 
 */
class Hopf : public ZXRule{
  public:
    typedef pair<ZXVertex*, ZXVertex*> MatchType;
    typedef vector<MatchType> MatchTypeVec;

    Hopf(){
      _matchTypeVec.clear();
      _name = "Hopf Rule";
    }
    ~Hopf(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};



/**
 * @brief Hadamard rule (h): H_BOX vertex -> HADAMARD edge (in hrule.cpp)
 * @ ZXRule's Derived class
 */
class HRule : public ZXRule{
  public:
    typedef ZXVertex* MatchType;
    typedef vector<MatchType> MatchTypeVec;

    HRule(){
      _matchTypeVec.clear();
      _name = "Hadamard Rule";
    }
    ~HRule(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};



/**
 * @brief Identity Removal Rule(i1): Finds non-interacting identity vertices. (in id.cpp)
 * 
 */
class IdRemoval : public ZXRule{
  public:
    typedef tuple<ZXVertex*, ZXVertex*, ZXVertex*, EdgeType> MatchType; // vertex, neighbor0, neighbor1, new edge type
    typedef vector<MatchType> MatchTypeVec;

    IdRemoval(){
      _matchTypeVec.clear();
      _name = "Identity Removal Rule";
    }
    ~IdRemoval(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};



/**
 * @brief Finds noninteracting matchings of the local complementation rule.
 * 
 */
class LComp : public ZXRule{
  public:
    typedef pair<ZXVertex*, vector<ZXVertex*> > MatchType;
    typedef vector<MatchType> MatchTypeVec;

    LComp(){
      _matchTypeVec.clear();
      _name = "Local Complementation Rule";
    }
    ~LComp(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};



/**
 * @brief Finds non-interacting matchings of the phase gadget rule.
 * 
 */
class PhaseGadget : public ZXRule{
  public:
    //TODO: Check MatchType
    typedef tuple<Phase, vector<ZXVertex*>, vector<ZXVertex*>> MatchType;
    typedef vector<MatchType> MatchTypeVec;

    PhaseGadget(){
      _matchTypeVec.clear();
      _name = "Phase Gadget Rule";
    }
    ~PhaseGadget(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
};



/**
 * @brief Finds non-interacting matchings of the pivot rule.
 * 
 */
class Pivot : public ZXRule{
  public:
    typedef int MatchType;
    typedef vector<MatchType> MatchTypeVec;

    Pivot(){
      _matchTypeVec.clear();
      _name = "Pivot Rule";
    }
    ~Pivot(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};



/**
 * @brief Finds non-interacting matchings of the pivot gadget rule.
 * 
 */
class PivotGadget : public ZXRule{
  public:
    //TODO: Check MatchType
    typedef vector<ZXVertex* > MatchType;    // vs, vt(need to remove), newVertex
    typedef vector<MatchType> MatchTypeVec;

    PivotGadget(){
      _matchTypeVec.clear();
      _name = "Pivot Gadget Rule";
    }
    ~PivotGadget(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
};



/**
 * @brief Finds non-interacting matchings of the pivot gadget rule.
 * 
 */
class PivotBoundary : public ZXRule{
  public:
    //TODO: Check MatchType
    typedef int MatchType;
    typedef vector<MatchType> MatchTypeVec;

    PivotBoundary(){
      _matchTypeVec.clear();
      _name = "Pivot Boundary Rule";
    }
    ~PivotBoundary(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
};



/**
 * @brief Spider Fusion(f): Finds non-interacting matchings of the spider fusion rule. (in sfusion.cpp)
 * 
 */
class SpiderFusion : public ZXRule{
  public:
    typedef pair<ZXVertex*, ZXVertex*> MatchType;
    typedef vector<MatchType> MatchTypeVec;

    SpiderFusion(){
      _matchTypeVec.clear();
      _name = "Spider Fusion Rule";
    }
    ~SpiderFusion(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    const MatchTypeVec& getMatchTypeVec() const     { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};

#endif