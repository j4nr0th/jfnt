//
// Created by jan on 19.8.2023.
//

#ifndef JFNT_JFNT_FONT_H
#define JFNT_JFNT_FONT_H
#include <stddef.h>
#include <uchar.h>
#include "jfnt_error.h"

typedef struct jfnt_font_T jfnt_font;
struct jfnt_error_callbacks_T
{
    void (*report)(const char* message, const char* function, const char* file, int line, void* param);
    void* report_param;
    void (*unsupported_char)(jfnt_font* font, char32_t c, const char* msg, void* param);
    void* char_param;
};
typedef struct jfnt_error_callbacks_T jfnt_error_callbacks;

struct jfnt_allocator_callbacks_T
{
    void* (*allocate)(void* state, size_t size);
    void* (*reallocate)(void* state, void* ptr, size_t new_size);
    void (*deallocate)(void* state, void* ptr);
    void* state;
};
typedef struct jfnt_allocator_callbacks_T jfnt_allocator_callbacks;


struct jfnt_glyph_T
{
    char32_t codepoint;
    signed short top, left;
    unsigned short w, h;
    unsigned short advance_x, advance_y;
    unsigned int offset_x;
};
typedef struct jfnt_glyph_T jfnt_glyph;

struct jfnt_codepoint_range_T
{
    char32_t first;
    char32_t last;
};
typedef struct jfnt_codepoint_range_T jfnt_codepoint_range;

struct jfnt_font_create_info_T
{
    const jfnt_allocator_callbacks* allocator_callbacks;
    const jfnt_error_callbacks* error_callbacks;
    unsigned n_ranges;
    const jfnt_codepoint_range* codepoint_ranges;
    int flip;
};
typedef struct jfnt_font_create_info_T jfnt_font_create_info;

void* jfnt_alloc(const jfnt_font* font, size_t size);

void* jfnt_realloc(const jfnt_font* font, void* ptr, size_t new_size);

void jfnt_free(const jfnt_font* font, void* ptr);

extern const jfnt_allocator_callbacks DEFAULT_ALLOCATOR;

/*
 * Create from memory
 */
jfnt_result jfnt_font_create_from_memory(
        size_t size, const void* mem, unsigned char_size, jfnt_font_create_info create_info, jfnt_font** p_out);

/*
 * Create from a file with specific name
 */
jfnt_result jfnt_font_create_from_filename(
        const char* filename, unsigned char_size, jfnt_font_create_info create_info, jfnt_font** p_out);

/*
 * Create from fontconfig string
 */
jfnt_result jfnt_font_create_from_fc_str(const char* fc_str, jfnt_font_create_info create_info, jfnt_font** p_out);

/*
 * Destroy the font and all of its associated structs
 */
void jfnt_font_destroy(jfnt_font* font);

jfnt_result jfnt_font_find_glyphs_u32(
        const jfnt_font* font, char32_t unsupported_replace, size_t count, const char32_t* codepoints, int* p_indices);

jfnt_result jfnt_font_find_glyphs_utf8(
        const jfnt_font* font, const char* utf8, char32_t unsupported_replace, size_t max_len, size_t* p_count,
        int* p_indices);

void
jfnt_font_get_sizes(const jfnt_font* font, unsigned* p_height, unsigned* p_avg_w, unsigned* p_size_h, unsigned* p_size_v);

void
jfnt_font_get_measures(const jfnt_font* font, unsigned* p_height, int* ascent, int* descent);

const jfnt_glyph* jfnt_font_get_glyphs(const jfnt_font* font);

void jfnt_font_image(const jfnt_font* font, unsigned* p_width, unsigned* p_height, const unsigned char** p_data);

#ifdef __GNUC__
__attribute__((format(printf, 5, 6)))
#endif
void jfnt_report_error(const jfnt_font* font, const char* file, int line, const char* function, const char* fmt, ...);

#ifdef __GNUC__
    #define JFNT_ERROR(fnt, fmt, ...) jfnt_report_error((fnt), __FILE__, __LINE__, __func__, (fmt) __VA_OPT__(,) __VA_ARGS__)
#else
    #define JFNT_ERROR(fnt, fmt, ...) jfnt_report_error((fnt), __FILE__, __LINE__, __func__, (fmt), __VA_ARGS__)
#endif

#endif //JFNT_JFNT_FONT_H
