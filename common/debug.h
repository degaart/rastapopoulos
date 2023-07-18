#pragma once

#define TRACE(...) trace(__FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define PANIC(...) panic(__FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define ASSERT(cond) if(!(cond)) PANIC("Assertion failed: " #cond "\n")
#define assert(cond) ASSERT(cond)
#define DUMP(var) TRACE(#var ": %u", var)
#define DUMPX(var) TRACE(#var ": 0x%X", var)
#define DUMPP(var) TRACE(#var ": %p", var)
#define BREAKPOINT() asm volatile("xchg bx, bx":::"memory")

void trace_init(void);
void trace(const char* file, int line, const char* fn, const char* fmt, ...) __attribute__((format(printf, 4, 5)));
void panic(const char* file, int line, const char* fn, const char* fmt, ...) __attribute__((format(printf, 4, 5)));

#define ADD_TEST(fn) add_test(#fn, fn);
typedef void (*testfn_t)(void);
void add_test(const char* name, testfn_t testfn);
void run_tests(void);

