// Axial Math Core — BigInt implementation
// Small path (|v| < 2^64): pure uint64 arithmetic, zero allocations.
// Big path: base-2^32 limbs, Knuth algorithm D.
#include "axial/bigint.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

// uint64 helpers for the small path ---------------------------------------

bool addU64(uint64_t a, uint64_t b, uint64_t& out) {
    out = a + b;
    return out >= a;             // no overflow
}
bool subU64(uint64_t a, uint64_t b, uint64_t& out) {
    out = a - b;
    return a >= b;               // no underflow
}
bool mulU64(uint64_t a, uint64_t b, uint64_t& out) {
    if (a == 0 || b == 0) { out = 0; return true; }
    out = a * b;
    return out / a == b;         // no overflow
}

}  // namespace

// ---------------------------------------------------------------- factories

BigInt BigInt::fromInt64(int64_t v) {
    if (v == 0) return BigInt();
    uint64_t m = (v < 0) ? (uint64_t)(-(v + 1)) + 1u : (uint64_t)v;
    return fromU64Small(m, v < 0 ? -1 : 1);
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
    // strip leading zeros for the decimal count
    size_t nz = digits.find_first_not_of('0');
    if (nz == std::string::npos) {
        out = BigInt();
        return true;
    }
    digits = digits.substr(nz);
    // small path: up to 19-20 digits fit in uint64 (with range check)
    if (digits.size() <= 19) {
        errno = 0;
        char* end = nullptr;
        unsigned long long v = std::strtoull(digits.c_str(), &end, 10);
        if (errno == 0 && end != nullptr && *end == '\0') {
            out = fromU64Small((uint64_t)v, sign);
            return true;
        }
        // 20-digit boundary values overflow strtoull: fall through to big path
        if (digits.size() <= 19) {
            // shouldn't happen for <= 19 digits, but be safe: big path
        }
    }
    // big path: chunks of nine digits, most significant chunk first
    std::vector<uint32_t> mag;
    size_t start = 0;
    size_t first = digits.size() % 9;
    if (first == 0) first = 9;
    uint32_t chunk = 0;
    for (size_t k = 0; k < first; ++k) chunk = chunk * 10u + (uint32_t)(digits[start + k] - '0');
    mag = addSmall(mag, chunk);
    start += first;
    while (start < digits.size()) {
        chunk = 0;
        for (size_t k = 0; k < 9; ++k) chunk = chunk * 10u + (uint32_t)(digits[start + k] - '0');
        mag = mulSmall(mag, kChunk);
        mag = addSmall(mag, chunk);
        start += 9;
    }
    out = fromVec(std::move(mag), sign);
    return true;
}

std::string BigInt::toDecimal() const {
    if (isZero()) return "0";
    if (!big_) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%llu", (unsigned long long)small_);
        return std::string(isNegative() ? "-" : "") + buf;
    }
    std::vector<uint32_t> mag = mag_;
    std::vector<std::string> chunks;          // least-significant first
    while (!mag.empty()) {
        std::vector<uint32_t> q;
        uint32_t rem = 0;
        divmodSmall(mag, kChunk, q, rem);     // q must not alias mag
        mag.swap(q);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%09u", rem);
        chunks.push_back(std::string(buf));
    }
    std::string out = chunks.back();
    size_t nz = out.find_first_not_of('0');
    out = (nz == std::string::npos) ? "0" : out.substr(nz);
    for (size_t i = chunks.size() - 1; i-- > 0;) out += chunks[i];
    return std::string(isNegative() ? "-" : "") + out;
}

size_t BigInt::decimalDigits() const { return toDecimal().size(); }

bool BigInt::fitsInt64(int64_t& out) const {
    if (big_) {
        if (mag_.size() > 2) return false;
        uint64_t v = mag_.empty() ? 0
                                  : ((uint64_t)mag_[0] |
                                     (mag_.size() > 1 ? ((uint64_t)mag_[1] << 32) : 0));
        if (v > 9007199254740992ULL) return false;
        out = sign_ < 0 ? -(int64_t)v : (int64_t)v;
        return true;
    }
    if (small_ > 9007199254740992ULL) return false;
    out = sign_ < 0 ? -(int64_t)small_ : (int64_t)small_;
    return true;
}

// ---------------------------------------------------------------- comparison

int BigInt::compare(const BigInt& o) const {
    if (sign_ != o.sign_) return sign_ < o.sign_ ? -1 : 1;
    if (sign_ == 0) return 0;
    int c;
    if (!big_ && !o.big_) {
        c = (small_ == o.small_) ? 0 : (small_ < o.small_ ? -1 : 1);
    } else if (big_ && o.big_) {
        c = compareMag(mag_, o.mag_);
    } else {
        // small (≤2 limbs, <2^64) vs big (≥3 limbs): big is always larger
        c = big_ ? 1 : -1;
    }
    return sign_ < 0 ? -c : c;
}

BigInt BigInt::operator-() const {
    if (sign_ == 0) return BigInt();
    BigInt r = *this;
    r.sign_ = -sign_;
    return r;
}

// ---------------------------------------------------------------- arithmetic

BigInt BigInt::operator+(const BigInt& o) const {
    if (sign_ == 0) return o;
    if (o.sign_ == 0) return *this;
    if (!big_ && !o.big_) {
        if (sign_ == o.sign_) {
            uint64_t r;
            if (addU64(small_, o.small_, r)) {
                return fromU64Small(r, sign_);
            }
        } else {
            uint64_t r;
            if (subU64(small_, o.small_, r)) {
                if (r != 0) return fromU64Small(r, sign_);
                return BigInt();
            }
            // underflow → magnitude is o.small_ - small_, sign of o
            if (subU64(o.small_, small_, r)) {
                if (r != 0) return fromU64Small(r, o.sign_);
                return BigInt();
            }
        }
    }
    if (sign_ == o.sign_) {
        return fromVec(addMag(toVec(), o.toVec()), sign_);
    }
    int c = big_ ? (o.big_ ? compareMag(mag_, o.mag_) : 1)
                 : (o.big_ ? -1 : (small_ == o.small_ ? 0 : (small_ < o.small_ ? -1 : 1)));
    if (c == 0) return BigInt();
    if (c > 0) return fromVec(subMag(toVec(), o.toVec()), sign_);
    return fromVec(subMag(o.toVec(), toVec()), o.sign_);
}

BigInt BigInt::operator-(const BigInt& o) const { return *this + (-o); }

BigInt BigInt::operator*(const BigInt& o) const {
    if (sign_ == 0 || o.sign_ == 0) return BigInt();
    if (!big_ && !o.big_) {
        uint64_t r;
        if (mulU64(small_, o.small_, r)) {
            return fromU64Small(r, sign_ * o.sign_);
        }
    }
    return fromVec(mulMag(toVec(), o.toVec()), sign_ * o.sign_);
}

BigInt BigInt::operator/(const BigInt& o) const {
    if (sign_ == 0 || o.sign_ == 0) return BigInt();
    if (!big_ && !o.big_) {
        return fromU64Small(small_ / o.small_, sign_ * o.sign_);
    }
    std::vector<uint32_t> q, r;
    divmodMag(toVec(), o.toVec(), q, r);
    return fromVec(std::move(q), sign_ * o.sign_);
}

bool BigInt::divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r,
                    std::string& err) {
    if (b.isZero()) {
        err = "Division durch Null";
        return false;
    }
    if (!a.big_ && !b.big_) {
        q = fromU64Small(a.small_ / b.small_, a.sign_ * b.sign_);
        uint64_t rem = a.small_ % b.small_;
        r = (rem == 0 || a.sign_ == 0) ? BigInt() : fromU64Small(rem, a.sign_);
        err.clear();
        return true;
    }
    std::vector<uint32_t> qm, rm;
    divmodMag(a.toVec(), b.toVec(), qm, rm);
    q = fromVec(std::move(qm), a.sign_ * b.sign_);
    r = fromVec(std::move(rm), a.sign_);
    err.clear();
    return true;
}

BigInt BigInt::mod(const BigInt& a, const BigInt& b) {
    if (b.isZero()) return BigInt();
    if (!a.big_ && !b.big_) {
        uint64_t rem = a.small_ % b.small_;
        if (rem == 0 || a.sign_ == 0) return BigInt();
        return fromU64Small(rem, a.sign_);
    }
    std::vector<uint32_t> q, r;
    divmodMag(a.toVec(), b.toVec(), q, r);
    return fromVec(std::move(r), a.sign_);
}

BigInt BigInt::gcd(BigInt a, BigInt b) {
    int sa = a.sign_, sb = b.sign_;
    a = abs(a);
    b = abs(b);
    (void)sa;
    (void)sb;
    if (!a.big_ && !b.big_) {
        uint64_t x = a.small_, y = b.small_;
        while (y != 0) {
            uint64_t t = x % y;
            x = y;
            y = t;
        }
        return fromU64Small(x, 1);
    }
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
    if (!a.big_) {
        // small path: double estimate + correction
        uint64_t n = a.small_;
        uint64_t r = (uint64_t)std::sqrt((double)n);
        while (r > 0 && r > n / r) --r;
        while (r + 1 <= n / (r + 1)) ++r;
        out = fromU64Small(r, 1);
        return true;
    }
    size_t bitLen = 32 * (a.mag_.size() - 1) + 32 - clz(a.mag_.back());
    std::vector<uint32_t> res;
    std::vector<uint32_t> one{1};
    for (size_t i = (bitLen + 1) / 2; i-- > 0;) {
        std::vector<uint32_t> bit(limbOf(i) + 1, 0);
        bit[limbOf(i)] = (i % 32 == 0) ? 1u : (1u << (i % 32));
        std::vector<uint32_t> cand = addMag(res, bit);
        std::vector<uint32_t> sq = mulMag(cand, cand);
        if (compareMag(sq, a.mag_) <= 0) res = cand;
    }
    out = fromVec(std::move(res), 1);
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

// ---------------------------------------------------------------- big path

std::vector<uint32_t> BigInt::shiftLeftBits(const std::vector<uint32_t>& a,
                                            int bits) {
    if (a.empty()) return {};
    if (bits == 0) return a;
    if (bits < 0 || bits >= 32) return {};
    std::vector<uint32_t> r(a.size(), 0);
    uint32_t carry = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        r[i] = (a[i] << bits) | carry;
        carry = a[i] >> (32 - bits);
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
    if (b.empty()) return;
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
    un.push_back(0);
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
        int64_t borrow = 0;
        for (size_t i = 0; i < n; ++i) {
            uint64_t pp = qhat * vn[i];
            uint32_t plo = (uint32_t)(pp & 0xffffffffu);
            uint64_t carry = pp >> 32;
            int64_t v = (int64_t)un[jj + i] - (int64_t)plo - borrow;
            int64_t extra = 0;
            while (v < 0) { v += (int64_t)kBase; ++extra; }
            un[jj + i] = (uint32_t)v;
            borrow = (int64_t)carry + extra;
        }
        int64_t t = (int64_t)un[jj + n] - borrow;
        q[jj] = (uint32_t)qhat;
        if (t < 0) {
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

}  // namespace axial
