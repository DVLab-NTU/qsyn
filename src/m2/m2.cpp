/****************************************************************************
  FileName     [ m2.cpp ]
  PackageName  [ m2 ]
  Synopsis     [ Define class m2 member functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "m2.h"
extern size_t verbose;
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

/// @brief 
void M2::printTrack() const {
  cout << "Track:" << endl;
  for(size_t i=0; i < _opStorage.size(); i++){
    cout << "Step " << i+1 << ": " << _opStorage[i].first << " to " << _opStorage[i].second << endl;
  }
  cout << endl;
}

void M2::defaultInit(){
  _size = 6;

  _matrix.push_back(Row(0, 6, bitset<16>{"101101"}));
  _matrix.push_back(Row(1, 6, bitset<16>{"001110"}));
  _matrix.push_back(Row(2, 6, bitset<16>{"010110"}));
  _matrix.push_back(Row(3, 6, bitset<16>{"011001"}));
  _matrix.push_back(Row(4, 6, bitset<16>{"011011"}));
  _matrix.push_back(Row(5, 6, bitset<16>{"101000"}));
}

bool M2::xorOper(size_t ctrl, size_t targ, bool track)  { 
  if( ctrl >= _size ) {
    cerr << "Error: wrong dimension " << ctrl << endl;
    return false;
  }
  if( targ >= _size ) {
    cerr << "Error: wrong dimension " << targ << endl;
    return false;
  }
  _matrix[targ] += _matrix[ctrl]; 
  if(track) _opStorage.emplace_back(ctrl, targ);
  return true;
}
/// @brief 
void M2::gaussianElim(bool track){
    if(verbose>=3) cout << "Performing Gaussian Elimination..." << endl;
    if(verbose>=8) printMatrix();
    _opStorage.clear();
    for(size_t i=0; i < _size - 1; i++){
      if(_matrix[i].getRow()[i] == 0){
        bool noSol = true; 
        for(size_t j=i+1; j<_size; j++){
          if(_matrix[j].getRow()[i] ==1){
            xorOper(j, i, track);
            if(verbose>=8) {
              cout << "Diag Add " << j << " to "<< i << endl;
              printMatrix();
            }
            noSol = false;
          }
          break;
        }
        if(noSol) { cout << "No Solution" << endl; return;}
		  }
		  for(size_t j=i+1; j < _size; j++){
        if(_matrix[j].getRow()[i] ==1 && _matrix[i].getRow()[i] ==1){
          xorOper(i, j, track);
          if(verbose>=8) {
            cout << "Add " << i << " to "<< j << endl;
            printMatrix();
          }
        }
		  }
	  }
    for(size_t i = 0; i < _size; i++){
		  for(size_t j=_size-i; j < _size; j++){
        if(_matrix[_size-i-1].getRow()[j] ==1){
          xorOper(j, _size-i-1, track);
          if(verbose>=8) printMatrix();
        }
		  }
	  }
}

/// @brief 
/// @return 
bool M2::isIdentity(){
  for(auto row: _matrix){
    if(!row.isSingular()) return false;
  }
  return true;
}