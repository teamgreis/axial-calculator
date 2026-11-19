// Axial Math Core — tokenizer implementation
#include "axial/tokenizer.hpp"

#include <cctype>

namespace axial {

namespace {

bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Maps a UTF-8 sequence to a normalized operator/prefix when known.
// Returns bytes consumed (0 = not a special sequence).
int mapUnicode(const std::string& s, size_t i, std::string& out,
               bool& isFunction) {
    isFunction = false;
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) return 0;
    // π (U+03C0): CF 80
    if (c == 0xCF && i + 1 < s.size() && (unsigned char)s[i + 1] == 0x80) {
        out = "pi"; return 2;
    }
    // τ (U+03C4): CF 84
    if (c == 0xCF && i + 1 < s.size() && (unsigned char)s[i + 1] == 0x84) {
        out = "tau"; return 2;
    }
    // √ (U+221A): E2 88 9A
    if (c == 0xE2 && i + 2 < s.size() &&
        (unsigned char)s[i + 1] == 0x88 && (unsigned char)s[i + 2] == 0x9A) {
        out = "sqrt"; isFunction = true; return 3;
    }
    // ∛ (U+221B): E2 88 9B
    if (c == 0xE2 && i + 2 < s.size() &&
        (unsigned char)s[i + 1] == 0x88 && (unsigned char)s[i + 2] == 0x9B) {
        out = "cbrt"; isFunction = true; return 3;
    }
    // · (U+00B7): C2 B7 | × (U+00D7): C3 97
    if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB7) {
        out = "*"; return 2;
    }
    if (c == 0xC3 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0x97) {
        out = "*"; return 2;
    }
    // − (U+2212): E2 88 92
    if (c == 0xE2 && i + 2 < s.size() &&
        (unsigned char)s[i + 1] == 0x88 && (unsigned char)s[i + 2] == 0x92) {
        out = "-"; return 3;
    }
    // ÷ (U+00F7): C3 B7
    if (c == 0xC3 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB7) {
        out = "/"; return 2;
    }
    // ² (U+00B2): C2 B2 | ³ (U+00B3): C2 B3
    if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB2) {
        out = "^2"; return 2;
    }
    if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB3) {
        out = "^3"; return 2;
    }
    return 0;
}

}  // namespace

bool Tokenizer::run(const std::string& src, std::vector<Token>& out,
                    AxialError& err) {
    out.clear();
    size_t i = 0;
    while (i < src.size()) {
        char c = src[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }

        if ((unsigned char)c >= 0x80) {
            std::string mapped;
            bool isFn = false;
            int n = mapUnicode(src, i, mapped, isFn);
            if (n == 0) {
                err.kind = ErrorKind::UnknownToken;
                err.pos = (int)i;
                err.token = std::string(1, c);
                err.message = "Unbekanntes Zeichen: " + err.token;
                err.hint = "Erlaubt sind Ziffern, Buchstaben, + - * / ^ % ! ( ) , und Operatoren.";
                return false;
            }
            Token t;
            t.pos = (int)i;
            if (mapped == "*" || mapped == "-" || mapped == "/" ||
                mapped == "^2" || mapped == "^3") {
                t.kind = Tok::Op;
            } else {
                t.kind = Tok::Ident;
            }
            t.text = mapped;
            out.push_back(std::move(t));
            i += n;
            continue;
        }

        if (isDigit(c) || c == '.') {
            size_t j = i;
            int dots = 0;
            while (j < src.size() && (isDigit(src[j]) || src[j] == '.')) {
                if (src[j] == '.') ++dots;
                ++j;
            }
            if (j < src.size() && (src[j] == 'e' || src[j] == 'E')) {
                size_t k = j + 1;
                if (k < src.size() && (src[k] == '+' || src[k] == '-')) ++k;
                if (k < src.size() && isDigit(src[k])) {
                    while (k < src.size() && isDigit(src[k])) ++k;
                    j = k;
                }
            }
            std::string raw = src.substr(i, j - i);
            if (dots > 1 || raw == "." || raw.empty()) {
                err.kind = ErrorKind::UnknownToken;
                err.pos = (int)i;
                err.token = raw;
                err.message = "Ungültige Zahl: " + raw;
                return false;
            }
            Token t;
            t.kind = Tok::Number;
            t.text = raw;
            t.pos = (int)i;
            out.push_back(std::move(t));
            i = j;
            continue;
        }

        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t j = i;
            while (j < src.size() &&
                   (std::isalnum((unsigned char)src[j]) || src[j] == '_')) {
                ++j;
            }
            Token t;
            t.kind = Tok::Ident;
            t.pos = (int)i;
            t.text = src.substr(i, j - i);
            for (auto& ch : t.text) ch = (char)std::tolower((unsigned char)ch);
            out.push_back(std::move(t));
            i = j;
            continue;
        }

        if (c == '(') { Token t{Tok::LParen, "(", (int)i}; out.push_back(t); ++i; continue; }
        if (c == ')') { Token t{Tok::RParen, ")", (int)i}; out.push_back(t); ++i; continue; }
        if (c == ',') { Token t{Tok::Comma, ",", (int)i}; out.push_back(t); ++i; continue; }

        if (std::string("+-*/^%!").find(c) != std::string::npos) {
            Token t;
            t.kind = Tok::Op;
            t.text = std::string(1, c);
            t.pos = (int)i;
            out.push_back(std::move(t));
            ++i;
            continue;
        }

        err.kind = ErrorKind::UnknownToken;
        err.pos = (int)i;
        err.token = std::string(1, c);
        err.message = "Unbekanntes Zeichen: " + err.token;
        err.hint = "Erlaubt sind Ziffern, Buchstaben, + - * / ^ % ! ( ) , und Operatoren.";
        return false;
    }
    Token end;
    end.kind = Tok::End;
    end.text = "";
    end.pos = (int)src.size();
    out.push_back(end);
    return true;
}

}  // namespace axial