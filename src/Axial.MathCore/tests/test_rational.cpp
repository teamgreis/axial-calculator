// Axial Math Core — Rational tests
#include "testkit.hpp"

#include "axial/bigint.hpp"
#include "axial/rational.hpp"

using namespace axial;

namespace {

Rational mk(int64_t n, int64_t d) {
    Rational q;
    std::string err;
    Rational::make(BigInt::fromInt64(n), BigInt::fromInt64(d), q, err);
    return q;
}

Rational D(const char* whole, const char* frac) {
    Rational q;
    if (!Rational::fromDecimalDigits(whole, frac, q)) return Rational::zero();
    return q;
}

}  // namespace

TEST(rational_decimal_exact) {
    CHECK_EQ_STR(D("3", "14").toString(), "157/50");
    CHECK_EQ_STR(D("0", "1").toString(), "1/10");
    CHECK_EQ_STR(D("7", "").toString(), "7");
    CHECK_EQ_STR(D("0", "3401").toString(), "3401/10000");
    CHECK_EQ_STR(D("1000000", "5").toString(), "2000001/2");
    CHECK_EQ_STR(D("-0", "5").toString(), "-1/2");
}

TEST(rational_arithmetic) {
    CHECK_EQ_STR(D("1", "3").add(D("1", "6")).toString(), "1/2");
    CHECK_EQ_STR(D("2", "4").toString(), "1/2");
    CHECK_EQ_STR(mk(1, 2).mul(mk(1, 2)).mul(mk(1, 2)).toString(), "1/8");
    CHECK_EQ_STR(mk(1, 7).mul(mk(7, 1)).toString(), "1");
    CHECK_EQ_STR(D("0", "1").add(D("0", "2")).toString(), "3/10");
    CHECK_EQ_STR(mk(1, 3).mul(mk(1, 3)).toString(), "1/9");
    CHECK_EQ_STR(mk(-1, 2).toString(), "-1/2");
    CHECK_EQ_STR(mk(6, 18).toString(), "1/3");
    CHECK_EQ_STR(mk(1, 3).sub(mk(1, 6)).toString(), "1/6");
}

TEST(rational_pow) {
    Rational out;
    std::string err;
    CHECK(Rational::powInt(mk(2, 1), BigInt::fromInt64(-2), out, err));
    CHECK_EQ_STR(out.toString(), "1/4");
    CHECK(Rational::powInt(mk(1, 2), BigInt::fromInt64(3), out, err));
    CHECK_EQ_STR(out.toString(), "1/8");
    CHECK(Rational::powInt(Rational::zero(), BigInt::fromInt64(0), out, err));
    CHECK_EQ_STR(out.toString(), "1");
    CHECK(!Rational::powInt(Rational::zero(), BigInt::fromInt64(-1), out, err));
}

TEST(rational_fromdouble_exact) {
    Rational q;
    CHECK(Rational::fromDouble(0.5, q));
    CHECK_EQ_STR(q.toString(), "1/2");
    CHECK(Rational::fromDouble(-0.25, q));
    CHECK_EQ_STR(q.toString(), "-1/4");
    CHECK(Rational::fromDouble(2.0, q));
    CHECK_EQ_STR(q.toString(), "2");
    CHECK(Rational::fromDouble(0.0, q));
    CHECK_EQ_STR(q.toString(), "0");
    double v = 1.0 / 3.0;
    CHECK(Rational::fromDouble(v, q));
    CHECK_CLOSE(q.toDouble(), v, 1e-15);
}

TEST(rational_todouble) {
    CHECK_CLOSE(mk(1, 3).toDouble(), 0.3333333333333333, 1e-15);
    CHECK_CLOSE(D("1", "3401").toDouble(), 1.3401, 1e-12);
    // large: 10^20/3 must not overflow and must stay precise enough
    std::string err;
    BigInt ten20 = BigInt::pow(BigInt::fromInt64(10), 20);
    Rational big;
    Rational::make(ten20, BigInt::fromInt64(3), big, err);
    CHECK_CLOSE(big.toDouble(), 1e20 / 3.0, 1e4);
    CHECK_EQ_STR(big.toString().substr(0, 4), "1000");
}

TEST(rational_sqrt_exact) {
    Rational out;
    CHECK(Rational::sqrtExact(mk(4, 1), out));
    CHECK_EQ_STR(out.toString(), "2");
    CHECK(Rational::sqrtExact(mk(1, 4), out));
    CHECK_EQ_STR(out.toString(), "1/2");
    CHECK(Rational::sqrtExact(mk(9, 16), out));
    CHECK_EQ_STR(out.toString(), "3/4");
    CHECK(!Rational::sqrtExact(mk(2, 1), out));
    CHECK(!Rational::sqrtExact(mk(-4, 1), out));
}

TEST(rational_division_by_zero) {
    Rational out;
    std::string err;
    CHECK(!Rational::div(mk(1, 2), Rational::zero(), out, err));
    CHECK_EQ_STR(err, "Division durch Null");
}