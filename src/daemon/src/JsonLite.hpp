#pragma once
/*
 * Tiny JSON reader/writer for JSON-RPC needs (no external deps).
 * Supports: null, bool, number, string, array, object.
 * (c) 2025 LinuxFanControl contributors
 */
#include <string>
#include <vector>
#include <map>
#include <variant>

namespace jsonlite {

  struct Value;

  using Object = std::map<std::string, Value>;
  using Array  = std::vector<Value>;

  struct Value {
    using V = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;
    V v;

    Value() : v(nullptr) {}
    explicit Value(std::nullptr_t) : v(nullptr) {}
    explicit Value(bool b) : v(b) {}
    explicit Value(double d) : v(d) {}
    explicit Value(const std::string& s) : v(s) {}
    explicit Value(std::string&& s) : v(std::move(s)) {}
    explicit Value(const char* s) : v(std::string(s)) {}
    explicit Value(const Array& a) : v(a) {}
    explicit Value(Array&& a) : v(std::move(a)) {}
    explicit Value(const Object& o) : v(o) {}
    explicit Value(Object&& o) : v(std::move(o)) {}

    bool isNull()   const { return std::holds_alternative<std::nullptr_t>(v); }
    bool isBool()   const { return std::holds_alternative<bool>(v); }
    bool isNum()    const { return std::holds_alternative<double>(v); }
    bool isStr()    const { return std::holds_alternative<std::string>(v); }
    bool isArray()  const { return std::holds_alternative<Array>(v); }
    bool isObject() const { return std::holds_alternative<Object>(v); }

    const std::string& asStr()  const { return std::get<std::string>(v); }
    double             asNum()  const { return std::get<double>(v); }
    bool               asBool() const { return std::get<bool>(v); }
    const Array&       asArray()const { return std::get<Array>(v); }
    const Object&      asObj()  const { return std::get<Object>(v); }

    Array&  mutArray() { return std::get<Array>(v); }
    Object& mutObj()   { return std::get<Object>(v); }
  };

  bool parse(const std::string& s, Value& out, std::string& err);
  std::string stringify(const Value& v);

  inline const Value* objGet(const Value& v, const std::string& key) {
    if (!v.isObject()) return nullptr;
    auto& o = v.asObj();
    auto it = o.find(key);
    if (it == o.end()) return nullptr;
    return &it->second;
  }

} // namespace jsonlite
