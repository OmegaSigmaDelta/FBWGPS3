// Minimal dependency-free JSON reader (C++17). Enough for Tiled 1.1 maps, TexturePacker atlases and save files.
// PS3 note: parse errors are reported through the optional `err` out-parameter; if it is null a
// std::runtime_error is thrown (PSL1GHT builds with exceptions enabled by default; otherwise always pass `err`).
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace fw {

struct Json {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> a;                              // Arr
    std::vector<std::pair<std::string, Json>> o;      // Obj (insertion order kept)

    bool isNull() const { return type == Null; }
    bool isNum()  const { return type == Num; }
    bool isStr()  const { return type == Str; }
    bool isArr()  const { return type == Arr; }
    bool isObj()  const { return type == Obj; }

    const Json* find(const std::string& k) const {
        if (type != Obj) return nullptr;
        for (auto& kv : o) if (kv.first == k) return &kv.second;
        return nullptr;
    }
    bool has(const std::string& k) const { return find(k) != nullptr; }
    const Json& operator[](const std::string& k) const { static const Json nil; auto p = find(k); return p ? *p : nil; }
    const Json& operator[](size_t i) const { static const Json nil; return (type == Arr && i < a.size()) ? a[i] : nil; }
    size_t size() const { return type == Arr ? a.size() : type == Obj ? o.size() : 0; }

    double num(double d = 0) const { return type == Num ? n : (type == Bool ? (b ? 1 : 0) : d); }
    int    i32(int d = 0)   const { return type == Num ? (int)n : d; }
    bool   boolean(bool d = false) const { return type == Bool ? b : (type == Num ? n != 0 : d); }
    std::string str(const std::string& d = "") const { return type == Str ? s : d; }

    static Json parse(const std::string& text, std::string* err = nullptr);
};

namespace detail {
struct P {
    const char* p; const char* e; std::string err;
    void ws() { while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p; }
    bool fail(const char* m) { if (err.empty()) err = m; return false; }
    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    bool str(std::string& out) {
        if (p >= e || *p != '"') return fail("string expected");
        ++p;
        while (p < e && *p != '"') {
            if (*p == '\\') {
                if (++p >= e) return fail("bad escape");
                switch (*p) {
                    case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                    case 'u': {
                        if (e - p < 5) return fail("bad \\u");
                        unsigned cp = (unsigned)strtoul(std::string(p + 1, p + 5).c_str(), nullptr, 16); p += 4;
                        if (cp >= 0xD800 && cp < 0xDC00 && e - p > 6 && p[1] == '\\' && p[2] == 'u') {
                            unsigned lo = (unsigned)strtoul(std::string(p + 3, p + 7).c_str(), nullptr, 16);
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); p += 6;
                        }
                        utf8(out, cp); break;
                    }
                    default: out += *p;
                }
                ++p;
            } else out += *p++;
        }
        if (p >= e) return fail("unterminated string");
        ++p; return true;
    }
    bool val(Json& j) {
        ws(); if (p >= e) return fail("unexpected end");
        char c = *p;
        if (c == '{') {
            ++p; j.type = Json::Obj; ws();
            if (p < e && *p == '}') { ++p; return true; }
            while (true) {
                ws(); std::string k; if (!str(k)) return false;
                ws(); if (p >= e || *p != ':') return fail("':' expected"); ++p;
                Json v; if (!val(v)) return false;
                j.o.emplace_back(std::move(k), std::move(v)); ws();
                if (p < e && *p == ',') { ++p; continue; }
                if (p < e && *p == '}') { ++p; return true; }
                return fail("',' or '}' expected");
            }
        }
        if (c == '[') {
            ++p; j.type = Json::Arr; ws();
            if (p < e && *p == ']') { ++p; return true; }
            while (true) {
                Json v; if (!val(v)) return false; j.a.push_back(std::move(v)); ws();
                if (p < e && *p == ',') { ++p; continue; }
                if (p < e && *p == ']') { ++p; return true; }
                return fail("',' or ']' expected");
            }
        }
        if (c == '"') { j.type = Json::Str; return str(j.s); }
        if (e - p >= 4 && !strncmp(p, "true", 4))  { j.type = Json::Bool; j.b = true;  p += 4; return true; }
        if (e - p >= 5 && !strncmp(p, "false", 5)) { j.type = Json::Bool; j.b = false; p += 5; return true; }
        if (e - p >= 4 && !strncmp(p, "null", 4))  { j.type = Json::Null; p += 4; return true; }
        char* endp = nullptr; double d = strtod(p, &endp);
        if (endp == p) return fail("value expected");
        j.type = Json::Num; j.n = d; p = endp; return true;
    }
};
} // namespace detail

inline Json Json::parse(const std::string& text, std::string* err) {
    detail::P ps{text.data(), text.data() + text.size(), {}};
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) ps.p += 3; // BOM
    Json j; ps.val(j);
    if (!ps.err.empty()) { if (err) *err = ps.err; else throw std::runtime_error("json: " + ps.err); return Json(); }
    return j;
}

inline bool readFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb"); if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize((size_t)n); size_t r = n ? fread(&out[0], 1, (size_t)n, f) : 0; fclose(f);
    return r == (size_t)n;
}
inline Json loadJson(const std::string& path) {
    std::string t; if (!readFile(path, t)) throw std::runtime_error("cannot read " + path);
    return Json::parse(t);
}
} // namespace fw
