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
#include "util.h"
#include <numbers>
#include <string>

enum class PhaseUnit {
    PI, ONE
};

class Phase {

public:
    Phase(): _rational(0, 1) {}
    Phase(int n): _rational(n, 1) { normalize(); }
    Phase(int n, int d): _rational(n, d) { normalize(); }
    template <class T> requires std::floating_point<T>
    Phase(T f, T eps = 1e-4): _rational(f/std::numbers::pi_v<T>, eps/std::numbers::pi_v<T>) { normalize(); }

    friend std::ostream& operator<<(std::ostream& os, const Phase& p);
    Phase operator+() const;
    Phase operator-() const;

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

    template <class T> requires std::floating_point<T>
    T toFloatType() const {return std::numbers::pi_v<T> * _rational.toFloatType<T>(); }

    float toFloat() { return toFloatType<float>(); }
    double toDouble() { return toFloatType<double>(); }
    long double toLongDouble() { return toFloatType<long double>(); }

    Rational getRational() const {
        return _rational;
    }

    template <class T> requires std::floating_point<T>
    static Phase toPhase(T f, T eps = 1e-4){
        Phase p(f, eps);
        return p;
    }

    static PhaseUnit getPrintUnit() {
        return _printUnit;
    }
    static void setPrintUnit(const PhaseUnit& pu) {
        _printUnit = pu;
    }

    std::string getAsciiString() const {
        std::string str;
        if (_rational.numerator() != 1) 
            str += to_string(_rational.numerator()) + "*";
        str += "pi";
        if (_rational.denominator() != 1) 
            str += "/" + to_string(_rational.denominator());
        return str;
    }

    void normalize();

    template<class T = double> requires std::floating_point<T>
    bool fromString(const std::string& str) {
        *this = 0;
        T f;
        if (!myStrValid<T>(str, f)) {
            return false;
        }
        *this = Phase::toPhase(f);
        return true;
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

template<class T = double> requires std::floating_point<T>
bool myStrValid(const std::string &str, T &f)
{
    // stack 1
    vector<char> operators;

    // stack 2
    vector<string> num_strings;
    vector<T> num_floats;

    // put string into stack
    for(size_t i=0; i<str.length(); i++){
        if(str[i] == '*' || str[i] == '/'){
            operators.push_back(str[i]);
        }
        else {
            size_t j = i;
            while(j < str.length()){
                if(str[j] == '*' || str[j] == '/'){
                    num_strings.push_back(str.substr(i,j-i));
                    i = j-1;
                    break;
                }
                else if (j == str.length()-1){
                    num_strings.push_back(str.substr(i,j-i+1));
                    i = j;
                    break;
                }
                else{
                    j++;
                }
            }
        }
    }   

    // Error detect
    if (operators.size() >= num_strings.size()) return false;

    // convert num_strings to num_floats & error detect
    for (auto &str : num_strings) {
        T temp_converted;
        if (str == "pi" || str == "PI") {
            num_floats.push_back(std::numbers::pi_v<T>);
        }
        else if (str == "-pi" || str == "-PI") {
            num_floats.push_back(-std::numbers::pi_v<T>);
        }
        else if (myStr2FloatType<T>(str, temp_converted)){
            num_floats.push_back(temp_converted);
        }
        else {
            return false;
        }

    }

    num_strings.clear();

    // Calculation
    f = num_floats[0];
    for (size_t i=0; i<operators.size(); i++){
        if (operators[i] == '*') f *= num_floats[i+1];
        else                     f /= num_floats[i+1];
    }

    return true;
}

#endif //PHASE_H