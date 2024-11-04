#pragma once

#include "parser_driver.h"
#include <cassert>
#include <cstring>
#include <string>

enum class ValueKind {
  ERROR,
  JSON,
  STRING,
  NUMBER,
  BOOLEAN,
  NIL,
};

// gcc is complaining about using memcpy on unions (we are doing it only for
// trivially copyable types)
#pragma GCC diagnostic ignored "-Wclass-memaccess"

union ValueData {
  std::string string;
  AstNode json;
  double number;
  bool boolean;

  ValueData() {}
  ~ValueData() {}
};

class Value {
  ValueKind kind;
  ValueData data;

public:
  Value() : kind(ValueKind::ERROR) {}
  Value(Value &other) : kind(other.kind) {
    if (kind == ValueKind::STRING) {
      this->data.string = other.data.string;
    } else {
      memcpy(&this->data, &other.data, sizeof(ValueData));
    }
  }

  Value &operator=(Value &other) {
    this->kind = other.kind;
    if (kind == ValueKind::STRING) {
      this->data.string = other.data.string;
    } else {
      memcpy(&this->data, &other.data, sizeof(ValueData));
    }
    return *this;
  }

  void operator=(Value other) {
    this->kind = other.kind;
    if (kind == ValueKind::STRING) {
      std::swap(data.string, other.data.string);
    } else {
      memcpy(&this->data, &other.data, sizeof(ValueData));
    }
  }

  ~Value() {
    if (kind == ValueKind::STRING) {
      std::string offering;
      std::swap(data.string, offering);
    }
  }

  ValueKind get_kind() const { return kind; }

  ValueData &get_data() { return data; }

  static Value error() {
    Value value{};
    value.kind = ValueKind::ERROR;
    return value;
  }

  static Value json(AstNode node) {
    Value value{};
    value.kind = ValueKind::JSON;
    value.data.json = node;
    return value;
  }

  static Value string(std::string_view str) {
    Value value{};
    value.kind = ValueKind::STRING;
    value.data.string = str;
    return value;
  }

  static Value number(double number) {
    Value value{};
    value.kind = ValueKind::NUMBER;
    value.data.number = number;
    return value;
  }

  static Value boolean(bool boolean) {
    Value value{};
    value.kind = ValueKind::BOOLEAN;
    value.data.boolean = boolean;
    return value;
  }

  static Value nil() {
    Value value{};
    value.kind = ValueKind::NIL;
    return value;
  }

  static bool same_kind(const Value &a, const Value &b, ValueKind kind) {
    return a.get_kind() == kind && b.get_kind() == kind;
  }

  static bool add(Value &a, Value &b);
  static bool sub(Value &a, Value &b);
  static bool mul(Value &a, Value &b);
  static bool div(Value &a, Value &b);
  // TODO do actual equality
  static bool eq(Value &a, Value &b);
  static bool max(Value &a, Value &b);
  static bool min(Value &a, Value &b);

  void debug_print(Arena &arena) const;
};

struct Evaluator {
  Arena &arena;
  std::vector<const char *> errors;
  AstNode json_root;

public:
  Evaluator(Arena &arena, AstNode json_root)
      : arena(arena), json_root(json_root) {}

  void error(const char *message) { errors.push_back(message); }
  void report_errors() {
    if (!errors.empty()) {
      printf("\n<<Errors>>\n");
    }
    for (const char *error : errors) {
      std::printf("%s\n", error);
    }
  }
};

Value builtin_size(AstNode expression, Evaluator &ev);

Value builtin_subscript(AstNode expression, Evaluator &ev);

Value map_lookup(AstNode json_map, std::string_view key, Evaluator &ev);

Value builtin_field(AstNode expression, Evaluator &ev);

Value eval(AstNode expression, Evaluator &ev);
