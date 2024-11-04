#include "json.h"
#include "util.h"
#include <cassert>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <optional>

JsonNode::JsonNode(JsonNodeKind kind, size_t data, JsonNodeValue value)
    : data((data << 3) | (size_t)kind), value(value) {
  assert((int)kind < (1 << 3));
  assert(data < (1L << 61));
}

StringIndex JsonArena::string_position() const {
  return StringIndex(string_arena.size());
}

std::string_view JsonArena::get_string(StringIndex start, size_t len) const {
  return std::string_view(string_arena.data() + start.raw(), len);
}

std::string_view JsonArena::get_string_between(StringIndex start,
                                               StringIndex end) const {
  size_t len = 0;
  if (start.raw() < end.raw()) {
    len = end.raw() - start.raw();
  }
  return std::string_view(string_arena.data() + start.raw(), len);
}

std::span<JsonNode> JsonArena::get_nodes(NodeIndex start, size_t len) {
  return std::span(node_arena.data() + start.raw(), len);
}

std::span<JsonNode> JsonArena::get_node_stack(NodeStackIndex start,
                                              size_t len) {
  return std::span(node_stack.data() + start.raw(), len);
}

std::span<JsonNode> JsonArena::get_node_stack_between(NodeStackIndex start,
                                                      NodeStackIndex end) {
  size_t len = 0;
  if (start.raw() < end.raw()) {
    len = end.raw() - start.raw();
  }
  return std::span(node_stack.data() + start.raw(), len);
}

std::pair<NodeIndex, size_t>
JsonArena::node_stack_finish(NodeStackIndex start) {
  std::span<JsonNode> children =
      get_node_stack_between(start, node_stack_position());
  size_t children_len = children.size();

  NodeIndex new_start(node_arena.size());
  for (JsonNode node : children) {
    node_arena.push_back(node);
  }

  node_stack_truncate(start);

  return {new_start, children_len};
}

void JsonArena::debug_print_impl(JsonNode node, int depth) {
  for (int i = 0; i < depth; i++) {
    printf("  ");
  }
  auto kind = node.get_kind();
  switch (kind) {
  case JsonNodeKind::ERROR:
    printf("Error\n");
    break;
  case JsonNodeKind::STRING: {
    auto string = get_string(node.get_value().string_start, node.get_data());
    std::cout << '"' << string << '"' << std::endl;
    break;
  }
  case JsonNodeKind::NUMBER:
    std::cout << node.get_value().number << std::endl;
    break;
  case JsonNodeKind::BOOLEAN:
    std::cout << node.get_value().boolean << std::endl;
    break;
  case JsonNodeKind::OBJECT: {
  case JsonNodeKind::ARRAY:
    std::cout << "{Object}\n";
    auto start = node.get_value().nodes_start;
    auto len = node.get_data();
    auto nodes = get_nodes(start, len);
    for (JsonNode node : nodes) {
      debug_print_impl(node, depth + 1);
    }
    break;
  }
  case JsonNodeKind::NIL:
    printf("null\n");
    break;
  default:
    PANIC("Unhandled case");
  }
}

void JsonArena::debug_print(JsonNode node) { debug_print_impl(node, 0); }

int JsonParser::peek() const { return current; }

int JsonParser::next() {
  if (current == '\n') {
    line++;
    column = 0;
  } else if (current != EOF) {
    column++;
  }

  int prev = current;
  current = file.get();
  return prev;
}

int JsonParser::eat(char c) {
  int peek = current;
  if (peek == c) {
    next();
    return peek;
  }
  return 0;
}

bool JsonParser::at(char c) const { return current == c; }

void JsonParser::error(const char *message) {
  errors.push_back(ParseError{line, column, message});
}

void JsonParser::report_errors(const char *filename) const {
  for (const ParseError &error : errors) {
    printf("%s:%d:%d %s\n", filename, error.line, error.column, error.message);
  }
}

void whitespace(JsonParser &p);
void hex_escape(JsonParser &p, JsonArena &arena);
JsonNode string(JsonParser &p, JsonArena &arena);
JsonNode number(JsonParser &p, JsonArena &arena);
std::optional<JsonNode> value(JsonParser &p, JsonArena &arena);
JsonNode array(JsonParser &p, JsonArena &arena);
JsonNode object(JsonParser &p, JsonArena &arena);

// whitespace
//     [ \n\r\t]*
void whitespace(JsonParser &p) {
  while (true) {
    switch (p.peek()) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
      p.next();
      continue;
    default:
      return;
    }
  }
}

// u[0-9abcdf]{4}
void hex_escape(JsonParser &p, JsonArena &arena) {
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

JsonNode string(JsonParser &p, JsonArena &arena) {
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
      return JsonNode::string(start, end.raw() - start.raw());
    }
    case EOF:
      p.error("Expected end of string");
      return JsonNode::error();
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
JsonNode number(JsonParser &p, JsonArena &arena) {
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
    if ((c = p.tryConsume(isdigit))) {
      arena.string_push(c);
    } else {
      p.error("Expected digit");
    }

    // [0-9]*
    while ((c = p.tryConsume(isdigit))) {
      arena.string_push(c);
    }

    if ((c = p.eat('.'))) {
      // [0-9]
      if ((c = p.tryConsume(isdigit))) {
        arena.string_push(c);
      } else {
        p.error("Expected digit");
      }

      // [0-9]*
      while ((c = p.tryConsume(isdigit))) {
        arena.string_push(c);
      }
    }

    auto match_e = [](int c) { return c == 'e' || c == 'E'; };
    if ((c = p.tryConsume(match_e))) {
      arena.string_push(c);

      auto match_op = [](int c) { return c == '+' || c == '-'; };
      if ((c = p.tryConsume(match_op))) {
        arena.string_push(c);
      }

      // [0-9]
      if ((c = p.tryConsume(isdigit))) {
        arena.string_push(c);
      } else {
        p.error("Expected digit");
      }

      // [0-9]*
      while ((c = p.tryConsume(isdigit))) {
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
    return JsonNode::number(value);
  } else {
    p.error("Invalid number");
    return JsonNode::error();
  }
}

std::optional<JsonNode> boolean(JsonParser &p, JsonArena &arena) {
  StringIndex start = arena.string_position();
  int c;
  while ((c = p.tryConsume(isalpha))) {
    arena.string_push(c);
  }
  arena.string_push('\0');
  StringIndex end = arena.string_position();

  const char *str = arena.get_string_between(start, end).data();

  JsonNode node;
  if (std::strcmp(str, "true") == 0) {
    node = JsonNode::boolean(true);
  } else if (std::strcmp(str, "false") == 0) {
    node = JsonNode::boolean(false);
  } else if (std::strcmp(str, "null") == 0) {
    node = JsonNode::nil();
  } else {
    p.error("Expected null or boolean");
    node = JsonNode::error();
  }

  arena.string_truncate(start);
  return node;
}

// value
//    object
//    array
//    string
//    number
std::optional<JsonNode> value(JsonParser &p, JsonArena &arena) {
  whitespace(p);
  switch (p.peek()) {
  case '{': {
    return object(p, arena);
  }
  case '[':
    return array(p, arena);
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
      return boolean(p, arena);
    } else {
      return {};
    }
  }
  }
}

JsonNode array(JsonParser &p, JsonArena &arena) {
  if (!p.eat('[')) {
    p.error("Expected array");
  }
  NodeStackIndex start = arena.node_stack_position();

  while (1) {
    auto node = value(p, arena);
    if (!node.has_value()) {
      break;
    }
    arena.node_stack_push(node.value());

    whitespace(p);

    if (!p.eat(',')) {
      break;
    }
  }

  whitespace(p);

  if (!p.eat(']')) {
    p.error("Expected closing ]");
  }

  auto pair = arena.node_stack_finish(start);
  return JsonNode::array(pair.first, pair.second);
}

JsonNode object(JsonParser &p, JsonArena &arena) {
  if (!p.eat('{')) {
    p.error("Expected array");
  }
  NodeStackIndex start = arena.node_stack_position();

  while (1) {
    whitespace(p);

    if (p.at('"')) {
      JsonNode name = string(p, arena);
      arena.node_stack_push(name);
    } else {
      break;
    }

    if (!p.eat(':')) {
      p.error("Expected :");
    }

    std::optional<JsonNode> node = value(p, arena);
    if (!node.has_value()) {
      p.error("Expected value");
      node = {JsonNode::error()};
    }
    arena.node_stack_push(node.value());

    whitespace(p);

    if (!p.eat(',')) {
      break;
    }
  }

  whitespace(p);

  if (!p.eat('}')) {
    p.error("Expected closing ]");
  }

  auto pair = arena.node_stack_finish(start);
  return JsonNode::object(pair.first, pair.second);
}

JsonNode parse_json(JsonParser &p, JsonArena &arena) {
  auto node = value(p, arena);
  if (node.has_value()) {
    return node.value();
  } else {
    p.error("Invalid json");
    return JsonNode::error();
  }
}
