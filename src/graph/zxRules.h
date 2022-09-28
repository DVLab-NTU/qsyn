/****************************************************************************
  FileName     [ ZXRule.h ]
  PackageName  [ graph ]
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

      virtual void match(ZXGraph* g){}
      virtual void rewrite(ZXGraph* g){}

      // Getter and Setter
      virtual int getMatchTypeVecNum() const                                  { return _matchTypeVecNum; }
      virtual string getName() const                                          { return _name; }       
      virtual vector<ZXVertex* > getRemoveVertices() const                    { return _removeVertices; }
      virtual vector<EdgePair> getRemoveEdges() const                         { return _removeEdges; }
      virtual vector<pair<ZXVertex*, ZXVertex*> > getEdgeTableKeys() const    { return _edgeTableKeys; }
      virtual vector<pair<int, int> > getEdgeTableValues() const              { return _edgeTableValues; }
      
      virtual void setMatchTypeVecNum(int n)                                  { _matchTypeVecNum = n; }
      virtual void setRemoveVertices(vector<ZXVertex* > v)                    { _removeVertices = v; }
      virtual void setName(string name)                                       { _name = name; }
      
      
    protected:
      int                                                                      _matchTypeVecNum;
      string                                                                   _name;
      vector<ZXVertex* >                                                       _removeVertices;
      vector<EdgePair>                                                         _removeEdges;
      vector<pair<ZXVertex*, ZXVertex*> >                                      _edgeTableKeys;
      vector<pair<int, int> >                                                  _edgeTableValues;

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
    MatchTypeVec getMatchTypeVec() const            { return _matchTypeVec; }
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
      _name = "Spider Fusion";
    }
    ~SpiderFusion(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    MatchTypeVec getMatchTypeVec() const            { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};



/**
 * @brief Bialgebra Rule(b): Finds noninteracting matchings of the bialgebra rule. (in bialg.cpp)
 * 
 */
class Bialgebra : public ZXRule{
  public:
    //TODO: Check MatchType
    typedef int MatchType;
    typedef vector<MatchType> MatchTypeVec;

    Bialgebra(){
      _matchTypeVec.clear();
      _name = "Bialgebra Rule";
    }
    ~Bialgebra(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    MatchTypeVec getMatchTypeVec() const            { return _matchTypeVec; }
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
    //TODO: Check MatchType
    typedef int MatchType;
    typedef vector<MatchType> MatchTypeVec;

    IdRemoval(){
      _matchTypeVec.clear();
      _name = "Identity Removal Rule";
    }
    ~IdRemoval(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    MatchTypeVec getMatchTypeVec() const            { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};


/**
 * @brief Pi copy rule(pi): Finds spiders with a 0 or pi phase that have a single neighbor. (in copy.cpp)
 * 
 */
class PiCopy : public ZXRule{
  public:
    //TODO: Check MatchType
    typedef int MatchType;
    typedef vector<MatchType> MatchTypeVec;

    PiCopy(){
      _matchTypeVec.clear();
      _name = "Identity Removal Rule";
    }
    ~PiCopy(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    MatchTypeVec getMatchTypeVec() const            { return _matchTypeVec; }
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
    //TODO: Check MatchType
    typedef int MatchType;
    typedef vector<MatchType> MatchTypeVec;

    HboxFusion(){
      _matchTypeVec.clear();
      _name = "Hadamard Cancellation Rule";
    }
    ~HboxFusion(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;

    // Getter and Setter
    MatchTypeVec getMatchTypeVec() const            { return _matchTypeVec; }
    void setMatchTypeVec(MatchTypeVec v)            { _matchTypeVec = v; }
    
  protected:
    MatchTypeVec                                      _matchTypeVec;
    
};



#endif