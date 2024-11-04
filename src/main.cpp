#include "eval.h"
#include "parser_driver.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

void print_help() {
  const char *message = "Usage: json_eval <JSON FILE> <EXPRESSION>\n";
  fprintf(stderr, "%s", message);
}

struct CliOptions {
  bool benchmark;
};

int main(int argc, const char *argv[]) {
  for (int i = 0; i < argc; i++) {
    if (std::strcmp(argv[i], "--help") == 0) {
      print_help();
      return 0;
    }
  }

  const char *path = "/dev/null";
  const char *expression = "";

  if (argc > 1) {
    path = argv[1];
  }
  if (argc > 2) {
    expression = argv[2];
  }

  // argv[0] is the executable path, so we expect 3 args in total
  if (argc != 3) {
    printf("Expected 2 arguments\n");
    // print_help();
    // return 1;
  }

  std::ifstream file(path);
  if (file.fail()) {
    printf("Couldn't open file '%s'", path);
    return 1;
  }

  Arena arena{};
  Parser parser{};

  parser.set_new_input(file);
  auto json = parse_json(parser, arena);

  std::istringstream expr(expression);
  parser.set_new_input(expr);
  auto ex = parse_expression(parser, arena);

  printf("\n<<Json>>\n");
  arena.debug_print(json);

  printf("\n<<Expression>>\n");
  arena.debug_print(ex);

  parser.report_errors(path);

  printf("\n<<Eval>>\n");
  Evaluator ev(arena, json);

  Value v = eval(ex, ev);
  v.debug_print(arena);

  ev.report_errors();
  return 0;
}
