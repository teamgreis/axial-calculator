// Axial Math Core — arbitrary precision integers
// Sign-magnitude, base-2^32 limbs (Knuth D division compatible).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace axial {

class BigInt {
public:
    BigInt() = default;                       // zero
    static BigInt fromInt64(int64_t v);
    // Strict decimal parse: optional sign, then 1..N decimal digits. No
    // separators, no exponent. Returns false for malformed input.
    static bool fromDecimal(const std::string& s, BigInt& out);
    std::string toDecimal() const;

    bool isZero() const { return sign_ == 0; }
    bool isNegative() const { return sign_ < 0; }
    int  sign() const { return sign_; }

    int  compare(const BigInt& o) const;      // -1 / 0 / 1
    bool operator==(const BigInt& o) const { return compare(o) == 0; }
    bool operator<(const BigInt& o) const { return compare(o) < 0; }
    bool operator>(const BigInt& o) const { return compare(o) > 0; }
    bool operator<=(const BigInt& o) const { return compare(o) <= 0; }
    bool operator>=(const BigInt& o) const { return compare(o) >= 0; }

    BigInt operator-() const;
    BigInt operator+(const BigInt& o) const;
    BigInt operator-(const BigInt& o) const;
    BigInt operator*(const BigInt& o) const;
    BigInt operator/(const BigInt& o) const;   // truncated; 0 when o == 0 (guarded by callers)

    // Truncated division: q = trunc(a/b), r = a - q*b (|q*b + r| = |a|).
    // Returns false with a message in err when b is zero.
    static bool divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r,
                       std::string& err);
    static BigInt mod(const BigInt& a, const BigInt& b);   // assumes b != 0

    static BigInt gcd(BigInt a, BigInt b);                 // result >= 0
    static BigInt pow(const BigInt& base, uint64_t exp);   // exp >= 0
    static bool  isqrt(const BigInt& a, BigInt& out);      // floor sqrt, a >= 0
    static bool  isPerfectSquare(const BigInt& a, BigInt& rootOut);  // a >= 0
    static BigInt abs(const BigInt& a) { return a.isNegative() ? -a : a; }

    // Number of decimal digits (1 for zero).
    size_t decimalDigits() const;

    // Exact conversion to int64 when |value| <= 2^53 (no strings involved).
    bool fitsInt64(int64_t& out) const {
        if (mag_.size() > 2) return false;
        uint64_t v = mag_.empty()
                         ? 0
                         : ((uint64_t)mag_[0] |
                            (mag_.size() > 1 ? ((uint64_t)mag_[1] << 32) : 0));
        if (v > 9007199254740992ULL) return false;
        out = sign_ < 0 ? -(int64_t)v : (int64_t)v;
        return true;
    }

private:
    friend struct BigIntAccess;
    std::vector<uint32_t> mag_;   // little-endian, no trailing zeros; empty = 0
    int sign_ = 0;                // -1, 0, +1

    static int compareMag(const std::vector<uint32_t>& a,
                          const std::vector<uint32_t>& b);
    static std::vector<uint32_t> addMag(const std::vector<uint32_t>& a,
                                        const std::vector<uint32_t>& b);
    static std::vector<uint32_t> subMag(const std::vector<uint32_t>& a,
                                        const std::vector<uint32_t>& b);  // a >= b
    static std::vector<uint32_t> mulMag(const std::vector<uint32_t>& a,
                                        const std::vector<uint32_t>& b);
    // |a| / |b|, b != 0: quotient and remainder magnitudes.
    static void divmodMag(const std::vector<uint32_t>& a,
                          const std::vector<uint32_t>& b,
                          std::vector<uint32_t>& q, std::vector<uint32_t>& r);
    static std::vector<uint32_t> shiftLeftBits(const std::vector<uint32_t>& a,
                                               int bits);
    static int clz32(uint32_t v);
    void normalize();             // strip trailing zero limbs, fix sign
    static BigInt fromMag(std::vector<uint32_t> mag, int sign);
};

}  // namespace axial