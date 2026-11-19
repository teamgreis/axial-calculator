// Axial Math Core — evaluation tests: ELIAN-parity + Axial extensions
#include "testkit.hpp"

#include "axial/engine.hpp"

using namespace axial;

namespace {

EvalResult E(const char* src, AngleMode angle = AngleMode::Deg) {
    EvalOptions o;
    o.angle = angle;
    return engineEval(src, o);
}

EvalResult EV(const char* src,
              std::initializer_list<std::pair<const char*, double>> vars) {
    EvalOptions o;
    for (auto& kv : vars) {
        Value v;
        Rational::fromDouble(kv.second, v.q);
        v.kind = ValueKind::Rational;
        o.vars.emplace_back(kv.first, v);
    }
    return engineEval(src, o);
}

}  // namespace

// ---- exakte rationale Arithmetik (ELIAN-Parität) ----

TEST(golden_rational_exactness) {
    CHECK_EQ_STR(E("1/3+1/6").text, "1/2");
    CHECK_EQ_STR(E("2/4").text, "1/2");
    CHECK_EQ_STR(E("(1/2)^3").text, "1/8");
    CHECK_EQ_STR(E("2^-2").text, "1/4");
    CHECK_EQ_STR(E("1/7*7").text, "1");
    CHECK_EQ_STR(E("0.1+0.2").text, "3/10");
    CHECK(E("1/3+1/6").exact);
    CHECK_EQ_STR(E("999999999999*999999999999").text,
                 "999999999998000000000001");
    CHECK_EQ_STR(E("(1/3)^2").text, "1/9");
    CHECK_EQ_STR(E("50%").text, "1/2");
    CHECK_EQ_STR(E("5%").text, "1/20");
    CHECK_EQ_STR(E("3.5").text, "7/2");
    CHECK_EQ_STR(E("-0.5").text, "-1/2");
    CHECK_EQ_STR(E("200*15%").text, "30");
}

TEST(golden_precedence_and_implicit_multiplication) {
    CHECK_EQ_STR(E("2+3*4").text, "14");
    CHECK_EQ_STR(E("2*3+4*5").text, "26");
    CHECK_EQ_STR(E("(2+3)*4").text, "20");
    CHECK_EQ_STR(E("-3+5").text, "2");
    CHECK_EQ_STR(E("-(2+3)").text, "-5");
    CHECK_EQ_STR(E("2^3^2").text, "512");
    CHECK_EQ_STR(E("2(3+4)").text, "14");
    CHECK_EQ_STR(E("3(2)(3)").text, "18");
    CHECK_EQ_STR(E("(1+1)(2+3)").text, "10");
    CHECK_EQ_STR(E("2 3").text, "6");
}

TEST(golden_factorial) {
    CHECK_EQ_STR(E("3!").text, "6");
    CHECK_EQ_STR(E("0!").text, "1");
    CHECK_EQ_STR(E("5!").text, "120");
    CHECK_EQ_STR(E("20!").text, "2432902008176640000");
    CHECK(!E("(-2)!").ok);
    CHECK(!E("10001!").ok);
    CHECK(E("10000!").ok);
    CHECK(E("10000!").text.size() > 30000);
}

// ---- Variablen, ans, Winkelmodus ----

TEST(golden_variables_and_ans) {
    CHECK_EQ_STR(EV("x*2", {{"x", 3}}).text, "6");
    CHECK(!E("x").ok);
    CHECK(E("x").err.kind == ErrorKind::UnknownVariable);
    CHECK_EQ_STR(EV("ans+1", {{"ans", 5}}).text, "6");
}

TEST(golden_angle_modes) {
    CHECK_EQ_STR(E("sin(30)").text, "0.5");
    CHECK_EQ_STR(E("cos(60)").text, "0.5");
    CHECK_EQ_STR(E("asin(0.5)").text, "30");
    CHECK_CLOSE(E("sin(30)", AngleMode::Rad).value.toDouble(),
                std::sin(30.0), 1e-12);
    CHECK_CLOSE(E("sin(pi/2)", AngleMode::Rad).value.toDouble(), 1.0, 1e-12);
    CHECK_CLOSE(E("sin(100)", AngleMode::Gon).value.toDouble(), 1.0, 1e-12);
}

// ---- Funktionen & Formatierung ----

TEST(golden_functions_and_formatting) {
    CHECK_EQ_STR(E("sqrt(4)").text, "2");
    CHECK(E("sqrt(4)").exact);
    CHECK_EQ_STR(E("sqrt(1/4)").text, "1/2");
    CHECK_EQ_STR(E("sqrt(2)").text, "1.41421356237");
    CHECK_EQ_STR(E("log(100)").text, "2");
    CHECK_EQ_STR(E("ln(e)").text, "1");
    CHECK_EQ_STR(E("ln(e^2)").text, "2");
    CHECK_EQ_STR(E("exp(0)").text, "1");
    CHECK_EQ_STR(E("abs(-7)").text, "7");
    CHECK_EQ_STR(E("floor(3.7)").text, "3");
    CHECK_EQ_STR(E("floor(-3.7)").text, "-4");
    CHECK_EQ_STR(E("ceil(3.2)").text, "4");
    CHECK_EQ_STR(E("round(2.5)").text, "3");
    CHECK_EQ_STR(E("round(-2.5)").text, "-3");
    CHECK_EQ_STR(E("round(2.4)").text, "2");
    CHECK_EQ_STR(E("sign(-3)").text, "-1");
    CHECK_EQ_STR(E("gcd(12,18)").text, "6");
    CHECK_EQ_STR(E("lcm(4,6)").text, "12");
    CHECK_EQ_STR(E("ncr(49,6)").text, "13983816");
    CHECK_EQ_STR(E("npr(5,3)").text, "60");
    CHECK_EQ_STR(E("min(3,1,2)").text, "1");
    CHECK_EQ_STR(E("max(3,1,2)").text, "3");
    CHECK_EQ_STR(E("sum(1,2,3)").text, "6");
    CHECK_EQ_STR(E("mean(1,2,3,4)").text, "5/2");
}

TEST(golden_display_numbers) {
    CHECK_EQ_STR(E("pi^0").text, "1");
    CHECK(E("pi^0").exact);
    CHECK(!E("sin(30)^2").exact);
    CHECK_CLOSE(E("sin(30)^2").value.toDouble(), 0.25, 1e-12);
    CHECK_EQ_STR(E("10^20").text, "100000000000000000000");
    CHECK(E("10^20").exact);
    CHECK_EQ_STR(E("sqrt(3)*sqrt(3)").text, "3");
}

// ---- Axial-Verbesserungen gegenüber ELIAN ----

TEST(axial_improvements) {
    // wissenschaftliche Notation exakt (ELIAN: Fehler)
    CHECK_EQ_STR(E("1e5").text, "100000");
    CHECK(E("1e5").exact);
    CHECK_EQ_STR(E("2.5e-3").text, "1/400");
    // √-Präfix und Hochzeichen (ELIAN: Fehler)
    CHECK_EQ_STR(E("√9").text, "3");
    CHECK(E("√9").exact);
    CHECK_EQ_STR(E("√(16)").text, "4");
    CHECK_EQ_STR(E("4√2").text, "5.65685424949");
    CHECK_EQ_STR(E("3²").text, "9");
    CHECK_EQ_STR(E("2³+1").text, "9");
    // Konstanten werden numerisch ehrlich dargestellt (ELIAN: Riesenbruch)
    CHECK_EQ_STR(E("pi").text, "3.14159265359");
    CHECK(!E("pi").exact);
    CHECK_EQ_STR(E("2π").text, "6.28318530718");
    // sqrt negativer Quadratzahl exakt imaginär (ELIAN: ∞)
    CHECK(E("sqrt(-4)").ok);
    CHECK(E("sqrt(-4)").value.isComplex());
    CHECK_EQ_STR(E("sqrt(-4)").text, "2i");
    // automatische Klammerergänzung (ELIAN: Fehler)
    CHECK(E("(2+3").ok);
    CHECK(E("(2+3").text == "5");
    CHECK(E("(2+3").parensFixed == 1);
    CHECK_EQ_STR(E("sin(30").text, "0.5");
}

TEST(axial_fuzz_no_crash) {
    // beliebige Gift-Eingaben: nur typisierte Fehler, niemals ein Absturz
    const char* junk[] = {
        "", " ", "   ", "(", ")", "(((", ")))", "1+", "+1", "*1", "1*", "/1",
        "1/", "^1", "1^", "!!", "%%", "sin", "sin()", "foo(3)", "1.2.3",
        "1..2", ".", "..", "§", "1 § 2", "1\n2", "1,,2", "sin(1,2)",
        "ln(-1)", "asin(2)", "log(1)", "1/0", "0/0", "0^-1", "(-2)!",
        "10001!", "πππ", "√", "x", "ans", "1e", "1e+", "1ee1",
        "2^999999", "1/0+1/0", "((1+2)*(3-4))^(1/2)",
    };
    for (const char* j : junk) {
        EvalResult r = E(j);
        // Ergebnis muss wahlweise ok (mit Text) oder ein typisierter Fehler sein
        if (r.ok) {
            CHECK(!r.text.empty());
        } else {
            CHECK(r.err.kind != ErrorKind::None);
            CHECK(!r.err.message.empty());
        }
    }
}