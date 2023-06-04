/****************************************************************************
  FileName     [ phase.h ]
  PackageName  [ util ]
  Synopsis     [ Definition of the Phase class and pertinent classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef PHASE_H
#define PHASE_H

#include <cmath>
#include <iosfwd>  // for ostream
#include <numbers>
#include <string>
#include <vector>

#include "myConcepts.h"
#include "util.h"

class Rational;

enum class PhaseUnit {
    PI,
    ONE
};

class Phase {
public:
    Phase() : _rational(0, 1) {}
    // explicitly ban `Phase phase = n;` to prevent confusing code
    explicit Phase(int n) : _rational(n, 1) { normalize(); }
    Phase(int n, int d) : _rational(n, d) { normalize(); }
    template <class T>
    requires std::floating_point<T>
    Phase(T f, T eps = 1e-4) : _rational(f / std::numbers::pi_v<T>, eps / std::numbers::pi_v<T>) { normalize(); }

    friend std::ostream& operator<<(std::ostream& os, const Phase& p);
    Phase operator+() const;
    Phase operator-() const;

    // Addition and subtraction are mod 2pi
    Phase& operator+=(const Phase& rhs);
    Phase& operator-=(const Phase& rhs);
    friend Phase operator+(Phase lhs, const Phase& rhs);
    friend Phase operator-(Phase lhs, const Phase& rhs);

    // Multiplication / Devision w/ unitless constants
    Phase& operator*=(const Unitless auto& rhs);
    Phase& operator/=(const Unitless auto& rhs);
    friend Phase operator*(Phase lhs, const Unitless auto& rhs);
    friend Phase operator*(const Unitless auto& lhs, Phase rhs);
    friend Phase operator/(Phase lhs, const Unitless auto& rhs);
    friend Rational operator/(const Phase& lhs, const Phase& rhs);
    // Operator *, / between phases are not supported deliberately as they don't make physical sense (Changes unit)

    bool operator==(const Phase& rhs) const;
    bool operator!=(const Phase& rhs) const;
    // Operator <, <=, >, >= are are not supported deliberately as they don't make physical sense (Phases are mod 2pi)

    template <class T>
    requires std::floating_point<T>
    T toFloatType()
        const { return std::numbers::pi_v<T> * _rational.toFloatType<T>(); }

    float toFloat() { return toFloatType<float>(); }
    double toDouble() { return toFloatType<double>(); }
    long double toLongDouble() { return toFloatType<long double>(); }

    Rational getRational() const { return _rational; }
    int numerator() const { return _rational.numerator(); }
    int denominator() const { return _rational.denominator(); }

    template <class T>
    requires std::floating_point<T>
    static Phase toPhase(T f, T eps = 1e-4) {
        Phase p(f, eps);
        return p;
    }

    static PhaseUnit getPrintUnit() { return _printUnit; }
    static void setPrintUnit(const PhaseUnit& pu) { _printUnit = pu; }

    std::string getAsciiString() const;
    std::string getPrintString() const;

    void normalize();

    template <class T = double>
    requires std::floating_point<T>
    static bool
    fromString(const std::string& str, Phase& phase) {
        if (!myStrValid<T>(str, phase)) {
            phase = Phase(0);
            return false;
        }
        return true;
    }

private:
    Rational _rational;
    static PhaseUnit _printUnit;
    template <class T = double>
    requires std::floating_point<T>
    static bool myStrValid(const std::string& str, Phase& p);
};

class setPhaseUnit {
public:
    friend class Phase;
    explicit setPhaseUnit(PhaseUnit pu) : _printUnit(pu) {}
    PhaseUnit getPhaseUnit() const { return _printUnit; }
    friend std::ostream& operator<<(std::ostream& os, const setPhaseUnit& pu);

private:
    PhaseUnit _printUnit;
};

Phase& Phase::operator*=(const Unitless auto& rhs) {
    this->_rational *= rhs;
    normalize();
    return *this;
}
Phase& Phase::operator/=(const Unitless auto& rhs) {
    this->_rational /= rhs;
    normalize();
    return *this;
}
Phase operator*(Phase lhs, const Unitless auto& rhs) {
    lhs *= rhs;
    return lhs;
}
Phase operator*(const Unitless auto& lhs, Phase rhs) {
    return rhs * lhs;
}
Phase operator/(Phase lhs, const Unitless auto& rhs) {
    lhs /= rhs;
    return lhs;
}

template <class T>
requires std::floating_point<T>
bool Phase::myStrValid(const std::string& str, Phase& p) {
    std::vector<std::string> numberStrings;
    std::vector<char> operators;

    // string parsing
    size_t curPos = 0;
    size_t operatorPos = 0;
    while (operatorPos != std::string::npos) {
        operatorPos = str.find_first_of("*/", curPos);
        if (operatorPos != std::string::npos) {
            operators.push_back(str[operatorPos]);
            numberStrings.push_back(str.substr(curPos, operatorPos - curPos));
        } else {
            numberStrings.push_back(str.substr(curPos));
        }
        curPos = operatorPos + 1;
    }

    // Error detection
    if (operators.size() >= numberStrings.size()) return false;

    int numOfPis = 0;
    int numerator = 1, denominator = 1;
    T tempFloat = 1.0;

    int bufferInt;
    T bufferFloat;

    bool doDivision = false;

    for (size_t i = 0; i < numberStrings.size(); ++i) {
        doDivision = (i != 0 && operators[i - 1] == '/');

        if (myStrNCmp("PI", numberStrings[i], 2) == 0) {
            if (doDivision)
                numOfPis -= 1;
            else
                numOfPis += 1;
        } else if (myStrNCmp("-PI", numberStrings[i], 3) == 0) {
            numerator *= -1;
            if (doDivision)
                numOfPis -= 1;
            else
                numOfPis += 1;
        } else if (myStr2Int(numberStrings[i], bufferInt)) {
            if (doDivision)
                denominator *= bufferInt;
            else
                numerator *= bufferInt;
        } else if (myStr2Number<T>(numberStrings[i], bufferFloat)) {
            if (doDivision)
                tempFloat /= bufferFloat;
            else
                tempFloat *= bufferFloat;
        } else {
            return false;
        }
    }

    Rational tempRational(tempFloat * std::pow(std::numbers::pi_v<T>, numOfPis - 1), 1e-4 / std::numbers::pi_v<T>);

    p = Phase(numerator, denominator) * tempRational;

    return true;
}

#endif  // PHASE_H