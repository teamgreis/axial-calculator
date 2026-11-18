// Axial Math Core — BigInt implementation (base-2^32 limbs, Knuth D division)
#include "axial/bigint.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace axial {

namespace {
constexpr uint64_t kBase = 4294967296ULL;      // 2^32
constexpr uint32_t kChunk = 1000000000u;      // 10^9 (decimal I/O)

int clz(uint32_t v) {
    int n = 0;
    if (v == 0) return 32;
    while ((v & 0x80000000u) == 0) { v <<= 1; ++n; }
    return n;
}

size_t limbOf(size_t bit) { return bit / 32; }

std::vector<uint32_t> mulSmall(const std::vector<uint32_t>& a, uint32_t m) {
    if (a.empty() || m == 0) return {};
    std::vector<uint32_t> r(a.size(), 0);
    uint64_t carry = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t cur = (uint64_t)a[i] * m + carry;
        r[i] = (uint32_t)(cur & 0xffffffffu);
        carry = cur >> 32;
    }
    if (carry) r.push_back((uint32_t)carry);
    return r;
}

std::vector<uint32_t> addSmall(const std::vector<uint32_t>& a, uint32_t v) {
    std::vector<uint32_t> r(a);
    uint64_t carry = v;
    size_t i = 0;
    while (carry && i < r.size()) {
        uint64_t cur = (uint64_t)r[i] + carry;
        r[i] = (uint32_t)(cur & 0xffffffffu);
        carry = cur >> 32;
        ++i;
    }
    if (carry) r.push_back((uint32_t)carry);
    return r;
}

void divmodSmall(const std::vector<uint32_t>& a, uint32_t d,
                 std::vector<uint32_t>& q, uint32_t& rem) {
    q.assign(a.size(), 0);
    uint64_t r = 0;
    for (size_t i = a.size(); i-- > 0;) {
        uint64_t cur = (r << 32) | a[i];
        q[i] = (uint32_t)(cur / d);
        r = cur % d;
    }
    while (!q.empty() && q.back() == 0) q.pop_back();
    rem = (uint32_t)r;
}
}  // namespace

int BigInt::clz32(uint32_t v) { return clz(v); }

std::vector<uint32_t> BigInt::shiftLeftBits(const std::vector<uint32_t>& a,
                                            int bits) {
    if (a.empty()) return {};
    if (bits == 0) return a;
    if (bits < 0 || bits >= 32) return {};   // not supported by this helper
    std::vector<uint32_t> r(a.size(), 0);
    uint32_t carry = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        r[i] = (a[i] << bits) | carry;
        carry = (bits == 0) ? 0 : (a[i] >> (32 - bits));
    }
    if (carry) r.push_back(carry);
    return r;
}

int BigInt::compareMag(const std::vector<uint32_t>& a,
                       const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (size_t i = a.size(); i-- > 0;) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

std::vector<uint32_t> BigInt::addMag(const std::vector<uint32_t>& a,
                                     const std::vector<uint32_t>& b) {
    std::vector<uint32_t> r(std::max(a.size(), b.size()) + 1, 0);
    uint64_t carry = 0;
    for (size_t i = 0; i < r.size(); ++i) {
        uint64_t cur = carry;
        if (i < a.size()) cur += a[i];
        if (i < b.size()) cur += b[i];
        r[i] = (uint32_t)(cur & 0xffffffffu);
        carry = cur >> 32;
    }
    while (!r.empty() && r.back() == 0) r.pop_back();
    return r;
}

std::vector<uint32_t> BigInt::subMag(const std::vector<uint32_t>& a,
                                     const std::vector<uint32_t>& b) {
    // requires |a| >= |b|
    std::vector<uint32_t> r(a.size(), 0);
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        int64_t cur = (int64_t)a[i] - borrow - (i < b.size() ? (int64_t)b[i] : 0);
        if (cur < 0) { cur += (int64_t)kBase; borrow = 1; } else borrow = 0;
        r[i] = (uint32_t)cur;
    }
    while (!r.empty() && r.back() == 0) r.pop_back();
    return r;
}

std::vector<uint32_t> BigInt::mulMag(const std::vector<uint32_t>& a,
                                     const std::vector<uint32_t>& b) {
    if (a.empty() || b.empty()) return {};
    std::vector<uint32_t> r(a.size() + b.size(), 0);
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t carry = 0;
        if (a[i] == 0) continue;
        for (size_t j = 0; j < b.size(); ++j) {
            uint64_t cur = (uint64_t)r[i + j] + (uint64_t)a[i] * b[j] + carry;
            r[i + j] = (uint32_t)(cur & 0xffffffffu);
            carry = cur >> 32;
        }
        size_t k = i + b.size();
        while (carry) {
            uint64_t cur = (uint64_t)r[k] + carry;
            r[k] = (uint32_t)(cur & 0xffffffffu);
            carry = cur >> 32;
            ++k;
        }
    }
    while (!r.empty() && r.back() == 0) r.pop_back();
    return r;
}

void BigInt::divmodMag(const std::vector<uint32_t>& a,
                       const std::vector<uint32_t>& b,
                       std::vector<uint32_t>& q, std::vector<uint32_t>& r) {
    q.clear();
    r.clear();
    if (b.empty()) return;                    // caller guarantees b != 0
    if (compareMag(a, b) < 0) { r = a; return; }

    if (b.size() == 1) {
        uint32_t d = b[0];
        uint32_t rem = 0;
        q.assign(a.size(), 0);
        for (size_t i = a.size(); i-- > 0;) {
            uint64_t cur = ((uint64_t)rem << 32) | a[i];
            q[i] = (uint32_t)(cur / d);
            rem = (uint32_t)(cur % d);
        }
        while (!q.empty() && q.back() == 0) q.pop_back();
        if (rem) r.push_back(rem);
        return;
    }

    // Knuth algorithm D, base 2^32 (Hacker's Delight formulation).
    int s = clz(b.back());
    std::vector<uint32_t> vn = shiftLeftBits(b, s);
    std::vector<uint32_t> un = shiftLeftBits(a, s);
    un.push_back(0);                          // extra head limb
    size_t n = vn.size();
    if (un.size() < n + 1) { r = a; return; }
    size_t m = un.size() - n - 1;
    q.assign(m + 1, 0);

    for (size_t jj = m + 1; jj-- > 0;) {
        uint64_t num = ((uint64_t)un[jj + n] << 32) | un[jj + n - 1];
        uint64_t qhat = num / vn[n - 1];
        uint64_t rhat = num % vn[n - 1];
        while (qhat >= kBase ||
               (n >= 2 && qhat * vn[n - 2] > kBase * rhat + un[jj + n - 2])) {
            --qhat;
            rhat += vn[n - 1];
            if (rhat >= kBase) break;
        }
        // multiply and subtract qhat * vn from un[jj .. jj+n]
        int64_t borrow = 0;
        for (size_t i = 0; i < n; ++i) {
            uint64_t p = qhat * vn[i];
            uint32_t plo = (uint32_t)(p & 0xffffffffu);
            uint64_t carry = p >> 32;
            int64_t v = (int64_t)un[jj + i] - (int64_t)plo - borrow;
            int64_t extra = 0;
            while (v < 0) { v += (int64_t)kBase; ++extra; }
            un[jj + i] = (uint32_t)v;
            borrow = (int64_t)carry + extra;
        }
        int64_t t = (int64_t)un[jj + n] - borrow;
        q[jj] = (uint32_t)qhat;
        if (t < 0) {
            // overestimate: add divisor back
            q[jj] = (uint32_t)(qhat - 1);
            uint64_t c = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t sum = (uint64_t)un[jj + i] + vn[i] + c;
                un[jj + i] = (uint32_t)(sum & 0xffffffffu);
                c = sum >> 32;
            }
            un[jj + n] = (uint32_t)((uint64_t)un[jj + n] + c);
        } else {
            un[jj + n] = (uint32_t)t;
        }
    }

    // remainder = first n limbs of un, shifted back right by s
    std::vector<uint32_t> rem(un.begin(), un.begin() + n);
    if (s != 0) {
        std::vector<uint32_t> sh(n, 0);
        for (size_t i = 0; i < n; ++i) {
            uint32_t lo = rem[i] >> s;
            uint32_t hi = (i + 1 < n) ? (rem[i + 1] << (32 - s)) : 0;
            sh[i] = lo | hi;
        }
        rem.swap(sh);
    }
    r = rem;
    while (!r.empty() && r.back() == 0) r.pop_back();
}

void BigInt::normalize() {
    while (!mag_.empty() && mag_.back() == 0) mag_.pop_back();
    if (mag_.empty()) sign_ = 0;
    else if (sign_ == 0) sign_ = 1;
}

BigInt BigInt::fromMag(std::vector<uint32_t> mag, int sign) {
    BigInt r;
    r.mag_ = std::move(mag);
    r.sign_ = sign;
    r.normalize();
    return r;
}

BigInt BigInt::fromInt64(int64_t v) {
    if (v == 0) return BigInt();
    uint64_t m = (v < 0) ? (uint64_t)(-(v + 1)) + 1u : (uint64_t)v;
    BigInt r;
    r.mag_.push_back((uint32_t)(m & 0xffffffffu));
    if (m >> 32) r.mag_.push_back((uint32_t)(m >> 32));
    r.sign_ = (v < 0) ? -1 : 1;
    return r;
}

bool BigInt::fromDecimal(const std::string& s, BigInt& out) {
    size_t i = 0;
    int sign = 1;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        if (s[i] == '-') sign = -1;
        ++i;
    }
    if (i >= s.size()) return false;
    for (size_t k = i; k < s.size(); ++k) {
        if (s[k] < '0' || s[k] > '9') return false;
    }
    std::string digits = s.substr(i);
    std::vector<uint32_t> mag;
    size_t start = 0;
    while (start < digits.size()) {
        size_t take = std::min<size_t>(9, digits.size() - start);
        uint32_t chunk = 0;
        for (size_t k = 0; k < take; ++k) {
            chunk = chunk * 10u + (uint32_t)(digits[start + k] - '0');
        }
        if (take == 9) {
            mag = mulSmall(mag, kChunk);
            mag = addSmall(mag, chunk);
        } else {
            // leading (short) chunk: value < 10^take
            for (size_t k = 0; k < take; ++k) {
                mag = mulSmall(mag, 10u);
            }
            mag = addSmall(mag, chunk);
        }
        start += take;
    }
    out = fromMag(std::move(mag), sign);
    return true;
}

std::string BigInt::toDecimal() const {
    if (isZero()) return "0";
    std::vector<uint32_t> mag = mag_;
    std::string out;
    while (!mag.empty()) {
        uint32_t rem = 0;
        divmodSmall(mag, kChunk, mag, rem);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%09u", rem);
        if (out.empty()) {
            // most significant chunk: strip leading zeros
            std::string chunk(buf);
            size_t nz = chunk.find_first_not_of('0');
            out = (nz == std::string::npos) ? "0" : chunk.substr(nz);
        } else {
            out = std::string(buf) + out;
        }
    }
    std::string s = isNegative() ? "-" : "";
    s += out;
    return s;
}

size_t BigInt::decimalDigits() const { return toDecimal().size(); }

int BigInt::compare(const BigInt& o) const {
    if (sign_ != o.sign_) return sign_ < o.sign_ ? -1 : 1;
    if (sign_ == 0) return 0;
    int c = compareMag(mag_, o.mag_);
    return sign_ < 0 ? -c : c;
}

BigInt BigInt::operator-() const {
    if (sign_ == 0) return BigInt();
    return fromMag(mag_, -sign_);
}

BigInt BigInt::operator+(const BigInt& o) const {
    if (sign_ == 0) return o;
    if (o.sign_ == 0) return *this;
    if (sign_ == o.sign_) return fromMag(addMag(mag_, o.mag_), sign_);
    int c = compareMag(mag_, o.mag_);
    if (c == 0) return BigInt();
    if (c > 0) return fromMag(subMag(mag_, o.mag_), sign_);
    return fromMag(subMag(o.mag_, mag_), o.sign_);
}

BigInt BigInt::operator-(const BigInt& o) const { return *this + (-o); }

BigInt BigInt::operator*(const BigInt& o) const {
    if (sign_ == 0 || o.sign_ == 0) return BigInt();
    return fromMag(mulMag(mag_, o.mag_), sign_ * o.sign_);
}

BigInt BigInt::operator/(const BigInt& o) const {
    if (sign_ == 0 || o.sign_ == 0) return BigInt();
    std::vector<uint32_t> qm, rm;
    divmodMag(mag_, o.mag_, qm, rm);
    return fromMag(std::move(qm), sign_ * o.sign_);
}

bool BigInt::divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r,
                    std::string& err) {
    if (b.isZero()) {
        err = "Division durch Null";
        return false;
    }
    std::vector<uint32_t> qm, rm;
    divmodMag(a.mag_, b.mag_, qm, rm);
    int qs = a.sign_ * b.sign_;
    q = fromMag(std::move(qm), qs);
    r = fromMag(std::move(rm), a.sign_);
    err.clear();
    return true;
}

BigInt BigInt::mod(const BigInt& a, const BigInt& b) {
    if (b.isZero()) return BigInt();
    std::vector<uint32_t> qm, rm;
    divmodMag(a.mag_, b.mag_, qm, rm);
    return fromMag(std::move(rm), a.sign_);
}

BigInt BigInt::gcd(BigInt a, BigInt b) {
    a = abs(a);
    b = abs(b);
    while (!b.isZero()) {
        BigInt t = mod(a, b);
        a = b;
        b = t;
    }
    return a;
}

BigInt BigInt::pow(const BigInt& base, uint64_t exp) {
    BigInt result = BigInt::fromInt64(1);
    BigInt b = base;
    while (exp) {
        if (exp & 1u) result = result * b;
        exp >>= 1;
        if (exp) b = b * b;
    }
    return result;
}

bool BigInt::isqrt(const BigInt& a, BigInt& out) {
    if (a.isNegative()) return false;
    if (a.isZero()) { out = BigInt(); return true; }
    size_t bitLen = 32 * (a.mag_.size() - 1) + 32 - clz(a.mag_.back());
    std::vector<uint32_t> res;          // magnitude of result (0)
    for (size_t i = (bitLen + 1) / 2; i-- > 0;) {
        // cand = res + 2^i
        std::vector<uint32_t> bit(limbOf(i) + 1, 0);
        bit[limbOf(i)] = (i % 32 == 0) ? 1u : (1u << (i % 32));
        std::vector<uint32_t> cand = addMag(res, bit);
        // cand*cand <= a ?
        std::vector<uint32_t> sq = mulMag(cand, cand);
        if (compareMag(sq, a.mag_) <= 0) res = cand;
    }
    out = fromMag(std::move(res), a.sign_ == 0 ? 0 : 1);
    return true;
}

bool BigInt::isPerfectSquare(const BigInt& a, BigInt& rootOut) {
    if (a.isNegative()) return false;
    if (a.isZero()) { rootOut = BigInt(); return true; }
    BigInt r;
    if (!isqrt(a, r)) return false;
    if ((r * r) == a) { rootOut = r; return true; }
    return false;
}

}  // namespace axial