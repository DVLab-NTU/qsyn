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




class ZXRule{
    public:
      typedef int MatchType;
      typedef vector<MatchType> MatchTypeVec;
      
      ZXRule(){};
      
      
      ~ZXRule(){};

      virtual void match(ZXGraph* g){}
      virtual void rewrite(ZXGraph* g){}

      // virtual MatchTypeVec getMatchTypeVec() const                                        {}
      virtual vector<ZXVertex* > getRemoveVertices() const                    { return _removeVertices; }
      virtual vector<EdgePair> getRemoveEdges() const                         { return _removeEdges; }
      virtual vector<pair<ZXVertex*, ZXVertex*> > getEdgeTableKeys() const    { return _edgeTableKeys; }
      virtual vector<pair<int, int> > getEdgeTableValues() const              { return _edgeTableValues; }
      // virtual void setMatchTypeVec(MatchTypeVec v)                         {  }
      virtual void setRemoveVertices(vector<ZXVertex* > v)           { _removeVertices = v; }
      
      
      template <typename T> T getMatchTypeVec() const  { return _matchTypeVec; }
      
    
    protected:
      MatchTypeVec                                  _matchTypeVec;
      vector<ZXVertex* >                            _removeVertices;
      vector<EdgePair>                              _removeEdges;
      vector<pair<ZXVertex*, ZXVertex*> >           _edgeTableKeys;
      vector<pair<int, int> >                       _edgeTableValues;

};


/**
 * @brief Hadamard rule (h): H_BOX vertex -> HADAMARD edge (in hrule.cpp)
 * 
 */

class HRule : public ZXRule{
  public:
    typedef ZXVertex* MatchType;
    typedef vector<MatchType> MatchTypeVec;

    HRule(){
      _matchTypeVec.clear();
      _removeVertices.clear();
      _removeEdges.clear();
      _edgeTableKeys.clear();
      _edgeTableValues.clear();
    }
    ~HRule(){}

    
    void match(ZXGraph* g) override; 
    void rewrite(ZXGraph* g) override;
    
  protected:
    
    // vector<ZXVertex* >                            _removeVertices;
    // vector<EdgePair>                              _removeEdges;
    // vector<pair<ZXVertex*, ZXVertex*> >           _edgeTableKeys;
    // vector<pair<int, int> >                       _edgeTableValues;
  private:
    
};



#endif