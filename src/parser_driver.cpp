#include "parser_driver.h"

#include <cassert>
#include <charconv>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>

void hex_escape(Parser &p, Arena &arena);
AstNode string(Parser &p, Arena &arena);
AstNode number(Parser &p, Arena &arena);
std::optional<AstNode> json_value(Parser &p, Arena &arena);
AstNode json_array(Parser &p, Arena &arena);
AstNode json_object(Parser &p, Arena &arena);

// u[0-9abcdf]{4}
void hex_escape(Parser &p, Arena &arena) {
  uint8_t value = 0;
  for (int j = 0; j < 2; j++) {
    for (int i = 0; i < 2; i++) {
      int c = p.peek();

      if (c >= '0' && c <= '9')
        c -= '0';
      else if (c >= 'A' && c <= 'F')
        c -= 'A' - 10;
      else if (c >= 'a' && c <= 'f')
        c -= 'a' - 10;
      else {
        p.error("Expected hexadecimal");
        return;
      }

      p.next();
      value <<= 4;
      value |= static_cast<uint8_t>(c);
    }
    arena.string_push(static_cast<char>(value));
  }
}

AstNode string(Parser &p, Arena &arena) {
  if (!p.eat('"')) {
    p.error("Expected string start");
  }

  StringIndex start = arena.string_position();
  while (true) {
    int c = p.next();
    switch (c) {
    case '\\': {
      char escaped = c;
      switch (p.next()) {
      case '"':
      case '/':
        escaped = '/';
        break;
      case '\\':
        escaped = '\\';
        break;
      case 'b':
        escaped = '\b';
        break;
      case 'f':
        escaped = '\f';
        break;
      case 'n':
        escaped = '\n';
        break;
      case 'r':
        escaped = '\r';
        break;
      case 't':
        escaped = '\t';
        break;
      case 'u':
        hex_escape(p, arena);
        continue;
      }
      arena.string_push(escaped);
      continue;
    }
    case '"': {
      StringIndex end = arena.string_position();
      return AstNode::string(start, end.raw() - start.raw());
    }
    case EOF:
      p.error("Expected end of string");
      return AstNode::error();
    default:
      arena.string_push(c);
      continue;
    }
  }
}

// Try to convert a std::string_view to a double.
//
// Returns number of characters consumed.
size_t string_view_to_double(std::string_view str, double &result) {
  // why is this so difficult
  // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2007r0.html#a-correct-approach
  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
  if (ec == std::errc()) {
    return (size_t)(ptr - str.begin());
  } else {
    return 0;
  }
}

// number
//     '-'? [0-9]+ ('.' [0-9]+)? (('E' | 'e') ('+' | '-')? [0-9]+)?
AstNode number(Parser &p, Arena &arena) {
  // we are repurposing the back of the string arena as scratch space
  // and will reset it back when we're done
  // make sure that no one else is addings strings to the arena!!!
  StringIndex start = arena.string_position();
  {
    char c;
    // '-'
    if ((c = p.eat('-'))) {
      arena.string_push(c);
    }

    // [0-9]
    if ((c = p.try_consume(isdigit))) {
      arena.string_push(c);
    } else {
      p.error("Expected digit");
    }

    // [0-9]*
    while ((c = p.try_consume(isdigit))) {
      arena.string_push(c);
    }

    if ((c = p.eat('.'))) {
      // [0-9]
      if ((c = p.try_consume(isdigit))) {
        arena.string_push(c);
      } else {
        p.error("Expected digit");
      }

      // [0-9]*
      while ((c = p.try_consume(isdigit))) {
        arena.string_push(c);
      }
    }

    auto match_e = [](int c) { return c == 'e' || c == 'E'; };
    if ((c = p.try_consume(match_e))) {
      arena.string_push(c);

      auto match_op = [](int c) { return c == '+' || c == '-'; };
      if ((c = p.try_consume(match_op))) {
        arena.string_push(c);
      }

      // [0-9]
      if ((c = p.try_consume(isdigit))) {
        arena.string_push(c);
      } else {
        p.error("Expected digit");
      }

      // [0-9]*
      while ((c = p.try_consume(isdigit))) {
        arena.string_push(c);
      }
    }
  }

  StringIndex end = arena.string_position();
  std::string_view view = arena.get_string_between(start, end);

  double value;
  size_t consumed = string_view_to_double(view, value);

  arena.string_truncate(start);

  if (consumed == view.size()) {
    return AstNode::number(value);
  } else {
    p.error("Invalid number");
    return AstNode::error();
  }
}

AstNode identifier_or_keyword(Parser &p, Arena &arena, bool is_expression) {
  StringIndex start = arena.string_position();
  int c;
  while ((c = p.try_consume(isalpha))) {
    arena.string_push(c);
  }

  arena.string_push('\0');
  StringIndex end = arena.string_position();
  const char *str = arena.get_string_between(start, end).data();

  AstNode node;
  // json
  if (std::strcmp(str, "true") == 0) {
    node = AstNode::boolean(true);
  } else if (std::strcmp(str, "false") == 0) {
    node = AstNode::boolean(false);
  } else if (std::strcmp(str, "null") == 0) {
    node = AstNode::nil();
  }
  // expression
  else if (is_expression && std::strcmp(str, "min") == 0) {
    node = AstNode::empty_function(NodeKind::Min);
  } else if (is_expression && std::strcmp(str, "max") == 0) {
    node = AstNode::empty_function(NodeKind::Max);
  } else if (is_expression && std::strcmp(str, "size") == 0) {
    node = AstNode::empty_function(NodeKind::Size);
  } else {
    if (is_expression) {
      node = AstNode::identifier(start, (end.raw() - start.raw()) - 1);
      // we want to keep the identifier in the string buffer
      return node;
    } else {
      p.error("Expected null or boolean");
      node = AstNode::error();
    }
  }

  arena.string_truncate(start);
  return node;
}

// value
//    object
//    array
//    string
//    number
std::optional<AstNode> json_value(Parser &p, Arena &arena) {
  p.consume_whitespace();
  switch (p.peek()) {
  case '{': {
    return json_object(p, arena);
  }
  case '[':
    return json_array(p, arena);
  case '"':
    return string(p, arena);
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case '-':
    return number(p, arena);
  default: {
    int c = p.peek();
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
      return identifier_or_keyword(p, arena, false);
    } else {
      return {};
    }
  }
  }
}

AstNode json_array(Parser &p, Arena &arena) {
  if (!p.eat('[')) {
    p.error("Expected array");
  }
  NodeStackIndex start = arena.node_stack_position();

  while (1) {
    auto node = json_value(p, arena);
    if (!node.has_value()) {
      break;
    }
    arena.node_stack_push(node.value());

    p.consume_whitespace();

    if (!p.eat(',')) {
      break;
    }
  }

  p.consume_whitespace();

  if (!p.eat(']')) {
    p.error("Expected closing ]");
  }

  auto pair = arena.node_stack_finish(start);
  return AstNode::array(pair.first, pair.second);
}

AstNode json_object(Parser &p, Arena &arena) {
  if (!p.eat('{')) {
    p.error("Expected array");
  }
  NodeStackIndex start = arena.node_stack_position();

  while (1) {
    p.consume_whitespace();

    if (p.at('"')) {
      AstNode name = string(p, arena);
      arena.node_stack_push(name);
    } else {
      break;
    }

    if (!p.eat(':')) {
      p.error("Expected :");
    }

    std::optional<AstNode> node = json_value(p, arena);
    if (!node.has_value()) {
      p.error("Expected value");
      node = {AstNode::error()};
    }
    arena.node_stack_push(node.value());

    p.consume_whitespace();

    if (!p.eat(',')) {
      break;
    }
  }

  p.consume_whitespace();

  if (!p.eat('}')) {
    p.error("Expected closing ]");
  }

  auto pair = arena.node_stack_finish(start);
  return AstNode::object(pair.first, pair.second);
}

AstNode parse_json(Parser &p, Arena &arena) {
  auto node = json_value(p, arena);
  if (node.has_value()) {
    return node.value();
  } else {
    p.error("Invalid json");
    return AstNode::error();
  }
}

std::optional<AstNode> expression_pratt(Parser &p, Arena &arena,
                                        int max_precedence);

std::pair<NodeIndex, size_t> function_arguments(Parser &p, Arena &arena) {
  p.consume_whitespace();
  if (!p.eat('(')) {
    p.error("Expected (");
  }

  NodeStackIndex start = arena.node_stack_position();
  while (true) {
    std::optional<AstNode> node = expression_pratt(p, arena, INT_MAX);
    if (node.has_value()) {
      arena.node_stack_push(node.value());
    } else {
      break;
    }

    p.consume_whitespace();
    if (!p.eat(',')) {
      break;
    }
  }

  p.consume_whitespace();
  if (!p.eat(')')) {
    p.error("Expected (");
  }

  return arena.node_stack_finish(start);
}

std::optional<AstNode> expression_atom(Parser &p, Arena &arena) {
  p.consume_whitespace();
  switch (p.peek()) {
  case '"':
    return string(p, arena);
  case '(': {
    p.eat('(');
    AstNode inner = parse_expression(p, arena);
    p.consume_whitespace();
    if (!p.eat(')')) {
      p.error("Expected )");
    }
    return inner;
  }
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case '-':
    return number(p, arena);
  default: {
    int c = p.peek();
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
      AstNode node = identifier_or_keyword(p, arena, true);
      switch (node.get_kind()) {
      case NodeKind::Min:
      case NodeKind::Max:
      case NodeKind::Size: {
        std::pair<NodeIndex, size_t> array = function_arguments(p, arena);
        return AstNode::function(node.get_kind(), array.first, array.second);
      }
      default:
        return node;
      }
    } else {
      return {};
    }
  }
  }
}

// "a.b[1]"
// "a.b[2].c"
// "a.b"
// "a.b[a.b[1]].c"
// "max(a.b[0], a.b[1])"
// "min(a.b[3])"
// "size(a)"
// "size(a.b)"
// "size(a.b[a.b[1]].c)"
// "max(a.b[0], 10, a.b[1], 15)"

AstNode expression_pratt_expect(Parser &p, Arena &arena, int max_precedence) {
  auto node = expression_pratt(p, arena, max_precedence);
  if (node.has_value()) {
    return node.value();
  } else {
    p.error("Expected expression");
    return AstNode::error();
  }
}

std::optional<AstNode> expression_pratt(Parser &p, Arena &arena,
                                        int max_precedence) {
  std::optional<AstNode> atom = expression_atom(p, arena);
  if (!atom.has_value()) {
    return {};
  }

  AstNode left = atom.value();
  while (true) {
    NodeKind function;
    AstNode right;

    p.consume_whitespace();
    switch (p.peek()) {
    case /* 2 */ '[':
      if (max_precedence > 2) {
        p.eat('[');
        function = NodeKind::Subscript;
        right = parse_expression(p, arena);
        if (!p.eat(']')) {
          p.error("Expected ]");
        }
        break;
      } else {
        goto end;
      }
    case /* 1 */ '.':
    case /* 3 */ '*':
    case /* 3 */ '/':
    case /* 4 */ '+':
    case /* 4 */ '-':
    case /* 5 */ '=': {
      int precedence = 0;
      switch (p.peek()) {
      case '.':
        function = NodeKind::Field;
        precedence = 1;
        break;
      case '*':
        function = NodeKind::Mul;
        precedence = 3;
        break;
      case '/':
        function = NodeKind::Div;
        precedence = 3;
        break;
      case '+':
        function = NodeKind::Add;
        precedence = 4;
        break;
      case '-':
        function = NodeKind::Sub;
        precedence = 4;
        break;
      case '=':
        function = NodeKind::Eq;
        precedence = 5;
        break;
      }
      if (max_precedence > precedence) {
        p.next();
        right = expression_pratt_expect(p, arena, precedence);
        break;
      } else {
        goto end;
      }
    }
    case EOF:
    default:
      goto end;
    }

    NodeIndex array = arena.nodes_push(left);
    arena.nodes_push(right);

    left = AstNode::function(function, array, 2);
  }

end:
  return left;
}

AstNode parse_expression(Parser &p, Arena &arena) {
  return expression_pratt_expect(p, arena, INT_MAX);
}
