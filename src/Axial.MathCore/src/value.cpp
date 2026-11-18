// Axial Math Core — value formatting
#include "axial/value.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace axial {

std::string formatDouble(double x) {
    if (!std::isfinite(x)) return "\xE2\x88\x9E";  // ∞
    if (x == 0.0) return "0";
    if (std::floor(x) == x && std::fabs(x) < 1e15) {
        int64_t v = (int64_t)x;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", (long long)v);
        return buf;
    }
    if (std::fabs(x) >= 1e15) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.8e", x);
        std::string s(buf);
        // trim: "1.26765060e+30" -> "1.2676506e+30", keep exponent
        size_t epos = s.find('e');
        if (epos != std::string::npos) {
            std::string mant = s.substr(0, epos);
            std::string exp = s.substr(epos);
            if (mant.find('.') != std::string::npos) {
                size_t last = mant.find_last_not_of('0');
                if (mant[last] == '.') --last;
                mant = mant.substr(0, last + 1);
            }
            return mant + exp;
        }
        return s;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.12g", x);
    std::string s(buf);
    // strip trailing zeros after a decimal point
    if (s.find('.') != std::string::npos && s.find('e') == std::string::npos) {
        size_t last = s.find_last_not_of('0');
        if (s[last] == '.') --last;
        s = s.substr(0, last + 1);
    }
    if (s == "-0") s = "0";
    return s;
}

static std::string formatComplex(const Value& v) {
    // Im-only form for pure imaginary: "2i"; otherwise "3 + 4i" / "3 − 4i".
    double re = v.re, im = v.im;
    auto fmt = [](double d) {
        return d == 0.0 ? std::string("0") : formatDouble(d);
    };
    if (re == 0.0) {
        // -3i or 2i
        return (im < 0 ? "-" : "") + formatDouble(std::fabs(im)) + "i";
    }
    std::string s = formatDouble(re);
    if (im == 0.0) return s;
    s += (im < 0) ? " − " : " + ";
    s += formatDouble(std::fabs(im)) + "i";
    return s;
}

std::string valueToString(const Value& v) {
    if (v.kind == ValueKind::Rational) return v.q.toString();
    if (v.kind == ValueKind::Complex) return formatComplex(v);
    return formatDouble(v.re);
}

std::string valueToStringPretty(const Value& v) {
    // Same layout as engine text; Unicode minus already handled inside
    // complex formatting. Rational strings use ASCII '-' per engine contract.
    return valueToString(v);
}

}  // namespace axial