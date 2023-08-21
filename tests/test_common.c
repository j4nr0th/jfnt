//
// Created by jan on 21.8.2023.
//

#include "test_common.h"

void test_report_callback(const char* msg, const char* function, const char* file, int line, void* param)
{
    (void) param;
    fprintf(stderr, "%s:%d - %s: \"%s\"\n", file, line, function, msg);
}

void test_unsupported_char(jfnt_font* font, char32_t c, const char* msg, void* param)
{
    (void) param;
    fprintf(stderr, "Unsupported char \"%lc\" by font %p, reason: %s\n", (wchar_t)c, font, msg);
}
