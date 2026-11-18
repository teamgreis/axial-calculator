// Axial Math Core — BigInt tests
#include "testkit.hpp"

#include "axial/bigint.hpp"

using namespace axial;

namespace {

BigInt D(const char* s) {
    BigInt b;
    if (!BigInt::fromDecimal(s, b)) return BigInt();
    return b;
}

}  // namespace

TEST(bigint_basic_decimal_roundtrip) {
    CHECK_EQ_STR(D("0").toDecimal(), "0");
    CHECK_EQ_STR(D("1").toDecimal(), "1");
    CHECK_EQ_STR(D("-123").toDecimal(), "-123");
    CHECK_EQ_STR(D("1000000000000000000000").toDecimal(),
                 "1000000000000000000000");
    // 10^30 needs two 32-bit limbs + carries
    CHECK_EQ_STR(D("1000000000000000000000000000000").toDecimal(),
                 "1000000000000000000000000000000");
}

TEST(bigint_parse_rejects_garbage) {
    BigInt b;
    CHECK(!BigInt::fromDecimal("12x3", b));
    CHECK(!BigInt::fromDecimal("", b));
    CHECK(!BigInt::fromDecimal("-", b));
    CHECK(!BigInt::fromDecimal("1 2", b));
    CHECK(BigInt::fromDecimal("000123", b) && b.toDecimal() == "123");
}

TEST(bigint_add_sub_mul) {
    CHECK_EQ_STR((D("999999999") + D("999999999")).toDecimal(), "1999999998");
    CHECK_EQ_STR((D("4294967295") * D("2")).toDecimal(), "8589934590");
    CHECK_EQ_STR((D("999999999999999999") * D("999999999999999999")).toDecimal(),
                 "999999999999999998000000000000000001");
    CHECK_EQ_STR((D("5") - D("7")).toDecimal(), "-2");
    CHECK_EQ_STR((D("7") - D("7")).toDecimal(), "0");
    CHECK_EQ_STR((D("1000000000000000000000") - D("1")).toDecimal(),
                 "999999999999999999999");
}

TEST(bigint_divmod) {
    BigInt q, r;
    std::string err;
    CHECK(BigInt::divmod(D("100"), D("3"), q, r, err));
    CHECK_EQ_STR(q.toDecimal(), "33");
    CHECK_EQ_STR(r.toDecimal(), "1");
    CHECK(BigInt::divmod(D("-100"), D("3"), q, r, err));
    CHECK_EQ_STR(q.toDecimal(), "-33");
    CHECK_EQ_STR(r.toDecimal(), "-1");
    CHECK(BigInt::divmod(D("7"), D("10"), q, r, err));
    CHECK_EQ_STR(q.toDecimal(), "0");
    CHECK_EQ_STR(r.toDecimal(), "7");
    // large divisor, multi-limb
    CHECK(BigInt::divmod(D("1000000000000000000000000000000000"),
                         D("987654321987654321"), q, r, err));
    CHECK_EQ_STR(q.toDecimal(), "1012499998974843");
    CHECK_EQ_STR(r.toDecimal(), "741753397740753397");
    // zero division reports an error
    CHECK(!BigInt::divmod(D("5"), D("0"), q, r, err));
}

TEST(bigint_divmod_property_random) {
    // deterministic pseudo-random property test over 64-bit magnitudes
    uint64_t seed = 0x12345678ABCDEF01ULL;
    auto next = [&]() {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return seed;
    };
    for (int i = 0; i < 200; ++i) {
        uint64_t av = next() >> 1, bv = next() >> 1;
        if (bv == 0) bv = 1;
        BigInt a = BigInt::fromInt64((int64_t)av);
        BigInt b = BigInt::fromInt64((int64_t)bv);
        BigInt q, r;
        std::string err;
        CHECK(BigInt::divmod(a, b, q, r, err));
        BigInt back = q * b + r;
        CHECK(back == a);
    }
}

TEST(bigint_gcd_and_pow) {
    CHECK_EQ_STR(BigInt::gcd(D("12"), D("18")).toDecimal(), "6");
    CHECK_EQ_STR(BigInt::gcd(D("0"), D("5")).toDecimal(), "5");
    CHECK_EQ_STR(BigInt::gcd(D("1000000007"), D("999999937")).toDecimal(), "1");
    CHECK_EQ_STR(BigInt::pow(D("2"), 100).toDecimal(),
                 "1267650600228229401496703205376");
    CHECK_EQ_STR(BigInt::pow(D("-3"), 3).toDecimal(), "-27");
    CHECK_EQ_STR(BigInt::pow(D("12345"), 0).toDecimal(), "1");
}

TEST(bigint_isqrt_and_squares) {
    BigInt r;
    CHECK(BigInt::isqrt(D("0"), r) && r.toDecimal() == "0");
    CHECK(BigInt::isqrt(D("1"), r) && r.toDecimal() == "1");
    CHECK(BigInt::isqrt(D("2"), r) && r.toDecimal() == "1");
    CHECK(BigInt::isqrt(D("4"), r) && r.toDecimal() == "2");
    CHECK(BigInt::isqrt(D("999999999999"), r) && r.toDecimal() == "999999");
    BigInt root;
    CHECK(BigInt::isPerfectSquare(D("1524157875323869381192020149401"), root));
    CHECK_EQ_STR(root.toDecimal(), "1234567890123451");
    CHECK(!BigInt::isPerfectSquare(D("1234567890123450"), root));
}

TEST(bigint_factorial_like_growth) {
    // 25! ends with 000000 (six factors of 5/2 pairs) and has 26 digits
    BigInt f = BigInt::fromInt64(1);
    for (int i = 2; i <= 25; ++i) f = f * BigInt::fromInt64(i);
    CHECK_EQ_STR(f.toDecimal(), "15511210043330985984000000");
}