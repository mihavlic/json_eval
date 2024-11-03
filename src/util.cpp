#include <cstdio>
#include <cstdlib>

[[gnu::cold]] [[noreturn]] void __panic_impl(const char *message,
                                             const char *file, int line) {
  fprintf(stderr, "%s:%d %s\n", file, line, message);
  exit(1);
}

void __error_impl(const char *message, const char *file, int line) {
  fprintf(stderr, "%s\n", message);
}
