// Axial Math Core — minimal test framework (header only)
// Checks count globally; every test is a free function registered via TEST.
#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace axtest {

struct Case {
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}

inline int& failures() {
    static int f = 0;
    return f;
}

inline int& checks() {
    static int c = 0;
    return c;
}

struct Reg {
    Reg(const char* name, void (*fn)()) {
        registry().push_back({name, fn});
    }
};

inline void fail(const char* file, int line, const std::string& expr) {
    ++failures();
    std::printf("  FEHLER %s:%d — %s\n", file, line, expr.c_str());
}

inline void check(bool cond, const char* file, int line, const char* expr) {
    ++checks();
    if (!cond) fail(file, line, expr);
}

inline void checkEqStr(const std::string& got, const std::string& want,
                       const char* file, int line, const char* expr,
                       const std::string& wantExpr) {
    ++checks();
    if (got != want) {
        ++failures();
        std::printf("  FEHLER %s:%d — %s\n    erwartet: %s\n    erhalten: %s\n",
                    file, line, expr, wantExpr.c_str(), got.c_str());
    }
}

inline void checkClose(double got, double want, double eps, const char* file,
                       int line, const char* expr, double wantV) {
    ++checks();
    if (!std::isfinite(got) || std::fabs(got - want) > eps) {
        ++failures();
        std::printf("  FEHLER %s:%d — %s\n    erwartet: ~%.17g\n    erhalten: %.17g\n",
                    file, line, expr, wantV, got);
    }
}

}  // namespace axtest

#define AX_CONCAT2(a, b) a##b
#define AX_CONCAT(a, b) AX_CONCAT2(a, b)

#define TEST(name)                                                       \
    static void name();                                                  \
    static axtest::Reg AX_CONCAT(reg_, __LINE__)(#name, name);           \
    static void name()

#define CHECK(cond) \
    axtest::check((cond) ? true : false, __FILE__, __LINE__, #cond)

#define CHECK_EQ_STR(got, want) \
    axtest::checkEqStr((got), (want), __FILE__, __LINE__, #got " == " #want, (want))

#define CHECK_CLOSE(got, want, eps) \
    axtest::checkClose((got), (want), (eps), __FILE__, __LINE__, #got " ~ " #want, (want))

#define RUN_TESTS()                                                       \
    int main() {                                                          \
        int f = 0;                                                        \
        for (auto& c : axtest::registry()) {                              \
            int before = axtest::failures();                              \
            c.fn();                                                       \
            int delta = axtest::failures() - before;                      \
            if (delta == 0) {                                             \
                std::printf("  ok    %s\n", c.name);                      \
            } else {                                                      \
                std::printf("  FAIL  %s (%d)\n", c.name, delta);          \
                f += delta;                                               \
            }                                                             \
            std::fflush(stdout);                                          \
        }                                                                 \
        std::printf("\n%d Prüfungen, %d Fehler\n",                 \
                    axtest::checks(), axtest::failures());                \
        return f == 0 ? 0 : 1;                                            \
    }