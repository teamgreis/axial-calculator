// Axial Math Core — exact rational numbers (BigInt numerator/denominator)
#pragma once

#include <cstdint>
#include <string>

#include "axial/bigint.hpp"

namespace axial {

struct Rational {
    BigInt n{BigInt::fromInt64(0)};
    BigInt d{BigInt::fromInt64(1)};

    static Rational zero();
    static Rational one();
    static Rational fromInt64(int64_t v);

    // Normalized construction. Fails (err set) on zero denominator.
    static bool make(BigInt n, BigInt d, Rational& out, std::string& err);
    // "whole.frac" decimal text -> exact fraction, e.g. "3","14" -> 157/50.
    static bool fromDecimalDigits(const std::string& whole,
                                  const std::string& frac, Rational& out);
    // Exact conversion of an IEEE-754 double (any finite value).
    static bool fromDouble(double x, Rational& out);

    Rational negated() const {
        Rational r = *this;
        r.n = -r.n;
        return r;
    }
    bool isZero() const { return n.isZero(); }
    bool isInteger() const { return d == BigInt::fromInt64(1); }
    int sign() const { return n.sign(); }
    const BigInt& num() const { return n; }
    const BigInt& den() const { return d; }

    Rational add(const Rational& o) const;
    Rational sub(const Rational& o) const;
    Rational mul(const Rational& o) const;
    static bool div(const Rational& a, const Rational& b, Rational& out,
                    std::string& err);
    // base^exp, exp integer (may be negative); fails for 0^negative.
    static bool powInt(const Rational& base, const BigInt& exp, Rational& out,
                       std::string& err);
    double toDouble() const;
    std::string toString() const;             // "n/d" or "n", ASCII

    // Exact square root when the value is a perfect square of rationals.
    static bool sqrtExact(const Rational& r, Rational& out);

private:
    static Rational fromMagOrExact(BigInt n, BigInt d);
};

}  // namespace axial