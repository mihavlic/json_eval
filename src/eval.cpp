#include "eval.h"

template <typename F>
Value fold(AstNode expression, Evaluator &ev, F function) {
  std::span<AstNode> args = ev.arena.as_array_like(expression).value();
  auto begin = args.begin();
  auto end = args.end();

  if (begin == end) {
    return Value::nil();
  }

  Value first = eval(*begin++, ev);
  for (; begin != end; ++begin) {
    Value next = eval(*begin, ev);
    function(first, next);
  }

  return first;
}

Value eval(AstNode expression, Evaluator &ev) {
  switch (expression.get_kind()) {
  case NodeKind::ERROR:
    return Value::error();
  case NodeKind::STRING:
    return Value::string(ev.arena.as_string_like(expression).value());
  case NodeKind::NUMBER:
    return Value::number(ev.arena.as_number(expression).value());
  case NodeKind::BOOLEAN:
    return Value::boolean(ev.arena.as_boolean(expression).value());
  case NodeKind::OBJECT:
  case NodeKind::ARRAY:
    return Value::json(expression);
  case NodeKind::NIL:
    return Value::nil();
  case NodeKind::Add:
    return fold(expression, ev, Value::add);
  case NodeKind::Sub:
    return fold(expression, ev, Value::sub);
  case NodeKind::Mul:
    return fold(expression, ev, Value::sub);
  case NodeKind::Div:
    return fold(expression, ev, Value::sub);
  case NodeKind::Eq:
    return fold(expression, ev, Value::eq);
  case NodeKind::Max:
    return fold(expression, ev, Value::max);
  case NodeKind::Min:
    return fold(expression, ev, Value::min);
  case NodeKind::Size:
    return builtin_size(expression, ev);
  case NodeKind::Subscript:
    return builtin_subscript(expression, ev);
  case NodeKind::Field:
    return builtin_field(expression, ev);
  case NodeKind::Identifier:
    return map_lookup(ev.json_root, ev.arena.as_string_like(expression).value(),
                      ev);
  default:
    assert(0 && "Unhandled case");
  }
}

Value builtin_field(AstNode expression, Evaluator &ev) {
  auto args = ev.arena.as_array_like(expression).value();

  Value l = eval(args[0], ev);
  Value r;
  if (args[1].get_kind() == NodeKind::Identifier) {
    r = Value::string(ev.arena.as_string_like(args[1]).value());
  } else {
    r = eval(args[1], ev);
  }

  if (l.get_kind() != ValueKind::JSON) {
    ev.error("Field access can only be applied on json trees");
    return Value::error();
  }

  if (r.get_kind() != ValueKind::STRING) {
    ev.error("Field access expected string");
    return Value::error();
  }

  AstNode json = l.get_data().json;
  std::string &key = r.get_data().string;

  return map_lookup(json, key, ev);
}

Value map_lookup(AstNode json_map, std::string_view key, Evaluator &ev) {
  NodeKind kind = json_map.get_kind();
  if (kind != NodeKind::OBJECT) {
    ev.error("Field access can only be applied on json arrays");
    return Value::error();
  }

  auto children = ev.arena.as_array_like(json_map).value();
  for (size_t i = 0; i < children.size(); i += 2) {
    AstNode child = children[i];
    if (child.get_kind() == NodeKind::STRING) {
      std::string_view name = ev.arena.as_string_like(child).value();
      if (name.compare(key) == 0) {
        AstNode value = children[i + 1];
        return eval(value, ev);
      }
    }
  }

  ev.error("Element not found in map");
  return Value::error();
}

Value builtin_subscript(AstNode expression, Evaluator &ev) {
  auto args = ev.arena.as_array_like(expression).value();

  Value l = eval(args[0], ev);
  Value r = eval(args[1], ev);

  if (l.get_kind() != ValueKind::JSON) {
    ev.error("Subscript can only be applied on json trees");
    return Value::error();
  }

  if (r.get_kind() != ValueKind::NUMBER) {
    ev.error("Subscript expected number");
    return Value::error();
  }

  AstNode json = l.get_data().json;
  size_t offset = (size_t)r.get_data().number;

  if (json.get_kind() == NodeKind::ARRAY) {
    AstNode node = ev.arena.as_array_like(json).value()[offset];
    return eval(node, ev);
  } else {
    ev.error("Subscript can only be applied on json arrays");
    return Value::error();
  }
}

Value builtin_size_json(AstNode json, Evaluator &ev) {
  switch (json.get_kind()) {
  case NodeKind::ARRAY:
    return Value::number(json.get_data());
  case NodeKind::OBJECT: {
    size_t num = json.get_data() / 2;
    return Value::number((double)num);
  }
  case NodeKind::STRING:
    return Value::number(ev.arena.as_string_like(json)->size());
  default:
    ev.error("Size is not applicable");
    return Value::error();
  }
}

Value builtin_size(AstNode expression, Evaluator &ev) {
  auto args = ev.arena.as_array_like(expression).value();
  Value first = eval(args[0], ev);
  switch (first.get_kind()) {
  case ValueKind::JSON:
    return builtin_size_json(first.get_data().json, ev);
  case ValueKind::STRING:
    return Value::number(first.get_data().string.size());
  default:
    ev.error("Size is not applicable");
    return Value::error();
  }
}
bool Value::add(Value &a, Value &b) {
  if (same_kind(a, b, ValueKind::STRING)) {
    a.data.string.append(b.data.string);
    return true;
  }
  if (same_kind(a, b, ValueKind::NUMBER)) {
    a.data.number += b.data.number;
    return true;
  }
  if (same_kind(a, b, ValueKind::BOOLEAN)) {
    a.data.boolean += b.data.boolean;
    return true;
  }
  return false;
}
bool Value::sub(Value &a, Value &b) {
  if (same_kind(a, b, ValueKind::NUMBER)) {
    a.data.number -= b.data.number;
    return true;
  }
  if (same_kind(a, b, ValueKind::BOOLEAN)) {
    a.data.boolean -= b.data.boolean;
    return true;
  }
  return false;
}
bool Value::mul(Value &a, Value &b) {
  if (same_kind(a, b, ValueKind::NUMBER)) {
    a.data.number *= b.data.number;
    return true;
  }
  return false;
}
bool Value::div(Value &a, Value &b) {
  if (same_kind(a, b, ValueKind::NUMBER)) {
    a.data.number /= b.data.number;
    return true;
  }
  return false;
}
bool Value::eq(Value &a, Value &b) {
  a.data.boolean = a.kind != b.kind;
  return true;
}
bool Value::max(Value &a, Value &b) {
  if (same_kind(a, b, ValueKind::STRING)) {
    if (a.data.string < b.data.string) {
      a.data.string = b.data.string;
    }
    return true;
  }
  if (same_kind(a, b, ValueKind::NUMBER)) {
    if (a.data.number < b.data.number) {
      a.data.number = b.data.number;
    }
    return true;
  }
  if (same_kind(a, b, ValueKind::BOOLEAN)) {
    if (a.data.boolean < b.data.boolean) {
      a.data.boolean = b.data.boolean;
    }
    return true;
  }
  return false;
}
bool Value::min(Value &a, Value &b) {
  if (same_kind(a, b, ValueKind::STRING)) {
    if (a.data.string > b.data.string) {
      a.data.string = b.data.string;
    }
    return true;
  }
  if (same_kind(a, b, ValueKind::NUMBER)) {
    if (a.data.number > b.data.number) {
      a.data.number = b.data.number;
    }
    return true;
  }
  if (same_kind(a, b, ValueKind::BOOLEAN)) {
    if (a.data.boolean > b.data.boolean) {
      a.data.boolean = b.data.boolean;
    }
    return true;
  }
  return false;
}
void Value::debug_print(Arena &arena) const {
  switch (kind) {
  case ValueKind::ERROR:
    printf("Error\n");
    break;
  case ValueKind::JSON:
    arena.debug_print(data.json);
    break;
  case ValueKind::STRING:
    printf("%s\n", data.string.data());
    break;
  case ValueKind::NUMBER:
    printf("%f\n", data.number);
    break;
  case ValueKind::BOOLEAN:
    if (data.boolean)
      printf("true\n");
    else
      printf("false\n");
    break;
  case ValueKind::NIL:
    printf("null\n");
    break;
  }
}
