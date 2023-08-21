//
// Created by jan on 21.8.2023.
//
#include "test_common.h"
#include "../include/jfnt_font.h"
#include <libpng16/png.h>
#include <wchar.h>
#include <locale.h>

enum {TEST_RANGE_FIRST = 0, TEST_RANGE_LAST = 0x52F};

int main()
{
    jfnt_font* font_monospace_fc;
    const jfnt_codepoint_range ranges[] =
            {
                    [0] = { .first = TEST_RANGE_FIRST, .last = TEST_RANGE_LAST },
            };
    const jfnt_error_callbacks err_callbacks =
            {
                    .char_param = NULL,
                    .report = test_report_callback,
                    .report_param = NULL,
                    .unsupported_char = test_unsupported_char,
            };
    const jfnt_font_create_info create_info =
            {
                    .n_ranges = sizeof(ranges) / sizeof(*ranges),
                    .codepoint_ranges = ranges,
                    .error_callbacks = &err_callbacks,
                    .flip = 1,
            };

    //  Create the font
    JFNT_TEST_CALL(jfnt_font_create_from_fc_str("Monospace:size=24", create_info, &font_monospace_fc),
                   JFNT_RESULT_SUCCESS);

    const jfnt_glyph* glyphs = jfnt_font_get_glyphs(font_monospace_fc);
    setlocale(LC_ALL, "en_US.utf8");

    for (unsigned i = 0; i < (TEST_RANGE_LAST - TEST_RANGE_FIRST + 1); ++i)
    {
        jfnt_glyph g = glyphs[i];
        printf(
                "Glyph %u: {.codepoint = \"%lc\", .top = %hd, .left = %hd, .w = %hu, .h = %hu, advance_x = %hu, .advance_y = %hu, .offset_x = %u}\n",
                i, g.codepoint, g.top, g.left, g.w, g.h, g.advance_x, g.advance_y, g.offset_x);
    }

    FILE* const f_out = fopen("raster_test.png", "wb");
    ASSERT(f_out != NULL);

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    ASSERT(png_ptr != NULL);

    png_infop info_ptr = png_create_info_struct(png_ptr);
    ASSERT(info_ptr != NULL);

    unsigned w, h;
    const unsigned char* img;
    jfnt_font_image(font_monospace_fc, &w, &h, &img);
    png_init_io(png_ptr, f_out);
    png_set_IHDR(
            png_ptr, info_ptr, w, h, 8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    png_bytepp rows = png_malloc(png_ptr, h * sizeof(const unsigned char*));
    ASSERT(rows != NULL);
    for (unsigned i = 0; i < h; ++i)
    {
        rows[i] = (png_bytep)(img + w * i);
    }
    png_write_image(png_ptr, rows);
    png_write_end(png_ptr, info_ptr);
    png_free(png_ptr, rows);
    rows = NULL;
    png_destroy_write_struct(&png_ptr, &info_ptr);


    fclose(f_out);
    //  Free the font and its memory
    jfnt_font_destroy(font_monospace_fc);
    return 0;
}
