//
// Created by jan on 21.8.2023.
//

#ifndef JFNT_TEST_COMMON_H
#define JFNT_TEST_COMMON_H
#include <stdlib.h>
#include <stdio.h>
#include "../include/jfnt_error.h"
#include <uchar.h>
typedef struct jfnt_font_T jfnt_font;

#ifndef NDEBUG
    #ifdef __GNUC__
        #define ASSERT(x) if (!(x)) {fprintf(stderr, "Assertion failed \"%s\"\n", #x); __builtin_trap();}(void)0
    #endif
#else
#define ASSERT(x) if (!(x)) {fprintf(stderr, "Assertion failed \"%s\"\n", #x); exit(EXIT_FAILED);}(void)0
#endif

#ifndef ASSERT
    #error macro ASSERT is undefined
#endif

#define JFNT_TEST_CALL(x, expected) {jfnt_result jfnt_res = (x); fprintf(stdout, "%s -> %s (%s)\n", #x, jfnt_result_to_str(jfnt_res), jfnt_result_message(jfnt_res)); ASSERT(jfnt_res == (expected));}(void)0

void test_report_callback(const char* msg, const char* function, const char* file, int line, void* param);

void test_unsupported_char(jfnt_font* font, char32_t c, const char* msg, void* param);

#endif //JFNT_TEST_COMMON_H
