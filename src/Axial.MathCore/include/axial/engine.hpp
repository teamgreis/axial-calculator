// Axial Math Core — engine API (C++) and stable C ABI
#pragma once

#include <string>

#include "axial/errors.hpp"
#include "axial/json.hpp"
#include "axial/parser.hpp"
#include "axial/value.hpp"
#include "axial/vm.hpp"

namespace axial {

struct EvalOptions {
    AngleMode angle = AngleMode::Deg;
    std::vector<std::pair<std::string, Value>> vars;
};

struct EvalResult {
    bool ok = false;
    bool exact = false;
    Value value;
    std::string text;             // engine display text
    AxialError err;
    int parensFixed = 0;          // automatically closed parentheses
};

// Full pipeline: tokenize → parse → compile → run. Never aborts.
EvalResult engineEval(const std::string& src, const EvalOptions& opts);

}  // namespace axial

// ---------------------------------------------------------------- C ABI
// Caller frees returned strings with axial_free(char*).
#ifdef __cplusplus
extern "C" {
#endif

AXIAL_API const char* axial_version(void);
// Evaluates an expression. options_json may be NULL/"" for defaults:
//   {"angle":"deg"|"rad"|"gon", "vars":{"x":3.5,"y":"1/2"}}
// Returns JSON result (never NULL):
//   {"ok":true,"exact":true,"text":"1/2","approx":0.5,"kind":"rational",
//    "parensFixed":0}
//   {"ok":false,"error":{"kind":"DivideByZero","pos":4,"token":"/",
//                        "message":"...","hint":"..."}}
AXIAL_API char* axial_eval(const char* text, const char* options_json);
AXIAL_API void axial_free(char* p);

#ifdef __cplusplus
}
#endif