// Axial Math Core — parser implementation
#include "axial/parser.hpp"

#include <cstdlib>

#include "axial/bigint.hpp"
#include "axial/tokenizer.hpp"

namespace axial {

namespace {

const char* kConstants[] = {"pi", "e", "tau", "phi"};

struct P {
    const std::vector<Token>& tk;
    size_t pos = 0;
    int fixed = 0;
    AxialError err;

    explicit P(const std::vector<Token>& t) : tk(t) {}

    const Token& peek() const { return tk[pos]; }
    const Token& advance() { return tk[pos++]; }
    bool atEnd() const { return tk[pos].kind == Tok::End; }

    bool isOp(const char* v) const {
        return tk[pos].kind == Tok::Op && tk[pos].text == v;
    }
    bool eatOp(const char* v) {
        if (isOp(v)) { ++pos; return true; }
        return false;
    }

    void fail(ErrorKind kind, const Token& t, const std::string& msg,
              const std::string& hint = "") {
        if (err.ok()) {
            err.kind = kind;
            err.pos = t.pos;
            err.token = t.text;
            err.message = msg;
            err.hint = hint;
        }
    }

    NodePtr make(NodeKind k, int pos) {
        auto n = makeNode(k, pos);
        return n;
    }

    // exact conversion of a decimal/scientific literal:
    //   value = (whole+frac) * 10^(e - frac.size())
    NodePtr numberNode(const Token& t) {
        auto n = make(NodeKind::Num, t.pos);
        std::string raw = t.text;
        size_t epos = raw.find_first_of("eE");
        std::string mant = raw;
        long long e = 0;
        if (epos != std::string::npos) {
            mant = raw.substr(0, epos);
            e = std::strtoll(raw.c_str() + epos + 1, nullptr, 10);
        }
        std::string whole = mant, frac;
        size_t dot = mant.find('.');
        if (dot != std::string::npos) {
            whole = mant.substr(0, dot);
            frac = mant.substr(dot + 1);
        }
        if (whole.empty()) whole = "0";
        long long scale = e - (long long)frac.size();
        if (scale > 100000 || scale < -100000) {
            fail(ErrorKind::Limit, t, "Zahl zu groß oder zu klein für exakte Darstellung: " + raw,
                 "Exponent verkleinern.");
            return nullptr;
        }
        // value = X * 10^e / 10^frac  — mantissa first, exponent after
        Rational q;
        bool ok = Rational::fromDecimalDigits(whole, frac, q);
        if (ok && e != 0) {
            std::string msg;
            BigInt pw = BigInt::pow(BigInt::fromInt64(10),
                                    (uint64_t)(e > 0 ? e : -e));
            Rational factor;
            Rational::make(pw, BigInt::fromInt64(1), factor, msg);
            if (e > 0) {
                q = q.mul(factor);
            } else {
                ok = Rational::div(q, factor, q, msg);
            }
        }
        if (!ok) {
            fail(ErrorKind::Internal, t, "Zahl nicht interpretierbar: " + raw);
            return nullptr;
        }
        n->num = q;
        return n;
    }

    NodePtr primary() {
        const Token t = advance();
        switch (t.kind) {
            case Tok::Number:
                return numberNode(t);
            case Tok::Ident: {
                std::string name = t.text;
                if (isCoreConstant(name)) {
                    return make(NodeKind::Const, t.pos);
                }
                if (isCoreFunction(name)) {
                    auto call = make(NodeKind::Call, t.pos);
                    call->name = name;
                    if (peek().kind == Tok::LParen) {
                        ++pos;
                        if (peek().kind == Tok::RParen) {
                            ++pos;
                        } else {
                            call->kids.push_back(expr());
                            while (peek().kind == Tok::Comma) {
                                ++pos;
                                call->kids.push_back(expr());
                            }
                            if (peek().kind == Tok::RParen) {
                                ++pos;
                            } else {
                                fail(ErrorKind::Syntax, peek(),
                                     "Schließende \")\" fehlt",
                                     "Klammer schließen oder Ausdruck kürzen.");
                                return nullptr;
                            }
                        }
                    } else {
                        // function without parentheses: sin 30, √9, 2√2
                        NodePtr arg = unary();
                        if (!arg) return nullptr;
                        call->kids.push_back(arg);
                    }
                    return call;
                }
                // variable: resolved at evaluation time
                auto v = make(NodeKind::Var, t.pos);
                v->name = name;
                return v;
            }
            case Tok::LParen: {
                NodePtr e = expr();
                if (!e) return nullptr;
                if (peek().kind == Tok::RParen) {
                    ++pos;
                } else {
                    // unclosed parenthesis: auto-fix
                    ++fixed;
                }
                return e;
            }
            case Tok::Op:
                if (t.text == "-") {
                    NodePtr a = primary();
                    if (!a) return nullptr;
                    auto n = make(NodeKind::Neg, t.pos);
                    n->kids.push_back(a);
                    return n;
                }
                if (t.text == "+") return primary();
                fail(ErrorKind::Syntax, t, "Unerwartetes Symbol: " + t.text);
                return nullptr;
            case Tok::RParen:
                fail(ErrorKind::Syntax, t, "Unerwartetes Symbol: )",
                     "Zu viele schließende Klammern.");
                return nullptr;
            case Tok::End:
                fail(ErrorKind::Syntax, t, "Unerwartetes Ende des Ausdrucks",
                     "Ausdruck vervollständigen.");
                return nullptr;
            case Tok::Comma:
                fail(ErrorKind::Syntax, t, "Unerwartetes Komma");
                return nullptr;
        }
        return nullptr;
    }

    NodePtr postfix() {
        NodePtr e = primary();
        if (!e) return nullptr;
        for (;;) {
            const Token& k = peek();
            if (k.kind == Tok::Op && k.text == "!") {
                ++pos;
                auto n = make(NodeKind::Fact, k.pos);
                n->kids.push_back(e);
                e = n;
                continue;
            }
            if (k.kind == Tok::Op && k.text == "%") {
                ++pos;
                auto n = make(NodeKind::Pct, k.pos);
                n->kids.push_back(e);
                e = n;
                continue;
            }
            if (k.kind == Tok::Op && (k.text == "^2" || k.text == "^3")) {
                ++pos;
                auto n = make(NodeKind::Pow, k.pos);
                auto lit = make(NodeKind::Num, k.pos);
                lit->num = Rational::fromInt64(k.text == "^2" ? 2 : 3);
                n->kids.push_back(e);
                n->kids.push_back(lit);
                e = n;
                continue;
            }
            break;
        }
        return e;
    }

    bool startsFactor() const {
        const Token& k = peek();
        if (k.kind == Tok::Number || k.kind == Tok::LParen || k.kind == Tok::Ident)
            return true;
        return false;
    }

    NodePtr implicit() {
        NodePtr e = postfix();
        if (!e) return nullptr;
        for (;;) {
            if (!startsFactor()) break;
            size_t before = pos;
            NodePtr r = postfix();
            if (pos <= before) break;
            if (!r) return nullptr;
            auto n = make(NodeKind::Mul, e->pos);
            n->kids.push_back(e);
            n->kids.push_back(r);
            e = n;
        }
        return e;
    }

    NodePtr power() {
        NodePtr b = implicit();
        if (!b) return nullptr;
        if (isOp("^")) {
            const Token caret = advance();
            NodePtr e = unary();
            if (!e) return nullptr;
            auto n = make(NodeKind::Pow, caret.pos);
            n->kids.push_back(b);
            n->kids.push_back(e);
            return n;
        }
        return b;
    }

    NodePtr unary() {
        const Token& k = peek();
        if (k.kind == Tok::Op && k.text == "-") {
            ++pos;
            NodePtr a = unary();
            if (!a) return nullptr;
            auto n = make(NodeKind::Neg, k.pos);
            n->kids.push_back(a);
            return n;
        }
        if (k.kind == Tok::Op && k.text == "+") {
            ++pos;
            return unary();
        }
        return power();
    }

    NodePtr term() {
        NodePtr l = unary();
        if (!l) return nullptr;
        for (;;) {
            if (isOp("*")) {
                const Token op = advance();
                NodePtr r = unary();
                if (!r) return nullptr;
                auto n = make(NodeKind::Mul, op.pos);
                n->kids.push_back(l);
                n->kids.push_back(r);
                l = n;
            } else if (isOp("/")) {
                const Token op = advance();
                NodePtr r = unary();
                if (!r) return nullptr;
                auto n = make(NodeKind::Div, op.pos);
                n->kids.push_back(l);
                n->kids.push_back(r);
                l = n;
            } else {
                break;
            }
        }
        return l;
    }

    NodePtr expr() {
        NodePtr l = term();
        if (!l) return nullptr;
        for (;;) {
            if (isOp("+")) {
                const Token op = advance();
                NodePtr r = term();
                if (!r) return nullptr;
                auto n = make(NodeKind::Add, op.pos);
                n->kids.push_back(l);
                n->kids.push_back(r);
                l = n;
            } else if (isOp("-")) {
                const Token op = advance();
                NodePtr r = term();
                if (!r) return nullptr;
                auto n = make(NodeKind::Sub, op.pos);
                n->kids.push_back(l);
                n->kids.push_back(r);
                l = n;
            } else {
                break;
            }
        }
        return l;
    }
};

}  // namespace

bool isCoreConstant(const std::string& name) {
    for (const char* c : kConstants) {
        if (name == c) return true;
    }
    return false;
}

ParseResult Parser::parse(const std::string& src) {
    ParseResult res;
    std::vector<Token> tokens;
    AxialError terr;
    if (!Tokenizer::run(src, tokens, terr)) {
        res.err = terr;
        return res;
    }
    P parser(tokens);
    NodePtr root = parser.expr();
    if (!root) {
        res.err = parser.err;
        return res;
    }
    // trailing tokens that are not consumed
    const Token& rest = tokens[parser.pos];
    if (rest.kind == Tok::RParen) {
        parser.fail(ErrorKind::Syntax, rest,
                    "Unerwartetes Symbol am Ende",
                    "Zu viele schließende Klammern.");
        res.err = parser.err;
        return res;
    }
    if (rest.kind != Tok::End) {
        parser.fail(ErrorKind::Syntax, rest,
                    "Unerwartetes Symbol am Ende",
                    "Operator oder fehlende Zahl prüfen.");
        res.err = parser.err;
        return res;
    }
    res.root = root;
    res.parensFixed = parser.fixed;
    return res;
}

}  // namespace axial