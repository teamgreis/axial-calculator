// Axial Math Core — arbitrary precision integers
//
// Two-tier representation:
//   * small path: |value| < 2^64 lives in a uint64_t — zero heap allocation,
//     which is the hot case for literals, small factors and results.
//   * big path: sign-magnitude base-2^32 limbs (Knuth D division) for anything
//     beyond. The generic vector algorithms are unchanged.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace axial {

class BigInt {
public:
    BigInt() = default;                       // zero
    static BigInt fromInt64(int64_t v);
    // Strict decimal parse: optional sign, then 1..N decimal digits.
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

    // Truncated division: q = trunc(a/b), r = a - q*b.
    static bool divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r,
                       std::string& err);
    static BigInt mod(const BigInt& a, const BigInt& b);

    static BigInt gcd(BigInt a, BigInt b);
    static BigInt pow(const BigInt& base, uint64_t exp);
    static bool  isqrt(const BigInt& a, BigInt& out);
    static bool  isPerfectSquare(const BigInt& a, BigInt& rootOut);
    static BigInt abs(const BigInt& a) { return a.isNegative() ? -a : a; }

    size_t decimalDigits() const;
    bool fitsInt64(int64_t& out) const;        // exact when |value| <= 2^53

private:
    friend struct BigIntAccess;

    uint64_t small_ = 0;      // magnitude on the small path (|v| < 2^64)
    bool big_ = false;        // false → small path, true → mag_ holds limbs
    std::vector<uint32_t> mag_;
    int sign_ = 0;

    // ---- small-path helpers ----
    uint32_t smallLimb(size_t i) const {
        return (uint32_t)((small_ >> (32 * i)) & 0xffffffffULL);
    }
    size_t limbCount() const {
        if (!big_) {
            if (small_ == 0) return 0;
            return small_ > 0xffffffffULL ? 2 : 1;
        }
        return mag_.size();
    }
    std::vector<uint32_t> toVec() const {
        if (!big_) {
            if (small_ == 0) return {};
            std::vector<uint32_t> v;
            v.push_back((uint32_t)(small_ & 0xffffffffULL));
            if (small_ > 0xffffffffULL) v.push_back((uint32_t)(small_ >> 32));
            return v;
        }
        return mag_;
    }
    static BigInt fromVec(std::vector<uint32_t> v, int sign) {
        BigInt r;
        if (v.size() <= 2) {
            uint64_t s = 0;
            if (v.size() >= 1) s |= (uint64_t)v[0];
            if (v.size() == 2) s |= (uint64_t)v[1] << 32;
            r.small_ = s;
            r.big_ = false;
            r.sign_ = (s == 0) ? 0 : sign;
            return r;
        }
        r.big_ = true;
        r.mag_ = std::move(v);
        r.sign_ = sign;
        r.normalizeBig();
        return r;
    }
    void normalizeBig() {
        while (!mag_.empty() && mag_.back() == 0) mag_.pop_back();
        if (mag_.empty()) {
            sign_ = 0;
            small_ = 0;
            big_ = false;
        }
    }
    static BigInt fromU64Small(uint64_t mag, int sign) {
        BigInt r;
        r.small_ = mag;
        r.big_ = false;
        r.sign_ = (mag == 0) ? 0 : sign;
        return r;
    }

    // ---- big-path helpers (generic algorithms) ----
    static int compareMag(const std::vector<uint32_t>& a,
                          const std::vector<uint32_t>& b);
    static std::vector<uint32_t> addMag(const std::vector<uint32_t>& a,
                                        const std::vector<uint32_t>& b);
    static std::vector<uint32_t> subMag(const std::vector<uint32_t>& a,
                                        const std::vector<uint32_t>& b);
    static std::vector<uint32_t> mulMag(const std::vector<uint32_t>& a,
                                        const std::vector<uint32_t>& b);
    static void divmodMag(const std::vector<uint32_t>& a,
                          const std::vector<uint32_t>& b,
                          std::vector<uint32_t>& q, std::vector<uint32_t>& r);
    static std::vector<uint32_t> shiftLeftBits(const std::vector<uint32_t>& a,
                                               int bits);
};

}  // namespace axial
