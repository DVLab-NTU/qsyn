/****************************************************************************
  FileName     [ phase.cpp ]
  PackageName  [ util ]
  Synopsis     [ Implementation of the Phase class and pertinent classes]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/

#include "phase.h"

PhaseUnit Phase::_printUnit = PhaseUnit::PI;

std::ostream& operator<<(std::ostream& os, const Phase& p) {
    if (Phase::getPrintUnit() == PhaseUnit::PI) {
        if (p._rational.numerator() > 1) os << p._rational.numerator();
        return os << "\u03C0/" << p._rational.denominator();
    } else {
        return os << (p._rational.numerator() * std::numbers::pi / p._rational.denominator());
    }
}

float Phase::toFloat() { 
    return (std::numbers::pi_v<float> * _rational.numerator()) / _rational.denominator(); 
}
double Phase::toDouble() { 
    return (std::numbers::pi_v<double> * _rational.numerator()) / _rational.denominator(); 
}
long double Phase::toLongDouble() { 
    return (std::numbers::pi_v<long double> * _rational.numerator()) / _rational.denominator(); 
}

std::ostream& operator<<(std::ostream& os, const setPhaseUnit& pu) {
    Phase::setPrintUnit(pu._printUnit); 
    return os;
}