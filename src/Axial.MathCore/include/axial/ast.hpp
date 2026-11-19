// Axial Math Core — abstract syntax tree
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "axial/rational.hpp"

namespace axial {

enum class NodeKind {
    Num,      // literal (exact rational)
    Const,    // transcendental constant (pi, e, tau, phi) — inexact
    Var,      // named variable (resolved at evaluation)
    Neg,
    Add, Sub, Mul, Div, Pow,
    Fact,     // n!
    Pct,      // n%
    Call      // function call: name, kids = arguments
};

struct Node {
    NodeKind kind = NodeKind::Num;
    int pos = -1;                 // source position (for error reports)
    Rational num;                 // when kind == Num
    std::string name;             // when kind == Const / Var / Call
    std::vector<struct Node*> kids;
};

using NodePtr = Node*;

// Normalized function names of the core library.
bool isCoreFunction(const std::string& name);
int  functionArity(const std::string& name);   // -1 = variadic
bool isCoreConstant(const std::string& name);

}  // namespace axial