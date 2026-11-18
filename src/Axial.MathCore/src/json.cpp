// Axial Math Core — minimal JSON implementation
#include "axial/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace axial {

namespace {

struct JParser {
    const char* s;
    size_t n;
    size_t i = 0;
    std::string err;
    int pos = -1;

    void die(const std::string& msg) {
        if (err.empty()) {
            err = msg;
            pos = (int)i;
        }
    }
    void skipWs() {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' ||
                         s[i] == '\r')) {
            ++i;
        }
    }
    bool lit(const char* word) {
        size_t len = std::strlen(word);
        if (i + len <= n && std::strncmp(s + i, word, len) == 0) {
            i += len;
            return true;
        }
        return false;
    }

    bool parseValue(JsonValue& out) {
        skipWs();
        if (i >= n) { die("Unerwartetes Ende der JSON-Eingabe"); return false; }
        char c = s[i];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') return parseString(out);
        if (c == 't' || c == 'f') {
            if (lit("true")) { out.kind = JsonValue::Bool; out.boolean = true; return true; }
            if (lit("false")) { out.kind = JsonValue::Bool; out.boolean = false; return true; }
            die("Ung\xC3\xBCltiges Literal");
            return false;
        }
        if (c == 'n') {
            if (lit("null")) { out.kind = JsonValue::Null; return true; }
            die("Ung\xC3\xBCltiges Literal");
            return false;
        }
        return parseNumber(out);
    }

    bool parseNumber(JsonValue& out) {
        size_t start = i;
        if (i < n && (s[i] == '-' || s[i] == '+')) ++i;
        bool digits = false;
        while (i < n && s[i] >= '0' && s[i] <= '9') { ++i; digits = true; }
        if (i < n && s[i] == '.') {
            ++i;
            while (i < n && s[i] >= '0' && s[i] <= '9') { ++i; digits = true; }
        }
        if (i < n && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            if (i < n && (s[i] == '-' || s[i] == '+')) ++i;
            while (i < n && s[i] >= '0' && s[i] <= '9') ++i;
        }
        if (!digits) { die("Ung\xC3\xBCltige Zahl"); return false; }
        std::string num(s + start, i - start);
        out.kind = JsonValue::Number;
        out.number = std::strtod(num.c_str(), nullptr);
        return true;
    }

    bool parseString(JsonValue& out) {
        if (s[i] != '"') { die("String erwartet"); return false; }
        ++i;
        std::string v;
        while (i < n && s[i] != '"') {
            char c = s[i];
            if (c == '\\' && i + 1 < n) {
                char e = s[i + 1];
                i += 2;
                switch (e) {
                    case '"': v.push_back('"'); break;
                    case '\\': v.push_back('\\'); break;
                    case '/': v.push_back('/'); break;
                    case 'b': v.push_back('\b'); break;
                    case 'f': v.push_back('\f'); break;
                    case 'n': v.push_back('\n'); break;
                    case 'r': v.push_back('\r'); break;
                    case 't': v.push_back('\t'); break;
                    case 'u': {
                        if (i + 4 <= n) {
                            unsigned cp = 0;
                            bool ok = true;
                            for (int k = 0; k < 4; ++k) {
                                char h = s[i + k];
                                cp <<= 4;
                                if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                                else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                                else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                                else { ok = false; break; }
                            }
                            if (ok) {
                                i += 4;
                                // encode as UTF-8
                                if (cp < 0x80) v.push_back((char)cp);
                                else if (cp < 0x800) {
                                    v.push_back((char)(0xC0 | (cp >> 6)));
                                    v.push_back((char)(0x80 | (cp & 0x3F)));
                                } else {
                                    v.push_back((char)(0xE0 | (cp >> 12)));
                                    v.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                                    v.push_back((char)(0x80 | (cp & 0x3F)));
                                }
                                continue;
                            }
                        }
                        v.push_back('u');
                        break;
                    }
                    default: v.push_back(e); break;
                }
                continue;
            }
            v.push_back(c);
            ++i;
        }
        if (i >= n) { die("Unbeendeter String"); return false; }
        ++i;  // closing quote
        out.kind = JsonValue::String;
        out.string = v;
        return true;
    }

    bool parseArray(JsonValue& out) {
        ++i;  // [
        out.kind = JsonValue::Array;
        skipWs();
        if (i < n && s[i] == ']') { ++i; return true; }
        for (;;) {
            JsonValue v;
            if (!parseValue(v)) return false;
            out.array.push_back(std::move(v));
            skipWs();
            if (i < n && s[i] == ',') { ++i; continue; }
            if (i < n && s[i] == ']') { ++i; return true; }
            die("Erwartete ',' oder ']' im Array");
            return false;
        }
    }

    bool parseObject(JsonValue& out) {
        ++i;  // {
        out.kind = JsonValue::Object;
        skipWs();
        if (i < n && s[i] == '}') { ++i; return true; }
        for (;;) {
            skipWs();
            JsonValue key;
            if (i >= n || s[i] != '"') { die("Objektschl\xC3\xBCssel erwartet"); return false; }
            if (!parseString(key)) return false;
            skipWs();
            if (i >= n || s[i] != ':') { die("Erwartete ':' im Objekt"); return false; }
            ++i;
            JsonValue v;
            if (!parseValue(v)) return false;
            out.object.emplace_back(key.string, std::move(v));
            skipWs();
            if (i < n && s[i] == ',') { ++i; continue; }
            if (i < n && s[i] == '}') { ++i; return true; }
            die("Erwartete ',' oder '}' im Objekt");
            return false;
        }
    }
};

}  // namespace

bool jsonParse(const std::string& text, JsonValue& out, std::string& err,
               int& pos) {
    JParser p{text.c_str(), text.size(), 0, "", -1};
    if (!p.parseValue(out)) {
        err = p.err.empty() ? "JSON-Fehler" : p.err;
        pos = p.pos;
        return false;
    }
    p.skipWs();
    if (p.i != p.n) {
        err = "Unbekannte Daten nach JSON-Wert";
        pos = (int)p.i;
        return false;
    }
    err.clear();
    pos = -1;
    return true;
}

std::string jsonEscape(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\b': r += "\\b"; break;
            case '\f': r += "\\f"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    r += buf;
                } else {
                    r.push_back((char)c);
                }
        }
    }
    return r;
}

}  // namespace axial