#pragma once

#include <cstddef>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

enum class JsonNodeKind {
  ERROR = 0,
  STRING,
  NUMBER,
  BOOLEAN,
  OBJECT,
  ARRAY,
  NIL,
};

// typed wrappers around integer offsets into the arena

class StringIndex {
  size_t index;

public:
  StringIndex() = default;
  StringIndex(size_t index) : index(index) {}
  size_t raw() const { return index; }
};

class NodeIndex {
  size_t index;

public:
  NodeIndex() = default;
  NodeIndex(size_t index) : index(index) {}
  size_t raw() const { return index; }
};

class NodeStackIndex {
  size_t index;

public:
  NodeStackIndex() = default;
  NodeStackIndex(size_t index) : index(index) {}
  size_t raw() const { return index; }
};

union JsonNodeValue {
  StringIndex string_start;
  NodeIndex nodes_start;
  double number;
  bool boolean;
};

class JsonNode {
  size_t data;
  JsonNodeValue value;

public:
  JsonNode() = default;
  JsonNode(JsonNodeKind kind, size_t data, JsonNodeValue value);

  JsonNodeKind get_kind() const { return (JsonNodeKind)(data & 7); }
  size_t get_data() const { return data >> 3; }
  JsonNodeValue get_value() const { return value; }

  static JsonNode string(StringIndex start, size_t len) {
    return JsonNode(JsonNodeKind::STRING, len, {.string_start = start});
  }
  static JsonNode number(double value) {
    return JsonNode(JsonNodeKind::NUMBER, {}, {.number = value});
  }
  static JsonNode boolean(bool value) {
    return JsonNode(JsonNodeKind::BOOLEAN, {}, {.boolean = value});
  }
  // Json objects are conceptually arrays of pairs of (string, json value),
  // since we don't have enough space in JsonNode for the entire pair, and want
  // to avoid indirection, objects will be stored as two consecutive
  // arrays with keys and values interleaved in pairs
  //
  // JsonNode::OBJECT
  //   [
  //     (JsonNode::STRING JsonNode value)*
  //   ]
  static JsonNode object(NodeIndex start, size_t len) {
    return JsonNode(JsonNodeKind::OBJECT, len, {.nodes_start = start});
  }
  static JsonNode array(NodeIndex start, size_t len) {
    return JsonNode(JsonNodeKind::ARRAY, len, {.nodes_start = start});
  }
  static JsonNode nil() { return JsonNode(JsonNodeKind::NIL, {}, {}); }
  static JsonNode error() { return JsonNode(JsonNodeKind::ERROR, {}, {}); }
};

struct JsonField {
  JsonNode name;
  JsonNode value;
};

struct JsonMap {
  std::span<JsonField> fields;
};

struct JsonArray {
  std::span<JsonNode> values;
};

class JsonArena {
  std::vector<char> string_arena;
  std::vector<JsonNode> node_arena;
  std::vector<JsonNode> node_stack;

public:
  JsonArena() = default;

  StringIndex string_position() const;

  std::string_view get_string(StringIndex start, size_t len) const;

  std::string_view get_string_between(StringIndex start, StringIndex end) const;

  void string_push(char c) { string_arena.push_back(c); }

  // This method is dangerous!
  // Use it only to remove termporarily allocated strings
  // before anyone else added a string.
  void string_truncate(StringIndex previous_position) {
    string_arena.resize(previous_position.raw());
  }

  std::span<JsonNode> get_nodes(NodeIndex start, size_t len);

  NodeStackIndex node_stack_position() const {
    return NodeStackIndex(node_stack.size());
  }

  std::span<JsonNode> get_node_stack(NodeStackIndex start, size_t len);

  std::span<JsonNode> get_node_stack_between(NodeStackIndex start,
                                             NodeStackIndex end);

  void node_stack_truncate(NodeStackIndex previous_position) {
    node_stack.resize(previous_position.raw());
  }

  void node_stack_push(JsonNode node) { node_stack.push_back(node); }

  std::pair<NodeIndex, size_t> node_stack_finish(NodeStackIndex start);

  void debug_print_impl(JsonNode node, int depth);
  void debug_print(JsonNode node);
};

struct ParseError {
  int line;
  int column;
  const char *message;
};

class JsonParser {
  std::ifstream &file;
  int current;
  int line;
  int column;
  std::vector<ParseError> errors;

public:
  JsonParser(std::ifstream &file) : file(file), line(0), column(0) {
    current = this->file.get();
  }

  int peek() const;

  int next();

  template <typename F> int tryConsume(F fun) {
    int peek = current;
    if (fun(peek)) {
      next();
      return peek;
    }
    return 0;
  }

  int eat(char c);

  bool at(char c) const;

  void error(const char *message);

  void report_errors(const char *filename) const;
};

JsonNode parse_json(JsonParser &p, JsonArena &arena);
