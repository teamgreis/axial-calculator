// Axial Math Core — minimal JSON (parse + serialize), no dependencies
#pragma once

#include <map>
#include <string>
#include <vector>

namespace axial {

struct JsonValue {
    enum Kind { Null, Bool, Number, String, Array, Object };
    Kind kind = Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;

    bool isObject() const { return kind == Object; }
    bool isArray() const { return kind == Array; }
    bool isString() const { return kind == String; }
    bool isNumber() const { return kind == Number; }

    const JsonValue* find(const std::string& key) const {
        if (kind != Object) return nullptr;
        for (const auto& kv : object) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
};

// Parses JSON text. On failure, err holds a message and pos the offset.
bool jsonParse(const std::string& text, JsonValue& out, std::string& err,
               int& pos);

// Serializes compact JSON (no spaces). Input is never invalid by construction
// when built through the helpers below.
std::string jsonEscape(const std::string& s);

}  // namespace axial