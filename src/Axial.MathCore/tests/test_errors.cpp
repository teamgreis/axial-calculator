// Axial Math Core — error-model tests: typed errors, positions, hints
#include "testkit.hpp"

#include "axial/engine.hpp"

using namespace axial;

namespace {

EvalResult E(const char* src) {
    EvalOptions o;
    return engineEval(src, o);
}

}  // namespace

TEST(error_empty_input) {
    EvalResult r = E("");
    CHECK(!r.ok);
    CHECK(r.err.kind == ErrorKind::Syntax);
    CHECK(r.err.message.find("Leer") != std::string::npos);
}

TEST(error_unknown_token) {
    EvalResult r = E("2 § 3");
    CHECK(!r.ok);
    CHECK(r.err.kind == ErrorKind::UnknownToken);
    CHECK(r.err.pos == 2);
    CHECK(r.err.message.find("Unbekanntes Zeichen") != std::string::npos);
    CHECK(!r.err.hint.empty());
}

TEST(error_division_by_zero) {
    EvalResult r = E("5/0");
    CHECK(!r.ok);
    CHECK(r.err.kind == ErrorKind::DivideByZero);
    CHECK(r.err.pos >= 0);
    CHECK(!r.err.hint.empty());
    CHECK(!E("1/0.0").ok);
}

TEST(error_unknown_variable) {
    EvalResult r = E("x");
    CHECK(!r.ok);
    CHECK(r.err.kind == ErrorKind::UnknownVariable);
    CHECK(r.err.message.find("x") != std::string::npos);
    CHECK(!r.err.hint.empty());
}

TEST(error_factorial_domain) {
    EvalResult r = E("(-2)!");
    CHECK(!r.ok);
    CHECK(r.err.kind == ErrorKind::Domain);
    CHECK(r.err.message.find("Fakult") != std::string::npos);
    CHECK(!r.err.hint.empty());
}

TEST(error_factorial_limit) {
    EvalResult r = E("10001!");
    CHECK(!r.ok);
    CHECK(r.err.kind == ErrorKind::Limit);
}

TEST(error_trailing_symbol) {
    EvalResult r = E("1+2)");
    CHECK(!r.ok);
    CHECK(r.err.kind == ErrorKind::Syntax);
    CHECK(!E("2 §").ok);
}

TEST(error_function_domain) {
    CHECK(!E("ln(-1)").ok);
    CHECK(E("ln(-1)").err.kind == ErrorKind::Domain);
    CHECK(!E("asin(2)").ok);
    CHECK(E("asin(2)").err.kind == ErrorKind::Domain);
    CHECK(!E("log(-5, 2)").ok);
}

TEST(error_arity) {
    CHECK(!E("sin(1,2)").ok);
    CHECK(E("sin(1,2)").err.kind == ErrorKind::Domain);
    CHECK(!E("min()").ok);
    CHECK(E("min()").err.kind == ErrorKind::Domain);
    CHECK(!E("mean()").ok);
}

TEST(error_pow_negative_base_fractional) {
    CHECK(!E("(-3)^(1/2)").ok);
    CHECK(E("(-3)^(1/2)").err.kind == ErrorKind::Domain);
}

TEST(error_position_reported) {
    EvalResult r = E("2+§4");
    CHECK(!r.ok);
    CHECK(r.err.pos == 2);
}

TEST(errors_typed_on_fuzz) {
    const char* more[] = {"1+2*3)", "((1)", "1..2", "sin(", "1e999999",
                          "%", "!", "^", "()", "1(2)(3", "x+", "2**3",
                          "ncr(-1, 2)", "ncr(2, 5)", "log()", "1e", "1e+"};
    for (const char* m : more) {
        EvalResult r = E(m);
        if (r.ok) {
            CHECK(!r.text.empty());
        } else {
            CHECK(r.err.kind != ErrorKind::None);
            CHECK(!r.err.message.empty());
        }
    }
}
