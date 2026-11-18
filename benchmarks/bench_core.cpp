// Axial Math Core — throughput benchmark
// Measures two modes per expression:
//   eval      = tokenize + parse + compile + run  (single shot)
//   compiled  = run an already compiled program   (the fast path)
// CI prints these lines into the workflow log; docs/BENCHMARKS.md documents
// the ELIAN baseline the 400x claim is measured against.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

#include "axial/engine.hpp"

using namespace axial;

namespace {

using Clock = std::chrono::steady_clock;

double nsPerOpEval(const char* src, int iters) {
    auto t0 = Clock::now();
    volatile double sink = 0.0;
    for (int i = 0; i < iters; ++i) {
        EvalOptions o;
        o.angle = AngleMode::Deg;
        EvalResult r = engineEval(src, o);
        sink += r.ok ? r.value.toDouble() : 0.0;
    }
    auto t1 = Clock::now();
    (void)sink;
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / iters;
}

double nsPerOpCompiled(const char* src, int iters, const EvalOptions& opts) {
    ParseResult pr = Parser::parse(src);
    if (!pr.ok()) return -1.0;
    Program prog;
    AxialError err;
    if (!Compiler::compile(*pr.root, prog, err)) return -1.0;
    Environment env;
    env.angle = opts.angle;
    env.vars = opts.vars;
    auto t0 = Clock::now();
    volatile double sink = 0.0;
    Value out;
    for (int i = 0; i < iters; ++i) {
        AxialError e;
        vmRun(prog, env, out, e);
        sink += e.ok() ? out.toDouble() : 0.0;
    }
    auto t1 = Clock::now();
    (void)sink;
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / iters;
}

}  // namespace

int main() {
    struct Case {
        const char* name;
        const char* src;
        bool usesVar;
    };
    const Case cases[] = {
        {"arithmetic", "2+3*4", false},
        {"rational", "(1/3+1/6)*840", false},
        {"trig", "sin(30)^2+cos(60)^2", false},
        {"radical", "sqrt(2)*sqrt(3)+ln(e)", false},
        {"variable", "(x+1)*(x+2)", true},
    };

    EvalOptions base;
    base.angle = AngleMode::Deg;
    EvalOptions withVar = base;
    {
        Value v;
        Rational::fromDouble(2.0, v.q);
        v.kind = ValueKind::Rational;
        withVar.vars.emplace_back("x", v);
    }

    std::printf("benchmark,unit=ns_per_op\n");
    for (auto& c : cases) {
        double ns1 = nsPerOpEval(c.src, 200000);
        double ns2 = nsPerOpCompiled(c.src, 2000000,
                                     c.usesVar ? withVar : base);
        std::printf("eval,%s,%.1f\n", c.name, ns1);
        std::printf("compiled,%s,%.1f\n", c.name, ns2);
        double sp = (ns2 > 0.0) ? ns1 / ns2 : 0.0;
        std::printf("speedup,%s,%.1fx\n", c.name, sp);
    }
    return 0;
}
