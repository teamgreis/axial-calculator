// Axial Math Core — runtime value model for the evaluation VM
// Rational (exact) | Real (double) | Complex (double pair)
#pragma once

#include <string>

#include "axial/rational.hpp"

namespace axial {

enum class ValueKind { Rational, Real, Complex };

struct Value {
    ValueKind kind = ValueKind::Real;
    Rational q;                 // valid when kind == Rational
    double re = 0.0;            // valid when Real / Complex
    double im = 0.0;           // imaginary part when Complex

    static Value rational(Rational v) {
        Value r;
        r.kind = ValueKind::Rational;
        r.q = v;
        r.re = v.toDouble();
        return r;
    }
    static Value real(double x) {
        Value r;
        r.kind = ValueKind::Real;
        r.re = x;
        return r;
    }
    static Value complex(double re, double im) {
        Value r;
        r.kind = ValueKind::Complex;
        r.re = re;
        r.im = im;
        return r;
    }

    bool isExact() const { return kind == ValueKind::Rational; }
    bool isComplex() const { return kind == ValueKind::Complex; }
    double toDouble() const {
        if (kind == ValueKind::Rational) return q.toDouble();
        return re;
    }
};

// ELIAN-compatible number formatting:
//   integers < 1e15            -> plain integer string
//   |x| >= 1e15                -> trimmed exponential
//   otherwise                  -> 12 significant digits, trailing zeros stripped
//   non-finite                 -> "∞"
std::string formatDouble(double x);

// Engine text (ASCII minus sign): "1/2", "14", "0.5", "2i", "3 − 4i".
std::string valueToString(const Value& v);

// UI text (Unicode minus "−" where ELIAN's display used it).
std::string valueToStringPretty(const Value& v);

}  // namespace axial