/****************************************************************************
  FileName     [ m2.cpp ]
  PackageName  [ m2 ]
  Synopsis     [ Define class m2 member functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "m2.h"

using namespace std;

Row operator+(Row& lhs, const Row& rhs){
  lhs += rhs;
  return lhs;
}

Row &Row::operator+=(const Row& rhs) {
    assert( _row.size() == rhs._row.size());
    _row ^= rhs._row;
    return *this;
}

void Row::printRow() const {
  for(size_t i=0; i< _size; i++){
    cout << _row[i] << " ";
  }
  cout << endl;
}
/// @brief 
void M2::printMatrix() const {
  cout << "M2 matrix:" << endl;
  for(const auto row: _matrix){
    row.printRow();
  }
  cout << endl;
}

void M2::defaultInit(){
  _size = 3;

  _matrix.push_back(Row(0, 3, bitset<16>{"111"}));
  _matrix.push_back(Row(1, 3, bitset<16>{"110"}));
  _matrix.push_back(Row(2, 3, bitset<16>{"100"}));
}

bool M2::xorOper(size_t ctrl, size_t targ)  { 
  if( ctrl >= _size ) {
    cerr << "Error: wrong dimension " << ctrl << endl;
    return false;
  }
  if( targ >= _size ) {
    cerr << "Error: wrong dimension " << targ << endl;
    return false;
  }
  _matrix[targ] += _matrix[ctrl]; 
  return true;
}
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