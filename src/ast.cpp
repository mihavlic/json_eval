#include "ast.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <optional>

bool kind_is_function(NodeKind kind) {
  return kind >= NodeKind::_FUNCTIONS_START;
}
bool kind_is_array_like(NodeKind kind) {
  return kind_is_function(kind) || (kind == NodeKind::OBJECT) ||
         (kind == NodeKind::ARRAY);
}

AstNode::AstNode(NodeKind kind, size_t data, AstData value)
    : packed((data << 5) | (size_t)kind), value(value) {
  assert((int)kind < (1 << 5));
  assert(data < (1L << 59));
}

StringIndex Arena::string_position() const {
  return StringIndex(string_arena.size());
}

std::string_view Arena::get_string(StringIndex start, size_t len) const {
  return std::string_view(string_arena.data() + start.raw(), len);
}

std::string_view Arena::get_string_between(StringIndex start,
                                           StringIndex end) const {
  size_t len = 0;
  if (start.raw() < end.raw()) {
    len = end.raw() - start.raw();
  }
  return std::string_view(string_arena.data() + start.raw(), len);
}

std::span<AstNode> Arena::get_nodes(NodeIndex start, size_t len) {
  return std::span(node_arena.data() + start.raw(), len);
}

std::span<AstNode> Arena::get_node_stack(NodeStackIndex start, size_t len) {
  return std::span(node_stack.data() + start.raw(), len);
}

std::span<AstNode> Arena::get_node_stack_between(NodeStackIndex start,
                                                 NodeStackIndex end) {
  size_t len = 0;
  if (start.raw() < end.raw()) {
    len = end.raw() - start.raw();
  }
  return std::span(node_stack.data() + start.raw(), len);
}

std::pair<NodeIndex, size_t> Arena::node_stack_finish(NodeStackIndex start) {
  std::span<AstNode> children =
      get_node_stack_between(start, node_stack_position());
  size_t children_len = children.size();

  NodeIndex new_start(node_arena.size());
  for (AstNode node : children) {
    node_arena.push_back(node);
  }

  node_stack_truncate(start);

  return {new_start, children_len};
}

void Arena::debug_print_array(AstNode node, const char *name, int depth) {
  printf("%s\n", name);
  std::span<AstNode> children = as_array_like(node).value();
  for (AstNode node : children) {
    debug_print_impl(node, depth + 1);
  }
}

void Arena::debug_print_impl(AstNode node, int depth) {
  for (int i = 0; i < depth; i++) {
    printf("  ");
  }
  auto kind = node.get_kind();
  switch (kind) {
  case NodeKind::ERROR:
    printf("Error\n");
    break;
  case NodeKind::STRING: {
    std::cout << '"' << as_string_like(node).value() << '"' << std::endl;
    break;
  }
  case NodeKind::NUMBER:
    std::cout << as_number(node).value() << std::endl;
    break;
  case NodeKind::BOOLEAN:
    if (as_boolean(node).value())
      printf("true\n");
    else
      printf("false\n");
    break;
  case NodeKind::OBJECT: {
    debug_print_array(node, "{Object}", depth);
    break;
  }
  case NodeKind::ARRAY: {
    debug_print_array(node, "[Array]", depth);
    break;
  }
  case NodeKind::NIL:
    printf("null\n");
    break;
  case NodeKind::Add:
    debug_print_array(node, "(Add)", depth);
    break;
  case NodeKind::Sub:
    debug_print_array(node, "(Sub)", depth);
    break;
  case NodeKind::Mul:
    debug_print_array(node, "(Mul)", depth);
    break;
  case NodeKind::Div:
    debug_print_array(node, "(Div)", depth);
    break;
  case NodeKind::Eq:
    debug_print_array(node, "(Eq)", depth);
    break;
  case NodeKind::Max:
    debug_print_array(node, "(Max)", depth);
    break;
  case NodeKind::Min:
    debug_print_array(node, "(Min)", depth);
    break;
  case NodeKind::Size:
    debug_print_array(node, "(Size)", depth);
    break;
  case NodeKind::Subscript:
    debug_print_array(node, "(Subscript)", depth);
    break;
  case NodeKind::Field:
    debug_print_array(node, "(Field)", depth);
    break;
  case NodeKind::Identifier:
    std::cout << as_string_like(node).value() << std::endl;
    break;
  default:
    assert(0 && "Unhandled variant");
  }
}

void Arena::debug_print(AstNode node) { debug_print_impl(node, 0); }

std::optional<std::string_view> Arena::as_string_like(AstNode node) {
  if ((node.get_kind() == NodeKind::STRING) ||
      (node.get_kind() == NodeKind::Identifier)) {
    return get_string(node.get_value().string_start, node.get_data());
  }
  return {};
}
std::optional<double> Arena::as_number(AstNode node) const {
  if (node.get_kind() == NodeKind::NUMBER) {
    return node.get_value().number;
  }
  return {};
}
std::optional<bool> Arena::as_boolean(AstNode node) const {
  if (node.get_kind() == NodeKind::BOOLEAN) {
    return node.get_value().boolean;
  }
  return {};
}
std::optional<std::span<AstNode>> Arena::as_array_like(AstNode node) {
  if (kind_is_array_like(node.get_kind())) {
    NodeIndex start = node.get_value().nodes_start;
    size_t len = node.get_data();
    return get_nodes(start, len);
  }
  return {};
}
AstNode AstNode::function(NodeKind function, NodeIndex args_start,
                          size_t args_len) {
  assert(kind_is_function(function));
  return AstNode(function, args_len, {.nodes_start = args_start});
}
AstNode AstNode::empty_function(NodeKind function) {
  assert(kind_is_function(function));
  return AstNode(function, {}, {});
}
