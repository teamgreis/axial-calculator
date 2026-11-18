// Axial Math Core — error model
// Every failure is a typed value with position, token, cause and hint.
// The core never aborts: functions return status, never throw across the ABI.
#pragma once

#include <string>

namespace axial {

enum class ErrorKind {
    None = 0,
    ReadableFailure,   // generic
    Syntax,            // grammar violation
    UnknownToken,      // character not part of the language
    UnknownFunction,   // function name not known
    UnknownVariable,   // identifier not bound
    DivideByZero,
    Domain,            // function outside its domain
    Overflow,          // magnitude beyond limits
    Limit,             // resource limit hit (e.g. factorial bound)
    NotExact,          // exact path requested but unavailable
    Internal
};

struct AxialError {
    ErrorKind kind = ErrorKind::None;
    int pos = -1;                 // character position in source, -1 = none
    std::string token;            // offending token ("" = none)
    std::string message;          // human readable cause
    std::string hint;             // correction hint ("" = none)

    bool ok() const { return kind == ErrorKind::None; }
};

inline const char* errorKindName(ErrorKind k) {
    switch (k) {
        case ErrorKind::None: return "None";
        case ErrorKind::ReadableFailure: return "ReadableFailure";
        case ErrorKind::Syntax: return "Syntax";
        case ErrorKind::UnknownToken: return "UnknownToken";
        case ErrorKind::UnknownFunction: return "UnknownFunction";
        case ErrorKind::UnknownVariable: return "UnknownVariable";
        case ErrorKind::DivideByZero: return "DivideByZero";
        case ErrorKind::Domain: return "Domain";
        case ErrorKind::Overflow: return "Overflow";
        case ErrorKind::Limit: return "Limit";
        case ErrorKind::NotExact: return "NotExact";
        case ErrorKind::Internal: return "Internal";
    }
    return "Unknown";
}

}  // namespace axial