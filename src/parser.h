#pragma once

#include <istream>
#include <vector>

class Parser {
  struct ParseError {
    int line;
    int column;
    const char *message;
  };

  std::istream &file;
  int current;
  int line;
  int column;
  std::vector<ParseError> errors;

public:
  Parser(std::istream &input) : file(input), line(0), column(0) {
    current = this->file.get();
  }

  int peek() const;

  int next();

  template <typename F> int try_consume(F fun) {
    int peek = current;
    if (fun(peek)) {
      next();
      return peek;
    }
    return 0;
  }

  int eat(char c);
  bool at(char c) const;

  void consume_whitespace();

  void error(const char *message);
  void report_errors(const char *filename) const;
};
