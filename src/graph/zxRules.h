/****************************************************************************
  FileName     [ zxRules.h ]
  PackageName  [ graph ]
  Synopsis     [ ZX Basic Rules ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#ifndef ZX_RULES_H
#define ZX_RULES_H

#include <vector>
#include <iostream>
#include "zxDef.h"
#include "zxGraph.h"

class zxRules{
  typedef pair<int, int> MatchType;
  typedef vector<MatchType> MatchTypeVec;

    public:
        zxRules(){}
        ~zxRules(){}

    private:

};

class spiderFusion : public zxRules{
  typedef pair<ZXVertex*, ZXVertex*> MatchType;
  typedef vector<MatchType> MatchTypeVec;

  public:
    MatchTypeVec match(ZXGraph* g);
    
  private:
};

class bialgebra : public zxRules{


};


#endif