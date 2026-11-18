// Axial Math Core — engine implementation (C++ + C ABI)
#include "axial/engine.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "axial/axial_config.hpp"
#include "axial/tokenizer.hpp"

namespace axial {

namespace {

bool toAngle(const std::string& s, AngleMode& out) {
    if (s == "deg") out = AngleMode::Deg;
    else if (s == "rad") out = AngleMode::Rad;
    else if (s == "gon") out = AngleMode::Gon;
    else return false;
    return true;
}

std::string numberToJson(double v) {
    if (!std::isfinite(v)) return "0";
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return std::string(buf);
}

}  // namespace

EvalResult engineEval(const std::string& src, const EvalOptions& opts) {
    EvalResult res;
    std::string trimmed = src;
    // trim spaces for a fast empty-check
    size_t a = trimmed.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) {
        res.err.kind = ErrorKind::Syntax;
        res.err.pos = -1;
        res.err.message = "Leerer Ausdruck";
        res.err.hint = "Einen Term eingeben, z. B. 2+3*4.";
        return res;
    }
    size_t b = trimmed.find_last_not_of(" \t\r\n");
    trimmed = trimmed.substr(a, b - a + 1);

    ParseResult pr = Parser::parse(trimmed);
    if (!pr.ok()) {
        res.err = pr.err;
        return res;
    }
    res.parensFixed = pr.parensFixed;

    Program prog;
    AxialError cerr;
    if (!Compiler::compile(*pr.root, prog, cerr)) {
        res.err = cerr;
        return res;
    }

    Environment env;
    env.angle = opts.angle;
    env.vars = opts.vars;

    Value out;
    AxialError rerr;
    if (!vmRun(prog, env, out, rerr)) {
        res.err = rerr;
        return res;
    }

    res.ok = true;
    res.value = out;
    res.exact = out.kind == ValueKind::Rational;
    res.text = valueToString(out);
    return res;
}

}  // namespace axial

// ---------------------------------------------------------------- C ABI

using namespace axial;

extern "C" AXIAL_API const char* axial_version(void) {
    return AXIAL_VERSION_STRING;
}

extern "C" AXIAL_API void axial_free(char* p) {
    std::free(p);
}

namespace {

char* dupString(const std::string& s) {
    char* p = static_cast<char*>(std::malloc(s.size() + 1));
    if (p == nullptr) return nullptr;
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

std::string errorJson(const AxialError& e) {
    std::string s = "{\"ok\":false,\"error\":{";
    s += "\"kind\":\"" + std::string(errorKindName(e.kind)) + "\",";
    s += "\"pos\":" + std::to_string(e.pos) + ",";
    s += "\"token\":\"" + jsonEscape(e.token) + "\",";
    s += "\"message\":\"" + jsonEscape(e.message) + "\",";
    s += "\"hint\":\"" + jsonEscape(e.hint) + "\"";
    s += "}}";
    return s;
}

std::string resultJson(const EvalResult& r) {
    std::string s = "{\"ok\":true,\"exact\":";
    s += r.exact ? "true" : "false";
    s += ",\"text\":\"" + jsonEscape(r.text) + "\"";
    s += ",\"approx\":" + numberToJson(r.value.toDouble());
    const char* kind = "real";
    if (r.value.kind == ValueKind::Rational) kind = "rational";
    else if (r.value.kind == ValueKind::Complex) kind = "complex";
    s += ",\"kind\":\"" + std::string(kind) + "\"";
    if (r.parensFixed > 0) {
        s += ",\"parensFixed\":" + std::to_string(r.parensFixed);
    }
    s += "}";
    return s;
}

}  // namespace

extern "C" AXIAL_API char* axial_eval(const char* text, const char* options_json) {
    if (text == nullptr) {
        return dupString(errorJson(AxialError{ErrorKind::Syntax, -1, "",
                                              "Kein Ausdruck", ""}));
    }
    EvalOptions opts;
    if (options_json != nullptr && options_json[0] != '\0') {
        std::string jerr;
        int jpos = -1;
        JsonValue root;
        if (jsonParse(options_json, root, jerr, jpos) && root.isObject()) {
            if (const JsonValue* angle = root.find("angle")) {
                if (angle->isString()) {
                    AngleMode m;
                    if (toAngle(angle->string, m)) opts.angle = m;
                }
            }
            if (const JsonValue* vars = root.find("vars")) {
                if (vars->isObject()) {
                    for (const auto& kv : vars->object) {
                        const JsonValue& v = kv.second;
                        if (v.isNumber()) {
                            Value val = Value::real(v.number);
                            Value exact;
                            if (Rational::fromDouble(v.number, exact.q)) {
                                exact.kind = ValueKind::Rational;
                                val = exact;
                            }
                            opts.vars.emplace_back(kv.first, val);
                        } else if (v.isString()) {
                            // parse variable values as expressions themselves
                            EvalResult sub = engineEval(v.string, opts);
                            if (sub.ok) opts.vars.emplace_back(kv.first, sub.value);
                        }
                    }
                }
            }
        }
    }
    EvalResult r = engineEval(text, opts);
    return dupString(r.ok ? resultJson(r) : errorJson(r.err));
}