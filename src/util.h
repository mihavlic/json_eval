#pragma once

#define PANIC(message) __panic_impl(message, __FILE__, __LINE__);

#define LOG_ERROR(message) __error_impl(message, __FILE__, __LINE__);

[[gnu::cold]] [[noreturn]] void __panic_impl(const char *message,
                                             const char *file, int line);

void __error_impl(const char *message, const char *file, int line);
