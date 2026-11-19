// Axial Math Core — Rational implementation
#include "axial/rational.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace axial {

namespace {
BigInt bigFromU64(uint64_t v) {
    BigInt lo = BigInt::fromInt64((int64_t)(v & 0xffffffffu));
    BigInt hi = BigInt::fromInt64((int64_t)(v >> 32));
    return lo + hi * BigInt::fromInt64(4294967296LL);
}

BigInt tenPow(uint64_t k) { return BigInt::pow(BigInt::fromInt64(10), k); }
}  // namespace

Rational Rational::zero() { return fromInt64(0); }
Rational Rational::one() { return fromInt64(1); }

Rational Rational::fromMagOrExact(BigInt n, BigInt d) {
    Rational r;
    r.n = n;
    r.d = d;
    return r;
}

Rational Rational::fromInt64(int64_t v) {
    Rational r;
    r.n = BigInt::fromInt64(v);
    r.d = BigInt::fromInt64(1);
    return r;
}

bool Rational::make(BigInt n, BigInt d, Rational& out, std::string& err) {
    if (d.isZero()) {
        err = "Division durch Null";
        return false;
    }
    if (d.isNegative()) { n = -n; d = -d; }
    if (n.isZero()) { out = zero(); return true; }
    BigInt g = BigInt::gcd(n, d);
    if (!(g == BigInt::fromInt64(1))) { n = n / g; d = d / g; }
    // division via divmod semantics: use operator/-style from BigInt (exact)
    // BigInt division produces truncated quotient; n/g, d/g are exact here.
    out = fromMagOrExact(n, d);
    return true;
}

bool Rational::fromDecimalDigits(const std::string& whole,
                                 const std::string& frac, Rational& out) {
    std::string digits = whole + frac;
    if (digits.empty()) { out = zero(); return true; }
    BigInt n;
    if (!BigInt::fromDecimal(digits.empty() ? "0" : digits, n)) return false;
    std::string err;
    if (frac.empty()) return make(n, BigInt::fromInt64(1), out, err);
    return make(n, tenPow(frac.size()), out, err);
}

bool Rational::fromDouble(double x, Rational& out) {
    if (!std::isfinite(x)) return false;
    if (x == 0.0) { out = zero(); return true; }
    int exp = 0;
    double m = std::frexp(x, &exp);           // x = m * 2^exp, 0.5 <= |m| < 1
    // scale mantissa to an integer: m * 2^53 is exact
    double mant = std::ldexp(m, 53);          // integer, |mant| < 2^53
    uint64_t mi = (uint64_t)std::fabs(mant);
    BigInt num = bigFromU64(mi);
    if (mant < 0) num = -num;
    BigInt den = BigInt::fromInt64(1);
    int e = exp - 53;
    if (e >= 0) {
        num = num * BigInt::pow(BigInt::fromInt64(2), (uint64_t)e);
    } else {
        den = BigInt::pow(BigInt::fromInt64(2), (uint64_t)(-e));
    }
    std::string err;
    return make(num, den, out, err);
}

Rational Rational::add(const Rational& o) const {
    if (isZero()) return o;
    if (o.isZero()) return *this;
    if (d == BigInt::fromInt64(1) && o.d == BigInt::fromInt64(1)) {
        Rational r;                      // integer fast path: no gcd
        r.n = n + o.n;
        return r;
    }
    // lcm approach as ELIAN: d = lcm(d1,d2)
    BigInt g = BigInt::gcd(d, o.d);
    BigInt l = d / g * o.d;    // exact
    Rational r;
    r.n = n * (l / d) + o.n * (l / o.d);
    r.d = l;
    if (r.n.isZero()) return zero();
    BigInt g2 = BigInt::gcd(r.n, l);
    if (!(g2 == BigInt::fromInt64(1))) { r.n = r.n / g2; r.d = l / g2; }
    if (r.n.isZero()) r.d = BigInt::fromInt64(1);
    return r;
}

Rational Rational::sub(const Rational& o) const { return add(o.negated()); }

int Rational::compare(const Rational& a, const Rational& b) {
    // denominators are always positive: compare cross products
    BigInt lhs = a.n * b.d;
    BigInt rhs = b.n * a.d;
    return lhs.compare(rhs);
}

Rational Rational::mul(const Rational& o) const {
    if (isZero() || o.isZero()) return zero();
    if (d == BigInt::fromInt64(1) && o.d == BigInt::fromInt64(1)) {
        Rational r;                      // integer fast path: no gcd
        r.n = n * o.n;
        return r;
    }
    // cross-cancel to keep operands small
    BigInt g1 = BigInt::gcd(n, o.d);
    BigInt g2 = BigInt::gcd(o.n, d);
    Rational r;
    r.n = (n / g1) * (o.n / g2);
    r.d = (d / g2) * (o.d / g1);
    return r;
}

bool Rational::div(const Rational& a, const Rational& b, Rational& out,
                   std::string& err) {
    if (b.isZero()) {
        err = "Division durch Null";
        return false;
    }
    // reciprocal then multiply: 1/b = den/num
    Rational inv;
    std::string e2;
    if (!make(b.d, b.n, inv, e2)) { err = e2; return false; }
    out = a.mul(inv);
    return true;
}

bool Rational::powInt(const Rational& base, const BigInt& exp, Rational& out,
                      std::string& err) {
    // base^exp with integer exponent
    BigInt e = exp;
    if (base.isZero()) {
        if (e.sign() < 0) { err = "Division durch Null"; return false; }
        out = e.isZero() ? one() : zero();
        return true;
    }
    bool neg = e.isNegative();
    uint64_t k = 0;
    {
        // |e| as uint64 — value range is guarded by the caller (|exp| < 1000)
        BigInt a = BigInt::abs(e);
        std::string s = a.toDecimal();
        k = std::strtoull(s.c_str(), nullptr, 10);
    }
    Rational factor = one();
    Rational b = base;
    uint64_t e2 = k;
    while (e2) {
        if (e2 & 1u) factor = factor.mul(b);
        e2 >>= 1;
        if (e2) b = b.mul(b);
    }
    if (neg) {
        return div(one(), factor, out, err);
    }
    out = factor;
    return true;
}

double Rational::toDouble() const {
    if (isZero()) return 0.0;
    // Small magnitudes: exact int64 conversion, then one rounded division.
    size_t dn = n.decimalDigits(), dd = d.decimalDigits();
    if (dn <= 18 && dd <= 18) {
        int64_t ni = std::strtoll(n.toDecimal().c_str(), nullptr, 10);
        int64_t di = std::strtoll(d.toDecimal().c_str(), nullptr, 10);
        if (di != 0) return (double)ni / (double)di;
    }
    // Long division to 17 significant digits, then strtod (rounds correctly).
    BigInt an = BigInt::abs(n);
    BigInt ip, rp;
    std::string err;
    BigInt::divmod(an, d, ip, rp, err);
    std::string intPart = ip.isZero() ? "0" : ip.toDecimal();
    std::string frac;
    int need = 17 - (int)intPart.size();
    BigInt cur = rp;
    BigInt ten = BigInt::fromInt64(10);
    int guard = 0;
    while (!cur.isZero() && (int)frac.size() < need + 2 && guard++ < 80) {
        BigInt num = cur * ten;
        BigInt qd, rd;
        BigInt::divmod(num, d, qd, rd, err);
        std::string qds = qd.toDecimal();
        frac.push_back(qds.empty() ? '0' : qds[0]);
        cur = rd;
    }
    if (frac.empty()) frac.push_back('0');
    std::string s = (n.isNegative() ? "-" : "") + intPart + "." + frac;
    return std::strtod(s.c_str(), nullptr);
}

std::string Rational::toString() const {
    if (n.isZero()) return "0";
    std::string ns = n.toDecimal();
    if (d == BigInt::fromInt64(1)) return ns;
    return ns + "/" + d.toDecimal();
}

bool Rational::sqrtExact(const Rational& r, Rational& out) {
    if (r.sign() < 0) return false;
    BigInt rn, rd;
    if (!BigInt::isPerfectSquare(r.n, rn)) return false;
    if (!BigInt::isPerfectSquare(r.d, rd)) return false;
    std::string err;
    return make(rn, rd, out, err);
}

}  // namespace axial