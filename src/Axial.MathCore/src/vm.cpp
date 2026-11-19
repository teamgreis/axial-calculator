// Axial Math Core — compiler + stack VM + function library
//
// Performance model (why this file is shaped the way it is):
//   * Slots are POD unless a fraction is involved: exact integers live in a
//     double (exact up to 2^53), fractions only as Rational (heap).
//     The hot path (arithmetic, transcendental calls, plotting) therefore
//     allocates nothing and copies 3 doubles.
//   * Constant pools are precomputed at compile time; no string formatting
//     happens on the evaluation path.
//   * Constant folding collapses pure-constant expressions to one push.
#include "axial/vm.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace axial {

namespace {

const double kPi = 3.14159265358979323846;
const double kE = 2.71828182845904523536;
const double kTau = 2.0 * kPi;
const double kPhi = 1.61803398874989484820;
const double kIntLimit = 9007199254740992.0;   // 2^53: exact integer bound

// ---------------------------------------------------------------- slots

enum SlotKind : uint8_t {
    SK_REAL = 0,     // inexact double
    SK_INT = 1,      // exact integer (in d, |d| <= 2^53)
    SK_RAT = 2,      // exact fraction (q)
    SK_COMPLEX = 3   // complex (d + i*im)
};

struct Slot {
    SlotKind kind = SK_REAL;
    double d = 0.0;    // REAL/INT value, COMPLEX real part
    double im = 0.0;   // COMPLEX imaginary part
    Rational q;        // valid when kind == SK_RAT (empty otherwise: no alloc)

    static Slot Real(double v) {
        Slot s;
        s.kind = SK_REAL;
        s.d = v;
        return s;
    }
    static Slot Int(int64_t v) {
        Slot s;
        s.kind = SK_INT;
        s.d = (double)v;
        return s;
    }
    static Slot IntD(double v) {   // integral double
        Slot s;
        s.kind = SK_INT;
        s.d = v;
        return s;
    }
    static Slot Rat(Rational v) {
        Slot s;
        s.kind = SK_RAT;
        s.q = v;
        return s;
    }
    static Slot Complex(double re, double im) {
        Slot s;
        s.kind = SK_COMPLEX;
        s.d = re;
        s.im = im;
        return s;
    }

    bool isExact() const { return kind == SK_INT || kind == SK_RAT; }
    bool isIntegral() const { return kind == SK_INT; }

    double toD() const {
        if (kind == SK_RAT) return q.toDouble();   // rare: fractions only
        return d;
    }
    // exact rational view (only called on exact slots)
    Rational toQ() const {
        if (kind == SK_RAT) return q;
        std::string err;
        Rational r;
        Rational::fromDouble(d, r);
        return r;
    }
};

Value slotToValue(const Slot& s) {
    if (s.kind == SK_RAT) return Value::rational(s.q);
    if (s.kind == SK_INT) return Value::integer(s.d);
    if (s.kind == SK_COMPLEX) return Value::complex(s.d, s.im);
    return Value::real(s.d);
}

bool fail(AxialError& e, ErrorKind kind, int pos, const std::string& msg,
          const std::string& hint = "") {
    if (e.ok()) {
        e.kind = kind;
        e.pos = pos;
        e.message = msg;
        e.hint = hint;
    }
    return false;
}

// exact integer guard: results beyond 2^53 fall back to Rational
bool fitsInt(double v) {
    return v >= -kIntLimit && v <= kIntLimit && std::floor(v) == v;
}

// exact conversion of an integral double to a fraction (for rare big results)
Rational ratOf(double v) {
    Rational r;
    std::string err;
    Rational::fromDouble(v, r);
    return r;
}

// ------------------------------------------------------- slot arithmetic ops

bool opAdd(const Slot& a, const Slot& b, Slot& out) {
    if (a.kind == SK_RAT && b.kind == SK_RAT) { out = Slot::Rat(a.q.add(b.q)); return true; }
    if (a.kind == SK_INT && b.kind == SK_INT) {
        double r = a.d + b.d;
        if (fitsInt(r)) { out = Slot::IntD(r); return true; }
        out = Slot::Rat(a.toQ().add(b.toQ()));
        return true;
    }
    if (a.isExact() && b.isExact()) { out = Slot::Rat(a.toQ().add(b.toQ())); return true; }
    out = Slot::Real(a.toD() + b.toD());
    return true;
}
bool opSub(const Slot& a, const Slot& b, Slot& out) {
    Slot nb = b;
    if (b.kind == SK_REAL || b.kind == SK_INT) nb.d = -b.d;
    else if (b.kind == SK_COMPLEX) nb.im = -b.im;
    else nb.q = b.q.negated();
    return opAdd(a, nb, out);
}
bool opMul(const Slot& a, const Slot& b, Slot& out) {
    if (a.kind == SK_RAT && b.kind == SK_RAT) { out = Slot::Rat(a.q.mul(b.q)); return true; }
    if (a.kind == SK_INT && b.kind == SK_INT) {
        double r = a.d * b.d;
        if (fitsInt(r)) { out = Slot::IntD(r); return true; }
        out = Slot::Rat(a.toQ().mul(b.toQ()));
        return true;
    }
    if (a.isExact() && b.isExact()) { out = Slot::Rat(a.toQ().mul(b.toQ())); return true; }
    out = Slot::Real(a.toD() * b.toD());
    return true;
}
bool opDiv(const Slot& a, const Slot& b, Slot& out, int pos, AxialError& e) {
    if (a.isExact() && b.isExact()) {
        if (b.kind == SK_INT && b.d == 0.0) {
            return fail(e, ErrorKind::DivideByZero, pos, "Division durch Null",
                        "Nenner prüfen.");
        }
        if (b.kind == SK_RAT && b.q.isZero()) {
            return fail(e, ErrorKind::DivideByZero, pos, "Division durch Null",
                        "Nenner prüfen.");
        }
        if (a.kind == SK_INT && b.kind == SK_INT) {
            double r = a.d / b.d;
            if (fitsInt(r) && std::fmod(a.d, b.d) == 0.0) {
                out = Slot::IntD(r);
                return true;
            }
        }
        std::string msg;
        Rational r;
        if (!Rational::div(a.toQ(), b.toQ(), r, msg)) {
            return fail(e, ErrorKind::DivideByZero, pos, msg);
        }
        out = Slot::Rat(r);
        return true;
    }
    out = Slot::Real(a.toD() / b.toD());   // IEEE: may yield inf (matches ELIAN)
    return true;
}
bool opPow(const Slot& a, const Slot& b, Slot& out, int pos, AxialError& e) {
    // anything^0 = 1 exactly
    if (b.kind == SK_INT && b.d == 0.0) {
        out = Slot::IntD(1.0);
        return true;
    }
    if (a.isExact() && b.isExact() && b.kind == SK_INT) {
        int64_t ex = (int64_t)b.d;
        if (ex > 1000 || ex < -1000) {
            return fail(e, ErrorKind::Limit, pos, "Exponent zu groß",
                        "Ganze Exponenten nur bis 1000.");
        }
        if (a.kind == SK_INT && ex >= 0) {
            double r = std::pow(a.d, (double)ex);
            if (fitsInt(r)) {
                out = Slot::IntD(r);
                return true;
            }
        }
        std::string msg;
        Rational r;
        if (!Rational::powInt(a.toQ(), BigInt::fromInt64(ex), r, msg)) {
            return fail(e, ErrorKind::DivideByZero, pos, msg);
        }
        int64_t iv = 0;
        if (r.isInteger() && r.num().fitsInt64(iv)) {
            out = Slot::IntD((double)iv);
            return true;
        }
        out = Slot::Rat(r);
        return true;
    }
    // fractional/rational exponent or inexact base: numeric
    double base = a.toD(), ex = b.toD();
    if (base < 0.0 && std::fmod(ex, 1.0) != 0.0) {
        return fail(e, ErrorKind::Domain, pos,
                    "Negative Basis mit gebrochenem Exponenten",
                    "Ganze Exponenten liefern ein reelles Ergebnis.");
    }
    out = Slot::Real(std::pow(base, ex));
    return true;
}
bool opNeg(const Slot& a, Slot& out) {
    out = a;
    if (a.kind == SK_REAL || a.kind == SK_INT) out.d = -a.d;
    else if (a.kind == SK_COMPLEX) out.im = -a.im;
    else out.q = a.q.negated();
    return true;
}
bool opFact(const Slot& a, Slot& out, int pos, AxialError& e) {
    const char* hintMsg = "Ganze Zahl von 0 bis 10000 verwenden.";
    if (a.kind == SK_INT) {
        int64_t n = (int64_t)a.d;
        if (n < 0) {
            return fail(e, ErrorKind::Domain, pos,
                        "Fakultät nur für nicht-negative Ganzzahlen", hintMsg);
        }
        if (n > 10000) {
            return fail(e, ErrorKind::Limit, pos,
                        "Fakultät nur bis 10000 berechenbar",
                        "Größere Werte mit Näherung berechnen.");
        }
        if (n <= 18) {   // 18! < 2^53
            double acc = 1.0;
            for (int64_t i = 2; i <= n; ++i) acc *= (double)i;
            out = Slot::IntD(acc);
            return true;
        }
        // exact BigInt fallback
        Rational acc = Rational::one();
        for (int64_t i = 2; i <= n; ++i) acc = acc.mul(Rational::fromInt64(i));
        out = Slot::Rat(acc);
        return true;
    }
    if (a.kind == SK_RAT && a.q.isInteger()) {
        int64_t n = (int64_t)std::strtoll(BigInt::abs(a.q.num()).toDecimal().c_str(),
                                          nullptr, 10);
        if (n < 0 || n > 10000) {
            return fail(e, ErrorKind::Limit, pos,
                        "Fakultät nur bis 10000 berechenbar");
        }
        Rational acc = Rational::one();
        for (int64_t i = 2; i <= n; ++i) acc = acc.mul(Rational::fromInt64(i));
        out = Slot::Rat(acc);
        return true;
    }
    return fail(e, ErrorKind::Domain, pos,
                "Fakultät nur für nicht-negative Ganzzahlen", hintMsg);
}
bool opPct(const Slot& a, Slot& out) {
    // x% = x/100 exactly
    if (a.kind == SK_INT) {
        if (std::fmod(a.d, 100.0) == 0.0) {
            out = Slot::IntD(a.d / 100.0);
            return true;
        }
        std::string msg;
        Rational r;
        Rational::div(a.toQ(), Rational::fromInt64(100), r, msg);
        out = Slot::Rat(r);
        return true;
    }
    if (a.kind == SK_RAT) {
        std::string msg;
        Rational r;
        Rational::div(a.q, Rational::fromInt64(100), r, msg);
        out = Slot::Rat(r);
        return true;
    }
    out = Slot::Real(a.toD() / 100.0);
    return true;
}

// ---------------------------------------------------------------- functions

struct FnDef {
    int arity;    // -1 = variadic
    bool exact;   // exactness preserving for exact integer/rational arguments
};

const std::unordered_map<std::string, FnDef>& functionTable() {
    static const std::unordered_map<std::string, FnDef> table = {
        {"sin", {1, false}},   {"cos", {1, false}},   {"tan", {1, false}},
        {"cot", {1, false}},   {"sec", {1, false}},   {"csc", {1, false}},
        {"asin", {1, false}},  {"acos", {1, false}},  {"atan", {1, false}},
        {"atan2", {2, false}},
        {"sinh", {1, false}},  {"cosh", {1, false}},  {"tanh", {1, false}},
        {"asinh", {1, false}}, {"acosh", {1, false}}, {"atanh", {1, false}},
        {"ln", {1, false}},    {"log", {-1, false}},  {"log2", {1, false}},
        {"log10", {1, false}}, {"exp", {1, false}},
        {"sqrt", {1, true}},   {"cbrt", {1, false}},
        {"abs", {1, true}},    {"sign", {1, true}},   {"floor", {1, true}},
        {"ceil", {1, true}},   {"round", {1, true}},  {"trunc", {1, true}},
        {"min", {-1, true}},   {"max", {-1, true}},   {"sum", {-1, true}},
        {"mean", {-1, true}},  {"gcd", {2, true}},    {"lcm", {2, true}},
        {"ncr", {2, true}},    {"npr", {2, true}},
    };
    return table;
}

double angleFactor(AngleMode m) {
    switch (m) {
        case AngleMode::Deg: return kPi / 180.0;
        case AngleMode::Gon: return kPi / 200.0;
        default: return 1.0;
    }
}

bool requireFinite(double v, int pos, AxialError& e, const char* fn) {
    if (!std::isfinite(v)) {
        return fail(e, ErrorKind::Domain, pos,
                    std::string(fn) + ": Ergebnis nicht definiert",
                    "Definitionsbereich prüfen.");
    }
    return true;
}

// exact rounding for rational slots (fractions only; integers are identity)
BigInt truncBig(const Rational& q) {
    std::string msg;
    BigInt whole, rem;
    BigInt::divmod(q.num(), q.den(), whole, rem, msg);
    return whole;
}

bool roundRat(const char* fn, const Rational& q, Slot& out) {
    BigInt whole = truncBig(q);
    std::string msg;
    Rational wholeR;
    Rational::make(whole, BigInt::fromInt64(1), wholeR, msg);
    if (fn == std::string("trunc")) { out = Slot::Rat(wholeR); return true; }
    Rational rem = q.sub(wholeR);
    if (fn == std::string("floor")) {
        if (rem.sign() < 0) wholeR = wholeR.sub(Rational::one());
        out = Slot::Rat(wholeR);
        return true;
    }
    if (fn == std::string("ceil")) {
        if (rem.sign() > 0) wholeR = wholeR.add(Rational::one());
        out = Slot::Rat(wholeR);
        return true;
    }
    Rational absRem = rem.sign() < 0 ? rem.negated() : rem;
    BigInt lhs = absRem.num() * BigInt::fromInt64(2);
    if (lhs >= absRem.den()) {
        if (rem.sign() > 0) wholeR = wholeR.add(Rational::one());
        else if (rem.sign() < 0) wholeR = wholeR.sub(Rational::one());
    }
    out = Slot::Rat(wholeR);
    return true;
}

bool callFunction(const std::string& fn, const Slot* args, size_t argc,
                  Slot& out, int pos, AngleMode mode, AxialError& e) {
    // arity guard: named functions get exactly the right argument count
    auto it = functionTable().find(fn);
    int want = (it != functionTable().end()) ? it->second.arity : 0;
    if (want >= 0 && (int)argc != want) {
        return fail(e, ErrorKind::Domain, pos,
                    fn + ": " + std::to_string(want) +
                        (want == 1 ? " Argument erwartet" : " Argumente erwartet"),
                    "Funktionssignatur prüfen.");
    }
    if (want < 0 && argc == 0) {
        return fail(e, ErrorKind::Domain, pos, fn + ": keine Argumente",
                    "Mindestens ein Argument angeben.");
    }

    bool allExact = true;
    for (size_t i = 0; i < argc; ++i) {
        if (!args[i].isExact()) { allExact = false; break; }
    }

    // ---- exact-preserving path ----
    if (allExact && argc > 0) {
        const Slot& a0 = args[0];
        if (fn == "abs") {
            out = a0;
            if (a0.kind == SK_INT && a0.d < 0) out.d = -a0.d;
            if (a0.kind == SK_RAT && a0.q.sign() < 0) out.q = a0.q.negated();
            return true;
        }
        if (fn == "sign") {
            int s = a0.kind == SK_INT ? (a0.d > 0 ? 1 : (a0.d < 0 ? -1 : 0))
                                      : a0.q.sign();
            out = Slot::Int(s);
            return true;
        }
        if (fn == "trunc" || fn == "floor" || fn == "ceil" || fn == "round") {
            if (a0.kind == SK_INT) { out = a0; return true; }
            return roundRat(fn.c_str(), a0.q, out);
        }
        if (fn == "gcd" || fn == "lcm") {
            BigInt x, y;
            if (a0.kind == SK_INT) {
                x = BigInt::fromInt64((int64_t)a0.d);
            } else if (a0.q.isInteger()) {
                x = a0.q.num();
            } else {
                return fail(e, ErrorKind::Domain, pos,
                            fn + ": ganze Zahlen erwartet");
            }
            const Slot& a1 = args[1];
            if (a1.kind == SK_INT) {
                y = BigInt::fromInt64((int64_t)a1.d);
            } else if (a1.q.isInteger()) {
                y = a1.q.num();
            } else {
                return fail(e, ErrorKind::Domain, pos,
                            fn + ": ganze Zahlen erwartet");
            }
            BigInt ax = BigInt::abs(x), ay = BigInt::abs(y);
            BigInt g = BigInt::gcd(ax, ay);
            BigInt res = (fn == "gcd") ? g
                                       : ((ax.isZero() || ay.isZero())
                                              ? BigInt::fromInt64(0)
                                              : (ax / g) * ay);
            int64_t iv = 0;
            if (res.fitsInt64(iv)) {
                out = Slot::IntD((double)iv);
            } else {
                std::string msg;
                Rational r;
                Rational::make(res, BigInt::fromInt64(1), r, msg);
                out = Slot::Rat(r);
            }
            return true;
        }
        if (fn == "ncr" || fn == "npr") {
            BigInt nB, rB;
            if (a0.kind == SK_INT) nB = BigInt::fromInt64((int64_t)a0.d);
            else if (a0.q.isInteger()) nB = a0.q.num();
            else return fail(e, ErrorKind::Domain, pos, fn + ": ganze Zahlen erwartet");
            const Slot& a1 = args[1];
            if (a1.kind == SK_INT) rB = BigInt::fromInt64((int64_t)a1.d);
            else if (a1.q.isInteger()) rB = a1.q.num();
            else return fail(e, ErrorKind::Domain, pos, fn + ": ganze Zahlen erwartet");
            int64_t n = (int64_t)std::strtoull(BigInt::abs(nB).toDecimal().c_str(), nullptr, 10);
            int64_t r = (int64_t)std::strtoull(BigInt::abs(rB).toDecimal().c_str(), nullptr, 10);
            if (r < 0 || n < 0 || r > n) {
                return fail(e, ErrorKind::Domain, pos,
                            std::string(fn) + ": 0 ≤ r ≤ n erwartet");
            }
            BigInt result = BigInt::fromInt64(1);
            if (fn == "npr") {
                for (int64_t i = 0; i < r; ++i) {
                    result = result * BigInt::fromInt64(n - i);
                }
            } else {
                int64_t k = (r > n - r) ? (n - r) : r;
                for (int64_t i = 0; i < k; ++i) {
                    result = result * BigInt::fromInt64(n - i);
                    std::string msg;
                    BigInt q, rem;
                    BigInt::divmod(result, BigInt::fromInt64(i + 1), q, rem, msg);
                    if (!rem.isZero()) {
                        return fail(e, ErrorKind::Internal, pos, fn + ": interne Ungenauigkeit");
                    }
                    result = q;
                }
            }
            int64_t iv = 0;
            if (result.fitsInt64(iv)) {
                out = Slot::IntD((double)iv);
            } else {
                std::string msg;
                Rational r;
                Rational::make(result, BigInt::fromInt64(1), r, msg);
                out = Slot::Rat(r);
            }
            return true;
        }
        if (fn == "sqrt" && a0.kind == SK_INT) {
            // integer fast path: perfect-square check without BigInt work
            int64_t iv = (int64_t)a0.d;
            if (iv >= 0) {
                int64_t r = (int64_t)std::sqrt((double)iv);
                while (r > 0 && r * r > iv) --r;
                while ((r + 1) * (r + 1) <= iv) ++r;
                if (r * r == iv) {
                    out = Slot::IntD((double)r);
                } else {
                    double v = std::sqrt(a0.d);
                    if (!requireFinite(v, pos, e, "sqrt")) return false;
                    out = Slot::Real(v);
                }
                return true;
            }
            int64_t p = -iv;
            int64_t r = (int64_t)std::sqrt((double)p);
            while (r > 0 && r * r > p) --r;
            while ((r + 1) * (r + 1) <= p) ++r;
            if (r * r == p) {
                out = Slot::Complex(0.0, (double)r);   // exact 2i
            } else {
                out = Slot::Complex(0.0, std::sqrt((double)p));
            }
            return true;
        }
        if (fn == "sqrt") {
            Rational arg = a0.toQ();
            if (arg.sign() >= 0) {
                Rational root;
                if (Rational::sqrtExact(arg, root)) {
                    if (root.isInteger() && fitsInt(root.toDouble())) {
                        out = Slot::IntD(root.toDouble());
                    } else {
                        out = Slot::Rat(root);
                    }
                    return true;
                }
                double v = std::sqrt(arg.toDouble());
                if (!requireFinite(v, pos, e, "sqrt")) return false;
                out = Slot::Real(v);
                return true;
            }
            Rational posArg = arg.negated();
            Rational root;
            if (Rational::sqrtExact(posArg, root)) {
                out = Slot::Complex(0.0, root.toDouble());   // exact 2i
                return true;
            }
            double v = std::sqrt(posArg.toDouble());
            out = Slot::Complex(0.0, v);
            return true;
        }
        if (fn == "min" || fn == "max") {
            out = args[0];
            bool anyRat = false;
            for (size_t i = 0; i < argc; ++i) {
                if (args[i].kind == SK_RAT) { anyRat = true; break; }
            }
            if (anyRat) {
                for (size_t i = 1; i < argc; ++i) {
                    int cmp = Rational::compare(args[i].toQ(), out.toQ());
                    if ((fn == "min" && cmp < 0) || (fn == "max" && cmp > 0)) {
                        out = args[i];
                    }
                }
            } else {
                for (size_t i = 1; i < argc; ++i) {
                    double v = args[i].d;
                    if ((fn == "min" && v < out.d) || (fn == "max" && v > out.d)) {
                        out = args[i];
                    }
                }
            }
            return true;
        }
        if (fn == "sum") {
            bool anyRat = false;
            for (size_t i = 0; i < argc; ++i) {
                if (args[i].kind == SK_RAT) { anyRat = true; break; }
            }
            if (!anyRat) {
                double acc = 0.0;
                for (size_t i = 0; i < argc; ++i) acc += args[i].d;
                if (fitsInt(acc)) { out = Slot::IntD(acc); return true; }
                std::string msg;
                Rational r;
                Rational::div(ratOf(acc), Rational::one(), r, msg);
                out = Slot::Rat(r);
                return true;
            }
            Rational acc = Rational::zero();
            for (size_t i = 0; i < argc; ++i) acc = acc.add(args[i].toQ());
            if (acc.isInteger() && fitsInt(acc.toDouble())) {
                out = Slot::IntD(acc.toDouble());
                return true;
            }
            out = Slot::Rat(acc);
            return true;
        }
        if (fn == "mean") {
            bool anyRat = false;
            for (size_t i = 0; i < argc; ++i) {
                if (args[i].kind == SK_RAT) { anyRat = true; break; }
            }
            if (!anyRat) {
                double acc = 0.0;
                for (size_t i = 0; i < argc; ++i) acc += args[i].d;
                std::string msg;
                Rational r;
                Rational::div(ratOf(acc),
                              Rational::fromInt64((int64_t)argc), r, msg);
                if (r.isInteger() && fitsInt(r.toDouble())) {
                    out = Slot::IntD(r.toDouble());
                    return true;
                }
                out = Slot::Rat(r);
                return true;
            }
            Rational acc = Rational::zero();
            for (size_t i = 0; i < argc; ++i) acc = acc.add(args[i].toQ());
            std::string msg;
            Rational m;
            Rational::div(acc, Rational::fromInt64((int64_t)argc), m, msg);
            out = Slot::Rat(m);
            return true;
        }
    }

    // ---- numeric path ----
    double x = argc > 0 ? args[0].toD() : 0.0;
    double factor = angleFactor(mode);
    if (fn == "sin") { out = Slot::Real(std::sin(x * factor)); return true; }
    if (fn == "cos") { out = Slot::Real(std::cos(x * factor)); return true; }
    if (fn == "tan") { out = Slot::Real(std::tan(x * factor)); return true; }
    if (fn == "cot") { out = Slot::Real(1.0 / std::tan(x * factor)); return true; }
    if (fn == "sec") { out = Slot::Real(1.0 / std::cos(x * factor)); return true; }
    if (fn == "csc") { out = Slot::Real(1.0 / std::sin(x * factor)); return true; }
    if (fn == "asin") {
        double v = std::asin(x);
        if (!requireFinite(v, pos, e, "asin")) return false;
        out = Slot::Real(v / factor);
        return true;
    }
    if (fn == "acos") {
        double v = std::acos(x);
        if (!requireFinite(v, pos, e, "acos")) return false;
        out = Slot::Real(v / factor);
        return true;
    }
    if (fn == "atan") {
        double v = std::atan(x);
        if (!requireFinite(v, pos, e, "atan")) return false;
        out = Slot::Real(v / factor);
        return true;
    }
    if (fn == "atan2") {
        double v = std::atan2(x, argc > 1 ? args[1].toD() : 0.0);
        if (!requireFinite(v, pos, e, "atan2")) return false;
        out = Slot::Real(v);
        return true;
    }
    if (fn == "sinh") { out = Slot::Real(std::sinh(x)); return true; }
    if (fn == "cosh") { out = Slot::Real(std::cosh(x)); return true; }
    if (fn == "tanh") { out = Slot::Real(std::tanh(x)); return true; }
    if (fn == "asinh") { out = Slot::Real(std::asinh(x)); return true; }
    if (fn == "acosh") {
        double v = std::acosh(x);
        if (!requireFinite(v, pos, e, "acosh")) return false;
        out = Slot::Real(v);
        return true;
    }
    if (fn == "atanh") {
        double v = std::atanh(x);
        if (!requireFinite(v, pos, e, "atanh")) return false;
        out = Slot::Real(v);
        return true;
    }
    if (fn == "ln" || fn == "log" || fn == "log2" || fn == "log10") {
        double b = (fn == "ln") ? kE : (fn == "log2" ? 2.0 : 10.0);
        if (fn == "log" && argc >= 2) b = args[1].toD();
        if (x <= 0.0 || b <= 0.0 || b == 1.0) {
            return fail(e, ErrorKind::Domain, pos,
                        std::string(fn) + ": nur für positive Zahlen definiert",
                        "Argument größer 0 wählen.");
        }
        out = Slot::Real(std::log(x) / std::log(b));
        return true;
    }
    if (fn == "exp") {
        double v = std::exp(x);
        if (!requireFinite(v, pos, e, "exp")) return false;
        out = Slot::Real(v);
        return true;
    }
    if (fn == "cbrt") { out = Slot::Real(std::cbrt(x)); return true; }
    if (fn == "abs") { out = Slot::Real(std::fabs(x)); return true; }
    if (fn == "sign") { out = Slot::Real(x > 0 ? 1.0 : (x < 0 ? -1.0 : 0.0)); return true; }
    if (fn == "trunc") { out = Slot::Real(std::trunc(x)); return true; }
    if (fn == "floor") { out = Slot::Real(std::floor(x)); return true; }
    if (fn == "ceil") { out = Slot::Real(std::ceil(x)); return true; }
    if (fn == "round") { out = Slot::Real(std::round(x)); return true; }
    if (fn == "sqrt") {
        if (x < 0.0) {
            out = Slot::Complex(0.0, std::sqrt(-x));
            return true;
        }
        out = Slot::Real(std::sqrt(x));
        return true;
    }
    if (fn == "min" || fn == "max" || fn == "sum" || fn == "mean") {
        double acc = 0.0, best = x;
        for (size_t i = 0; i < argc; ++i) {
            double v = args[i].toD();
            acc += v;
            if (i == 0) continue;
            if (fn == "min" && v < best) best = v;
            if (fn == "max" && v > best) best = v;
        }
        if (fn == "min") out = Slot::Real(best);
        else if (fn == "max") out = Slot::Real(best);
        else if (fn == "sum") out = Slot::Real(acc);
        else out = Slot::Real(acc / (double)argc);
        return true;
    }
    return fail(e, ErrorKind::UnknownFunction, pos, "Unbekannte Funktion: " + fn);
}

// ---------------------------------------------------------------- compiling

Slot foldConst(const Node& n) {
    switch (n.kind) {
        case NodeKind::Num:
            if (n.num.isInteger() && fitsInt(n.num.toDouble())) {
                return Slot::IntD(n.num.toDouble());
            }
            return Slot::Rat(n.num);
        case NodeKind::Const: {
            double v = (n.name == "pi") ? kPi
                     : (n.name == "e") ? kE
                     : (n.name == "tau") ? kTau
                     : kPhi;
            return Slot::Real(v);
        }
        default:
            return Slot::Real(0.0);
    }
}

struct C {
    Program& p;
    AxialError& err;
    explicit C(Program& prog, AxialError& e) : p(prog), err(e) {}

    int poolQ(const Rational& q) {
        for (size_t i = 0; i < p.qpool.size(); ++i) {
            if (p.qpool[i] == q) return (int)i;
        }
        p.qpool.push_back(q);
        return (int)p.qpool.size() - 1;
    }
    int poolR(double v) {
        for (size_t i = 0; i < p.rpool.size(); ++i) {
            if (p.rpool[i] == v) return (int)i;
        }
        p.rpool.push_back(v);
        return (int)p.rpool.size() - 1;
    }
    int callIdx(const std::string& name) {
        for (size_t i = 0; i < p.calls.size(); ++i) {
            if (p.calls[i] == name) return (int)i;
        }
        p.calls.push_back(name);
        return (int)p.calls.size() - 1;
    }
    int varIdx(const std::string& name) {
        for (size_t i = 0; i < p.vars.size(); ++i) {
            if (p.vars[i] == name) return (int)i;
        }
        p.vars.push_back(name);
        return (int)p.vars.size() - 1;
    }

    void emit(Op op, int32_t arg, int32_t pos) {
        Instr in;
        in.op = op;
        in.arg = arg;
        in.pos = pos;
        p.code.push_back(in);
    }
    void emitPush(const Slot& s, int pos) {
        switch (s.kind) {
            case SK_INT:
                emit(Op::PushI, poolR(s.d), pos);
                break;
            case SK_REAL:
            case SK_COMPLEX:
                emit(Op::PushR, poolR(s.d), pos);
                break;
            case SK_RAT:
                emit(Op::PushQ, poolQ(s.q), pos);
                break;
        }
    }

    bool fold(const Node& n, Slot& out) {
        Slot a, b;
        switch (n.kind) {
            case NodeKind::Num:
            case NodeKind::Const:
                out = foldConst(n);
                return true;
            case NodeKind::Neg:
                if (!fold(*n.kids[0], a)) return false;
                opNeg(a, out);
                return true;
            case NodeKind::Add:
                if (!fold(*n.kids[0], a)) return false;
                if (!fold(*n.kids[1], b)) return false;
                opAdd(a, b, out);
                return true;
            case NodeKind::Sub:
                if (!fold(*n.kids[0], a)) return false;
                if (!fold(*n.kids[1], b)) return false;
                opSub(a, b, out);
                return true;
            case NodeKind::Mul:
                if (!fold(*n.kids[0], a)) return false;
                if (!fold(*n.kids[1], b)) return false;
                opMul(a, b, out);
                return true;
            case NodeKind::Div:
                if (!fold(*n.kids[0], a)) return false;
                if (!fold(*n.kids[1], b)) return false;
                if (b.isExact() &&
                    ((b.kind == SK_INT && b.d == 0.0) ||
                     (b.kind == SK_RAT && b.q.isZero()))) {
                    return false;   // runtime error
                }
                opDiv(a, b, out, n.pos, err);
                return err.ok();
            case NodeKind::Pow:
                if (!fold(*n.kids[0], a)) return false;
                if (!fold(*n.kids[1], b)) return false;
                if (b.kind == SK_INT && b.d == 0.0) {
                    out = Slot::IntD(1.0);
                    return true;
                }
                if (!a.isExact() || !b.isExact()) {
                    double base = a.toD(), ex = b.toD();
                    if (base < 0.0 && std::fmod(ex, 1.0) != 0.0) return false;
                    out = Slot::Real(std::pow(base, ex));
                    return true;
                }
                opPow(a, b, out, n.pos, err);
                return err.ok();
            case NodeKind::Fact:
                if (!fold(*n.kids[0], a)) return false;
                if (a.kind != SK_INT) return false;
                opFact(a, out, n.pos, err);
                return err.ok();
            case NodeKind::Pct:
                if (!fold(*n.kids[0], a)) return false;
                opPct(a, out);
                return true;
            default:
                return false;
        }
    }

    bool emitNode(const Node& n) {
        Slot folded;
        if (fold(n, folded)) {
            emitPush(folded, n.pos);
            return true;
        }
        switch (n.kind) {
            case NodeKind::Num: {
                if (n.num.isInteger() && fitsInt(n.num.toDouble())) {
                    emit(Op::PushI, poolR(n.num.toDouble()), n.pos);
                } else {
                    emit(Op::PushQ, poolQ(n.num), n.pos);
                }
                return true;
            }
            case NodeKind::Const: {
                double v = (n.name == "pi") ? kPi : (n.name == "e") ? kE
                         : (n.name == "tau") ? kTau : kPhi;
                emit(Op::PushR, poolR(v), n.pos);
                return true;
            }
            case NodeKind::Var: {
                emit(Op::LoadVar, varIdx(n.name), n.pos);
                return true;
            }
            case NodeKind::Neg:
                if (!emitNode(*n.kids[0])) return false;
                emit(Op::Neg, 0, n.pos);
                return true;
            case NodeKind::Add:
            case NodeKind::Sub:
            case NodeKind::Mul:
            case NodeKind::Div:
            case NodeKind::Pow:
                if (!emitNode(*n.kids[0])) return false;
                if (!emitNode(*n.kids[1])) return false;
                emit(n.kind == NodeKind::Add ? Op::Add
                     : n.kind == NodeKind::Sub ? Op::Sub
                     : n.kind == NodeKind::Mul ? Op::Mul
                     : n.kind == NodeKind::Div ? Op::Div
                                               : Op::Pow,
                     0, n.pos);
                return true;
            case NodeKind::Fact:
                if (!emitNode(*n.kids[0])) return false;
                emit(Op::Fact, 0, n.pos);
                return true;
            case NodeKind::Pct:
                if (!emitNode(*n.kids[0])) return false;
                emit(Op::Pct, 0, n.pos);
                return true;
            case NodeKind::Call: {
                for (auto& kid : n.kids) {
                    if (!emitNode(*kid)) return false;
                }
                emit(Op::Call, callIdx(n.name), n.pos);
                p.code.back().arg2 = (int32_t)n.kids.size();
                return true;
            }
        }
        return fail(err, ErrorKind::Internal, n.pos, "Knoten nicht kompilierbar");
    }
};

}  // namespace

bool isCoreFunction(const std::string& name) {
    return functionTable().find(name) != functionTable().end();
}

int functionArity(const std::string& name) {
    auto it = functionTable().find(name);
    return it == functionTable().end() ? 0 : it->second.arity;
}

bool Compiler::compile(const Node& root, Program& out, AxialError& err) {
    out = Program{};
    C c(out, err);
    if (!c.emitNode(root)) return false;
    Instr halt;
    halt.op = Op::Halt;
    out.code.push_back(halt);
    return true;
}

bool vmRun(const Program& p, const Environment& env, Value& out, AxialError& err) {
    // thread-local stack: zero allocation per run
    static thread_local std::vector<Slot> st;
    st.clear();
    const std::vector<Instr>& code = p.code;
    for (size_t pc = 0; pc < code.size(); ++pc) {
        const Instr& in = code[pc];
        switch (in.op) {
            case Op::PushQ:
                st.push_back(Slot::Rat(p.qpool[in.arg]));
                break;
            case Op::PushR:
                st.push_back(Slot::Real(p.rpool[in.arg]));
                break;
            case Op::PushI:
                st.push_back(Slot::IntD(p.rpool[in.arg]));
                break;
            case Op::Add: {
                Slot b = st.back(); st.pop_back();
                Slot a = st.back(); st.pop_back();
                Slot r;
                opAdd(a, b, r);
                st.push_back(r);
                break;
            }
            case Op::Sub: {
                Slot b = st.back(); st.pop_back();
                Slot a = st.back(); st.pop_back();
                Slot r;
                opSub(a, b, r);
                st.push_back(r);
                break;
            }
            case Op::Mul: {
                Slot b = st.back(); st.pop_back();
                Slot a = st.back(); st.pop_back();
                Slot r;
                opMul(a, b, r);
                st.push_back(r);
                break;
            }
            case Op::Div: {
                Slot b = st.back(); st.pop_back();
                Slot a = st.back(); st.pop_back();
                Slot r;
                if (!opDiv(a, b, r, in.pos, err)) return false;
                st.push_back(r);
                break;
            }
            case Op::Pow: {
                Slot b = st.back(); st.pop_back();
                Slot a = st.back(); st.pop_back();
                Slot r;
                if (!opPow(a, b, r, in.pos, err)) return false;
                st.push_back(r);
                break;
            }
            case Op::Neg: {
                Slot a = st.back(); st.pop_back();
                Slot r;
                opNeg(a, r);
                st.push_back(r);
                break;
            }
            case Op::Fact: {
                Slot a = st.back(); st.pop_back();
                Slot r;
                if (!opFact(a, r, in.pos, err)) return false;
                st.push_back(r);
                break;
            }
            case Op::Pct: {
                Slot a = st.back(); st.pop_back();
                Slot r;
                opPct(a, r);
                st.push_back(r);
                break;
            }
            case Op::Call: {
                const std::string& fn = p.calls[in.arg];
                int argc = in.arg2;
                if ((int)st.size() < argc) {
                    return fail(err, ErrorKind::Internal, in.pos,
                                fn + ": Stack-Unterlauf");
                }
                Slot* base = st.data() + (st.size() - argc);
                Slot r;
                if (!callFunction(fn, base, (size_t)argc, r, in.pos, env.angle, err)) {
                    return false;
                }
                st.resize(st.size() - argc);
                st.push_back(r);
                break;
            }
            case Op::LoadVar: {
                const std::string& name = p.vars[in.arg];
                const Value* v = env.find(name);
                if (v == nullptr) {
                    return fail(err, ErrorKind::UnknownVariable, in.pos,
                                "Unbekannte Variable: " + name,
                                "Bekannte Variablen: x, y, ans.");
                }
                if (v->kind == ValueKind::Rational) {
                    int64_t iv = 0;
                    if (v->q.isInteger() && v->q.num().fitsInt64(iv)) {
                        st.push_back(Slot::IntD((double)iv));
                    } else {
                        st.push_back(Slot::Rat(v->q));
                    }
                } else if (v->kind == ValueKind::Complex) {
                    st.push_back(Slot::Complex(v->re, v->im));
                } else {
                    st.push_back(Slot::Real(v->re));
                }
                break;
            }
            case Op::Halt:
                if (st.empty()) {
                    return fail(err, ErrorKind::Internal, in.pos, "Leeres Ergebnis");
                }
                out = slotToValue(st.back());
                return true;
        }
    }
    if (st.empty()) {
        return fail(err, ErrorKind::Internal, -1, "Kein Ergebnis");
    }
    out = slotToValue(st.back());
    return true;
}

}  // namespace axial
