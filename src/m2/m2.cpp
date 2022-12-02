/****************************************************************************
  FileName     [ m2.cpp ]
  PackageName  [ m2 ]
  Synopsis     [ Define class m2 member functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "m2.h"

using namespace std;


/// @brief 
void M2::gaussianElim(){

}

/// @brief 
/// @return 
bool M2::isIdentity(){
  for(auto row: _matrix){
    if(!row.isSingular()) return false;
  }
  return true;
}