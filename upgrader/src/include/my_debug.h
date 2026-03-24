#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void put_out_error_message(const char *s);
void put_out_info_message(const char *s);
void put_out_debug_message(const char *s);

int error_message_printf(const char *func, int line, const char *fmt, ...)
    __attribute__((format(__printf__, 3, 4)));
#ifndef EMSG_PRINTF
#define EMSG_PRINTF(fmt, ...) ((void)0)
#endif

int error_message_printf_plain(const char *fmt, ...)
    __attribute__((format(__printf__, 1, 2)));

int debug_message_printf(const char *func, int line, const char *fmt, ...)
    __attribute__((format(__printf__, 3, 4)));
#ifndef DBG_PRINTF
#define DBG_PRINTF(fmt, ...) ((void)0)
#endif

int info_message_printf(const char *fmt, ...)
    __attribute__((format(__printf__, 1, 2)));
#ifndef IMSG_PRINTF
#define IMSG_PRINTF(fmt, ...) ((void)0)
#endif

void lock_printf(void);
void unlock_printf(void);

void my_assert_func(const char *file, int line, const char *func,
                    const char *pred) __attribute__((noreturn));
#ifdef NDEBUG
#define myASSERT(__e) ((void)0)
#else
#define myASSERT(__e) \
  ((__e) ? (void)0 : my_assert_func(__FILE__, __LINE__, __func__, #__e))
#endif

void assert_case_is(const char *file, int line, const char *func, int v,
                    int expected) __attribute__((noreturn));
#define ASSERT_CASE_IS(__v, __e) \
  ((__v == __e) ? (void)0        \
                : assert_case_is(__FILE__, __LINE__, __func__, __v, __e))

void assert_case_not_func(const char *file, int line, const char *func, int v)
    __attribute__((noreturn));
#define ASSERT_CASE_NOT(__v) \
  (assert_case_not_func(__FILE__, __LINE__, __func__, __v))

#ifdef NDEBUG
#define DBG_ASSERT_CASE_NOT(__v) ((void)0)
#else
#define DBG_ASSERT_CASE_NOT(__v) \
  (assert_case_not_func(__FILE__, __LINE__, __func__, __v))
#endif

static inline void dump_bytes(size_t num, uint8_t bytes[]) {
  (void)num;
  (void)bytes;
}

void dump8buf(char *buf, size_t buf_sz, uint8_t *pbytes, size_t nbytes);
void hexdump_8(const char *s, const uint8_t *pbytes, size_t nbytes);
bool compare_buffers_8(const char *s0, const uint8_t *pbytes0, const char *s1,
                       const uint8_t *pbytes1, size_t nbytes);
void hexdump_32(const char *s, const uint32_t *pwords, size_t nwords);
bool compare_buffers_32(const char *s0, const uint32_t *pwords0, const char *s1,
                        const uint32_t *pwords1, size_t nwords);

#ifdef __cplusplus
}
#endif
