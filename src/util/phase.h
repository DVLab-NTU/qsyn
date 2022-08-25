/****************************************************************************
  FileName     [ phase.cpp ]
  PackageName  [ util ]
  Synopsis     [ Definition of the Phase class and pertinent classes]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/

#ifndef PHASE_H
#define PHASE_H

#include "rationalNumber.h"
#include <numbers>

enum class PhaseUnit {
    PI, ONE
};

class Phase {

public:
    Phase(int n, int d): _rational(n, d) {}
    template <class T>
    Phase(T f): _rational(f/std::numbers::pi_v<T>) {}

    friend std::ostream& operator<<(std::ostream& os, const Phase& p);

    virtual float toFloat();
    virtual double toDouble();
    virtual long double toLongDouble();
    static PhaseUnit getPrintUnit() {
        return _printUnit;
    }
    static void setPrintUnit(const PhaseUnit& pu) {
        _printUnit = pu;
    }
private:
    Rational _rational;
    static PhaseUnit _printUnit;
};

class setPhaseUnit {

public:
    friend class Phase;
    explicit setPhaseUnit(PhaseUnit pu): _printUnit(pu) {}
    PhaseUnit getPhaseUnit() const {
        return _printUnit;
    }
    friend std::ostream& operator<<(std::ostream& os, const setPhaseUnit& pu);

private:
    PhaseUnit _printUnit;
};

#endif //PHASE_H