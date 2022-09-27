/****************************************************************************
  FileName     [ ZXRules.h ]
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


class ZXRules{
    public:
        ZXRules(){}
        ~ZXRules(){}
    private:
};

class SpiderFusion : public ZXRules{
  public:
    vector<pair<ZXVertex*, ZXVertex*> > match(ZXGraph* g);
    void spiderFuse(ZXGraph* g, vector<pair<ZXVertex*, ZXVertex*> > matches);
    
  private:
};

/**
 * @brief 
 * 
 */
class Bialgebra : public ZXRules{
  public:
    vector<vector<ZXVertex*> > match(ZXGraph* g);
    void bialg(ZXGraph* g, vector<vector<ZXVertex*> > matches);

  private:
};

/**
 * @brief Hadamard rule (h)
 * 
 */
class Hadamard : public ZXRules{
  public:
    vector<ZXVertex*> match(ZXGraph* g);
    void hadamard2Edge(ZXGraph* g, vector<ZXVertex*> matches);
    
  
  private:

};

/**
 * @brief Hadamard-cancellation (i2)
 * 
 */
class HCancel : public ZXRules{
  public:
    vector<EdgePair> match(ZXGraph* g);
    void hboxesFuse(ZXGraph* g, vector<ZXVertex*> matches);
    
  
  private:

};


#endif