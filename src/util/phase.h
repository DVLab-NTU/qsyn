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
#include "myConcepts.h"
#include <numbers>

enum class PhaseUnit {
    PI, ONE
};

class Phase {

public:
    Phase(int n, int d): _rational(n, d) { normalize(); }
    template <class T>
    Phase(T f): _rational(f/std::numbers::pi_v<T>) { std::cout << f/std::numbers::pi_v<T> << std::endl; normalize(); }

    friend std::ostream& operator<<(std::ostream& os, const Phase& p);
    Phase& operator+();
    Phase& operator-();

    // Addition and subtraction are mod 2pi
    Phase& operator+=(const Phase& rhs);
    Phase& operator-=(const Phase& rhs);
    friend Phase operator+(Phase lhs, const Phase& rhs);
    friend Phase operator-(Phase lhs, const Phase& rhs);

    // Multiplication / Devision w/ unitless constants
    Phase& operator*=(const Unitless auto& rhs){
        this->_rational *= rhs;
        normalize();
        return *this;
    }
    Phase& operator/=(const Unitless auto& rhs) {
        this->_rational /= rhs;
        normalize();
        return *this;
    }
    friend Phase operator* (Phase lhs, const Unitless auto& rhs) {
        lhs *= rhs;
        return lhs;
    }
    friend Phase operator* (const Unitless auto& lhs, Phase rhs) {
        return rhs * lhs;
    }
    friend Phase operator/ (Phase lhs, const Unitless auto& rhs) {
        lhs /= rhs;
        return lhs;
    }
    friend Rational operator/ (const Phase& lhs, const Phase& rhs) {
        Rational q = lhs._rational / rhs._rational;
        return q;
    }
    // Operator *, / between phases are not supported deliberately as they don't make physical sense (Changes unit)

    bool operator==(const Phase& rhs) const;
    bool operator!=(const Phase& rhs) const;
    // Operator <, <=, >, >= are are not supported deliberately as they don't make physical sense (Phases are mod 2pi)

    virtual float toFloat();
    virtual double toDouble();
    virtual long double toLongDouble();
    static PhaseUnit getPrintUnit() {
        return _printUnit;
    }
    static void setPrintUnit(const PhaseUnit& pu) {
        _printUnit = pu;
    }

    void normalize();
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