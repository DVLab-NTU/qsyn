/****************************************************************************
  FileName     [ variables.h ]
  PackageName  [ duostra ]
  Synopsis     [ Define global variables for Duostra ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>

extern size_t DUOSTRA_SCHEDULER;  // 0:base 1:static 2:random 3:greedy 4:search
extern size_t DUOSTRA_ROUTER;     // 0:apsp 1:duostra
extern size_t DUOSTRA_PLACER;     // 0:static 1:random 2:dfs
extern bool DUOSTRA_ORIENT;       // t/f smaller logical qubit index with little priority

// SECTION - Initialize in Greedy Scheduler
extern size_t DUOSTRA_CANDIDATES;  // top k candidates, -1: all
extern size_t DUOSTRA_APSP_COEFF;  // coefficient of apsp cost
extern bool DUOSTRA_AVAILABLE;     // 0:min 1:max, available time of double-qubit gate is set to min or max of occupied time
extern bool DUOSTRA_COST;          // 0:min 1:max, select min or max cost from the waitlist

// SECTION - Initialize in Search Scheduler
extern size_t DUOSTRA_DEPTH;         // depth of searching region
extern bool DUOSTRA_NEVER_CACHE;     // never cache any children unless children() is called
extern bool DUOSTRA_EXECUTE_SINGLE;  // execute the single gates when they are available