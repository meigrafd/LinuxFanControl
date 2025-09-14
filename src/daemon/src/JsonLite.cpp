#include "JsonLite.hpp"
#include <cctype>
#include <sstream>
#include <iomanip>

namespace jsonlite {

  namespace {

    struct P {
      const std::string& s;
      size_t i = 0;

      char peek() const { return i < s.size() ? s[i] : '\0'; }
      char get() { return i < s.size() ? s[i++] : '\0'; }
      void ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; }

      bool match(const char* kw) {
        size_t j = 0, k = i;
        while (kw[j] && k < s.size() && s[k] == kw[j]) { ++j; ++k; }
        if (kw[j] == 0) { i = k; return true; }
        return false;
      }
    };

    bool parseValue(P& p, Value& out);

    bool parseString(P& p, std::string& out) {
      if (p.get() != '"') return false;
      std::ostringstream o;
      while (true) {
        char c = p.get();
        if (c == '\0') return false;
        if (c == '"') break;
        if (c == '\\') {
          char e = p.get();
          switch (e) {
            case '"': o << '"'; break;
            case '\\': o << '\\'; break;
            case '/': o << '/'; break;
            case 'b': o << '\b'; break;
            case 'f': o << '\f'; break;
            case 'n': o << '\n'; break;
            case 'r': o << '\r'; break;
            case 't': o << '\t'; break;
            case 'u': {
              int u = 0;
              for (int k = 0; k < 4; ++k) {
                char h = p.get();
                if (!std::isxdigit((unsigned char)h)) return false;
                u = u * 16 + (std::isdigit((unsigned char)h) ? h - '0' : (std::tolower((unsigned char)h) - 'a' + 10));
              }
              if (u <= 0x7F) o << (char)u;
              else if (u <= 0x7FF) { o << (char)(0xC0 | (u >> 6)) << (char)(0x80 | (u & 0x3F)); }
              else { o << (char)(0xE0 | (u >> 12)) << (char)(0x80 | ((u >> 6) & 0x3F)) << (char)(0x80 | (u & 0x3F)); }
            } break;
            default: return false;
          }
        } else {
          o << c;
        }
      }
      out = o.str();
      return true;
    }

    bool parseNumber(P& p, double& out) {
      size_t start = p.i;
      if (p.peek() == '-') p.get();
      if (!std::isdigit((unsigned char)p.peek())) return false;
      if (p.peek() == '0') {
        p.get();
      } else {
        while (std::isdigit((unsigned char)p.peek())) p.get();
      }
      if (p.peek() == '.') {
        p.get();
        if (!std::isdigit((unsigned char)p.peek())) return false;
        while (std::isdigit((unsigned char)p.peek())) p.get();
      }
      if (p.peek() == 'e' || p.peek() == 'E') {
        p.get();
        if (p.peek() == '+' || p.peek() == '-') p.get();
        if (!std::isdigit((unsigned char)p.peek())) return false;
        while (std::isdigit((unsigned char)p.peek())) p.get();
      }
      try {
        out = std::stod(p.s.substr(start, p.i - start));
        return true;
      } catch (...) { return false; }
    }

    bool parseArray(P& p, Value& out) {
      if (p.get() != '[') return false;
      Array arr;
      p.ws();
      if (p.peek() == ']') { p.get(); out = Value(std::move(arr)); return true; }
      while (true) {
        Value el;
        p.ws();
        if (!parseValue(p, el)) return false;
        arr.push_back(std::move(el));
        p.ws();
        char c = p.get();
        if (c == ']') break;
        if (c != ',') return false;
      }
      out = Value(std::move(arr));
      return true;
    }

    bool parseObject(P& p, Value& out) {
      if (p.get() != '{') return false;
      Object obj;
      p.ws();
      if (p.peek() == '}') { p.get(); out = Value(std::move(obj)); return true; }
      while (true) {
        p.ws();
        std::string k;
        if (!parseString(p, k)) return false;
        p.ws();
        if (p.get() != ':') return false;
        p.ws();
        Value v;
        if (!parseValue(p, v)) return false;
        obj.emplace(std::move(k), std::move(v));
        p.ws();
        char c = p.get();
        if (c == '}') break;
        if (c != ',') return false;
      }
      out = Value(std::move(obj));
      return true;
    }

    bool parseValue(P& p, Value& out) {
      p.ws();
      char c = p.peek();
      if (c == '"') {
        std::string s;
        if (!parseString(p, s)) return false;
        out = Value(std::move(s)); return true;
      }
      if (c == '-' || std::isdigit((unsigned char)c)) {
        double d;
        if (!parseNumber(p, d)) return false;
        out = Value(d); return true;
      }
      if (c == 'n') { if (p.match("null"))  { out = Value(); return true; } return false; }
      if (c == 't') { if (p.match("true"))  { out = Value(true); return true; } return false; }
      if (c == 'f') { if (p.match("false")) { out = Value(false); return true; } return false; }
      if (c == '[') return parseArray(p, out);
      if (c == '{') return parseObject(p, out);
      return false;
    }

    void stringifyImpl(const Value& v, std::ostringstream& o);

    void escapeStr(const std::string& s, std::ostringstream& o) {
      o << '"';
      for (char c : s) {
        switch (c) {
          case '\"': o << "\\\""; break;
          case '\\': o << "\\\\"; break;
          case '\b': o << "\\b";  break;
          case '\f': o << "\\f";  break;
          case '\n': o << "\\n";  break;
          case '\r': o << "\\r";  break;
          case '\t': o << "\\t";  break;
          default:
            if ((unsigned char)c < 0x20) {
              o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c;
            } else o << c;
        }
      }
      o << '"';
    }

    void stringifyImpl(const Value& v, std::ostringstream& o) {
      if (v.isNull()) { o << "null"; return; }
      if (v.isBool()) { o << (v.asBool() ? "true" : "false"); return; }
      if (v.isNum())  { o << std::setprecision(15) << v.asNum(); return; }
      if (v.isStr())  { escapeStr(v.asStr(), o); return; }
      if (v.isArray()) {
        o << "[";
        const auto& a = v.asArray();
        for (size_t i=0;i<a.size();++i) { if (i) o<<","; stringifyImpl(a[i], o); }
        o << "]";
        return;
      }
      o << "{";
      const auto& m = v.asObj();
      bool first = true;
      for (auto& kv : m) {
        if (!first) o << ",";
        first = false;
        escapeStr(kv.first, o);
        o << ":";
        stringifyImpl(kv.second, o);
      }
      o << "}";
    }

  } // anon

  bool parse(const std::string& s, Value& out, std::string& err) {
    P p{s, 0};
    if (!parseValue(p, out)) { err = "parse error"; return false; }
    p.ws();
    if (p.i != s.size()) { err = "trailing characters"; return false; }
    return true;
  }

  std::string stringify(const Value& v) {
    std::ostringstream o;
    stringifyImpl(v, o);
    return o.str();
  }

} // namespace jsonlite
