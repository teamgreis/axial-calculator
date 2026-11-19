// Axial Math Core — bytecode compiler and register/stack VM
#pragma once

#include <string>
#include <vector>

#include "axial/ast.hpp"
#include "axial/errors.hpp"
#include "axial/value.hpp"

namespace axial {

enum class AngleMode { Deg, Rad, Gon };

enum class Op : uint8_t {
    PushQ,     // arg: index into Program::qpool  (exact fraction)
    PushR,     // arg: index into Program::rpool  (inexact double)
    PushI,     // arg: index into Program::rpool  (exact integer double)
    Add, Sub, Mul, Div, Pow, Neg, Fact, Pct,
    Call,      // arg: index into Program::calls
    LoadVar,   // arg: index into Program::vars
    Halt
};

struct Instr {
    Op op = Op::Halt;
    int32_t arg = 0;    // pool/table index
    int32_t arg2 = 0;   // secondary index (e.g. call argument count)
    int32_t pos = -1;   // source position for runtime errors
};

struct Program {
    std::vector<Instr> code;
    std::vector<Rational> qpool;
    std::vector<double> rpool;
    std::vector<std::string> calls;
    std::vector<std::string> vars;
    bool hasVariables = false;
};

class Compiler {
public:
    // Compiles the AST to bytecode with constant folding.
    static bool compile(const Node& root, Program& out, AxialError& err);
};

struct Environment {
    AngleMode angle = AngleMode::Deg;
    std::vector<std::pair<std::string, Value>> vars;
    Value* ans = nullptr;   // 'ans' reads this slot when set

    const Value* find(const std::string& name) const {
        for (const auto& kv : vars) {
            if (kv.first == name) return &kv.second;
        }
        return nullptr;
    }
};

// Runs a compiled program. Filled errors carry position, kind and hint.
bool vmRun(const Program& p, const Environment& env, Value& out, AxialError& err);

}  // namespace axial