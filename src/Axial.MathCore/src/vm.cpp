// Axial Math Core — compiler + stack VM + function library
//
// Layers in this file:
//   1. slot arithmetic        — exact rational first, double fallback
//   2. function library       — ELIAN-compatible semantics + exact extensions
//   3. Compiler (with constant folding) and vmRun
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

// ---------------------------------------------------------------- slots

struct Slot {
    bool exact = false;     // rational value
    bool complex = false;   // complex value
    Rational q;
    double re = 0.0;
    double im = 0.0;
    mutable bool converted = false;   // dCache holds q's double value once computed
    mutable double dCache = 0.0;

    static Slot ExactQ(Rational v) {
        Slot s;
        s.exact = true;
        s.q = v;            // double conversion is lazy (toD) — never eagerly
        return s;
    }
    static Slot Real(double v) {
        Slot s;
        s.re = v;
        return s;
    }
    static Slot Complex(double re, double im) {
        Slot s;
        s.complex = true;
        s.re = re;
        s.im = im;
        return s;
    }
    double toD() const {
        if (exact && !converted) {
            re = q.toDouble();
            converted = true;
        }
        return re;
    }
};

Value slotToValue(const Slot& s) {
    if (s.exact) return Value::rational(s.q);
    if (s.complex) return Value::complex(s.re, s.im);
    return Value::real(s.re);
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

// ------------------------------------------------------- slot arithmetic ops

bool opAdd(const Slot& a, const Slot& b, Slot& out) {
    if (a.exact && b.exact) { out = Slot::ExactQ(a.q.add(b.q)); return true; }
    out = Slot::Real(a.toD() + b.toD());
    return true;
}
bool opSub(const Slot& a, const Slot& b, Slot& out) {
    if (a.exact && b.exact) { out = Slot::ExactQ(a.q.sub(b.q)); return true; }
    out = Slot::Real(a.toD() - b.toD());
    return true;
}
bool opMul(const Slot& a, const Slot& b, Slot& out) {
    if (a.exact && b.exact) { out = Slot::ExactQ(a.q.mul(b.q)); return true; }
    out = Slot::Real(a.toD() * b.toD());
    return true;
}
bool opDiv(const Slot& a, const Slot& b, Slot& out, int pos, AxialError& e) {
    if (a.exact && b.exact) {
        if (b.q.isZero()) {
            return fail(e, ErrorKind::DivideByZero, pos, "Division durch Null",
                        "Nenner prüfen.");
        }
        std::string msg;
        Rational r;
        if (!Rational::div(a.q, b.q, r, msg)) {
            return fail(e, ErrorKind::DivideByZero, pos, msg,
                        "Nenner prüfen.");
        }
        out = Slot::ExactQ(r);
        return true;
    }
    out = Slot::Real(a.toD() / b.toD());   // IEEE: may yield ∞ (matches ELIAN)
    return true;
}
bool opPow(const Slot& a, const Slot& b, Slot& out, int pos, AxialError& e) {
    // anything^0 = 1 exactly
    if (b.exact && b.q.isZero()) {
        out = Slot::ExactQ(Rational::one());
        return true;
    }
    if (a.exact && b.exact && b.q.isInteger()) {
        const BigInt& ex = b.q.num();
        if (ex < BigInt::fromInt64(-1000) || ex > BigInt::fromInt64(1000)) {
            return fail(e, ErrorKind::Limit, pos, "Exponent zu groß",
                        "Ganze Exponenten nur bis 1000.");
        }
        std::string msg;
        Rational r;
        if (!Rational::powInt(a.q, ex, r, msg)) {
            return fail(e, ErrorKind::DivideByZero, pos, msg);
        }
        out = Slot::ExactQ(r);
        return true;
    }
    {
        double base = a.toD(), ex = b.toD();
        if (base < 0.0 && std::fmod(ex, 1.0) != 0.0) {
            return fail(e, ErrorKind::Domain, pos,
                        "Negative Basis mit gebrochenem Exponenten",
                        "Ganze Exponenten liefern ein reelles Ergebnis.");
        }
        out = Slot::Real(std::pow(base, ex));
    }
    return true;
}
bool opNeg(const Slot& a, Slot& out) {
    if (a.exact) { out = Slot::ExactQ(a.q.negated()); return true; }
    out = Slot::Real(-a.toD());
    return true;
}
bool opFact(const Slot& a, Slot& out, int pos, AxialError& e) {
    const char* hintMsg = "Ganze Zahl von 0 bis 10000 verwenden.";
    if (a.exact && a.q.isInteger()) {
        const BigInt& n = a.q.num();
        if (n.sign() < 0) {
            return fail(e, ErrorKind::Domain, pos,
                        "Fakultät nur für nicht-negative Ganzzahlen", hintMsg);
        }
        if (n > BigInt::fromInt64(10000)) {
            return fail(e, ErrorKind::Limit, pos,
                        "Fakultät nur bis 10000 berechenbar",
                        "Größere Werte mit Näherung berechnen.");
        }
        uint64_t k = (uint64_t)std::strtoull(n.toDecimal().c_str(), nullptr, 10);
        Rational acc = Rational::one();
        for (uint64_t i = 2; i <= k; ++i) {
            acc = acc.mul(Rational::fromInt64((int64_t)i));
        }
        out = Slot::ExactQ(acc);
        return true;
    }
    return fail(e, ErrorKind::Domain, pos,
                "Fakultät nur für nicht-negative Ganzzahlen", hintMsg);
}
bool opPct(const Slot& a, Slot& out) {
    if (a.exact) {
        // x% = x/100 exactly
        std::string msg;
        Rational r;
        if (!Rational::div(a.q, Rational::fromInt64(100), r, msg)) {
            r = Rational::zero();
        }
        out = Slot::ExactQ(r);
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

// exact rounding helpers for rationals -------------------------------------

// truncation toward zero as integer
BigInt truncBig(const Rational& q) {
    std::string msg;
    BigInt whole, rem;
    BigInt::divmod(q.num(), q.den(), whole, rem, msg);
    return whole;
}

bool roundExact(const char* fn, const Slot& a, Slot& out) {
    const Rational& q = a.q;
    BigInt whole = truncBig(q);
    std::string msg;
    Rational wholeR;
    Rational::make(whole, BigInt::fromInt64(1), wholeR, msg);
    Rational rem = q.sub(wholeR);                      // |rem| < 1, sign of q
    if (fn == std::string("trunc")) { out = Slot::ExactQ(wholeR); return true; }
    if (fn == std::string("floor")) {
        if (rem.sign() < 0) wholeR = wholeR.sub(Rational::one());
        out = Slot::ExactQ(wholeR);
        return true;
    }
    if (fn == std::string("ceil")) {
        if (rem.sign() > 0) wholeR = wholeR.add(Rational::one());
        out = Slot::ExactQ(wholeR);
        return true;
    }
    // round: half away from zero  →  compare 2*|rem| with 1
    Rational absRem = rem.sign() < 0 ? rem.negated() : rem;
    // cross-multiply: 2*|rem.n| >= rem.d ?
    BigInt lhs = absRem.num() * BigInt::fromInt64(2);
    if (lhs >= absRem.den()) {
        if (rem.sign() > 0) wholeR = wholeR.add(Rational::one());
        else if (rem.sign() < 0) wholeR = wholeR.sub(Rational::one());
    }
    out = Slot::ExactQ(wholeR);
    return true;
}

bool callFunction(const std::string& fn, const std::vector<Slot>& args,
                  Slot& out, int pos, AngleMode mode, AxialError& e) {
    // arity guard: named functions get exactly the right argument count
    int want = functionTable().find(fn) != functionTable().end()
                   ? functionTable().at(fn).arity
                   : 0;
    if (want >= 0 && (int)args.size() != want) {
        return fail(e, ErrorKind::Domain, pos,
                    fn + ": " + std::to_string(want) +
                        (want == 1 ? " Argument erwartet" : " Argumente erwartet"),
                    "Funktionssignatur prüfen.");
    }
    if (want < 0 && args.empty()) {
        return fail(e, ErrorKind::Domain, pos, fn + ": keine Argumente",
                    "Mindestens ein Argument angeben.");
    }
    bool allExact = true;
    for (const auto& s : args) {
        if (!s.exact) { allExact = false; break; }
    }

    // ---- exact-preserving path (exact rational arguments) ------------------
    if (allExact && !args.empty()) {
        const Rational& a0 = args[0].q;
        if (fn == "abs") {
            out = Slot::ExactQ(a0.sign() < 0 ? a0.negated() : a0);
            return true;
        }
        if (fn == "sign") {
            out = Slot::ExactQ(Rational::fromInt64(a0.sign()));
            return true;
        }
        if (fn == "trunc" || fn == "floor" || fn == "ceil" || fn == "round") {
            return roundExact(fn.c_str(), args[0], out);
        }
        if (fn == "gcd" || fn == "lcm") {
            if (!args[0].q.isInteger() || !args[1].q.isInteger()) {
                return fail(e, ErrorKind::Domain, pos,
                            std::string(fn) + ": ganze Zahlen erwartet");
            }
            BigInt x = BigInt::abs(args[0].q.num());
            BigInt y = BigInt::abs(args[1].q.num());
            BigInt g = BigInt::gcd(x, y);
            BigInt res = (fn == "gcd") ? g : ((x.isZero() || y.isZero())
                                                  ? BigInt::fromInt64(0)
                                                  : (x / g) * y);
            std::string msg;
            Rational r;
            Rational::make(res, BigInt::fromInt64(1), r, msg);
            out = Slot::ExactQ(r);
            return true;
        }
        if (fn == "ncr" || fn == "npr") {
            if (!args[0].q.isInteger() || !args[1].q.isInteger()) {
                return fail(e, ErrorKind::Domain, pos,
                            std::string(fn) + ": ganze Zahlen erwartet");
            }
            int64_t n = (int64_t)std::strtoull(
                BigInt::abs(args[0].q.num()).toDecimal().c_str(), nullptr, 10);
            int64_t r = (int64_t)std::strtoull(
                BigInt::abs(args[1].q.num()).toDecimal().c_str(), nullptr, 10);
            if (r < 0 || n < 0 || r > n) {
                return fail(e, ErrorKind::Domain, pos,
                            std::string(fn) + ": 0 ≤ r ≤ n erwartet");
            }
            BigInt result = BigInt::fromInt64(1);
            if (fn == "npr") {
                // n·(n-1)···(n-r+1) with the original r
                for (int64_t i = 0; i < r; ++i) {
                    result = result * BigInt::fromInt64(n - i);
                }
            } else {
                // binomial coefficient: exploit symmetry on a clamped copy
                int64_t k = (r > n - r) ? (n - r) : r;
                for (int64_t i = 0; i < k; ++i) {
                    result = result * BigInt::fromInt64(n - i);
                    std::string msg;
                    BigInt q, rem;
                    BigInt::divmod(result, BigInt::fromInt64(i + 1), q, rem, msg);
                    if (!rem.isZero()) {
                        return fail(e, ErrorKind::Internal, pos,
                                    fn + ": interne Ungenauigkeit");
                    }
                    result = q;
                }
            }
            std::string msg;
            Rational v;
            Rational::make(result, BigInt::fromInt64(1), v, msg);
            out = Slot::ExactQ(v);
            return true;
        }
        if (fn == "sqrt") {
            if (a0.sign() >= 0) {
                Rational root;
                if (Rational::sqrtExact(a0, root)) {
                    out = Slot::ExactQ(root);
                    return true;
                }
                double v = std::sqrt(a0.toDouble());
                if (!requireFinite(v, pos, e, "sqrt")) return false;
                out = Slot::Real(v);
                return true;
            }
            // negative: exact imaginary when the magnitude is a perfect square
            Rational pos = a0.negated();
            Rational root;
            if (Rational::sqrtExact(pos, root)) {
                out = Slot::Complex(0.0, root.toDouble());
                return true;
            }
            double v = std::sqrt(pos.toDouble());
            out = Slot::Complex(0.0, v);
            return true;
        }
        if (fn == "min" || fn == "max") {
            out = args[0];
            for (size_t i = 1; i < args.size(); ++i) {
                int cmp = Rational::compare(args[i].q, out.q);
                if ((fn == "min" && cmp < 0) || (fn == "max" && cmp > 0)) {
                    out = args[i];
                }
            }
            return true;
        }
        if (fn == "sum") {
            Rational acc = Rational::zero();
            for (const auto& s : args) acc = acc.add(s.q);
            out = Slot::ExactQ(acc);
            return true;
        }
        if (fn == "mean") {
            if (args.empty()) {
                return fail(e, ErrorKind::Domain, pos, "mean: keine Argumente");
            }
            Rational acc = Rational::zero();
            for (const auto& s : args) acc = acc.add(s.q);
            std::string msg;
            Rational v;
            Rational::div(acc, Rational::fromInt64((int64_t)args.size()), v, msg);
            out = Slot::ExactQ(v);
            return true;
        }
    }

    // ---- numeric path -------------------------------------------------------
    double x = args.size() > 0 ? args[0].toD() : 0.0;
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
        double v = std::atan2(x, args[1].toD());
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
        if (fn == "log" && args.size() >= 2) b = args[1].toD();
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
        for (size_t i = 0; i < args.size(); ++i) {
            double v = args[i].toD();
            acc += v;
            if (i == 0) continue;
            if (fn == "min" && v < best) best = v;
            if (fn == "max" && v > best) best = v;
        }
        if (fn == "min") out = Slot::Real(best);
        else if (fn == "max") out = Slot::Real(best);
        else if (fn == "sum") out = Slot::Real(acc);
        else out = Slot::Real(acc / (double)args.size());
        return true;
    }
    return fail(e, ErrorKind::UnknownFunction, pos, "Unbekannte Funktion: " + fn);
}

// ---------------------------------------------------------------- compiling

Slot foldConst(const Node& n, bool& folded) {
    folded = true;
    switch (n.kind) {
        case NodeKind::Num:
            return Slot::ExactQ(n.num);
        case NodeKind::Const: {
            double v = (n.name == "pi") ? kPi
                     : (n.name == "e") ? kE
                     : (n.name == "tau") ? kTau
                     : kPhi;
            return Slot::Real(v);
        }
        default:
            break;
    }
    folded = false;
    return Slot::Real(0.0);
}

struct C {
    Program& p;
    AxialError& err;
    explicit C(Program& prog, AxialError& e) : p(prog), err(e) {}

    int poolQ(const Rational& q) {
        // reuse pool entries: linear scan is fine for small pools
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
        // Complex values are never produced by folding (only Call runtime).
        if (s.exact) {
            emit(Op::PushQ, poolQ(s.q), pos);
        } else {
            emit(Op::PushR, poolR(s.re), pos);
        }
    }

    bool fold(const Node& n, Slot& out) {
        Slot a, b;
        switch (n.kind) {
            case NodeKind::Num:
            case NodeKind::Const: {
                bool f = false;
                out = foldConst(n, f);
                return true;
            }
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
                if (b.exact && b.q.isZero()) return false;   // runtime error
                if (a.exact && b.exact) {
                    std::string msg;
                    Rational r;
                    if (!Rational::div(a.q, b.q, r, msg)) return false;
                    out = Slot::ExactQ(r);
                    return true;
                }
                out = Slot::Real(a.toD() / b.toD());
                return true;
            case NodeKind::Pow:
                if (!fold(*n.kids[0], a)) return false;
                if (!fold(*n.kids[1], b)) return false;
                if (b.exact && b.q.isZero()) {
                    out = Slot::ExactQ(Rational::one());
                    return true;
                }
                if (a.exact && b.exact && b.q.isInteger()) {
                    const BigInt& ex = b.q.num();
                    if (ex < BigInt::fromInt64(-1000) || ex > BigInt::fromInt64(1000))
                        return false;
                    std::string msg;
                    Rational r;
                    if (!Rational::powInt(a.q, ex, r, msg)) return false;
                    out = Slot::ExactQ(r);
                    return true;
                }
                {
                    double base = a.toD(), ex = b.toD();
                    if (base < 0.0 && std::fmod(ex, 1.0) != 0.0) return false;
                    out = Slot::Real(std::pow(base, ex));
                }
                return true;
            case NodeKind::Fact:
                if (!fold(*n.kids[0], a)) return false;
                if (a.exact && a.q.isInteger() && a.q.sign() >= 0 &&
                    a.q.num() <= BigInt::fromInt64(10000)) {
                    AxialError tmp;
                    opFact(a, out, n.pos, tmp);
                    return tmp.ok();
                }
                return false;
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
                emit(Op::PushQ, poolQ(n.num), n.pos);
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
    std::vector<Slot> st;
    st.reserve(32);
    const std::vector<Instr>& code = p.code;
    for (size_t pc = 0; pc < code.size(); ++pc) {
        const Instr& in = code[pc];
        switch (in.op) {
            case Op::PushQ:
                st.push_back(Slot::ExactQ(p.qpool[in.arg]));
                break;
            case Op::PushR:
                st.push_back(Slot::Real(p.rpool[in.arg]));
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
                std::vector<Slot> args(st.end() - argc, st.end());
                st.resize(st.size() - argc);
                Slot r;
                if (!callFunction(fn, args, r, in.pos, env.angle, err)) return false;
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
                    st.push_back(Slot::ExactQ(v->q));
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