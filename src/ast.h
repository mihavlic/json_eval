#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

enum class NodeKind {
  ERROR = 0,
  // json value nodes
  STRING,
  NUMBER,
  BOOLEAN,
  OBJECT,
  ARRAY,
  NIL,

  // filter language
  _FUNCTIONS_START,

  Add,
  Sub,
  Mul,
  Div,
  Eq,
  Max,
  Min,
  Size,
  Subscript,
  Field,
  Identifier
};

bool kind_is_function(NodeKind kind);
bool kind_is_array_like(NodeKind kind);

// typed wrappers of integer offsets into the arena

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

union AstData {
  StringIndex string_start;
  NodeIndex nodes_start;
  double number;
  bool boolean;
};

class AstNode {
  size_t packed;
  AstData value;

public:
  AstNode() = default;
  AstNode(NodeKind kind, size_t data, AstData value);

  NodeKind get_kind() const { return (NodeKind)(packed & 31); }
  size_t get_data() const { return packed >> 5; }
  AstData get_value() const { return value; }

  static AstNode string(StringIndex start, size_t len) {
    return AstNode(NodeKind::STRING, len, {.string_start = start});
  }
  static AstNode number(double value) {
    return AstNode(NodeKind::NUMBER, {}, {.number = value});
  }
  static AstNode boolean(bool value) {
    return AstNode(NodeKind::BOOLEAN, {}, {.boolean = value});
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
  static AstNode object(NodeIndex start, size_t len) {
    return AstNode(NodeKind::OBJECT, len, {.nodes_start = start});
  }
  static AstNode array(NodeIndex start, size_t len) {
    return AstNode(NodeKind::ARRAY, len, {.nodes_start = start});
  }
  static AstNode nil() { return AstNode(NodeKind::NIL, {}, {}); }
  static AstNode error() { return AstNode(NodeKind::ERROR, {}, {}); }
  static AstNode function(NodeKind function, NodeIndex args_start,
                          size_t args_len);
  static AstNode empty_function(NodeKind function);
  static AstNode identifier(StringIndex start, size_t len) {
    return AstNode(NodeKind::Identifier, len, {.string_start = start});
  }
};

struct Function {
  NodeKind function;
  std::span<AstNode> arguments;
};

class Arena {
  std::vector<char> string_arena;
  std::vector<AstNode> node_arena;
  std::vector<AstNode> node_stack;

public:
  Arena() = default;

  StringIndex string_position() const;

  std::string_view get_string(StringIndex start, size_t len) const;

  std::string_view get_string_between(StringIndex start, StringIndex end) const;

  void string_push(char c) { string_arena.push_back(c); }

  // This method is dangerous!
  // Use it only if you are sure there is no StringIndex to the truncated
  // position remaining
  void string_truncate(StringIndex previous_position) {
    string_arena.resize(previous_position.raw());
  }

  std::span<AstNode> get_nodes(NodeIndex start, size_t len);

  NodeStackIndex node_stack_position() const {
    return NodeStackIndex(node_stack.size());
  }

  std::span<AstNode> get_node_stack(NodeStackIndex start, size_t len);

  std::span<AstNode> get_node_stack_between(NodeStackIndex start,
                                            NodeStackIndex end);

  void node_stack_truncate(NodeStackIndex previous_position) {
    node_stack.resize(previous_position.raw());
  }

  void node_stack_push(AstNode node) { node_stack.push_back(node); }

  NodeIndex nodes_push(AstNode node) {
    NodeIndex index(node_arena.size());
    node_arena.push_back(node);
    return index;
  }

  std::pair<NodeIndex, size_t> node_stack_finish(NodeStackIndex start);

  std::optional<std::string_view> as_string_like(AstNode node);
  std::optional<double> as_number(AstNode node) const;
  std::optional<bool> as_boolean(AstNode node) const;
  std::optional<std::span<AstNode>> as_array_like(AstNode node);

  void debug_print(AstNode node);

private:
  void debug_print_impl(AstNode node, int depth);
  void debug_print_array(AstNode node, const char *name, int depth);
};
