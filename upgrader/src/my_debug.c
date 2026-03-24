#include "my_debug.h"

#include <stdarg.h>

#include "pico/stdlib.h"

static void __attribute__((noreturn)) upgrader_halt(void) {
  while (true) {
    tight_loop_contents();
  }
  __builtin_unreachable();
}

void put_out_error_message(const char *s) { (void)s; }

void put_out_info_message(const char *s) { (void)s; }

void put_out_debug_message(const char *s) { (void)s; }

int error_message_printf(const char *func, int line, const char *fmt, ...) {
  (void)func;
  (void)line;
  (void)fmt;
  return 0;
}

int error_message_printf_plain(const char *fmt, ...) {
  (void)fmt;
  return 0;
}

int debug_message_printf(const char *func, int line, const char *fmt, ...) {
  (void)func;
  (void)line;
  (void)fmt;
  return 0;
}

int info_message_printf(const char *fmt, ...) {
  (void)fmt;
  return 0;
}

void lock_printf(void) {}

void unlock_printf(void) {}

void my_assert_func(const char *file, int line, const char *func,
                    const char *pred) {
  (void)file;
  (void)line;
  (void)func;
  (void)pred;
  upgrader_halt();
}

void assert_case_is(const char *file, int line, const char *func, int v,
                    int expected) {
  (void)v;
  (void)expected;
  my_assert_func(file, line, func, "ASSERT_CASE_IS");
}

void assert_case_not_func(const char *file, int line, const char *func, int v) {
  (void)v;
  my_assert_func(file, line, func, "ASSERT_CASE_NOT");
}

void dump8buf(char *buf, size_t buf_sz, uint8_t *pbytes, size_t nbytes) {
  (void)buf;
  (void)buf_sz;
  (void)pbytes;
  (void)nbytes;
}

void hexdump_8(const char *s, const uint8_t *pbytes, size_t nbytes) {
  (void)s;
  (void)pbytes;
  (void)nbytes;
}

bool compare_buffers_8(const char *s0, const uint8_t *pbytes0, const char *s1,
                       const uint8_t *pbytes1, size_t nbytes) {
  (void)s0;
  (void)pbytes0;
  (void)s1;
  (void)pbytes1;
  (void)nbytes;
  return true;
}

void hexdump_32(const char *s, const uint32_t *pwords, size_t nwords) {
  (void)s;
  (void)pwords;
  (void)nwords;
}

bool compare_buffers_32(const char *s0, const uint32_t *pwords0, const char *s1,
                        const uint32_t *pwords1, size_t nwords) {
  (void)s0;
  (void)pwords0;
  (void)s1;
  (void)pwords1;
  (void)nwords;
  return true;
}
