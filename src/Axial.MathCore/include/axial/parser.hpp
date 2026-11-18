// Axial Math Core — precedence parser
#pragma once

#include <string>

#include "axial/ast.hpp"
#include "axial/errors.hpp"

namespace axial {

struct ParseResult {
    NodePtr root;
    AxialError err;
    int parensFixed = 0;    // number of automatically closed parentheses
    bool ok() const { return err.ok(); }
};

class Parser {
public:
    // Grammar (ELIAN-compatible, with Axial improvements):
    //   expr    = term (('+'|'-') term)*
    //   term    = unary (('*'|'/') unary)*
    //   unary   = ('-'|'+') unary | power
    //   power   = implicit ('^' unary)?            [right associative]
    //   implicit= postfix ((factor)* -> multiplication)
    //   postfix = primary (('!'|'%'|'^2'|'^3'))*
    //   primary = number | const | var | funcall | '(' expr ')'
    // Functions accept 'sin(30)' and 'sin 30' forms. Unclosed parentheses
    // are closed automatically (reported via parensFixed).
    static ParseResult parse(const std::string& src);
};

}  // namespace axial