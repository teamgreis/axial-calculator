// Axial Math Core — tokenizer
#pragma once

#include <string>
#include <vector>

#include "axial/errors.hpp"

namespace axial {

enum class Tok { Number, Ident, Op, LParen, RParen, Comma, End };

struct Token {
    Tok kind = Tok::End;
    std::string text;      // raw text (Num) / normalized symbol (Op, Ident)
    int pos = -1;          // character position in source
};

class Tokenizer {
public:
    // Splits the source into tokens. Unicode operators are normalized:
    //   π→pi  τ→tau  ·/×→*  −→-  ÷→/  √→sqrt(func)  ∛→cbrt(func)  ²→^2  ³→^3
    // Numbers support decimal and scientific notation (1.5e-3).
    static bool run(const std::string& src, std::vector<Token>& out,
                    AxialError& err);
};

}  // namespace axial