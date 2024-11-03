#include "util.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

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

  // argv[0] is the executable path, so we expect 3 args in total
  if (argc != 3) {
    LOG_ERROR("Expected 2 arguments");
    print_help();
    return 1;
  }

  const char *path = argv[1];
  const char *expression = argv[2];

  std::ifstream file(path);
  if (file.fail()) {
    LOG_ERROR("Couldn't open file");
    LOG_ERROR(path);
    return 1;
  }

  (void)expression;

  return 0;
}
