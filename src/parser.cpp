#include "parser.h"

int Parser::peek() const { return current; }

int Parser::next() {
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

int Parser::eat(char c) {
  int peek = current;
  if (peek == c) {
    next();
    return peek;
  }
  return 0;
}

bool Parser::at(char c) const { return current == c; }

void Parser::consume_whitespace() {
  while (true) {
    switch (current) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
      next();
      continue;
    default:
      return;
    }
  }
}

void Parser::error(const char *message) {
  errors.push_back(ParseError{line, column, message});
}

void Parser::report_errors(const char *filename) const {
  for (const ParseError &error : errors) {
    printf("%s:%d:%d %s\n", filename, error.line, error.column, error.message);
  }
}
